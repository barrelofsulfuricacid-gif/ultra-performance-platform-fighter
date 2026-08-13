#ifndef PF_M4_H
#define PF_M4_H

#include "pf/sim.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_M4_CONTENT_SCHEMA_VERSION UINT16_C(78)
#define PF_M4_FIGHTER_SCHEMA_VERSION UINT16_C(70)
#define PF_M4_STAGE_SCHEMA_VERSION UINT16_C(5)
#define PF_M4_ITEM_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_PROJECTILE_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_REFLECTOR_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_CHARGE_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_RECOVERY_SCHEMA_VERSION UINT16_C(1)
#define PF_M4_INSPECTION_SCHEMA_VERSION UINT16_C(57)
#define PF_M4_INSPECTION_HIT_SPHERE_CAPACITY 4
#define PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY 11
#define PF_M4_PLACEHOLDER_FIGHTER_COUNT UINT8_C(1)
#define PF_M4_TEST_STAGE_COUNT UINT8_C(1)
#define PF_M4_TEST_ITEM_COUNT UINT8_C(1)
#define PF_M4_TEST_PROJECTILE_COUNT UINT8_C(1)
#define PF_M4_TEST_REFLECTOR_COUNT UINT8_C(1)
#define PF_M4_TEST_CHARGE_COUNT UINT8_C(1)
#define PF_M4_TEST_RECOVERY_COUNT UINT8_C(1)

enum action_state
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
    PF_M4_ACTION_RESERVED_71 = 71,
    PF_M4_ACTION_RESERVED_72 = 72,
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
    PF_M4_ACTION_REVIVAL_PLATFORM = 94,
    PF_M4_ACTION_FORWARD_AERIAL_LANDING = 95,
    PF_M4_ACTION_BACK_AERIAL_LANDING = 96,
    PF_M4_ACTION_UP_AERIAL_LANDING = 97,
    PF_M4_ACTION_DOWN_AERIAL_LANDING = 98,
    PF_M4_ACTION_FORWARD_AERIAL_L_CANCEL_LANDING = 99,
    PF_M4_ACTION_BACK_AERIAL_L_CANCEL_LANDING = 100,
    PF_M4_ACTION_UP_AERIAL_L_CANCEL_LANDING = 101,
    PF_M4_ACTION_DOWN_AERIAL_L_CANCEL_LANDING = 102,
    PF_M4_ACTION_STANDING_TURN = 103,
    PF_M4_ACTION_CROUCH_START = 104,
    PF_M4_ACTION_CROUCH_END = 105,
    PF_M4_ACTION_DASH_GRAB = 106,
    PF_M4_ACTION_FALCON_PUNCH_GROUND = 107,
    PF_M4_ACTION_FALCON_PUNCH_AIR = 108,
    PF_M4_ACTION_RAPTOR_BOOST_START_GROUND = 109,
    PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND = 110,
    PF_M4_ACTION_RAPTOR_BOOST_START_AIR = 111,
    PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR = 112,
    PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS = 113,
    PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT = 114,
    PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS = 115,
    PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT = 116,
    PF_M4_ACTION_FALCON_DIVE_START_GROUND = 117,
    PF_M4_ACTION_FALCON_DIVE_START_AIR = 118,
    PF_M4_ACTION_FALCON_DIVE_CATCH = 119,
    PF_M4_ACTION_FALCON_DIVE_THROW = 120,
    PF_M4_ACTION_FALCON_DIVE_FALL = 121,
    PF_M4_ACTION_FALCON_DIVE_LANDING = 122,
    PF_M4_ACTION_FALCON_KICK_START_GROUND = 123,
    PF_M4_ACTION_FALCON_KICK_END_GROUND = 124,
    PF_M4_ACTION_FALCON_KICK_START_AIR = 125,
    PF_M4_ACTION_FALCON_KICK_LANDING = 126,
    PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND = 127,
    PF_M4_ACTION_FALCON_KICK_END_AIR = 128,
    PF_M4_ACTION_FALCON_KICK_WALL_REBOUND = 129,
    PF_M4_ACTION_DAMAGE_LOW_1 = 130,
    PF_M4_ACTION_DAMAGE_LOW_2 = 131,
    PF_M4_ACTION_DAMAGE_LOW_3 = 132,
    PF_M4_ACTION_LEDGE_CATCH = 133,
    PF_M4_ACTION_LEDGE_JUMP = 134,
    PF_M4_ACTION_REBOUND_STOP = 135,
    PF_M4_ACTION_REBOUND = 136,
    PF_M4_ACTION_JAB_THIRD = 137,
    PF_M4_ACTION_RAPID_JAB_START = 138,
    PF_M4_ACTION_RAPID_JAB_LOOP = 139,
    PF_M4_ACTION_RAPID_JAB_END = 140,
    PF_M4_ACTION_FORWARD_ATTACK_HIGH = 141,
    PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH = 142,
    PF_M4_ACTION_FORWARD_ATTACK_MID_LOW = 143,
    PF_M4_ACTION_FORWARD_ATTACK_LOW = 144,
    PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH = 145,
    PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW = 146,
    PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH = 147,
    PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW = 148,
    PF_M4_ACTION_MATCH_ENTRY = 149,
    PF_M4_ACTION_MATCH_ENTRY_START = 150,
    PF_M4_ACTION_MATCH_ENTRY_END = 151
};

