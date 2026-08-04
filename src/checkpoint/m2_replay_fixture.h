#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "47bb7eb24cf96c2a5bc46b2b11a83e451226fa0308fdc8d5cdaa43c6a38a9acd"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "9fef35cd32abe0fa98013bf32c1f98d953289551465aa5f2dffec60834bb5f56"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "b92e065007f6794dc1d26bb38313c185775ac14833f02d2f951fe9ba3a2aab18"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
