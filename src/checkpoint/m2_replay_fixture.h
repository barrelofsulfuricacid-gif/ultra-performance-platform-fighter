#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "4e0e593f8221b917a40fc3eb02af0aaecb387a47f15080b5f646958d04b487c8"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "bb4f27c373668db557afb3232ac43b0a1a59a980c8753cefd7b55da8ea6e00a3"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "cea7f525bc5cb4009c69f8ca7c1daf85e6bdfebc12f1a9583d45dde34da4d10a"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