typedef enum projectile_state
{
    PF_M4_PROJECTILE_STATE_INACTIVE = 0,
    PF_M4_PROJECTILE_STATE_SPAWNING = 1,
    PF_M4_PROJECTILE_STATE_ACTIVE = 2
} projectile_state;

typedef struct projectile_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    float half_width_f32;
    float half_height_f32;
    float spawn_offset_x_f32;
    float spawn_offset_y_f32;
    float speed_f32;
    float damage_f32;
    float base_knockback_x_f32;
    float base_knockback_y_f32;
    float knockback_growth_f32;
    uint16_t lifetime_ticks;
    uint16_t fire_recovery_ticks;
    uint16_t hitlag_ticks;
    uint16_t powershield_reflect_window_ticks;
} projectile_data;

typedef struct reflector_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    float hitbox_offset_x_f32;
    float hitbox_offset_y_f32;
    float hitbox_half_width_f32;
    float hitbox_half_height_f32;
    float damage_f32;
    float base_knockback_x_f32;
    float base_knockback_y_f32;
    float knockback_growth_f32;
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
} reflector_data;

typedef struct charge_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    float hitbox_offset_x_f32;
    float hitbox_offset_y_f32;
    float hitbox_half_width_f32;
    float hitbox_half_height_f32;
    float base_damage_f32;
    float bonus_damage_f32;
    float base_knockback_x_f32;
    float base_knockback_y_f32;
    float knockback_growth_f32;
    uint16_t max_charge_ticks;
    uint16_t store_animation_ticks;
    uint16_t release_startup_ticks;
    uint16_t release_active_ticks;
    uint16_t release_recovery_ticks;
    uint16_t release_hitlag_ticks;
} charge_data;

typedef struct recovery_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    float horizontal_speed_f32;
    float vertical_speed_f32;
    uint16_t ascent_ticks;
    uint16_t reserved2;
} recovery_data;

typedef struct melee_knockback_data
{
    uint16_t angle_degrees;
    uint16_t growth;
    uint16_t weight_set;
    uint16_t base;
    uint8_t enabled;
    uint8_t reserved[3];
} melee_knockback_data;

struct throw_data
{
    float damage_f32;
    float base_velocity_x_f32;
    float base_velocity_y_f32;
    float velocity_growth_x_f32;
    float velocity_growth_y_f32;
    uint16_t release_tick;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
    uint16_t reserved;
    melee_knockback_data melee_knockback;
};

typedef struct attack_data
{
    float hitbox_offset_x_f32;
    float hitbox_offset_y_f32;
    float hitbox_half_width_f32;
    float hitbox_half_height_f32;
    float damage_f32;
    float base_knockback_x_f32;
    float base_knockback_y_f32;
    float knockback_growth_f32;
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
} attack_data;

