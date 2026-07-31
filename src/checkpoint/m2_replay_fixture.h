#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "bc930671111a7bd821efc8d67f862e06425ea34d3ccc011f3ba52f69c5a96507"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "2c8bacffc869812c9c4a576a796cbcde7a7f923166fba6e17b16d595bc43366d"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "32df182c93ce9143357b6472615d90c9cc01e622488400d4eec54d7c89cab35f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
