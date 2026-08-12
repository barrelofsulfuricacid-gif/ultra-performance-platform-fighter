#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "1b2d49314b692a03114396f7eb662b5b574a1a2e0b045b9fa0a366db12852301"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "d9552577f2a31dcbcf582045cfc5af4033c15b519d4d315271e79e74a177c2af"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "7930e2a2d90ed4dd9f5234ba47f4d4fc11e2ce4fbc2cd22b9367473a71bb2451"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
