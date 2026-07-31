#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "41451f8c98092eaf119e2d112628fd9a19a12b94450ca5d05d2483316ff2b802"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "c4e6bdc3506c85f6e97569da28f921f9e5aa87f085fcad8d1289fff5d56330c6"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "d2f5992ecc10cd4fb54a6c7bb5165e2983b019207b76c3792cc4bde4379be14f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
