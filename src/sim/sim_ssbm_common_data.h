#ifndef PF_SIM_SSBM_COMMON_DATA_H
#define PF_SIM_SSBM_COMMON_DATA_H

#include <stdint.h>

#define PF_M4_SSBM_COMMON_RAW_WORD_COUNT UINT16_C(518)

typedef struct ssbm_damage_response_attributes
{
    float hitstun_per_knockback_f32;
    float launch_speed_x_per_knockback_f32;
    float launch_speed_y_per_knockback_f32;
    float sakurai_air_angle_degrees_f32;
    float sakurai_max_ground_angle_degrees_f32;
    float sakurai_low_knockback_f32;
    float sakurai_high_knockback_f32;
    float damage_level_1_threshold_f32;
    float damage_level_2_threshold_f32;
    float grounded_damage_max_level_f32;
    float ground_knockback_max_speed_f32;
    float di_max_angle_radians_f32;
    float ground_knockback_decay_scale_f32;
    float air_knockback_decay_f32;
    float sdi_distance_x_f32;
    float sdi_distance_y_f32;
    float asdi_distance_x_f32;
    float asdi_distance_y_f32;
    float shield_sdi_scale_f32;
    float hitlag_damage_scale_f32;
    float crouch_hitlag_scale_f32;
    float electric_hitlag_scale_f32;
    float crouch_knockback_scale_f32;
    float smash_charge_knockback_scale_f32;
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
    float damage_fly_top_horizontal_ratio_f32;
    float damage_floor_down_speed_f32;
    float damage_floor_landing_speed_f32;
    float ground_damage_steep_angle_sine_f32;
    float ground_damage_vertical_reflection_f32;
    uint16_t damage_fly_roll_damage_threshold;
    uint16_t damage_fly_roll_random_threshold_u16;
} ssbm_damage_response_attributes;

typedef struct ssbm_surface_response_attributes
{
    float collision_threshold_x_f32;
    float collision_threshold_y_f32;
    float bounce_multiplier_f32;
    uint16_t wall_tech_stall_ticks;
    uint16_t wall_tech_invulnerability_ticks;
    uint16_t bounce_invulnerability_ticks;
    uint16_t bounce_collision_lockout_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t tech_roll_axis_threshold;
    uint16_t down_wait_ticks;
    float down_horizontal_angle_tan_f32;
    uint16_t down_up_axis_threshold;
    uint16_t down_horizontal_axis_threshold;
    uint16_t down_attack_input_window_ticks;
    uint16_t down_c_up_axis_threshold;
} ssbm_surface_response_attributes;

typedef struct ssbm_ledge_response_attributes
{
    float direction_angle_tan_f32;
    uint16_t grab_down_axis_threshold;
    uint16_t damage_threshold_percent;
    uint16_t quick_wait_ticks;
    uint16_t slow_wait_ticks;
    uint16_t stick_axis_threshold;
    uint16_t regrab_cooldown_ticks;
    uint16_t wait_invulnerability_ticks;
    uint16_t c_attack_axis_threshold;
    uint16_t c_roll_axis_threshold;
} ssbm_ledge_response_attributes;

typedef struct ssbm_mash_attributes
{
    float furafura_shield_health_f32;
    float capture_base_f32;
    float capture_handicap_scale_f32;
    float capture_handicap_reference_f32;
    float capture_rank_scale_f32;
    float capture_rank_reference_f32;
    float capture_damage_scale_f32;
    uint16_t stick_axis_threshold;
    uint16_t furafura_max_damage_reduction_ticks;
    uint16_t furafura_minimum_ticks;
    uint16_t furafura_tick_decrement;
    uint16_t furafura_mash_reduction_ticks;
    uint16_t capture_tick_decrement;
    uint16_t capture_mash_reduction_ticks;
    uint16_t reserved;
} ssbm_mash_attributes;

