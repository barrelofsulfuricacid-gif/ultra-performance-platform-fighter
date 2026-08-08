#!/usr/bin/env python3
"""Run manifest-selected SSBM equivalence checks without launching Dolphin."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import Any


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent


def fail(message: str) -> None:
    raise SystemExit(f"ssbm-stored-equivalence=fail {message}")


def repository_path(value: Any, field: str) -> Path:
    if not isinstance(value, str) or not value:
        fail(f"operation=manifest field={field} reason=invalid-path")
    candidate = (REPOSITORY_ROOT / value).resolve()
    try:
        candidate.relative_to(REPOSITORY_ROOT)
    except ValueError:
        fail(f"operation=manifest field={field} reason=path-escapes-repository")
    return candidate


def load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"operation=manifest label={label} reason={error}")
    if not isinstance(value, dict) or value.get("schema") != 1:
        fail(f"operation=manifest label={label} reason=unsupported-schema")
    return value


def normalized_repository_relative_path(value: str) -> str:
    candidate = Path(value)
    if candidate.is_absolute():
        try:
            candidate = candidate.resolve().relative_to(REPOSITORY_ROOT)
        except ValueError:
            fail(f"operation=selection reason=path-outside-repository path={value}")
    normalized = candidate.as_posix().removeprefix("./")
    if normalized == ".." or normalized.startswith("../"):
        fail(f"operation=selection reason=path-outside-repository path={value}")
    return normalized


def string_list(value: Any, field: str) -> list[str]:
    if (
        not isinstance(value, list)
        or any(not isinstance(item, str) or not item for item in value)
    ):
        fail(f"operation=manifest field={field} reason=expected-string-list")
    return list(value)


def changed_paths_from_git(base: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", "--relative", base, "--"],
        cwd=REPOSITORY_ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        fail(
            "operation=selection reason=git-diff-failed "
            f"detail={result.stderr.strip()!r}"
        )
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        cwd=REPOSITORY_ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if untracked.returncode != 0:
        fail(
            "operation=selection reason=git-untracked-failed "
            f"detail={untracked.stderr.strip()!r}"
        )
    return [
        normalized_repository_relative_path(line)
        for line in (*result.stdout.splitlines(), *untracked.stdout.splitlines())
        if line.strip()
    ]


def path_matches_any(path: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def executable_path(build_directory: Path, name: str) -> Path:
    suffixes = [".exe", ""] if os.name == "nt" else ["", ".exe"]
    configurations = [Path(), Path("Release")]
    for configuration in configurations:
        for suffix in suffixes:
            candidate = build_directory / configuration / f"{name}{suffix}"
            if candidate.is_file():
                return candidate.resolve()
    fail(
        "operation=runner reason=missing-executable "
        f"name={name} build_dir={build_directory}"
    )


def run_checked(command: list[str], label: str) -> str:
    try:
        result = subprocess.run(
            command,
            cwd=REPOSITORY_ROOT,
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
        )
    except subprocess.TimeoutExpired:
        fail(f"operation={label} reason=timeout")
    if result.returncode != 0:
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        fail(
            f"operation={label} reason=exit-code code={result.returncode}"
        )
    return result.stdout


def output_fields(output: str, prefix: str, label: str) -> dict[str, str]:
    for line in output.splitlines():
        fields = dict(
            token.split("=", 1)
            for token in line.split()
            if "=" in token
        )
        if fields.get(prefix) == "pass":
            return fields
    fail(f"operation={label} reason=missing-pass-record")


def require_runner(value: Any, field: str) -> tuple[str, list[str]]:
    if not isinstance(value, dict):
        fail(f"operation=manifest field={field} reason=expected-object")
    executable = value.get("executable")
    if not isinstance(executable, str) or not executable:
        fail(f"operation=manifest field={field}.executable reason=invalid")
    arguments = string_list(value.get("arguments"), f"{field}.arguments")
    return executable, arguments


def pose_count(domain: dict[str, Any]) -> int:
    tracks = domain.get("pose_tracks")
    if not isinstance(tracks, list):
        fail("operation=manifest field=pose_tracks reason=expected-list")
    total = 0
    for track in tracks:
        if not isinstance(track, dict) or not isinstance(track.get("frames"), dict):
            fail("operation=manifest field=pose_tracks reason=invalid-track")
        frames = track["frames"]
        try:
            first = int(frames["first"])
            last = int(frames["last"])
            step = int(frames["step"])
        except (KeyError, TypeError, ValueError):
            fail("operation=manifest field=pose_tracks reason=invalid-frame-range")
        if first < 1 or last < first or step < 1 or (last - first) % step:
            fail("operation=manifest field=pose_tracks reason=invalid-frame-range")
        total += (last - first) // step + 1
    return total


def manifest_digest(
    root_manifest_path: Path,
    domain_manifest_paths: list[Path],
) -> str:
    digest = hashlib.sha256()
    for path in (root_manifest_path, *domain_manifest_paths):
        relative = path.relative_to(REPOSITORY_ROOT).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("tools/ssbm_equivalence_manifest.json"),
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("build/windows-msvc-release" if os.name == "nt" else "build/release"),
    )
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--all", action="store_true")
    selection.add_argument("--domain", action="append", default=[])
    selection.add_argument("--changed-from")
    parser.add_argument("--changed-file", action="append", default=[])
    args = parser.parse_args()

    started = time.perf_counter()
    manifest_path = repository_path(args.manifest.as_posix(), "manifest")
    manifest = load_json(manifest_path, "root")
    try:
        budget_ms = int(manifest["post_build_budget_ms"])
        selection_manifest = manifest["selection"]
        domain_names = string_list(manifest["domains"], "domains")
    except (KeyError, TypeError, ValueError):
        fail("operation=manifest label=root reason=missing-required-field")
    if budget_ms <= 0 or not isinstance(selection_manifest, dict):
        fail("operation=manifest label=root reason=invalid-budget-or-selection")
    shared_patterns = string_list(
        selection_manifest.get("shared_paths"),
        "selection.shared_paths",
    )

    domains: list[tuple[Path, dict[str, Any]]] = []
    seen_domains: set[str] = set()
    for index, relative_name in enumerate(domain_names):
        path = repository_path(relative_name, f"domains[{index}]")
        domain = load_json(path, relative_name)
        name = domain.get("domain")
        if not isinstance(name, str) or not name or name in seen_domains:
            fail(f"operation=manifest label={relative_name} reason=invalid-domain")
        seen_domains.add(name)
        domains.append((path, domain))

    requested_domains = set(args.domain)
    unknown_domains = requested_domains - seen_domains
    if unknown_domains:
        fail(
            "operation=selection reason=unknown-domain domains="
            + ",".join(sorted(unknown_domains))
        )
    changed_files = [
        normalized_repository_relative_path(path) for path in args.changed_file
    ]
    if args.changed_from:
        changed_files.extend(changed_paths_from_git(args.changed_from))
    changed_files = sorted(set(changed_files))

    if args.all:
        selected = list(domains)
    elif requested_domains:
        selected = [item for item in domains if item[1]["domain"] in requested_domains]
    elif changed_files:
        shared_change = any(
            path_matches_any(path, shared_patterns) for path in changed_files
        )
        selected = []
        for item in domains:
            domain_selection = item[1].get("selection")
            if not isinstance(domain_selection, dict):
                fail(
                    f"operation=manifest domain={item[1]['domain']} "
                    "reason=missing-selection"
                )
            patterns = string_list(
                domain_selection.get("paths"),
                f"{item[1]['domain']}.selection.paths",
            )
            if shared_change or any(
                path_matches_any(path, patterns) for path in changed_files
            ):
                selected.append(item)
    else:
        selected = list(domains)

    if not selected:
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        print(
            "ssbm-stored-equivalence=pass domains=none generated_checks=0 "
            f"stored_cases=0 replay=skipped changed_files={len(changed_files)} "
            f"elapsed_ms={elapsed_ms:.3f} budget_ms={budget_ms}"
        )
        return 0

    build_directory = args.build_dir
    if not build_directory.is_absolute():
        build_directory = (REPOSITORY_ROOT / build_directory).resolve()
    generated_checks = 0
    stored_cases = 0
    for domain_path, domain in selected:
        domain_name = str(domain["domain"])
        stored = domain.get("stored_oracle")
        if not isinstance(stored, dict):
            fail(f"operation=manifest domain={domain_name} reason=missing-stored-oracle")
        generator = stored.get("generator")
        if not isinstance(generator, dict):
            fail(f"operation=manifest domain={domain_name} reason=missing-generator")
        generator_script = repository_path(
            generator.get("script"),
            f"{domain_name}.stored_oracle.generator.script",
        )
        generated_output = repository_path(
            generator.get("output"),
            f"{domain_name}.stored_oracle.generator.output",
        )
        run_checked(
            [
                sys.executable,
                str(generator_script),
                str(domain_path),
                str(generated_output),
                "--check",
            ],
            f"generated-check-{domain_name}",
        )
        generated_checks += 1

        executable, arguments = require_runner(
            stored.get("runner"),
            f"{domain_name}.stored_oracle.runner",
        )
        output = run_checked(
            [str(executable_path(build_directory, executable)), *arguments],
            f"stored-runner-{domain_name}",
        )
        fields = output_fields(
            output,
            "m4-ssbm-stored-oracle",
            f"stored-runner-{domain_name}",
        )
        kind = stored.get("kind", "pose-geometry-v1")
        if kind == "numeric-trace-v1":
            checkpoint_pack = domain.get("checkpoint_pack")
            capture_plan = (
                checkpoint_pack.get("capture_plan")
                if isinstance(checkpoint_pack, dict)
                else None
            )
            cases = (
                capture_plan.get("damage_response_cases")
                if isinstance(capture_plan, dict)
                else None
            )
            if not isinstance(cases, list) or not cases:
                fail(
                    f"operation=manifest domain={domain_name} "
                    "reason=invalid-trace-cases"
                )
            samples_per_case = stored.get("samples_per_case")
            if (
                not isinstance(samples_per_case, int)
                or isinstance(samples_per_case, bool)
                or samples_per_case <= 0
            ):
                fail(
                    f"operation=manifest domain={domain_name} "
                    "reason=invalid-samples-per-case"
                )
            expected = {
                "domain": domain_name,
                "poses": "0",
                "cases": str(len(cases)),
                "samples": str(len(cases) * samples_per_case),
                "source_trace_sha256": str(
                    stored.get("source_trace_sha256")
                ),
                "production_trace_sha256": str(
                    stored.get("production_trace_sha256")
                ),
            }
        elif kind == "pose-geometry-v1":
            cases = stored.get("cases")
            if not isinstance(cases, list):
                fail(
                    f"operation=manifest domain={domain_name} "
                    "reason=invalid-cases"
                )
            expected = {
                "domain": domain_name,
                "poses": str(pose_count(domain)),
                "cases": str(len(cases)),
                "source_pose_sha256": str(
                    stored.get("source_pose_sha256")
                ),
                "production_pose_sha256": str(
                    stored.get("production_pose_sha256")
                ),
            }
        else:
            fail(
                f"operation=manifest domain={domain_name} "
                f"reason=unsupported-stored-kind kind={kind!r}"
            )
        for field, value in expected.items():
            if fields.get(field) != value:
                fail(
                    f"operation=stored-runner-{domain_name} field={field} "
                    f"expected={value} actual={fields.get(field)}"
                )
        stored_cases += len(cases)

    replay = manifest.get("replay")
    executable, arguments = require_runner(replay, "replay")
    replay_output = run_checked(
        [str(executable_path(build_directory, executable)), *arguments],
        "replay",
    )
    replay_fields = output_fields(replay_output, "sim-replay", "replay")
    if not isinstance(replay, dict) or not isinstance(replay.get("expected"), dict):
        fail("operation=manifest field=replay.expected reason=expected-object")
    for field, value in replay["expected"].items():
        if not isinstance(value, str) or replay_fields.get(field) != value:
            fail(
                f"operation=replay field={field} expected={value} "
                f"actual={replay_fields.get(field)}"
            )

    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if elapsed_ms > budget_ms:
        fail(
            f"operation=budget elapsed_ms={elapsed_ms:.3f} budget_ms={budget_ms}"
        )
    selected_names = ",".join(str(domain["domain"]) for _, domain in selected)
    digest = manifest_digest(manifest_path, [path for path, _ in domains])
    print(
        f"ssbm-stored-equivalence=pass domains={selected_names} "
        f"generated_checks={generated_checks} stored_cases={stored_cases} "
        f"replay=pass changed_files={len(changed_files)} "
        f"manifest_sha256={digest} elapsed_ms={elapsed_ms:.3f} "
        f"budget_ms={budget_ms}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
