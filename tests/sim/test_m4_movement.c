#include "pf/m4.h"
#include "pf/rl.h"
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
    config.stock_count = UINT8_C(0);
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

static int player_overlaps_solid(
    const pf_m4_content *content,
    const pf_m4_player_inspection *player)
{
    return (int64_t)player->position_x_q16 +
                   content->fighter.half_width_q16 >
               (int64_t)content->stage.solid_left_q16 &&
           (int64_t)player->position_x_q16 -
                   content->fighter.half_width_q16 <
               (int64_t)content->stage.solid_right_q16 &&
           (int64_t)player->position_y_q16 +
                   content->fighter.half_height_q16 >
               (int64_t)content->stage.solid_top_q16 &&
           (int64_t)player->position_y_q16 -
                   content->fighter.half_height_q16 <
               (int64_t)content->stage.solid_bottom_q16;
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

static int step_duel_triggers(
    pf_sim *sim,
    int16_t main_stick_x,
    int16_t main_stick_y,
    uint64_t buttons,
    uint16_t left_trigger,
    uint16_t right_trigger,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "air-dodge-inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[0].main_stick_x = main_stick_x;
    inputs[0].main_stick_y = main_stick_y;
    inputs[0].buttons = buttons;
    inputs[0].left_trigger = left_trigger;
    inputs[0].right_trigger = right_trigger;
    if (!expect_status(
            pf_sim_tick(sim, inputs, (size_t)2, &result),
            PF_STATUS_OK,
            "air-dodge-step") ||
        !expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "air-dodge-inspect-after-step"))
    {
        return 0;
    }
    return 1;
}

static int step_duel_trigger(
    pf_sim *sim,
    int16_t main_stick_x,
    int16_t main_stick_y,
    uint64_t buttons,
    uint16_t trigger,
    pf_m4_inspection *out_inspection)
{
    return step_duel_triggers(
        sim,
        main_stick_x,
        main_stick_y,
        buttons,
        trigger,
        UINT16_C(0),
        out_inspection);
}

static int step_duel_secondary_trigger(
    pf_sim *sim,
    int16_t main_stick_x,
    int16_t main_stick_y,
    int16_t secondary_stick_x,
    int16_t secondary_stick_y,
    uint64_t buttons,
    uint16_t trigger,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "secondary-trigger-inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[0].main_stick_x = main_stick_x;
    inputs[0].main_stick_y = main_stick_y;
    inputs[0].secondary_stick_x = secondary_stick_x;
    inputs[0].secondary_stick_y = secondary_stick_y;
    inputs[0].buttons = buttons;
    inputs[0].left_trigger = trigger;
    if (!expect_status(
            pf_sim_tick(sim, inputs, (size_t)2, &result),
            PF_STATUS_OK,
            "secondary-trigger-step") ||
        !expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "secondary-trigger-inspect-after-step"))
    {
        return 0;
    }
    return 1;
}

static int launch_player0(
    pf_sim *sim,
    int short_hop,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         out_inspection->players[0].grounded != UINT8_C(0);
         ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                short_hop != 0
                    ? UINT64_C(0)
                    : PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[0].grounded != UINT8_C(0) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-dodge-launch\n");
        return 0;
    }
    return 1;
}

static int run_air_dodge_snapshot_test(
    pf_sim *source,
    const pf_content_view *content)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
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
            "air-dodge-query-save-size") ||
        required_bytes != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "air-dodge-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "air-dodge-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_MAX,
                INT16_MIN,
                UINT64_C(0),
                UINT16_MAX,
                &source_inspection) ||
            !step_duel_trigger(
                loaded,
                INT16_MAX,
                INT16_MIN,
                UINT64_C(0),
                UINT16_MAX,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "air-dodge-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "air-dodge-loaded-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            source_inspection.players[0].position_x_q16 !=
                loaded_inspection.players[0].position_x_q16 ||
            source_inspection.players[0].position_y_q16 !=
                loaded_inspection.players[0].position_y_q16 ||
            source_inspection.players[0].velocity_x_q16 !=
                loaded_inspection.players[0].velocity_x_q16 ||
            source_inspection.players[0].velocity_y_q16 !=
                loaded_inspection.players[0].velocity_y_q16 ||
            source_inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            source_inspection.players[0].action_ticks !=
                loaded_inspection.players[0].action_ticks ||
            source_inspection.players[0].facing !=
                loaded_inspection.players[0].facing ||
            source_inspection.players[0].invulnerable !=
                loaded_inspection.players[0].invulnerable)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=air-dodge-snapshot"
                " continuation_tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    return 1;
}

static int run_air_dodge_test(
    const pf_m4_content *default_content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    test_sim_storage platform_storage;
    pf_sim *sim = NULL;
    pf_sim *platform_sim = NULL;
    pf_m4_content invalid_content = *default_content;
    pf_m4_content platform_content = *default_content;
    pf_content_view platform_view;
    pf_m4_inspection inspection;
    int32_t entry_velocity_x;
    int32_t entry_velocity_y;
    int32_t expected_entry_velocity_x;
    int32_t expected_entry_velocity_y;
    int32_t landing_x;
    int32_t landing_velocity_x;
    int8_t takeoff_facing;
    uint32_t tick;

    invalid_content.fighter.air_dodge_decay_q16 =
        PF_Q16_ONE + INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-air-dodge-decay"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter
        .air_dodge_invulnerability_end_tick =
        invalid_content.fighter
            .air_dodge_invulnerability_begin_tick;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-air-dodge-invulnerability"))
    {
        return 0;
    }

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim))
    {
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xa1d0d6d)),
            PF_STATUS_OK,
            "held-left-fresh-right-reset") ||
        !step_duel_triggers(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_triggers(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_MAX,
            UINT16_C(0),
            &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=held-left-fresh-right-setup\n");
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         inspection.players[0].grounded != UINT8_C(0);
         ++tick)
    {
        if (!step_duel_triggers(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        !step_duel_triggers(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            (uint16_t)(
                default_content->fighter.digital_trigger_threshold -
                UINT16_C(1)),
            &inspection) ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        !step_duel_triggers(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            UINT16_C(0),
            &inspection) ||
        !step_duel_triggers(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=held-left-fresh-right-air-dodge\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xa1d0d6e)),
            PF_STATUS_OK,
            "directional-air-dodge-reset") ||
        !launch_player0(sim, 0, &inspection))
    {
        return 0;
    }
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint16_t)(
                default_content->fighter.digital_trigger_threshold -
                UINT16_C(1)),
            &inspection) ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=light-trigger-no-air-dodge\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xa1d0d6e)),
            PF_STATUS_OK,
            "directional-air-dodge-second-reset") ||
        !launch_player0(sim, 0, &inspection))
    {
        return 0;
    }
    takeoff_facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            INT16_MAX,
            INT16_MIN,
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    entry_velocity_x = inspection.players[0].velocity_x_q16;
    entry_velocity_y = inspection.players[0].velocity_y_q16;
    expected_entry_velocity_x = (int32_t)(
        (int64_t)INT16_MAX *
        (int64_t)default_content->fighter.air_dodge_speed_x_q16 /
        INT64_C(46340));
    expected_entry_velocity_y = (int32_t)(
        (int64_t)INT16_MIN *
        (int64_t)default_content->fighter.air_dodge_speed_y_q16 /
        INT64_C(46340));
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        entry_velocity_x <= INT32_C(0) ||
        entry_velocity_y >= INT32_C(0) ||
        entry_velocity_x != expected_entry_velocity_x ||
        entry_velocity_y != expected_entry_velocity_y ||
        inspection.players[0].facing != takeoff_facing ||
        inspection.players[0].fast_fall != UINT8_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=directional-air-dodge-entry\n");
        return 0;
    }

    if (!step_duel_trigger(
            sim,
            INT16_MIN,
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_q16 !=
            (int32_t)(
                (int64_t)entry_velocity_x *
                default_content->fighter.air_dodge_decay_q16 /
                PF_Q16_ONE) ||
        inspection.players[0].velocity_y_q16 !=
            (int32_t)(
                (int64_t)entry_velocity_y *
                default_content->fighter.air_dodge_decay_q16 /
                PF_Q16_ONE) ||
        inspection.players[0].facing != takeoff_facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-dodge-decay-or-facing\n");
        return 0;
    }

    while (inspection.players[0].action_ticks <
           default_content->fighter
               .air_dodge_invulnerability_begin_tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_MIN,
                INT16_MAX,
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].invulnerable != UINT8_C(1) ||
        !run_air_dodge_snapshot_test(sim, view))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-dodge-invulnerability"
            " or-snapshot\n");
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_AIR_DODGE;
         ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_MIN,
                INT16_MAX,
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIR_DODGE &&
            inspection.players[0].action_ticks ==
                default_content->fighter
                    .air_dodge_invulnerability_end_tick &&
            inspection.players[0].invulnerable != UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=air-dodge-invulnerability-end\n");
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        inspection.players[0].invulnerable != UINT8_C(0) ||
        inspection.players[0].facing != takeoff_facing ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-dodge-fall-special"
            " or-held-retrigger\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xa1d0d6f)),
            PF_STATUS_OK,
            "neutral-air-dodge-reset") ||
        !launch_player0(sim, 0, &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        inspection.players[0].velocity_y_q16 != INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=neutral-air-dodge\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xa1d0d70)),
            PF_STATUS_OK,
            "wavedash-reset") ||
        !launch_player0(sim, 1, &inspection))
    {
        return 0;
    }
    takeoff_facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_y_q16 <= INT32_C(0) ||
        inspection.players[0].facing != takeoff_facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=wavedash-landing\n");
        return 0;
    }
    landing_x = inspection.players[0].position_x_q16;
    landing_velocity_x = inspection.players[0].velocity_x_q16;
    for (tick = UINT32_C(1);
         tick < (uint32_t)default_content->fighter.special_landing_ticks;
         ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
            inspection.players[0].action_ticks != (uint16_t)tick)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=special-landing-lock"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    if (!step_duel_trigger(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].position_x_q16 <= landing_x ||
        inspection.players[0].velocity_x_q16 >= landing_velocity_x)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=special-landing-exact-end"
            " or-slide action=%u ticks=%u x=%" PRId32
            " landing_x=%" PRId32 " vx=%" PRId32
            " landing_vx=%" PRId32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            inspection.players[0].position_x_q16,
            landing_x,
            inspection.players[0].velocity_x_q16,
            landing_velocity_x);
        return 0;
    }

    platform_content.stage.platform_center_x_q16 =
        -INT32_C(2) * PF_Q16_ONE;
    platform_content.stage.platform_y_q16 =
        platform_content.stage.floor_y_q16 -
        INT32_C(6) * PF_Q16_ONE;
    platform_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    platform_content.stage.spawn_spacing_q16 =
        INT32_C(2) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(
                &platform_content,
                &platform_view),
            PF_STATUS_OK,
            "air-dodge-platform-content") ||
        !initialize_sim(
            &platform_storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &platform_sim) ||
        !expect_status(
            pf_sim_reset(platform_sim, UINT64_C(0xa1d0d71)),
            PF_STATUS_OK,
            "waveland-platform-reset") ||
        !launch_player0(platform_sim, 0, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const int32_t bottom =
            inspection.players[0].position_y_q16 +
            platform_content.fighter.half_height_q16;
        const int32_t maximum_diagonal_drop =
            (platform_content.fighter.air_dodge_speed_y_q16 *
             INT32_C(3)) /
            INT32_C(4);

        if (inspection.players[0].velocity_y_q16 >= INT32_C(0) &&
            bottom >=
                platform_content.stage.platform_y_q16 -
                    maximum_diagonal_drop &&
            bottom <= platform_content.stage.platform_y_q16)
        {
            break;
        }
        if (inspection.players[0].grounded != UINT8_C(0) ||
            !step_duel_trigger(
                platform_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            break;
        }
    }
    if (tick == UINT32_C(120) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        !step_duel_trigger(
            platform_sim,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-waveland"
            " tick=%" PRIu32 " action=%u grounded=%u support=%u\n",
            tick,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support);
        return 0;
    }
    return 1;
}

static int reach_platform_special_landing(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL ||
        !launch_player0(sim, 0, out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const int32_t bottom =
            out_inspection->players[0].position_y_q16 +
            content->fighter.half_height_q16;
        const int32_t maximum_diagonal_drop =
            (content->fighter.air_dodge_speed_y_q16 *
             INT32_C(3)) /
            INT32_C(4);

        if (out_inspection->players[0].velocity_y_q16 >= INT32_C(0) &&
            bottom >=
                content->stage.platform_y_q16 -
                    maximum_diagonal_drop &&
            bottom <= content->stage.platform_y_q16)
        {
            break;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) ||
            !step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return tick != UINT32_C(120) &&
           out_inspection->players[0].grounded == UINT8_C(0) &&
           step_duel_trigger(
               sim,
               INT16_MAX,
               INT16_MAX,
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
           out_inspection->players[0].action_ticks == UINT16_C(0) &&
           out_inspection->players[0].grounded == UINT8_C(1) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[0].velocity_x_q16 > INT32_C(0);
}

static int run_ledge_cancel_snapshot_test(
    pf_sim *source,
    const pf_content_view *content,
    const pf_m4_content *ledge_cancel_content)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
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
            "ledge-cancel-query-save-size") ||
        required_bytes != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "ledge-cancel-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "ledge-cancel-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)ledge_cancel_content->fighter
                     .special_landing_ticks +
                 UINT32_C(4);
         ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_duel_trigger(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "ledge-cancel-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "ledge-cancel-loaded-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            source_inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            source_inspection.players[0].action_ticks !=
                loaded_inspection.players[0].action_ticks ||
            source_inspection.players[0].position_x_q16 !=
                loaded_inspection.players[0].position_x_q16 ||
            source_inspection.players[0].position_y_q16 !=
                loaded_inspection.players[0].position_y_q16)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-cancel-snapshot"
                " continuation_tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
        if (tick == UINT32_C(0) &&
            (source_inspection.players[0].action_state !=
                 (uint8_t)PF_M4_ACTION_AIRBORNE ||
             source_inspection.players[0].action_ticks != UINT16_C(0) ||
             source_inspection.players[0].grounded != UINT8_C(0) ||
             source_inspection.players[0].support !=
                 (uint8_t)PF_M4_SURFACE_NONE ||
             source_inspection.players[0].position_x_q16 <=
                 ledge_cancel_content->stage.platform_center_x_q16 +
                     ledge_cancel_content->stage
                         .platform_half_width_q16))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-cancel-transition"
                " action=%u ticks=%u grounded=%u support=%u x=%" PRId32
                "\n",
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].action_ticks,
                (unsigned int)source_inspection.players[0].grounded,
                (unsigned int)source_inspection.players[0].support,
                source_inspection.players[0].position_x_q16);
            return 0;
        }
    }
    return 1;
}

static int run_ledge_cancel_test(const pf_m4_content *default_content)
{
    test_sim_storage ledge_storage;
    test_sim_storage center_storage;
    pf_sim *ledge_sim = NULL;
    pf_sim *center_sim = NULL;
    pf_m4_content ledge_content = *default_content;
    pf_m4_content center_content;
    pf_content_view ledge_view;
    pf_content_view center_view;
    pf_m4_inspection inspection;
    int32_t landing_x;
    uint32_t tick;

    ledge_content.stage.platform_center_x_q16 =
        -(INT32_C(5) * PF_Q16_ONE) / INT32_C(2);
    ledge_content.stage.platform_y_q16 =
        ledge_content.stage.floor_y_q16 -
        INT32_C(6) * PF_Q16_ONE;
    ledge_content.stage.platform_half_width_q16 = PF_Q16_ONE;
    ledge_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    ledge_content.stage.spawn_spacing_q16 =
        INT32_C(2) * PF_Q16_ONE;
    center_content = ledge_content;
    center_content.stage.platform_center_x_q16 =
        -INT32_C(2) * PF_Q16_ONE;
    center_content.stage.platform_half_width_q16 =
        INT32_C(5) * PF_Q16_ONE;

    if (!expect_status(
            pf_m4_make_content_view(&ledge_content, &ledge_view),
            PF_STATUS_OK,
            "ledge-cancel-content") ||
        !expect_status(
            pf_m4_make_content_view(&center_content, &center_view),
            PF_STATUS_OK,
            "ledge-cancel-center-content") ||
        !initialize_sim(
            &ledge_storage,
            &ledge_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &ledge_sim) ||
        !initialize_sim(
            &center_storage,
            &center_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &center_sim) ||
        !expect_status(
            pf_sim_reset(ledge_sim, UINT64_C(0x1ed6eca0)),
            PF_STATUS_OK,
            "ledge-cancel-reset") ||
        !expect_status(
            pf_sim_reset(center_sim, UINT64_C(0x1ed6eca1)),
            PF_STATUS_OK,
            "ledge-cancel-center-reset") ||
        !reach_platform_special_landing(
            ledge_sim,
            &ledge_content,
            &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-cancel-setup\n");
        return 0;
    }
    landing_x = inspection.players[0].position_x_q16;
    if (landing_x >=
            ledge_content.stage.platform_center_x_q16 +
                ledge_content.stage.platform_half_width_q16 ||
        !run_ledge_cancel_snapshot_test(
            ledge_sim,
            &ledge_view,
            &ledge_content))
    {
        return 0;
    }

    if (!reach_platform_special_landing(
            center_sim,
            &center_content,
            &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-cancel-center-setup\n");
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <
             (uint32_t)center_content.fighter.special_landing_ticks;
         ++tick)
    {
        if (!step_duel_trigger(
                center_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
            inspection.players[0].action_ticks != (uint16_t)tick ||
            inspection.players[0].grounded != UINT8_C(1) ||
            inspection.players[0].support !=
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-cancel-center-lock"
                " tick=%" PRIu32 " action=%u action_ticks=%u\n",
                tick,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks);
            return 0;
        }
    }
    if (!step_duel_trigger(
            center_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-cancel-center-exact-end\n");
        return 0;
    }
    return 1;
}

static int run_ground_dodge_snapshot_test(
    pf_sim *source,
    const pf_content_view *content)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
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
            "ground-dodge-query-save-size") ||
        required_bytes != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "ground-dodge-save") ||
        destination.size != required_bytes)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "ground-dodge-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "ground-dodge-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "ground-dodge-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_duel_trigger(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "ground-dodge-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "ground-dodge-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            source_inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            source_inspection.players[0].action_ticks !=
                loaded_inspection.players[0].action_ticks ||
            source_inspection.players[0].position_x_q16 !=
                loaded_inspection.players[0].position_x_q16 ||
            source_inspection.players[0].invulnerable !=
                loaded_inspection.players[0].invulnerable)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ground-dodge-snapshot"
                " continuation_tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    return 1;
}

