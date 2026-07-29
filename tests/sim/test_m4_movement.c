#include "pf/m4.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static int expect_status(
    pf_status actual,
    pf_status expected,
    const char *operation)
{
    if (actual != expected)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    pf_sim **out_sim)
{
    pf_sim_config config;

    if (!expect_status(
            pf_sim_default_config(&config, player_count, mode),
            PF_STATUS_OK,
            "default-config"))
    {
        return 0;
    }
    config.max_ticks = UINT64_C(100000);
    return expect_status(
        pf_sim_init(
            storage->state,
            sizeof(storage->state),
            storage->scratch,
            sizeof(storage->scratch),
            content,
            &config,
            out_sim),
        PF_STATUS_OK,
        "init");
}

static void make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint8_t player_count,
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(inputs, 0, sizeof(*inputs) * PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)player_count;
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }
}

static int32_t absolute_i32(int32_t value)
{
    return value < INT32_C(0) ? -value : value;
}

static int step_duel_players(
    pf_sim *sim,
    int16_t player0_x,
    int16_t player0_y,
    uint64_t player0_buttons,
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[0].main_stick_x = player0_x;
    inputs[0].main_stick_y = player0_y;
    inputs[0].buttons = player0_buttons;
    inputs[1].main_stick_x = player1_x;
    inputs[1].main_stick_y = player1_y;
    inputs[1].buttons = player1_buttons;
    if (!expect_status(
            pf_sim_tick(sim, inputs, (size_t)2, &result),
            PF_STATUS_OK,
            "duel-step") ||
        !expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "inspect-after-step"))
    {
        return 0;
    }
    return 1;
}

static int step_duel(
    pf_sim *sim,
    int16_t main_stick_x,
    int16_t main_stick_y,
    uint64_t buttons,
    pf_m4_inspection *out_inspection)
{
    return step_duel_players(
        sim,
        main_stick_x,
        main_stick_y,
        buttons,
        INT16_C(0),
        INT16_C(0),
        UINT64_C(0),
        out_inspection);
}

static int run_content_contract_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage rejected_storage;
    test_sim_storage default_storage;
    test_sim_storage tuned_storage;
    pf_m4_content invalid_content = *default_content;
    pf_m4_content tuned_content = *default_content;
    pf_content_view damaged_view = *default_view;
    pf_content_view tuned_view;
    pf_sim_config config;
    pf_sim *rejected = NULL;
    pf_sim *default_sim = NULL;
    pf_sim *tuned_sim = NULL;
    pf_m4_inspection default_inspection;
    pf_m4_inspection tuned_inspection;
    uint32_t tick;

    invalid_content.fighter.full_hop_speed_q16 =
        invalid_content.fighter.short_hop_speed_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-content"))
    {
        return 0;
    }

    damaged_view.content_hash.bytes[0] ^= UINT8_C(1);
    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "damaged-content-config") ||
        !expect_status(
            pf_sim_init(
                rejected_storage.state,
                sizeof(rejected_storage.state),
                rejected_storage.scratch,
                sizeof(rejected_storage.scratch),
                &damaged_view,
                &config,
                &rejected),
            PF_STATUS_CHECKSUM_MISMATCH,
            "reject-content-checksum"))
    {
        return 0;
    }

    tuned_content.fighter.walk_speed_q16 = PF_Q16_ONE / INT32_C(10);
    if (!expect_status(
            pf_m4_make_content_view(&tuned_content, &tuned_view),
            PF_STATUS_OK,
            "tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=content-hash-identity\n");
        return 0;
    }

    if (!initialize_sim(
            &default_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &default_sim) ||
        !initialize_sim(
            &tuned_storage,
            &tuned_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &tuned_sim) ||
        !expect_status(
            pf_sim_reset(default_sim, UINT64_C(1)),
            PF_STATUS_OK,
            "default-tuning-reset") ||
        !expect_status(
            pf_sim_reset(tuned_sim, UINT64_C(1)),
            PF_STATUS_OK,
            "tuned-tuning-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                default_sim,
                INT16_C(12000),
                INT16_C(0),
                UINT64_C(0),
                &default_inspection) ||
            !step_duel(
                tuned_sim,
                INT16_C(12000),
                INT16_C(0),
                UINT64_C(0),
                &tuned_inspection))
        {
            return 0;
        }
    }
    if (default_inspection.players[0].velocity_x_q16 <=
        tuned_inspection.players[0].velocity_x_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=data-driven-walk-speed\n");
        return 0;
    }
    return 1;
}

