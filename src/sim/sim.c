#include "pf/sim.h"

_Static_assert(sizeof(uint32_t) == 4U,
               "pf_sim requires an exact 32-bit uint32_t");

uint32_t pf_sim_abi_version(void)
{
    return PF_SIM_ABI_VERSION;
}

uint32_t pf_sim_tick_rate_hz(void)
{
    return PF_SIM_TICK_RATE_HZ;
}