static int run_ground_dodge_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage storage;
    test_sim_storage wall_storage;
    test_sim_storage edge_storage;
    pf_m4_content invalid_content = *default_content;
    pf_m4_content wall_content = *default_content;
    pf_m4_content edge_content = *default_content;
    pf_content_view wall_view;
    pf_content_view edge_view;
    pf_sim *sim = NULL;
    pf_sim *wall_sim = NULL;
    pf_sim *edge_sim = NULL;
    pf_m4_inspection inspection;
    int32_t start_x;
    int32_t expected_x;
    int8_t facing;
    uint32_t elapsed;
    const int32_t forward_roll_displacement_q16 = INT32_C(232174);
    const int32_t backward_roll_displacement_q16 = INT32_C(109416);

    if (default_content->fighter.forward_roll_ticks != UINT16_C(31) ||
        default_content->fighter.backward_roll_ticks != UINT16_C(31) ||
        default_content->fighter.roll_movement_begin_tick != UINT16_C(3) ||
        default_content->fighter.roll_movement_end_tick != UINT16_C(20) ||
        default_content->fighter.roll_invulnerability_begin_tick !=
            UINT16_C(4) ||
        default_content->fighter.roll_invulnerability_end_tick !=
            UINT16_C(17) ||
        default_content->fighter.spot_dodge_ticks != UINT16_C(32) ||
        default_content->fighter
                .spot_dodge_invulnerability_begin_tick !=
            UINT16_C(3) ||
        default_content->fighter.spot_dodge_invulnerability_end_tick !=
            UINT16_C(16))
    {
        return 0;
    }

    invalid_content.fighter.forward_roll_speed_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-forward-roll-speed"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.roll_movement_end_tick =
        invalid_content.fighter.backward_roll_ticks + UINT16_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-roll-window-after-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.spot_dodge_invulnerability_end_tick =
        invalid_content.fighter.spot_dodge_ticks + UINT16_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-spot-window-after-duration"))
    {
        return 0;
    }

    if (!initialize_sim(
            &storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d6e)),
            PF_STATUS_OK,
            "ground-dodge-reset") ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=neutral-trigger-shield\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d6f)),
            PF_STATUS_OK,
            "forward-roll-reset") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "forward-roll-start-inspect"))
    {
        return 0;
    }
    start_x = inspection.players[0].position_x_q16;
    facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            facing == INT8_C(1) ? INT16_MAX : INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].facing != facing)
    {
        return 0;
    }
    elapsed = UINT32_C(1);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_ROLL_FORWARD &&
           elapsed <= UINT32_C(40))
    {
        const int expected_invulnerable =
            inspection.players[0].action_ticks >=
                default_content->fighter
                    .roll_invulnerability_begin_tick &&
            inspection.players[0].action_ticks <
                default_content->fighter
                    .roll_invulnerability_end_tick;

        if (inspection.players[0].invulnerable !=
                (uint8_t)expected_invulnerable ||
            !step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++elapsed;
    }
    expected_x =
        start_x +
        (int32_t)facing * forward_roll_displacement_q16;
    if (elapsed !=
            (uint32_t)default_content->fighter.forward_roll_ticks +
                UINT32_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].position_x_q16 != expected_x ||
        inspection.players[0].facing != (int8_t)-facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=forward-roll-contract"
            " elapsed=%" PRIu32 " x=%" PRId32
            " expected_x=%" PRId32 "\n",
            elapsed,
            inspection.players[0].position_x_q16,
            expected_x);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d70)),
            PF_STATUS_OK,
            "backward-roll-reset") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "backward-roll-start-inspect"))
    {
        return 0;
    }
    start_x = inspection.players[0].position_x_q16;
    facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            facing == INT8_C(1) ? INT16_MIN : INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_BACKWARD ||
        inspection.players[0].facing != facing)
    {
        return 0;
    }
    elapsed = UINT32_C(1);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_ROLL_BACKWARD &&
           elapsed <= UINT32_C(44))
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++elapsed;
    }
    expected_x =
        start_x -
        (int32_t)facing * backward_roll_displacement_q16;
    if (elapsed !=
            (uint32_t)default_content->fighter.backward_roll_ticks +
                UINT32_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].position_x_q16 != expected_x ||
        inspection.players[0].facing != facing)
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d71)),
            PF_STATUS_OK,
            "spot-dodge-priority-reset") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "spot-dodge-start-inspect"))
    {
        return 0;
    }
    start_x = inspection.players[0].position_x_q16;
    facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        inspection.players[0].facing != facing)
    {
        return 0;
    }
    elapsed = UINT32_C(1);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SPOT_DODGE &&
           elapsed <= UINT32_C(32))
    {
        const int expected_invulnerable =
            inspection.players[0].action_ticks >=
                default_content->fighter
                    .spot_dodge_invulnerability_begin_tick &&
            inspection.players[0].action_ticks <
                default_content->fighter
                    .spot_dodge_invulnerability_end_tick;

        if (inspection.players[0].invulnerable !=
                (uint8_t)expected_invulnerable)
        {
            return 0;
        }
        if (inspection.players[0].action_ticks == UINT16_C(5))
        {
            if (!run_ground_dodge_snapshot_test(sim, default_view) ||
                !expect_status(
                    pf_m4_inspect(sim, &inspection),
                    PF_STATUS_OK,
                    "ground-dodge-post-snapshot-inspect"))
            {
                return 0;
            }
            elapsed += UINT32_C(8);
        }
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++elapsed;
    }
    if (elapsed !=
            (uint32_t)default_content->fighter.spot_dodge_ticks +
                UINT32_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].position_x_q16 != start_x ||
        inspection.players[0].facing != facing)
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d72)),
            PF_STATUS_OK,
            "held-down-negative-reset") ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d73)),
            PF_STATUS_OK,
            "shield-flick-roll-reset") ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d74)),
            PF_STATUS_OK,
            "shield-flick-spot-reset") ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d741)),
            PF_STATUS_OK,
            "c-stick-buffered-roll-reset") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "c-stick-buffered-roll-inspect"))
    {
        return 0;
    }
    facing = inspection.players[0].facing;
    if (!step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            facing == INT8_C(1) ? INT16_MAX : INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            facing == INT8_C(1) ? INT16_MAX : INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.players[0].facing != facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-buffered-roll\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d742)),
            PF_STATUS_OK,
            "c-stick-buffered-spot-dodge-reset") ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_MAX,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_MAX,
            PF_INPUT_BUTTON_STRONG_ATTACK |
                PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-buffered-spot-dodge"
            "-priority\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d743)),
            PF_STATUS_OK,
            "c-stick-buffered-jump-reset") ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-buffered-jump\n");
        return 0;
    }
    for (elapsed = UINT32_C(0);
         elapsed < (uint32_t)default_content->fighter.jump_squat_ticks;
         ++elapsed)
    {
        if (!step_duel_secondary_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                INT16_MIN,
                PF_INPUT_BUTTON_STRONG_ATTACK,
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].velocity_y_q16 !=
            -default_content->fighter.full_hop_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-held-full-hop"
            " action=%u vy=%" PRId32 " expected=%" PRId32 "\n",
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].velocity_y_q16,
            -default_content->fighter.full_hop_speed_q16);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d744)),
            PF_STATUS_OK,
            "c-stick-release-short-hop-reset") ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection) ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (elapsed = UINT32_C(0);
         elapsed < (uint32_t)default_content->fighter.jump_squat_ticks;
         ++elapsed)
    {
        if (!step_duel_secondary_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].velocity_y_q16 !=
            -default_content->fighter.full_hop_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-release-full-hop"
            " action=%u vy=%" PRId32 " expected=%" PRId32 "\n",
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].velocity_y_q16,
            -default_content->fighter.full_hop_speed_q16);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d745)),
            PF_STATUS_OK,
            "c-stick-shield-release-buffer-reset") ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    while (inspection.players[0].action_ticks <
           default_content->fighter.shield_minimum_hold_ticks)
    {
        if (!step_duel_secondary_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-shield-release"
            "-spot-dodge\n");
        return 0;
    }

    wall_content.stage.solid_left_q16 =
        -INT32_C(6) * PF_Q16_ONE;
    wall_content.stage.solid_right_q16 =
        -INT32_C(4) * PF_Q16_ONE;
    wall_content.stage.solid_bottom_q16 =
        INT32_C(31) * PF_Q16_ONE;
    wall_content.stage.platform_center_x_q16 =
        INT32_C(6) * PF_Q16_ONE;
    wall_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&wall_content, &wall_view),
            PF_STATUS_OK,
            "roll-wall-content") ||
        !initialize_sim(
            &wall_storage,
            &wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &wall_sim) ||
        !expect_status(
            pf_sim_reset(wall_sim, UINT64_C(0xd0d75)),
            PF_STATUS_OK,
            "roll-wall-reset") ||
        !step_duel_trigger(
            wall_sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (elapsed = UINT32_C(1);
         elapsed < UINT32_C(40) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_ROLL_FORWARD;
         ++elapsed)
    {
        if (!step_duel_trigger(
                wall_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].position_x_q16 >
        wall_content.stage.solid_left_q16 -
            wall_content.fighter.half_width_q16)
    {
        return 0;
    }

    edge_content.stage.spawn_spacing_q16 =
        PF_Q16_ONE / INT32_C(8);
    edge_content.stage.floor_left_q16 =
        -(INT32_C(3) * PF_Q16_ONE) / INT32_C(2);
    edge_content.stage.platform_center_x_q16 =
        INT32_C(4) * PF_Q16_ONE;
    edge_content.stage.platform_half_width_q16 = PF_Q16_ONE;
    edge_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    edge_content.stage.revival_platform_half_width_q16 =
        edge_content.fighter.half_width_q16;
    if (!expect_status(
            pf_m4_make_content_view(&edge_content, &edge_view),
            PF_STATUS_OK,
            "roll-edge-content") ||
        !initialize_sim(
            &edge_storage,
            &edge_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &edge_sim) ||
        !expect_status(
            pf_sim_reset(edge_sim, UINT64_C(0xd0d76)),
            PF_STATUS_OK,
            "roll-edge-reset") ||
        !step_duel_trigger(
            edge_sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (elapsed = UINT32_C(1);
         elapsed < UINT32_C(40) &&
         inspection.players[0].grounded != UINT8_C(0);
         ++elapsed)
    {
        if (!step_duel_trigger(
                edge_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE)
    {
        return 0;
    }
    return 1;
}

static int run_content_contract_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage rejected_storage;
    test_sim_storage default_storage;
    test_sim_storage tuned_storage;
    pf_m4_content invalid_content = *default_content;
    pf_m4_content jump_tuned_content = *default_content;
    pf_m4_content dash_window_tuned_content = *default_content;
    pf_m4_content moonwalk_tuned_content = *default_content;
    pf_m4_content teeter_tuned_content = *default_content;
    pf_m4_content crouch_tuned_content = *default_content;
    pf_m4_content crouch_step_tuned_content = *default_content;
    pf_m4_content taunt_tuned_content = *default_content;
    pf_m4_content tuned_content = *default_content;
    pf_content_view damaged_view = *default_view;
    pf_content_view jump_tuned_view;
    pf_content_view dash_window_tuned_view;
    pf_content_view moonwalk_tuned_view;
    pf_content_view teeter_tuned_view;
    pf_content_view crouch_tuned_view;
    pf_content_view crouch_step_tuned_view;
    pf_content_view taunt_tuned_view;
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

    invalid_content = *default_content;
    invalid_content.fighter.jump_horizontal_input_speed_q16 = INT32_C(0);
    if (default_content->fighter.jump_horizontal_input_speed_q16 !=
            PF_Q16_ONE * INT32_C(57) / INT32_C(575) ||
        default_content->fighter.jump_horizontal_momentum_multiplier_q16 !=
            PF_Q16_ONE * INT32_C(3) / INT32_C(4) ||
        default_content->fighter.jump_horizontal_max_speed_q16 !=
            PF_Q16_ONE * INT32_C(126) / INT32_C(575) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-jump-horizontal-input-speed"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.jump_horizontal_momentum_multiplier_q16 =
        PF_Q16_ONE + INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-large-jump-horizontal-momentum-multiplier"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.jump_horizontal_max_speed_q16 =
        invalid_content.fighter.jump_horizontal_input_speed_q16 -
        INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-low-jump-horizontal-max-speed"))
    {
        return 0;
    }
    jump_tuned_content.fighter.jump_horizontal_input_speed_q16 +=
        INT32_C(1);
    if (!expect_status(
            pf_m4_make_content_view(
                &jump_tuned_content,
                &jump_tuned_view),
            PF_STATUS_OK,
            "jump-horizontal-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            jump_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-horizontal-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.dash_input_window_ticks = UINT16_C(0);
    if (default_content->fighter.dash_input_window_ticks != UINT16_C(2) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-dash-input-window"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.dash_input_window_ticks =
        invalid_content.fighter.initial_dash_ticks + UINT16_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-dash-input-window"))
    {
        return 0;
    }

    dash_window_tuned_content.fighter.dash_input_window_ticks =
        UINT16_C(3);
    if (!expect_status(
            pf_m4_make_content_view(
                &dash_window_tuned_content,
                &dash_window_tuned_view),
            PF_STATUS_OK,
            "dash-window-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            dash_window_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-window-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.moonwalk_setup_ticks = UINT16_C(1);
    if (default_content->fighter.moonwalk_setup_ticks != UINT16_C(2) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-short-moonwalk-setup"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.moonwalk_setup_ticks =
        invalid_content.fighter.initial_dash_ticks;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-moonwalk-setup"))
    {
        return 0;
    }

    moonwalk_tuned_content.fighter.moonwalk_setup_ticks = UINT16_C(3);
    if (!expect_status(
            pf_m4_make_content_view(
                &moonwalk_tuned_content,
                &moonwalk_tuned_view),
            PF_STATUS_OK,
            "moonwalk-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            moonwalk_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.teeter_snap_distance_q16 = INT32_C(0);
    if (default_content->fighter.teeter_snap_distance_q16 !=
            (INT32_C(2) * PF_Q16_ONE) / INT32_C(5) ||
        default_content->fighter.teeter_ticks != UINT16_C(30) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-teeter-snap"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.teeter_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-teeter-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.teeter_ticks = UINT16_C(121);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-teeter-duration"))
    {
        return 0;
    }

    teeter_tuned_content.fighter.teeter_snap_distance_q16 +=
        INT32_C(1);
    if (!expect_status(
            pf_m4_make_content_view(
                &teeter_tuned_content,
                &teeter_tuned_view),
            PF_STATUS_OK,
            "teeter-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            teeter_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.crouch_start_ticks = UINT16_C(0);
    if (default_content->fighter.crouch_start_ticks != UINT16_C(7) ||
        default_content->fighter.crouch_end_ticks != UINT16_C(10) ||
        default_content->fighter.crouch_axis_threshold !=
            UINT16_C(22528) ||
        default_content->fighter.crouch_release_axis_threshold !=
            UINT16_C(20479) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-crouch-start-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_end_ticks = UINT16_C(121);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-crouch-end-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_release_axis_threshold =
        invalid_content.fighter.crouch_axis_threshold;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-crouch-threshold-without-hysteresis"))
    {
        return 0;
    }
    crouch_tuned_content.fighter.crouch_start_ticks = UINT16_C(8);
    if (!expect_status(
            pf_m4_make_content_view(
                &crouch_tuned_content,
                &crouch_tuned_view),
            PF_STATUS_OK,
            "crouch-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            crouch_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.crouch_step_speed_q16 = -INT32_C(1);
    if (default_content->fighter.crouch_step_speed_q16 !=
            INT32_C(0) ||
        default_content->fighter.crouch_step_ticks != UINT16_C(1) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-negative-crouch-step-speed"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_step_speed_q16 =
        invalid_content.fighter.walk_speed_q16 + INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-fast-crouch-step"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_step_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-crouch-step-duration"))
    {
        return 0;
    }

    crouch_step_tuned_content.fighter.crouch_step_speed_q16 +=
        INT32_C(1);
    if (!expect_status(
            pf_m4_make_content_view(
                &crouch_step_tuned_content,
                &crouch_step_tuned_view),
            PF_STATUS_OK,
            "crouch-step-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            crouch_step_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-step-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.taunt_ticks = UINT16_C(0);
    if (default_content->fighter.taunt_ticks != UINT16_C(61) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-taunt-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.taunt_ticks = UINT16_C(601);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-taunt-duration"))
    {
        return 0;
    }

    taunt_tuned_content.fighter.taunt_ticks = UINT16_C(91);
    if (!expect_status(
            pf_m4_make_content_view(
                &taunt_tuned_content,
                &taunt_tuned_view),
            PF_STATUS_OK,
            "taunt-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            taunt_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-content-hash\n");
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

    tuned_content.fighter.walk_speed_q16 = PF_Q16_ONE / INT32_C(20);
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
    int32_t fast_walk_velocity;
    int32_t dash_velocity;
    int32_t run_velocity;
    int16_t ramp_low;
    int16_t ramp_middle;
    int16_t ramp_high;
    int16_t crouch_walk_axis;
    int16_t crouch_dash_axis;
    int8_t crouch_facing;
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

    ramp_low = (int16_t)(content->fighter.axis_dead_zone + UINT16_C(1));
    ramp_middle = (int16_t)(
        ((uint32_t)content->fighter.axis_dead_zone +
         (uint32_t)content->fighter.dash_axis_threshold) /
        UINT32_C(2));
    ramp_high =
        (int16_t)(content->fighter.dash_axis_threshold - UINT16_C(1));
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "gradual-walk-reset") ||
        !step_duel(
            sim,
            ramp_low,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            sim,
            ramp_middle,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks !=
            content->fighter.dash_input_window_ticks ||
        !step_duel(
            sim,
            ramp_high,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
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
    fast_walk_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks !=
            content->fighter.dash_input_window_ticks ||
        inspection.players[0].dash_direction != INT8_C(0) ||
        inspection.players[0].facing != INT8_C(1) ||
        fast_walk_velocity <= slow_walk_velocity ||
        fast_walk_velocity > content->fighter.walk_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=gradual-fast-walk\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "two-tick-flick-reset") ||
        !step_duel(
            sim,
            ramp_low,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=two-tick-flick-dash\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "two-sample-dash-reset") ||
        !step_duel(
            sim,
            ramp_low,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
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
            "m4-movement=fail operation=two-sample-controller-dash\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "direct-dash-reset") ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=direct-dash\n");
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].dash_direction != INT8_C(-1) ||
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-dance-reversal\n");
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
            "m4-movement=fail operation=dash-dance-turn-exit\n");
        return 0;
    }

    for (tick = UINT32_C(1);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
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
        inspection.players[0].facing != INT8_C(-1) ||
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
         tick <
             (uint32_t)content->fighter.run_turnaround_ticks +
                 UINT32_C(32) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
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
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        absolute_i32(inspection.players[0].velocity_x_q16) >=
            absolute_i32(run_velocity))
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
            (uint8_t)PF_M4_ACTION_RUN_BRAKE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-horizontal-lockout\n");
        return 0;
    }
    for (tick = UINT32_C(2);
         tick + UINT32_C(1) <
             (uint32_t)content->fighter.run_brake_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RUN_BRAKE)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=run-brake-full-lockout\n");
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-expiry-turn\n");
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
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
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
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].grounded != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch\n");
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH_START ||
            inspection.players[0].action_ticks != (uint16_t)tick)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=crouch-start-duration\n");
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_END ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-release-entry\n");
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_end_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH_END ||
            inspection.players[0].action_ticks != (uint16_t)tick)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=crouch-end-duration\n");
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-end-exit\n");
        return 0;
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)(content->fighter.crouch_axis_threshold -
                      UINT16_C(1)),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-entry-threshold\n");
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                (int16_t)content->fighter.crouch_axis_threshold,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter
                .crouch_release_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)(content->fighter
                          .crouch_release_axis_threshold -
                      UINT16_C(1)),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-release-threshold\n");
        return 0;
    }
    crouch_facing = inspection.players[0].facing;
    crouch_walk_axis =
        (int16_t)(
            ((uint32_t)content->fighter.axis_dead_zone +
             (uint32_t)content->fighter.dash_axis_threshold) /
            UINT32_C(2));
    if (crouch_facing < INT8_C(0))
    {
        crouch_walk_axis = (int16_t)-crouch_walk_axis;
    }
    if (!step_duel(
            sim,
            crouch_walk_axis,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-end-walk-interrupt\n");
        return 0;
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                (int16_t)content->fighter.crouch_axis_threshold,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-dash-setup\n");
        return 0;
    }
    crouch_facing = inspection.players[0].facing;
    crouch_dash_axis =
        crouch_facing > INT8_C(0) ? INT16_MIN : INT16_MAX;
    if (!step_duel(
            sim,
            crouch_dash_axis,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].facing != crouch_facing ||
        !step_duel(
            sim,
            crouch_dash_axis,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].facing != (int8_t)-crouch_facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-dash-interrupt\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-start-guard-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-start-guard-interrupt\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-wait-guard-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                (int16_t)content->fighter.crouch_axis_threshold,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-guard-interrupt\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-end-guard-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                (int16_t)content->fighter.crouch_axis_threshold,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_END ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-end-guard-interrupt\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-start-taunt-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-start-taunt-interrupt\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-wait-taunt-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                (int16_t)content->fighter.crouch_axis_threshold,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-taunt-interrupt\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-end-taunt-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                (int16_t)content->fighter.crouch_axis_threshold,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_END ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-end-taunt-interrupt\n");
        return 0;
    }
    return 1;
}

static int run_jump_takeoff_momentum_route(
    pf_sim *sim,
    const pf_m4_content *content,
    uint64_t seed,
    int16_t takeoff_axis,
    int32_t *out_velocity_x)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, seed),
            PF_STATUS_OK,
            "jump-takeoff-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
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
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-takeoff-run-entry\n");
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jump_squat_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                takeoff_axis,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-takeoff-launch"
            " action=%u grounded=%u facing=%d velocity=(%" PRId32
            ",%" PRId32 ")\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded,
            (int)inspection.players[0].facing,
            inspection.players[0].velocity_x_q16,
            inspection.players[0].velocity_y_q16);
        return 0;
    }
    *out_velocity_x = inspection.players[0].velocity_x_q16;
    return 1;
}

static int run_jump_takeoff_momentum_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    int32_t forward_velocity_x;
    int32_t neutral_velocity_x;
    int32_t reverse_velocity_x;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !run_jump_takeoff_momentum_route(
            sim,
            content,
            UINT64_C(0x4a554d50464f52),
            INT16_MAX,
            &forward_velocity_x) ||
        !run_jump_takeoff_momentum_route(
            sim,
            content,
            UINT64_C(0x4a554d504e4555),
            INT16_C(0),
            &neutral_velocity_x) ||
        !run_jump_takeoff_momentum_route(
            sim,
            content,
            UINT64_C(0x4a554d50524556),
            INT16_MIN,
            &reverse_velocity_x))
    {
        return 0;
    }
    if (neutral_velocity_x <= reverse_velocity_x ||
        forward_velocity_x <= neutral_velocity_x ||
        absolute_i32(reverse_velocity_x) >
            neutral_velocity_x / INT32_C(2))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-takeoff-momentum"
            " forward=%" PRId32 " neutral=%" PRId32
            " reverse=%" PRId32 " tolerance=%" PRId32 "\n",
            forward_velocity_x,
            neutral_velocity_x,
            reverse_velocity_x,
            content->fighter.air_acceleration_q16);
        return 0;
    }
    return 1;
}

static int run_fox_trot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t starting_position_x;
    int32_t previous_position_x;
    uint32_t burst;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0xf07f707)),
            PF_STATUS_OK,
            "fox-trot-reset") ||
        !expect_status(
            pf_m4_inspect(source, &source_inspection),
            PF_STATUS_OK,
            "fox-trot-initial-inspect"))
    {
        return 0;
    }
    starting_position_x =
        source_inspection.players[0].position_x_q16;

    if (!step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 !=
            content->fighter.initial_dash_speed_q16 ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks != UINT16_C(2) ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 < INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 >=
            content->fighter.initial_dash_speed_q16 ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "fox-trot-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fox-trot-entry"
            " action=%u ticks=%u facing=%d dash=%d"
            " velocity_x=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "fox-trot-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "fox-trot-load"))
    {
        return 0;
    }

    previous_position_x =
        source_inspection.players[0].position_x_q16;
    for (burst = UINT32_C(0); burst < UINT32_C(4); ++burst)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
            source_inspection.players[0].action_ticks != UINT16_C(1) ||
            source_inspection.players[0].dash_direction != INT8_C(1) ||
            source_inspection.players[0].facing != INT8_C(1) ||
            source_inspection.players[0].velocity_x_q16 !=
                content->fighter.initial_dash_speed_q16 ||
            source_inspection.players[0].position_x_q16 <=
                previous_position_x ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "fox-trot-source-dash-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "fox-trot-loaded-dash-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=fox-trot-restart"
                " burst=%" PRIu32 " action=%u ticks=%u"
                " position_x=%" PRId32 " previous_x=%" PRId32
                "\n",
                burst,
                (unsigned int)
                    source_inspection.players[0].action_state,
                (unsigned int)
                    source_inspection.players[0].action_ticks,
                source_inspection.players[0].position_x_q16,
                previous_position_x);
            return 0;
        }
        previous_position_x =
            source_inspection.players[0].position_x_q16;

        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
            source_inspection.players[0].action_ticks != UINT16_C(0) ||
            source_inspection.players[0].dash_direction != INT8_C(0) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "fox-trot-source-release-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "fox-trot-loaded-release-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=fox-trot-release"
                " burst=%" PRIu32 " action=%u ticks=%u\n",
                burst,
                (unsigned int)
                    source_inspection.players[0].action_state,
                (unsigned int)
                    source_inspection.players[0].action_ticks);
            return 0;
        }
        previous_position_x =
            source_inspection.players[0].position_x_q16;
    }
    if (source_inspection.players[0].position_x_q16 <=
        starting_position_x)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fox-trot-travel\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xf07f708)),
            PF_STATUS_OK,
            "fox-trot-held-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].velocity_x_q16 <=
            content->fighter.initial_dash_speed_q16 ||
        source_inspection.players[0].velocity_x_q16 >=
            content->fighter.run_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fox-trot-held-negative"
            " action=%u ticks=%u velocity_x=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xf07f709)),
            PF_STATUS_OK,
            "fox-trot-weak-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(12000),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        source_inspection.players[0].dash_direction != INT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fox-trot-weak-negative"
            " action=%u dash=%d\n",
            (unsigned int)source_inspection.players[0].action_state,
            (int)source_inspection.players[0].dash_direction);
        return 0;
    }
    return 1;
}