static int run_ground_control_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t slow_walk_velocity;
    int32_t dash_velocity;
    int32_t run_velocity;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "ground-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(12000),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    slow_walk_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        slow_walk_velocity <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=analog-walk\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "dash-reset") ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    dash_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(1) ||
        inspection.players[0].facing != INT8_C(1) ||
        dash_velocity <= slow_walk_velocity)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=initial-dash\n");
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(-1) ||
        inspection.players[0].facing != INT8_C(-1) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-dance-reversal\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(11); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        run_velocity >= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-transition\n");
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        inspection.players[0].dash_direction != INT8_C(1) ||
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        absolute_i32(inspection.players[0].velocity_x_q16) >=
            absolute_i32(run_velocity))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-turnaround-entry\n");
        return 0;
    }

    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.run_turnaround_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_INITIAL_DASH)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=run-turnaround-window\n");
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-turnaround-exit\n");
        return 0;
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.run_turnaround_lockout_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RUN)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=run-turnaround-lockout\n");
            return 0;
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
        absolute_i32(inspection.players[0].velocity_x_q16) >=
            absolute_i32(run_velocity))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-entry\n");
        return 0;
    }
    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-turnaround\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "run-brake-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.initial_dash_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_BRAKE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-exit-setup\n");
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.run_brake_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        absolute_i32(inspection.players[0].velocity_x_q16) >=
            absolute_i32(run_velocity))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-exit\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].grounded != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch\n");
        return 0;
    }
    return 1;
}

static int measure_hop(
    const pf_content_view *content,
    uint32_t held_ticks,
    int32_t *out_launch_velocity,
    int32_t *out_apex_y)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int launched = 0;
    int32_t apex_y = INT32_MAX;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(3)),
            PF_STATUS_OK,
            "hop-reset"))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(100); ++tick)
    {
        const uint64_t buttons =
            tick < held_ticks
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                buttons,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            if (!launched)
            {
                *out_launch_velocity =
                    inspection.players[0].velocity_y_q16;
                launched = 1;
            }
            if (inspection.players[0].position_y_q16 < apex_y)
            {
                apex_y = inspection.players[0].position_y_q16;
            }
            if (inspection.players[0].velocity_y_q16 > INT32_C(0))
            {
                *out_apex_y = apex_y;
                return 1;
            }
        }
    }

    (void)fprintf(
        stderr,
        "m4-movement=fail operation=hop-profile held_ticks=%" PRIu32
        "\n",
        held_ticks);
    return 0;
}

static int run_air_control_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t short_launch_early;
    int32_t short_launch_late;
    int32_t full_launch_early;
    int32_t full_launch_late;
    int32_t short_apex_early;
    int32_t short_apex_late;
    int32_t full_apex_early;
    int32_t full_apex_late;
    uint32_t tick;

    if (!measure_hop(
            view,
            UINT32_C(1),
            &short_launch_early,
            &short_apex_early) ||
        !measure_hop(
            view,
            UINT32_C(2),
            &short_launch_late,
            &short_apex_late) ||
        !measure_hop(
            view,
            UINT32_C(3),
            &full_launch_early,
            &full_apex_early) ||
        !measure_hop(
            view,
            UINT32_C(10),
            &full_launch_late,
            &full_apex_late))
    {
        return 0;
    }
    if (short_launch_early != short_launch_late ||
        short_apex_early != short_apex_late ||
        full_launch_early != full_launch_late ||
        full_apex_early != full_apex_late ||
        full_apex_early >= short_apex_early)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=binary-hop-heights"
            " short=(%" PRId32 ",%" PRId32 ")"
            " full=(%" PRId32 ",%" PRId32 ")\n",
            short_apex_early,
            short_apex_late,
            full_apex_early,
            full_apex_late);
        return 0;
    }

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(4)),
            PF_STATUS_OK,
            "air-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-air-drift\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(5)),
            PF_STATUS_OK,
            "fast-fall-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].velocity_y_q16 > INT32_C(0))
        {
            break;
        }
    }
    if (inspection.players[0].velocity_y_q16 <= INT32_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].fast_fall != UINT8_C(1) ||
        inspection.players[0].velocity_y_q16 !=
            content->fighter.fast_fall_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fast-fall\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded != UINT8_C(0))
        {
            break;
        }
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=landing\n");
        return 0;
    }
    return 1;
}

