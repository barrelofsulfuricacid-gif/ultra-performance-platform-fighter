#include "sim_internal.h"
#include "sim_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PF_Q16_RATIO(numerator, denominator)                           \
    ((int32_t)(((int64_t)(numerator) * (int64_t)PF_Q16_ONE) /         \
               (int64_t)(denominator)))

static const uint8_t pf_m4_content_hash_domain[8] = {
    UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x4d), UINT8_C(0x34),
    UINT8_C(0x44), UINT8_C(0x41), UINT8_C(0x54), UINT8_C(0x31)};

static void pf_m4_hash_u8(pf_sha256 *hash, uint8_t value)
{
    pf_sha256_update(hash, &value, sizeof(value));
}

static void pf_m4_hash_u16(pf_sha256 *hash, uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    pf_sha256_update(hash, bytes, sizeof(bytes));
}

static void pf_m4_hash_u32(pf_sha256 *hash, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
    pf_sha256_update(hash, bytes, sizeof(bytes));
}

static void pf_m4_hash_i32(pf_sha256 *hash, int32_t value)
{
    pf_m4_hash_u32(hash, (uint32_t)value);
}

static void pf_m4_hash_fighter(
    pf_sha256 *hash,
    const pf_m4_fighter_data *fighter)
{
    pf_m4_hash_u16(hash, fighter->schema_version);
    pf_m4_hash_i32(hash, fighter->half_width_q16);
    pf_m4_hash_i32(hash, fighter->half_height_q16);
    pf_m4_hash_i32(hash, fighter->ground_acceleration_q16);
    pf_m4_hash_i32(hash, fighter->turn_acceleration_q16);
    pf_m4_hash_i32(hash, fighter->traction_q16);
    pf_m4_hash_i32(hash, fighter->walk_speed_q16);
    pf_m4_hash_i32(hash, fighter->run_speed_q16);
    pf_m4_hash_i32(hash, fighter->initial_dash_speed_q16);
    pf_m4_hash_i32(hash, fighter->air_acceleration_q16);
    pf_m4_hash_i32(hash, fighter->air_speed_q16);
    pf_m4_hash_i32(hash, fighter->gravity_q16);
    pf_m4_hash_i32(hash, fighter->fall_speed_q16);
    pf_m4_hash_i32(hash, fighter->fast_fall_speed_q16);
    pf_m4_hash_i32(hash, fighter->full_hop_speed_q16);
    pf_m4_hash_i32(hash, fighter->short_hop_speed_q16);
    pf_m4_hash_i32(hash, fighter->double_jump_speed_q16);
    pf_m4_hash_i32(hash, fighter->platform_drop_nudge_q16);
    pf_m4_hash_i32(hash, fighter->air_dodge_speed_q16);
    pf_m4_hash_i32(hash, fighter->air_dodge_decay_q16);
    pf_m4_hash_i32(hash, fighter->fall_special_mobility_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_half_width_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->jab_damage_q16);
    pf_m4_hash_i32(hash, fighter->jab_base_knockback_x_q16);
    pf_m4_hash_i32(hash, fighter->jab_base_knockback_y_q16);
    pf_m4_hash_i32(hash, fighter->jab_knockback_growth_q16);
    pf_m4_hash_i32(hash, fighter->strong_hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->strong_hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->strong_hitbox_half_width_q16);
    pf_m4_hash_i32(hash, fighter->strong_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->strong_damage_q16);
    pf_m4_hash_i32(hash, fighter->strong_base_knockback_x_q16);
    pf_m4_hash_i32(hash, fighter->strong_base_knockback_y_q16);
    pf_m4_hash_i32(hash, fighter->strong_knockback_growth_q16);
    pf_m4_hash_i32(hash, fighter->aerial_hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->aerial_hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->aerial_hitbox_half_width_q16);
    pf_m4_hash_i32(hash, fighter->aerial_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->aerial_damage_q16);
    pf_m4_hash_i32(hash, fighter->aerial_base_knockback_x_q16);
    pf_m4_hash_i32(hash, fighter->aerial_base_knockback_y_q16);
    pf_m4_hash_i32(hash, fighter->aerial_knockback_growth_q16);
    pf_m4_hash_i32(hash, fighter->hitstun_velocity_per_tick_q16);
    pf_m4_hash_i32(hash, fighter->di_max_tangent_q16);
    pf_m4_hash_i32(hash, fighter->sdi_distance_q16);
    pf_m4_hash_i32(hash, fighter->asdi_distance_q16);
    pf_m4_hash_i32(hash, fighter->tech_roll_speed_q16);
    pf_m4_hash_i32(hash, fighter->wall_tech_speed_q16);
    pf_m4_hash_i32(hash, fighter->wall_tech_jump_speed_x_q16);
    pf_m4_hash_i32(hash, fighter->wall_tech_jump_speed_y_q16);
    pf_m4_hash_i32(hash, fighter->ceiling_tech_speed_q16);
    pf_m4_hash_i32(hash, fighter->surface_bounce_multiplier_q16);
    pf_m4_hash_i32(hash, fighter->getup_roll_speed_q16);
    pf_m4_hash_i32(hash, fighter->forward_roll_speed_q16);
    pf_m4_hash_i32(hash, fighter->backward_roll_speed_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_hitbox_offset_x_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_hitbox_offset_y_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_hitbox_half_width_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->getup_attack_damage_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_base_knockback_x_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_base_knockback_y_q16);
    pf_m4_hash_i32(
        hash,
        fighter->getup_attack_knockback_growth_q16);
    pf_m4_hash_u32(hash, fighter->shield_health_q16);
    pf_m4_hash_u32(hash, fighter->shield_reset_health_q16);
    pf_m4_hash_u32(hash, fighter->shield_hold_depletion_q16);
    pf_m4_hash_u32(hash, fighter->shield_regeneration_q16);
    pf_m4_hash_u32(hash, fighter->shield_damage_multiplier_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_stun_damage_multiplier_q16);
    pf_m4_hash_i32(hash, fighter->shield_stun_base_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_defender_pushback_damage_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_defender_pushback_base_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_defender_pushback_scale_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_attacker_pushback_damage_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_attacker_pushback_base_q16);
    pf_m4_hash_u16(hash, fighter->jump_squat_ticks);
    pf_m4_hash_u16(hash, fighter->initial_dash_ticks);
    pf_m4_hash_u16(hash, fighter->landing_ticks);
    pf_m4_hash_u16(hash, fighter->platform_drop_ticks);
    pf_m4_hash_u16(hash, fighter->air_dodge_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->air_dodge_invulnerability_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->air_dodge_invulnerability_end_tick);
    pf_m4_hash_u16(hash, fighter->special_landing_ticks);
    pf_m4_hash_u16(hash, fighter->run_turnaround_ticks);
    pf_m4_hash_u16(hash, fighter->run_brake_ticks);
    pf_m4_hash_u16(hash, fighter->axis_dead_zone);
    pf_m4_hash_u16(hash, fighter->dash_axis_threshold);
    pf_m4_hash_u16(hash, fighter->run_turnaround_axis_threshold);
    pf_m4_hash_u16(hash, fighter->run_continue_axis_threshold);
    pf_m4_hash_u16(hash, fighter->run_turnaround_lockout_ticks);
    pf_m4_hash_u16(hash, fighter->crouch_axis_threshold);
    pf_m4_hash_u16(hash, fighter->jab_startup_ticks);
    pf_m4_hash_u16(hash, fighter->jab_active_ticks);
    pf_m4_hash_u16(hash, fighter->jab_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->jab_hitlag_ticks);
    pf_m4_hash_u16(hash, fighter->strong_startup_ticks);
    pf_m4_hash_u16(hash, fighter->strong_active_ticks);
    pf_m4_hash_u16(hash, fighter->strong_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->strong_hitlag_ticks);
    pf_m4_hash_u16(hash, fighter->aerial_startup_ticks);
    pf_m4_hash_u16(hash, fighter->aerial_active_ticks);
    pf_m4_hash_u16(hash, fighter->aerial_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->aerial_hitlag_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->aerial_landing_lag_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->aerial_landing_lag_end_tick);
    pf_m4_hash_u16(hash, fighter->aerial_landing_lag_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->strong_aerial_landing_lag_ticks);
    pf_m4_hash_u16(hash, fighter->l_cancel_window_ticks);
    pf_m4_hash_u16(hash, fighter->l_cancel_divisor);
    pf_m4_hash_u16(hash, fighter->sdi_axis_threshold);
    pf_m4_hash_u16(hash, fighter->digital_trigger_threshold);
    pf_m4_hash_u16(hash, fighter->tumble_hitstun_threshold_ticks);
    pf_m4_hash_u16(hash, fighter->tech_window_ticks);
    pf_m4_hash_u16(hash, fighter->tech_lockout_ticks);
    pf_m4_hash_u16(hash, fighter->tech_in_place_ticks);
    pf_m4_hash_u16(hash, fighter->tech_roll_ticks);
    pf_m4_hash_u16(hash, fighter->tech_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->wall_tech_stall_ticks);
    pf_m4_hash_u16(hash, fighter->wall_tech_ticks);
    pf_m4_hash_u16(hash, fighter->ceiling_tech_ticks);
    pf_m4_hash_u16(hash, fighter->knockdown_ticks);
    pf_m4_hash_u16(hash, fighter->down_wait_ticks);
    pf_m4_hash_u16(hash, fighter->getup_neutral_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->getup_neutral_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->getup_roll_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->getup_roll_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->getup_attack_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->getup_attack_invulnerability_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->getup_attack_front_active_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->getup_attack_front_active_end_tick);
    pf_m4_hash_u16(
        hash,
        fighter->getup_attack_back_active_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->getup_attack_back_active_end_tick);
    pf_m4_hash_u16(hash, fighter->getup_attack_hitlag_ticks);
    pf_m4_hash_u16(hash, fighter->forward_roll_ticks);
    pf_m4_hash_u16(hash, fighter->backward_roll_ticks);
    pf_m4_hash_u16(hash, fighter->roll_movement_begin_tick);
    pf_m4_hash_u16(hash, fighter->roll_movement_end_tick);
    pf_m4_hash_u16(
        hash,
        fighter->roll_invulnerability_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->roll_invulnerability_end_tick);
    pf_m4_hash_u16(hash, fighter->spot_dodge_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->spot_dodge_invulnerability_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->spot_dodge_invulnerability_end_tick);
    pf_m4_hash_u16(hash, fighter->shield_minimum_hold_ticks);
    pf_m4_hash_u16(hash, fighter->shield_release_ticks);
    pf_m4_hash_u16(hash, fighter->powershield_window_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->powershield_cancel_delay_ticks);
    pf_m4_hash_u16(hash, fighter->shield_break_ticks);
    pf_m4_hash_u8(hash, fighter->air_jump_count);
    pf_m4_hash_u8(
        hash,
        fighter->powershield_cancel_enabled);
}

