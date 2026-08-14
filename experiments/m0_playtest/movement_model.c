#include "movement_model.h"

#include <stddef.h>
#include <string.h>

enum
{
    JUMP_SQUAT_TICKS = 3,
    AIR_JUMPS = 1,
    PLATFORM_DROP_TICKS = 8,
    DASH_TICKS = 10,
    DASH_STICK_THRESHOLD = 24575
};

static const float k_spawn_x = 0.0f;
static const float k_fighter_half_height = 0.55f;
static const float k_floor_left = -6.5f;
static const float k_floor_right = 6.5f;
static const float k_floor_y = 5.5f;
static const float k_platform_left = -2.2f;
static const float k_platform_right = 2.2f;
static const float k_platform_y = 2.75f;
static const float k_blast_left = -10.5f;
static const float k_blast_right = 10.5f;
static const float k_blast_bottom = 9.5f;

static const float k_ground_acceleration = 0.018f;
static const float k_turn_acceleration = 0.031f;
static const float k_ground_speed = 0.14f;
static const float k_dash_speed = 0.2f;
static const float k_traction = 0.014f;
static const float k_air_acceleration = 0.0045f;
static const float k_air_speed = 0.11f;
static const float k_gravity = 0.0068f;
static const float k_fall_speed = 0.18f;
static const float k_fast_fall_speed = 0.24f;
static const float k_full_jump_speed = 0.24f;
static const float k_short_jump_speed = 0.1392f;
static const float k_double_jump_speed = 0.21f;
static const float k_drop_nudge = 0.00390625f;

static const M0StageGeometry k_geometry = {
    -6.5f,
    6.5f,
    5.5f,
    -2.2f,
    2.2f,
    2.75f,
    0.35f,
    0.55f,
    -10.5f,
    10.5f,
    9.5f};

static float approach(float value, float target, float amount)
{
    if (value < target)
    {
        float next = value + amount;
        return next > target ? target : next;
    }
    if (value > target)
    {
        float next = value - amount;
        return next < target ? target : next;
    }
    return value;
}

static int signs_differ(float left, float right)
{
    return (left < 0.0f && right > 0.0f) ||
           (left > 0.0f && right < 0.0f);
}

static int8_t strong_direction(int16_t axis)
{
    if (axis >= DASH_STICK_THRESHOLD)
    {
        return 1;
    }
    if (axis <= -DASH_STICK_THRESHOLD)
    {
        return -1;
    }
    return 0;
}

static void spawn(M0Motion *state, int preserve_clock)
{
    uint32_t tick = preserve_clock ? state->tick : 0U;
    uint32_t respawns = preserve_clock ? state->respawns + 1U : 0U;

    memset(state, 0, sizeof(*state));
    state->x = k_spawn_x;
    state->y = k_floor_y - k_fighter_half_height;
    state->tick = tick;
    state->respawns = respawns;
    state->grounded = 1U;
    state->air_jumps = AIR_JUMPS;
}

void m0_reset(M0Motion *state)
{
    spawn(state, 0);
}

void m0_pair_reset(M0MovementPair *pair)
{
    m0_reset(&pair->primary);
    m0_reset(&pair->repeat);
}

static void leave_invalid_support(M0Motion *state,
                                  M0MovementInput input)
{
    if (!state->grounded)
    {
        return;
    }

    if (state->on_platform)
    {
        if (input.down_held || state->x < k_platform_left ||
            state->x > k_platform_right)
        {
            state->grounded = 0U;
            state->on_platform = 0U;
            if (input.down_held)
            {
                state->platform_drop_ticks = PLATFORM_DROP_TICKS;
                state->y += k_drop_nudge;
            }
        }
    }
    else if (state->x < k_floor_left || state->x > k_floor_right)
    {
        state->grounded = 0U;
    }
}

static void land(M0Motion *state, float surface_y, int on_platform)
{
    state->y = surface_y - k_fighter_half_height;
    state->velocity_y = 0.0f;
    state->grounded = 1U;
    state->on_platform = (uint8_t)on_platform;
    state->air_jumps = AIR_JUMPS;
    state->short_hop_latched = 0U;
    state->dash_ticks = 0U;
    state->dash_direction = 0;
}

