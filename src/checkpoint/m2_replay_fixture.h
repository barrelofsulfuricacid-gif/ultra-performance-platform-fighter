#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "0100de6c59b7b31306710bfd55923fa78e367d996c4bd4d1a60dd6efd1db9c16"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "840e3df343bd58e176f80b48a2e05578537f326583ff10f29e162ff83eafaba0"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
