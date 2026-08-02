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

static void pf_m4_hash_getup_roll_timing(
    pf_sha256 *hash,
    const pf_m4_getup_roll_timing *timing)
{
    pf_m4_hash_u16(hash, timing->movement_begin_tick);
    pf_m4_hash_u16(hash, timing->invulnerability_begin_tick);
    pf_m4_hash_u16(hash, timing->invulnerability_end_tick);
    pf_m4_hash_u16(hash, timing->reserved);
}

static int pf_m4_getup_roll_timing_is_valid(
    const pf_m4_getup_roll_timing *timing,
    uint16_t total_ticks)
{
    return timing->movement_begin_tick != UINT16_C(0) &&
           timing->movement_begin_tick <= total_ticks &&
           timing->invulnerability_begin_tick != UINT16_C(0) &&
           timing->invulnerability_begin_tick <=
               timing->invulnerability_end_tick &&
           timing->invulnerability_end_tick <= total_ticks &&
           timing->reserved == UINT16_C(0);
}

static void pf_m4_hash_throw(
    pf_sha256 *hash,
    const pf_m4_throw_data *throw_data)
{
    pf_m4_hash_u32(hash, throw_data->damage_q16);
    pf_m4_hash_i32(hash, throw_data->base_velocity_x_q16);
    pf_m4_hash_i32(hash, throw_data->base_velocity_y_q16);
    pf_m4_hash_i32(hash, throw_data->velocity_growth_x_q16);
    pf_m4_hash_i32(hash, throw_data->velocity_growth_y_q16);
    pf_m4_hash_u16(hash, throw_data->release_tick);
    pf_m4_hash_u16(hash, throw_data->recovery_ticks);
    pf_m4_hash_u16(hash, throw_data->hitlag_ticks);
    pf_m4_hash_u16(hash, throw_data->reserved);
}

static void pf_m4_hash_attack(
    pf_sha256 *hash,
    const pf_m4_attack_data *attack)
{
    pf_m4_hash_i32(hash, attack->hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, attack->hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, attack->hitbox_half_width_q16);
    pf_m4_hash_i32(hash, attack->hitbox_half_height_q16);
    pf_m4_hash_u32(hash, attack->damage_q16);
    pf_m4_hash_i32(hash, attack->base_knockback_x_q16);
    pf_m4_hash_i32(hash, attack->base_knockback_y_q16);
    pf_m4_hash_i32(hash, attack->knockback_growth_q16);
    pf_m4_hash_u16(hash, attack->startup_ticks);
    pf_m4_hash_u16(hash, attack->active_ticks);
    pf_m4_hash_u16(hash, attack->recovery_ticks);
    pf_m4_hash_u16(hash, attack->hitlag_ticks);
}

