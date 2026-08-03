#include "sim_internal.h"

#include <stdint.h>
#include <string.h>

static void pf_write_result(
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    pf_tick_result *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->completed_tick = world->tick;
    result->fault_flags = world->fault_flags;
    result->terminated = world->terminated;
    result->truncated = world->truncated;
    result->winner_mask = world->winner_mask;
    if (scratch != NULL)
    {
        result->event_count = scratch->combat_event_count;
        (void)memcpy(
            result->events,
            scratch->combat_events,
            sizeof(result->events[0]) *
                (size_t)result->event_count);
    }
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

static uint8_t pf_m4_winner_mask_for_team(
    const pf_world_state *world,
    uint8_t team)
{
    uint8_t winner_mask = UINT8_C(0);
    uint32_t player_index;

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        if (world->team[player_index] == team)
        {
            winner_mask |=
                (uint8_t)(UINT32_C(1) << player_index);
        }
    }
    return winner_mask;
}

static pf_status pf_m4_begin_sudden_death(
    pf_sim *sim,
    pf_sim_scratch *scratch,
    uint64_t event_tick)
{
    pf_world_state *world = &sim->world;
    uint32_t player_index;

    if (pf_sim_push_event(
            scratch,
            event_tick,
            PF_SIM_EVENT_SUDDEN_DEATH,
            PF_SIM_EVENT_NO_PLAYER,
            PF_SIM_EVENT_NO_PLAYER,
            UINT32_C(300) * (uint32_t)PF_Q16_ONE,
            INT32_C(0),
            INT32_C(0),
            (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH,
            (uint16_t)world->player_count,
            NULL) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    world->sudden_death = UINT8_C(1);
    world->winner_mask = UINT8_C(0);

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const uint16_t respawn_count =
            world->respawn_count[player_index];

        pf_m4_reset_player(sim, player_index, 0);
        world->respawn_count[player_index] = respawn_count;
        world->stocks_remaining[player_index] = UINT8_C(1);
        world->active[player_index] = UINT8_C(0);
        world->respawn_ticks[player_index] =
            world->respawn_delay_config_ticks != UINT16_C(0)
                ? world->respawn_delay_config_ticks
                : UINT16_C(1);
        world->respawn_invulnerability_ticks[player_index] =
            UINT16_C(0);
        world->grounded[player_index] = UINT8_C(0);
        world->support[player_index] =
            (uint8_t)PF_M4_SURFACE_NONE;
        world->action_state[player_index] =
            (uint8_t)PF_M4_ACTION_RESPAWN_WAIT;
        world->damage_q16[player_index] =
            UINT32_C(300) * (uint32_t)PF_Q16_ONE;
    }
    scratch->stale_move_sync_valid = UINT8_C(0);
    return PF_STATUS_OK;
}

static pf_status pf_m4_resolve_stock_result(
    pf_sim *sim,
    pf_sim_scratch *scratch,
    uint64_t event_tick)
{
    pf_world_state *world = &sim->world;
    uint8_t alive_teams = UINT8_C(0);
    uint32_t player_index;

    if (world->stock_count == UINT8_C(0))
    {
        return PF_STATUS_OK;
    }

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        if (world->stocks_remaining[player_index] != UINT8_C(0))
        {
            alive_teams |=
                (uint8_t)(UINT32_C(1) << world->team[player_index]);
        }
    }

    if (alive_teams != UINT8_C(0) &&
        (alive_teams & (uint8_t)(alive_teams - UINT8_C(1))) ==
            UINT8_C(0))
    {
        uint8_t winning_team = UINT8_C(0);
        uint8_t winner_mask;

        while ((alive_teams &
                (uint8_t)(UINT32_C(1) << winning_team)) ==
               UINT8_C(0))
        {
            ++winning_team;
        }
        winner_mask =
            pf_m4_winner_mask_for_team(world, winning_team);
        if (pf_sim_push_event(
                scratch,
                event_tick,
                PF_SIM_EVENT_MATCH_RESULT,
                PF_SIM_EVENT_NO_PLAYER,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                INT32_C(0),
                INT32_C(0),
                world->sudden_death != UINT8_C(0)
                    ? (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH
                    : UINT16_C(0),
                (uint16_t)winner_mask,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        world->terminated = UINT8_C(1);
        world->winner_mask = winner_mask;
        return PF_STATUS_OK;
    }
    if (alive_teams != UINT8_C(0))
    {
        return PF_STATUS_OK;
    }

    if (world->sudden_death == UINT8_C(0))
    {
        return pf_m4_begin_sudden_death(
            sim,
            scratch,
            event_tick);
    }

    if (pf_sim_push_event(
            scratch,
            event_tick,
            PF_SIM_EVENT_MATCH_RESULT,
            PF_SIM_EVENT_NO_PLAYER,
            PF_SIM_EVENT_NO_PLAYER,
            UINT32_C(0),
            INT32_C(0),
            INT32_C(0),
            (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH,
            (uint16_t)pf_m4_winner_mask_for_team(
                world,
                world->team[0]),
            NULL) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    world->terminated = UINT8_C(1);
    world->winner_mask =
        pf_m4_winner_mask_for_team(world, world->team[0]);
    return PF_STATUS_OK;
}

static pf_status pf_m4_emit_action_transitions(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t event_tick)
{
    uint32_t previous_actions = UINT32_C(0);
    uint32_t next_actions = UINT32_C(0);
    uint16_t changed_mask;
    uint32_t player_index;

    _Static_assert(
        PF_M4_ACTION_REVIVAL_PLATFORM < 128,
        "packed action-transition values must remain nonnegative int32 values");

    if (scratch->action_transition_mask == UINT8_C(0))
    {
        return PF_STATUS_OK;
    }
    changed_mask = (uint16_t)scratch->action_transition_mask;

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const uint32_t shift = player_index * UINT32_C(8);
        const uint8_t previous_action =
            world->action_state[player_index];
        const uint8_t next_action =
            scratch->action_state[player_index];

        previous_actions |= (uint32_t)previous_action << shift;
        next_actions |= (uint32_t)next_action << shift;
    }
    return pf_sim_push_event(
        scratch,
        event_tick,
        PF_SIM_EVENT_ACTION_TRANSITIONS,
        PF_SIM_EVENT_NO_PLAYER,
        PF_SIM_EVENT_NO_PLAYER,
        next_actions,
        (int32_t)previous_actions,
        INT32_C(0),
        UINT16_C(0),
        changed_mask,
        NULL);
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
        pf_write_result(world, NULL, out_result);
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (world->terminated != UINT8_C(0) ||
        world->truncated != UINT8_C(0))
    {
        pf_write_result(world, NULL, out_result);
        return PF_STATUS_EPISODE_DONE;
    }

    status = pf_validate_inputs(world, inputs, player_count);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }
    if (world->combat_event_sequence >
        UINT32_MAX -
            (UINT32_C(2) * (uint32_t)world->player_count +
             UINT32_C(6)))
    {
        world->fault_flags |= (uint32_t)PF_SIM_FAULT_CAPACITY;
        pf_write_result(world, NULL, out_result);
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    scratch->combat_event_sequence = world->combat_event_sequence;
    scratch->combat_event_count = UINT8_C(0);
    (void)memset(
        scratch->combat_events,
        0,
        sizeof(scratch->combat_events));
    if (scratch->stale_move_sync_valid == UINT8_C(0))
    {
        (void)memcpy(
            scratch->stale_move_count,
            world->stale_move_count,
            sizeof(scratch->stale_move_count));
        (void)memcpy(
            scratch->stale_move_ids,
            world->stale_move_ids,
            sizeof(scratch->stale_move_ids));
    }
    scratch->stale_move_sync_valid = UINT8_C(0);
    scratch->stale_move_dirty_mask = UINT8_C(0);
    scratch->action_transition_mask = UINT8_C(0);
    pf_m4_begin_item_tick(world, scratch);
    pf_m4_begin_projectile_tick(world, scratch);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const pf_input_frame *input = &inputs[player_index];
        pf_input_frame charge_input;
        pf_input_frame reflector_input;
        pf_input_frame projectile_input;
        pf_input_frame effective_input;
        pf_m4_prepare_charge_input(
            &sim->content,
            world,
            input,
            player_index,
            &charge_input);
        pf_m4_prepare_reflector_input(
            &sim->content,
            world,
            &charge_input,
            player_index,
            &reflector_input);
        const pf_m4_projectile_input_intent projectile_intent =
            pf_m4_prepare_projectile_input(
                &sim->content,
                world,
                scratch,
                &reflector_input,
                player_index,
                &projectile_input);
        const pf_m4_item_input_intent item_intent =
            pf_m4_prepare_item_input(
                &sim->content,
                world,
                scratch,
                &projectile_input,
                player_index,
                &effective_input);

        if ((input->buttons & PF_INPUT_BUTTON_FORFEIT) != UINT64_C(0))
        {
            forfeit_mask |= UINT64_C(1) << player_index;
        }

        status = pf_m4_step_player(
            &sim->content,
            world,
            scratch,
            &effective_input,
            player_index);
        if (status != PF_STATUS_OK)
        {
            pf_write_result(world, NULL, out_result);
            return status;
        }
        scratch->previous_buttons[player_index] = input->buttons;
        scratch->shield_held[player_index] =
            pf_m4_input_trigger_state(&sim->content.fighter, input);
        if (projectile_intent != PF_M4_PROJECTILE_INPUT_NONE)
        {
            status = pf_m4_apply_projectile_input(
                &sim->content,
                world,
                scratch,
                player_index,
                projectile_intent);
            if (status != PF_STATUS_OK)
            {
                pf_write_result(world, NULL, out_result);
                return status;
            }
        }
        if (item_intent != PF_M4_ITEM_INPUT_NONE)
        {
            status = pf_m4_apply_item_input(
                &sim->content,
                world,
                scratch,
                input,
                player_index,
                item_intent);
            if (status != PF_STATUS_OK)
            {
                pf_write_result(world, NULL, out_result);
                return status;
            }
        }
        pf_m4_track_action_transition(world, scratch, player_index);
    }

    status = pf_m4_step_item(&sim->content, world, scratch);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }

    status = pf_m4_resolve_combat(&sim->content, world, scratch);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }

    status = pf_m4_step_projectile(&sim->content, scratch);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }

    status = pf_m4_emit_action_transitions(
        world,
        scratch,
        world->tick);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
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
        world->respawn_ticks[player_index] =
            scratch->respawn_ticks[player_index];
        world->respawn_invulnerability_ticks[player_index] =
            scratch
                ->respawn_invulnerability_ticks[player_index];
        world->ledge_invulnerability_ticks[player_index] =
            scratch->ledge_invulnerability_ticks[player_index];
        world->ledge_regrab_lockout_ticks[player_index] =
            scratch->ledge_regrab_lockout_ticks[player_index];
        world->grab_escape_ticks[player_index] =
            scratch->grab_escape_ticks[player_index];
        world->charge_ticks[player_index] =
            scratch->charge_ticks[player_index];
        world->smash_charge_ticks[player_index] =
            scratch->smash_charge_ticks[player_index];
        world->shield_strength[player_index] =
            scratch->shield_strength[player_index];
        world->shield_tilt_x[player_index] =
            scratch->shield_tilt_x[player_index];
        world->shield_tilt_y[player_index] =
            scratch->shield_tilt_y[player_index];
        world->grab_target_slot[player_index] =
            scratch->grab_target_slot[player_index];
        world->grab_owner_slot[player_index] =
            scratch->grab_owner_slot[player_index];
        world->previous_buttons[player_index] =
            scratch->previous_buttons[player_index];
        world->grounded[player_index] =
            scratch->grounded[player_index];
        world->active[player_index] =
            scratch->active[player_index];
        world->stocks_remaining[player_index] =
            scratch->stocks_remaining[player_index];
        world->action_state[player_index] =
            scratch->action_state[player_index];
        world->support[player_index] =
            scratch->support[player_index];
        world->air_jumps_remaining[player_index] =
            scratch->air_jumps_remaining[player_index];
        world->recovery_available[player_index] =
            scratch->recovery_available[player_index];
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
        world->previous_dodge_down[player_index] =
            scratch->previous_dodge_down[player_index];
        world->previous_tilt_x_direction[player_index] =
            scratch->previous_tilt_x_direction[player_index];
        world->previous_tilt_y_direction[player_index] =
            scratch->previous_tilt_y_direction[player_index];
        world->tilt_x_age[player_index] =
            scratch->tilt_x_age[player_index];
        world->tilt_y_age[player_index] =
            scratch->tilt_y_age[player_index];
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
        world->attack_stale_registered[player_index] =
            scratch->attack_stale_registered[player_index];
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
        world->prone_orientation[player_index] =
            scratch->prone_orientation[player_index];
    }
    if (scratch->stale_move_dirty_mask != UINT8_C(0))
    {
        for (player_index = UINT32_C(0);
             player_index < PF_SIM_MAX_PLAYERS;
             ++player_index)
        {
            if ((scratch->stale_move_dirty_mask &
                 (uint8_t)(UINT32_C(1) << player_index)) != UINT8_C(0))
            {
                world->stale_move_count[player_index] =
                    scratch->stale_move_count[player_index];
                (void)memcpy(
                    world->stale_move_ids[player_index],
                    scratch->stale_move_ids[player_index],
                    sizeof(world->stale_move_ids[player_index]));
            }
        }
    }
    scratch->stale_move_sync_valid = UINT8_C(1);
    world->item_position_x_q16 = scratch->item_position_x_q16;
    world->item_position_y_q16 = scratch->item_position_y_q16;
    world->item_velocity_x_q16 = scratch->item_velocity_x_q16;
    world->item_velocity_y_q16 = scratch->item_velocity_y_q16;
    world->item_lifetime_ticks = scratch->item_lifetime_ticks;
    world->item_respawn_ticks = scratch->item_respawn_ticks;
    world->item_pickup_lockout_ticks =
        scratch->item_pickup_lockout_ticks;
    world->item_state = scratch->item_state;
    world->item_holder_slot = scratch->item_holder_slot;
    world->item_source_slot = scratch->item_source_slot;
    world->item_hit_mask = scratch->item_hit_mask;
    world->item_stale_registered = scratch->item_stale_registered;
    world->item_throw_direction = scratch->item_throw_direction;
    world->projectile_position_x_q16 =
        scratch->projectile_position_x_q16;
    world->projectile_position_y_q16 =
        scratch->projectile_position_y_q16;
    world->projectile_velocity_x_q16 =
        scratch->projectile_velocity_x_q16;
    world->projectile_velocity_y_q16 =
        scratch->projectile_velocity_y_q16;
    world->projectile_lifetime_ticks =
        scratch->projectile_lifetime_ticks;
    world->projectile_state = scratch->projectile_state;
    world->projectile_owner_slot = scratch->projectile_owner_slot;
    ++world->tick;

    if (forfeit_mask != UINT64_C(0))
    {
        const uint64_t active_mask =
            (UINT64_C(1) << world->player_count) - UINT64_C(1);
        uint8_t winner_mask = UINT8_C(0);

        if (world->mode == (uint8_t)PF_SIM_MODE_DUEL)
        {
            winner_mask = (uint8_t)(active_mask & ~forfeit_mask);
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
                        winner_mask |=
                            (uint8_t)(UINT32_C(1) << player_index);
                    }
                }
            }
        }
        if (pf_sim_push_event(
                scratch,
                world->tick - UINT64_C(1),
                PF_SIM_EVENT_FORFEIT,
                PF_SIM_EVENT_NO_PLAYER,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                INT32_C(0),
                INT32_C(0),
                UINT16_C(0),
                (uint16_t)forfeit_mask,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (pf_sim_push_event(
                scratch,
                world->tick - UINT64_C(1),
                PF_SIM_EVENT_MATCH_RESULT,
                PF_SIM_EVENT_NO_PLAYER,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                INT32_C(0),
                INT32_C(0),
                UINT16_C(0),
                (uint16_t)winner_mask,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        world->terminated = UINT8_C(1);
        world->winner_mask = winner_mask;
    }
    if (world->terminated == UINT8_C(0))
    {
        status = pf_m4_resolve_stock_result(
            sim,
            scratch,
            world->tick - UINT64_C(1));
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    if (world->terminated == UINT8_C(0) &&
        world->tick >= world->max_ticks)
    {
        if (pf_sim_push_event(
                scratch,
                world->tick - UINT64_C(1),
                PF_SIM_EVENT_TIME_LIMIT,
                PF_SIM_EVENT_NO_PLAYER,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                INT32_C(0),
                INT32_C(0),
                UINT16_C(0),
                UINT16_C(0),
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        world->truncated = UINT8_C(1);
    }

    world->combat_event_sequence = scratch->combat_event_sequence;
    pf_write_result(world, scratch, out_result);
    return PF_STATUS_OK;
}
