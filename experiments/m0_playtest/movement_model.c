#include "movement_model.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

enum
{
    Q_SHIFT = 16,
    Q_ONE = 1 << Q_SHIFT,
    JUMP_SQUAT_TICKS = 3,
    AIR_JUMPS = 1,
    PLATFORM_DROP_TICKS = 8,
    DASH_TICKS = 10,
    DASH_STICK_THRESHOLD = 24575
};

#define Q_FROM_RATIO(numerator, denominator) \
    ((int32_t)(((int64_t)(numerator) * Q_ONE) / (denominator)))

static const int32_t k_spawn_x = 0;
static const int32_t k_fighter_half_height = Q_FROM_RATIO(11, 20);
static const int32_t k_floor_left = Q_FROM_RATIO(-13, 2);
static const int32_t k_floor_right = Q_FROM_RATIO(13, 2);
static const int32_t k_floor_y = Q_FROM_RATIO(11, 2);
static const int32_t k_platform_left = Q_FROM_RATIO(-11, 5);
static const int32_t k_platform_right = Q_FROM_RATIO(11, 5);
static const int32_t k_platform_y = Q_FROM_RATIO(11, 4);
static const int32_t k_blast_left = Q_FROM_RATIO(-21, 2);
static const int32_t k_blast_right = Q_FROM_RATIO(21, 2);
static const int32_t k_blast_bottom = Q_FROM_RATIO(19, 2);

static const int32_t k_ground_acceleration = Q_FROM_RATIO(9, 500);
static const int32_t k_turn_acceleration = Q_FROM_RATIO(31, 1000);
static const int32_t k_ground_speed = Q_FROM_RATIO(7, 50);
static const int32_t k_dash_speed = Q_FROM_RATIO(1, 5);
static const int32_t k_traction = Q_FROM_RATIO(7, 500);
static const int32_t k_air_acceleration = Q_FROM_RATIO(9, 2000);
static const int32_t k_air_speed = Q_FROM_RATIO(11, 100);
static const int32_t k_gravity = Q_FROM_RATIO(17, 2500);
static const int32_t k_fall_speed = Q_FROM_RATIO(9, 50);
static const int32_t k_fast_fall_speed = Q_FROM_RATIO(6, 25);
static const int32_t k_full_jump_speed = Q_FROM_RATIO(6, 25);
static const int32_t k_short_jump_speed = Q_FROM_RATIO(87, 625);
static const int32_t k_double_jump_speed = Q_FROM_RATIO(21, 100);
static const int32_t k_drop_nudge = Q_FROM_RATIO(1, 256);

static const M0StageGeometry k_geometry = {
    -6.5,
    6.5,
    5.5,
    -2.2,
    2.2,
    2.75,
    0.35,
    0.55,
    -10.5,
    10.5,
    9.5};

static int32_t q_multiply(int32_t left, int32_t right)
{
    int64_t product = (int64_t)left * (int64_t)right;
    if (product >= 0)
    {
        return (int32_t)((product + (Q_ONE / 2)) / Q_ONE);
    }
    return (int32_t)(-((-product + (Q_ONE / 2)) / Q_ONE));
}

static int32_t q_axis(int16_t axis)
{
    return (int32_t)(((int64_t)axis * Q_ONE) / M0_AXIS_MAX);
}

static float q_to_float(int32_t value)
{
    return (float)value / (float)Q_ONE;
}

static int32_t approach_q(int32_t value, int32_t target, int32_t amount)
{
    if (value < target)
    {
        int64_t next = (int64_t)value + amount;
        return next > target ? target : (int32_t)next;
    }
    if (value > target)
    {
        int64_t next = (int64_t)value - amount;
        return next < target ? target : (int32_t)next;
    }
    return value;
}

static float approach_float(float value, float target, float amount)
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

static int signs_differ_q(int32_t left, int32_t right)
{
    return (left < 0 && right > 0) || (left > 0 && right < 0);
}

static int signs_differ_float(float left, float right)
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

static void float_spawn(M0FloatMotion *state, int preserve_clock)
{
    uint32_t tick = preserve_clock ? state->tick : 0U;
    uint32_t respawns = preserve_clock ? state->respawns + 1U : 0U;

    memset(state, 0, sizeof(*state));
    state->x = q_to_float(k_spawn_x);
    state->y = q_to_float(k_floor_y - k_fighter_half_height);
    state->tick = tick;
    state->respawns = respawns;
    state->grounded = 1U;
    state->air_jumps = AIR_JUMPS;
}

static void fixed_spawn(M0FixedMotion *state, int preserve_clock)
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

void m0_float_reset(M0FloatMotion *state)
{
    float_spawn(state, 0);
}

void m0_fixed_reset(M0FixedMotion *state)
{
    fixed_spawn(state, 0);
}

void m0_pair_reset(M0MovementPair *pair)
{
    m0_float_reset(&pair->float32);
    m0_fixed_reset(&pair->q16_16);
}

