#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "469c03272c7ce71f684bad27dd53f55d76a4ace72535152ed2fa5cc451a78315"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "c00595389591d404fd06e60780138a99dfda498a6160c7318b3b6acf713d3081"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
