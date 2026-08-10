#!/usr/bin/env python3
"""Generate common NTSC 1.02 fighter data from the owner-supplied PlCo.dat."""

from __future__ import annotations

import argparse
import hashlib
import math
from pathlib import Path
import struct

from ssbm_dat import ft_common_data


SOURCE_COMMON_DAT_SHA256 = (
    "63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c"
)
DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"
FT_COMMON_DATA_SIZE = 0x818
MELEE_X_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0
MELEE_Y_TO_SIM_Q16 = 65536.0 * 11.0 / 62.0


def q16(value: float) -> int:
    return round(value * 65536.0)


def generate(raw: bytes) -> str:
    data, pointers = ft_common_data(raw)
    common_offset = pointers[0]
    if common_offset + FT_COMMON_DATA_SIZE > len(data):
        raise ValueError("truncated ftCommonData object")
    raw_common = data[common_offset : common_offset + FT_COMMON_DATA_SIZE]
    raw_words = struct.unpack(f">{FT_COMMON_DATA_SIZE // 4}I", raw_common)

    def f32(offset: int) -> float:
        return struct.unpack_from(">f", raw_common, offset)[0]

    def i32(offset: int) -> int:
        return struct.unpack_from(">i", raw_common, offset)[0]

    tilt_x = f32(0x008)
    tilt_y = f32(0x00C)
    if tilt_x != tilt_y or not 0.0 < tilt_x < 1.0:
        raise ValueError("unexpected asymmetric common stick-tilt threshold")
    jump_backward_axis_threshold = f32(0x078)
    if not 0.0 < jump_backward_axis_threshold < 1.0:
        raise ValueError("invalid common backward-jump axis threshold")
    sdi_threshold = f32(0x4B0)
    sdi_window = i32(0x4B4)
    if not 0.0 < sdi_threshold <= 1.0:
        raise ValueError("invalid common SDI stick threshold")
    if sdi_window <= 0 or sdi_window > 0xFFFF:
        raise ValueError("common SDI window does not fit uint16_t")
    wall_tech_stall_ticks = i32(0x760)
    wall_tech_invulnerability_ticks = i32(0x764)
    bounce_invulnerability_ticks = i32(0x1B8)
    bounce_collision_lockout = f32(0x1C0)
    tech_lockout_ticks = i32(0x01C)
    tech_window_ticks = f32(0x250)
    tech_roll_axis_threshold = f32(0x254)
    down_wait_ticks = f32(0x424)
    down_horizontal_angle = f32(0x020)
    down_up_axis_threshold = f32(0x244)
    down_horizontal_axis_threshold = f32(0x248)
    down_attack_input_window_ticks = f32(0x24C)
    down_c_up_axis_threshold = f32(0x7F4)
    ledge_damage_threshold = i32(0x488)
    ledge_quick_wait_ticks = f32(0x48C)
    ledge_slow_wait_ticks = f32(0x490)
    ledge_stick_axis_threshold = f32(0x494)
    ledge_regrab_cooldown_ticks = i32(0x498)
    ledge_wait_invulnerability_ticks = i32(0x49C)
    ledge_c_attack_axis_threshold = f32(0x7F8)
    ledge_c_roll_axis_threshold = f32(0x7FC)
    if (
        not 0 < wall_tech_stall_ticks <= 0xFFFF
        or not 0 < wall_tech_invulnerability_ticks <= 0xFFFF
        or not 0 < bounce_invulnerability_ticks <= 0xFFFF
        or not bounce_collision_lockout.is_integer()
        or not 0 < bounce_collision_lockout <= 0xFFFF
        or not 0 < tech_lockout_ticks <= 0xFFFF
        or not tech_window_ticks.is_integer()
        or not 0 < tech_window_ticks <= 0xFFFF
        or not 0.0 < tech_roll_axis_threshold <= 1.0
        or not down_wait_ticks.is_integer()
        or not 0 < down_wait_ticks <= 0xFFFF
        or not 0.0 < down_horizontal_angle < math.pi / 2.0
        or not 0.0 < down_up_axis_threshold <= 1.0
        or not 0.0 < down_horizontal_axis_threshold <= 1.0
        or not down_attack_input_window_ticks.is_integer()
        or not 0 < down_attack_input_window_ticks <= 0xFFFF
        or not 0.0 < down_c_up_axis_threshold <= 1.0
        or not 0 < ledge_damage_threshold <= 0xFFFF
        or not ledge_quick_wait_ticks.is_integer()
        or not 0 < ledge_quick_wait_ticks <= 0xFFFF
        or not ledge_slow_wait_ticks.is_integer()
        or not 0 < ledge_slow_wait_ticks <= 0xFFFF
        or not 0.0 < ledge_stick_axis_threshold <= 1.0
        or not 0 < ledge_regrab_cooldown_ticks <= 0xFFFF
        or not 0 < ledge_wait_invulnerability_ticks <= 0xFFFF
        or not 0.0 < ledge_c_attack_axis_threshold <= 1.0
        or not 0.0 < ledge_c_roll_axis_threshold <= 1.0
    ):
        raise ValueError("common surface-response timing does not fit uint16_t")

    attributes = {
        "stick_tilt_threshold": round(tilt_x * 32767.0),
        "hitstun_per_knockback_q16": q16(f32(0x154)),
        "launch_speed_x_per_knockback_q16": round(
            f32(0x100) * MELEE_X_TO_SIM_Q16
        ),
        "launch_speed_y_per_knockback_q16": round(
            f32(0x100) * MELEE_Y_TO_SIM_Q16
        ),
        "sakurai_air_angle_degrees_q16": q16(math.degrees(f32(0x144))),
        "sakurai_max_ground_angle_degrees_q16": q16(f32(0x148)),
        "sakurai_low_knockback_q16": q16(f32(0x14C)),
        "sakurai_high_knockback_q16": q16(f32(0x150)),
        "damage_level_1_threshold_q16": q16(f32(0x158)),
        "damage_level_2_threshold_q16": q16(f32(0x15C)),
        "grounded_damage_max_level_q16": q16(f32(0x160)),
        "ground_knockback_max_speed_q16": round(
            f32(0x164) * MELEE_X_TO_SIM_Q16
        ),
        "di_max_angle_radians_q30": round(
            math.radians(f32(0x1A8)) * float(1 << 30)
        ),
        "ground_knockback_decay_scale_q16": q16(f32(0x200)),
        # Knockback decay is a magnitude subtraction in Melee coordinates.
        # Keep the source scalar: per-axis conversion before the vector
        # operation changes the angle under this project's anisotropic scale.
        "air_knockback_decay_q16": q16(f32(0x204)),
        "sdi_stick_threshold": round(sdi_threshold * 32767.0),
        "sdi_stick_window_ticks": sdi_window,
        "sdi_distance_x_q16": round(f32(0x4B8) * MELEE_X_TO_SIM_Q16),
        "sdi_distance_y_q16": round(f32(0x4B8) * MELEE_Y_TO_SIM_Q16),
        "asdi_distance_x_q16": round(f32(0x4BC) * MELEE_X_TO_SIM_Q16),
        "asdi_distance_y_q16": round(f32(0x4BC) * MELEE_Y_TO_SIM_Q16),
        "shield_sdi_scale_q16": q16(f32(0x4C0)),
    }
    surface_attributes = {
        "collision_threshold_x_q16": round(f32(0x1B0) * MELEE_X_TO_SIM_Q16),
        "collision_threshold_y_q16": round(f32(0x1B0) * MELEE_Y_TO_SIM_Q16),
        "bounce_multiplier_q16": q16(f32(0x1BC)),
        "wall_tech_stall_ticks": wall_tech_stall_ticks,
        "wall_tech_invulnerability_ticks": wall_tech_invulnerability_ticks,
        "bounce_invulnerability_ticks": bounce_invulnerability_ticks,
        "bounce_collision_lockout_ticks": int(bounce_collision_lockout),
        "tech_window_ticks": int(tech_window_ticks),
        "tech_lockout_ticks": tech_lockout_ticks,
        "tech_roll_axis_threshold": round(
            tech_roll_axis_threshold * 32767.0
        ),
        "down_wait_ticks": int(down_wait_ticks),
        "down_horizontal_angle_tan_q16": q16(
            math.tan(down_horizontal_angle)
        ),
        "down_up_axis_threshold": round(
            down_up_axis_threshold * 32767.0
        ),
        "down_horizontal_axis_threshold": round(
            down_horizontal_axis_threshold * 32767.0
        ),
        "down_attack_input_window_ticks": int(
            down_attack_input_window_ticks
        ),
        "down_c_up_axis_threshold": round(
            down_c_up_axis_threshold * 32767.0
        ),
    }
    ledge_attributes = {
        "direction_angle_tan_q16": q16(math.tan(down_horizontal_angle)),
        "damage_threshold_percent": ledge_damage_threshold,
        "quick_wait_ticks": int(ledge_quick_wait_ticks),
        "slow_wait_ticks": int(ledge_slow_wait_ticks),
        "stick_axis_threshold": round(
            ledge_stick_axis_threshold * 32767.0
        ),
        "regrab_cooldown_ticks": ledge_regrab_cooldown_ticks,
        "wait_invulnerability_ticks": ledge_wait_invulnerability_ticks,
        "c_attack_axis_threshold": round(
            ledge_c_attack_axis_threshold * 32767.0
        ),
        "c_roll_axis_threshold": round(
            ledge_c_roll_axis_threshold * 32767.0
        ),
    }

    lines = [
        "/* Generated by tools/import_ssbm_common_data.py; do not edit. */",
        f"/* PlCo.dat SHA-256: {SOURCE_COMMON_DAT_SHA256} */",
        f"/* doldecomp/melee revision: {DECOMP_REVISION} */",
        "",
        "static const uint8_t pf_m4_ssbm_common_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{SOURCE_COMMON_DAT_SHA256[index:index + 2]})"
            for index in range(0, 64, 2)
        ),
        "};",
        "",
        "static const uint32_t",
        "pf_m4_ssbm_common_raw_words[PF_M4_SSBM_COMMON_RAW_WORD_COUNT] = {",
    ]
    lines.extend(
        "    "
        + ", ".join(
            f"UINT32_C(0x{word:08x})" for word in raw_words[index : index + 6]
        )
        + ","
        for index in range(0, len(raw_words), 6)
    )
    lines.extend(
        [
            "};",
            "",
            "static const uint16_t",
            "pf_m4_ssbm_jump_backward_axis_threshold =",
            f"    UINT16_C({round(jump_backward_axis_threshold * 32767.0)});",
            "",
            "static const pf_m4_ssbm_damage_response_attributes",
            "pf_m4_ssbm_damage_response_attribute_data = {",
            f"    INT32_C({attributes['hitstun_per_knockback_q16']}),",
            f"    INT32_C({attributes['launch_speed_x_per_knockback_q16']}),",
            f"    INT32_C({attributes['launch_speed_y_per_knockback_q16']}),",
            f"    INT32_C({attributes['sakurai_air_angle_degrees_q16']}),",
            f"    INT32_C({attributes['sakurai_max_ground_angle_degrees_q16']}),",
            f"    INT32_C({attributes['sakurai_low_knockback_q16']}),",
            f"    INT32_C({attributes['sakurai_high_knockback_q16']}),",
            f"    INT32_C({attributes['damage_level_1_threshold_q16']}),",
            f"    INT32_C({attributes['damage_level_2_threshold_q16']}),",
            f"    INT32_C({attributes['grounded_damage_max_level_q16']}),",
            f"    INT32_C({attributes['ground_knockback_max_speed_q16']}),",
            f"    INT32_C({attributes['di_max_angle_radians_q30']}),",
            f"    INT32_C({attributes['ground_knockback_decay_scale_q16']}),",
            f"    INT32_C({attributes['air_knockback_decay_q16']}),",
            f"    INT32_C({attributes['sdi_distance_x_q16']}),",
            f"    INT32_C({attributes['sdi_distance_y_q16']}),",
            f"    INT32_C({attributes['asdi_distance_x_q16']}),",
            f"    INT32_C({attributes['asdi_distance_y_q16']}),",
            f"    INT32_C({attributes['shield_sdi_scale_q16']}),",
            f"    UINT16_C({attributes['stick_tilt_threshold']}),",
            f"    UINT16_C({attributes['sdi_stick_threshold']}),",
            f"    UINT16_C({attributes['sdi_stick_window_ticks']}),",
            "    UINT16_C(0),",
            "};",
            "",
            "static const pf_m4_ssbm_surface_response_attributes",
            "pf_m4_ssbm_surface_response_attribute_data = {",
            f"    INT32_C({surface_attributes['collision_threshold_x_q16']}),",
            f"    INT32_C({surface_attributes['collision_threshold_y_q16']}),",
            f"    INT32_C({surface_attributes['bounce_multiplier_q16']}),",
            f"    UINT16_C({surface_attributes['wall_tech_stall_ticks']}),",
            f"    UINT16_C({surface_attributes['wall_tech_invulnerability_ticks']}),",
            f"    UINT16_C({surface_attributes['bounce_invulnerability_ticks']}),",
            f"    UINT16_C({surface_attributes['bounce_collision_lockout_ticks']}),",
            f"    UINT16_C({surface_attributes['tech_window_ticks']}),",
            f"    UINT16_C({surface_attributes['tech_lockout_ticks']}),",
            f"    UINT16_C({surface_attributes['tech_roll_axis_threshold']}),",
            f"    UINT16_C({surface_attributes['down_wait_ticks']}),",
            f"    INT32_C({surface_attributes['down_horizontal_angle_tan_q16']}),",
            f"    UINT16_C({surface_attributes['down_up_axis_threshold']}),",
            f"    UINT16_C({surface_attributes['down_horizontal_axis_threshold']}),",
            f"    UINT16_C({surface_attributes['down_attack_input_window_ticks']}),",
            f"    UINT16_C({surface_attributes['down_c_up_axis_threshold']}),",
            "};",
            "",
            "static const pf_m4_ssbm_ledge_response_attributes",
            "pf_m4_ssbm_ledge_response_attribute_data = {",
            f"    INT32_C({ledge_attributes['direction_angle_tan_q16']}),",
            f"    UINT16_C({ledge_attributes['damage_threshold_percent']}),",
            f"    UINT16_C({ledge_attributes['quick_wait_ticks']}),",
            f"    UINT16_C({ledge_attributes['slow_wait_ticks']}),",
            f"    UINT16_C({ledge_attributes['stick_axis_threshold']}),",
            f"    UINT16_C({ledge_attributes['regrab_cooldown_ticks']}),",
            f"    UINT16_C({ledge_attributes['wait_invulnerability_ticks']}),",
            f"    UINT16_C({ledge_attributes['c_attack_axis_threshold']}),",
            f"    UINT16_C({ledge_attributes['c_roll_axis_threshold']}),",
            "};",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("common_dat", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    raw = args.common_dat.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != SOURCE_COMMON_DAT_SHA256:
        raise SystemExit(f"unexpected PlCo.dat SHA-256: {digest}")
    generated = generate(raw)
    if args.check:
        if (
            not args.output.is_file()
            or args.output.read_text(encoding="utf-8") != generated
        ):
            raise SystemExit(f"stale generated common data: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
    print(
        "ssbm-common-data=pass "
        f"raw_words={FT_COMMON_DATA_SIZE // 4} "
        f"source_sha256={digest} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