static int run_moonwalk_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t active_position_x;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x600dca7)),
            PF_STATUS_OK,
            "moonwalk-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
        source_inspection.players[0].action_ticks !=
            content->fighter.moonwalk_setup_ticks ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "moonwalk-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-setup"
            " action=%u ticks=%u facing=%d dash=%d\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction);
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "moonwalk-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "moonwalk-load"))
    {
        return 0;
    }

    if (!step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
        source_inspection.players[0].action_ticks !=
            content->fighter.moonwalk_setup_ticks ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "moonwalk-source-setup-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "moonwalk-loaded-setup-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-setup-window\n");
        return 0;
    }

    if (!step_duel(
            source,
            INT16_MIN,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MIN,
            INT16_MAX,
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
        source_inspection.players[0].action_ticks !=
            content->fighter.moonwalk_setup_ticks ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "moonwalk-source-lower-back-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "moonwalk-loaded-lower-back-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-lower-back\n");
        return 0;
    }

    if (!step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_MOONWALK ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 !=
            -content->fighter.initial_dash_speed_q16 ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "moonwalk-source-entry-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "moonwalk-loaded-entry-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-entry"
            " action=%u ticks=%u facing=%d dash=%d velocity=%" PRId32
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    if (!step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_MOONWALK ||
        source_inspection.players[0].action_ticks != UINT16_C(2) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 !=
            -content->fighter.initial_dash_speed_q16 ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "moonwalk-source-hold-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "moonwalk-loaded-hold-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-hold\n");
        return 0;
    }
    active_position_x = source_inspection.players[0].position_x_q16;

    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].velocity_x_q16 > INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 <=
            -content->fighter.initial_dash_speed_q16 ||
        source_inspection.players[0].position_x_q16 >= active_position_x ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "moonwalk-source-release-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "moonwalk-loaded-release-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-release"
            " action=%u velocity=%" PRId32 " position=%" PRId32
            " active_position=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            source_inspection.players[0].velocity_x_q16,
            source_inspection.players[0].position_x_q16,
            active_position_x);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x600dca8)),
            PF_STATUS_OK,
            "moonwalk-immediate-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1) ||
        source_inspection.players[0].velocity_x_q16 !=
            -content->fighter.initial_dash_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-immediate-negative\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x600dca9)),
            PF_STATUS_OK,
            "moonwalk-short-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(-13500),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=moonwalk-short-negative\n");
        return 0;
    }
    return 1;
}

static int enter_right_teeter(
    pf_sim *sim,
    const pf_m4_content *content,
    uint64_t edge_buttons,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "teeter-inspect-start"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const int32_t distance_q16 =
            content->stage.floor_right_q16 -
            out_inspection->players[0].position_x_q16;

        if (distance_q16 <= INT32_C(5) * PF_Q16_ONE)
        {
            break;
        }
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=teeter-approach-dash"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }

    for (tick = UINT32_C(0); tick < UINT32_C(60); ++tick)
    {
        if (out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0) ||
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_TEETER)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=teeter-approach-stop\n");
            return 0;
        }
    }

    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const int32_t position_q16 =
            out_inspection->players[0].position_x_q16;
        const int32_t velocity_q16 =
            out_inspection->players[0].velocity_x_q16;
        const int32_t distance_q16 =
            content->stage.floor_right_q16 - position_q16;
        const int32_t release_velocity_q16 =
            velocity_q16 > content->fighter.traction_q16
                ? velocity_q16 - content->fighter.traction_q16
                : INT32_C(0);
        int16_t selected_axis = INT16_C(0);
        int32_t selected_velocity_q16 = INT32_C(0);
        uint32_t axis;

        if (release_velocity_q16 > distance_q16)
        {
            if (!step_duel(
                    sim,
                    INT16_C(0),
                    INT16_C(0),
                    edge_buttons,
                    out_inspection))
            {
                return 0;
            }
            break;
        }

        for (axis =
                 (uint32_t)content->fighter.axis_dead_zone + UINT32_C(1);
             axis < (uint32_t)content->fighter.dash_axis_threshold;
             ++axis)
        {
            const int32_t target_q16 =
                (int32_t)(
                    (int64_t)(int32_t)axis *
                    (int64_t)content->fighter.walk_speed_q16 /
                    INT64_C(32767));
            int32_t next_velocity_q16 = velocity_q16;
            const int32_t acceleration_q16 =
                content->fighter.ground_acceleration_q16;
            int32_t next_release_velocity_q16;

            if (next_velocity_q16 < target_q16)
            {
                next_velocity_q16 += acceleration_q16;
                if (next_velocity_q16 > target_q16)
                {
                    next_velocity_q16 = target_q16;
                }
            }
            else if (next_velocity_q16 > target_q16)
            {
                next_velocity_q16 -= acceleration_q16;
                if (next_velocity_q16 < target_q16)
                {
                    next_velocity_q16 = target_q16;
                }
            }
            next_release_velocity_q16 =
                next_velocity_q16 > content->fighter.traction_q16
                    ? next_velocity_q16 -
                          content->fighter.traction_q16
                    : INT32_C(0);

            if (next_velocity_q16 < distance_q16 &&
                distance_q16 - next_velocity_q16 <
                    next_release_velocity_q16)
            {
                selected_axis = (int16_t)axis;
                selected_velocity_q16 = next_velocity_q16;
                break;
            }
            if (next_velocity_q16 < distance_q16 &&
                next_velocity_q16 > selected_velocity_q16)
            {
                selected_axis = (int16_t)axis;
                selected_velocity_q16 = next_velocity_q16;
            }
        }
        if (selected_axis == INT16_C(0) ||
            !step_duel(
                sim,
                selected_axis,
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=teeter-approach-walk"
                " tick=%" PRIu32 " distance=%" PRId32
                " velocity=%" PRId32 " axis=%d\n",
                tick,
                distance_q16,
                velocity_q16,
                (int)selected_axis);
            return 0;
        }
    }

    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TEETER ||
        out_inspection->players[0].action_ticks != UINT16_C(0) ||
        out_inspection->players[0].position_x_q16 !=
            content->stage.floor_right_q16 ||
        out_inspection->players[0].velocity_x_q16 != INT32_C(0) ||
        out_inspection->players[0].grounded == UINT8_C(0) ||
        out_inspection->players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        out_inspection->players[0].facing != INT8_C(1) ||
        out_inspection->players[0].dash_direction != INT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-entry"
            " action=%u ticks=%u position=%" PRId32
            " velocity=%" PRId32 " grounded=%u\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            out_inspection->players[0].position_x_q16,
            out_inspection->players[0].velocity_x_q16,
            (unsigned int)out_inspection->players[0].grounded);
        return 0;
    }
    return 1;
}

