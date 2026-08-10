#ifndef PF_SIM_FALCON_FRAME_DATA_H
#define PF_SIM_FALCON_FRAME_DATA_H

#include <stdint.h>

#define PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT UINT16_C(97)
#define PF_M4_FALCON_SUBMOTION_COUNT UINT16_C(318)
#define PF_M4_FALCON_SUBMOTION_DAMAGE_LOW_1 UINT16_C(171)
#define PF_M4_FALCON_SCRIPT_EVENT_COUNT UINT16_C(2056)
#define PF_M4_FALCON_SCRIPT_BYTE_COUNT UINT16_C(16516)
#define PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT UINT16_C(2536)
#define PF_M4_MELEE_STALE_MOVE_SLOT_COUNT UINT16_C(9)
#define PF_M4_FALCON_FALL_SPECIAL_ECB_FRAME_COUNT UINT16_C(8)
#define PF_M4_FALCON_AIR_DODGE_ECB_FRAME_COUNT UINT16_C(48)
#define PF_M4_FALCON_RAPTOR_BOOST_HIT_AIR_ECB_FRAME_COUNT UINT16_C(45)
#define PF_M4_FALCON_DIVE_ECB_FRAME_COUNT UINT16_C(64)
#define PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT UINT16_C(26)
#define PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT UINT16_C(24)
#define PF_M4_FALCON_PLATFORM_DROP_ECB_FRAME_COUNT UINT16_C(30)
#define PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT UINT16_C(35)
#define PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT UINT16_C(9)
#define PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT UINT16_C(51)
#define PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST UINT16_C(219)
#define PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT UINT16_C(10)
#define PF_M4_FALCON_LEDGE_JUMP1_QUICK_FRAME_COUNT UINT16_C(11)
#define PF_M4_FALCON_LEDGE_JUMP1_SLOW_FRAME_COUNT UINT16_C(18)

typedef enum pf_m4_falcon_move_index
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
} pf_m4_falcon_move_index;

/*
 * PlCa.dat submotion indices used directly by the simulation. The complete
 * 318-entry catalog remains index-addressable, including intentional empty
 * animation slots; only semantic aliases needed by runtime code are named
 * here so the public surface does not duplicate the source name table.
 */
typedef enum pf_m4_falcon_submotion_index
{
    PF_M4_FALCON_SUBMOTION_WAIT = 2,
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
    PF_M4_FALCON_SUBMOTION_SQUAT = 30,
    PF_M4_FALCON_SUBMOTION_SQUAT_WAIT = 31,
    PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE = 34,
    PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL = 36,
    PF_M4_FALCON_SUBMOTION_GUARD_OFF = 39,
    PF_M4_FALCON_SUBMOTION_SPOT_DODGE = 41,
    PF_M4_FALCON_SUBMOTION_ROLL_FORWARD = 42,
    PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD = 43,
    PF_M4_FALCON_SUBMOTION_AIR_DODGE = 44,
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
    PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT = 239,
    PF_M4_FALCON_SUBMOTION_APPEAL_LEFT = 240,
    PF_M4_FALCON_SUBMOTION_PLATFORM_DROP = 244
} pf_m4_falcon_submotion_index;

/* Qualified common-state hurt-pose tracks. Keep this compact index separate
 * from public action-state values so additional captured common motions append
 * without widening snapshots or coupling generated data to the API enum. */
typedef enum pf_m4_falcon_common_hurt_index
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
} pf_m4_falcon_common_hurt_index;

typedef struct pf_m4_falcon_submotion_data
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
} pf_m4_falcon_submotion_data;

typedef struct pf_m4_falcon_script_event
{
    uint16_t byte_offset;
    uint8_t byte_count;
    uint8_t command_id;
} pf_m4_falcon_script_event;

typedef struct pf_m4_falcon_animation_decode_summary
{
    uint32_t node_count;
    uint32_t track_count;
    uint32_t key_count;
} pf_m4_falcon_animation_decode_summary;

