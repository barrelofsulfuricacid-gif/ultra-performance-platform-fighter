#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "0d1c16c1e231d29c89a49d193f6b10deb081297821d5448239307cae4d33f4ad"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "6c648e4463b070ad4b7e3b013ea620e21463b281fe39b00980cf0cbf558bfcd5"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
