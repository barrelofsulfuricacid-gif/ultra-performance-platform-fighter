#include "pf/render_packet.h"

#include <stdio.h>

int main(void)
{
    PF_RenderPacket packet;

    pf_render_packet_build_probe(&packet);
    if (!pf_render_packet_validate(&packet))
    {
        (void)puts("render-packet=fail");
        return 1;
    }

    (void)printf(
        "render-packet=pass vertices=%zu texture=%ux%u\n",
        packet.vertex_count,
        PF_RENDER_PACKET_TEXTURE_WIDTH,
        PF_RENDER_PACKET_TEXTURE_HEIGHT);
    return 0;
}
