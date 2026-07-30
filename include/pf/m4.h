#ifndef PF_M4_H
#define PF_M4_H

#include "pf/sim.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_M4_CONTENT_SCHEMA_VERSION UINT16_C(14)
#define PF_M4_FIGHTER_SCHEMA_VERSION UINT16_C(14)
#define PF_M4_STAGE_SCHEMA_VERSION UINT16_C(2)
#define PF_M4_INSPECTION_SCHEMA_VERSION UINT16_C(14)
#define PF_M4_PLACEHOLDER_FIGHTER_COUNT UINT8_C(1)
#define PF_M4_TEST_STAGE_COUNT UINT8_C(1)

typedef enum pf_m4_action_state
{
    PF_M4_ACTION_GROUND_IDLE = 0,
    PF_M4_ACTION_WALK = 1,
    PF_M4_ACTION_INITIAL_DASH = 2,
    PF_M4_ACTION_RUN = 3,
    PF_M4_ACTION_CROUCH = 4,
    PF_M4_ACTION_JUMP_SQUAT = 5,
    PF_M4_ACTION_AIRBORNE = 6,
    PF_M4_ACTION_LANDING = 7,
    PF_M4_ACTION_LEDGE_HANG = 8,
    PF_M4_ACTION_LEDGE_CLIMB = 9,
    PF_M4_ACTION_RUN_TURNAROUND = 10,
    PF_M4_ACTION_RUN_BRAKE = 11,
    PF_M4_ACTION_GROUND_ATTACK = 12,
    PF_M4_ACTION_HITLAG = 13,
    PF_M4_ACTION_HITSTUN = 14,
    PF_M4_ACTION_KNOCKDOWN = 15,
    PF_M4_ACTION_TECH_IN_PLACE = 16,
    PF_M4_ACTION_TECH_ROLL = 17,
    PF_M4_ACTION_SHIELD = 18,
    PF_M4_ACTION_SHIELD_STUN = 19,
    PF_M4_ACTION_SHIELD_RELEASE = 20,
    PF_M4_ACTION_SHIELD_BREAK = 21,
    PF_M4_ACTION_STRONG_ATTACK = 22,
    PF_M4_ACTION_DOWN_WAIT = 23,
    PF_M4_ACTION_GETUP_NEUTRAL = 24,
    PF_M4_ACTION_GETUP_ROLL = 25,
    PF_M4_ACTION_GETUP_ATTACK = 26,
    PF_M4_ACTION_WALL_TECH = 27,
    PF_M4_ACTION_WALL_TECH_JUMP = 28,
    PF_M4_ACTION_CEILING_TECH = 29,
    PF_M4_ACTION_WALL_BOUNCE = 30,
    PF_M4_ACTION_CEILING_BOUNCE = 31,
    PF_M4_ACTION_AIR_DODGE = 32,
    PF_M4_ACTION_FALL_SPECIAL = 33,
    PF_M4_ACTION_SPECIAL_LANDING = 34,
    PF_M4_ACTION_AERIAL_ATTACK = 35,
    PF_M4_ACTION_AERIAL_LANDING = 36,
    PF_M4_ACTION_L_CANCEL_LANDING = 37,
    PF_M4_ACTION_ROLL_FORWARD = 38,
    PF_M4_ACTION_ROLL_BACKWARD = 39,
    PF_M4_ACTION_SPOT_DODGE = 40,
    PF_M4_ACTION_STRONG_AERIAL_ATTACK = 41,
    PF_M4_ACTION_STRONG_AERIAL_LANDING = 42,
    PF_M4_ACTION_STRONG_L_CANCEL_LANDING = 43,
    PF_M4_ACTION_RESPAWN_WAIT = 44,
    PF_M4_ACTION_ELIMINATED = 45,
    PF_M4_ACTION_SHIELD_BREAK_DOWN = 46,
    PF_M4_ACTION_SHIELD_BREAK_STAND = 47,
    PF_M4_ACTION_SHIELD_BREAK_STUN = 48
} pf_m4_action_state;

