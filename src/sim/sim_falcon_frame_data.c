#include "sim_falcon_frame_data.h"

#include "pf/m4.h"

#include <stddef.h>
#include <stdint.h>

#include "../../generated/data/m4_falcon_ntsc102_frame_data.inc"

_Static_assert(
    sizeof(pf_m4_falcon_moves) / sizeof(pf_m4_falcon_moves[0]) ==
        (size_t)PF_M4_FALCON_MOVE_COUNT,
    "Falcon move table must cover every indexed move");

const uint8_t *pf_m4_falcon_reference_source_sha256(void)
{
    return pf_m4_falcon_source_sha256;
}

const pf_m4_reference_move *pf_m4_falcon_reference_move(
    pf_m4_falcon_move_index move_index)
{
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return NULL;
    }
    return &pf_m4_falcon_moves[move_index];
}

const pf_m4_reference_hit_phase *pf_m4_falcon_reference_phase(
    pf_m4_falcon_move_index move_index,
    uint16_t phase_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        phase_index >= (uint16_t)move->phase_count)
    {
        return NULL;
    }
    return &pf_m4_falcon_hit_phases[move->phase_offset + phase_index];
}

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_effect(
    pf_m4_falcon_move_index move_index,
    uint16_t effect_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        effect_index >= (uint16_t)move->effect_count)
    {
        return NULL;
    }
    return &pf_m4_falcon_hit_effects[move->effect_offset + effect_index];
}

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_primary_effect(
    pf_m4_falcon_move_index move_index)
{
    const pf_m4_reference_hit_phase *phase =
        pf_m4_falcon_reference_phase(move_index, UINT16_C(0));
    uint16_t effect_index = UINT16_C(0);
    uint16_t mask;

    if (phase == NULL || phase->effect_mask == UINT16_C(0))
    {
        return NULL;
    }
    mask = phase->effect_mask;
    while ((mask & UINT16_C(1)) == UINT16_C(0))
    {
        mask >>= 1U;
        ++effect_index;
    }
    return pf_m4_falcon_reference_effect(move_index, effect_index);
}

const pf_m4_reference_hit_phase *pf_m4_falcon_reference_phase_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);
    uint16_t phase_index;

    if (move == NULL || move->present == UINT8_C(0))
    {
        return NULL;
    }
    for (phase_index = UINT16_C(0);
         phase_index < (uint16_t)move->phase_count;
         ++phase_index)
    {
        const pf_m4_reference_hit_phase *phase =
            pf_m4_falcon_reference_phase(move_index, phase_index);

        if (phase != NULL && action_frame >= phase->first_frame &&
            action_frame <= phase->last_frame)
        {
            return phase;
        }
    }
    return NULL;
}

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_effect_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame)
{
    const pf_m4_reference_hit_phase *phase =
        pf_m4_falcon_reference_phase_at_frame(move_index, action_frame);
    uint16_t effect_index = UINT16_C(0);
    uint16_t mask;

    if (phase == NULL || phase->effect_mask == UINT16_C(0))
    {
        return NULL;
    }
    mask = phase->effect_mask;
    while ((mask & UINT16_C(1)) == UINT16_C(0))
    {
        mask >>= 1U;
        ++effect_index;
    }
    return pf_m4_falcon_reference_effect(move_index, effect_index);
}

int pf_m4_falcon_reference_move_for_action(
    uint8_t action_state,
    pf_m4_falcon_move_index *out_move_index)
{
    pf_m4_falcon_move_index move_index;

    switch ((pf_m4_action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
            move_index = PF_M4_FALCON_JAB1;
            break;
        case PF_M4_ACTION_JAB_FINAL:
            move_index = PF_M4_FALCON_JAB2;
            break;
        case PF_M4_ACTION_DASH_ATTACK:
            move_index = PF_M4_FALCON_DASH_ATTACK;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK:
            move_index = PF_M4_FALCON_FORWARD_TILT;
            break;
        case PF_M4_ACTION_UP_ATTACK:
            move_index = PF_M4_FALCON_UP_TILT;
            break;
        case PF_M4_ACTION_DOWN_ATTACK:
            move_index = PF_M4_FALCON_DOWN_TILT;
            break;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
            move_index = PF_M4_FALCON_FORWARD_SMASH;
            break;
        case PF_M4_ACTION_UP_STRONG_ATTACK:
            move_index = PF_M4_FALCON_UP_SMASH;
            break;
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
            move_index = PF_M4_FALCON_DOWN_SMASH;
            break;
        case PF_M4_ACTION_AERIAL_ATTACK:
            move_index = PF_M4_FALCON_NEUTRAL_AERIAL;
            break;
        case PF_M4_ACTION_FORWARD_AERIAL:
            move_index = PF_M4_FALCON_FORWARD_AERIAL;
            break;
        case PF_M4_ACTION_BACK_AERIAL:
            move_index = PF_M4_FALCON_BACK_AERIAL;
            break;
        case PF_M4_ACTION_UP_AERIAL:
            move_index = PF_M4_FALCON_UP_AERIAL;
            break;
        case PF_M4_ACTION_DOWN_AERIAL:
            move_index = PF_M4_FALCON_DOWN_AERIAL;
            break;
        default:
            return 0;
    }
    if (out_move_index != NULL)
    {
        *out_move_index = move_index;
    }
    return 1;
}

const pf_m4_reference_throw *pf_m4_falcon_reference_throw(
    pf_m4_falcon_move_index move_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        move->throw_index == UINT16_MAX)
    {
        return NULL;
    }
    return &pf_m4_falcon_throws[move->throw_index];
}

pf_m4_reference_timing pf_m4_falcon_reference_timing(
    pf_m4_falcon_move_index move_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);
    pf_m4_reference_timing timing = {0};
    const pf_m4_reference_hit_phase *first;
    const pf_m4_reference_hit_phase *last;

    if (move == NULL || move->present == UINT8_C(0) ||
        move->phase_count == UINT8_C(0))
    {
        return timing;
    }
    first = pf_m4_falcon_reference_phase(move_index, UINT16_C(0));
    last = pf_m4_falcon_reference_phase(
        move_index,
        (uint16_t)(move->phase_count - UINT8_C(1)));
    if (first == NULL || last == NULL)
    {
        return timing;
    }
    timing.startup_ticks = first->first_frame - UINT16_C(1);
    timing.active_ticks = (uint16_t)(
        (uint32_t)last->last_frame -
        (uint32_t)first->first_frame + UINT32_C(1));
    timing.recovery_ticks = move->total_frames - last->last_frame;
    return timing;
}

int pf_m4_falcon_reference_landing_lag_active(
    uint8_t action_state,
    uint16_t action_frame)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;

    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return -1;
    }
    move = pf_m4_falcon_reference_move(move_index);
    if (move == NULL || move->landing_lag == UINT16_C(0) ||
        move->autocancel_before == UINT16_C(0) ||
        move->autocancel_after == UINT16_C(0))
    {
        return -1;
    }
    return action_frame >= move->autocancel_before &&
           action_frame <= move->autocancel_after;
}
