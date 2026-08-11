#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "f7f59a2f68b3431ff459fb8342684f4701e936cb6f83298834adddcbc365a49e"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "f7697243c6a07965e31224c54f015798bef6d615ce6e2d1441576d6f1450f98b"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
