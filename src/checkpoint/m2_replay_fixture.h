#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "649b9ab2540b5e8d38b972756925b3349e82209235ed1aa8c58c8f51485ce1be"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "e4834ffac8b7be8ce77cf604710ca307caea512cca5eb00fae8487ca0fdc75b4"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
