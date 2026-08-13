#ifndef PF_SIM_FALCON_FRAME_DATA_H
#define PF_SIM_FALCON_FRAME_DATA_H

#include "sim_hsd_pose.h"

#include <stdint.h>

#define PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT UINT16_C(97)
#define PF_M4_FALCON_SUBMOTION_COUNT UINT16_C(318)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_HIGH_1 UINT16_C(165)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_NEUTRAL_1 UINT16_C(168)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_NEUTRAL_2 UINT16_C(169)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_LOW_1 UINT16_C(171)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_AIR_1 UINT16_C(174)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_HIGH UINT16_C(177)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_NEUTRAL UINT16_C(178)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_LOW UINT16_C(179)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_TOP UINT16_C(180)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL UINT16_C(181)
#define PF_M4_FALCON_SCRIPT_EVENT_COUNT UINT16_C(2056)
#define PF_M4_FALCON_SCRIPT_BYTE_COUNT UINT16_C(16516)
#define PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT UINT16_C(2536)
#define PF_M4_MELEE_STALE_MOVE_SLOT_COUNT UINT16_C(9)
#define PF_M4_FALCON_AIR_DODGE_ECB_FRAME_COUNT UINT16_C(48)
#define PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16 INT32_MIN
#define PF_M4_FALCON_PRONE_ORIENTATION_COUNT UINT16_C(2)
#define PF_M4_FALCON_GETUP_ROLL_DIRECTION_COUNT UINT16_C(2)
#define PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT UINT16_C(26)
#define PF_M4_FALCON_DOWN_WAIT_ECB_FRAME_COUNT UINT16_C(70)
#define PF_M4_FALCON_GETUP_NEUTRAL_ECB_FRAME_COUNT UINT16_C(30)
#define PF_M4_FALCON_GETUP_ATTACK_ECB_FRAME_COUNT UINT16_C(49)
#define PF_M4_FALCON_GETUP_ROLL_ECB_FRAME_COUNT UINT16_C(35)
#define PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT UINT16_C(158)
#define PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT UINT16_C(24)
#define PF_M4_FALCON_PLATFORM_DROP_ECB_FRAME_COUNT UINT16_C(30)
#define PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT UINT16_C(35)
#define PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT UINT16_C(50)
#define PF_M4_FALCON_JUMP_AERIAL_FORWARD_ECB_FRAME_COUNT UINT16_C(50)
#define PF_M4_FALCON_JUMP_AERIAL_BACKWARD_ECB_FRAME_COUNT UINT16_C(35)
#define PF_M4_FALCON_FALL_ECB_FRAME_COUNT UINT16_C(8)
#define PF_M4_FALCON_FALL_AERIAL_ECB_FRAME_COUNT UINT16_C(8)
#define PF_M4_FALCON_AIRBORNE_ECB_FRAME_COUNT UINT16_C(186)
#define PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT UINT16_C(44)
#define PF_M4_FALCON_FORWARD_AERIAL_ECB_FRAME_COUNT UINT16_C(39)
#define PF_M4_FALCON_BACK_AERIAL_ECB_FRAME_COUNT UINT16_C(35)
#define PF_M4_FALCON_UP_AERIAL_ECB_FRAME_COUNT UINT16_C(33)
#define PF_M4_FALCON_DOWN_AERIAL_ECB_FRAME_COUNT UINT16_C(44)
#define PF_M4_FALCON_SHIELD_BREAK_FLY_ECB_FRAME_COUNT UINT16_C(42)
#define PF_M4_FALCON_SHIELD_BREAK_DOWN_ECB_FRAME_COUNT UINT16_C(26)
#define PF_M4_FALCON_SHIELD_BREAK_STAND_ECB_FRAME_COUNT UINT16_C(30)
#define PF_M4_FALCON_SHIELD_BREAK_STUN_ECB_FRAME_COUNT UINT16_C(100)
#define PF_M4_FALCON_GUARD_ON_FRAME_COUNT UINT16_C(8)
#define PF_M4_FALCON_GUARD_FRAME_COUNT UINT16_C(1)
#define PF_M4_FALCON_GUARD_OFF_FRAME_COUNT UINT16_C(16)
#define PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT UINT16_C(9)
#define PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT UINT16_C(51)
#define PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST UINT16_C(219)
#define PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT UINT16_C(10)
#define PF_M4_FALCON_LEDGE_JUMP1_QUICK_FRAME_COUNT UINT16_C(11)
#define PF_M4_FALCON_LEDGE_JUMP1_SLOW_FRAME_COUNT UINT16_C(18)

