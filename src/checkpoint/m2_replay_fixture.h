#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "2af600d2b7582a9b6b1d8f4fbd4db2e95360cef5e2fc70718bed8883cc4530ff"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "ff4f441ae65ef09babd3c66db0b293eaa0d7967275d27c84b8285c888c1b8a9d"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "d2f5992ecc10cd4fb54a6c7bb5165e2983b019207b76c3792cc4bde4379be14f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
