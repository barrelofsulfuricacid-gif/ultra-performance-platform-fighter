#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "7c0a7c7a332e95e34fe414436c7d0c9d34faafc460264e4488dc83c66f0f820d"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "466a56f8b8767534b22ba11e8c61643c68f5b559cba9006b2095fb2259fc9745"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "509d826181cd7d047a2241b06fda4cb4c875477bd5b0828fcafcb65865b80ae5"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