typedef enum falcon_move_index
{
    PF_M4_FALCON_JAB1 = 0,
    PF_M4_FALCON_JAB2,
    PF_M4_FALCON_JAB3,
    PF_M4_FALCON_RAPID_JABS_START,
    PF_M4_FALCON_RAPID_JABS_LOOP,
    PF_M4_FALCON_RAPID_JABS_END,
    PF_M4_FALCON_DASH_ATTACK,
    PF_M4_FALCON_FORWARD_TILT_HIGH,
    PF_M4_FALCON_FORWARD_TILT_MID_HIGH,
    PF_M4_FALCON_FORWARD_TILT,
    PF_M4_FALCON_FORWARD_TILT_MID_LOW,
    PF_M4_FALCON_FORWARD_TILT_LOW,
    PF_M4_FALCON_UP_TILT,
    PF_M4_FALCON_DOWN_TILT,
    PF_M4_FALCON_FORWARD_SMASH_HIGH,
    PF_M4_FALCON_FORWARD_SMASH_MID_HIGH,
    PF_M4_FALCON_FORWARD_SMASH,
    PF_M4_FALCON_FORWARD_SMASH_MID_LOW,
    PF_M4_FALCON_FORWARD_SMASH_LOW,
    PF_M4_FALCON_UP_SMASH,
    PF_M4_FALCON_DOWN_SMASH,
    PF_M4_FALCON_NEUTRAL_AERIAL,
    PF_M4_FALCON_FORWARD_AERIAL,
    PF_M4_FALCON_BACK_AERIAL,
    PF_M4_FALCON_UP_AERIAL,
    PF_M4_FALCON_DOWN_AERIAL,
    PF_M4_FALCON_GRAB,
    PF_M4_FALCON_DASH_GRAB,
    PF_M4_FALCON_PUMMEL,
    PF_M4_FALCON_FORWARD_THROW,
    PF_M4_FALCON_BACK_THROW,
    PF_M4_FALCON_UP_THROW,
    PF_M4_FALCON_DOWN_THROW,
    PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND,
    PF_M4_FALCON_NEUTRAL_SPECIAL_AIR,
    PF_M4_FALCON_SIDE_SPECIAL_START_GROUND,
    PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND,
    PF_M4_FALCON_SIDE_SPECIAL_START_AIR,
    PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR,
    PF_M4_FALCON_UP_SPECIAL_GROUND,
    PF_M4_FALCON_UP_SPECIAL_AIR,
    PF_M4_FALCON_UP_SPECIAL_CATCH,
    PF_M4_FALCON_UP_SPECIAL_THROW,
    PF_M4_FALCON_DOWN_SPECIAL_GROUND,
    PF_M4_FALCON_DOWN_SPECIAL_END_GROUND,
    PF_M4_FALCON_DOWN_SPECIAL_AIR,
    PF_M4_FALCON_DOWN_SPECIAL_LANDING_HIT,
    PF_M4_FALCON_DOWN_SPECIAL_END_AIR_FROM_GROUND,
    PF_M4_FALCON_DOWN_SPECIAL_END_AIR,
    PF_M4_FALCON_DOWN_SPECIAL_WALL_REBOUND,
    PF_M4_FALCON_MOVE_COUNT
} falcon_move_index;

/*
 * PlCa.dat submotion indices used directly by the simulation. The complete
 * 318-entry catalog remains index-addressable, including intentional empty
 * animation slots; only semantic aliases needed by runtime code are named
 * here so the public enum surface does not duplicate the source name table.
 */