typedef enum pf_m4_surface
{
    PF_M4_SURFACE_NONE = 0,
    PF_M4_SURFACE_FLOOR = 1,
    PF_M4_SURFACE_PLATFORM = 2,
    PF_M4_SURFACE_SOLID_TOP = 3
} pf_m4_surface;

typedef enum pf_m4_ledge
{
    PF_M4_LEDGE_NONE = 0,
    PF_M4_LEDGE_LEFT = 1,
    PF_M4_LEDGE_RIGHT = 2
} pf_m4_ledge;

typedef struct pf_m4_fighter_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    int32_t half_width_q16;
    int32_t half_height_q16;
    int32_t ground_acceleration_q16;
    int32_t turn_acceleration_q16;
    int32_t traction_q16;
    int32_t walk_speed_q16;
    int32_t run_speed_q16;
    int32_t initial_dash_speed_q16;
    int32_t air_acceleration_q16;
    int32_t air_speed_q16;
    int32_t gravity_q16;
    int32_t fall_speed_q16;
    int32_t fast_fall_speed_q16;
    int32_t full_hop_speed_q16;
    int32_t short_hop_speed_q16;
    int32_t double_jump_speed_q16;
    int32_t platform_drop_nudge_q16;
    int32_t air_dodge_speed_q16;
    int32_t air_dodge_decay_q16;
    int32_t fall_special_mobility_q16;
    int32_t shield_break_launch_speed_q16;
    int32_t jab_hitbox_offset_x_q16;
    int32_t jab_hitbox_offset_y_q16;
    int32_t jab_hitbox_half_width_q16;
    int32_t jab_hitbox_half_height_q16;
    uint32_t jab_damage_q16;
    int32_t jab_base_knockback_x_q16;
    int32_t jab_base_knockback_y_q16;
    int32_t jab_knockback_growth_q16;
    int32_t strong_hitbox_offset_x_q16;
    int32_t strong_hitbox_offset_y_q16;
    int32_t strong_hitbox_half_width_q16;
    int32_t strong_hitbox_half_height_q16;
    uint32_t strong_damage_q16;
    int32_t strong_base_knockback_x_q16;
    int32_t strong_base_knockback_y_q16;
    int32_t strong_knockback_growth_q16;
    int32_t aerial_hitbox_offset_x_q16;
    int32_t aerial_hitbox_offset_y_q16;
    int32_t aerial_hitbox_half_width_q16;
    int32_t aerial_hitbox_half_height_q16;
    uint32_t aerial_damage_q16;
    int32_t aerial_base_knockback_x_q16;
    int32_t aerial_base_knockback_y_q16;
    int32_t aerial_knockback_growth_q16;
    int32_t hitstun_velocity_per_tick_q16;
    int32_t di_max_tangent_q16;
    int32_t sdi_distance_q16;
    int32_t asdi_distance_q16;
    int32_t tech_roll_speed_q16;
    int32_t wall_tech_speed_q16;
    int32_t wall_tech_jump_speed_x_q16;
    int32_t wall_tech_jump_speed_y_q16;
    int32_t ceiling_tech_speed_q16;
    int32_t surface_bounce_multiplier_q16;
    int32_t getup_roll_speed_q16;
    int32_t forward_roll_speed_q16;
    int32_t backward_roll_speed_q16;
    int32_t getup_attack_hitbox_offset_x_q16;
    int32_t getup_attack_hitbox_offset_y_q16;
    int32_t getup_attack_hitbox_half_width_q16;
    int32_t getup_attack_hitbox_half_height_q16;
    uint32_t getup_attack_damage_q16;
    int32_t getup_attack_base_knockback_x_q16;
    int32_t getup_attack_base_knockback_y_q16;
    int32_t getup_attack_knockback_growth_q16;
    uint32_t shield_health_q16;
    uint32_t shield_reset_health_q16;
    uint32_t shield_hold_depletion_q16;
    uint32_t shield_regeneration_q16;
    uint32_t shield_damage_multiplier_q16;
    int32_t shield_stun_damage_multiplier_q16;
    int32_t shield_stun_base_q16;
    int32_t shield_defender_pushback_damage_q16;
    int32_t shield_defender_pushback_base_q16;
    int32_t shield_defender_pushback_scale_q16;
    int32_t shield_attacker_pushback_damage_q16;
    int32_t shield_attacker_pushback_base_q16;
    uint16_t jump_squat_ticks;
    uint16_t initial_dash_ticks;
    uint16_t landing_ticks;
    uint16_t platform_drop_ticks;
    uint16_t air_dodge_ticks;
    uint16_t air_dodge_invulnerability_begin_tick;
    uint16_t air_dodge_invulnerability_end_tick;
    uint16_t special_landing_ticks;
    uint16_t run_turnaround_ticks;
    uint16_t run_brake_ticks;
    uint16_t axis_dead_zone;
    uint16_t dash_axis_threshold;
    uint16_t run_turnaround_axis_threshold;
    uint16_t run_continue_axis_threshold;
    uint16_t run_turnaround_lockout_ticks;
    uint16_t crouch_axis_threshold;
    uint16_t jab_startup_ticks;
    uint16_t jab_active_ticks;
    uint16_t jab_recovery_ticks;
    uint16_t jab_hitlag_ticks;
    uint16_t strong_startup_ticks;
    uint16_t strong_active_ticks;
    uint16_t strong_recovery_ticks;
    uint16_t strong_hitlag_ticks;
    uint16_t aerial_startup_ticks;
    uint16_t aerial_active_ticks;
    uint16_t aerial_recovery_ticks;
    uint16_t aerial_hitlag_ticks;
    uint16_t aerial_landing_lag_begin_tick;
    uint16_t aerial_landing_lag_end_tick;
    uint16_t aerial_landing_lag_ticks;
    uint16_t strong_aerial_landing_lag_ticks;
    uint16_t l_cancel_window_ticks;
    uint16_t l_cancel_divisor;
    uint16_t sdi_axis_threshold;
    uint16_t digital_trigger_threshold;
    uint16_t tumble_hitstun_threshold_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t tech_in_place_ticks;
    uint16_t tech_roll_ticks;
    uint16_t tech_invulnerability_ticks;
    uint16_t wall_tech_stall_ticks;
    uint16_t wall_tech_ticks;
    uint16_t ceiling_tech_ticks;
    uint16_t knockdown_ticks;
    uint16_t down_wait_ticks;
    uint16_t getup_neutral_ticks;
    uint16_t getup_neutral_invulnerability_ticks;
    uint16_t getup_roll_ticks;
    uint16_t getup_roll_invulnerability_ticks;
    uint16_t getup_attack_ticks;
    uint16_t getup_attack_invulnerability_ticks;
    uint16_t getup_attack_front_active_begin_tick;
    uint16_t getup_attack_front_active_end_tick;
    uint16_t getup_attack_back_active_begin_tick;
    uint16_t getup_attack_back_active_end_tick;
    uint16_t getup_attack_hitlag_ticks;
    uint16_t forward_roll_ticks;
    uint16_t backward_roll_ticks;
    uint16_t roll_movement_begin_tick;
    uint16_t roll_movement_end_tick;
    uint16_t roll_invulnerability_begin_tick;
    uint16_t roll_invulnerability_end_tick;
    uint16_t spot_dodge_ticks;
    uint16_t spot_dodge_invulnerability_begin_tick;
    uint16_t spot_dodge_invulnerability_end_tick;
    uint16_t shield_minimum_hold_ticks;
    uint16_t shield_release_ticks;
    uint16_t powershield_window_ticks;
    uint16_t powershield_cancel_delay_ticks;
    uint16_t shield_break_stun_ticks;
    uint16_t shield_break_minimum_stun_ticks;
    uint16_t shield_break_down_ticks;
    uint16_t shield_break_stand_ticks;
    uint16_t shield_break_mash_reduction_ticks;
    uint8_t air_jump_count;
    uint8_t powershield_cancel_enabled;
} pf_m4_fighter_data;

