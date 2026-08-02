#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "08955dbf44be5e54f229796b42342904ba6933b4ad5779e61c515b71fe1a62fa"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "17bfd5133e5926221fd71d526f2bbb62359a8a36b8c44949d92954137e25a5e2"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ad1be8cd1b341cef74f23b39edd511124fb7515cafb36c81bee6ac87ff8e6a28"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
