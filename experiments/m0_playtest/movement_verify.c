#include "movement_model.h"

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
    M0MovementView float_view;
    M0MovementView fixed_view;

    memset(&pair, 0xA5, sizeof(pair));
    m0_pair_reset(&pair);
    float_view = m0_float_view(&pair.float32);
    fixed_view = m0_fixed_view(&pair.q16_16);

    require(fabs(float_view.x - fixed_view.x) < 0.00002,
            "reset x equivalence");
    require(fabs(float_view.y - fixed_view.y) < 0.00002,
            "reset y equivalence");
    require(float_view.grounded && fixed_view.grounded,
            "reset grounded");
    require(float_view.air_jumps == 1U && fixed_view.air_jumps == 1U,
            "reset air jump");
}

static void test_fixed_replay(void)
{
    M0FixedMotion first;
    M0FixedMotion second;
    uint32_t tick;

    m0_fixed_reset(&first);
    m0_fixed_reset(&second);
    for (tick = 0; tick < TRACE_TICKS; ++tick)
    {
        M0MovementInput input = trace_input(tick);
        m0_fixed_step(&first, input);
        m0_fixed_step(&second, input);
        require(memcmp(&first, &second, sizeof(first)) == 0,
                "fixed replay diverged");
    }
}

static uint64_t test_fixed_save_restore(void)
{
    M0FixedMotion state;
    M0FixedMotion snapshot;
    uint64_t expected_hash;
    uint32_t tick;

    m0_fixed_reset(&state);
    for (tick = 0; tick < 1000U; ++tick)
    {
        m0_fixed_step(&state, trace_input(tick));
    }
    snapshot = state;
    for (; tick < 1000U + RESTORE_TICKS; ++tick)
    {
        m0_fixed_step(&state, trace_input(tick));
    }
    expected_hash = m0_fixed_hash(&state);

    state = snapshot;
    for (tick = 1000U; tick < 1000U + RESTORE_TICKS; ++tick)
    {
        m0_fixed_step(&state, trace_input(tick));
    }
    require(m0_fixed_hash(&state) == expected_hash,
            "fixed save/restore replay diverged");
    return expected_hash;
}

static double test_candidate_comparability(void)
{
    M0MovementPair pair;
    double max_delta = 0.0;
    uint32_t tick;

    m0_pair_reset(&pair);
    for (tick = 0; tick < TRACE_TICKS; ++tick)
    {
        M0MovementView float_view;
        M0MovementView fixed_view;
        double delta_x;
        double delta_y;
        M0MovementInput input = trace_input(tick);

        m0_pair_step(&pair, input);
        float_view = m0_float_view(&pair.float32);
        fixed_view = m0_fixed_view(&pair.q16_16);
        delta_x = fabs(float_view.x - fixed_view.x);
        delta_y = fabs(float_view.y - fixed_view.y);
        if (delta_x > max_delta)
        {
            max_delta = delta_x;
        }
        if (delta_y > max_delta)
        {
            max_delta = delta_y;
        }

        require(float_view.tick == fixed_view.tick,
                "candidate clocks diverged");
        require(float_view.respawns == fixed_view.respawns,
                "candidate respawn count diverged");
        require(delta_x < 0.08 && delta_y < 0.08,
                "candidate trace exceeds playtest envelope");
    }
    return max_delta;
}

int main(void)
{
    uint64_t restore_hash;
    double max_delta;

    test_axis_clamp();
    test_reset();
    test_fixed_replay();
    restore_hash = test_fixed_save_restore();
    max_delta = test_candidate_comparability();

    printf("self-test=pass cases=5 trace_ticks=%d "
           "max_position_delta=%.9f fixed_restore_hash=%" PRIu64 "\n",
           TRACE_TICKS, max_delta, restore_hash);
    return EXIT_SUCCESS;
}