typedef enum item_state
{
    PF_M4_ITEM_STATE_INACTIVE = 0,
    PF_M4_ITEM_STATE_GROUND = 1,
    PF_M4_ITEM_STATE_HELD = 2,
    PF_M4_ITEM_STATE_AIRBORNE = 3,
    PF_M4_ITEM_STATE_RESPAWN_WAIT = 4
} item_state;

typedef enum item_throw_direction
{
    PF_M4_ITEM_THROW_NONE = 0,
    PF_M4_ITEM_THROW_FORWARD = 1,
    PF_M4_ITEM_THROW_BACK = 2,
    PF_M4_ITEM_THROW_UP = 3,
    PF_M4_ITEM_THROW_DOWN = 4
} item_throw_direction;

typedef struct item_velocity
{
    float velocity_x_f32;
    float velocity_y_f32;
} item_velocity;

typedef struct item_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t enabled;
    uint8_t reserved;
    float half_width_f32;
    float half_height_f32;
    float spawn_x_f32;
    float spawn_y_f32;
    float pickup_half_width_f32;
    float pickup_half_height_f32;
    float held_offset_x_f32;
    float held_offset_y_f32;
    float gravity_f32;
    float fall_speed_f32;
    float drop_velocity_y_f32;
    item_velocity forward_throw;
    item_velocity back_throw;
    item_velocity up_throw;
    item_velocity down_throw;
    float momentum_transfer_f32;
    float hitbox_half_width_f32;
    float hitbox_half_height_f32;
    float damage_f32;
    float base_knockback_x_f32;
    float base_knockback_y_f32;
    float knockback_growth_f32;
    float hit_bounce_velocity_y_f32;
    float dash_throw_speed_f32;
    uint16_t throw_recovery_ticks;
    uint16_t dash_throw_recovery_ticks;
    uint16_t glide_toss_begin_tick;
    uint16_t glide_toss_end_tick;
    uint16_t pickup_lockout_ticks;
    uint16_t lifetime_ticks;
    uint16_t respawn_ticks;
    uint16_t hitlag_ticks;
    uint16_t reserved2;
} item_data;

enum surface
{
    PF_M4_SURFACE_NONE = 0,
    PF_M4_SURFACE_FLOOR = 1,
    PF_M4_SURFACE_PLATFORM = 2,
    PF_M4_SURFACE_SOLID_TOP = 3,
    PF_M4_SURFACE_REVIVAL_PLATFORM = 4,
    PF_M4_SURFACE_UPPER_PLATFORM = 5
};

enum reference_stage
{
    PF_M4_REFERENCE_STAGE_AUTHORED = 0,
    PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE = 1,
    PF_M4_REFERENCE_STAGE_BATTLEFIELD = 2
};

typedef enum reference_stage_line_kind
{
    PF_M4_REFERENCE_STAGE_LINE_UNCLASSIFIED = 0,
    PF_M4_REFERENCE_STAGE_LINE_FLOOR = 1,
    PF_M4_REFERENCE_STAGE_LINE_CEILING = 2,
    PF_M4_REFERENCE_STAGE_LINE_RIGHT_WALL = 3,
    PF_M4_REFERENCE_STAGE_LINE_LEFT_WALL = 4,
    PF_M4_REFERENCE_STAGE_LINE_DYNAMIC = 5
} reference_stage_line_kind;

typedef struct reference_stage_line
{
    float start_x_f32;
    float start_y_f32;
    float end_x_f32;
    float end_y_f32;
    float source_normal_x_f32;
    float source_normal_y_f32;
    uint16_t support;
    uint8_t kind;
    uint8_t reserved;
} reference_stage_line;

enum ledge
{
    PF_M4_LEDGE_NONE = 0,
    PF_M4_LEDGE_LEFT = 1,
    PF_M4_LEDGE_RIGHT = 2
};

enum prone_orientation
{
    PF_M4_PRONE_NONE = 0,
    PF_M4_PRONE_BACK = 1,
    PF_M4_PRONE_STOMACH = 2
};

typedef enum gameplay_ruleset
{
    PF_M4_GAMEPLAY_RULESET_GENERIC = 0,
    PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084 = 1
} gameplay_ruleset;

