#include "pf/rl.h"

#include "sim_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(
    PF_RL_COMPACT_VALUE_COUNT ==
        PF_RL_COMPACT_GLOBAL_VALUES +
            PF_SIM_MAX_PLAYERS * PF_RL_COMPACT_PLAYER_STRIDE,
    "compact RL observation dimensions must cover every player slot");

static int32_t pf_rl_u32_bits(uint32_t value)
{
    int32_t result;

    (void)memcpy(&result, &value, sizeof(result));
    return result;
}

static int32_t pf_rl_u64_low(uint64_t value)
{
    return pf_rl_u32_bits((uint32_t)value);
}

static int32_t pf_rl_u64_high(uint64_t value)
{
    return pf_rl_u32_bits((uint32_t)(value >> 32U));
}

static void pf_rl_transition_init(pf_rl_transition *transition)
{
    (void)memset(transition, 0, sizeof(*transition));
    transition->struct_size = (uint32_t)sizeof(*transition);
    transition->schema_version = PF_RL_TRANSITION_SCHEMA_VERSION;
    transition->compact_observation.struct_size =
        (uint32_t)sizeof(transition->compact_observation);
    transition->compact_observation.schema_version =
        PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION;
    transition->compact_observation.value_count =
        PF_RL_COMPACT_VALUE_COUNT;
}

static void pf_rl_world_result(
    const pf_world_state *world,
    pf_tick_result *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->completed_tick = world->tick;
    result->fault_flags = world->fault_flags;
    result->terminated = world->terminated;
    result->truncated = world->truncated;
    result->winner_mask = world->winner_mask;
}

static void pf_rl_fill_compact(
    const pf_sim *sim,
    const pf_sim_observation *observation,
    pf_rl_compact_observation *compact)
{
    uint32_t match_bits;
    uint32_t player_index;

    compact->values[PF_RL_COMPACT_TICK_LOW_INDEX] =
        pf_rl_u64_low(observation->tick);
    compact->values[PF_RL_COMPACT_TICK_HIGH_INDEX] =
        pf_rl_u64_high(observation->tick);
    compact->values[PF_RL_COMPACT_SEED_LOW_INDEX] =
        pf_rl_u64_low(observation->seed);
    compact->values[PF_RL_COMPACT_SEED_HIGH_INDEX] =
        pf_rl_u64_high(observation->seed);
    compact->values[PF_RL_COMPACT_MAX_TICKS_LOW_INDEX] =
        pf_rl_u64_low(sim->world.max_ticks);
    compact->values[PF_RL_COMPACT_MAX_TICKS_HIGH_INDEX] =
        pf_rl_u64_high(sim->world.max_ticks);
    compact->values[PF_RL_COMPACT_FAULT_FLAGS_INDEX] =
        pf_rl_u32_bits(observation->fault_flags);

    match_bits = (uint32_t)observation->player_count |
                 ((uint32_t)observation->mode << 8U) |
                 ((uint32_t)observation->terminated << 16U) |
                 ((uint32_t)observation->truncated << 17U) |
                 ((uint32_t)observation->winner_mask << 24U);
    compact->values[PF_RL_COMPACT_MATCH_BITS_INDEX] =
        pf_rl_u32_bits(match_bits);

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const pf_player_observation *player =
            &observation->players[player_index];
        const uint16_t base =
            PF_RL_COMPACT_PLAYER_BASE(player_index);
        const uint32_t player_bits =
            (uint32_t)player->player_slot |
            ((uint32_t)player->team << 8U) |
            ((uint32_t)player->grounded << 16U) |
            ((uint32_t)player->active << 17U);

        compact->values[base] =
            pf_rl_u64_low(player->previous_buttons);
        compact->values[base + UINT16_C(1)] =
            pf_rl_u64_high(player->previous_buttons);
        compact->values[base + UINT16_C(2)] =
            player->position_x_q16;
        compact->values[base + UINT16_C(3)] =
            player->position_y_q16;
        compact->values[base + UINT16_C(4)] =
            player->velocity_x_q16;
        compact->values[base + UINT16_C(5)] =
            player->velocity_y_q16;
        compact->values[base + UINT16_C(6)] =
            pf_rl_u32_bits(player_bits);
    }
}

