#include "pf/sim.h"

#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    uint32_t abi_version = pf_sim_abi_version();
    uint32_t tick_rate_hz = pf_sim_tick_rate_hz();

    if (abi_version != PF_SIM_ABI_VERSION ||
        tick_rate_hz != PF_SIM_TICK_RATE_HZ)
    {
        (void)fprintf(stderr, "sim-contract=fail\n");
        return 1;
    }

    (void)printf(
        "sim-contract=pass abi=%" PRIu32 " tick_hz=%" PRIu32 "\n",
        abi_version,
        tick_rate_hz);
    return 0;
}
