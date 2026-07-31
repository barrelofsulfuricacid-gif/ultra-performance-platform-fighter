#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "6078e1428783c2c3dcd3e423515023f65d214c34c03eb9b8fc41a9f7f3a7270c"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "d8e9cae6ef79561d07ccf6de41f64251e7546d25974ef624cc770c5da4fcccf1"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "d2f5992ecc10cd4fb54a6c7bb5165e2983b019207b76c3792cc4bde4379be14f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
