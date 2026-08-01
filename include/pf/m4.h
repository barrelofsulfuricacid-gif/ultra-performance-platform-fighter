#ifndef PF_M4_H
#define PF_M4_H

#include "pf/sim.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_M4_CONTENT_SCHEMA_VERSION UINT16_C(49)
#define PF_M4_FIGHTER_SCHEMA_VERSION UINT16_C(43)
#define PF_M4_STAGE_SCHEMA_VERSION UINT16_C(3)
#define PF_M4_ITEM_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_PROJECTILE_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_REFLECTOR_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_CHARGE_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_RECOVERY_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_INSPECTION_SCHEMA_VERSION UINT16_C(42)
#define PF_M4_PLACEHOLDER_FIGHTER_COUNT UINT8_C(1)
#define PF_M4_TEST_STAGE_COUNT UINT8_C(1)
#define PF_M4_TEST_ITEM_COUNT UINT8_C(1)
#define PF_M4_TEST_PROJECTILE_COUNT UINT8_C(1)
#define PF_M4_TEST_REFLECTOR_COUNT UINT8_C(1)
#define PF_M4_TEST_CHARGE_COUNT UINT8_C(1)
#define PF_M4_TEST_RECOVERY_COUNT UINT8_C(1)

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
    PF_M4_ACTION_SHIELD_BREAK_STUN = 48,
    PF_M4_ACTION_GRAB = 49,
    PF_M4_ACTION_GRAB_HOLD = 50,
    PF_M4_ACTION_GRABBED = 51,
    PF_M4_ACTION_GRAB_RELEASE = 52,
    PF_M4_ACTION_THROW_FORWARD = 53,
    PF_M4_ACTION_THROW_BACK = 54,
    PF_M4_ACTION_THROW_UP = 55,
    PF_M4_ACTION_THROW_DOWN = 56,
    PF_M4_ACTION_DASH_ATTACK = 57,
    PF_M4_ACTION_JAB_FINAL = 58,
    PF_M4_ACTION_RESET_BOUND = 59,
    PF_M4_ACTION_FORCED_GETUP = 60,
    PF_M4_ACTION_DELAYED_AIR_JUMP = 61,
    PF_M4_ACTION_ITEM_THROW = 62,
    PF_M4_ACTION_ITEM_DASH_THROW = 63,
    PF_M4_ACTION_PROJECTILE_FIRE_GROUND = 64,
    PF_M4_ACTION_PROJECTILE_FIRE_AIR = 65,
    PF_M4_ACTION_REFLECTOR_GROUND = 66,
    PF_M4_ACTION_REFLECTOR_AIR = 67,
    PF_M4_ACTION_CHARGE_GROUND = 68,
    PF_M4_ACTION_CHARGE_STORE_GROUND = 69,
    PF_M4_ACTION_CHARGE_RELEASE_GROUND = 70,
    PF_M4_ACTION_MOONWALK_SETUP = 71,
    PF_M4_ACTION_MOONWALK = 72,
    PF_M4_ACTION_TEETER = 73,
    PF_M4_ACTION_CROUCH_STEP = 74,
    PF_M4_ACTION_TAUNT = 75,
    PF_M4_ACTION_WALL_JUMP = 76,
    PF_M4_ACTION_VECTOR_ASCENT = 77,
    PF_M4_ACTION_PUMMEL = 78,
    PF_M4_ACTION_UP_ATTACK = 79,
    PF_M4_ACTION_DOWN_ATTACK = 80,
    PF_M4_ACTION_FORWARD_AERIAL = 81,
    PF_M4_ACTION_BACK_AERIAL = 82,
    PF_M4_ACTION_UP_AERIAL = 83,
    PF_M4_ACTION_DOWN_AERIAL = 84,
    PF_M4_ACTION_LEDGE_ROLL = 85,
    PF_M4_ACTION_LEDGE_ATTACK = 86,
    PF_M4_ACTION_FORWARD_ATTACK = 87,
    PF_M4_ACTION_FORWARD_STRONG_ATTACK = 88,
    PF_M4_ACTION_UP_STRONG_ATTACK = 89,
    PF_M4_ACTION_DOWN_STRONG_ATTACK = 90,
    PF_M4_ACTION_FORWARD_STRONG_CHARGE = 91,
    PF_M4_ACTION_UP_STRONG_CHARGE = 92,
    PF_M4_ACTION_DOWN_STRONG_CHARGE = 93,
    PF_M4_ACTION_REVIVAL_PLATFORM = 94
} pf_m4_action_state;