static void pf_m4_hash_stage(
    pf_sha256 *hash,
    const pf_m4_stage_data *stage)
{
    pf_m4_hash_u16(hash, stage->schema_version);
    pf_m4_hash_i32(hash, stage->floor_left_q16);
    pf_m4_hash_i32(hash, stage->floor_right_q16);
    pf_m4_hash_i32(hash, stage->floor_y_q16);
    pf_m4_hash_i32(hash, stage->platform_center_x_q16);
    pf_m4_hash_i32(hash, stage->platform_y_q16);
    pf_m4_hash_i32(hash, stage->platform_half_width_q16);
    pf_m4_hash_i32(hash, stage->platform_motion_amplitude_q16);
    pf_m4_hash_i32(hash, stage->solid_left_q16);
    pf_m4_hash_i32(hash, stage->solid_right_q16);
    pf_m4_hash_i32(hash, stage->solid_top_q16);
    pf_m4_hash_i32(hash, stage->solid_bottom_q16);
    pf_m4_hash_i32(hash, stage->blast_left_q16);
    pf_m4_hash_i32(hash, stage->blast_right_q16);
    pf_m4_hash_i32(hash, stage->blast_top_q16);
    pf_m4_hash_i32(hash, stage->blast_bottom_q16);
    pf_m4_hash_i32(hash, stage->spawn_spacing_q16);
    pf_m4_hash_u16(hash, stage->platform_motion_period_ticks);
}

static void pf_m4_content_hash(
    const pf_m4_content *content,
    uint8_t digest[32])
{
    pf_sha256 hash;

    pf_sha256_init(&hash);
    pf_sha256_update(
        &hash,
        pf_m4_content_hash_domain,
        sizeof(pf_m4_content_hash_domain));
    pf_m4_hash_u16(&hash, content->schema_version);
    pf_m4_hash_u8(&hash, content->fighter_count);
    pf_m4_hash_u8(&hash, content->stage_count);
    pf_m4_hash_fighter(&hash, &content->fighter);
    pf_m4_hash_stage(&hash, &content->stage);
    pf_sha256_finish(&hash, digest);
}

static int pf_m4_hash_equal(
    const uint8_t left[32],
    const uint8_t right[32])
{
    uint8_t difference = UINT8_C(0);
    uint32_t byte_index;

    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(32);
         ++byte_index)
    {
        difference |= (uint8_t)(left[byte_index] ^ right[byte_index]);
    }
    return difference == UINT8_C(0);
}