static void float_leave_invalid_support(M0FloatMotion *state,
                                        M0MovementInput input)
{
    float floor_left = q_to_float(k_floor_left);
    float floor_right = q_to_float(k_floor_right);
    float platform_left = q_to_float(k_platform_left);
    float platform_right = q_to_float(k_platform_right);

    if (!state->grounded)
    {
        return;
    }

    if (state->on_platform)
    {
        if (input.down_held || state->x < platform_left ||
            state->x > platform_right)
        {
            state->grounded = 0U;
            state->on_platform = 0U;
            if (input.down_held)
            {
                state->platform_drop_ticks = PLATFORM_DROP_TICKS;
                state->y += q_to_float(k_drop_nudge);
            }
        }
    }
    else if (state->x < floor_left || state->x > floor_right)
    {
        state->grounded = 0U;
    }
}

static void fixed_leave_invalid_support(M0FixedMotion *state,
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

static void float_land(M0FloatMotion *state, float surface_y,
                       int on_platform)
{
    state->y = surface_y - q_to_float(k_fighter_half_height);
    state->velocity_y = 0.0f;
    state->grounded = 1U;
    state->on_platform = (uint8_t)on_platform;
    state->air_jumps = AIR_JUMPS;
    state->short_hop_latched = 0U;
    state->dash_ticks = 0U;
    state->dash_direction = 0;
}

static void fixed_land(M0FixedMotion *state, int32_t surface_y,
                       int on_platform)
{
    state->y = surface_y - k_fighter_half_height;
    state->velocity_y = 0;
    state->grounded = 1U;
    state->on_platform = (uint8_t)on_platform;
    state->air_jumps = AIR_JUMPS;
    state->short_hop_latched = 0U;
    state->dash_ticks = 0U;
    state->dash_direction = 0;
}

void m0_float_step(M0FloatMotion *state, M0MovementInput input)
{
    int16_t clamped_axis = m0_axis_clamp(input.move_x);
    int8_t input_strong_direction = strong_direction(clamped_axis);
    float axis = (float)clamped_axis / (float)M0_AXIS_MAX;
    float ground_speed = q_to_float(k_ground_speed);
    float air_speed = q_to_float(k_air_speed);

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
                (float)state->dash_direction * q_to_float(k_dash_speed);
        }
        else if (state->dash_ticks > 0U &&
                 input_strong_direction == state->dash_direction)
        {
            state->velocity_x =
                (float)state->dash_direction * q_to_float(k_dash_speed);
            state->dash_ticks--;
        }
        else
        {
            float target = axis * ground_speed;
            float acceleration = signs_differ_float(state->velocity_x, target)
                                     ? q_to_float(k_turn_acceleration)
                                     : q_to_float(k_ground_acceleration);
            state->dash_ticks = 0U;
            state->dash_direction = 0;
            if (clamped_axis == 0)
            {
                state->velocity_x =
                    approach_float(state->velocity_x, 0.0f,
                                   q_to_float(k_traction));
            }
            else
            {
                state->velocity_x =
                    approach_float(state->velocity_x, target, acceleration);
            }
        }
    }
    else
    {
        state->dash_ticks = 0U;
        state->dash_direction = 0;
        state->velocity_x =
            approach_float(state->velocity_x, axis * air_speed,
                           q_to_float(k_air_acceleration));
    }

    state->x += state->velocity_x;
    float_leave_invalid_support(state, input);
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
            float speed = q_to_float(state->short_hop_latched
                                         ? k_short_jump_speed
                                         : k_full_jump_speed);
            state->velocity_y = -speed;
            state->grounded = 0U;
            state->on_platform = 0U;
            state->dash_ticks = 0U;
            state->dash_direction = 0;
        }
    }
    else if (!state->grounded && input.jump_pressed && state->air_jumps > 0U)
    {
        state->velocity_y = -q_to_float(k_double_jump_speed);
        state->air_jumps--;
        state->short_hop_latched = 0U;
    }

    if (!state->grounded)
    {
        float previous_bottom =
            state->y + q_to_float(k_fighter_half_height);

        if (input.down_held && state->velocity_y > 0.0f)
        {
            state->velocity_y = q_to_float(k_fast_fall_speed);
        }
        else
        {
            state->velocity_y =
                approach_float(state->velocity_y,
                               q_to_float(k_fall_speed),
                               q_to_float(k_gravity));
        }

        state->y += state->velocity_y;

        {
            float new_bottom =
                state->y + q_to_float(k_fighter_half_height);
            float platform_y = q_to_float(k_platform_y);
            float floor_y = q_to_float(k_floor_y);
            if (!input.down_held && state->platform_drop_ticks == 0U &&
                state->velocity_y >= 0.0f &&
                state->x >= q_to_float(k_platform_left) &&
                state->x <= q_to_float(k_platform_right) &&
                previous_bottom <= platform_y && new_bottom >= platform_y)
            {
                float_land(state, platform_y, 1);
            }
            else if (state->velocity_y >= 0.0f &&
                     state->x >= q_to_float(k_floor_left) &&
                     state->x <= q_to_float(k_floor_right) &&
                     previous_bottom <= floor_y && new_bottom >= floor_y)
            {
                float_land(state, floor_y, 0);
            }
        }
    }
    else
    {
        state->y =
            q_to_float((state->on_platform ? k_platform_y : k_floor_y) -
                       k_fighter_half_height);
    }

    state->previous_strong_direction = input_strong_direction;

    if (state->x < q_to_float(k_blast_left) ||
        state->x > q_to_float(k_blast_right) ||
        state->y > q_to_float(k_blast_bottom))
    {
        float_spawn(state, 1);
    }
    state->tick++;
}

