#!/usr/bin/env python3
"""Qualify source-evaluated action hurt poses against Dolphin captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from hsd_figatree import decode_figatree
from hsd_joint_pose import fighter_animation_slice
from verify_ssbm_dynamic_hurt_pose_source import (
    build_hurt_pose_source,
    compare_hurt_pose_q16,
    load_pinned_source,
    require,
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("fighter_dat", type=Path)
    parser.add_argument("animation_dat", type=Path)
    parser.add_argument("common_dat", type=Path)
    parser.add_argument("model_dat", type=Path)
    parser.add_argument("captures", type=Path, nargs="+")
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    qualification = manifest.get("action_hurt_qualification")
    require(
        isinstance(qualification, dict),
        "manifest has no action hurt qualification",
    )
    cases = qualification.get("cases")
    capture_specs = qualification.get("captures")
    require(
        isinstance(cases, list) and cases,
        "action hurt qualification has no cases",
    )
    require(
        isinstance(capture_specs, list) and capture_specs,
        "action hurt qualification has no captures",
    )
    specs_by_digest = {
        str(spec["sha256"]): spec
        for spec in capture_specs
        if isinstance(spec, dict)
    }
    require(
        len(specs_by_digest) == len(capture_specs),
        "action hurt capture digests must be unique",
    )
    supplied: dict[str, tuple[Path, dict[str, Any]]] = {}
    for path in args.captures:
        raw = path.read_bytes()
        digest = sha256(raw)
        require(digest in specs_by_digest, f"undeclared capture SHA-256: {digest}")
        require(digest not in supplied, f"duplicate capture SHA-256: {digest}")
        supplied[digest] = (path, json.loads(raw))
    require(
        set(supplied) == set(specs_by_digest),
        "supplied action hurt captures do not match the manifest",
    )

    fighter_raw = load_pinned_source(args.fighter_dat, manifest, "fighter_dat")
    animation_raw = load_pinned_source(
        args.animation_dat,
        manifest,
        "animation_dat",
    )
    common_raw = load_pinned_source(args.common_dat, manifest, "common_dat")
    model_raw = load_pinned_source(args.model_dat, manifest, "model_dat")
    source = build_hurt_pose_source(
        manifest,
        fighter_raw,
        animation_raw,
        common_raw,
        model_raw,
    )
    tolerance = int(qualification["coordinate_tolerance_q16"])
    require(tolerance >= 0, "action hurt tolerance must be nonnegative")
    animations: dict[int, Any] = {}
    total_samples = 0
    maximum_difference = 0

    for digest, spec in specs_by_digest.items():
        _, capture = supplied[digest]
        capture_name = str(spec["id"])
        require(
            capture.get("fighter") == manifest["fighter"],
            f"{capture_name}: fighter mismatch",
        )
        require(
            capture.get("disc", {}).get("sha256")
            == qualification["disc_sha256"],
            f"{capture_name}: disc mismatch",
        )
        require(
            capture.get("oracle_execution", {}).get("release_artifact_sha256")
            == qualification["dolphin_release_artifact_sha256"],
            f"{capture_name}: Dolphin oracle artifact mismatch",
        )
        require(
            capture.get(str(qualification["capture_route_field"])) is True,
            f"{capture_name}: wrong capture route",
        )
        probe = capture.get("hitbox_memory_probe")
        require(
            isinstance(probe, dict)
            and probe.get("engine_version")
            == qualification["probe_engine_version"]
            and probe.get("decomp_revision")
            == qualification["decomp_revision"],
            f"{capture_name}: probe provenance mismatch",
        )
        rows = capture.get("rows")
        require(isinstance(rows, list), f"{capture_name}: missing capture rows")
        for case in cases:
            action = str(case["source_action"])
            submotion = int(case["submotion_index"])
            selected = [row for row in rows if row.get("action") == action]
            require(
                len(selected) == int(case["expected_samples"]),
                f"{capture_name}/{action}: expected "
                f"{case['expected_samples']} rows, got {len(selected)}",
            )
            source_frames = [
                float(row["hitbox_memory"]["fighter_animation_frame"])
                for row in selected
            ]
            first_source_frame = int(case["first_source_frame"])
            last_source_frame = int(case["last_source_frame"])
            source_frame_cycle = case.get("source_frame_cycle")
            if source_frame_cycle is None:
                expected_source_frames = [
                    float(frame)
                    for frame in range(
                        first_source_frame,
                        last_source_frame + 1,
                    )
                ]
            else:
                cycle = int(source_frame_cycle)
                require(
                    cycle > 0
                    and first_source_frame >= 0
                    and last_source_frame == cycle - 1,
                    f"{capture_name}/{action}: invalid source-frame cycle",
                )
                expected_source_frames = [
                    float((first_source_frame + index) % cycle)
                    for index in range(int(case["expected_samples"]))
                ]
            require(
                source_frames == expected_source_frames,
                f"{capture_name}/{action}: incomplete source-frame sequence",
            )
            if submotion not in animations:
                animations[submotion] = decode_figatree(
                    fighter_animation_slice(
                        source.fighter_archive,
                        source.animation_raw,
                        source.fighter_root,
                        submotion,
                    )
                )
            case_maximum = 0
            for row in selected:
                case_maximum = max(
                    case_maximum,
                    compare_hurt_pose_q16(
                        row,
                        source.source_joints,
                        animations[submotion],
                        source.capsules,
                        source.layout,
                        source.coordinate_scale_q16,
                        source.axis_sign,
                        tolerance,
                        f"{capture_name}/{action}",
                    ),
                )
            print(
                "ssbm-hsd-action-hurt-source-case=pass "
                f"capture={capture_name} action={action} "
                f"samples={len(selected)} max_q16={case_maximum}"
            )
            total_samples += len(selected)
            maximum_difference = max(maximum_difference, case_maximum)

    print(
        "ssbm-hsd-action-hurt-source=pass "
        f"captures={len(capture_specs)} cases={len(cases)} "
        f"motions={len(animations)} samples={total_samples} "
        f"capsules={total_samples * len(source.capsules)} "
        f"max_q16={maximum_difference}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
