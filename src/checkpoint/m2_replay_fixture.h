#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "97df1b6238c1fd789ee0861bd6fc126ab21113d02d37e5d2f4347a296edca634"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "3c679aa1d7b1a4dc8c816c94e730adcbca50a0bb4634e5e407f18d3a6316b573"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
