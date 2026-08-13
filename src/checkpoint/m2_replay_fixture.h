#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "fb0f4e7251e70f7660801222b5b5a2627e9c45e1b56b7d5763035947cb553d1c"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "5a7db4a5e899b1af31909f7997dcb1a08226aec79f4f09fab7422fe9602f246f"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
