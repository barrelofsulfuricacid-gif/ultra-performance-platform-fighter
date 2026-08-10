#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "540695baf9fa0bce01ac9342310f78ecd40b5406bd03df57a81e8b557663d798"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "4ffccdd98a49489adf6737f54d5d987bc1c591c71cb1d39aa53d33f2e9c630f6"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
