#!/usr/bin/env python3
"""Generate the C view of a manifest-owned stored SSBM oracle domain."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
from typing import Any


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


def generate(
    manifest: dict[str, Any],
    *,
    allow_pending_production_digest: bool = False,
) -> str:
    if manifest.get("schema") != 1:
        raise ValueError("unsupported common-hurt coverage schema")
    tracks = manifest.get("pose_tracks")
    stored = manifest.get("stored_oracle")
    if not isinstance(tracks, list) or not isinstance(stored, dict):
        raise ValueError("coverage manifest is missing pose tracks or stored oracle")
    c_config = stored.get("c")
    if not isinstance(c_config, dict):
        raise ValueError("stored oracle is missing its C integration")
    symbol_prefix = c_identifier(
        c_config.get("symbol_prefix"), "stored_oracle.c.symbol_prefix"
    )
    macro_prefix = c_identifier(
        c_config.get("macro_prefix"), "stored_oracle.c.macro_prefix"
    )
    track_count_expression = c_identifier(
        c_config.get("track_count_expression"),
        "stored_oracle.c.track_count_expression",
    )
    actions = identifier_map(
        c_config.get("actions"), "stored_oracle.c.actions"
    )
    buttons = identifier_map(
        c_config.get("buttons"), "stored_oracle.c.buttons"
    )
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
    cases = stored.get("cases")
    if not isinstance(cases, list) or not cases:
        raise ValueError("stored oracle must declare cases")

    track_rows: list[str] = []
    track_by_action_enum: dict[str, tuple[int, int, int]] = {}
    pose_count = 0
    for index, track in enumerate(tracks):
        if not isinstance(track, dict):
            raise ValueError(f"pose_tracks[{index}] must be an object")
        action = track.get("action")
        frames = track.get("frames")
        if not isinstance(action, str) or not isinstance(frames, dict):
            raise ValueError(f"pose_tracks[{index}] is incomplete")
        first = require_int(frames.get("first"), f"{action}.first", 1, 65535)
        last = require_int(frames.get("last"), f"{action}.last", first, 65535)
        step = require_int(frames.get("step"), f"{action}.step", 1, 65535)
        if (last - first) % step != 0:
            raise ValueError(f"{action} frame range is not divisible by its step")
        enum_name = action_enum(action, actions)
        if enum_name in track_by_action_enum:
            raise ValueError(f"duplicate runtime action in pose tracks: {action}")
        track_by_action_enum[enum_name] = (first, last, step)
        pose_count += (last - first) // step + 1
        track_rows.append(
            "    { "
            f"{c_string(action)}, (uint8_t){enum_name}, "
            f"UINT16_C({first}), UINT16_C({last}), UINT16_C({step})"
            " },"
        )
    if pose_count != expected_pose_count:
        raise ValueError(
            f"expected {expected_pose_count} stored poses, got {pose_count}"
        )

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
        if not isinstance(expect_hit, bool):
            raise ValueError(f"{case_id}.expect_hit must be boolean")
        target_action = action_enum(case.get("target_action"), actions)
        distance = require_int(
            case.get("distance_hundredths"),
            f"{case_id}.distance_hundredths",
            0,
            65535,
        )
        if mode == "runtime":
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
            action_frame = require_int(
                case.get("action_frame"), f"{case_id}.action_frame", 1, 65535
            )
            source_frame = require_int(
                case.get("source_frame"), f"{case_id}.source_frame", 1, 65535
            )
            jab_frame = require_int(
                case.get("jab_frame"), f"{case_id}.jab_frame", 1, 65535
            )
            facing = require_int(case.get("facing"), f"{case_id}.facing", -1, 1)
            if facing == 0:
                raise ValueError(f"{case_id}.facing may not be zero")
            height = require_int(
                case.get("height_hundredths"),
                f"{case_id}.height_hundredths",
                0,
                65535,
            )
            track = track_by_action_enum.get(target_action)
            if track is None:
                raise ValueError(f"{case_id}.target_action has no pose track")
            first, last, step = track
            expected_source_frame = first + (action_frame - 1) * step
            if expected_source_frame > last or source_frame != expected_source_frame:
                raise ValueError(
                    f"{case_id} maps runtime frame {action_frame} to source "
                    f"frame {expected_source_frame}, not {source_frame}"
                )
            values = (
                "PF_SSBM_STORED_GEOMETRY",
                target_action,
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
            raise ValueError(f"{case_id}.mode is unsupported: {mode!r}")
        case_rows.append(
            "    { "
            f"{c_string(case_id)}, {values[0]}, (uint8_t){values[1]}, "
            f"UINT32_C({values[2]}), UINT32_C({values[3]}), "
            f"UINT16_C({values[4]}), UINT16_C({values[5]}), "
            f"INT16_C({values[6]}), INT16_C({values[7]}), {values[8]}, "
            f"UINT16_C({values[9]}), UINT16_C({values[10]}), "
            f"UINT8_C({values[11]})"
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
        f"poses={pose_count(manifest)} cases={len(manifest['stored_oracle']['cases'])} "
        f"output={args.output}"
    )
    return 0


def pose_count(manifest: dict[str, Any]) -> int:
    return sum(
        (int(track["frames"]["last"]) - int(track["frames"]["first"]))
        // int(track["frames"]["step"])
        + 1
        for track in manifest["pose_tracks"]
    )


if __name__ == "__main__":
    raise SystemExit(main())