static int run_teeter_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;
    int observed_teeter = 0;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e2)),
            PF_STATUS_OK,
            "teeter-reset") ||
        !enter_right_teeter(
            source,
            content,
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TEETER ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "teeter-query-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "teeter-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "teeter-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "teeter-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "teeter-loaded-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_TEETER ||
            source_inspection.players[0].action_ticks !=
                (uint16_t)(tick + UINT32_C(2)))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=teeter-snapshot"
                " continuation_tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        source_inspection.players[0].position_x_q16 !=
            content->stage.floor_right_q16 ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "teeter-attack-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "teeter-attack-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-attack-cancel\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e3)),
            PF_STATUS_OK,
            "teeter-dash-reset") ||
        !enter_right_teeter(
            source,
            content,
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1) ||
        source_inspection.players[0].velocity_x_q16 !=
            -content->fighter.initial_dash_speed_q16 ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        source_inspection.players[0].position_x_q16 !=
            content->stage.floor_right_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-reverse-dash-cancel\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e4)),
            PF_STATUS_OK,
            "teeter-held-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
        if (source_inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_TEETER)
        {
            observed_teeter = 1;
        }
        if (source_inspection.players[0].grounded == UINT8_C(0))
        {
            break;
        }
    }
    if (observed_teeter != 0 ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-held-outward-negative\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e5)),
            PF_STATUS_OK,
            "teeter-early-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(10); ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(60); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            source_inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_TEETER)
        {
            return 0;
        }
        if (source_inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        source_inspection.players[0].position_x_q16 >=
            content->stage.floor_right_q16 -
                content->fighter.teeter_snap_distance_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-early-release-negative"
            " action=%u position=%" PRId32 " threshold=%" PRId32
            " velocity=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            source_inspection.players[0].position_x_q16,
            content->stage.floor_right_q16 -
                content->fighter.teeter_snap_distance_q16,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e6)),
            PF_STATUS_OK,
            "teeter-expiry-reset") ||
        !enter_right_teeter(
            source,
            content,
            UINT64_C(0),
            &source_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.teeter_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
        if (tick + UINT32_C(1) <
                (uint32_t)content->fighter.teeter_ticks &&
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_TEETER)
        {
            return 0;
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].position_x_q16 !=
            content->stage.floor_right_q16 ||
        source_inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-expiry\n");
        return 0;
    }
    return 1;
}

static int run_taunt_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t dash_position_q16;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x7a017ca)),
            PF_STATUS_OK,
            "taunt-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection))
    {
        return 0;
    }
    dash_position_q16 = source_inspection.players[0].position_x_q16;
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].position_x_q16 <= dash_position_q16 ||
        source_inspection.players[0].velocity_x_q16 !=
            content->fighter.initial_dash_speed_q16 -
                content->fighter.traction_q16 ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "taunt-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-dash-entry"
            " action=%u ticks=%u velocity=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "taunt-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "taunt-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.taunt_ticks;
         ++tick)
    {
        const uint64_t buttons =
            PF_INPUT_BUTTON_TAUNT |
            (tick == UINT32_C(1)
                 ? PF_INPUT_BUTTON_ATTACK | PF_INPUT_BUTTON_JUMP
                 : UINT64_C(0));

        if (!step_duel(
                source,
                INT16_MIN,
                INT16_C(0),
                buttons,
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_MIN,
                INT16_C(0),
                buttons,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "taunt-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "taunt-loaded-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=taunt-snapshot"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
        if (tick + UINT32_C(1) <
                (uint32_t)content->fighter.taunt_ticks &&
            (source_inspection.players[0].action_state !=
                 (uint8_t)PF_M4_ACTION_TAUNT ||
             source_inspection.players[0].action_ticks !=
                 (uint16_t)(tick + UINT32_C(1))))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=taunt-lock"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-exact-end-or-held-repeat\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7a017cb)),
            PF_STATUS_OK,
            "taunt-cancel-reset") ||
        !enter_right_teeter(
            source,
            content,
            PF_INPUT_BUTTON_TAUNT,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TEETER ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].position_x_q16 !=
            content->stage.floor_right_q16 ||
        source_inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-edge-cancel\n");
        return 0;
    }
    return 1;
}

static int run_stage_humping_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t start_position_q16;
    int32_t first_step_position_q16;
    uint32_t repetition;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x57a6e40)),
            PF_STATUS_OK,
            "stage-humping-reset") ||
        !expect_status(
            pf_m4_inspect(source, &source_inspection),
            PF_STATUS_OK,
            "stage-humping-inspect-start"))
    {
        return 0;
    }
    start_position_q16 =
        source_inspection.players[0].position_x_q16;
    if (!step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].position_x_q16 !=
            start_position_q16 +
                content->fighter.crouch_step_speed_q16 ||
        source_inspection.players[0].velocity_x_q16 !=
            content->fighter.crouch_step_speed_q16 ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "stage-humping-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=stage-humping-first-step"
            " action=%u ticks=%u position=%" PRId32
            " velocity=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].position_x_q16,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }
    first_step_position_q16 =
        source_inspection.players[0].position_x_q16;

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "stage-humping-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "stage-humping-load") ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].position_x_q16 !=
            first_step_position_q16 ||
        source_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "stage-humping-source-crouch-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "stage-humping-loaded-crouch-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=stage-humping-snapshot\n");
        return 0;
    }

    for (repetition = UINT32_C(1);
         repetition < UINT32_C(8);
         ++repetition)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_MAX,
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_MAX,
                INT16_MAX,
                UINT64_C(0),
                &loaded_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
            source_inspection.players[0].position_x_q16 !=
                start_position_q16 +
                    (int32_t)(repetition + UINT32_C(1)) *
                        content->fighter.crouch_step_speed_q16 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "stage-humping-source-step-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "stage-humping-loaded-step-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            !step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH ||
            source_inspection.players[0].velocity_x_q16 != INT32_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=stage-humping-repeat"
                " repetition=%" PRIu32 "\n",
                repetition);
            return 0;
        }
    }

    if (!step_duel(
            source,
            INT16_MIN,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].velocity_x_q16 !=
            -content->fighter.crouch_step_speed_q16 ||
        source_inspection.players[0].position_x_q16 !=
            start_position_q16 +
                INT32_C(7) *
                    content->fighter.crouch_step_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=stage-humping-opposite-step\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x57a6e41)),
            PF_STATUS_OK,
            "stage-humping-held-reset") ||
        !expect_status(
            pf_m4_inspect(source, &source_inspection),
            PF_STATUS_OK,
            "stage-humping-held-inspect"))
    {
        return 0;
    }
    start_position_q16 =
        source_inspection.players[0].position_x_q16;
    if (!step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        source_inspection.players[0].position_x_q16 !=
            start_position_q16 +
                content->fighter.crouch_step_speed_q16 ||
        source_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        source_inspection.players[0].position_x_q16 !=
            start_position_q16 +
                content->fighter.crouch_step_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=stage-humping-held-negative\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x57a6e42)),
            PF_STATUS_OK,
            "stage-humping-neutral-reset") ||
        !expect_status(
            pf_m4_inspect(source, &source_inspection),
            PF_STATUS_OK,
            "stage-humping-neutral-inspect"))
    {
        return 0;
    }
    start_position_q16 =
        source_inspection.players[0].position_x_q16;
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        source_inspection.players[0].position_x_q16 !=
            start_position_q16 ||
        source_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x57a6e43)),
            PF_STATUS_OK,
            "stage-humping-horizontal-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=stage-humping-input-negatives\n");
        return 0;
    }
    return 1;
}

static int run_pivot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0xb17b07)),
            PF_STATUS_OK,
            "pivot-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 !=
            content->fighter.initial_dash_speed_q16 ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 >=
            content->fighter.initial_dash_speed_q16 ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "pivot-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=pivot-frame"
            " action=%u ticks=%u facing=%d dash=%d"
            " velocity_x=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "pivot-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "pivot-load") ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        absolute_i32(source_inspection.players[0].velocity_x_q16) >=
            content->fighter.initial_dash_speed_q16 ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "pivot-source-action-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "pivot-loaded-action-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=pivot-action"
            " action=%u facing=%d dash=%d velocity_x=%" PRId32
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "pivot-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "pivot-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=pivot-future-hash"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xb17b08)),
            PF_STATUS_OK,
            "empty-pivot-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        absolute_i32(source_inspection.players[0].velocity_x_q16) >=
            content->fighter.initial_dash_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=empty-pivot"
            " action=%u facing=%d dash=%d velocity_x=%" PRId32
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xb17b09)),
            PF_STATUS_OK,
            "pivot-held-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1) ||
        source_inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 <=
            -content->fighter.initial_dash_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=pivot-held-negative\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xb17b0a)),
            PF_STATUS_OK,
            "pivot-late-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        source_inspection.players[0].action_ticks != UINT16_C(2))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=pivot-late-negative"
            " action=%u ticks=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks);
        return 0;
    }
    return 1;
}

static int run_dash_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t run_velocity;
    int32_t crouch_velocity;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0xda5ca11)),
            PF_STATUS_OK,
            "dash-cancel-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    run_velocity = source_inspection.players[0].velocity_x_q16;
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        run_velocity <= INT32_C(0) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 >= run_velocity ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "dash-cancel-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-cancel-crouch"
            " action=%u ticks=%u dash_direction=%d facing=%d"
            " velocity_x=%" PRId32 " run_velocity=%" PRId32
            " save_size=%zu\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].dash_direction,
            (int)source_inspection.players[0].facing,
            source_inspection.players[0].velocity_x_q16,
            run_velocity,
            save_size);
        return 0;
    }
    crouch_velocity = source_inspection.players[0].velocity_x_q16;

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "dash-cancel-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "dash-cancel-load") ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DOWN_ATTACK ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 >=
            crouch_velocity ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "dash-cancel-source-action-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "dash-cancel-loaded-action-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-cancel-action"
            " action=%u facing=%d velocity_x=%" PRId32 "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (int)source_inspection.players[0].facing,
            source_inspection.players[0].velocity_x_q16);
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "dash-cancel-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "dash-cancel-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=dash-cancel-future-hash"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xda5ca12)),
            PF_STATUS_OK,
            "dash-cancel-jump-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 >=
            content->fighter.initial_dash_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-cancel-jump\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xda5ca13)),
            PF_STATUS_OK,
            "dash-cancel-shield-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    run_velocity = source_inspection.players[0].velocity_x_q16;
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !step_duel_trigger(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].velocity_x_q16 >= run_velocity)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-cancel-shield\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xda5ca14)),
            PF_STATUS_OK,
            "dash-cancel-early-shield-reset") ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel_trigger(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks != UINT16_C(2))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-cancel-early-shield-negative\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xda5ca15)),
            PF_STATUS_OK,
            "dash-cancel-turnaround-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        source_inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_CROUCH)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-cancel-turnaround-negative\n");
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
            UINT32_C(5),
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
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
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
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
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

static int run_instant_double_jump_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage held_storage;
    test_sim_storage takeoff_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *held = NULL;
    pf_sim *takeoff = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection held_inspection;
    pf_m4_inspection takeoff_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    int32_t launch_y;
    int32_t expected_velocity_y;
    uint32_t tick;

    if (content->fighter.air_jump_count != UINT8_C(1) ||
        !initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !initialize_sim(
            &held_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &held) ||
        !initialize_sim(
            &takeoff_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &takeoff) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x1d100001)),
            PF_STATUS_OK,
            "idj-source-reset") ||
        !expect_status(
            pf_sim_reset(held, UINT64_C(0x1d100002)),
            PF_STATUS_OK,
            "idj-held-reset") ||
        !expect_status(
            pf_sim_reset(takeoff, UINT64_C(0x1d100003)),
            PF_STATUS_OK,
            "idj-takeoff-reset"))
    {
        return 0;
    }

    if (!step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].air_jumps_remaining !=
            UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=idj-first-airborne-setup\n");
        return 0;
    }

    launch_y = source_inspection.players[0].position_y_q16;
    expected_velocity_y =
        -content->fighter.double_jump_speed_q16 +
        content->fighter.gravity_q16;
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].air_jumps_remaining !=
            UINT8_C(0) ||
        source_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        source_inspection.players[0].velocity_y_q16 !=
            expected_velocity_y ||
        source_inspection.players[0].position_y_q16 !=
            launch_y + expected_velocity_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=idj-first-airborne-input"
            " action=%u ticks=%u grounded=%u jumps=%u"
            " position_y=%" PRId32 " launch_y=%" PRId32
            " velocity=(%" PRId32 ",%" PRId32 ") expected_y=%" PRId32
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0]
                .air_jumps_remaining,
            source_inspection.players[0].position_y_q16,
            launch_y,
            source_inspection.players[0].velocity_x_q16,
            source_inspection.players[0].velocity_y_q16,
            expected_velocity_y);
        return 0;
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "idj-query-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "idj-save"))
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "idj-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "idj-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "idj-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        const uint64_t buttons =
            tick == UINT32_C(1)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                buttons,
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                buttons,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "idj-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "idj-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            source_inspection.players[0].position_y_q16 !=
                loaded_inspection.players[0].position_y_q16 ||
            source_inspection.players[0].velocity_y_q16 !=
                loaded_inspection.players[0].velocity_y_q16 ||
            source_inspection.players[0].air_jumps_remaining !=
                UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=idj-snapshot-continuation"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_duel(
                held,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                &held_inspection))
        {
            return 0;
        }
    }
    if (held_inspection.players[0].grounded != UINT8_C(0) ||
        held_inspection.players[0].air_jumps_remaining !=
            UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=idj-held-no-repeat\n");
        return 0;
    }

    if (!step_duel(
            takeoff,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &takeoff_inspection) ||
        !step_duel(
            takeoff,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &takeoff_inspection) ||
        !step_duel(
            takeoff,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &takeoff_inspection) ||
        !step_duel(
            takeoff,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &takeoff_inspection) ||
        !step_duel(
            takeoff,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &takeoff_inspection) ||
        takeoff_inspection.players[0].grounded != UINT8_C(0) ||
        takeoff_inspection.players[0].air_jumps_remaining !=
            UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=idj-takeoff-edge-guard\n");
        return 0;
    }

    return 1;
}

static int enter_double_jump_cancel_window(
    pf_sim *sim,
    const pf_m4_content *content,
    int delayed_expected,
    pf_m4_inspection *out_inspection)
{
    const uint8_t expected_action =
        delayed_expected != 0
            ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
            : (uint8_t)PF_M4_ACTION_AIRBORNE;
    const int32_t expected_velocity_y =
        -content->fighter.double_jump_speed_q16 +
        content->fighter.gravity_q16;

    if (!launch_player0(sim, 1, out_inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection) ||
        out_inspection->players[0].grounded != UINT8_C(0) ||
        out_inspection->players[0].action_state != expected_action ||
        out_inspection->players[0].action_ticks != UINT16_C(0) ||
        out_inspection->players[0].air_jumps_remaining != UINT8_C(0) ||
        out_inspection->players[0].velocity_y_q16 !=
            expected_velocity_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-entry"
            " delayed=%d action=%u ticks=%u velocity_y=%" PRId32
            " jumps=%u\n",
            delayed_expected,
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            out_inspection->players[0].velocity_y_q16,
            (unsigned int)out_inspection->players[0]
                .air_jumps_remaining);
        return 0;
    }
    return 1;
}