typedef struct pf_m4_falcon_body_collision_timing
{
    /* Raw action-script frames; UINT16_MAX means the command is absent. */
    uint16_t state_two_frame;
    uint16_t state_zero_frame;
} pf_m4_falcon_body_collision_timing;

typedef enum pf_m4_reference_hit_element
{
    PF_M4_REFERENCE_HIT_EMPTY = 0,
    PF_M4_REFERENCE_HIT_NORMAL = 1,
    PF_M4_REFERENCE_HIT_FIRE = 2,
    PF_M4_REFERENCE_HIT_ELECTRIC = 3,
    PF_M4_REFERENCE_HIT_GRAB = 4
} pf_m4_reference_hit_element;

typedef struct pf_m4_reference_hit_effect
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
} pf_m4_reference_hit_effect;

typedef struct pf_m4_reference_hit_phase
{
    uint16_t first_frame;
    uint16_t last_frame;
    uint16_t effect_mask;
    uint16_t reserved;
} pf_m4_reference_hit_phase;

typedef struct pf_m4_reference_geometry_move
{
    uint16_t frame_offset;
    uint8_t first_frame;
    uint8_t frame_count;
} pf_m4_reference_geometry_move;

typedef struct pf_m4_reference_hit_frame
{
    uint16_t sphere_offset;
    uint8_t sphere_count;
    uint8_t reserved;
} pf_m4_reference_hit_frame;

typedef struct pf_m4_reference_hit_sphere
{
    int32_t offset_x_q16;
    int32_t offset_y_q16;
    int32_t offset_z_q16;
    int32_t radius_q16;
    uint8_t effect_index;
    uint8_t hitbox_id;
    uint8_t group_id;
    uint8_t collision_state;
} pf_m4_reference_hit_sphere;

typedef struct pf_m4_reference_hurt_capsule
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
} pf_m4_reference_hurt_capsule;

typedef struct pf_m4_reference_hurt_move
{
    uint16_t frame_offset;
    uint8_t first_frame;
    uint8_t frame_count;
} pf_m4_reference_hurt_move;

typedef struct pf_m4_reference_hurt_frame
{
    uint16_t capsule_offset;
    uint8_t capsule_count;
    uint8_t reserved;
} pf_m4_reference_hurt_frame;

/*
 * Deterministic, unit-converted view of the gameplay fields in Falcon's
 * complete 97-word ftCo_DatAttrs block. The generated raw-word table remains
 * authoritative for fields that are opaque or irrelevant to this simulator.
 */
typedef struct pf_m4_falcon_common_attributes
{
    int32_t initial_walk_velocity_q16;
    int32_t walk_acceleration_q16;
    int32_t walk_maximum_velocity_q16;
    int32_t friction_q16;
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
    int32_t max_aerial_horizontal_velocity_q16;
    int32_t air_friction_q16;
    int32_t fast_fall_terminal_velocity_q16;
    int32_t maximum_horizontal_air_velocity_q16;
    int32_t shield_break_initial_velocity_q16;
    int32_t ledge_jump_horizontal_velocity_q16;
    int32_t ledge_jump_vertical_velocity_q16;
    int32_t wall_jump_horizontal_velocity_q16;
    int32_t wall_jump_vertical_velocity_q16;
    uint16_t jump_startup_ticks;
    uint16_t number_of_jumps;
    uint16_t turn_duration_ticks;
    uint16_t weight;
    uint16_t normal_landing_lag_ticks;
    uint16_t neutral_aerial_landing_lag_ticks;
    uint16_t forward_aerial_landing_lag_ticks;
    uint16_t back_aerial_landing_lag_ticks;
    uint16_t up_aerial_landing_lag_ticks;
    uint16_t down_aerial_landing_lag_ticks;
} pf_m4_falcon_common_attributes;

typedef struct pf_m4_falcon_ledge_attributes
{
    int32_t snap_x_q16;
    int32_t snap_y_q16;
    int32_t snap_height_q16;
} pf_m4_falcon_ledge_attributes;

typedef struct pf_m4_falcon_ledge_root_positions
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
} pf_m4_falcon_ledge_root_positions;