static pf_status pf_rl_fill_transition(
    const pf_sim *sim,
    pf_status operation_status,
    int grant_terminal_reward,
    pf_rl_transition *transition)
{
    pf_status observe_status;
    uint32_t player_index;

    transition->status = (uint32_t)operation_status;
    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return operation_status;
    }

    observe_status = pf_sim_observe(
        sim,
        &transition->structured_observation);
    if (observe_status != PF_STATUS_OK)
    {
        transition->status = (uint32_t)observe_status;
        return observe_status;
    }
    pf_rl_world_result(&sim->world, &transition->tick_result);
    transition->diagnostic_flags = sim->world.fault_flags;
    pf_rl_fill_compact(
        sim,
        &transition->structured_observation,
        &transition->compact_observation);

    if (sim->world.fault_flags == UINT32_C(0) &&
        sim->world.terminated == UINT8_C(0) &&
        sim->world.truncated == UINT8_C(0))
    {
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)sim->world.player_count;
             ++player_index)
        {
            transition->legal_buttons[player_index] =
                PF_INPUT_KNOWN_BUTTONS;
        }
    }

    if (grant_terminal_reward != 0 &&
        sim->world.terminated != UINT8_C(0) &&
        sim->world.winner_mask != UINT8_C(0))
    {
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)sim->world.player_count;
             ++player_index)
        {
            const uint8_t player_bit =
                (uint8_t)(UINT32_C(1) << player_index);
            transition->reward_q16[player_index] =
                (sim->world.winner_mask & player_bit) != UINT8_C(0)
                    ? PF_Q16_ONE
                    : -PF_Q16_ONE;
        }
    }
    return operation_status;
}

static int pf_rl_action_valid(const pf_rl_action *action)
{
    return action->schema_version == PF_RL_ACTION_SCHEMA_VERSION &&
           action->reserved == UINT16_C(0) &&
           (action->buttons & ~PF_INPUT_KNOWN_BUTTONS) == UINT64_C(0);
}

