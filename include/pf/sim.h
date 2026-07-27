#ifndef PF_SIM_H
#define PF_SIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_SIM_ABI_VERSION UINT32_C(1)
#define PF_SIM_TICK_RATE_HZ UINT32_C(60)

uint32_t pf_sim_abi_version(void);
uint32_t pf_sim_tick_rate_hz(void);

#ifdef __cplusplus
}
#endif

#endif
