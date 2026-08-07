#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "47a9fe041eaf90013aa080907ca0168ca488616b95901f207cbe4cc755704590"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "370bdaa36efeeb6d0b7dc0278a46316018f68a8c0ace8c7a213327d142aea66f"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "79bff77cc0438838f3c40ed054ac6d96396414deca781d0f3b03b07bfa637811"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
