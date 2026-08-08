#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "5893af587684844c22c0fc6c7019f13748c4366c586e088ed1a24d4e1819c942"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "0235c47f05fdd37257bdd59ac5cfd5c7e107316a19f99001eefdeea7d78e951d"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