typedef enum falcon_submotion_index
{
    PF_M4_FALCON_SUBMOTION_WAIT = 2,
    PF_M4_FALCON_SUBMOTION_WAIT_2 = 3,
    PF_M4_FALCON_SUBMOTION_WAIT_3 = 4,
    PF_M4_FALCON_SUBMOTION_WALK_SLOW = 7,
    PF_M4_FALCON_SUBMOTION_WALK_MIDDLE = 8,
    PF_M4_FALCON_SUBMOTION_WALK_FAST = 9,
    PF_M4_FALCON_SUBMOTION_TURN = 10,
    PF_M4_FALCON_SUBMOTION_TURN_RUN = 11,
    PF_M4_FALCON_SUBMOTION_DASH = 12,
    PF_M4_FALCON_SUBMOTION_RUN = 13,
    PF_M4_FALCON_SUBMOTION_RUN_BRAKE = 14,
    PF_M4_FALCON_SUBMOTION_LANDING = 15,
    PF_M4_FALCON_SUBMOTION_JUMP_FORWARD = 16,
    PF_M4_FALCON_SUBMOTION_JUMP_BACKWARD = 17,
    PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD = 18,
    PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD = 19,
    PF_M4_FALCON_SUBMOTION_FALL = 20,
    PF_M4_FALCON_SUBMOTION_FALL_FORWARD = 21,
    PF_M4_FALCON_SUBMOTION_FALL_BACKWARD = 22,
    PF_M4_FALCON_SUBMOTION_FALL_AERIAL = 23,
    PF_M4_FALCON_SUBMOTION_FALL_AERIAL_FORWARD = 24,
    PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD = 25,
    PF_M4_FALCON_SUBMOTION_FALL_SPECIAL = 26,
    PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_FORWARD = 27,
    PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_BACKWARD = 28,
    PF_M4_FALCON_SUBMOTION_SQUAT = 30,
    PF_M4_FALCON_SUBMOTION_SQUAT_WAIT = 31,
    PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE = 34,
    PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL = 36,
    PF_M4_FALCON_SUBMOTION_GUARD_ON = 37,
    PF_M4_FALCON_SUBMOTION_GUARD = 38,
    PF_M4_FALCON_SUBMOTION_GUARD_OFF = 39,
    PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF = 40,
    PF_M4_FALCON_SUBMOTION_SPOT_DODGE = 41,
    PF_M4_FALCON_SUBMOTION_ROLL_FORWARD = 42,
    PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD = 43,
    PF_M4_FALCON_SUBMOTION_AIR_DODGE = 44,
    PF_M4_FALCON_SUBMOTION_REBOUND = 45,
    PF_M4_FALCON_SUBMOTION_DOWN_BOUND_BACK = 183,
    PF_M4_FALCON_SUBMOTION_DOWN_WAIT_BACK = 184,
    PF_M4_FALCON_SUBMOTION_GETUP_NEUTRAL_BACK = 186,
    PF_M4_FALCON_SUBMOTION_GETUP_ATTACK_BACK = 187,
    PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_BACK = 188,
    PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_BACK = 189,
    PF_M4_FALCON_SUBMOTION_DOWN_BOUND_STOMACH = 191,
    PF_M4_FALCON_SUBMOTION_DOWN_WAIT_STOMACH = 192,
    PF_M4_FALCON_SUBMOTION_GETUP_NEUTRAL_STOMACH = 194,
    PF_M4_FALCON_SUBMOTION_GETUP_ATTACK_STOMACH = 195,
    PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_STOMACH = 196,
    PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_STOMACH = 197,
    PF_M4_FALCON_SUBMOTION_TECH_IN_PLACE = 199,
    PF_M4_FALCON_SUBMOTION_TECH_ROLL_FORWARD = 200,
    PF_M4_FALCON_SUBMOTION_TECH_ROLL_BACKWARD = 201,
    PF_M4_FALCON_SUBMOTION_WALL_TECH = 202,
    PF_M4_FALCON_SUBMOTION_WALL_TECH_JUMP = 203,
    PF_M4_FALCON_SUBMOTION_CEILING_TECH = 204,
    PF_M4_FALCON_SUBMOTION_FURAFURA = 205,
    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_FLY = 286,
    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_FALL = 287,
    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_UP = 288,
    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN = 289,
    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_UP = 290,
    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN = 291,
    PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_GROUND = 303,
    PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_GROUND = 304,
    PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_AIR = 305,
    PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_AIR = 306,
    PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND = 307,
    PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR = 308,
    PF_M4_FALCON_SUBMOTION_FALCON_DIVE_CATCH = 309,
    PF_M4_FALCON_SUBMOTION_FALCON_DIVE_THROW = 310,
    PF_M4_FALCON_SUBMOTION_CATCH = 242,
    PF_M4_FALCON_SUBMOTION_CATCH_DASH = 243,
    PF_M4_FALCON_SUBMOTION_CATCH_WAIT = 244,
    PF_M4_FALCON_SUBMOTION_CATCH_ATTACK = 245,
    PF_M4_FALCON_SUBMOTION_CATCH_CUT = 246,
    PF_M4_FALCON_SUBMOTION_THROW_FORWARD = 247,
    PF_M4_FALCON_SUBMOTION_THROW_BACK = 248,
    PF_M4_FALCON_SUBMOTION_THROW_UP = 249,
    PF_M4_FALCON_SUBMOTION_THROW_DOWN = 250,
    PF_M4_FALCON_SUBMOTION_CAPTURE_PULLED_LOW = 254,
    PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_LOW = 255,
    PF_M4_FALCON_SUBMOTION_THROWN_FORWARD = 262,
    PF_M4_FALCON_SUBMOTION_THROWN_BACK = 263,
    PF_M4_FALCON_SUBMOTION_THROWN_UP = 264,
    PF_M4_FALCON_SUBMOTION_THROWN_DOWN = 265,
    PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_HIGH = 252,
    PF_M4_FALCON_SUBMOTION_CAPTURE_DAMAGE_HIGH = 253,
    PF_M4_FALCON_SUBMOTION_CAPTURE_CUT = 257,
    PF_M4_FALCON_SUBMOTION_LEDGE_CATCH = 216,
    PF_M4_FALCON_SUBMOTION_LEDGE_WAIT = 217,
    PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_SLOW = 219,
    PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_QUICK = 220,
    PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW = 221,
    PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK = 222,
    PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_SLOW = 223,
    PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_QUICK = 224,
    PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_1 = 225,
    PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_2 = 226,
    PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_1 = 227,
    PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2 = 228,
    PF_M4_FALCON_SUBMOTION_ENTRY_START = 238,
    PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT = 239,
    PF_M4_FALCON_SUBMOTION_APPEAL_LEFT = 240,
    PF_M4_FALCON_SUBMOTION_PLATFORM_DROP = 209,
    PF_M4_FALCON_SUBMOTION_TEETER = 210,
    PF_M4_FALCON_SUBMOTION_TEETER_WAIT = 211
} falcon_submotion_index;

/* Ordinary motion changes use Falcon's six-frame default HSD blend. A Wait
 * pose is source-direct only after this displayed-frame boundary. */
#define PF_M4_FALCON_WAIT_HSD_FIRST_UNBLENDED_FRAME UINT16_C(5)

static inline int falcon_wait_hsd_pose_is_direct(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16)
{
    return source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT_2 ||
           source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT_3 ||
           (source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT &&
            source_animation_frame_q16 >=
                (int32_t)PF_M4_FALCON_WAIT_HSD_FIRST_UNBLENDED_FRAME *
                    INT32_C(65536));
}

typedef enum falcon_capture_hurt_index
{
    PF_M4_FALCON_CAPTURE_HURT_WAIT_HIGH = 0,
    PF_M4_FALCON_CAPTURE_HURT_DAMAGE_HIGH = 1,
    PF_M4_FALCON_CAPTURE_HURT_COUNT = 2
} falcon_capture_hurt_index;

/* Qualified common-state hurt-pose tracks. Keep this compact index separate
 * from public action-state values so additional captured common motions append
 * without widening snapshots or coupling generated data to the API enum. */
typedef enum falcon_common_hurt_index
{
    PF_M4_FALCON_COMMON_HURT_INITIAL_DASH = 0,
    PF_M4_FALCON_COMMON_HURT_RUN_BRAKE = 1,
    PF_M4_FALCON_COMMON_HURT_CROUCH_START = 2,
    PF_M4_FALCON_COMMON_HURT_CROUCH_END = 3,
    PF_M4_FALCON_COMMON_HURT_KNEE_BEND = 4,
    PF_M4_FALCON_COMMON_HURT_SPOT_DODGE = 5,
    PF_M4_FALCON_COMMON_HURT_ROLL_FORWARD = 6,
    PF_M4_FALCON_COMMON_HURT_ROLL_BACKWARD = 7,
    PF_M4_FALCON_COMMON_HURT_AIR_DODGE = 8,
    PF_M4_FALCON_COMMON_HURT_FALL_SPECIAL = 9,
    PF_M4_FALCON_COMMON_HURT_LANDING_FALL_SPECIAL = 10,
    PF_M4_FALCON_COMMON_HURT_LANDING = 11,
    PF_M4_FALCON_COMMON_HURT_COUNT = 12
} falcon_common_hurt_index;