typedef struct getup_roll_timing
{
    uint8_t movement_begin_tick;
    uint8_t invulnerability_begin_tick;
    uint8_t invulnerability_end_tick;
    uint8_t reserved;
} getup_roll_timing;

typedef struct fighter_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t reference_frame_data_enabled;
    uint8_t reserved;
    float half_width_f32;
    float half_height_f32;
    float player_push_half_width_f32;
    float player_push_speed_f32;
    float weight_f32;
    float ground_acceleration_f32;
    float turn_acceleration_f32;
    float traction_f32;
    float walk_speed_f32;
    float run_speed_f32;
    float initial_dash_speed_f32;
    float walk_initial_velocity_f32;
    float walk_acceleration_f32;
    float slow_walk_animation_scaling_f32;
    float middle_walk_animation_scaling_f32;
    float fast_walk_animation_scaling_f32;
    float run_animation_scaling_f32;
    float walk_middle_speed_ratio_f32;
    float walk_fast_speed_ratio_f32;
    float dash_run_base_acceleration_f32;
    float ground_max_horizontal_speed_f32;
    float walk_acceleration_taper_f32;
    float run_acceleration_taper_f32;
    float teeter_snap_distance_f32;
    float crouch_step_speed_f32;
    float air_acceleration_f32;
    float air_base_acceleration_f32;
    float air_friction_f32;
    float air_max_horizontal_speed_f32;
    float air_speed_f32;
    float jump_horizontal_input_speed_f32;
    float jump_horizontal_momentum_multiplier_f32;
    float jump_horizontal_max_speed_f32;
    float gravity_f32;
    float fall_speed_f32;
    float fast_fall_speed_f32;
    float full_hop_speed_f32;
    float short_hop_speed_f32;
    float double_jump_speed_f32;
    float double_jump_horizontal_speed_f32;
    float platform_drop_nudge_f32;
    float platform_drop_speed_y_f32;
    float ledge_jump_speed_x_f32;
    float ledge_jump_speed_y_f32;
    float ledge_roll_distance_f32;
    float drop_cancel_snap_distance_f32;
    float air_dodge_speed_x_f32;
    float air_dodge_speed_y_f32;
    float air_dodge_decay_f32;
    float fall_special_mobility_f32;
    float shield_break_launch_speed_f32;
    float dash_attack_speed_f32;
    float dash_attack_hitbox_offset_x_f32;
    float dash_attack_hitbox_offset_y_f32;
    float dash_attack_hitbox_half_width_f32;
    float dash_attack_hitbox_half_height_f32;
    float dash_attack_damage_f32;
    float dash_attack_base_knockback_x_f32;
    float dash_attack_base_knockback_y_f32;
    float dash_attack_knockback_growth_f32;
    float jab_hitbox_offset_x_f32;
    float jab_hitbox_offset_y_f32;
    float jab_hitbox_half_width_f32;
    float jab_hitbox_half_height_f32;
    float jab_damage_f32;
    float jab_base_knockback_x_f32;
    float jab_base_knockback_y_f32;
    float jab_knockback_growth_f32;
    melee_knockback_data jab_melee_knockback;
    float jab_final_hitbox_offset_x_f32;
    float jab_final_hitbox_offset_y_f32;
    float jab_final_hitbox_half_width_f32;
    float jab_final_hitbox_half_height_f32;
    float jab_final_damage_f32;
    float jab_final_base_knockback_x_f32;
    float jab_final_base_knockback_y_f32;
    float jab_final_knockback_growth_f32;
    melee_knockback_data jab_final_melee_knockback;
    attack_data up_attack;
    attack_data down_attack;
    attack_data forward_attack;
    attack_data forward_strong_attack;
    attack_data up_strong_attack;
    attack_data down_strong_attack;
    float smash_charge_damage_bonus_f32;
    uint16_t smash_charge_max_ticks;
    uint16_t smash_charge_reserved;
    attack_data forward_aerial;
    attack_data back_aerial;
    attack_data up_aerial;
    attack_data down_aerial;
    attack_data ledge_attack;
    float reset_max_damage_f32;
    float reset_bound_speed_f32;
    float strong_hitbox_offset_x_f32;
    float strong_hitbox_offset_y_f32;
    float strong_hitbox_half_width_f32;
    float strong_hitbox_half_height_f32;
    float strong_damage_f32;
    float strong_base_knockback_x_f32;
    float strong_base_knockback_y_f32;
    float strong_knockback_growth_f32;
    float aerial_hitbox_offset_x_f32;
    float aerial_hitbox_offset_y_f32;
    float aerial_hitbox_half_width_f32;
    float aerial_hitbox_half_height_f32;
    float aerial_damage_f32;
    float aerial_base_knockback_x_f32;
    float aerial_base_knockback_y_f32;
    float aerial_knockback_growth_f32;
    float hitstun_velocity_per_tick_f32;
    float v_cancel_velocity_scale_f32;
    uint16_t knockback_weight;
    uint16_t knockback_reserved;
    float crouch_cancel_max_damage_f32;
    float crouch_cancel_velocity_scale_f32;
    float crouch_cancel_hitstun_scale_f32;
    int32_t di_max_angle_radians_q30;
    float ground_knockback_decay_scale_f32;
    float air_knockback_decay_f32;
    float sdi_distance_x_f32;
    float sdi_distance_y_f32;
    float asdi_distance_x_f32;
    float asdi_distance_y_f32;
    float shield_sdi_scale_f32;
    float tech_roll_speed_f32;
    float wall_tech_speed_f32;
    float wall_tech_jump_speed_x_f32;
    float wall_tech_jump_speed_y_f32;
    float wall_jump_speed_x_f32;
    float wall_jump_speed_y_f32;
    float ceiling_tech_speed_f32;
    float surface_collision_threshold_x_f32;
    float surface_collision_threshold_y_f32;
    float surface_bounce_multiplier_f32;
    float forward_roll_speed_f32;
    float backward_roll_speed_f32;
    float getup_attack_hitbox_offset_x_f32;
    float getup_attack_hitbox_offset_y_f32;
    float getup_attack_hitbox_half_width_f32;
    float getup_attack_hitbox_half_height_f32;
    float getup_attack_damage_f32;
    float getup_attack_base_knockback_x_f32;
    float getup_attack_base_knockback_y_f32;
    float getup_attack_knockback_growth_f32;
    float shield_health_f32;
    float shield_reset_health_f32;
    float shield_hold_depletion_f32;
    float light_shield_hold_depletion_f32;
    float shield_regeneration_f32;
    float light_shield_damage_multiplier_f32;
    float dense_shield_damage_multiplier_f32;
    float light_shield_stun_damage_multiplier_f32;
    float dense_shield_stun_damage_multiplier_f32;
    float shield_stun_base_f32;
    float shield_defender_pushback_stun_scale_f32;
    float shield_defender_pushback_normal_scale_f32;
    float shield_defender_pushback_max_f32;
    float shield_attacker_pushback_damage_f32;
    float shield_attacker_pushback_base_f32;
    float shield_attacker_pushback_air_decay_f32;
    float shield_attacker_pushback_ground_friction_scale_f32;
    float shield_radius_x_f32;
    float shield_radius_y_f32;
    float shield_minimum_size_scale_f32;
    float dense_shield_size_scale_f32;
    float shield_center_forward_f32;
    float shield_center_up_f32;
    float shield_animation_scale_x_f32;
    float shield_animation_scale_y_f32;
    float grabbox_offset_x_f32;
    float grabbox_offset_y_f32;
    float grabbox_half_width_f32;
    float grabbox_half_height_f32;
    float grabbed_offset_x_f32;
    float grabbed_offset_y_f32;
    float grab_escape_damage_ticks_f32;
    float pummel_damage_f32;
    uint16_t pummel_hit_tick;
    uint16_t pummel_total_ticks;
    struct throw_data forward_throw;
    struct throw_data back_throw;
    struct throw_data up_throw;
    struct throw_data down_throw;
    uint16_t jump_squat_ticks;
    uint16_t double_jump_cancel_ticks;
    uint16_t double_jump_armor_max_hitstun_ticks;
    uint16_t initial_dash_ticks;
    uint16_t dash_run_transition_ticks;
    uint16_t standing_turn_ticks;
    uint16_t standing_turn_facing_tick;
    uint16_t dash_input_window_ticks;
    uint16_t teeter_ticks;
    uint16_t teeter_turn_axis_threshold;
    uint16_t teeter_walk_axis_threshold;
    uint16_t walk_axis_threshold;
    uint16_t crouch_step_ticks;
    uint16_t taunt_ticks;
    uint16_t forward_smash_input_window_ticks;
    uint16_t landing_ticks;
    uint16_t landing_interruptible_tick;
    uint16_t platform_drop_ticks;
    uint16_t platform_drop_startup_ticks;
    uint16_t air_dodge_ticks;
    uint16_t air_dodge_invulnerability_begin_tick;
    uint16_t air_dodge_invulnerability_end_tick;
    uint16_t air_dodge_ordinary_physics_begin_tick;
    uint16_t ledge_invulnerability_ticks;
    uint16_t ledge_regrab_lockout_ticks;
    uint16_t ledge_transition_ticks;
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
    uint16_t tilt_axis_threshold;
    uint16_t tap_jump_axis_threshold;
    uint16_t tap_jump_input_window_ticks;
    uint16_t fast_fall_axis_threshold;
    uint16_t fast_fall_input_window_ticks;
    uint16_t air_dodge_dead_zone;
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
    uint16_t forward_aerial_landing_lag_ticks;
    uint16_t back_aerial_landing_lag_ticks;
    uint16_t up_aerial_landing_lag_ticks;
    uint16_t down_aerial_landing_lag_ticks;
    uint16_t strong_aerial_landing_lag_ticks;
    uint16_t l_cancel_window_ticks;
    uint16_t l_cancel_divisor;
    uint16_t v_cancel_window_ticks;
    uint16_t sdi_stick_threshold;
    uint16_t sdi_stick_window_ticks;
    uint16_t light_shield_trigger_threshold;
    uint16_t digital_trigger_threshold;
    uint16_t tumble_hitstun_threshold_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t tech_roll_axis_threshold;
    uint16_t tech_in_place_ticks;
    uint16_t tech_roll_ticks;
    uint16_t tech_invulnerability_ticks;
    uint16_t wall_tech_stall_ticks;
    uint16_t wall_tech_invulnerability_ticks;
    uint16_t wall_tech_ticks;
    uint16_t wall_tech_jump_ticks;
    uint16_t surface_bounce_invulnerability_ticks;
    uint16_t surface_bounce_collision_lockout_ticks;
    uint16_t wall_jump_ticks;
    uint16_t wall_jump_invulnerability_ticks;
    uint16_t ceiling_tech_control_tick;
    uint16_t ceiling_tech_ticks;
    uint16_t knockdown_ticks;
    uint16_t down_wait_ticks;
    float down_horizontal_angle_tan_f32;
    uint16_t down_up_axis_threshold;
    uint16_t down_horizontal_axis_threshold;
    uint16_t down_attack_input_window_ticks;
    uint16_t down_c_up_axis_threshold;
    uint16_t getup_neutral_ticks;
    uint16_t getup_neutral_invulnerability_ticks;
    uint16_t getup_roll_ticks;
    getup_roll_timing getup_roll_back_forward;
    getup_roll_timing getup_roll_back_backward;
    getup_roll_timing getup_roll_stomach_forward;
    getup_roll_timing getup_roll_stomach_backward;
    uint16_t getup_attack_ticks;
    uint16_t getup_attack_back_invulnerability_ticks;
    uint16_t getup_attack_stomach_invulnerability_ticks;
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
    uint16_t mash_stick_axis_threshold;
    uint16_t shield_break_stun_tick_decrement;
    uint16_t grab_startup_ticks;
    uint16_t grab_active_ticks;
    uint16_t grab_recovery_ticks;
    uint16_t dash_grab_startup_ticks;
    uint16_t dash_grab_active_ticks;
    uint16_t dash_grab_recovery_ticks;
    uint16_t grab_escape_base_ticks;
    uint16_t grab_escape_max_ticks;
    uint16_t grab_mash_reduction_ticks;
    uint16_t grab_escape_tick_decrement;
    uint16_t grab_release_ticks;
    float grab_release_speed_x_f32;
    float grab_release_air_speed_x_f32;
    float grab_release_air_speed_y_f32;
    uint8_t air_jump_count;
    uint8_t powershield_cancel_enabled;
    uint8_t wall_jump_enabled;
    uint8_t reserved2;
    float stale_move_slot_reduction_f32[
        PF_SIM_STALE_MOVE_QUEUE_CAPACITY];
    uint16_t crouch_start_ticks;
    uint16_t crouch_end_ticks;
    uint16_t crouch_release_axis_threshold;
} fighter_data;

