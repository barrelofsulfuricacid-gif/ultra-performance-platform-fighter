#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "36452288611860eea89e051f26ce32dfe1a431537a0dae9bb7379047eadf1c2f"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "d015347ede291c4f8f3dd08cc794ac12d04a74bc1b789d5ecb86facef7e36745"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "d2f5992ecc10cd4fb54a6c7bb5165e2983b019207b76c3792cc4bde4379be14f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
