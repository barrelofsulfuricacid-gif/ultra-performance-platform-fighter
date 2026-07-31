#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "b502a4dcbbaa83c5561c35c77d3305be44ad3dddbda59f3dfed85edaea50d95b"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "d0466359837de1292c9195d3ecfd9790aac2d7c3ab976228d62a1baa2be2c99a"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "32df182c93ce9143357b6472615d90c9cc01e622488400d4eec54d7c89cab35f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
