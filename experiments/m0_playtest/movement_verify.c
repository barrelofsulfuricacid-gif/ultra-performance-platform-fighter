#include "movement_model.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    TRACE_TICKS = 7200,
    RESTORE_TICKS = 900
};

static void require(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "self-test=fail reason=%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static M0MovementInput trace_input(uint32_t tick)
{
    M0MovementInput input = {0};
    uint32_t phase = tick % 480U;
    uint32_t jump_phase = tick % 173U;

    if (phase < 80U)
    {
        input.move_x = M0_AXIS_MAX;
    }
    else if (phase < 145U)
    {
        input.move_x = M0_AXIS_MIN;
    }
    else if (phase < 220U)
    {
        input.move_x = 13500;
    }
    else if (phase < 300U)
    {
        input.move_x = -22500;
    }
    else if (phase < 350U)
    {
        input.move_x = M0_AXIS_MAX;
    }

    input.jump_pressed =
        (uint8_t)(jump_phase == 12U || jump_phase == 94U);
    input.jump_held =
        (uint8_t)((jump_phase >= 12U && jump_phase < 19U) ||
                  (jump_phase >= 94U && jump_phase < 112U));
    input.down_held =
        (uint8_t)((phase >= 226U && phase < 245U) ||
                  (phase >= 405U && phase < 420U));
    return input;
}

static void test_axis_clamp(void)
{
    require(m0_axis_clamp(-40000) == M0_AXIS_MIN,
            "negative axis clamp");
    require(m0_axis_clamp(40000) == M0_AXIS_MAX,
            "positive axis clamp");
    require(m0_axis_clamp(1234) == 1234, "axis passthrough");
}

static void test_reset(void)
{
    M0MovementPair pair;
    M0MovementView primary_view;
    M0MovementView repeat_view;

    memset(&pair, 0xA5, sizeof(pair));
    m0_pair_reset(&pair);
    primary_view = m0_view(&pair.primary);
    repeat_view = m0_view(&pair.repeat);

    require(primary_view.x == repeat_view.x, "reset x equality");
    require(primary_view.y == repeat_view.y, "reset y equality");
    require(primary_view.grounded && repeat_view.grounded,
            "reset grounded");
    require(primary_view.air_jumps == 1U && repeat_view.air_jumps == 1U,
            "reset air jump");
}

static void test_walk_dash_and_pivot(void)
{
    M0MovementPair pair;
    M0MovementInput input = {0};
    M0MovementView primary_view;
    M0MovementView repeat_view;

    m0_pair_reset(&pair);
    input.move_x = 13500;
    m0_pair_step(&pair, input);
    primary_view = m0_view(&pair.primary);
    repeat_view = m0_view(&pair.repeat);
    require(primary_view.dash_ticks == 0U && repeat_view.dash_ticks == 0U,
            "walk-strength input started a dash");
    require(fabsf(primary_view.velocity_x) < 0.1f &&
                fabsf(repeat_view.velocity_x) < 0.1f,
            "walk-strength input was not slow");

    m0_pair_reset(&pair);
    input.move_x = M0_AXIS_MAX;
    m0_pair_step(&pair, input);
    primary_view = m0_view(&pair.primary);
    repeat_view = m0_view(&pair.repeat);
    require(primary_view.dash_ticks > 0U && repeat_view.dash_ticks > 0U,
            "full input did not start a dash");
    require(primary_view.velocity_x > 0.19f &&
                repeat_view.velocity_x > 0.19f,
            "initial dash was not immediate");

    input.move_x = M0_AXIS_MIN;
    m0_pair_step(&pair, input);
    primary_view = m0_view(&pair.primary);
    repeat_view = m0_view(&pair.repeat);
    require(primary_view.dash_ticks > 0U && repeat_view.dash_ticks > 0U,
            "pivot ended the dash window");
    require(primary_view.velocity_x < -0.19f &&
                repeat_view.velocity_x < -0.19f,
            "dash pivot did not reverse immediately");
}

static void jump_apex(uint32_t release_tick, float *primary_apex,
                      float *repeat_apex)
{
    M0MovementPair pair;
    uint32_t tick;

    *primary_apex = FLT_MAX;
    *repeat_apex = FLT_MAX;
    m0_pair_reset(&pair);
    for (tick = 0U; tick < 180U; ++tick)
    {
        M0MovementInput input = {0};
        M0MovementView primary_view;
        M0MovementView repeat_view;

        input.jump_pressed = (uint8_t)(tick == 0U);
        input.jump_held = (uint8_t)(tick < release_tick);
        m0_pair_step(&pair, input);
        primary_view = m0_view(&pair.primary);
        repeat_view = m0_view(&pair.repeat);
        if (primary_view.y < *primary_apex)
        {
            *primary_apex = primary_view.y;
        }
        if (repeat_view.y < *repeat_apex)
        {
            *repeat_apex = repeat_view.y;
        }
    }
}

static void test_binary_jump_heights(void)
{
    float short_early_primary;
    float short_early_repeat;
    float short_late_primary;
    float short_late_repeat;
    float full_early_primary;
    float full_early_repeat;
    float full_late_primary;
    float full_late_repeat;

    jump_apex(0U, &short_early_primary, &short_early_repeat);
    jump_apex(2U, &short_late_primary, &short_late_repeat);
    jump_apex(3U, &full_early_primary, &full_early_repeat);
    jump_apex(20U, &full_late_primary, &full_late_repeat);

    require(short_early_primary == short_late_primary &&
                short_early_repeat == short_late_repeat,
            "short-hop height changed inside jumpsquat");
    require(full_early_primary == full_late_primary &&
                full_early_repeat == full_late_repeat,
            "full-hop height changed after takeoff");
    require(full_early_primary < short_early_primary &&
                full_early_repeat < short_early_repeat,
            "full hop was not higher than short hop");
}

static void test_float32_replay(void)
{
    M0Motion first;
    M0Motion second;
    uint32_t tick;

    m0_reset(&first);
    m0_reset(&second);
    for (tick = 0; tick < TRACE_TICKS; ++tick)
    {
        M0MovementInput input = trace_input(tick);
        m0_step(&first, input);
        m0_step(&second, input);
        require(memcmp(&first, &second, sizeof(first)) == 0,
                "float32 replay diverged");
    }
}

static uint64_t test_float32_save_restore(void)
{
    M0Motion state;
    M0Motion snapshot;
    uint64_t expected_hash;
    uint32_t tick;

    m0_reset(&state);
    for (tick = 0; tick < 1000U; ++tick)
    {
        m0_step(&state, trace_input(tick));
    }
    snapshot = state;
    for (; tick < 1000U + RESTORE_TICKS; ++tick)
    {
        m0_step(&state, trace_input(tick));
    }
    expected_hash = m0_hash(&state);

    state = snapshot;
    for (tick = 1000U; tick < 1000U + RESTORE_TICKS; ++tick)
    {
        m0_step(&state, trace_input(tick));
    }
    require(m0_hash(&state) == expected_hash,
            "float32 save/restore replay diverged");
    return expected_hash;
}

static float test_repeat_lane(void)
{
    M0MovementPair pair;
    float max_delta = 0.0f;
    uint32_t tick;

    m0_pair_reset(&pair);
    for (tick = 0; tick < TRACE_TICKS; ++tick)
    {
        M0MovementView primary_view;
        M0MovementView repeat_view;
        float delta_x;
        float delta_y;
        M0MovementInput input = trace_input(tick);

        m0_pair_step(&pair, input);
        primary_view = m0_view(&pair.primary);
        repeat_view = m0_view(&pair.repeat);
        delta_x = fabsf(primary_view.x - repeat_view.x);
        delta_y = fabsf(primary_view.y - repeat_view.y);
        if (delta_x > max_delta)
        {
            max_delta = delta_x;
        }
        if (delta_y > max_delta)
        {
            max_delta = delta_y;
        }

        require(memcmp(&pair.primary, &pair.repeat,
                       sizeof(pair.primary)) == 0,
                "float32 repeat lane diverged");
    }
    return max_delta;
}

int main(void)
{
    uint64_t restore_hash;
    float max_delta;

    test_axis_clamp();
    test_reset();
    test_walk_dash_and_pivot();
    test_binary_jump_heights();
    test_float32_replay();
    restore_hash = test_float32_save_restore();
    max_delta = test_repeat_lane();

    printf("self-test=pass cases=7 trace_ticks=%d "
           "max_position_delta=%.9f float32_restore_hash=%" PRIu64 "\n",
           TRACE_TICKS, (double)max_delta, restore_hash);
    return EXIT_SUCCESS;
}
