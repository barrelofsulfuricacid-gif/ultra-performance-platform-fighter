#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "4e5a99751df55cee2900d5d777e2fc727593c795cf984aed2a2b7d8abf3ad478"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "a4dcf5ecfd9b77068156d6deb4e26c8fdfff0036f0d4d395ed2d22e97a1b32af"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "9ee69c2aceb5a2f2eb9b547dc86bcddf424b88fb4d1c4453a059c72eccec80f6"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