enum
{
    PF_M4_FALCON_LEDGE_HURT_TRACK_COUNT = 10
};

enum
{
    PF_M4_FALCON_GUARD_HURT_TRACK_COUNT = 3
};

typedef struct falcon_submotion_data
{
    /* Raw FigaTree endpoint count and extractor-compatible last frame. */
    uint16_t animation_frame_count;
    uint16_t gameplay_frame_count;
    uint16_t event_count;
    uint16_t event_offset;
    uint16_t translation_offset;
    uint16_t translation_count;
    uint32_t animation_flags;
    uint32_t animation_size;
} falcon_submotion_data;

typedef struct falcon_script_event
{
    uint16_t byte_offset;
    uint8_t byte_count;
    uint8_t command_id;
} falcon_script_event;

typedef struct falcon_animation_decode_summary
{
    uint32_t node_count;
    uint32_t track_count;
    uint32_t key_count;
} falcon_animation_decode_summary;

typedef struct falcon_body_collision_timing
{
    /* Raw action-script frames; UINT16_MAX means the command is absent. */
    uint16_t state_two_frame;
    uint16_t state_zero_frame;
} falcon_body_collision_timing;

typedef enum reference_hit_element
{
    PF_M4_REFERENCE_HIT_EMPTY = 0,
    PF_M4_REFERENCE_HIT_NORMAL = 1,
    PF_M4_REFERENCE_HIT_FIRE = 2,
    PF_M4_REFERENCE_HIT_ELECTRIC = 3,
    PF_M4_REFERENCE_HIT_GRAB = 4
} reference_hit_element;

typedef struct reference_hit_effect
{
    uint16_t angle_degrees;
    uint16_t growth;
    uint16_t weight_set;
    uint16_t base;
    uint8_t damage;
    uint8_t shield_damage;
    uint8_t interaction;
    uint8_t element;
    uint8_t hits_grounded;
    uint8_t hits_airborne;
    uint8_t reserved[2];
} reference_hit_effect;

typedef struct reference_hit_phase
{
    uint16_t first_frame;
    uint16_t last_frame;
    uint16_t effect_mask;
    uint16_t reserved;
} reference_hit_phase;

typedef struct reference_geometry_move
{
    uint16_t frame_offset;
    uint8_t first_frame;
    uint8_t frame_count;
} reference_geometry_move;

typedef struct reference_hit_frame
{
    uint16_t sphere_offset;
    uint8_t sphere_count;
    uint8_t reserved;
} reference_hit_frame;

typedef struct reference_hit_sphere
{
    int32_t offset_x_q16;
    int32_t offset_y_q16;
    int32_t offset_z_q16;
    int32_t radius_q16;
    uint8_t effect_index;
    uint8_t hitbox_id;
    uint8_t group_id;
    uint8_t collision_state;
} reference_hit_sphere;

typedef struct reference_hurt_capsule
{
    int32_t endpoint_a_x_q16;
    int32_t endpoint_a_y_q16;
    int32_t endpoint_a_z_q16;
    int32_t endpoint_b_x_q16;
    int32_t endpoint_b_y_q16;
    int32_t endpoint_b_z_q16;
    int32_t radius_q16;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
    uint8_t reserved;
} reference_hurt_capsule;

typedef struct reference_hurt_move
{
    uint16_t frame_offset;
    uint8_t first_frame;
    uint8_t frame_count;
} reference_hurt_move;

typedef struct reference_hurt_frame
{
    uint16_t capsule_offset;
    uint8_t capsule_count;
    uint8_t reserved;
} reference_hurt_frame;

/*
 * Deterministic, unit-converted view of the gameplay fields in Falcon's
 * complete 97-word ftCo_DatAttrs block. The generated raw-word table remains
 * authoritative for fields that are opaque or irrelevant to this simulator.
 */
