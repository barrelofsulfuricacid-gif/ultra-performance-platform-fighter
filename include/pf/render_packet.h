#ifndef PF_RENDER_PACKET_H
#define PF_RENDER_PACKET_H

#include <stddef.h>
#include <stdint.h>

#define PF_RENDER_PACKET_VERTEX_CAPACITY 12u
#define PF_RENDER_PACKET_TEXTURE_WIDTH 2u
#define PF_RENDER_PACKET_TEXTURE_HEIGHT 2u
#define PF_RENDER_PACKET_TEXTURE_BYTES 16u

typedef struct PF_RenderPacket
{
    float clear_rgba[4];
    float positions_xy[PF_RENDER_PACKET_VERTEX_CAPACITY * 2u];
    float texture_uv[PF_RENDER_PACKET_VERTEX_CAPACITY * 2u];
    float colors_rgba[PF_RENDER_PACKET_VERTEX_CAPACITY * 4u];
    uint8_t texture_rgba[PF_RENDER_PACKET_TEXTURE_BYTES];
    size_t vertex_count;
} PF_RenderPacket;

void pf_render_packet_build_probe(PF_RenderPacket *packet);
int pf_render_packet_validate(const PF_RenderPacket *packet);

#endif