typedef struct stage_data
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reference_collision_profile;
    float floor_left_f32;
    float floor_right_f32;
    float floor_y_f32;
    float platform_center_x_f32;
    float platform_y_f32;
    float platform_half_width_f32;
    float platform_motion_amplitude_f32;
    float solid_left_f32;
    float solid_right_f32;
    float solid_top_f32;
    float solid_bottom_f32;
    float blast_left_f32;
    float blast_right_f32;
    float blast_top_f32;
    float blast_bottom_f32;
    float spawn_spacing_f32;
    uint16_t platform_motion_period_ticks;
    uint16_t reference_spawn_line;
    float reference_spawn_x_f32;
    float revival_platform_start_y_f32;
    float revival_platform_end_y_f32;
    float revival_platform_half_width_f32;
    uint16_t revival_platform_descent_ticks;
    uint16_t revival_platform_hold_ticks;
    float upper_platform_center_x_f32;
    float upper_platform_y_f32;
    float upper_platform_half_width_f32;
} stage_data;

struct content
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
    uint8_t gameplay_ruleset;
    fighter_data fighter;
    stage_data stage;
    item_data item;
    projectile_data projectile;
    reflector_data reflector;
    charge_data charge;
    recovery_data recovery;
};

typedef struct hit_sphere_inspection
{
    float center_x_f32;
    float center_y_f32;
    float center_z_f32;
    float radius_f32;
    uint8_t effect_index;
    uint8_t hitbox_id;
    uint8_t group_id;
    uint8_t collision_state;
} hit_sphere_inspection;

