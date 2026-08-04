#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "2c9b2e214c336b95408592b29669c31cf9fc36d7f8fe6714cd35387b8c82bd64"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "0b99990e67bd75d868dbf421edbc8a3a1727add168be367c88b11be8a64b52f6"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
