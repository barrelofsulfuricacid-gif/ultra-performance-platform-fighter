#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "02f52e1f9c9dbf29e21264c50d2139b8968c6bff810da3b30e00d9ba34fb2e0b"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "e5c235be9bf70b79f62d383b2bbc58db55ffa602c9ce513adbc8a7df3ac0c257"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
