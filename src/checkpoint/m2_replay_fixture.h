#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(240)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "f5b82e98dd6d61e716e11ab7b4d55502bfce6d0a522c73a88b556bf054505e5c"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "46441cbb10851c1fc73e663e844be90ea5407367ca50b049309ca3a73c551424"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "9e8e1c27c3c0624f7552033252cab603d6f6c64c2bfaf5440d2b3a78b8df83d8"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
