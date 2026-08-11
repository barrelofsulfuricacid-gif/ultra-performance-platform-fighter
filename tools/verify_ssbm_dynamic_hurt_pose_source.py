#!/usr/bin/env python3
"""Qualify source-driven fighter hurt poses against a live Dolphin capture."""

from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from hsd_figatree import decode_figatree
from hsd_joint_pose import (
    evaluate_hurt_capsules,
    fighter_animation_slice,
    fighter_model_scale,
    read_fighter_hurt_capsules,
    read_fighter_part_layout,
    read_joint_tree,
)
from ssbm_collision import canonical_hurt_pose_q16
from ssbm_dat import read_hsd_archive


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_pinned_source(
    path: Path,
    manifest: dict[str, Any],
    source_name: str,
) -> bytes:
    raw = path.read_bytes()
    expected = manifest["source_sha256"].get(source_name)
    actual = sha256(raw)
    require(actual == expected, f"unexpected {source_name} SHA-256: {actual}")
    return raw


def canonical_source_pose_q16(
    capsules: tuple[tuple[float, ...], ...],
    coordinate_scale_q16: float,
    axis_sign: tuple[int, int, int],
) -> tuple[tuple[int, ...], ...]:
    return tuple(
        (
            round(capsule[0] * coordinate_scale_q16 * axis_sign[0]),
            round(capsule[1] * coordinate_scale_q16 * axis_sign[1]),
            round(capsule[2] * coordinate_scale_q16 * axis_sign[2]),
            round(capsule[3] * coordinate_scale_q16 * axis_sign[0]),
            round(capsule[4] * coordinate_scale_q16 * axis_sign[1]),
            round(capsule[5] * coordinate_scale_q16 * axis_sign[2]),
            round(capsule[6] * coordinate_scale_q16),
            hurtbox_id,
            int(capsule[7]),
            int(capsule[8]),
        )
        for hurtbox_id, capsule in enumerate(capsules)
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("fighter_dat", type=Path)
    parser.add_argument("animation_dat", type=Path)
    parser.add_argument("common_dat", type=Path)
    parser.add_argument("model_dat", type=Path)
    parser.add_argument(
        "--tolerance-q16",
        type=int,
        help="diagnostic override for the manifest coordinate tolerance",
    )
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    qualification = manifest.get("live_qualification")
    require(isinstance(qualification, dict), "manifest has no live qualification")
    cases = qualification.get("cases")
    require(isinstance(cases, list) and cases, "live qualification has no cases")

    capture_raw = args.capture.read_bytes()
    capture_digest = sha256(capture_raw)
    require(
        capture_digest == qualification.get("capture_sha256"),
        f"unexpected live capture SHA-256: {capture_digest}",
    )
    capture = json.loads(capture_raw)
    require(capture.get("fighter") == manifest.get("fighter"), "fighter mismatch")
    require(
        capture.get("disc", {}).get("sha256")
        == qualification.get("disc_sha256"),
        "disc mismatch",
    )
    require(
        capture.get("oracle_execution", {}).get("release_artifact_sha256")
        == qualification.get("dolphin_release_artifact_sha256"),
        "Dolphin oracle artifact mismatch",
    )

    fighter_raw = load_pinned_source(args.fighter_dat, manifest, "fighter_dat")
    animation_raw = load_pinned_source(
        args.animation_dat, manifest, "animation_dat"
    )
    common_raw = load_pinned_source(args.common_dat, manifest, "common_dat")
    model_raw = load_pinned_source(args.model_dat, manifest, "model_dat")
    fighter_archive = read_hsd_archive(fighter_raw)
    model_archive = read_hsd_archive(model_raw)
    fighter_root = str(manifest["fighter_root"])
    conversion = manifest.get("pose_conversion")
    require(isinstance(conversion, dict), "manifest has no pose conversion")
    root_rotation_turns = conversion.get("root_rotation_turns")
    axis_sign_raw = conversion.get("axis_sign")
    numerator = conversion.get("source_to_sim_numerator")
    denominator = conversion.get("source_to_sim_denominator")
    require(
        isinstance(root_rotation_turns, list)
        and len(root_rotation_turns) == 3
        and all(isinstance(value, (int, float)) for value in root_rotation_turns)
        and isinstance(axis_sign_raw, list)
        and len(axis_sign_raw) == 3
        and all(value in (-1, 1) for value in axis_sign_raw)
        and isinstance(numerator, int)
        and isinstance(denominator, int)
        and numerator > 0
        and denominator > 0,
        "manifest pose conversion is invalid",
    )
    axis_sign = tuple(int(value) for value in axis_sign_raw)
    require(axis_sign == (1, -1, 1), "live canonicalizer requires sim axis convention")
    coordinate_scale_q16 = 65536.0 * float(numerator) / float(denominator)
    joints = list(read_joint_tree(model_archive, str(manifest["model_root"])))
    model_scale = fighter_model_scale(fighter_archive, fighter_root)
    root = joints[0]
    joints[0] = replace(
        root,
        rotation=tuple(
            root.rotation[axis] + float(root_rotation_turns[axis]) * 2.0 * math.pi
            for axis in range(3)
        ),
        scale=(model_scale, model_scale, model_scale),
    )
    source_joints = tuple(joints)
    layout = read_fighter_part_layout(common_raw, int(manifest["fighter_kind"]))
    capsules = read_fighter_hurt_capsules(fighter_archive, fighter_root)
    rows = capture.get("rows")
    require(isinstance(rows, list), "live capture rows are missing")

    tolerance = (
        int(qualification["coordinate_tolerance_q16"])
        if args.tolerance_q16 is None
        else args.tolerance_q16
    )
    require(tolerance >= 0, "coordinate tolerance must be nonnegative")
    total_samples = 0
    total_capsules = 0
    maximum_difference = 0
    for case in cases:
        action = str(case["source_action"])
        submotion = int(case["submotion_index"])
        animation = decode_figatree(
            fighter_animation_slice(
                fighter_archive, animation_raw, fighter_root, submotion
            )
        )
        selected: list[dict[str, Any]] = []
        for row in rows:
            if row.get("action") != action:
                continue
            memory = row.get("hitbox_memory")
            require(isinstance(memory, dict), f"{action}: missing hitbox memory")
            blend_frames = memory.get("fighter_animation_blend_frames")
            blend_progress = memory.get("fighter_animation_blend_progress")
            require(
                isinstance(blend_frames, (int, float))
                and isinstance(blend_progress, (int, float)),
                f"{action}: missing animation blend probe",
            )
            if float(blend_progress) >= float(blend_frames):
                selected.append(row)
        require(
            len(selected) == int(case["expected_samples"]),
            f"{action}: expected {case['expected_samples']} qualified rows, "
            f"got {len(selected)}",
        )

        case_maximum = 0
        for row in selected:
            memory = row["hitbox_memory"]
            frame = memory.get("fighter_animation_frame")
            facing = row.get("facing")
            require(
                isinstance(frame, (int, float)) and facing in (-1, 1),
                f"{action}: invalid pose clock or facing",
            )
            expected = canonical_hurt_pose_q16(
                memory,
                "fighter_hurtboxes",
                "fighter_position",
                int(facing),
                coordinate_scale_q16,
                "position",
            )
            actual = canonical_source_pose_q16(
                evaluate_hurt_capsules(
                    source_joints,
                    animation,
                    capsules,
                    float(frame),
                    layout,
                ),
                coordinate_scale_q16,
                axis_sign,
            )
            require(len(actual) == len(expected), f"{action}: capsule count mismatch")
            for capsule_index, (left, right) in enumerate(
                zip(actual, expected, strict=True)
            ):
                require(
                    left[7:] == right[7:],
                    f"{action}: capsule {capsule_index} metadata mismatch",
                )
                difference = max(
                    abs(left_value - right_value)
                    for left_value, right_value in zip(
                        left[:7], right[:7], strict=True
                    )
                )
                if difference > tolerance:
                    raise ValueError(
                        f"{action}: trace={row['trace_frame']} "
                        f"capsule={capsule_index} Q16 difference={difference}"
                    )
                case_maximum = max(case_maximum, difference)
        print(
            "ssbm-dynamic-hurt-source-case=pass "
            f"action={action} samples={len(selected)} max_q16={case_maximum}"
        )
        total_samples += len(selected)
        total_capsules += len(selected) * len(capsules)
        maximum_difference = max(maximum_difference, case_maximum)

    print(
        "ssbm-dynamic-hurt-source=pass "
        f"cases={len(cases)} samples={total_samples} capsules={total_capsules} "
        f"max_q16={maximum_difference} capture_sha256={capture_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
