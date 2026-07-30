#include "sim_internal.h"

#include <stdint.h>
#include <string.h>

static void pf_write_result(const pf_world_state *world, pf_tick_result *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->completed_tick = world->tick;
    result->fault_flags = world->fault_flags;
    result->terminated = world->terminated;
    result->truncated = world->truncated;
    result->winner_mask = world->winner_mask;
}

static pf_status pf_validate_inputs(
    const pf_world_state *world,
    const pf_input_frame *inputs,
    size_t player_count)
{
    size_t player_index;

    if (inputs == NULL ||
        player_count != (size_t)world->player_count)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    for (player_index = (size_t)0; player_index < player_count; ++player_index)
    {
        const pf_input_frame *input = &inputs[player_index];
        if (input->schema_version != PF_SIM_INPUT_SCHEMA_VERSION)
        {
            return PF_STATUS_UNSUPPORTED_VERSION;
        }
        if (input->reserved != UINT8_C(0) ||
            input->player_slot != (uint8_t)player_index ||
            (input->buttons & ~PF_INPUT_KNOWN_BUTTONS) != UINT64_C(0))
        {
            return PF_STATUS_INVALID_ARGUMENT;
        }
        if (input->tick != world->tick)
        {
            return PF_STATUS_TICK_MISMATCH;
        }
    }

    return PF_STATUS_OK;
}

