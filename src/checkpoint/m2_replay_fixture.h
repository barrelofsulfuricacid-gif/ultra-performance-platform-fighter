#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "7c88b63efe32f0aee0ea95ff29c48eaaa9cb83c7f8b16f854fc047fe73ca7baf"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "c08fe7518639715fd1d88a2dc2cc8e763e8a11a484ecbff71b8790c85afdbb88"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
