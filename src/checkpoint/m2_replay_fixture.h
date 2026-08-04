#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "30ab31b9c38c7f34c8d81324a40547db84b64353ddfbe6d8ca6602e2b0c31c2b"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "71bfda9f3448a5c140e1654578ad730806f4aad6b0f84bc0bb5eda6ddbed7e7c"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "12c446555a8e4b81e544d762a9c066003f509f9859a7d8ba6afb5b5fab95db71"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
