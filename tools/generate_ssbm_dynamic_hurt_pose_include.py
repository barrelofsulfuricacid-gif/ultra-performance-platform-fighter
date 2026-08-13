#!/usr/bin/env python3
"""Generate compact deterministic HSD pose data from owner-supplied DATs."""

from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import json
import math
from pathlib import Path
import re
from typing import Any

from hsd_figatree import FigaTree, decode_figatree
from hsd_joint_pose import (
    JOBJ_CLASSICAL_SCALE,
    SUPPORTED_TRACK_TYPES,
    evaluate_joint_matrices,
    fighter_animation_slice,
    fighter_model_scale,
    read_fighter_hurt_capsules,
    read_fighter_part_layout,
    read_joint_tree,
    required_joint_indices,
)
from ssbm_dat import fighter_wait_animations, read_hsd_archive


Q16_ONE = 65536
ROTATION_TRACKS = frozenset({1, 2, 3})


def q16(value: float) -> int:
    result = round(value * Q16_ONE)
    if not -(1 << 31) <= result < (1 << 31):
        raise ValueError("Q16 value is out of range")
    return result


def rotation_turns_q16(radians: float) -> int:
    return q16(radians / (2.0 * math.pi))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def c_i32(value: int) -> str:
    return f"INT32_C({value})" if value >= 0 else f"-INT32_C({-value})"


def load_manifest(path: Path) -> dict[str, Any]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if (
        manifest.get("schema") != 1
        or manifest.get("scope") != "ssbm-dynamic-hsd-hurt-pose-import"
        or not isinstance(manifest.get("motions"), list)
        or not manifest["motions"]
    ):
        raise ValueError("invalid dynamic HSD hurt-pose manifest")
    return manifest


