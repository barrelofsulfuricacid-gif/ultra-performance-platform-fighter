#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "1149ddd6bfd08048ff48c833a736ce6d023f975a718351b9db9d399177cb2af1"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "1bcf9444a66610479106ff4bc0782e891365a1a36e4561513fe47e1777b5272a"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "61d93989ea9c831cd2cf562787fd978cb9e7bd05f694c87671e9520ec26ae280"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
