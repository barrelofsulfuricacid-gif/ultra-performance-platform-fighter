#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "9c0c87842664d5bbde8f50dd672f804e8a62d1a32f2a5d9bf5c3d7a9c031cc73"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "3a9bb1e28fd635dcde8f1ec98d0705babd12ee64ee7e036e8f986c5a15a874d5"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "370975f72bbd6546f5253607ef62b811cb4f126889ad3c89bf4b2955703430cb"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