typedef struct hurt_capsule_inspection
{
    float endpoint_a_x_f32;
    float endpoint_a_y_f32;
    float endpoint_a_z_f32;
    float endpoint_b_x_f32;
    float endpoint_b_y_f32;
    float endpoint_b_z_f32;
    float radius_f32;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
    uint8_t reserved;
} hurt_capsule_inspection;

typedef struct player_inspection
{
    float position_x_f32;
    float position_y_f32;
    float velocity_x_f32;
    float velocity_y_f32;
    float self_velocity_x_f32;
    float self_velocity_y_f32;
    float knockback_velocity_x_f32;
    float knockback_velocity_y_f32;
    float ground_knockback_velocity_f32;
    float shield_recoil_x_f32;
    float source_animation_frame_f32;
    float source_animation_rate_f32;
    float fall_animation_blend_f32;
    float ecb_bottom_y_from_origin_f32;
    uint16_t action_ticks;
    uint16_t source_submotion;
    uint16_t respawn_count;
    uint8_t action_state;
    uint8_t hitlag_resume_action;
    int8_t facing;
    int8_t dash_direction;
    int8_t previous_strong_direction;
    uint8_t grounded;
    uint8_t support;
    uint8_t air_jumps_remaining;
    uint8_t fast_fall;
    uint8_t short_hop_latched;
    uint8_t platform_drop_ticks;
    uint8_t fall_animation_target_switched;
    uint8_t active;
    uint8_t ledge;
    uint8_t tilt_x_age;
    uint64_t last_hit_tick;
    float damage_f32;
    uint32_t last_hit_sequence;
    float last_hit_damage_f32;
    float hitbox_left_f32;
    float hitbox_right_f32;
    float hitbox_top_f32;
    float hitbox_bottom_f32;
    hit_sphere_inspection
        hit_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    float grabbox_left_f32;
    float grabbox_right_f32;
    float grabbox_top_f32;
    float grabbox_bottom_f32;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint16_t tech_window_ticks;
    uint16_t tech_lockout_ticks;
    uint16_t shield_stun_ticks;
    uint8_t attack_hit_mask;
    uint8_t hitbox_active;
    uint8_t hit_sphere_count;
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
    float shield_health_f32;
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
    uint16_t shield_angle_turn;
    uint16_t shield_magnitude;
    int16_t shield_tilt_x;
    int16_t shield_tilt_y;
    float shield_left_f32;
    float shield_right_f32;
    float shield_top_f32;
    float shield_bottom_f32;
    uint8_t shield_active;
    uint8_t revival_platform_active;
    uint8_t reserved2[2];
    float revival_platform_left_f32;
    float revival_platform_right_f32;
    float revival_platform_y_f32;
    float stale_move_multiplier_f32;
    uint8_t stale_move_count;
    uint8_t attack_stale_registered;
    uint8_t stale_move_ids[PF_SIM_STALE_MOVE_QUEUE_CAPACITY];
    uint8_t prone_orientation;
    uint8_t reserved3[3];
    uint8_t hurt_capsule_count;
    uint8_t reserved4[3];
    hurt_capsule_inspection
        hurt_capsules[PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY];
} player_inspection;

