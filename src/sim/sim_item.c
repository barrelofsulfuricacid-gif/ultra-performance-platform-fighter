#include "sim_internal.h"

#include <math.h>
#include <stdint.h>

static float item_clamp_speed(float value)
{
    if (value > PF_SIM_MAX_MOTION_SPEED_F32)
    {
        return PF_SIM_MAX_MOTION_SPEED_F32;
    }
    if (value < -PF_SIM_MAX_MOTION_SPEED_F32)
    {
        return -PF_SIM_MAX_MOTION_SPEED_F32;
    }
    return value;
}

static int item_checked_add(
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

static int item_player_in_pickup_range(
    const item_data *item,
    float player_x_f32,
    float player_y_f32,
    float item_x_f32,
    float item_y_f32)
{
    const float delta_x = player_x_f32 - item_x_f32;
    const float delta_y = player_y_f32 - item_y_f32;

    return delta_x >= -item->pickup_half_width_f32 &&
           delta_x <= item->pickup_half_width_f32 &&
           delta_y >= -item->pickup_half_height_f32 &&
           delta_y <= item->pickup_half_height_f32;
}

static int item_action_can_throw(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_END ||
           action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
           action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD;
}

static item_throw_direction item_direction_for_input(
    const fighter_data *fighter,
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

static const item_velocity *item_velocity_for_direction(
    const item_data *item,
    item_throw_direction direction)
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

static void item_attach_to_player(
    const item_data *item,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    scratch->item_position_x_f32 =
        scratch->position_x_f32[player_index] +
        (float)scratch->facing[player_index] *
            item->held_offset_x_f32;
    scratch->item_position_y_f32 =
        scratch->position_y_f32[player_index] +
        item->held_offset_y_f32;
}

static void item_enter_respawn_wait(
    const item_data *item,
    pf_sim_scratch *scratch)
{
    scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_RESPAWN_WAIT;
    scratch->item_position_x_f32 = INT32_C(0);
    scratch->item_position_y_f32 = INT32_C(0);
    scratch->item_velocity_x_f32 = INT32_C(0);
    scratch->item_velocity_y_f32 = INT32_C(0);
    scratch->item_lifetime_ticks = UINT16_C(0);
    scratch->item_respawn_ticks = item->respawn_ticks;
    scratch->item_pickup_lockout_ticks = UINT16_C(0);
    scratch->item_holder_slot = UINT8_C(0);
    scratch->item_source_slot = UINT8_C(0);
    scratch->item_hit_mask = UINT8_C(0);
    scratch->item_stale_registered = UINT8_C(0);
    scratch->item_throw_direction =
        (uint8_t)PF_M4_ITEM_THROW_NONE;
}

void reset_item(pf_sim *sim)
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
        world->item_stale_registered = UINT8_C(0);
        return;
    }
    world->item_position_x_f32 = sim->content.item.spawn_x_f32;
    world->item_position_y_f32 = sim->content.item.spawn_y_f32;
    world->item_lifetime_ticks = sim->content.item.lifetime_ticks;
    world->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
    world->item_stale_registered = UINT8_C(0);
}

void begin_item_tick(
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    scratch->item_position_x_f32 = world->item_position_x_f32;
    scratch->item_position_y_f32 = world->item_position_y_f32;
    scratch->item_velocity_x_f32 = world->item_velocity_x_f32;
    scratch->item_velocity_y_f32 = world->item_velocity_y_f32;
    scratch->item_lifetime_ticks = world->item_lifetime_ticks;
    scratch->item_respawn_ticks = world->item_respawn_ticks;
    scratch->item_pickup_lockout_ticks =
        world->item_pickup_lockout_ticks;
    scratch->item_state = world->item_state;
    scratch->item_holder_slot = world->item_holder_slot;
    scratch->item_source_slot = world->item_source_slot;
    scratch->item_hit_mask = world->item_hit_mask;
    scratch->item_stale_registered = world->item_stale_registered;
    scratch->item_throw_direction = world->item_throw_direction;
}