def build_payload(
    manifest: dict[str, Any],
    fighter_raw: bytes,
    animation_raw: bytes,
    common_raw: bytes,
    model_raw: bytes,
    static_pose_payloads: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    fighter_root = str(manifest["fighter_root"])
    model = read_hsd_archive(model_raw)
    fighter = read_hsd_archive(fighter_raw)
    joints = read_joint_tree(model, str(manifest["model_root"]))
    layout = read_fighter_part_layout(common_raw, int(manifest["fighter_kind"]))
    capsules = read_fighter_hurt_capsules(fighter, fighter_root)
    raw_point_sets = manifest.get("joint_point_sets", [])
    if not isinstance(raw_point_sets, list):
        raise ValueError("manifest joint_point_sets must be a list")
    capture_constraint = manifest.get("capture_constraint")
    if (
        not isinstance(capture_constraint, dict)
        or capture_constraint.get("fighter_root_source_joint_index") != 1
        or capture_constraint.get("model_offset_source_joint_index") != 2
        or capture_constraint.get("holder_attachment_source_joint_index") != 61
        or capture_constraint.get("world_y_source_to_sim_numerator") != 11
        or capture_constraint.get("world_y_source_to_sim_denominator") != 62
    ):
        raise ValueError("manifest capture constraint is invalid")
    point_set_specs: list[tuple[str, tuple[int, ...]]] = []
    point_set_ids: set[str] = set()
    for point_set in raw_point_sets:
        if not isinstance(point_set, dict):
            raise ValueError("joint point set must be an object")
        point_set_id = point_set.get("id")
        raw_indices = point_set.get("source_joint_indices")
        if (
            not isinstance(point_set_id, str)
            or re.fullmatch(r"[a-z][a-z0-9_]*", point_set_id) is None
            or point_set_id in point_set_ids
            or not isinstance(raw_indices, list)
            or not raw_indices
            or any(
                not isinstance(index, int)
                or isinstance(index, bool)
                or not 0 <= index < len(joints)
                for index in raw_indices
            )
        ):
            raise ValueError("manifest joint point set is invalid")
        point_set_ids.add(point_set_id)
        point_set_specs.append((point_set_id, tuple(raw_indices)))
    required = required_joint_indices(
        joints,
        capsules,
        layout,
        (
            source_index
            for _, source_indices in point_set_specs
            for source_index in source_indices
        ),
    )
    compact_by_source = {source: index for index, source in enumerate(required)}
    model_scale = fighter_model_scale(fighter, fighter_root)
    conversion = manifest.get("pose_conversion")
    if not isinstance(conversion, dict):
        raise ValueError("manifest is missing pose conversion")
    root_rotation_turns = conversion.get("root_rotation_turns")
    axis_sign = conversion.get("axis_sign")
    numerator = conversion.get("source_to_sim_numerator")
    denominator = conversion.get("source_to_sim_denominator")
    if (
        not isinstance(root_rotation_turns, list)
        or len(root_rotation_turns) != 3
        or not all(isinstance(value, (int, float)) for value in root_rotation_turns)
        or not isinstance(axis_sign, list)
        or len(axis_sign) != 3
        or any(value not in (-1, 1) for value in axis_sign)
        or not isinstance(numerator, int)
        or not isinstance(denominator, int)
        or numerator <= 0
        or denominator <= 0
    ):
        raise ValueError("manifest pose conversion is invalid")

    converted_joints = list(joints)
    root = converted_joints[0]
    converted_joints[0] = replace(
        root,
        rotation=tuple(
            root.rotation[axis]
            + float(root_rotation_turns[axis]) * 2.0 * math.pi
            for axis in range(3)
        ),
        scale=(model_scale, model_scale, model_scale),
    )
    converted_joint_tuple = tuple(converted_joints)
    base_matrices = evaluate_joint_matrices(
        joints,
        FigaTree(frame_count=1.0, nodes=((),) * len(joints)),
        0.0,
    )
    root_index = int(capture_constraint["fighter_root_source_joint_index"])
    offset_index = int(capture_constraint["model_offset_source_joint_index"])
    capture_root_offset_source_q16 = [
        q16(base_matrices[root_index][axis][3] -
            base_matrices[offset_index][axis][3])
        for axis in range(3)
    ]

    joint_rows: list[dict[str, Any]] = []
    for compact_index, source_index in enumerate(required):
        source = joints[source_index]
        parent = source.parent_index
        while parent >= 0 and parent not in compact_by_source:
            parent = joints[parent].parent_index
        rotation = [rotation_turns_q16(value) for value in source.rotation]
        scale = [q16(value) for value in source.scale]
        if compact_index == 0:
            rotation = [
                value + q16(float(root_rotation_turns[axis]))
                for axis, value in enumerate(rotation)
            ]
            scale = [q16(model_scale)] * 3
        joint_rows.append(
            {
                "source_index": source_index,
                "parent_index": -1 if parent < 0 else compact_by_source[parent],
                "classical_scale": bool(source.flags & JOBJ_CLASSICAL_SCALE),
                "rotation": rotation,
                "scale": scale,
                "translation": [q16(value) for value in source.translation],
            }
        )

    static_local_poses: list[dict[str, Any]] = []
    for pose_spec in manifest.get("static_local_poses", []):
        pose_id = pose_spec.get("id")
        pose_payload = static_pose_payloads.get(str(pose_id))
        if (
            not isinstance(pose_id, str)
            or re.fullmatch(r"[a-z][a-z0-9_]*", pose_id) is None
            or not isinstance(pose_payload, dict)
            or pose_payload.get("schema") != 1
            or pose_payload.get("scope") != "ssbm-static-hsd-joint-pose"
            or pose_payload.get("pose") != pose_id
            or pose_payload.get("target_parts_count") != len(joints)
            or not isinstance(pose_payload.get("joints"), list)
            or len(pose_payload["joints"]) != len(joints) - 1
        ):
            raise ValueError("invalid static local pose")
        by_part = {
            row.get("target_part_index"): row
            for row in pose_payload["joints"]
            if isinstance(row, dict)
        }
        pose_rows: list[dict[str, Any]] = []
        for compact_index, source_index in enumerate(required):
            if source_index == 0:
                base = joint_rows[compact_index]
                pose_rows.append(
                    {
                        "rotation": [*base["rotation"], 0],
                        "scale": list(base["scale"]),
                        "translation": list(base["translation"]),
                        "use_quaternion": 0,
                    }
                )
                continue
            source = by_part.get(source_index)
            if source is None:
                raise ValueError(
                    f"static local pose {pose_id} omits part {source_index}"
                )
            for field in ("rotation", "scale", "translation"):
                values = source.get(field)
                if not isinstance(values, list) or len(values) != 3:
                    raise ValueError(f"invalid static local pose {field}")
            pose_rows.append(
                {
                    "rotation": [
                        rotation_turns_q16(float(value))
                        for value in source["rotation"]
                    ] + [0],
                    "scale": [q16(float(value)) for value in source["scale"]],
                    "translation": [
                        q16(float(value)) for value in source["translation"]
                    ],
                    "use_quaternion": 0,
                }
            )
        static_local_poses.append({"id": pose_id, "joints": pose_rows})

    key_rows: list[dict[str, int]] = []
    track_rows: list[dict[str, int]] = []
    motion_rows: list[dict[str, Any]] = []
    compact_track_rows: list[dict[str, int]] = []
    compact_motion_count = 0
    for motion_spec in manifest["motions"]:
        compact_blend = motion_spec.get("compact_blend", False)
        if not isinstance(compact_blend, bool):
            raise ValueError("motion compact_blend must be boolean")
        action_frame_offset = motion_spec.get("action_frame_offset")
        if action_frame_offset is not None and (
            not isinstance(action_frame_offset, int)
            or isinstance(action_frame_offset, bool)
            or action_frame_offset not in (-1, 0)
        ):
            raise ValueError("motion action_frame_offset must be -1 or 0")
        submotion = int(motion_spec["submotion_index"])
        tree = decode_figatree(
            fighter_animation_slice(fighter, animation_raw, fighter_root, submotion)
        )
        if len(tree.nodes) > len(joints):
            raise ValueError("animation has more joints than the fighter model")
        track_offset = len(track_rows)
        for source_index in required:
            # Common victim motions such as Thrown* legitimately omit the
            # model's unused trailing joints. HSD leaves those joints at their
            # base pose; only authored FigaTree nodes contribute tracks.
            if source_index >= len(tree.nodes):
                continue
            compact_index = compact_by_source[source_index]
            for track in tree.nodes[source_index]:
                if track.track_type not in SUPPORTED_TRACK_TYPES:
                    continue
                key_offset = len(key_rows)
                for key in track.keys:
                    convert = (
                        rotation_turns_q16
                        if track.track_type in ROTATION_TRACKS
                        else q16
                    )
                    key_rows.append(
                        {
                            "frame": q16(key.frame),
                            "value": convert(key.value),
                            "tangent": convert(key.tangent),
                            "interpolation": key.interpolation,
                        }
                    )
                track_rows.append(
                    {
                        "key_offset": key_offset,
                        "key_count": len(track.keys),
                        "start_frame": track.start_frame,
                        "joint_index": compact_index,
                        "track_type": track.track_type,
                    }
                )
        motion_rows.append(
            {
                "submotion": str(motion_spec["c_submotion"]),
                "track_offset": track_offset,
                "track_count": len(track_rows) - track_offset,
                "frame_count": round(tree.frame_count),
                "action_frame_offset": action_frame_offset,
            }
        )
        if compact_blend:
            compact_track_rows.extend(track_rows[track_offset:])
            compact_motion_count += 1
    if compact_motion_count == 0:
        raise ValueError("at least one motion must own compact blend state")

    wait_animation_rows: list[dict[str, Any]] = []
    wait_animation_spec = manifest.get("wait_animation_table")
    if wait_animation_spec is not None:
        if not isinstance(wait_animation_spec, dict):
            raise ValueError("manifest wait animation table must be an object")
        expected_weight_total = wait_animation_spec.get(
            "expected_weight_total"
        )
        if (
            not isinstance(expected_weight_total, int)
            or isinstance(expected_weight_total, bool)
            or expected_weight_total <= 0
        ):
            raise ValueError("wait animation weight total is invalid")
        motion_names = {
            int(spec["submotion_index"]): str(spec["c_submotion"])
            for spec in manifest["motions"]
        }
        source_wait_animations = fighter_wait_animations(
            fighter, fighter_root
        )
        if not source_wait_animations:
            raise ValueError("fighter has no wait animation table")
        for row in source_wait_animations:
            try:
                c_submotion = motion_names[row.animation_id]
            except KeyError as error:
                raise ValueError(
                    "wait animation motion was not imported: "
                    f"{row.animation_id}"
                ) from error
            wait_animation_rows.append(
                {
                    "submotion": c_submotion,
                    "weight": row.weight,
                    "blend_frames": row.blend_frames,
                    "blend_parameter": row.blend_parameter,
                }
            )
        weight_total = sum(row["weight"] for row in wait_animation_rows)
        if weight_total != expected_weight_total:
            raise ValueError(
                "unexpected wait animation weight total: "
                f"expected={expected_weight_total} actual={weight_total}"
            )

    capsule_rows: list[dict[str, Any]] = []
    for hurtbox_id, capsule in enumerate(capsules):
        source_index = layout.source_joint_by_runtime_part[capsule.bone_index]
        if source_index not in compact_by_source:
            raise ValueError("hurt capsule source joint is not parent-closed")
        capsule_rows.append(
            {
                "joint_index": compact_by_source[source_index],
                "hurtbox_id": hurtbox_id,
                "height": capsule.height,
                "grabbable": capsule.grabbable,
                "offset_a": [q16(value) for value in capsule.offset_a],
                "offset_b": [q16(value) for value in capsule.offset_b],
                "radius": q16(capsule.radius),
            }
        )
    point_set_rows = [
        {
            "id": point_set_id,
            "source_joint_indices": list(source_indices),
            "joint_indices": [
                compact_by_source[source_index]
                for source_index in source_indices
            ],
        }
        for point_set_id, source_indices in point_set_specs
    ]
    raw_copy_target = manifest.get("blend_copy_target_source_joint_indices", [])
    if (
        not isinstance(raw_copy_target, list)
        or any(
            not isinstance(source_index, int)
            or isinstance(source_index, bool)
            or source_index not in compact_by_source
            for source_index in raw_copy_target
        )
        or len(set(raw_copy_target)) != len(raw_copy_target)
    ):
        raise ValueError("manifest blend copy-target joints are invalid")
    copy_target_joint_indices = sorted(
        compact_by_source[source_index] for source_index in raw_copy_target
    )
    branch_rows: list[dict[str, Any]] = []
    raw_branches = manifest.get("pose_branches", [])
    if not isinstance(raw_branches, list):
        raise ValueError("manifest pose_branches must be a list")
    branch_ids: set[str] = set()
    for branch in raw_branches:
        if not isinstance(branch, dict):
            raise ValueError("pose branch must be an object")
        branch_id = branch.get("id")
        submotion = branch.get("submotion_index")
        runtime_part = branch.get("runtime_part_index")
        matrix_row = branch.get("matrix_row")
        matrix_column = branch.get("matrix_column")
        if (
            not isinstance(branch_id, str)
            or re.fullmatch(r"[a-z][a-z0-9_]*", branch_id) is None
            or branch_id in branch_ids
            or not isinstance(submotion, int)
            or not isinstance(runtime_part, int)
            or not 0 <= runtime_part < len(layout.source_joint_by_runtime_part)
            or matrix_row not in (0, 1, 2)
            or matrix_column not in (0, 1, 2)
            or branch.get("sample") != "last_frame"
        ):
            raise ValueError("manifest pose branch is invalid")
        branch_ids.add(branch_id)
        source_joint = layout.source_joint_by_runtime_part[runtime_part]
        if not 0 <= source_joint < len(joints):
            raise ValueError("pose branch runtime part has no source joint")
        tree = decode_figatree(
            fighter_animation_slice(
                fighter, animation_raw, fighter_root, submotion
            )
        )
        if len(tree.nodes) != len(joints) or tree.frame_count < 1.0:
            raise ValueError("pose branch animation/model data is invalid")
        frame = tree.frame_count - 1.0
        component = evaluate_joint_matrices(
            converted_joint_tuple, tree, frame
        )[source_joint][matrix_row][matrix_column]
        component_q16 = q16(component)
        branch_rows.append(
            {
                "id": branch_id,
                "submotion_index": submotion,
                "runtime_part_index": runtime_part,
                "source_joint_index": source_joint,
                "frame_q16": q16(frame),
                "matrix_row": matrix_row,
                "matrix_column": matrix_column,
                "component_q16": component_q16,
                "positive": component > 0.0,
            }
        )
    return {
        "source_joint_count": len(joints),
        "runtime_part_count": len(layout.source_joint_by_runtime_part),
        "model_scale_q16": q16(model_scale),
        "source_to_sim_numerator": numerator,
        "source_to_sim_denominator": denominator,
        "capture_root_offset_source_q16": capture_root_offset_source_q16,
        "capture_world_y_source_to_sim_numerator": int(
            capture_constraint["world_y_source_to_sim_numerator"]
        ),
        "capture_world_y_source_to_sim_denominator": int(
            capture_constraint["world_y_source_to_sim_denominator"]
        ),
        "axis_sign": axis_sign,
        "joints": joint_rows,
        "static_local_poses": static_local_poses,
        "motions": motion_rows,
        "wait_animations": wait_animation_rows,
        "compact_motion_count": compact_motion_count,
        "tracks": track_rows,
        "keys": key_rows,
        "rotation_joint_indices": sorted(
            {
                row["joint_index"]
                for row in compact_track_rows
                if row["track_type"] in (1, 2, 3)
            }
        ),
        "translation_joint_indices": sorted(
            {
                row["joint_index"]
                for row in compact_track_rows
                if row["track_type"] in (5, 6, 7)
            }
        ),
        "copy_target_joint_indices": copy_target_joint_indices,
        "capsules": capsule_rows,
        "joint_point_sets": point_set_rows,
        "pose_branches": branch_rows,
    }


def validate_expected(manifest: dict[str, Any], payload: dict[str, Any]) -> str:
    expected = manifest.get("expected")
    if not isinstance(expected, dict):
        raise ValueError("manifest is missing expected counts")
    counts = {
        "source_joint_count": payload["source_joint_count"],
        "runtime_part_count": payload["runtime_part_count"],
        "joint_count": len(payload["joints"]),
        "motion_count": len(payload["motions"]),
        "wait_animation_count": len(payload["wait_animations"]),
        "compact_motion_count": payload["compact_motion_count"],
        "track_count": len(payload["tracks"]),
        "key_count": len(payload["keys"]),
        "capsule_count": len(payload["capsules"]),
        "joint_point_set_count": len(payload["joint_point_sets"]),
        "pose_branch_count": len(payload["pose_branches"]),
        "rotation_joint_count": len(payload["rotation_joint_indices"]),
        "translation_joint_count": len(payload["translation_joint_indices"]),
        "copy_target_joint_count": len(payload["copy_target_joint_indices"]),
        "static_local_pose_count": len(payload["static_local_poses"]),
    }
    for name, actual in counts.items():
        if expected.get(name) != actual:
            raise ValueError(
                f"unexpected {name}: expected={expected.get(name)!r} actual={actual}"
            )
    digest = sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    )
    pinned = expected.get("data_sha256")
    if pinned is not None and pinned != digest:
        raise ValueError(
            f"unexpected generated data SHA-256: expected={pinned} actual={digest}"
        )
    return digest


