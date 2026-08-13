#include "sim_internal.h"

#include <stdint.h>
#include <string.h>

/*
 * One player can emit one movement event and one combat event in a tick. A
 * tick can additionally emit one packed action-transition event, one
 * coalesced forfeit event, one match-resolution event, two item events, and
 * one projectile event.
 */
_Static_assert(
    PF_SIM_MAX_EVENTS_PER_TICK >=
        UINT8_C(2) * PF_SIM_MAX_PLAYERS + UINT8_C(6),
    "the per-tick journal must hold every current production event");
_Static_assert(
    sizeof(pf_sim_event) == (size_t)32,
    "pf_sim_event is a fixed-size public ABI record");

pf_status pf_sim_push_event(
    pf_sim_scratch *scratch,
    uint64_t tick,
    pf_sim_event_type type,
    uint8_t source_player,
    uint8_t target_player,
    float value_f32,
    float velocity_x_f32,
    float velocity_y_f32,
    uint16_t flags,
    uint16_t detail,
    uint32_t *out_sequence)
{
    pf_sim_event *event;

    if (scratch == NULL ||
        type <= PF_SIM_EVENT_NONE ||
        type > PF_SIM_EVENT_ACTION_TRANSITIONS ||
        (source_player != PF_SIM_EVENT_NO_PLAYER &&
         source_player >= PF_SIM_MAX_PLAYERS) ||
        (target_player != PF_SIM_EVENT_NO_PLAYER &&
         target_player >= PF_SIM_MAX_PLAYERS))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (scratch->combat_event_count >= PF_SIM_MAX_EVENTS_PER_TICK ||
        scratch->combat_event_sequence == UINT32_MAX)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    ++scratch->combat_event_sequence;
    event = &scratch->combat_events[scratch->combat_event_count];
    (void)memset(event, 0, sizeof(*event));
    event->tick = tick;
    event->sequence = scratch->combat_event_sequence;
    event->value_f32 = value_f32;
    event->velocity_x_f32 = velocity_x_f32;
    event->velocity_y_f32 = velocity_y_f32;
    event->type = (uint16_t)type;
    event->flags = flags;
    event->detail = detail;
    event->source_player = source_player;
    event->target_player = target_player;
    ++scratch->combat_event_count;

    if (out_sequence != NULL)
    {
        *out_sequence = event->sequence;
    }
    return PF_STATUS_OK;
}
