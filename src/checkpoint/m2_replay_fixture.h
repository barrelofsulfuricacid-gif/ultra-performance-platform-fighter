#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "84fe270389ef33e6b39d2ea7afcee0435f1b5b731f36e47e2bdb02b62a5d207a"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "3f9275e0a1b0a07e8d9373696783ace52e8b660a9daf0b45d70b0e3e711c2c60"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ddc1f793a4d9919988f4f44f6a78d7492a37b0f4721867f7f1f8ca5bb89ce2d7"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
