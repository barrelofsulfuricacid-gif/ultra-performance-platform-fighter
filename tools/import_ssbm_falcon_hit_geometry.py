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
    SOURCE_DAT_JSON_SHA256,
    canonical_sha256,
    command_variable_assignments,
)
from ssbm_collision import (
    canonical_hurt_pose_q16,
    q16_hurt_poses_equivalent,
)


EXPECTED_FULL_SOURCE_SHA256 = (
    "287d53686aedb7469e455600cd749001b2f1a04081158236f26b1fae205f6dde"
)
EXPECTED_CAPTURE_SHA256 = (
    "5a7ac3a35775b0352d48566d622860c846fa2907c4bef03f760080f2a18ba3e8"
)
EXPECTED_HURT_CAPTURE_SHA256 = (
    "d9fea72b7eb86447e5bd53b2157ec7f3dde9a27f02a28750ec4964ab6bd7ef32"
)
EXPECTED_COMMON_HURT_CAPTURE_SHA256 = (
    "8ddb3245936d9ded82763481010e67f5968dbe7b50d14fe251db4ae25fedfbcc"
)
EXPECTED_THROW_CAPTURE_SHA256 = (
    "368c623e49231aff0f70c8aa687345f10e615b121a675dbddcb8abd99a3a0b95"
)
SPECIAL_CAPTURE_ACTION_VALUES = {
    "2c8bc604024cfad745e266239dcc4d3e1b1ff1c4a07afcc6eecb9938b5f155b1": frozenset(
        {347}
    ),
    "4db3840dc864b494c91b1b185e232391e54cf0572dc0bdb81ba3316b64782e84": frozenset(
        {348}
    ),
    "634e04888af1048e19d63302a9ede3d21819ad253b15d944649c9d5d71145b4b": frozenset(
        {349, 351}
    ),
    "6244baaf1354749a118a3577f3ca080f87dc4ba59d60f14b947077922a667a2d": frozenset(
        {357, 358}
    ),
    "1b72cb23727cd0770ee2fb5c4a7c8e9e17e91c71548e45be57bbe85cb8df5990": frozenset(
        {359, 361}
    ),
    "7b0a4d6a855a88fd88dd83270b9f8812df0023159e6ab95a4236e5391ba3cd3a": frozenset(
        {350}
    ),
    "8eda88a578afb770af4d28a0a166413d2ee3ecf9da38fb533b45012c958e262a": frozenset(
        {352}
    ),
    "c06fdb3edb1caeccdd2fe7e27b0dec3d1b62115d0b53235995d2c0b33e7ed315": frozenset(
        {353}
    ),
    "a6a49ebc4f49e6bd9b8c9861b7393a002623e7090a7bbd63ba1891896a838c9d": frozenset(
        {354}
    ),
    "27d869d3d9873d91690223d014cf0e7875fcd2b7138013bac1229c8512c32c60": frozenset(
        {355, 356}
    ),
    "b86de007baeb6048488d4f2aaa258690ef2d09693033fd9b0f78865d81ea80d2": frozenset(
        {360}
    ),
    "ae4d9c2f3cb19af35e9288d5f9dcc9a7a4a72adc1baa220bc3ffe03b8e17ddfc": frozenset(
        {362}
    ),
}
EXPECTED_SPECIAL_CAPTURE_SHA256S = frozenset(SPECIAL_CAPTURE_ACTION_VALUES)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"
EXPECTED_DOLPHIN_VERSION = "3.4.0"
EXPECTED_COMMON_HURT_DOLPHIN_VERSION = "3.5.1"
EXPECTED_COMMON_HURT_LIBMELEE_VERSION = "0.47.2"
EXPECTED_COMMON_HURT_ORACLE_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
MELEE_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0

