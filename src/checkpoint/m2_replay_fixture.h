#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "25b9ce8b95c8eb77ab92635d7480fc83f7e97ded6bd0925e4ca0d5cb423eb75b"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "768db033f6f9f0841b00cf876ef94b4aaa499dcce89b5b55e15e5bc4c3491537"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
