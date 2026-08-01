#include "sim_internal.h"

#include <stdint.h>
#include <string.h>

/*
 * One player can emit one movement event, one combat event, and one forfeit
 * event in a tick. A match can additionally emit one resolution event, while
 * the canonical item can emit one input/reset event and one collision event,
 * and the bounded projectile slot can emit one fire/collision event.
 */
_Static_assert(
    PF_SIM_MAX_EVENTS_PER_TICK >=
        UINT8_C(3) * PF_SIM_MAX_PLAYERS + UINT8_C(4),
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
    uint32_t value_q16,
    int32_t velocity_x_q16,
    int32_t velocity_y_q16,
    uint16_t flags,
    uint16_t detail,
    uint32_t *out_sequence)
{
    pf_sim_event *event;

    if (scratch == NULL ||
        type <= PF_SIM_EVENT_NONE ||
        type > PF_SIM_EVENT_REVIVAL_DROP ||
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
    event->value_q16 = value_q16;
    event->velocity_x_q16 = velocity_x_q16;
    event->velocity_y_q16 = velocity_y_q16;
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