COMMON_HURT_ACTIONS = (
    ("DASHING", tuple(range(1, 16))),
    ("RUN_BRAKE", tuple(range(1, 29))),
    ("CROUCH_START", tuple(range(1, 8))),
    ("CROUCH_END", tuple(range(1, 11))),
    ("KNEE_BEND", tuple(range(1, 5))),
    ("SPOTDODGE", tuple(range(1, 33))),
    ("ROLL_FORWARD", tuple(range(1, 32))),
    ("ROLL_BACKWARD", tuple(range(1, 32))),
    ("AIRDODGE", tuple(range(1, 50))),
    ("DEAD_FALL", tuple(range(1, 9))),
    # LandingFallSpecial plays Falcon's 29-frame source motion at 3.0
    # animation frames per simulation tick for the common 10-frame lag.
    ("LANDING_SPECIAL", tuple(range(1, 29, 3))),
    ("LANDING", tuple(range(1, 31))),
)

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
    "fthrow": "THROW_FORWARD",
    "bthrow": "THROW_BACK",
    "uthrow": "THROW_UP",
    "dthrow": "THROW_DOWN",
    "0x12d": "NEUTRAL_B_ATTACKING_AIR",
    "0x12e": "NEUTRAL_B_FULL_CHARGE_AIR",
    "0x12f": "SWORD_DANCE_1",
    "0x130": "SWORD_DANCE_2_HIGH",
    "0x131": "SWORD_DANCE_2_MID",
    "0x132": "SWORD_DANCE_3_HIGH",
    "0x133": "SWORD_DANCE_3_MID",
    "0x134": "SWORD_DANCE_3_LOW",
    "0x135": "SWORD_DANCE_4_HIGH",
    "0x136": "SWORD_DANCE_4_MID",
    "0x137": "SWORD_DANCE_4_LOW",
    "0x138": "SWORD_DANCE_1_AIR",
    "0x139": "SWORD_DANCE_2_HIGH_AIR",
    "0x13a": "DOWN_B_GROUND_START",
    "0x13b": "SWORD_DANCE_3_MID_AIR",
    "0x13c": "DOWN_B_GROUND",
}

ACTION_VALUE_BY_MOVE = {
    "0x12d": 347,
    "0x12e": 348,
    "0x12f": 349,
    "0x130": 350,
    "0x131": 351,
    "0x132": 352,
    "0x133": 353,
    "0x134": 354,
    "0x135": 355,
    "0x136": 356,
    "0x137": 357,
    "0x138": 358,
    "0x139": 359,
    "0x13a": 360,
    "0x13b": 362,
    "0x13c": 361,
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
    "0x139": frozenset(range(15, 30)),
    "0x133": frozenset(range(13, 34)),
    "0x134": frozenset(range(13, 34)),
    "0x135": frozenset({1, 2}),
    "0x13a": frozenset({0, 1}),
}
SOURCE_FRAME_OFFSET = {
    "jab2": -1,
    "grab": -1,
    "dashgrab": -1,
    "0x135": 2,
    "0x13a": 2,
}
LIVE_EFFECT_ONLY_FRAMES = {"0x139": frozenset(range(26, 30))}
POSE_ALIAS = {"0x13d": "0x136"}
THROW_MOVE_KEYS = frozenset({"fthrow", "bthrow", "uthrow", "dthrow"})


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


def captured_collision_key(memory: dict[str, Any]) -> tuple[object, ...]:
    fighter_position = [float(value) for value in memory["fighter_position"]]
    return tuple(
        (
            index,
            float(hitbox["damage"]),
            tuple(
                round(
                    (float(hitbox["position"][axis]) - fighter_position[axis])
                    * MELEE_TO_SIM_Q16
                )
                for axis in range(3)
            ),
            round(float(hitbox["radius"]) * MELEE_TO_SIM_Q16),
            int(hitbox["angle"]),
            int(hitbox["knockback_growth"]),
            int(hitbox["weight_set_knockback"]),
            int(hitbox["base_knockback"]),
            int(hitbox["element"]),
        )
        for index, hitbox in enumerate(memory["hitboxes"])
        if int(hitbox["state"]) != 0
    )


def captured_search_spheres(
    memory: dict[str, Any], facing: int
) -> tuple[tuple[int, ...], ...]:
    """Canonicalize live zero-damage SpecialS searches into facing-right space."""

    fighter_position = [float(value) for value in memory["fighter_position"]]
    spheres = []
    for hitbox_id, source in enumerate(memory["hitboxes"]):
        hitbox = dict(source)
        if (
            int(hitbox["state"]) == 0
            or float(hitbox["damage"]) != 0.0
            or float(hitbox["radius"]) <= 0.0
        ):
            continue
        position = [float(value) for value in hitbox["position"]]
        spheres.append(
            (
                round(
                    facing
                    * (position[0] - fighter_position[0])
                    * MELEE_TO_SIM_Q16
                ),
                round(-(position[1] - fighter_position[1]) * MELEE_TO_SIM_Q16),
                round(
                    facing
                    * (position[2] - fighter_position[2])
                    * MELEE_TO_SIM_Q16
                ),
                round(float(hitbox["radius"]) * MELEE_TO_SIM_Q16),
                hitbox_id,
            )
        )
    return tuple(spheres)


