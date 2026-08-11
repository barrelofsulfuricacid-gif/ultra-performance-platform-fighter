#!/usr/bin/env python3
"""Generate a compact stored oracle for a dynamic HSD pose evaluator."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

from ssbm_collision import canonical_hurt_pose_q16
from ssbm_ecb_pose import pose_q16


def c_i32(value: int) -> str:
    return f"INT32_C({value})" if value >= 0 else f"-INT32_C({-value})"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--oracle-key", default="stored_oracle")
    parser.add_argument("--capture-id", default="primary")
    parser.add_argument(
        "--additional-capture",
        action="append",
        default=[],
        metavar="ID=PATH",
    )
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    oracle = manifest.get(args.oracle_key)
    expected = manifest.get("expected")
    conversion = manifest.get("pose_conversion")
    symbol_prefix = str(
        oracle.get("symbol_prefix", manifest.get("symbol_prefix", ""))
        if isinstance(oracle, dict)
        else ""
    )
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol_prefix):
        raise SystemExit("manifest symbol_prefix is not a C identifier")
    if not isinstance(expected, dict) or not isinstance(conversion, dict):
        raise SystemExit("manifest is missing expected counts or pose conversion")
    capsule_count = expected.get("capsule_count")
    numerator = conversion.get("source_to_sim_numerator")
    denominator = conversion.get("source_to_sim_denominator")
    if (
        not isinstance(capsule_count, int)
        or capsule_count <= 0
        or not isinstance(numerator, int)
        or not isinstance(denominator, int)
        or numerator <= 0
        or denominator <= 0
    ):
        raise SystemExit("manifest has invalid capsule count or coordinate scale")
    coordinate_scale_q16 = 65536.0 * float(numerator) / float(denominator)
    macro_prefix = symbol_prefix.upper() + "_ORACLE"
    if not isinstance(oracle, dict) or not isinstance(oracle.get("cases"), list):
        raise SystemExit("manifest has no dynamic hurt-pose stored oracle")
    capture_paths = {args.capture_id: args.capture}
    for value in args.additional_capture:
        capture_id, separator, path = value.partition("=")
        if (
            not separator
            or not capture_id
            or not path
            or capture_id in capture_paths
        ):
            raise SystemExit(f"invalid or duplicate --additional-capture: {value}")
        capture_paths[capture_id] = Path(path)
    capture_specs = oracle.get("captures")
    if isinstance(capture_specs, list):
        expected_digests = {
            str(spec["id"]): str(spec["sha256"])
            for spec in capture_specs
            if isinstance(spec, dict) and "id" in spec and "sha256" in spec
        }
        if len(expected_digests) != len(capture_specs):
            raise SystemExit("oracle has invalid capture specifications")
    else:
        expected_digests = {args.capture_id: str(oracle.get("capture_sha256", ""))}
    if set(capture_paths) != set(expected_digests):
        raise SystemExit(
            "capture IDs do not match oracle: "
            f"expected={sorted(expected_digests)} actual={sorted(capture_paths)}"
        )
    rows_by_capture: dict[str, dict[int, dict[str, object]]] = {}
    actual_digests: dict[str, str] = {}
    for capture_id, capture_path in capture_paths.items():
        capture_bytes = capture_path.read_bytes()
        actual_digest = hashlib.sha256(capture_bytes).hexdigest()
        if expected_digests[capture_id] != actual_digest:
            raise SystemExit(
                "unexpected dynamic hurt-pose capture SHA-256: "
                f"id={capture_id} expected={expected_digests[capture_id]} "
                f"actual={actual_digest}"
            )
        capture = json.loads(capture_bytes)
        rows_by_capture[capture_id] = {
            int(row["trace_frame"]): row for row in capture.get("rows", [])
        }
        actual_digests[capture_id] = actual_digest
    rendered_cases: list[
        tuple[
            dict[str, object],
            int,
            tuple[tuple[int, ...], ...],
            dict[str, list[int]],
        ]
    ] = []
    for case in oracle["cases"]:
        capture_id = str(case.get("capture_id", args.capture_id))
        rows = rows_by_capture.get(capture_id)
        if rows is None:
            raise SystemExit(
                f"unknown capture ID for case {case.get('id')}: {capture_id}"
            )
        trace_frame = int(case["trace_frame"])
        row = rows.get(trace_frame)
        if row is None:
            raise SystemExit(
                f"missing capture trace frame {capture_id}:{trace_frame}"
            )
        memory = row.get("hitbox_memory")
        if not isinstance(memory, dict):
            raise SystemExit(f"trace frame {trace_frame} has no hitbox memory")
        frame = memory.get("fighter_animation_frame")
        facing = row.get("facing")
        if not isinstance(frame, (int, float)) or facing not in (-1, 1):
            raise SystemExit(f"trace frame {trace_frame} has invalid pose metadata")
        capsules = canonical_hurt_pose_q16(
            memory,
            "fighter_hurtboxes",
            "fighter_position",
            int(facing),
            coordinate_scale_q16,
            "position",
        )
        if len(capsules) != capsule_count:
            raise SystemExit(f"trace frame {trace_frame} has {len(capsules)} capsules")
        ecb = memory.get("fighter_ecb")
        if not isinstance(ecb, dict):
            raise SystemExit(f"trace frame {trace_frame} has no fighter ECB")
        rendered_cases.append(
            (case, round(float(frame) * 65536.0), capsules, pose_q16(ecb))
        )

    lines = [
        "/* Generated by tools/generate_ssbm_dynamic_hurt_oracle.py. */",
        f"#define {macro_prefix}_TOLERANCE_Q16 INT32_C({int(oracle['coordinate_tolerance_q16'])})",
        f"#define {macro_prefix}_CASE_COUNT UINT8_C({len(rendered_cases)})",
        f"#define {macro_prefix}_CAPSULE_COUNT UINT8_C({capsule_count})",
        "",
        "static const pf_m4_hsd_hurt_oracle_case",
        f"{symbol_prefix}_oracle_cases[] = {{",
    ]
    for case, frame_q16, capsules, ecb in rendered_cases:
        lines.append(
            f"    {{ \"{case['id']}\", (uint16_t){case['c_submotion']}, "
            f"{c_i32(frame_q16)}, {{"
        )
        for capsule in capsules:
            lines.append(
                "        { " + ", ".join(c_i32(value) for value in capsule[:7]) +
                f", UINT8_C({capsule[7]}), UINT8_C({capsule[8]}), "
                f"UINT8_C({capsule[9]}), UINT8_C(0) }},"
            )
        ecb_values = [
            value
            for point in ("top", "bottom", "right", "left")
            for value in ecb[point]
        ]
        lines.append(
            "    }, { " + ", ".join(c_i32(value) for value in ecb_values) +
            " } },"
        )
    lines.extend(["};", ""])
    output = "\n".join(lines)
    if args.check:
        if (
            not args.output.is_file()
            or args.output.read_text(encoding="utf-8") != output
        ):
            raise SystemExit(f"stale dynamic hurt-pose oracle: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-dynamic-hurt-oracle=pass "
        f"cases={len(rendered_cases)} capsules={len(rendered_cases) * capsule_count} "
        "captures=" + ",".join(
            f"{capture_id}:{actual_digests[capture_id]}"
            for capture_id in sorted(actual_digests)
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
