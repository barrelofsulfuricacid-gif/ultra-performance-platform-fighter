#include "pf/render_packet.h"
#include "pf/sim.h"
#include "m4_native_playtest.h"

#include <SDL3/SDL.h>

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PF_SMOKE_SURFACE_WIDTH 96
#define PF_SMOKE_SURFACE_HEIGHT 64

static int report_sdl_failure(const char *stage)
{
    (void)fprintf(
        stderr,
        "native-client-smoke=fail stage=%s error=%s\n",
        stage,
        SDL_GetError());
    return 0;
}

static int run_event_and_gamepad_probe(void)
{
    static const char guid_text[] = "03000000deadbeef0000000000000000";
    char mapping[256];
    char *stored_mapping;
    SDL_GUID guid;
    SDL_Event event;
    Uint32 event_type;
    int mapping_result;
    int written;
    int found_event = 0;

    event_type = SDL_RegisterEvents(1);
    if (event_type == 0u)
    {
        return report_sdl_failure("register-event");
    }

    SDL_zero(event);
    event.type = event_type;
    event.user.code = 0x5046;
    if (!SDL_PushEvent(&event))
    {
        return report_sdl_failure("push-event");
    }

    while (SDL_PollEvent(&event))
    {
        if (event.type == event_type && event.user.code == 0x5046)
        {
            found_event = 1;
        }
    }
    if (!found_event)
    {
        return report_sdl_failure("poll-event");
    }

    written = snprintf(
        mapping,
        sizeof(mapping),
        "%s,PF M1 Virtual,a:b0,b:b1,leftx:a0,lefty:a1,platform:%s,",
        guid_text,
        SDL_GetPlatform());
    if (written < 0 || (size_t)written >= sizeof(mapping))
    {
        return report_sdl_failure("format-gamepad-mapping");
    }

    mapping_result = SDL_AddGamepadMapping(mapping);
    if (mapping_result < 0)
    {
        return report_sdl_failure("add-gamepad-mapping");
    }

    guid = SDL_StringToGUID(guid_text);
    stored_mapping = SDL_GetGamepadMappingForGUID(guid);
    if (stored_mapping == NULL ||
        strstr(stored_mapping, "PF M1 Virtual") == NULL)
    {
        SDL_free(stored_mapping);
        return report_sdl_failure("read-gamepad-mapping");
    }

    SDL_free(stored_mapping);
    return 1;
}

static SDL_Texture *create_probe_texture(
    SDL_Renderer *renderer,
    const PF_RenderPacket *packet)
{
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        (int)PF_RENDER_PACKET_TEXTURE_WIDTH,
        (int)PF_RENDER_PACKET_TEXTURE_HEIGHT);

    if (texture == NULL)
    {
        return NULL;
    }

    if (!SDL_UpdateTexture(
            texture,
            NULL,
            packet->texture_rgba,
            (int)(PF_RENDER_PACKET_TEXTURE_WIDTH * 4u)) ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND))
    {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    return texture;
}

static int render_probe_packet(
    SDL_Renderer *renderer,
    SDL_Texture *texture,
    int width,
    int height,
    const PF_RenderPacket *packet)
{
    float positions[PF_RENDER_PACKET_VERTEX_CAPACITY * 2u];
    SDL_FColor colors[PF_RENDER_PACKET_VERTEX_CAPACITY];
    size_t index;

    for (index = 0u; index < packet->vertex_count; ++index)
    {
        size_t position_offset = index * 2u;
        size_t color_offset = index * 4u;

        positions[position_offset] =
            (packet->positions_xy[position_offset] + 1.0F) *
            (float)width *
            0.5F;
        positions[position_offset + 1u] =
            (1.0F - packet->positions_xy[position_offset + 1u]) *
            (float)height *
            0.5F;
        colors[index].r = packet->colors_rgba[color_offset];
        colors[index].g = packet->colors_rgba[color_offset + 1u];
        colors[index].b = packet->colors_rgba[color_offset + 2u];
        colors[index].a = packet->colors_rgba[color_offset + 3u];
    }

    if (!SDL_SetRenderDrawColorFloat(
            renderer,
            packet->clear_rgba[0],
            packet->clear_rgba[1],
            packet->clear_rgba[2],
            packet->clear_rgba[3]) ||
        !SDL_RenderClear(renderer) ||
        !SDL_RenderGeometryRaw(
            renderer,
            texture,
            positions,
            (int)(sizeof(float) * 2u),
            colors,
            (int)sizeof(SDL_FColor),
            packet->texture_uv,
            (int)(sizeof(float) * 2u),
            (int)packet->vertex_count,
            NULL,
            0,
            0) ||
        !SDL_RenderPresent(renderer))
    {
        return report_sdl_failure("render-geometry");
    }

    return 1;
}

