#include "movement_model.h"

#include <stdint.h>

#if defined(__wasm__)
#define M0_EXPORT(name)                                                        \
    __attribute__((export_name(name))) __attribute__((used))
#else
#define M0_EXPORT(name)
#endif

enum
{
    M0_MODEL_FLOAT32 = 0,
    M0_MODEL_Q16_16 = 1
};

static M0MovementPair g_pair;
static uint32_t g_seed;
static int g_float_candidate;

static uint32_t mix_seed(uint32_t value)
{
    value += UINT32_C(0x9e3779b9);
    value = (value ^ (value >> 16U)) * UINT32_C(0x21f0aaad);
    value = (value ^ (value >> 15U)) * UINT32_C(0x735a2d97);
    return value ^ (value >> 15U);
}

static int model_for_candidate(int candidate)
{
    int normalized = candidate == 0 ? 0 : 1;
    return normalized == g_float_candidate ? M0_MODEL_FLOAT32
                                            : M0_MODEL_Q16_16;
}

static M0MovementView view_for_candidate(int candidate)
{
    if (model_for_candidate(candidate) == M0_MODEL_FLOAT32)
    {
        return m0_float_view(&g_pair.float32);
    }
    return m0_fixed_view(&g_pair.q16_16);
}

M0_EXPORT("m0_version")
int m0_web_version(void)
{
    return 2;
}

M0_EXPORT("m0_reset")
void m0_web_reset(uint32_t seed)
{
    g_seed = seed;
    g_float_candidate = (int)(mix_seed(seed) & 1U);
    m0_pair_reset(&g_pair);
}

M0_EXPORT("m0_step")
void m0_web_step(int move_x, int jump_pressed, int jump_held, int down_held)
{
    M0MovementInput input = {0};
    input.move_x = m0_axis_clamp(move_x);
    input.jump_pressed = jump_pressed != 0;
    input.jump_held = jump_held != 0;
    input.down_held = down_held != 0;
    m0_pair_step(&g_pair, input);
}

M0_EXPORT("m0_get")
double m0_web_get(int candidate, int field)
{
    M0MovementView view = view_for_candidate(candidate);
    switch (field)
    {
    case 0:
        return view.x;
    case 1:
        return view.y;
    case 2:
        return view.velocity_x;
    case 3:
        return view.velocity_y;
    case 4:
        return (double)view.tick;
    case 5:
        return (double)view.respawns;
    case 6:
        return (double)view.jump_squat;
    case 7:
        return (double)view.grounded;
    case 8:
        return (double)view.on_platform;
    case 9:
        return (double)view.air_jumps;
    case 10:
        return (double)view.dash_ticks;
    default:
        return 0.0;
    }
}

M0_EXPORT("m0_stage_get")
double m0_web_stage_get(int field)
{
    const M0StageGeometry *stage = m0_stage_geometry();
    switch (field)
    {
    case 0:
        return stage->floor_left;
    case 1:
        return stage->floor_right;
    case 2:
        return stage->floor_y;
    case 3:
        return stage->platform_left;
    case 4:
        return stage->platform_right;
    case 5:
        return stage->platform_y;
    case 6:
        return stage->fighter_half_width;
    case 7:
        return stage->fighter_half_height;
    case 8:
        return stage->blast_left;
    case 9:
        return stage->blast_right;
    case 10:
        return stage->blast_bottom;
    default:
        return 0.0;
    }
}

M0_EXPORT("m0_model")
int m0_web_model(int candidate)
{
    return model_for_candidate(candidate);
}

M0_EXPORT("m0_seed")
uint32_t m0_web_seed(void)
{
    return g_seed;
}
