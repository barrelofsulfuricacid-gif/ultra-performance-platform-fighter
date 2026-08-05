#!/usr/bin/env python3
"""Convert the pinned Falcon NTSC 1.02 frame-data dump to compact C tables."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any

from hsd_figatree import (
    TRACK_TRANSLATE_Y,
    TRACK_TRANSLATE_Z,
    decode_figatree,
    sample_track,
)


EXPECTED_CANONICAL_SHA256 = (
    "42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356"
)
EXTRACTOR_REVISION = "0b12c5cb988da3fb9b67630b1d8347e12cd91528"
DAT_READER_REVISION = "d4e6074aa26f388fccc7fe8e825761cf1c1bc7b0"
HSD_READER_REVISION = "29546ad77fdf9ebd9a9940ed44903ef309e810d6"
SOURCE_DAT_SHA256 = "4cf61a52737d464df9298fd15573345fb3b9a15c79ab47dce4fd2e3e707917af"
SOURCE_ANIMATION_DAT_SHA256 = (
    "a9a0ccc2382a2f02d5423675469719488540dd119a14577712c97348f70e1c1a"
)
SOURCE_DAT_JSON_SHA256 = (
    "fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266"
)
SOURCE_COMMON_DAT_SHA256 = (
    "63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c"
)

COMMON_ATTRIBUTE_COUNT = 97
SPECIAL_ATTRIBUTE_SIZE = 0x8C

# ftCaptain_DatAttrs at doldecomp/melee revision 9509dc0. Keep the raw words
# as the source of truth; the typed Q16 view below exists only so the runtime
# never has to reinterpret host floats or duplicate these values by hand.
SPECIAL_FLOAT_ATTRIBUTES = (
    "specialn_stick_range_y_neg",
    "specialn_stick_range_y_pos",
    "specialn_angle_diff",
    "specialn_vel_x",
    "specialn_vel_mul",
    "specials_gr_vel_x",
    "specials_grav",
    "specials_terminal_vel",
    "specials_unk0",
    "specials_unk1",
    "specials_unk2",
    "specials_unk3",
    "specials_unk4",
    "specials_unk5",
    "specials_miss_landing_lag",
    "specials_hit_landing_lag",
    "specialhi_air_friction_mul",
    "specialhi_horz_vel",
    "specialhi_freefall_air_spd_mul",
    "specialhi_landing_lag",
    "specialhi_unk0",
    "specialhi_unk1",
    "specialhi_input_var",
    "specialhi_unk2",
    "specialhi_catch_grav",
)

SPECIAL_TAIL_ATTRIBUTES = (
    ("specialhi_air_var", "i32", 0x64),
    ("x68", "bits", 0x68),
    ("speciallw_unk1", "u32", 0x6C),
    ("speciallw_flame_particle_angle", "f32", 0x70),
    ("speciallw_on_hit_spd_modifier", "f32", 0x74),
    ("speciallw_unk2", "i32", 0x78),
    ("speciallw_ground_lag_mul", "f32", 0x7C),
    ("speciallw_landing_lag_mul", "f32", 0x80),
    ("speciallw_ground_traction", "f32", 0x84),
    ("speciallw_air_landing_traction", "f32", 0x88),
)

SPECIAL_UNSIGNED_FIELDS = frozenset({"x68_bits", "speciallw_unk1"})

MOVE_KEYS = (
    "jab1",
    "jab2",
    "jab3",
    "rapidjabs_start",
    "rapidjabs_loop",
    "rapidjabs_end",
    "dashattack",
    "ftilt_h",
    "ftilt_mh",
    "ftilt_m",
    "ftilt_ml",
    "ftilt_l",
    "utilt",
    "dtilt",
    "fsmash_h",
    "fsmash_mh",
    "fsmash_m",
    "fsmash_ml",
    "fsmash_l",
    "usmash",
    "dsmash",
    "nair",
    "fair",
    "bair",
    "uair",
    "dair",
    "grab",
    "dashgrab",
    "pummel",
    "fthrow",
    "bthrow",
    "uthrow",
    "dthrow",
    "0x12d",
    "0x12e",
    "0x12f",
    "0x130",
    "0x131",
    "0x132",
    "0x133",
    "0x134",
    "0x135",
    "0x136",
    "0x137",
    "0x138",
    "0x139",
    "0x13a",
    "0x13b",
    "0x13c",
    "0x13d",
)

# The extractor uses the common five-angle forward-smash schema. Falcon's DAT
# has only high, straight, and low variants, so these two slots are
# intentionally absent rather than incomplete extraction results.
EXPECTED_ABSENT_MOVE_KEYS = frozenset({"fsmash_mh", "fsmash_ml"})

ELEMENTS = {
    "empty": "PF_M4_REFERENCE_HIT_EMPTY",
    "normal": "PF_M4_REFERENCE_HIT_NORMAL",
    "fire": "PF_M4_REFERENCE_HIT_FIRE",
    "electric": "PF_M4_REFERENCE_HIT_ELECTRIC",
    "grab": "PF_M4_REFERENCE_HIT_GRAB",
}

ANIMATION_TRANSLATION_FLAG = 0x80000000
MELEE_X_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0
MELEE_Y_TO_SIM_Q16 = 65536.0 * 11.0 / 62.0

# ftCa_SpecialHiThrow0 starts by running ftCo_800DE2A8. When Falcon caught a
# grounded victim, that source routine relocates the thrower from the victim's
# TransN2 attachment before animation-root motion begins. These NTSC 1.02
# offsets are the observed relocation at SpecialHiCatch frame 15 ->
# SpecialHiThrow frame 0 in the strict Dolphin capture whose SHA-256 is
# 27d869d3d9873d91690223d014cf0e7875fcd2b7138013bac1229c8512c32c60.
SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE = -10.707747459411621
SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE = 2.545643227005005

# Falcon's Falling ECB bottom is animation-derived, not the authored gameplay
# body's half-height. Captured directly at fighter+0x794 from the same NTSC
# 1.02 process (ECB capture SHA-256
# 4518dbb5cd43158baeaa1ddad7d5ffd073b4dda46ecbe2aa55d8c7efa9eadfdb).
FALLING_ECB_BOTTOM_Y_MELEE = 7.932853698730469


def canonical_sha256(data: dict[str, Any]) -> str:
    encoded = json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def u16(value: Any) -> int:
    return 0 if value is None else int(value)


def q16(value: float) -> int:
    return round(value * 65536.0)


def raw_f32(words: list[int], index: int) -> float:
    return struct.unpack(">f", words[index].to_bytes(4, "big"))[0]


def fighter_data_offsets(source_dat: bytes) -> tuple[int, int]:
    """Return ftDataCaptain's common- and extended-attribute offsets."""

    if len(source_dat) < 0x20:
        raise ValueError("truncated PlCa.dat header")
    data_size, relocation_count, root_count, reference_count = struct.unpack_from(
        ">4I", source_dat, 0x04
    )
    if root_count != 1 or reference_count != 0:
        raise ValueError("unexpected PlCa.dat root/reference table")
    root_table = 0x20 + data_size + relocation_count * 4
    if root_table + 8 > len(source_dat):
        raise ValueError("PlCa.dat root table is out of bounds")
    root_offset, root_name_offset = struct.unpack_from(">2I", source_dat, root_table)
    string_table = root_table + root_count * 8 + reference_count * 8
    name_start = string_table + root_name_offset
    name_end = source_dat.find(b"\0", name_start)
    if name_end < 0 or source_dat[name_start:name_end] != b"ftDataCaptain":
        raise ValueError("unexpected PlCa.dat root name")
    data = source_dat[0x20 : 0x20 + data_size]
    if root_offset + 8 > len(data):
        raise ValueError("ftDataCaptain root is out of bounds")
    common_offset, special_offset = struct.unpack_from(">2I", data, root_offset)
    if common_offset + COMMON_ATTRIBUTE_COUNT * 4 > len(data):
        raise ValueError("Falcon common attributes are out of bounds")
    if special_offset + SPECIAL_ATTRIBUTE_SIZE > len(data):
        raise ValueError("Falcon special attributes are out of bounds")
    return common_offset, special_offset