typedef struct falcon_common_attributes
{
    int32_t initial_walk_velocity_q16;
    int32_t walk_acceleration_q16;
    int32_t walk_maximum_velocity_q16;
    int32_t slow_walk_animation_scaling_q16;
    int32_t middle_walk_animation_scaling_q16;
    int32_t fast_walk_animation_scaling_q16;
    int32_t run_animation_scaling_q16;
    int32_t friction_q16;
    int64_t friction_q32;
    int32_t dash_initial_velocity_q16;
    int32_t dash_run_acceleration_a_q16;
    int32_t dash_run_acceleration_b_q16;
    int32_t dash_run_terminal_velocity_q16;
    int32_t ground_maximum_horizontal_velocity_q16;
    int32_t jump_horizontal_initial_velocity_q16;
    int32_t jump_vertical_initial_velocity_q16;
    int32_t ground_air_jump_momentum_multiplier_q16;
    int32_t jump_horizontal_maximum_velocity_q16;
    int32_t shorthop_vertical_initial_velocity_q16;
    int32_t air_jump_multiplier_q16;
    int32_t double_jump_momentum_q16;
    int32_t double_jump_vertical_velocity_q16;
    int32_t double_jump_horizontal_velocity_q16;
    int32_t gravity_q16;
    int32_t terminal_velocity_q16;
    int32_t air_mobility_a_q16;
    int32_t air_mobility_b_q16;
    int64_t air_mobility_a_q32;
    int64_t air_mobility_b_q32;
    int32_t max_aerial_horizontal_velocity_q16;
    int32_t air_friction_q16;
    int32_t fast_fall_terminal_velocity_q16;
    int32_t maximum_horizontal_air_velocity_q16;
    int32_t shield_break_initial_velocity_q16;
    int32_t rebound_animation_length_q16;
    int32_t ledge_jump_horizontal_velocity_q16;
    int32_t ledge_jump_vertical_velocity_q16;
    int32_t wall_jump_horizontal_velocity_q16;
    int32_t wall_jump_vertical_velocity_q16;
    int32_t match_entry_rise_q16;
    uint16_t jump_startup_ticks;
    uint16_t number_of_jumps;
    uint16_t turn_duration_ticks;
    uint16_t weight;
    uint16_t jab_2_input_window_ticks;
    uint16_t jab_3_input_window_ticks;
    uint16_t rapid_jab_input_count;
    uint16_t jab_1_combo_enable_frame;
    uint16_t jab_2_combo_enable_frame;
    uint16_t jab_3_rapid_enable_frame;
    uint16_t rapid_jab_first_decision_frame;
    uint16_t rapid_jab_decision_interval;
    uint16_t rapid_jab_last_decision_frame;
    uint16_t rapid_jab_loop_frame_count;
    uint16_t down_tilt_repeat_enable_frame;
    uint8_t weight_independent_throws_mask;
    uint8_t reserved;
    uint16_t normal_landing_lag_ticks;
    uint16_t neutral_aerial_landing_lag_ticks;
    uint16_t forward_aerial_landing_lag_ticks;
    uint16_t back_aerial_landing_lag_ticks;
    uint16_t up_aerial_landing_lag_ticks;
    uint16_t down_aerial_landing_lag_ticks;
} falcon_common_attributes;

typedef struct falcon_ledge_attributes
{
    int32_t snap_x_q16;
    int32_t snap_y_q16;
    int32_t snap_height_q16;
} falcon_ledge_attributes;

typedef struct falcon_ledge_root_positions
{
    int32_t catch_frame_one_x_q16;
    int32_t catch_frame_one_y_q16;
    int32_t wait_frame_one_x_q16;
    int32_t wait_frame_one_y_q16;
    int32_t option_frame_one_x_q16[
        PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT];
    int32_t option_frame_one_y_q16[
        PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT];
    uint16_t option_ground_frame[
        PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT];
} falcon_ledge_root_positions;

typedef struct falcon_ledge_attack_reference
{
    uint16_t total_frames;
    uint16_t first_active_frame;
    uint16_t last_active_frame;
    uint16_t reserved;
    reference_hit_effect effect;
} falcon_ledge_attack_reference;

/*
 * Exact deterministic view of the 0x8c-byte ftCaptain_DatAttrs block. Float
 * members are converted from the owner DAT's big-endian IEEE-754 words to
 * Q16.16 by the pinned importer. The raw source words remain in the generated
 * table as a completeness/provenance surface.
 */
typedef struct falcon_special_attributes
{
    int32_t specialn_stick_range_y_neg_q16;
    int32_t specialn_stick_range_y_pos_q16;
    int32_t specialn_angle_diff_q16;
    int32_t specialn_vel_x_q16;
    int32_t specialn_vel_mul_q16;
    int32_t specials_gr_vel_x_q16;
    int32_t specials_grav_q16;
    int32_t specials_terminal_vel_q16;
    int32_t specials_unk0_q16;
    int32_t specials_unk1_q16;
    int32_t specials_unk2_q16;
    int32_t specials_unk3_q16;
    int32_t specials_unk4_q16;
    int32_t specials_unk5_q16;
    int32_t specials_miss_landing_lag_q16;
    int32_t specials_hit_landing_lag_q16;
    int32_t specialhi_air_friction_mul_q16;
    int32_t specialhi_horz_vel_q16;
    int32_t specialhi_freefall_air_spd_mul_q16;
    int32_t specialhi_landing_lag_q16;
    int32_t specialhi_unk0_q16;
    int32_t specialhi_unk1_q16;
    int32_t specialhi_input_var_q16;
    int32_t specialhi_unk2_q16;
    int32_t specialhi_catch_grav_q16;
    int32_t specialhi_air_var;
    uint32_t x68_bits;
    uint32_t speciallw_unk1;
    int32_t speciallw_flame_particle_angle_q16;
    int32_t speciallw_on_hit_spd_modifier_q16;
    int32_t speciallw_unk2;
    int32_t speciallw_ground_lag_mul_q16;
    int32_t speciallw_landing_lag_mul_q16;
    int32_t speciallw_ground_traction_q16;
    int32_t speciallw_air_landing_traction_q16;
} falcon_special_attributes;

typedef struct falcon_common_special_attributes
{
    int32_t fast_ground_friction_multiplier_q16;
    int32_t air_drift_over_maximum_deceleration_q16;
    int32_t side_special_stick_threshold_q16;
    int32_t side_special_turn_threshold_q16;
    int32_t air_drift_dead_zone_q16;
} falcon_common_special_attributes;

typedef struct falcon_air_dodge_attributes
{
    int32_t initial_velocity_x_q16;
    int32_t initial_velocity_y_q16;
    int32_t decay_q16;
    uint16_t dead_zone;
    uint16_t item_throw_window_ticks;
    /* Raw EscapeAir displayed frame from command-variable zero becoming one. */
    uint16_t ordinary_physics_begin_frame;
    uint16_t reserved;
} falcon_air_dodge_attributes;

