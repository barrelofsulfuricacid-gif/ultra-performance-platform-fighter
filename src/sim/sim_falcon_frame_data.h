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
} pf_m4_reference_move;

typedef struct pf_m4_reference_timing
{
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
} pf_m4_reference_timing;

const uint8_t *pf_m4_falcon_reference_source_sha256(void);

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

int pf_m4_falcon_reference_move_for_action(
    uint8_t action_state,
    pf_m4_falcon_move_index *out_move_index);

const pf_m4_reference_throw *pf_m4_falcon_reference_throw(
    pf_m4_falcon_move_index move_index);

pf_m4_reference_timing pf_m4_falcon_reference_timing(
    pf_m4_falcon_move_index move_index);

#endif
