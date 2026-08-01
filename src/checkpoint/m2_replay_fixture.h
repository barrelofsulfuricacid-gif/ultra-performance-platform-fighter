#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "67ff1c3503bdda326906273ceffad8b175bcced103781dde448a5aeb1303ce7b"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "b7a5fbfea9010aee916851a95bbd8c6daef01abcd59bc3ec51113da62334e64f"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "cea7f525bc5cb4009c69f8ca7c1daf85e6bdfebc12f1a9583d45dde34da4d10a"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