typedef struct pf_m4_falcon_ledge_attack_reference
{
    uint16_t total_frames;
    uint16_t first_active_frame;
    uint16_t last_active_frame;
    uint16_t reserved;
    pf_m4_reference_hit_effect effect;
} pf_m4_falcon_ledge_attack_reference;

/*
 * Exact deterministic view of the 0x8c-byte ftCaptain_DatAttrs block. Float
 * members are converted from the owner DAT's big-endian IEEE-754 words to
 * Q16.16 by the pinned importer. The raw source words remain in the generated
 * table as a completeness/provenance surface.
 */
typedef struct pf_m4_falcon_special_attributes
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
} pf_m4_falcon_special_attributes;

typedef struct pf_m4_falcon_common_special_attributes
{
    int32_t fast_ground_friction_multiplier_q16;
    int32_t air_drift_over_maximum_deceleration_q16;
    int32_t side_special_stick_threshold_q16;
    int32_t side_special_turn_threshold_q16;
    int32_t air_drift_dead_zone_q16;
} pf_m4_falcon_common_special_attributes;

typedef struct pf_m4_falcon_air_dodge_attributes
{
    int32_t initial_velocity_x_q16;
    int32_t initial_velocity_y_q16;
    int32_t decay_q16;
    uint16_t dead_zone;
    uint16_t item_throw_window_ticks;
    /* Raw EscapeAir displayed frame from command-variable zero becoming one. */
    uint16_t ordinary_physics_begin_frame;
    uint16_t reserved;
} pf_m4_falcon_air_dodge_attributes;

typedef struct pf_m4_falcon_neutral_special_timing
{
    uint16_t launch_frame;
    uint16_t velocity_scale_begin_frame;
    uint16_t velocity_scale_end_frame;
    uint16_t ordinary_air_physics_begin_frame;
} pf_m4_falcon_neutral_special_timing;

typedef struct pf_m4_falcon_side_special_timing
{
    uint16_t ground_search_begin_frame;
    uint16_t ground_search_end_frame;
    uint16_t air_search_begin_frame;
    uint16_t air_search_end_frame;
    uint16_t air_gravity_begin_frame;
} pf_m4_falcon_side_special_timing;

typedef struct pf_m4_falcon_up_special_timing
{
    uint16_t air_control_begin_frame;
    uint16_t throw_gravity_begin_frame;
    uint16_t victim_release_hitstun_ticks;
    uint16_t reserved;
    int32_t grounded_throw_reposition_x_q16;
    int32_t grounded_throw_reposition_y_q16;
} pf_m4_falcon_up_special_timing;

typedef struct pf_m4_melee_stale_move_data
{
    uint16_t slot_reduction_q16[PF_M4_MELEE_STALE_MOVE_SLOT_COUNT];
} pf_m4_melee_stale_move_data;

typedef struct pf_m4_falcon_down_special_timing
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
} pf_m4_falcon_down_special_timing;

typedef struct pf_m4_falcon_ecb_pose_q16
{
    int32_t top_x_from_origin_q16;
    int32_t top_y_from_origin_q16;
    int32_t bottom_x_from_origin_q16;
    int32_t bottom_y_from_origin_q16;
    int32_t right_x_from_origin_q16;
    int32_t right_y_from_origin_q16;
    int32_t left_x_from_origin_q16;
    int32_t left_y_from_origin_q16;
} pf_m4_falcon_ecb_pose_q16;

typedef struct pf_m4_falcon_collision_pose
{
    int32_t falling_bottom_y_from_origin_q16;
    uint32_t down_bound_back_floor_contact_mask;
    uint32_t down_bound_stomach_floor_contact_mask;
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
    int32_t jump_forward_bottom_y_from_origin_q16[
        PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT];
    pf_m4_falcon_ecb_pose_q16 ceiling_bounce[
        PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT];
    pf_m4_falcon_ecb_pose_q16 wall_bounce[
        PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT];
    int32_t fall_special_bottom_y_from_origin_q16[
        PF_M4_FALCON_FALL_SPECIAL_ECB_FRAME_COUNT];
    int32_t raptor_boost_hit_air_bottom_y_from_origin_q16[
        PF_M4_FALCON_RAPTOR_BOOST_HIT_AIR_ECB_FRAME_COUNT];
    int32_t falcon_dive_right_x_from_origin_q16[
        PF_M4_FALCON_DIVE_ECB_FRAME_COUNT];
    int32_t falcon_dive_bottom_y_from_origin_q16[
        PF_M4_FALCON_DIVE_ECB_FRAME_COUNT];
} pf_m4_falcon_collision_pose;

