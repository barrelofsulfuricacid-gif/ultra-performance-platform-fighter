#include "sim_internal.h"
#include "sim_falcon_frame_data.h"
#include "sim_ssbm_stage_data.h"

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
        if (input->player_slot != (uint8_t)player_index ||
            (input->buttons & ~PF_INPUT_FRAME_KNOWN_BITS) != UINT64_C(0) ||
            !pf_input_raw_payload_valid(input))
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

static uint8_t winner_mask_for_team(
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

static pf_status begin_sudden_death(
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
            UINT32_C(300) * (uint32_t)1.0f,
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

        reset_player(sim, player_index, 0);
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
        world->damage_f32[player_index] =
            UINT32_C(300) * (uint32_t)1.0f;
    }
    scratch->stale_move_sync_valid = UINT8_C(0);
    return PF_STATUS_OK;
}

static pf_input_frame reference_priority_input(
    const struct content *content,
    const pf_world_state *world,
    const pf_input_frame *input,
    uint32_t player_index)
{
    pf_input_frame result = *input;
    const int attack_edge =
        (input->buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0) &&
        (world->previous_buttons[player_index] &
         PF_INPUT_BUTTON_ATTACK) == UINT64_C(0);
    const int shield_held =
        input->left_trigger >=
            content->fighter.light_shield_trigger_threshold ||
        input->right_trigger >=
            content->fighter.light_shield_trigger_threshold;

    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_WALK &&
        attack_edge != 0 && shield_held != 0)
    {
        /* Walk_IASA checks Catch before SpecialS and returns immediately.
         * Clear B before the charge/reflector/projectile intent pipeline so
         * those generic frontends observe the same source callback result. */
        result.buttons &= ~PF_INPUT_BUTTON_SPECIAL;
    }
    return result;
}

static int abs_raw_axis(int8_t axis)
{
    return axis < INT8_C(0) ? -(int)axis : (int)axis;
}

static void apply_ucf084_cardinals(pf_input_frame *input)
{
    const pf_input_raw_pad raw = pf_input_get_raw_pad(input);

    if (abs_raw_axis(raw.main_stick_x) >= 80 &&
        abs_raw_axis(raw.main_stick_y) <= 6)
    {
        input->main_stick_x =
            raw.main_stick_x < INT8_C(0) ? INT16_MIN : INT16_MAX;
        input->main_stick_y = INT16_C(0);
    }
    else if (abs_raw_axis(raw.main_stick_y) >= 80 &&
             abs_raw_axis(raw.main_stick_x) <= 6)
    {
        input->main_stick_x = INT16_C(0);
        input->main_stick_y =
            raw.main_stick_y > INT8_C(0) ? INT16_MIN : INT16_MAX;
    }

    if (abs_raw_axis(raw.secondary_stick_x) >= 80 &&
        abs_raw_axis(raw.secondary_stick_y) <= 6)
    {
        input->secondary_stick_x =
            raw.secondary_stick_x < INT8_C(0) ? INT16_MIN : INT16_MAX;
        input->secondary_stick_y = INT16_C(0);
    }
    else if (abs_raw_axis(raw.secondary_stick_y) >= 80 &&
             abs_raw_axis(raw.secondary_stick_x) <= 6)
    {
        input->secondary_stick_x = INT16_C(0);
        input->secondary_stick_y =
            raw.secondary_stick_y > INT8_C(0) ? INT16_MIN : INT16_MAX;
    }
}

static uint8_t ucf084_pad_buffer_count(
    const pf_input_frame *input,
    pf_input_raw_pad current_raw,
    int8_t raw_main_t2_y,
    uint8_t previous_tilt_y_age,
    uint8_t previous_count)
{
    const int raw_delta_y =
        (int)current_raw.main_stick_y - (int)raw_main_t2_y;
    const int down_qualifies =
        (int32_t)input->main_stick_y * INT32_C(64) >=
        INT32_C(39) * INT32_C(32767);
    const int radial_qualifies = ucf084_adjusted_radial_qualifies(
        input->main_stick_x,
        input->main_stick_y);

    if (down_qualifies == 0 || radial_qualifies == 0)
    {
        return UINT8_C(0);
    }
    if (previous_count != UINT8_C(0) ||
        (previous_tilt_y_age <= UINT8_C(1) &&
         raw_delta_y * raw_delta_y > 44 * 44))
    {
        return (uint8_t)(previous_count + UINT8_C(1));
    }
    return UINT8_C(0);
}