typedef enum pf_m4_projectile_state
{
    PF_M4_PROJECTILE_STATE_INACTIVE = 0,
    PF_M4_PROJECTILE_STATE_SPAWNING = 1,
    PF_M4_PROJECTILE_STATE_ACTIVE = 2
} pf_m4_projectile_state;

typedef struct pf_m4_projectile_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    int32_t half_width_q16;
    int32_t half_height_q16;
    int32_t spawn_offset_x_q16;
    int32_t spawn_offset_y_q16;
    int32_t speed_q16;
    uint32_t damage_q16;
    int32_t base_knockback_x_q16;
    int32_t base_knockback_y_q16;
    int32_t knockback_growth_q16;
    uint16_t lifetime_ticks;
    uint16_t fire_recovery_ticks;
    uint16_t hitlag_ticks;
    uint16_t powershield_reflect_window_ticks;
} pf_m4_projectile_data;

typedef struct pf_m4_reflector_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    int32_t hitbox_offset_x_q16;
    int32_t hitbox_offset_y_q16;
    int32_t hitbox_half_width_q16;
    int32_t hitbox_half_height_q16;
    uint32_t damage_q16;
    int32_t base_knockback_x_q16;
    int32_t base_knockback_y_q16;
    int32_t knockback_growth_q16;
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
} pf_m4_reflector_data;

typedef struct pf_m4_charge_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    int32_t hitbox_offset_x_q16;
    int32_t hitbox_offset_y_q16;
    int32_t hitbox_half_width_q16;
    int32_t hitbox_half_height_q16;
    uint32_t base_damage_q16;
    uint32_t bonus_damage_q16;
    int32_t base_knockback_x_q16;
    int32_t base_knockback_y_q16;
    int32_t knockback_growth_q16;
    uint16_t max_charge_ticks;
    uint16_t store_animation_ticks;
    uint16_t release_startup_ticks;
    uint16_t release_active_ticks;
    uint16_t release_recovery_ticks;
    uint16_t release_hitlag_ticks;
} pf_m4_charge_data;

typedef struct pf_m4_recovery_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    int32_t horizontal_speed_q16;
    int32_t vertical_speed_q16;
    uint16_t ascent_ticks;
    uint16_t reserved2;
} pf_m4_recovery_data;

typedef struct pf_m4_throw_data
{
    uint32_t damage_q16;
    int32_t base_velocity_x_q16;
    int32_t base_velocity_y_q16;
    int32_t velocity_growth_x_q16;
    int32_t velocity_growth_y_q16;
    uint16_t release_tick;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
    uint16_t reserved;
} pf_m4_throw_data;

typedef struct pf_m4_attack_data
{
    int32_t hitbox_offset_x_q16;
    int32_t hitbox_offset_y_q16;
    int32_t hitbox_half_width_q16;
    int32_t hitbox_half_height_q16;
    uint32_t damage_q16;
    int32_t base_knockback_x_q16;
    int32_t base_knockback_y_q16;
    int32_t knockback_growth_q16;
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
} pf_m4_attack_data;

typedef enum pf_m4_item_state
{
    PF_M4_ITEM_STATE_INACTIVE = 0,
    PF_M4_ITEM_STATE_GROUND = 1,
    PF_M4_ITEM_STATE_HELD = 2,
    PF_M4_ITEM_STATE_AIRBORNE = 3,
    PF_M4_ITEM_STATE_RESPAWN_WAIT = 4
} pf_m4_item_state;

typedef enum pf_m4_item_throw_direction
{
    PF_M4_ITEM_THROW_NONE = 0,
    PF_M4_ITEM_THROW_FORWARD = 1,
    PF_M4_ITEM_THROW_BACK = 2,
    PF_M4_ITEM_THROW_UP = 3,
    PF_M4_ITEM_THROW_DOWN = 4
} pf_m4_item_throw_direction;

typedef struct pf_m4_item_velocity
{
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
} pf_m4_item_velocity;

