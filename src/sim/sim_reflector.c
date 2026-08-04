#include "sim_internal.h"

static int pf_m4_reflector_action_can_start(
    uint8_t grounded,
    uint8_t action_state)
{
    if (grounded != UINT8_C(0))
    {
        return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
               action_state == (uint8_t)PF_M4_ACTION_WALK ||
               action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
               action_state == (uint8_t)PF_M4_ACTION_RUN ||
               action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
               action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
               action_state == (uint8_t)PF_M4_ACTION_CROUCH_END;
    }
    return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP;
}

void pf_m4_prepare_reflector_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input)
{
    int down_special;

    if (effective_input == NULL)
    {
        return;
    }
    if (content == NULL || world == NULL || input == NULL ||
        player_index >= (uint32_t)world->player_count)
    {
        *effective_input = (pf_input_frame){0};
        return;
    }
    *effective_input = *input;
    down_special =
        (input->buttons & PF_INPUT_BUTTON_SPECIAL) != UINT64_C(0) &&
        input->main_stick_y >=
            (int16_t)content->fighter.crouch_axis_threshold;
    if (down_special == 0)
    {
        return;
    }
    if (content->reflector.enabled == UINT8_C(0) ||
        world->active[player_index] == UINT8_C(0) ||
        world->hitlag_ticks[player_index] != UINT16_C(0) ||
        world->tumble[player_index] != UINT8_C(0) ||
        !pf_m4_reflector_action_can_start(
            world->grounded[player_index],
            world->action_state[player_index]))
    {
        effective_input->buttons &= ~PF_INPUT_BUTTON_SPECIAL;
    }
}
