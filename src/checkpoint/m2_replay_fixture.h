#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "a1d9c1d97a3f20bdb9c76094c39b856f731a1eb2c0cca64ac05dd28a6e121949"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "3bdbbbc5d7faa6c8fd077ebd47aaa061f738a3561aa4c66ae2bfe4f8455cda6a"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "a4020969be032543b9b229c8801bde77581b9f7fe26a9fe8aca91527627b13ec"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
