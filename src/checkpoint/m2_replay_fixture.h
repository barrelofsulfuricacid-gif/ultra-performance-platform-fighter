#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "e04c09a50c2de94b589c64c58e86d248ed31f46d96caad1042197008ef4111b0"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "82fcbafc2aa277aa614cb1f2fc5a026a3984667f3c089f0e3b689c8fbbf2ab18"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "32df182c93ce9143357b6472615d90c9cc01e622488400d4eec54d7c89cab35f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