def collision_keys_q16_equivalent(
    left: tuple[object, ...], right: tuple[object, ...]
) -> bool:
    if len(left) != len(right):
        return False
    for left_hitbox, right_hitbox in zip(left, right, strict=True):
        left_values = tuple(left_hitbox)
        right_values = tuple(right_hitbox)
        if (
            left_values[:2] != right_values[:2]
            or left_values[3:] != right_values[3:]
            or any(
                abs(left_axis - right_axis) > 1
                for left_axis, right_axis in zip(
                    left_values[2], right_values[2], strict=True
                )
            )
        ):
            return False
    return True


def captured_positions_q16_equivalent(
    left: object, right: object
) -> bool:
    left_position = tuple(
        round(float(value) * MELEE_TO_SIM_Q16) for value in list(left)
    )
    right_position = tuple(
        round(float(value) * MELEE_TO_SIM_Q16) for value in list(right)
    )
    return all(
        abs(left_axis - right_axis) <= 1
        for left_axis, right_axis in zip(
            left_position, right_position, strict=True
        )
    )


def captured_hurt_capsules(
    memory: dict[str, Any],
    hurtbox_key: str,
    fighter_position_key: str,
    facing: int,
) -> tuple[tuple[int, ...], ...]:
    """Canonicalize one live pose into facing-right simulation space."""
    return canonical_hurt_pose_q16(
        memory,
        hurtbox_key,
        fighter_position_key,
        facing,
        MELEE_TO_SIM_Q16,
    )


hurt_poses_q16_equivalent = q16_hurt_poses_equivalent


def validate_capture(
    capture: dict[str, Any],
    expected_schema: int,
    expected_dolphin_version: str = EXPECTED_DOLPHIN_VERSION,
) -> None:
    if capture.get("schema") != expected_schema:
        raise ValueError("unexpected hit-geometry capture schema")
    if capture.get("fighter") != "CPTFALCON":
        raise ValueError("hit-geometry capture is not Captain Falcon")
    if capture.get("opponent") != "CPTFALCON":
        raise ValueError("hit-geometry opponent is not Captain Falcon")
    if capture.get("stage") != "FINAL_DESTINATION":
        raise ValueError("hit-geometry capture is not on Final Destination")
    if capture.get("dolphin_version") != expected_dolphin_version:
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


def row_matches_move(row: dict[str, Any], move_key: str) -> bool:
    if row.get("action") != ACTION_BY_MOVE[move_key]:
        return False
    action_value = ACTION_VALUE_BY_MOVE.get(move_key)
    return action_value is None or int(row["action_value"]) == action_value


def active_frames(move: dict[str, Any]) -> set[int]:
    return {
        frame
        for phase in move.get("hitFrames", [])
        for frame in range(int(phase["start"]), int(phase["end"]) + 1)
    }


