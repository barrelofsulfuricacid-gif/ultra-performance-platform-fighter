#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "9d40ce0f622f748e85ebc663cc1394d531b6757ee66ef3538287a85576756a6b"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "863a4ed6b2f6334f6b67c15cf8c219d93ef84c8300792d6820db304dfa2e8e23"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "6a58ca0e2ef8dd3e08308d8b8d3085c22c73530400286013f305f2343f38bf87"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
