#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "972c1e83bb5a7f3c94195a0f6df6041f84eb739a534f9985a7f24d63bbc2f166"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "89d8e1749c908490db14f9e69e1c72ca2a51b8eaf6359a730aaa684b0b6555c3"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