/* Raw parameters of Falcon's shared action-script Smash Charge command.
 * damage_multiplier_q8 is the command's 16-bit rate interpreted over 256,
 * exactly as ftAction_80073008 does before ftCo_800DEEB8 applies it. */
typedef struct falcon_smash_charge_attributes
{
    uint16_t max_charge_ticks;
    uint16_t damage_multiplier_q8;
} falcon_smash_charge_attributes;

typedef struct falcon_neutral_special_timing
{
    uint16_t launch_frame;
    uint16_t velocity_scale_begin_frame;
    uint16_t velocity_scale_end_frame;
    uint16_t ordinary_air_physics_begin_frame;
} falcon_neutral_special_timing;

typedef struct falcon_side_special_timing
{
    uint16_t ground_search_begin_frame;
    uint16_t ground_search_end_frame;
    uint16_t air_search_begin_frame;
    uint16_t air_search_end_frame;
    uint16_t air_gravity_begin_frame;
} falcon_side_special_timing;

typedef struct falcon_up_special_timing
{
    uint16_t air_control_begin_frame;
    uint16_t throw_gravity_begin_frame;
    uint16_t victim_release_hitstun_ticks;
    uint16_t reserved;
    int32_t grounded_throw_reposition_x_q16;
    int32_t grounded_throw_reposition_y_q16;
} falcon_up_special_timing;

typedef struct melee_stale_move_data
{
    uint16_t slot_reduction_q16[PF_M4_MELEE_STALE_MOVE_SLOT_COUNT];
} melee_stale_move_data;

typedef struct falcon_down_special_timing
{
    uint16_t ground_wall_rebound_begin_frame;
    uint16_t air_wall_rebound_begin_frame;
    uint16_t ground_end_traction_begin_frame;
    uint16_t ground_end_traction_end_frame;
    uint16_t ground_end_edge_fall_begin_frame;
    uint16_t landing_traction_begin_frame;
    uint16_t landing_traction_end_frame;
    uint16_t ground_origin_air_physics_begin_frame;
    uint16_t ground_origin_edge_fall_begin_frame;
    uint16_t reserved;
    int32_t ground_end_entry_velocity_scale_q16;
} falcon_down_special_timing;

typedef struct falcon_ecb_pose_q16
{
    int32_t top_x_from_origin_q16;
    int32_t top_y_from_origin_q16;
    int32_t bottom_x_from_origin_q16;
    int32_t bottom_y_from_origin_q16;
    int32_t right_x_from_origin_q16;
    int32_t right_y_from_origin_q16;
    int32_t left_x_from_origin_q16;
    int32_t left_y_from_origin_q16;
} falcon_ecb_pose_q16;

