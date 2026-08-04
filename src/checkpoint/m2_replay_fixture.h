#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "93e60fef3c6afcac94d66b96ac4a29dd5257c39617700969447e47fe51c8278f"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "61160ef3e40848b4e5e529a27f81a0658938152bf6e3e4b0acb7395d32d2890e"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