typedef struct pf_m4_reference_search_sphere
{
    int32_t offset_x_q16;
    int32_t offset_y_q16;
    int32_t offset_z_q16;
    int32_t radius_q16;
} pf_m4_reference_search_sphere;

typedef struct pf_m4_reference_throw
{
    uint16_t angle_degrees;
    uint16_t growth;
    uint16_t weight_set;
    uint16_t base;
    uint8_t damage;
    uint8_t element;
    uint16_t release_frame;
    uint16_t reserved;
} pf_m4_reference_throw;

typedef struct pf_m4_reference_move
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
} pf_m4_reference_move;

typedef struct pf_m4_reference_timing
{
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
} pf_m4_reference_timing;

typedef enum pf_m4_reference_iasa_policy
{
    PF_M4_REFERENCE_IASA_NONE = 0,
    PF_M4_REFERENCE_IASA_JAB_CHAIN,
    PF_M4_REFERENCE_IASA_WAIT,
    PF_M4_REFERENCE_IASA_DOWN_TILT,
    PF_M4_REFERENCE_IASA_FORWARD_SMASH
} pf_m4_reference_iasa_policy;

typedef enum pf_m4_reference_ground_physics
{
    PF_M4_REFERENCE_GROUND_PHYSICS_FRICTION = 0,
    PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION
} pf_m4_reference_ground_physics;

const uint8_t *pf_m4_falcon_reference_source_sha256(void);

const uint8_t *pf_m4_falcon_reference_complete_source_sha256(void);

const uint8_t *pf_m4_falcon_reference_submotion_catalog_sha256(void);

const uint8_t *pf_m4_falcon_reference_action_script_sha256(void);

const uint8_t *pf_m4_falcon_reference_animation_tracks_sha256(void);

const pf_m4_falcon_animation_decode_summary *
pf_m4_falcon_reference_animation_decode_summary(void);

const pf_m4_falcon_submotion_data *pf_m4_falcon_reference_submotion(
    uint16_t submotion_index);

const pf_m4_falcon_script_event *pf_m4_falcon_reference_submotion_event(
    uint16_t submotion_index,
    uint16_t event_index,
    const uint8_t **out_bytes);

const pf_m4_falcon_body_collision_timing *
pf_m4_falcon_reference_body_collision_timing(uint16_t submotion_index);

const uint32_t *pf_m4_falcon_reference_common_attribute_bits(
    uint16_t *out_count);

const pf_m4_falcon_common_attributes *
pf_m4_falcon_reference_common_attributes(void);

const pf_m4_falcon_ledge_attributes *
pf_m4_falcon_reference_ledge_attributes(void);

const pf_m4_falcon_ledge_root_positions *
pf_m4_falcon_reference_ledge_root_positions(void);

const pf_m4_falcon_ledge_attack_reference *
pf_m4_falcon_reference_ledge_attack(uint16_t submotion_index);

int pf_m4_falcon_reference_ledge_option_anchor_q16(
    uint16_t submotion_index,
    int32_t *out_x_q16,
    int32_t *out_y_q16);

uint16_t pf_m4_falcon_reference_ledge_option_ground_frame(
    uint16_t submotion_index);

int pf_m4_falcon_reference_hyrule_ledge_jump_position_q16(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    int32_t *out_x_from_wait_q16,
    int32_t *out_y_from_wait_q16);

int pf_m4_falcon_reference_body_invulnerable(
    uint16_t submotion_index,
    uint16_t action_ticks);

const pf_m4_falcon_special_attributes *
pf_m4_falcon_reference_special_attributes(void);