typedef struct falcon_collision_pose
{
    int32_t falling_bottom_y_from_origin_q16;
    falcon_ecb_pose_q16 crouch_wait[
        PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT];
    uint32_t down_bound_floor_contact_mask[
        PF_M4_FALCON_PRONE_ORIENTATION_COUNT];
    falcon_ecb_pose_q16 down_bound[
        PF_M4_FALCON_PRONE_ORIENTATION_COUNT]
        [PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 down_wait[
        PF_M4_FALCON_PRONE_ORIENTATION_COUNT]
        [PF_M4_FALCON_DOWN_WAIT_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 getup_neutral[
        PF_M4_FALCON_PRONE_ORIENTATION_COUNT]
        [PF_M4_FALCON_GETUP_NEUTRAL_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 getup_attack[
        PF_M4_FALCON_PRONE_ORIENTATION_COUNT]
        [PF_M4_FALCON_GETUP_ATTACK_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 getup_roll[
        PF_M4_FALCON_PRONE_ORIENTATION_COUNT]
        [PF_M4_FALCON_GETUP_ROLL_DIRECTION_COUNT]
        [PF_M4_FALCON_GETUP_ROLL_ECB_FRAME_COUNT];
    int32_t damage_fly_bottom_y_from_origin_q16[
        PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT];
    int32_t damage_fly_top_y_from_origin_q16[
        PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT];
    int32_t damage_fly_side_x_from_origin_q16[
        PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT];
    int32_t damage_fly_side_y_from_origin_q16[
        PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT];
    int32_t air_dodge_bottom_y_from_origin_q16[
        PF_M4_FALCON_AIR_DODGE_ECB_FRAME_COUNT];
    int32_t platform_drop_bottom_y_from_origin_q16[
        PF_M4_FALCON_PLATFORM_DROP_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 airborne[
        PF_M4_FALCON_AIRBORNE_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 shield_break_fly[
        PF_M4_FALCON_SHIELD_BREAK_FLY_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 shield_break_down_down[
        PF_M4_FALCON_SHIELD_BREAK_DOWN_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 shield_break_stand_down[
        PF_M4_FALCON_SHIELD_BREAK_STAND_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 shield_break_stun[
        PF_M4_FALCON_SHIELD_BREAK_STUN_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 guard_on[
        PF_M4_FALCON_GUARD_ON_FRAME_COUNT];
    falcon_ecb_pose_q16 guard[
        PF_M4_FALCON_GUARD_FRAME_COUNT];
    falcon_ecb_pose_q16 guard_off[
        PF_M4_FALCON_GUARD_OFF_FRAME_COUNT];
    falcon_ecb_pose_q16 ceiling_bounce[
        PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT];
    falcon_ecb_pose_q16 wall_bounce[
        PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT];
} falcon_collision_pose;

typedef struct reference_search_sphere
{
    int32_t offset_x_q16;
    int32_t offset_y_q16;
    int32_t offset_z_q16;
    int32_t radius_q16;
} reference_search_sphere;

typedef struct reference_throw
{
    uint16_t angle_degrees;
    uint16_t growth;
    uint16_t weight_set;
    uint16_t base;
    uint8_t damage;
    uint8_t element;
    uint16_t release_frame;
    uint16_t reserved;
} reference_throw;

struct reference_move
{
    uint16_t subaction_index;
    uint16_t total_frames;
    uint16_t iasa_frame;
    uint16_t charge_frame;
    uint16_t autocancel_before;
    uint16_t autocancel_after;
    uint16_t landing_lag;
    uint16_t l_cancelled_landing_lag;
    uint16_t phase_offset;
    uint16_t effect_offset;
    uint16_t throw_index;
    uint8_t phase_count;
    uint8_t effect_count;
    uint8_t present;
    uint8_t reserved;
    uint32_t animation_flags;
    uint16_t motion_offset;
    uint16_t motion_count;
};

typedef struct reference_timing
{
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
} reference_timing;

typedef enum reference_iasa_policy
{
    PF_M4_REFERENCE_IASA_NONE = 0,
    PF_M4_REFERENCE_IASA_JAB_CHAIN,
    PF_M4_REFERENCE_IASA_WAIT,
    PF_M4_REFERENCE_IASA_DOWN_TILT,
    PF_M4_REFERENCE_IASA_FORWARD_SMASH
} reference_iasa_policy;

typedef enum reference_ground_physics
{
    PF_M4_REFERENCE_GROUND_PHYSICS_FRICTION = 0,
    PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION
} reference_ground_physics;

const uint8_t *falcon_reference_source_sha256(void);

const uint8_t *falcon_reference_complete_source_sha256(void);

const uint8_t *falcon_reference_submotion_catalog_sha256(void);

const uint8_t *falcon_reference_action_script_sha256(void);

const uint8_t *falcon_reference_animation_tracks_sha256(void);

const falcon_animation_decode_summary *
falcon_reference_animation_decode_summary(void);

const falcon_submotion_data *falcon_reference_submotion(
    uint16_t submotion_index);
int falcon_reference_damage_submotion(
    uint8_t source_grounded,
    uint8_t damage_level,
    uint8_t hurtbox_height,
    uint16_t *out_submotion_index);

const falcon_script_event *falcon_reference_submotion_event(
    uint16_t submotion_index,
    uint16_t event_index,
    const uint8_t **out_bytes);

const falcon_body_collision_timing *
falcon_reference_body_collision_timing(uint16_t submotion_index);

const uint32_t *falcon_reference_common_attribute_bits(
    uint16_t *out_count);

const falcon_common_attributes *
falcon_reference_common_attributes(void);

const falcon_ledge_attributes *
falcon_reference_ledge_attributes(void);

const falcon_ledge_root_positions *
falcon_reference_ledge_root_positions(void);

const falcon_ledge_attack_reference *
falcon_reference_ledge_attack(uint16_t submotion_index);

int falcon_reference_ledge_option_anchor_q16(
    uint16_t submotion_index,
    int32_t *out_x_q16,
    int32_t *out_y_q16);

uint16_t falcon_reference_ledge_option_ground_frame(
    uint16_t submotion_index);

int falcon_reference_hyrule_ledge_jump_position_q16(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    int32_t *out_x_from_wait_q16,
    int32_t *out_y_from_wait_q16);

int falcon_reference_body_invulnerable(
    uint16_t submotion_index,
    uint16_t action_ticks);

const falcon_special_attributes *
falcon_reference_special_attributes(void);

const falcon_common_special_attributes *
falcon_reference_common_special_attributes(void);

const falcon_air_dodge_attributes *
falcon_reference_air_dodge_attributes(void);

const melee_stale_move_data *
falcon_reference_stale_move_data(void);

const falcon_smash_charge_attributes *
falcon_reference_smash_charge_attributes(void);

const falcon_neutral_special_timing *
falcon_reference_neutral_special_timing(void);

const falcon_side_special_timing *
falcon_reference_side_special_timing(void);

const falcon_up_special_timing *
falcon_reference_up_special_timing(void);

const falcon_down_special_timing *
falcon_reference_down_special_timing(void);

const falcon_collision_pose *
falcon_reference_collision_pose(void);

const falcon_ecb_pose_q16 *
falcon_reference_prone_ecb_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing);

const falcon_ecb_pose_q16 *
falcon_reference_guard_ecb_pose(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks);

int falcon_reference_ecb_apply_bottom_lock_q16(
    int32_t locked_bottom_y_q16,
    falcon_ecb_pose_q16 *pose);

int
falcon_reference_hsd_ecb_pose(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int grounded,
    int32_t locked_bottom_y_q16,
    falcon_ecb_pose_q16 *out_pose);

int falcon_reference_action_hsd_ecb_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    int32_t locked_bottom_y_q16,
    falcon_ecb_pose_q16 *out_pose);

int falcon_reference_action_hsd_source(
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t *out_submotion,
    int32_t *out_frame_q16);

const hsd_pose_data *
falcon_reference_hsd_pose_data(void);
const hsd_local_pose *
falcon_reference_guard_target_hsd_pose(void);
int falcon_resolve_compact_hsd_pose(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int32_t progress_q16,
    const hsd_compact_pose *compact,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS]);

const hsd_wait_animation *
falcon_reference_wait_animations(uint8_t *out_count);

const hsd_wait_animation *
falcon_reference_wait_animation(uint16_t source_submotion);

int falcon_reference_direct_hsd_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    uint16_t *out_submotion,
    int32_t *out_frame_q16);

int falcon_reference_hsd_ground_ecb_pose_from_local_pose(
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    falcon_ecb_pose_q16 *out_pose);

int falcon_reference_hsd_fall_ecb_pose(
    uint16_t directional_submotion,
    int32_t source_animation_frame_q16,
    int32_t directional_blend_q16,
    uint8_t directional_target_switched,
    int32_t locked_bottom_y_q16,
    falcon_ecb_pose_q16 *out_pose);

const falcon_ecb_pose_q16 *
falcon_reference_airborne_ecb_pose(
    uint16_t source_submotion,
    uint16_t action_ticks);

const reference_search_sphere *
falcon_reference_side_special_search_spheres(
    int airborne,
    uint8_t *out_count);

const uint8_t *falcon_reference_geometry_sha256(void);

void falcon_reference_capture_offset_q16(
    int32_t *out_x_q16,
    int32_t *out_y_q16);

int falcon_reference_capture_constraint_q16(
    uint16_t holder_submotion,
    int32_t holder_frame_q16,
    int8_t holder_facing,
    uint16_t victim_submotion,
    int32_t victim_frame_q16,
    int8_t victim_facing,
    int32_t *out_x_q16,
    int32_t *out_y_q16);

int falcon_reference_collision_sweep_step_count_q16(
    int32_t position_delta_x_q16,
    int32_t position_delta_y_q16,
    const falcon_ecb_pose_q16 *current_ecb,
    const falcon_ecb_pose_q16 *desired_ecb,
    uint16_t *out_step_count);

int falcon_reference_throw_motions(
    uint8_t action_state,
    uint16_t *out_holder_submotion,
    uint16_t *out_victim_submotion,
    int32_t *out_animation_rate_q16);

const struct reference_move *falcon_reference_move(
    falcon_move_index move_index);

const reference_hit_phase *falcon_reference_phase(
    falcon_move_index move_index,
    uint16_t phase_index);

const reference_hit_effect *falcon_reference_effect(
    falcon_move_index move_index,
    uint16_t effect_index);

const reference_hit_effect *falcon_reference_primary_effect(
    falcon_move_index move_index);

const reference_hit_phase *falcon_reference_phase_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame);
uint16_t falcon_reference_effective_hit_frame(
    falcon_move_index move_index,
    uint16_t action_frame);

const reference_hit_effect *falcon_reference_effect_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame);

const reference_hit_sphere *
falcon_reference_hit_spheres_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_sphere_count);

int falcon_reference_has_hit_geometry(
    falcon_move_index move_index);

const reference_hurt_capsule *
falcon_reference_standing_hurt_capsules(uint8_t *out_count);

const reference_hurt_capsule *
falcon_reference_hurt_capsules_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_count);

const reference_hurt_capsule *
falcon_reference_common_hurt_capsules_at_frame(
    uint8_t action_state,
    uint16_t action_frame,
    uint8_t *out_count);

const reference_hurt_capsule *
falcon_reference_common_hurt_capsules_for_submotion_at_frame(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_frame,
    uint8_t *out_count);

int falcon_reference_hsd_hurt_capsules(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

int falcon_reference_hsd_hurt_capsules_from_local_pose(
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

int falcon_reference_damage_hsd_hurt_capsules(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int8_t facing,
    int32_t total_velocity_x_q16,
    int32_t total_velocity_y_q16,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

int falcon_reference_damage_hsd_ecb_pose(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int8_t facing,
    int32_t total_velocity_x_q16,
    int32_t total_velocity_y_q16,
    int grounded,
    int32_t locked_bottom_y_q16,
    falcon_ecb_pose_q16 *out_pose);

int falcon_reference_retained_hsd_pose(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks,
    int32_t source_animation_frame_q16,
    int32_t *out_frame_q16);

int falcon_reference_retained_hsd_hurt_capsules(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks,
    int32_t source_animation_frame_q16,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

uint16_t falcon_reference_shield_break_down_submotion(void);

int falcon_reference_move_for_action(
    uint8_t action_state,
    falcon_move_index *out_move_index);

const reference_throw *falcon_reference_throw(
    falcon_move_index move_index);

reference_timing falcon_reference_timing(
    falcon_move_index move_index);

const struct reference_move *falcon_reference_attack(
    uint8_t action_state,
    reference_timing authored_timing,
    uint32_t authored_damage_q16);

int falcon_reference_attack_matches(
    uint8_t action_state,
    reference_timing authored_timing,
    uint32_t authored_damage_q16);

reference_iasa_policy falcon_reference_iasa_policy_for_action(
    uint8_t action_state);

reference_ground_physics
falcon_reference_ground_physics_for_action(uint8_t action_state);

int falcon_reference_iasa_active(
    uint8_t action_state,
    uint32_t displayed_frame);

int falcon_reference_special_iasa_active(
    uint8_t action_state,
    uint16_t action_ticks);

/*
 * Returns the source animation TransN delta for a one-based displayed frame.
 * This accessor deliberately does not decide whether a state callback replaces
 * velocity, adds displacement, or ignores the track.
 */
int falcon_reference_translation_q16(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    int32_t *out_translation_x_q16,
    int32_t *out_translation_y_q16);

int falcon_reference_motion_x_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_x_q16);

int falcon_reference_motion_y_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_y_q16);

/*
 * Returns one while an aerial's source-defined landing-lag flag is active,
 * zero while the source action auto-cancels, and -1 when the action/frame has
 * no imported aerial landing data.
 */
int falcon_reference_landing_lag_active(
    uint8_t action_state,
    uint16_t action_frame);

#endif
