#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "6531cd69c3f0766ffb5c252ec0e4799b0a4ff5353ce1a7aa31ae37d740a28046"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "0cd7a7327a0e6fdbdaf149ebd12f69c473446a5d1a85e017c4b5df51cb68b16f"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "509d826181cd7d047a2241b06fda4cb4c875477bd5b0828fcafcb65865b80ae5"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
