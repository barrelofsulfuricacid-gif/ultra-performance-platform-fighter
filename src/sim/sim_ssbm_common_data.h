#ifndef PF_SIM_SSBM_COMMON_DATA_H
#define PF_SIM_SSBM_COMMON_DATA_H

#include <stdint.h>

#define PF_M4_SSBM_COMMON_RAW_WORD_COUNT UINT16_C(518)

typedef struct pf_m4_ssbm_damage_response_attributes
{
    int32_t hitstun_per_knockback_q16;
    int32_t launch_speed_x_per_knockback_q16;
    int32_t launch_speed_y_per_knockback_q16;
    int32_t sakurai_air_angle_degrees_q16;
    int32_t sakurai_max_ground_angle_degrees_q16;
    int32_t sakurai_low_knockback_q16;
    int32_t sakurai_high_knockback_q16;
    int32_t damage_level_1_threshold_q16;
    int32_t damage_level_2_threshold_q16;
    int32_t grounded_damage_max_level_q16;
    int32_t ground_knockback_max_speed_q16;
    int32_t di_max_angle_radians_q30;
    int32_t ground_knockback_decay_scale_q16;
    int32_t air_knockback_decay_q16;
    int32_t sdi_distance_x_q16;
    int32_t sdi_distance_y_q16;
    int32_t asdi_distance_x_q16;
    int32_t asdi_distance_y_q16;
    int32_t shield_sdi_scale_q16;
    int32_t hitlag_damage_scale_q30;
    uint32_t crouch_hitlag_scale_q16;
    uint32_t electric_hitlag_scale_q16;
    uint32_t crouch_knockback_scale_q16;
    uint32_t smash_charge_knockback_scale_q16;
    uint16_t stick_tilt_threshold;
    uint16_t sdi_stick_threshold;
    uint16_t sdi_stick_window_ticks;
    uint16_t hitlag_base_ticks;
    uint16_t maximum_hitlag_ticks;
    uint16_t meteor_angle_min_degrees;
    uint16_t meteor_angle_max_degrees;
    uint16_t meteor_cancel_lockout_ticks;
    uint16_t meteor_cancel_invulnerability_ticks;
    uint16_t damage_fall_wiggle_axis_threshold;
    uint16_t damage_fall_wiggle_tilt_window_ticks;
    uint16_t damage_velocity_replace_window_ticks;
    uint16_t damage_jump_buffer_window_ticks;
    int32_t damage_fly_top_horizontal_ratio_q16;
    int32_t damage_floor_down_speed_q16;
    int32_t damage_floor_landing_speed_q16;
    int32_t ground_damage_steep_angle_sine_q16;
    int32_t ground_damage_vertical_reflection_q16;
    uint16_t damage_fly_roll_damage_threshold;
    uint16_t damage_fly_roll_random_threshold_u16;
} pf_m4_ssbm_damage_response_attributes;

typedef struct pf_m4_ssbm_surface_response_attributes
{
    int32_t collision_threshold_x_q16;
    int32_t collision_threshold_y_q16;
    int32_t bounce_multiplier_q16;
    uint16_t wall_tech_stall_ticks;
    uint16_t wall_tech_invulnerability_ticks;
    uint16_t bounce_invulnerability_ticks;
    uint16_t bounce_collision_lockout_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t tech_roll_axis_threshold;
    uint16_t down_wait_ticks;
    int32_t down_horizontal_angle_tan_q16;
    uint16_t down_up_axis_threshold;
    uint16_t down_horizontal_axis_threshold;
    uint16_t down_attack_input_window_ticks;
    uint16_t down_c_up_axis_threshold;
} pf_m4_ssbm_surface_response_attributes;

typedef struct pf_m4_ssbm_ledge_response_attributes
{
    int32_t direction_angle_tan_q16;
    uint16_t grab_down_axis_threshold;
    uint16_t damage_threshold_percent;
    uint16_t quick_wait_ticks;
    uint16_t slow_wait_ticks;
    uint16_t stick_axis_threshold;
    uint16_t regrab_cooldown_ticks;
    uint16_t wait_invulnerability_ticks;
    uint16_t c_attack_axis_threshold;
    uint16_t c_roll_axis_threshold;
} pf_m4_ssbm_ledge_response_attributes;

