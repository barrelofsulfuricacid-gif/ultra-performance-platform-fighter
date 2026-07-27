#include "pf/sim.h"

#if PF_PRODUCT_RENDER_PROBE
#include "pf/render_packet.h"
#endif

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifndef PF_PRODUCT_NAME
#error "PF_PRODUCT_NAME must identify the product boundary"
#endif

#if PF_PRODUCT_BROWSER
extern void pf_web_set_status(const char *message);
extern int pf_web_render_probe(
    const float *clear_rgba,
    const float *positions_xy,
    const float *texture_uv,
    const float *colors_rgba,
    const uint8_t *texture_rgba,
    int vertex_count);
#endif

static int run_smoke(void)
{
#if PF_PRODUCT_RENDER_PROBE
    PF_RenderPacket render_packet;
#endif
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

#if PF_PRODUCT_RENDER_PROBE
    pf_render_packet_build_probe(&render_packet);
    if (!pf_render_packet_validate(&render_packet))
    {
        (void)fprintf(stderr, "%s-smoke=fail reason=render-packet\n", PF_PRODUCT_NAME);
        return 1;
    }
#endif

    (void)puts(message);

#if PF_PRODUCT_BROWSER
    pf_web_set_status(message);
#if PF_PRODUCT_RENDER_PROBE
    if (!pf_web_render_probe(
            render_packet.clear_rgba,
            render_packet.positions_xy,
            render_packet.texture_uv,
            render_packet.colors_rgba,
            render_packet.texture_rgba,
            (int)render_packet.vertex_count))
    {
        (void)fprintf(stderr, "%s-smoke=fail reason=webgl2\n", PF_PRODUCT_NAME);
        return 1;
    }
#endif
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