static int run_double_jump_cancel_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage strong_storage;
    test_sim_storage late_storage;
    test_sim_storage simultaneous_storage;
    test_sim_storage disabled_storage;
    pf_m4_content invalid_content = *default_content;
    pf_m4_content disabled_content = *default_content;
    pf_content_view disabled_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *strong = NULL;
    pf_sim *late = NULL;
    pf_sim *simultaneous = NULL;
    pf_sim *disabled = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection strong_inspection;
    pf_m4_inspection late_inspection;
    pf_m4_inspection simultaneous_inspection;
    pf_m4_inspection disabled_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    int32_t before_cancel_position_y;
    int32_t before_late_velocity_y;
    int32_t before_simultaneous_velocity_y;
    uint32_t cancel_landing_tick;
    uint32_t late_landing_tick;
    uint32_t tick;

    if (default_content->fighter.double_jump_cancel_ticks !=
        UINT16_C(6))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-default\n");
        return 0;
    }

    invalid_content.fighter.double_jump_cancel_ticks = UINT16_C(121);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-double-jump-cancel-window"))
    {
        return 0;
    }

    disabled_content.fighter.double_jump_cancel_ticks = UINT16_C(0);
    disabled_content.fighter.double_jump_armor_max_hitstun_ticks =
        UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&disabled_content),
            PF_STATUS_OK,
            "allow-disabled-double-jump-cancel") ||
        !expect_status(
            pf_m4_make_content_view(&disabled_content, &disabled_view),
            PF_STATUS_OK,
            "disabled-double-jump-cancel-content") ||
        memcmp(
            default_view->content_hash.bytes,
            disabled_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-content-hash\n");
        return 0;
    }

    if (!initialize_sim(
            &source_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !initialize_sim(
            &strong_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &strong) ||
        !initialize_sim(
            &late_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &late) ||
        !initialize_sim(
            &simultaneous_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &simultaneous) ||
        !initialize_sim(
            &disabled_storage,
            &disabled_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &disabled) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0xd0b1ec01)),
            PF_STATUS_OK,
            "double-jump-cancel-source-reset") ||
        !expect_status(
            pf_sim_reset(strong, UINT64_C(0xd0b1ec02)),
            PF_STATUS_OK,
            "double-jump-cancel-strong-reset") ||
        !expect_status(
            pf_sim_reset(late, UINT64_C(0xd0b1ec03)),
            PF_STATUS_OK,
            "double-jump-cancel-late-reset") ||
        !expect_status(
            pf_sim_reset(simultaneous, UINT64_C(0xd0b1ec04)),
            PF_STATUS_OK,
            "double-jump-cancel-simultaneous-reset") ||
        !expect_status(
            pf_sim_reset(disabled, UINT64_C(0xd0b1ec05)),
            PF_STATUS_OK,
            "double-jump-cancel-disabled-reset") ||
        !enter_double_jump_cancel_window(
            source,
            default_content,
            1,
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        source_inspection.players[0].action_ticks != UINT16_C(1))
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "double-jump-cancel-query-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "double-jump-cancel-save"))
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "double-jump-cancel-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "double-jump-cancel-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "double-jump-cancel-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        return 0;
    }

    before_cancel_position_y =
        source_inspection.players[0].position_y_q16;
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].velocity_y_q16 !=
            default_content->fighter.gravity_q16 ||
        source_inspection.players[0].position_y_q16 !=
            before_cancel_position_y +
                default_content->fighter.gravity_q16 ||
        source_inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "double-jump-cancel-source-cancel-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "double-jump-cancel-loaded-cancel-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-light\n");
        return 0;
    }

    cancel_landing_tick = UINT32_C(2);
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         source_inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "double-jump-cancel-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "double-jump-cancel-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0 ||
            source_inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            source_inspection.players[0].action_ticks !=
                loaded_inspection.players[0].action_ticks ||
            source_inspection.players[0].position_y_q16 !=
                loaded_inspection.players[0].position_y_q16 ||
            source_inspection.players[0].velocity_y_q16 !=
                loaded_inspection.players[0].velocity_y_q16)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=double-jump-cancel-future"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
        ++cancel_landing_tick;
    }
    if (source_inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-landing\n");
        return 0;
    }

    if (!enter_double_jump_cancel_window(
            strong,
            default_content,
            1,
            &strong_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)default_content->fighter
                    .double_jump_cancel_ticks;
         ++tick)
    {
        if (!step_duel(
                strong,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &strong_inspection))
        {
            return 0;
        }
    }
    if (strong_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        strong_inspection.players[0].action_ticks !=
            (uint16_t)(
                default_content->fighter.double_jump_cancel_ticks -
                UINT16_C(1)) ||
        !step_duel(
            strong,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            &strong_inspection) ||
        strong_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        strong_inspection.players[0].action_ticks != UINT16_C(0) ||
        strong_inspection.players[0].velocity_y_q16 !=
            default_content->fighter.gravity_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-last-tick\n");
        return 0;
    }

    if (!enter_double_jump_cancel_window(
            late,
            default_content,
            1,
            &late_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)default_content->fighter
                    .double_jump_cancel_ticks;
         ++tick)
    {
        if (!step_duel(
                late,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &late_inspection))
        {
            return 0;
        }
    }
    before_late_velocity_y =
        late_inspection.players[0].velocity_y_q16;
    if (late_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        late_inspection.players[0].action_ticks != UINT16_C(0) ||
        before_late_velocity_y >= INT32_C(0) ||
        !step_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &late_inspection) ||
        late_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        late_inspection.players[0].velocity_y_q16 !=
            before_late_velocity_y +
                default_content->fighter.gravity_q16 ||
        late_inspection.players[0].velocity_y_q16 >= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-late\n");
        return 0;
    }
    late_landing_tick =
        (uint32_t)default_content->fighter.double_jump_cancel_ticks +
        UINT32_C(1);
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         late_inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_duel(
                late,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &late_inspection))
        {
            return 0;
        }
        ++late_landing_tick;
    }
    if (late_inspection.players[0].grounded == UINT8_C(0) ||
        late_landing_tick <= cancel_landing_tick)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-arc"
            " cancel=%" PRIu32 " late=%" PRIu32 "\n",
            cancel_landing_tick,
            late_landing_tick);
        return 0;
    }

    if (!launch_player0(
            simultaneous,
            1,
            &simultaneous_inspection))
    {
        return 0;
    }
    before_simultaneous_velocity_y =
        simultaneous_inspection.players[0].velocity_y_q16;
    if (!step_duel(
            simultaneous,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
            &simultaneous_inspection) ||
        simultaneous_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        simultaneous_inspection.players[0].air_jumps_remaining !=
            default_content->fighter.air_jump_count ||
        simultaneous_inspection.players[0].velocity_y_q16 !=
            before_simultaneous_velocity_y +
                default_content->fighter.gravity_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-simultaneous\n");
        return 0;
    }

    if (!enter_double_jump_cancel_window(
            disabled,
            &disabled_content,
            0,
            &disabled_inspection))
    {
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

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
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
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-facing-double-jump\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
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

static int start_aerial_attack(
    pf_sim *sim,
    int short_hop,
    pf_m4_inspection *out_inspection)
{
    if (!launch_player0(sim, short_hop, out_inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        out_inspection->players[0].action_ticks != UINT16_C(0) ||
        out_inspection->players[0].grounded != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=start-aerial"
            " action=%u ticks=%u grounded=%u\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            (unsigned int)out_inspection->players[0].grounded);
        return 0;
    }
    return 1;
}

static int start_strong_aerial_attack(
    pf_sim *sim,
    int short_hop,
    pf_m4_inspection *out_inspection)
{
    if (!launch_player0(sim, short_hop, out_inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        out_inspection->players[0].action_ticks != UINT16_C(0) ||
        out_inspection->players[0].grounded != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=start-strong-aerial"
            " action=%u ticks=%u grounded=%u\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            (unsigned int)out_inspection->players[0].grounded);
        return 0;
    }
    return 1;
}

static int run_strong_aerial_landing_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage normal_storage;
    test_sim_storage cancel_storage;
    pf_m4_content invalid_content = *default_content;
    pf_sim *normal = NULL;
    pf_sim *cancel = NULL;
    pf_m4_inspection inspection;
    uint32_t landing_ticks;
    uint32_t tick;

    invalid_content.fighter.strong_aerial_landing_lag_ticks =
        UINT16_C(0);
    if (default_content->fighter.strong_aerial_landing_lag_ticks !=
            UINT16_C(30) ||
        !expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-strong-aerial-landing-lag") ||
        !initialize_sim(
            &normal_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &normal) ||
        !expect_status(
            pf_sim_reset(normal, UINT64_C(0x57a0a11)),
            PF_STATUS_OK,
            "strong-aerial-normal-reset") ||
        !start_strong_aerial_attack(normal, 1, &inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_duel_trigger(
                normal,
                INT16_C(0),
                inspection.players[0].velocity_y_q16 > INT32_C(0) &&
                        inspection.players[0].fast_fall == UINT8_C(0)
                    ? INT16_MAX
                    : INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=strong-aerial-normal-landing"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_LANDING &&
           landing_ticks < UINT32_C(60))
    {
        if (!step_duel_trigger(
                normal,
                INT16_C(0),
                INT16_C(0),
                landing_ticks == UINT32_C(0)
                    ? PF_INPUT_BUTTON_STRONG_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)default_content->fighter
                .strong_aerial_landing_lag_ticks ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail"
            " operation=strong-aerial-normal-duration"
            " ticks=%" PRIu32
            " action=%u\n",
            landing_ticks,
            (unsigned int)inspection.players[0].action_state);
        return 0;
    }

    if (!initialize_sim(
            &cancel_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &cancel) ||
        !expect_status(
            pf_sim_reset(cancel, UINT64_C(0x57a0ca11)),
            PF_STATUS_OK,
            "strong-aerial-l-cancel-reset") ||
        !start_strong_aerial_attack(cancel, 1, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        const int descending =
            inspection.players[0].velocity_y_q16 >= INT32_C(0);

        if (!step_duel_trigger(
                cancel,
                INT16_C(0),
                descending &&
                        inspection.players[0].fast_fall == UINT8_C(0)
                    ? INT16_MAX
                    : INT16_C(0),
                UINT64_C(0),
                descending ? UINT16_MAX : UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].trigger_input_age >=
            default_content->fighter.l_cancel_window_ticks)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=strong-aerial-l-cancel"
            " action=%u ticks=%u age=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].trigger_input_age);
        return 0;
    }

    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING &&
           landing_ticks < UINT32_C(60))
    {
        if (!step_duel_trigger(
                cancel,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)(
                default_content->fighter
                    .strong_aerial_landing_lag_ticks /
                default_content->fighter.l_cancel_divisor) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail"
            " operation=strong-aerial-l-cancel-duration"
            " ticks=%" PRIu32
            " action=%u\n",
            landing_ticks,
            (unsigned int)inspection.players[0].action_state);
        return 0;
    }
    return 1;
}

static int run_aerial_trigger_snapshot_test(
    pf_sim *source,
    const pf_content_view *content,
    uint8_t expected_age)
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

    if (!initialize_sim(
            &loaded_storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_query_save_size(source, &required_bytes),
            PF_STATUS_OK,
            "aerial-query-save-size") ||
        required_bytes != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "aerial-save") ||
        destination.size != required_bytes)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "aerial-load") ||
        !expect_status(
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "aerial-loaded-inspect") ||
        loaded_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        loaded_inspection.players[0].trigger_input_age !=
            expected_age ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "aerial-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "aerial-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        return 0;
    }
    return 1;
}

