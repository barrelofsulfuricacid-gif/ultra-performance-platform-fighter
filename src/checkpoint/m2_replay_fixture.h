#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "e3359756020e844f973406666cc87874389374c4d42689d9a52a67eba63d941d"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "40fc23de68fe634ff70b7919e663a55780a3fa2e00e4ee60f8aec1d7b8008e60"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ddc1f793a4d9919988f4f44f6a78d7492a37b0f4721867f7f1f8ca5bb89ce2d7"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
