#include "sim_internal.h"

#include <limits.h>
#include <stdint.h>

static int32_t pf_m4_item_clamp_speed(int64_t value)
{
    if (value > (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        return PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    if (value < -(int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        return -PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    return (int32_t)value;
}

static int pf_m4_item_checked_add(
    int32_t left,
    int32_t right,
    int32_t *out_value)
{
    const int64_t value = (int64_t)left + (int64_t)right;

    if (out_value == NULL || value < (int64_t)INT32_MIN ||
        value > (int64_t)INT32_MAX)
    {
        return 0;
    }
    *out_value = (int32_t)value;
    return 1;
}

static int pf_m4_item_player_in_pickup_range(
    const pf_m4_item_data *item,
    int32_t player_x_q16,
    int32_t player_y_q16,
    int32_t item_x_q16,
    int32_t item_y_q16)
{
    const int64_t delta_x =
        (int64_t)player_x_q16 - (int64_t)item_x_q16;
    const int64_t delta_y =
        (int64_t)player_y_q16 - (int64_t)item_y_q16;

    return delta_x >= -(int64_t)item->pickup_half_width_q16 &&
           delta_x <= (int64_t)item->pickup_half_width_q16 &&
           delta_y >= -(int64_t)item->pickup_half_height_q16 &&
           delta_y <= (int64_t)item->pickup_half_height_q16;
}

static int pf_m4_item_action_can_throw(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
           action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
           action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD;
}

static pf_m4_item_throw_direction pf_m4_item_direction_for_input(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input,
    int8_t facing)
{
    const int32_t x = (int32_t)input->main_stick_x;
    const int32_t y = (int32_t)input->main_stick_y;
    const int32_t absolute_x = x < INT32_C(0) ? -x : x;
    const int32_t absolute_y = y < INT32_C(0) ? -y : y;

    if (absolute_y >= (int32_t)fighter->dash_axis_threshold &&
        absolute_y >= absolute_x)
    {
        return y < INT32_C(0) ? PF_M4_ITEM_THROW_UP
                              : PF_M4_ITEM_THROW_DOWN;
    }
    if (absolute_x >= (int32_t)fighter->axis_dead_zone)
    {
        const int8_t direction = x < INT32_C(0) ? INT8_C(-1)
                                                : INT8_C(1);
        return direction == facing ? PF_M4_ITEM_THROW_FORWARD
                                   : PF_M4_ITEM_THROW_BACK;
    }
    return PF_M4_ITEM_THROW_FORWARD;
}

static const pf_m4_item_velocity *pf_m4_item_velocity_for_direction(
    const pf_m4_item_data *item,
    pf_m4_item_throw_direction direction)
{
    if (direction == PF_M4_ITEM_THROW_BACK)
    {
        return &item->back_throw;
    }
    if (direction == PF_M4_ITEM_THROW_UP)
    {
        return &item->up_throw;
    }
    if (direction == PF_M4_ITEM_THROW_DOWN)
    {
        return &item->down_throw;
    }
    return &item->forward_throw;
}

static void pf_m4_item_attach_to_player(
    const pf_m4_item_data *item,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    scratch->item_position_x_q16 =
        scratch->position_x_q16[player_index] +
        (int32_t)scratch->facing[player_index] *
            item->held_offset_x_q16;
    scratch->item_position_y_q16 =
        scratch->position_y_q16[player_index] +
        item->held_offset_y_q16;
}

static void pf_m4_item_enter_respawn_wait(
    const pf_m4_item_data *item,
    pf_sim_scratch *scratch)
{
    scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_RESPAWN_WAIT;
    scratch->item_position_x_q16 = INT32_C(0);
    scratch->item_position_y_q16 = INT32_C(0);
    scratch->item_velocity_x_q16 = INT32_C(0);
    scratch->item_velocity_y_q16 = INT32_C(0);
    scratch->item_lifetime_ticks = UINT16_C(0);
    scratch->item_respawn_ticks = item->respawn_ticks;
    scratch->item_pickup_lockout_ticks = UINT16_C(0);
    scratch->item_holder_slot = UINT8_C(0);
    scratch->item_source_slot = UINT8_C(0);
    scratch->item_hit_mask = UINT8_C(0);
    scratch->item_throw_direction =
        (uint8_t)PF_M4_ITEM_THROW_NONE;
}

void pf_m4_reset_item(pf_sim *sim)
{
    pf_world_state *world;

    if (sim == NULL)
    {
        return;
    }
    world = &sim->world;
    if (sim->content.item.enabled == UINT8_C(0))
    {
        world->item_state = (uint8_t)PF_M4_ITEM_STATE_INACTIVE;
        return;
    }
    world->item_position_x_q16 = sim->content.item.spawn_x_q16;
    world->item_position_y_q16 = sim->content.item.spawn_y_q16;
    world->item_lifetime_ticks = sim->content.item.lifetime_ticks;
    world->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
}

void pf_m4_begin_item_tick(
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    scratch->item_position_x_q16 = world->item_position_x_q16;
    scratch->item_position_y_q16 = world->item_position_y_q16;
    scratch->item_velocity_x_q16 = world->item_velocity_x_q16;
    scratch->item_velocity_y_q16 = world->item_velocity_y_q16;
    scratch->item_lifetime_ticks = world->item_lifetime_ticks;
    scratch->item_respawn_ticks = world->item_respawn_ticks;
    scratch->item_pickup_lockout_ticks =
        world->item_pickup_lockout_ticks;
    scratch->item_state = world->item_state;
    scratch->item_holder_slot = world->item_holder_slot;
    scratch->item_source_slot = world->item_source_slot;
    scratch->item_hit_mask = world->item_hit_mask;
    scratch->item_throw_direction = world->item_throw_direction;
}

pf_m4_item_input_intent pf_m4_prepare_item_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input)
{
    const uint64_t previous_buttons =
        world->previous_buttons[player_index];
    const int light_pressed =
        (input->buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_ATTACK) == UINT64_C(0);
    const int strong_pressed =
        (input->buttons & PF_INPUT_BUTTON_STRONG_ATTACK) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_STRONG_ATTACK) == UINT64_C(0);
    const int shield_held =
        input->left_trigger >=
            content->fighter.light_shield_trigger_threshold ||
        input->right_trigger >=
            content->fighter.light_shield_trigger_threshold;
    const uint8_t player_slot =
        (uint8_t)(player_index + UINT32_C(1));
    const uint8_t action_state =
        world->action_state[player_index];
    pf_m4_item_input_intent intent = PF_M4_ITEM_INPUT_NONE;

    if (effective_input == NULL)
    {
        return PF_M4_ITEM_INPUT_NONE;
    }
    *effective_input = *input;
    if (content->item.enabled == UINT8_C(0) ||
        world->active[player_index] == UINT8_C(0))
    {
        return PF_M4_ITEM_INPUT_NONE;
    }

    if (scratch->item_state == (uint8_t)PF_M4_ITEM_STATE_HELD &&
        scratch->item_holder_slot == player_slot &&
        (light_pressed != 0 || strong_pressed != 0) &&
        pf_m4_item_action_can_throw(action_state))
    {
        if (action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
            action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD)
        {
            if (world->action_ticks[player_index] >=
                    content->item.glide_toss_begin_tick &&
                world->action_ticks[player_index] <=
                    content->item.glide_toss_end_tick)
            {
                intent = PF_M4_ITEM_INPUT_GLIDE_TOSS;
            }
        }
        else if (shield_held != 0 && light_pressed != 0)
        {
            intent = PF_M4_ITEM_INPUT_DROP;
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
        {
            intent = PF_M4_ITEM_INPUT_JUMP_CANCEL_THROW;
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_RUN ||
                 action_state ==
                     (uint8_t)PF_M4_ACTION_INITIAL_DASH)
        {
            intent = PF_M4_ITEM_INPUT_DASH_THROW;
        }
        else
        {
            intent = PF_M4_ITEM_INPUT_THROW;
        }
    }
    else if (scratch->item_state ==
                 (uint8_t)PF_M4_ITEM_STATE_GROUND &&
             scratch->item_pickup_lockout_ticks == UINT16_C(0) &&
             world->grounded[player_index] != UINT8_C(0) &&
             light_pressed != 0 && shield_held != 0 &&
             pf_m4_item_player_in_pickup_range(
                 &content->item,
                 world->position_x_q16[player_index],
                 world->position_y_q16[player_index],
                 scratch->item_position_x_q16,
                 scratch->item_position_y_q16))
    {
        intent = PF_M4_ITEM_INPUT_PICKUP;
    }

    if (intent != PF_M4_ITEM_INPUT_NONE)
    {
        effective_input->buttons &=
            ~(PF_INPUT_BUTTON_ATTACK | PF_INPUT_BUTTON_STRONG_ATTACK);
    }
    if (intent == PF_M4_ITEM_INPUT_PICKUP ||
        intent == PF_M4_ITEM_INPUT_DROP)
    {
        effective_input->left_trigger = UINT16_C(0);
        effective_input->right_trigger = UINT16_C(0);
    }
    return intent;
}

pf_status pf_m4_apply_item_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_m4_item_input_intent intent)
{
    const pf_m4_item_data *item = &content->item;
    const uint8_t player_slot =
        (uint8_t)(player_index + UINT32_C(1));

    if (intent == PF_M4_ITEM_INPUT_NONE)
    {
        return PF_STATUS_OK;
    }
    if (intent == PF_M4_ITEM_INPUT_PICKUP)
    {
        if (scratch->item_state != (uint8_t)PF_M4_ITEM_STATE_GROUND ||
            scratch->item_pickup_lockout_ticks != UINT16_C(0) ||
            scratch->active[player_index] == UINT8_C(0) ||
            scratch->grounded[player_index] == UINT8_C(0) ||
            !pf_m4_item_player_in_pickup_range(
                item,
                scratch->position_x_q16[player_index],
                scratch->position_y_q16[player_index],
                scratch->item_position_x_q16,
                scratch->item_position_y_q16))
        {
            return PF_STATUS_OK;
        }
        scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_HELD;
        scratch->item_holder_slot = player_slot;
        scratch->item_source_slot = UINT8_C(0);
        scratch->item_hit_mask = UINT8_C(0);
        scratch->item_throw_direction =
            (uint8_t)PF_M4_ITEM_THROW_NONE;
        scratch->item_velocity_x_q16 = INT32_C(0);
        scratch->item_velocity_y_q16 = INT32_C(0);
        scratch->item_lifetime_ticks = item->lifetime_ticks;
        pf_m4_item_attach_to_player(item, scratch, player_index);
        if (pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_ITEM_PICKUP,
                (uint8_t)player_index,
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
        return PF_STATUS_OK;
    }

    if (scratch->item_state != (uint8_t)PF_M4_ITEM_STATE_HELD ||
        scratch->item_holder_slot != player_slot)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    pf_m4_item_attach_to_player(item, scratch, player_index);
    scratch->item_holder_slot = UINT8_C(0);
    scratch->item_source_slot = player_slot;
    scratch->item_hit_mask = UINT8_C(0);
    scratch->item_pickup_lockout_ticks = item->pickup_lockout_ticks;
    scratch->item_lifetime_ticks = item->lifetime_ticks;

    if (intent == PF_M4_ITEM_INPUT_DROP)
    {
        scratch->item_throw_direction =
            (uint8_t)PF_M4_ITEM_THROW_NONE;
        scratch->item_velocity_x_q16 = INT32_C(0);
        scratch->item_velocity_y_q16 = item->drop_velocity_y_q16;
        if (scratch->grounded[player_index] != UINT8_C(0))
        {
            scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
            scratch->item_position_y_q16 =
                content->stage.floor_y_q16 - item->half_height_q16;
            scratch->item_velocity_y_q16 = INT32_C(0);
        }
        else
        {
            scratch->item_state =
                (uint8_t)PF_M4_ITEM_STATE_AIRBORNE;
        }
        if (pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_ITEM_DROP,
                (uint8_t)player_index,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                scratch->item_velocity_x_q16,
                scratch->item_velocity_y_q16,
                UINT16_C(0),
                (uint16_t)scratch->item_state,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        return PF_STATUS_OK;
    }

    {
        const pf_m4_item_throw_direction direction =
            pf_m4_item_direction_for_input(
                &content->fighter,
                input,
                world->facing[player_index]);
        const pf_m4_item_velocity *throw_velocity =
            pf_m4_item_velocity_for_direction(item, direction);
        const int64_t momentum_x =
            ((int64_t)scratch->velocity_x_q16[player_index] *
             (int64_t)item->momentum_transfer_q16) /
            (int64_t)PF_Q16_ONE;
        const int64_t momentum_y =
            ((int64_t)scratch->velocity_y_q16[player_index] *
             (int64_t)item->momentum_transfer_q16) /
            (int64_t)PF_Q16_ONE;

        scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_AIRBORNE;
        scratch->item_throw_direction = (uint8_t)direction;
        scratch->item_velocity_x_q16 = pf_m4_item_clamp_speed(
            (int64_t)world->facing[player_index] *
                (int64_t)throw_velocity->velocity_x_q16 +
            momentum_x);
        scratch->item_velocity_y_q16 = pf_m4_item_clamp_speed(
            (int64_t)throw_velocity->velocity_y_q16 + momentum_y);

        if (scratch->grounded[player_index] != UINT8_C(0) ||
            intent == PF_M4_ITEM_INPUT_JUMP_CANCEL_THROW)
        {
            if (intent == PF_M4_ITEM_INPUT_JUMP_CANCEL_THROW)
            {
                scratch->position_y_q16[player_index] =
                    world->position_y_q16[player_index];
                scratch->velocity_y_q16[player_index] = INT32_C(0);
                scratch->grounded[player_index] = UINT8_C(1);
                scratch->support[player_index] =
                    world->support[player_index];
            }
            scratch->action_state[player_index] =
                intent == PF_M4_ITEM_INPUT_DASH_THROW
                    ? (uint8_t)PF_M4_ACTION_ITEM_DASH_THROW
                    : (uint8_t)PF_M4_ACTION_ITEM_THROW;
            scratch->action_ticks[player_index] = UINT16_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->dash_direction[player_index] = INT8_C(0);
            scratch->short_hop_latched[player_index] = UINT8_C(0);
            if (intent == PF_M4_ITEM_INPUT_DASH_THROW)
            {
                scratch->velocity_x_q16[player_index] =
                    (int32_t)scratch->facing[player_index] *
                    item->dash_throw_speed_q16;
            }
        }
        if (pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_ITEM_THROW,
                (uint8_t)player_index,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                scratch->item_velocity_x_q16,
                scratch->item_velocity_y_q16,
                UINT16_C(0),
                (uint16_t)direction,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
    }
    return PF_STATUS_OK;
}

pf_status pf_m4_step_item(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    const pf_m4_item_data *item = &content->item;

    if (item->enabled == UINT8_C(0))
    {
        return scratch->item_state ==
                       (uint8_t)PF_M4_ITEM_STATE_INACTIVE
                   ? PF_STATUS_OK
                   : PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (scratch->item_pickup_lockout_ticks > UINT16_C(0))
    {
        --scratch->item_pickup_lockout_ticks;
    }

    if (scratch->item_state == (uint8_t)PF_M4_ITEM_STATE_HELD)
    {
        const uint32_t holder_index =
            (uint32_t)scratch->item_holder_slot - UINT32_C(1);

        if (scratch->item_holder_slot == UINT8_C(0) ||
            holder_index >= (uint32_t)world->player_count)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (scratch->active[holder_index] == UINT8_C(0))
        {
            scratch->item_state =
                (uint8_t)PF_M4_ITEM_STATE_AIRBORNE;
            scratch->item_source_slot = scratch->item_holder_slot;
            scratch->item_holder_slot = UINT8_C(0);
            scratch->item_velocity_x_q16 = INT32_C(0);
            scratch->item_velocity_y_q16 = item->drop_velocity_y_q16;
            scratch->item_pickup_lockout_ticks =
                item->pickup_lockout_ticks;
        }
        else
        {
            pf_m4_item_attach_to_player(item, scratch, holder_index);
            scratch->item_velocity_x_q16 = INT32_C(0);
            scratch->item_velocity_y_q16 = INT32_C(0);
            return PF_STATUS_OK;
        }
    }

    if (scratch->item_state == (uint8_t)PF_M4_ITEM_STATE_GROUND)
    {
        scratch->item_position_y_q16 =
            content->stage.floor_y_q16 - item->half_height_q16;
        scratch->item_velocity_x_q16 = INT32_C(0);
        scratch->item_velocity_y_q16 = INT32_C(0);
        scratch->item_holder_slot = UINT8_C(0);
        scratch->item_source_slot = UINT8_C(0);
        scratch->item_hit_mask = UINT8_C(0);
        scratch->item_throw_direction =
            (uint8_t)PF_M4_ITEM_THROW_NONE;
    }
    else if (scratch->item_state ==
             (uint8_t)PF_M4_ITEM_STATE_AIRBORNE)
    {
        int32_t next_x;
        int32_t next_y;

        scratch->item_velocity_y_q16 = pf_m4_item_clamp_speed(
            (int64_t)scratch->item_velocity_y_q16 +
            (int64_t)item->gravity_q16);
        if (scratch->item_velocity_y_q16 > item->fall_speed_q16)
        {
            scratch->item_velocity_y_q16 = item->fall_speed_q16;
        }
        if (!pf_m4_item_checked_add(
                scratch->item_position_x_q16,
                scratch->item_velocity_x_q16,
                &next_x) ||
            !pf_m4_item_checked_add(
                scratch->item_position_y_q16,
                scratch->item_velocity_y_q16,
                &next_y))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        scratch->item_position_x_q16 = next_x;
        scratch->item_position_y_q16 = next_y;
        if (scratch->item_position_y_q16 + item->half_height_q16 >=
                content->stage.floor_y_q16 &&
            scratch->item_velocity_y_q16 >= INT32_C(0) &&
            scratch->item_position_x_q16 >=
                content->stage.floor_left_q16 &&
            scratch->item_position_x_q16 <=
                content->stage.floor_right_q16)
        {
            scratch->item_position_y_q16 =
                content->stage.floor_y_q16 - item->half_height_q16;
            scratch->item_velocity_x_q16 = INT32_C(0);
            scratch->item_velocity_y_q16 = INT32_C(0);
            scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
            scratch->item_source_slot = UINT8_C(0);
            scratch->item_hit_mask = UINT8_C(0);
            scratch->item_throw_direction =
                (uint8_t)PF_M4_ITEM_THROW_NONE;
        }
    }
    else if (scratch->item_state ==
             (uint8_t)PF_M4_ITEM_STATE_RESPAWN_WAIT)
    {
        if (scratch->item_respawn_ticks > UINT16_C(0))
        {
            --scratch->item_respawn_ticks;
        }
        if (scratch->item_respawn_ticks == UINT16_C(0))
        {
            scratch->item_position_x_q16 = item->spawn_x_q16;
            scratch->item_position_y_q16 = item->spawn_y_q16;
            scratch->item_lifetime_ticks = item->lifetime_ticks;
            scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_ITEM_RESET,
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
        }
        return PF_STATUS_OK;
    }
    else
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    if (scratch->item_lifetime_ticks > UINT16_C(0))
    {
        --scratch->item_lifetime_ticks;
    }
    if (scratch->item_lifetime_ticks == UINT16_C(0) ||
        scratch->item_position_x_q16 < content->stage.blast_left_q16 ||
        scratch->item_position_x_q16 > content->stage.blast_right_q16 ||
        scratch->item_position_y_q16 < content->stage.blast_top_q16 ||
        scratch->item_position_y_q16 > content->stage.blast_bottom_q16)
    {
        pf_m4_item_enter_respawn_wait(item, scratch);
    }
    return PF_STATUS_OK;
}