static int run_air_facing_lock_test(const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(14)),
            PF_STATUS_OK,
            "air-facing-reset"))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-facing-takeoff\n");
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-facing-left-drift\n");
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-facing-double-jump\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].grounded != UINT8_C(0) ||
            inspection.players[0].facing != INT8_C(1))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=air-facing-right-drift\n");
            return 0;
        }
    }
    if (inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-drift-reversal\n");
        return 0;
    }
    return 1;
}

static int run_platform_test(const pf_m4_content *default_content)
{
    test_sim_storage storage;
    pf_m4_content platform_content = *default_content;
    pf_content_view platform_view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t previous_player_x;
    int32_t previous_platform_left;
    uint32_t tick;

    platform_content.stage.platform_center_x_q16 =
        -INT32_C(8) * PF_Q16_ONE;
    platform_content.stage.platform_motion_amplitude_q16 =
        INT32_C(2) * PF_Q16_ONE;
    platform_content.stage.platform_half_width_q16 =
        INT32_C(6) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(
                &platform_content,
                &platform_view),
            PF_STATUS_OK,
            "platform-content-view") ||
        !initialize_sim(
            &storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(6)),
            PF_STATUS_OK,
            "platform-reset") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "platform-initial-inspect") ||
        inspection.stage.left_ledge_x_q16 !=
            platform_content.stage.floor_left_q16 ||
        inspection.stage.right_ledge_x_q16 !=
            platform_content.stage.floor_right_q16)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            break;
        }
    }
    if (inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-landing\n");
        return 0;
    }

    while (inspection.players[0].action_state ==
           (uint8_t)PF_M4_ACTION_LANDING)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    previous_player_x = inspection.players[0].position_x_q16;
    previous_platform_left = inspection.stage.platform_left_q16;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].position_x_q16 - previous_player_x !=
            inspection.stage.platform_left_q16 -
                previous_platform_left)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-motion-carry\n");
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        inspection.players[0].platform_drop_ticks == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-drop\n");
        return 0;
    }
    return 1;
}

static int make_solid_geometry_content(
    const pf_m4_content *default_content,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    *out_content = *default_content;
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 = INT32_C(0);
    out_content->stage.solid_right_q16 =
        INT32_C(8) * PF_Q16_ONE;
    out_content->stage.solid_top_q16 =
        INT32_C(25) * PF_Q16_ONE;
    out_content->stage.solid_bottom_q16 =
        INT32_C(28) * PF_Q16_ONE;
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "solid-geometry-content-view");
}