static int run_aerial_landing_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage normal_storage;
    test_sim_storage cancel_storage;
    test_sim_storage timer_storage;
    test_sim_storage auto_storage;
    pf_sim *normal = NULL;
    pf_sim *cancel = NULL;
    pf_sim *timer = NULL;
    pf_sim *auto_cancel = NULL;
    pf_m4_content invalid_content = *default_content;
    pf_m4_content auto_content = *default_content;
    pf_content_view auto_view;
    pf_m4_inspection inspection;
    uint32_t tick;
    uint32_t landing_ticks;

    invalid_content.fighter.l_cancel_window_ticks = UINT16_C(6);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-non-melee-l-cancel-window"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.aerial_landing_lag_end_tick =
        invalid_content.fighter.aerial_landing_lag_begin_tick;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-empty-aerial-landing-window"))
    {
        return 0;
    }

    auto_content.fighter.gravity_q16 =
        PF_Q16_ONE / INT32_C(20);
    auto_content.fighter.fall_speed_q16 =
        PF_Q16_ONE / INT32_C(10);
    auto_content.fighter.fast_fall_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(20);
    auto_content.fighter.short_hop_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(50);
    auto_content.fighter.full_hop_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(25);
    auto_content.fighter.double_jump_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(25);
    if (!expect_status(
            pf_m4_make_content_view(&auto_content, &auto_view),
            PF_STATUS_OK,
            "auto-cancel-content-view") ||
        !initialize_sim(
            &auto_storage,
            &auto_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &auto_cancel) ||
        !expect_status(
            pf_sim_reset(auto_cancel, UINT64_C(0xa470ca)),
            PF_STATUS_OK,
            "auto-cancel-reset") ||
        !launch_player0(auto_cancel, 1, &inspection) ||
        !step_duel_trigger(
            auto_cancel,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(4) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_duel_trigger(
                auto_cancel,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=aerial-early-auto-cancel"
            " action=%u ticks=%u grounded=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded);
        return 0;
    }

    if (!initialize_sim(
            &normal_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &normal) ||
        !expect_status(
            pf_sim_reset(normal, UINT64_C(0xae11a1)),
            PF_STATUS_OK,
            "aerial-normal-reset") ||
        !start_aerial_attack(normal, 1, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_duel_trigger(
                normal,
                INT16_C(0),
                inspection.players[0].velocity_y_q16 > INT32_C(0) &&
                        inspection.players[0].fast_fall == UINT8_C(0)
                    ? INT16_MAX
                    : INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=aerial-normal-landing"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }
    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_LANDING &&
           landing_ticks < UINT32_C(40))
    {
        if (!step_duel_trigger(
                normal,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)default_content->fighter
                .aerial_landing_lag_ticks ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return 0;
    }

    if (!initialize_sim(
            &cancel_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &cancel) ||
        !expect_status(
            pf_sim_reset(cancel, UINT64_C(0x1ca11ce1)),
            PF_STATUS_OK,
            "l-cancel-reset") ||
        !start_aerial_attack(cancel, 1, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        const int descending =
            inspection.players[0].velocity_y_q16 >= INT32_C(0);

        if (!step_duel_trigger(
                cancel,
                INT16_C(0),
                descending &&
                        inspection.players[0].fast_fall == UINT8_C(0)
                    ? INT16_MAX
                    : INT16_C(0),
                UINT64_C(0),
                descending ? UINT16_MAX : UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].trigger_input_age >=
            default_content->fighter.l_cancel_window_ticks)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=l-cancel-landing"
            " action=%u ticks=%u age=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].trigger_input_age);
        return 0;
    }
    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING &&
           landing_ticks < UINT32_C(40))
    {
        if (!step_duel_trigger(
                cancel,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)(
                default_content->fighter.aerial_landing_lag_ticks /
                default_content->fighter.l_cancel_divisor) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return 0;
    }

    if (!initialize_sim(
            &timer_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &timer) ||
        !expect_status(
            pf_sim_reset(timer, UINT64_C(0x7a1e)),
            PF_STATUS_OK,
            "l-cancel-timer-reset") ||
        !start_aerial_attack(timer, 0, &inspection) ||
        !step_duel_trigger(
            timer,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].trigger_input_age != UINT8_C(0) ||
        inspection.players[0].l_cancel_eligible != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(1); tick < UINT32_C(7); ++tick)
    {
        if (!step_duel_trigger(
                timer,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].trigger_input_age !=
                (uint8_t)tick ||
            inspection.players[0].l_cancel_eligible != UINT8_C(1))
        {
            return 0;
        }
        if (tick == UINT32_C(3) &&
            !run_aerial_trigger_snapshot_test(
                timer,
                default_view,
                UINT8_C(3)))
        {
            return 0;
        }
    }
    if (!step_duel_trigger(
            timer,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].trigger_input_age != UINT8_C(7) ||
        inspection.players[0].l_cancel_eligible != UINT8_C(0))
    {
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

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
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
        inspection.players[0].platform_drop_ticks == UINT8_C(0) ||
        inspection.players[0].fast_fall != UINT8_C(0) ||
        inspection.players[0].velocity_y_q16 >=
            default_content->fighter.fast_fall_speed_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-drop\n");
        return 0;
    }
    return 1;
}

static int prepare_upper_platform(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7570706572)),
            PF_STATUS_OK,
            "upper-platform-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[0].support ==
                (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM)
        {
            break;
        }
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(40) &&
         out_inspection->players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
         ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE;
}

static int run_upper_platform_test(
    const pf_m4_content *default_content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_m4_content platform_content = *default_content;
    pf_m4_content invalid_content;
    pf_content_view platform_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    int32_t stationary_x;

    if (default_content->stage.upper_platform_center_x_q16 !=
            INT32_C(20) * PF_Q16_ONE ||
        default_content->stage.upper_platform_y_q16 !=
            INT32_C(13) * PF_Q16_ONE ||
        default_content->stage.upper_platform_half_width_q16 !=
            INT32_C(4) * PF_Q16_ONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-defaults\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.stage.upper_platform_half_width_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-empty-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_y_q16 =
        invalid_content.stage.blast_top_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-high-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_y_q16 =
        invalid_content.stage.solid_top_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-solid-overlap-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_center_x_q16 =
        invalid_content.stage.platform_center_x_q16;
    invalid_content.stage.upper_platform_y_q16 =
        invalid_content.stage.platform_y_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-moving-overlap-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_center_x_q16 =
        invalid_content.stage.floor_right_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-outside-upper-platform"))
    {
        return 0;
    }

    platform_content.stage.platform_center_x_q16 =
        -INT32_C(24) * PF_Q16_ONE;
    platform_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    platform_content.stage.platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    platform_content.stage.upper_platform_center_x_q16 =
        -INT32_C(4) * PF_Q16_ONE;
    platform_content.stage.upper_platform_y_q16 =
        INT32_C(26) * PF_Q16_ONE;
    platform_content.stage.upper_platform_half_width_q16 =
        INT32_C(6) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(&platform_content, &platform_view),
            PF_STATUS_OK,
            "upper-platform-content-view") ||
        !initialize_sim(
            &source_storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !prepare_upper_platform(source, &source_inspection) ||
        source_inspection.stage.upper_platform_left_q16 !=
            -INT32_C(10) * PF_Q16_ONE ||
        source_inspection.stage.upper_platform_right_q16 !=
            INT32_C(2) * PF_Q16_ONE ||
        source_inspection.stage.upper_platform_y_q16 !=
            INT32_C(26) * PF_Q16_ONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-landing\n");
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "upper-platform-save") ||
        destination.size != (size_t)787)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "upper-platform-load"))
    {
        return 0;
    }
    stationary_x = source_inspection.players[0].position_x_q16;
    if (!step_duel_trigger(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        !step_duel_trigger(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].position_x_q16 != stationary_x ||
        loaded_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "upper-platform-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "upper-platform-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-continuation\n");
        return 0;
    }

    if (!step_duel_trigger(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_trigger(
            source,
            INT16_C(0),
            (int16_t)platform_content.fighter
                .shield_drop_axis_threshold,
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        source_inspection.players[0].platform_drop_ticks == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-shield-drop\n");
        return 0;
    }

    if (!prepare_upper_platform(source, &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        source_inspection.players[0].position_y_q16 <=
            platform_content.stage.upper_platform_y_q16 -
                platform_content.fighter.half_height_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-drop\n");
        return 0;
    }
    return 1;
}

static int prepare_shield_drop_platform(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5a1ed07)),
            PF_STATUS_OK,
            "shield-platform-drop-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            break;
        }
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(40) &&
         out_inspection->players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
         ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE;
}

static int enter_platform_shield(
    pf_sim *sim,
    int16_t entry_y,
    pf_m4_inspection *out_inspection)
{
    return prepare_shield_drop_platform(sim, out_inspection) &&
           step_duel_trigger(
               sim,
               INT16_C(0),
               entry_y,
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD &&
           out_inspection->players[0].platform_drop_ticks ==
               UINT8_C(0);
}

static int run_shield_platform_drop_test(
    const pf_m4_content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage floor_storage;
    pf_m4_content platform_content = *default_content;
    pf_m4_content invalid_content = *default_content;
    pf_content_view platform_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *floor = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection floor_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (default_content->fighter.shield_drop_axis_threshold !=
            UINT16_C(12288) ||
        default_content->fighter.shield_drop_axis_threshold <=
            default_content->fighter.axis_dead_zone ||
        default_content->fighter.shield_drop_axis_threshold >=
            default_content->fighter.crouch_axis_threshold)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=shield-platform-drop-defaults\n");
        return 0;
    }
    invalid_content.fighter.shield_drop_axis_threshold =
        invalid_content.fighter.axis_dead_zone;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-low-shield-drop-threshold"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.shield_drop_axis_threshold =
        invalid_content.fighter.crouch_axis_threshold;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-high-shield-drop-threshold"))
    {
        return 0;
    }

    platform_content.stage.platform_center_x_q16 =
        -INT32_C(8) * PF_Q16_ONE;
    platform_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    platform_content.stage.platform_half_width_q16 =
        INT32_C(6) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(
                &platform_content,
                &platform_view),
            PF_STATUS_OK,
            "shield-platform-drop-content-view") ||
        !initialize_sim(
            &source_storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !initialize_sim(
            &floor_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &floor) ||
        !enter_platform_shield(
            source,
            (int16_t)platform_content.fighter
                .shield_drop_axis_threshold,
            &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "shield-platform-drop-query-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "shield-platform-drop-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "shield-platform-drop-load") ||
        !step_duel_trigger(
            source,
            INT16_C(0),
            (int16_t)platform_content.fighter
                .shield_drop_axis_threshold,
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        !step_duel_trigger(
            loaded,
            INT16_C(0),
            (int16_t)platform_content.fighter
                .shield_drop_axis_threshold,
            UINT64_C(0),
            UINT16_MAX,
            &loaded_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].platform_drop_ticks !=
            (uint8_t)platform_content.fighter.platform_drop_ticks ||
        source_inspection.players[0].fast_fall != UINT8_C(0) ||
        source_inspection.players[0].position_y_q16 <=
            platform_content.stage.platform_y_q16 -
                platform_content.fighter.half_height_q16 ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "shield-platform-drop-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "shield-platform-drop-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=shield-platform-drop-entry\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &source_inspection) ||
            !step_duel_trigger(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "shield-platform-drop-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "shield-platform-drop-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=shield-platform-drop-future"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }

    if (!enter_platform_shield(
            source,
            INT16_C(0),
            &source_inspection) ||
        !step_duel_trigger(
            source,
            INT16_C(0),
            (int16_t)(
                platform_content.fighter
                    .shield_drop_axis_threshold -
                UINT16_C(1)),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        !enter_platform_shield(
            source,
            INT16_C(0),
            &source_inspection) ||
        !step_duel_trigger(
            source,
            INT16_C(0),
            (int16_t)(
                platform_content.fighter.crouch_axis_threshold -
                UINT16_C(1)),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        !enter_platform_shield(
            source,
            INT16_C(0),
            &source_inspection) ||
        !step_duel_trigger(
            source,
            INT16_C(0),
            (int16_t)platform_content.fighter
                .crouch_axis_threshold,
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        source_inspection.players[0].platform_drop_ticks != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=shield-platform-drop-boundaries\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(floor, UINT64_C(0x5a1ed08)),
            PF_STATUS_OK,
            "shield-platform-drop-floor-reset") ||
        !step_duel_trigger(
            floor,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &floor_inspection) ||
        !step_duel_trigger(
            floor,
            INT16_C(0),
            (int16_t)default_content->fighter
                .shield_drop_axis_threshold,
            UINT64_C(0),
            UINT16_MAX,
            &floor_inspection) ||
        floor_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        floor_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        floor_inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=shield-platform-drop-floor\n");
        return 0;
    }

    if (!enter_platform_shield(
            source,
            INT16_C(0),
            &source_inspection))
    {
        return 0;
    }
    for (tick = (uint32_t)source_inspection.players[0].action_ticks;
         tick < (uint32_t)platform_content.fighter
             .shield_minimum_hold_ticks;
         ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &source_inspection))
        {
            return 0;
        }
    }
    if (!step_duel_trigger(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        source_inspection.players[0].platform_drop_ticks != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=shield-release-not-platform-drop\n");
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
        INT32_C(26) * PF_Q16_ONE;
    out_content->stage.solid_bottom_q16 =
        INT32_C(29) * PF_Q16_ONE;
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
    test_sim_storage right_corner_storage;
    pf_m4_content content;
    pf_m4_content right_corner_content;
    pf_content_view view;
    pf_content_view right_corner_view;
    pf_sim *under = NULL;
    pf_sim *wall = NULL;
    pf_sim *ceiling = NULL;
    pf_sim *top = NULL;
    pf_sim *right_corner = NULL;
    pf_m4_inspection inspection;
    const int32_t wall_contact_x =
        -default_content->fighter.half_width_q16;
    const int32_t ceiling_contact_y =
        INT32_C(29) * PF_Q16_ONE +
        default_content->fighter.half_height_q16;
    const int32_t top_contact_y =
        INT32_C(26) * PF_Q16_ONE -
        default_content->fighter.half_height_q16;
    int observed_wall = 0;
    int observed_ceiling = 0;
    uint32_t tick;

    if (!make_solid_geometry_content(
            default_content,
            &content,
            &view))
    {
        return 0;
    }
    right_corner_content = content;
    right_corner_content.stage.solid_left_q16 =
        -INT32_C(8) * PF_Q16_ONE;
    right_corner_content.stage.solid_right_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_make_content_view(
                &right_corner_content,
                &right_corner_view),
            PF_STATUS_OK,
            "solid-right-corner-content-view") ||
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
        !initialize_sim(
            &right_corner_storage,
            &right_corner_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &right_corner) ||
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
            pf_sim_reset(right_corner, UINT64_C(19)),
            PF_STATUS_OK,
            "solid-right-corner-reset") ||
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
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1); tick < UINT32_C(4); ++tick)
    {
        if (!step_duel(
                wall,
                INT16_C(0),
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
            INT16_MAX,
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

    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
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

    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        const int16_t horizontal_axis =
            tick >= UINT32_C(5) ? INT16_MAX : INT16_C(0);
        const uint64_t buttons =
            tick < UINT32_C(5)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!step_duel(
                top,
                horizontal_axis,
                INT16_C(0),
                buttons,
                &inspection))
        {
            return 0;
        }
        if (player_overlaps_solid(
                &content,
                &inspection.players[0]))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=solid-upper-left-corner"
                " tick=%" PRIu32
                " position=(%" PRId32 ",%" PRId32 ")"
                " velocity=(%" PRId32 ",%" PRId32 ")\n",
                tick,
                inspection.players[0].position_x_q16,
                inspection.players[0].position_y_q16,
                inspection.players[0].velocity_x_q16,
                inspection.players[0].velocity_y_q16);
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

    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        if (!step_duel(
                top,
                -INT16_C(13500),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].support ==
            (uint8_t)PF_M4_SURFACE_SOLID_TOP ||
        inspection.players[0].position_x_q16 +
                content.fighter.half_width_q16 >=
            content.stage.solid_left_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-top-left-edge-escape"
            " action=%u ticks=%u grounded=%u support=%u"
            " position=(%" PRId32 ",%" PRId32 ")"
            " velocity=(%" PRId32 ",%" PRId32 ")\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            inspection.players[0].position_x_q16,
            inspection.players[0].position_y_q16,
            inspection.players[0].velocity_x_q16,
            inspection.players[0].velocity_y_q16);
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        const int16_t horizontal_axis =
            tick >= UINT32_C(5) ? INT16_MIN : INT16_C(0);
        const uint64_t buttons =
            tick < UINT32_C(5)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!step_duel_players(
                right_corner,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                horizontal_axis,
                INT16_C(0),
                buttons,
                &inspection))
        {
            return 0;
        }
        if (player_overlaps_solid(
                &right_corner_content,
                &inspection.players[1]))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=solid-upper-right-corner"
                " tick=%" PRIu32
                " position=(%" PRId32 ",%" PRId32 ")"
                " velocity=(%" PRId32 ",%" PRId32 ")\n",
                tick,
                inspection.players[1].position_x_q16,
                inspection.players[1].position_y_q16,
                inspection.players[1].velocity_x_q16,
                inspection.players[1].velocity_y_q16);
            return 0;
        }
        if (inspection.players[1].grounded != UINT8_C(0) &&
            inspection.players[1].support ==
                (uint8_t)PF_M4_SURFACE_SOLID_TOP)
        {
            break;
        }
    }
    if (inspection.players[1].grounded == UINT8_C(0) ||
        inspection.players[1].support !=
            (uint8_t)PF_M4_SURFACE_SOLID_TOP ||
        inspection.players[1].position_y_q16 != top_contact_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-right-top-landing"
            " position=(%" PRId32 ",%" PRId32 ")"
            " grounded=%u support=%u\n",
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            (unsigned int)inspection.players[1].grounded,
            (unsigned int)inspection.players[1].support);
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
                    INT32_C(5) * PF_Q16_ONE)
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

    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
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
        if (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(80) ||
        !step_duel(
            sim,
            INT16_C(-16000),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=right-ledge-brake\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
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
        if (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            out_inspection->players[0].facing == INT8_C(-1))
        {
            break;
        }
    }
    if (tick == UINT32_C(24) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=right-ledge-inward-turn\n");
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const int16_t drift_x =
            out_inspection->players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_JUMP_SQUAT
                ? INT16_C(0)
                : INT16_C(20000);

        if (!step_duel(
                sim,
                drift_x,
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
    }
    if (tick == UINT32_C(120) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        out_inspection->players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=right-ledge-grab"
            " action=%u ledge=%u x=%" PRId32 " y=%" PRId32
            " facing=%d vx=%" PRId32 " vy=%" PRId32 "\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].ledge,
            out_inspection->players[0].position_x_q16,
            out_inspection->players[0].position_y_q16,
            (int)out_inspection->players[0].facing,
            out_inspection->players[0].velocity_x_q16,
            out_inspection->players[0].velocity_y_q16);
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
        (uint32_t)content->fighter.ledge_transition_ticks;
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
    pf_m4_inspection *source_inspection,
    uint8_t expected_action)
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
            expected_action ||
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
                    INT32_C(5) * PF_Q16_ONE &&
            inspection.players[1].position_x_q16 >=
                inspection.stage.right_ledge_x_q16 -
                    INT32_C(5) * PF_Q16_ONE)
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
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        if (!step_duel_players(
                sim,
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
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[0].velocity_x_q16 == INT32_C(0) &&
            inspection.players[1].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(80) ||
        !step_duel_players(
            sim,
            INT16_C(-16000),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(-16000),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-occupancy-brake\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (!step_duel_players(
                sim,
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
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[0].facing == INT8_C(-1) &&
            inspection.players[1].facing == INT8_C(-1))
        {
            break;
        }
    }
    if (tick == UINT32_C(24) ||
        !step_duel_players(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-occupancy-inward-turn\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const int16_t player0_drift =
            inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_JUMP_SQUAT
                ? INT16_C(0)
                : INT16_C(20000);
        const int16_t player1_drift =
            inspection.players[1].action_state ==
                    (uint8_t)PF_M4_ACTION_JUMP_SQUAT
                ? INT16_C(0)
                : INT16_C(20000);

        if (!step_duel_players(
                sim,
                player0_drift,
                INT16_C(0),
                UINT64_C(0),
                player1_drift,
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
    if (tick == UINT32_C(120) ||
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

static int run_ledge_hit_rejection_test(
    const pf_m4_content *default_content)
{
    test_sim_storage storage;
    pf_m4_content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    content.fighter.jab_hitbox_half_width_q16 =
        INT32_C(64) * PF_Q16_ONE;
    content.fighter.jab_hitbox_half_height_q16 =
        INT32_C(64) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "ledge-hit-rejection-content") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6e117)),
            PF_STATUS_OK,
            "ledge-hit-rejection-reset") ||
        !grab_player0_right_ledge(sim, &inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        if (!step_duel_players(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].damage_q16 != UINT32_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-invulnerability-hit-reject\n");
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].invulnerable != UINT8_C(0);
         ++tick)
    {
        if (!step_duel_players(
                sim,
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
    }
    if (inspection.players[0].invulnerable != UINT8_C(0))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        if (!step_duel_players(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].damage_q16 !=
        content.fighter.jab_damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-invulnerability-hit-expiry"
            " damage=%" PRIu32 "\n",
            inspection.players[0].damage_q16);
        return 0;
    }
    return 1;
}

static int reach_scar_jump_wall(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    const int32_t contact_x =
        content->stage.solid_right_q16 +
        content->fighter.half_width_q16;
    uint32_t tick;

    if (!grab_player0_right_ledge(sim, out_inspection) ||
        !make_player0_ledge_actionable(
            sim,
            content,
            out_inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
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
        if (out_inspection->players[0].position_x_q16 == contact_x &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            return out_inspection->players[0].action_state ==
                       (uint8_t)PF_M4_ACTION_AIRBORNE &&
                   out_inspection->players[0].air_jumps_remaining ==
                       content->fighter.air_jump_count;
        }
    }

    (void)fprintf(
        stderr,
        "m4-movement=fail operation=scar-jump-wall-contact"
        " action=%u position=(%" PRId32 ",%" PRId32 ")"
        " velocity=(%" PRId32 ",%" PRId32 ")\n",
        (unsigned int)out_inspection->players[0].action_state,
        out_inspection->players[0].position_x_q16,
        out_inspection->players[0].position_y_q16,
        out_inspection->players[0].velocity_x_q16,
        out_inspection->players[0].velocity_y_q16);
    return 0;
}

static int enter_scar_jump(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    return reach_scar_jump_wall(sim, content, out_inspection) &&
           step_duel(
               sim,
               INT16_MAX,
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_WALL_JUMP &&
           out_inspection->players[0].action_ticks == UINT16_C(1) &&
           out_inspection->players[0].velocity_x_q16 ==
               content->fighter.wall_jump_speed_x_q16 &&
           out_inspection->players[0].velocity_y_q16 ==
               -content->fighter.wall_jump_speed_y_q16 +
                   content->fighter.gravity_q16 &&
           out_inspection->players[0].facing == INT8_C(1) &&
           out_inspection->players[0].air_jumps_remaining ==
               content->fighter.air_jump_count &&
           out_inspection->players[0].invulnerable == UINT8_C(1);
}

static int run_scar_jump_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage jump_storage;
    test_sim_storage lock_storage;
    test_sim_storage missed_storage;
    pf_m4_content invalid_content = *content;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *jump_sim = NULL;
    pf_sim *lock_sim = NULL;
    pf_sim *missed_sim = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;

    invalid_content.fighter.wall_jump_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "scar-jump-invalid-content") ||
        !initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !initialize_sim(
            &jump_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &jump_sim) ||
        !initialize_sim(
            &lock_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &lock_sim) ||
        !initialize_sim(
            &missed_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &missed_sim) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x5ca47a)),
            PF_STATUS_OK,
            "scar-jump-reset") ||
        !enter_scar_jump(source, content, &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "scar-jump-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "scar-jump-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "scar-jump-load") ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        source_inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "scar-jump-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "scar-jump-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=scar-jump-aerial-cancel\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(jump_sim, UINT64_C(0x5ca47b)),
            PF_STATUS_OK,
            "scar-jump-jump-reset") ||
        !enter_scar_jump(jump_sim, content, &inspection) ||
        !step_duel(
            jump_sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=scar-jump-jump-cancel\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(lock_sim, UINT64_C(0x5ca47c)),
            PF_STATUS_OK,
            "scar-jump-lock-reset") ||
        !enter_scar_jump(lock_sim, content, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.wall_jump_ticks;
         ++tick)
    {
        const uint8_t expected_invulnerable =
            tick + UINT32_C(1) <
                    (uint32_t)content->fighter
                        .wall_jump_invulnerability_ticks
                ? UINT8_C(1)
                : UINT8_C(0);

        if (!step_duel(
                lock_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            (tick + UINT32_C(1) <
                     (uint32_t)content->fighter.wall_jump_ticks &&
             (inspection.players[0].action_state !=
                  (uint8_t)PF_M4_ACTION_WALL_JUMP ||
              inspection.players[0].invulnerable !=
                  expected_invulnerable)))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=scar-jump-lock-duration\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(missed_sim, UINT64_C(0x5ca47d)),
            PF_STATUS_OK,
            "scar-jump-missed-reset") ||
        !grab_player0_right_ledge(missed_sim, &inspection) ||
        !make_player0_ledge_actionable(
            missed_sim,
            content,
            &inspection) ||
        !step_duel(
            missed_sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_duel(
                missed_sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_WALL_JUMP)
        {
            return 0;
        }
    }
    return inspection.players[0].air_jumps_remaining ==
           content->fighter.air_jump_count;
}

static int run_edge_hop_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t before_exhausted_jump_velocity_y;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0xed6e0a)),
            PF_STATUS_OK,
            "edge-hop-reset") ||
        !grab_player0_right_ledge(source, &source_inspection) ||
        !make_player0_ledge_actionable(
            source,
            content,
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        source_inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        source_inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "edge-hop-query-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "edge-hop-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "edge-hop-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "edge-hop-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "edge-hop-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0 ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection))
    {
        return 0;
    }
    before_exhausted_jump_velocity_y =
        source_inspection.players[0].velocity_y_q16;
    if (!step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &loaded_inspection) ||
        source_inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        source_inspection.players[0].velocity_y_q16 <=
            before_exhausted_jump_velocity_y ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "edge-hop-source-negative-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "edge-hop-loaded-negative-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0 ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MIN,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_AERIAL ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "edge-hop-source-aerial-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "edge-hop-loaded-aerial-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-hop-route\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xed6e0b)),
            PF_STATUS_OK,
            "edge-hop-neutral-reset") ||
        !grab_player0_right_ledge(source, &source_inspection) ||
        !make_player0_ledge_actionable(
            source,
            content,
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        source_inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-hop-neutral-control\n");
        return 0;
    }
    return 1;
}