def hitboxes_for_frame(move: dict[str, Any], action_frame: int) -> list[dict[str, Any]]:
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
    dat_data: dict[str, Any],
    hit_capture: dict[str, Any],
    hurt_capture: dict[str, Any],
    common_hurt_capture: dict[str, Any],
    throw_capture: dict[str, Any],
    special_captures: list[dict[str, Any]],
    special_capture_digests: list[str],
) -> str:
    special_rows = [
        row
        for capture, digest in zip(
            special_captures, special_capture_digests, strict=True
        )
        for row in capture["rows"]
        if int(row["action_value"]) in SPECIAL_CAPTURE_ACTION_VALUES[digest]
    ]
    rows = list(hit_capture["rows"]) + list(throw_capture["rows"]) + special_rows
    # Throw animation rate depends on the captured fighter's weight, so its
    # hurt poses require a separate weight-qualified time axis. Keep this
    # import scoped to the throw attack spheres; do not pretend the raw
    # animation-frame samples are one fixed-tick hurt-pose sequence.
    hurt_rows = list(hurt_capture["rows"]) + special_rows
    frames: list[dict[str, int]] = []
    spheres: list[dict[str, int]] = []
    geometry_moves: list[dict[str, int]] = []
    hurt_frames: list[dict[str, int]] = []
    hurt_capsules: list[tuple[int, ...]] = []
    hurt_moves: list[dict[str, int]] = []
    common_hurt_moves: list[dict[str, int]] = []
    hurt_pose_offsets: dict[tuple[tuple[int, ...], ...], int] = {}

    def append_common_hurt_track(
        first_frame: int,
        poses: list[tuple[tuple[int, ...], ...]],
    ) -> None:
        frame_offset = len(hurt_frames)
        for pose in poses:
            capsule_offset = hurt_pose_offsets.get(pose)
            if capsule_offset is None:
                capsule_offset = len(hurt_capsules)
                if capsule_offset > 0xFFFF:
                    raise ValueError("too many Falcon hurt capsules")
                hurt_pose_offsets[pose] = capsule_offset
                hurt_capsules.extend(pose)
            hurt_frames.append(
                {
                    "capsule_offset": capsule_offset,
                    "capsule_count": len(pose),
                }
            )
        common_hurt_moves.append(
            {
                "frame_offset": frame_offset,
                "first_frame": first_frame,
                "frame_count": len(poses),
            }
        )

    subactions = dat_data["nodes"][0]["data"]["subactions"]
    ground_assignments = command_variable_assignments(subactions, 303)
    air_assignments = command_variable_assignments(subactions, 305)
    search_specs = (
        (349, ground_assignments[(0, 1)], ground_assignments[(0, 0)] - 1),
        (351, air_assignments[(0, 1)], air_assignments[(0, 0)] - 1),
    )
    search_spheres: list[tuple[int, ...]] = []
    search_offsets: list[int] = []
    search_counts: list[int] = []
    for action_value, first_frame, last_frame in search_specs:
        action_rows = [
            row
            for row in special_rows
            if int(row["action_value"]) == action_value
            and first_frame <= round(float(row["action_frame"])) <= last_frame
        ]
        search_frame_set = {
            round(float(row["action_frame"]))
            for row in action_rows
        }
        if search_frame_set != set(range(first_frame, last_frame + 1)):
            raise ValueError(
                f"SpecialS {action_value}: incomplete executable search frames"
            )
        poses = []
        for row in action_rows:
            facing = int(row["facing"])
            if facing not in (-1, 1):
                raise ValueError(f"SpecialS {action_value}: invalid facing")
            pose = captured_search_spheres(dict(row["hitbox_memory"]), facing)
            if len(pose) != 3:
                raise ValueError(
                    f"SpecialS {action_value}: expected three search spheres"
                )
            poses.append(pose)
        source_pose = poses[0]
        if any(
            len(pose) != len(source_pose)
            or any(
                left[4] != right[4]
                or any(abs(a - b) > 1 for a, b in zip(left[:4], right[:4], strict=True))
                for left, right in zip(source_pose, pose, strict=True)
            )
            for pose in poses[1:]
        ):
            raise ValueError(f"SpecialS {action_value}: moving search geometry")
        search_offsets.append(len(search_spheres))
        search_counts.append(len(source_pose))
        search_spheres.extend(source_pose)
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
        raise ValueError("expected one collision-evaluated standing hurt-capsule pose")
    standing_memory = dict(standing_rows[0]["hitbox_memory"])
    standing_hurtboxes = captured_hurt_capsules(
        standing_memory,
        "opponent_hurtboxes",
        "opponent_fighter_position",
        -1,
    )
    if len(standing_hurtboxes) != 11:
        raise ValueError("unexpected Falcon standing hurt-capsule count")

    for move_key in MOVE_KEYS:
        capture_move_key = POSE_ALIAS.get(move_key, move_key)
        action_name = ACTION_BY_MOVE.get(capture_move_key)
        if action_name is None:
            geometry_moves.append(
                {"frame_offset": 0, "first_frame": 0, "frame_count": 0}
            )
            hurt_moves.append({"frame_offset": 0, "first_frame": 0, "frame_count": 0})
            continue

        timing_move = dict(timing_data[move_key])
        full_move = dict(full_data[move_key])
        if capture_move_key != move_key:
            capture_move = dict(full_data[capture_move_key])
            if (
                full_move["subactionName"] != capture_move["subactionName"]
                or full_move["totalFrames"] != capture_move["totalFrames"]
            ):
                raise ValueError(f"{move_key}: aliased pose source is not identical")
        total_frames = int(timing_move["totalFrames"])
        hurt_by_frame: dict[int, tuple[tuple[int, ...], ...]] = {}
        for row in hurt_rows:
            if not row_matches_move(row, capture_move_key):
                continue
            raw_frame = float(row["action_frame"])
            action_frame = round(raw_frame)
            if abs(raw_frame - action_frame) > 0.000001:
                raise ValueError(f"{move_key}: fractional action frame {raw_frame}")
            if action_frame < 1 or action_frame > total_frames:
                continue
            facing = int(row["facing"])
            if facing not in (-1, 1):
                raise ValueError(f"{move_key}: invalid facing {facing}")
            pose = captured_hurt_capsules(
                dict(row["hitbox_memory"]),
                "fighter_hurtboxes",
                "fighter_position",
                facing,
            )
            previous_pose = hurt_by_frame.get(action_frame)
            if previous_pose is not None and not hurt_poses_q16_equivalent(
                previous_pose, pose
            ):
                raise ValueError(
                    f"{move_key}: inconsistent hurt pose on frame " f"{action_frame}"
                )
            hurt_by_frame[action_frame] = pose
        expected_hurt_frames = (
            set() if move_key in THROW_MOVE_KEYS else set(range(1, total_frames + 1))
        )
        if set(hurt_by_frame) != expected_hurt_frames:
            raise ValueError(
                f"{move_key}: hurt-frame mismatch: "
                f"expected {min(expected_hurt_frames)}-"
                f"{max(expected_hurt_frames)}, captured "
                f"{sorted(hurt_by_frame)}"
            )
        hurt_frame_offset = len(hurt_frames)
        for action_frame in sorted(expected_hurt_frames):
            pose = hurt_by_frame[action_frame]
            capsule_offset = hurt_pose_offsets.get(pose)
            if capsule_offset is None:
                capsule_offset = len(hurt_capsules)
                if capsule_offset > 0xFFFF:
                    raise ValueError("too many Falcon hurt capsules")
                hurt_pose_offsets[pose] = capsule_offset
                hurt_capsules.extend(pose)
            hurt_frames.append(
                {
                    "capsule_offset": capsule_offset,
                    "capsule_count": len(pose),
                }
            )
        hurt_moves.append(
            {
                "frame_offset": hurt_frame_offset,
                "first_frame": 0 if move_key in THROW_MOVE_KEYS else 1,
                "frame_count": 0 if move_key in THROW_MOVE_KEYS else total_frames,
            }
        )
        source_frames = active_frames(full_move)
        expected_frames = set(EXECUTABLE_ACTIVE_FRAMES.get(move_key, source_frames))
        if not expected_frames:
            geometry_moves.append(
                {"frame_offset": 0, "first_frame": 0, "frame_count": 0}
            )
            continue
        captured_by_frame: dict[int, dict[str, Any]] = {}
        last_captured_by_frame: dict[int, dict[str, Any]] = {}
        for row in rows:
            if not row_matches_move(row, capture_move_key):
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
                raise ValueError(f"{move_key}: fractional action frame {raw_frame}")
            if int(row["facing"]) != 1:
                raise ValueError(f"{move_key}: capture must face right")
            previous = captured_by_frame.get(action_frame)
            if previous is not None:
                previous_memory = dict(previous["hitbox_memory"])
                if not collision_keys_q16_equivalent(
                    captured_collision_key(previous_memory),
                    captured_collision_key(memory),
                ):
                    raise ValueError(
                        f"{move_key}: inconsistent duplicate frame {action_frame}"
                    )
                last_captured_by_frame[action_frame] = row
                continue
            captured_by_frame[action_frame] = row
            last_captured_by_frame[action_frame] = row

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
                source_action_frame = action_frame + SOURCE_FRAME_OFFSET.get(
                    move_key, 0
                )
                captured_hitboxes = [dict(hitbox) for hitbox in memory["hitboxes"]]
                if action_frame in LIVE_EFFECT_ONLY_FRAMES.get(move_key, frozenset()):
                    source_hitboxes = []
                    for hitbox_id, captured in enumerate(captured_hitboxes):
                        if int(captured["state"]) == 0:
                            continue
                        captured_key = captured_effect_key(captured)
                        matching_effects = [
                            effect_index
                            for effect_index, effect in enumerate(timing_effects)
                            if captured_key
                            == (
                                int(effect["damage"]),
                                int(effect["angle"]),
                                int(effect["kbGrowth"]),
                                int(effect["weightDepKb"]),
                                int(effect["baseKb"]),
                            )
                        ]
                        if len(matching_effects) != 1:
                            raise ValueError(
                                f"{move_key} frame {action_frame}: live "
                                f"effect match is ambiguous: {matching_effects}"
                            )
                        source_hitbox = dict(timing_effects[matching_effects[0]])
                        source_hitbox["id"] = hitbox_id
                        source_hitbox["groupId"] = matching_effects[0]
                        source_hitboxes.append(source_hitbox)
                else:
                    source_hitboxes = hitboxes_for_frame(full_move, source_action_frame)
                if len(source_hitboxes) > len(captured_hitboxes):
                    raise ValueError(
                        f"{move_key} frame {action_frame}: too many hitboxes"
                    )
                previous_row = last_captured_by_frame.get(action_frame - 1)
                previous_captured_hitboxes = (
                    [
                        dict(hitbox)
                        for hitbox in dict(previous_row["hitbox_memory"])["hitboxes"]
                    ]
                    if previous_row is not None
                    else []
                )
                for source_hitbox in source_hitboxes:
                    hitbox_id = int(source_hitbox["id"])
                    captured = captured_hitboxes[hitbox_id]
                    if int(captured["state"]) == 0:
                        raise ValueError(
                            f"{move_key} frame {action_frame}: "
                            f"hitbox {hitbox_id} is disabled"
                        )
                    collision_state = int(captured["state"])
                    if collision_state == 2:
                        if not captured_positions_q16_equivalent(
                            captured["previous_position"],
                            captured["position"],
                        ):
                            raise ValueError(
                                f"{move_key} frame {action_frame}: newly "
                                f"created hitbox {hitbox_id} has a moving x58"
                            )
                    elif collision_state == 3:
                        if (
                            hitbox_id >= len(previous_captured_hitboxes)
                            or int(previous_captured_hitboxes[hitbox_id]["state"])
                            == 0
                            or not captured_positions_q16_equivalent(
                                captured["previous_position"],
                                previous_captured_hitboxes[hitbox_id]["position"],
                            )
                        ):
                            raise ValueError(
                                f"{move_key} frame {action_frame}: continuing "
                                f"hitbox {hitbox_id} does not preserve x58"
                            )
                    else:
                        raise ValueError(
                            f"{move_key} frame {action_frame}: active hitbox "
                            f"{hitbox_id} has collision state {collision_state}"
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
                    position = [float(value) for value in captured["position"]]
                    spheres.append(
                        {
                            "offset_x": round(
                                (position[0] - fighter_position[0]) * MELEE_TO_SIM_Q16
                            ),
                            "offset_y": round(
                                -(position[1] - fighter_position[1]) * MELEE_TO_SIM_Q16
                            ),
                            "offset_z": round(
                                (position[2] - fighter_position[2]) * MELEE_TO_SIM_Q16
                            ),
                            "radius": round(
                                float(captured["radius"]) * MELEE_TO_SIM_Q16
                            ),
                            "effect_index": effect_index,
                            "hitbox_id": hitbox_id,
                            "group_id": int(source_hitbox["groupId"]),
                            "collision_state": collision_state,
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

    for action_name, source_frames in COMMON_HURT_ACTIONS:
        first_frame = 1
        hurt_by_frame: dict[int, tuple[tuple[int, ...], ...]] = {}
        for row in common_hurt_capture["rows"]:
            if row.get("action") != action_name:
                continue
            raw_frame = float(row["action_frame"])
            action_frame = round(raw_frame)
            if abs(raw_frame - action_frame) > 0.000001:
                raise ValueError(
                    f"common {action_name}: fractional action frame {raw_frame}"
                )
            if action_frame not in source_frames:
                continue
            facing = int(row["facing"])
            if facing not in (-1, 1):
                raise ValueError(
                    f"common {action_name}: invalid facing {facing}"
                )
            pose = captured_hurt_capsules(
                dict(row["hitbox_memory"]),
                "fighter_hurtboxes",
                "fighter_position",
                facing,
            )
            previous_pose = hurt_by_frame.get(action_frame)
            if previous_pose is not None and not hurt_poses_q16_equivalent(
                previous_pose, pose
            ):
                raise ValueError(
                    f"common {action_name}: inconsistent frame {action_frame}"
                )
            hurt_by_frame[action_frame] = pose
        expected_frames = set(source_frames)
        if set(hurt_by_frame) != expected_frames:
            raise ValueError(
                f"common {action_name}: expected frames "
                f"{list(source_frames)}, captured {sorted(hurt_by_frame)}"
            )
        append_common_hurt_track(
            first_frame,
            [hurt_by_frame[action_frame] for action_frame in source_frames],
        )

    geometry_digest = hashlib.sha256(
        json.dumps(
            {
                "geometry_moves": geometry_moves,
                "hit_frames": frames,
                "hit_spheres": spheres,
                "hurt_moves": hurt_moves,
                "common_hurt_moves": common_hurt_moves,
                "hurt_frames": hurt_frames,
                "hurt_capsules": hurt_capsules,
                "standing_hurt_capsules": standing_hurtboxes,
                "side_special_search_spheres": search_spheres,
                "side_special_search_offsets": search_offsets,
                "side_special_search_counts": search_counts,
            },
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
    ).hexdigest()
    lines = [
        "/* Generated by tools/import_ssbm_falcon_hit_geometry.py. */",
        f"/* full source SHA-256: {EXPECTED_FULL_SOURCE_SHA256} */",
        f"/* hit-sphere capture SHA-256: {EXPECTED_CAPTURE_SHA256} */",
        f"/* hurt-pose capture SHA-256: {EXPECTED_HURT_CAPTURE_SHA256} */",
        f"/* common hurt-pose capture SHA-256: "
        f"{EXPECTED_COMMON_HURT_CAPTURE_SHA256} */",
        f"/* throw capture SHA-256: {EXPECTED_THROW_CAPTURE_SHA256} */",
        *(
            f"/* special capture SHA-256: {digest} */"
            for digest in special_capture_digests
        ),
        f"/* disc SHA-256: {EXPECTED_DISC_SHA256} */",
        f"/* decomp revision: {EXPECTED_DECOMP_REVISION} */",
        f"/* canonical geometry SHA-256: {geometry_digest} */",
        "",
        "static const uint8_t pf_m4_falcon_geometry_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{geometry_digest[index:index + 2]})"
            for index in range(0, len(geometry_digest), 2)
        ),
        "};",
        "",
        "static const uint16_t pf_m4_falcon_side_special_ground_search_offset = "
        f"UINT16_C({search_offsets[0]});",
        "static const uint8_t pf_m4_falcon_side_special_ground_search_count = "
        f"UINT8_C({search_counts[0]});",
        "static const uint16_t pf_m4_falcon_side_special_air_search_offset = "
        f"UINT16_C({search_offsets[1]});",
        "static const uint8_t pf_m4_falcon_side_special_air_search_count = "
        f"UINT8_C({search_counts[1]});",
        "static const pf_m4_reference_search_sphere",
        "pf_m4_falcon_side_special_search_spheres[] = {",
    ]
    lines.extend(
        "    { "
        f"INT32_C({sphere[0]}), INT32_C({sphere[1]}), "
        f"INT32_C({sphere[2]}), INT32_C({sphere[3]}) "
        "},"
        for sphere in search_spheres
    )
    lines.extend((
        "};",
        "",
        "static const pf_m4_reference_geometry_move",
        "pf_m4_falcon_geometry_moves[PF_M4_FALCON_MOVE_COUNT] = {",
    ))
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
        f"UINT8_C({sphere['group_id']}), "
        f"UINT8_C({sphere['collision_state']}) "
        "},"
        for sphere in spheres
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hurt_move",
            "pf_m4_falcon_hurt_moves[PF_M4_FALCON_MOVE_COUNT] = {",
        )
    )
    lines.extend(
        "    { "
        f"UINT16_C({move['frame_offset']}), "
        f"UINT8_C({move['first_frame']}), "
        f"UINT8_C({move['frame_count']}) "
        "},"
        for move in hurt_moves
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hurt_move",
            "pf_m4_falcon_common_hurt_moves[PF_M4_FALCON_COMMON_HURT_COUNT] = {",
        )
    )
    lines.extend(
        "    { "
        f"UINT16_C({move['frame_offset']}), "
        f"UINT8_C({move['first_frame']}), "
        f"UINT8_C({move['frame_count']}) "
        "},"
        for move in common_hurt_moves
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hurt_frame",
            "pf_m4_falcon_hurt_frames[] = {",
        )
    )
    lines.extend(
        "    { "
        f"UINT16_C({frame['capsule_offset']}), "
        f"UINT8_C({frame['capsule_count']}), UINT8_C(0) "
        "},"
        for frame in hurt_frames
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hurt_capsule",
            "pf_m4_falcon_hurt_capsules[] = {",
        )
    )
    lines.extend(
        "    { "
        f"INT32_C({hurtbox[0]}), "
        f"INT32_C({hurtbox[1]}), "
        f"INT32_C({hurtbox[2]}), "
        f"INT32_C({hurtbox[3]}), "
        f"INT32_C({hurtbox[4]}), "
        f"INT32_C({hurtbox[5]}), "
        f"INT32_C({hurtbox[6]}), "
        f"UINT8_C({hurtbox[7]}), "
        f"UINT8_C({hurtbox[8]}), "
        f"UINT8_C({hurtbox[9]}), UINT8_C(0) "
        "},"
        for hurtbox in hurt_capsules
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
        f"INT32_C({hurtbox[0]}), "
        f"INT32_C({hurtbox[1]}), "
        f"INT32_C({hurtbox[2]}), "
        f"INT32_C({hurtbox[3]}), "
        f"INT32_C({hurtbox[4]}), "
        f"INT32_C({hurtbox[5]}), "
        f"INT32_C({hurtbox[6]}), "
        f"UINT8_C({hurtbox[7]}), "
        f"UINT8_C({hurtbox[8]}), "
        f"UINT8_C({hurtbox[9]}), UINT8_C(0) "
        "},"
        for hurtbox in standing_hurtboxes
    )
    lines.extend(("};", ""))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("timing_source", type=Path)
    parser.add_argument("full_source", type=Path)
    parser.add_argument("dat_source", type=Path)
    parser.add_argument("hit_capture", type=Path)
    parser.add_argument("hurt_capture", type=Path)
    parser.add_argument("common_hurt_capture", type=Path)
    parser.add_argument("throw_capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--special-capture", action="append", default=[], type=Path)
    args = parser.parse_args()

    timing_data = json.loads(args.timing_source.read_text(encoding="utf-8"))
    timing_digest = canonical_sha256(timing_data)
    if timing_digest != EXPECTED_CANONICAL_SHA256:
        raise SystemExit(f"unexpected Falcon timing source SHA-256: {timing_digest}")
    full_digest = file_sha256(args.full_source)
    if full_digest != EXPECTED_FULL_SOURCE_SHA256:
        raise SystemExit(f"unexpected Falcon full source SHA-256: {full_digest}")
    dat_digest = file_sha256(args.dat_source)
    if dat_digest != SOURCE_DAT_JSON_SHA256:
        raise SystemExit(f"unexpected Falcon DAT JSON SHA-256: {dat_digest}")
    hit_capture_digest = file_sha256(args.hit_capture)
    if hit_capture_digest != EXPECTED_CAPTURE_SHA256:
        raise SystemExit(
            "unexpected Dolphin hit-sphere capture SHA-256: " f"{hit_capture_digest}"
        )
    hurt_capture_digest = file_sha256(args.hurt_capture)
    if hurt_capture_digest != EXPECTED_HURT_CAPTURE_SHA256:
        raise SystemExit(
            "unexpected Dolphin hurt-pose capture SHA-256: " f"{hurt_capture_digest}"
        )
    common_hurt_capture_digest = file_sha256(args.common_hurt_capture)
    if common_hurt_capture_digest != EXPECTED_COMMON_HURT_CAPTURE_SHA256:
        raise SystemExit(
            "unexpected Dolphin common hurt-pose capture SHA-256: "
            f"{common_hurt_capture_digest}"
        )
    throw_capture_digest = file_sha256(args.throw_capture)
    if throw_capture_digest != EXPECTED_THROW_CAPTURE_SHA256:
        raise SystemExit(
            "unexpected Dolphin throw capture SHA-256: " f"{throw_capture_digest}"
        )
    special_capture_digests = [file_sha256(path) for path in args.special_capture]
    if (
        len(special_capture_digests) != len(EXPECTED_SPECIAL_CAPTURE_SHA256S)
        or set(special_capture_digests) != EXPECTED_SPECIAL_CAPTURE_SHA256S
    ):
        raise SystemExit(
            "unexpected Dolphin special capture SHA-256 set: "
            f"{special_capture_digests}"
        )
    full_data = json.loads(args.full_source.read_text(encoding="utf-8"))
    dat_data = json.loads(args.dat_source.read_text(encoding="utf-8"))
    hit_capture = json.loads(args.hit_capture.read_text(encoding="utf-8"))
    hurt_capture = json.loads(args.hurt_capture.read_text(encoding="utf-8"))
    common_hurt_capture = json.loads(
        args.common_hurt_capture.read_text(encoding="utf-8")
    )
    common_execution = dict(common_hurt_capture.get("oracle_execution", {}))
    if (
        common_execution.get("mode") != "exiai-headless-null-fast-forward"
        or common_execution.get("release") != "exi-ai-0.2.0"
        or common_execution.get("release_artifact_sha256")
        != EXPECTED_COMMON_HURT_ORACLE_SHA256
        or common_hurt_capture.get("libmelee_version")
        != EXPECTED_COMMON_HURT_LIBMELEE_VERSION
    ):
        raise SystemExit("unexpected accelerated common-hurt oracle provenance")
    throw_capture = json.loads(args.throw_capture.read_text(encoding="utf-8"))
    special_captures = [
        json.loads(path.read_text(encoding="utf-8")) for path in args.special_capture
    ]
    validate_capture(hit_capture, 8)
    validate_capture(hurt_capture, 9)
    validate_capture(
        common_hurt_capture,
        9,
        EXPECTED_COMMON_HURT_DOLPHIN_VERSION,
    )
    validate_capture(throw_capture, 9)
    for special_capture in special_captures:
        validate_capture(special_capture, 9)
    output = generate(
        timing_data,
        full_data,
        dat_data,
        hit_capture,
        hurt_capture,
        common_hurt_capture,
        throw_capture,
        special_captures,
        special_capture_digests,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-falcon-hit-geometry=pass "
        f"moves={len(ACTION_BY_MOVE)} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
