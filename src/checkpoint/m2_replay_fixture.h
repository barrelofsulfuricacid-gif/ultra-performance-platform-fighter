#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "a62e9ee777bc4571f38fe7019e0c7016cdf0e4a3681afbfbfd640f103b714904"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "d4327bbaa0b8a681ce87076f6db48367502497051376b298cb48da018d3d5c83"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "07cf2309c1da3ce73cddbee2a651698e3f6cdb2a1295c231863e7fe11d4e2d1f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