typedef struct stage_inspection
{
    float floor_left_f32;
    float floor_right_f32;
    float floor_y_f32;
    float platform_left_f32;
    float platform_right_f32;
    float platform_y_f32;
    float solid_left_f32;
    float solid_right_f32;
    float solid_top_f32;
    float solid_bottom_f32;
    float left_ledge_x_f32;
    float right_ledge_x_f32;
    float ledge_y_f32;
    float blast_left_f32;
    float blast_right_f32;
    float blast_top_f32;
    float blast_bottom_f32;
    float revival_platform_start_y_f32;
    float revival_platform_end_y_f32;
    float revival_platform_half_width_f32;
    uint16_t revival_platform_descent_ticks;
    uint16_t revival_platform_hold_ticks;
    float upper_platform_left_f32;
    float upper_platform_right_f32;
    float upper_platform_y_f32;
} stage_inspection;

typedef struct item_inspection
{
    float position_x_f32;
    float position_y_f32;
    float velocity_x_f32;
    float velocity_y_f32;
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
    uint8_t stale_registered;
} item_inspection;

typedef struct projectile_inspection
{
    float position_x_f32;
    float position_y_f32;
    float velocity_x_f32;
    float velocity_y_f32;
    float hitbox_left_f32;
    float hitbox_right_f32;
    float hitbox_top_f32;
    float hitbox_bottom_f32;
    uint16_t lifetime_ticks;
    uint8_t enabled;
    uint8_t state;
    uint8_t owner;
    uint8_t hitbox_active;
} projectile_inspection;

struct inspection
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
    stage_inspection stage;
    item_inspection item;
    projectile_inspection projectile;
    player_inspection players[PF_SIM_MAX_PLAYERS];
};

pf_status default_content(struct content *out_content);

pf_status reference_stage_content(
    enum reference_stage stage,
    struct content *out_content);

pf_status reference_stage_geometry_line_count(
    enum reference_stage stage,
    uint16_t *out_line_count);

pf_status reference_stage_geometry_line(
    enum reference_stage stage,
    uint16_t line_index,
    reference_stage_line *out_line);

pf_status validate_content(const struct content *content);

pf_status make_content_view(
    const struct content *content,
    pf_content_view *out_view);

pf_status inspect(
    const pf_sim *sim,
    struct inspection *out_inspection);

/* Replace the generic reset pose with the exact two-player SSBM match-entry
 * lifecycle. This remains separate from pf_sim_reset so focused fixtures and
 * post-KO revival keep their existing contracts. */
pf_status start_reference_match(pf_sim *sim);

#ifdef __cplusplus
}
#endif

#endif