void m0_fixed_step(M0FixedMotion *state, M0MovementInput input)
{
    int16_t clamped_axis = m0_axis_clamp(input.move_x);
    int8_t input_strong_direction = strong_direction(clamped_axis);
    int32_t axis = q_axis(clamped_axis);

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
            state->velocity_x = state->dash_direction * k_dash_speed;
        }
        else if (state->dash_ticks > 0U &&
                 input_strong_direction == state->dash_direction)
        {
            state->velocity_x = state->dash_direction * k_dash_speed;
            state->dash_ticks--;
        }
        else
        {
            int32_t target = q_multiply(axis, k_ground_speed);
            int32_t acceleration = signs_differ_q(state->velocity_x, target)
                                       ? k_turn_acceleration
                                       : k_ground_acceleration;
            state->dash_ticks = 0U;
            state->dash_direction = 0;
            if (clamped_axis == 0)
            {
                state->velocity_x =
                    approach_q(state->velocity_x, 0, k_traction);
            }
            else
            {
                state->velocity_x =
                    approach_q(state->velocity_x, target, acceleration);
            }
        }
    }
    else
    {
        state->dash_ticks = 0U;
        state->dash_direction = 0;
        state->velocity_x =
            approach_q(state->velocity_x,
                       q_multiply(axis, k_air_speed),
                       k_air_acceleration);
    }

    state->x += state->velocity_x;
    fixed_leave_invalid_support(state, input);
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
            int32_t speed = state->short_hop_latched
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
        int32_t previous_bottom = state->y + k_fighter_half_height;

        if (input.down_held && state->velocity_y > 0)
        {
            state->velocity_y = k_fast_fall_speed;
        }
        else
        {
            state->velocity_y =
                approach_q(state->velocity_y, k_fall_speed, k_gravity);
        }

        state->y += state->velocity_y;

        {
            int32_t new_bottom = state->y + k_fighter_half_height;
            if (!input.down_held && state->platform_drop_ticks == 0U &&
                state->velocity_y >= 0 &&
                state->x >= k_platform_left &&
                state->x <= k_platform_right &&
                previous_bottom <= k_platform_y &&
                new_bottom >= k_platform_y)
            {
                fixed_land(state, k_platform_y, 1);
            }
            else if (state->velocity_y >= 0 &&
                     state->x >= k_floor_left &&
                     state->x <= k_floor_right &&
                     previous_bottom <= k_floor_y &&
                     new_bottom >= k_floor_y)
            {
                fixed_land(state, k_floor_y, 0);
            }
        }
    }
    else
    {
        state->y =
            (state->on_platform ? k_platform_y : k_floor_y) -
            k_fighter_half_height;
    }

    state->previous_strong_direction = input_strong_direction;

    if (state->x < k_blast_left || state->x > k_blast_right ||
        state->y > k_blast_bottom)
    {
        fixed_spawn(state, 1);
    }
    state->tick++;
}

void m0_pair_step(M0MovementPair *pair, M0MovementInput input)
{
    m0_float_step(&pair->float32, input);
    m0_fixed_step(&pair->q16_16, input);
}

M0MovementView m0_float_view(const M0FloatMotion *state)
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

M0MovementView m0_fixed_view(const M0FixedMotion *state)
{
    M0MovementView view;
    view.x = (double)state->x / Q_ONE;
    view.y = (double)state->y / Q_ONE;
    view.velocity_x = (double)state->velocity_x / Q_ONE;
    view.velocity_y = (double)state->velocity_y / Q_ONE;
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

uint64_t m0_float_hash(const M0FloatMotion *state)
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

uint64_t m0_fixed_hash(const M0FixedMotion *state)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = hash_bytes(hash, &state->x, sizeof(state->x));
    hash = hash_bytes(hash, &state->y, sizeof(state->y));
    hash = hash_bytes(hash, &state->velocity_x, sizeof(state->velocity_x));
    hash = hash_bytes(hash, &state->velocity_y, sizeof(state->velocity_y));
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
               "the float32 candidate requires a 32-bit float");
_Static_assert(INT32_MAX >= 2147483647,
               "the float32 candidate requires 32-bit integers");
