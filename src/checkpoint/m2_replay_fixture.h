#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "b0f176b41a3031756c236f1827080808c49104c3938e407287653e1557ac0ce9"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "e96399e8c83d2e148554d57b4b2287e11316eaa48780e8e6e729f92e47ee7517"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "ff1b77013c60df79c5d130be72f67e37205998038b9983621bd33cd88cd1d253"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
