#include "pf/sim.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int run_smoke(void)
{
    uint32_t abi_version = pf_sim_abi_version();
    uint32_t tick_rate_hz = pf_sim_tick_rate_hz();

    if (abi_version != PF_SIM_ABI_VERSION ||
        tick_rate_hz != PF_SIM_TICK_RATE_HZ)
    {
        (void)fprintf(stderr, "headless-smoke=fail\n");
        return 1;
    }

    (void)printf(
        "headless-smoke=pass sim_abi=%" PRIu32 " tick_hz=%" PRIu32 "\n",
        abi_version,
        tick_rate_hz);
    return 0;
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && strcmp(arguments[1], "--smoke") == 0)
    {
        return run_smoke();
    }

    (void)fprintf(stderr, "usage: headless --smoke\n");
    return 2;
}