static int run_solid_geometry_test(
    const pf_m4_content *default_content)
{
    test_sim_storage under_storage;
    test_sim_storage wall_storage;
    test_sim_storage ceiling_storage;
    test_sim_storage top_storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim *under = NULL;
    pf_sim *wall = NULL;
    pf_sim *ceiling = NULL;
    pf_sim *top = NULL;
    pf_m4_inspection inspection;
    const int32_t wall_contact_x =
        -default_content->fighter.half_width_q16;
    const int32_t ceiling_contact_y =
        INT32_C(28) * PF_Q16_ONE +
        default_content->fighter.half_height_q16;
    const int32_t top_contact_y =
        INT32_C(25) * PF_Q16_ONE -
        default_content->fighter.half_height_q16;
    int observed_wall = 0;
    int observed_ceiling = 0;
    uint32_t tick;

    if (!make_solid_geometry_content(
            default_content,
            &content,
            &view) ||
        !initialize_sim(
            &under_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &under) ||
        !initialize_sim(
            &wall_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &wall) ||
        !initialize_sim(
            &ceiling_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &ceiling) ||
        !initialize_sim(
            &top_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &top) ||
        !expect_status(
            pf_sim_reset(under, UINT64_C(15)),
            PF_STATUS_OK,
            "solid-under-reset") ||
        !expect_status(
            pf_sim_reset(wall, UINT64_C(16)),
            PF_STATUS_OK,
            "solid-wall-reset") ||
        !expect_status(
            pf_sim_reset(ceiling, UINT64_C(17)),
            PF_STATUS_OK,
            "solid-ceiling-reset") ||
        !expect_status(
            pf_sim_reset(top, UINT64_C(18)),
            PF_STATUS_OK,
            "solid-top-reset") ||
        !expect_status(
            pf_m4_inspect(under, &inspection),
            PF_STATUS_OK,
            "solid-geometry-inspect") ||
        inspection.stage.solid_left_q16 !=
            content.stage.solid_left_q16 ||
        inspection.stage.solid_right_q16 !=
            content.stage.solid_right_q16 ||
        inspection.stage.solid_top_q16 !=
            content.stage.solid_top_q16 ||
        inspection.stage.solid_bottom_q16 !=
            content.stage.solid_bottom_q16)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        if (!step_duel_players(
                under,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].position_x_q16 <=
            content.stage.solid_right_q16 +
                content.fighter.half_width_q16 ||
        inspection.players[1].grounded == UINT8_C(0) ||
        inspection.players[1].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-walk-under"
            " position=(%" PRId32 ",%" PRId32 ")"
            " grounded=%u support=%u\n",
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            (unsigned int)inspection.players[1].grounded,
            (unsigned int)inspection.players[1].support);
        return 0;
    }

    if (!step_duel(
            wall,
            INT16_C(13500),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                wall,
                INT16_C(13500),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        if (!step_duel(
                wall,
                INT16_C(13500),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].position_x_q16 == wall_contact_x &&
            inspection.players[0].velocity_x_q16 == INT32_C(0))
        {
            observed_wall = 1;
            break;
        }
    }
    if (observed_wall == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-side-contact"
            " position=(%" PRId32 ",%" PRId32 ")"
            " velocity=(%" PRId32 ",%" PRId32 ")\n",
            inspection.players[0].position_x_q16,
            inspection.players[0].position_y_q16,
            inspection.players[0].velocity_x_q16,
            inspection.players[0].velocity_y_q16);
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel_players(
                ceiling,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        if (!step_duel_players(
                ceiling,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].position_y_q16 ==
                ceiling_contact_y &&
            inspection.players[1].velocity_y_q16 ==
                INT32_C(0))
        {
            observed_ceiling = 1;
            break;
        }
    }
    if (observed_ceiling == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-ceiling-contact"
            " position=(%" PRId32 ",%" PRId32 ")"
            " velocity=(%" PRId32 ",%" PRId32 ")\n",
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            inspection.players[1].velocity_x_q16,
            inspection.players[1].velocity_y_q16);
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                top,
                INT16_MAX,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        if (!step_duel(
                top,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_SOLID_TOP)
        {
            break;
        }
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_SOLID_TOP ||
        inspection.players[0].position_y_q16 != top_contact_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-top-landing"
            " position=(%" PRId32 ",%" PRId32 ")"
            " grounded=%u support=%u\n",
            inspection.players[0].position_x_q16,
            inspection.players[0].position_y_q16,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support);
        return 0;
    }
    return 1;
}

static int drive_player0_to_right_ledge(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded == UINT8_C(0) &&
            out_inspection->players[0].position_x_q16 >
                out_inspection->stage.right_ledge_x_q16)
        {
            return 1;
        }
    }

    (void)fprintf(
        stderr,
        "m4-movement=fail operation=reach-right-ledge\n");
    return 0;
}

static int grab_player0_right_ledge(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_RUN &&
            out_inspection->players[0].position_x_q16 >=
                out_inspection->stage.right_ledge_x_q16 -
                    PF_Q16_ONE / INT32_C(2))
        {
            break;
        }
    }
    if (tick == UINT32_C(240))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=right-ledge-turn-setup\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            break;
        }
        if (out_inspection->players[0].grounded == UINT8_C(0) &&
            out_inspection->players[0].facing != INT8_C(-1))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=right-ledge-inward-facing\n");
            return 0;
        }
    }
    if (tick == UINT32_C(32) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        out_inspection->players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=right-ledge-grab"
            " action=%u ledge=%u\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].ledge);
        return 0;
    }
    return 1;
}

