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
    running_jump_axis_threshold = f32(0x080)
    if not 0.0 < running_jump_axis_threshold < 1.0:
        raise ValueError("invalid common running-jump axis threshold")
    fall_animation_direction_threshold = f32(0x444)
    fall_animation_blend_rate = f32(0x448)
    if (
        not 0.0 < fall_animation_direction_threshold < 1.0
        or not 0.0 < fall_animation_blend_rate <= 1.0
    ):
        raise ValueError("invalid common fall-animation blend attributes")
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
    ledge_grab_down_axis_threshold = f32(0x480)
    ledge_damage_threshold = i32(0x488)
    ledge_quick_wait_ticks = f32(0x48C)
    ledge_slow_wait_ticks = f32(0x490)
    ledge_stick_axis_threshold = f32(0x494)
    ledge_regrab_cooldown_ticks = i32(0x498)
    ledge_wait_invulnerability_ticks = i32(0x49C)
    ledge_c_attack_axis_threshold = f32(0x7F8)
    ledge_c_roll_axis_threshold = f32(0x7FC)
    mash_stick_axis_threshold = f32(0x308)
    furafura_max_damage_reduction_ticks = f32(0x2F8)
    furafura_minimum_ticks = f32(0x2FC)
    furafura_tick_decrement = f32(0x300)
    furafura_mash_reduction_ticks = f32(0x304)
    capture_tick_decrement = f32(0x3A4)
    capture_mash_reduction_ticks = f32(0x3A8)
    teeter_turn_axis_threshold = -f32(0x034)
    teeter_walk_axis_threshold = f32(0x474)
    walk_axis_threshold = f32(0x024)
    walk_middle_speed_ratio = f32(0x028)
    walk_fast_speed_ratio = f32(0x02C)
    aerial_neutral_x_threshold = f32(0x0DC)
    aerial_neutral_y_threshold = f32(0x0E0)
    c_stick_horizontal_smash_threshold = f32(0x03C)
    c_stick_up_smash_threshold = f32(0x0CC)
    c_stick_down_smash_threshold = -f32(0x0D4)
    forward_smash_input_window_ticks = i32(0x040)
    vertical_smash_input_window_ticks = int(f32(0x0D0))
    forward_tilt_axis_threshold = f32(0x098)
    vertical_tilt_axis_threshold = f32(0x0AC)
    vertical_smash_axis_threshold = f32(0x0CC)
    tilt_direction_angle = f32(0x020)
    escape_spot_dodge_axis_threshold = -f32(0x314)
    escape_spot_dodge_tilt_window_ticks = i32(0x318)
    escape_roll_axis_threshold = f32(0x31C)
    escape_roll_tilt_window_ticks = i32(0x320)
    special_vertical_axis_threshold = f32(0x21C)
    neutral_special_turn_window_ticks = i32(0x224)
    initial_dash_early_end_frame = f32(0x044)
    initial_dash_special_end_frame = f32(0x04C)
    throw_animation_weight_scale = f32(0x37C)
    grab_release_air_speed_x = f32(0x374)
    grab_release_air_speed_y = f32(0x378)
    rebirth_descent_ticks = i32(0x5D0)
    rebirth_wait_ticks = i32(0x5D4)
    rebirth_invulnerability_ticks = i32(0x5D8)
    maximum_hitlag_ticks = f32(0x194)
    hitlag_damage_scale = f32(0x198)
    hitlag_base_ticks = f32(0x19C)
    crouch_hitlag_scale = f32(0x1A0)
    electric_hitlag_scale = f32(0x1A4)
    crouch_knockback_scale = f32(0x124)
    smash_charge_knockback_scale = f32(0x7C4)
    damage_fall_wiggle_axis_threshold = f32(0x210)
    damage_fall_wiggle_tilt_window_ticks = i32(0x214)
    meteor_angle_min_degrees = i32(0x7E8)
    meteor_angle_max_degrees = i32(0x7EC)
    meteor_cancel_lockout_ticks = i32(0x7F0)
    # ftCo_Damage.doIasa clears knockback and calls ftCommon_8007EBAC(fp, 12,
    # 0) when the meteor cancel succeeds. This value is code-authored in the
    # pinned decomp rather than stored in ftCommonData.
    meteor_cancel_invulnerability_ticks = 12
    damage_velocity_replace_window_ticks = i32(0x0FC)
    damage_jump_buffer_window_ticks = f32(0x1D0)
    forward_tilt_angles = (
        f32(0x09C),
        f32(0x0A0),
        f32(0x0A4),
        f32(0x0A8),
    )
    forward_smash_angles = (
        f32(0x0B8),
        f32(0x0BC),
        f32(0x0C0),
        f32(0x0C4),
    )
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
        or not 0.0 < ledge_grab_down_axis_threshold <= 1.0
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
        or not 0.0 < mash_stick_axis_threshold <= 1.0
        or not furafura_max_damage_reduction_ticks.is_integer()
        or not 0 < furafura_max_damage_reduction_ticks <= 0xFFFF
        or not furafura_minimum_ticks.is_integer()
        or not 0 < furafura_minimum_ticks <= 0xFFFF
        or not furafura_tick_decrement.is_integer()
        or not 0 < furafura_tick_decrement <= 0xFFFF
        or not furafura_mash_reduction_ticks.is_integer()
        or not 0 < furafura_mash_reduction_ticks <= 0xFFFF
        or not capture_tick_decrement.is_integer()
        or not 0 < capture_tick_decrement <= 0xFFFF
        or not capture_mash_reduction_ticks.is_integer()
        or not 0 < capture_mash_reduction_ticks <= 0xFFFF
        or not 0.0 < teeter_turn_axis_threshold <= 1.0
        or not 0.0 < teeter_walk_axis_threshold <= 1.0
        or not 0.0 < walk_axis_threshold <= 1.0
        or not 0.0 < walk_middle_speed_ratio < walk_fast_speed_ratio <= 1.0
        or not 0.0 < aerial_neutral_x_threshold <= 1.0
        or not 0.0 < aerial_neutral_y_threshold <= 1.0
        or not 0.0 < c_stick_horizontal_smash_threshold <= 1.0
        or not 0.0 < c_stick_up_smash_threshold <= 1.0
        or not 0.0 < c_stick_down_smash_threshold <= 1.0
        or not 0 < forward_smash_input_window_ticks <= 0xFFFF
        or not 0 < vertical_smash_input_window_ticks <= 0xFFFF
        or not 0.0 < forward_tilt_axis_threshold <= 1.0
        or not 0.0 < vertical_tilt_axis_threshold <= 1.0
        or f32(0x0B0) != -vertical_tilt_axis_threshold
        or not 0.0 < vertical_smash_axis_threshold <= 1.0
        or f32(0x0D4) != -vertical_smash_axis_threshold
        or not 0.0 < tilt_direction_angle < math.pi / 2.0
        or not 0.0 < escape_spot_dodge_axis_threshold <= 1.0
        or escape_spot_dodge_axis_threshold != escape_roll_axis_threshold
        or not 0 < escape_spot_dodge_tilt_window_ticks <= 0xFFFF
        or escape_spot_dodge_tilt_window_ticks != escape_roll_tilt_window_ticks
        or not 0.0 < special_vertical_axis_threshold <= 1.0
        or not 0 < neutral_special_turn_window_ticks <= 0xFFFF
        or not initial_dash_early_end_frame.is_integer()
        or not 0 < initial_dash_early_end_frame <= 0xFFFF
        or not initial_dash_special_end_frame.is_integer()
        or not initial_dash_early_end_frame
        < initial_dash_special_end_frame
        <= 0xFFFF
        or not 0.0 < throw_animation_weight_scale <= 1.0
        or not 0.0 < grab_release_air_speed_x <= 16.0
        or not 0.0 < grab_release_air_speed_y <= 16.0
        or not 0 < rebirth_descent_ticks <= 0xFFFF
        or not 0 < rebirth_wait_ticks <= 0xFFFF
        or not 0 < rebirth_invulnerability_ticks <= 0xFFFF
        or not maximum_hitlag_ticks.is_integer()
        or not 0 < maximum_hitlag_ticks <= 0xFFFF
        or not 0.0 < hitlag_damage_scale <= 1.0
        or not hitlag_base_ticks.is_integer()
        or not 0 < hitlag_base_ticks <= 0xFFFF
        or not 0.0 < crouch_hitlag_scale <= 1.0
        or not 1.0 <= electric_hitlag_scale <= 4.0
        or not 0.0 < crouch_knockback_scale <= 1.0
        or not 1.0 <= smash_charge_knockback_scale <= 4.0
        or not 0.0 < damage_fall_wiggle_axis_threshold <= 1.0
        or not 0 < damage_fall_wiggle_tilt_window_ticks <= 0xFFFF
        or not 180 < meteor_angle_min_degrees <= meteor_angle_max_degrees < 360
        or not 0 < meteor_cancel_lockout_ticks <= 0xFFFF
        or not 0 < meteor_cancel_invulnerability_ticks <= 0xFFFF
        or not 0 < damage_velocity_replace_window_ticks <= 0xFFFF
        or not damage_jump_buffer_window_ticks.is_integer()
        or not 0 < damage_jump_buffer_window_ticks <= 0xFFFF
        or forward_tilt_angles
        != (
            -forward_tilt_angles[3],
            -forward_tilt_angles[2],
            -forward_tilt_angles[1],
            -forward_tilt_angles[0],
        )
        or forward_smash_angles
        != (
            -forward_smash_angles[3],
            -forward_smash_angles[2],
            -forward_smash_angles[1],
            -forward_smash_angles[0],
        )
        or not 0.0 < forward_tilt_angles[1] < forward_tilt_angles[0]
        or not 0.0 < forward_smash_angles[1] < forward_smash_angles[0]
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
        "hitlag_damage_scale_q30": round(
            hitlag_damage_scale * float(1 << 30)
        ),
        "crouch_hitlag_scale_q16": q16(crouch_hitlag_scale),
        "electric_hitlag_scale_q16": q16(electric_hitlag_scale),
        "crouch_knockback_scale_q16": q16(crouch_knockback_scale),
        "smash_charge_knockback_scale_q16": q16(
            smash_charge_knockback_scale
        ),
        "hitlag_base_ticks": int(hitlag_base_ticks),
        "maximum_hitlag_ticks": int(maximum_hitlag_ticks),
        "meteor_angle_min_degrees": meteor_angle_min_degrees,
        "meteor_angle_max_degrees": meteor_angle_max_degrees,
        "meteor_cancel_lockout_ticks": meteor_cancel_lockout_ticks,
        "meteor_cancel_invulnerability_ticks": (
            meteor_cancel_invulnerability_ticks
        ),
        "damage_fall_wiggle_axis_threshold": round(
            damage_fall_wiggle_axis_threshold * 32767.0
        ),
        "damage_fall_wiggle_tilt_window_ticks": (
            damage_fall_wiggle_tilt_window_ticks
        ),
        "damage_velocity_replace_window_ticks": (
            damage_velocity_replace_window_ticks
        ),
        "damage_jump_buffer_window_ticks": int(
            damage_jump_buffer_window_ticks
        ),
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
        "grab_down_axis_threshold": round(
            ledge_grab_down_axis_threshold * 32767.0
        ),
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
    mash_attributes = {
        "furafura_shield_health_q16": q16(f32(0x280)),
        "capture_base_q16": q16(f32(0x354)),
        "capture_handicap_scale_q16": q16(f32(0x358)),
        "capture_handicap_reference_q16": q16(f32(0x35C)),
        "capture_rank_scale_q16": q16(f32(0x360)),
        "capture_rank_reference_q16": q16(f32(0x364)),
        "capture_damage_scale_q16": q16(f32(0x368)),
        "stick_axis_threshold": round(mash_stick_axis_threshold * 32767.0),
        "furafura_max_damage_reduction_ticks": int(
            furafura_max_damage_reduction_ticks
        ),
        "furafura_minimum_ticks": int(furafura_minimum_ticks),
        "furafura_tick_decrement": int(furafura_tick_decrement),
        "furafura_mash_reduction_ticks": int(
            furafura_mash_reduction_ticks
        ),
        "capture_tick_decrement": int(capture_tick_decrement),
        "capture_mash_reduction_ticks": int(capture_mash_reduction_ticks),
    }
    ground_input_attributes = {
        "grab_release_speed_x_q16": round(
            f32(0x370) * MELEE_X_TO_SIM_Q16
        ),
        "grab_release_air_speed_x_q16": round(
            grab_release_air_speed_x * MELEE_X_TO_SIM_Q16
        ),
        "grab_release_air_speed_y_q16": round(
            grab_release_air_speed_y * MELEE_Y_TO_SIM_Q16
        ),
        "throw_animation_weight_scale_q16": q16(
            throw_animation_weight_scale
        ),
        "teeter_turn_axis_threshold": round(
            teeter_turn_axis_threshold * 32767.0
        ),
        "teeter_walk_axis_threshold": round(
            teeter_walk_axis_threshold * 32767.0
        ),
        "walk_axis_threshold": round(walk_axis_threshold * 32767.0),
        "walk_middle_speed_ratio_q16": q16(walk_middle_speed_ratio),
        "walk_fast_speed_ratio_q16": q16(walk_fast_speed_ratio),
        "aerial_neutral_x_threshold": round(
            aerial_neutral_x_threshold * 32767.0
        ),
        "aerial_neutral_y_threshold": round(
            aerial_neutral_y_threshold * 32767.0
        ),
        "c_stick_horizontal_smash_threshold": round(
            c_stick_horizontal_smash_threshold * 32767.0
        ),
        "c_stick_up_smash_threshold": round(
            c_stick_up_smash_threshold * 32767.0
        ),
        "c_stick_down_smash_threshold": round(
            c_stick_down_smash_threshold * 32767.0
        ),
        "escape_axis_threshold": round(
            escape_roll_axis_threshold * 32767.0
        ),
        "escape_tilt_window_ticks": escape_roll_tilt_window_ticks,
        "special_vertical_axis_threshold": round(
            special_vertical_axis_threshold * 32767.0
        ),
        "neutral_special_turn_window_ticks": (
            neutral_special_turn_window_ticks
        ),
        "initial_dash_early_end_frame": int(
            initial_dash_early_end_frame
        ),
        "initial_dash_special_end_frame": int(
            initial_dash_special_end_frame
        ),
        "running_jump_axis_threshold": round(
            running_jump_axis_threshold * 32767.0
        ),
        "forward_smash_input_window_ticks": (
            forward_smash_input_window_ticks
        ),
        "vertical_smash_input_window_ticks": (
            vertical_smash_input_window_ticks
        ),
        "forward_tilt_axis_threshold": round(
            forward_tilt_axis_threshold * 32767.0
        ),
        "vertical_tilt_axis_threshold": round(
            vertical_tilt_axis_threshold * 32767.0
        ),
        "vertical_smash_axis_threshold": round(
            vertical_smash_axis_threshold * 32767.0
        ),
        "aerial_direction_angle_tan_q16": q16(
            math.tan(down_horizontal_angle)
        ),
        "tilt_direction_angle_tan_q16": q16(
            math.tan(tilt_direction_angle)
        ),
        "forward_tilt_outer_angle_tan_q16": q16(
            math.tan(forward_tilt_angles[0])
        ),
        "forward_tilt_inner_angle_tan_q16": q16(
            math.tan(forward_tilt_angles[1])
        ),
        "forward_smash_outer_angle_tan_q16": q16(
            math.tan(forward_smash_angles[0])
        ),
        "forward_smash_inner_angle_tan_q16": q16(
            math.tan(forward_smash_angles[1])
        ),
    }
    rebirth_attributes = {
        "descent_ticks": rebirth_descent_ticks,
        "wait_ticks": rebirth_wait_ticks,
        "invulnerability_ticks": rebirth_invulnerability_ticks,
    }
    clank_attributes = {
        "rebound_strength_damage_scale_q16": q16(f32(0x3D0)),
        "rebound_strength_base_q16": q16(f32(0x3D4)),
        "rebound_velocity_strength_scale_q16": round(
            f32(0x3D8) * MELEE_X_TO_SIM_Q16
        ),
        "rebound_velocity_base_q16": round(
            f32(0x3DC) * MELEE_X_TO_SIM_Q16
        ),
        "damage_margin": i32(0x3CC),
    }
    fall_animation_attributes = {
        "direction_threshold_q16": q16(
            fall_animation_direction_threshold
        ),
        "blend_rate_q16": q16(fall_animation_blend_rate),
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
            f"    INT32_C({attributes['hitlag_damage_scale_q30']}),",
            f"    UINT32_C({attributes['crouch_hitlag_scale_q16']}),",
            f"    UINT32_C({attributes['electric_hitlag_scale_q16']}),",
            f"    UINT32_C({attributes['crouch_knockback_scale_q16']}),",
            f"    UINT32_C({attributes['smash_charge_knockback_scale_q16']}),",
            f"    UINT16_C({attributes['stick_tilt_threshold']}),",
            f"    UINT16_C({attributes['sdi_stick_threshold']}),",
            f"    UINT16_C({attributes['sdi_stick_window_ticks']}),",
            f"    UINT16_C({attributes['hitlag_base_ticks']}),",
            f"    UINT16_C({attributes['maximum_hitlag_ticks']}),",
            f"    UINT16_C({attributes['meteor_angle_min_degrees']}),",
            f"    UINT16_C({attributes['meteor_angle_max_degrees']}),",
            f"    UINT16_C({attributes['meteor_cancel_lockout_ticks']}),",
            f"    UINT16_C({attributes['meteor_cancel_invulnerability_ticks']}),",
            f"    UINT16_C({attributes['damage_fall_wiggle_axis_threshold']}),",
            f"    UINT16_C({attributes['damage_fall_wiggle_tilt_window_ticks']}),",
            f"    UINT16_C({attributes['damage_velocity_replace_window_ticks']}),",
            f"    UINT16_C({attributes['damage_jump_buffer_window_ticks']}),",
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
            f"    UINT16_C({ledge_attributes['grab_down_axis_threshold']}),",
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
            "static const pf_m4_ssbm_mash_attributes",
            "pf_m4_ssbm_mash_attribute_data = {",
            f"    UINT32_C({mash_attributes['furafura_shield_health_q16']}),",
            f"    INT32_C({mash_attributes['capture_base_q16']}),",
            f"    INT32_C({mash_attributes['capture_handicap_scale_q16']}),",
            f"    INT32_C({mash_attributes['capture_handicap_reference_q16']}),",
            f"    INT32_C({mash_attributes['capture_rank_scale_q16']}),",
            f"    INT32_C({mash_attributes['capture_rank_reference_q16']}),",
            f"    INT32_C({mash_attributes['capture_damage_scale_q16']}),",
            f"    UINT16_C({mash_attributes['stick_axis_threshold']}),",
            f"    UINT16_C({mash_attributes['furafura_max_damage_reduction_ticks']}),",
            f"    UINT16_C({mash_attributes['furafura_minimum_ticks']}),",
            f"    UINT16_C({mash_attributes['furafura_tick_decrement']}),",
            f"    UINT16_C({mash_attributes['furafura_mash_reduction_ticks']}),",
            f"    UINT16_C({mash_attributes['capture_tick_decrement']}),",
            f"    UINT16_C({mash_attributes['capture_mash_reduction_ticks']}),",
            "    UINT16_C(0),",
            "};",
            "",
            "static const pf_m4_ssbm_ground_input_attributes",
            "pf_m4_ssbm_ground_input_attribute_data = {",
            f"    INT32_C({ground_input_attributes['grab_release_speed_x_q16']}),",
            f"    INT32_C({ground_input_attributes['grab_release_air_speed_x_q16']}),",
            f"    INT32_C({ground_input_attributes['grab_release_air_speed_y_q16']}),",
            f"    INT32_C({ground_input_attributes['throw_animation_weight_scale_q16']}),",
            f"    UINT16_C({ground_input_attributes['teeter_turn_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['teeter_walk_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['walk_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['walk_middle_speed_ratio_q16']}),",
            f"    UINT16_C({ground_input_attributes['walk_fast_speed_ratio_q16']}),",
            f"    UINT16_C({ground_input_attributes['aerial_neutral_x_threshold']}),",
            f"    UINT16_C({ground_input_attributes['aerial_neutral_y_threshold']}),",
            f"    UINT16_C({ground_input_attributes['c_stick_horizontal_smash_threshold']}),",
            f"    UINT16_C({ground_input_attributes['c_stick_up_smash_threshold']}),",
            f"    UINT16_C({ground_input_attributes['c_stick_down_smash_threshold']}),",
            f"    UINT16_C({ground_input_attributes['escape_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['escape_tilt_window_ticks']}),",
            f"    UINT16_C({ground_input_attributes['special_vertical_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['neutral_special_turn_window_ticks']}),",
            f"    UINT16_C({ground_input_attributes['initial_dash_early_end_frame']}),",
            f"    UINT16_C({ground_input_attributes['initial_dash_special_end_frame']}),",
            f"    UINT16_C({ground_input_attributes['running_jump_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['forward_smash_input_window_ticks']}),",
            f"    UINT16_C({ground_input_attributes['vertical_smash_input_window_ticks']}),",
            f"    UINT16_C({ground_input_attributes['forward_tilt_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['vertical_tilt_axis_threshold']}),",
            f"    UINT16_C({ground_input_attributes['vertical_smash_axis_threshold']}),",
            f"    INT32_C({ground_input_attributes['aerial_direction_angle_tan_q16']}),",
            f"    INT32_C({ground_input_attributes['tilt_direction_angle_tan_q16']}),",
            f"    INT32_C({ground_input_attributes['forward_tilt_outer_angle_tan_q16']}),",
            f"    INT32_C({ground_input_attributes['forward_tilt_inner_angle_tan_q16']}),",
            f"    INT32_C({ground_input_attributes['forward_smash_outer_angle_tan_q16']}),",
            f"    INT32_C({ground_input_attributes['forward_smash_inner_angle_tan_q16']}),",
            "};",
            "",
            "static const pf_m4_ssbm_rebirth_attributes",
            "pf_m4_ssbm_rebirth_attribute_data = {",
            f"    UINT16_C({rebirth_attributes['descent_ticks']}),",
            f"    UINT16_C({rebirth_attributes['wait_ticks']}),",
            f"    UINT16_C({rebirth_attributes['invulnerability_ticks']}),",
            "    UINT16_C(0),",
            "};",
            "",
            "static const pf_m4_ssbm_clank_attributes",
            "pf_m4_ssbm_clank_attribute_data = {",
            f"    INT32_C({clank_attributes['rebound_strength_damage_scale_q16']}),",
            f"    INT32_C({clank_attributes['rebound_strength_base_q16']}),",
            f"    INT32_C({clank_attributes['rebound_velocity_strength_scale_q16']}),",
            f"    INT32_C({clank_attributes['rebound_velocity_base_q16']}),",
            f"    UINT16_C({clank_attributes['damage_margin']}),",
            "    UINT16_C(0),",
            "};",
            "",
            "static const pf_m4_ssbm_fall_animation_attributes",
            "pf_m4_ssbm_fall_animation_attribute_data = {",
            f"    INT32_C({fall_animation_attributes['direction_threshold_q16']}),",
            f"    INT32_C({fall_animation_attributes['blend_rate_q16']}),",
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
