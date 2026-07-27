#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if PF_PRODUCT_BROWSER
#include <emscripten.h>
#endif

#ifndef PF_PRODUCT_NAME
#error "PF_PRODUCT_NAME must identify the product boundary"
#endif

static int run_smoke(void)
{
    char message[128];
    uint32_t abi_version = pf_sim_abi_version();
    uint32_t tick_rate_hz = pf_sim_tick_rate_hz();
    int written;

    if (abi_version != PF_SIM_ABI_VERSION ||
        tick_rate_hz != PF_SIM_TICK_RATE_HZ)
    {
        (void)fprintf(stderr, "%s-smoke=fail\n", PF_PRODUCT_NAME);
        return 1;
    }

    written = snprintf(
        message,
        sizeof(message),
        "%s-smoke=pass sim_abi=%" PRIu32 " tick_hz=%" PRIu32,
        PF_PRODUCT_NAME,
        abi_version,
        tick_rate_hz);
    if (written < 0 || (size_t)written >= sizeof(message))
    {
        (void)fprintf(stderr, "%s-smoke=fail reason=format\n", PF_PRODUCT_NAME);
        return 1;
    }

    (void)puts(message);

#if PF_PRODUCT_BROWSER
    EM_ASM(
        {
            var status = document.getElementById("pf-status");
            if (!status)
            {
                status = document.createElement("pre");
                status.id = "pf-status";
                document.body.appendChild(status);
            }
            status.textContent = UTF8ToString($0);
        },
        message);
#endif

    return 0;
}

int main(int argument_count, char **arguments)
{
#if PF_PRODUCT_BROWSER
    (void)argument_count;
    (void)arguments;
    return run_smoke();
#else
    if (argument_count == 2 && strcmp(arguments[1], "--smoke") == 0)
    {
        return run_smoke();
    }

    (void)fprintf(stderr, "usage: %s --smoke\n", PF_PRODUCT_NAME);
    return 2;
#endif
}
