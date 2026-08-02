#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "3c7130d92683b83e6b6260e74907c6719f0e511d043fdbb1185e1d70403b50e1"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "6fa0766f63c3582bfd61edbd231ee455c59ae4ca2729e80b3d10b0cd981ea405"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
