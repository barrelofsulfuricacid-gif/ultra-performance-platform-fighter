#ifndef PF_SIM_FALCON_FRAME_DATA_H
#define PF_SIM_FALCON_FRAME_DATA_H

#include <stdint.h>

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
    PF_M4_FALCON_UP_SPECIAL_THROW_AIR,
    PF_M4_FALCON_MOVE_COUNT
} pf_m4_falcon_move_index;

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
    uint8_t reserved;
} pf_m4_reference_hit_sphere;

typedef struct pf_m4_reference_hurt_capsule
{
    int32_t endpoint_a_x_q16;
    int32_t endpoint_a_y_q16;
    int32_t endpoint_b_x_q16;
    int32_t endpoint_b_y_q16;
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

int pf_m4_falcon_reference_special_iasa_active(
    uint8_t action_state,
    uint16_t action_ticks);

int pf_m4_falcon_reference_motion_x_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_x_q16);

/*
 * Returns one while an aerial's source-defined landing-lag flag is active,
 * zero while the source action auto-cancels, and -1 when the action/frame has
 * no imported aerial landing data.
 */
int pf_m4_falcon_reference_landing_lag_active(
    uint8_t action_state,
    uint16_t action_frame);

#endif
