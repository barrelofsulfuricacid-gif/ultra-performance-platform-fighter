#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "e852f315c5bb42a7022c0e95afe411ce4fd625c62f8aaca908fbfed7c1ce5a80"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "85aecb7bdf76b7c6aa4c95986e087e7c34d62359f8123e9803db24e3bf53c87c"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ad1be8cd1b341cef74f23b39edd511124fb7515cafb36c81bee6ac87ff8e6a28"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