typedef struct pf_m4_item_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    int32_t half_width_q16;
    int32_t half_height_q16;
    int32_t spawn_x_q16;
    int32_t spawn_y_q16;
    int32_t pickup_half_width_q16;
    int32_t pickup_half_height_q16;
    int32_t held_offset_x_q16;
    int32_t held_offset_y_q16;
    int32_t gravity_q16;
    int32_t fall_speed_q16;
    int32_t drop_velocity_y_q16;
    pf_m4_item_velocity forward_throw;
    pf_m4_item_velocity back_throw;
    pf_m4_item_velocity up_throw;
    pf_m4_item_velocity down_throw;
    int32_t momentum_transfer_q16;
    int32_t hitbox_half_width_q16;
    int32_t hitbox_half_height_q16;
    uint32_t damage_q16;
    int32_t base_knockback_x_q16;
    int32_t base_knockback_y_q16;
    int32_t knockback_growth_q16;
    int32_t hit_bounce_velocity_y_q16;
    int32_t dash_throw_speed_q16;
    uint16_t throw_recovery_ticks;
    uint16_t dash_throw_recovery_ticks;
    uint16_t glide_toss_begin_tick;
    uint16_t glide_toss_end_tick;
    uint16_t pickup_lockout_ticks;
    uint16_t lifetime_ticks;
    uint16_t respawn_ticks;
    uint16_t hitlag_ticks;
    uint16_t reserved2;
} pf_m4_item_data;

