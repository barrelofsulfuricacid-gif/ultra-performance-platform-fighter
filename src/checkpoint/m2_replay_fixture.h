#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "430f8ce21ac7c8326e9adb3eb41d9d59973cd15cf66afacd0cc12eb73aeb3504"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "d9d47e250920373653ff0906a42cf3806004c4347726a9ed91213f2975f4e640"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "3ac6da05eb0d45cbc5944ee94d00f053f122d6099236b1e1b80fa7738b0ce9f0"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
