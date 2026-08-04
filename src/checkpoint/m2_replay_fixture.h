#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "0d303a7a8a30fe59f391bf7779e716f193de5fcabf7a9ae026fa4b566aafa028"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "2dca650173bd419852de3011d7e75f8887d61b1da8e31688f22c1a912c21905b"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "c9b0f348b2ca91d83ced7c5e2c290847c118ec8c8da936db4fad7a1639660206"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
