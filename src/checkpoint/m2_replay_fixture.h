#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "3f12c9091b250032e989b361d2c66621afc2657393a0942d820722ad1164ae68"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "559d0dba9f59dc44f2f5567cfa88f7c2ae2ac636e60586eeae556aa3f2435c36"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
