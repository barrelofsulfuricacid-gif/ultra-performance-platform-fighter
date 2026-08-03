#ifndef PF_M2_REPLAY_FIXTURE_H
#define PF_M2_REPLAY_FIXTURE_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_M2_REPLAY_TICKS UINT64_C(180)
#define PF_M2_REPLAY_PLAYERS UINT8_C(4)
#define PF_M2_REPLAY_SEED UINT64_C(0x0123456789abcdef)
#define PF_M2_REPLAY_CORPUS_SHA256                                      \
    "6a898284ea4273b633f10c05e0956d2a161752467458afd2fe045aa7fd1d6259"
#define PF_M2_REPLAY_FINAL_SHA256                                       \
    "12d114300ad716e48a422e302228a13b5ff39ed5711263ef538b103f327faf37"
#define PF_M2_REPLAY_EVENTS_SHA256                                      \
    "726ba7e815663eed26c6a6adfab6012e4214c393aabeb7e0a468f395fd4aa224"

pf_content_view pf_m2_replay_make_content(void);

void pf_m2_replay_make_tick_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick);

#endif
