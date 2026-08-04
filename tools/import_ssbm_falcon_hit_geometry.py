#!/usr/bin/env python3
"""Generate compact Falcon hit-sphere poses from a pinned Dolphin capture."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from import_ssbm_falcon_frame_data import (
    EXPECTED_CANONICAL_SHA256,
    MOVE_KEYS,
    canonical_sha256,
)


EXPECTED_FULL_SOURCE_SHA256 = (
    "287d53686aedb7469e455600cd749001b2f1a04081158236f26b1fae205f6dde"
)
EXPECTED_CAPTURE_SHA256 = (
    "5a7ac3a35775b0352d48566d622860c846fa2907c4bef03f760080f2a18ba3e8"
)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"
EXPECTED_DOLPHIN_VERSION = "3.4.0"
MELEE_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0

ACTION_BY_MOVE = {
    "jab1": "NEUTRAL_ATTACK_1",
    "jab2": "NEUTRAL_ATTACK_2",
    "dashattack": "DASH_ATTACK",
    "ftilt_m": "FTILT_MID",
    "utilt": "UPTILT",
    "dtilt": "DOWNTILT",
    "fsmash_m": "FSMASH_MID",
    "usmash": "UPSMASH",
    "dsmash": "DOWNSMASH",
    "grab": "GRAB",
    "dashgrab": "GRAB_RUNNING",
    "nair": "NAIR",
    "fair": "FAIR",
    "bair": "BAIR",
    "uair": "UAIR",
    "dair": "DAIR",
}

# The static action-script extractor and the executable disagree on two
# collision boundaries. The pinned memory capture is authoritative for the
# post-pose collision state: Attack12 creates its spheres one displayed frame
# later than the script-only view, while Uair clears them before displayed
# frame 14. Every other routed move matches the static active-frame set.
EXECUTABLE_ACTIVE_FRAMES = {
    "jab2": frozenset({5, 6, 7}),
    "grab": frozenset({7, 8}),
    "dashgrab": frozenset({11, 12}),
    "uair": frozenset(range(6, 14)),
}
SOURCE_FRAME_OFFSET = {"jab2": -1, "grab": -1, "dashgrab": -1}


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def effect_key(hitbox: dict[str, Any]) -> tuple[object, ...]:
    return (
        int(hitbox["damage"]),
        int(hitbox["angle"]),
        int(hitbox["kbGrowth"]),
        int(hitbox["weightDepKb"]),
        int(hitbox["baseKb"]),
        int(hitbox["shieldDamage"]),
        str(hitbox["hitboxInteraction"]),
        str(hitbox["element"]),
        bool(hitbox["hitGrounded"]),
        bool(hitbox["hitAirborne"]),
    )


def captured_effect_key(hitbox: dict[str, Any]) -> tuple[int, ...]:
    return (
        round(float(hitbox["damage"])),
        int(hitbox["angle"]),
        int(hitbox["knockback_growth"]),
        int(hitbox["weight_set_knockback"]),
        int(hitbox["base_knockback"]),
    )


def validate_capture(capture: dict[str, Any]) -> None:
    if capture.get("schema") != 8:
        raise ValueError("unexpected hit-geometry capture schema")
    if capture.get("fighter") != "CPTFALCON":
        raise ValueError("hit-geometry capture is not Captain Falcon")
    if capture.get("opponent") != "CPTFALCON":
        raise ValueError("hit-geometry opponent is not Captain Falcon")
    if capture.get("stage") != "FINAL_DESTINATION":
        raise ValueError("hit-geometry capture is not on Final Destination")
    if capture.get("dolphin_version") != EXPECTED_DOLPHIN_VERSION:
        raise ValueError("unexpected Dolphin version")
    disc = dict(capture.get("disc", {}))
    if (
        disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != EXPECTED_DISC_SHA256
    ):
        raise ValueError("unexpected SSBM disc identity")
    probe = dict(capture.get("hitbox_memory_probe", {}))
    if probe.get("decomp_revision") != EXPECTED_DECOMP_REVISION:
        raise ValueError("unexpected hitbox memory layout revision")
    if (
        probe.get("fighter_hurtbox_array") != "fighter+0x11a0"
        or probe.get("hurtbox_stride") != "0x4c"
    ):
        raise ValueError("capture is missing Falcon hurt-capsule provenance")


def active_frames(move: dict[str, Any]) -> set[int]:
    return {
        frame
        for phase in move.get("hitFrames", [])
        for frame in range(int(phase["start"]), int(phase["end"]) + 1)
    }


def hitboxes_for_frame(
    move: dict[str, Any], action_frame: int
) -> list[dict[str, Any]]:
    phases = [
        phase
        for phase in move.get("hitFrames", [])
        if int(phase["start"]) <= action_frame <= int(phase["end"])
    ]
    if len(phases) != 1:
        raise ValueError(
            f"expected one hitbox phase on frame {action_frame}, "
            f"found {len(phases)}"
        )
    return sorted(phases[0]["hitboxes"], key=lambda hitbox: int(hitbox["id"]))


def generate(
    timing_data: dict[str, Any],
    full_data: dict[str, Any],
    capture: dict[str, Any],
) -> str:
    rows = list(capture["rows"])
    frames: list[dict[str, int]] = []
    spheres: list[dict[str, int]] = []
    geometry_moves: list[dict[str, int]] = []
    standing_rows = [
        row
        for row in rows
        if row.get("action") == "FTILT_MID"
        and float(row.get("action_frame", 0.0)) == 9.0
        and row.get("opponent_action") == "STANDING"
        and float(row.get("opponent_action_frame", 0.0)) == 18.0
        and int(row.get("opponent_facing", 0)) == -1
    ]
    if len(standing_rows) != 1:
        raise ValueError(
            "expected one collision-evaluated standing hurt-capsule pose"
        )
    standing_memory = dict(standing_rows[0]["hitbox_memory"])
    standing_position = [
        float(value)
        for value in standing_memory["opponent_fighter_position"]
    ]
    standing_hurtboxes = []
    for hurtbox_id, source in enumerate(
        standing_memory["opponent_hurtboxes"]
    ):
        hurtbox = dict(source)
        if int(hurtbox["state"]) != 0:
            continue
        endpoint_a = [float(value) for value in hurtbox["position_a"]]
        endpoint_b = [float(value) for value in hurtbox["position_b"]]
        standing_hurtboxes.append(
            {
                "a_x": round(
                    -(endpoint_a[0] - standing_position[0])
                    * MELEE_TO_SIM_Q16
                ),
                "a_y": round(
                    -(endpoint_a[1] - standing_position[1])
                    * MELEE_TO_SIM_Q16
                ),
                "b_x": round(
                    -(endpoint_b[0] - standing_position[0])
                    * MELEE_TO_SIM_Q16
                ),
                "b_y": round(
                    -(endpoint_b[1] - standing_position[1])
                    * MELEE_TO_SIM_Q16
                ),
                "radius": round(
                    float(hurtbox["radius"]) * MELEE_TO_SIM_Q16
                ),
                "hurtbox_id": hurtbox_id,
                "height": int(hurtbox["height"]),
                "grabbable": int(hurtbox["grabbable"]),
            }
        )
    if len(standing_hurtboxes) != 11:
        raise ValueError("unexpected Falcon standing hurt-capsule count")

    for move_key in MOVE_KEYS:
        action_name = ACTION_BY_MOVE.get(move_key)
        if action_name is None:
            geometry_moves.append(
                {"frame_offset": 0, "first_frame": 0, "frame_count": 0}
            )
            continue

        timing_move = dict(timing_data[move_key])
        full_move = dict(full_data[move_key])
        source_frames = active_frames(full_move)
        expected_frames = set(
            EXECUTABLE_ACTIVE_FRAMES.get(move_key, source_frames)
        )
        captured_by_frame: dict[int, dict[str, Any]] = {}
        for row in rows:
            if row.get("action") != action_name:
                continue
            memory = dict(row["hitbox_memory"])
            active = [
                dict(hitbox)
                for hitbox in memory["hitboxes"]
                if int(hitbox["state"]) != 0
            ]
            if not active:
                continue
            raw_frame = float(row["action_frame"])
            action_frame = round(raw_frame)
            if abs(raw_frame - action_frame) > 0.000001:
                raise ValueError(
                    f"{move_key}: fractional action frame {raw_frame}"
                )
            if int(row["facing"]) != 1:
                raise ValueError(f"{move_key}: capture must face right")
            previous = captured_by_frame.get(action_frame)
            if previous is not None:
                previous_memory = dict(previous["hitbox_memory"])
                if (
                    previous_memory["fighter_position"]
                    != memory["fighter_position"]
                    or previous_memory["hitboxes"] != memory["hitboxes"]
                ):
                    raise ValueError(
                        f"{move_key}: inconsistent duplicate frame {action_frame}"
                    )
                continue
            captured_by_frame[action_frame] = row

        if set(captured_by_frame) != expected_frames:
            raise ValueError(
                f"{move_key}: active frame mismatch: "
                f"expected {sorted(expected_frames)}, "
                f"captured {sorted(captured_by_frame)}"
            )

        first_frame = min(expected_frames)
        last_frame = max(expected_frames)
        frame_offset = len(frames)
        timing_effects = list(timing_move["hitboxes"])
        effect_keys = [effect_key(effect) for effect in timing_effects]
        for action_frame in range(first_frame, last_frame + 1):
            row = captured_by_frame.get(action_frame)
            sphere_offset = len(spheres)
            if row is not None:
                memory = dict(row["hitbox_memory"])
                fighter_position = [
                    float(value) for value in memory["fighter_position"]
                ]
                source_action_frame = (
                    action_frame + SOURCE_FRAME_OFFSET.get(move_key, 0)
                )
                source_hitboxes = hitboxes_for_frame(
                    full_move, source_action_frame
                )
                captured_hitboxes = [
                    dict(hitbox) for hitbox in memory["hitboxes"]
                ]
                if len(source_hitboxes) > len(captured_hitboxes):
                    raise ValueError(
                        f"{move_key} frame {action_frame}: too many hitboxes"
                    )
                for source_hitbox in source_hitboxes:
                    hitbox_id = int(source_hitbox["id"])
                    captured = captured_hitboxes[hitbox_id]
                    if int(captured["state"]) == 0:
                        raise ValueError(
                            f"{move_key} frame {action_frame}: "
                            f"hitbox {hitbox_id} is disabled"
                        )
                    expected_effect = (
                        int(source_hitbox["damage"]),
                        int(source_hitbox["angle"]),
                        int(source_hitbox["kbGrowth"]),
                        int(source_hitbox["weightDepKb"]),
                        int(source_hitbox["baseKb"]),
                    )
                    if captured_effect_key(captured) != expected_effect:
                        raise ValueError(
                            f"{move_key} frame {action_frame}: "
                            f"hitbox {hitbox_id} effect mismatch"
                        )
                    source_key = effect_key(source_hitbox)
                    try:
                        effect_index = effect_keys.index(source_key)
                    except ValueError as error:
                        raise ValueError(
                            f"{move_key} frame {action_frame}: "
                            f"hitbox {hitbox_id} has no timing-table effect"
                        ) from error
                    position = [
                        float(value) for value in captured["position"]
                    ]
                    spheres.append(
                        {
                            "offset_x": round(
                                (position[0] - fighter_position[0])
                                * MELEE_TO_SIM_Q16
                            ),
                            "offset_y": round(
                                -(position[1] - fighter_position[1])
                                * MELEE_TO_SIM_Q16
                            ),
                            "offset_z": round(
                                (position[2] - fighter_position[2])
                                * MELEE_TO_SIM_Q16
                            ),
                            "radius": round(
                                float(captured["radius"])
                                * MELEE_TO_SIM_Q16
                            ),
                            "effect_index": effect_index,
                            "hitbox_id": hitbox_id,
                            "group_id": int(source_hitbox["groupId"]),
                        }
                    )
            frames.append(
                {
                    "sphere_offset": sphere_offset,
                    "sphere_count": len(spheres) - sphere_offset,
                }
            )
        geometry_moves.append(
            {
                "frame_offset": frame_offset,
                "first_frame": first_frame,
                "frame_count": last_frame - first_frame + 1,
            }
        )

    lines = [
        "/* Generated by tools/import_ssbm_falcon_hit_geometry.py. */",
        f"/* full source SHA-256: {EXPECTED_FULL_SOURCE_SHA256} */",
        f"/* Dolphin capture SHA-256: {EXPECTED_CAPTURE_SHA256} */",
        f"/* disc SHA-256: {EXPECTED_DISC_SHA256} */",
        f"/* decomp revision: {EXPECTED_DECOMP_REVISION} */",
        "",
        "static const pf_m4_reference_geometry_move",
        "pf_m4_falcon_geometry_moves[PF_M4_FALCON_MOVE_COUNT] = {",
    ]
    lines.extend(
        "    { "
        f"UINT16_C({move['frame_offset']}), "
        f"UINT8_C({move['first_frame']}), "
        f"UINT8_C({move['frame_count']}) "
        "},"
        for move in geometry_moves
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hit_frame",
            "pf_m4_falcon_hit_frames[] = {",
        )
    )
    lines.extend(
        "    { "
        f"UINT16_C({frame['sphere_offset']}), "
        f"UINT8_C({frame['sphere_count']}), UINT8_C(0) "
        "},"
        for frame in frames
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hit_sphere",
            "pf_m4_falcon_hit_spheres[] = {",
        )
    )
    lines.extend(
        "    { "
        f"INT32_C({sphere['offset_x']}), "
        f"INT32_C({sphere['offset_y']}), "
        f"INT32_C({sphere['offset_z']}), "
        f"INT32_C({sphere['radius']}), "
        f"UINT8_C({sphere['effect_index']}), "
        f"UINT8_C({sphere['hitbox_id']}), "
        f"UINT8_C({sphere['group_id']}), UINT8_C(0) "
        "},"
        for sphere in spheres
    )
    lines.extend(
        (
            "};",
            "",
            "/* Opponent Stand pose 18, collision-evaluated during Ftilt frame 9. */",
            "static const pf_m4_reference_hurt_capsule",
            "pf_m4_falcon_standing_hurt_capsules[] = {",
        )
    )
    lines.extend(
        "    { "
        f"INT32_C({hurtbox['a_x']}), "
        f"INT32_C({hurtbox['a_y']}), "
        f"INT32_C({hurtbox['b_x']}), "
        f"INT32_C({hurtbox['b_y']}), "
        f"INT32_C({hurtbox['radius']}), "
        f"UINT8_C({hurtbox['hurtbox_id']}), "
        f"UINT8_C({hurtbox['height']}), "
        f"UINT8_C({hurtbox['grabbable']}), UINT8_C(0) "
        "},"
        for hurtbox in standing_hurtboxes
    )
    lines.extend(("};", ""))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("timing_source", type=Path)
    parser.add_argument("full_source", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    timing_data = json.loads(args.timing_source.read_text(encoding="utf-8"))
    timing_digest = canonical_sha256(timing_data)
    if timing_digest != EXPECTED_CANONICAL_SHA256:
        raise SystemExit(
            f"unexpected Falcon timing source SHA-256: {timing_digest}"
        )
    full_digest = file_sha256(args.full_source)
    if full_digest != EXPECTED_FULL_SOURCE_SHA256:
        raise SystemExit(
            f"unexpected Falcon full source SHA-256: {full_digest}"
        )
    capture_digest = file_sha256(args.capture)
    if capture_digest != EXPECTED_CAPTURE_SHA256:
        raise SystemExit(
            f"unexpected Dolphin geometry capture SHA-256: {capture_digest}"
        )
    full_data = json.loads(args.full_source.read_text(encoding="utf-8"))
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    validate_capture(capture)
    output = generate(timing_data, full_data, capture)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-falcon-hit-geometry=pass "
        f"moves={len(ACTION_BY_MOVE)} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