pf_status pf_sim_tick_impl(
    pf_sim *sim,
    const pf_input_frame *inputs,
    size_t player_count,
    pf_tick_result *out_result)
{
    pf_world_state *world;
    pf_sim_scratch *scratch;
    pf_status status;
    uint64_t forfeit_mask = UINT64_C(0);
    uint32_t player_index;

    if (out_result == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_result, 0, sizeof(*out_result));

    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    world = &sim->world;
    scratch = sim->scratch;

    if (world->fault_flags != UINT32_C(0))
    {
        pf_write_result(world, out_result);
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (world->terminated != UINT8_C(0) ||
        world->truncated != UINT8_C(0))
    {
        pf_write_result(world, out_result);
        return PF_STATUS_EPISODE_DONE;
    }

    status = pf_validate_inputs(world, inputs, player_count);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, out_result);
        return status;
    }

    scratch->combat_event_sequence = world->combat_event_sequence;
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const pf_input_frame *input = &inputs[player_index];

        if ((input->buttons & PF_INPUT_BUTTON_FORFEIT) != UINT64_C(0))
        {
            forfeit_mask |= UINT64_C(1) << player_index;
        }

        status = pf_m4_step_player(
            &sim->content,
            world,
            scratch,
            input,
            player_index);
        if (status != PF_STATUS_OK)
        {
            pf_write_result(world, out_result);
            return status;
        }
    }

    status = pf_m4_resolve_combat(&sim->content, world, scratch);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, out_result);
        return status;
    }

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        world->position_x_q16[player_index] =
            scratch->position_x_q16[player_index];
        world->position_y_q16[player_index] =
            scratch->position_y_q16[player_index];
        world->velocity_x_q16[player_index] =
            scratch->velocity_x_q16[player_index];
        world->velocity_y_q16[player_index] =
            scratch->velocity_y_q16[player_index];
        world->action_ticks[player_index] =
            scratch->action_ticks[player_index];
        world->respawn_count[player_index] =
            scratch->respawn_count[player_index];
        world->previous_buttons[player_index] =
            scratch->previous_buttons[player_index];
        world->grounded[player_index] =
            scratch->grounded[player_index];
        world->action_state[player_index] =
            scratch->action_state[player_index];
        world->support[player_index] =
            scratch->support[player_index];
        world->air_jumps_remaining[player_index] =
            scratch->air_jumps_remaining[player_index];
        world->short_hop_latched[player_index] =
            scratch->short_hop_latched[player_index];
        world->platform_drop_ticks[player_index] =
            scratch->platform_drop_ticks[player_index];
        world->fast_fall[player_index] =
            scratch->fast_fall[player_index];
        world->facing[player_index] =
            scratch->facing[player_index];
        world->dash_direction[player_index] =
            scratch->dash_direction[player_index];
        world->previous_strong_direction[player_index] =
            scratch->previous_strong_direction[player_index];
        world->damage_q16[player_index] =
            scratch->damage_q16[player_index];
        world->pending_velocity_x_q16[player_index] =
            scratch->pending_velocity_x_q16[player_index];
        world->pending_velocity_y_q16[player_index] =
            scratch->pending_velocity_y_q16[player_index];
        world->last_hit_sequence[player_index] =
            scratch->last_hit_sequence[player_index];
        world->last_hit_tick[player_index] =
            scratch->last_hit_tick[player_index];
        world->last_hit_damage_q16[player_index] =
            scratch->last_hit_damage_q16[player_index];
        world->hitlag_ticks[player_index] =
            scratch->hitlag_ticks[player_index];
        world->hitstun_ticks[player_index] =
            scratch->hitstun_ticks[player_index];
        world->tech_window_ticks[player_index] =
            scratch->tech_window_ticks[player_index];
        world->tech_lockout_ticks[player_index] =
            scratch->tech_lockout_ticks[player_index];
        world->shield_stun_ticks[player_index] =
            scratch->shield_stun_ticks[player_index];
        world->shield_health_q16[player_index] =
            scratch->shield_health_q16[player_index];
        world->hitlag_resume_action[player_index] =
            scratch->hitlag_resume_action[player_index];
        world->attack_hit_mask[player_index] =
            scratch->attack_hit_mask[player_index];
        world->last_hit_attacker[player_index] =
            scratch->last_hit_attacker[player_index];
        world->shield_held[player_index] =
            scratch->shield_held[player_index];
        world->trigger_input_age[player_index] =
            scratch->trigger_input_age[player_index];
        world->powershield[player_index] =
            scratch->powershield[player_index];
        world->tumble[player_index] =
            scratch->tumble[player_index];
        world->sdi_pulse_count[player_index] =
            scratch->sdi_pulse_count[player_index];
        world->sdi_direction_x[player_index] =
            scratch->sdi_direction_x[player_index];
        world->sdi_direction_y[player_index] =
            scratch->sdi_direction_y[player_index];
        world->tech_direction[player_index] =
            scratch->tech_direction[player_index];
    }
    world->combat_event_sequence = scratch->combat_event_sequence;

    ++world->tick;

    if (forfeit_mask != UINT64_C(0))
    {
        const uint64_t active_mask =
            (UINT64_C(1) << world->player_count) - UINT64_C(1);
        world->terminated = UINT8_C(1);
        if (world->mode == (uint8_t)PF_SIM_MODE_DUEL)
        {
            world->winner_mask =
                (uint8_t)(active_mask & ~forfeit_mask);
        }
        else
        {
            uint8_t forfeiting_teams = UINT8_C(0);

            for (player_index = UINT32_C(0);
                 player_index < (uint32_t)world->player_count;
                 ++player_index)
            {
                if ((forfeit_mask &
                     (UINT64_C(1) << player_index)) != UINT64_C(0))
                {
                    forfeiting_teams |=
                        (uint8_t)(UINT32_C(1) <<
                                  world->team[player_index]);
                }
            }

            world->winner_mask = UINT8_C(0);
            if (forfeiting_teams == UINT8_C(1) ||
                forfeiting_teams == UINT8_C(2))
            {
                const uint8_t winning_team =
                    forfeiting_teams == UINT8_C(1)
                        ? UINT8_C(1)
                        : UINT8_C(0);
                for (player_index = UINT32_C(0);
                     player_index < (uint32_t)world->player_count;
                     ++player_index)
                {
                    if (world->team[player_index] == winning_team)
                    {
                        world->winner_mask |=
                            (uint8_t)(UINT32_C(1) << player_index);
                    }
                }
            }
        }
    }
    if (world->tick >= world->max_ticks)
    {
        world->truncated = UINT8_C(1);
    }

    pf_write_result(world, out_result);
    return PF_STATUS_OK;
}