const pf_m4_falcon_common_special_attributes *
pf_m4_falcon_reference_common_special_attributes(void);

const pf_m4_falcon_air_dodge_attributes *
pf_m4_falcon_reference_air_dodge_attributes(void);

const pf_m4_melee_stale_move_data *
pf_m4_falcon_reference_stale_move_data(void);

const pf_m4_falcon_neutral_special_timing *
pf_m4_falcon_reference_neutral_special_timing(void);

const pf_m4_falcon_side_special_timing *
pf_m4_falcon_reference_side_special_timing(void);

const pf_m4_falcon_up_special_timing *
pf_m4_falcon_reference_up_special_timing(void);

const pf_m4_falcon_down_special_timing *
pf_m4_falcon_reference_down_special_timing(void);

const pf_m4_falcon_collision_pose *
pf_m4_falcon_reference_collision_pose(void);

const pf_m4_reference_search_sphere *
pf_m4_falcon_reference_side_special_search_spheres(
    int airborne,
    uint8_t *out_count);

const uint8_t *pf_m4_falcon_reference_geometry_sha256(void);

const pf_m4_reference_move *pf_m4_falcon_reference_move(
    pf_m4_falcon_move_index move_index);

const pf_m4_reference_hit_phase *pf_m4_falcon_reference_phase(
    pf_m4_falcon_move_index move_index,
    uint16_t phase_index);

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_effect(
    pf_m4_falcon_move_index move_index,
    uint16_t effect_index);

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_primary_effect(
    pf_m4_falcon_move_index move_index);

const pf_m4_reference_hit_phase *pf_m4_falcon_reference_phase_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame);

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_effect_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame);

const pf_m4_reference_hit_sphere *
pf_m4_falcon_reference_hit_spheres_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_sphere_count);

int pf_m4_falcon_reference_has_hit_geometry(
    pf_m4_falcon_move_index move_index);

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_standing_hurt_capsules(uint8_t *out_count);

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_hurt_capsules_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_count);

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_common_hurt_capsules_at_frame(
    uint8_t action_state,
    uint16_t action_frame,
    uint8_t *out_count);

int pf_m4_falcon_reference_move_for_action(
    uint8_t action_state,
    pf_m4_falcon_move_index *out_move_index);

const pf_m4_reference_throw *pf_m4_falcon_reference_throw(
    pf_m4_falcon_move_index move_index);

pf_m4_reference_timing pf_m4_falcon_reference_timing(
    pf_m4_falcon_move_index move_index);

const pf_m4_reference_move *pf_m4_falcon_reference_attack(
    uint8_t action_state,
    pf_m4_reference_timing authored_timing,
    uint32_t authored_damage_q16);

int pf_m4_falcon_reference_attack_matches(
    uint8_t action_state,
    pf_m4_reference_timing authored_timing,
    uint32_t authored_damage_q16);

pf_m4_reference_iasa_policy pf_m4_falcon_reference_iasa_policy_for_action(
    uint8_t action_state);

pf_m4_reference_ground_physics
pf_m4_falcon_reference_ground_physics_for_action(uint8_t action_state);

int pf_m4_falcon_reference_iasa_active(
    uint8_t action_state,
    uint32_t displayed_frame);

int pf_m4_falcon_reference_special_iasa_active(
    uint8_t action_state,
    uint16_t action_ticks);

/*
 * Returns the source animation TransN delta for a one-based displayed frame.
 * This accessor deliberately does not decide whether a state callback replaces
 * velocity, adds displacement, or ignores the track.
 */
int pf_m4_falcon_reference_translation_q16(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    int32_t *out_translation_x_q16,
    int32_t *out_translation_y_q16);

int pf_m4_falcon_reference_motion_x_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_x_q16);

int pf_m4_falcon_reference_motion_y_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_y_q16);

/*
 * Returns one while an aerial's source-defined landing-lag flag is active,
 * zero while the source action auto-cancels, and -1 when the action/frame has
 * no imported aerial landing data.
 */
int pf_m4_falcon_reference_landing_lag_active(
    uint8_t action_state,
    uint16_t action_frame);

#endif