static int make_player0_ledge_actionable(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    const uint32_t catch_ticks =
        (uint32_t)content->fighter.landing_ticks +
        (uint32_t)content->fighter.jump_squat_ticks;
    uint32_t tick;

    for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        out_inspection->players[0].action_ticks !=
            (uint16_t)catch_ticks)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-catch-window\n");
        return 0;
    }
    return 1;
}

static int run_ledge_snapshot_test(
    pf_sim *source,
    const pf_content_view *content,
    pf_m4_inspection *source_inspection)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t required_bytes = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &loaded_storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_query_save_size(source, &required_bytes),
            PF_STATUS_OK,
            "ledge-query-save-size") ||
        required_bytes > sizeof(save_bytes))
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "ledge-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "ledge-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "ledge-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "ledge-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0 ||
        !expect_status(
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "ledge-loaded-inspect") ||
        loaded_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
        loaded_inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-snapshot-round-trip\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "ledge-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "ledge-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-snapshot-future\n");
            return 0;
        }
    }
    return 1;
}

static int run_ledge_occupancy_test(
    const pf_m4_content *default_content)
{
    test_sim_storage storage;
    pf_m4_content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    content.stage.spawn_spacing_q16 = INT32_C(1);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "ledge-occupancy-content") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(12)),
            PF_STATUS_OK,
            "ledge-occupancy-reset"))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (!step_duel_players(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[1].grounded != UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_RUN &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_RUN &&
            inspection.players[0].position_x_q16 >=
                inspection.stage.right_ledge_x_q16 -
                    PF_Q16_ONE / INT32_C(2) &&
            inspection.players[1].position_x_q16 >=
                inspection.stage.right_ledge_x_q16 -
                    PF_Q16_ONE / INT32_C(2))
        {
            break;
        }
    }
    if (tick == UINT32_C(240))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-occupancy-turn-setup\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_duel_players(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            break;
        }
    }
    if (tick == UINT32_C(32) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        inspection.players[1].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-single-occupancy"
            " p0=(%u,%u) p1=(%u,%u)\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].ledge,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].ledge);
        return 0;
    }
    return 1;
}

static int run_ledge_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t hang_x;
    int32_t hang_y;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(8)),
            PF_STATUS_OK,
            "ledge-facing-reset") ||
        !drive_player0_to_right_ledge(sim, &inspection))
    {
        return 0;
    }
    if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=outward-ledge-rejection\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(9)),
            PF_STATUS_OK,
            "ledge-hang-reset") ||
        !grab_player0_right_ledge(sim, &inspection))
    {
        return 0;
    }
    hang_x = inspection.players[0].position_x_q16;
    hang_y = inspection.players[0].position_y_q16;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].position_x_q16 != hang_x ||
        inspection.players[0].position_y_q16 != hang_y ||
        inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        inspection.players[0].velocity_y_q16 != INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-hang-pin\n");
        return 0;
    }
    if (!make_player0_ledge_actionable(sim, content, &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE ||
        inspection.players[0].position_y_q16 <= hang_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-release\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(10)),
            PF_STATUS_OK,
            "ledge-jump-reset") ||
        !grab_player0_right_ledge(sim, &inspection) ||
        !make_player0_ledge_actionable(sim, content, &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-jump\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(11)),
            PF_STATUS_OK,
            "ledge-climb-reset") ||
        !grab_player0_right_ledge(sim, &inspection) ||
        !make_player0_ledge_actionable(sim, content, &inspection) ||
        !step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !run_ledge_snapshot_test(sim, view, &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-climb-snapshot-setup\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (inspection.players[0].grounded != UINT8_C(0))
        {
            break;
        }
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].position_x_q16 >=
            inspection.stage.right_ledge_x_q16 ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE ||
        !run_ledge_occupancy_test(content))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-climb-or-occupancy\n");
        return 0;
    }
    return 1;
}

