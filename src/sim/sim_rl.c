#include "pf/rl.h"

#include "sim_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(
    PF_RL_COMPACT_VALUE_COUNT ==
        PF_RL_COMPACT_GLOBAL_VALUES +
            PF_SIM_MAX_PLAYERS * PF_RL_COMPACT_PLAYER_STRIDE +
            PF_RL_COMPACT_ITEM_VALUES,
    "compact RL observation dimensions must cover players and item state");
_Static_assert(
    (PF_RL_ENGAGEMENT_REFERENCE_DISTANCE_Q16 >> 9U) ==
        PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_Q16,
    "engagement reference and potential must retain the shift fast path");

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

static int64_t pf_rl_horizontal_distance_q16(
    int32_t left_position_q16,
    int32_t right_position_q16)
{
    const int64_t difference =
        (int64_t)left_position_q16 - (int64_t)right_position_q16;
    return difference < INT64_C(0) ? -difference : difference;
}

static int32_t pf_rl_engagement_potential_from_distance_q16(
    int64_t distance_q16)
{
    if (distance_q16 >
        (int64_t)PF_RL_ENGAGEMENT_REFERENCE_DISTANCE_Q16)
    {
        distance_q16 =
            (int64_t)PF_RL_ENGAGEMENT_REFERENCE_DISTANCE_Q16;
    }

    return -(int32_t)(distance_q16 >> 9U);
}

static int32_t pf_rl_duel_engagement_potential_q16(
    const pf_world_state *world)
{
    if (world->active[0] == UINT8_C(0) ||
        world->active[1] == UINT8_C(0) ||
        world->team[0] == world->team[1])
    {
        return INT32_C(0);
    }
    return pf_rl_engagement_potential_from_distance_q16(
        pf_rl_horizontal_distance_q16(
            world->position_x_q16[0],
            world->position_x_q16[1]));
}

static int32_t pf_rl_engagement_potential_q16(
    const pf_world_state *world,
    uint32_t player_index)
{
    int64_t nearest_distance_q16 = INT64_MAX;
    uint32_t opponent_index;

    if (player_index >= (uint32_t)world->player_count ||
        world->active[player_index] == UINT8_C(0))
    {
        return INT32_C(0);
    }

    for (opponent_index = UINT32_C(0);
         opponent_index < (uint32_t)world->player_count;
         ++opponent_index)
    {
        int64_t distance_q16;

        if (world->active[opponent_index] == UINT8_C(0) ||
            world->team[opponent_index] == world->team[player_index])
        {
            continue;
        }
        distance_q16 = pf_rl_horizontal_distance_q16(
            world->position_x_q16[player_index],
            world->position_x_q16[opponent_index]);
        if (distance_q16 < nearest_distance_q16)
        {
            nearest_distance_q16 = distance_q16;
        }
    }

    if (nearest_distance_q16 == INT64_MAX)
    {
        return INT32_C(0);
    }

    return pf_rl_engagement_potential_from_distance_q16(
        nearest_distance_q16);
}

static void pf_rl_engagement_potentials(
    const pf_world_state *world,
    int32_t potentials_q16[PF_SIM_MAX_PLAYERS])
{
    uint32_t player_index;

    (void)memset(
        potentials_q16,
        0,
        sizeof(*potentials_q16) * (size_t)PF_SIM_MAX_PLAYERS);
    if (world->player_count == UINT8_C(2) &&
        world->active[0] != UINT8_C(0) &&
        world->active[1] != UINT8_C(0) &&
        world->team[0] != world->team[1])
    {
        const int32_t shared_potential_q16 =
            pf_rl_duel_engagement_potential_q16(world);
        potentials_q16[0] = shared_potential_q16;
        potentials_q16[1] = shared_potential_q16;
        return;
    }

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        potentials_q16[player_index] =
            pf_rl_engagement_potential_q16(world, player_index);
    }
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
    compact->values[PF_RL_COMPACT_RESERVED_LOW_INDEX] =
        INT32_C(0);
    compact->values[PF_RL_COMPACT_RESERVED_HIGH_INDEX] =
        INT32_C(0);
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
                 ((uint32_t)observation->sudden_death << 18U) |
                 ((uint32_t)observation->stock_count << 19U) |
                 ((uint32_t)observation->winner_mask << 26U);
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
        compact->values[
            base + PF_RL_COMPACT_PLAYER_STOCKS_OFFSET] =
            (int32_t)player->stocks_remaining;
        compact->values[
            base + PF_RL_COMPACT_PLAYER_RESPAWN_OFFSET] =
            (int32_t)player->respawn_ticks;
        compact->values[
            base +
            PF_RL_COMPACT_PLAYER_INVULNERABILITY_OFFSET] =
                (int32_t)player->respawn_invulnerability_ticks;
    }
    {
        const pf_item_observation *item = &observation->item;
        const uint32_t item_bits =
            (uint32_t)item->state |
            ((uint32_t)item->holder_slot << 3U) |
            ((uint32_t)item->source_slot << 6U) |
            ((uint32_t)item->throw_direction << 9U) |
            ((uint32_t)item->hit_mask << 12U);

        compact->values[PF_RL_COMPACT_ITEM_BASE] =
            item->position_x_q16;
        compact->values[PF_RL_COMPACT_ITEM_BASE + UINT16_C(1)] =
            item->position_y_q16;
        compact->values[PF_RL_COMPACT_ITEM_BASE + UINT16_C(2)] =
            item->velocity_x_q16;
        compact->values[PF_RL_COMPACT_ITEM_BASE + UINT16_C(3)] =
            item->velocity_y_q16;
        compact->values[
            PF_RL_COMPACT_ITEM_BASE +
            PF_RL_COMPACT_ITEM_STATE_BITS_OFFSET] =
            pf_rl_u32_bits(item_bits);
        compact->values[
            PF_RL_COMPACT_ITEM_BASE +
            PF_RL_COMPACT_ITEM_LIFETIME_OFFSET] =
            (int32_t)item->lifetime_ticks;
        compact->values[
            PF_RL_COMPACT_ITEM_BASE +
            PF_RL_COMPACT_ITEM_RESPAWN_OFFSET] =
            (int32_t)item->respawn_ticks;
        compact->values[
            PF_RL_COMPACT_ITEM_BASE +
            PF_RL_COMPACT_ITEM_LOCKOUT_OFFSET] =
            (int32_t)item->pickup_lockout_ticks;
    }
}

