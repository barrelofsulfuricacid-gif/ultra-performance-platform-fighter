#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "cee8d6fa7b625d745fb927e62bb512ebe1722e6127d1fa076d85a94ccb77ea5d"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "6758ad700cd0dab99c30a97d32d663e05eb4061f90cae6719b3d4e133c5284a4"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "be839fb07f9c44eb30d1ef7391b004f84a1dcf9bbb047ab5f49f7fac7d6454a1"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