pf_status pf_m4_default_content(pf_m4_content *out_content)
{
    pf_m4_fighter_data *fighter;
    pf_m4_stage_data *stage;

    if (out_content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(out_content, 0, sizeof(*out_content));
    out_content->struct_size = (uint32_t)sizeof(*out_content);
    out_content->schema_version = PF_M4_CONTENT_SCHEMA_VERSION;
    out_content->fighter_count = PF_M4_PLACEHOLDER_FIGHTER_COUNT;
    out_content->stage_count = PF_M4_TEST_STAGE_COUNT;

    fighter = &out_content->fighter;
    fighter->struct_size = (uint32_t)sizeof(*fighter);
    fighter->schema_version = PF_M4_FIGHTER_SCHEMA_VERSION;
    fighter->half_width_q16 = PF_Q16_RATIO(9, 20);
    fighter->half_height_q16 = PF_Q16_RATIO(4, 5);
    fighter->ground_acceleration_q16 = PF_Q16_RATIO(1, 40);
    fighter->turn_acceleration_q16 = PF_Q16_RATIO(1, 25);
    fighter->traction_q16 = PF_Q16_RATIO(1, 50);
    fighter->walk_speed_q16 = PF_Q16_RATIO(3, 20);
    fighter->run_speed_q16 = PF_Q16_RATIO(6, 25);
    fighter->initial_dash_speed_q16 = PF_Q16_RATIO(3, 10);
    fighter->air_acceleration_q16 = PF_Q16_RATIO(1, 100);
    fighter->air_speed_q16 = PF_Q16_RATIO(4, 25);
    fighter->gravity_q16 = PF_Q16_RATIO(1, 50);
    fighter->fall_speed_q16 = PF_Q16_RATIO(2, 5);
    fighter->fast_fall_speed_q16 = PF_Q16_RATIO(3, 5);
    fighter->full_hop_speed_q16 = PF_Q16_RATIO(11, 20);
    fighter->short_hop_speed_q16 = PF_Q16_RATIO(9, 25);
    fighter->double_jump_speed_q16 = PF_Q16_RATIO(1, 2);
    fighter->platform_drop_nudge_q16 = PF_Q16_RATIO(1, 256);
    fighter->air_dodge_speed_q16 = PF_Q16_RATIO(1, 2);
    fighter->air_dodge_decay_q16 = PF_Q16_RATIO(9, 10);
    fighter->fall_special_mobility_q16 = PF_Q16_RATIO(2, 25);
    fighter->jab_hitbox_offset_x_q16 = PF_Q16_RATIO(3, 4);
    fighter->jab_hitbox_offset_y_q16 = INT32_C(0);
    fighter->jab_hitbox_half_width_q16 = PF_Q16_RATIO(3, 5);
    fighter->jab_hitbox_half_height_q16 = PF_Q16_RATIO(9, 20);
    fighter->jab_damage_q16 = UINT32_C(6) * UINT32_C(65536);
    fighter->jab_base_knockback_x_q16 = PF_Q16_RATIO(9, 50);
    fighter->jab_base_knockback_y_q16 = PF_Q16_RATIO(11, 50);
    fighter->jab_knockback_growth_q16 = PF_Q16_RATIO(1, 512);
    fighter->strong_hitbox_offset_x_q16 = PF_Q16_RATIO(9, 10);
    fighter->strong_hitbox_offset_y_q16 = -PF_Q16_RATIO(1, 10);
    fighter->strong_hitbox_half_width_q16 = PF_Q16_RATIO(3, 4);
    fighter->strong_hitbox_half_height_q16 = PF_Q16_RATIO(11, 20);
    fighter->strong_damage_q16 = UINT32_C(12) * UINT32_C(65536);
    fighter->strong_base_knockback_x_q16 = PF_Q16_RATIO(9, 20);
    fighter->strong_base_knockback_y_q16 = PF_Q16_RATIO(17, 20);
    fighter->strong_knockback_growth_q16 = PF_Q16_RATIO(1, 512);
    fighter->aerial_hitbox_offset_x_q16 = PF_Q16_RATIO(7, 20);
    fighter->aerial_hitbox_offset_y_q16 = INT32_C(0);
    fighter->aerial_hitbox_half_width_q16 = PF_Q16_RATIO(17, 20);
    fighter->aerial_hitbox_half_height_q16 = PF_Q16_RATIO(13, 20);
    fighter->aerial_damage_q16 = UINT32_C(8) * UINT32_C(65536);
    fighter->aerial_base_knockback_x_q16 = PF_Q16_RATIO(1, 4);
    fighter->aerial_base_knockback_y_q16 = PF_Q16_RATIO(7, 20);
    fighter->aerial_knockback_growth_q16 = PF_Q16_RATIO(1, 1024);
    fighter->hitstun_velocity_per_tick_q16 = PF_Q16_RATIO(1, 25);
    fighter->di_max_tangent_q16 = INT32_C(21294);
    fighter->sdi_distance_q16 = PF_Q16_RATIO(3, 10);
    fighter->asdi_distance_q16 = PF_Q16_RATIO(3, 20);
    fighter->tech_roll_speed_q16 = PF_Q16_RATIO(1, 5);
    fighter->wall_tech_speed_q16 = PF_Q16_RATIO(3, 20);
    fighter->wall_tech_jump_speed_x_q16 = PF_Q16_RATIO(3, 10);
    fighter->wall_tech_jump_speed_y_q16 = PF_Q16_RATIO(1, 2);
    fighter->ceiling_tech_speed_q16 = PF_Q16_RATIO(4, 25);
    fighter->surface_bounce_multiplier_q16 = PF_Q16_RATIO(4, 5);
    fighter->getup_roll_speed_q16 = PF_Q16_RATIO(1, 5);
    fighter->forward_roll_speed_q16 = PF_Q16_RATIO(9, 50);
    fighter->backward_roll_speed_q16 = PF_Q16_RATIO(4, 25);
    fighter->getup_attack_hitbox_offset_x_q16 =
        PF_Q16_RATIO(3, 4);
    fighter->getup_attack_hitbox_offset_y_q16 =
        PF_Q16_RATIO(1, 5);
    fighter->getup_attack_hitbox_half_width_q16 =
        PF_Q16_RATIO(4, 5);
    fighter->getup_attack_hitbox_half_height_q16 =
        PF_Q16_RATIO(2, 5);
    fighter->getup_attack_damage_q16 =
        UINT32_C(6) * UINT32_C(65536);
    fighter->getup_attack_base_knockback_x_q16 =
        PF_Q16_RATIO(3, 20);
    fighter->getup_attack_base_knockback_y_q16 =
        PF_Q16_RATIO(1, 8);
    fighter->getup_attack_knockback_growth_q16 =
        PF_Q16_RATIO(1, 1024);
    fighter->shield_health_q16 =
        UINT32_C(60) * UINT32_C(65536);
    fighter->shield_reset_health_q16 =
        UINT32_C(30) * UINT32_C(65536);
    fighter->shield_hold_depletion_q16 =
        (uint32_t)PF_Q16_RATIO(7, 25);
    fighter->shield_regeneration_q16 =
        (uint32_t)PF_Q16_RATIO(7, 100);
    fighter->shield_damage_multiplier_q16 =
        (uint32_t)PF_Q16_RATIO(7, 10);
    fighter->shield_stun_damage_multiplier_q16 =
        PF_Q16_RATIO(9, 20);
    fighter->shield_stun_base_q16 = INT32_C(2) * PF_Q16_ONE;
    fighter->shield_defender_pushback_damage_q16 =
        PF_Q16_RATIO(9, 100);
    fighter->shield_defender_pushback_base_q16 =
        PF_Q16_RATIO(2, 5);
    fighter->shield_defender_pushback_scale_q16 =
        PF_Q16_RATIO(3, 5);
    fighter->shield_attacker_pushback_damage_q16 =
        PF_Q16_RATIO(7, 100);
    fighter->shield_attacker_pushback_base_q16 =
        PF_Q16_RATIO(1, 50);
    fighter->jump_squat_ticks = UINT16_C(3);
    fighter->initial_dash_ticks = UINT16_C(10);
    fighter->landing_ticks = UINT16_C(4);
    fighter->platform_drop_ticks = UINT16_C(6);
    fighter->air_dodge_ticks = UINT16_C(49);
    fighter->air_dodge_invulnerability_begin_tick = UINT16_C(3);
    fighter->air_dodge_invulnerability_end_tick = UINT16_C(29);
    fighter->special_landing_ticks = UINT16_C(10);
    fighter->run_turnaround_ticks = UINT16_C(12);
    fighter->run_brake_ticks = UINT16_C(8);
    fighter->axis_dead_zone = UINT16_C(4096);
    fighter->dash_axis_threshold = UINT16_C(24575);
    fighter->run_turnaround_axis_threshold = UINT16_C(12288);
    fighter->run_continue_axis_threshold = UINT16_C(20480);
    fighter->run_turnaround_lockout_ticks = UINT16_C(10);
    fighter->crouch_axis_threshold = UINT16_C(16384);
    fighter->jab_startup_ticks = UINT16_C(2);
    fighter->jab_active_ticks = UINT16_C(2);
    fighter->jab_recovery_ticks = UINT16_C(8);
    fighter->jab_hitlag_ticks = UINT16_C(4);
    fighter->strong_startup_ticks = UINT16_C(5);
    fighter->strong_active_ticks = UINT16_C(3);
    fighter->strong_recovery_ticks = UINT16_C(18);
    fighter->strong_hitlag_ticks = UINT16_C(6);
    fighter->aerial_startup_ticks = UINT16_C(4);
    fighter->aerial_active_ticks = UINT16_C(5);
    fighter->aerial_recovery_ticks = UINT16_C(23);
    fighter->aerial_hitlag_ticks = UINT16_C(5);
    fighter->aerial_landing_lag_begin_tick = UINT16_C(4);
    fighter->aerial_landing_lag_end_tick = UINT16_C(25);
    fighter->aerial_landing_lag_ticks = UINT16_C(12);
    fighter->strong_aerial_landing_lag_ticks = UINT16_C(30);
    fighter->l_cancel_window_ticks = UINT16_C(7);
    fighter->l_cancel_divisor = UINT16_C(2);
    fighter->sdi_axis_threshold = UINT16_C(16384);
    fighter->digital_trigger_threshold = UINT16_C(32768);
    fighter->tumble_hitstun_threshold_ticks = UINT16_C(32);
    fighter->tech_window_ticks = UINT16_C(20);
    fighter->tech_lockout_ticks = UINT16_C(40);
    fighter->tech_in_place_ticks = UINT16_C(26);
    fighter->tech_roll_ticks = UINT16_C(40);
    fighter->tech_invulnerability_ticks = UINT16_C(20);
    fighter->wall_tech_stall_ticks = UINT16_C(3);
    fighter->wall_tech_ticks = UINT16_C(24);
    fighter->ceiling_tech_ticks = UINT16_C(30);
    fighter->knockdown_ticks = UINT16_C(26);
    fighter->down_wait_ticks = UINT16_C(180);
    fighter->getup_neutral_ticks = UINT16_C(30);
    fighter->getup_neutral_invulnerability_ticks = UINT16_C(23);
    fighter->getup_roll_ticks = UINT16_C(35);
    fighter->getup_roll_invulnerability_ticks = UINT16_C(19);
    fighter->getup_attack_ticks = UINT16_C(49);
    fighter->getup_attack_invulnerability_ticks = UINT16_C(26);
    fighter->getup_attack_front_active_begin_tick = UINT16_C(17);
    fighter->getup_attack_front_active_end_tick = UINT16_C(19);
    fighter->getup_attack_back_active_begin_tick = UINT16_C(24);
    fighter->getup_attack_back_active_end_tick = UINT16_C(26);
    fighter->getup_attack_hitlag_ticks = UINT16_C(3);
    fighter->forward_roll_ticks = UINT16_C(31);
    fighter->backward_roll_ticks = UINT16_C(35);
    fighter->roll_movement_begin_tick = UINT16_C(3);
    fighter->roll_movement_end_tick = UINT16_C(20);
    fighter->roll_invulnerability_begin_tick = UINT16_C(4);
    fighter->roll_invulnerability_end_tick = UINT16_C(17);
    fighter->spot_dodge_ticks = UINT16_C(25);
    fighter->spot_dodge_invulnerability_begin_tick = UINT16_C(3);
    fighter->spot_dodge_invulnerability_end_tick = UINT16_C(16);
    fighter->shield_minimum_hold_ticks = UINT16_C(8);
    fighter->shield_release_ticks = UINT16_C(15);
    fighter->powershield_window_ticks = UINT16_C(4);
    fighter->powershield_cancel_delay_ticks = UINT16_C(1);
    fighter->shield_break_ticks = UINT16_C(180);
    fighter->air_jump_count = UINT8_C(1);
    fighter->powershield_cancel_enabled = UINT8_C(1);

    stage = &out_content->stage;
    stage->struct_size = (uint32_t)sizeof(*stage);
    stage->schema_version = PF_M4_STAGE_SCHEMA_VERSION;
    stage->floor_left_q16 = -INT32_C(32) * PF_Q16_ONE;
    stage->floor_right_q16 = INT32_C(32) * PF_Q16_ONE;
    stage->floor_y_q16 = INT32_C(32) * PF_Q16_ONE;
    stage->platform_center_x_q16 = INT32_C(0);
    stage->platform_y_q16 = INT32_C(25) * PF_Q16_ONE;
    stage->platform_half_width_q16 = INT32_C(5) * PF_Q16_ONE;
    stage->platform_motion_amplitude_q16 =
        INT32_C(4) * PF_Q16_ONE;
    stage->solid_left_q16 = INT32_C(14) * PF_Q16_ONE;
    stage->solid_right_q16 = INT32_C(27) * PF_Q16_ONE;
    stage->solid_top_q16 = INT32_C(16) * PF_Q16_ONE;
    stage->solid_bottom_q16 = INT32_C(29) * PF_Q16_ONE;
    stage->blast_left_q16 = -INT32_C(52) * PF_Q16_ONE;
    stage->blast_right_q16 = INT32_C(52) * PF_Q16_ONE;
    stage->blast_top_q16 = INT32_C(2) * PF_Q16_ONE;
    stage->blast_bottom_q16 = INT32_C(58) * PF_Q16_ONE;
    stage->spawn_spacing_q16 = INT32_C(8) * PF_Q16_ONE;
    stage->platform_motion_period_ticks = UINT16_C(120);

    return PF_STATUS_OK;
}

pf_status pf_m4_validate_content(const pf_m4_content *content)
{
    const pf_m4_fighter_data *fighter;
    const pf_m4_stage_data *stage;
    const int32_t maximum_coordinate_q16 =
        INT32_C(4096) * PF_Q16_ONE;
    const int32_t maximum_fighter_extent_q16 =
        INT32_C(64) * PF_Q16_ONE;
    int64_t platform_left_extent;
    int64_t platform_right_extent;
    int64_t spawn_left_extent;
    int64_t spawn_right_extent;
    int64_t maximum_jab_knockback_x;
    int64_t maximum_jab_knockback_y;
    int64_t maximum_strong_knockback_x;
    int64_t maximum_strong_knockback_y;
    int64_t maximum_aerial_knockback_x;
    int64_t maximum_aerial_knockback_y;
    int64_t maximum_getup_attack_knockback_x;
    int64_t maximum_getup_attack_knockback_y;
    int solid_overlaps_platform;

    if (content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (content->struct_size != (uint32_t)sizeof(*content) ||
        content->schema_version != PF_M4_CONTENT_SCHEMA_VERSION ||
        content->fighter.struct_size !=
            (uint32_t)sizeof(content->fighter) ||
        content->fighter.schema_version !=
            PF_M4_FIGHTER_SCHEMA_VERSION ||
        content->stage.struct_size !=
            (uint32_t)sizeof(content->stage) ||
        content->stage.schema_version != PF_M4_STAGE_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (content->fighter_count != PF_M4_PLACEHOLDER_FIGHTER_COUNT ||
        content->stage_count != PF_M4_TEST_STAGE_COUNT ||
        content->fighter.reserved != UINT16_C(0) ||
        content->stage.reserved != UINT16_C(0) ||
        content->stage.reserved2 != UINT16_C(0))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    fighter = &content->fighter;
    maximum_jab_knockback_x =
        (int64_t)fighter->jab_base_knockback_x_q16 +
        (((int64_t)fighter->jab_knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_jab_knockback_y =
        (int64_t)fighter->jab_base_knockback_y_q16 +
        ((((int64_t)fighter->jab_knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    maximum_strong_knockback_x =
        (int64_t)fighter->strong_base_knockback_x_q16 +
        (((int64_t)fighter->strong_knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_strong_knockback_y =
        (int64_t)fighter->strong_base_knockback_y_q16 +
        ((((int64_t)fighter->strong_knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    maximum_aerial_knockback_x =
        (int64_t)fighter->aerial_base_knockback_x_q16 +
        (((int64_t)fighter->aerial_knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_aerial_knockback_y =
        (int64_t)fighter->aerial_base_knockback_y_q16 +
        ((((int64_t)fighter->aerial_knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    maximum_getup_attack_knockback_x =
        (int64_t)fighter->getup_attack_base_knockback_x_q16 +
        (((int64_t)fighter->getup_attack_knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_getup_attack_knockback_y =
        (int64_t)fighter->getup_attack_base_knockback_y_q16 +
        ((((int64_t)fighter->getup_attack_knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    if (fighter->half_width_q16 <= INT32_C(0) ||
        fighter->half_height_q16 <= INT32_C(0) ||
        fighter->half_width_q16 > maximum_fighter_extent_q16 ||
        fighter->half_height_q16 > maximum_fighter_extent_q16 ||
        fighter->ground_acceleration_q16 <= INT32_C(0) ||
        fighter->turn_acceleration_q16 <
            fighter->ground_acceleration_q16 ||
        fighter->traction_q16 <= INT32_C(0) ||
        fighter->walk_speed_q16 <= INT32_C(0) ||
        fighter->run_speed_q16 <= fighter->walk_speed_q16 ||
        fighter->initial_dash_speed_q16 < fighter->run_speed_q16 ||
        fighter->air_acceleration_q16 <= INT32_C(0) ||
        fighter->air_speed_q16 <= INT32_C(0) ||
        fighter->gravity_q16 <= INT32_C(0) ||
        fighter->fall_speed_q16 <= fighter->gravity_q16 ||
        fighter->fast_fall_speed_q16 <= fighter->fall_speed_q16 ||
        fighter->full_hop_speed_q16 <=
            fighter->short_hop_speed_q16 ||
        fighter->short_hop_speed_q16 <= INT32_C(0) ||
        fighter->double_jump_speed_q16 <= INT32_C(0) ||
        fighter->platform_drop_nudge_q16 <= INT32_C(0) ||
        fighter->air_dodge_speed_q16 <= INT32_C(0) ||
        fighter->air_dodge_decay_q16 <= INT32_C(0) ||
        fighter->air_dodge_decay_q16 > PF_Q16_ONE ||
        fighter->fall_special_mobility_q16 <= INT32_C(0) ||
        fighter->fall_special_mobility_q16 >
            fighter->air_speed_q16 ||
        fighter->jab_hitbox_offset_x_q16 < -maximum_fighter_extent_q16 ||
        fighter->jab_hitbox_offset_x_q16 > maximum_fighter_extent_q16 ||
        fighter->jab_hitbox_offset_y_q16 < -maximum_fighter_extent_q16 ||
        fighter->jab_hitbox_offset_y_q16 > maximum_fighter_extent_q16 ||
        fighter->jab_hitbox_half_width_q16 <= INT32_C(0) ||
        fighter->jab_hitbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->jab_hitbox_half_height_q16 <= INT32_C(0) ||
        fighter->jab_hitbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->jab_damage_q16 == UINT32_C(0) ||
        fighter->jab_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->jab_base_knockback_x_q16 <= INT32_C(0) ||
        fighter->jab_base_knockback_y_q16 <= INT32_C(0) ||
        fighter->jab_knockback_growth_q16 <= INT32_C(0) ||
        fighter->strong_hitbox_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->strong_hitbox_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->strong_hitbox_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->strong_hitbox_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->strong_hitbox_half_width_q16 <= INT32_C(0) ||
        fighter->strong_hitbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->strong_hitbox_half_height_q16 <= INT32_C(0) ||
        fighter->strong_hitbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->strong_damage_q16 == UINT32_C(0) ||
        fighter->strong_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->strong_base_knockback_x_q16 <= INT32_C(0) ||
        fighter->strong_base_knockback_y_q16 <= INT32_C(0) ||
        fighter->strong_knockback_growth_q16 <= INT32_C(0) ||
        fighter->aerial_hitbox_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->aerial_hitbox_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->aerial_hitbox_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->aerial_hitbox_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->aerial_hitbox_half_width_q16 <= INT32_C(0) ||
        fighter->aerial_hitbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->aerial_hitbox_half_height_q16 <= INT32_C(0) ||
        fighter->aerial_hitbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->aerial_damage_q16 == UINT32_C(0) ||
        fighter->aerial_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->aerial_base_knockback_x_q16 <= INT32_C(0) ||
        fighter->aerial_base_knockback_y_q16 <= INT32_C(0) ||
        fighter->aerial_knockback_growth_q16 <= INT32_C(0) ||
        fighter->getup_roll_speed_q16 <= INT32_C(0) ||
        fighter->getup_roll_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->forward_roll_speed_q16 <= INT32_C(0) ||
        fighter->forward_roll_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->backward_roll_speed_q16 <= INT32_C(0) ||
        fighter->backward_roll_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->getup_attack_hitbox_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->getup_attack_hitbox_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->getup_attack_hitbox_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->getup_attack_hitbox_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->getup_attack_hitbox_half_width_q16 <=
            INT32_C(0) ||
        fighter->getup_attack_hitbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->getup_attack_hitbox_half_height_q16 <=
            INT32_C(0) ||
        fighter->getup_attack_hitbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->getup_attack_damage_q16 == UINT32_C(0) ||
        fighter->getup_attack_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->getup_attack_base_knockback_x_q16 <= INT32_C(0) ||
        fighter->getup_attack_base_knockback_y_q16 <= INT32_C(0) ||
        fighter->getup_attack_knockback_growth_q16 <= INT32_C(0) ||
        fighter->hitstun_velocity_per_tick_q16 <= INT32_C(0) ||
        fighter->di_max_tangent_q16 <= INT32_C(0) ||
        fighter->di_max_tangent_q16 > PF_Q16_ONE ||
        fighter->sdi_distance_q16 <= INT32_C(0) ||
        fighter->sdi_distance_q16 >
            INT32_C(4) * PF_Q16_ONE ||
        fighter->asdi_distance_q16 <= INT32_C(0) ||
        fighter->asdi_distance_q16 >
            fighter->sdi_distance_q16 ||
        fighter->tech_roll_speed_q16 <= INT32_C(0) ||
        fighter->tech_roll_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->wall_tech_speed_q16 <= INT32_C(0) ||
        fighter->wall_tech_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->wall_tech_jump_speed_x_q16 <= INT32_C(0) ||
        fighter->wall_tech_jump_speed_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->wall_tech_jump_speed_y_q16 <= fighter->gravity_q16 ||
        fighter->wall_tech_jump_speed_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->ceiling_tech_speed_q16 <= INT32_C(0) ||
        fighter->ceiling_tech_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->surface_bounce_multiplier_q16 <= INT32_C(0) ||
        fighter->surface_bounce_multiplier_q16 > PF_Q16_ONE ||
        fighter->shield_health_q16 == UINT32_C(0) ||
        fighter->shield_health_q16 >
            PF_SIM_MAX_SHIELD_HEALTH_Q16 ||
        fighter->shield_reset_health_q16 == UINT32_C(0) ||
        fighter->shield_reset_health_q16 >
            fighter->shield_health_q16 ||
        fighter->shield_hold_depletion_q16 == UINT32_C(0) ||
        fighter->shield_hold_depletion_q16 >
            fighter->shield_health_q16 ||
        fighter->shield_regeneration_q16 == UINT32_C(0) ||
        fighter->shield_regeneration_q16 >
            fighter->shield_health_q16 ||
        fighter->shield_damage_multiplier_q16 == UINT32_C(0) ||
        fighter->shield_damage_multiplier_q16 >
            UINT32_C(2) * UINT32_C(65536) ||
        fighter->shield_stun_damage_multiplier_q16 <= INT32_C(0) ||
        fighter->shield_stun_damage_multiplier_q16 >
            INT32_C(2) * PF_Q16_ONE ||
        fighter->shield_stun_base_q16 <= INT32_C(0) ||
        fighter->shield_stun_base_q16 >
            INT32_C(16) * PF_Q16_ONE ||
        fighter->shield_defender_pushback_damage_q16 <=
            INT32_C(0) ||
        fighter->shield_defender_pushback_damage_q16 >
            PF_Q16_ONE ||
        fighter->shield_defender_pushback_base_q16 <=
            INT32_C(0) ||
        fighter->shield_defender_pushback_base_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->shield_defender_pushback_scale_q16 <=
            INT32_C(0) ||
        fighter->shield_defender_pushback_scale_q16 >
            PF_Q16_ONE ||
        fighter->shield_attacker_pushback_damage_q16 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_damage_q16 >
            PF_Q16_ONE ||
        fighter->shield_attacker_pushback_base_q16 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_base_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_jab_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_jab_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_strong_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_strong_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_aerial_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_aerial_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_getup_attack_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_getup_attack_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->ground_acceleration_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->turn_acceleration_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->traction_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->walk_speed_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->run_speed_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->initial_dash_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->air_acceleration_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->air_speed_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->gravity_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->fall_speed_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->fast_fall_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->full_hop_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->double_jump_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->platform_drop_nudge_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->air_dodge_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->fall_special_mobility_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->jab_base_knockback_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->jab_base_knockback_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->strong_base_knockback_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->strong_base_knockback_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->aerial_base_knockback_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->aerial_base_knockback_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->getup_attack_base_knockback_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->getup_attack_base_knockback_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->hitstun_velocity_per_tick_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->jump_squat_ticks == UINT16_C(0) ||
        fighter->jump_squat_ticks > UINT16_C(60) ||
        fighter->initial_dash_ticks == UINT16_C(0) ||
        fighter->initial_dash_ticks > UINT16_C(120) ||
        fighter->landing_ticks == UINT16_C(0) ||
        fighter->landing_ticks > UINT16_C(120) ||
        fighter->platform_drop_ticks == UINT16_C(0) ||
        fighter->platform_drop_ticks > UINT16_C(120) ||
        fighter->air_dodge_ticks == UINT16_C(0) ||
        fighter->air_dodge_ticks > UINT16_C(240) ||
        fighter->air_dodge_invulnerability_begin_tick >=
            fighter->air_dodge_invulnerability_end_tick ||
        fighter->air_dodge_invulnerability_end_tick >
            fighter->air_dodge_ticks ||
        fighter->special_landing_ticks == UINT16_C(0) ||
        fighter->special_landing_ticks > UINT16_C(240) ||
        fighter->run_turnaround_ticks < UINT16_C(2) ||
        fighter->run_turnaround_ticks > UINT16_C(120) ||
        fighter->run_brake_ticks == UINT16_C(0) ||
        fighter->run_brake_ticks > UINT16_C(120) ||
        fighter->axis_dead_zone >= fighter->dash_axis_threshold ||
        fighter->dash_axis_threshold > UINT16_C(32767) ||
        fighter->run_turnaround_axis_threshold <=
            fighter->axis_dead_zone ||
        fighter->run_turnaround_axis_threshold >=
            fighter->run_continue_axis_threshold ||
        fighter->run_continue_axis_threshold >
            fighter->dash_axis_threshold ||
        fighter->run_turnaround_lockout_ticks == UINT16_C(0) ||
        fighter->run_turnaround_lockout_ticks > UINT16_C(120) ||
        fighter->crouch_axis_threshold <= fighter->axis_dead_zone ||
        fighter->crouch_axis_threshold > UINT16_C(32767) ||
        fighter->jab_startup_ticks == UINT16_C(0) ||
        fighter->jab_startup_ticks > UINT16_C(120) ||
        fighter->jab_active_ticks == UINT16_C(0) ||
        fighter->jab_active_ticks > UINT16_C(120) ||
        fighter->jab_recovery_ticks == UINT16_C(0) ||
        fighter->jab_recovery_ticks > UINT16_C(240) ||
        fighter->jab_hitlag_ticks == UINT16_C(0) ||
        fighter->jab_hitlag_ticks > UINT16_C(120) ||
        fighter->strong_startup_ticks == UINT16_C(0) ||
        fighter->strong_startup_ticks > UINT16_C(120) ||
        fighter->strong_active_ticks == UINT16_C(0) ||
        fighter->strong_active_ticks > UINT16_C(120) ||
        fighter->strong_recovery_ticks == UINT16_C(0) ||
        fighter->strong_recovery_ticks > UINT16_C(240) ||
        fighter->strong_hitlag_ticks == UINT16_C(0) ||
        fighter->strong_hitlag_ticks > UINT16_C(120) ||
        fighter->aerial_startup_ticks == UINT16_C(0) ||
        fighter->aerial_startup_ticks > UINT16_C(120) ||
        fighter->aerial_active_ticks == UINT16_C(0) ||
        fighter->aerial_active_ticks > UINT16_C(120) ||
        fighter->aerial_recovery_ticks == UINT16_C(0) ||
        fighter->aerial_recovery_ticks > UINT16_C(240) ||
        fighter->aerial_hitlag_ticks == UINT16_C(0) ||
        fighter->aerial_hitlag_ticks > UINT16_C(120) ||
        fighter->aerial_landing_lag_begin_tick >
            fighter->aerial_startup_ticks ||
        fighter->aerial_landing_lag_end_tick <=
            fighter->aerial_landing_lag_begin_tick ||
        fighter->aerial_landing_lag_end_tick >
            fighter->aerial_startup_ticks +
                fighter->aerial_active_ticks +
                fighter->aerial_recovery_ticks ||
        fighter->aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->strong_aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->strong_aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->l_cancel_window_ticks != UINT16_C(7) ||
        fighter->l_cancel_divisor != UINT16_C(2) ||
        fighter->sdi_axis_threshold <= fighter->axis_dead_zone ||
        fighter->sdi_axis_threshold > UINT16_C(32767) ||
        fighter->digital_trigger_threshold == UINT16_C(0) ||
        fighter->tumble_hitstun_threshold_ticks == UINT16_C(0) ||
        fighter->tumble_hitstun_threshold_ticks >
            PF_SIM_MAX_HITSTUN_TICKS ||
        fighter->tech_window_ticks == UINT16_C(0) ||
        fighter->tech_window_ticks > UINT16_C(120) ||
        fighter->tech_lockout_ticks <
            fighter->tech_window_ticks ||
        fighter->tech_lockout_ticks > UINT16_C(240) ||
        fighter->tech_in_place_ticks == UINT16_C(0) ||
        fighter->tech_in_place_ticks > UINT16_C(240) ||
        fighter->tech_roll_ticks == UINT16_C(0) ||
        fighter->tech_roll_ticks > UINT16_C(240) ||
        fighter->tech_invulnerability_ticks == UINT16_C(0) ||
        fighter->tech_invulnerability_ticks >
            fighter->tech_in_place_ticks ||
        fighter->tech_invulnerability_ticks >
            fighter->tech_roll_ticks ||
        fighter->wall_tech_stall_ticks == UINT16_C(0) ||
        fighter->wall_tech_stall_ticks >= fighter->wall_tech_ticks ||
        fighter->wall_tech_ticks > UINT16_C(240) ||
        fighter->ceiling_tech_ticks == UINT16_C(0) ||
        fighter->ceiling_tech_ticks > UINT16_C(240) ||
        fighter->tech_invulnerability_ticks >
            fighter->wall_tech_ticks ||
        fighter->tech_invulnerability_ticks >
            fighter->ceiling_tech_ticks ||
        fighter->knockdown_ticks == UINT16_C(0) ||
        fighter->knockdown_ticks > UINT16_C(480) ||
        fighter->down_wait_ticks == UINT16_C(0) ||
        fighter->down_wait_ticks > UINT16_C(480) ||
        fighter->getup_neutral_ticks == UINT16_C(0) ||
        fighter->getup_neutral_ticks > UINT16_C(240) ||
        fighter->getup_neutral_invulnerability_ticks ==
            UINT16_C(0) ||
        fighter->getup_neutral_invulnerability_ticks >
            fighter->getup_neutral_ticks ||
        fighter->getup_roll_ticks == UINT16_C(0) ||
        fighter->getup_roll_ticks > UINT16_C(240) ||
        fighter->getup_roll_invulnerability_ticks ==
            UINT16_C(0) ||
        fighter->getup_roll_invulnerability_ticks >
            fighter->getup_roll_ticks ||
        fighter->getup_attack_ticks == UINT16_C(0) ||
        fighter->getup_attack_ticks > UINT16_C(240) ||
        fighter->getup_attack_invulnerability_ticks ==
            UINT16_C(0) ||
        fighter->getup_attack_invulnerability_ticks >
            fighter->getup_attack_ticks ||
        fighter->getup_attack_front_active_begin_tick ==
            UINT16_C(0) ||
        fighter->getup_attack_front_active_begin_tick >
            fighter->getup_attack_front_active_end_tick ||
        fighter->getup_attack_front_active_end_tick >=
            fighter->getup_attack_back_active_begin_tick ||
        fighter->getup_attack_back_active_begin_tick >
            fighter->getup_attack_back_active_end_tick ||
        fighter->getup_attack_back_active_end_tick >
        fighter->getup_attack_ticks ||
        fighter->getup_attack_hitlag_ticks == UINT16_C(0) ||
        fighter->getup_attack_hitlag_ticks > UINT16_C(120) ||
        fighter->forward_roll_ticks == UINT16_C(0) ||
        fighter->forward_roll_ticks > UINT16_C(240) ||
        fighter->backward_roll_ticks == UINT16_C(0) ||
        fighter->backward_roll_ticks > UINT16_C(240) ||
        fighter->roll_movement_begin_tick >=
            fighter->roll_movement_end_tick ||
        fighter->roll_movement_end_tick >
            fighter->forward_roll_ticks ||
        fighter->roll_movement_end_tick >
            fighter->backward_roll_ticks ||
        fighter->roll_invulnerability_begin_tick >=
            fighter->roll_invulnerability_end_tick ||
        fighter->roll_invulnerability_end_tick >
            fighter->forward_roll_ticks ||
        fighter->roll_invulnerability_end_tick >
            fighter->backward_roll_ticks ||
        fighter->spot_dodge_ticks == UINT16_C(0) ||
        fighter->spot_dodge_ticks > UINT16_C(240) ||
        fighter->spot_dodge_invulnerability_begin_tick >=
            fighter->spot_dodge_invulnerability_end_tick ||
        fighter->spot_dodge_invulnerability_end_tick >
            fighter->spot_dodge_ticks ||
        fighter->shield_minimum_hold_ticks == UINT16_C(0) ||
        fighter->shield_minimum_hold_ticks > UINT16_C(120) ||
        fighter->shield_release_ticks == UINT16_C(0) ||
        fighter->shield_release_ticks > UINT16_C(240) ||
        fighter->powershield_window_ticks == UINT16_C(0) ||
        fighter->powershield_window_ticks >=
            fighter->shield_minimum_hold_ticks ||
        fighter->powershield_cancel_delay_ticks == UINT16_C(0) ||
        fighter->powershield_cancel_delay_ticks >=
            fighter->shield_release_ticks ||
        fighter->shield_break_ticks == UINT16_C(0) ||
        fighter->shield_break_ticks > UINT16_C(480) ||
        fighter->air_jump_count > UINT8_C(8) ||
        fighter->powershield_cancel_enabled > UINT8_C(1))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    stage = &content->stage;
    platform_left_extent =
        (int64_t)stage->platform_center_x_q16 -
        (int64_t)stage->platform_motion_amplitude_q16 -
        (int64_t)stage->platform_half_width_q16;
    platform_right_extent =
        (int64_t)stage->platform_center_x_q16 +
        (int64_t)stage->platform_motion_amplitude_q16 +
        (int64_t)stage->platform_half_width_q16;
    spawn_left_extent =
        -INT64_C(3) * (int64_t)stage->spawn_spacing_q16 -
        (int64_t)fighter->half_width_q16;
    spawn_right_extent =
        INT64_C(3) * (int64_t)stage->spawn_spacing_q16 +
        (int64_t)fighter->half_width_q16;
    solid_overlaps_platform =
        stage->platform_y_q16 >= stage->solid_top_q16 &&
        stage->platform_y_q16 <= stage->solid_bottom_q16 &&
        platform_right_extent >= (int64_t)stage->solid_left_q16 &&
        platform_left_extent <= (int64_t)stage->solid_right_q16;
    if (stage->floor_left_q16 >= stage->floor_right_q16 ||
        stage->blast_left_q16 >= stage->floor_left_q16 ||
        stage->blast_right_q16 <= stage->floor_right_q16 ||
        stage->blast_top_q16 >= stage->platform_y_q16 ||
        stage->platform_y_q16 >= stage->floor_y_q16 ||
        stage->solid_left_q16 >= stage->solid_right_q16 ||
        stage->solid_left_q16 < stage->floor_left_q16 ||
        stage->solid_right_q16 > stage->floor_right_q16 ||
        stage->blast_top_q16 >= stage->solid_top_q16 ||
        stage->solid_top_q16 >= stage->solid_bottom_q16 ||
        stage->solid_bottom_q16 >= stage->floor_y_q16 ||
        solid_overlaps_platform != 0 ||
        stage->floor_y_q16 >= stage->blast_bottom_q16 ||
        stage->platform_half_width_q16 <= INT32_C(0) ||
        stage->platform_motion_amplitude_q16 < INT32_C(0) ||
        platform_left_extent < (int64_t)stage->floor_left_q16 ||
        platform_right_extent > (int64_t)stage->floor_right_q16 ||
        stage->spawn_spacing_q16 <= INT32_C(0) ||
        spawn_right_extent > (int64_t)stage->floor_right_q16 ||
        spawn_left_extent < (int64_t)stage->floor_left_q16 ||
        stage->blast_left_q16 < -maximum_coordinate_q16 ||
        stage->blast_right_q16 > maximum_coordinate_q16 ||
        stage->blast_top_q16 < INT32_C(0) ||
        stage->blast_bottom_q16 > maximum_coordinate_q16 ||
        stage->platform_motion_period_ticks < UINT16_C(4) ||
        (stage->platform_motion_period_ticks % UINT16_C(4)) !=
            UINT16_C(0))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    return PF_STATUS_OK;
}

pf_status pf_m4_make_content_view(
    const pf_m4_content *content,
    pf_content_view *out_view)
{
    pf_status status;

    if (out_view == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_view, 0, sizeof(*out_view));

    status = pf_m4_validate_content(content);
    if (status != PF_STATUS_OK)
    {
        return status;
    }

    out_view->struct_size = (uint32_t)sizeof(*out_view);
    out_view->schema_version = PF_SIM_CONTENT_SCHEMA_VERSION;
    out_view->bytes = content;
    out_view->byte_count = sizeof(*content);
    pf_m4_content_hash(content, out_view->content_hash.bytes);
    return PF_STATUS_OK;
}

pf_status pf_m4_content_from_view(
    const pf_content_view *view,
    pf_m4_content *out_content)
{
    pf_content_view canonical_view;
    pf_m4_content candidate;
    pf_status status;

    if (view == NULL || out_content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (view->byte_count == (size_t)0)
    {
        return pf_m4_default_content(out_content);
    }
    if (view->bytes == NULL ||
        view->byte_count != sizeof(pf_m4_content))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    (void)memcpy(&candidate, view->bytes, sizeof(candidate));
    status = pf_m4_make_content_view(&candidate, &canonical_view);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (!pf_m4_hash_equal(
            canonical_view.content_hash.bytes,
            view->content_hash.bytes))
    {
        return PF_STATUS_CHECKSUM_MISMATCH;
    }

    *out_content = candidate;
    return PF_STATUS_OK;
}