static pf_status pf_rl_fill_transition(
    const pf_sim *sim,
    pf_status operation_status,
    const int32_t previous_potentials_q16[PF_SIM_MAX_PLAYERS],
    int grant_step_reward,
    pf_rl_transition *transition)
{
    pf_status observe_status;
    int32_t current_potentials_q16[PF_SIM_MAX_PLAYERS];
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
    transition->structured_observation.seed = UINT64_C(0);
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
                sim->world.active[player_index] != UINT8_C(0)
                    ? PF_INPUT_KNOWN_BUTTONS
                    : PF_INPUT_BUTTON_FORFEIT;
        }
    }

    if (grant_step_reward != 0 &&
        previous_potentials_q16 != NULL)
    {
        if (sim->world.player_count == UINT8_C(2))
        {
            const int32_t shared_reward_q16 =
                pf_rl_duel_engagement_potential_q16(
                    &sim->world) -
                previous_potentials_q16[0];
            transition->reward_q16[0] = shared_reward_q16;
            transition->reward_q16[1] = shared_reward_q16;
        }
        else
        {
            pf_rl_engagement_potentials(
                &sim->world,
                current_potentials_q16);
            for (player_index = UINT32_C(0);
                 player_index <
                     (uint32_t)sim->world.player_count;
                 ++player_index)
            {
                transition->reward_q16[player_index] =
                    current_potentials_q16[player_index] -
                    previous_potentials_q16[player_index];
            }
        }
    }

    if (grant_step_reward != 0 &&
        sim->world.terminated != UINT8_C(0) &&
        sim->world.winner_mask != UINT8_C(0))
    {
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)sim->world.player_count;
             ++player_index)
        {
            const uint8_t player_bit =
                (uint8_t)(UINT32_C(1) << player_index);
            transition->reward_q16[player_index] +=
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
    out_spec->reward_component_flags =
        PF_RL_REWARD_COMPONENT_TERMINAL |
        PF_RL_REWARD_COMPONENT_ENGAGEMENT;
    out_spec->known_buttons = PF_INPUT_KNOWN_BUTTONS;
    out_spec->axis_minimum = INT16_MIN;
    out_spec->axis_maximum = INT16_MAX;
    out_spec->trigger_minimum = UINT16_C(0);
    out_spec->trigger_maximum = UINT16_MAX;
    out_spec->terminal_reward_one_q16 = PF_Q16_ONE;
    out_spec->engagement_potential_limit_q16 =
        PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_Q16;
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
        NULL,
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
    int32_t previous_potentials_q16[PF_SIM_MAX_PLAYERS];
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
        return pf_rl_fill_transition(
            sim,
            status,
            NULL,
            0,
            out_transition);
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
                NULL,
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

    if (sim->world.player_count == UINT8_C(2))
    {
        previous_potentials_q16[0] =
            pf_rl_duel_engagement_potential_q16(&sim->world);
    }
    else
    {
        pf_rl_engagement_potentials(
            &sim->world,
            previous_potentials_q16);
    }
    status = pf_sim_tick(
        sim,
        inputs,
        action_count,
        &tick_result);
    return pf_rl_fill_transition(
        sim,
        status,
        previous_potentials_q16,
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
