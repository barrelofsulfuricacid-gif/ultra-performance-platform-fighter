#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "617e8a8503be61670f7683f4478baf0699d49d21182346edb1d80003b130f86e"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "80e4140554f0ca0b797d5056049768128cdcd08487341464835a1909812ad4c7"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "c9b0f348b2ca91d83ced7c5e2c290847c118ec8c8da936db4fad7a1639660206"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
