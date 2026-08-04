#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "a7be7272ed74f5c409edfbb62670d3b79396e33552c037b8f67ef459416997fe"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "9bfff0050dec26d578658301b99d52644a142617d99d7c00e22b7c4a43c7b225"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "12c446555a8e4b81e544d762a9c066003f509f9859a7d8ba6afb5b5fab95db71"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