static int pf_m4_attack_data_is_valid(
    const pf_m4_attack_data *attack,
    int32_t maximum_extent_q16)
{
    const int64_t maximum_knockback_x =
        (int64_t)attack->base_knockback_x_q16 +
        (((int64_t)attack->knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    const int64_t maximum_knockback_y =
        (int64_t)attack->base_knockback_y_q16 +
        ((((int64_t)attack->knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));

    return attack->hitbox_offset_x_q16 >= -maximum_extent_q16 &&
           attack->hitbox_offset_x_q16 <= maximum_extent_q16 &&
           attack->hitbox_offset_y_q16 >= -maximum_extent_q16 &&
           attack->hitbox_offset_y_q16 <= maximum_extent_q16 &&
           attack->hitbox_half_width_q16 > INT32_C(0) &&
           attack->hitbox_half_width_q16 <= maximum_extent_q16 &&
           attack->hitbox_half_height_q16 > INT32_C(0) &&
           attack->hitbox_half_height_q16 <= maximum_extent_q16 &&
           attack->damage_q16 != UINT32_C(0) &&
           attack->damage_q16 <= UINT32_C(50) * UINT32_C(65536) &&
           attack->base_knockback_x_q16 > INT32_C(0) &&
           attack->base_knockback_y_q16 > INT32_C(0) &&
           attack->knockback_growth_q16 > INT32_C(0) &&
           maximum_knockback_x <=
               (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 &&
           maximum_knockback_y <=
               (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 &&
           attack->startup_ticks != UINT16_C(0) &&
           attack->startup_ticks <= UINT16_C(120) &&
           attack->active_ticks != UINT16_C(0) &&
           attack->active_ticks <= UINT16_C(120) &&
           attack->recovery_ticks != UINT16_C(0) &&
           attack->recovery_ticks <= UINT16_C(240) &&
           attack->hitlag_ticks != UINT16_C(0) &&
           attack->hitlag_ticks <= UINT16_C(120) &&
           (uint32_t)attack->startup_ticks +
                   (uint32_t)attack->active_ticks +
                   (uint32_t)attack->recovery_ticks <=
               UINT32_C(600);
}

static int pf_m4_charged_attack_damage_is_valid(
    const pf_m4_attack_data *attack,
    uint32_t bonus_q16)
{
    const uint64_t charged_damage =
        (uint64_t)attack->damage_q16 +
        ((uint64_t)attack->damage_q16 * (uint64_t)bonus_q16 >> 16U);

    return charged_damage <=
           (uint64_t)UINT32_C(50) * UINT64_C(65536);
}

static int pf_m4_throw_data_is_valid(
    const pf_m4_throw_data *throw_data)
{
    const int64_t maximum_velocity_x =
        (int64_t)throw_data->base_velocity_x_q16 +
        (((int64_t)throw_data->velocity_growth_x_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    const int64_t maximum_velocity_y =
        (int64_t)throw_data->base_velocity_y_q16 +
        (((int64_t)throw_data->velocity_growth_y_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);

    return throw_data->damage_q16 != UINT32_C(0) &&
           throw_data->damage_q16 <=
               UINT32_C(50) * UINT32_C(65536) &&
           (throw_data->base_velocity_x_q16 != INT32_C(0) ||
            throw_data->base_velocity_y_q16 != INT32_C(0)) &&
           !((throw_data->base_velocity_x_q16 < INT32_C(0) &&
              throw_data->velocity_growth_x_q16 > INT32_C(0)) ||
             (throw_data->base_velocity_x_q16 > INT32_C(0) &&
              throw_data->velocity_growth_x_q16 < INT32_C(0)) ||
             (throw_data->base_velocity_y_q16 < INT32_C(0) &&
              throw_data->velocity_growth_y_q16 > INT32_C(0)) ||
             (throw_data->base_velocity_y_q16 > INT32_C(0) &&
              throw_data->velocity_growth_y_q16 < INT32_C(0))) &&
           maximum_velocity_x >=
               -(int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 &&
           maximum_velocity_x <=
               (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 &&
           maximum_velocity_y >=
               -(int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 &&
           maximum_velocity_y <=
               (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 &&
           throw_data->release_tick != UINT16_C(0) &&
           throw_data->release_tick <= UINT16_C(120) &&
           throw_data->recovery_ticks != UINT16_C(0) &&
           throw_data->recovery_ticks <= UINT16_C(240) &&
           (uint32_t)throw_data->release_tick +
                   (uint32_t)throw_data->recovery_ticks <=
               UINT32_C(600) &&
           throw_data->hitlag_ticks != UINT16_C(0) &&
           throw_data->hitlag_ticks <= UINT16_C(120) &&
           throw_data->reserved == UINT16_C(0);
}

static void pf_m4_hash_fighter(
    pf_sha256 *hash,
    const pf_m4_fighter_data *fighter)
{
    uint32_t stale_index;

    pf_m4_hash_u16(hash, fighter->schema_version);
    pf_m4_hash_i32(hash, fighter->half_width_q16);
    pf_m4_hash_i32(hash, fighter->half_height_q16);
    pf_m4_hash_i32(hash, fighter->weight_q16);
    pf_m4_hash_i32(hash, fighter->ground_acceleration_q16);
    pf_m4_hash_i32(hash, fighter->turn_acceleration_q16);
    pf_m4_hash_i32(hash, fighter->traction_q16);
    pf_m4_hash_i32(hash, fighter->walk_speed_q16);
    pf_m4_hash_i32(hash, fighter->run_speed_q16);
    pf_m4_hash_i32(hash, fighter->initial_dash_speed_q16);
    pf_m4_hash_i32(hash, fighter->teeter_snap_distance_q16);
    pf_m4_hash_i32(hash, fighter->crouch_step_speed_q16);
    pf_m4_hash_i32(hash, fighter->air_acceleration_q16);
    pf_m4_hash_i32(hash, fighter->air_speed_q16);
    pf_m4_hash_i32(hash, fighter->gravity_q16);
    pf_m4_hash_i32(hash, fighter->fall_speed_q16);
    pf_m4_hash_i32(hash, fighter->fast_fall_speed_q16);
    pf_m4_hash_i32(hash, fighter->full_hop_speed_q16);
    pf_m4_hash_i32(hash, fighter->short_hop_speed_q16);
    pf_m4_hash_i32(hash, fighter->double_jump_speed_q16);
    pf_m4_hash_i32(hash, fighter->platform_drop_nudge_q16);
    pf_m4_hash_i32(hash, fighter->ledge_roll_distance_q16);
    pf_m4_hash_i32(hash, fighter->drop_cancel_snap_distance_q16);
    pf_m4_hash_i32(hash, fighter->air_dodge_speed_q16);
    pf_m4_hash_i32(hash, fighter->air_dodge_decay_q16);
    pf_m4_hash_i32(hash, fighter->fall_special_mobility_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_break_launch_speed_q16);
    pf_m4_hash_i32(hash, fighter->dash_attack_speed_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_hitbox_offset_x_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_hitbox_offset_y_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_hitbox_half_width_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->dash_attack_damage_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_base_knockback_x_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_base_knockback_y_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dash_attack_knockback_growth_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_half_width_q16);
    pf_m4_hash_i32(hash, fighter->jab_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->jab_damage_q16);
    pf_m4_hash_i32(hash, fighter->jab_base_knockback_x_q16);
    pf_m4_hash_i32(hash, fighter->jab_base_knockback_y_q16);
    pf_m4_hash_i32(hash, fighter->jab_knockback_growth_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_hitbox_half_width_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_hitbox_half_height_q16);
    pf_m4_hash_u32(hash, fighter->jab_final_damage_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_base_knockback_x_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_base_knockback_y_q16);
    pf_m4_hash_i32(hash, fighter->jab_final_knockback_growth_q16);
    pf_m4_hash_attack(hash, &fighter->up_attack);
    pf_m4_hash_attack(hash, &fighter->down_attack);
    pf_m4_hash_attack(hash, &fighter->forward_attack);
    pf_m4_hash_attack(hash, &fighter->forward_strong_attack);
    pf_m4_hash_attack(hash, &fighter->up_strong_attack);
    pf_m4_hash_attack(hash, &fighter->down_strong_attack);
    pf_m4_hash_u32(hash, fighter->smash_charge_damage_bonus_q16);
    pf_m4_hash_u16(hash, fighter->smash_charge_max_ticks);
    pf_m4_hash_u16(hash, fighter->smash_charge_reserved);
    pf_m4_hash_attack(hash, &fighter->forward_aerial);
    pf_m4_hash_attack(hash, &fighter->back_aerial);
    pf_m4_hash_attack(hash, &fighter->up_aerial);
    pf_m4_hash_attack(hash, &fighter->down_aerial);
    pf_m4_hash_attack(hash, &fighter->ledge_attack);
    pf_m4_hash_u32(hash, fighter->reset_max_damage_q16);
    pf_m4_hash_i32(hash, fighter->reset_bound_speed_q16);
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
    pf_m4_hash_i32(hash, fighter->v_cancel_velocity_scale_q16);
    pf_m4_hash_u32(hash, fighter->crouch_cancel_max_damage_q16);
    pf_m4_hash_i32(hash, fighter->crouch_cancel_velocity_scale_q16);
    pf_m4_hash_i32(hash, fighter->crouch_cancel_hitstun_scale_q16);
    pf_m4_hash_i32(hash, fighter->di_max_tangent_q16);
    pf_m4_hash_i32(hash, fighter->sdi_distance_q16);
    pf_m4_hash_i32(hash, fighter->asdi_distance_q16);
    pf_m4_hash_i32(hash, fighter->shield_sdi_scale_q16);
    pf_m4_hash_i32(hash, fighter->tech_roll_speed_q16);
    pf_m4_hash_i32(hash, fighter->wall_tech_speed_q16);
    pf_m4_hash_i32(hash, fighter->wall_tech_jump_speed_x_q16);
    pf_m4_hash_i32(hash, fighter->wall_tech_jump_speed_y_q16);
    pf_m4_hash_i32(hash, fighter->wall_jump_speed_x_q16);
    pf_m4_hash_i32(hash, fighter->wall_jump_speed_y_q16);
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
    pf_m4_hash_u32(
        hash,
        fighter->light_shield_hold_depletion_q16);
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
        fighter->light_shield_defender_pushback_scale_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_attacker_pushback_damage_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_attacker_pushback_base_q16);
    pf_m4_hash_i32(hash, fighter->shield_half_width_q16);
    pf_m4_hash_i32(hash, fighter->shield_half_height_q16);
    pf_m4_hash_i32(
        hash,
        fighter->shield_minimum_size_scale_q16);
    pf_m4_hash_i32(
        hash,
        fighter->dense_shield_size_scale_q16);
    pf_m4_hash_i32(hash, fighter->shield_tilt_max_x_q16);
    pf_m4_hash_i32(hash, fighter->shield_tilt_max_y_q16);
    pf_m4_hash_i32(hash, fighter->grabbox_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->grabbox_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->grabbox_half_width_q16);
    pf_m4_hash_i32(hash, fighter->grabbox_half_height_q16);
    pf_m4_hash_i32(hash, fighter->grabbed_offset_x_q16);
    pf_m4_hash_i32(hash, fighter->grabbed_offset_y_q16);
    pf_m4_hash_i32(hash, fighter->grab_escape_damage_ticks_q16);
    pf_m4_hash_u32(hash, fighter->pummel_damage_q16);
    pf_m4_hash_u16(hash, fighter->pummel_hit_tick);
    pf_m4_hash_u16(hash, fighter->pummel_total_ticks);
    pf_m4_hash_throw(hash, &fighter->forward_throw);
    pf_m4_hash_throw(hash, &fighter->back_throw);
    pf_m4_hash_throw(hash, &fighter->up_throw);
    pf_m4_hash_throw(hash, &fighter->down_throw);
    pf_m4_hash_u16(hash, fighter->jump_squat_ticks);
    pf_m4_hash_u16(hash, fighter->double_jump_cancel_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->double_jump_armor_max_hitstun_ticks);
    pf_m4_hash_u16(hash, fighter->initial_dash_ticks);
    pf_m4_hash_u16(hash, fighter->moonwalk_setup_ticks);
    pf_m4_hash_u16(hash, fighter->teeter_ticks);
    pf_m4_hash_u16(hash, fighter->crouch_step_ticks);
    pf_m4_hash_u16(hash, fighter->taunt_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->forward_smash_input_window_ticks);
    pf_m4_hash_u16(hash, fighter->landing_ticks);
    pf_m4_hash_u16(hash, fighter->platform_drop_ticks);
    pf_m4_hash_u16(hash, fighter->air_dodge_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->air_dodge_invulnerability_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->air_dodge_invulnerability_end_tick);
    pf_m4_hash_u16(hash, fighter->ledge_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->ledge_regrab_lockout_ticks);
    pf_m4_hash_u16(hash, fighter->ledge_roll_ticks);
    pf_m4_hash_u16(hash, fighter->ledge_roll_movement_ticks);
    pf_m4_hash_u16(hash, fighter->ledge_roll_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->ledge_attack_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->special_landing_ticks);
    pf_m4_hash_u16(hash, fighter->run_turnaround_ticks);
    pf_m4_hash_u16(hash, fighter->run_brake_ticks);
    pf_m4_hash_u16(hash, fighter->axis_dead_zone);
    pf_m4_hash_u16(hash, fighter->dash_axis_threshold);
    pf_m4_hash_u16(hash, fighter->run_turnaround_axis_threshold);
    pf_m4_hash_u16(hash, fighter->run_continue_axis_threshold);
    pf_m4_hash_u16(hash, fighter->run_turnaround_lockout_ticks);
    pf_m4_hash_u16(hash, fighter->crouch_axis_threshold);
    pf_m4_hash_u16(hash, fighter->shield_drop_axis_threshold);
    pf_m4_hash_u16(hash, fighter->dash_attack_startup_ticks);
    pf_m4_hash_u16(hash, fighter->dash_attack_active_ticks);
    pf_m4_hash_u16(hash, fighter->dash_attack_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->dash_attack_hitlag_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->boost_grab_cancel_begin_tick);
    pf_m4_hash_u16(
        hash,
        fighter->boost_grab_cancel_end_tick);
    pf_m4_hash_u16(hash, fighter->jab_startup_ticks);
    pf_m4_hash_u16(hash, fighter->jab_active_ticks);
    pf_m4_hash_u16(hash, fighter->jab_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->jab_hitlag_ticks);
    pf_m4_hash_u16(hash, fighter->jab_combo_input_begin_tick);
    pf_m4_hash_u16(hash, fighter->jab_combo_input_end_tick);
    pf_m4_hash_u16(hash, fighter->jab_final_startup_ticks);
    pf_m4_hash_u16(hash, fighter->jab_final_active_ticks);
    pf_m4_hash_u16(hash, fighter->jab_final_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->jab_final_hitlag_ticks);
    pf_m4_hash_u16(hash, fighter->reset_max_hitstun_ticks);
    pf_m4_hash_u16(hash, fighter->reset_bound_ticks);
    pf_m4_hash_u16(hash, fighter->reset_forced_getup_ticks);
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
    pf_m4_hash_u16(hash, fighter->v_cancel_window_ticks);
    pf_m4_hash_u16(hash, fighter->sdi_axis_threshold);
    pf_m4_hash_u16(
        hash,
        fighter->light_shield_trigger_threshold);
    pf_m4_hash_u16(hash, fighter->digital_trigger_threshold);
    pf_m4_hash_u16(hash, fighter->tumble_hitstun_threshold_ticks);
    pf_m4_hash_u16(hash, fighter->tech_window_ticks);
    pf_m4_hash_u16(hash, fighter->tech_lockout_ticks);
    pf_m4_hash_u16(hash, fighter->tech_in_place_ticks);
    pf_m4_hash_u16(hash, fighter->tech_roll_ticks);
    pf_m4_hash_u16(hash, fighter->tech_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->wall_tech_stall_ticks);
    pf_m4_hash_u16(hash, fighter->wall_tech_ticks);
    pf_m4_hash_u16(hash, fighter->wall_jump_ticks);
    pf_m4_hash_u16(hash, fighter->wall_jump_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->ceiling_tech_ticks);
    pf_m4_hash_u16(hash, fighter->knockdown_ticks);
    pf_m4_hash_u16(hash, fighter->down_wait_ticks);
    pf_m4_hash_u16(hash, fighter->getup_neutral_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->getup_neutral_invulnerability_ticks);
    pf_m4_hash_u16(hash, fighter->getup_roll_ticks);
    pf_m4_hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_back_forward);
    pf_m4_hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_back_backward);
    pf_m4_hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_stomach_forward);
    pf_m4_hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_stomach_backward);
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
    pf_m4_hash_u16(hash, fighter->shield_break_stun_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->shield_break_minimum_stun_ticks);
    pf_m4_hash_u16(hash, fighter->shield_break_down_ticks);
    pf_m4_hash_u16(hash, fighter->shield_break_stand_ticks);
    pf_m4_hash_u16(
        hash,
        fighter->shield_break_mash_reduction_ticks);
    pf_m4_hash_u16(hash, fighter->grab_startup_ticks);
    pf_m4_hash_u16(hash, fighter->grab_active_ticks);
    pf_m4_hash_u16(hash, fighter->grab_recovery_ticks);
    pf_m4_hash_u16(hash, fighter->grab_escape_base_ticks);
    pf_m4_hash_u16(hash, fighter->grab_escape_max_ticks);
    pf_m4_hash_u16(hash, fighter->grab_mash_reduction_ticks);
    pf_m4_hash_u16(hash, fighter->grab_release_ticks);
    pf_m4_hash_u8(hash, fighter->air_jump_count);
    pf_m4_hash_u8(
        hash,
        fighter->powershield_cancel_enabled);
    pf_m4_hash_u8(hash, fighter->wall_jump_enabled);
    for (stale_index = UINT32_C(0);
         stale_index <
             (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++stale_index)
    {
        pf_m4_hash_u16(
            hash,
            fighter->stale_move_slot_reduction_q16[stale_index]);
    }
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
    pf_m4_hash_i32(hash, stage->revival_platform_start_y_q16);
    pf_m4_hash_i32(hash, stage->revival_platform_end_y_q16);
    pf_m4_hash_i32(hash, stage->revival_platform_half_width_q16);
    pf_m4_hash_u16(hash, stage->revival_platform_descent_ticks);
    pf_m4_hash_u16(hash, stage->revival_platform_hold_ticks);
    pf_m4_hash_i32(hash, stage->upper_platform_center_x_q16);
    pf_m4_hash_i32(hash, stage->upper_platform_y_q16);
    pf_m4_hash_i32(hash, stage->upper_platform_half_width_q16);
}

static void pf_m4_hash_item(
    pf_sha256 *hash,
    const pf_m4_item_data *item)
{
    pf_m4_hash_u16(hash, item->schema_version);
    pf_m4_hash_u8(hash, item->enabled);
    pf_m4_hash_i32(hash, item->half_width_q16);
    pf_m4_hash_i32(hash, item->half_height_q16);
    pf_m4_hash_i32(hash, item->spawn_x_q16);
    pf_m4_hash_i32(hash, item->spawn_y_q16);
    pf_m4_hash_i32(hash, item->pickup_half_width_q16);
    pf_m4_hash_i32(hash, item->pickup_half_height_q16);
    pf_m4_hash_i32(hash, item->held_offset_x_q16);
    pf_m4_hash_i32(hash, item->held_offset_y_q16);
    pf_m4_hash_i32(hash, item->gravity_q16);
    pf_m4_hash_i32(hash, item->fall_speed_q16);
    pf_m4_hash_i32(hash, item->drop_velocity_y_q16);
    pf_m4_hash_i32(hash, item->forward_throw.velocity_x_q16);
    pf_m4_hash_i32(hash, item->forward_throw.velocity_y_q16);
    pf_m4_hash_i32(hash, item->back_throw.velocity_x_q16);
    pf_m4_hash_i32(hash, item->back_throw.velocity_y_q16);
    pf_m4_hash_i32(hash, item->up_throw.velocity_x_q16);
    pf_m4_hash_i32(hash, item->up_throw.velocity_y_q16);
    pf_m4_hash_i32(hash, item->down_throw.velocity_x_q16);
    pf_m4_hash_i32(hash, item->down_throw.velocity_y_q16);
    pf_m4_hash_i32(hash, item->momentum_transfer_q16);
    pf_m4_hash_i32(hash, item->hitbox_half_width_q16);
    pf_m4_hash_i32(hash, item->hitbox_half_height_q16);
    pf_m4_hash_u32(hash, item->damage_q16);
    pf_m4_hash_i32(hash, item->base_knockback_x_q16);
    pf_m4_hash_i32(hash, item->base_knockback_y_q16);
    pf_m4_hash_i32(hash, item->knockback_growth_q16);
    pf_m4_hash_i32(hash, item->hit_bounce_velocity_y_q16);
    pf_m4_hash_i32(hash, item->dash_throw_speed_q16);
    pf_m4_hash_u16(hash, item->throw_recovery_ticks);
    pf_m4_hash_u16(hash, item->dash_throw_recovery_ticks);
    pf_m4_hash_u16(hash, item->glide_toss_begin_tick);
    pf_m4_hash_u16(hash, item->glide_toss_end_tick);
    pf_m4_hash_u16(hash, item->pickup_lockout_ticks);
    pf_m4_hash_u16(hash, item->lifetime_ticks);
    pf_m4_hash_u16(hash, item->respawn_ticks);
    pf_m4_hash_u16(hash, item->hitlag_ticks);
}

static void pf_m4_hash_projectile(
    pf_sha256 *hash,
    const pf_m4_projectile_data *projectile)
{
    pf_m4_hash_u16(hash, projectile->schema_version);
    pf_m4_hash_u8(hash, projectile->enabled);
    pf_m4_hash_i32(hash, projectile->half_width_q16);
    pf_m4_hash_i32(hash, projectile->half_height_q16);
    pf_m4_hash_i32(hash, projectile->spawn_offset_x_q16);
    pf_m4_hash_i32(hash, projectile->spawn_offset_y_q16);
    pf_m4_hash_i32(hash, projectile->speed_q16);
    pf_m4_hash_u32(hash, projectile->damage_q16);
    pf_m4_hash_i32(hash, projectile->base_knockback_x_q16);
    pf_m4_hash_i32(hash, projectile->base_knockback_y_q16);
    pf_m4_hash_i32(hash, projectile->knockback_growth_q16);
    pf_m4_hash_u16(hash, projectile->lifetime_ticks);
    pf_m4_hash_u16(hash, projectile->fire_recovery_ticks);
    pf_m4_hash_u16(hash, projectile->hitlag_ticks);
    pf_m4_hash_u16(hash, projectile->powershield_reflect_window_ticks);
}

static void pf_m4_hash_reflector(
    pf_sha256 *hash,
    const pf_m4_reflector_data *reflector)
{
    pf_m4_hash_u16(hash, reflector->schema_version);
    pf_m4_hash_u8(hash, reflector->enabled);
    pf_m4_hash_i32(hash, reflector->hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, reflector->hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, reflector->hitbox_half_width_q16);
    pf_m4_hash_i32(hash, reflector->hitbox_half_height_q16);
    pf_m4_hash_u32(hash, reflector->damage_q16);
    pf_m4_hash_i32(hash, reflector->base_knockback_x_q16);
    pf_m4_hash_i32(hash, reflector->base_knockback_y_q16);
    pf_m4_hash_i32(hash, reflector->knockback_growth_q16);
    pf_m4_hash_u16(hash, reflector->startup_ticks);
    pf_m4_hash_u16(hash, reflector->active_ticks);
    pf_m4_hash_u16(hash, reflector->recovery_ticks);
    pf_m4_hash_u16(hash, reflector->hitlag_ticks);
}

static void pf_m4_hash_charge(
    pf_sha256 *hash,
    const pf_m4_charge_data *charge)
{
    pf_m4_hash_u16(hash, charge->schema_version);
    pf_m4_hash_u8(hash, charge->enabled);
    pf_m4_hash_i32(hash, charge->hitbox_offset_x_q16);
    pf_m4_hash_i32(hash, charge->hitbox_offset_y_q16);
    pf_m4_hash_i32(hash, charge->hitbox_half_width_q16);
    pf_m4_hash_i32(hash, charge->hitbox_half_height_q16);
    pf_m4_hash_u32(hash, charge->base_damage_q16);
    pf_m4_hash_u32(hash, charge->bonus_damage_q16);
    pf_m4_hash_i32(hash, charge->base_knockback_x_q16);
    pf_m4_hash_i32(hash, charge->base_knockback_y_q16);
    pf_m4_hash_i32(hash, charge->knockback_growth_q16);
    pf_m4_hash_u16(hash, charge->max_charge_ticks);
    pf_m4_hash_u16(hash, charge->store_animation_ticks);
    pf_m4_hash_u16(hash, charge->release_startup_ticks);
    pf_m4_hash_u16(hash, charge->release_active_ticks);
    pf_m4_hash_u16(hash, charge->release_recovery_ticks);
    pf_m4_hash_u16(hash, charge->release_hitlag_ticks);
}

static void pf_m4_hash_recovery(
    pf_sha256 *hash,
    const pf_m4_recovery_data *recovery)
{
    pf_m4_hash_u16(hash, recovery->schema_version);
    pf_m4_hash_u8(hash, recovery->enabled);
    pf_m4_hash_i32(hash, recovery->horizontal_speed_q16);
    pf_m4_hash_i32(hash, recovery->vertical_speed_q16);
    pf_m4_hash_u16(hash, recovery->ascent_ticks);
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
    pf_m4_hash_u8(&hash, content->item_count);
    pf_m4_hash_u8(&hash, content->projectile_count);
    pf_m4_hash_u8(&hash, content->reflector_count);
    pf_m4_hash_u8(&hash, content->charge_count);
    pf_m4_hash_u8(&hash, content->recovery_count);
    pf_m4_hash_fighter(&hash, &content->fighter);
    pf_m4_hash_stage(&hash, &content->stage);
    pf_m4_hash_item(&hash, &content->item);
    pf_m4_hash_projectile(&hash, &content->projectile);
    pf_m4_hash_reflector(&hash, &content->reflector);
    pf_m4_hash_charge(&hash, &content->charge);
    pf_m4_hash_recovery(&hash, &content->recovery);
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

const pf_m4_getup_roll_timing *pf_m4_getup_roll_timing_for(
    const pf_m4_fighter_data *fighter,
    uint8_t prone_orientation,
    int8_t roll_direction,
    int8_t facing)
{
    const int is_forward = roll_direction == facing;

    if (fighter == NULL ||
        (roll_direction != INT8_C(-1) &&
         roll_direction != INT8_C(1)) ||
        (facing != INT8_C(-1) && facing != INT8_C(1)))
    {
        return NULL;
    }

    if (prone_orientation == (uint8_t)PF_M4_PRONE_BACK)
    {
        return is_forward ? &fighter->getup_roll_back_forward
                          : &fighter->getup_roll_back_backward;
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_STOMACH)
    {
        return is_forward ? &fighter->getup_roll_stomach_forward
                          : &fighter->getup_roll_stomach_backward;
    }
    return NULL;
}

pf_status pf_m4_default_content(pf_m4_content *out_content)
{
    pf_m4_fighter_data *fighter;
    pf_m4_stage_data *stage;
    pf_m4_item_data *item;
    pf_m4_projectile_data *projectile;
    pf_m4_reflector_data *reflector;
    pf_m4_charge_data *charge;
    pf_m4_recovery_data *recovery;

    if (out_content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(out_content, 0, sizeof(*out_content));
    out_content->struct_size = (uint32_t)sizeof(*out_content);
    out_content->schema_version = PF_M4_CONTENT_SCHEMA_VERSION;
    out_content->fighter_count = PF_M4_PLACEHOLDER_FIGHTER_COUNT;
    out_content->stage_count = PF_M4_TEST_STAGE_COUNT;
    out_content->item_count = PF_M4_TEST_ITEM_COUNT;
    out_content->projectile_count = PF_M4_TEST_PROJECTILE_COUNT;
    out_content->reflector_count = PF_M4_TEST_REFLECTOR_COUNT;
    out_content->charge_count = PF_M4_TEST_CHARGE_COUNT;
    out_content->recovery_count = PF_M4_TEST_RECOVERY_COUNT;

    fighter = &out_content->fighter;
    fighter->struct_size = (uint32_t)sizeof(*fighter);
    fighter->schema_version = PF_M4_FIGHTER_SCHEMA_VERSION;
    fighter->half_width_q16 = PF_Q16_RATIO(9, 20);
    fighter->half_height_q16 = PF_Q16_RATIO(4, 5);
    fighter->weight_q16 = PF_Q16_ONE;
    fighter->ground_acceleration_q16 = PF_Q16_RATIO(1, 40);
    fighter->turn_acceleration_q16 = PF_Q16_RATIO(1, 25);
    fighter->traction_q16 = PF_Q16_RATIO(1, 50);
    fighter->walk_speed_q16 = PF_Q16_RATIO(3, 20);
    fighter->run_speed_q16 = PF_Q16_RATIO(6, 25);
    fighter->initial_dash_speed_q16 = PF_Q16_RATIO(3, 10);
    fighter->teeter_snap_distance_q16 = PF_Q16_RATIO(2, 5);
    fighter->crouch_step_speed_q16 = PF_Q16_RATIO(1, 10);
    fighter->air_acceleration_q16 = PF_Q16_RATIO(1, 100);
    fighter->air_speed_q16 = PF_Q16_RATIO(4, 25);
    fighter->gravity_q16 = PF_Q16_RATIO(1, 50);
    fighter->fall_speed_q16 = PF_Q16_RATIO(2, 5);
    fighter->fast_fall_speed_q16 = PF_Q16_RATIO(3, 5);
    fighter->full_hop_speed_q16 = PF_Q16_RATIO(11, 20);
    fighter->short_hop_speed_q16 = PF_Q16_RATIO(9, 25);
    fighter->double_jump_speed_q16 = PF_Q16_RATIO(1, 2);
    fighter->platform_drop_nudge_q16 = PF_Q16_RATIO(1, 256);
    fighter->ledge_roll_distance_q16 = PF_Q16_RATIO(7, 4);
    fighter->drop_cancel_snap_distance_q16 = PF_Q16_RATIO(5, 8);
    fighter->air_dodge_speed_q16 = PF_Q16_RATIO(1, 2);
    fighter->air_dodge_decay_q16 = PF_Q16_RATIO(9, 10);
    fighter->fall_special_mobility_q16 = PF_Q16_RATIO(2, 25);
    fighter->shield_break_launch_speed_q16 =
        PF_Q16_RATIO(7, 10);
    fighter->dash_attack_speed_q16 = PF_Q16_RATIO(7, 20);
    fighter->dash_attack_hitbox_offset_x_q16 =
        PF_Q16_RATIO(4, 5);
    fighter->dash_attack_hitbox_offset_y_q16 =
        PF_Q16_RATIO(1, 20);
    fighter->dash_attack_hitbox_half_width_q16 =
        PF_Q16_RATIO(13, 20);
    fighter->dash_attack_hitbox_half_height_q16 =
        PF_Q16_RATIO(9, 20);
    fighter->dash_attack_damage_q16 =
        UINT32_C(8) * UINT32_C(65536);
    fighter->dash_attack_base_knockback_x_q16 =
        PF_Q16_RATIO(6, 25);
    fighter->dash_attack_base_knockback_y_q16 =
        PF_Q16_RATIO(3, 10);
    fighter->dash_attack_knockback_growth_q16 =
        PF_Q16_RATIO(1, 768);
    fighter->jab_hitbox_offset_x_q16 = PF_Q16_RATIO(3, 4);
    fighter->jab_hitbox_offset_y_q16 = INT32_C(0);
    fighter->jab_hitbox_half_width_q16 = PF_Q16_RATIO(3, 5);
    fighter->jab_hitbox_half_height_q16 = PF_Q16_RATIO(9, 20);
    fighter->jab_damage_q16 = UINT32_C(6) * UINT32_C(65536);
    fighter->jab_base_knockback_x_q16 = PF_Q16_RATIO(9, 50);
    fighter->jab_base_knockback_y_q16 = PF_Q16_RATIO(11, 50);
    fighter->jab_knockback_growth_q16 = PF_Q16_RATIO(1, 512);
    fighter->jab_final_hitbox_offset_x_q16 = PF_Q16_RATIO(4, 5);
    fighter->jab_final_hitbox_offset_y_q16 = INT32_C(0);
    fighter->jab_final_hitbox_half_width_q16 = PF_Q16_RATIO(13, 20);
    fighter->jab_final_hitbox_half_height_q16 = PF_Q16_RATIO(9, 20);
    fighter->jab_final_damage_q16 = UINT32_C(7) * UINT32_C(65536);
    fighter->jab_final_base_knockback_x_q16 = PF_Q16_RATIO(1, 4);
    fighter->jab_final_base_knockback_y_q16 = PF_Q16_RATIO(3, 10);
    fighter->jab_final_knockback_growth_q16 = PF_Q16_RATIO(1, 512);
    fighter->up_attack.hitbox_offset_x_q16 = PF_Q16_RATIO(1, 4);
    fighter->up_attack.hitbox_offset_y_q16 = -PF_Q16_RATIO(1, 2);
    fighter->up_attack.hitbox_half_width_q16 = PF_Q16_RATIO(13, 20);
    fighter->up_attack.hitbox_half_height_q16 = PF_Q16_RATIO(3, 4);
    fighter->up_attack.damage_q16 = UINT32_C(9) * UINT32_C(65536);
    fighter->up_attack.base_knockback_x_q16 = PF_Q16_RATIO(1, 10);
    fighter->up_attack.base_knockback_y_q16 = PF_Q16_RATIO(21, 50);
    fighter->up_attack.knockback_growth_q16 = PF_Q16_RATIO(1, 640);
    fighter->up_attack.startup_ticks = UINT16_C(4);
    fighter->up_attack.active_ticks = UINT16_C(3);
    fighter->up_attack.recovery_ticks = UINT16_C(12);
    fighter->up_attack.hitlag_ticks = UINT16_C(5);
    fighter->down_attack.hitbox_offset_x_q16 = PF_Q16_RATIO(3, 5);
    fighter->down_attack.hitbox_offset_y_q16 = PF_Q16_RATIO(9, 20);
    fighter->down_attack.hitbox_half_width_q16 = PF_Q16_RATIO(3, 4);
    fighter->down_attack.hitbox_half_height_q16 = PF_Q16_RATIO(7, 20);
    fighter->down_attack.damage_q16 = UINT32_C(8) * UINT32_C(65536);
    fighter->down_attack.base_knockback_x_q16 = PF_Q16_RATIO(1, 5);
    fighter->down_attack.base_knockback_y_q16 = PF_Q16_RATIO(9, 50);
    fighter->down_attack.knockback_growth_q16 = PF_Q16_RATIO(1, 768);
    fighter->down_attack.startup_ticks = UINT16_C(5);
    fighter->down_attack.active_ticks = UINT16_C(3);
    fighter->down_attack.recovery_ticks = UINT16_C(11);
    fighter->down_attack.hitlag_ticks = UINT16_C(4);
    fighter->forward_attack.hitbox_offset_x_q16 =
        PF_Q16_RATIO(4, 5);
    fighter->forward_attack.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(1, 20);
    fighter->forward_attack.hitbox_half_width_q16 =
        PF_Q16_RATIO(7, 10);
    fighter->forward_attack.hitbox_half_height_q16 =
        PF_Q16_RATIO(9, 20);
    fighter->forward_attack.damage_q16 =
        UINT32_C(7) * UINT32_C(65536);
    fighter->forward_attack.base_knockback_x_q16 =
        PF_Q16_RATIO(6, 25);
    fighter->forward_attack.base_knockback_y_q16 =
        PF_Q16_RATIO(1, 4);
    fighter->forward_attack.knockback_growth_q16 =
        PF_Q16_RATIO(1, 704);
    fighter->forward_attack.startup_ticks = UINT16_C(4);
    fighter->forward_attack.active_ticks = UINT16_C(3);
    fighter->forward_attack.recovery_ticks = UINT16_C(12);
    fighter->forward_attack.hitlag_ticks = UINT16_C(4);
    fighter->forward_strong_attack.hitbox_offset_x_q16 =
        PF_Q16_RATIO(9, 10);
    fighter->forward_strong_attack.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(1, 10);
    fighter->forward_strong_attack.hitbox_half_width_q16 =
        PF_Q16_RATIO(3, 4);
    fighter->forward_strong_attack.hitbox_half_height_q16 =
        PF_Q16_RATIO(11, 20);
    fighter->forward_strong_attack.damage_q16 =
        UINT32_C(12) * UINT32_C(65536);
    fighter->forward_strong_attack.base_knockback_x_q16 =
        PF_Q16_RATIO(9, 20);
    fighter->forward_strong_attack.base_knockback_y_q16 =
        PF_Q16_RATIO(17, 20);
    fighter->forward_strong_attack.knockback_growth_q16 =
        PF_Q16_RATIO(1, 512);
    fighter->forward_strong_attack.startup_ticks = UINT16_C(5);
    fighter->forward_strong_attack.active_ticks = UINT16_C(3);
    fighter->forward_strong_attack.recovery_ticks = UINT16_C(18);
    fighter->forward_strong_attack.hitlag_ticks = UINT16_C(6);
    fighter->up_strong_attack.hitbox_offset_x_q16 =
        PF_Q16_RATIO(1, 10);
    fighter->up_strong_attack.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(4, 5);
    fighter->up_strong_attack.hitbox_half_width_q16 =
        PF_Q16_RATIO(11, 10);
    fighter->up_strong_attack.hitbox_half_height_q16 =
        PF_Q16_RATIO(4, 5);
    fighter->up_strong_attack.damage_q16 =
        UINT32_C(13) * UINT32_C(65536);
    fighter->up_strong_attack.base_knockback_x_q16 =
        PF_Q16_RATIO(3, 20);
    fighter->up_strong_attack.base_knockback_y_q16 =
        PF_Q16_RATIO(9, 10);
    fighter->up_strong_attack.knockback_growth_q16 =
        PF_Q16_RATIO(1, 544);
    fighter->up_strong_attack.startup_ticks = UINT16_C(7);
    fighter->up_strong_attack.active_ticks = UINT16_C(4);
    fighter->up_strong_attack.recovery_ticks = UINT16_C(22);
    fighter->up_strong_attack.hitlag_ticks = UINT16_C(6);
    fighter->down_strong_attack.hitbox_offset_x_q16 =
        PF_Q16_RATIO(7, 10);
    fighter->down_strong_attack.hitbox_offset_y_q16 =
        PF_Q16_RATIO(2, 5);
    fighter->down_strong_attack.hitbox_half_width_q16 =
        PF_Q16_RATIO(9, 10);
    fighter->down_strong_attack.hitbox_half_height_q16 =
        PF_Q16_RATIO(2, 5);
    fighter->down_strong_attack.damage_q16 =
        UINT32_C(11) * UINT32_C(65536);
    fighter->down_strong_attack.base_knockback_x_q16 =
        PF_Q16_RATIO(2, 5);
    fighter->down_strong_attack.base_knockback_y_q16 =
        PF_Q16_RATIO(3, 10);
    fighter->down_strong_attack.knockback_growth_q16 =
        PF_Q16_RATIO(1, 576);
    fighter->down_strong_attack.startup_ticks = UINT16_C(6);
    fighter->down_strong_attack.active_ticks = UINT16_C(4);
    fighter->down_strong_attack.recovery_ticks = UINT16_C(20);
    fighter->down_strong_attack.hitlag_ticks = UINT16_C(5);
    fighter->smash_charge_damage_bonus_q16 = PF_Q16_RATIO(1, 2);
    fighter->smash_charge_max_ticks = UINT16_C(60);
    fighter->forward_aerial.hitbox_offset_x_q16 =
        PF_Q16_RATIO(3, 4);
    fighter->forward_aerial.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(1, 20);
    fighter->forward_aerial.hitbox_half_width_q16 =
        PF_Q16_RATIO(13, 20);
    fighter->forward_aerial.hitbox_half_height_q16 =
        PF_Q16_RATIO(9, 20);
    fighter->forward_aerial.damage_q16 =
        UINT32_C(10) * UINT32_C(65536);
    fighter->forward_aerial.base_knockback_x_q16 =
        PF_Q16_RATIO(3, 10);
    fighter->forward_aerial.base_knockback_y_q16 =
        PF_Q16_RATIO(1, 4);
    fighter->forward_aerial.knockback_growth_q16 =
        PF_Q16_RATIO(1, 640);
    fighter->forward_aerial.startup_ticks = UINT16_C(5);
    fighter->forward_aerial.active_ticks = UINT16_C(4);
    fighter->forward_aerial.recovery_ticks = UINT16_C(19);
    fighter->forward_aerial.hitlag_ticks = UINT16_C(5);
    fighter->back_aerial.hitbox_offset_x_q16 =
        PF_Q16_RATIO(7, 10);
    fighter->back_aerial.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(1, 10);
    fighter->back_aerial.hitbox_half_width_q16 =
        PF_Q16_RATIO(7, 10);
    fighter->back_aerial.hitbox_half_height_q16 =
        PF_Q16_RATIO(1, 2);
    fighter->back_aerial.damage_q16 =
        UINT32_C(11) * UINT32_C(65536);
    fighter->back_aerial.base_knockback_x_q16 =
        PF_Q16_RATIO(19, 50);
    fighter->back_aerial.base_knockback_y_q16 =
        PF_Q16_RATIO(11, 50);
    fighter->back_aerial.knockback_growth_q16 =
        PF_Q16_RATIO(1, 576);
    fighter->back_aerial.startup_ticks = UINT16_C(4);
    fighter->back_aerial.active_ticks = UINT16_C(4);
    fighter->back_aerial.recovery_ticks = UINT16_C(20);
    fighter->back_aerial.hitlag_ticks = UINT16_C(5);
    fighter->up_aerial.hitbox_offset_x_q16 =
        PF_Q16_RATIO(1, 10);
    fighter->up_aerial.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(7, 10);
    fighter->up_aerial.hitbox_half_width_q16 =
        PF_Q16_RATIO(13, 20);
    fighter->up_aerial.hitbox_half_height_q16 =
        PF_Q16_RATIO(13, 20);
    fighter->up_aerial.damage_q16 =
        UINT32_C(9) * UINT32_C(65536);
    fighter->up_aerial.base_knockback_x_q16 =
        PF_Q16_RATIO(3, 25);
    fighter->up_aerial.base_knockback_y_q16 =
        PF_Q16_RATIO(19, 50);
    fighter->up_aerial.knockback_growth_q16 =
        PF_Q16_RATIO(1, 672);
    fighter->up_aerial.startup_ticks = UINT16_C(5);
    fighter->up_aerial.active_ticks = UINT16_C(4);
    fighter->up_aerial.recovery_ticks = UINT16_C(18);
    fighter->up_aerial.hitlag_ticks = UINT16_C(5);
    fighter->down_aerial.hitbox_offset_x_q16 =
        PF_Q16_RATIO(1, 10);
    fighter->down_aerial.hitbox_offset_y_q16 =
        PF_Q16_RATIO(13, 20);
    fighter->down_aerial.hitbox_half_width_q16 =
        PF_Q16_RATIO(3, 5);
    fighter->down_aerial.hitbox_half_height_q16 =
        PF_Q16_RATIO(11, 20);
    fighter->down_aerial.damage_q16 =
        UINT32_C(10) * UINT32_C(65536);
    fighter->down_aerial.base_knockback_x_q16 =
        PF_Q16_RATIO(7, 50);
    fighter->down_aerial.base_knockback_y_q16 =
        PF_Q16_RATIO(17, 50);
    fighter->down_aerial.knockback_growth_q16 =
        PF_Q16_RATIO(1, 640);
    fighter->down_aerial.startup_ticks = UINT16_C(7);
    fighter->down_aerial.active_ticks = UINT16_C(4);
    fighter->down_aerial.recovery_ticks = UINT16_C(21);
    fighter->down_aerial.hitlag_ticks = UINT16_C(5);
    fighter->ledge_attack.hitbox_offset_x_q16 =
        PF_Q16_RATIO(3, 4);
    fighter->ledge_attack.hitbox_offset_y_q16 =
        -PF_Q16_RATIO(1, 20);
    fighter->ledge_attack.hitbox_half_width_q16 =
        PF_Q16_RATIO(13, 20);
    fighter->ledge_attack.hitbox_half_height_q16 =
        PF_Q16_RATIO(9, 20);
    fighter->ledge_attack.damage_q16 =
        UINT32_C(10) * UINT32_C(65536);
    fighter->ledge_attack.base_knockback_x_q16 =
        PF_Q16_RATIO(8, 25);
    fighter->ledge_attack.base_knockback_y_q16 =
        PF_Q16_RATIO(1, 4);
    fighter->ledge_attack.knockback_growth_q16 =
        PF_Q16_RATIO(1, 640);
    fighter->ledge_attack.startup_ticks = UINT16_C(6);
    fighter->ledge_attack.active_ticks = UINT16_C(3);
    fighter->ledge_attack.recovery_ticks = UINT16_C(20);
    fighter->ledge_attack.hitlag_ticks = UINT16_C(5);
    fighter->reset_max_damage_q16 =
        UINT32_C(7) * UINT32_C(65536);
    fighter->reset_bound_speed_q16 = PF_Q16_RATIO(1, 10);
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
    fighter->v_cancel_velocity_scale_q16 = PF_Q16_RATIO(95, 100);
    fighter->crouch_cancel_max_damage_q16 =
        UINT32_C(40) * UINT32_C(65536);
    fighter->crouch_cancel_velocity_scale_q16 = PF_Q16_RATIO(2, 3);
    fighter->crouch_cancel_hitstun_scale_q16 = PF_Q16_RATIO(2, 3);
    fighter->di_max_tangent_q16 = INT32_C(21294);
    fighter->sdi_distance_q16 = PF_Q16_RATIO(3, 10);
    fighter->asdi_distance_q16 = PF_Q16_RATIO(3, 20);
    fighter->shield_sdi_scale_q16 = PF_Q16_RATIO(33, 50);
    fighter->tech_roll_speed_q16 = PF_Q16_RATIO(1, 5);
    fighter->wall_tech_speed_q16 = PF_Q16_RATIO(3, 20);
    fighter->wall_tech_jump_speed_x_q16 = PF_Q16_RATIO(3, 10);
    fighter->wall_tech_jump_speed_y_q16 = PF_Q16_RATIO(1, 2);
    fighter->wall_jump_speed_x_q16 = PF_Q16_RATIO(3, 10);
    fighter->wall_jump_speed_y_q16 = PF_Q16_RATIO(1, 2);
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
    fighter->light_shield_hold_depletion_q16 =
        (uint32_t)PF_Q16_RATIO(7, 100);
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
    fighter->light_shield_defender_pushback_scale_q16 =
        PF_Q16_RATIO(5, 4);
    fighter->shield_attacker_pushback_damage_q16 =
        PF_Q16_RATIO(7, 100);
    fighter->shield_attacker_pushback_base_q16 =
        PF_Q16_RATIO(1, 50);
    fighter->shield_half_width_q16 = PF_Q16_RATIO(4, 5);
    fighter->shield_half_height_q16 = PF_Q16_RATIO(7, 5);
    fighter->shield_minimum_size_scale_q16 =
        PF_Q16_RATIO(3, 20);
    fighter->dense_shield_size_scale_q16 =
        PF_Q16_RATIO(1, 2);
    fighter->shield_tilt_max_x_q16 = PF_Q16_RATIO(3, 10);
    fighter->shield_tilt_max_y_q16 = PF_Q16_RATIO(3, 10);
    fighter->grabbox_offset_x_q16 = PF_Q16_RATIO(3, 4);
    fighter->grabbox_offset_y_q16 = INT32_C(0);
    fighter->grabbox_half_width_q16 = PF_Q16_RATIO(1, 2);
    fighter->grabbox_half_height_q16 = PF_Q16_RATIO(7, 10);
    fighter->grabbed_offset_x_q16 = PF_Q16_RATIO(3, 5);
    fighter->grabbed_offset_y_q16 = INT32_C(0);
    fighter->grab_escape_damage_ticks_q16 = PF_Q16_RATIO(1, 10);
    fighter->forward_throw.damage_q16 =
        UINT32_C(8) * UINT32_C(65536);
    fighter->forward_throw.base_velocity_x_q16 =
        PF_Q16_RATIO(1, 4);
    fighter->forward_throw.base_velocity_y_q16 =
        -PF_Q16_RATIO(9, 50);
    fighter->forward_throw.velocity_growth_x_q16 =
        PF_Q16_RATIO(1, 512);
    fighter->forward_throw.velocity_growth_y_q16 =
        -PF_Q16_RATIO(1, 1024);
    fighter->forward_throw.release_tick = UINT16_C(3);
    fighter->forward_throw.recovery_ticks = UINT16_C(12);
    fighter->forward_throw.hitlag_ticks = UINT16_C(4);
    fighter->back_throw.damage_q16 =
        UINT32_C(9) * UINT32_C(65536);
    fighter->back_throw.base_velocity_x_q16 =
        -PF_Q16_RATIO(3, 10);
    fighter->back_throw.base_velocity_y_q16 =
        -PF_Q16_RATIO(4, 25);
    fighter->back_throw.velocity_growth_x_q16 =
        -PF_Q16_RATIO(1, 512);
    fighter->back_throw.velocity_growth_y_q16 =
        -PF_Q16_RATIO(1, 1024);
    fighter->back_throw.release_tick = UINT16_C(4);
    fighter->back_throw.recovery_ticks = UINT16_C(14);
    fighter->back_throw.hitlag_ticks = UINT16_C(4);
    fighter->up_throw.damage_q16 =
        UINT32_C(7) * UINT32_C(65536);
    fighter->up_throw.base_velocity_x_q16 =
        PF_Q16_RATIO(3, 100);
    fighter->up_throw.base_velocity_y_q16 =
        -PF_Q16_RATIO(9, 25);
    fighter->up_throw.velocity_growth_x_q16 =
        PF_Q16_RATIO(1, 4096);
    fighter->up_throw.velocity_growth_y_q16 =
        -PF_Q16_RATIO(1, 512);
    fighter->up_throw.release_tick = UINT16_C(3);
    fighter->up_throw.recovery_ticks = UINT16_C(11);
    fighter->up_throw.hitlag_ticks = UINT16_C(4);
    fighter->down_throw.damage_q16 =
        UINT32_C(6) * UINT32_C(65536);
    fighter->down_throw.base_velocity_x_q16 =
        PF_Q16_RATIO(1, 25);
    fighter->down_throw.base_velocity_y_q16 =
        -PF_Q16_RATIO(2, 25);
    fighter->down_throw.velocity_growth_x_q16 =
        PF_Q16_RATIO(1, 512);
    fighter->down_throw.velocity_growth_y_q16 =
        -PF_Q16_RATIO(1, 2048);
    fighter->down_throw.release_tick = UINT16_C(2);
    fighter->down_throw.recovery_ticks = UINT16_C(5);
    fighter->down_throw.hitlag_ticks = UINT16_C(3);
    fighter->jump_squat_ticks = UINT16_C(3);
    fighter->double_jump_cancel_ticks = UINT16_C(6);
    fighter->double_jump_armor_max_hitstun_ticks = UINT16_C(20);
    fighter->initial_dash_ticks = UINT16_C(10);
    fighter->moonwalk_setup_ticks = UINT16_C(2);
    fighter->teeter_ticks = UINT16_C(30);
    fighter->crouch_step_ticks = UINT16_C(1);
    fighter->taunt_ticks = UINT16_C(90);
    fighter->forward_smash_input_window_ticks = UINT16_C(3);
    fighter->landing_ticks = UINT16_C(4);
    fighter->platform_drop_ticks = UINT16_C(9);
    fighter->air_dodge_ticks = UINT16_C(49);
    fighter->air_dodge_invulnerability_begin_tick = UINT16_C(3);
    fighter->air_dodge_invulnerability_end_tick = UINT16_C(29);
    fighter->ledge_invulnerability_ticks = UINT16_C(37);
    fighter->ledge_regrab_lockout_ticks = UINT16_C(29);
    fighter->ledge_roll_ticks = UINT16_C(30);
    fighter->ledge_roll_movement_ticks = UINT16_C(20);
    fighter->ledge_roll_invulnerability_ticks = UINT16_C(22);
    fighter->ledge_attack_invulnerability_ticks = UINT16_C(10);
    fighter->special_landing_ticks = UINT16_C(10);
    fighter->run_turnaround_ticks = UINT16_C(12);
    fighter->run_brake_ticks = UINT16_C(8);
    fighter->axis_dead_zone = UINT16_C(4096);
    fighter->dash_axis_threshold = UINT16_C(24575);
    fighter->run_turnaround_axis_threshold = UINT16_C(12288);
    fighter->run_continue_axis_threshold = UINT16_C(20480);
    fighter->run_turnaround_lockout_ticks = UINT16_C(10);
    fighter->crouch_axis_threshold = UINT16_C(16384);
    fighter->shield_drop_axis_threshold = UINT16_C(12288);
    fighter->dash_attack_startup_ticks = UINT16_C(4);
    fighter->dash_attack_active_ticks = UINT16_C(3);
    fighter->dash_attack_recovery_ticks = UINT16_C(12);
    fighter->dash_attack_hitlag_ticks = UINT16_C(5);
    fighter->boost_grab_cancel_begin_tick = UINT16_C(1);
    fighter->boost_grab_cancel_end_tick = UINT16_C(3);
    fighter->jab_startup_ticks = UINT16_C(2);
    fighter->jab_active_ticks = UINT16_C(2);
    fighter->jab_recovery_ticks = UINT16_C(8);
    fighter->jab_hitlag_ticks = UINT16_C(4);
    fighter->jab_combo_input_begin_tick = UINT16_C(4);
    fighter->jab_combo_input_end_tick = UINT16_C(7);
    fighter->jab_final_startup_ticks = UINT16_C(2);
    fighter->jab_final_active_ticks = UINT16_C(2);
    fighter->jab_final_recovery_ticks = UINT16_C(10);
    fighter->jab_final_hitlag_ticks = UINT16_C(4);
    fighter->reset_max_hitstun_ticks = UINT16_C(12);
    fighter->reset_bound_ticks = UINT16_C(12);
    fighter->reset_forced_getup_ticks = UINT16_C(30);
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
    fighter->v_cancel_window_ticks = UINT16_C(3);
    fighter->sdi_axis_threshold = UINT16_C(16384);
    fighter->light_shield_trigger_threshold = UINT16_C(8192);
    fighter->digital_trigger_threshold = UINT16_C(32768);
    fighter->tumble_hitstun_threshold_ticks = UINT16_C(32);
    fighter->tech_window_ticks = UINT16_C(20);
    fighter->tech_lockout_ticks = UINT16_C(40);
    fighter->tech_in_place_ticks = UINT16_C(26);
    fighter->tech_roll_ticks = UINT16_C(40);
    fighter->tech_invulnerability_ticks = UINT16_C(20);
    fighter->wall_tech_stall_ticks = UINT16_C(3);
    fighter->wall_tech_ticks = UINT16_C(24);
    fighter->wall_jump_ticks = UINT16_C(24);
    fighter->wall_jump_invulnerability_ticks = UINT16_C(4);
    fighter->ceiling_tech_ticks = UINT16_C(30);
    fighter->knockdown_ticks = UINT16_C(26);
    fighter->down_wait_ticks = UINT16_C(180);
    fighter->getup_neutral_ticks = UINT16_C(30);
    fighter->getup_neutral_invulnerability_ticks = UINT16_C(23);
    fighter->getup_roll_ticks = UINT16_C(35);
    fighter->getup_roll_back_forward.movement_begin_tick = UINT16_C(6);
    fighter->getup_roll_back_forward.invulnerability_begin_tick =
        UINT16_C(1);
    fighter->getup_roll_back_forward.invulnerability_end_tick =
        UINT16_C(19);
    fighter->getup_roll_back_backward.movement_begin_tick = UINT16_C(12);
    fighter->getup_roll_back_backward.invulnerability_begin_tick =
        UINT16_C(12);
    fighter->getup_roll_back_backward.invulnerability_end_tick =
        UINT16_C(29);
    fighter->getup_roll_stomach_forward.movement_begin_tick = UINT16_C(8);
    fighter->getup_roll_stomach_forward.invulnerability_begin_tick =
        UINT16_C(1);
    fighter->getup_roll_stomach_forward.invulnerability_end_tick =
        UINT16_C(19);
    fighter->getup_roll_stomach_backward.movement_begin_tick = UINT16_C(5);
    fighter->getup_roll_stomach_backward.invulnerability_begin_tick =
        UINT16_C(1);
    fighter->getup_roll_stomach_backward.invulnerability_end_tick =
        UINT16_C(24);
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
    fighter->shield_break_stun_ticks = UINT16_C(490);
    fighter->shield_break_minimum_stun_ticks = UINT16_C(90);
    fighter->shield_break_down_ticks = UINT16_C(30);
    fighter->shield_break_stand_ticks = UINT16_C(30);
    fighter->shield_break_mash_reduction_ticks = UINT16_C(3);
    fighter->grab_startup_ticks = UINT16_C(4);
    fighter->grab_active_ticks = UINT16_C(2);
    fighter->grab_recovery_ticks = UINT16_C(10);
    fighter->grab_escape_base_ticks = UINT16_C(30);
    fighter->grab_escape_max_ticks = UINT16_C(90);
    fighter->grab_mash_reduction_ticks = UINT16_C(3);
    fighter->grab_release_ticks = UINT16_C(8);
    fighter->pummel_damage_q16 = UINT32_C(3) * UINT32_C(65536);
    fighter->pummel_hit_tick = UINT16_C(2);
    fighter->pummel_total_ticks = UINT16_C(10);
    fighter->air_jump_count = UINT8_C(1);
    fighter->powershield_cancel_enabled = UINT8_C(1);
    fighter->wall_jump_enabled = UINT8_C(1);
    fighter->stale_move_slot_reduction_q16[0] = UINT16_C(4608);
    fighter->stale_move_slot_reduction_q16[1] = UINT16_C(4096);
    fighter->stale_move_slot_reduction_q16[2] = UINT16_C(3584);
    fighter->stale_move_slot_reduction_q16[3] = UINT16_C(3072);
    fighter->stale_move_slot_reduction_q16[4] = UINT16_C(2560);
    fighter->stale_move_slot_reduction_q16[5] = UINT16_C(2048);
    fighter->stale_move_slot_reduction_q16[6] = UINT16_C(1536);
    fighter->stale_move_slot_reduction_q16[7] = UINT16_C(1024);
    fighter->stale_move_slot_reduction_q16[8] = UINT16_C(512);

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
    stage->revival_platform_start_y_q16 =
        INT32_C(4) * PF_Q16_ONE;
    stage->revival_platform_end_y_q16 =
        INT32_C(12) * PF_Q16_ONE;
    stage->revival_platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    stage->revival_platform_descent_ticks = UINT16_C(30);
    stage->revival_platform_hold_ticks = UINT16_C(90);
    stage->upper_platform_center_x_q16 =
        INT32_C(20) * PF_Q16_ONE;
    stage->upper_platform_y_q16 = INT32_C(13) * PF_Q16_ONE;
    stage->upper_platform_half_width_q16 =
        INT32_C(4) * PF_Q16_ONE;

    item = &out_content->item;
    item->struct_size = (uint32_t)sizeof(*item);
    item->schema_version = PF_M4_ITEM_SCHEMA_VERSION;
    item->enabled = UINT8_C(0);
    item->half_width_q16 = PF_Q16_RATIO(1, 8);
    item->half_height_q16 = PF_Q16_RATIO(1, 2);
    item->spawn_x_q16 = -INT32_C(7) * PF_Q16_ONE;
    item->spawn_y_q16 =
        stage->floor_y_q16 - item->half_height_q16;
    item->pickup_half_width_q16 = PF_Q16_RATIO(3, 2);
    item->pickup_half_height_q16 = PF_Q16_RATIO(5, 4);
    item->held_offset_x_q16 = PF_Q16_RATIO(3, 4);
    item->held_offset_y_q16 = -PF_Q16_RATIO(3, 4);
    item->gravity_q16 = PF_Q16_RATIO(1, 40);
    item->fall_speed_q16 = PF_Q16_RATIO(1, 2);
    item->drop_velocity_y_q16 = INT32_C(0);
    item->forward_throw.velocity_x_q16 = PF_Q16_RATIO(1, 2);
    item->forward_throw.velocity_y_q16 = -PF_Q16_RATIO(1, 10);
    item->back_throw.velocity_x_q16 = -PF_Q16_RATIO(9, 20);
    item->back_throw.velocity_y_q16 = -PF_Q16_RATIO(3, 25);
    item->up_throw.velocity_x_q16 = PF_Q16_RATIO(1, 20);
    item->up_throw.velocity_y_q16 = -PF_Q16_RATIO(11, 20);
    item->down_throw.velocity_x_q16 = PF_Q16_RATIO(1, 20);
    item->down_throw.velocity_y_q16 = PF_Q16_RATIO(3, 5);
    item->momentum_transfer_q16 = PF_Q16_RATIO(3, 4);
    item->hitbox_half_width_q16 = PF_Q16_RATIO(7, 20);
    item->hitbox_half_height_q16 = PF_Q16_RATIO(11, 20);
    item->damage_q16 = UINT32_C(7) * UINT32_C(65536);
    item->base_knockback_x_q16 = PF_Q16_RATIO(9, 50);
    item->base_knockback_y_q16 = PF_Q16_RATIO(11, 50);
    item->knockback_growth_q16 = PF_Q16_RATIO(1, 768);
    item->hit_bounce_velocity_y_q16 = -PF_Q16_RATIO(7, 20);
    item->dash_throw_speed_q16 = PF_Q16_RATIO(1, 20);
    item->throw_recovery_ticks = UINT16_C(12);
    item->dash_throw_recovery_ticks = UINT16_C(20);
    item->glide_toss_begin_tick = UINT16_C(0);
    item->glide_toss_end_tick = UINT16_C(4);
    item->pickup_lockout_ticks = UINT16_C(8);
    item->lifetime_ticks = UINT16_C(600);
    item->respawn_ticks = UINT16_C(60);
    item->hitlag_ticks = UINT16_C(4);

    projectile = &out_content->projectile;
    projectile->struct_size = (uint32_t)sizeof(*projectile);
    projectile->schema_version = PF_M4_PROJECTILE_SCHEMA_VERSION;
    projectile->enabled = UINT8_C(0);
    projectile->half_width_q16 = PF_Q16_RATIO(1, 5);
    projectile->half_height_q16 = PF_Q16_RATIO(1, 5);
    projectile->spawn_offset_x_q16 = PF_Q16_RATIO(4, 5);
    projectile->spawn_offset_y_q16 = INT32_C(0);
    projectile->speed_q16 = PF_Q16_RATIO(3, 5);
    projectile->damage_q16 = UINT32_C(6) * UINT32_C(65536);
    projectile->base_knockback_x_q16 = PF_Q16_RATIO(1, 5);
    projectile->base_knockback_y_q16 = PF_Q16_RATIO(1, 10);
    projectile->knockback_growth_q16 = PF_Q16_RATIO(1, 1024);
    projectile->lifetime_ticks = UINT16_C(120);
    projectile->fire_recovery_ticks = UINT16_C(8);
    projectile->hitlag_ticks = UINT16_C(3);
    projectile->powershield_reflect_window_ticks = UINT16_C(2);

    reflector = &out_content->reflector;
    reflector->struct_size = (uint32_t)sizeof(*reflector);
    reflector->schema_version = PF_M4_REFLECTOR_SCHEMA_VERSION;
    reflector->enabled = UINT8_C(0);
    reflector->hitbox_offset_x_q16 = INT32_C(0);
    reflector->hitbox_offset_y_q16 = INT32_C(0);
    reflector->hitbox_half_width_q16 = PF_Q16_RATIO(7, 5);
    reflector->hitbox_half_height_q16 = PF_Q16_RATIO(3, 2);
    reflector->damage_q16 = UINT32_C(3) * UINT32_C(65536);
    reflector->base_knockback_x_q16 = PF_Q16_RATIO(4, 5);
    reflector->base_knockback_y_q16 = PF_Q16_RATIO(7, 20);
    reflector->knockback_growth_q16 = PF_Q16_RATIO(1, 2048);
    reflector->startup_ticks = UINT16_C(1);
    reflector->active_ticks = UINT16_C(2);
    reflector->recovery_ticks = UINT16_C(9);
    reflector->hitlag_ticks = UINT16_C(3);

    charge = &out_content->charge;
    charge->struct_size = (uint32_t)sizeof(*charge);
    charge->schema_version = PF_M4_CHARGE_SCHEMA_VERSION;
    charge->enabled = UINT8_C(0);
    charge->hitbox_offset_x_q16 = PF_Q16_RATIO(7, 10);
    charge->hitbox_offset_y_q16 = INT32_C(0);
    charge->hitbox_half_width_q16 = PF_Q16_RATIO(4, 5);
    charge->hitbox_half_height_q16 = PF_Q16_RATIO(3, 4);
    charge->base_damage_q16 = UINT32_C(4) * UINT32_C(65536);
    charge->bonus_damage_q16 = UINT32_C(16) * UINT32_C(65536);
    charge->base_knockback_x_q16 = PF_Q16_RATIO(1, 5);
    charge->base_knockback_y_q16 = PF_Q16_RATIO(3, 20);
    charge->knockback_growth_q16 = PF_Q16_RATIO(1, 768);
    charge->max_charge_ticks = UINT16_C(120);
    charge->store_animation_ticks = UINT16_C(4);
    charge->release_startup_ticks = UINT16_C(4);
    charge->release_active_ticks = UINT16_C(3);
    charge->release_recovery_ticks = UINT16_C(14);
    charge->release_hitlag_ticks = UINT16_C(5);

    recovery = &out_content->recovery;
    recovery->struct_size = (uint32_t)sizeof(*recovery);
    recovery->schema_version = PF_M4_RECOVERY_SCHEMA_VERSION;
    recovery->enabled = UINT8_C(0);
    recovery->horizontal_speed_q16 = PF_Q16_RATIO(1, 4);
    recovery->vertical_speed_q16 = PF_Q16_RATIO(4, 5);
    recovery->ascent_ticks = UINT16_C(18);

    return PF_STATUS_OK;
}

pf_status pf_m4_validate_content(const pf_m4_content *content)
{
    const pf_m4_fighter_data *fighter;
    const pf_m4_stage_data *stage;
    const pf_m4_item_data *item;
    const pf_m4_projectile_data *projectile;
    const pf_m4_reflector_data *reflector;
    const pf_m4_charge_data *charge;
    const pf_m4_recovery_data *recovery;
    const int32_t maximum_coordinate_q16 =
        INT32_C(4096) * PF_Q16_ONE;
    const int32_t maximum_fighter_extent_q16 =
        INT32_C(64) * PF_Q16_ONE;
    int64_t platform_left_extent;
    int64_t platform_right_extent;
    int64_t spawn_left_extent;
    int64_t spawn_right_extent;
    int64_t revival_left_extent;
    int64_t revival_right_extent;
    int64_t upper_platform_left_extent;
    int64_t upper_platform_right_extent;
    int64_t maximum_dash_attack_knockback_x;
    int64_t maximum_dash_attack_knockback_y;
    int64_t maximum_jab_knockback_x;
    int64_t maximum_jab_knockback_y;
    int64_t maximum_jab_final_knockback_x;
    int64_t maximum_jab_final_knockback_y;
    int64_t maximum_strong_knockback_x;
    int64_t maximum_strong_knockback_y;
    int64_t maximum_aerial_knockback_x;
    int64_t maximum_aerial_knockback_y;
    int64_t maximum_getup_attack_knockback_x;
    int64_t maximum_getup_attack_knockback_y;
    int64_t maximum_item_knockback_x;
    int64_t maximum_item_knockback_y;
    int64_t maximum_projectile_knockback_x;
    int64_t maximum_projectile_knockback_y;
    int64_t maximum_reflector_knockback_x;
    int64_t maximum_reflector_knockback_y;
    int64_t maximum_charge_knockback_x;
    int64_t maximum_charge_knockback_y;
    uint32_t stale_index;
    uint32_t stale_reduction_total_q16 = UINT32_C(0);
    int solid_overlaps_platform;
    int upper_overlaps_platform;
    int upper_overlaps_revival;
    int upper_overlaps_solid;

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
        content->stage.schema_version != PF_M4_STAGE_SCHEMA_VERSION ||
        content->item.struct_size !=
            (uint32_t)sizeof(content->item) ||
        content->item.schema_version != PF_M4_ITEM_SCHEMA_VERSION ||
        content->projectile.struct_size !=
            (uint32_t)sizeof(content->projectile) ||
        content->projectile.schema_version !=
            PF_M4_PROJECTILE_SCHEMA_VERSION ||
        content->reflector.struct_size !=
            (uint32_t)sizeof(content->reflector) ||
        content->reflector.schema_version !=
            PF_M4_REFLECTOR_SCHEMA_VERSION ||
        content->charge.struct_size !=
            (uint32_t)sizeof(content->charge) ||
        content->charge.schema_version !=
            PF_M4_CHARGE_SCHEMA_VERSION ||
        content->recovery.struct_size !=
            (uint32_t)sizeof(content->recovery) ||
        content->recovery.schema_version !=
            PF_M4_RECOVERY_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (content->fighter_count != PF_M4_PLACEHOLDER_FIGHTER_COUNT ||
        content->stage_count != PF_M4_TEST_STAGE_COUNT ||
        content->item_count != PF_M4_TEST_ITEM_COUNT ||
        content->projectile_count != PF_M4_TEST_PROJECTILE_COUNT ||
        content->reflector_count != PF_M4_TEST_REFLECTOR_COUNT ||
        content->charge_count != PF_M4_TEST_CHARGE_COUNT ||
        content->recovery_count != PF_M4_TEST_RECOVERY_COUNT ||
        content->fighter.reserved != UINT16_C(0) ||
        content->fighter.smash_charge_reserved != UINT16_C(0) ||
        content->fighter.reserved2 != UINT8_C(0) ||
        content->stage.reserved != UINT16_C(0) ||
        content->stage.reserved2 != UINT16_C(0) ||
        content->item.reserved != UINT8_C(0) ||
        content->item.reserved2 != UINT16_C(0) ||
        content->projectile.reserved != UINT8_C(0) ||
        content->reflector.reserved != UINT8_C(0) ||
        content->charge.reserved != UINT8_C(0) ||
        content->recovery.reserved != UINT8_C(0) ||
        content->recovery.reserved2 != UINT16_C(0))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    fighter = &content->fighter;
    for (stale_index = UINT32_C(0);
         stale_index <
             (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++stale_index)
    {
        const uint16_t reduction =
            fighter->stale_move_slot_reduction_q16[stale_index];

        if (reduction == UINT16_C(0) ||
            (stale_index != UINT32_C(0) &&
             reduction >= fighter->stale_move_slot_reduction_q16[
                              stale_index - UINT32_C(1)]))
        {
            return PF_STATUS_INVALID_CONFIG;
        }
        stale_reduction_total_q16 += (uint32_t)reduction;
    }
    if (stale_reduction_total_q16 >
        (uint32_t)PF_Q16_ONE / UINT32_C(2))
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    if (!pf_m4_throw_data_is_valid(&fighter->forward_throw) ||
        !pf_m4_throw_data_is_valid(&fighter->back_throw) ||
        !pf_m4_throw_data_is_valid(&fighter->up_throw) ||
        !pf_m4_throw_data_is_valid(&fighter->down_throw) ||
        !pf_m4_attack_data_is_valid(
            &fighter->up_attack,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->down_attack,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->forward_attack,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->forward_strong_attack,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->up_strong_attack,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->down_strong_attack,
            maximum_fighter_extent_q16) ||
        fighter->smash_charge_damage_bonus_q16 == UINT32_C(0) ||
        fighter->smash_charge_damage_bonus_q16 >
            (uint32_t)PF_Q16_ONE ||
        fighter->smash_charge_max_ticks == UINT16_C(0) ||
        fighter->smash_charge_max_ticks > UINT16_C(600) ||
        !pf_m4_charged_attack_damage_is_valid(
            &fighter->forward_strong_attack,
            fighter->smash_charge_damage_bonus_q16) ||
        !pf_m4_charged_attack_damage_is_valid(
            &fighter->up_strong_attack,
            fighter->smash_charge_damage_bonus_q16) ||
        !pf_m4_charged_attack_damage_is_valid(
            &fighter->down_strong_attack,
            fighter->smash_charge_damage_bonus_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->forward_aerial,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->back_aerial,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->up_aerial,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->down_aerial,
            maximum_fighter_extent_q16) ||
        !pf_m4_attack_data_is_valid(
            &fighter->ledge_attack,
            maximum_fighter_extent_q16))
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    maximum_dash_attack_knockback_x =
        (int64_t)fighter->dash_attack_base_knockback_x_q16 +
        (((int64_t)fighter->dash_attack_knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_dash_attack_knockback_y =
        (int64_t)fighter->dash_attack_base_knockback_y_q16 +
        ((((int64_t)fighter->dash_attack_knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
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
    maximum_jab_final_knockback_x =
        (int64_t)fighter->jab_final_base_knockback_x_q16 +
        (((int64_t)fighter->jab_final_knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_jab_final_knockback_y =
        (int64_t)fighter->jab_final_base_knockback_y_q16 +
        ((((int64_t)fighter->jab_final_knockback_growth_q16 *
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
        fighter->weight_q16 < PF_Q16_ONE / INT32_C(2) ||
        fighter->weight_q16 > INT32_C(2) * PF_Q16_ONE ||
        fighter->ground_acceleration_q16 <= INT32_C(0) ||
        fighter->turn_acceleration_q16 <
            fighter->ground_acceleration_q16 ||
        fighter->traction_q16 <= INT32_C(0) ||
        fighter->walk_speed_q16 <= INT32_C(0) ||
        fighter->run_speed_q16 <= fighter->walk_speed_q16 ||
        fighter->initial_dash_speed_q16 < fighter->run_speed_q16 ||
        fighter->teeter_snap_distance_q16 <= INT32_C(0) ||
        fighter->crouch_step_speed_q16 <= INT32_C(0) ||
        fighter->crouch_step_speed_q16 > fighter->walk_speed_q16 ||
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
        fighter->ledge_roll_distance_q16 <=
            fighter->half_width_q16 +
                fighter->platform_drop_nudge_q16 ||
        fighter->ledge_roll_distance_q16 >
            INT32_C(8) * PF_Q16_ONE ||
        fighter->drop_cancel_snap_distance_q16 <=
            fighter->platform_drop_nudge_q16 ||
        fighter->drop_cancel_snap_distance_q16 >
            fighter->half_height_q16 ||
        fighter->air_dodge_speed_q16 <= INT32_C(0) ||
        fighter->air_dodge_decay_q16 <= INT32_C(0) ||
        fighter->air_dodge_decay_q16 > PF_Q16_ONE ||
        fighter->fall_special_mobility_q16 <= INT32_C(0) ||
        fighter->fall_special_mobility_q16 >
            fighter->air_speed_q16 ||
        fighter->dash_attack_speed_q16 <=
            fighter->initial_dash_speed_q16 ||
        fighter->dash_attack_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->dash_attack_hitbox_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->dash_attack_hitbox_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->dash_attack_hitbox_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->dash_attack_hitbox_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->dash_attack_hitbox_half_width_q16 <= INT32_C(0) ||
        fighter->dash_attack_hitbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->dash_attack_hitbox_half_height_q16 <= INT32_C(0) ||
        fighter->dash_attack_hitbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->dash_attack_damage_q16 == UINT32_C(0) ||
        fighter->dash_attack_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->dash_attack_base_knockback_x_q16 <= INT32_C(0) ||
        fighter->dash_attack_base_knockback_y_q16 <= INT32_C(0) ||
        fighter->dash_attack_knockback_growth_q16 <= INT32_C(0) ||
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
        fighter->jab_final_hitbox_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->jab_final_hitbox_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->jab_final_hitbox_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->jab_final_hitbox_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->jab_final_hitbox_half_width_q16 <= INT32_C(0) ||
        fighter->jab_final_hitbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->jab_final_hitbox_half_height_q16 <= INT32_C(0) ||
        fighter->jab_final_hitbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->jab_final_damage_q16 == UINT32_C(0) ||
        fighter->jab_final_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->jab_final_base_knockback_x_q16 <= INT32_C(0) ||
        fighter->jab_final_base_knockback_y_q16 <= INT32_C(0) ||
        fighter->jab_final_knockback_growth_q16 <= INT32_C(0) ||
        fighter->reset_max_damage_q16 == UINT32_C(0) ||
        fighter->reset_max_damage_q16 >
            UINT32_C(7) * UINT32_C(65536) ||
        fighter->reset_bound_speed_q16 <= INT32_C(0) ||
        fighter->reset_bound_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
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
        fighter->v_cancel_velocity_scale_q16 <= INT32_C(0) ||
        fighter->v_cancel_velocity_scale_q16 >= PF_Q16_ONE ||
        fighter->crouch_cancel_max_damage_q16 == UINT32_C(0) ||
        fighter->crouch_cancel_max_damage_q16 >
            UINT32_C(300) * UINT32_C(65536) ||
        fighter->crouch_cancel_velocity_scale_q16 <= INT32_C(0) ||
        fighter->crouch_cancel_velocity_scale_q16 >= PF_Q16_ONE ||
        fighter->crouch_cancel_hitstun_scale_q16 <= INT32_C(0) ||
        fighter->crouch_cancel_hitstun_scale_q16 >= PF_Q16_ONE ||
        fighter->di_max_tangent_q16 <= INT32_C(0) ||
        fighter->di_max_tangent_q16 > PF_Q16_ONE ||
        fighter->sdi_distance_q16 <= INT32_C(0) ||
        fighter->sdi_distance_q16 >
            INT32_C(4) * PF_Q16_ONE ||
        fighter->asdi_distance_q16 <= INT32_C(0) ||
        fighter->asdi_distance_q16 >
            fighter->sdi_distance_q16 ||
        fighter->shield_sdi_scale_q16 <= INT32_C(0) ||
        fighter->shield_sdi_scale_q16 > PF_Q16_ONE ||
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
        fighter->wall_jump_speed_x_q16 <= INT32_C(0) ||
        fighter->wall_jump_speed_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->wall_jump_speed_y_q16 <= fighter->gravity_q16 ||
        fighter->wall_jump_speed_y_q16 >
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
        fighter->light_shield_hold_depletion_q16 ==
            UINT32_C(0) ||
        fighter->light_shield_hold_depletion_q16 >
            fighter->shield_hold_depletion_q16 ||
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
        fighter->light_shield_defender_pushback_scale_q16 <
            PF_Q16_ONE ||
        fighter->light_shield_defender_pushback_scale_q16 >
            INT32_C(2) * PF_Q16_ONE ||
        fighter->shield_attacker_pushback_damage_q16 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_damage_q16 >
            PF_Q16_ONE ||
        fighter->shield_attacker_pushback_base_q16 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_base_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->shield_half_width_q16 <= INT32_C(0) ||
        fighter->shield_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->shield_half_height_q16 <= INT32_C(0) ||
        fighter->shield_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->shield_minimum_size_scale_q16 <= INT32_C(0) ||
        fighter->shield_minimum_size_scale_q16 >=
            fighter->dense_shield_size_scale_q16 ||
        fighter->dense_shield_size_scale_q16 > PF_Q16_ONE ||
        fighter->shield_tilt_max_x_q16 < INT32_C(0) ||
        fighter->shield_tilt_max_x_q16 >
            fighter->shield_half_width_q16 ||
        fighter->shield_tilt_max_y_q16 < INT32_C(0) ||
        fighter->shield_tilt_max_y_q16 >
            fighter->shield_half_height_q16 ||
        maximum_dash_attack_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_dash_attack_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_jab_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_jab_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_jab_final_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_jab_final_knockback_y >
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
        fighter->teeter_snap_distance_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->crouch_step_speed_q16 >
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
        fighter->double_jump_cancel_ticks > UINT16_C(120) ||
        fighter->double_jump_armor_max_hitstun_ticks >
            PF_SIM_MAX_HITSTUN_TICKS ||
        (fighter->double_jump_cancel_ticks == UINT16_C(0) &&
         fighter->double_jump_armor_max_hitstun_ticks !=
             UINT16_C(0)) ||
        fighter->initial_dash_ticks == UINT16_C(0) ||
        fighter->initial_dash_ticks > UINT16_C(120) ||
        fighter->moonwalk_setup_ticks < UINT16_C(2) ||
        fighter->moonwalk_setup_ticks >=
            fighter->initial_dash_ticks ||
        fighter->teeter_ticks == UINT16_C(0) ||
        fighter->teeter_ticks > UINT16_C(120) ||
        fighter->crouch_step_ticks == UINT16_C(0) ||
        fighter->crouch_step_ticks > UINT16_C(30) ||
        fighter->taunt_ticks == UINT16_C(0) ||
        fighter->taunt_ticks > UINT16_C(600) ||
        fighter->forward_smash_input_window_ticks == UINT16_C(0) ||
        fighter->forward_smash_input_window_ticks >
            fighter->initial_dash_ticks ||
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
        fighter->ledge_invulnerability_ticks == UINT16_C(0) ||
        fighter->ledge_invulnerability_ticks > UINT16_C(600) ||
        fighter->ledge_regrab_lockout_ticks == UINT16_C(0) ||
        fighter->ledge_regrab_lockout_ticks > UINT16_C(600) ||
        fighter->ledge_roll_ticks == UINT16_C(0) ||
        fighter->ledge_roll_ticks > UINT16_C(240) ||
        fighter->ledge_roll_movement_ticks == UINT16_C(0) ||
        fighter->ledge_roll_movement_ticks >
            fighter->ledge_roll_ticks ||
        fighter->ledge_roll_invulnerability_ticks == UINT16_C(0) ||
        fighter->ledge_roll_invulnerability_ticks >
            fighter->ledge_roll_ticks ||
        fighter->ledge_attack_invulnerability_ticks == UINT16_C(0) ||
        fighter->ledge_attack_invulnerability_ticks >
            (uint32_t)fighter->ledge_attack.startup_ticks +
                (uint32_t)fighter->ledge_attack.active_ticks +
                (uint32_t)fighter->ledge_attack.recovery_ticks ||
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
        fighter->shield_drop_axis_threshold <=
            fighter->axis_dead_zone ||
        fighter->shield_drop_axis_threshold >=
            fighter->crouch_axis_threshold ||
        fighter->dash_attack_startup_ticks == UINT16_C(0) ||
        fighter->dash_attack_startup_ticks > UINT16_C(120) ||
        fighter->dash_attack_active_ticks == UINT16_C(0) ||
        fighter->dash_attack_active_ticks > UINT16_C(120) ||
        fighter->dash_attack_recovery_ticks == UINT16_C(0) ||
        fighter->dash_attack_recovery_ticks > UINT16_C(240) ||
        fighter->dash_attack_hitlag_ticks == UINT16_C(0) ||
        fighter->dash_attack_hitlag_ticks > UINT16_C(120) ||
        (uint32_t)fighter->dash_attack_startup_ticks +
                (uint32_t)fighter->dash_attack_active_ticks +
                (uint32_t)fighter->dash_attack_recovery_ticks >
            UINT32_C(600) ||
        fighter->boost_grab_cancel_begin_tick == UINT16_C(0) ||
        fighter->boost_grab_cancel_begin_tick >
            fighter->boost_grab_cancel_end_tick ||
        fighter->boost_grab_cancel_end_tick >=
            fighter->dash_attack_startup_ticks ||
        fighter->jab_startup_ticks == UINT16_C(0) ||
        fighter->jab_startup_ticks > UINT16_C(120) ||
        fighter->jab_active_ticks == UINT16_C(0) ||
        fighter->jab_active_ticks > UINT16_C(120) ||
        fighter->jab_recovery_ticks == UINT16_C(0) ||
        fighter->jab_recovery_ticks > UINT16_C(240) ||
        fighter->jab_hitlag_ticks == UINT16_C(0) ||
        fighter->jab_hitlag_ticks > UINT16_C(120) ||
        fighter->jab_combo_input_begin_tick <
            fighter->jab_startup_ticks + fighter->jab_active_ticks ||
        fighter->jab_combo_input_begin_tick >
            fighter->jab_combo_input_end_tick ||
        fighter->jab_combo_input_end_tick >=
            fighter->jab_startup_ticks + fighter->jab_active_ticks +
                fighter->jab_recovery_ticks ||
        fighter->jab_final_startup_ticks == UINT16_C(0) ||
        fighter->jab_final_startup_ticks > UINT16_C(120) ||
        fighter->jab_final_active_ticks == UINT16_C(0) ||
        fighter->jab_final_active_ticks > UINT16_C(120) ||
        fighter->jab_final_recovery_ticks == UINT16_C(0) ||
        fighter->jab_final_recovery_ticks > UINT16_C(240) ||
        fighter->jab_final_hitlag_ticks == UINT16_C(0) ||
        fighter->jab_final_hitlag_ticks > UINT16_C(120) ||
        (uint32_t)fighter->jab_final_startup_ticks +
                (uint32_t)fighter->jab_final_active_ticks +
                (uint32_t)fighter->jab_final_recovery_ticks >
            UINT32_C(600) ||
        fighter->reset_max_hitstun_ticks == UINT16_C(0) ||
        fighter->reset_max_hitstun_ticks > UINT16_C(12) ||
        fighter->reset_max_hitstun_ticks >=
            fighter->tumble_hitstun_threshold_ticks ||
        fighter->reset_bound_ticks == UINT16_C(0) ||
        fighter->reset_bound_ticks > UINT16_C(120) ||
        fighter->reset_forced_getup_ticks == UINT16_C(0) ||
        fighter->reset_forced_getup_ticks > UINT16_C(240) ||
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
        fighter->platform_drop_ticks <=
            fighter->aerial_startup_ticks + UINT16_C(1) ||
        fighter->platform_drop_ticks >
            fighter->aerial_startup_ticks + UINT16_C(1) +
                fighter->aerial_hitlag_ticks ||
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
        fighter->v_cancel_window_ticks == UINT16_C(0) ||
        fighter->v_cancel_window_ticks >
            fighter->tech_lockout_ticks ||
        fighter->sdi_axis_threshold <= fighter->axis_dead_zone ||
        fighter->sdi_axis_threshold > UINT16_C(32767) ||
        fighter->light_shield_trigger_threshold == UINT16_C(0) ||
        fighter->light_shield_trigger_threshold >=
            fighter->digital_trigger_threshold ||
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
        fighter->wall_jump_ticks == UINT16_C(0) ||
        fighter->wall_jump_ticks > UINT16_C(240) ||
        fighter->wall_jump_invulnerability_ticks == UINT16_C(0) ||
        fighter->wall_jump_invulnerability_ticks >
            fighter->wall_jump_ticks ||
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
        !pf_m4_getup_roll_timing_is_valid(
            &fighter->getup_roll_back_forward,
            fighter->getup_roll_ticks) ||
        !pf_m4_getup_roll_timing_is_valid(
            &fighter->getup_roll_back_backward,
            fighter->getup_roll_ticks) ||
        !pf_m4_getup_roll_timing_is_valid(
            &fighter->getup_roll_stomach_forward,
            fighter->getup_roll_ticks) ||
        !pf_m4_getup_roll_timing_is_valid(
            &fighter->getup_roll_stomach_backward,
            fighter->getup_roll_ticks) ||
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
        fighter->shield_break_launch_speed_q16 <=
            fighter->gravity_q16 ||
        fighter->shield_break_launch_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        fighter->shield_break_stun_ticks == UINT16_C(0) ||
        fighter->shield_break_stun_ticks > UINT16_C(600) ||
        fighter->shield_break_minimum_stun_ticks == UINT16_C(0) ||
        fighter->shield_break_minimum_stun_ticks >
            fighter->shield_break_stun_ticks ||
        fighter->shield_break_down_ticks == UINT16_C(0) ||
        fighter->shield_break_down_ticks > UINT16_C(240) ||
        fighter->shield_break_stand_ticks == UINT16_C(0) ||
        fighter->shield_break_stand_ticks > UINT16_C(240) ||
        fighter->shield_break_mash_reduction_ticks ==
            UINT16_C(0) ||
        fighter->shield_break_mash_reduction_ticks >
            UINT16_C(60) ||
        fighter->grabbox_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->grabbox_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->grabbox_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->grabbox_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->grabbox_half_width_q16 <= INT32_C(0) ||
        fighter->grabbox_half_width_q16 >
            maximum_fighter_extent_q16 ||
        fighter->grabbox_half_height_q16 <= INT32_C(0) ||
        fighter->grabbox_half_height_q16 >
            maximum_fighter_extent_q16 ||
        fighter->grabbed_offset_x_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->grabbed_offset_x_q16 >
            maximum_fighter_extent_q16 ||
        fighter->grabbed_offset_y_q16 <
            -maximum_fighter_extent_q16 ||
        fighter->grabbed_offset_y_q16 >
            maximum_fighter_extent_q16 ||
        fighter->grab_escape_damage_ticks_q16 < INT32_C(0) ||
        fighter->grab_escape_damage_ticks_q16 > PF_Q16_ONE ||
        fighter->grab_startup_ticks == UINT16_C(0) ||
        fighter->grab_startup_ticks > UINT16_C(120) ||
        fighter->grab_active_ticks == UINT16_C(0) ||
        fighter->grab_active_ticks > UINT16_C(120) ||
        fighter->grab_recovery_ticks == UINT16_C(0) ||
        fighter->grab_recovery_ticks > UINT16_C(240) ||
        (uint32_t)fighter->grab_startup_ticks +
                (uint32_t)fighter->grab_active_ticks +
                (uint32_t)fighter->grab_recovery_ticks >
            UINT32_C(600) ||
        fighter->grab_escape_base_ticks == UINT16_C(0) ||
        fighter->grab_escape_max_ticks <
            fighter->grab_escape_base_ticks ||
        fighter->grab_escape_max_ticks > UINT16_C(600) ||
        fighter->grab_mash_reduction_ticks == UINT16_C(0) ||
        fighter->grab_mash_reduction_ticks > UINT16_C(60) ||
        fighter->grab_release_ticks == UINT16_C(0) ||
        fighter->grab_release_ticks > UINT16_C(120) ||
        fighter->pummel_damage_q16 == UINT32_C(0) ||
        fighter->pummel_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        fighter->pummel_hit_tick == UINT16_C(0) ||
        fighter->pummel_hit_tick >= fighter->pummel_total_ticks ||
        fighter->pummel_total_ticks > UINT16_C(120) ||
        fighter->air_jump_count > UINT8_C(8) ||
        fighter->powershield_cancel_enabled > UINT8_C(1) ||
        fighter->wall_jump_enabled > UINT8_C(1))
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
    revival_left_extent =
        -INT64_C(3) * (int64_t)stage->spawn_spacing_q16 -
        (int64_t)stage->revival_platform_half_width_q16;
    revival_right_extent =
        INT64_C(3) * (int64_t)stage->spawn_spacing_q16 +
        (int64_t)stage->revival_platform_half_width_q16;
    upper_platform_left_extent =
        (int64_t)stage->upper_platform_center_x_q16 -
        (int64_t)stage->upper_platform_half_width_q16;
    upper_platform_right_extent =
        (int64_t)stage->upper_platform_center_x_q16 +
        (int64_t)stage->upper_platform_half_width_q16;
    solid_overlaps_platform =
        stage->platform_y_q16 >= stage->solid_top_q16 &&
        stage->platform_y_q16 <= stage->solid_bottom_q16 &&
        platform_right_extent >= (int64_t)stage->solid_left_q16 &&
        platform_left_extent <= (int64_t)stage->solid_right_q16;
    upper_overlaps_platform =
        stage->upper_platform_y_q16 == stage->platform_y_q16 &&
        upper_platform_right_extent >= platform_left_extent &&
        upper_platform_left_extent <= platform_right_extent;
    upper_overlaps_revival =
        stage->upper_platform_y_q16 >=
            stage->revival_platform_start_y_q16 &&
        stage->upper_platform_y_q16 <=
            stage->revival_platform_end_y_q16 &&
        upper_platform_right_extent >= revival_left_extent &&
        upper_platform_left_extent <= revival_right_extent;
    upper_overlaps_solid =
        stage->upper_platform_y_q16 >= stage->solid_top_q16 &&
        stage->upper_platform_y_q16 <= stage->solid_bottom_q16 &&
        upper_platform_right_extent >= (int64_t)stage->solid_left_q16 &&
        upper_platform_left_extent <= (int64_t)stage->solid_right_q16;
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
        stage->blast_top_q16 >= stage->upper_platform_y_q16 ||
        stage->upper_platform_y_q16 >= stage->floor_y_q16 ||
        stage->upper_platform_half_width_q16 <= INT32_C(0) ||
        upper_platform_left_extent < (int64_t)stage->floor_left_q16 ||
        upper_platform_right_extent > (int64_t)stage->floor_right_q16 ||
        upper_overlaps_platform != 0 ||
        upper_overlaps_revival != 0 ||
        upper_overlaps_solid != 0 ||
        stage->spawn_spacing_q16 <= INT32_C(0) ||
        spawn_right_extent > (int64_t)stage->floor_right_q16 ||
        spawn_left_extent < (int64_t)stage->floor_left_q16 ||
        (int64_t)stage->revival_platform_start_y_q16 -
                (int64_t)fighter->half_height_q16 <
            (int64_t)stage->blast_top_q16 ||
        stage->revival_platform_end_y_q16 <=
            stage->revival_platform_start_y_q16 ||
        stage->revival_platform_end_y_q16 >= stage->solid_top_q16 ||
        stage->revival_platform_half_width_q16 <
            fighter->half_width_q16 ||
        revival_left_extent < (int64_t)stage->floor_left_q16 ||
        revival_right_extent > (int64_t)stage->floor_right_q16 ||
        stage->revival_platform_descent_ticks == UINT16_C(0) ||
        stage->revival_platform_hold_ticks == UINT16_C(0) ||
        (uint32_t)stage->revival_platform_descent_ticks +
                (uint32_t)stage->revival_platform_hold_ticks >
            UINT32_C(600) ||
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

    item = &content->item;
    maximum_item_knockback_x =
        (int64_t)item->base_knockback_x_q16 +
        (((int64_t)item->knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_item_knockback_y =
        (int64_t)item->base_knockback_y_q16 +
        ((((int64_t)item->knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    if (item->enabled > UINT8_C(1) ||
        item->half_width_q16 <= INT32_C(0) ||
        item->half_height_q16 <= INT32_C(0) ||
        item->half_width_q16 > maximum_fighter_extent_q16 ||
        item->half_height_q16 > maximum_fighter_extent_q16 ||
        item->spawn_x_q16 < -maximum_coordinate_q16 ||
        item->spawn_x_q16 > maximum_coordinate_q16 ||
        item->spawn_y_q16 < INT32_C(0) ||
        item->spawn_y_q16 > maximum_coordinate_q16 ||
        item->pickup_half_width_q16 < item->half_width_q16 ||
        item->pickup_half_height_q16 < item->half_height_q16 ||
        item->pickup_half_width_q16 > maximum_fighter_extent_q16 ||
        item->pickup_half_height_q16 > maximum_fighter_extent_q16 ||
        item->held_offset_x_q16 < -maximum_fighter_extent_q16 ||
        item->held_offset_x_q16 > maximum_fighter_extent_q16 ||
        item->held_offset_y_q16 < -maximum_fighter_extent_q16 ||
        item->held_offset_y_q16 > maximum_fighter_extent_q16 ||
        item->gravity_q16 <= INT32_C(0) ||
        item->gravity_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->fall_speed_q16 < item->gravity_q16 ||
        item->fall_speed_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->drop_velocity_y_q16 < INT32_C(0) ||
        item->drop_velocity_y_q16 > item->fall_speed_q16 ||
        item->forward_throw.velocity_x_q16 <= INT32_C(0) ||
        item->forward_throw.velocity_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->forward_throw.velocity_y_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->forward_throw.velocity_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->back_throw.velocity_x_q16 >= INT32_C(0) ||
        item->back_throw.velocity_x_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->back_throw.velocity_y_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->back_throw.velocity_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->up_throw.velocity_y_q16 >= INT32_C(0) ||
        item->up_throw.velocity_x_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->up_throw.velocity_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->up_throw.velocity_y_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->down_throw.velocity_y_q16 <= INT32_C(0) ||
        item->down_throw.velocity_x_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->down_throw.velocity_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->down_throw.velocity_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->momentum_transfer_q16 < INT32_C(0) ||
        item->momentum_transfer_q16 > PF_Q16_ONE ||
        item->hitbox_half_width_q16 < item->half_width_q16 ||
        item->hitbox_half_height_q16 < item->half_height_q16 ||
        item->hitbox_half_width_q16 > maximum_fighter_extent_q16 ||
        item->hitbox_half_height_q16 > maximum_fighter_extent_q16 ||
        item->damage_q16 == UINT32_C(0) ||
        item->damage_q16 > UINT32_C(50) * UINT32_C(65536) ||
        item->base_knockback_x_q16 <= INT32_C(0) ||
        item->base_knockback_y_q16 <= INT32_C(0) ||
        item->knockback_growth_q16 < INT32_C(0) ||
        maximum_item_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_item_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->hit_bounce_velocity_y_q16 >= INT32_C(0) ||
        item->hit_bounce_velocity_y_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->dash_throw_speed_q16 < INT32_C(0) ||
        item->dash_throw_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        item->throw_recovery_ticks == UINT16_C(0) ||
        item->throw_recovery_ticks > UINT16_C(240) ||
        item->dash_throw_recovery_ticks <=
            item->throw_recovery_ticks ||
        item->dash_throw_recovery_ticks > UINT16_C(240) ||
        item->glide_toss_begin_tick >
            item->glide_toss_end_tick ||
        item->glide_toss_end_tick >= fighter->forward_roll_ticks ||
        item->glide_toss_end_tick >= fighter->backward_roll_ticks ||
        item->pickup_lockout_ticks == UINT16_C(0) ||
        item->pickup_lockout_ticks > UINT16_C(240) ||
        item->lifetime_ticks == UINT16_C(0) ||
        item->lifetime_ticks > UINT16_C(3600) ||
        item->respawn_ticks == UINT16_C(0) ||
        item->respawn_ticks > UINT16_C(3600) ||
        item->hitlag_ticks == UINT16_C(0) ||
        item->hitlag_ticks > UINT16_C(120) ||
        (item->enabled != UINT8_C(0) &&
         (item->spawn_x_q16 - item->half_width_q16 <
              stage->floor_left_q16 ||
          item->spawn_x_q16 + item->half_width_q16 >
              stage->floor_right_q16 ||
          item->spawn_y_q16 !=
              stage->floor_y_q16 - item->half_height_q16)))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    projectile = &content->projectile;
    maximum_projectile_knockback_x =
        (int64_t)projectile->base_knockback_x_q16 +
        (((int64_t)projectile->knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_projectile_knockback_y =
        (int64_t)projectile->base_knockback_y_q16 +
        ((((int64_t)projectile->knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    if (projectile->enabled > UINT8_C(1) ||
        projectile->half_width_q16 <= INT32_C(0) ||
        projectile->half_height_q16 <= INT32_C(0) ||
        projectile->half_width_q16 > maximum_fighter_extent_q16 ||
        projectile->half_height_q16 > maximum_fighter_extent_q16 ||
        projectile->spawn_offset_x_q16 < INT32_C(0) ||
        projectile->spawn_offset_x_q16 > maximum_fighter_extent_q16 ||
        projectile->spawn_offset_y_q16 < -maximum_fighter_extent_q16 ||
        projectile->spawn_offset_y_q16 > maximum_fighter_extent_q16 ||
        projectile->speed_q16 <= INT32_C(0) ||
        projectile->speed_q16 > PF_SIM_MAX_MOTION_SPEED_Q16 ||
        projectile->damage_q16 == UINT32_C(0) ||
        projectile->damage_q16 > UINT32_C(50) * UINT32_C(65536) ||
        projectile->base_knockback_x_q16 <= INT32_C(0) ||
        projectile->base_knockback_y_q16 <= INT32_C(0) ||
        projectile->knockback_growth_q16 < INT32_C(0) ||
        maximum_projectile_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_projectile_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        projectile->lifetime_ticks == UINT16_C(0) ||
        projectile->lifetime_ticks > UINT16_C(3600) ||
        projectile->fire_recovery_ticks <= UINT16_C(1) ||
        projectile->fire_recovery_ticks > UINT16_C(240) ||
        projectile->hitlag_ticks == UINT16_C(0) ||
        projectile->hitlag_ticks > UINT16_C(120) ||
        projectile->powershield_reflect_window_ticks == UINT16_C(0) ||
        projectile->powershield_reflect_window_ticks >
            fighter->powershield_window_ticks)
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    reflector = &content->reflector;
    maximum_reflector_knockback_x =
        (int64_t)reflector->base_knockback_x_q16 +
        (((int64_t)reflector->knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_reflector_knockback_y =
        (int64_t)reflector->base_knockback_y_q16 +
        ((((int64_t)reflector->knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    if (reflector->enabled > UINT8_C(1) ||
        reflector->hitbox_offset_x_q16 < -maximum_fighter_extent_q16 ||
        reflector->hitbox_offset_x_q16 > maximum_fighter_extent_q16 ||
        reflector->hitbox_offset_y_q16 < -maximum_fighter_extent_q16 ||
        reflector->hitbox_offset_y_q16 > maximum_fighter_extent_q16 ||
        reflector->hitbox_half_width_q16 <= INT32_C(0) ||
        reflector->hitbox_half_width_q16 > maximum_fighter_extent_q16 ||
        reflector->hitbox_half_height_q16 <= INT32_C(0) ||
        reflector->hitbox_half_height_q16 > maximum_fighter_extent_q16 ||
        reflector->damage_q16 == UINT32_C(0) ||
        reflector->damage_q16 > UINT32_C(50) * UINT32_C(65536) ||
        reflector->base_knockback_x_q16 <= INT32_C(0) ||
        reflector->base_knockback_y_q16 < INT32_C(0) ||
        reflector->knockback_growth_q16 < INT32_C(0) ||
        maximum_reflector_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_reflector_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        reflector->startup_ticks > UINT16_C(60) ||
        reflector->active_ticks == UINT16_C(0) ||
        reflector->active_ticks > UINT16_C(60) ||
        reflector->recovery_ticks == UINT16_C(0) ||
        reflector->recovery_ticks > UINT16_C(240) ||
        (uint32_t)reflector->startup_ticks +
                (uint32_t)reflector->active_ticks +
                (uint32_t)reflector->recovery_ticks >
            UINT32_C(600) ||
        reflector->hitlag_ticks == UINT16_C(0) ||
        reflector->hitlag_ticks > UINT16_C(120))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    charge = &content->charge;
    maximum_charge_knockback_x =
        (int64_t)charge->base_knockback_x_q16 +
        (((int64_t)charge->knockback_growth_q16 *
          (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
         16U);
    maximum_charge_knockback_y =
        (int64_t)charge->base_knockback_y_q16 +
        ((((int64_t)charge->knockback_growth_q16 *
           (int64_t)PF_SIM_MAX_DAMAGE_Q16) >>
          16U) /
         INT64_C(2));
    if (charge->enabled > UINT8_C(1) ||
        charge->hitbox_offset_x_q16 < -maximum_fighter_extent_q16 ||
        charge->hitbox_offset_x_q16 > maximum_fighter_extent_q16 ||
        charge->hitbox_offset_y_q16 < -maximum_fighter_extent_q16 ||
        charge->hitbox_offset_y_q16 > maximum_fighter_extent_q16 ||
        charge->hitbox_half_width_q16 <= INT32_C(0) ||
        charge->hitbox_half_width_q16 > maximum_fighter_extent_q16 ||
        charge->hitbox_half_height_q16 <= INT32_C(0) ||
        charge->hitbox_half_height_q16 > maximum_fighter_extent_q16 ||
        charge->base_damage_q16 == UINT32_C(0) ||
        charge->base_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        charge->bonus_damage_q16 >
            UINT32_C(50) * UINT32_C(65536) ||
        (uint64_t)charge->base_damage_q16 +
                (uint64_t)charge->bonus_damage_q16 >
            UINT64_C(50) * UINT64_C(65536) ||
        charge->base_knockback_x_q16 <= INT32_C(0) ||
        charge->base_knockback_y_q16 < INT32_C(0) ||
        charge->knockback_growth_q16 < INT32_C(0) ||
        maximum_charge_knockback_x >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        maximum_charge_knockback_y >
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16 ||
        charge->max_charge_ticks == UINT16_C(0) ||
        charge->max_charge_ticks > UINT16_C(600) ||
        charge->store_animation_ticks == UINT16_C(0) ||
        charge->store_animation_ticks > UINT16_C(120) ||
        charge->release_startup_ticks > UINT16_C(120) ||
        charge->release_active_ticks == UINT16_C(0) ||
        charge->release_active_ticks > UINT16_C(120) ||
        charge->release_recovery_ticks == UINT16_C(0) ||
        charge->release_recovery_ticks > UINT16_C(600) ||
        (uint32_t)charge->release_startup_ticks +
                (uint32_t)charge->release_active_ticks +
                (uint32_t)charge->release_recovery_ticks >
            UINT32_C(600) ||
        charge->release_hitlag_ticks == UINT16_C(0) ||
        charge->release_hitlag_ticks > UINT16_C(120))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    recovery = &content->recovery;
    if (recovery->enabled > UINT8_C(1) ||
        recovery->horizontal_speed_q16 <= INT32_C(0) ||
        recovery->horizontal_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        recovery->vertical_speed_q16 <= fighter->gravity_q16 ||
        recovery->vertical_speed_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        recovery->ascent_ticks == UINT16_C(0) ||
        recovery->ascent_ticks > UINT16_C(120))
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
