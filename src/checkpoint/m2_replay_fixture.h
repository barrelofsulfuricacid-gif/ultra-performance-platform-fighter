#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "7de13f6a61f41619113c004203979d889a3603d2d8e5a60cd0ed2fab96d7a35f"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "55581ad6489814368e5408eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
