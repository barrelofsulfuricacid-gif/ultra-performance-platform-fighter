#!/usr/bin/env python3
"""Generate the C view of a manifest-owned stored SSBM oracle domain."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
from typing import Any

from ssbm_collision import (
    canonical_json_sha256,
    hurt_pose_tracks_semantic_payload,
)


PRODUCTION_POSE_SERIALIZATION = (
    "action-name-nul,source-frame-u16le,capsule-count-u8,"
    "capsules-seven-i32le-four-u8-v1"
)


def require_int(value: Any, name: str, low: int, high: int) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise ValueError(f"{name} must be an integer")
    if not low <= value <= high:
        raise ValueError(f"{name} must be in [{low}, {high}]")
    return value


def c_identifier(value: Any, name: str) -> str:
    if not isinstance(value, str) or re.fullmatch(
        r"[A-Za-z_][A-Za-z0-9_]*", value
    ) is None:
        raise ValueError(f"{name} must be a C identifier")
    return value


def c_constant_expression(value: Any, name: str) -> str:
    if not isinstance(value, str) or re.fullmatch(
        r"(?:[A-Za-z_][A-Za-z0-9_]*|[1-9][0-9]*)"
        r"(?:\s*\+\s*(?:[A-Za-z_][A-Za-z0-9_]*|[1-9][0-9]*))*",
        value,
    ) is None:
        raise ValueError(f"{name} must be a sum of C identifiers or counts")
    return value


def identifier_map(value: Any, name: str) -> dict[str, str]:
    if not isinstance(value, dict):
        raise ValueError(f"{name} must be an object")
    result: dict[str, str] = {}
    for key, identifier in value.items():
        if not isinstance(key, str) or not key:
            raise ValueError(f"{name} keys must be non-empty strings")
        result[key] = c_identifier(identifier, f"{name}.{key}")
    return result


def action_enum(name: Any, actions: dict[str, str]) -> str:
    if not isinstance(name, str) or name not in actions:
        raise ValueError(f"unsupported stored-oracle action: {name!r}")
    return actions[name]


def button_expression(names: Any, buttons: dict[str, str]) -> str:
    if not isinstance(names, list) or any(name not in buttons for name in names):
        raise ValueError(f"unsupported stored-oracle buttons: {names!r}")
    if not names:
        return "UINT64_C(0)"
    return " | ".join(buttons[name] for name in names)


def c_string(value: str) -> str:
    return json.dumps(value)


def stored_pose_tracks(
    manifest_tracks: Any,
    stored: dict[str, Any],
    manifest_directory: Path,
    expected_capsules_per_pose: int,
) -> list[dict[str, Any]]:
    if not isinstance(manifest_tracks, list):
        raise ValueError("coverage manifest is missing pose tracks")
    tracks = [
        dict(track) if isinstance(track, dict) else track
        for track in manifest_tracks
    ]
    profiles = stored.get("pose_profiles", [])
    if not isinstance(profiles, list):
        raise ValueError("stored_oracle.pose_profiles must be an array")
    for profile_index, declaration in enumerate(profiles):
        label = f"stored_oracle.pose_profiles[{profile_index}]"
        if not isinstance(declaration, dict):
            raise ValueError(f"{label} must be an object")
        relative_path = declaration.get("path")
        if not isinstance(relative_path, str) or not relative_path:
            raise ValueError(f"{label}.path must be a non-empty string")
        profile_relative_path = Path(relative_path)
        if profile_relative_path.is_absolute() or ".." in profile_relative_path.parts:
            raise ValueError(f"{label}.path must stay below the manifest directory")
        profile_path = manifest_directory / profile_relative_path
        profile_bytes = profile_path.read_bytes()
        profile_digest = hashlib.sha256(profile_bytes).hexdigest()
        if profile_digest != declaration.get("profile_sha256"):
            raise ValueError(
                f"{label} profile SHA-256 is {profile_digest}, "
                f"expected {declaration.get('profile_sha256')}"
            )
        profile = json.loads(profile_bytes)
        profile_tracks = profile.get("tracks")
        if (
            profile.get("schema") != 1
            or profile.get("scope") != "ssbm-bounded-hurt-pose-tracks"
            or profile.get("capture_sha256") != declaration.get("capture_sha256")
            or not isinstance(profile_tracks, list)
        ):
            raise ValueError(f"{label} has unsupported hurt-pose profile provenance")
        semantic_digest = canonical_json_sha256(
            hurt_pose_tracks_semantic_payload(profile_tracks)
        )
        if (
            semantic_digest != declaration.get("semantic_sha256")
            or semantic_digest != profile.get("semantic_sha256")
        ):
            raise ValueError(
                f"{label} semantic SHA-256 is {semantic_digest}, "
                f"expected {declaration.get('semantic_sha256')}"
            )
        track_submotions = declaration.get("track_submotions")
        if not isinstance(track_submotions, dict):
            raise ValueError(f"{label}.track_submotions must be an object")
        track_ids = [
            track.get("id")
            for track in profile_tracks
            if isinstance(track, dict)
        ]
        if (
            len(track_ids) != len(profile_tracks)
            or any(not isinstance(track_id, str) or not track_id for track_id in track_ids)
            or len(set(track_ids)) != len(track_ids)
            or set(track_submotions) != set(track_ids)
        ):
            raise ValueError(
                f"{label}.track_submotions must bind every profile track exactly once"
            )
        track_first_action_frames = declaration.get(
            "track_first_action_frames", {}
        )
        if (
            not isinstance(track_first_action_frames, dict)
            or any(
                track_id not in track_ids
                for track_id in track_first_action_frames
            )
        ):
            raise ValueError(
                f"{label}.track_first_action_frames must reference profile tracks"
            )
        for track_index, profile_track in enumerate(profile_tracks):
            track_label = f"{label}.tracks[{track_index}]"
            action = profile_track.get("source_action")
            first = profile_track.get("first_displayed_frame")
            frame_count = profile_track.get("frame_count")
            frames = profile_track.get("frames")
            if (
                not isinstance(action, str)
                or not isinstance(first, int)
                or isinstance(first, bool)
                or not isinstance(frame_count, int)
                or isinstance(frame_count, bool)
                or first < 0
                or frame_count < 1
                or not isinstance(frames, list)
                or len(frames) != frame_count
            ):
                raise ValueError(f"{track_label} is incomplete")
            for frame_offset, frame in enumerate(frames):
                if (
                    not isinstance(frame, dict)
                    or frame.get("displayed_frame") != first + frame_offset
                    or not isinstance(frame.get("capsules_f32"), list)
                    or len(frame["capsules_f32"])
                    != expected_capsules_per_pose
                ):
                    raise ValueError(
                        f"{track_label} frame {first + frame_offset} is incomplete"
                    )
            tracks.append(
                {
                    "action": action,
                    "source_submotion": track_submotions[profile_track["id"]],
                    "first_action_frame": require_int(
                        track_first_action_frames.get(
                            profile_track["id"], 1
                        ),
                        f"{track_label}.first_action_frame",
                        0,
                        65535,
                    ),
                    "frames": {
                        "first": first,
                        "last": first + frame_count - 1,
                        "step": 1,
                    },
                }
            )
    return tracks


def generate(
    manifest: dict[str, Any],
    manifest_directory: Path,
    *,
    allow_pending_production_digest: bool = False,
) -> str:
    if manifest.get("schema") != 1:
        raise ValueError("unsupported common-hurt coverage schema")
    stored = manifest.get("stored_oracle")
    if not isinstance(stored, dict):
        raise ValueError("coverage manifest is missing its stored oracle")
    expected_pose_count = require_int(
        stored.get("expected_pose_count"),
        "stored_oracle.expected_pose_count",
        1,
        65535,
    )
    expected_capsules_per_pose = require_int(
        stored.get("expected_capsules_per_pose"),
        "stored_oracle.expected_capsules_per_pose",
        1,
        255,
    )
    tracks = stored_pose_tracks(
        manifest.get("pose_tracks"),
        stored,
        manifest_directory,
        expected_capsules_per_pose,
    )
    c_config = stored.get("c")
    if not isinstance(c_config, dict):
        raise ValueError("stored oracle is missing its C integration")
    symbol_prefix = c_identifier(
        c_config.get("symbol_prefix"), "stored_oracle.c.symbol_prefix"
    )
    macro_prefix = c_identifier(
        c_config.get("macro_prefix"), "stored_oracle.c.macro_prefix"
    )
    track_count_expression = c_constant_expression(
        c_config.get("track_count_expression"),
        "stored_oracle.c.track_count_expression",
    )
    actions = identifier_map(
        c_config.get("actions"), "stored_oracle.c.actions"
    )
    submotions = identifier_map(
        c_config.get("submotions", {}), "stored_oracle.c.submotions"
    )
    buttons = identifier_map(
        c_config.get("buttons"), "stored_oracle.c.buttons"
    )
    moves = identifier_map(
        c_config.get("moves", {}), "stored_oracle.c.moves"
    )
    cases = stored.get("cases")
    if not isinstance(cases, list) or not cases:
        raise ValueError("stored oracle must declare cases")

    track_rows: list[str] = []
    track_by_runtime_state: dict[
        tuple[str, str], tuple[int, int, int, int]
    ] = {}
    pose_count = 0
    for index, track in enumerate(tracks):
        if not isinstance(track, dict):
            raise ValueError(f"pose_tracks[{index}] must be an object")
        action = track.get("action")
        frames = track.get("frames")
        if not isinstance(action, str) or not isinstance(frames, dict):
            raise ValueError(f"pose_tracks[{index}] is incomplete")
        # Melee's TurnRun owns displayed source frame zero while the target
        # runtime intentionally enters its public action at tick one. Keep
        # those clocks independently expressible instead of shifting or
        # cloning the source pose track.
        first = require_int(frames.get("first"), f"{action}.first", 0, 65535)
        last = require_int(frames.get("last"), f"{action}.last", first, 65535)
        step = require_int(frames.get("step"), f"{action}.step", 1, 65535)
        first_action_frame = require_int(
            track.get("first_action_frame", 1),
            f"{action}.first_action_frame",
            0,
            65535,
        )
        if (last - first) % step != 0:
            raise ValueError(f"{action} frame range is not divisible by its step")
        enum_name = action_enum(action, actions)
        source_submotion = track.get("source_submotion")
        if source_submotion is None:
            submotion_expression = "UINT16_C(0)"
        elif isinstance(source_submotion, str) and source_submotion in submotions:
            submotion_expression = f"(uint16_t){submotions[source_submotion]}"
        else:
            raise ValueError(
                f"unsupported stored-oracle source submotion: {source_submotion!r}"
            )
        runtime_state = (enum_name, submotion_expression)
        if runtime_state in track_by_runtime_state:
            raise ValueError(
                f"duplicate runtime action/submotion in pose tracks: {action}"
            )
        track_by_runtime_state[runtime_state] = (
            first,
            last,
            step,
            first_action_frame,
        )
        pose_count += (last - first) // step + 1
        track_rows.append(
            "    { "
            f"{c_string(action)}, (uint8_t){enum_name}, "
            f"{submotion_expression}, "
            f"UINT16_C({first}), UINT16_C({last}), UINT16_C({step}), "
            f"UINT16_C({first_action_frame})"
            " },"
        )
    if pose_count != expected_pose_count:
        raise ValueError(
            f"expected {expected_pose_count} stored poses, got {pose_count}"
        )

    def mapped_case_frame(
        case_id: str,
        case: dict[str, Any],
        target_action: str,
        target_submotion_expression: str,
    ) -> tuple[int, int]:
        action_frame = require_int(
            case.get("action_frame"), f"{case_id}.action_frame", 0, 65535
        )
        source_frame = require_int(
            case.get("source_frame"), f"{case_id}.source_frame", 0, 65535
        )
        track = track_by_runtime_state.get(
            (target_action, target_submotion_expression)
        )
        if track is None:
            raise ValueError(f"{case_id}.target_action has no pose track")
        first, last, step, first_action_frame = track
        if action_frame < first_action_frame:
            raise ValueError(
                f"{case_id}.action_frame precedes its production track"
            )
        expected_source_frame = (
            first + (action_frame - first_action_frame) * step
        )
        if expected_source_frame > last or source_frame != expected_source_frame:
            raise ValueError(
                f"{case_id} maps runtime frame {action_frame} to source "
                f"frame {expected_source_frame}, not {source_frame}"
            )
        return action_frame, source_frame

    case_rows: list[str] = []
    case_ids: set[str] = set()
    for index, case in enumerate(cases):
        if not isinstance(case, dict):
            raise ValueError(f"stored case {index} must be an object")
        case_id = case.get("id")
        mode = case.get("mode")
        if not isinstance(case_id, str) or not case_id or case_id in case_ids:
            raise ValueError(f"invalid or duplicate stored case id: {case_id!r}")
        case_ids.add(case_id)
        expect_hit = case.get("expect_hit")
        if mode in ("runtime", "geometry") and not isinstance(expect_hit, bool):
            raise ValueError(f"{case_id}.expect_hit must be boolean")
        if mode == "pose_facing":
            expect_hit = False
        target_action = action_enum(case.get("target_action"), actions)
        target_source_submotion = case.get("target_source_submotion")
        if target_source_submotion is None:
            target_submotion_expression = "UINT16_C(0)"
        elif (
            isinstance(target_source_submotion, str)
            and target_source_submotion in submotions
        ):
            target_submotion_expression = (
                f"(uint16_t){submotions[target_source_submotion]}"
            )
        else:
            raise ValueError(
                f"{case_id}.target_source_submotion is unsupported"
            )
        geometry_values: tuple[Any, ...] = (0, 0, 0, 0, 0, 0, 0, 0)
        pose_facing_values: tuple[Any, ...] = (0, 0, 0, 0, 0)
        if mode == "runtime":
            distance = require_int(
                case.get("distance_hundredths"),
                f"{case_id}.distance_hundredths",
                0,
                65535,
            )
            stick = case.get("target_stick")
            if (
                not isinstance(stick, list)
                or len(stick) != 2
                or any(not isinstance(value, int) for value in stick)
            ):
                raise ValueError(f"{case_id}.target_stick must contain two integers")
            stick_x = require_int(stick[0], f"{case_id}.target_stick[0]", -32767, 32767)
            stick_y = require_int(stick[1], f"{case_id}.target_stick[1]", -32767, 32767)
            button_mask = button_expression(case.get("target_buttons"), buttons)
            delay = require_int(
                case.get("target_button_delay_ticks"),
                f"{case_id}.target_button_delay_ticks",
                0,
                65535,
            )
            hit_tick = require_int(
                case.get("expected_hit_action_tick"),
                f"{case_id}.expected_hit_action_tick",
                0,
                65535,
            )
            values = (
                "PF_SSBM_STORED_RUNTIME",
                target_action,
                target_submotion_expression,
                distance,
                0,
                0,
                0,
                stick_x,
                stick_y,
                button_mask,
                delay,
                hit_tick,
                int(expect_hit),
            )
        elif mode == "geometry":
            action_frame, source_frame = mapped_case_frame(
                case_id,
                case,
                target_action,
                target_submotion_expression,
            )
            geometry = case.get("geometry_f32")
            if geometry is None:
                distance = require_int(
                    case.get("distance_hundredths"),
                    f"{case_id}.distance_hundredths",
                    0,
                    65535,
                )
                jab_frame = require_int(
                    case.get("jab_frame"), f"{case_id}.jab_frame", 1, 65535
                )
                facing = require_int(
                    case.get("facing"), f"{case_id}.facing", -1, 1
                )
                if facing == 0:
                    raise ValueError(f"{case_id}.facing may not be zero")
                height = require_int(
                    case.get("height_hundredths"),
                    f"{case_id}.height_hundredths",
                    0,
                    65535,
                )
                values = (
                    "PF_SSBM_STORED_GEOMETRY",
                    target_action,
                    target_submotion_expression,
                    distance,
                    height,
                    action_frame,
                    source_frame,
                    facing,
                    0,
                    "UINT64_C(0)",
                    jab_frame,
                    0,
                    int(expect_hit),
                )
            else:
                if not isinstance(geometry, dict):
                    raise ValueError(f"{case_id}.geometry_f32 must be an object")
                move_name = geometry.get("attacker_move")
                if not isinstance(move_name, str) or move_name not in moves:
                    raise ValueError(
                        f"{case_id}.geometry_f32.attacker_move is unsupported"
                    )
                attacker_frame = require_int(
                    geometry.get("attacker_action_frame"),
                    f"{case_id}.geometry_f32.attacker_action_frame",
                    1,
                    65535,
                )
                offset = geometry.get("target_offset_f32")
                if not isinstance(offset, list) or len(offset) != 2:
                    raise ValueError(
                        f"{case_id}.geometry_f32.target_offset_f32 must have two values"
                    )
                offset_x = require_int(
                    offset[0],
                    f"{case_id}.geometry_f32.target_offset_f32[0]",
                    -(1 << 31),
                    (1 << 31) - 1,
                )
                offset_y = require_int(
                    offset[1],
                    f"{case_id}.geometry_f32.target_offset_f32[1]",
                    -(1 << 31),
                    (1 << 31) - 1,
                )
                attacker_facing = require_int(
                    geometry.get("attacker_facing"),
                    f"{case_id}.geometry_f32.attacker_facing",
                    -1,
                    1,
                )
                target_facing = require_int(
                    geometry.get("target_facing"),
                    f"{case_id}.geometry_f32.target_facing",
                    -1,
                    1,
                )
                if attacker_facing == 0 or target_facing == 0:
                    raise ValueError(f"{case_id}.geometry_f32 facing may not be zero")
                grabbable_only = geometry.get("grabbable_only")
                if not isinstance(grabbable_only, bool):
                    raise ValueError(
                        f"{case_id}.geometry_f32.grabbable_only must be boolean"
                    )
                values = (
                    "PF_SSBM_STORED_GEOMETRY",
                    target_action,
                    target_submotion_expression,
                    0,
                    0,
                    action_frame,
                    source_frame,
                    0,
                    0,
                    "UINT64_C(0)",
                    0,
                    0,
                    int(expect_hit),
                )
                geometry_values = (
                    moves[move_name],
                    attacker_frame,
                    offset_x,
                    offset_y,
                    attacker_facing,
                    target_facing,
                    int(grabbable_only),
                    1,
                )
        elif mode == "pose_facing":
            action_frame, source_frame = mapped_case_frame(
                case_id,
                case,
                target_action,
                target_submotion_expression,
            )
            pose_facing = case.get("pose_facing")
            if not isinstance(pose_facing, dict):
                raise ValueError(f"{case_id}.pose_facing must be an object")
            action_ticks = require_int(
                pose_facing.get("action_ticks"),
                f"{case_id}.pose_facing.action_ticks",
                0,
                65535,
            )
            gameplay_facing = require_int(
                pose_facing.get("gameplay_facing"),
                f"{case_id}.pose_facing.gameplay_facing",
                -1,
                1,
            )
            dash_direction = require_int(
                pose_facing.get("dash_direction"),
                f"{case_id}.pose_facing.dash_direction",
                -1,
                1,
            )
            expected_pose_facing = require_int(
                pose_facing.get("expected_pose_facing"),
                f"{case_id}.pose_facing.expected_pose_facing",
                -1,
                1,
            )
            if gameplay_facing == 0 or expected_pose_facing == 0:
                raise ValueError(
                    f"{case_id}.pose_facing gameplay/expected facing may not be zero"
                )
            values = (
                "PF_SSBM_STORED_POSE_FACING",
                target_action,
                target_submotion_expression,
                0,
                0,
                action_frame,
                source_frame,
                0,
                0,
                "UINT64_C(0)",
                0,
                0,
                0,
            )
            pose_facing_values = (
                action_ticks,
                gameplay_facing,
                dash_direction,
                expected_pose_facing,
                1,
            )
        else:
            raise ValueError(f"{case_id}.mode is unsupported: {mode!r}")
        case_rows.append(
            "    { "
            f"{c_string(case_id)}, {values[0]}, (uint8_t){values[1]}, "
            f"{values[2]}, UINT32_C({values[3]}), UINT32_C({values[4]}), "
            f"UINT16_C({values[5]}), UINT16_C({values[6]}), "
            f"INT16_C({values[7]}), INT16_C({values[8]}), {values[9]}, "
            f"UINT16_C({values[10]}), UINT16_C({values[11]}), "
            f"UINT8_C({values[12]}), "
            "{ "
            f"(uint16_t){geometry_values[0]}, "
            f"UINT16_C({geometry_values[1]}), "
            f"INT32_C({geometry_values[2]}), "
            f"INT32_C({geometry_values[3]}), "
            f"INT8_C({geometry_values[4]}), "
            f"INT8_C({geometry_values[5]}), "
            f"UINT8_C({geometry_values[6]}), "
            f"UINT8_C({geometry_values[7]}) "
            "}, { "
            f"UINT16_C({pose_facing_values[0]}), "
            f"INT8_C({pose_facing_values[1]}), "
            f"INT8_C({pose_facing_values[2]}), "
            f"INT8_C({pose_facing_values[3]}), "
            f"UINT8_C({pose_facing_values[4]}) "
            "}"
            " },"
        )

    serialization = stored.get("production_pose_serialization")
    if serialization != PRODUCTION_POSE_SERIALIZATION:
        raise ValueError(
            "production_pose_serialization must be "
            f"{PRODUCTION_POSE_SERIALIZATION!r}"
        )
    source_digest = stored.get("source_pose_sha256")
    production_digest = stored.get("production_pose_sha256")
    for name, digest in (
        ("source_pose_sha256", source_digest),
        ("production_pose_sha256", production_digest),
    ):
        if (
            name == "production_pose_sha256"
            and digest == "pending"
            and allow_pending_production_digest
        ):
            continue
        if (
            not isinstance(digest, str)
            or len(digest) != 64
            or any(character not in "0123456789abcdef" for character in digest)
        ):
            raise ValueError(f"{name} must be a lowercase SHA-256")

    return "\n".join(
        [
            "/* Generated by tools/generate_ssbm_stored_oracle.py. */",
            f"#ifndef {macro_prefix}_ORACLE_INC",
            f"#define {macro_prefix}_ORACLE_INC",
            "",
            "static const pf_ssbm_stored_pose_track",
            f"{symbol_prefix}_pose_tracks[] = {{",
            *track_rows,
            "};",
            "",
            "static const pf_ssbm_stored_case",
            f"{symbol_prefix}_cases[] = {{",
            *case_rows,
            "};",
            "",
            f"#define {macro_prefix}_POSE_COUNT UINT16_C({pose_count})",
            f"#define {macro_prefix}_CASE_COUNT UINT16_C({len(cases)})",
            f"#define {macro_prefix}_CAPSULES_PER_POSE "
            f"UINT8_C({expected_capsules_per_pose})",
            f"#define {macro_prefix}_PRODUCTION_SERIALIZATION \\",
            f"    {c_string(serialization)}",
            f"#define {macro_prefix}_SOURCE_POSE_SHA256 \\",
            f"    {c_string(str(source_digest))}",
            f"#define {macro_prefix}_PRODUCTION_POSE_SHA256 \\",
            f"    {c_string(str(production_digest))}",
            "",
            "_Static_assert(",
            f"    sizeof({symbol_prefix}_pose_tracks) /",
            f"            sizeof({symbol_prefix}_pose_tracks[0]) ==",
            f"        {track_count_expression},",
            '    "stored-oracle pose-track manifest is incomplete");',
            "_Static_assert(",
            f"    sizeof({symbol_prefix}_cases) /",
            f"            sizeof({symbol_prefix}_cases[0]) ==",
            f"        {macro_prefix}_CASE_COUNT,",
            '    "stored-oracle case manifest is incomplete");',
            "",
            "#endif",
            "",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--allow-pending-production-digest",
        action="store_true",
        help="bootstrap only: generate before the production digest is pinned",
    )
    args = parser.parse_args()
    if args.check and args.allow_pending_production_digest:
        parser.error("--check cannot accept an unpinned production digest")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    generated = generate(
        manifest,
        args.manifest.parent,
        allow_pending_production_digest=args.allow_pending_production_digest,
    )
    if args.check:
        try:
            current = args.output.read_text(encoding="utf-8")
        except FileNotFoundError:
            raise SystemExit(f"missing generated stored oracle: {args.output}")
        if current != generated:
            raise SystemExit(f"stale generated stored oracle: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
    print(
        "ssbm-stored-oracle-generation=pass "
        f"poses={pose_count(manifest, args.manifest.parent)} "
        f"cases={len(manifest['stored_oracle']['cases'])} "
        f"output={args.output}"
    )
    return 0


def pose_count(manifest: dict[str, Any], manifest_directory: Path) -> int:
    stored = manifest["stored_oracle"]
    expected_capsules_per_pose = require_int(
        stored.get("expected_capsules_per_pose"),
        "stored_oracle.expected_capsules_per_pose",
        1,
        255,
    )
    tracks = stored_pose_tracks(
        manifest["pose_tracks"],
        stored,
        manifest_directory,
        expected_capsules_per_pose,
    )
    return sum(
        (int(track["frames"]["last"]) - int(track["frames"]["first"]))
        // int(track["frames"]["step"])
        + 1
        for track in tracks
    )


if __name__ == "__main__":
    raise SystemExit(main())
