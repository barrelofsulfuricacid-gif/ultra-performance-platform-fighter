#include "sim_internal.h"
#include "sim_falcon_frame_data.h"

#include <math.h>

static int projectile_checked_add(
    float left,
    float right,
    float *out_value)
{
    const float value = left + right;

    if (out_value == NULL || !isfinite(value))
    {
        return 0;
    }
    *out_value = value;
    return 1;
}

static int projectile_action_can_fire(
    uint8_t grounded,
    uint8_t action_state)
{
    if (grounded != UINT8_C(0))
    {
        return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
               action_state == (uint8_t)PF_M4_ACTION_WALK ||
               action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
               action_state == (uint8_t)PF_M4_ACTION_RUN ||
               action_state == (uint8_t)PF_M4_ACTION_CROUCH_START;
    }
    return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP;
}

static void projectile_clear(pf_sim_scratch *scratch)
{
    scratch->projectile_position_x_f32 = INT32_C(0);
    scratch->projectile_position_y_f32 = INT32_C(0);
    scratch->projectile_velocity_x_f32 = INT32_C(0);
    scratch->projectile_velocity_y_f32 = INT32_C(0);
    scratch->projectile_lifetime_ticks = UINT16_C(0);
    scratch->projectile_state =
        (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE;
    scratch->projectile_owner_slot = UINT8_C(0);
}

void reset_projectile(pf_sim *sim)
{
    if (sim == NULL)
    {
        return;
    }
    sim->world.projectile_position_x_f32 = INT32_C(0);
    sim->world.projectile_position_y_f32 = INT32_C(0);
    sim->world.projectile_velocity_x_f32 = INT32_C(0);
    sim->world.projectile_velocity_y_f32 = INT32_C(0);
    sim->world.projectile_lifetime_ticks = UINT16_C(0);
    sim->world.projectile_state =
        (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE;
    sim->world.projectile_owner_slot = UINT8_C(0);
}

void begin_projectile_tick(
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    if (world == NULL || scratch == NULL)
    {
        return;
    }
    scratch->projectile_position_x_f32 =
        world->projectile_position_x_f32;
    scratch->projectile_position_y_f32 =
        world->projectile_position_y_f32;
    scratch->projectile_velocity_x_f32 =
        world->projectile_velocity_x_f32;
    scratch->projectile_velocity_y_f32 =
        world->projectile_velocity_y_f32;
    scratch->projectile_lifetime_ticks =
        world->projectile_lifetime_ticks;
    scratch->projectile_state = world->projectile_state;
    scratch->projectile_owner_slot = world->projectile_owner_slot;
}

projectile_input_intent prepare_projectile_input(
    const struct content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input)
{
    const uint8_t player_slot = (uint8_t)(player_index + UINT32_C(1));
    int special_pressed;

    if (content == NULL || world == NULL || scratch == NULL ||
        input == NULL || effective_input == NULL ||
        player_index >= (uint32_t)world->player_count)
    {
        return PF_M4_PROJECTILE_INPUT_NONE;
    }
    *effective_input = *input;
    special_pressed =
        (input->buttons & PF_INPUT_BUTTON_SPECIAL) != UINT64_C(0) &&
        (world->previous_buttons[player_index] &
         PF_INPUT_BUTTON_SPECIAL) == UINT64_C(0);
    if (special_pressed != 0 &&
        content->fighter.reference_frame_data_enabled != UINT8_C(0))
    {
        /* The reference fighter's complete SpecialN/S/Hi/Lw dispatcher lives
         * in movement. A low non-neutral axis is still Falcon Punch when it
         * misses every directional threshold; it must not leak into the
         * authored Pulse Bolt frontend. */
        return PF_M4_PROJECTILE_INPUT_NONE;
    }
    if (special_pressed != 0 &&
        (input->main_stick_y >=
             (int16_t)content->fighter.crouch_axis_threshold ||
         input->main_stick_y <=
             -(int16_t)content->fighter.dash_axis_threshold))
    {
        return PF_M4_PROJECTILE_INPUT_NONE;
    }
    if (content->projectile.enabled == UINT8_C(0) ||
        special_pressed == 0 ||
        scratch->projectile_state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE ||
        world->active[player_index] == UINT8_C(0) ||
        world->hitlag_ticks[player_index] != UINT16_C(0) ||
        world->tumble[player_index] != UINT8_C(0) ||
        (scratch->item_state == (uint8_t)PF_M4_ITEM_STATE_HELD &&
         scratch->item_holder_slot == player_slot) ||
        (!projectile_action_can_fire(
             world->grounded[player_index],
             world->action_state[player_index]) &&
         !falcon_reference_special_iasa_active(
             world->action_state[player_index],
             world->action_ticks[player_index])))
    {
        effective_input->buttons &= ~PF_INPUT_BUTTON_SPECIAL;
        return PF_M4_PROJECTILE_INPUT_NONE;
    }
    return PF_M4_PROJECTILE_INPUT_FIRE;
}

pf_status apply_projectile_input(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    projectile_input_intent intent)
{
    const projectile_data *projectile;
    const uint8_t player_slot = (uint8_t)(player_index + UINT32_C(1));
    const uint8_t fire_action =
        world != NULL && player_index < (uint32_t)world->player_count &&
                world->grounded[player_index] != UINT8_C(0)
            ? (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND
            : (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR;
    float offset_x;
    float position_x;
    float position_y;

    if (content == NULL || world == NULL || scratch == NULL ||
        player_index >= (uint32_t)world->player_count ||
        intent != PF_M4_PROJECTILE_INPUT_FIRE)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    projectile = &content->projectile;
    if (projectile->enabled == UINT8_C(0) ||
        scratch->projectile_state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE ||
        scratch->active[player_index] == UINT8_C(0) ||
        (scratch->action_state[player_index] != fire_action &&
         !(fire_action ==
               (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR &&
           scratch->grounded[player_index] != UINT8_C(0) &&
           scratch->action_state[player_index] ==
               (uint8_t)PF_M4_ACTION_LANDING)))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    offset_x =
        (float)scratch->facing[player_index] *
        projectile->spawn_offset_x_f32;
    if (!projectile_checked_add(
            scratch->position_x_f32[player_index],
            offset_x,
            &position_x) ||
        !projectile_checked_add(
            scratch->position_y_f32[player_index],
            projectile->spawn_offset_y_f32,
            &position_y))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->projectile_position_x_f32 = position_x;
    scratch->projectile_position_y_f32 = position_y;
    scratch->projectile_velocity_x_f32 =
        (float)scratch->facing[player_index] * projectile->speed_f32;
    scratch->projectile_velocity_y_f32 = INT32_C(0);
    scratch->projectile_lifetime_ticks = projectile->lifetime_ticks;
    scratch->projectile_state =
        (uint8_t)PF_M4_PROJECTILE_STATE_SPAWNING;
    scratch->projectile_owner_slot = player_slot;

    if (pf_sim_push_event(
            scratch,
            world->tick,
            PF_SIM_EVENT_PROJECTILE_FIRE,
            (uint8_t)player_index,
            PF_SIM_EVENT_NO_PLAYER,
            UINT32_C(0),
            scratch->projectile_velocity_x_f32,
            scratch->projectile_velocity_y_f32,
            UINT16_C(0),
            (uint16_t)fire_action,
            NULL) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    return PF_STATUS_OK;
}

pf_status step_projectile(
    const struct content *content,
    pf_sim_scratch *scratch)
{
    const projectile_data *projectile;
    float next_x;
    float next_y;

    if (content == NULL || scratch == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    projectile = &content->projectile;
    if (projectile->enabled == UINT8_C(0))
    {
        projectile_clear(scratch);
        return PF_STATUS_OK;
    }
    if (scratch->projectile_state ==
        (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE)
    {
        return PF_STATUS_OK;
    }
    if (scratch->projectile_state ==
        (uint8_t)PF_M4_PROJECTILE_STATE_SPAWNING)
    {
        scratch->projectile_state =
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE;
        return PF_STATUS_OK;
    }
    if (scratch->projectile_state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE ||
        scratch->projectile_lifetime_ticks == UINT16_C(0) ||
        scratch->projectile_owner_slot == UINT8_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    --scratch->projectile_lifetime_ticks;
    if (scratch->projectile_lifetime_ticks == UINT16_C(0))
    {
        projectile_clear(scratch);
        return PF_STATUS_OK;
    }
    if (!projectile_checked_add(
            scratch->projectile_position_x_f32,
            scratch->projectile_velocity_x_f32,
            &next_x) ||
        !projectile_checked_add(
            scratch->projectile_position_y_f32,
            scratch->projectile_velocity_y_f32,
            &next_y))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->projectile_position_x_f32 = next_x;
    scratch->projectile_position_y_f32 = next_y;
    if (next_x + projectile->half_width_f32 <
            content->stage.blast_left_f32 ||
        next_x - projectile->half_width_f32 >
            content->stage.blast_right_f32 ||
        next_y + projectile->half_height_f32 <
            content->stage.blast_top_f32 ||
        next_y - projectile->half_height_f32 >
            content->stage.blast_bottom_f32)
    {
        projectile_clear(scratch);
    }
    return PF_STATUS_OK;
}