pf_status pf_rl_query_spec(pf_rl_spec *out_spec)
{
    if (out_spec == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(out_spec, 0, sizeof(*out_spec));
    out_spec->struct_size = (uint32_t)sizeof(*out_spec);
    out_spec->schema_version = PF_RL_SCHEMA_VERSION;
    out_spec->action_schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    out_spec->transition_schema_version =
        PF_RL_TRANSITION_SCHEMA_VERSION;
    out_spec->compact_observation_schema_version =
        PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION;
    out_spec->compact_value_count = PF_RL_COMPACT_VALUE_COUNT;
    out_spec->action_stride = (uint16_t)PF_SIM_MAX_PLAYERS;
    out_spec->max_players = (uint8_t)PF_SIM_MAX_PLAYERS;
    out_spec->known_buttons = PF_INPUT_KNOWN_BUTTONS;
    out_spec->axis_minimum = INT16_MIN;
    out_spec->axis_maximum = INT16_MAX;
    out_spec->trigger_minimum = UINT16_C(0);
    out_spec->trigger_maximum = UINT16_MAX;
    out_spec->terminal_reward_one_q16 = PF_Q16_ONE;
    return PF_STATUS_OK;
}

pf_status pf_rl_reset(
    pf_sim *sim,
    uint64_t seed,
    pf_rl_transition *out_transition)
{
    pf_status status;

    if (out_transition == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    pf_rl_transition_init(out_transition);
    status = pf_sim_reset(sim, seed);
    return pf_rl_fill_transition(
        sim,
        status,
        0,
        out_transition);
}

pf_status pf_rl_step(
    pf_sim *sim,
    const pf_rl_action *actions,
    size_t action_count,
    pf_rl_transition *out_transition)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result tick_result;
    pf_status status;
    uint32_t player_index;

    if (out_transition == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    pf_rl_transition_init(out_transition);
    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        out_transition->status = (uint32_t)PF_STATUS_INVALID_STATE;
        return PF_STATUS_INVALID_STATE;
    }
    if (actions == NULL ||
        action_count != (size_t)sim->world.player_count)
    {
        status = PF_STATUS_INVALID_ARGUMENT;
        return pf_rl_fill_transition(sim, status, 0, out_transition);
    }

    (void)memset(inputs, 0, sizeof(inputs));
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)sim->world.player_count;
         ++player_index)
    {
        const pf_rl_action *action = &actions[player_index];
        pf_input_frame *input = &inputs[player_index];
        if (!pf_rl_action_valid(action))
        {
            status = action->schema_version !=
                         PF_RL_ACTION_SCHEMA_VERSION
                         ? PF_STATUS_UNSUPPORTED_VERSION
                         : PF_STATUS_INVALID_ARGUMENT;
            return pf_rl_fill_transition(
                sim,
                status,
                0,
                out_transition);
        }

        input->tick = sim->world.tick;
        input->buttons = action->buttons;
        input->main_stick_x = action->main_stick_x;
        input->main_stick_y = action->main_stick_y;
        input->secondary_stick_x = action->secondary_stick_x;
        input->secondary_stick_y = action->secondary_stick_y;
        input->left_trigger = action->left_trigger;
        input->right_trigger = action->right_trigger;
        input->schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        input->player_slot = (uint8_t)player_index;
    }

    status = pf_sim_tick(
        sim,
        inputs,
        action_count,
        &tick_result);
    return pf_rl_fill_transition(
        sim,
        status,
        status == PF_STATUS_OK,
        out_transition);
}

pf_status pf_rl_reset_batch(
    pf_sim *const *sims,
    const uint64_t *seeds,
    size_t environment_count,
    pf_rl_transition *out_transitions)
{
    pf_status first_status = PF_STATUS_OK;
    size_t environment_index;

    if (environment_count == (size_t)0)
    {
        return PF_STATUS_OK;
    }
    if (sims == NULL || seeds == NULL || out_transitions == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    for (environment_index = (size_t)0;
         environment_index < environment_count;
         ++environment_index)
    {
        const pf_status status = pf_rl_reset(
            sims[environment_index],
            seeds[environment_index],
            &out_transitions[environment_index]);
        if (first_status == PF_STATUS_OK && status != PF_STATUS_OK)
        {
            first_status = status;
        }
    }
    return first_status;
}

pf_status pf_rl_step_batch(
    pf_sim *const *sims,
    size_t environment_count,
    const pf_rl_action *actions,
    size_t action_stride,
    pf_rl_transition *out_transitions)
{
    pf_status first_status = PF_STATUS_OK;
    size_t environment_index;

    if (environment_count == (size_t)0)
    {
        return PF_STATUS_OK;
    }
    if (sims == NULL || actions == NULL || out_transitions == NULL ||
        action_stride != (size_t)PF_SIM_MAX_PLAYERS ||
        environment_count > SIZE_MAX / action_stride)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    for (environment_index = (size_t)0;
         environment_index < environment_count;
         ++environment_index)
    {
        pf_sim *sim = sims[environment_index];
        const size_t action_offset =
            environment_index * action_stride;
        const size_t action_count =
            pf_sim_is_valid(sim)
                ? (size_t)sim->world.player_count
                : (size_t)PF_SIM_MAX_PLAYERS;
        const pf_status status = pf_rl_step(
            sim,
            &actions[action_offset],
            action_count,
            &out_transitions[environment_index]);
        if (first_status == PF_STATUS_OK && status != PF_STATUS_OK)
        {
            first_status = status;
        }
    }
    return first_status;
}
