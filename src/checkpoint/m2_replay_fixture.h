#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "5b35706ece2a4bf54b2401d0257b7363e56a9182871563a463992300e1fee632"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "6b4364e6d2f93d5bb174193e5d09b01af367b70bce05502a14f7ff988a9531da"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "bd40b6291385ada8027c37ea7c34af7cc7ef3c339a642ce53f037391b3a06f43"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