typedef struct ssbm_ground_input_attributes
{
    float grab_release_speed_x_f32;
    float grab_release_air_speed_x_f32;
    float grab_release_air_speed_y_f32;
    float throw_animation_weight_scale_f32;
    uint16_t teeter_turn_axis_threshold;
    uint16_t teeter_walk_axis_threshold;
    uint16_t walk_axis_threshold;
    float walk_middle_speed_ratio_f32;
    float walk_fast_speed_ratio_f32;
    uint16_t aerial_neutral_x_threshold;
    uint16_t aerial_neutral_y_threshold;
    uint16_t c_stick_horizontal_smash_threshold;
    uint16_t c_stick_up_smash_threshold;
    uint16_t c_stick_down_smash_threshold;
    uint16_t escape_axis_threshold;
    uint16_t escape_tilt_window_ticks;
    uint16_t platform_drop_axis_threshold;
    uint16_t platform_drop_tilt_window_ticks;
    uint16_t crouch_pass_delay_ticks;
    uint16_t guard_dash_grab_window_ticks;
    uint16_t special_vertical_axis_threshold;
    uint16_t up_special_repress_interval_ticks;
    uint16_t neutral_special_turn_window_ticks;
    uint16_t initial_dash_early_end_frame;
    uint16_t initial_dash_forward_roll_end_frame;
    uint16_t initial_dash_special_end_frame;
    uint16_t running_jump_axis_threshold;
    uint16_t forward_smash_input_window_ticks;
    uint16_t vertical_smash_input_window_ticks;
    uint16_t forward_tilt_axis_threshold;
    uint16_t vertical_tilt_axis_threshold;
    uint16_t vertical_smash_axis_threshold;
    float aerial_direction_angle_tan_f32;
    float tilt_direction_angle_tan_f32;
    float forward_tilt_outer_angle_tan_f32;
    float forward_tilt_inner_angle_tan_f32;
    float forward_smash_outer_angle_tan_f32;
    float forward_smash_inner_angle_tan_f32;
    float initial_dash_iasa_velocity_decay_f32;
} ssbm_ground_input_attributes;

typedef struct ssbm_rebirth_attributes
{
    uint16_t descent_ticks;
    uint16_t wait_ticks;
    uint16_t invulnerability_ticks;
    uint16_t reserved;
} ssbm_rebirth_attributes;

typedef struct ssbm_match_entry_attributes
{
    uint16_t ascent_ticks;
    uint16_t descent_ticks;
    uint16_t invulnerability_ticks;
    uint16_t player_delay_stride_ticks;
} ssbm_match_entry_attributes;

typedef struct ssbm_clank_attributes
{
    float rebound_strength_damage_scale_f32;
    float rebound_strength_base_f32;
    float rebound_velocity_strength_scale_f32;
    float rebound_velocity_base_f32;
    uint16_t damage_margin;
    uint16_t reserved;
} ssbm_clank_attributes;

typedef struct ssbm_fall_animation_attributes
{
    float direction_threshold_f32;
    float blend_rate_f32;
} ssbm_fall_animation_attributes;

const uint8_t *ssbm_common_reference_source_sha256(void);

const uint32_t *ssbm_common_reference_raw_words(
    uint16_t *out_count);

uint16_t ssbm_common_reference_jump_backward_axis_threshold(void);

const ssbm_damage_response_attributes *
ssbm_common_reference_damage_response(void);

const ssbm_surface_response_attributes *
ssbm_common_reference_surface_response(void);

const ssbm_ledge_response_attributes *
ssbm_common_reference_ledge_response(void);

const ssbm_mash_attributes *
ssbm_common_reference_mash(void);

const ssbm_ground_input_attributes *
ssbm_common_reference_ground_input(void);

float ssbm_throw_animation_rate_f32(
    uint16_t fighter_weight,
    int weight_independent);

uint16_t ssbm_throw_animation_ticks(
    uint16_t source_frames,
    uint16_t fighter_weight,
    int weight_independent);

const ssbm_rebirth_attributes *
ssbm_common_reference_rebirth(void);

const ssbm_match_entry_attributes *
ssbm_common_reference_match_entry(void);

const ssbm_clank_attributes *
ssbm_common_reference_clank(void);

const ssbm_fall_animation_attributes *
ssbm_common_reference_fall_animation(void);

#endif