typedef struct pf_m4_ssbm_mash_attributes
{
    uint32_t furafura_shield_health_q16;
    int32_t capture_base_q16;
    int32_t capture_handicap_scale_q16;
    int32_t capture_handicap_reference_q16;
    int32_t capture_rank_scale_q16;
    int32_t capture_rank_reference_q16;
    int32_t capture_damage_scale_q16;
    uint16_t stick_axis_threshold;
    uint16_t furafura_max_damage_reduction_ticks;
    uint16_t furafura_minimum_ticks;
    uint16_t furafura_tick_decrement;
    uint16_t furafura_mash_reduction_ticks;
    uint16_t capture_tick_decrement;
    uint16_t capture_mash_reduction_ticks;
    uint16_t reserved;
} pf_m4_ssbm_mash_attributes;

typedef struct pf_m4_ssbm_ground_input_attributes
{
    int32_t grab_release_speed_x_q16;
    int32_t grab_release_air_speed_x_q16;
    int32_t grab_release_air_speed_y_q16;
    int32_t throw_animation_weight_scale_q16;
    uint16_t teeter_turn_axis_threshold;
    uint16_t teeter_walk_axis_threshold;
    uint16_t walk_axis_threshold;
    uint16_t walk_middle_speed_ratio_q16;
    uint16_t walk_fast_speed_ratio_q16;
    uint16_t aerial_neutral_x_threshold;
    uint16_t aerial_neutral_y_threshold;
    uint16_t c_stick_horizontal_smash_threshold;
    uint16_t c_stick_up_smash_threshold;
    uint16_t c_stick_down_smash_threshold;
    uint16_t escape_axis_threshold;
    uint16_t escape_tilt_window_ticks;
    uint16_t platform_drop_axis_threshold;
    uint16_t platform_drop_tilt_window_ticks;
    uint16_t guard_dash_grab_window_ticks;
    uint16_t special_vertical_axis_threshold;
    uint16_t neutral_special_turn_window_ticks;
    uint16_t initial_dash_early_end_frame;
    uint16_t initial_dash_special_end_frame;
    uint16_t running_jump_axis_threshold;
    uint16_t forward_smash_input_window_ticks;
    uint16_t vertical_smash_input_window_ticks;
    uint16_t forward_tilt_axis_threshold;
    uint16_t vertical_tilt_axis_threshold;
    uint16_t vertical_smash_axis_threshold;
    int32_t aerial_direction_angle_tan_q16;
    int32_t tilt_direction_angle_tan_q16;
    int32_t forward_tilt_outer_angle_tan_q16;
    int32_t forward_tilt_inner_angle_tan_q16;
    int32_t forward_smash_outer_angle_tan_q16;
    int32_t forward_smash_inner_angle_tan_q16;
} pf_m4_ssbm_ground_input_attributes;

typedef struct pf_m4_ssbm_rebirth_attributes
{
    uint16_t descent_ticks;
    uint16_t wait_ticks;
    uint16_t invulnerability_ticks;
    uint16_t reserved;
} pf_m4_ssbm_rebirth_attributes;

typedef struct pf_m4_ssbm_clank_attributes
{
    int32_t rebound_strength_damage_scale_q16;
    int32_t rebound_strength_base_q16;
    int32_t rebound_velocity_strength_scale_q16;
    int32_t rebound_velocity_base_q16;
    uint16_t damage_margin;
    uint16_t reserved;
} pf_m4_ssbm_clank_attributes;

typedef struct pf_m4_ssbm_fall_animation_attributes
{
    int32_t direction_threshold_q16;
    int32_t blend_rate_q16;
} pf_m4_ssbm_fall_animation_attributes;

const uint8_t *pf_m4_ssbm_common_reference_source_sha256(void);

const uint32_t *pf_m4_ssbm_common_reference_raw_words(
    uint16_t *out_count);

uint16_t pf_m4_ssbm_common_reference_jump_backward_axis_threshold(void);

const pf_m4_ssbm_damage_response_attributes *
pf_m4_ssbm_common_reference_damage_response(void);

const pf_m4_ssbm_surface_response_attributes *
pf_m4_ssbm_common_reference_surface_response(void);

const pf_m4_ssbm_ledge_response_attributes *
pf_m4_ssbm_common_reference_ledge_response(void);

const pf_m4_ssbm_mash_attributes *
pf_m4_ssbm_common_reference_mash(void);

const pf_m4_ssbm_ground_input_attributes *
pf_m4_ssbm_common_reference_ground_input(void);

const pf_m4_ssbm_rebirth_attributes *
pf_m4_ssbm_common_reference_rebirth(void);

const pf_m4_ssbm_clank_attributes *
pf_m4_ssbm_common_reference_clank(void);

const pf_m4_ssbm_fall_animation_attributes *
pf_m4_ssbm_common_reference_fall_animation(void);

#endif