item_input_intent prepare_item_input(
    const struct content *content,
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
    item_input_intent intent = PF_M4_ITEM_INPUT_NONE;

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
        item_action_can_throw(action_state))
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
             action_state !=
                 (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM &&
             light_pressed != 0 && shield_held != 0 &&
             item_player_in_pickup_range(
                 &content->item,
                 world->position_x_f32[player_index],
                 world->position_y_f32[player_index],
                 scratch->item_position_x_f32,
                 scratch->item_position_y_f32))
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

pf_status apply_item_input(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    item_input_intent intent)
{
    const item_data *item = &content->item;
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
            scratch->action_state[player_index] ==
                (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM ||
            !item_player_in_pickup_range(
                item,
                scratch->position_x_f32[player_index],
                scratch->position_y_f32[player_index],
                scratch->item_position_x_f32,
                scratch->item_position_y_f32))
        {
            return PF_STATUS_OK;
        }
        scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_HELD;
        scratch->item_holder_slot = player_slot;
        scratch->item_source_slot = UINT8_C(0);
        scratch->item_hit_mask = UINT8_C(0);
        scratch->item_stale_registered = UINT8_C(0);
        scratch->item_throw_direction =
            (uint8_t)PF_M4_ITEM_THROW_NONE;
        scratch->item_velocity_x_f32 = INT32_C(0);
        scratch->item_velocity_y_f32 = INT32_C(0);
        scratch->item_lifetime_ticks = item->lifetime_ticks;
        item_attach_to_player(item, scratch, player_index);
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
    item_attach_to_player(item, scratch, player_index);
    scratch->item_holder_slot = UINT8_C(0);
    scratch->item_source_slot = player_slot;
    scratch->item_hit_mask = UINT8_C(0);
    scratch->item_stale_registered = UINT8_C(0);
    scratch->item_pickup_lockout_ticks = item->pickup_lockout_ticks;
    scratch->item_lifetime_ticks = item->lifetime_ticks;

    if (intent == PF_M4_ITEM_INPUT_DROP)
    {
        scratch->item_throw_direction =
            (uint8_t)PF_M4_ITEM_THROW_NONE;
        scratch->item_velocity_x_f32 = INT32_C(0);
        scratch->item_velocity_y_f32 = item->drop_velocity_y_f32;
        if (scratch->grounded[player_index] != UINT8_C(0))
        {
            scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
            scratch->item_position_y_f32 =
                content->stage.floor_y_f32 - item->half_height_f32;
            scratch->item_velocity_y_f32 = INT32_C(0);
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
                scratch->item_velocity_x_f32,
                scratch->item_velocity_y_f32,
                UINT16_C(0),
                (uint16_t)scratch->item_state,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        return PF_STATUS_OK;
    }

    {
        const item_throw_direction direction =
            item_direction_for_input(
                &content->fighter,
                input,
                world->facing[player_index]);
        const item_velocity *throw_velocity =
            item_velocity_for_direction(item, direction);
        const float momentum_x =
            scratch->velocity_x_f32[player_index] *
            item->momentum_transfer_f32;
        const float momentum_y =
            scratch->velocity_y_f32[player_index] *
            item->momentum_transfer_f32;

        scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_AIRBORNE;
        scratch->item_throw_direction = (uint8_t)direction;
        scratch->item_velocity_x_f32 = item_clamp_speed(
            (float)world->facing[player_index] *
                throw_velocity->velocity_x_f32 +
            momentum_x);
        scratch->item_velocity_y_f32 = item_clamp_speed(
            throw_velocity->velocity_y_f32 + momentum_y);

        if (scratch->grounded[player_index] != UINT8_C(0) ||
            intent == PF_M4_ITEM_INPUT_JUMP_CANCEL_THROW)
        {
            if (intent == PF_M4_ITEM_INPUT_JUMP_CANCEL_THROW)
            {
                scratch->position_y_f32[player_index] =
                    world->position_y_f32[player_index];
                scratch->velocity_y_f32[player_index] = INT32_C(0);
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
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
            scratch->dash_direction[player_index] = INT8_C(0);
            scratch->short_hop_latched[player_index] = UINT8_C(0);
            if (intent == PF_M4_ITEM_INPUT_DASH_THROW)
            {
                scratch->velocity_x_f32[player_index] =
                    (float)scratch->facing[player_index] *
                    item->dash_throw_speed_f32;
            }
        }
        if (pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_ITEM_THROW,
                (uint8_t)player_index,
                PF_SIM_EVENT_NO_PLAYER,
                UINT32_C(0),
                scratch->item_velocity_x_f32,
                scratch->item_velocity_y_f32,
                UINT16_C(0),
                (uint16_t)direction,
                NULL) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
    }
    return PF_STATUS_OK;
}

pf_status step_item(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    const item_data *item = &content->item;

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
            scratch->item_velocity_x_f32 = INT32_C(0);
            scratch->item_velocity_y_f32 = item->drop_velocity_y_f32;
            scratch->item_pickup_lockout_ticks =
                item->pickup_lockout_ticks;
        }
        else
        {
            item_attach_to_player(item, scratch, holder_index);
            scratch->item_velocity_x_f32 = INT32_C(0);
            scratch->item_velocity_y_f32 = INT32_C(0);
            return PF_STATUS_OK;
        }
    }

    if (scratch->item_state == (uint8_t)PF_M4_ITEM_STATE_GROUND)
    {
        scratch->item_position_y_f32 =
            content->stage.floor_y_f32 - item->half_height_f32;
        scratch->item_velocity_x_f32 = INT32_C(0);
        scratch->item_velocity_y_f32 = INT32_C(0);
        scratch->item_holder_slot = UINT8_C(0);
        scratch->item_source_slot = UINT8_C(0);
        scratch->item_hit_mask = UINT8_C(0);
        scratch->item_stale_registered = UINT8_C(0);
        scratch->item_throw_direction =
            (uint8_t)PF_M4_ITEM_THROW_NONE;
    }
    else if (scratch->item_state ==
             (uint8_t)PF_M4_ITEM_STATE_AIRBORNE)
    {
        float next_x;
        float next_y;

        scratch->item_velocity_y_f32 = item_clamp_speed(
            scratch->item_velocity_y_f32 + item->gravity_f32);
        if (scratch->item_velocity_y_f32 > item->fall_speed_f32)
        {
            scratch->item_velocity_y_f32 = item->fall_speed_f32;
        }
        if (!item_checked_add(
                scratch->item_position_x_f32,
                scratch->item_velocity_x_f32,
                &next_x) ||
            !item_checked_add(
                scratch->item_position_y_f32,
                scratch->item_velocity_y_f32,
                &next_y))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        scratch->item_position_x_f32 = next_x;
        scratch->item_position_y_f32 = next_y;
        if (scratch->item_position_y_f32 + item->half_height_f32 >=
                content->stage.floor_y_f32 &&
            scratch->item_velocity_y_f32 >= INT32_C(0) &&
            scratch->item_position_x_f32 >=
                content->stage.floor_left_f32 &&
            scratch->item_position_x_f32 <=
                content->stage.floor_right_f32)
        {
            scratch->item_position_y_f32 =
                content->stage.floor_y_f32 - item->half_height_f32;
            scratch->item_velocity_x_f32 = INT32_C(0);
            scratch->item_velocity_y_f32 = INT32_C(0);
            scratch->item_state = (uint8_t)PF_M4_ITEM_STATE_GROUND;
            scratch->item_source_slot = UINT8_C(0);
            scratch->item_hit_mask = UINT8_C(0);
            scratch->item_stale_registered = UINT8_C(0);
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
            scratch->item_position_x_f32 = item->spawn_x_f32;
            scratch->item_position_y_f32 = item->spawn_y_f32;
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
        scratch->item_position_x_f32 < content->stage.blast_left_f32 ||
        scratch->item_position_x_f32 > content->stage.blast_right_f32 ||
        scratch->item_position_y_f32 < content->stage.blast_top_f32 ||
        scratch->item_position_y_f32 > content->stage.blast_bottom_f32)
    {
        item_enter_respawn_wait(item, scratch);
    }
    return PF_STATUS_OK;
}
