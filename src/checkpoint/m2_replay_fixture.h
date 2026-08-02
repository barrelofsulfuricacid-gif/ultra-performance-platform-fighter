#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "278844212e6d27dcbf2f859212289d589cb78785c77c865e927e2177e8486dd2"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "21092c36a062e6f6f13c3859b63b49fbcb1d747a6136548eb96868554cd26206"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
