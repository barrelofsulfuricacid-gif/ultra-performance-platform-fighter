#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "6727023fb07bcb7a4fcbaf9c0beac0f8220c1c1802b19da891ae2ae2be252240"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "de96572115c1e4850d79353839576efc4b780ccbd75e8e70a2f23bee419c14af"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "124a94734029321020513ec749b2f4d26cd60b4ed2129e25ce104692739fa9af"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
