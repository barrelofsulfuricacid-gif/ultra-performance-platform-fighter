#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