static float player_nudge_x_f32(
    const struct content *content,
    const pf_world_state *world,
    uint32_t player_index)
{
    const fighter_data *fighter = &content->fighter;
    const uint8_t action_state = world->action_state[player_index];
    const float overlap_distance_f32 =
        INT64_C(2) * fighter->player_push_half_width_f32;
    float nudge_x_f32 = INT32_C(0);
    uint32_t other_index;

    if (world->active[player_index] == UINT8_C(0) ||
        world->grounded[player_index] == UINT8_C(0) ||
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_RESPAWN_WAIT ||
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_ELIMINATED ||
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM ||
        world->support[player_index] ==
            (uint8_t)PF_M4_SURFACE_NONE ||
        world->support[player_index] ==
            (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM ||
        world->grab_target_slot[player_index] != UINT8_C(0) ||
        world->grab_owner_slot[player_index] != UINT8_C(0) ||
        (content->gameplay_ruleset ==
             (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084 &&
         (action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
          action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD ||
          action_state == (uint8_t)PF_M4_ACTION_SPOT_DODGE)))
    {
        return INT32_C(0);
    }

    /*
     * SSBM ftCommon_8007DD7C accumulates one fixed horizontal nudge for
     * every grounded fighter whose character push radii overlap on the same
     * or an immediately adjacent collision line.
     */
    for (other_index = UINT32_C(0);
         other_index < (uint32_t)world->player_count;
         ++other_index)
    {
        float delta_x_f32;

        if (other_index == player_index ||
            world->active[other_index] == UINT8_C(0) ||
            world->grounded[other_index] == UINT8_C(0) ||
            world->action_state[other_index] ==
                (uint8_t)PF_M4_ACTION_RESPAWN_WAIT ||
            world->action_state[other_index] ==
                (uint8_t)PF_M4_ACTION_ELIMINATED ||
            world->action_state[other_index] ==
                (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM ||
            world->grab_target_slot[other_index] !=
                UINT8_C(0))
        {
            continue;
        }

        if (world->support[other_index] != world->support[player_index])
        {
            const ssbm_stage_collision_profile *profile =
                ssbm_reference_stage_collision(
                    content->stage.reference_collision_profile);
            const ssbm_stage_collision_line *line =
                ssbm_reference_stage_line(
                    content->stage.reference_collision_profile,
                    world->support[player_index]);
            const ssbm_stage_collision_line *other_line =
                ssbm_reference_stage_line(
                    content->stage.reference_collision_profile,
                    world->support[other_index]);
            int16_t other_line_index;

            if (profile == NULL || line == NULL || other_line == NULL ||
                other_line < profile->lines ||
                other_line >= profile->lines + profile->line_count)
            {
                continue;
            }
            other_line_index = (int16_t)(other_line - profile->lines);
            if (line->previous_line != other_line_index &&
                line->next_line != other_line_index)
            {
                continue;
            }
        }

        delta_x_f32 =
            world->position_x_f32[player_index] -
            world->position_x_f32[other_index];
        if (delta_x_f32 <= -overlap_distance_f32 ||
            delta_x_f32 >= overlap_distance_f32)
        {
            continue;
        }

        if (delta_x_f32 < 0.0f ||
            (delta_x_f32 == 0.0f &&
             player_index < other_index))
        {
            nudge_x_f32 -= fighter->player_push_speed_f32;
        }
        else
        {
            nudge_x_f32 += fighter->player_push_speed_f32;
        }
    }

    return nudge_x_f32;
}

static pf_status resolve_stock_result(
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
            winner_mask_for_team(world, winning_team);
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
        return begin_sudden_death(
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
            (uint16_t)winner_mask_for_team(
                world,
                world->team[0]),
            NULL) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    world->terminated = UINT8_C(1);
    world->winner_mask =
        winner_mask_for_team(world, world->team[0]);
    return PF_STATUS_OK;
}

static pf_status emit_action_transitions(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t event_tick)
{
    uint32_t previous_actions = UINT32_C(0);
    uint32_t next_actions = UINT32_C(0);
    uint16_t changed_mask;
    uint32_t player_index;

    _Static_assert(
        PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW <= UINT8_MAX,
        "packed action-transition values must fit one byte per player");

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
        pf_sim_f32_from_bits(next_actions),
        pf_sim_f32_from_bits(previous_actions),
        0.0f,
        UINT16_C(0),
        changed_mask,
        NULL);
}

static void canonicalize_source_animation_state(
    const fighter_data *fighter,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    uint32_t player_index;

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        if (scratch->active[player_index] == UINT8_C(0))
        {
            scratch->source_submotion[player_index] = UINT16_C(0);
            scratch->source_animation_frame_f32[player_index] = INT32_C(0);
            scratch->source_animation_rate_f32[player_index] = INT32_C(0);
            scratch->fall_animation_blend_f32[player_index] = INT32_C(0);
            scratch->fall_animation_target_switched[player_index] =
                UINT8_C(0);
            scratch->ecb_bottom_lock_ticks[player_index] = UINT8_C(0);
            scratch->ecb_locked_bottom_y_f32[player_index] = INT32_C(0);
            (void)memset(
                &scratch->ground_blend_pose[player_index],
                0,
                sizeof(scratch->ground_blend_pose[player_index]));
            scratch->ground_blend_progress_f32[player_index] = INT32_C(0);
            continue;
        }
        if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
            action_uses_direct_hsd_pose(
                effective_action_state(
                    scratch->action_state[player_index],
                    scratch->hitlag_resume_action[player_index])))
        {
            uint16_t canonical_submotion = UINT16_C(0);
            float ignored_frame_f32 = 0.0f;

            if (falcon_reference_direct_hsd_pose(
                    effective_action_state(
                        scratch->action_state[player_index],
                        scratch->hitlag_resume_action[player_index]),
                    scratch->action_ticks[player_index],
                    scratch->grounded[player_index],
                    &canonical_submotion,
                    &ignored_frame_f32))
            {
                scratch->source_submotion[player_index] =
                    canonical_submotion;
            }
        }
        if ((fighter->reference_frame_data_enabled == UINT8_C(0) &&
             action_uses_source_animation_clock(
                 scratch->action_state[player_index],
                 scratch->hitlag_resume_action[player_index])) ||
            !action_retains_source_submotion(
                scratch->action_state[player_index],
                scratch->hitlag_resume_action[player_index]))
        {
            scratch->source_submotion[player_index] =
                (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
        }
        if (!action_uses_source_animation_clock(
                scratch->action_state[player_index],
                scratch->hitlag_resume_action[player_index]))
        {
            scratch->source_animation_frame_f32[player_index] = INT32_C(0);
            scratch->source_animation_rate_f32[player_index] = INT32_C(0);
        }
        if (effective_action_state(
                scratch->action_state[player_index],
                scratch->hitlag_resume_action[player_index]) !=
                (uint8_t)PF_M4_ACTION_AIRBORNE &&
            !action_uses_fall_special_pose(
                effective_action_state(
                    scratch->action_state[player_index],
                    scratch->hitlag_resume_action[player_index])))
        {
            scratch->fall_animation_blend_f32[player_index] = INT32_C(0);
            scratch->fall_animation_target_switched[player_index] =
                UINT8_C(0);
        }
        if (!action_uses_ground_animation_clock(
                scratch->action_state[player_index],
                scratch->hitlag_resume_action[player_index]) &&
            !(effective_action_state(
                  scratch->action_state[player_index],
                  scratch->hitlag_resume_action[player_index]) ==
                  (uint8_t)PF_M4_ACTION_SHIELD &&
              scratch->source_submotion[player_index] ==
                  (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON))
        {
            (void)memset(
                &scratch->ground_blend_pose[player_index],
                0,
                sizeof(scratch->ground_blend_pose[player_index]));
            scratch->ground_blend_progress_f32[player_index] = INT32_C(0);
        }
    }
}

static pf_input_frame reference_match_fighter_input(
    const pf_input_frame *input,
    uint8_t input_lock_ticks)
{
    pf_input_frame effective = *input;

    if (input_lock_ticks != UINT8_C(0))
    {
        effective.buttons = UINT64_C(0);
        effective.main_stick_x = INT16_C(0);
        effective.main_stick_y = INT16_C(0);
        effective.secondary_stick_x = INT16_C(0);
        effective.secondary_stick_y = INT16_C(0);
        effective.left_trigger = UINT16_C(0);
        effective.right_trigger = UINT16_C(0);
        effective.raw_axis_valid_mask = UINT8_C(0);
    }
    return effective;
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
    uint64_t rng_state;
    float player_nudge_x_f32_value[PF_SIM_MAX_PLAYERS] = {0.0f};
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
    rng_state = world->rng_state;

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
    (void)memcpy(
        scratch->match_kos,
        world->match_kos,
        sizeof(scratch->match_kos));
    (void)memcpy(
        scratch->match_falls,
        world->match_falls,
        sizeof(scratch->match_falls));
    scratch->shield_recoil_mask = world->shield_recoil_mask;
    if (world->shield_recoil_mask != UINT8_C(0))
    {
        (void)memcpy(
            scratch->shield_recoil_x_f32,
            world->shield_recoil_x_f32,
            sizeof(scratch->shield_recoil_x_f32));
    }
    begin_item_tick(world, scratch);
    begin_projectile_tick(world, scratch);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        player_nudge_x_f32_value[player_index] =
            player_nudge_x_f32(
                &sim->content,
                world,
                player_index);
    }
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const pf_input_frame *input = &inputs[player_index];
        pf_input_frame source_input = *input;
        const int8_t previous_tilt_x_direction =
            world->previous_tilt_x_direction[player_index];
        const int8_t previous_tilt_y_direction =
            world->previous_tilt_y_direction[player_index];
        int8_t input_tilt_x_direction;
        int8_t input_tilt_y_direction;
        uint8_t input_tilt_x_age;
        uint8_t input_tilt_y_age;
        uint8_t ucf_tilt_x_age;
        uint8_t ucf_tilt_y_age;
        uint8_t ucf_pad_buffer_count;
        pf_input_raw_pad current_raw;
        pf_input_raw_pad previous_raw;

        pf_input_resolve_raw_pad(&source_input);
        current_raw = pf_input_get_raw_pad(&source_input);
        previous_raw.main_stick_x = pf_input_decode_raw_axis((uint8_t)(
            (world->previous_buttons[player_index] >>
             PF_INPUT_RAW_MAIN_X_SHIFT) & PF_INPUT_RAW_AXIS_MASK));
        previous_raw.main_stick_y = pf_input_decode_raw_axis((uint8_t)(
            (world->previous_buttons[player_index] >>
             PF_INPUT_RAW_MAIN_Y_SHIFT) & PF_INPUT_RAW_AXIS_MASK));
        previous_raw.secondary_stick_x = INT8_C(0);
        previous_raw.secondary_stick_y = INT8_C(0);
        input_tilt_x_age = source_stick_age(
            source_input.main_stick_x,
            sim->content.fighter.tilt_axis_threshold,
            previous_tilt_x_direction,
            world->tilt_x_age[player_index],
            &input_tilt_x_direction);
        input_tilt_y_age = source_stick_age(
            source_input.main_stick_y,
            sim->content.fighter.tilt_axis_threshold,
            previous_tilt_y_direction,
            world->tilt_y_age[player_index],
            &input_tilt_y_direction);
        ucf_tilt_x_age = source_stick_age(
            source_input.main_stick_x,
            sim->content.fighter.tilt_axis_threshold,
            previous_tilt_x_direction,
            world->ucf_tilt_x_age[player_index],
            &input_tilt_x_direction);
        ucf_tilt_y_age = source_stick_age(
            source_input.main_stick_y,
            sim->content.fighter.tilt_axis_threshold,
            previous_tilt_y_direction,
            world->ucf_tilt_y_age[player_index],
            &input_tilt_y_direction);
        if (sim->content.gameplay_ruleset ==
            (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084)
        {
            apply_ucf084_cardinals(&source_input);
        }
        ucf_pad_buffer_count =
            sim->content.gameplay_ruleset ==
                    (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084
                ? ucf084_pad_buffer_count(
                      &source_input,
                      current_raw,
                      world->raw_main_t2_y[player_index],
                      input_tilt_y_age,
                      world->ucf_pad_buffer_count[player_index])
                : UINT8_C(0);
        const pf_input_frame fighter_input =
            reference_match_fighter_input(
                &source_input,
                world->reference_match_input_lock_ticks);
        const pf_input_frame priority_input =
            reference_priority_input(
                &sim->content,
                world,
                &fighter_input,
                player_index);
        pf_input_frame charge_input;
        pf_input_frame reflector_input;
        pf_input_frame projectile_input;
        pf_input_frame effective_input;
        prepare_charge_input(
            &sim->content,
            world,
            &priority_input,
            player_index,
            &charge_input);
        prepare_reflector_input(
            &sim->content,
            world,
            &charge_input,
            player_index,
            &reflector_input);
        const projectile_input_intent projectile_intent =
            prepare_projectile_input(
                &sim->content,
                world,
                scratch,
                &reflector_input,
                player_index,
                &projectile_input);
        const item_input_intent item_intent =
            prepare_item_input(
                &sim->content,
                world,
                scratch,
                &projectile_input,
                player_index,
                &effective_input);

        /* These values are updated by UCF's pad hook before the ordinary
         * per-action callbacks. Keep them visible to those callbacks; the
         * movement step owns any technique-specific x670/x671 resets only. */
        scratch->ucf_tilt_x_age[player_index] = ucf_tilt_x_age;
        scratch->ucf_tilt_y_age[player_index] = ucf_tilt_y_age;
        scratch->tilt_x_age[player_index] = input_tilt_x_age;
        scratch->tilt_y_age[player_index] = input_tilt_y_age;
        scratch->previous_tilt_x_direction[player_index] =
            input_tilt_x_direction;
        scratch->previous_tilt_y_direction[player_index] =
            input_tilt_y_direction;
        scratch->raw_main_t2_x[player_index] =
            world->raw_main_t2_x[player_index];
        scratch->raw_main_t2_y[player_index] =
            world->raw_main_t2_y[player_index];
        scratch->ucf_pad_buffer_count[player_index] =
            ucf_pad_buffer_count;

        if ((input->buttons & PF_INPUT_BUTTON_FORFEIT) != UINT64_C(0))
        {
            forfeit_mask |= UINT64_C(1) << player_index;
        }

        status = step_player(
            &sim->content,
            world,
            scratch,
            &effective_input,
            &priority_input,
            player_index,
            player_nudge_x_f32_value[player_index],
            &rng_state);
        if (status != PF_STATUS_OK)
        {
            pf_write_result(world, NULL, out_result);
            return status;
        }
        /* Fighter_UpdateInputTimer increments x18ac only while the fighter is
         * outside hitlag.  Use the action at the start of this update: the
         * tick that releases hitlag is still paused in Melee because the
         * hitlag flag is cleared later in the fighter callback pipeline. */
        if (world->last_hit_sequence[player_index] != UINT32_C(0) &&
            scratch->last_hit_sequence[player_index] != UINT32_C(0) &&
            world->action_state[player_index] !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            scratch->damage_time_since_hit_ticks[player_index] <
                UINT8_C(254))
        {
            ++scratch->damage_time_since_hit_ticks[player_index];
        }
        scratch->previous_buttons[player_index] = source_input.buttons;
        scratch->previous_main_stick_x[player_index] =
            source_input.main_stick_x;
        scratch->previous_main_stick_y[player_index] =
            source_input.main_stick_y;
        scratch->previous_tilt_x_direction[player_index] =
            source_stick_direction(
                source_input.main_stick_x,
                sim->content.fighter.tilt_axis_threshold);
        scratch->previous_tilt_y_direction[player_index] =
            source_stick_direction(
                source_input.main_stick_y,
                sim->content.fighter.tilt_axis_threshold);
        scratch->ucf_tilt_x_age[player_index] = ucf_tilt_x_age;
        scratch->ucf_tilt_y_age[player_index] = ucf_tilt_y_age;
        scratch->raw_main_t2_x[player_index] = previous_raw.main_stick_x;
        scratch->raw_main_t2_y[player_index] = previous_raw.main_stick_y;
        scratch->ucf_pad_buffer_count[player_index] =
            ucf_pad_buffer_count;
        scratch->shield_held[player_index] =
            input_trigger_state(&sim->content.fighter, &source_input);
        if (projectile_intent != PF_M4_PROJECTILE_INPUT_NONE)
        {
            status = apply_projectile_input(
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
            status = apply_item_input(
                &sim->content,
                world,
                scratch,
                &source_input,
                player_index,
                item_intent);
            if (status != PF_STATUS_OK)
            {
                pf_write_result(world, NULL, out_result);
                return status;
            }
        }
        track_action_transition(world, scratch, player_index);
    }

    status = step_item(&sim->content, world, scratch);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }

    status = resolve_combat(
        &sim->content, world, scratch, &rng_state);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }

    status = step_projectile(&sim->content, scratch);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, NULL, out_result);
        return status;
    }

    canonicalize_source_animation_state(
        &sim->content.fighter,
        world,
        scratch);

    status = emit_action_transitions(
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
        world->position_x_f32[player_index] =
            scratch->position_x_f32[player_index];
        world->position_y_f32[player_index] =
            scratch->position_y_f32[player_index];
        world->velocity_x_f32[player_index] =
            scratch->velocity_x_f32[player_index];
        world->velocity_y_f32[player_index] =
            scratch->velocity_y_f32[player_index];
        world->match_kos[player_index] =
            scratch->match_kos[player_index];
        world->match_falls[player_index] =
            scratch->match_falls[player_index];
        world->action_ticks[player_index] =
            scratch->action_ticks[player_index];
        world->source_submotion[player_index] =
            scratch->source_submotion[player_index];
        world->source_animation_frame_f32[player_index] =
            scratch->source_animation_frame_f32[player_index];
        world->source_animation_rate_f32[player_index] =
            scratch->source_animation_rate_f32[player_index];
        world->fall_animation_blend_f32[player_index] =
            scratch->fall_animation_blend_f32[player_index];
        world->fall_animation_target_switched[player_index] =
            scratch->fall_animation_target_switched[player_index];
        world->ecb_bottom_lock_ticks[player_index] =
            scratch->ecb_bottom_lock_ticks[player_index];
        world->ecb_locked_bottom_y_f32[player_index] =
            scratch->ecb_locked_bottom_y_f32[player_index];
        world->ground_blend_pose[player_index] =
            scratch->ground_blend_pose[player_index];
        world->ground_blend_progress_f32[player_index] =
            scratch->ground_blend_progress_f32[player_index];
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
        world->damage_jump_buffer_ticks[player_index] =
            scratch->damage_jump_buffer_ticks[player_index];
        world->charge_ticks[player_index] =
            scratch->charge_ticks[player_index];
        world->smash_charge_ticks[player_index] =
            scratch->smash_charge_ticks[player_index];
        world->shield_strength[player_index] =
            scratch->shield_strength[player_index];
        world->shield_angle_turn[player_index] =
            scratch->shield_angle_turn[player_index];
        world->shield_magnitude[player_index] =
            scratch->shield_magnitude[player_index];
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
        world->crouch_pass_pending_ticks[player_index] =
            scratch->crouch_pass_pending_ticks[player_index];
        world->fast_fall[player_index] =
            scratch->fast_fall[player_index];
        world->facing[player_index] =
            scratch->facing[player_index];
        world->dash_direction[player_index] =
            scratch->dash_direction[player_index];
        world->previous_strong_direction[player_index] =
            scratch->previous_strong_direction[player_index];
        world->previous_directional_input_flags[player_index] =
            scratch->previous_directional_input_flags[player_index];
        world->previous_tilt_x_direction[player_index] =
            scratch->previous_tilt_x_direction[player_index];
        world->previous_tilt_y_direction[player_index] =
            scratch->previous_tilt_y_direction[player_index];
        world->mash_stick_x_direction[player_index] =
            scratch->mash_stick_x_direction[player_index];
        world->mash_stick_y_direction[player_index] =
            scratch->mash_stick_y_direction[player_index];
        world->previous_secondary_stick_x[player_index] =
            scratch->previous_secondary_stick_x[player_index];
        world->previous_secondary_stick_y[player_index] =
            scratch->previous_secondary_stick_y[player_index];
        world->previous_main_stick_x[player_index] =
            scratch->previous_main_stick_x[player_index];
        world->previous_main_stick_y[player_index] =
            scratch->previous_main_stick_y[player_index];
        world->tilt_x_age[player_index] =
            scratch->tilt_x_age[player_index];
        world->tilt_y_age[player_index] =
            scratch->tilt_y_age[player_index];
        world->ucf_tilt_x_age[player_index] =
            scratch->ucf_tilt_x_age[player_index];
        world->ucf_tilt_y_age[player_index] =
            scratch->ucf_tilt_y_age[player_index];
        world->raw_main_t2_x[player_index] =
            scratch->raw_main_t2_x[player_index];
        world->raw_main_t2_y[player_index] =
            scratch->raw_main_t2_y[player_index];
        world->ucf_pad_buffer_count[player_index] =
            scratch->ucf_pad_buffer_count[player_index];
        world->horizontal_input_age[player_index] =
            scratch->horizontal_input_age[player_index];
        world->horizontal_input_direction[player_index] =
            scratch->horizontal_input_direction[player_index];
        world->damage_f32[player_index] =
            scratch->damage_f32[player_index];
        world->knockback_velocity_x_f32[player_index] =
            scratch->knockback_velocity_x_f32[player_index];
        world->knockback_velocity_y_f32[player_index] =
            scratch->knockback_velocity_y_f32[player_index];
        world->ground_knockback_velocity_f32[player_index] =
            scratch->ground_knockback_velocity_f32[player_index];
        world->last_hit_sequence[player_index] =
            scratch->last_hit_sequence[player_index];
        world->last_hit_tick[player_index] =
            scratch->last_hit_tick[player_index];
        world->last_hit_damage_f32[player_index] =
            scratch->last_hit_damage_f32[player_index];
        world->damage_time_since_hit_ticks[player_index] =
            scratch->damage_time_since_hit_ticks[player_index];
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
        world->shield_health_f32[player_index] =
            scratch->shield_health_f32[player_index];
        world->hitlag_resume_action[player_index] =
            scratch->hitlag_resume_action[player_index];
        world->attack_hit_mask[player_index] =
            scratch->attack_hit_mask[player_index];
        world->attack_stale_registered[player_index] =
            scratch->attack_stale_registered[player_index];
        world->falcon_kick_hit_count[player_index] =
            scratch->falcon_kick_hit_count[player_index];
        world->rebound_duration_ticks[player_index] =
            scratch->rebound_duration_ticks[player_index];
        world->jab_chain_buffered[player_index] =
            scratch->jab_chain_buffered[player_index];
        world->rapid_jab_input_count[player_index] =
            scratch->rapid_jab_input_count[player_index];
        world->rapid_jab_continue[player_index] =
            scratch->rapid_jab_continue[player_index];
        world->down_tilt_repeat_buffered[player_index] =
            scratch->down_tilt_repeat_buffered[player_index];
        world->last_hit_attacker[player_index] =
            scratch->last_hit_attacker[player_index];
        world->shield_held[player_index] =
            scratch->shield_held[player_index];
        world->trigger_input_age[player_index] =
            scratch->trigger_input_age[player_index];
        world->prone_attack_input_age[player_index] =
            scratch->prone_attack_input_age[player_index];
        world->up_special_input_age[player_index] =
            scratch->up_special_input_age[player_index];
        world->powershield[player_index] =
            scratch->powershield[player_index];
        world->guard_dash_grab_window_ticks[player_index] =
            scratch->guard_dash_grab_window_ticks[player_index];
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
        world->prone_roll_motion_orientation[player_index] =
            scratch->prone_roll_motion_orientation[player_index];
    }
    if ((world->shield_recoil_mask | scratch->shield_recoil_mask) !=
        UINT8_C(0))
    {
        for (player_index = UINT32_C(0);
             player_index < PF_SIM_MAX_PLAYERS;
             ++player_index)
        {
            const uint8_t recoil_bit =
                (uint8_t)(UINT8_C(1) << player_index);

            world->shield_recoil_x_f32[player_index] =
                (scratch->shield_recoil_mask & recoil_bit) != UINT8_C(0)
                    ? scratch->shield_recoil_x_f32[player_index]
                    : INT32_C(0);
        }
    }
    world->shield_recoil_mask = scratch->shield_recoil_mask;
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
    world->item_position_x_f32 = scratch->item_position_x_f32;
    world->item_position_y_f32 = scratch->item_position_y_f32;
    world->item_velocity_x_f32 = scratch->item_velocity_x_f32;
    world->item_velocity_y_f32 = scratch->item_velocity_y_f32;
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
    world->projectile_position_x_f32 =
        scratch->projectile_position_x_f32;
    world->projectile_position_y_f32 =
        scratch->projectile_position_y_f32;
    world->projectile_velocity_x_f32 =
        scratch->projectile_velocity_x_f32;
    world->projectile_velocity_y_f32 =
        scratch->projectile_velocity_y_f32;
    world->projectile_lifetime_ticks =
        scratch->projectile_lifetime_ticks;
    world->projectile_state = scratch->projectile_state;
    world->projectile_owner_slot = scratch->projectile_owner_slot;
    world->rng_state = rng_state;
    if (world->reference_match_input_lock_ticks != UINT8_C(0))
    {
        --world->reference_match_input_lock_ticks;
    }
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
        status = resolve_stock_result(
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
