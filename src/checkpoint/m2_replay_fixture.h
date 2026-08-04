#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "478547e440e1fcc274760a9d6c0bdfbd62286438f27b2f448702cb6af9a3f03e"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "eda4430b3f2623afe857cafbf39929e81d87ae05d7b2128c68ceace2803f6c4b"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ffe86482a586401206fc75c01d8ecc959ab48e6ed053350d261352a19ddc25ca"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
