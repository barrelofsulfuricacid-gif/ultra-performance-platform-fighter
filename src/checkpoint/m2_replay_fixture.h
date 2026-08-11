#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "0c76acded6f603a54a0c66c1b1f0fdfab053b485d22adabf3da44bdc0ec37cfc"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "f04b6ff2ff80bf5dba91788ce69e0b62f0e394047a60928812943a0613c55637"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "9e8e1c27c3c0624f7552033252cab603d6f6c64c2bfaf5440d2b3a78b8df83d8"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