typedef struct pf_m4_stage_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    int32_t floor_left_q16;
    int32_t floor_right_q16;
    int32_t floor_y_q16;
    int32_t platform_center_x_q16;
    int32_t platform_y_q16;
    int32_t platform_half_width_q16;
    int32_t platform_motion_amplitude_q16;
    int32_t solid_left_q16;
    int32_t solid_right_q16;
    int32_t solid_top_q16;
    int32_t solid_bottom_q16;
    int32_t blast_left_q16;
    int32_t blast_right_q16;
    int32_t blast_top_q16;
    int32_t blast_bottom_q16;
    int32_t spawn_spacing_q16;
    uint16_t platform_motion_period_ticks;
    uint16_t reserved2;
} pf_m4_stage_data;

typedef struct pf_m4_content
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t fighter_count;
    uint8_t stage_count;
    pf_m4_fighter_data fighter;
    pf_m4_stage_data stage;
} pf_m4_content;

typedef struct pf_m4_player_inspection
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint16_t action_ticks;
    uint16_t respawn_count;
    uint8_t action_state;
    int8_t facing;
    int8_t dash_direction;
    int8_t previous_strong_direction;
    uint8_t grounded;
    uint8_t support;
    uint8_t air_jumps_remaining;
    uint8_t fast_fall;
    uint8_t short_hop_latched;
    uint8_t platform_drop_ticks;
    uint8_t active;
    uint8_t ledge;
    uint64_t last_hit_tick;
    uint32_t damage_q16;
    uint32_t last_hit_sequence;
    uint32_t last_hit_damage_q16;
    int32_t hitbox_left_q16;
    int32_t hitbox_right_q16;
    int32_t hitbox_top_q16;
    int32_t hitbox_bottom_q16;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t shield_stun_ticks;
    uint8_t attack_hit_mask;
    uint8_t hitbox_active;
    uint8_t last_hit_valid;
    uint8_t last_hit_attacker;
    uint8_t shield_held;
    uint8_t powershield;
    uint8_t tumble;
    uint8_t invulnerable;
    uint8_t trigger_input_age;
    uint8_t l_cancel_eligible;
    uint8_t sdi_pulse_count;
    int8_t sdi_direction_x;
    int8_t sdi_direction_y;
    int8_t tech_direction;
    uint32_t shield_health_q16;
    uint16_t respawn_ticks;
    uint16_t respawn_invulnerability_ticks;
    uint8_t stocks_remaining;
    uint8_t reserved;
} pf_m4_player_inspection;