typedef enum pf_m4_surface
{
    PF_M4_SURFACE_NONE = 0,
    PF_M4_SURFACE_FLOOR = 1,
    PF_M4_SURFACE_PLATFORM = 2,
    PF_M4_SURFACE_SOLID_TOP = 3,
    PF_M4_SURFACE_REVIVAL_PLATFORM = 4
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
    int32_t weight_q16;
    int32_t ground_acceleration_q16;
    int32_t turn_acceleration_q16;
    int32_t traction_q16;
    int32_t walk_speed_q16;
    int32_t run_speed_q16;
    int32_t initial_dash_speed_q16;
    int32_t teeter_snap_distance_q16;
    int32_t crouch_step_speed_q16;
    int32_t air_acceleration_q16;
    int32_t air_speed_q16;
    int32_t gravity_q16;
    int32_t fall_speed_q16;
    int32_t fast_fall_speed_q16;
    int32_t full_hop_speed_q16;
    int32_t short_hop_speed_q16;
    int32_t double_jump_speed_q16;
    int32_t platform_drop_nudge_q16;
    int32_t ledge_roll_distance_q16;
    int32_t drop_cancel_snap_distance_q16;
    int32_t air_dodge_speed_q16;
    int32_t air_dodge_decay_q16;
    int32_t fall_special_mobility_q16;
    int32_t shield_break_launch_speed_q16;
    int32_t dash_attack_speed_q16;
    int32_t dash_attack_hitbox_offset_x_q16;
    int32_t dash_attack_hitbox_offset_y_q16;
    int32_t dash_attack_hitbox_half_width_q16;
    int32_t dash_attack_hitbox_half_height_q16;
    uint32_t dash_attack_damage_q16;
    int32_t dash_attack_base_knockback_x_q16;
    int32_t dash_attack_base_knockback_y_q16;
    int32_t dash_attack_knockback_growth_q16;
    int32_t jab_hitbox_offset_x_q16;
    int32_t jab_hitbox_offset_y_q16;
    int32_t jab_hitbox_half_width_q16;
    int32_t jab_hitbox_half_height_q16;
    uint32_t jab_damage_q16;
    int32_t jab_base_knockback_x_q16;
    int32_t jab_base_knockback_y_q16;
    int32_t jab_knockback_growth_q16;
    int32_t jab_final_hitbox_offset_x_q16;
    int32_t jab_final_hitbox_offset_y_q16;
    int32_t jab_final_hitbox_half_width_q16;
    int32_t jab_final_hitbox_half_height_q16;
    uint32_t jab_final_damage_q16;
    int32_t jab_final_base_knockback_x_q16;
    int32_t jab_final_base_knockback_y_q16;
    int32_t jab_final_knockback_growth_q16;
    pf_m4_attack_data up_attack;
    pf_m4_attack_data down_attack;
    pf_m4_attack_data forward_attack;
    pf_m4_attack_data forward_strong_attack;
    pf_m4_attack_data up_strong_attack;
    pf_m4_attack_data down_strong_attack;
    uint32_t smash_charge_damage_bonus_q16;
    uint16_t smash_charge_max_ticks;
    uint16_t smash_charge_reserved;
    pf_m4_attack_data forward_aerial;
    pf_m4_attack_data back_aerial;
    pf_m4_attack_data up_aerial;
    pf_m4_attack_data down_aerial;
    pf_m4_attack_data ledge_attack;
    uint32_t reset_max_damage_q16;
    int32_t reset_bound_speed_q16;
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
    int32_t v_cancel_velocity_scale_q16;
    uint32_t crouch_cancel_max_damage_q16;
    int32_t crouch_cancel_velocity_scale_q16;
    int32_t crouch_cancel_hitstun_scale_q16;
    int32_t di_max_tangent_q16;
    int32_t sdi_distance_q16;
    int32_t asdi_distance_q16;
    int32_t shield_sdi_scale_q16;
    int32_t tech_roll_speed_q16;
    int32_t wall_tech_speed_q16;
    int32_t wall_tech_jump_speed_x_q16;
    int32_t wall_tech_jump_speed_y_q16;
    int32_t wall_jump_speed_x_q16;
    int32_t wall_jump_speed_y_q16;
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
    uint32_t light_shield_hold_depletion_q16;
    uint32_t shield_regeneration_q16;
    uint32_t shield_damage_multiplier_q16;
    int32_t shield_stun_damage_multiplier_q16;
    int32_t shield_stun_base_q16;
    int32_t shield_defender_pushback_damage_q16;
    int32_t shield_defender_pushback_base_q16;
    int32_t shield_defender_pushback_scale_q16;
    int32_t light_shield_defender_pushback_scale_q16;
    int32_t shield_attacker_pushback_damage_q16;
    int32_t shield_attacker_pushback_base_q16;
    int32_t shield_half_width_q16;
    int32_t shield_half_height_q16;
    int32_t shield_minimum_size_scale_q16;
    int32_t dense_shield_size_scale_q16;
    int32_t shield_tilt_max_x_q16;
    int32_t shield_tilt_max_y_q16;
    int32_t grabbox_offset_x_q16;
    int32_t grabbox_offset_y_q16;
    int32_t grabbox_half_width_q16;
    int32_t grabbox_half_height_q16;
    int32_t grabbed_offset_x_q16;
    int32_t grabbed_offset_y_q16;
    int32_t grab_escape_damage_ticks_q16;
    uint32_t pummel_damage_q16;
    uint16_t pummel_hit_tick;
    uint16_t pummel_total_ticks;
    pf_m4_throw_data forward_throw;
    pf_m4_throw_data back_throw;
    pf_m4_throw_data up_throw;
    pf_m4_throw_data down_throw;
    uint16_t jump_squat_ticks;
    uint16_t double_jump_cancel_ticks;
    uint16_t double_jump_armor_max_hitstun_ticks;
    uint16_t initial_dash_ticks;
    uint16_t moonwalk_setup_ticks;
    uint16_t teeter_ticks;
    uint16_t crouch_step_ticks;
    uint16_t taunt_ticks;
    uint16_t forward_smash_input_window_ticks;
    uint16_t landing_ticks;
    uint16_t platform_drop_ticks;
    uint16_t air_dodge_ticks;
    uint16_t air_dodge_invulnerability_begin_tick;
    uint16_t air_dodge_invulnerability_end_tick;
    uint16_t ledge_invulnerability_ticks;
    uint16_t ledge_regrab_lockout_ticks;
    uint16_t ledge_roll_ticks;
    uint16_t ledge_roll_movement_ticks;
    uint16_t ledge_roll_invulnerability_ticks;
    uint16_t ledge_attack_invulnerability_ticks;
    uint16_t special_landing_ticks;
    uint16_t run_turnaround_ticks;
    uint16_t run_brake_ticks;
    uint16_t axis_dead_zone;
    uint16_t dash_axis_threshold;
    uint16_t run_turnaround_axis_threshold;
    uint16_t run_continue_axis_threshold;
    uint16_t run_turnaround_lockout_ticks;
    uint16_t crouch_axis_threshold;
    uint16_t shield_drop_axis_threshold;
    uint16_t dash_attack_startup_ticks;
    uint16_t dash_attack_active_ticks;
    uint16_t dash_attack_recovery_ticks;
    uint16_t dash_attack_hitlag_ticks;
    uint16_t boost_grab_cancel_begin_tick;
    uint16_t boost_grab_cancel_end_tick;
    uint16_t jab_startup_ticks;
    uint16_t jab_active_ticks;
    uint16_t jab_recovery_ticks;
    uint16_t jab_hitlag_ticks;
    uint16_t jab_combo_input_begin_tick;
    uint16_t jab_combo_input_end_tick;
    uint16_t jab_final_startup_ticks;
    uint16_t jab_final_active_ticks;
    uint16_t jab_final_recovery_ticks;
    uint16_t jab_final_hitlag_ticks;
    uint16_t reset_max_hitstun_ticks;
    uint16_t reset_bound_ticks;
    uint16_t reset_forced_getup_ticks;
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
    uint16_t v_cancel_window_ticks;
    uint16_t sdi_axis_threshold;
    uint16_t light_shield_trigger_threshold;
    uint16_t digital_trigger_threshold;
    uint16_t tumble_hitstun_threshold_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t tech_in_place_ticks;
    uint16_t tech_roll_ticks;
    uint16_t tech_invulnerability_ticks;
    uint16_t wall_tech_stall_ticks;
    uint16_t wall_tech_ticks;
    uint16_t wall_jump_ticks;
    uint16_t wall_jump_invulnerability_ticks;
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
    uint16_t grab_startup_ticks;
    uint16_t grab_active_ticks;
    uint16_t grab_recovery_ticks;
    uint16_t grab_escape_base_ticks;
    uint16_t grab_escape_max_ticks;
    uint16_t grab_mash_reduction_ticks;
    uint16_t grab_release_ticks;
    uint8_t air_jump_count;
    uint8_t powershield_cancel_enabled;
    uint8_t wall_jump_enabled;
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
    int32_t revival_platform_start_y_q16;
    int32_t revival_platform_end_y_q16;
    int32_t revival_platform_half_width_q16;
    uint16_t revival_platform_descent_ticks;
    uint16_t revival_platform_hold_ticks;
} pf_m4_stage_data;

