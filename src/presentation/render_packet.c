#include "pf/render_packet.h"

#include <stddef.h>

static void write_vertex(
    PF_RenderPacket *packet,
    size_t index,
    float x,
    float y,
    float u,
    float v,
    float red,
    float green,
    float blue,
    float alpha)
{
    size_t position_offset = index * 2u;
    size_t color_offset = index * 4u;

    packet->positions_xy[position_offset] = x;
    packet->positions_xy[position_offset + 1u] = y;
    packet->texture_uv[position_offset] = u;
    packet->texture_uv[position_offset + 1u] = v;
    packet->colors_rgba[color_offset] = red;
    packet->colors_rgba[color_offset + 1u] = green;
    packet->colors_rgba[color_offset + 2u] = blue;
    packet->colors_rgba[color_offset + 3u] = alpha;
}

static void write_quad(
    PF_RenderPacket *packet,
    size_t first_vertex,
    float left,
    float top,
    float right,
    float bottom,
    const float color[4])
{
    write_vertex(
        packet,
        first_vertex,
        left,
        top,
        0.0F,
        0.0F,
        color[0],
        color[1],
        color[2],
        color[3]);
    write_vertex(
        packet,
        first_vertex + 1u,
        right,
        top,
        1.0F,
        0.0F,
        color[0],
        color[1],
        color[2],
        color[3]);
    write_vertex(
        packet,
        first_vertex + 2u,
        right,
        bottom,
        1.0F,
        1.0F,
        color[0],
        color[1],
        color[2],
        color[3]);
    write_vertex(
        packet,
        first_vertex + 3u,
        left,
        top,
        0.0F,
        0.0F,
        color[0],
        color[1],
        color[2],
        color[3]);
    write_vertex(
        packet,
        first_vertex + 4u,
        right,
        bottom,
        1.0F,
        1.0F,
        color[0],
        color[1],
        color[2],
        color[3]);
    write_vertex(
        packet,
        first_vertex + 5u,
        left,
        bottom,
        0.0F,
        1.0F,
        color[0],
        color[1],
        color[2],
        color[3]);
}

void pf_render_packet_build_probe(PF_RenderPacket *packet)
{
    static const float warm_color[4] = {1.0F, 0.45F, 0.20F, 0.78F};
    static const float cool_color[4] = {0.20F, 0.65F, 1.0F, 0.68F};
    static const uint8_t texture[PF_RENDER_PACKET_TEXTURE_BYTES] = {
        255u,
        255u,
        255u,
        255u,
        210u,
        235u,
        255u,
        255u,
        255u,
        220u,
        190u,
        255u,
        190u,
        255u,
        225u,
        255u};
    size_t index;

    if (packet == NULL)
    {
        return;
    }

    packet->clear_rgba[0] = 0.05F;
    packet->clear_rgba[1] = 0.08F;
    packet->clear_rgba[2] = 0.15F;
    packet->clear_rgba[3] = 1.0F;
    packet->vertex_count = PF_RENDER_PACKET_VERTEX_CAPACITY;

    write_quad(
        packet,
        0u,
        -0.78F,
        -0.62F,
        0.30F,
        0.66F,
        warm_color);
    write_quad(
        packet,
        6u,
        -0.20F,
        -0.48F,
        0.78F,
        0.48F,
        cool_color);

    for (index = 0u; index < PF_RENDER_PACKET_TEXTURE_BYTES; ++index)
    {
        packet->texture_rgba[index] = texture[index];
    }
}

int pf_render_packet_validate(const PF_RenderPacket *packet)
{
    size_t index;

    if (packet == NULL ||
        packet->vertex_count != PF_RENDER_PACKET_VERTEX_CAPACITY)
    {
        return 0;
    }

    for (index = 0u; index < packet->vertex_count * 2u; ++index)
    {
        float position = packet->positions_xy[index];
        float uv = packet->texture_uv[index];

        if (position != position || position < -1.0F || position > 1.0F)
        {
            return 0;
        }
        if (uv != uv || uv < 0.0F || uv > 1.0F)
        {
            return 0;
        }
    }

    for (index = 0u; index < packet->vertex_count * 4u; ++index)
    {
        float color = packet->colors_rgba[index];

        if (color != color || color < 0.0F || color > 1.0F)
        {
            return 0;
        }
    }

    return 1;
}