def source_attributes(source_dat: bytes) -> tuple[list[int], dict[str, int]]:
    """Decode every common word and every decomp-defined Falcon special."""

    common_offset, special_offset = fighter_data_offsets(source_dat)
    data = source_dat[0x20:]
    common_bits = list(struct.unpack_from(">97I", data, common_offset))
    special: dict[str, int] = {}
    for index, name in enumerate(SPECIAL_FLOAT_ATTRIBUTES):
        value = struct.unpack_from(">f", data, special_offset + index * 4)[0]
        special[f"{name}_q16"] = q16(value)
    for name, kind, offset in SPECIAL_TAIL_ATTRIBUTES:
        if kind == "f32":
            value = struct.unpack_from(">f", data, special_offset + offset)[0]
            special[f"{name}_q16"] = q16(value)
        elif kind == "i32":
            special[name] = struct.unpack_from(">i", data, special_offset + offset)[0]
        elif kind == "u32":
            special[name] = struct.unpack_from(">I", data, special_offset + offset)[0]
        elif kind == "bits":
            special[f"{name}_bits"] = struct.unpack_from(
                ">I", data, special_offset + offset
            )[0]
        else:
            raise AssertionError(f"unsupported special attribute kind {kind}")
    if len(common_bits) != COMMON_ATTRIBUTE_COUNT or len(special) != 35:
        raise ValueError("incomplete Falcon attribute decode")
    return common_bits, special