typedef struct pf_m4_stage_inspection
{
    int32_t floor_left_q16;
    int32_t floor_right_q16;
    int32_t floor_y_q16;
    int32_t platform_left_q16;
    int32_t platform_right_q16;
    int32_t platform_y_q16;
    int32_t solid_left_q16;
    int32_t solid_right_q16;
    int32_t solid_top_q16;
    int32_t solid_bottom_q16;
    int32_t left_ledge_x_q16;
    int32_t right_ledge_x_q16;
    int32_t ledge_y_q16;
    int32_t blast_left_q16;
    int32_t blast_right_q16;
    int32_t blast_top_q16;
    int32_t blast_bottom_q16;
} pf_m4_stage_inspection;

typedef struct pf_m4_inspection
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t player_count;
    uint8_t stock_count;
    uint64_t tick;
    uint16_t respawn_delay_ticks;
    uint16_t respawn_invulnerability_ticks;
    uint8_t sudden_death;
    uint8_t terminated;
    uint8_t truncated;
    uint8_t winner_mask;
    pf_m4_stage_inspection stage;
    pf_m4_player_inspection players[PF_SIM_MAX_PLAYERS];
} pf_m4_inspection;

pf_status pf_m4_default_content(pf_m4_content *out_content);

pf_status pf_m4_validate_content(const pf_m4_content *content);

pf_status pf_m4_make_content_view(
    const pf_m4_content *content,
    pf_content_view *out_view);

pf_status pf_m4_inspect(
    const pf_sim *sim,
    pf_m4_inspection *out_inspection);

#ifdef __cplusplus
}
#endif

#endif
