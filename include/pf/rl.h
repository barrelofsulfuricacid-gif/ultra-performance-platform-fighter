#ifndef PF_RL_H
#define PF_RL_H

#include "pf/sim.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_RL_SCHEMA_VERSION UINT16_C(10)
#define PF_RL_ACTION_SCHEMA_VERSION UINT16_C(1)
#define PF_RL_TRANSITION_SCHEMA_VERSION UINT16_C(8)
#define PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION UINT16_C(9)
#define PF_RL_COMPACT_GLOBAL_VALUES UINT16_C(8)
#define PF_RL_COMPACT_PLAYER_STRIDE UINT16_C(10)
#define PF_RL_COMPACT_ITEM_VALUES UINT16_C(8)
#define PF_RL_COMPACT_ITEM_BASE UINT16_C(48)
#define PF_RL_COMPACT_PROJECTILE_VALUES UINT16_C(6)
#define PF_RL_COMPACT_PROJECTILE_BASE UINT16_C(56)
#define PF_RL_COMPACT_CHARGE_VALUES UINT16_C(4)
#define PF_RL_COMPACT_CHARGE_BASE UINT16_C(62)
#define PF_RL_COMPACT_SMASH_CHARGE_VALUES UINT16_C(4)
#define PF_RL_COMPACT_SMASH_CHARGE_BASE UINT16_C(66)
#define PF_RL_COMPACT_SHIELD_STRENGTH_VALUES UINT16_C(4)
#define PF_RL_COMPACT_SHIELD_STRENGTH_BASE UINT16_C(70)
#define PF_RL_COMPACT_VALUE_COUNT UINT16_C(74)

#define PF_RL_REWARD_COMPONENT_TERMINAL (UINT8_C(1) << 0U)
#define PF_RL_REWARD_COMPONENT_ENGAGEMENT (UINT8_C(1) << 1U)
#define PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_Q16 INT32_C(16384)
#define PF_RL_ENGAGEMENT_REFERENCE_DISTANCE_Q16 INT32_C(8388608)

#define PF_RL_COMPACT_TICK_LOW_INDEX UINT16_C(0)
#define PF_RL_COMPACT_TICK_HIGH_INDEX UINT16_C(1)
#define PF_RL_COMPACT_RESERVED_LOW_INDEX UINT16_C(2)
#define PF_RL_COMPACT_RESERVED_HIGH_INDEX UINT16_C(3)
#define PF_RL_COMPACT_MAX_TICKS_LOW_INDEX UINT16_C(4)
#define PF_RL_COMPACT_MAX_TICKS_HIGH_INDEX UINT16_C(5)
#define PF_RL_COMPACT_FAULT_FLAGS_INDEX UINT16_C(6)
#define PF_RL_COMPACT_MATCH_BITS_INDEX UINT16_C(7)
#define PF_RL_COMPACT_PLAYER_BASE(slot)                              \
    ((uint16_t)(PF_RL_COMPACT_GLOBAL_VALUES +                       \
                (uint16_t)(slot) * PF_RL_COMPACT_PLAYER_STRIDE))
#define PF_RL_COMPACT_PLAYER_STOCKS_OFFSET UINT16_C(7)
#define PF_RL_COMPACT_PLAYER_RESPAWN_OFFSET UINT16_C(8)
#define PF_RL_COMPACT_PLAYER_INVULNERABILITY_OFFSET UINT16_C(9)
#define PF_RL_COMPACT_ITEM_STATE_BITS_OFFSET UINT16_C(4)
#define PF_RL_COMPACT_ITEM_LIFETIME_OFFSET UINT16_C(5)
#define PF_RL_COMPACT_ITEM_RESPAWN_OFFSET UINT16_C(6)
#define PF_RL_COMPACT_ITEM_LOCKOUT_OFFSET UINT16_C(7)
#define PF_RL_COMPACT_PROJECTILE_STATE_BITS_OFFSET UINT16_C(4)
#define PF_RL_COMPACT_PROJECTILE_LIFETIME_OFFSET UINT16_C(5)

typedef struct pf_rl_action
{
    uint64_t buttons;
    int16_t main_stick_x;
    int16_t main_stick_y;
    int16_t secondary_stick_x;
    int16_t secondary_stick_y;
    uint16_t left_trigger;
    uint16_t right_trigger;
    uint16_t schema_version;
    uint16_t reserved;
} pf_rl_action;

typedef struct pf_rl_spec
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t action_schema_version;
    uint16_t transition_schema_version;
    uint16_t compact_observation_schema_version;
    uint16_t compact_value_count;
    uint16_t action_stride;
    uint8_t max_players;
    uint8_t reward_component_flags;
    uint8_t reserved[2];
    uint64_t known_buttons;
    int16_t axis_minimum;
    int16_t axis_maximum;
    uint16_t trigger_minimum;
    uint16_t trigger_maximum;
    int32_t terminal_reward_one_q16;
    int32_t engagement_potential_limit_q16;
} pf_rl_spec;

typedef struct pf_rl_compact_observation
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t value_count;
    int32_t values[PF_RL_COMPACT_VALUE_COUNT];
} pf_rl_compact_observation;

typedef struct pf_rl_transition
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    uint32_t status;
    uint32_t diagnostic_flags;
    pf_tick_result tick_result;
    pf_sim_observation structured_observation;
    pf_rl_compact_observation compact_observation;
    int32_t reward_q16[PF_SIM_MAX_PLAYERS];
    uint64_t legal_buttons[PF_SIM_MAX_PLAYERS];
} pf_rl_transition;

pf_status pf_rl_query_spec(pf_rl_spec *out_spec);

pf_status pf_rl_reset(
    pf_sim *sim,
    uint64_t seed,
    pf_rl_transition *out_transition);

pf_status pf_rl_step(
    pf_sim *sim,
    const pf_rl_action *actions,
    size_t action_count,
    pf_rl_transition *out_transition);

pf_status pf_rl_reset_batch(
    pf_sim *const *sims,
    const uint64_t *seeds,
    size_t environment_count,
    pf_rl_transition *out_transitions);

pf_status pf_rl_step_batch(
    pf_sim *const *sims,
    size_t environment_count,
    const pf_rl_action *actions,
    size_t action_stride,
    pf_rl_transition *out_transitions);

#ifdef __cplusplus
}
#endif

#endif
