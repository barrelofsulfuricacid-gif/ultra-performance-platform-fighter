#!/usr/bin/env python3
"""Qualify source-driven fighter hurt poses against a live Dolphin capture."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, replace
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from hsd_figatree import decode_figatree
from extract_ssbm_ecb_pose_tracks import extract_track
from hsd_joint_pose import (
    FIGHTER_ANIMATION_TRANSLATION_FLAG,
    evaluate_hurt_capsules,
    evaluate_joint_matrices,
    fighter_animation_flags,
    fighter_animation_slice,
    fighter_model_scale,
    read_fighter_hurt_capsules,
    read_fighter_part_layout,
    read_joint_tree,
)
from ssbm_collision import binary32, canonical_hurt_pose_f32
from ssbm_dat import read_hsd_archive
from ssbm_ecb_pose import (
    Y_Q16_PER_MELEE_UNIT,
    canonical_source_ecb,
    pose_f32,
)
from ssbm_ecb_pose import (
    canonical_sha256 as ecb_canonical_sha256,
    semantic_payload as ecb_semantic_payload,
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_qualified_capture(
    path: Path,
    expected_digest: object,
    manifest: dict[str, Any],
    qualification: dict[str, Any],
    capture_name: str,
    required_route: str | None = None,
) -> tuple[str, list[dict[str, Any]]]:
    capture_raw = path.read_bytes()
    capture_digest = sha256(capture_raw)
    require(
        capture_digest == expected_digest,
        f"unexpected {capture_name} live capture SHA-256: {capture_digest}",
    )
    capture = json.loads(capture_raw)
    require(
        capture.get("fighter") == manifest.get("fighter"),
        f"{capture_name}: fighter mismatch",
    )
    require(
        capture.get("disc", {}).get("sha256")
        == qualification.get("disc_sha256"),
        f"{capture_name}: disc mismatch",
    )
    require(
        capture.get("oracle_execution", {}).get("release_artifact_sha256")
        == qualification.get("dolphin_release_artifact_sha256"),
        f"{capture_name}: Dolphin oracle artifact mismatch",
    )
    if required_route is not None:
        require(
            capture.get(required_route) is True,
            f"{capture_name}: wrong capture route",
        )
    rows = capture.get("rows")
    require(isinstance(rows, list), f"{capture_name}: live capture rows are missing")
    return capture_digest, rows


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


def canonical_source_pose_f32(
    capsules: tuple[tuple[float, ...], ...],
    coordinate_scale_f32: float,
    axis_sign: tuple[int, int, int],
) -> tuple[tuple[float | int, ...], ...]:
    return tuple(
        (
            binary32(capsule[0] * coordinate_scale_f32 * axis_sign[0]),
            binary32(capsule[1] * coordinate_scale_f32 * axis_sign[1]),
            binary32(capsule[2] * coordinate_scale_f32 * axis_sign[2]),
            binary32(capsule[3] * coordinate_scale_f32 * axis_sign[0]),
            binary32(capsule[4] * coordinate_scale_f32 * axis_sign[1]),
            binary32(capsule[5] * coordinate_scale_f32 * axis_sign[2]),
            binary32(capsule[6] * coordinate_scale_f32),
            hurtbox_id,
            int(capsule[7]),
            int(capsule[8]),
        )
        for hurtbox_id, capsule in enumerate(capsules)
    )


def compare_hurt_pose_f32(
    row: dict[str, Any],
    source_joints: tuple[Any, ...],
    animation: Any,
    capsules: tuple[Any, ...],
    layout: Any,
    coordinate_scale_f32: float,
    axis_sign: tuple[int, int, int],
    tolerance_f32: float,
    context: str,
) -> int:
    memory = row.get("hitbox_memory")
    require(isinstance(memory, dict), f"{context}: missing hitbox memory")
    frame = memory.get("fighter_animation_frame")
    facing = row.get("facing")
    require(
        isinstance(frame, (int, float)) and facing in (-1, 1),
        f"{context}: invalid pose clock or facing",
    )
    expected = canonical_hurt_pose_f32(
        memory,
        "fighter_hurtboxes",
        "fighter_position",
        int(facing),
        coordinate_scale_f32,
        "position",
    )
    actual = canonical_source_pose_f32(
        evaluate_hurt_capsules(
            source_joints,
            animation,
            capsules,
            float(frame),
            layout,
        ),
        coordinate_scale_f32,
        axis_sign,
    )
    require(len(actual) == len(expected), f"{context}: capsule count mismatch")
    maximum_difference = 0
    for capsule_index, (left, right) in enumerate(
        zip(actual, expected, strict=True)
    ):
        require(
            left[7:] == right[7:],
            f"{context}: capsule {capsule_index} metadata mismatch",
        )
        difference = max(
            abs(left_value - right_value)
            for left_value, right_value in zip(left[:7], right[:7], strict=True)
        )
        require(
            difference <= tolerance_f32,
            f"{context}: trace={row['trace_frame']} capsule={capsule_index} "
            f"float32 difference={difference}",
        )
        maximum_difference = max(maximum_difference, difference)
    return maximum_difference


@dataclass(frozen=True)
class HurtPoseSource:
    fighter_archive: Any
    animation_raw: bytes
    fighter_root: str
    source_joints: tuple[Any, ...]
    layout: Any
    capsules: tuple[Any, ...]
    coordinate_scale_f32: float
    axis_sign: tuple[int, int, int]


def build_hurt_pose_source(
    manifest: dict[str, Any],
    fighter_raw: bytes,
    animation_raw: bytes,
    common_raw: bytes,
    model_raw: bytes,
) -> HurtPoseSource:
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
    joints = list(read_joint_tree(model_archive, str(manifest["model_root"])))
    model_scale = fighter_model_scale(fighter_archive, fighter_root)
    root = joints[0]
    joints[0] = replace(
        root,
        rotation=tuple(
            root.rotation[axis]
            + float(root_rotation_turns[axis]) * 2.0 * math.pi
            for axis in range(3)
        ),
        scale=(model_scale, model_scale, model_scale),
    )
    return HurtPoseSource(
        fighter_archive=fighter_archive,
        animation_raw=animation_raw,
        fighter_root=fighter_root,
        source_joints=tuple(joints),
        layout=read_fighter_part_layout(
            common_raw,
            int(manifest["fighter_kind"]),
        ),
        capsules=read_fighter_hurt_capsules(fighter_archive, fighter_root),
        coordinate_scale_f32=float(numerator) / float(denominator),
        axis_sign=axis_sign,
    )


def source_joint_ecb_f32(
    matrices: tuple[tuple[tuple[float, ...], ...], ...],
    source_joint_indices: tuple[int, ...],
    reference_joint_index: int | None = None,
    grounded: bool = True,
    locked_bottom_y_f32: int | None = None,
) -> dict[str, list[int]]:
    reference_x = (
        0.0 if reference_joint_index is None
        else matrices[reference_joint_index][0][3]
    )
    reference_y = (
        0.0 if reference_joint_index is None
        else matrices[reference_joint_index][1][3]
    )
    points = [
        (
            matrices[index][0][3] - reference_x,
            matrices[index][1][3] - reference_y,
        )
        for index in source_joint_indices
    ]
    left = min(point[0] for point in points)
    right = max(point[0] for point in points)
    bottom = min(point[1] for point in points)
    top = max(point[1] for point in points)
    if right - left < 10.0:
        right = 0.5 * (right - left)
        left = -right
    if top - bottom < 10.0:
        half_height = 0.5 * (top - bottom)
        middle = 0.5 * (top + bottom)
        top = middle + half_height
        bottom = middle - half_height
    right = max(right, 2.0)
    left = min(left, -2.0)
    bottom = 0.0 if grounded else max(bottom, 0.0)
    side_y = 0.5 * (bottom + top)
    if locked_bottom_y_f32 is not None:
        bottom = locked_bottom_y_f32 / Y_Q16_PER_MELEE_UNIT
        if abs(top - bottom) < 1.0:
            top += 1.0
            side_y = 0.5 * (top + bottom)
        top = max(top, 1.0)
        if top < bottom:
            top = 1.0 + bottom
        if side_y > top or side_y < bottom:
            side_y = 0.5 * (top + bottom)
        if top - side_y < 0.001 or side_y - bottom < 0.001:
            side_y = 0.5 * (top + bottom)
    return pose_f32(
        {
            "top": [0.0, top],
            "bottom": [0.0, bottom],
            "right": [right, side_y],
            "left": [left, side_y],
        }
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
        "--repeat-capture",
        type=Path,
        help="independent live capture declared by repeat_capture_sha256",
    )
    parser.add_argument(
        "--pose-branch-capture",
        type=Path,
        help="live capture declared by pose_branch_qualification.capture_sha256",
    )
    parser.add_argument(
        "--pose-branch-repeat-capture",
        type=Path,
        help=(
            "independent live capture declared by "
            "pose_branch_qualification.repeat_capture_sha256"
        ),
    )
    parser.add_argument(
        "--pose-branch-ecb-profile",
        type=Path,
        help="ECB profile declared by pose_branch_qualification.ecb_profile",
    )
    parser.add_argument(
        "--tolerance-f32",
        type=float,
        help="diagnostic override for the manifest coordinate tolerance",
    )
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    qualification = manifest.get("live_qualification")
    require(isinstance(qualification, dict), "manifest has no live qualification")
    cases = qualification.get("cases")
    require(isinstance(cases, list) and cases, "live qualification has no cases")

    repeat_digest = qualification.get("repeat_capture_sha256")
    require(
        (repeat_digest is None) == (args.repeat_capture is None),
        "repeat capture path and repeat_capture_sha256 must be declared together",
    )
    capture_specs = [
        (
            "control",
            *load_qualified_capture(
                args.capture,
                qualification.get("capture_sha256"),
                manifest,
                qualification,
                "control",
            ),
        )
    ]
    if args.repeat_capture is not None:
        capture_specs.append(
            (
                "repeat",
                *load_qualified_capture(
                    args.repeat_capture,
                    repeat_digest,
                    manifest,
                    qualification,
                    "repeat",
                ),
            )
        )

    fighter_raw = load_pinned_source(args.fighter_dat, manifest, "fighter_dat")
    animation_raw = load_pinned_source(
        args.animation_dat, manifest, "animation_dat"
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
    fighter_archive = source.fighter_archive
    fighter_root = source.fighter_root
    source_joints = source.source_joints
    layout = source.layout
    capsules = source.capsules
    coordinate_scale_f32 = source.coordinate_scale_f32
    axis_sign = source.axis_sign
    point_sets = manifest.get("joint_point_sets")
    require(isinstance(point_sets, list), "manifest joint point sets are missing")
    ecb_point_sets = [
        point_set
        for point_set in point_sets
        if isinstance(point_set, dict) and point_set.get("id") == "ecb"
    ]
    require(len(ecb_point_sets) == 1, "manifest ECB joint point set is not unique")
    raw_ecb_source_joints = ecb_point_sets[0].get("source_joint_indices")
    require(
        isinstance(raw_ecb_source_joints, list)
        and len(raw_ecb_source_joints) == 6
        and all(
            isinstance(index, int) and 0 <= index < len(source_joints)
            for index in raw_ecb_source_joints
        ),
        "manifest ECB source joints are invalid",
    )
    ecb_source_joints = tuple(int(index) for index in raw_ecb_source_joints)
    tolerance = (
        float(qualification["coordinate_tolerance_f32"])
        if args.tolerance_f32 is None
        else args.tolerance_f32
    )
    require(
        math.isfinite(tolerance) and tolerance >= 0.0,
        "coordinate tolerance must be finite and nonnegative",
    )
    animations = {
        int(case["submotion_index"]): decode_figatree(
            fighter_animation_slice(
                fighter_archive,
                animation_raw,
                fighter_root,
                int(case["submotion_index"]),
            )
        )
        for case in cases
    }
    animation_flags = {
        submotion: fighter_animation_flags(
            fighter_archive,
            fighter_root,
            submotion,
        )
        for submotion in animations
    }
    total_samples = 0
    total_capsules = 0
    maximum_difference = 0.0
    maximum_ecb_difference = 0.0
    capture_digests: list[str] = []
    for capture_name, capture_digest, rows in capture_specs:
        capture_digests.append(capture_digest)
        for case in cases:
            action = str(case["source_action"])
            submotion = int(case["submotion_index"])
            animation = animations[submotion]
            selected: list[dict[str, Any]] = []
            for row in rows:
                if row.get("action") != action:
                    continue
                memory = row.get("hitbox_memory")
                require(
                    isinstance(memory, dict),
                    f"{capture_name}/{action}: missing hitbox memory",
                )
                blend_frames = memory.get("fighter_animation_blend_frames")
                blend_progress = memory.get("fighter_animation_blend_progress")
                require(
                    isinstance(blend_frames, (int, float))
                    and isinstance(blend_progress, (int, float)),
                    f"{capture_name}/{action}: missing animation blend probe",
                )
                if float(blend_progress) >= float(blend_frames):
                    selected.append(row)
            require(
                len(selected) == int(case["expected_samples"]),
                f"{capture_name}/{action}: expected "
                f"{case['expected_samples']} qualified rows, got {len(selected)}",
            )

            case_maximum = 0.0
            for row in selected:
                memory = row["hitbox_memory"]
                frame = memory.get("fighter_animation_frame")
                facing = row.get("facing")
                difference = compare_hurt_pose_f32(
                    row,
                    source_joints,
                    animation,
                    capsules,
                    layout,
                    coordinate_scale_f32,
                    axis_sign,
                    tolerance,
                    f"{capture_name}/{action}",
                )
                case_maximum = max(case_maximum, difference)
                captured_ecb = memory.get("fighter_ecb")
                require(
                    isinstance(captured_ecb, dict),
                    f"{capture_name}/{action}: missing fighter ECB",
                )
                expected_ecb = pose_f32(
                    canonical_source_ecb(captured_ecb, int(facing))
                )
                actual_ecb = source_joint_ecb_f32(
                    evaluate_joint_matrices(
                        source_joints,
                        animation,
                        float(frame),
                    ),
                    ecb_source_joints,
                    int(manifest["blend_copy_target_source_joint_indices"][0])
                    if animation_flags[submotion]
                    & FIGHTER_ANIMATION_TRANSLATION_FLAG
                    else None,
                )
                ecb_difference = max(
                    abs(actual_ecb[point][axis] - expected_ecb[point][axis])
                    for point in ("top", "bottom", "right", "left")
                    for axis in (0, 1)
                )
                if ecb_difference > tolerance:
                    raise ValueError(
                        f"{capture_name}/{action}: trace={row['trace_frame']} "
                        f"ECB float32 difference={ecb_difference}"
                    )
                maximum_ecb_difference = max(
                    maximum_ecb_difference,
                    ecb_difference,
                )
            print(
                "ssbm-dynamic-hurt-source-case=pass "
                f"capture={capture_name} action={action} "
                f"samples={len(selected)} max_f32={case_maximum}"
            )
            total_samples += len(selected)
            total_capsules += len(selected) * len(capsules)
            maximum_difference = max(maximum_difference, case_maximum)

    print(
        "ssbm-dynamic-hurt-source=pass "
        f"captures={len(capture_specs)} cases={len(cases)} "
        f"samples={total_samples} capsules={total_capsules} "
        f"max_f32={maximum_difference} ecb_max_f32={maximum_ecb_difference} "
        f"capture_sha256={','.join(capture_digests)}"
    )

    branch_qualification = manifest.get("pose_branch_qualification")
    require(
        (branch_qualification is None)
        == (args.pose_branch_capture is None),
        "pose branch qualification and capture path must be declared together",
    )
    if branch_qualification is not None:
        require(
            isinstance(branch_qualification, dict),
            "pose branch qualification must be an object",
        )
        branch_repeat_digest = branch_qualification.get(
            "repeat_capture_sha256"
        )
        branch_capture_route = branch_qualification.get(
            "capture_route_field"
        )
        branch_action_prefix = branch_qualification.get(
            "action_family_prefix"
        )
        require(
            (branch_repeat_digest is None)
            == (args.pose_branch_repeat_capture is None),
            "pose branch repeat capture path and digest must be declared together",
        )
        require(
            isinstance(branch_capture_route, str)
            and branch_capture_route
            and isinstance(branch_action_prefix, str)
            and branch_action_prefix,
            "pose branch qualification routing metadata is invalid",
        )
        branch_capture_specs = [
            (
                "control",
                *load_qualified_capture(
                    args.pose_branch_capture,
                    branch_qualification.get("capture_sha256"),
                    manifest,
                    branch_qualification,
                    "pose-branch-control",
                    branch_capture_route,
                ),
            )
        ]
        if args.pose_branch_repeat_capture is not None:
            branch_capture_specs.append(
                (
                    "repeat",
                    *load_qualified_capture(
                        args.pose_branch_repeat_capture,
                        branch_repeat_digest,
                        manifest,
                        branch_qualification,
                        "pose-branch-repeat",
                        branch_capture_route,
                    ),
                )
            )

        branch_id = branch_qualification.get("branch_id")
        branches = manifest.get("pose_branches")
        require(isinstance(branches, list), "manifest pose branches are missing")
        matching = [
            branch for branch in branches
            if isinstance(branch, dict) and branch.get("id") == branch_id
        ]
        require(len(matching) == 1, "qualified pose branch is not unique")
        branch = matching[0]
        require(
            branch.get("sample") == "last_frame",
            "qualified pose branch must sample the last frame",
        )
        runtime_part = branch.get("runtime_part_index")
        matrix_row = branch.get("matrix_row")
        matrix_column = branch.get("matrix_column")
        require(
            isinstance(runtime_part, int)
            and 0 <= runtime_part < len(layout.source_joint_by_runtime_part)
            and matrix_row in (0, 1, 2)
            and matrix_column in (0, 1, 2),
            "qualified pose branch indices are invalid",
        )
        branch_animation = decode_figatree(
            fighter_animation_slice(
                fighter_archive,
                animation_raw,
                fighter_root,
                int(branch["submotion_index"]),
            )
        )
        require(
            len(branch_animation.nodes) == len(source_joints)
            and branch_animation.frame_count >= 1.0,
            "qualified pose branch animation is invalid",
        )
        source_joint = layout.source_joint_by_runtime_part[runtime_part]
        component = evaluate_joint_matrices(
            source_joints,
            branch_animation,
            branch_animation.frame_count - 1.0,
        )[source_joint][matrix_row][matrix_column]
        component_f32 = round(component * 65536.0)
        expected_positive = branch_qualification.get("expected_positive")
        require(
            isinstance(expected_positive, bool)
            and (component > 0.0) == expected_positive,
            f"pose branch source result differs: component_f32={component_f32}",
        )

        expected_sequence = branch_qualification.get("expected_sequence")
        require(
            isinstance(expected_sequence, list) and expected_sequence,
            "pose branch qualification sequence is missing",
        )
        ecb_profile_spec = branch_qualification.get("ecb_profile")
        require(
            (ecb_profile_spec is None)
            == (args.pose_branch_ecb_profile is None),
            "pose branch ECB profile and path must be declared together",
        )
        require(
            ecb_profile_spec is None or isinstance(ecb_profile_spec, dict),
            "pose branch ECB profile declaration is invalid",
        )
        branch_capture_digests: list[str] = []
        for capture_name, capture_digest, rows in branch_capture_specs:
            branch_capture_digests.append(capture_digest)
            observed_sequence: list[str] = []
            previous_action: object = None
            for row in rows:
                action = row.get("action")
                if action != previous_action and isinstance(action, str):
                    if action.startswith(branch_action_prefix):
                        observed_sequence.append(action)
                previous_action = action
            declared_actions = [
                str(entry["source_action"])
                for entry in expected_sequence
            ]
            require(
                observed_sequence == declared_actions,
                f"pose-branch-{capture_name}: sequence mismatch: "
                f"{observed_sequence}",
            )
            for entry in expected_sequence:
                action = str(entry["source_action"])
                selected = [row for row in rows if row.get("action") == action]
                require(
                    len(selected) == int(entry["expected_samples"]),
                    f"pose-branch-{capture_name}/{action}: expected "
                    f"{entry['expected_samples']} rows, got {len(selected)}",
                )
                require(
                    all(
                        row.get("action_value") == int(entry["action_value"])
                        for row in selected
                    ),
                    f"pose-branch-{capture_name}/{action}: action value mismatch",
                )
        ecb_semantic_digest = "none"
        if ecb_profile_spec is not None:
            track_arguments = (
                str(ecb_profile_spec["track_id"]),
                str(ecb_profile_spec["source_action"]),
                str(ecb_profile_spec["label_substring"]),
                int(ecb_profile_spec["first_displayed_frame"]),
                int(ecb_profile_spec["last_displayed_frame"]),
            )
            extracted_tracks = [
                extract_track(rows, *track_arguments)
                for _, _, rows in branch_capture_specs
            ]
            require(
                all(track == extracted_tracks[0] for track in extracted_tracks[1:]),
                "pose branch ECB repeat semantics disagree",
            )
            profile_raw = args.pose_branch_ecb_profile.read_bytes()
            require(
                sha256(profile_raw) == ecb_profile_spec.get("sha256"),
                "pose branch ECB profile digest differs",
            )
            profile = json.loads(profile_raw)
            profile_tracks = profile.get("tracks")
            require(
                isinstance(profile_tracks, list),
                "pose branch ECB profile tracks are missing",
            )
            ecb_semantic_digest = ecb_canonical_sha256(
                ecb_semantic_payload(profile_tracks)
            )
            matching_tracks = [
                track
                for track in profile_tracks
                if isinstance(track, dict)
                and track.get("id") == track_arguments[0]
            ]
            require(
                profile.get("schema") == 1
                and profile.get("scope")
                == "ssbm-action-owned-ecb-pose-tracks"
                and profile.get("capture_sha256")
                == branch_capture_digests[0]
                and profile.get("semantic_sha256") == ecb_semantic_digest
                and ecb_semantic_digest
                == ecb_profile_spec.get("semantic_sha256")
                and matching_tracks == [extracted_tracks[0]],
                "pose branch ECB profile differs from live source",
            )
        print(
            "ssbm-pose-branch-source=pass "
            f"branch={branch_id} component_f32={component_f32} "
            f"positive={int(component > 0.0)} "
            f"captures={len(branch_capture_specs)} "
            f"ecb_semantic_sha256={ecb_semantic_digest} "
            f"capture_sha256={','.join(branch_capture_digests)}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