static int run_smoke(void)
{
    PF_RenderPacket packet;
    SDL_Surface *surface = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    Uint8 red = 0u;
    Uint8 green = 0u;
    Uint8 blue = 0u;
    Uint8 alpha = 0u;
    int result = 1;

    if (pf_sim_abi_version() != PF_SIM_ABI_VERSION ||
        pf_sim_tick_rate_hz() != PF_SIM_TICK_RATE_HZ)
    {
        (void)puts("native-client-smoke=fail stage=sim-contract");
        return 1;
    }
    if (SDL_GetVersion() != SDL_VERSION)
    {
        (void)puts("native-client-smoke=fail stage=sdl-version");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
    {
        (void)report_sdl_failure("init-events-gamepad");
        return 1;
    }
    if (!run_event_and_gamepad_probe())
    {
        result = 1;
        goto cleanup;
    }
    if (!pf_m4_native_playtest_smoke())
    {
        (void)puts("native-client-smoke=fail stage=playtest-contract");
        result = 1;
        goto cleanup;
    }

    pf_render_packet_build_probe(&packet);
    if (!pf_render_packet_validate(&packet))
    {
        (void)puts("native-client-smoke=fail stage=render-packet");
        result = 1;
        goto cleanup;
    }

    surface = SDL_CreateSurface(
        PF_SMOKE_SURFACE_WIDTH,
        PF_SMOKE_SURFACE_HEIGHT,
        SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL)
    {
        (void)report_sdl_failure("create-surface");
        result = 1;
        goto cleanup;
    }

    renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == NULL)
    {
        (void)report_sdl_failure("create-software-renderer");
        result = 1;
        goto cleanup;
    }

    texture = create_probe_texture(renderer, &packet);
    if (texture == NULL)
    {
        (void)report_sdl_failure("create-texture");
        result = 1;
        goto cleanup;
    }

    if (!render_probe_packet(
            renderer,
            texture,
            PF_SMOKE_SURFACE_WIDTH,
            PF_SMOKE_SURFACE_HEIGHT,
            &packet) ||
        !SDL_ReadSurfacePixel(
            surface,
            PF_SMOKE_SURFACE_WIDTH / 2,
            PF_SMOKE_SURFACE_HEIGHT / 2,
            &red,
            &green,
            &blue,
            &alpha))
    {
        (void)report_sdl_failure("read-pixel");
        result = 1;
        goto cleanup;
    }

    if (red <= 20u && green <= 30u && blue <= 50u)
    {
        (void)puts("native-client-smoke=fail stage=pixel-check");
        result = 1;
        goto cleanup;
    }

    (void)printf(
        "native-client-smoke=pass sim_abi=%" PRIu32 " tick_hz=%" PRIu32 " "
        "sdl=%d.%d.%d event=pass gamepad=pass geometry=pass "
        "batch_draws=1 pixel=%u,%u,%u,%u\n",
        (uint32_t)PF_SIM_ABI_VERSION,
        (uint32_t)PF_SIM_TICK_RATE_HZ,
        SDL_MAJOR_VERSION,
        SDL_MINOR_VERSION,
        SDL_MICRO_VERSION,
        red,
        green,
        blue,
        alpha);
    result = 0;

cleanup:
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
    return result;
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && strcmp(arguments[1], "--smoke") == 0)
    {
        return run_smoke();
    }
    if (argument_count == 1)
    {
        return pf_m4_native_playtest_run();
    }

    (void)fprintf(stderr, "usage: %s [--smoke]\n", arguments[0]);
    return 2;
}