def source_common_special_attributes(common_dat: bytes) -> dict[str, int]:
    """Decode the common-data fields used by SpecialS input selection."""

    if len(common_dat) < 0x20:
        raise ValueError("truncated PlCo.dat header")
    data_size, relocation_count, root_count, reference_count = struct.unpack_from(
        ">4I", common_dat, 0x04
    )
    if root_count != 1 or reference_count != 0:
        raise ValueError("unexpected PlCo.dat root/reference table")
    root_table = 0x20 + data_size + relocation_count * 4
    if root_table + 8 > len(common_dat):
        raise ValueError("PlCo.dat root table is out of bounds")
    root_offset, root_name_offset = struct.unpack_from(">2I", common_dat, root_table)
    string_table = root_table + root_count * 8 + reference_count * 8
    name_start = string_table + root_name_offset
    name_end = common_dat.find(b"\0", name_start)
    if name_end < 0 or common_dat[name_start:name_end] != b"ftLoadCommonData":
        raise ValueError("unexpected PlCo.dat root name")
    data = common_dat[0x20 : 0x20 + data_size]
    if root_offset + 4 > len(data):
        raise ValueError("ftLoadCommonData root is out of bounds")
    common_offset = struct.unpack_from(">I", data, root_offset)[0]
    if common_offset + 0x25C > len(data):
        raise ValueError("ftCommonData is out of bounds")
    return {
        "air_drift_over_maximum_deceleration_q16": round(
            struct.unpack_from(">f", data, common_offset + 0x1FC)[0]
            * MELEE_X_TO_SIM_Q16
        ),
        "side_special_stick_threshold_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x218)[0]
        ),
        "side_special_turn_threshold_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x220)[0]
        ),
        "air_drift_dead_zone_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x258)[0]
        ),
    }


def command_variable_assignments(
    subactions: list[dict[str, Any]], subaction_index: int
) -> dict[tuple[int, int], int]:
    """Return exact displayed frames for action command-variable writes."""

    frame = 0
    assignments: dict[tuple[int, int], int] = {}
    for event in subactions[subaction_index].get("events", []):
        command_id = int(str(event["commandId"]), 16)
        fields = event.get("fields") or {}
        if command_id == 0x08:
            frame = int(fields["frame"])
        elif command_id == 0x04:
            frame += int(fields["frames"])
        command = bytes.fromhex(str(event["bytes"]))
        if len(command) == 4 and command[0] in (0x4C, 0x4D, 0x4E):
            assignments[(command[0], command[3])] = frame
    return assignments


def throw_release_frame(
    dat_data: dict[str, Any], subaction_index: int, subaction_name: str
) -> int:
    """Read the release flag's exact action-script frame.

    meleeDat2Json calls opcode 0x14 (encoded first byte 0x50)
    ``reverseDirection``. The NTSC 1.02 decomp dispatches that opcode to
    ftAction_800718A4: argument zero raises throw_flags_b3, which
    ftCo_800DD724 consumes to release the victim. Argument one reverses
    facing instead. Reading the encoded argument avoids relying on the
    upstream display label.
    """
    subactions = dat_data["nodes"][0]["data"]["subactions"]
    subaction = subactions[subaction_index]
    if str(subaction["name"]) != subaction_name:
        raise ValueError(f"subaction {subaction_index}: name mismatch")

    frame = 0
    releases: list[int] = []
    for event in subaction.get("events", []):
        command_id = int(str(event["commandId"]), 16)
        fields = event.get("fields") or {}
        if command_id == 0x08:
            frame = int(fields["frame"])
        elif command_id == 0x04:
            frame += int(fields["frames"])
        elif command_id == 0x50:
            encoded = bytes.fromhex(str(event["bytes"]))
            argument = int.from_bytes(encoded, "big") & ((1 << 26) - 1)
            if argument == 0:
                releases.append(frame)
    if len(releases) != 1:
        raise ValueError(
            f"subaction {subaction_index}: expected one throw release, "
            f"found {releases}"
        )
    return releases[0]


