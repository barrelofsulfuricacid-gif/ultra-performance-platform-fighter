#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "d273cbd7b852712bcbb5eadff4f144dc906bca5bd16657a6630ac79c8aedce62"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "b8dd78966252c546b4a4807d8f0c2b110ca9cdac3f1e95cf68ed2c491fe82479"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "67d69f92e62c1d5f59c8f26ad3734b0b3e7200f03667f3f908a368bf62fbe84d"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