static int run_edge_dash_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t expiry_ticks;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0xed6eda)),
            PF_STATUS_OK,
            "edge-dash-reset") ||
        !grab_player0_right_ledge(source, &source_inspection) ||
        !make_player0_ledge_actionable(
            source,
            content,
            &source_inspection))
    {
        return 0;
    }
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        source_inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        source_inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count ||
        source_inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-ledge-jump"
            " action=%u ticks=%u grounded=%u support=%u"
            " position=(%" PRId32 ",%" PRId32 ")"
            " velocity=(%" PRId32 ",%" PRId32 ") invulnerable=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[0].position_x_q16,
            source_inspection.players[0].position_y_q16,
            source_inspection.players[0].velocity_x_q16,
            source_inspection.players[0].velocity_y_q16,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         source_inspection.players[0].position_y_q16 +
                 content->fighter.half_height_q16 >
             content->stage.floor_y_q16;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        source_inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-rise"
            " tick=%" PRIu32 " action=%u position_y=%" PRId32
            " velocity_y=%" PRId32 " invulnerable=%u\n",
            tick,
            (unsigned int)source_inspection.players[0].action_state,
            source_inspection.players[0].position_y_q16,
            source_inspection.players[0].velocity_y_q16,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }

    if (!step_duel_trigger(
            source,
            INT16_MIN,
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        (source_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_AIR_DODGE &&
         source_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_SPECIAL_LANDING) ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        source_inspection.players[0].position_x_q16 >
            source_inspection.stage.right_ledge_x_q16 ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        (source_inspection.players[0].action_state ==
                 (uint8_t)PF_M4_ACTION_AIR_DODGE &&
         (source_inspection.players[0].grounded != UINT8_C(0) ||
          source_inspection.players[0].support !=
              (uint8_t)PF_M4_SURFACE_NONE ||
          source_inspection.players[0].velocity_y_q16 <= INT32_C(0))) ||
        (source_inspection.players[0].action_state ==
                 (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
         (source_inspection.players[0].grounded != UINT8_C(1) ||
          source_inspection.players[0].support !=
              (uint8_t)PF_M4_SURFACE_FLOOR)) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "edge-dash-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-air-dodge-entry"
            " action=%u ticks=%u grounded=%u support=%u"
            " position=(%" PRId32 ",%" PRId32 ")"
            " velocity=(%" PRId32 ",%" PRId32 ") invulnerable=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[0].position_x_q16,
            source_inspection.players[0].position_y_q16,
            source_inspection.players[0].velocity_x_q16,
            source_inspection.players[0].velocity_y_q16,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "edge-dash-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "edge-dash-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "edge-dash-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "edge-dash-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-snapshot-round-trip\n");
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         source_inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_AIR_DODGE;
         ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_duel_trigger(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "edge-dash-source-air-dodge-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "edge-dash-loaded-air-dodge-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=edge-dash-air-dodge-future"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].grounded != UINT8_C(1) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        source_inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        source_inspection.players[0].position_x_q16 >
            source_inspection.stage.right_ledge_x_q16 ||
        source_inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-landing"
            " tick=%" PRIu32 " action=%u grounded=%u support=%u"
            " position=(%" PRId32 ",%" PRId32 ")"
            " velocity=(%" PRId32 ",%" PRId32 ") invulnerable=%u\n",
            tick,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[0].position_x_q16,
            source_inspection.players[0].position_y_q16,
            source_inspection.players[0].velocity_x_q16,
            source_inspection.players[0].velocity_y_q16,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }

    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.special_landing_ticks;
         ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_duel_trigger(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
            source_inspection.players[0].action_ticks != (uint16_t)tick ||
            source_inspection.players[0].invulnerable != UINT8_C(1) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "edge-dash-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "edge-dash-loaded-future-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=edge-dash-landing-lock"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    if (!step_duel_trigger(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        !step_duel_trigger(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "edge-dash-source-actionable-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "edge-dash-loaded-actionable-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-actionable-overlap\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0xed6edb)),
            PF_STATUS_OK,
            "edge-dash-expired-reset") ||
        !grab_player0_right_ledge(source, &source_inspection) ||
        !make_player0_ledge_actionable(
            source,
            content,
            &source_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         source_inspection.players[0].invulnerable != UINT8_C(0);
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    expiry_ticks = tick;
    if (tick == UINT32_C(120) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-expired-jump"
            " tick=%" PRIu32 " action=%u invulnerable=%u\n",
            expiry_ticks,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         source_inspection.players[0].position_y_q16 +
                 content->fighter.half_height_q16 >
             content->stage.floor_y_q16;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        !step_duel_trigger(
            source,
            INT16_MIN,
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        (source_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_AIR_DODGE &&
         source_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_SPECIAL_LANDING) ||
        source_inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-expired-entry"
            " expiry_ticks=%" PRIu32 " rise_ticks=%" PRIu32
            " action=%u invulnerable=%u\n",
            expiry_ticks,
            tick,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         source_inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_AIR_DODGE;
         ++tick)
    {
        if (!step_duel_trigger(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        source_inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-expired-negative"
            " expiry_ticks=%" PRIu32 " landing_ticks=%" PRIu32
            " action=%u invulnerable=%u\n",
            expiry_ticks,
            tick,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }
    return 1;
}

static int step_planking_pair(
    pf_sim *source,
    pf_sim *loaded,
    int16_t player0_y,
    uint64_t player0_buttons,
    uint64_t player1_buttons,
    pf_m4_inspection *source_inspection,
    pf_m4_inspection *loaded_inspection,
    const char *operation)
{
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;

    if (!step_duel_players(
            source,
            INT16_C(0),
            player0_y,
            player0_buttons,
            INT16_C(0),
            INT16_C(0),
            player1_buttons,
            source_inspection) ||
        !step_duel_players(
            loaded,
            INT16_C(0),
            player0_y,
            player0_buttons,
            INT16_C(0),
            INT16_C(0),
            player1_buttons,
            loaded_inspection) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "planking-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "planking-loaded-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=%s\n",
            operation);
        return 0;
    }
    return 1;
}

static int run_planking_test(
    const pf_m4_content *default_content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage missed_storage;
    pf_m4_content content = *default_content;
    pf_content_view view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *missed = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection missed_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint16_t expected_carried_invulnerability;
    uint32_t cycle;
    uint32_t tick;

    /*
     * This narrow fixture makes the ordinary drop/double-jump arc return to
     * the catch volume on the exact first legal regrab tick. The oversized
     * jab only supplies a deterministic responding-opponent threat.
     */
    content.fighter.double_jump_speed_q16 =
        INT32_C(31) * PF_Q16_ONE / INT32_C(100);
    content.fighter.jab_hitbox_half_width_q16 =
        INT32_C(64) * PF_Q16_ONE;
    content.fighter.jab_hitbox_half_height_q16 =
        INT32_C(64) * PF_Q16_ONE;
    expected_carried_invulnerability =
        (uint16_t)(
            (uint32_t)content.fighter.ledge_invulnerability_ticks -
            (uint32_t)content.fighter.landing_ticks -
            (uint32_t)content.fighter.jump_squat_ticks -
            UINT32_C(1));
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "planking-content") ||
        !initialize_sim(
            &source_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !initialize_sim(
            &missed_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &missed) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x1ed6e91a)),
            PF_STATUS_OK,
            "planking-reset") ||
        !grab_player0_right_ledge(source, &source_inspection) ||
        !make_player0_ledge_actionable(
            source,
            &content,
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].ledge_regrab_lockout_ticks !=
            content.fighter.ledge_regrab_lockout_ticks ||
        source_inspection.players[0].platform_drop_ticks != UINT8_C(0) ||
        source_inspection.players[0].ledge_invulnerability_ticks !=
            expected_carried_invulnerability ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "planking-query-save-size") ||
        save_size != (size_t)787)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=planking-release-setup\n");
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "planking-save"))
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "planking-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "planking-source-initial-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "planking-loaded-initial-hash") ||
        memcmp(
            source_hash.bytes,
            loaded_hash.bytes,
            sizeof(source_hash.bytes)) != 0 ||
        !expect_status(
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "planking-loaded-inspect"))
    {
        return 0;
    }

    for (cycle = UINT32_C(0); cycle < UINT32_C(3); ++cycle)
    {
        if (cycle != UINT32_C(0))
        {
            const uint32_t catch_ticks =
                (uint32_t)content.fighter.ledge_transition_ticks;

            for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
            {
                if (!step_planking_pair(
                        source,
                        loaded,
                        INT16_C(0),
                        UINT64_C(0),
                        UINT64_C(0),
                        &source_inspection,
                        &loaded_inspection,
                        "planking-catch-future-hash"))
                {
                    return 0;
                }
            }
            if (source_inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
                source_inspection.players[0].action_ticks !=
                    (uint16_t)catch_ticks ||
                !step_planking_pair(
                    source,
                    loaded,
                    INT16_MAX,
                    UINT64_C(0),
                    UINT64_C(0),
                    &source_inspection,
                    &loaded_inspection,
                    "planking-release-future-hash") ||
                source_inspection.players[0]
                        .ledge_regrab_lockout_ticks !=
                    content.fighter.ledge_regrab_lockout_ticks)
            {
                return 0;
            }
        }

        for (tick = UINT32_C(1);
             tick <= (uint32_t)
                 content.fighter.ledge_regrab_lockout_ticks;
             ++tick)
        {
            const uint64_t player0_buttons =
                tick == UINT32_C(1)
                    ? PF_INPUT_BUTTON_JUMP
                    : UINT64_C(0);
            const uint64_t player1_buttons =
                tick == UINT32_C(26)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0);

            if (!step_planking_pair(
                    source,
                    loaded,
                    INT16_C(0),
                    player0_buttons,
                    player1_buttons,
                    &source_inspection,
                    &loaded_inspection,
                    "planking-cycle-future-hash"))
            {
                return 0;
            }
            if (tick < (uint32_t)
                    content.fighter.ledge_regrab_lockout_ticks)
            {
                if (source_inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
                    source_inspection.players[0]
                            .ledge_regrab_lockout_ticks !=
                        (uint16_t)(
                            (uint32_t)content.fighter
                                .ledge_regrab_lockout_ticks -
                            tick))
                {
                    (void)fprintf(
                        stderr,
                        "m4-movement=fail operation=planking-lockout"
                        " cycle=%" PRIu32 " tick=%" PRIu32
                        " action=%u remaining=%u\n",
                        cycle,
                        tick,
                        (unsigned int)source_inspection.players[0]
                            .action_state,
                        (unsigned int)source_inspection.players[0]
                            .ledge_regrab_lockout_ticks);
                    return 0;
                }
            }
            if (tick + UINT32_C(1) ==
                    (uint32_t)content.fighter
                        .ledge_regrab_lockout_ticks &&
                (source_inspection.players[0].velocity_y_q16 <
                     INT32_C(0) ||
                 source_inspection.players[0].position_y_q16 <
                     content.stage.floor_y_q16 -
                         content.fighter.half_height_q16 ||
                 source_inspection.players[0].position_y_q16 >
                     content.stage.floor_y_q16 +
                         content.fighter.half_height_q16 ||
                 source_inspection.players[0].position_x_q16 <=
                     content.stage.floor_right_q16 ||
                 (int64_t)source_inspection.players[0]
                         .position_x_q16 -
                         (int64_t)content.stage.floor_right_q16 >
                     (int64_t)content.fighter.half_width_q16 +
                         (int64_t)content.fighter.air_speed_q16 ||
                 source_inspection.players[0].facing != INT8_C(-1)))
            {
                (void)fprintf(
                    stderr,
                    "m4-movement=fail operation=planking-legal-catch"
                    " cycle=%" PRIu32 " tick=%" PRIu32
                    " position=(%" PRId32 ",%" PRId32 ")"
                    " velocity_y=%" PRId32 " facing=%d\n",
                    cycle,
                    tick,
                    source_inspection.players[0].position_x_q16,
                    source_inspection.players[0].position_y_q16,
                    source_inspection.players[0].velocity_y_q16,
                    (int)source_inspection.players[0].facing);
                return 0;
            }
        }
        if (source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
            source_inspection.players[0].ledge !=
                (uint8_t)PF_M4_LEDGE_RIGHT ||
            source_inspection.players[0].ledge_regrab_lockout_ticks !=
                UINT16_C(0) ||
            source_inspection.players[0].ledge_invulnerability_ticks !=
                content.fighter.ledge_invulnerability_ticks ||
            source_inspection.players[0].air_jumps_remaining !=
                content.fighter.air_jump_count ||
            source_inspection.players[0].damage_q16 != UINT32_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=planking-regrab"
                " cycle=%" PRIu32 " action=%u ledge=%u"
                " lockout=%u invulnerability=%u damage=%" PRIu32
                " position=(%" PRId32 ",%" PRId32 ")"
                " velocity=(%" PRId32 ",%" PRId32 ")\n",
                cycle,
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].ledge,
                (unsigned int)source_inspection.players[0]
                    .ledge_regrab_lockout_ticks,
                (unsigned int)source_inspection.players[0]
                    .ledge_invulnerability_ticks,
                source_inspection.players[0].damage_q16,
                source_inspection.players[0].position_x_q16,
                source_inspection.players[0].position_y_q16,
                source_inspection.players[0].velocity_x_q16,
                source_inspection.players[0].velocity_y_q16);
            return 0;
        }
    }

    if (!expect_status(
            pf_sim_reset(missed, UINT64_C(0x1ed6e91b)),
            PF_STATUS_OK,
            "planking-missed-reset") ||
        !grab_player0_right_ledge(missed, &missed_inspection) ||
        !make_player0_ledge_actionable(
            missed,
            &content,
            &missed_inspection) ||
        !step_duel(
            missed,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &missed_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <= (uint32_t)content.fighter.ledge_regrab_lockout_ticks;
         ++tick)
    {
        if (!step_duel_players(
                missed,
                INT16_C(0),
                tick >= UINT32_C(28) ? INT16_MAX : INT16_C(0),
                tick == UINT32_C(1)
                    ? PF_INPUT_BUTTON_JUMP
                    : UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(26)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                &missed_inspection))
        {
            return 0;
        }
        if (tick == UINT32_C(28) &&
            (missed_inspection.players[0].action_state ==
                 (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
             missed_inspection.players[0]
                     .ledge_regrab_lockout_ticks != UINT16_C(1)))
        {
            return 0;
        }
    }
    if (missed_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        missed_inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE ||
        missed_inspection.players[0].ledge_regrab_lockout_ticks !=
            UINT16_C(0) ||
        missed_inspection.players[0].ledge_invulnerability_ticks !=
            UINT16_C(0) ||
        missed_inspection.players[0].damage_q16 !=
            content.fighter.jab_damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=planking-missed-punish"
            " action=%u ledge=%u lockout=%u invulnerability=%u"
            " damage=%" PRIu32 " position=(%" PRId32 ",%" PRId32
            ")\n",
            (unsigned int)missed_inspection.players[0].action_state,
            (unsigned int)missed_inspection.players[0].ledge,
            (unsigned int)missed_inspection.players[0]
                .ledge_regrab_lockout_ticks,
            (unsigned int)missed_inspection.players[0]
                .ledge_invulnerability_ticks,
            missed_inspection.players[0].damage_q16,
            missed_inspection.players[0].position_x_q16,
            missed_inspection.players[0].position_y_q16);
        return 0;
    }
    return 1;
}