def generate(
    data: dict[str, Any],
    dat_data: dict[str, Any],
    source_dat: bytes,
    animation_dat: bytes,
    common_dat: bytes,
) -> str:
    phases: list[tuple[int, int, int]] = []
    effects: list[dict[str, Any]] = []
    throws: list[dict[str, Any]] = []
    moves: list[dict[str, int]] = []
    motion_x_q16: list[int] = []
    motion_y_q16: list[int] = []

    fighter_data = dat_data["nodes"][0]["data"]
    subactions = fighter_data["subactions"]
    subactions_offset = int(fighter_data["subactionsOffset"])
    attributes = {
        str(attribute["name"]): attribute["value"]
        for attribute in fighter_data["attributes"]
    }
    model_scaling = float(attributes["modelScaling"])
    source_dat_block = source_dat[0x20:]
    common_attribute_bits, special_attributes = source_attributes(source_dat)

    special_air_n_assignments = command_variable_assignments(subactions, 302)
    try:
        specialn_launch_frame = special_air_n_assignments[(0x4C, 1)]
        specialn_scale_begin_frame = special_air_n_assignments[(0x4D, 1)]
        specialn_air_physics_begin_frame = special_air_n_assignments[(0x4D, 2)]
    except KeyError as error:
        raise ValueError("incomplete SpecialAirN command-variable timeline") from error
    if not (
        specialn_launch_frame == specialn_scale_begin_frame
        and specialn_scale_begin_frame < specialn_air_physics_begin_frame
    ):
        raise ValueError("invalid SpecialAirN command-variable ordering")
    special_s_ground_assignments = command_variable_assignments(subactions, 303)
    special_s_air_assignments = command_variable_assignments(subactions, 305)
    try:
        specials_ground_search_begin = special_s_ground_assignments[(0x4C, 1)]
        specials_ground_search_end = special_s_ground_assignments[(0x4C, 0)] - 1
        specials_air_search_begin = special_s_air_assignments[(0x4C, 1)]
        specials_air_search_end = special_s_air_assignments[(0x4C, 0)] - 1
        specials_air_gravity_begin = special_s_air_assignments[(0x4D, 1)]
    except KeyError as error:
        raise ValueError("incomplete SpecialS command-variable timeline") from error
    if not (
        specials_ground_search_begin <= specials_ground_search_end
        and specials_air_search_begin <= specials_air_search_end
        and specials_air_gravity_begin >= specials_air_search_begin
    ):
        raise ValueError("invalid SpecialS command-variable ordering")
    common_special_attributes = source_common_special_attributes(common_dat)
    special_hi_ground_assignments = command_variable_assignments(subactions, 307)
    special_hi_air_assignments = command_variable_assignments(subactions, 308)
    special_hi_throw_assignments = command_variable_assignments(subactions, 310)
    try:
        specialhi_ground_control_begin = special_hi_ground_assignments[(0x4C, 1)]
        specialhi_air_control_begin = special_hi_air_assignments[(0x4C, 1)]
        specialhi_throw_gravity_begin = special_hi_throw_assignments[(0x4C, 1)]
    except KeyError as error:
        raise ValueError("incomplete SpecialHi command-variable timeline") from error
    if not (
        specialhi_ground_control_begin == specialhi_air_control_begin
        and specialhi_ground_control_begin > 0
        and specialhi_throw_gravity_begin > 0
    ):
        raise ValueError("invalid SpecialHi command-variable ordering")
    common_attributes = {
        "initial_walk_velocity_q16": round(
            raw_f32(common_attribute_bits, 0) * MELEE_X_TO_SIM_Q16
        ),
        "walk_acceleration_q16": round(
            raw_f32(common_attribute_bits, 1) * MELEE_X_TO_SIM_Q16
        ),
        "walk_maximum_velocity_q16": round(
            raw_f32(common_attribute_bits, 2) * MELEE_X_TO_SIM_Q16
        ),
        "friction_q16": round(raw_f32(common_attribute_bits, 6) * MELEE_X_TO_SIM_Q16),
        "dash_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 7) * MELEE_X_TO_SIM_Q16
        ),
        "dash_run_acceleration_a_q16": round(
            raw_f32(common_attribute_bits, 8) * MELEE_X_TO_SIM_Q16
        ),
        "dash_run_acceleration_b_q16": round(
            raw_f32(common_attribute_bits, 9) * MELEE_X_TO_SIM_Q16
        ),
        "dash_run_terminal_velocity_q16": round(
            raw_f32(common_attribute_bits, 10) * MELEE_X_TO_SIM_Q16
        ),
        "ground_maximum_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 13) * MELEE_X_TO_SIM_Q16
        ),
        "jump_horizontal_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 15) * MELEE_X_TO_SIM_Q16
        ),
        "jump_vertical_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 16) * MELEE_Y_TO_SIM_Q16
        ),
        "ground_air_jump_momentum_multiplier_q16": q16(
            raw_f32(common_attribute_bits, 17)
        ),
        "jump_horizontal_maximum_velocity_q16": round(
            raw_f32(common_attribute_bits, 18) * MELEE_X_TO_SIM_Q16
        ),
        "shorthop_vertical_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 19) * MELEE_Y_TO_SIM_Q16
        ),
        "air_jump_multiplier_q16": q16(raw_f32(common_attribute_bits, 20)),
        "double_jump_momentum_q16": q16(raw_f32(common_attribute_bits, 21)),
        "double_jump_vertical_velocity_q16": round(
            raw_f32(common_attribute_bits, 16)
            * raw_f32(common_attribute_bits, 20)
            * MELEE_Y_TO_SIM_Q16
        ),
        "double_jump_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 21) * MELEE_X_TO_SIM_Q16
        ),
        "gravity_q16": round(raw_f32(common_attribute_bits, 23) * MELEE_Y_TO_SIM_Q16),
        "terminal_velocity_q16": round(
            raw_f32(common_attribute_bits, 24) * MELEE_Y_TO_SIM_Q16
        ),
        "air_mobility_a_q16": round(
            raw_f32(common_attribute_bits, 25) * MELEE_X_TO_SIM_Q16
        ),
        "air_mobility_b_q16": round(
            raw_f32(common_attribute_bits, 26) * MELEE_X_TO_SIM_Q16
        ),
        "max_aerial_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 27) * MELEE_X_TO_SIM_Q16
        ),
        "air_friction_q16": round(
            raw_f32(common_attribute_bits, 28) * MELEE_X_TO_SIM_Q16
        ),
        "fast_fall_terminal_velocity_q16": round(
            raw_f32(common_attribute_bits, 29) * MELEE_Y_TO_SIM_Q16
        ),
        "maximum_horizontal_air_velocity_q16": round(
            raw_f32(common_attribute_bits, 30) * MELEE_X_TO_SIM_Q16
        ),
        "shield_break_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 37) * MELEE_Y_TO_SIM_Q16
        ),
        "ledge_jump_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 42) * MELEE_X_TO_SIM_Q16
        ),
        "ledge_jump_vertical_velocity_q16": round(
            raw_f32(common_attribute_bits, 43) * MELEE_Y_TO_SIM_Q16
        ),
        "wall_jump_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 65) * MELEE_X_TO_SIM_Q16
        ),
        "wall_jump_vertical_velocity_q16": round(
            raw_f32(common_attribute_bits, 66) * MELEE_Y_TO_SIM_Q16
        ),
        "jump_startup_ticks": round(raw_f32(common_attribute_bits, 14)),
        "number_of_jumps": common_attribute_bits[22],
        "turn_duration_ticks": round(raw_f32(common_attribute_bits, 33)),
        "weight": round(raw_f32(common_attribute_bits, 34)),
        "normal_landing_lag_ticks": round(raw_f32(common_attribute_bits, 57)),
        "neutral_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 58)),
        "forward_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 59)),
        "back_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 60)),
        "up_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 61)),
        "down_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 62)),
    }

    complete_source_digest = hashlib.sha256(
        bytes.fromhex(EXPECTED_CANONICAL_SHA256)
        + bytes.fromhex(SOURCE_DAT_SHA256)
        + bytes.fromhex(SOURCE_ANIMATION_DAT_SHA256)
        + bytes.fromhex(SOURCE_COMMON_DAT_SHA256)
        + b"".join(value.to_bytes(4, "big") for value in common_attribute_bits)
        + json.dumps(
            {
                "common_special": common_special_attributes,
                "fighter_special": special_attributes,
                "falcon_dive_grounded_throw_reposition_melee": {
                    "x": SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE,
                    "y": SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE,
                },
                "falcon_falling_collision_pose_melee": {
                    "bottom_y_from_origin": FALLING_ECB_BOTTOM_Y_MELEE,
                },
            },
            sort_keys=True,
            separators=(",", ":"),
        ).encode("ascii")
    ).hexdigest()

    if tuple(data) != MOVE_KEYS:
        raise ValueError("unexpected move order or incomplete Falcon table")

    for key in MOVE_KEYS:
        move = data[key]
        if move is None:
            if key not in EXPECTED_ABSENT_MOVE_KEYS:
                raise ValueError(f"{key}: incomplete Falcon frame-data row")
            moves.append({"present": 0})
            continue
        if key in EXPECTED_ABSENT_MOVE_KEYS:
            raise ValueError(f"{key}: unexpected Falcon frame-data row")

        phase_offset = len(phases)
        effect_offset = len(effects)
        move_effects = move.get("hitboxes", [])
        if len(move_effects) > 16:
            raise ValueError(f"{key}: too many hit effects")
        effects.extend(move_effects)
        for phase in move.get("hitFrames", []):
            mask = 0
            for effect_index in phase["hitboxes"]:
                if not 0 <= int(effect_index) < len(move_effects):
                    raise ValueError(f"{key}: invalid hit-effect index")
                mask |= 1 << int(effect_index)
            phases.append((int(phase["start"]), int(phase["end"]), mask))

        throw_index = 0xFFFF
        if "throw" in move:
            throw_index = len(throws)
            throw = dict(move["throw"])
            throw["releaseFrame"] = (
                throw_release_frame(
                    dat_data,
                    int(move["subactionIndex"]),
                    str(move["subactionName"]),
                )
                if key in {"fthrow", "bthrow", "uthrow", "dthrow"}
                else 0
            )
            throws.append(throw)
        subaction_index = int(move["subactionIndex"])
        action_flags = struct.unpack_from(
            ">I",
            source_dat_block,
            subactions_offset + subaction_index * 0x18 + 0x10,
        )[0]
        motion_offset = len(motion_x_q16)
        if action_flags & ANIMATION_TRANSLATION_FLAG:
            subaction = subactions[subaction_index]
            animation_offset = int(subaction["animOffset"])
            animation_size = int(subaction["animSize"])
            tree = decode_figatree(
                animation_dat[animation_offset : animation_offset + animation_size]
            )
            translation_node = (action_flags & 0xFF) - 1
            if not 0 <= translation_node < len(tree.nodes):
                raise ValueError(f"{key}: invalid translation node {translation_node}")
            translation_x_tracks = [
                track
                for track in tree.nodes[translation_node]
                if track.track_type == TRACK_TRANSLATE_Z
            ]
            translation_y_tracks = [
                track
                for track in tree.nodes[translation_node]
                if track.track_type == TRACK_TRANSLATE_Y
            ]
            if len(translation_x_tracks) != 1:
                raise ValueError(
                    f"{key}: expected one translation-Z track, "
                    f"found {len(translation_x_tracks)}"
                )
            if len(translation_y_tracks) > 1:
                raise ValueError(
                    f"{key}: expected at most one translation-Y track, "
                    f"found {len(translation_y_tracks)}"
                )
            positions_x = [
                sample_track(translation_x_tracks[0], float(frame))
                for frame in range(int(move["totalFrames"]) + 1)
            ]
            motion_x_q16.extend(
                round(
                    (positions_x[frame] - positions_x[frame - 1])
                    * model_scaling
                    * MELEE_X_TO_SIM_Q16
                )
                for frame in range(1, len(positions_x))
            )
            positions_y = (
                [
                    sample_track(translation_y_tracks[0], float(frame))
                    for frame in range(int(move["totalFrames"]) + 1)
                ]
                if translation_y_tracks
                else [0.0] * (int(move["totalFrames"]) + 1)
            )
            motion_y_q16.extend(
                round(
                    -(positions_y[frame] - positions_y[frame - 1])
                    * model_scaling
                    * MELEE_Y_TO_SIM_Q16
                )
                for frame in range(1, len(positions_y))
            )
        if len(motion_x_q16) != len(motion_y_q16):
            raise ValueError(f"{key}: mismatched translation table lengths")
        moves.append(
            {
                "present": 1,
                "subaction": int(move["subactionIndex"]),
                "total": int(move["totalFrames"]),
                "iasa": u16(move.get("iasa")),
                "charge": u16(move.get("chargeFrame")),
                "autocancel_before": u16(move.get("autoCancelBefore")),
                "autocancel_after": u16(move.get("autoCancelAfter")),
                "landing": u16(move.get("landingLag")),
                "l_cancelled": u16(move.get("lcancelledLandingLag")),
                "phase_offset": phase_offset,
                "effect_offset": effect_offset,
                "throw_index": throw_index,
                "phase_count": len(move.get("hitFrames", [])),
                "effect_count": len(move_effects),
                "animation_flags": action_flags,
                "motion_offset": motion_offset,
                "motion_count": len(motion_x_q16) - motion_offset,
            }
        )

    lines = [
        "/* Generated by tools/import_ssbm_falcon_frame_data.py; do not edit. */",
        f"/* canonical source SHA-256: {EXPECTED_CANONICAL_SHA256} */",
        f"/* extractor revision: {EXTRACTOR_REVISION} */",
        f"/* DAT reader revision: {DAT_READER_REVISION} */",
        f"/* HSD animation reader revision: {HSD_READER_REVISION} */",
        f"/* PlCa.dat SHA-256: {SOURCE_DAT_SHA256} */",
        f"/* PlCaAJ.dat SHA-256: {SOURCE_ANIMATION_DAT_SHA256} */",
        f"/* PlCa.dat JSON SHA-256: {SOURCE_DAT_JSON_SHA256} */",
        f"/* PlCo.dat SHA-256: {SOURCE_COMMON_DAT_SHA256} */",
        f"/* complete Falcon source SHA-256: {complete_source_digest} */",
        "",
        "static const uint8_t pf_m4_falcon_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{EXPECTED_CANONICAL_SHA256[index:index + 2]})"
            for index in range(0, len(EXPECTED_CANONICAL_SHA256), 2)
        ),
        "};",
        "",
        "static const uint8_t pf_m4_falcon_complete_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{complete_source_digest[index:index + 2]})"
            for index in range(0, len(complete_source_digest), 2)
        ),
        "};",
        "",
        "static const uint32_t",
        "pf_m4_falcon_common_attribute_bits[PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT] = {",
    ]
    lines.extend(
        "    "
        + ", ".join(
            f"UINT32_C(0x{value:08x})"
            for value in common_attribute_bits[index : index + 8]
        )
        + ","
        for index in range(0, len(common_attribute_bits), 8)
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_common_attributes",
            "pf_m4_falcon_common_attribute_data = {",
        )
    )
    lines.extend(
        (
            f"    .{name} = UINT16_C({value}),"
            if name.endswith("_ticks") or name in {"number_of_jumps", "weight"}
            else f"    .{name} = INT32_C({value}),"
        )
        for name, value in common_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_common_special_attributes",
            "pf_m4_falcon_common_special_attribute_data = {",
        )
    )
    lines.extend(
        f"    .{name} = INT32_C({value}),"
        for name, value in common_special_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_special_attributes",
            "pf_m4_falcon_special_attribute_data = {",
        )
    )
    lines.extend(
        (
            f"    .{name} = UINT32_C({value}),"
            if name in SPECIAL_UNSIGNED_FIELDS
            else f"    .{name} = INT32_C({value}),"
        )
        for name, value in special_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_neutral_special_timing",
            "pf_m4_falcon_neutral_special_timing_data = {",
            f"    .launch_frame = UINT16_C({specialn_launch_frame}),",
            "    .velocity_scale_begin_frame = "
            f"UINT16_C({specialn_scale_begin_frame}),",
            "    .velocity_scale_end_frame = "
            f"UINT16_C({specialn_air_physics_begin_frame - 1}),",
            "    .ordinary_air_physics_begin_frame = "
            f"UINT16_C({specialn_air_physics_begin_frame}),",
            "};",
            "",
            "static const pf_m4_falcon_side_special_timing",
            "pf_m4_falcon_side_special_timing_data = {",
            "    .ground_search_begin_frame = "
            f"UINT16_C({specials_ground_search_begin}),",
            "    .ground_search_end_frame = "
            f"UINT16_C({specials_ground_search_end}),",
            "    .air_search_begin_frame = "
            f"UINT16_C({specials_air_search_begin}),",
            "    .air_search_end_frame = "
            f"UINT16_C({specials_air_search_end}),",
            "    .air_gravity_begin_frame = "
            f"UINT16_C({specials_air_gravity_begin}),",
            "};",
            "",
            "static const pf_m4_falcon_up_special_timing",
            "pf_m4_falcon_up_special_timing_data = {",
            "    .air_control_begin_frame = "
            f"UINT16_C({specialhi_ground_control_begin}),",
            "    .throw_gravity_begin_frame = "
            f"UINT16_C({specialhi_throw_gravity_begin}),",
            "    .grounded_throw_reposition_x_q16 = "
            "INT32_C("
            f"{round(SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE * MELEE_X_TO_SIM_Q16)}"
            "),",
            "    .grounded_throw_reposition_y_q16 = "
            "INT32_C("
            f"{round(-SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE * MELEE_Y_TO_SIM_Q16)}"
            "),",
            "};",
            "",
            "static const pf_m4_falcon_collision_pose",
            "pf_m4_falcon_collision_pose_data = {",
            "    .falling_bottom_y_from_origin_q16 = INT32_C("
            f"{round(FALLING_ECB_BOTTOM_Y_MELEE * MELEE_Y_TO_SIM_Q16)}"
            "),",
            "};",
            "",
            "static const pf_m4_reference_hit_phase pf_m4_falcon_hit_phases[] = {",
        )
    )
    lines.extend(
        f"    {{ UINT16_C({start}), UINT16_C({end}), UINT16_C({mask}), UINT16_C(0) }},"
        for start, end, mask in phases
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_hit_effect pf_m4_falcon_hit_effects[] = {",
        )
    )
    for effect in effects:
        element = ELEMENTS[str(effect["element"])]
        lines.append(
            "    { "
            f"UINT16_C({int(effect['angle'])}), "
            f"UINT16_C({int(effect['kbGrowth'])}), "
            f"UINT16_C({int(effect['weightDepKb'])}), "
            f"UINT16_C({int(effect['baseKb'])}), "
            f"UINT8_C({int(effect['damage'])}), "
            f"UINT8_C({int(effect['shieldDamage'])}), "
            f"UINT8_C({int(effect['hitboxInteraction'])}), "
            f"(uint8_t){element}, "
            f"UINT8_C({1 if effect['hitGrounded'] else 0}), "
            f"UINT8_C({1 if effect['hitAirborne'] else 0}), "
            "{ UINT8_C(0), UINT8_C(0) } },"
        )
    lines.extend(
        ("};", "", "static const pf_m4_reference_throw pf_m4_falcon_throws[] = {")
    )
    for throw in throws:
        lines.append(
            "    { "
            f"UINT16_C({int(throw['angle'])}), "
            f"UINT16_C({int(throw['kbGrowth'])}), "
            f"UINT16_C({int(throw['weightDepKb'])}), "
            f"UINT16_C({int(throw['baseKb'])}), "
            f"UINT8_C({int(throw['damage'])}), "
            f"UINT8_C({int(throw['element'])}), "
            f"UINT16_C({int(throw['releaseFrame'])}), "
            "UINT16_C(0) },"
        )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_reference_move pf_m4_falcon_moves[PF_M4_FALCON_MOVE_COUNT] = {",
        )
    )
    for move in moves:
        if move["present"] == 0:
            lines.append("    { 0 },")
            continue
        lines.append(
            "    { "
            f"UINT16_C({move['subaction']}), UINT16_C({move['total']}), "
            f"UINT16_C({move['iasa']}), UINT16_C({move['charge']}), "
            f"UINT16_C({move['autocancel_before']}), "
            f"UINT16_C({move['autocancel_after']}), "
            f"UINT16_C({move['landing']}), UINT16_C({move['l_cancelled']}), "
            f"UINT16_C({move['phase_offset']}), UINT16_C({move['effect_offset']}), "
            f"UINT16_C({move['throw_index']}), UINT8_C({move['phase_count']}), "
            f"UINT8_C({move['effect_count']}), UINT8_C(1), UINT8_C(0), "
            f"UINT32_C({move['animation_flags']}), "
            f"UINT16_C({move['motion_offset']}), "
            f"UINT16_C({move['motion_count']}) }},"
        )
    lines.extend(("};", "", "static const int32_t pf_m4_falcon_motion_x_q16[] = {"))
    lines.extend(
        "    "
        + ", ".join(f"INT32_C({value})" for value in motion_x_q16[index : index + 8])
        + ","
        for index in range(0, len(motion_x_q16), 8)
    )
    lines.extend(("};", "", "static const int32_t pf_m4_falcon_motion_y_q16[] = {"))
    lines.extend(
        "    "
        + ", ".join(f"INT32_C({value})" for value in motion_y_q16[index : index + 8])
        + ","
        for index in range(0, len(motion_y_q16), 8)
    )
    lines.extend(("};", ""))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("dat_source", type=Path)
    parser.add_argument("source_dat", type=Path)
    parser.add_argument("animation_dat", type=Path)
    parser.add_argument("common_dat", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.source.read_text(encoding="utf-8"))
    dat_bytes = args.dat_source.read_bytes()
    dat_digest = hashlib.sha256(dat_bytes).hexdigest()
    if dat_digest != SOURCE_DAT_JSON_SHA256:
        raise SystemExit(f"unexpected Falcon DAT JSON SHA-256: {dat_digest}")
    dat_data = json.loads(dat_bytes)
    source_dat = args.source_dat.read_bytes()
    source_dat_digest = hashlib.sha256(source_dat).hexdigest()
    if source_dat_digest != SOURCE_DAT_SHA256:
        raise SystemExit(f"unexpected PlCa.dat SHA-256: {source_dat_digest}")
    animation_dat = args.animation_dat.read_bytes()
    animation_dat_digest = hashlib.sha256(animation_dat).hexdigest()
    if animation_dat_digest != SOURCE_ANIMATION_DAT_SHA256:
        raise SystemExit("unexpected PlCaAJ.dat SHA-256: " f"{animation_dat_digest}")
    common_dat = args.common_dat.read_bytes()
    common_dat_digest = hashlib.sha256(common_dat).hexdigest()
    if common_dat_digest != SOURCE_COMMON_DAT_SHA256:
        raise SystemExit(f"unexpected PlCo.dat SHA-256: {common_dat_digest}")
    digest = canonical_sha256(data)
    if digest != EXPECTED_CANONICAL_SHA256:
        raise SystemExit(f"unexpected Falcon frame-data SHA-256: {digest}")
    output = generate(data, dat_data, source_dat, animation_dat, common_dat)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-falcon-frame-data=pass "
        f"slots={len(MOVE_KEYS)} "
        f"subactions={sum(data[key] is not None for key in MOVE_KEYS)} "
        f"source_sha256={digest} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
