#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "e327a55b18221e35eb106a70de7cae48db1c38068e38fa13fc9680ba4e4759a4"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "77ddce5af5e91b4c4e5d91bebcc3c4ad73ca0dc214262fbbc922222799033f33"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
