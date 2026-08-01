#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "3115914e6972924b856ccb02f9e4457818483c661efee8a1981c873ac52ebe13"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "1985071d6a58c81c7842e378fe8f0ff229d9846c0fabd2ee341308f057d087e6"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "32df182c93ce9143357b6472615d90c9cc01e622488400d4eec54d7c89cab35f"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