static int run_blast_zone_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(7)),
            PF_STATUS_OK,
            "blast-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(600); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].respawn_count != UINT16_C(0))
        {
            return 1;
        }
    }

    (void)fprintf(
        stderr,
        "m4-movement=fail operation=blast-zone-respawn\n");
    return 0;
}

static int run_team_hash_trace(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_state_hash hash;
    uint64_t tick;

    if (!initialize_sim(
            &storage,
            content,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(31003)),
            PF_STATUS_OK,
            "team-reset"))
    {
        return 0;
    }

    for (tick = UINT64_C(0); tick < UINT64_C(20000); ++tick)
    {
        uint32_t player_index;

        make_inputs(inputs, UINT8_C(4), tick);
        for (player_index = UINT32_C(0);
             player_index < UINT32_C(4);
             ++player_index)
        {
            const int32_t magnitude =
                (player_index & UINT32_C(1)) == UINT32_C(0)
                    ? INT32_C(24576)
                    : INT32_C(-24576);
            inputs[player_index].main_stick_x =
                (tick % (UINT64_C(7) + (uint64_t)player_index)) <
                        UINT64_C(5)
                    ? (int16_t)magnitude
                    : INT16_C(0);
            inputs[player_index].main_stick_y =
                (int16_t)(
                    (int32_t)(tick % UINT64_C(17)) * INT32_C(2048) -
                    INT32_C(16384));
            if ((tick + (uint64_t)player_index * UINT64_C(3)) %
                    UINT64_C(41) ==
                UINT64_C(4))
            {
                inputs[player_index].buttons =
                    PF_INPUT_BUTTON_JUMP;
            }
        }

        if (!expect_status(
                pf_sim_tick(sim, inputs, (size_t)4, &result),
                PF_STATUS_OK,
                "team-tick"))
        {
            return 0;
        }
        if (pf_sim_hash(sim, &hash) != PF_STATUS_OK)
        {
            pf_m4_inspection inspection;

            (void)pf_m4_inspect(sim, &inspection);
            for (player_index = UINT32_C(0);
                 player_index < UINT32_C(4);
                 ++player_index)
            {
                const pf_m4_player_inspection *player =
                    &inspection.players[player_index];
                (void)fprintf(
                    stderr,
                    "m4-movement=trace tick=%" PRIu64
                    " player=%" PRIu32
                    " action=%u grounded=%u support=%u"
                    " position=(%" PRId32 ",%" PRId32 ")"
                    " velocity=(%" PRId32 ",%" PRId32 ")"
                    " action_ticks=%u facing=%d dash=%d previous=%d"
                    " fast=%u short=%u drop=%u jumps=%u respawns=%u\n",
                    inspection.tick,
                    player_index,
                    (unsigned int)player->action_state,
                    (unsigned int)player->grounded,
                    (unsigned int)player->support,
                    player->position_x_q16,
                    player->position_y_q16,
                    player->velocity_x_q16,
                    player->velocity_y_q16,
                    (unsigned int)player->action_ticks,
                    (int)player->facing,
                    (int)player->dash_direction,
                    (int)player->previous_strong_direction,
                    (unsigned int)player->fast_fall,
                    (unsigned int)player->short_hop_latched,
                    (unsigned int)player->platform_drop_ticks,
                    (unsigned int)player->air_jumps_remaining,
                    (unsigned int)player->respawn_count);
            }
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=team-hash tick=%" PRIu64
                "\n",
                tick);
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    pf_m4_content content;
    pf_content_view view;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "default-content") ||
        !expect_status(
            pf_m4_validate_content(&content),
            PF_STATUS_OK,
            "validate-content") ||
        !expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "content-view") ||
        !run_content_contract_test(&content, &view) ||
        !run_ground_control_test(&content, &view) ||
        !run_air_control_test(&content, &view) ||
        !run_air_facing_lock_test(&view) ||
        !run_platform_test(&content) ||
        !run_solid_geometry_test(&content) ||
        !run_ledge_test(&content, &view) ||
        !run_blast_zone_test(&view) ||
        !run_team_hash_trace(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-movement=pass content_schema=%u deterministic_ticks=20000 "
        "movement_invariants=35\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION);
    return 0;
}