def render(manifest: dict[str, Any], payload: dict[str, Any], digest: str) -> str:
    prefix = str(manifest["symbol_prefix"])
    lines = [
        "/* Generated by tools/generate_ssbm_dynamic_hurt_pose_include.py. */",
        f"/* Canonical decoded data SHA-256: {digest} */",
        "",
        f"static const int32_t {prefix}_capture_root_offset_source_q16[3] = {{",
        "    " + ", ".join(
            c_i32(value) for value in payload["capture_root_offset_source_q16"]
        ),
        "};",
        f"#define {prefix}_capture_world_y_source_to_sim_numerator \\",
        f"    INT32_C({payload['capture_world_y_source_to_sim_numerator']})",
        f"#define {prefix}_capture_world_y_source_to_sim_denominator \\",
        f"    INT32_C({payload['capture_world_y_source_to_sim_denominator']})",
        "",
        f"static const hsd_joint {prefix}_joints[] = {{",
    ]
    for row in payload["joints"]:
        lines.append(
            "    { { " + ", ".join(c_i32(v) for v in row["rotation"]) +
            " }, { " + ", ".join(c_i32(v) for v in row["scale"]) +
            " }, { " + ", ".join(c_i32(v) for v in row["translation"]) +
            f" }}, INT8_C({row['parent_index']}), UINT8_C({int(row['classical_scale'])}), "
            "{ UINT8_C(0), UINT8_C(0) } },"
        )
    lines.extend(["};", "", f"static const hsd_motion {prefix}_motions[] = {{"])
    for row in payload["motions"]:
        lines.append(
            f"    {{ (uint16_t){row['submotion']}, UINT16_C({row['track_offset']}), "
            f"UINT16_C({row['track_count']}), UINT16_C({row['frame_count']}) }},"
        )
    action_clock_rows = [
        row
        for row in payload["motions"]
        if row["action_frame_offset"] is not None
    ]
    lines.extend(
        [
            "};",
            "",
            f"static int {prefix}_action_frame_offset(",
            "    uint16_t source_submotion,",
            "    int8_t *out_offset)",
            "{",
            "    if (out_offset == NULL)",
            "    {",
            "        return 0;",
            "    }",
            "    switch (source_submotion)",
            "    {",
        ]
    )
    for row in action_clock_rows:
        lines.extend(
            [
                f"    case (uint16_t){row['submotion']}:",
                f"        *out_offset = INT8_C({row['action_frame_offset']});",
                "        return 1;",
            ]
        )
    lines.extend(
        [
            "    default:",
            "        return 0;",
            "    }",
            "}",
            "",
            f"static const hsd_wait_animation {prefix}_wait_animations[] = {{",
        ]
    )
    for row in payload["wait_animations"]:
        lines.append(
            f"    {{ (uint16_t){row['submotion']}, UINT8_C({row['weight']}), "
            f"UINT8_C({row['blend_frames']}), "
            f"UINT8_C({row['blend_parameter']}), "
            "{ UINT8_C(0), UINT8_C(0), UINT8_C(0) } },"
        )
    lines.extend(["};", "", f"static const hsd_track {prefix}_tracks[] = {{"])
    for row in payload["tracks"]:
        lines.append(
            f"    {{ UINT16_C({row['key_offset']}), UINT16_C({row['key_count']}), "
            f"INT16_C({row['start_frame']}), UINT8_C({row['joint_index']}), "
            f"UINT8_C({row['track_type']}) }},"
        )
    lines.extend(["};", "", f"static const hsd_key {prefix}_keys[] = {{"])
    for row in payload["keys"]:
        lines.append(
            "    { " + ", ".join(c_i32(row[name]) for name in ("frame", "value", "tangent")) +
            f", UINT8_C({row['interpolation']}), "
            "{ UINT8_C(0), UINT8_C(0), UINT8_C(0) } },"
        )
    lines.extend(
        ["};", "", f"static const hsd_hurt_capsule {prefix}_capsules[] = {{"]
    )
    for row in payload["capsules"]:
        lines.append(
            "    { { " + ", ".join(c_i32(v) for v in row["offset_a"]) +
            " }, { " + ", ".join(c_i32(v) for v in row["offset_b"]) +
            f" }}, {c_i32(row['radius'])}, UINT8_C({row['joint_index']}), "
            f"UINT8_C({row['hurtbox_id']}), UINT8_C({row['height']}), "
            f"UINT8_C({row['grabbable']}) }},"
    )
    lines.extend(["};", ""])
    for pose in payload["static_local_poses"]:
        lines.append(
            f"static const hsd_local_pose {prefix}_{pose['id']}_pose[] = {{"
        )
        for row in pose["joints"]:
            lines.append(
                "    { { "
                + ", ".join(c_i32(value) for value in row["rotation"])
                + " }, { "
                + ", ".join(c_i32(value) for value in row["scale"])
                + " }, { "
                + ", ".join(c_i32(value) for value in row["translation"])
                + f" }}, UINT8_C({row['use_quaternion']}), "
                "{ UINT8_C(0), UINT8_C(0), UINT8_C(0) } },"
            )
        lines.extend(["};", ""])
    for channel in ("rotation", "translation", "copy_target"):
        indices = payload[f"{channel}_joint_indices"]
        lines.extend(
            [
                f"static const uint8_t {prefix}_{channel}_joint_indices[] = {{",
                "    " + ", ".join(f"UINT8_C({value})" for value in indices),
                "};",
                "",
            ]
        )
    for row in payload["joint_point_sets"]:
        point_set_prefix = f"{prefix}_{row['id']}_joint_indices"
        lines.extend(
            [
                f"static const uint8_t {point_set_prefix}[] = {{",
                "    " + ", ".join(
                    f"UINT8_C({value})" for value in row["joint_indices"]
                ),
                "};",
                "",
            ]
        )
    lines.extend(
        [
            f"static const hsd_pose_data {prefix}_data = {{",
            f"    {prefix}_joints,",
            f"    {prefix}_motions,",
            f"    {prefix}_tracks,",
            f"    {prefix}_keys,",
            f"    {prefix}_capsules,",
            f"    {prefix}_rotation_joint_indices,",
            f"    {prefix}_translation_joint_indices,",
            f"    {prefix}_copy_target_joint_indices,",
            f"    UINT16_C({len(payload['keys'])}),",
            f"    UINT16_C({len(payload['tracks'])}),",
            f"    UINT8_C({len(payload['joints'])}),",
            f"    UINT8_C({len(payload['motions'])}),",
            f"    UINT8_C({len(payload['capsules'])}),",
            f"    UINT8_C({len(payload['rotation_joint_indices'])}),",
            f"    UINT8_C({len(payload['translation_joint_indices'])}),",
            f"    UINT8_C({len(payload['copy_target_joint_indices'])}),",
            "    UINT8_C(0),",
            f"    INT32_C({payload['source_to_sim_numerator']}),",
            f"    INT32_C({payload['source_to_sim_denominator']}),",
            "    { " + ", ".join(
                f"INT8_C({value})" for value in payload["axis_sign"]
            ) + " },",
            "    UINT8_C(0)",
            "};",
            "",
        ]
    )
    for row in payload["pose_branches"]:
        branch_prefix = f"{prefix}_pose_branch_{row['id']}"
        lines.extend(
            [
                f"#define {branch_prefix}_component_q16 \\",
                f"    {c_i32(row['component_q16'])}",
                f"#define {branch_prefix} \\",
                f"    UINT8_C({int(row['positive'])})",
                "",
            ]
        )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("fighter_dat", type=Path)
    parser.add_argument("animation_dat", type=Path)
    parser.add_argument("common_dat", type=Path)
    parser.add_argument("model_dat", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    static_pose_payloads: dict[str, dict[str, Any]] = {}
    for spec in manifest.get("static_local_poses", []):
        if not isinstance(spec, dict) or not isinstance(spec.get("path"), str):
            raise SystemExit("invalid static_local_poses manifest entry")
        path = args.manifest.parent / spec["path"]
        raw = path.read_bytes()
        actual = sha256(raw)
        if actual != spec.get("sha256"):
            raise SystemExit(
                f"unexpected static pose SHA-256: expected={spec.get('sha256')} "
                f"actual={actual}"
            )
        static_pose_payloads[str(spec.get("id"))] = json.loads(raw)
    sources = {
        "fighter_dat": args.fighter_dat.read_bytes(),
        "animation_dat": args.animation_dat.read_bytes(),
        "common_dat": args.common_dat.read_bytes(),
        "model_dat": args.model_dat.read_bytes(),
    }
    for name, data in sources.items():
        expected = manifest["source_sha256"].get(name)
        actual = sha256(data)
        if expected != actual:
            raise SystemExit(
                f"unexpected {name} SHA-256: expected={expected} actual={actual}"
            )
    payload = build_payload(
        manifest,
        sources["fighter_dat"],
        sources["animation_dat"],
        sources["common_dat"],
        sources["model_dat"],
        static_pose_payloads,
    )
    digest = validate_expected(manifest, payload)
    output = render(manifest, payload, digest)
    if args.check:
        if (
            not args.output.is_file()
            or args.output.read_text(encoding="utf-8") != output
        ):
            raise SystemExit(f"stale dynamic HSD hurt-pose include: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-dynamic-hurt-pose-import=pass "
        f"joints={len(payload['joints'])} motions={len(payload['motions'])} "
        f"tracks={len(payload['tracks'])} keys={len(payload['keys'])} "
        f"capsules={len(payload['capsules'])} "
        f"pose_branches={len(payload['pose_branches'])} data_sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