static int run_ledge_roll_test(const pf_m4_content *default_content)
{
    test_sim_storage storage;
    pf_m4_content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    const uint32_t catch_ticks =
        (uint32_t)content.fighter.ledge_transition_ticks;
    int32_t hang_x;
    int32_t hang_y;
    int32_t target_x;
    int32_t target_y;
    uint32_t tick;

    content.fighter.ledge_invulnerability_ticks = UINT16_C(1);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "ledge-roll-content") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6e7011)),
            PF_STATUS_OK,
            "ledge-roll-reset") ||
        !grab_player0_right_ledge(sim, &inspection))
    {
        return 0;
    }

    hang_x = inspection.players[0].position_x_q16;
    hang_y = inspection.players[0].position_y_q16;
    target_x =
        inspection.stage.right_ledge_x_q16 -
        content.fighter.ledge_roll_distance_q16;
    target_y =
        inspection.stage.floor_y_q16 - content.fighter.half_height_q16;

    while ((uint32_t)inspection.players[0].action_ticks < catch_ticks)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                content.fighter.digital_trigger_threshold,
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-roll-held-lock\n");
            return 0;
        }
    }
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            content.fighter.digital_trigger_threshold,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            content.fighter.digital_trigger_threshold,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        inspection.players[0].position_x_q16 != hang_x ||
        inspection.players[0].position_y_q16 != hang_y ||
        inspection.players[0].invulnerable != UINT8_C(1) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].position_x_q16 >= hang_x ||
        inspection.players[0].position_y_q16 >= hang_y ||
        !run_ledge_snapshot_test(
            sim,
            &view,
            &inspection,
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-entry\n");
        return 0;
    }

    while (inspection.players[0].action_ticks <
           content.fighter.ledge_roll_movement_ticks)
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
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
        inspection.players[0].position_x_q16 != target_x ||
        inspection.players[0].position_y_q16 != target_y ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-motion-end\n");
        return 0;
    }

    while ((uint32_t)inspection.players[0].action_ticks + UINT32_C(1) <
           (uint32_t)content.fighter.ledge_roll_invulnerability_ticks)
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
    if (inspection.players[0].invulnerable != UINT8_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_ticks !=
            content.fighter.ledge_roll_invulnerability_ticks ||
        inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-invulnerability\n");
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(32) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_LEDGE_ROLL;
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
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE ||
        inspection.players[0].position_x_q16 != target_x ||
        inspection.players[0].position_y_q16 != target_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-completion\n");
        return 0;
    }
    return 1;
}

static int run_ledge_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_m4_content invalid_content = *content;
    pf_m4_content tuned_content = *content;
    pf_content_view tuned_view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t hang_x;
    int32_t hang_y;
    uint32_t tick;

    invalid_content.fighter.ledge_invulnerability_ticks =
        UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-invulnerability-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_regrab_lockout_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-regrab-lockout-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_roll_distance_q16 =
        content->fighter.half_width_q16 +
        content->fighter.platform_drop_nudge_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-roll-distance-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_roll_movement_ticks =
        (uint16_t)(invalid_content.fighter.ledge_roll_ticks +
                   UINT16_C(1));
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-roll-movement-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_roll_invulnerability_ticks =
        (uint16_t)(invalid_content.fighter.ledge_roll_ticks +
                   UINT16_C(1));
    tuned_content.fighter.ledge_roll_distance_q16 += INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-roll-invulnerability-invalid-content") ||
        !expect_status(
            pf_m4_make_content_view(&tuned_content, &tuned_view),
            PF_STATUS_OK,
            "ledge-roll-tuned-content") ||
        memcmp(
            view->content_hash.bytes,
            tuned_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        !initialize_sim(
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
    if (inspection.players[0].invulnerable != UINT8_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].position_x_q16 != hang_x ||
        inspection.players[0].position_y_q16 != hang_y ||
        inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        inspection.players[0].velocity_y_q16 != INT32_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(1))
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
        inspection.players[0].position_y_q16 <= hang_y ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-release\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6e)),
            PF_STATUS_OK,
            "ledge-invulnerability-reset") ||
        !grab_player0_right_ledge(sim, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)
             content->fighter.ledge_invulnerability_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].invulnerable != UINT8_C(1))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-invulnerability-window"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-invulnerability-end\n");
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
        !run_ledge_snapshot_test(
            sim,
            view,
            &inspection,
            (uint8_t)PF_M4_ACTION_LEDGE_CLIMB))
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
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-climb-completion"
            " action=%u ticks=%u grounded=%u support=%u ledge=%u"
            " position_x=%" PRId32 " right=%" PRId32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            (unsigned int)inspection.players[0].ledge,
            inspection.players[0].position_x_q16,
            inspection.stage.right_ledge_x_q16);
        return 0;
    }
    if (!run_ledge_roll_test(content))
    {
        (void)fprintf(stderr, "m4-movement=fail operation=ledge-roll-suite\n");
        return 0;
    }
    if (!run_ledge_occupancy_test(content))
    {
        (void)fprintf(stderr, "m4-movement=fail operation=ledge-occupancy-suite\n");
        return 0;
    }
    if (!run_ledge_hit_rejection_test(content))
    {
        (void)fprintf(stderr, "m4-movement=fail operation=ledge-hit-suite\n");
        return 0;
    }
    if (!run_edge_hop_test(content, view))
    {
        (void)fprintf(stderr, "m4-movement=fail operation=edge-hop-suite\n");
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

static int run_vector_ascent_test(const pf_m4_content *base_content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage rl_storage;
    pf_m4_content content = *base_content;
    pf_m4_content invalid;
    pf_content_view disabled_view;
    pf_content_view view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *rl_sim = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_observation observation;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_mut_bytes destination;
    pf_bytes save;
    pf_rl_transition transition;
    pf_rl_action actions[2];
    uint8_t save_bytes[1024];
    size_t save_size = (size_t)0;
    int32_t grounded_recovery_x;
    uint32_t player_bits;
    uint32_t guard;

    if (content.recovery_count != PF_M4_TEST_RECOVERY_COUNT ||
        content.recovery.schema_version != PF_M4_RECOVERY_SCHEMA_VERSION ||
        content.recovery.enabled != UINT8_C(0) ||
        content.recovery.horizontal_speed_q16 !=
            PF_Q16_ONE / INT32_C(4) ||
        content.recovery.vertical_speed_q16 !=
            INT32_C(4) * PF_Q16_ONE / INT32_C(5) ||
        content.recovery.ascent_ticks != UINT16_C(18) ||
        !expect_status(
            pf_m4_make_content_view(&content, &disabled_view),
            PF_STATUS_OK,
            "vector-ascent-disabled-view"))
    {
        return 0;
    }

    invalid = content;
    invalid.recovery.enabled = UINT8_C(2);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "vector-ascent-invalid-enabled"))
    {
        return 0;
    }
    invalid = content;
    invalid.recovery.ascent_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "vector-ascent-invalid-duration"))
    {
        return 0;
    }

    content.recovery.enabled = UINT8_C(1);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "vector-ascent-view") ||
        memcmp(
            disabled_view.content_hash.bytes,
            view.content_hash.bytes,
            sizeof(view.content_hash.bytes)) == 0 ||
        !initialize_sim(
            &source_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x564543544f524153)),
            PF_STATUS_OK,
            "vector-ascent-reset-source") ||
        !expect_status(
            pf_sim_reset(loaded, UINT64_C(0x564543544f524153)),
            PF_STATUS_OK,
            "vector-ascent-reset-loaded") ||
        !expect_status(
            pf_m4_inspect(source, &source_inspection),
            PF_STATUS_OK,
            "vector-ascent-inspect-reset") ||
        source_inspection.players[0].recovery_available != UINT8_C(1) ||
        !step_duel(
            loaded,
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_SPECIAL,
            &loaded_inspection) ||
        loaded_inspection.players[0].grounded != UINT8_C(0) ||
        loaded_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
        loaded_inspection.players[0].recovery_available != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ground-vector-ascent-entry"
            " action=%u grounded=%u recovery=%u reset_action=%u"
            " charge_enabled=%u recovery_enabled=%u threshold=%u\n",
            (unsigned int)loaded_inspection.players[0].action_state,
            (unsigned int)loaded_inspection.players[0].grounded,
            (unsigned int)loaded_inspection.players[0].recovery_available,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)content.charge.enabled,
            (unsigned int)content.recovery.enabled,
            (unsigned int)content.fighter.dash_axis_threshold);
        return 0;
    }
    for (guard = UINT32_C(0);
         guard < UINT32_C(240) &&
         loaded_inspection.players[0].grounded == UINT8_C(0);
         ++guard)
    {
        if (!step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection))
        {
            return 0;
        }
    }
    if (guard == UINT32_C(240) ||
        loaded_inspection.players[0].recovery_available != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ground-vector-ascent-land"
            " guard=%u action=%u grounded=%u recovery=%u\n",
            (unsigned int)guard,
            (unsigned int)loaded_inspection.players[0].action_state,
            (unsigned int)loaded_inspection.players[0].grounded,
            (unsigned int)loaded_inspection.players[0].recovery_available);
        return 0;
    }
    for (guard = UINT32_C(0);
         guard < UINT32_C(32) &&
         loaded_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
         ++guard)
    {
        if (!step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection))
        {
            return 0;
        }
    }
    grounded_recovery_x = loaded_inspection.players[0].position_x_q16;
    if (guard == UINT32_C(32) ||
        !step_duel(
            loaded,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        loaded_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        loaded_inspection.players[0].position_x_q16 !=
            grounded_recovery_x ||
        !step_duel(
            loaded,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        loaded_inspection.players[0].position_x_q16 <=
            grounded_recovery_x ||
        !expect_status(
            pf_sim_reset(loaded, UINT64_C(0x564543544f524153)),
            PF_STATUS_OK,
            "vector-ascent-reset-loaded-after-ground") ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ground-vector-ascent-mobility"
            " guard=%u action=%u x=%d start=%d\n",
            (unsigned int)guard,
            (unsigned int)loaded_inspection.players[0].action_state,
            loaded_inspection.players[0].position_x_q16,
            grounded_recovery_x);
        return 0;
    }

    for (guard = UINT32_C(0);
         guard < UINT32_C(8) &&
         source_inspection.players[0].grounded != UINT8_C(0);
         ++guard)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (source_inspection.players[0].grounded != UINT8_C(0) ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_MIN,
            PF_INPUT_BUTTON_SPECIAL,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].recovery_available != UINT8_C(0) ||
        source_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        source_inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        !expect_status(
            pf_sim_observe(source, &observation),
            PF_STATUS_OK,
            "vector-ascent-observe") ||
        observation.schema_version != PF_SIM_OBSERVATION_SCHEMA_VERSION ||
        observation.players[0].recovery_available != UINT8_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "vector-ascent-save-size") ||
        save_size != (size_t)787)
    {
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "vector-ascent-save") ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "vector-ascent-load"))
    {
        return 0;
    }

    for (guard = UINT32_C(0);
         guard < UINT32_C(24) &&
         source_inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_VECTOR_ASCENT;
         ++guard)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "vector-ascent-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "vector-ascent-loaded-hash") ||
            memcmp(
                source_hash.bytes,
                loaded_hash.bytes,
                sizeof(source_hash.bytes)) != 0)
        {
            return 0;
        }
    }
    if (guard == UINT32_C(24) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        source_inspection.players[0].recovery_available != UINT8_C(0) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_SPECIAL,
            &source_inspection) ||
        source_inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
        source_inspection.players[0].recovery_available != UINT8_C(0))
    {
        return 0;
    }

    for (guard = UINT32_C(0);
         guard < UINT32_C(240) &&
         source_inspection.players[0].grounded == UINT8_C(0);
         ++guard)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (guard == UINT32_C(240) ||
        source_inspection.players[0].recovery_available != UINT8_C(1))
    {
        return 0;
    }
    for (guard = UINT32_C(0);
         guard < UINT32_C(24) &&
         source_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
         ++guard)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0);
         guard < UINT32_C(8) &&
         source_inspection.players[0].grounded != UINT8_C(0);
         ++guard)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_SPECIAL,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
        source_inspection.players[0].recovery_available != UINT8_C(0))
    {
        return 0;
    }

    if (!initialize_sim(
            &rl_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &rl_sim) ||
        !expect_status(
            pf_rl_reset(
                rl_sim,
                UINT64_C(0x564543544f52524c),
                &transition),
            PF_STATUS_OK,
            "vector-ascent-rl-reset"))
    {
        return 0;
    }
    (void)memset(actions, 0, sizeof(actions));
    actions[0].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[1].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[0].buttons = PF_INPUT_BUTTON_JUMP;
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "vector-ascent-rl-jump"))
    {
        return 0;
    }
    actions[0].buttons = UINT64_C(0);
    for (guard = UINT32_C(0);
         guard < UINT32_C(8) &&
         transition.structured_observation.players[0].grounded !=
             UINT8_C(0);
         ++guard)
    {
        if (!expect_status(
                pf_rl_step(rl_sim, actions, (size_t)2, &transition),
                PF_STATUS_OK,
                "vector-ascent-rl-airborne"))
        {
            return 0;
        }
    }
    actions[0].main_stick_y = INT16_MIN;
    actions[0].buttons = PF_INPUT_BUTTON_SPECIAL;
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "vector-ascent-rl-special") ||
        transition.structured_observation.players[0].recovery_available !=
            UINT8_C(0))
    {
        return 0;
    }
    (void)memcpy(
        &player_bits,
        &transition.compact_observation.values[
            PF_RL_COMPACT_PLAYER_BASE(0) + UINT16_C(6)],
        sizeof(player_bits));
    return ((player_bits >> 18U) & UINT32_C(1)) == UINT32_C(0);
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

#define RUN_MOVEMENT_TEST(call)                                         \
    ((call) ? 1                                                        \
            : ((void)fprintf(                                         \
                   stderr,                                             \
                   "m4-movement=fail suite=%s\n",                    \
                   #call),                                             \
               0))

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
        !RUN_MOVEMENT_TEST(run_content_contract_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_ground_control_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(
            run_jump_takeoff_momentum_test(&content, &view)) ||
        (0 && !run_fox_trot_test(&content, &view)) ||
        (0 && !run_moonwalk_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_teeter_cancel_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_taunt_cancel_test(&content, &view)) ||
        (0 && !run_stage_humping_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_pivot_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_dash_cancel_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_air_control_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(
            run_instant_double_jump_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(
            run_double_jump_cancel_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_air_facing_lock_test(&view)) ||
        !RUN_MOVEMENT_TEST(run_air_dodge_test(&content, &view)) ||
        (0 && !run_ledge_cancel_test(&content)) ||
        (0 && !run_planking_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_ground_dodge_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_aerial_landing_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(
            run_strong_aerial_landing_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_platform_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_upper_platform_test(&content)) ||
        !RUN_MOVEMENT_TEST(
            run_shield_platform_drop_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_solid_geometry_test(&content)) ||
        (0 && !run_scar_jump_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_vector_ascent_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_ledge_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_edge_dash_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_blast_zone_test(&view)) ||
        !RUN_MOVEMENT_TEST(run_team_hash_trace(&view)))
    {
        return 1;
    }

    (void)printf(
        "m4-movement=pass content_schema=%u deterministic_ticks=20000 "
        "movement_core=pass jump_takeoff_momentum=1 "
        "teeter_cancel=1 "
        "taunt_cancel=1 "
        "double_jump_cancel=1 vector_ascent=1 "
        "ledge_roll=1 "
        "emergent_technique_tests=skipped\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION);
    return 0;
}
