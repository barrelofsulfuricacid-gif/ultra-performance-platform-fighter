#!/usr/bin/env python3
"""Generate the compact fixed-pose transition oracle from a pinned Dolphin trace."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


JOBJ_USE_QUATERNION = 1 << 17
ROTATION_SOURCE_JOINTS = (2, 3, 4, 7, 8, 13, 14, 18, 19, 21, 23, 24, 25, 38, 39, 42, 45, 46, 47)
TRANSLATION_SOURCE_JOINTS = (1, 2, 3, 18, 19, 39)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def round_integer(value: float) -> int:
    return math.floor(value + 0.5) if value >= 0.0 else math.ceil(value - 0.5)


def euler_to_quaternion(rotation: list[float]) -> tuple[float, float, float, float]:
    cx = math.cos(0.5 * rotation[0])
    cy = math.cos(0.5 * rotation[1])
    cz = math.cos(0.5 * rotation[2])
    sx = math.sin(0.5 * rotation[0])
    sy = math.sin(0.5 * rotation[1])
    sz = math.sin(0.5 * rotation[2])
    ss = sy * sz
    cc = cy * cz
    return (
        sx * cc - cx * ss,
        cz * (cx * sy) + sz * (sx * cy),
        sz * (cx * cy) - cz * (sx * sy),
        cx * cc + sx * ss,
    )


def quaternion(pose: dict[str, Any]) -> tuple[float, float, float, float]:
    rotation = pose["rotation"]
    values = (
        tuple(float(value) for value in rotation)
        if int(pose["flags"]) & JOBJ_USE_QUATERNION
        else euler_to_quaternion([float(value) for value in rotation[:3]])
    )
    length = math.sqrt(sum(value * value for value in values))
    require(length > 0.0, "zero-length captured quaternion")
    result = tuple(value / length for value in values)
    return tuple(-value for value in result) if result[3] < 0.0 else result


def closure_by_source(row: dict[str, Any]) -> dict[int, dict[str, Any]]:
    joints = row["surface_collision_memory"]["ecb_source"]["joint_closure"]
    return {int(joint["source_index"]): joint for joint in joints}


def compact_pose(row: dict[str, Any]) -> dict[str, list[list[int]]]:
    joints = closure_by_source(row)
    rotations: list[list[int]] = []
    translations: list[list[int]] = []
    for source_index in ROTATION_SOURCE_JOINTS:
        values = quaternion(joints[source_index]["pose"])
        rotations.append(
            [round_integer(value * 32767.0) for value in values[:3]]
        )
    for source_index in TRANSLATION_SOURCE_JOINTS:
        values = joints[source_index]["pose"]["translation"]
        translations.append(
            [round_integer(float(value) * 65536.0) for value in values]
        )
    return {"rotation_q15": rotations, "translation_q16": translations}


def continuation(previous: dict[str, Any], row: dict[str, Any]) -> bool:
    old = previous["surface_collision_memory"]
    new = row["surface_collision_memory"]
    frames = float(new["fighter_animation_blend_frames"])
    old_progress = float(old["fighter_animation_blend_progress"])
    progress = float(new["fighter_animation_blend_progress"])
    return (
        row["action"] == previous["action"]
        and new["fighter_motion_id"] == old["fighter_motion_id"]
        and new["fighter_animation_id"] == old["fighter_animation_id"]
        and frames > 0.0
        and 0.0 < progress - old_progress
        and old_progress < frames
        and progress <= frames
    )


def build_cases(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    previous_case_row = -2
    for index in range(1, len(rows)):
        previous = rows[index - 1]
        row = rows[index]
        if not continuation(previous, row):
            continue
        old = previous["surface_collision_memory"]
        new = row["surface_collision_memory"]
        frames = float(new["fighter_animation_blend_frames"])
        old_progress = float(old["fighter_animation_blend_progress"])
        progress = float(new["fighter_animation_blend_progress"])
        current_weight = (frames - progress) / (frames - old_progress)
        cases.append(
            {
                "trace_frame": int(row["trace_frame"]),
                "source_submotion": int(new["fighter_animation_id"]),
                "frame_q16": round_integer(float(new["fighter_animation_frame"]) * 65536.0),
                "current_weight_q16": round_integer(current_weight * 65536.0),
                "reset": index - 1 != previous_case_row,
                "prior": compact_pose(previous),
                "expected": compact_pose(row),
            }
        )
        previous_case_row = index
    return cases


def c_i16(value: int) -> str:
    return f"-INT16_C({-value})" if value < 0 else f"INT16_C({value})"


def c_i32(value: int) -> str:
    return f"-INT32_C({-value})" if value < 0 else f"INT32_C({value})"


def emit_compact(pose: dict[str, list[list[int]]]) -> str:
    rotations = ", ".join(
        "{ " + ", ".join(c_i16(value) for value in row) + " }"
        for row in pose["rotation_q15"]
    )
    translations = ", ".join(
        "{ " + ", ".join(c_i32(value) for value in row) + " }"
        for row in pose["translation_q16"]
    )
    return "{ { " + rotations + " }, { " + translations + " } }"


def emit(cases: list[dict[str, Any]], semantic_sha256: str) -> str:
    lines = [
        "/* Generated by tools/generate_ssbm_hsd_transition_oracle.py. */",
        "typedef struct pf_m4_hsd_transition_oracle_case",
        "{",
        "    uint32_t trace_frame;",
        "    uint16_t source_submotion;",
        "    uint8_t reset;",
        "    uint8_t reserved;",
        "    int32_t frame_q16;",
        "    int32_t current_weight_q16;",
        "    pf_m4_hsd_compact_pose prior;",
        "    pf_m4_hsd_compact_pose expected;",
        "} pf_m4_hsd_transition_oracle_case;",
        "",
        "static const pf_m4_hsd_transition_oracle_case",
        "    pf_m4_hsd_transition_oracle_cases[] = {",
    ]
    for case in cases:
        lines.extend(
            [
                "    {",
                f"        UINT32_C({case['trace_frame']}), UINT16_C({case['source_submotion']}),",
                f"        UINT8_C({1 if case['reset'] else 0}), UINT8_C(0),",
                f"        {c_i32(case['frame_q16'])}, {c_i32(case['current_weight_q16'])},",
                f"        {emit_compact(case['prior'])},",
                f"        {emit_compact(case['expected'])}",
                "    },",
            ]
        )
    lines.extend(
        [
            "};",
            "",
            f"#define PF_M4_HSD_TRANSITION_ORACLE_CASE_COUNT UINT32_C({len(cases)})",
            f"#define PF_M4_HSD_TRANSITION_ORACLE_SEMANTIC_SHA256 \"{semantic_sha256}\"",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    raw = args.capture.read_bytes()
    qualification = manifest["live_qualification"]
    require(
        hashlib.sha256(raw).hexdigest() == qualification["repeat_capture_sha256"],
        "repeat capture SHA-256 changed",
    )
    capture = json.loads(raw)
    cases = build_cases(capture["rows"])
    require(len(cases) == 27, f"expected 27 recurrence rows, got {len(cases)}")
    payload = json.dumps(cases, separators=(",", ":"), sort_keys=True).encode("utf-8")
    semantic_sha256 = hashlib.sha256(payload).hexdigest()
    expected = qualification.get("compact_oracle_semantic_sha256")
    if expected is not None:
        require(semantic_sha256 == expected, "compact oracle semantic SHA-256 changed")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(emit(cases, semantic_sha256), encoding="utf-8", newline="\n")
    print(
        "ssbm-hsd-transition-oracle-generation=pass "
        f"cases={len(cases)} semantic_sha256={semantic_sha256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
