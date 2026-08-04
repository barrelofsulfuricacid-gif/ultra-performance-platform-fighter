#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "41c5f767c01a728a4633a09af2e69c44c0c2aa292e3214c2dff87575a5ab8cca"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "71d182d5536c3129c3ce79e978cc119eca9a90b04789d9749d1e4c665590fbba"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "6894dc902cde9f95468041086e665ce8da590f9d3b8a8940aed5ddde98683e52"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
