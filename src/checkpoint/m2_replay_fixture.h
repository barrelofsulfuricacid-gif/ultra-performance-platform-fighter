#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "741b5f9451a3a81bf57a632c65e35a5ba35cbbd3fe45e331da4d196b6bfe3847"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "f9fb60d1a468e9cd99d43bd412b408d0dfe8c1b414b8a1ef8dfe08f4dea27702"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ddc1f793a4d9919988f4f44f6a78d7492a37b0f4721867f7f1f8ca5bb89ce2d7"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