typedef struct pf_m4_content
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t fighter_count;
    uint8_t stage_count;
    uint8_t item_count;
    uint8_t projectile_count;
    uint8_t reflector_count;
    uint8_t charge_count;
    uint8_t recovery_count;
    pf_m4_fighter_data fighter;
    pf_m4_stage_data stage;
    pf_m4_item_data item;
    pf_m4_projectile_data projectile;
    pf_m4_reflector_data reflector;
    pf_m4_charge_data charge;
    pf_m4_recovery_data recovery;
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
    int32_t grabbox_left_q16;
    int32_t grabbox_right_q16;
    int32_t grabbox_top_q16;
    int32_t grabbox_bottom_q16;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t shield_stun_ticks;
    uint8_t attack_hit_mask;
    uint8_t hitbox_active;
    uint8_t grabbox_active;
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
    uint16_t ledge_invulnerability_ticks;
    uint16_t ledge_regrab_lockout_ticks;
    uint16_t grab_escape_ticks;
    uint16_t charge_ticks;
    uint16_t smash_charge_ticks;
    uint8_t grab_target;
    uint8_t grab_owner;
    uint8_t stocks_remaining;
    uint8_t recovery_available;
    uint16_t shield_strength;
    int16_t shield_tilt_x;
    int16_t shield_tilt_y;
    int32_t shield_left_q16;
    int32_t shield_right_q16;
    int32_t shield_top_q16;
    int32_t shield_bottom_q16;
    uint8_t shield_active;
    uint8_t revival_platform_active;
    uint8_t reserved2[2];
    int32_t revival_platform_left_q16;
    int32_t revival_platform_right_q16;
    int32_t revival_platform_y_q16;
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
    int32_t revival_platform_start_y_q16;
    int32_t revival_platform_end_y_q16;
    int32_t revival_platform_half_width_q16;
    uint16_t revival_platform_descent_ticks;
    uint16_t revival_platform_hold_ticks;
} pf_m4_stage_inspection;

typedef struct pf_m4_item_inspection
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint16_t lifetime_ticks;
    uint16_t respawn_ticks;
    uint16_t pickup_lockout_ticks;
    uint8_t enabled;
    uint8_t state;
    uint8_t holder;
    uint8_t source;
    uint8_t throw_direction;
    uint8_t hit_mask;
    uint8_t hitbox_active;
    uint8_t reserved;
} pf_m4_item_inspection;

typedef struct pf_m4_projectile_inspection
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    int32_t hitbox_left_q16;
    int32_t hitbox_right_q16;
    int32_t hitbox_top_q16;
    int32_t hitbox_bottom_q16;
    uint16_t lifetime_ticks;
    uint8_t enabled;
    uint8_t state;
    uint8_t owner;
    uint8_t hitbox_active;
} pf_m4_projectile_inspection;

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
    pf_m4_item_inspection item;
    pf_m4_projectile_inspection projectile;
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
