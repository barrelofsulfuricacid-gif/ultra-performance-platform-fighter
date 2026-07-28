#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "35fe264e2c2c8c4062cb84dbe73d3698cca8d92eb9e24adb46b6af732ea08b52"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "9ddeaa38cbba6050c3b15d0de04e91c99cf5b0182af5ca52a01d79f0f8387ff7"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