void m0_step(M0Motion *state, M0MovementInput input)
{
    int16_t clamped_axis = m0_axis_clamp(input.move_x);
    int8_t input_strong_direction = strong_direction(clamped_axis);
    float axis = (float)clamped_axis / (float)M0_AXIS_MAX;
    const float ground_speed = k_ground_speed;
    const float air_speed = k_air_speed;

    if (state->platform_drop_ticks > 0U)
    {
        state->platform_drop_ticks--;
    }

    if (state->grounded)
    {
        int dash_started =
            (input_strong_direction != 0 &&
             state->previous_strong_direction == 0) ||
            (state->dash_ticks > 0U &&
             input_strong_direction == -state->dash_direction);

        if (dash_started)
        {
            state->dash_ticks = DASH_TICKS;
            state->dash_direction = input_strong_direction;
            state->velocity_x =
                (float)state->dash_direction * k_dash_speed;
        }
        else if (state->dash_ticks > 0U &&
                 input_strong_direction == state->dash_direction)
        {
            state->velocity_x =
                (float)state->dash_direction * k_dash_speed;
            state->dash_ticks--;
        }
        else
        {
            float target = axis * ground_speed;
            const float acceleration = signs_differ(state->velocity_x, target)
                                           ? k_turn_acceleration
                                           : k_ground_acceleration;
            state->dash_ticks = 0U;
            state->dash_direction = 0;
            if (clamped_axis == 0)
            {
                state->velocity_x =
                    approach(state->velocity_x, 0.0f, k_traction);
            }
            else
            {
                state->velocity_x =
                    approach(state->velocity_x, target, acceleration);
            }
        }
    }
    else
    {
        state->dash_ticks = 0U;
        state->dash_direction = 0;
        state->velocity_x =
            approach(state->velocity_x, axis * air_speed,
                     k_air_acceleration);
    }

    state->x += state->velocity_x;
    leave_invalid_support(state, input);
    if (!state->grounded)
    {
        state->dash_ticks = 0U;
        state->dash_direction = 0;
    }

    if (state->grounded && input.jump_pressed && state->jump_squat == 0U)
    {
        state->jump_squat = JUMP_SQUAT_TICKS;
        state->short_hop_latched = 0U;
    }

    if (state->jump_squat > 0U)
    {
        if (!input.jump_held)
        {
            state->short_hop_latched = 1U;
        }
        state->jump_squat--;
        if (state->jump_squat == 0U)
        {
            const float speed = state->short_hop_latched
                                    ? k_short_jump_speed
                                    : k_full_jump_speed;
            state->velocity_y = -speed;
            state->grounded = 0U;
            state->on_platform = 0U;
            state->dash_ticks = 0U;
            state->dash_direction = 0;
        }
    }
    else if (!state->grounded && input.jump_pressed && state->air_jumps > 0U)
    {
        state->velocity_y = -k_double_jump_speed;
        state->air_jumps--;
        state->short_hop_latched = 0U;
    }

    if (!state->grounded)
    {
        const float previous_bottom = state->y + k_fighter_half_height;

        if (input.down_held && state->velocity_y > 0.0f)
        {
            state->velocity_y = k_fast_fall_speed;
        }
        else
        {
            state->velocity_y =
                approach(state->velocity_y, k_fall_speed, k_gravity);
        }

        state->y += state->velocity_y;

        {
            const float new_bottom = state->y + k_fighter_half_height;
            if (!input.down_held && state->platform_drop_ticks == 0U &&
                state->velocity_y >= 0.0f &&
                state->x >= k_platform_left &&
                state->x <= k_platform_right &&
                previous_bottom <= k_platform_y &&
                new_bottom >= k_platform_y)
            {
                land(state, k_platform_y, 1);
            }
            else if (state->velocity_y >= 0.0f &&
                     state->x >= k_floor_left &&
                     state->x <= k_floor_right &&
                     previous_bottom <= k_floor_y &&
                     new_bottom >= k_floor_y)
            {
                land(state, k_floor_y, 0);
            }
        }
    }
    else
    {
        state->y = (state->on_platform ? k_platform_y : k_floor_y) -
                   k_fighter_half_height;
    }

    state->previous_strong_direction = input_strong_direction;

    if (state->x < k_blast_left || state->x > k_blast_right ||
        state->y > k_blast_bottom)
    {
        spawn(state, 1);
    }
    state->tick++;
}

void m0_pair_step(M0MovementPair *pair, M0MovementInput input)
{
    m0_step(&pair->primary, input);
    m0_step(&pair->repeat, input);
}

M0MovementView m0_view(const M0Motion *state)
{
    M0MovementView view;
    view.x = state->x;
    view.y = state->y;
    view.velocity_x = state->velocity_x;
    view.velocity_y = state->velocity_y;
    view.tick = state->tick;
    view.respawns = state->respawns;
    view.jump_squat = state->jump_squat;
    view.grounded = state->grounded;
    view.on_platform = state->on_platform;
    view.air_jumps = state->air_jumps;
    view.dash_ticks = state->dash_ticks;
    return view;
}

const M0StageGeometry *m0_stage_geometry(void)
{
    return &k_geometry;
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t index;
    for (index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

uint64_t m0_hash(const M0Motion *state)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    uint32_t bits[4];
    memcpy(&bits[0], &state->x, sizeof(bits[0]));
    memcpy(&bits[1], &state->y, sizeof(bits[1]));
    memcpy(&bits[2], &state->velocity_x, sizeof(bits[2]));
    memcpy(&bits[3], &state->velocity_y, sizeof(bits[3]));
    hash = hash_bytes(hash, bits, sizeof(bits));
    hash = hash_bytes(hash, &state->tick, sizeof(state->tick));
    hash = hash_bytes(hash, &state->respawns, sizeof(state->respawns));
    hash = hash_bytes(hash, &state->jump_squat, sizeof(state->jump_squat));
    hash = hash_bytes(hash, &state->grounded, sizeof(state->grounded));
    hash = hash_bytes(hash, &state->on_platform, sizeof(state->on_platform));
    hash = hash_bytes(hash, &state->air_jumps, sizeof(state->air_jumps));
    hash = hash_bytes(hash, &state->short_hop_latched,
                      sizeof(state->short_hop_latched));
    hash = hash_bytes(hash, &state->platform_drop_ticks,
                      sizeof(state->platform_drop_ticks));
    hash = hash_bytes(hash, &state->dash_ticks, sizeof(state->dash_ticks));
    hash = hash_bytes(hash, &state->dash_direction,
                      sizeof(state->dash_direction));
    hash = hash_bytes(hash, &state->previous_strong_direction,
                      sizeof(state->previous_strong_direction));
    return hash;
}

int16_t m0_axis_clamp(int value)
{
    if (value < M0_AXIS_MIN)
    {
        return M0_AXIS_MIN;
    }
    if (value > M0_AXIS_MAX)
    {
        return M0_AXIS_MAX;
    }
    return (int16_t)value;
}

_Static_assert(sizeof(float) == sizeof(uint32_t),
               "the movement model requires a 32-bit float");
