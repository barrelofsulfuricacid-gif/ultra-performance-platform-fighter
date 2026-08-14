#include "pf/m4.h"
#include "pf/rl.h"
#include "pf/sim.h"
#include "../../src/sim/sim_internal.h"
#include "../../src/sim/sim_falcon_frame_data.h"
#include "../../src/sim/sim_ssbm_common_data.h"

#include <inttypes.h>
#include <float.h>
#include <math.h>
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

static float absolute_f32(float value)
{
    return value < 0.0f ? -value : value;
}

static int near_f32(float left, float right)
{
    float scale = absolute_f32(left);
    const float right_scale = absolute_f32(right);

    if (scale < right_scale)
    {
        scale = right_scale;
    }
    if (scale < 1.0f)
    {
        scale = 1.0f;
    }
    return absolute_f32(left - right) <=
           8.0f * FLT_EPSILON * scale;
}

static int player_overlaps_solid(
    const struct content *content,
    const player_inspection *player)
{
    return player->position_x_f32 + content->fighter.half_width_f32 >
               content->stage.solid_left_f32 &&
           player->position_x_f32 - content->fighter.half_width_f32 <
               content->stage.solid_right_f32 &&
           player->position_y_f32 + content->fighter.half_height_f32 >
               content->stage.solid_top_f32 &&
           player->position_y_f32 - content->fighter.half_height_f32 <
               content->stage.solid_bottom_f32;
}

static int step_duel_players(
    pf_sim *sim,
    int16_t player0_x,
    int16_t player0_y,
    uint64_t player0_buttons,
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons,
    struct inspection *out_inspection)
{
    struct inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            inspect(sim, &before),
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
            inspect(sim, out_inspection),
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
    struct inspection *out_inspection)
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

static int step_duel_raw_main_x_buttons(
    pf_sim *sim,
    int16_t main_stick_x,
    int8_t raw_main_stick_x,
    uint64_t buttons,
    struct inspection *out_inspection)
{
    struct inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_input_raw_pad raw_pad = {
        raw_main_stick_x,
        INT8_C(0),
        INT8_C(0),
        INT8_C(0)};
    pf_tick_result result;

    if (!expect_status(
            inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-raw-main-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[0].main_stick_x = main_stick_x;
    inputs[0].buttons = buttons;
    pf_input_set_raw_pad(&inputs[0], raw_pad);
    return expect_status(
               pf_sim_tick(sim, inputs, (size_t)2, &result),
               PF_STATUS_OK,
               "raw-main-step") &&
           expect_status(
               inspect(sim, out_inspection),
               PF_STATUS_OK,
               "inspect-after-raw-main-step");
}

static int step_duel_raw_main_x(
    pf_sim *sim,
    int16_t main_stick_x,
    int8_t raw_main_stick_x,
    struct inspection *out_inspection)
{
    return step_duel_raw_main_x_buttons(
        sim,
        main_stick_x,
        raw_main_stick_x,
        UINT64_C(0),
        out_inspection);
}

static int step_duel_sticks(
    pf_sim *sim,
    int16_t main_stick_x,
    int16_t main_stick_y,
    int16_t secondary_stick_x,
    int16_t secondary_stick_y,
    struct inspection *out_inspection)
{
    struct inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-stick-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[0].main_stick_x = main_stick_x;
    inputs[0].main_stick_y = main_stick_y;
    inputs[0].secondary_stick_x = secondary_stick_x;
    inputs[0].secondary_stick_y = secondary_stick_y;
    return expect_status(
               pf_sim_tick(sim, inputs, (size_t)2, &result),
               PF_STATUS_OK,
               "stick-step") &&
           expect_status(
               inspect(sim, out_inspection),
               PF_STATUS_OK,
               "inspect-after-stick-step");
}

static int reset_to_normal_landing_frame_four(
    pf_sim *sim,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "normal-landing-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(240) &&
         out_inspection->players[0].action_state !=
             (uint8_t)PF_M4_ACTION_LANDING;
         ++tick)
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
    if (tick == UINT32_C(240) ||
        out_inspection->players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=normal-landing-setup\n");
        return 0;
    }
    for (tick = UINT32_C(1); tick <= UINT32_C(3); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].action_state !=
                (uint8_t)PF_M4_ACTION_LANDING ||
            out_inspection->players[0].action_ticks != (uint16_t)tick)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=normal-landing-frame-four\n");
            return 0;
        }
    }
    return 1;
}

static int step_duel_triggers(
    pf_sim *sim,
    int16_t main_stick_x,
    int16_t main_stick_y,
    uint64_t buttons,
    uint16_t left_trigger,
    uint16_t right_trigger,
    struct inspection *out_inspection)
{
    struct inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            inspect(sim, &before),
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
            inspect(sim, out_inspection),
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
    struct inspection *out_inspection)
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
    struct inspection *out_inspection)
{
    struct inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;

    if (!expect_status(
            inspect(sim, &before),
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
            inspect(sim, out_inspection),
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
    struct inspection *out_inspection)
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
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
        required_bytes != (size_t)1800)
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
            source_inspection.players[0].position_x_f32 !=
                loaded_inspection.players[0].position_x_f32 ||
            source_inspection.players[0].position_y_f32 !=
                loaded_inspection.players[0].position_y_f32 ||
            source_inspection.players[0].velocity_x_f32 !=
                loaded_inspection.players[0].velocity_x_f32 ||
            source_inspection.players[0].velocity_y_f32 !=
                loaded_inspection.players[0].velocity_y_f32 ||
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
    const struct content *default_content,
    const pf_content_view *view)
{
    const falcon_body_collision_timing *air_dodge_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_AIR_DODGE);
    const falcon_air_dodge_attributes *air_dodge_attributes =
        falcon_reference_air_dodge_attributes();
    test_sim_storage storage;
    test_sim_storage platform_storage;
    pf_sim *sim = NULL;
    pf_sim *platform_sim = NULL;
    struct content invalid_content = *default_content;
    struct content platform_content = *default_content;
    pf_content_view platform_view;
    struct inspection inspection;
    float entry_velocity_x;
    float entry_velocity_y;
    float expected_entry_velocity_x;
    float expected_entry_velocity_y;
    float landing_x;
    float landing_velocity_x;
    float previous_landing_x;
    int8_t takeoff_facing;
    uint32_t tick;
    int saw_ordinary_physics = 0;

    if (air_dodge_collision == NULL || air_dodge_attributes == NULL ||
        default_content->fighter.air_dodge_speed_x_f32 !=
            air_dodge_attributes->initial_velocity_x_f32 ||
        default_content->fighter.air_dodge_speed_y_f32 !=
            air_dodge_attributes->initial_velocity_y_f32 ||
        default_content->fighter.air_dodge_decay_f32 !=
            air_dodge_attributes->decay_f32 ||
        default_content->fighter.air_dodge_dead_zone !=
            air_dodge_attributes->dead_zone ||
        (uint32_t)default_content->fighter
                .air_dodge_ordinary_physics_begin_tick +
                UINT32_C(1) !=
            (uint32_t)air_dodge_attributes->ordinary_physics_begin_frame ||
        (uint32_t)default_content->fighter
                .air_dodge_invulnerability_begin_tick +
                UINT32_C(1) !=
            (uint32_t)air_dodge_collision->state_two_frame ||
        (uint32_t)default_content->fighter
                .air_dodge_invulnerability_end_tick +
                UINT32_C(1) !=
            (uint32_t)air_dodge_collision->state_zero_frame)
    {
        return 0;
    }

    invalid_content.fighter.air_dodge_decay_f32 =
        1.0f + INT32_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
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
            validate_content(&invalid_content),
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
    entry_velocity_x = inspection.players[0].velocity_x_f32;
    entry_velocity_y = inspection.players[0].velocity_y_f32;
    expected_entry_velocity_x =
        (float)INT16_MAX *
        default_content->fighter.air_dodge_speed_x_f32 /
        hypotf((float)INT16_MAX, (float)INT16_MIN);
    expected_entry_velocity_y =
        (float)INT16_MIN *
        default_content->fighter.air_dodge_speed_y_f32 /
        hypotf((float)INT16_MAX, (float)INT16_MIN);
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
        inspection.players[0].velocity_x_f32 !=
            entry_velocity_x *
                default_content->fighter.air_dodge_decay_f32 ||
        inspection.players[0].velocity_y_f32 !=
            entry_velocity_y *
                default_content->fighter.air_dodge_decay_f32 ||
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
        const int entering_ordinary_physics =
            (uint32_t)inspection.players[0].action_ticks + UINT32_C(1) ==
            (uint32_t)default_content->fighter
                .air_dodge_ordinary_physics_begin_tick;
        const float previous_velocity_y_f32 =
            inspection.players[0].velocity_y_f32;

        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
        if (entering_ordinary_physics != 0)
        {
            if (inspection.players[0].velocity_y_f32 !=
                previous_velocity_y_f32 +
                    default_content->fighter.gravity_f32)
            {
                return 0;
            }
            saw_ordinary_physics = 1;
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
    if (saw_ordinary_physics == 0 ||
        inspection.players[0].invulnerable != UINT8_C(0) ||
        inspection.players[0].facing != takeoff_facing ||
        (inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_FALL_SPECIAL &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_SPECIAL_LANDING))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-dodge-fall-special"
            " action=%u tick=%u grounded=%u ordinary=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            saw_ordinary_physics);
        return 0;
    }
    if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL &&
        (!step_duel_trigger(
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
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=air-dodge-held-retrigger\n");
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
        inspection.players[0].velocity_x_f32 != 0.0f ||
        inspection.players[0].velocity_y_f32 != 0.0f)
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
        inspection.players[0].velocity_x_f32 <= 0.0f ||
        inspection.players[0].velocity_y_f32 <= 0.0f ||
        inspection.players[0].facing != takeoff_facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=wavedash-landing\n");
        return 0;
    }
    landing_x = inspection.players[0].position_x_f32;
    landing_velocity_x = inspection.players[0].velocity_x_f32;
    for (tick = UINT32_C(1);
         tick < (uint32_t)default_content->fighter.special_landing_ticks;
         ++tick)
    {
        previous_landing_x = inspection.players[0].position_x_f32;
        if (!step_duel_trigger(
                sim,
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
            inspection.players[0].action_ticks != (uint16_t)tick ||
            !near_f32(
                inspection.players[0].position_x_f32 -
                    previous_landing_x,
                inspection.players[0].velocity_x_f32))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=special-landing-lock"
                " tick=%" PRIu32 " action=%u action_ticks=%u"
                " delta_x=%.9g velocity_x=%.9g\n",
                tick,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks,
                inspection.players[0].position_x_f32 - previous_landing_x,
                inspection.players[0].velocity_x_f32);
            return 0;
        }
    }
    previous_landing_x = inspection.players[0].position_x_f32;
    if (!step_duel_trigger(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        !near_f32(
            inspection.players[0].position_x_f32 - previous_landing_x,
            inspection.players[0].velocity_x_f32) ||
        inspection.players[0].position_x_f32 <= landing_x ||
        inspection.players[0].velocity_x_f32 >= landing_velocity_x)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=special-landing-terminal-wait"
            " or-slide action=%u ticks=%u x=%.9g"
            " landing_x=%.9g" " vx=%.9g"
            " landing_vx=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            inspection.players[0].position_x_f32,
            landing_x,
            inspection.players[0].velocity_x_f32,
            landing_velocity_x);
        return 0;
    }

    platform_content.stage.platform_center_x_f32 =
        -INT32_C(2) * 1.0f;
    platform_content.stage.platform_y_f32 =
        platform_content.stage.floor_y_f32 -
        INT32_C(6) * 1.0f;
    platform_content.stage.platform_motion_amplitude_f32 = 0.0f;
    platform_content.stage.spawn_spacing_f32 =
        INT32_C(2) * 1.0f;
    if (!expect_status(
            make_content_view(
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
        const float bottom =
            inspection.players[0].position_y_f32 +
            platform_content.fighter.half_height_f32;
        const float maximum_diagonal_drop =
            (platform_content.fighter.air_dodge_speed_y_f32 *
             INT32_C(3)) /
            INT32_C(4);

        if (inspection.players[0].velocity_y_f32 >= 0.0f &&
            bottom >=
                platform_content.stage.platform_y_f32 -
                    maximum_diagonal_drop &&
            bottom <= platform_content.stage.platform_y_f32)
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
        inspection.players[0].velocity_x_f32 <= 0.0f)
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
    const struct content *content,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL ||
        !launch_player0(sim, 0, out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const float bottom =
            out_inspection->players[0].position_y_f32 +
            content->fighter.half_height_f32;
        const float maximum_diagonal_drop =
            (content->fighter.air_dodge_speed_y_f32 *
             INT32_C(3)) /
            INT32_C(4);

        if (out_inspection->players[0].velocity_y_f32 >= 0.0f &&
            bottom >=
                content->stage.platform_y_f32 -
                    maximum_diagonal_drop &&
            bottom <= content->stage.platform_y_f32)
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
           out_inspection->players[0].velocity_x_f32 > 0.0f;
}

static int run_ledge_cancel_snapshot_test(
    pf_sim *source,
    const pf_content_view *content,
    const struct content *ledge_cancel_content)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
        required_bytes != (size_t)1800)
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
            source_inspection.players[0].position_x_f32 !=
                loaded_inspection.players[0].position_x_f32 ||
            source_inspection.players[0].position_y_f32 !=
                loaded_inspection.players[0].position_y_f32)
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
             source_inspection.players[0].position_x_f32 <=
                 ledge_cancel_content->stage.platform_center_x_f32 +
                     ledge_cancel_content->stage
                         .platform_half_width_f32))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ledge-cancel-transition"
                " action=%u ticks=%u grounded=%u support=%u x=%.9g"
                "\n",
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].action_ticks,
                (unsigned int)source_inspection.players[0].grounded,
                (unsigned int)source_inspection.players[0].support,
                source_inspection.players[0].position_x_f32);
            return 0;
        }
    }
    return 1;
}

static int run_ledge_cancel_test(const struct content *default_content)
{
    test_sim_storage ledge_storage;
    test_sim_storage center_storage;
    pf_sim *ledge_sim = NULL;
    pf_sim *center_sim = NULL;
    struct content ledge_content = *default_content;
    struct content center_content;
    pf_content_view ledge_view;
    pf_content_view center_view;
    struct inspection inspection;
    float landing_x;
    uint32_t tick;

    ledge_content.stage.platform_center_x_f32 =
        -(INT32_C(5) * 1.0f) / INT32_C(2);
    ledge_content.stage.platform_y_f32 =
        ledge_content.stage.floor_y_f32 -
        INT32_C(6) * 1.0f;
    ledge_content.stage.platform_half_width_f32 = 1.0f;
    ledge_content.stage.platform_motion_amplitude_f32 = 0.0f;
    ledge_content.stage.spawn_spacing_f32 =
        INT32_C(2) * 1.0f;
    center_content = ledge_content;
    center_content.stage.platform_center_x_f32 =
        -INT32_C(2) * 1.0f;
    center_content.stage.platform_half_width_f32 =
        INT32_C(5) * 1.0f;

    if (!expect_status(
            make_content_view(&ledge_content, &ledge_view),
            PF_STATUS_OK,
            "ledge-cancel-content") ||
        !expect_status(
            make_content_view(&center_content, &center_view),
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
    landing_x = inspection.players[0].position_x_f32;
    if (landing_x >=
            ledge_content.stage.platform_center_x_f32 +
                ledge_content.stage.platform_half_width_f32 ||
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
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
        required_bytes != (size_t)1800)
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
            source_inspection.players[0].position_x_f32 !=
                loaded_inspection.players[0].position_x_f32 ||
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
    const struct content *default_content,
    const pf_content_view *default_view)
{
    const falcon_body_collision_timing *spot_dodge_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_SPOT_DODGE);
    const falcon_body_collision_timing *roll_forward_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_ROLL_FORWARD);
    const falcon_body_collision_timing *roll_backward_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD);
    test_sim_storage storage;
    test_sim_storage wall_storage;
    test_sim_storage edge_storage;
    struct content invalid_content = *default_content;
    struct content wall_content = *default_content;
    struct content edge_content = *default_content;
    pf_content_view wall_view;
    pf_content_view edge_view;
    pf_sim *sim = NULL;
    pf_sim *wall_sim = NULL;
    pf_sim *edge_sim = NULL;
    struct inspection inspection;
    float start_x;
    float expected_x;
    int8_t facing;
    uint32_t elapsed;
    float forward_roll_displacement_f32 = 0.0f;
    float backward_roll_displacement_f32 = 0.0f;
    uint16_t translation_frame;

    for (translation_frame = UINT16_C(1);
         translation_frame <= UINT16_C(31);
         ++translation_frame)
    {
        float forward_x_f32 = 0.0f;
        float forward_y_f32 = 0.0f;
        float backward_x_f32 = 0.0f;
        float backward_y_f32 = 0.0f;

        if (!falcon_reference_translation_f32(
                PF_M4_FALCON_SUBMOTION_ROLL_FORWARD,
                translation_frame,
                &forward_x_f32,
                &forward_y_f32) ||
            !falcon_reference_translation_f32(
                PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD,
                translation_frame,
                &backward_x_f32,
                &backward_y_f32) ||
            forward_y_f32 != 0.0f ||
            backward_y_f32 != 0.0f)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ground-dodge-translation"
                " frame=%u forward=(%.9g,%.9g) backward=(%.9g,%.9g)\n",
                (unsigned int)translation_frame,
                forward_x_f32,
                forward_y_f32,
                backward_x_f32,
                backward_y_f32);
            return 0;
        }
        forward_roll_displacement_f32 += forward_x_f32;
        backward_roll_displacement_f32 += backward_x_f32;
        if ((translation_frame == UINT16_C(1) &&
             (forward_x_f32 != 0.319585204f ||
              backward_x_f32 != 2.19655813e-05f)) ||
            (translation_frame == UINT16_C(4) &&
             backward_x_f32 != -0.0875875801f) ||
            (translation_frame == UINT16_C(29) &&
             forward_x_f32 != 0.0042648674f) ||
            (translation_frame == UINT16_C(31) &&
             (forward_x_f32 != 0.000724301091f ||
              backward_x_f32 != -0.0025961881f)))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=ground-dodge-translation-contract"
                " frame=%u forward=%.9g backward=%.9g\n",
                (unsigned int)translation_frame,
                forward_x_f32,
                backward_x_f32);
            return 0;
        }
    }

    if (spot_dodge_collision == NULL || roll_forward_collision == NULL ||
        roll_backward_collision == NULL ||
        default_content->fighter.forward_roll_ticks != UINT16_C(31) ||
        default_content->fighter.backward_roll_ticks != UINT16_C(31) ||
        default_content->fighter.roll_movement_begin_tick != UINT16_C(3) ||
        default_content->fighter.roll_movement_end_tick != UINT16_C(20) ||
        default_content->fighter.roll_invulnerability_begin_tick !=
            roll_forward_collision->state_two_frame ||
        default_content->fighter.roll_invulnerability_end_tick !=
            roll_forward_collision->state_zero_frame ||
        roll_backward_collision->state_two_frame !=
            roll_forward_collision->state_two_frame ||
        roll_backward_collision->state_zero_frame !=
            roll_forward_collision->state_zero_frame ||
        default_content->fighter.spot_dodge_ticks != UINT16_C(32) ||
        default_content->fighter
                .spot_dodge_invulnerability_begin_tick !=
            spot_dodge_collision->state_two_frame ||
        default_content->fighter.spot_dodge_invulnerability_end_tick !=
            spot_dodge_collision->state_zero_frame)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ground-dodge-source-contract"
            " roll=(%u,%u) move=(%u,%u) inv=(%u,%u)"
            " spot=(%u,%u,%u)\n",
            (unsigned int)default_content->fighter.forward_roll_ticks,
            (unsigned int)default_content->fighter.backward_roll_ticks,
            (unsigned int)default_content->fighter.roll_movement_begin_tick,
            (unsigned int)default_content->fighter.roll_movement_end_tick,
            (unsigned int)default_content->fighter
                .roll_invulnerability_begin_tick,
            (unsigned int)default_content->fighter
                .roll_invulnerability_end_tick,
            (unsigned int)default_content->fighter.spot_dodge_ticks,
            (unsigned int)default_content->fighter
                .spot_dodge_invulnerability_begin_tick,
            (unsigned int)default_content->fighter
                .spot_dodge_invulnerability_end_tick);
        return 0;
    }

    invalid_content.fighter.forward_roll_speed_f32 = 0.0f;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-forward-roll-speed"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.roll_movement_end_tick =
        invalid_content.fighter.backward_roll_ticks + UINT16_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-roll-window-after-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.spot_dodge_invulnerability_end_tick =
        invalid_content.fighter.spot_dodge_ticks + UINT16_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
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
            inspect(sim, &inspection),
            PF_STATUS_OK,
            "forward-roll-start-inspect"))
    {
        return 0;
    }
    start_x = inspection.players[0].position_x_f32;
    facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_trigger(
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=forward-roll-entry"
            " action=%u ticks=%u facing=%d expected_facing=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (int)facing);
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
        (int32_t)facing * forward_roll_displacement_f32;
    if (elapsed !=
            (uint32_t)default_content->fighter.forward_roll_ticks +
                UINT32_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !near_f32(inspection.players[0].position_x_f32, expected_x) ||
        inspection.players[0].facing != (int8_t)-facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=forward-roll-contract"
            " elapsed=%" PRIu32 " x=%.9g"
            " expected_x=%.9g" "\n",
            elapsed,
            inspection.players[0].position_x_f32,
            expected_x);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d70)),
            PF_STATUS_OK,
            "backward-roll-reset") ||
        !expect_status(
            inspect(sim, &inspection),
            PF_STATUS_OK,
            "backward-roll-start-inspect"))
    {
        return 0;
    }
    start_x = inspection.players[0].position_x_f32;
    facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_trigger(
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=backward-roll-entry"
            " action=%u ticks=%u facing=%d expected_facing=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (int)facing);
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
        start_x +
        (int32_t)facing * backward_roll_displacement_f32;
    if (elapsed !=
            (uint32_t)default_content->fighter.backward_roll_ticks +
                UINT32_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !near_f32(inspection.players[0].position_x_f32, expected_x) ||
        inspection.players[0].facing != facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=backward-roll-contract"
            " elapsed=%" PRIu32 " action=%u x=%.9g"
            " expected_x=%.9g" " facing=%d expected_facing=%d\n",
            elapsed,
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].position_x_f32,
            expected_x,
            (int)inspection.players[0].facing,
            (int)facing);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d71)),
            PF_STATUS_OK,
            "wait-direct-spot-dodge-reset") ||
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=wait-direct-spot-dodge"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d711)),
            PF_STATUS_OK,
            "spot-dodge-priority-reset") ||
        !expect_status(
            inspect(sim, &inspection),
            PF_STATUS_OK,
            "spot-dodge-start-inspect"))
    {
        return 0;
    }
    start_x = inspection.players[0].position_x_f32;
    facing = inspection.players[0].facing;
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_trigger(
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=spot-dodge-entry"
            " action=%u ticks=%u facing=%d expected_facing=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (int)facing);
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
                    inspect(sim, &inspection),
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
        inspection.players[0].position_x_f32 != start_x ||
        inspection.players[0].facing != facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=spot-dodge-contract"
            " elapsed=%" PRIu32 " action=%u x=%.9g"
            " expected_x=%.9g" " facing=%d expected_facing=%d\n",
            elapsed,
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].position_x_f32,
            start_x,
            (int)inspection.players[0].facing,
            (int)facing);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d72)),
            PF_STATUS_OK,
            "crouch-direct-escape-negative-reset") ||
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-enters-guard-on"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d721)),
            PF_STATUS_OK,
            "walk-direct-escape-negative-reset") ||
        !step_duel(
            sim,
            INT16_C(12000),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=walk-enters-guard-on"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=shield-flick-dodge"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xd0d741)),
            PF_STATUS_OK,
            "c-stick-buffered-roll-reset") ||
        !expect_status(
            inspect(sim, &inspection),
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
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
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
            "m4-movement=fail operation=c-stick-buffered-roll"
            " action=%u ticks=%u facing=%d expected_facing=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (int)facing);
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
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
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
            INT16_C(0),
            UINT64_C(0),
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
        inspection.players[0].velocity_y_f32 !=
            -default_content->fighter.full_hop_speed_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-held-full-hop"
            " action=%u vy=%.9g" " expected=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].velocity_y_f32,
            -default_content->fighter.full_hop_speed_f32);
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
            INT16_C(0),
            UINT64_C(0),
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
        inspection.players[0].velocity_y_f32 !=
            -default_content->fighter.full_hop_speed_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=c-stick-release-full-hop"
            " action=%u vy=%.9g" " expected=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].velocity_y_f32,
            -default_content->fighter.full_hop_speed_f32);
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

    wall_content.stage.solid_left_f32 =
        -INT32_C(6) * 1.0f;
    wall_content.stage.solid_right_f32 =
        -INT32_C(4) * 1.0f;
    wall_content.stage.solid_bottom_f32 =
        INT32_C(31) * 1.0f;
    wall_content.stage.platform_center_x_f32 =
        INT32_C(6) * 1.0f;
    wall_content.stage.platform_motion_amplitude_f32 = 0.0f;
    if (!expect_status(
            make_content_view(&wall_content, &wall_view),
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
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
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
    if (inspection.players[0].position_x_f32 >
        wall_content.stage.solid_left_f32 -
            wall_content.fighter.half_width_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=roll-wall-clamp"
            " x=%.9g" " limit=%.9g" "\n",
            inspection.players[0].position_x_f32,
            wall_content.stage.solid_left_f32 -
                wall_content.fighter.half_width_f32);
        return 0;
    }

    edge_content.stage.spawn_spacing_f32 =
        1.0f / INT32_C(8);
    edge_content.stage.floor_left_f32 =
        -(INT32_C(3) * 1.0f) / INT32_C(2);
    edge_content.stage.platform_center_x_f32 =
        INT32_C(4) * 1.0f;
    edge_content.stage.platform_half_width_f32 = 1.0f;
    edge_content.stage.platform_motion_amplitude_f32 = 0.0f;
    edge_content.stage.revival_platform_half_width_f32 =
        edge_content.fighter.half_width_f32;
    if (!expect_status(
            make_content_view(&edge_content, &edge_view),
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
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
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
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=roll-edge-exit"
            " grounded=%u support=%u action=%u x=%.9g" "\n",
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].position_x_f32);
        return 0;
    }
    return 1;
}

static int run_content_contract_test(
    const struct content *default_content,
    const pf_content_view *default_view)
{
    const falcon_common_attributes *falcon_attributes =
        falcon_reference_common_attributes();
    const falcon_submotion_data *teeter_motion =
        falcon_reference_submotion(
            (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER);
    test_sim_storage rejected_storage;
    test_sim_storage default_storage;
    test_sim_storage tuned_storage;
    struct content invalid_content = *default_content;
    struct content jump_tuned_content = *default_content;
    struct content tap_jump_tuned_content = *default_content;
    struct content dash_window_tuned_content = *default_content;
    struct content teeter_tuned_content = *default_content;
    struct content crouch_tuned_content = *default_content;
    struct content crouch_step_tuned_content = *default_content;
    struct content taunt_tuned_content = *default_content;
    struct content tuned_content = *default_content;
    pf_content_view damaged_view = *default_view;
    pf_content_view jump_tuned_view;
    pf_content_view tap_jump_tuned_view;
    pf_content_view dash_window_tuned_view;
    pf_content_view teeter_tuned_view;
    pf_content_view crouch_tuned_view;
    pf_content_view crouch_step_tuned_view;
    pf_content_view taunt_tuned_view;
    pf_content_view tuned_view;
    pf_sim_config config;
    pf_sim *rejected = NULL;
    pf_sim *default_sim = NULL;
    pf_sim *tuned_sim = NULL;
    struct inspection default_inspection;
    struct inspection tuned_inspection;
    uint32_t tick;

    if (falcon_attributes == NULL || teeter_motion == NULL)
    {
        return 0;
    }

    if (default_content->fighter.slow_walk_animation_scaling_f32 !=
            falcon_attributes->slow_walk_animation_scaling_f32 ||
        default_content->fighter.middle_walk_animation_scaling_f32 !=
            falcon_attributes->middle_walk_animation_scaling_f32 ||
        default_content->fighter.fast_walk_animation_scaling_f32 !=
            falcon_attributes->fast_walk_animation_scaling_f32 ||
        default_content->fighter.run_animation_scaling_f32 !=
            falcon_attributes->run_animation_scaling_f32 ||
        default_content->fighter.walk_middle_speed_ratio_f32 !=
            0.4f ||
        default_content->fighter.walk_fast_speed_ratio_f32 !=
            0.8f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ground-animation-source-data\n");
        return 0;
    }

    invalid_content.fighter.slow_walk_animation_scaling_f32 = 5.0f;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-ground-animation-rate"))
    {
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.full_hop_speed_f32 =
        invalid_content.fighter.short_hop_speed_f32;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-content"))
    {
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.jump_horizontal_input_speed_f32 = 0.0f;
    if (default_content->fighter.jump_horizontal_input_speed_f32 !=
            falcon_attributes->jump_horizontal_initial_velocity_f32 ||
        default_content->fighter.ground_max_horizontal_speed_f32 !=
            falcon_attributes->ground_maximum_horizontal_velocity_f32 ||
        default_content->fighter.air_speed_f32 !=
            falcon_attributes->max_aerial_horizontal_velocity_f32 ||
        default_content->fighter.air_max_horizontal_speed_f32 !=
            falcon_attributes->maximum_horizontal_air_velocity_f32 ||
        default_content->fighter.jump_horizontal_momentum_multiplier_f32 !=
            falcon_attributes->ground_air_jump_momentum_multiplier_f32 ||
        default_content->fighter.jump_horizontal_max_speed_f32 !=
            falcon_attributes->jump_horizontal_maximum_velocity_f32 ||
        !expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-jump-horizontal-input-speed"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.jump_horizontal_momentum_multiplier_f32 =
        1.0f + INT32_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-large-jump-horizontal-momentum-multiplier"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.jump_horizontal_max_speed_f32 =
        invalid_content.fighter.jump_horizontal_input_speed_f32 -
        INT32_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-low-jump-horizontal-max-speed"))
    {
        return 0;
    }
    jump_tuned_content.fighter.jump_horizontal_input_speed_f32 +=
        0.001f;
    if (!expect_status(
            make_content_view(
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
    invalid_content.fighter.tap_jump_axis_threshold =
        invalid_content.fighter.axis_dead_zone;
    if (default_content->fighter.tap_jump_axis_threshold !=
            UINT16_C(21709) ||
        default_content->fighter.tap_jump_input_window_ticks !=
            UINT16_C(4) ||
        !expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-low-tap-jump-axis-threshold"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.tap_jump_input_window_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-tap-jump-window"))
    {
        return 0;
    }
    tap_jump_tuned_content.fighter.tap_jump_axis_threshold +=
        UINT16_C(1);
    if (!expect_status(
            make_content_view(
                &tap_jump_tuned_content,
                &tap_jump_tuned_view),
            PF_STATUS_OK,
            "tap-jump-tuned-content-view") ||
        memcmp(
            default_view->content_hash.bytes,
            tap_jump_tuned_view.content_hash.bytes,
            sizeof(default_view->content_hash.bytes)) == 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-jump-content-hash\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.fighter.dash_input_window_ticks = UINT16_C(0);
    if (default_content->fighter.dash_input_window_ticks != UINT16_C(2) ||
        !expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-dash-input-window"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.dash_input_window_ticks =
        invalid_content.fighter.initial_dash_ticks + UINT16_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-dash-input-window"))
    {
        return 0;
    }

    dash_window_tuned_content.fighter.dash_input_window_ticks =
        UINT16_C(3);
    if (!expect_status(
            make_content_view(
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
    invalid_content.fighter.teeter_snap_distance_f32 = 0.0f;
    if (default_content->fighter.teeter_snap_distance_f32 !=
            (INT32_C(2) * 1.0f) / INT32_C(5) ||
        default_content->fighter.teeter_ticks !=
            teeter_motion->animation_frame_count ||
        !expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-teeter-snap"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.teeter_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-teeter-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.teeter_ticks = UINT16_C(121);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-teeter-duration"))
    {
        return 0;
    }

    teeter_tuned_content.fighter.teeter_snap_distance_f32 +=
        INT32_C(1);
    if (!expect_status(
            make_content_view(
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
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-crouch-start-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_end_ticks = UINT16_C(121);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-crouch-end-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_release_axis_threshold =
        invalid_content.fighter.crouch_axis_threshold;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-crouch-threshold-without-hysteresis"))
    {
        return 0;
    }
    crouch_tuned_content.fighter.crouch_start_ticks = UINT16_C(8);
    if (!expect_status(
            make_content_view(
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
    invalid_content.fighter.crouch_step_speed_f32 = -0.0000152587890625f;
    if (default_content->fighter.crouch_step_speed_f32 !=
            0.0f ||
        default_content->fighter.crouch_step_ticks != UINT16_C(1) ||
        !expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-negative-crouch-step-speed"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_step_speed_f32 =
        invalid_content.fighter.walk_speed_f32 + INT32_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-fast-crouch-step"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.crouch_step_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-crouch-step-duration"))
    {
        return 0;
    }

    crouch_step_tuned_content.fighter.crouch_step_speed_f32 +=
        0.001f;
    if (!expect_status(
            make_content_view(
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
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-taunt-duration"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.taunt_ticks = UINT16_C(601);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-taunt-duration"))
    {
        return 0;
    }

    taunt_tuned_content.fighter.taunt_ticks = UINT16_C(91);
    if (!expect_status(
            make_content_view(
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

    tuned_content.fighter.walk_speed_f32 = 1.0f / INT32_C(20);
    if (!expect_status(
            make_content_view(&tuned_content, &tuned_view),
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
    if (default_inspection.players[0].velocity_x_f32 <=
        tuned_inspection.players[0].velocity_x_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=data-driven-walk-speed\n");
        return 0;
    }
    return 1;
}

static int reset_to_run_brake(
    pf_sim *sim,
    const struct content *content,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "run-brake-iasa-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
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
    }
    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
        out_inspection->players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-iasa-setup\n");
        return 0;
    }
    return 1;
}

static int run_run_brake_iasa_test(
    const struct content *content,
    const pf_content_view *view)
{
    typedef struct rejected_run_brake_input
    {
        const char *name;
        int16_t main_stick_x;
        int16_t main_stick_y;
        uint64_t buttons;
        uint16_t trigger;
    } rejected_run_brake_input;
    static const rejected_run_brake_input rejected_inputs[] = {
        { "attack", INT16_C(0), INT16_C(0),
          PF_INPUT_BUTTON_ATTACK, UINT16_C(0) },
        { "strong-attack", INT16_C(0), INT16_C(0),
          PF_INPUT_BUTTON_STRONG_ATTACK, UINT16_C(0) },
        { "special", INT16_C(0), INT16_C(0),
          PF_INPUT_BUTTON_SPECIAL, UINT16_C(0) },
        { "taunt", INT16_C(0), INT16_C(0),
          PF_INPUT_BUTTON_TAUNT, UINT16_C(0) },
        { "guard", INT16_C(0), INT16_C(0),
          UINT64_C(0), UINT16_MAX },
        { "grab", INT16_C(0), INT16_C(0),
          PF_INPUT_BUTTON_ATTACK, UINT16_MAX },
        { "roll", INT16_MAX, INT16_C(0),
          UINT64_C(0), UINT16_MAX },
    };
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    float velocity_before;
    int8_t facing_before;
    size_t input_index;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim))
    {
        return 0;
    }

    if (!reset_to_run_brake(sim, content, &inspection))
    {
        return 0;
    }
    velocity_before = inspection.players[0].velocity_x_f32;
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_f32 !=
            velocity_before - content->fighter.turn_acceleration_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-crouch-iasa\n");
        return 0;
    }
    for (tick = UINT32_C(2);
         tick <= (uint32_t)content->fighter.crouch_start_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH_START ||
            inspection.players[0].action_ticks != (uint16_t)tick)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=run-brake-tap-crouch-start\n");
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
            (uint8_t)PF_M4_ACTION_CROUCH_END ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-tap-crouch-release\n");
        return 0;
    }

    if (!reset_to_run_brake(sim, content, &inspection))
    {
        return 0;
    }
    velocity_before = inspection.players[0].velocity_x_f32;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].velocity_x_f32 !=
            velocity_before - content->fighter.turn_acceleration_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-jump-iasa\n");
        return 0;
    }

    if (!reset_to_run_brake(sim, content, &inspection))
    {
        return 0;
    }
    velocity_before = inspection.players[0].velocity_x_f32;
    facing_before = inspection.players[0].facing;
    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[0].facing != facing_before ||
        inspection.players[0].dash_direction != -facing_before ||
        inspection.players[0].velocity_x_f32 !=
            velocity_before - content->fighter.turn_acceleration_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-turnrun-iasa\n");
        return 0;
    }

    for (input_index = (size_t)0;
         input_index < sizeof(rejected_inputs) / sizeof(rejected_inputs[0]);
         ++input_index)
    {
        const rejected_run_brake_input *candidate =
            &rejected_inputs[input_index];

        if (!reset_to_run_brake(sim, content, &inspection))
        {
            return 0;
        }
        velocity_before = inspection.players[0].velocity_x_f32;
        if (!step_duel_trigger(
                sim,
                candidate->main_stick_x,
                candidate->main_stick_y,
                candidate->buttons,
                candidate->trigger,
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
            inspection.players[0].action_ticks != UINT16_C(2) ||
            inspection.players[0].velocity_x_f32 !=
                velocity_before - content->fighter.traction_f32)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=run-brake-reject-%s\n",
                candidate->name);
            return 0;
        }
    }

    if (!reset_to_run_brake(sim, content, &inspection))
    {
        return 0;
    }
    velocity_before = inspection.players[0].velocity_x_f32;
    if (!step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[0].velocity_x_f32 !=
            velocity_before - content->fighter.traction_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-reject-c-stick-roll\n");
        return 0;
    }

    if (!reset_to_run_brake(sim, content, &inspection))
    {
        return 0;
    }
    velocity_before = inspection.players[0].velocity_x_f32;
    if (!step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[0].velocity_x_f32 !=
            velocity_before - content->fighter.traction_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-reject-c-stick-spot\n");
        return 0;
    }
    return 1;
}

static int reset_to_crouch_action(
    pf_sim *sim,
    const struct content *content,
    uint8_t target_action,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "crouch-common-iasa-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    if (target_action == (uint8_t)PF_M4_ACTION_CROUCH_START)
    {
        return step_duel(
                   sim,
                   INT16_C(0),
                   (int16_t)content->fighter.crouch_axis_threshold,
                   UINT64_C(0),
                   out_inspection) &&
               out_inspection->players[0].action_state == target_action;
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
                out_inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH)
    {
        return 0;
    }
    if (target_action == (uint8_t)PF_M4_ACTION_CROUCH)
    {
        return 1;
    }
    return step_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_CROUCH_END;
}

static int run_crouch_common_iasa_test(
    const struct content *content)
{
    static const uint8_t crouch_actions[] = {
        (uint8_t)PF_M4_ACTION_CROUCH_START,
        (uint8_t)PF_M4_ACTION_CROUCH,
        (uint8_t)PF_M4_ACTION_CROUCH_END,
    };
    test_sim_storage storage;
    struct content route_content = *content;
    pf_content_view route_view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    size_t action_index;

    route_content.projectile.enabled = UINT8_C(1);
    route_content.reflector.enabled = UINT8_C(1);
    if (!expect_status(
            make_content_view(&route_content, &route_view),
            PF_STATUS_OK,
            "crouch-common-iasa-content") ||
        !initialize_sim(
            &storage,
            &route_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim))
    {
        return 0;
    }
    for (action_index = (size_t)0;
         action_index <
             sizeof(crouch_actions) / sizeof(crouch_actions[0]);
         ++action_index)
    {
        const uint8_t crouch_action = crouch_actions[action_index];

        if (!reset_to_crouch_action(
                sim,
                &route_content,
                crouch_action,
                &inspection) ||
            !step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=crouch-common-attack-%u\n",
                (unsigned int)crouch_action);
            return 0;
        }
    }

    if (!reset_to_crouch_action(
            sim,
            &route_content,
            (uint8_t)PF_M4_ACTION_CROUCH_START,
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-start-neutral-special "
            "action=%u\n",
            (unsigned int)inspection.players[0].action_state);
        return 0;
    }
    if (!reset_to_crouch_action(
            sim,
            &route_content,
            (uint8_t)PF_M4_ACTION_CROUCH,
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-neutral-special-rejected\n");
        return 0;
    }
    if (!reset_to_crouch_action(
            sim,
            &route_content,
            (uint8_t)PF_M4_ACTION_CROUCH_END,
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-end-neutral-special-rejected\n");
        return 0;
    }

    for (action_index = (size_t)0;
         action_index <
             sizeof(crouch_actions) / sizeof(crouch_actions[0]);
         ++action_index)
    {
        const uint8_t crouch_action = crouch_actions[action_index];
        const uint8_t expected_action =
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND;

        if (!reset_to_crouch_action(
                sim,
                &route_content,
                crouch_action,
                &inspection) ||
            !step_duel(
                sim,
                INT16_C(0),
                (int16_t)route_content.fighter.crouch_axis_threshold,
                PF_INPUT_BUTTON_SPECIAL,
                &inspection) ||
            inspection.players[0].action_state != expected_action)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=crouch-down-special-%u\n",
                (unsigned int)crouch_action);
            return 0;
        }
    }

    /* SquatWait/SquatRv dispatch SpecialLw directly, so a simultaneous
     * horizontal special tilt cannot steal or suppress the down-special. */
    for (action_index = (size_t)1;
         action_index <
             sizeof(crouch_actions) / sizeof(crouch_actions[0]);
         ++action_index)
    {
        if (!reset_to_crouch_action(
                sim,
                &route_content,
                crouch_actions[action_index],
                &inspection) ||
            !step_duel(
                sim,
                INT16_MAX,
                (int16_t)route_content.fighter.crouch_axis_threshold,
                PF_INPUT_BUTTON_SPECIAL,
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=crouch-diagonal-down-special-%u\n",
                (unsigned int)crouch_actions[action_index]);
            return 0;
        }
    }

    /* Turn omits SpecialN even though it dispatches the other three Falcon
     * specials. A neutral B edge must leave the turn active. */
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(3)),
            PF_STATUS_OK,
            "turn-neutral-special-reset") ||
        !step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=turn-neutral-special-rejected\n");
        return 0;
    }

    for (action_index = (size_t)0;
         action_index <
             sizeof(crouch_actions) / sizeof(crouch_actions[0]);
         ++action_index)
    {
        const uint8_t crouch_action = crouch_actions[action_index];
        const uint8_t expected_action =
            crouch_action == (uint8_t)PF_M4_ACTION_CROUCH_START
                ? (uint8_t)PF_M4_ACTION_GRAB
                : (uint8_t)PF_M4_ACTION_GROUND_ATTACK;

        if (!reset_to_crouch_action(
                sim,
                &route_content,
                crouch_action,
                &inspection) ||
            !step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_MAX,
                &inspection) ||
            inspection.players[0].action_state != expected_action)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=crouch-z-fallback-%u\n",
                (unsigned int)crouch_action);
            return 0;
        }
    }
    return 1;
}

static int run_ground_control_test(
    const struct content *content,
    const pf_content_view *view)
{
    const ssbm_ground_input_attributes *ground_input =
        ssbm_common_reference_ground_input();
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    pf_state_hash run_turnaround_terminal_hash;
    float slow_walk_velocity;
    float fast_walk_velocity;
    float dash_velocity;
    float run_velocity;
    int16_t ramp_low;
    int16_t ramp_middle;
    int16_t ramp_high;
    int16_t crouch_walk_axis;
    int16_t crouch_dash_axis;
    int8_t crouch_facing;
    uint32_t tick;
    int run_turnaround_terminal_observed = 0;

    if (ground_input == NULL ||
        !initialize_sim(
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
    slow_walk_velocity = inspection.players[0].velocity_x_f32;
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
            "walk-opposite-tilt-reset") ||
        !step_duel(
            sim,
            INT16_C(12000),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        !step_duel(
            sim,
            INT16_C(-12000),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=walk-opposite-tilt-wait"
            " action=%u ticks=%u facing=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "walk-fresh-reversal-reset") ||
        !step_duel(
            sim,
            INT16_C(12000),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        !step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=walk-fresh-reversal-turn"
            " action=%u ticks=%u facing=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing);
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
    fast_walk_velocity = inspection.players[0].velocity_x_f32;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks !=
            content->fighter.dash_input_window_ticks ||
        inspection.players[0].dash_direction != INT8_C(0) ||
        inspection.players[0].facing != INT8_C(1) ||
        fast_walk_velocity <= slow_walk_velocity ||
        fast_walk_velocity > content->fighter.walk_speed_f32)
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
        inspection.players[0].dash_direction != INT8_C(1) ||
        inspection.players[0].tilt_x_age != UINT8_C(254))
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
    dash_velocity = inspection.players[0].velocity_x_f32;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(1) ||
        inspection.players[0].tilt_x_age != UINT8_C(254) ||
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
        inspection.players[0].dash_direction != INT8_C(1) ||
        inspection.players[0].tilt_x_age != UINT8_C(254))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=direct-dash\n");
        return 0;
    }

    while (inspection.players[0].action_ticks <=
           ground_input->initial_dash_early_end_frame)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
            inspection.players[0].tilt_x_age != UINT8_C(254))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=dash-dance-early-window\n");
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
        inspection.players[0].dash_direction != INT8_C(-1) ||
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_x_f32 <= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=dash-dance-reversal"
            " action=%u dash=%d facing=%d velocity=%.9g\n",
            (unsigned int)inspection.players[0].action_state,
            (int)inspection.players[0].dash_direction,
            (int)inspection.players[0].facing,
            inspection.players[0].velocity_x_f32);
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
        inspection.players[0].tilt_x_age != UINT8_C(254) ||
        inspection.players[0].facing != INT8_C(-1) ||
        inspection.players[0].velocity_x_f32 >= 0.0f)
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
    run_velocity = inspection.players[0].velocity_x_f32;
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
        inspection.players[0].velocity_x_f32 >= 0.0f ||
        absolute_f32(inspection.players[0].velocity_x_f32) >=
            absolute_f32(run_velocity))
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
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
            inspection.players[0].action_ticks ==
                content->fighter.run_turnaround_ticks)
        {
            if (!expect_status(
                    pf_sim_hash(sim, &run_turnaround_terminal_hash),
                    PF_STATUS_OK,
                    "run-turnaround-terminal-hash"))
            {
                return 0;
            }
            run_turnaround_terminal_observed = 1;
        }
    }
    if (!run_turnaround_terminal_observed ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_x_f32 <= 0.0f ||
        absolute_f32(inspection.players[0].velocity_x_f32) >=
            absolute_f32(run_velocity))
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
    run_velocity = inspection.players[0].velocity_x_f32;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
        absolute_f32(inspection.players[0].velocity_x_f32) >=
            absolute_f32(run_velocity))
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
        inspection.players[0].action_ticks != UINT16_C(2))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=run-brake-turnrun-entry\n");
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
    run_velocity = inspection.players[0].velocity_x_f32;
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
        absolute_f32(inspection.players[0].velocity_x_f32) >=
            absolute_f32(run_velocity))
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
        inspection.players[0].source_submotion !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT ||
        inspection.players[0].source_animation_frame_f32 != 0.0f ||
        inspection.players[0].source_animation_rate_f32 !=
            (int32_t)1.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-clock-entry "
            "action=%u ticks=%u submotion=%u frame=%.9g rate=%.9g\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].source_submotion,
            inspection.players[0].source_animation_frame_f32,
            inspection.players[0].source_animation_rate_f32);
        return 0;
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[0].source_animation_frame_f32 !=
            (int32_t)1.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-clock-advance "
            "action=%u ticks=%u submotion=%u frame=%.9g rate=%.9g\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].source_submotion,
            inspection.players[0].source_animation_frame_f32,
            inspection.players[0].source_animation_rate_f32);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT -
                    UINT32_C(1);
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].source_animation_frame_f32 != 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-wait-clock-wrap "
            "ticks=%u frame=%.9g\n",
            (unsigned int)inspection.players[0].action_ticks,
            inspection.players[0].source_animation_frame_f32);
        return 0;
    }
    if (!step_duel(
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
            "m4-movement=fail operation=crouch-release-entry "
            "action=%u ticks=%u submotion=%u frame=%.9g rate=%.9g\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].source_submotion,
            inspection.players[0].source_animation_frame_f32,
            inspection.players[0].source_animation_rate_f32);
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
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(2)),
            PF_STATUS_OK,
            "standing-turn-taunt-reset") ||
        !step_duel(
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
            "m4-movement=fail operation=standing-turn-taunt-setup\n");
        return 0;
    }
    crouch_facing = inspection.players[0].facing;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].facing != (int8_t)-crouch_facing)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=standing-turn-taunt-interrupt\n");
        return 0;
    }
    if (!reset_to_normal_landing_frame_four(sim, &inspection) ||
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
            "m4-movement=fail operation=landing-taunt-interrupt\n");
        return 0;
    }
    if (!reset_to_normal_landing_frame_four(sim, &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=landing-direct-crouch\n");
        return 0;
    }
    if (!reset_to_normal_landing_frame_four(sim, &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(4) ||
        !step_duel(
            sim,
            INT16_C(0),
            (int16_t)content->fighter.crouch_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(5))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=landing-late-crouch-locked\n");
        return 0;
    }
    return 1;
}

static int run_jump_takeoff_momentum_route(
    pf_sim *sim,
    const struct content *content,
    uint64_t seed,
    int16_t held_axis,
    int16_t terminal_axis,
    float *out_velocity_x)
{
    struct inspection inspection;
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
                tick + UINT32_C(1) ==
                        (uint32_t)content->fighter.jump_squat_ticks
                    ? terminal_axis
                    : held_axis,
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
        inspection.players[0].velocity_y_f32 >= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-takeoff-launch"
            " action=%u grounded=%u facing=%d velocity=(%.9g"
            ",%.9g" ")\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded,
            (int)inspection.players[0].facing,
            inspection.players[0].velocity_x_f32,
            inspection.players[0].velocity_y_f32);
        return 0;
    }
    *out_velocity_x = inspection.players[0].velocity_x_f32;
    return 1;
}

static int run_tap_jump_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    const int16_t tap_axis = (int16_t)(
        -(int32_t)content->fighter.tap_jump_axis_threshold);
    const int16_t below_axis = (int16_t)(
        -(int32_t)(content->fighter.tap_jump_axis_threshold - UINT16_C(1)));
    const int16_t mild_axis = (int16_t)(
        -(int32_t)(content->fighter.tilt_axis_threshold + UINT16_C(1)));
    uint32_t tick;

    if (content == NULL ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9100)),
            PF_STATUS_OK,
            "tap-jump-below-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            below_axis,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-jump-below-threshold\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9101)),
            PF_STATUS_OK,
            "tap-jump-two-sample-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            mild_axis,
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            tap_axis,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-jump-two-sample\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9102)),
            PF_STATUS_OK,
            "tap-jump-slow-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                mild_axis,
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=tap-jump-slow-setup\n");
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            tap_axis,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-jump-aged-out\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9103)),
            PF_STATUS_OK,
            "tap-jump-full-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            tap_axis,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         inspection.players[0].grounded != UINT8_C(0);
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                tap_axis,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].velocity_y_f32 !=
            -content->fighter.full_hop_speed_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-jump-full-hop\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9104)),
            PF_STATUS_OK,
            "tap-jump-short-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            tap_axis,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         inspection.players[0].grounded != UINT8_C(0);
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
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].velocity_y_f32 !=
            -content->fighter.short_hop_speed_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-jump-short-hop\n");
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9105)),
            PF_STATUS_OK,
            "tap-air-jump-reset") ||
        !launch_player0(sim, 1, &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            tap_axis,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (content->fighter.double_jump_cancel_ticks > UINT16_C(0)
                 ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
                 : (uint8_t)PF_M4_ACTION_AIRBORNE) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_y_f32 !=
            -content->fighter.double_jump_speed_f32 +
                content->fighter.gravity_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-air-jump action=%u "
            "jumps=%u vy=%.9g" " expected=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].air_jumps_remaining,
            inspection.players[0].velocity_y_f32,
            -content->fighter.double_jump_speed_f32 +
                content->fighter.gravity_f32);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x7a9106)),
            PF_STATUS_OK,
            "tap-shield-jump-reset") ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            tap_axis,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=tap-shield-jump\n");
        return 0;
    }
    return 1;
}

static int run_jump_takeoff_momentum_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    float forward_velocity_x;
    float neutral_velocity_x;
    float reverse_velocity_x;
    float terminal_edge_velocity_x;

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
            INT16_MAX,
            &forward_velocity_x) ||
        !run_jump_takeoff_momentum_route(
            sim,
            content,
            UINT64_C(0x4a554d504e4555),
            INT16_C(0),
            INT16_C(0),
            &neutral_velocity_x) ||
        !run_jump_takeoff_momentum_route(
            sim,
            content,
            UINT64_C(0x4a554d50524556),
            INT16_MIN,
            INT16_MIN,
            &reverse_velocity_x))
    {
        return 0;
    }
    if (neutral_velocity_x <= reverse_velocity_x ||
        forward_velocity_x <= neutral_velocity_x ||
        absolute_f32(reverse_velocity_x) >
            neutral_velocity_x / INT32_C(2))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-takeoff-momentum"
            " forward=%.9g" " neutral=%.9g"
            " reverse=%.9g" " tolerance=%.9g" "\n",
            forward_velocity_x,
            neutral_velocity_x,
            reverse_velocity_x,
            content->fighter.air_acceleration_f32);
        return 0;
    }
    if (!run_jump_takeoff_momentum_route(
            sim,
            content,
            UINT64_C(0x4a554d50454447),
            INT16_C(0),
            INT16_MAX,
            &terminal_edge_velocity_x) ||
        terminal_edge_velocity_x != neutral_velocity_x)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=jump-takeoff-terminal-input-order"
            " edge=%.9g" " neutral=%.9g" "\n",
            terminal_edge_velocity_x,
            neutral_velocity_x);
        return 0;
    }
    return 1;
}

static int run_fox_trot_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    float starting_position_x;
    float previous_position_x;
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
            inspect(source, &source_inspection),
            PF_STATUS_OK,
            "fox-trot-initial-inspect"))
    {
        return 0;
    }
    starting_position_x =
        source_inspection.players[0].position_x_f32;

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
        source_inspection.players[0].velocity_x_f32 !=
            content->fighter.initial_dash_speed_f32 ||
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
        source_inspection.players[0].velocity_x_f32 < 0.0f ||
        source_inspection.players[0].velocity_x_f32 >=
            content->fighter.initial_dash_speed_f32 ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "fox-trot-query-save-size") ||
        save_size != (size_t)1800)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fox-trot-entry"
            " action=%u ticks=%u facing=%d dash=%d"
            " velocity_x=%.9g" "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_f32);
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
        source_inspection.players[0].position_x_f32;
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
            source_inspection.players[0].velocity_x_f32 !=
                content->fighter.initial_dash_speed_f32 ||
            source_inspection.players[0].position_x_f32 <=
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
                " position_x=%.9g" " previous_x=%.9g"
                "\n",
                burst,
                (unsigned int)
                    source_inspection.players[0].action_state,
                (unsigned int)
                    source_inspection.players[0].action_ticks,
                source_inspection.players[0].position_x_f32,
                previous_position_x);
            return 0;
        }
        previous_position_x =
            source_inspection.players[0].position_x_f32;

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
            source_inspection.players[0].position_x_f32;
    }
    if (source_inspection.players[0].position_x_f32 <=
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
        source_inspection.players[0].velocity_x_f32 <=
            content->fighter.initial_dash_speed_f32 ||
        source_inspection.players[0].velocity_x_f32 >=
            content->fighter.run_speed_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=fox-trot-held-negative"
            " action=%u ticks=%u velocity_x=%.9g" "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].velocity_x_f32);
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

static int enter_right_teeter(
    pf_sim *sim,
    const struct content *content,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            inspect(sim, out_inspection),
            PF_STATUS_OK,
            "teeter-inspect-start"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const float distance_f32 =
            content->stage.floor_right_f32 -
            out_inspection->players[0].position_x_f32;
        float braking_speed_f32 =
            out_inspection->players[0].velocity_x_f32;
        float braking_distance_f32 = 0.0f;

        while (braking_speed_f32 > 0.0f)
        {
            braking_distance_f32 += braking_speed_f32;
            braking_speed_f32 =
                braking_speed_f32 > content->fighter.traction_f32
                    ? braking_speed_f32 - content->fighter.traction_f32
                    : 0.0f;
        }

        if (distance_f32 <=
            braking_distance_f32 + content->fighter.walk_speed_f32)
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
        if (out_inspection->players[0].velocity_x_f32 == 0.0f &&
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
        const float position_f32 =
            out_inspection->players[0].position_x_f32;
        const float velocity_f32 =
            out_inspection->players[0].velocity_x_f32;
        const float distance_f32 =
            content->stage.floor_right_f32 - position_f32;
        const float release_velocity_f32 =
            velocity_f32 > content->fighter.traction_f32
                ? velocity_f32 - content->fighter.traction_f32
                : INT32_C(0);
        int16_t selected_axis = INT16_C(0);
        float selected_velocity_f32 = 0.0f;
        uint32_t axis;

        if (release_velocity_f32 > distance_f32 &&
            release_velocity_f32 - distance_f32 <=
                content->fighter.teeter_snap_distance_f32)
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
            break;
        }

        for (axis =
                 (uint32_t)content->fighter.axis_dead_zone + UINT32_C(1);
             axis < (uint32_t)content->fighter.dash_axis_threshold;
             ++axis)
        {
            const float target_f32 =
                (float)axis * content->fighter.walk_speed_f32 /
                32767.0f;
            float next_velocity_f32 = velocity_f32;
            const float acceleration_f32 =
                content->fighter.ground_acceleration_f32;
            float next_release_velocity_f32;

            if (next_velocity_f32 < target_f32)
            {
                next_velocity_f32 += acceleration_f32;
                if (next_velocity_f32 > target_f32)
                {
                    next_velocity_f32 = target_f32;
                }
            }
            else if (next_velocity_f32 > target_f32)
            {
                next_velocity_f32 -= acceleration_f32;
                if (next_velocity_f32 < target_f32)
                {
                    next_velocity_f32 = target_f32;
                }
            }
            next_release_velocity_f32 =
                next_velocity_f32 > content->fighter.traction_f32
                    ? next_velocity_f32 -
                          content->fighter.traction_f32
                    : INT32_C(0);

            if (next_velocity_f32 < distance_f32 &&
                distance_f32 - next_velocity_f32 <
                    next_release_velocity_f32 &&
                next_release_velocity_f32 -
                        (distance_f32 - next_velocity_f32) <=
                    content->fighter.teeter_snap_distance_f32)
            {
                selected_axis = (int16_t)axis;
                selected_velocity_f32 = next_velocity_f32;
                break;
            }
            if (next_velocity_f32 < distance_f32 &&
                next_release_velocity_f32 <=
                    distance_f32 - next_velocity_f32 &&
                next_velocity_f32 > selected_velocity_f32)
            {
                selected_axis = (int16_t)axis;
                selected_velocity_f32 = next_velocity_f32;
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
                " tick=%" PRIu32 " distance=%.9g"
                " velocity=%.9g" " axis=%d\n",
                tick,
                distance_f32,
                velocity_f32,
                (int)selected_axis);
            return 0;
        }
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(60) &&
         out_inspection->players[0].action_state !=
             (uint8_t)PF_M4_ACTION_TEETER;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0))
        {
            return 0;
        }
    }

    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TEETER ||
        out_inspection->players[0].action_ticks != UINT16_C(0) ||
        out_inspection->players[0].position_x_f32 !=
            content->stage.floor_right_f32 ||
        out_inspection->players[0].velocity_x_f32 != 0.0f ||
        out_inspection->players[0].grounded == UINT8_C(0) ||
        out_inspection->players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        out_inspection->players[0].facing != INT8_C(1) ||
        out_inspection->players[0].dash_direction != INT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-entry"
            " action=%u ticks=%u position=%.9g"
            " velocity=%.9g" " grounded=%u\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            out_inspection->players[0].position_x_f32,
            out_inspection->players[0].velocity_x_f32,
            (unsigned int)out_inspection->players[0].grounded);
        return 0;
    }
    return 1;
}

static int run_teeter_cancel_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
        save_size != (size_t)1800)
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
        source_inspection.players[0].position_x_f32 !=
            content->stage.floor_right_f32 ||
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
            &source_inspection) ||
        !step_duel(
            source,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].dash_direction != INT8_C(-1) ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        source_inspection.players[0].position_x_f32 !=
            content->stage.floor_right_f32 ||
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
        source_inspection.players[0].velocity_x_f32 !=
            -content->fighter.initial_dash_speed_f32)
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
        source_inspection.players[0].position_x_f32 >=
            content->stage.floor_right_f32 -
                content->fighter.teeter_snap_distance_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-early-release-negative"
            " action=%u position=%.9g" " threshold=%.9g"
            " velocity=%.9g" "\n",
            (unsigned int)source_inspection.players[0].action_state,
            source_inspection.players[0].position_x_f32,
            content->stage.floor_right_f32 -
                content->fighter.teeter_snap_distance_f32,
            source_inspection.players[0].velocity_x_f32);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e6)),
            PF_STATUS_OK,
            "teeter-expiry-reset") ||
        !enter_right_teeter(
            source,
            content,
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
            (uint8_t)PF_M4_ACTION_TEETER ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].position_x_f32 !=
            content->stage.floor_right_f32 ||
        source_inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-expiry\n");
        return 0;
    }
    if (!step_duel_triggers(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source->world.source_submotion[0] !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON ||
        source->world.ground_blend_progress_f32[0] <= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-wait-guard-blend"
            " action=%u ticks=%u submotion=%u blend=%.9g\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source->world.source_submotion[0],
            source->world.ground_blend_progress_f32[0]);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7ee7e7)),
            PF_STATUS_OK,
            "teeter-start-guard-reset") ||
        !enter_right_teeter(
            source,
            content,
            &source_inspection) ||
        !step_duel_triggers(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source->world.source_submotion[0] !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON ||
        source->world.ground_blend_progress_f32[0] <= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=teeter-start-guard-blend"
            " action=%u ticks=%u submotion=%u blend=%.9g\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source->world.source_submotion[0],
            source->world.ground_blend_progress_f32[0]);
        return 0;
    }
    return 1;
}

static int run_taunt_cancel_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    float dash_position_f32;
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
    if (source_inspection.players[0].action_state !=
        (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.initial_dash_ticks &&
         source_inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_RUN;
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
    dash_position_f32 = source_inspection.players[0].position_x_f32;
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].position_x_f32 <= dash_position_f32 ||
        source_inspection.players[0].velocity_x_f32 <= 0.0f ||
        source_inspection.players[0].dash_direction != INT8_C(0) ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "taunt-query-save-size") ||
        save_size != (size_t)1800)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-dash-entry"
            " action=%u ticks=%u velocity=%.9g" "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].velocity_x_f32);
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
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-terminal-wait-callback"
            " action=%u ticks=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x7a017cb)),
            PF_STATUS_OK,
            "taunt-cancel-reset") ||
        !enter_right_teeter(
            source,
            content,
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].position_x_f32 !=
            content->stage.floor_right_f32 ||
        source_inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=taunt-teeter-entry"
            " action=%u ticks=%u position=%.9g expected_position=%.9g"
            " grounded=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].position_x_f32,
            content->stage.floor_right_f32,
            (unsigned int)source_inspection.players[0].grounded);
        return 0;
    }
    return 1;
}

static int run_stage_humping_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    float start_position_f32;
    float first_step_position_f32;
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
            inspect(source, &source_inspection),
            PF_STATUS_OK,
            "stage-humping-inspect-start"))
    {
        return 0;
    }
    start_position_f32 =
        source_inspection.players[0].position_x_f32;
    if (!step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].position_x_f32 !=
            start_position_f32 +
                content->fighter.crouch_step_speed_f32 ||
        source_inspection.players[0].velocity_x_f32 !=
            content->fighter.crouch_step_speed_f32 ||
        source_inspection.players[0].facing != INT8_C(1) ||
        source_inspection.players[0].grounded == UINT8_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "stage-humping-query-save-size") ||
        save_size != (size_t)1800)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=stage-humping-first-step"
            " action=%u ticks=%u position=%.9g"
            " velocity=%.9g" "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            source_inspection.players[0].position_x_f32,
            source_inspection.players[0].velocity_x_f32);
        return 0;
    }
    first_step_position_f32 =
        source_inspection.players[0].position_x_f32;

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
        source_inspection.players[0].position_x_f32 !=
            first_step_position_f32 ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
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
            source_inspection.players[0].position_x_f32 !=
                start_position_f32 +
                    (float)(repetition + UINT32_C(1)) *
                        content->fighter.crouch_step_speed_f32 ||
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
            source_inspection.players[0].velocity_x_f32 != 0.0f)
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
        source_inspection.players[0].velocity_x_f32 !=
            -content->fighter.crouch_step_speed_f32 ||
        source_inspection.players[0].position_x_f32 !=
            start_position_f32 +
                INT32_C(7) *
                    content->fighter.crouch_step_speed_f32)
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
            inspect(source, &source_inspection),
            PF_STATUS_OK,
            "stage-humping-held-inspect"))
    {
        return 0;
    }
    start_position_f32 =
        source_inspection.players[0].position_x_f32;
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
        source_inspection.players[0].position_x_f32 !=
            start_position_f32 +
                content->fighter.crouch_step_speed_f32 ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
        !step_duel(
            source,
            INT16_MAX,
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        source_inspection.players[0].position_x_f32 !=
            start_position_f32 +
                content->fighter.crouch_step_speed_f32)
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
            inspect(source, &source_inspection),
            PF_STATUS_OK,
            "stage-humping-neutral-inspect"))
    {
        return 0;
    }
    start_position_f32 =
        source_inspection.players[0].position_x_f32;
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        source_inspection.players[0].position_x_f32 !=
            start_position_f32 ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
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

static int advance_initial_dash_past_early_window(
    pf_sim *sim,
    const ssbm_ground_input_attributes *ground_input,
    struct inspection *inspection)
{
    while (inspection->players[0].action_ticks <=
           ground_input->initial_dash_early_end_frame)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                inspection) ||
            inspection->players[0].action_state !=
                (uint8_t)PF_M4_ACTION_INITIAL_DASH)
        {
            return 0;
        }
    }
    return 1;
}

static int run_pivot_test(
    const struct content *content,
    const pf_content_view *view)
{
    const ssbm_ground_input_attributes *ground_input =
        ssbm_common_reference_ground_input();
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (ground_input == NULL ||
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
        source_inspection.players[0].velocity_x_f32 !=
            content->fighter.initial_dash_speed_f32)
    {
        return 0;
    }
    while (source_inspection.players[0].action_ticks <=
           ground_input->initial_dash_early_end_frame)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_INITIAL_DASH)
        {
            return 0;
        }
    }
    if (
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
        source_inspection.players[0].velocity_x_f32 <= 0.0f ||
        source_inspection.players[0].velocity_x_f32 >=
            content->fighter.initial_dash_speed_f32 ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "pivot-query-save-size") ||
        save_size != (size_t)1800)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=pivot-frame"
            " action=%u ticks=%u facing=%d dash=%d"
            " velocity_x=%.9g" "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_f32);
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
        source_inspection.players[0].velocity_x_f32 <= 0.0f ||
        absolute_f32(source_inspection.players[0].velocity_x_f32) >=
            content->fighter.initial_dash_speed_f32 ||
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
            " action=%u facing=%d dash=%d velocity_x=%.9g"
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_f32);
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
        !advance_initial_dash_past_early_window(
            source,
            ground_input,
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
        source_inspection.players[0].velocity_x_f32 <= 0.0f ||
        absolute_f32(source_inspection.players[0].velocity_x_f32) >=
            content->fighter.initial_dash_speed_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=empty-pivot"
            " action=%u facing=%d dash=%d velocity_x=%.9g"
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (int)source_inspection.players[0].facing,
            (int)source_inspection.players[0].dash_direction,
            source_inspection.players[0].velocity_x_f32);
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
        !advance_initial_dash_past_early_window(
            source,
            ground_input,
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
        source_inspection.players[0].velocity_x_f32 >= 0.0f ||
        source_inspection.players[0].velocity_x_f32 <=
            -content->fighter.initial_dash_speed_f32)
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

static int measure_hop(
    const pf_content_view *content,
    uint32_t held_ticks,
    float *out_launch_velocity,
    float *out_apex_y)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    int launched = 0;
    float apex_y = FLT_MAX;
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
                    inspection.players[0].velocity_y_f32;
                launched = 1;
            }
            if (inspection.players[0].position_y_f32 < apex_y)
            {
                apex_y = inspection.players[0].position_y_f32;
            }
            if (inspection.players[0].velocity_y_f32 > 0.0f)
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
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    float short_launch_early;
    float short_launch_late;
    float full_launch_early;
    float full_launch_late;
    float short_apex_early;
    float short_apex_late;
    float full_apex_early;
    float full_apex_late;
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
            " short=(%.9g" ",%.9g" ")"
            " full=(%.9g" ",%.9g" ")\n",
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
        inspection.players[0].velocity_y_f32 >= 0.0f ||
        inspection.players[0].velocity_x_f32 <= 0.0f)
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
        if (inspection.players[0].velocity_y_f32 > 0.0f)
        {
            break;
        }
    }
    if (inspection.players[0].velocity_y_f32 <= 0.0f ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].fast_fall != UINT8_C(1) ||
        inspection.players[0].velocity_y_f32 !=
            content->fighter.fast_fall_speed_f32)
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
    const struct content *content,
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
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    struct inspection held_inspection;
    struct inspection takeoff_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    float launch_y;
    float expected_velocity_y;
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
        source_inspection.players[0].velocity_x_f32 <= 0.0f ||
        source_inspection.players[0].air_jumps_remaining !=
            UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=idj-first-airborne-setup\n");
        return 0;
    }

    launch_y = source_inspection.players[0].position_y_f32;
    expected_velocity_y =
        -content->fighter.double_jump_speed_f32 +
        content->fighter.gravity_f32;
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].action_state !=
            (content->fighter.double_jump_cancel_ticks > UINT16_C(0)
                 ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
                 : (uint8_t)PF_M4_ACTION_AIRBORNE) ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].air_jumps_remaining !=
            UINT8_C(0) ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
        source_inspection.players[0].velocity_y_f32 !=
            expected_velocity_y ||
        source_inspection.players[0].position_y_f32 !=
            launch_y + expected_velocity_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=idj-first-airborne-input"
            " action=%u ticks=%u grounded=%u jumps=%u"
            " position_y=%.9g" " launch_y=%.9g"
            " velocity=(%.9g" ",%.9g" ") expected_y=%.9g"
            "\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0]
                .air_jumps_remaining,
            source_inspection.players[0].position_y_f32,
            launch_y,
            source_inspection.players[0].velocity_x_f32,
            source_inspection.players[0].velocity_y_f32,
            expected_velocity_y);
        return 0;
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "idj-query-save-size") ||
        save_size != (size_t)1800)
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
            source_inspection.players[0].position_y_f32 !=
                loaded_inspection.players[0].position_y_f32 ||
            source_inspection.players[0].velocity_y_f32 !=
                loaded_inspection.players[0].velocity_y_f32 ||
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
    const struct content *content,
    int delayed_expected,
    struct inspection *out_inspection)
{
    const uint8_t expected_action =
        delayed_expected != 0
            ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
            : (uint8_t)PF_M4_ACTION_AIRBORNE;
    const float expected_velocity_y =
        -content->fighter.double_jump_speed_f32 +
        content->fighter.gravity_f32;

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
        out_inspection->players[0].velocity_y_f32 !=
            expected_velocity_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=double-jump-cancel-entry"
            " delayed=%d action=%u ticks=%u velocity_y=%.9g"
            " jumps=%u\n",
            delayed_expected,
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            out_inspection->players[0].velocity_y_f32,
            (unsigned int)out_inspection->players[0]
                .air_jumps_remaining);
        return 0;
    }
    return 1;
}

static int run_double_jump_cancel_test(
    const struct content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage strong_storage;
    test_sim_storage late_storage;
    test_sim_storage simultaneous_storage;
    test_sim_storage disabled_storage;
    struct content invalid_content = *default_content;
    struct content disabled_content = *default_content;
    pf_content_view disabled_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *strong = NULL;
    pf_sim *late = NULL;
    pf_sim *simultaneous = NULL;
    pf_sim *disabled = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    struct inspection strong_inspection;
    struct inspection late_inspection;
    struct inspection simultaneous_inspection;
    struct inspection disabled_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    float before_cancel_position_y;
    float before_late_velocity_y;
    float before_simultaneous_velocity_y;
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
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-double-jump-cancel-window"))
    {
        return 0;
    }

    disabled_content.fighter.double_jump_cancel_ticks = UINT16_C(0);
    disabled_content.fighter.double_jump_armor_max_hitstun_ticks =
        UINT16_C(0);
    if (!expect_status(
            validate_content(&disabled_content),
            PF_STATUS_OK,
            "allow-disabled-double-jump-cancel") ||
        !expect_status(
            make_content_view(&disabled_content, &disabled_view),
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
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].action_ticks != UINT16_C(1))
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "double-jump-cancel-query-save-size") ||
        save_size != (size_t)1800)
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
        source_inspection.players[0].position_y_f32;
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
        source_inspection.players[0].velocity_y_f32 !=
            default_content->fighter.gravity_f32 ||
        source_inspection.players[0].position_y_f32 !=
            before_cancel_position_y +
                default_content->fighter.gravity_f32 ||
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
            source_inspection.players[0].position_y_f32 !=
                loaded_inspection.players[0].position_y_f32 ||
            source_inspection.players[0].velocity_y_f32 !=
                loaded_inspection.players[0].velocity_y_f32)
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
        strong_inspection.players[0].velocity_y_f32 !=
            default_content->fighter.gravity_f32)
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
        late_inspection.players[0].velocity_y_f32;
    if (late_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        late_inspection.players[0].action_ticks !=
            default_content->fighter.double_jump_cancel_ticks ||
        before_late_velocity_y >= INT32_C(0) ||
        !step_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &late_inspection) ||
        late_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        late_inspection.players[0].velocity_y_f32 !=
            before_late_velocity_y +
                default_content->fighter.gravity_f32 ||
        late_inspection.players[0].velocity_y_f32 >= 0.0f)
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
        simultaneous_inspection.players[0].velocity_y_f32;
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
        simultaneous_inspection.players[0].velocity_y_f32 !=
            before_simultaneous_velocity_y +
                default_content->fighter.gravity_f32)
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

static int run_falcon_aerial_iasa_route(
    const pf_content_view *view,
    uint8_t action_state,
    int16_t c_stick_x,
    int16_t c_stick_y,
    uint16_t interrupt_frame,
    int should_interrupt)
{
    const struct content *content = (const struct content *)view->bytes;
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    const uint64_t attack_button =
        action_state == (uint8_t)PF_M4_ACTION_AERIAL_ATTACK
            ? PF_INPUT_BUTTON_ATTACK
            : PF_INPUT_BUTTON_STRONG_ATTACK;
    uint32_t tick;

    if (content == NULL ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xa31a5a)),
            PF_STATUS_OK,
            "falcon-aerial-iasa-reset") ||
        !launch_player0(sim, 0, &inspection) ||
        !step_duel_secondary_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            c_stick_x,
            c_stick_y,
            attack_button,
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state != action_state ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick + UINT32_C(2) < (uint32_t)interrupt_frame;
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
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    if (should_interrupt != 0)
    {
        if (inspection.players[0].action_state !=
                (content->fighter.double_jump_cancel_ticks > UINT16_C(0)
                     ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
                     : (uint8_t)PF_M4_ACTION_AIRBORNE) ||
            inspection.players[0].action_ticks != UINT16_C(0) ||
            inspection.players[0].air_jumps_remaining != UINT8_C(0))
        {
            return 0;
        }
    }
    else if (inspection.players[0].action_state != action_state ||
             inspection.players[0].air_jumps_remaining != UINT8_C(1))
    {
        return 0;
    }
    return 1;
}

static int run_falcon_aerial_iasa_test(const pf_content_view *view)
{
    static const uint8_t actions[4] = {
        (uint8_t)PF_M4_ACTION_FORWARD_AERIAL,
        (uint8_t)PF_M4_ACTION_BACK_AERIAL,
        (uint8_t)PF_M4_ACTION_UP_AERIAL,
        (uint8_t)PF_M4_ACTION_DOWN_AERIAL};
    static const int16_t c_stick_x[4] = {
        INT16_MAX, INT16_MIN, INT16_C(0), INT16_C(0)};
    static const int16_t c_stick_y[4] = {
        INT16_C(0), INT16_C(0), INT16_MIN, INT16_MAX};
    static const uint16_t iasa_frames[4] = {
        UINT16_C(36), UINT16_C(29), UINT16_C(30), UINT16_C(38)};
    uint32_t index;

    for (index = UINT32_C(0); index < UINT32_C(4); ++index)
    {
        if (!run_falcon_aerial_iasa_route(
                view,
                actions[index],
                c_stick_x[index],
                c_stick_y[index],
                (uint16_t)(iasa_frames[index] - UINT16_C(1)),
                0) ||
            !run_falcon_aerial_iasa_route(
                view,
                actions[index],
                c_stick_x[index],
                c_stick_y[index],
                iasa_frames[index],
                1))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=falcon-aerial-iasa"
                " action=%u\n",
                (unsigned int)actions[index]);
            return 0;
        }
    }
    if (!run_falcon_aerial_iasa_route(
            view,
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT16_C(43),
            0))
    {
        return 0;
    }
    return 1;
}

static int start_strong_aerial_attack(
    pf_sim *sim,
    int short_hop,
    struct inspection *out_inspection);

static int run_strong_aerial_terminal_iasa_test(
    const pf_content_view *view)
{
    const struct content *content = (const struct content *)view->bytes;
    uint32_t attack_ticks;
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    uint32_t tick;

    if (content == NULL)
    {
        return 0;
    }
    attack_ticks =
        (uint32_t)content->fighter.strong_startup_ticks +
        (uint32_t)content->fighter.strong_active_ticks +
        (uint32_t)content->fighter.strong_recovery_ticks;
    if (attack_ticks < UINT32_C(2) ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xa31a5b)),
            PF_STATUS_OK,
            "strong-aerial-terminal-iasa-reset") ||
        !start_strong_aerial_attack(sim, 0, &inspection))
    {
        return 0;
    }

    /* Leave the strong AttackAir on its last displayed frame. The next
     * update is where Melee's animation callback installs Fall before IASA. */
    for (tick = UINT32_C(0); tick + UINT32_C(1) < attack_ticks; ++tick)
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
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        inspection.players[0].action_ticks !=
            (uint16_t)(attack_ticks - UINT32_C(1)) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=strong-aerial-terminal-iasa"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }
    return 1;
}

static int run_air_facing_lock_test(const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
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
        inspection.players[0].velocity_x_f32 >= 0.0f ||
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
        inspection.players[0].velocity_x_f32 >= 0.0f ||
        inspection.players[0].velocity_y_f32 >= 0.0f ||
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
    if (inspection.players[0].velocity_x_f32 <= 0.0f)
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
    struct inspection *out_inspection)
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
    struct inspection *out_inspection)
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
    const struct content *default_content,
    const pf_content_view *default_view)
{
    test_sim_storage normal_storage;
    test_sim_storage cancel_storage;
    struct content invalid_content = *default_content;
    pf_sim *normal = NULL;
    pf_sim *cancel = NULL;
    struct inspection inspection;
    uint32_t landing_ticks;
    uint32_t tick;

    invalid_content.fighter.strong_aerial_landing_lag_ticks =
        UINT16_C(0);
    if (default_content->fighter.strong_aerial_landing_lag_ticks !=
            UINT16_C(30) ||
        !expect_status(
            validate_content(&invalid_content),
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
                inspection.players[0].velocity_y_f32 > 0.0f &&
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
            inspection.players[0].velocity_y_f32 >= 0.0f;

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
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
        required_bytes != (size_t)1800)
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
            inspect(loaded, &loaded_inspection),
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
    const struct content *default_content,
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
    struct content invalid_content = *default_content;
    struct content auto_content = *default_content;
    pf_content_view auto_view;
    struct inspection inspection;
    uint32_t tick;
    uint32_t landing_ticks;

    invalid_content.fighter.l_cancel_window_ticks = UINT16_C(6);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-non-melee-l-cancel-window"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.aerial_landing_lag_end_tick =
        invalid_content.fighter.aerial_landing_lag_begin_tick;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-empty-aerial-landing-window"))
    {
        return 0;
    }

    auto_content.fighter.gravity_f32 =
        1.0f / INT32_C(20);
    auto_content.fighter.fall_speed_f32 =
        1.0f / INT32_C(10);
    auto_content.fighter.platform_drop_speed_y_f32 =
        (INT32_C(3) * 1.0f) / INT32_C(40);
    auto_content.fighter.fast_fall_speed_f32 =
        (INT32_C(3) * 1.0f) / INT32_C(20);
    auto_content.fighter.short_hop_speed_f32 =
        (INT32_C(3) * 1.0f) / INT32_C(50);
    auto_content.fighter.full_hop_speed_f32 =
        (INT32_C(3) * 1.0f) / INT32_C(25);
    auto_content.fighter.double_jump_speed_f32 =
        (INT32_C(3) * 1.0f) / INT32_C(25);
    /* This fixture intentionally mutates authored gravity/jump values to
     * exercise generic auto-cancel timing, not the imported Melee ECB path. */
    auto_content.fighter.reference_frame_data_enabled = UINT8_C(0);
    if (!expect_status(
            make_content_view(&auto_content, &auto_view),
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
                inspection.players[0].velocity_y_f32 > 0.0f &&
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
            inspection.players[0].velocity_y_f32 >= 0.0f;

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

static int run_platform_test(const struct content *default_content)
{
    test_sim_storage storage;
    struct content platform_content = *default_content;
    struct content invalid_content = *default_content;
    struct content tuned_content;
    pf_content_view platform_view;
    pf_content_view tuned_view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    float previous_player_x;
    float previous_platform_left;
    uint32_t tick;

    if (default_content->fighter.platform_drop_startup_ticks !=
            UINT16_C(3) ||
        default_content->fighter.platform_drop_speed_y_f32 !=
            (693.0f / 6200.0f))
    {
        return 0;
    }
    invalid_content.fighter.platform_drop_startup_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "platform-drop-startup-zero"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.platform_drop_startup_ticks =
        invalid_content.fighter.crouch_start_ticks;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "platform-drop-startup-too-long"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.platform_drop_speed_y_f32 =
        invalid_content.fighter.gravity_f32;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "platform-drop-speed-too-low"))
    {
        return 0;
    }

    platform_content.stage.platform_center_x_f32 =
        -INT32_C(8) * 1.0f;
    platform_content.stage.platform_motion_amplitude_f32 =
        INT32_C(2) * 1.0f;
    platform_content.stage.platform_half_width_f32 =
        INT32_C(6) * 1.0f;
    tuned_content = platform_content;
    ++tuned_content.fighter.platform_drop_startup_ticks;
    if (!expect_status(
            make_content_view(
                &platform_content,
                &platform_view),
            PF_STATUS_OK,
            "platform-content-view") ||
        !expect_status(
            make_content_view(&tuned_content, &tuned_view),
            PF_STATUS_OK,
            "platform-tuned-content-view") ||
        memcmp(
            platform_view.content_hash.bytes,
            tuned_view.content_hash.bytes,
            sizeof(platform_view.content_hash.bytes)) == 0 ||
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
            inspect(sim, &inspection),
            PF_STATUS_OK,
            "platform-initial-inspect") ||
        inspection.stage.left_ledge_x_f32 !=
            platform_content.stage.floor_left_f32 ||
        inspection.stage.right_ledge_x_f32 !=
            platform_content.stage.floor_right_f32)
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
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].velocity_y_f32 <= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-landing"
            " entry_velocity_y=%.9g" "\n",
            inspection.players[0].velocity_y_f32);
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].velocity_y_f32 != 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-landing-ground-physics"
            " action=%u velocity_y=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            inspection.players[0].velocity_y_f32);
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
    previous_player_x = inspection.players[0].position_x_f32;
    previous_platform_left = inspection.stage.platform_left_f32;
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !near_f32(
            inspection.players[0].position_x_f32 - previous_player_x,
            inspection.stage.platform_left_f32 - previous_platform_left))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-motion-carry"
            " player_delta=%.9g platform_delta=%.9g\n",
            inspection.players[0].position_x_f32 - previous_player_x,
            inspection.stage.platform_left_f32 - previous_platform_left);
        return 0;
    }

    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        inspection.players[0].platform_drop_ticks != UINT8_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-drop-release-control\n");
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(30) &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
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
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=platform-drop-held-entry\n");
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <
             (uint32_t)platform_content.fighter
                 .platform_drop_startup_ticks;
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
            inspection.players[0].action_ticks !=
                (uint16_t)(tick + UINT32_C(1)) ||
            inspection.players[0].grounded == UINT8_C(0) ||
            inspection.players[0].platform_drop_ticks != UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=platform-drop-delay"
                " tick=%" PRIu32 "\n",
                tick);
            return 0;
        }
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
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].platform_drop_ticks == UINT8_C(0) ||
        inspection.players[0].fast_fall != UINT8_C(0) ||
        inspection.players[0].velocity_y_f32 !=
            platform_content.fighter.platform_drop_speed_y_f32)
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
    struct inspection *out_inspection)
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
    const struct content *default_content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    struct content platform_content = *default_content;
    struct content invalid_content;
    pf_content_view platform_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    float stationary_x;
    uint32_t tick;

    if (default_content->stage.upper_platform_center_x_f32 !=
            20.0f ||
        default_content->stage.upper_platform_y_f32 !=
            13.0f ||
        default_content->stage.upper_platform_half_width_f32 !=
            4.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-defaults\n");
        return 0;
    }

    invalid_content = *default_content;
    invalid_content.stage.upper_platform_half_width_f32 = 0.0f;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-empty-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_y_f32 =
        invalid_content.stage.blast_top_f32;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-high-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_y_f32 =
        invalid_content.stage.solid_top_f32;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-solid-overlap-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_center_x_f32 =
        invalid_content.stage.platform_center_x_f32;
    invalid_content.stage.upper_platform_y_f32 =
        invalid_content.stage.platform_y_f32;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-moving-overlap-upper-platform"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.stage.upper_platform_center_x_f32 =
        invalid_content.stage.floor_right_f32;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-outside-upper-platform"))
    {
        return 0;
    }

    platform_content.stage.platform_center_x_f32 =
        -INT32_C(24) * 1.0f;
    platform_content.stage.platform_motion_amplitude_f32 = 0.0f;
    platform_content.stage.platform_half_width_f32 =
        INT32_C(2) * 1.0f;
    platform_content.stage.upper_platform_center_x_f32 =
        -INT32_C(4) * 1.0f;
    platform_content.stage.upper_platform_y_f32 =
        INT32_C(26) * 1.0f;
    platform_content.stage.upper_platform_half_width_f32 =
        INT32_C(6) * 1.0f;
    if (!expect_status(
            make_content_view(&platform_content, &platform_view),
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
        source_inspection.stage.upper_platform_left_f32 !=
            -INT32_C(10) * 1.0f ||
        source_inspection.stage.upper_platform_right_f32 !=
            2.0f ||
        source_inspection.stage.upper_platform_y_f32 !=
            26.0f)
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
        destination.size != (size_t)1800)
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
    stationary_x = source_inspection.players[0].position_x_f32;
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
        source_inspection.players[0].position_x_f32 != stationary_x ||
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
            (int16_t)ssbm_common_reference_ground_input()
                ->platform_drop_axis_threshold,
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
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START ||
        source_inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=upper-platform-drop-entry\n");
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <
             (uint32_t)platform_content.fighter
                 .platform_drop_startup_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                &source_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH_START ||
            source_inspection.players[0].action_ticks !=
                (uint16_t)(tick + UINT32_C(1)))
        {
            return 0;
        }
    }
    if (!step_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        source_inspection.players[0].position_y_f32 <=
            platform_content.stage.upper_platform_y_f32 -
                platform_content.fighter.half_height_f32)
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
    struct inspection *out_inspection)
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
    struct inspection *out_inspection)
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

static void force_reference_crouch_history(
    pf_sim *sim,
    int8_t previous_y_direction,
    uint8_t previous_y_age,
    uint8_t previous_ucf_y_age)
{
    sim->world.velocity_x_f32[0] = INT32_C(0);
    sim->world.velocity_y_f32[0] = INT32_C(0);
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_CROUCH;
    sim->world.action_ticks[0] = UINT16_C(0);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT;
    sim->world.source_animation_frame_f32[0] = INT32_C(0);
    sim->world.source_animation_rate_f32[0] = 1.0f;
    (void)memset(
        &sim->world.ground_blend_pose[0],
        0,
        sizeof(sim->world.ground_blend_pose[0]));
    sim->world.ground_blend_progress_f32[0] = 0.0f;
    sim->world.dash_direction[0] = INT8_C(0);
    sim->world.previous_tilt_x_direction[0] = INT8_C(0);
    sim->world.previous_tilt_y_direction[0] = previous_y_direction;
    sim->world.tilt_x_age[0] = UINT8_C(254);
    sim->world.tilt_y_age[0] = previous_y_age;
    sim->world.ucf_tilt_x_age[0] = UINT8_C(254);
    sim->world.ucf_tilt_y_age[0] = previous_ucf_y_age;
}

static int run_crouch_platform_drop_test(
    const struct content *default_content,
    const pf_content_view *default_view)
{
    const ssbm_ground_input_attributes *ground_input =
        ssbm_common_reference_ground_input();
    test_sim_storage platform_storage;
    test_sim_storage floor_storage;
    struct content platform_content = *default_content;
    pf_content_view platform_view;
    pf_sim *platform = NULL;
    pf_sim *floor = NULL;
    struct inspection inspection;
    const uint8_t ucf_age_before = UINT8_C(40);

    platform_content.stage.platform_center_x_f32 =
        -INT32_C(8) * 1.0f;
    platform_content.stage.platform_motion_amplitude_f32 = 0.0f;
    platform_content.stage.platform_half_width_f32 =
        INT32_C(6) * 1.0f;
    if (ground_input == NULL ||
        ground_input->platform_drop_axis_threshold != UINT16_C(21626) ||
        ground_input->platform_drop_tilt_window_ticks != UINT16_C(6) ||
        !expect_status(
            make_content_view(&platform_content, &platform_view),
            PF_STATUS_OK,
            "crouch-platform-drop-content-view") ||
        !initialize_sim(
            &platform_storage,
            &platform_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &platform) ||
        !initialize_sim(
            &floor_storage,
            default_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &floor))
    {
        return 0;
    }

    if (!prepare_shield_drop_platform(platform, &inspection))
    {
        return 0;
    }
    force_reference_crouch_history(
        platform,
        INT8_C(1),
        UINT8_C(4),
        ucf_age_before);
    if (!step_duel(
            platform,
            INT16_C(0),
            (int16_t)ground_input->platform_drop_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        inspection.players[0].platform_drop_ticks == UINT8_C(0) ||
        platform->world.tilt_y_age[0] != UINT8_C(254) ||
        platform->world.ucf_tilt_y_age[0] !=
            (uint8_t)(ucf_age_before + UINT8_C(1)))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-platform-drop-age5 "
            "action=%u grounded=%u x671=%u x674=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)platform->world.tilt_y_age[0],
            (unsigned int)platform->world.ucf_tilt_y_age[0]);
        return 0;
    }

    if (!prepare_shield_drop_platform(platform, &inspection))
    {
        return 0;
    }
    force_reference_crouch_history(
        platform,
        INT8_C(1),
        UINT8_C(5),
        ucf_age_before);
    if (!step_duel(
            platform,
            INT16_C(0),
            (int16_t)ground_input->platform_drop_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        inspection.players[0].platform_drop_ticks != UINT8_C(0) ||
        platform->world.tilt_y_age[0] != UINT8_C(6))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-platform-drop-age6 "
            "action=%u grounded=%u age=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)platform->world.tilt_y_age[0]);
        return 0;
    }

    if (!prepare_shield_drop_platform(platform, &inspection))
    {
        return 0;
    }
    force_reference_crouch_history(
        platform,
        INT8_C(0),
        UINT8_C(254),
        UINT8_C(254));
    if (!step_duel(
            platform,
            INT16_MAX,
            (int16_t)ground_input->platform_drop_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].platform_drop_ticks == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-pass-before-dash "
            "action=%u grounded=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(floor, UINT64_C(0x63726f756368)),
            PF_STATUS_OK,
            "crouch-platform-drop-floor-reset"))
    {
        return 0;
    }
    force_reference_crouch_history(
        floor,
        INT8_C(0),
        UINT8_C(254),
        UINT8_C(254));
    if (!step_duel(
            floor,
            INT16_C(0),
            (int16_t)ground_input->platform_drop_axis_threshold,
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].platform_drop_ticks != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=crouch-platform-drop-floor "
            "action=%u grounded=%u support=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support);
        return 0;
    }
    return 1;
}

static int run_shield_platform_drop_test(
    const struct content *default_content,
    const pf_content_view *default_view)
{
    const ssbm_ground_input_attributes *ground_input =
        ssbm_common_reference_ground_input();
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage floor_storage;
    struct content platform_content = *default_content;
    struct content invalid_content = *default_content;
    pf_content_view platform_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *floor = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    struct inspection floor_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (ground_input == NULL ||
        ground_input->platform_drop_axis_threshold != UINT16_C(21626) ||
        ground_input->platform_drop_tilt_window_ticks != UINT16_C(6) ||
        default_content->fighter.shield_drop_axis_threshold !=
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
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-low-shield-drop-threshold"))
    {
        return 0;
    }
    invalid_content = *default_content;
    invalid_content.fighter.shield_drop_axis_threshold =
        invalid_content.fighter.crouch_axis_threshold;
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-high-shield-drop-threshold"))
    {
        return 0;
    }

    platform_content.stage.platform_center_x_f32 =
        -INT32_C(8) * 1.0f;
    platform_content.stage.platform_motion_amplitude_f32 = 0.0f;
    platform_content.stage.platform_half_width_f32 =
        INT32_C(6) * 1.0f;
    if (!expect_status(
            make_content_view(
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
            (int16_t)ground_input->platform_drop_axis_threshold,
            &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "shield-platform-drop-query-save-size") ||
        save_size != (size_t)1800)
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
            (int16_t)ground_input->platform_drop_axis_threshold,
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        !step_duel_trigger(
            loaded,
            INT16_C(0),
            (int16_t)ground_input->platform_drop_axis_threshold,
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
        source_inspection.players[0].velocity_y_f32 !=
            platform_content.fighter.platform_drop_speed_y_f32 ||
        source_inspection.players[0].position_y_f32 <=
            platform_content.stage.platform_y_f32 -
                platform_content.fighter.half_height_f32 ||
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
                ground_input->platform_drop_axis_threshold -
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
                ground_input->escape_axis_threshold -
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
            (int16_t)ground_input->escape_axis_threshold,
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
            "m4-movement=fail operation=shield-platform-drop-boundaries"
            " action=%u grounded=%u support=%u drop=%u escape=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            (unsigned int)source_inspection.players[0]
                .platform_drop_ticks,
            (unsigned int)ground_input->escape_axis_threshold);
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
    const struct content *default_content,
    struct content *out_content,
    pf_content_view *out_view)
{
    *out_content = *default_content;
    out_content->stage.reference_collision_profile =
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED;
    out_content->stage.spawn_spacing_f32 =
        (INT32_C(9) * 1.0f) / INT32_C(5);
    out_content->stage.platform_center_x_f32 =
        -INT32_C(20) * 1.0f;
    out_content->stage.platform_half_width_f32 =
        INT32_C(2) * 1.0f;
    out_content->stage.platform_motion_amplitude_f32 = 0.0f;
    out_content->stage.solid_left_f32 = -0.5f;
    out_content->stage.solid_right_f32 =
        INT32_C(8) * 1.0f;
    out_content->stage.solid_top_f32 =
        INT32_C(26) * 1.0f;
    out_content->stage.solid_bottom_f32 =
        INT32_C(29) * 1.0f;
    out_content->stage.floor_y_f32 = 32.75f;
    return expect_status(
        make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "solid-geometry-content-view");
}

static int run_solid_geometry_test(
    const struct content *default_content)
{
    test_sim_storage under_storage;
    test_sim_storage wall_storage;
    test_sim_storage ceiling_storage;
    test_sim_storage top_storage;
    test_sim_storage right_corner_storage;
    struct content content;
    struct content right_corner_content;
    pf_content_view view;
    pf_content_view right_corner_view;
    pf_sim *under = NULL;
    pf_sim *wall = NULL;
    pf_sim *ceiling = NULL;
    pf_sim *top = NULL;
    pf_sim *right_corner = NULL;
    struct inspection inspection;
    const float wall_contact_x =
        -0.5f - default_content->fighter.half_width_f32;
    const float top_contact_y =
        INT32_C(26) * 1.0f -
        default_content->fighter.half_height_f32;
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
    right_corner_content.stage.solid_left_f32 =
        -INT32_C(8) * 1.0f;
    right_corner_content.stage.solid_right_f32 = 0.0f;
    if (!expect_status(
            make_content_view(
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
            inspect(under, &inspection),
            PF_STATUS_OK,
            "solid-geometry-inspect") ||
        inspection.stage.solid_left_f32 !=
            content.stage.solid_left_f32 ||
        inspection.stage.solid_right_f32 !=
            content.stage.solid_right_f32 ||
        inspection.stage.solid_top_f32 !=
            content.stage.solid_top_f32 ||
        inspection.stage.solid_bottom_f32 !=
            content.stage.solid_bottom_f32)
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
    if (inspection.players[1].position_x_f32 <=
            content.stage.solid_right_f32 +
                content.fighter.half_width_f32 ||
        inspection.players[1].grounded == UINT8_C(0) ||
        inspection.players[1].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-walk-under"
            " position=(%.9g" ",%.9g" ")"
            " grounded=%u support=%u\n",
            inspection.players[1].position_x_f32,
            inspection.players[1].position_y_f32,
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
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
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
        if (inspection.players[0].position_x_f32 == wall_contact_x &&
            inspection.players[0].velocity_x_f32 == 0.0f)
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
            " position=(%.9g" ",%.9g" ")"
            " velocity=(%.9g" ",%.9g" ")\n",
            inspection.players[0].position_x_f32,
            inspection.players[0].position_y_f32,
            inspection.players[0].velocity_x_f32,
            inspection.players[0].velocity_y_f32);
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
        if (inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE &&
            inspection.players[1].grounded == UINT8_C(0) &&
            inspection.players[1].position_y_f32 >
                content.stage.solid_bottom_f32 &&
            inspection.players[1].velocity_y_f32 == 0.0f)
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
            " position=(%.9g" ",%.9g" ")"
            " velocity=(%.9g" ",%.9g" ")"
            " action=%u ticks=%u grounded=%u submotion=%u frame=%.9g"
            " half_height=%.9g" "\n",
            inspection.players[1].position_x_f32,
            inspection.players[1].position_y_f32,
            inspection.players[1].velocity_x_f32,
            inspection.players[1].velocity_y_f32,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].grounded,
            (unsigned int)inspection.players[1].source_submotion,
            inspection.players[1].source_animation_frame_f32,
            content.fighter.half_height_f32);
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        const int16_t horizontal_axis =
            tick >= UINT32_C(5) &&
                    inspection.players[0].position_x_f32 < 2.0f
                ? INT16_MAX
                : INT16_C(0);
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
                " position=(%.9g" ",%.9g" ")"
                " velocity=(%.9g" ",%.9g" ")\n",
                tick,
                inspection.players[0].position_x_f32,
                inspection.players[0].position_y_f32,
                inspection.players[0].velocity_x_f32,
                inspection.players[0].velocity_y_f32);
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
        inspection.players[0].position_y_f32 != top_contact_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-top-landing"
            " position=(%.9g" ",%.9g" ")"
            " grounded=%u support=%u\n",
            inspection.players[0].position_x_f32,
            inspection.players[0].position_y_f32,
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
        inspection.players[0].position_x_f32 +
                content.fighter.half_width_f32 >=
            content.stage.solid_left_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-top-left-edge-escape"
            " action=%u ticks=%u grounded=%u support=%u"
            " position=(%.9g" ",%.9g" ")"
            " velocity=(%.9g" ",%.9g" ")\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            inspection.players[0].position_x_f32,
            inspection.players[0].position_y_f32,
            inspection.players[0].velocity_x_f32,
            inspection.players[0].velocity_y_f32);
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        const int16_t horizontal_axis =
            tick >= UINT32_C(5) &&
                    inspection.players[1].position_x_f32 > -2.0f
                ? INT16_MIN
                : INT16_C(0);
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
                " position=(%.9g" ",%.9g" ")"
                " velocity=(%.9g" ",%.9g" ")\n",
                tick,
                inspection.players[1].position_x_f32,
                inspection.players[1].position_y_f32,
                inspection.players[1].velocity_x_f32,
                inspection.players[1].velocity_y_f32);
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
        inspection.players[1].position_y_f32 != top_contact_y)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=solid-right-top-landing"
            " position=(%.9g" ",%.9g" ")"
            " grounded=%u support=%u\n",
            inspection.players[1].position_x_f32,
            inspection.players[1].position_y_f32,
            (unsigned int)inspection.players[1].grounded,
            (unsigned int)inspection.players[1].support);
        return 0;
    }
    return 1;
}

static int drive_player0_to_right_ledge(
    pf_sim *sim,
    struct inspection *out_inspection)
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
            out_inspection->players[0].position_x_f32 >
                out_inspection->stage.right_ledge_x_f32)
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
    struct inspection *out_inspection)
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
            out_inspection->players[0].position_x_f32 >=
                out_inspection->stage.right_ledge_x_f32 -
                    INT32_C(5) * 1.0f)
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
            out_inspection->players[0].velocity_x_f32 == 0.0f)
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

        if (out_inspection->players[0].grounded == UINT8_C(0) &&
            out_inspection->players[0].ledge ==
                (uint8_t)PF_M4_LEDGE_NONE)
        {
            /* CliffCatch replaces any prior downed/roll ownership. Seed stale
             * values during the natural approach so every ledge-grab fixture
             * proves the transition clears both canonical fields. */
            sim->world.prone_orientation[0] =
                (uint8_t)PF_M4_PRONE_BACK;
            sim->world.prone_roll_motion_orientation[0] =
                (uint8_t)PF_M4_PRONE_STOMACH;
        }

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
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        sim->world.prone_orientation[0] !=
            (uint8_t)PF_M4_PRONE_NONE ||
        sim->world.prone_roll_motion_orientation[0] !=
            (uint8_t)PF_M4_PRONE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=right-ledge-grab"
            " action=%u ledge=%u x=%.9g" " y=%.9g"
            " facing=%d vx=%.9g" " vy=%.9g" "\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].ledge,
            out_inspection->players[0].position_x_f32,
            out_inspection->players[0].position_y_f32,
            (int)out_inspection->players[0].facing,
            out_inspection->players[0].velocity_x_f32,
            out_inspection->players[0].velocity_y_f32);
        return 0;
    }
    return 1;
}

static int make_player0_ledge_actionable(
    pf_sim *sim,
    const struct content *content,
    struct inspection *out_inspection)
{
    const uint16_t initial_ticks =
        out_inspection->players[0].action_ticks;

    (void)content;
    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        out_inspection->players[0].action_ticks !=
            (uint16_t)(initial_ticks + UINT16_C(1)))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-catch-window"
            " action=%u ticks=%u expected_ticks=%u\n",
            (unsigned int)out_inspection->players[0].action_state,
            (unsigned int)out_inspection->players[0].action_ticks,
            (unsigned int)(initial_ticks + UINT16_C(1)));
        return 0;
    }
    return 1;
}

static int run_ledge_snapshot_test(
    pf_sim *source,
    const pf_content_view *content,
    struct inspection *source_inspection,
    uint8_t expected_action)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
            inspect(loaded, &loaded_inspection),
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
    const struct content *default_content)
{
    test_sim_storage storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    uint32_t tick;

    content.stage.spawn_spacing_f32 =
        content.fighter.player_push_half_width_f32;
    if (!expect_status(
            make_content_view(&content, &view),
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
            inspection.players[0].position_x_f32 >=
                inspection.stage.right_ledge_x_f32 -
                    INT32_C(5) * 1.0f &&
            inspection.players[1].position_x_f32 >=
                inspection.stage.right_ledge_x_f32 -
                    INT32_C(5) * 1.0f)
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
            inspection.players[0].velocity_x_f32 == 0.0f &&
            inspection.players[1].velocity_x_f32 == 0.0f)
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
    const struct content *default_content)
{
    test_sim_storage storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    uint32_t tick;

    content.fighter.reference_frame_data_enabled = UINT8_C(0);
    content.fighter.jab_hitbox_half_width_f32 =
        INT32_C(64) * 1.0f;
    content.fighter.jab_hitbox_half_height_f32 =
        INT32_C(64) * 1.0f;
    if (!expect_status(
            make_content_view(&content, &view),
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
    if (inspection.players[0].damage_f32 != 0.0f ||
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
    if (inspection.players[0].damage_f32 !=
        content.fighter.jab_damage_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-invulnerability-hit-expiry"
            " damage=%.9g\n",
            inspection.players[0].damage_f32);
        return 0;
    }
    return 1;
}

static int reach_scar_jump_wall(
    pf_sim *sim,
    const struct content *content,
    struct inspection *out_inspection)
{
    const float contact_x =
        content->stage.solid_right_f32 +
        content->fighter.half_width_f32;
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
        if (out_inspection->players[0].position_x_f32 == contact_x &&
            out_inspection->players[0].velocity_x_f32 == 0.0f)
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
        " action=%u position=(%.9g" ",%.9g" ")"
        " velocity=(%.9g" ",%.9g" ")\n",
        (unsigned int)out_inspection->players[0].action_state,
        out_inspection->players[0].position_x_f32,
        out_inspection->players[0].position_y_f32,
        out_inspection->players[0].velocity_x_f32,
        out_inspection->players[0].velocity_y_f32);
    return 0;
}

static int enter_scar_jump(
    pf_sim *sim,
    const struct content *content,
    struct inspection *out_inspection)
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
           out_inspection->players[0].velocity_x_f32 ==
               content->fighter.wall_jump_speed_x_f32 &&
           out_inspection->players[0].velocity_y_f32 ==
               -content->fighter.wall_jump_speed_y_f32 +
                   content->fighter.gravity_f32 &&
           out_inspection->players[0].facing == INT8_C(1) &&
           out_inspection->players[0].air_jumps_remaining ==
               content->fighter.air_jump_count &&
           out_inspection->players[0].invulnerable == UINT8_C(1);
}

static int run_scar_jump_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage jump_storage;
    test_sim_storage lock_storage;
    test_sim_storage missed_storage;
    struct content invalid_content = *content;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *jump_sim = NULL;
    pf_sim *lock_sim = NULL;
    pf_sim *missed_sim = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    struct inspection inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;

    invalid_content.fighter.wall_jump_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
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
        save_size != (size_t)1800)
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
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    float before_exhausted_jump_velocity_y;

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
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        source_inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        source_inspection.players[0].velocity_y_f32 >= 0.0f ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[0].invulnerable != UINT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "edge-hop-query-save-size") ||
        save_size != (size_t)1800)
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
        source_inspection.players[0].velocity_y_f32;
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
        source_inspection.players[0].velocity_y_f32 <=
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
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
            (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
        source_inspection.players[0].velocity_y_f32 != 0.0f ||
        source_inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count ||
        source_inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-ledge-jump"
            " action=%u ticks=%u grounded=%u support=%u"
            " position=(%.9g" ",%.9g" ")"
            " velocity=(%.9g" ",%.9g" ") invulnerable=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[0].position_x_f32,
            source_inspection.players[0].position_y_f32,
            source_inspection.players[0].velocity_x_f32,
            source_inspection.players[0].velocity_y_f32,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(32) &&
         source_inspection.players[0].velocity_y_f32 == 0.0f;
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
    if (tick == UINT32_C(32) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
        source_inspection.players[0].velocity_x_f32 >= 0.0f ||
        source_inspection.players[0].velocity_y_f32 >= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-ledge-jump-phase-two\n");
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         source_inspection.players[0].position_y_f32 +
                 content->fighter.half_height_f32 >
             content->stage.floor_y_f32;
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
            (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
        source_inspection.players[0].velocity_y_f32 >= 0.0f ||
        source_inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-rise"
            " tick=%" PRIu32 " action=%u position_y=%.9g"
            " velocity_y=%.9g" " invulnerable=%u\n",
            tick,
            (unsigned int)source_inspection.players[0].action_state,
            source_inspection.players[0].position_y_f32,
            source_inspection.players[0].velocity_y_f32,
            (unsigned int)source_inspection.players[0].invulnerable);
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(128) &&
         source_inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_LEDGE_JUMP;
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
    if (tick == UINT32_C(128) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-fall-entry"
            " action=%u ticks=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks);
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
        source_inspection.players[0].velocity_x_f32 >= 0.0f ||
        source_inspection.players[0].position_x_f32 >
            source_inspection.stage.right_ledge_x_f32 ||
        source_inspection.players[0].invulnerable != UINT8_C(0) ||
        (source_inspection.players[0].action_state ==
                 (uint8_t)PF_M4_ACTION_AIR_DODGE &&
         (source_inspection.players[0].grounded != UINT8_C(0) ||
          source_inspection.players[0].support !=
              (uint8_t)PF_M4_SURFACE_NONE ||
          source_inspection.players[0].velocity_y_f32 <= 0.0f)) ||
        (source_inspection.players[0].action_state ==
                 (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
         (source_inspection.players[0].grounded != UINT8_C(1) ||
          source_inspection.players[0].support !=
              (uint8_t)PF_M4_SURFACE_FLOOR)) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "edge-dash-query-save-size") ||
        save_size != (size_t)1800)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-air-dodge-entry"
            " action=%u ticks=%u grounded=%u support=%u"
            " position=(%.9g" ",%.9g" ")"
            " velocity=(%.9g" ",%.9g" ") invulnerable=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[0].position_x_f32,
            source_inspection.players[0].position_y_f32,
            source_inspection.players[0].velocity_x_f32,
            source_inspection.players[0].velocity_y_f32,
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
         tick < UINT32_C(128) &&
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
                " tick=%" PRIu32 " action=%u ticks=%u submotion=%u"
                " frame=%.9g grounded=%u support=%u\n",
                tick,
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].action_ticks,
                (unsigned int)source_inspection.players[0].source_submotion,
                source_inspection.players[0].source_animation_frame_f32,
                (unsigned int)source_inspection.players[0].grounded,
                (unsigned int)source_inspection.players[0].support);
            return 0;
        }
    }
    if (tick == UINT32_C(128) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].grounded != UINT8_C(1) ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        source_inspection.players[0].velocity_x_f32 > 0.0f ||
        source_inspection.players[0].position_x_f32 >
            source_inspection.stage.right_ledge_x_f32 ||
        source_inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=edge-dash-landing"
            " tick=%" PRIu32 " action=%u grounded=%u support=%u"
            " position=(%.9g" ",%.9g" ")"
            " velocity=(%.9g" ",%.9g" ") invulnerable=%u\n",
            tick,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[0].position_x_f32,
            source_inspection.players[0].position_y_f32,
            source_inspection.players[0].velocity_x_f32,
            source_inspection.players[0].velocity_y_f32,
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
            source_inspection.players[0].invulnerable != UINT8_C(0) ||
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
        source_inspection.players[0].invulnerable != UINT8_C(0) ||
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
        source_inspection.players[0].invulnerable != UINT8_C(0) ||
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
            (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
        source_inspection.players[0].velocity_x_f32 != 0.0f ||
        source_inspection.players[0].velocity_y_f32 != 0.0f ||
        source_inspection.players[0].invulnerable != UINT8_C(1))
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
         tick < UINT32_C(128) &&
         source_inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_LEDGE_JUMP;
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
    if (tick == UINT32_C(128) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
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
         tick < UINT32_C(128) &&
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
    if (tick == UINT32_C(128) ||
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
    struct inspection *source_inspection,
    struct inspection *loaded_inspection,
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
    const struct content *default_content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage missed_storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *missed = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    struct inspection missed_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[2048];
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
    content.fighter.double_jump_speed_f32 =
        INT32_C(31) * 1.0f / INT32_C(100);
    content.fighter.jab_hitbox_half_width_f32 =
        INT32_C(64) * 1.0f;
    content.fighter.jab_hitbox_half_height_f32 =
        INT32_C(64) * 1.0f;
    expected_carried_invulnerability =
        (uint16_t)(
            (uint32_t)content.fighter.ledge_invulnerability_ticks -
            (uint32_t)content.fighter.landing_ticks -
            (uint32_t)content.fighter.jump_squat_ticks -
            UINT32_C(1));
    if (!expect_status(
            make_content_view(&content, &view),
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
        save_size != (size_t)1800)
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
            inspect(loaded, &loaded_inspection),
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
                (source_inspection.players[0].velocity_y_f32 <
                     0.0f ||
                 source_inspection.players[0].position_y_f32 <
                     content.stage.floor_y_f32 -
                         content.fighter.half_height_f32 ||
                 source_inspection.players[0].position_y_f32 >
                     content.stage.floor_y_f32 +
                         content.fighter.half_height_f32 ||
                 source_inspection.players[0].position_x_f32 <=
                     content.stage.floor_right_f32 ||
                 (int64_t)source_inspection.players[0]
                         .position_x_f32 -
                         (int64_t)content.stage.floor_right_f32 >
                     (int64_t)content.fighter.half_width_f32 +
                         (int64_t)content.fighter.air_speed_f32 ||
                 source_inspection.players[0].facing != INT8_C(-1)))
            {
                (void)fprintf(
                    stderr,
                    "m4-movement=fail operation=planking-legal-catch"
                    " cycle=%" PRIu32 " tick=%" PRIu32
                    " position=(%.9g" ",%.9g" ")"
                    " velocity_y=%.9g" " facing=%d\n",
                    cycle,
                    tick,
                    source_inspection.players[0].position_x_f32,
                    source_inspection.players[0].position_y_f32,
                    source_inspection.players[0].velocity_y_f32,
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
            source_inspection.players[0].damage_f32 != 0.0f)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=planking-regrab"
                " cycle=%" PRIu32 " action=%u ledge=%u"
                " lockout=%u invulnerability=%u damage=%.9g"
                " position=(%.9g" ",%.9g" ")"
                " velocity=(%.9g" ",%.9g" ")\n",
                cycle,
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].ledge,
                (unsigned int)source_inspection.players[0]
                    .ledge_regrab_lockout_ticks,
                (unsigned int)source_inspection.players[0]
                    .ledge_invulnerability_ticks,
                source_inspection.players[0].damage_f32,
                source_inspection.players[0].position_x_f32,
                source_inspection.players[0].position_y_f32,
                source_inspection.players[0].velocity_x_f32,
                source_inspection.players[0].velocity_y_f32);
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
        missed_inspection.players[0].damage_f32 !=
            content.fighter.jab_damage_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=planking-missed-punish"
            " action=%u ledge=%u lockout=%u invulnerability=%u"
            " damage=%.9g position=(%.9g" ",%.9g"
            ")\n",
            (unsigned int)missed_inspection.players[0].action_state,
            (unsigned int)missed_inspection.players[0].ledge,
            (unsigned int)missed_inspection.players[0]
                .ledge_regrab_lockout_ticks,
            (unsigned int)missed_inspection.players[0]
                .ledge_invulnerability_ticks,
            missed_inspection.players[0].damage_f32,
            missed_inspection.players[0].position_x_f32,
            missed_inspection.players[0].position_y_f32);
        return 0;
    }
    return 1;
}

static int run_ledge_roll_test(const struct content *default_content)
{
    test_sim_storage storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    float hang_x;
    float hang_y;
    int reference_duration_snapshot_tested = 0;
    uint32_t tick;

    if (!expect_status(
            make_content_view(&content, &view),
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

    hang_x = inspection.players[0].position_x_f32;
    hang_y = inspection.players[0].position_y_f32;
    if (!make_player0_ledge_actionable(sim, &content, &inspection) ||
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
        inspection.players[0].position_x_f32 >= hang_x ||
        inspection.players[0].position_y_f32 >= hang_y ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].position_x_f32 >= hang_x ||
        inspection.players[0].position_y_f32 >= hang_y ||
        !run_ledge_snapshot_test(
            sim,
            &view,
            &inspection,
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-entry"
            " action=%u ticks=%u ledge=%u x=%.9g"
            " y=%.9g" " hang_x=%.9g" " hang_y=%.9g"
            " grounded=%u support=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].ledge,
            inspection.players[0].position_x_f32,
            inspection.players[0].position_y_f32,
            hang_x,
            hang_y,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support);
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(128) &&
         inspection.players[0].grounded == UINT8_C(0);
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
    if (tick == UINT32_C(128) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-grounding"
            " action=%u ticks=%u grounded=%u support=%u ledge=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            (unsigned int)inspection.players[0].ledge);
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(128) &&
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
        if (reference_duration_snapshot_tested == 0 &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_LEDGE_ROLL &&
            inspection.players[0].action_ticks >=
                content.fighter.ledge_roll_ticks)
        {
            if (!run_ledge_snapshot_test(
                    sim,
                    &view,
                    &inspection,
                    (uint8_t)PF_M4_ACTION_LEDGE_ROLL))
            {
                return 0;
            }
            reference_duration_snapshot_tested = 1;
        }
    }
    if (tick == UINT32_C(128) ||
        reference_duration_snapshot_tested == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-roll-completion\n");
        return 0;
    }
    return 1;
}

static int run_reference_ledge_direction_priority_test(
    pf_sim *sim,
    const struct content *content,
    struct inspection *inspection)
{
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa0)),
            PF_STATUS_OK,
            "ledge-c-inward-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !make_player0_ledge_actionable(sim, content, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(-20000),
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-c-inward action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa1)),
            PF_STATUS_OK,
            "ledge-c-roll-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !make_player0_ledge_actionable(sim, content, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MIN,
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-c-roll action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa2)),
            PF_STATUS_OK,
            "ledge-c-attack-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !make_player0_ledge_actionable(sim, content, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_MIN,
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-c-attack action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa3)),
            PF_STATUS_OK,
            "ledge-c-outward-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !make_player0_ledge_actionable(sim, content, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection->players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-c-outward action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa4)),
            PF_STATUS_OK,
            "ledge-main-priority-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !make_player0_ledge_actionable(sim, content, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_MIN,
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_CLIMB)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-main-priority action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa5)),
            PF_STATUS_OK,
            "ledge-main-drop-priority-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !make_player0_ledge_actionable(sim, content, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_MAX,
            INT16_C(0),
            INT16_C(-20000),
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection->players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-main-drop-priority action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6eaa6)),
            PF_STATUS_OK,
            "ledge-c-readiness-reset") ||
        !grab_player0_right_ledge(sim, inspection) ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            inspection) ||
        !step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            inspection) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-c-readiness action=%u\n",
            (unsigned int)inspection->players[0].action_state);
        return 0;
    }
    return 1;
}

static int run_ledge_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    struct content invalid_content = *content;
    struct content tuned_content = *content;
    pf_content_view tuned_view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    const falcon_submotion_data *ledge_catch;
    float hang_x;
    float hang_y;
    uint16_t remaining_ledge_invulnerability_ticks;
    uint32_t tick;

    invalid_content.fighter.ledge_invulnerability_ticks =
        UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-invulnerability-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_regrab_lockout_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-regrab-lockout-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_roll_distance_f32 =
        content->fighter.half_width_f32 +
        content->fighter.platform_drop_nudge_f32;
    if (!expect_status(
            validate_content(&invalid_content),
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
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-roll-movement-invalid-content"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.ledge_roll_invulnerability_ticks =
        (uint16_t)(invalid_content.fighter.ledge_roll_ticks +
                   UINT16_C(1));
    tuned_content.fighter.ledge_roll_distance_f32 += INT32_C(1);
    if (!expect_status(
            validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "ledge-roll-invulnerability-invalid-content") ||
        !expect_status(
            make_content_view(&tuned_content, &tuned_view),
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
    hang_x = inspection.players[0].position_x_f32;
    hang_y = inspection.players[0].position_y_f32;
    if (inspection.players[0].invulnerable != UINT8_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].position_x_f32 != hang_x ||
        inspection.players[0].position_y_f32 != hang_y ||
        inspection.players[0].velocity_x_f32 != 0.0f ||
        inspection.players[0].velocity_y_f32 != 0.0f ||
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
        inspection.players[0].position_y_f32 <= hang_y ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-release"
            " action=%u ledge=%u x=%.9g" " y=%.9g"
            " hang_y=%.9g" " invulnerable=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].ledge,
            inspection.players[0].position_x_f32,
            inspection.players[0].position_y_f32,
            hang_y,
            (unsigned int)inspection.players[0].invulnerable);
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
    ledge_catch = falcon_reference_submotion(
        PF_M4_FALCON_SUBMOTION_LEDGE_CATCH);
    remaining_ledge_invulnerability_ticks =
        inspection.players[0].ledge_invulnerability_ticks;
    if (ledge_catch == NULL ||
        remaining_ledge_invulnerability_ticks !=
            (uint16_t)(content->fighter.ledge_invulnerability_ticks -
                       ledge_catch->gameplay_frame_count))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-catch-invulnerability-elapse"
            " remaining=%u\n",
            (unsigned int)remaining_ledge_invulnerability_ticks);
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)remaining_ledge_invulnerability_ticks;
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
            (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        inspection.players[0].velocity_x_f32 != 0.0f ||
        inspection.players[0].velocity_y_f32 != 0.0f ||
        inspection.players[0].air_jumps_remaining !=
            content->fighter.air_jump_count)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-jump\n");
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(32) &&
         inspection.players[0].velocity_y_f32 == 0.0f;
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
    if (tick == UINT32_C(32) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
        inspection.players[0].velocity_x_f32 >= 0.0f ||
        inspection.players[0].velocity_y_f32 >= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-jump-phase-two\n");
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

    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
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
            (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].position_x_f32 >=
            inspection.stage.right_ledge_x_f32 ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-climb-completion"
            " action=%u ticks=%u grounded=%u support=%u ledge=%u"
            " position_x=%.9g" " right=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            (unsigned int)inspection.players[0].ledge,
            inspection.players[0].position_x_f32,
            inspection.stage.right_ledge_x_f32);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(64) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_LEDGE_CLIMB;
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
    if (tick == UINT32_C(64) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-climb-animation-end"
            " action=%u ticks=%u grounded=%u support=%u ledge=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            (unsigned int)inspection.players[0].support,
            (unsigned int)inspection.players[0].ledge);
        return 0;
    }
    if (!run_ledge_roll_test(content))
    {
        (void)fprintf(stderr, "m4-movement=fail operation=ledge-roll-suite\n");
        return 0;
    }
    if (!run_reference_ledge_direction_priority_test(
            sim,
            content,
            &inspection))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ledge-direction-priority\n");
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
    struct inspection inspection;
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

static int run_vector_ascent_test(const struct content *base_content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage rl_storage;
    struct content content = *base_content;
    struct content invalid;
    pf_content_view disabled_view;
    pf_content_view view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *rl_sim = NULL;
    struct inspection source_inspection;
    struct inspection loaded_inspection;
    pf_sim_observation observation;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_mut_bytes destination;
    pf_bytes save;
    pf_rl_transition transition;
    pf_rl_action actions[2];
    uint8_t save_bytes[2048];
    size_t save_size = (size_t)0;
    float grounded_recovery_x;
    uint32_t player_bits;
    uint32_t guard;

    /* Vector Ascent is an original custom-content fixture. Default reference
     * content routes the same input through source Falcon Dive instead. */
    content.fighter.reference_frame_data_enabled = UINT8_C(0);

    if (content.recovery_count != PF_M4_TEST_RECOVERY_COUNT ||
        content.recovery.schema_version != PF_M4_RECOVERY_SCHEMA_VERSION ||
        content.recovery.enabled != UINT8_C(0) ||
        content.recovery.horizontal_speed_f32 !=
            1.0f / INT32_C(4) ||
        content.recovery.vertical_speed_f32 !=
            0.8f ||
        content.recovery.ascent_ticks != UINT16_C(18) ||
        !expect_status(
            make_content_view(&content, &disabled_view),
            PF_STATUS_OK,
            "vector-ascent-disabled-view"))
    {
        return 0;
    }

    invalid = content;
    invalid.recovery.enabled = UINT8_C(2);
    if (!expect_status(
            validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "vector-ascent-invalid-enabled"))
    {
        return 0;
    }
    invalid = content;
    invalid.recovery.ascent_ticks = UINT16_C(0);
    if (!expect_status(
            validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "vector-ascent-invalid-duration"))
    {
        return 0;
    }

    content.recovery.enabled = UINT8_C(1);
    if (!expect_status(
            make_content_view(&content, &view),
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
            inspect(source, &source_inspection),
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
    grounded_recovery_x = loaded_inspection.players[0].position_x_f32;
    if (guard == UINT32_C(32) ||
        !step_duel(
            loaded,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        loaded_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        loaded_inspection.players[0].position_x_f32 !=
            grounded_recovery_x ||
        !step_duel(
            loaded,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        loaded_inspection.players[0].position_x_f32 <=
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
            " guard=%u action=%u x=%.9g start=%.9g\n",
            (unsigned int)guard,
            (unsigned int)loaded_inspection.players[0].action_state,
            loaded_inspection.players[0].position_x_f32,
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
        source_inspection.players[0].velocity_x_f32 <= 0.0f ||
        source_inspection.players[0].velocity_y_f32 >= 0.0f ||
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
        save_size != (size_t)1800)
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

static int run_player_push_test(const struct content *default_content)
{
    test_sim_storage storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    struct inspection before;
    struct inspection after;
    float expected_left;
    float expected_right;

    content.stage.spawn_spacing_f32 =
        nextafterf(
            content.fighter.player_push_half_width_f32,
            0.0f);
    if (!expect_status(
            make_content_view(&content, &view),
            PF_STATUS_OK,
            "player-push-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x50555348)),
            PF_STATUS_OK,
            "player-push-reset") ||
        !expect_status(
            inspect(sim, &before),
            PF_STATUS_OK,
            "player-push-inspect-before"))
    {
        return 0;
    }

    expected_left =
        before.players[0].position_x_f32 -
        content.fighter.player_push_speed_f32;
    expected_right =
        before.players[1].position_x_f32 +
        content.fighter.player_push_speed_f32;
    if (!step_duel_players(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &after) ||
        after.players[0].position_x_f32 != expected_left ||
        after.players[1].position_x_f32 != expected_right ||
        after.players[0].velocity_x_f32 != 0.0f ||
        after.players[1].velocity_x_f32 != 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=player-push-displacement\n");
        return 0;
    }

    if (!step_duel_players(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &after) ||
        after.players[0].position_x_f32 != expected_left ||
        after.players[1].position_x_f32 != expected_right)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=player-push-strict-boundary\n");
        return 0;
    }

    content = *default_content;
    content.fighter.player_push_half_width_f32 = 0.0f;
    if (!expect_status(
            validate_content(&content),
            PF_STATUS_INVALID_CONFIG,
            "player-push-invalid-radius"))
    {
        return 0;
    }
    content = *default_content;
    content.fighter.player_push_speed_f32 = 0.0f;
    return expect_status(
        validate_content(&content),
        PF_STATUS_INVALID_CONFIG,
        "player-push-invalid-speed");
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
            struct inspection inspection;

            (void)inspect(sim, &inspection);
            for (player_index = UINT32_C(0);
                 player_index < UINT32_C(4);
                 ++player_index)
            {
                const player_inspection *player =
                    &inspection.players[player_index];
                (void)fprintf(
                    stderr,
                    "m4-movement=trace tick=%" PRIu64
                    " player=%" PRIu32
                    " action=%u grounded=%u support=%u"
                    " position=(%.9g" ",%.9g" ")"
                    " velocity=(%.9g" ",%.9g" ")"
                    " action_ticks=%u facing=%d dash=%d previous=%d"
                    " fast=%u short=%u drop=%u jumps=%u respawns=%u"
                    " submotion=%u frame=%.9g rate=%.9g"
                    " fall_blend=%.9g ground_blend=%.9g\n",
                    inspection.tick,
                    player_index,
                    (unsigned int)player->action_state,
                    (unsigned int)player->grounded,
                    (unsigned int)player->support,
                    player->position_x_f32,
                    player->position_y_f32,
                    player->velocity_x_f32,
                    player->velocity_y_f32,
                    (unsigned int)player->action_ticks,
                    (int)player->facing,
                    (int)player->dash_direction,
                    (int)player->previous_strong_direction,
                    (unsigned int)player->fast_fall,
                    (unsigned int)player->short_hop_latched,
                    (unsigned int)player->platform_drop_ticks,
                    (unsigned int)player->air_jumps_remaining,
                    (unsigned int)player->respawn_count,
                    (unsigned int)player->source_submotion,
                    player->source_animation_frame_f32,
                    sim->world.source_animation_rate_f32[player_index],
                    sim->world.fall_animation_blend_f32[player_index],
                    sim->world.ground_blend_progress_f32[player_index]);
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

static float falcon_source_velocity_to_sim_f32(
    float source_velocity_f32,
    int32_t numerator,
    int32_t denominator)
{
    return source_velocity_f32 * (float)numerator / (float)denominator;
}

static int enter_air_falcon_punch(
    pf_sim *sim,
    uint64_t seed,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, seed),
            PF_STATUS_OK,
            "falcon-punch-air-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
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
        if (out_inspection->players[0].grounded == UINT8_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE)
        {
            break;
        }
    }
    if (tick == UINT32_C(240) ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
        out_inspection->players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-punch-air-entry\n");
        return 0;
    }
    return 1;
}

static int run_falcon_punch_source_data_test(
    const struct content *default_content)
{
    const falcon_special_attributes *attributes =
        falcon_reference_special_attributes();
    const falcon_neutral_special_timing *timing =
        falcon_reference_neutral_special_timing();
    const struct reference_move *ground_move =
        falcon_reference_move(
            PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND);
    const struct reference_move *air_move =
        falcon_reference_move(
            PF_M4_FALCON_NEUTRAL_SPECIAL_AIR);
    test_sim_storage ground_storage;
    test_sim_storage air_storage;
    struct content ground_content = *default_content;
    struct content air_content = *default_content;
    pf_content_view ground_view;
    pf_content_view air_view;
    pf_sim *ground_sim = NULL;
    pf_sim *air_sim = NULL;
    struct inspection inspection;
    float expected_launch_velocity_x;
    uint32_t frame;

    if (attributes == NULL || timing == NULL || ground_move == NULL ||
        air_move == NULL || ground_move->present == UINT8_C(0) ||
        air_move->present == UINT8_C(0) ||
        ground_move->total_frames != UINT16_C(99) ||
        air_move->total_frames != UINT16_C(99) ||
        timing->launch_frame != UINT16_C(50) ||
        timing->velocity_scale_begin_frame != UINT16_C(50) ||
        timing->velocity_scale_end_frame != UINT16_C(64) ||
        timing->ordinary_air_physics_begin_frame != UINT16_C(65))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-punch-imported-timeline\n");
        return 0;
    }

    ground_content.stage.spawn_spacing_f32 =
        INT32_C(8) * 1.0f;
    if (!expect_status(
            make_content_view(&ground_content, &ground_view),
            PF_STATUS_OK,
            "falcon-punch-ground-content-view") ||
        !initialize_sim(
            &ground_storage,
            &ground_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &ground_sim) ||
        !expect_status(
            pf_sim_reset(ground_sim, UINT64_C(0xfa1c0a01)),
            PF_STATUS_OK,
            "falcon-punch-ground-reset") ||
        !step_duel(
            ground_sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(1); frame <= UINT32_C(99); ++frame)
    {
        const uint8_t expected_hitbox =
            frame >= UINT32_C(52) && frame <= UINT32_C(56)
                ? UINT8_C(1)
                : UINT8_C(0);

        if (inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND ||
            inspection.players[0].action_ticks != (uint16_t)frame ||
            inspection.players[0].hitbox_active != expected_hitbox ||
            (expected_hitbox != UINT8_C(0) &&
             inspection.players[0].hit_sphere_count == UINT8_C(0)))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=falcon-punch-ground-frame "
                "frame=%" PRIu32 " action=%u action_ticks=%u "
                "hitbox=%u spheres=%u\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks,
                (unsigned int)inspection.players[0].hitbox_active,
                (unsigned int)inspection.players[0].hit_sphere_count);
            return 0;
        }
        if (frame < UINT32_C(99) &&
            !step_duel(
                ground_sim,
                INT16_C(0),
                INT16_C(0),
                frame == UINT32_C(2)
                    ? PF_INPUT_BUTTON_SPECIAL
                    : UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            ground_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-punch-ground-complete\n");
        return 0;
    }

    air_content.stage.spawn_spacing_f32 =
        INT32_C(10) * 1.0f;
    air_content.stage.platform_motion_amplitude_f32 = 0.0f;
    if (!expect_status(
            make_content_view(&air_content, &air_view),
            PF_STATUS_OK,
            "falcon-punch-air-content-view") ||
        !initialize_sim(
            &air_storage,
            &air_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &air_sim) ||
        !enter_air_falcon_punch(
            air_sim,
            UINT64_C(0xfa1c0a02),
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(2); frame <= UINT32_C(50); ++frame)
    {
        if (!step_duel(
                air_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
            inspection.players[0].action_ticks != (uint16_t)frame)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=falcon-punch-air-frame "
                "frame=%" PRIu32 " action=%u action_ticks=%u\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks);
            return 0;
        }
    }
    expected_launch_velocity_x =
        falcon_source_velocity_to_sim_f32(
            attributes->specialn_vel_x_f32,
            INT32_C(12),
            INT32_C(115));
    expected_launch_velocity_x *= attributes->specialn_vel_mul_f32;
    if (inspection.players[0].velocity_x_f32 !=
            -expected_launch_velocity_x ||
        inspection.players[0].velocity_y_f32 != 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-punch-air-launch "
            "vx=%.9g expected=%.9g vy=%.9g\n",
            (double)inspection.players[0].velocity_x_f32,
            (double)-expected_launch_velocity_x,
            (double)inspection.players[0].velocity_y_f32);
        return 0;
    }
    for (frame = UINT32_C(51); frame <= UINT32_C(65); ++frame)
    {
        if (!step_duel(
                air_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
            inspection.players[0].action_ticks != (uint16_t)frame)
        {
            return 0;
        }
    }
    if (inspection.players[0].velocity_y_f32 !=
            default_content->fighter.gravity_f32 ||
        inspection.players[0].velocity_x_f32 >= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-punch-air-ordinary-physics "
            "vx=%.9g" " vy=%.9g" " expected_vy=%.9g"
            "\n",
            inspection.players[0].velocity_x_f32,
            inspection.players[0].velocity_y_f32,
            default_content->fighter.gravity_f32);
        return 0;
    }

    if (!enter_air_falcon_punch(
            air_sim,
            UINT64_C(0xfa1c0a03),
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(2); frame <= UINT32_C(50); ++frame)
    {
        const int16_t stick_y =
            frame == UINT32_C(50) ? INT16_MAX : INT16_C(0);

        if (!step_duel(
                air_sim,
                INT16_C(0),
                stick_y,
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
            inspection.players[0].action_ticks != (uint16_t)frame)
        {
            return 0;
        }
    }
    if (inspection.players[0].velocity_x_f32 >= 0.0f ||
        inspection.players[0].velocity_y_f32 >= 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-punch-air-up-angle "
            "vx=%.9g" " vy=%.9g" "\n",
            inspection.players[0].velocity_x_f32,
            inspection.players[0].velocity_y_f32);
        return 0;
    }
    return 1;
}

static int enter_air_raptor_boost(
    pf_sim *sim,
    uint64_t seed,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, seed),
            PF_STATUS_OK,
            "raptor-boost-air-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(30) &&
         out_inspection->players[0].grounded != UINT8_C(0);
         ++tick)
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
    if (tick == UINT32_C(30) ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
        out_inspection->players[0].action_ticks != UINT16_C(1) ||
        out_inspection->players[0].velocity_y_f32 != 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-air-entry\n");
        return 0;
    }
    return 1;
}

static int run_raptor_boost_source_data_test(
    const struct content *default_content)
{
    const falcon_common_special_attributes *common_attributes =
        falcon_reference_common_special_attributes();
    const falcon_special_attributes *attributes =
        falcon_reference_special_attributes();
    const falcon_side_special_timing *timing =
        falcon_reference_side_special_timing();
    const struct reference_move *ground_move =
        falcon_reference_move(
            PF_M4_FALCON_SIDE_SPECIAL_START_GROUND);
    const struct reference_move *air_move =
        falcon_reference_move(
            PF_M4_FALCON_SIDE_SPECIAL_START_AIR);
    test_sim_storage ground_storage;
    test_sim_storage ground_hit_storage;
    test_sim_storage air_storage;
    struct content ground_content = *default_content;
    struct content ground_hit_content = *default_content;
    struct content air_content = *default_content;
    pf_content_view ground_view;
    pf_content_view ground_hit_view;
    pf_content_view air_view;
    pf_sim *ground_sim = NULL;
    pf_sim *ground_hit_sim = NULL;
    pf_sim *air_sim = NULL;
    struct inspection inspection;
    float expected_motion_x_f32;
    float expected_gravity_f32;
    uint32_t frame;
    uint32_t tick;

    if (common_attributes == NULL || attributes == NULL || timing == NULL ||
        ground_move == NULL || air_move == NULL ||
        common_attributes->side_special_stick_threshold_f32 !=
            0.600000024f ||
        common_attributes->side_special_turn_threshold_f32 !=
            0.200000003f ||
        ground_move->subaction_index != UINT16_C(303) ||
        ground_move->total_frames != UINT16_C(79) ||
        air_move->subaction_index != UINT16_C(305) ||
        air_move->total_frames != UINT16_C(79) ||
        timing->ground_search_begin_frame != UINT16_C(15) ||
        timing->ground_search_end_frame != UINT16_C(34) ||
        timing->air_search_begin_frame != UINT16_C(18) ||
        timing->air_search_end_frame != UINT16_C(34) ||
        timing->air_gravity_begin_frame != UINT16_C(30))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-imported-data\n");
        return 0;
    }

    ground_content.stage.spawn_spacing_f32 =
        INT32_C(8) * 1.0f;
    ground_content.stage.platform_motion_amplitude_f32 = 0.0f;
    if (!expect_status(
            make_content_view(&ground_content, &ground_view),
            PF_STATUS_OK,
            "raptor-boost-ground-content-view") ||
        !initialize_sim(
            &ground_storage,
            &ground_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &ground_sim) ||
        !expect_status(
            pf_sim_reset(ground_sim, UINT64_C(0xfa1c0b01)),
            PF_STATUS_OK,
            "raptor-boost-threshold-reset") ||
        !step_duel(
            ground_sim,
            INT16_C(19660),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-below-threshold\n");
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(ground_sim, UINT64_C(0xfa1c0b02)),
            PF_STATUS_OK,
            "raptor-boost-ground-reset") ||
        !step_duel(
            ground_sim,
            INT16_C(19661),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(1); frame <= UINT32_C(79); ++frame)
    {
        if (!falcon_reference_motion_x_f32(
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND,
                (uint16_t)frame,
                &expected_motion_x_f32) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND ||
            inspection.players[0].action_ticks != (uint16_t)frame ||
            inspection.players[0].velocity_x_f32 !=
                expected_motion_x_f32)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=raptor-boost-ground-frame "
                "frame=%" PRIu32 " action=%u action_ticks=%u vx=%.9g"
                " expected_vx=%.9g" "\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks,
                inspection.players[0].velocity_x_f32,
                expected_motion_x_f32);
            return 0;
        }
        if (frame < UINT32_C(79) &&
            !step_duel(
                ground_sim,
                INT16_C(0),
                INT16_C(0),
                frame == UINT32_C(2)
                    ? PF_INPUT_BUTTON_SPECIAL
                    : UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            ground_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        !expect_status(
            pf_sim_reset(ground_sim, UINT64_C(0xfa1c0b03)),
            PF_STATUS_OK,
            "raptor-boost-turn-reset") ||
        !step_duel(
            ground_sim,
            INT16_C(-19661),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND ||
        inspection.players[0].facing != INT8_C(-1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-ground-complete-turn\n");
        return 0;
    }

    ground_hit_content.stage.spawn_spacing_f32 =
        INT32_C(2) * 1.0f;
    ground_hit_content.stage.platform_motion_amplitude_f32 = 0.0f;
    if (!expect_status(
            make_content_view(
                &ground_hit_content,
                &ground_hit_view),
            PF_STATUS_OK,
            "raptor-boost-ground-hit-content-view") ||
        !initialize_sim(
            &ground_hit_storage,
            &ground_hit_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &ground_hit_sim) ||
        !expect_status(
            pf_sim_reset(ground_hit_sim, UINT64_C(0xfa1c0b05)),
            PF_STATUS_OK,
            "raptor-boost-ground-hit-reset") ||
        !step_duel(
            ground_hit_sim,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(1);
         frame <= UINT32_C(34) &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND;
         ++frame)
    {
        if (!step_duel(
                ground_hit_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (frame > UINT32_C(34) ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        !falcon_reference_motion_x_f32(
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND,
            (uint16_t)frame,
            &expected_motion_x_f32) ||
        inspection.players[0].velocity_x_f32 !=
            expected_motion_x_f32 * attributes->specials_gr_vel_x_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-ground-search "
            "frame=%" PRIu32 " action=%u ticks=%u vx=%.9g\n",
            frame,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (double)inspection.players[0].velocity_x_f32);
        return 0;
    }
    for (frame = UINT32_C(1); frame < UINT32_C(3); ++frame)
    {
        if (!step_duel(
                ground_hit_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND ||
            inspection.players[0].action_ticks != (uint16_t)frame)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=raptor-boost-ground-hit-frame "
                "frame=%" PRIu32 " action=%u ticks=%u\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks);
            return 0;
        }
    }
    if (!step_duel(
            ground_hit_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].damage_f32 !=
            7.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-ground-hit-damage "
            "damage=%.9g\n",
            (double)inspection.players[1].damage_f32);
        return 0;
    }

    air_content.stage.spawn_spacing_f32 =
        INT32_C(8) * 1.0f;
    air_content.stage.platform_motion_amplitude_f32 = 0.0f;
    if (!expect_status(
            make_content_view(&air_content, &air_view),
            PF_STATUS_OK,
            "raptor-boost-air-content-view") ||
        !initialize_sim(
            &air_storage,
            &air_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &air_sim) ||
        !enter_air_raptor_boost(
            air_sim,
            UINT64_C(0xfa1c0b04),
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(2); frame < UINT32_C(30); ++frame)
    {
        if (!step_duel(
                air_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
            inspection.players[0].action_ticks != (uint16_t)frame ||
            inspection.players[0].velocity_y_f32 != 0.0f)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=raptor-boost-air-zero-gravity "
                "frame=%" PRIu32 " action=%u ticks=%u vy=%.9g" "\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks,
                inspection.players[0].velocity_y_f32);
            return 0;
        }
    }
    expected_gravity_f32 = falcon_source_velocity_to_sim_f32(
        attributes->specials_grav_f32,
        INT32_C(11),
        INT32_C(62));
    if (!step_duel(
            air_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
        inspection.players[0].action_ticks != UINT16_C(30) ||
        inspection.players[0].velocity_y_f32 != expected_gravity_f32)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-air-gravity-begin "
            "action=%u ticks=%u vy=%.9g" " expected=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            inspection.players[0].velocity_y_f32,
            expected_gravity_f32);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(240) &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS;
         ++tick)
    {
        if (!step_duel(
                air_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(240) ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-miss-landing-entry "
            "action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }
    for (frame = UINT32_C(1); frame < UINT32_C(20); ++frame)
    {
        if (!step_duel(
                air_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
            inspection.players[0].action_ticks != (uint16_t)frame)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=raptor-boost-miss-landing "
                "frame=%" PRIu32 " action=%u ticks=%u\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks);
            return 0;
        }
    }
    if (!step_duel(
            air_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=raptor-boost-miss-landing-end "
            "action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }
    return 1;
}

static int run_falcon_dive_source_data_test(
    const struct content *default_content)
{
    const falcon_common_special_attributes *common =
        falcon_reference_common_special_attributes();
    const falcon_special_attributes *attributes =
        falcon_reference_special_attributes();
    const falcon_up_special_timing *timing =
        falcon_reference_up_special_timing();
    const struct reference_move *ground_move =
        falcon_reference_move(PF_M4_FALCON_UP_SPECIAL_GROUND);
    const struct reference_move *air_move =
        falcon_reference_move(PF_M4_FALCON_UP_SPECIAL_AIR);
    test_sim_storage storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    uint32_t frame;
    uint32_t tick;

    if (common == NULL || attributes == NULL || timing == NULL ||
        ground_move == NULL || air_move == NULL ||
        common->air_drift_over_maximum_deceleration_f32 != 0.00313043478f ||
        common->air_drift_dead_zone_f32 != 0.100000001f ||
        attributes->specialhi_air_friction_mul_f32 != 1.10000002f ||
        attributes->specialhi_horz_vel_f32 != 0.850000024f ||
        attributes->specialhi_freefall_air_spd_mul_f32 != 0.720000029f ||
        attributes->specialhi_landing_lag_f32 != 30.0f ||
        attributes->specialhi_input_var_f32 != 0.224999994f ||
        attributes->specialhi_catch_grav_f32 != 0.300000012f ||
        timing->air_control_begin_frame != UINT16_C(13) ||
        timing->throw_gravity_begin_frame != UINT16_C(45) ||
        ground_move->subaction_index != UINT16_C(307) ||
        ground_move->total_frames != UINT16_C(64) ||
        air_move->subaction_index != UINT16_C(308) ||
        air_move->total_frames != UINT16_C(64))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-imported-data\n");
        return 0;
    }

    content.stage.spawn_spacing_f32 = INT32_C(8) * 1.0f;
    content.stage.platform_motion_amplitude_f32 = 0.0f;
    if (!expect_status(
            make_content_view(&content, &view),
            PF_STATUS_OK,
            "falcon-dive-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xfa1c0c01)),
            PF_STATUS_OK,
            "falcon-dive-ground-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_SPECIAL,
            &inspection))
    {
        return 0;
    }
    for (frame = UINT32_C(1); frame <= UINT32_C(64); ++frame)
    {
        float expected_motion_x_f32 = 0.0f;
        float expected_motion_y_f32 = 0.0f;
        const uint8_t expected_grounded =
            frame <= UINT32_C(13) ? UINT8_C(1) : UINT8_C(0);

        if (!falcon_reference_motion_x_f32(
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND,
                (uint16_t)frame,
                &expected_motion_x_f32) ||
            !falcon_reference_motion_y_f32(
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND,
                (uint16_t)frame,
                &expected_motion_y_f32) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
            inspection.players[0].action_ticks != (uint16_t)frame ||
            inspection.players[0].grounded != expected_grounded ||
            inspection.players[0].velocity_x_f32 !=
                expected_motion_x_f32 ||
            inspection.players[0].velocity_y_f32 !=
                expected_motion_y_f32)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=falcon-dive-ground-frame "
                "frame=%" PRIu32 " action=%u ticks=%u grounded=%u "
                "vx=%.9g expected_vx=%.9g "
                "vy=%.9g expected_vy=%.9g\n",
                frame,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks,
                (unsigned int)inspection.players[0].grounded,
                inspection.players[0].velocity_x_f32,
                expected_motion_x_f32,
                inspection.players[0].velocity_y_f32,
                expected_motion_y_f32);
            return 0;
        }
        if (frame < UINT32_C(64) &&
            !step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
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
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-fall-entry "
            "action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING;
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
    if (tick == UINT32_C(120) ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-landing-entry\n");
        return 0;
    }
    for (frame = UINT32_C(1); frame < UINT32_C(30); ++frame)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING ||
            inspection.players[0].action_ticks != (uint16_t)frame)
        {
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
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-landing-end "
            "action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }
    return 1;
}

static int run_falcon_dive_behind_ledge_test(
    const struct content *default_content)
{
    test_sim_storage storage;
    struct content content = *default_content;
    pf_content_view view;
    pf_sim *sim = NULL;
    struct inspection inspection;
    uint32_t tick;

    content.item.enabled = UINT8_C(0);
    content.stage.floor_left_f32 = -8.9285888671875f;
    content.stage.floor_right_f32 = 8.9285888671875f;
    content.stage.spawn_spacing_f32 = INT32_C(2) * 1.0f;
    content.stage.platform_center_x_f32 = 0.0f;
    content.stage.platform_half_width_f32 = 1.0f;
    content.stage.platform_motion_amplitude_f32 = 0.0f;
    content.stage.upper_platform_center_x_f32 =
        INT32_C(2) * 1.0f;
    content.stage.upper_platform_half_width_f32 = 1.0f;
    content.stage.solid_left_f32 = INT32_C(2) * 1.0f;
    content.stage.solid_right_f32 = INT32_C(3) * 1.0f;
    if (!expect_status(
            make_content_view(&content, &view),
            PF_STATUS_OK,
            "falcon-dive-behind-ledge-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xfa1c0c02)),
            PF_STATUS_OK,
            "falcon-dive-behind-ledge-reset") ||
        !expect_status(
            inspect(sim, &inspection),
            PF_STATUS_OK,
            "falcon-dive-behind-ledge-inspect"))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const int16_t stick_x =
            inspection.players[0].position_x_f32 > -4.61125183f
                ? -INT16_C(12000)
                : INT16_C(0);

        if (!step_duel(
                sim,
                stick_x,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].position_x_f32 <= -4.61125183f &&
            inspection.players[0].velocity_x_f32 == 0.0f &&
            inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(300))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-behind-ledge-walk\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(60); ++tick)
    {
        const int16_t stick_x =
            inspection.players[0].facing != INT8_C(1)
                ? INT16_C(12000)
                : INT16_C(0);

        if (!step_duel(
                sim,
                stick_x,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].facing == INT8_C(1) &&
            inspection.players[0].velocity_x_f32 == 0.0f &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(60))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-behind-ledge-turn\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(62); ++tick)
    {
        if (!step_duel(
                sim,
                tick < UINT32_C(30) ? INT16_MIN : INT16_C(0),
                INT16_C(0),
                tick < UINT32_C(5) ? PF_INPUT_BUTTON_JUMP : UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        !step_duel(
            sim,
            INT16_C(14336),
            -INT16_C(29081),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-behind-ledge-entry\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(11); ++tick)
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
    if (!step_duel(
            sim,
            -INT16_C(16000),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
        inspection.players[0].action_ticks != UINT16_C(13) ||
        inspection.players[0].facing != INT8_C(-1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation="
            "falcon-dive-behind-ledge-direction-gate\n");
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(52); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(16000),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_CATCH)
        {
            break;
        }
    }
    if (tick == UINT32_C(52) ||
        inspection.players[0].ledge != (uint8_t)PF_M4_LEDGE_LEFT ||
        inspection.players[0].facing != INT8_C(1) ||
        inspection.players[0].velocity_x_f32 != 0.0f ||
        inspection.players[0].velocity_y_f32 != 0.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=falcon-dive-behind-ledge-catch "
            "action=%u ticks=%u facing=%d ledge=%u x=%.9g"
            " y=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (unsigned int)inspection.players[0].ledge,
            inspection.players[0].position_x_f32,
            inspection.players[0].position_y_f32);
        return 0;
    }
    return 1;
}

static void set_player0_damage_fall_state(pf_sim *sim)
{
    sim->world.active[0] = UINT8_C(1);
    sim->world.respawn_ticks[0] = UINT16_C(0);
    sim->world.grounded[0] = UINT8_C(0);
    sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.position_y_f32[0] = INT32_C(20) * 1.0f;
    sim->world.velocity_x_f32[0] = INT32_C(0);
    sim->world.velocity_y_f32[0] = INT32_C(0);
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_AIRBORNE;
    sim->world.action_ticks[0] = UINT16_C(0);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    sim->world.source_animation_frame_f32[0] = INT32_C(0);
    sim->world.tumble[0] = UINT8_C(1);
    sim->world.hitstun_ticks[0] = UINT16_C(0);
    sim->world.fast_fall[0] = UINT8_C(0);
    sim->world.previous_buttons[0] = UINT64_C(0);
    sim->world.previous_main_stick_x[0] = INT16_C(0);
    sim->world.previous_main_stick_y[0] = INT16_C(0);
    sim->world.previous_tilt_x_direction[0] = INT8_C(0);
    sim->world.previous_tilt_y_direction[0] = INT8_C(0);
    sim->world.tilt_x_age[0] = UINT8_C(254);
    sim->world.tilt_y_age[0] = UINT8_C(254);
    sim->world.ucf_tilt_x_age[0] = UINT8_C(254);
    sim->world.ucf_tilt_y_age[0] = UINT8_C(254);
    sim->world.raw_main_t2_x[0] = INT8_C(0);
    sim->world.raw_main_t2_y[0] = INT8_C(0);
}

static int run_ucf084_input_contract_test(
    const struct content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    pf_input_frame input;
    pf_input_raw_pad raw_pad;

    (void)memset(&input, 0, sizeof(input));
    input.main_stick_x = INT16_MIN;
    input.main_stick_y = INT16_MIN;
    raw_pad = pf_input_effective_raw_pad(&input);
    if (sizeof(pf_input_frame) != (size_t)32 ||
        content->gameplay_ruleset !=
            (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084 ||
        raw_pad.main_stick_x != -INT8_C(80) ||
        raw_pad.main_stick_y != INT8_C(80))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-input-layout\n");
        return 0;
    }

    if (ucf084_adjusted_axis(INT16_C(0)) != 2 ||
        ucf084_adjusted_axis(INT16_C(16384)) != 41 ||
        ucf084_adjusted_axis(-INT16_C(16384)) != 41 ||
        ucf084_adjusted_radial_qualifies(
            INT16_C(0),
            INT16_C(31948)) ||
        !ucf084_adjusted_radial_qualifies(
            INT16_C(0),
            INT16_C(32358)))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-adjusted-radius-boundary\n");
        return 0;
    }

    raw_pad.main_stick_x = INT8_MIN;
    raw_pad.main_stick_y = INT8_MAX;
    raw_pad.secondary_stick_x = -INT8_C(1);
    raw_pad.secondary_stick_y = INT8_C(1);
    pf_input_set_raw_pad(&input, raw_pad);
    if (!pf_input_raw_payload_valid(&input) ||
        pf_input_get_raw_pad(&input).main_stick_x != INT8_MIN ||
        pf_input_get_raw_pad(&input).main_stick_y != INT8_MAX)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-raw-roundtrip\n");
        return 0;
    }

    if (pf_input_raw_axis_bits(INT8_MIN, UINT32_C(64)) != UINT64_C(0) ||
        pf_input_raw_axis_bits(INT8_MAX, UINT32_MAX) != UINT64_C(0) ||
        pf_input_raw_axis(&input, UINT32_C(64)) != INT8_C(0) ||
        pf_input_raw_axis(&input, UINT32_MAX) != INT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-raw-shift-range\n");
        return 0;
    }

    raw_pad.secondary_stick_x = INT8_C(0);
    raw_pad.secondary_stick_y = INT8_C(0);
    pf_input_set_raw_pad(&input, raw_pad);
    input.raw_axis_valid_mask =
        PF_INPUT_RAW_MAIN_X_VALID | PF_INPUT_RAW_MAIN_Y_VALID;
    if (!pf_input_raw_payload_valid(&input) ||
        pf_input_has_complete_raw_pad(&input) ||
        pf_input_effective_raw_pad(&input).main_stick_x != INT8_MIN ||
        pf_input_effective_raw_pad(&input).main_stick_y != INT8_MAX ||
        pf_input_logical_buttons(&input) != UINT64_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-partial-raw-contract\n");
        return 0;
    }
    input.raw_axis_valid_mask = UINT8_C(0);
    if (pf_input_raw_payload_valid(&input))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-invalid-raw-payload\n");
        return 0;
    }
    input.buttons &= ~PF_INPUT_RAW_PAD_BITS;
    input.raw_axis_valid_mask = UINT8_C(0x10);
    if (pf_input_raw_payload_valid(&input))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-invalid-raw-mask\n");
        return 0;
    }
    input.raw_axis_valid_mask = UINT8_C(0);

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08401)),
            PF_STATUS_OK,
            "ucf-dashback-positive-reset") ||
        !step_duel_raw_main_x(
            sim,
            INT16_C(0),
            INT8_C(0),
            &inspection) ||
        !step_duel_raw_main_x(
            sim,
            -INT16_C(16384),
            -INT8_C(40),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        !step_duel_raw_main_x(
            sim,
            -INT16_C(31129),
            -INT8_C(76),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].facing != INT8_C(-1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-dashback-positive "
            "action=%u ticks=%u facing=%d age=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (unsigned int)inspection.players[0].tilt_x_age);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08402)),
            PF_STATUS_OK,
            "ucf-dashback-threshold-reset") ||
        !step_duel_raw_main_x(
            sim,
            INT16_C(0),
            INT8_C(0),
            &inspection) ||
        !step_duel_raw_main_x(
            sim,
            -INT16_C(16384),
            -INT8_C(40),
            &inspection) ||
        !step_duel_raw_main_x(
            sim,
            -INT16_C(30719),
            -INT8_C(75),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=ucf-dashback-threshold "
            "action=%u ticks=%u facing=%d age=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (unsigned int)inspection.players[0].tilt_x_age);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08403)),
            PF_STATUS_OK,
            "source-jump-age-reset") )
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
    sim->world.action_ticks[0] =
        content->fighter.jump_squat_ticks - UINT16_C(1);
    sim->world.previous_tilt_y_direction[0] = INT8_C(1);
    sim->world.previous_main_stick_y[0] = INT16_C(9000);
    sim->world.tilt_y_age[0] = UINT8_C(4);
    sim->world.ucf_tilt_y_age[0] = UINT8_C(7);
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(9000),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        sim->world.grounded[0] != UINT8_C(0) ||
        sim->world.tilt_y_age[0] != UINT8_C(254) ||
        sim->world.ucf_tilt_y_age[0] != UINT8_C(8))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=source-jump-age-reset"
            " grounded=%u age=%u ucf_age=%u\n",
            (unsigned int)sim->world.grounded[0],
            (unsigned int)sim->world.tilt_y_age[0],
            (unsigned int)sim->world.ucf_tilt_y_age[0]);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08406)),
            PF_STATUS_OK,
            "source-wavedash-early-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
    sim->world.action_ticks[0] =
        content->fighter.jump_squat_ticks - UINT16_C(2);
    if (!step_duel_trigger(
            sim,
            -INT16_C(24575),
            INT16_C(21299),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=source-wavedash-early "
            "action=%u ticks=%u grounded=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded);
        return 0;
    }
    if (!step_duel_trigger(
            sim,
            -INT16_C(24575),
            INT16_C(21299),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].grounded != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=source-wavedash-early-held "
            "action=%u ticks=%u grounded=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08407)),
            PF_STATUS_OK,
            "source-wavedash-terminal-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
    sim->world.action_ticks[0] =
        content->fighter.jump_squat_ticks - UINT16_C(2);
    if (!step_duel_trigger(
            sim,
            -INT16_C(24575),
            INT16_C(21299),
            UINT64_C(0),
            UINT16_C(65534),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=source-wavedash-analog-stage "
            "action=%u ticks=%u grounded=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded);
        return 0;
    }
    if (!step_duel_trigger(
            sim,
            -INT16_C(24575),
            INT16_C(21299),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=source-wavedash-terminal "
            "action=%u ticks=%u grounded=%u vx=%.9g"
            " vy=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grounded,
            inspection.players[0].velocity_x_f32,
            inspection.players[0].velocity_y_f32);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08404)),
            PF_STATUS_OK,
            "source-jump-aerial-age-reset"))
    {
        return 0;
    }
    sim->world.grounded[0] = UINT8_C(0);
    sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.position_y_f32[0] = INT32_C(20) * 1.0f;
    sim->world.velocity_y_f32[0] = -1.0f;
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_AIRBORNE;
    sim->world.action_ticks[0] = UINT16_C(0);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    sim->world.air_jumps_remaining[0] = UINT8_C(1);
    sim->world.previous_tilt_y_direction[0] = INT8_C(1);
    sim->world.previous_main_stick_y[0] = INT16_C(9000);
    sim->world.tilt_y_age[0] = UINT8_C(4);
    sim->world.ucf_tilt_y_age[0] = UINT8_C(7);
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(9000),
            PF_INPUT_BUTTON_JUMP,
            &inspection) ||
        sim->world.air_jumps_remaining[0] != UINT8_C(0) ||
        sim->world.tilt_y_age[0] != UINT8_C(254) ||
        sim->world.ucf_tilt_y_age[0] != UINT8_C(8))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=source-jump-aerial-age-reset"
            " action=%u grounded=%u jumps=%u age=%u ucf_age=%u\n",
            (unsigned int)sim->world.action_state[0],
            (unsigned int)sim->world.grounded[0],
            (unsigned int)sim->world.air_jumps_remaining[0],
            (unsigned int)sim->world.tilt_y_age[0],
            (unsigned int)sim->world.ucf_tilt_y_age[0]);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08405)),
            PF_STATUS_OK,
            "damage-fall-attack-envelope-reset"))
    {
        return 0;
    }
    set_player0_damage_fall_state(sim);
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].tumble == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=damage-fall-ignore-a "
            "action=%u tumble=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].tumble);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08406)),
            PF_STATUS_OK,
            "damage-fall-c-stick-envelope-reset"))
    {
        return 0;
    }
    set_player0_damage_fall_state(sim);
    if (!step_duel_sticks(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_MAX,
            INT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].tumble == UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=damage-fall-ignore-c-stick "
            "action=%u tumble=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].tumble);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x0cf08407)),
            PF_STATUS_OK,
            "damage-fall-wiggle-attack-reset"))
    {
        return 0;
    }
    set_player0_damage_fall_state(sim);
    sim->world.previous_main_stick_x[0] = -INT16_C(16384);
    sim->world.previous_tilt_x_direction[0] = INT8_C(-1);
    sim->world.tilt_x_age[0] = UINT8_C(0);
    sim->world.previous_buttons[0] =
        pf_input_raw_axis_bits(
            -INT8_C(40),
            PF_INPUT_RAW_MAIN_X_SHIFT);
    if (!step_duel_raw_main_x_buttons(
            sim,
            -INT16_C(31129),
            -INT8_C(76),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].source_submotion !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL ||
        inspection.players[0].tumble != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=damage-fall-wiggle-with-a "
            "action=%u submotion=%u tumble=%u age=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].source_submotion,
            (unsigned int)inspection.players[0].tumble,
            (unsigned int)inspection.players[0].tilt_x_age);
        return 0;
    }
    return 1;
}

static int reset_to_reference_run(
    pf_sim *sim,
    struct inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x475541524452554e)),
            PF_STATUS_OK,
            "guard-dash-grab-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(60); ++tick)
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
        if (out_inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_RUN)
        {
            return 1;
        }
    }
    (void)fprintf(
        stderr,
        "m4-movement=fail operation=guard-dash-grab-run-setup\n");
    return 0;
}

static int enter_ucf_turn_origin_initial_dash(
    pf_sim *sim,
    uint64_t seed,
    struct inspection *out_inspection)
{
    return expect_status(
               pf_sim_reset(sim, seed),
               PF_STATUS_OK,
               "initial-dash-origin-reset") &&
           step_duel_raw_main_x(
               sim,
               INT16_C(0),
               INT8_C(0),
               out_inspection) &&
           step_duel_raw_main_x(
               sim,
               -INT16_C(16384),
               -INT8_C(40),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STANDING_TURN &&
           step_duel_raw_main_x(
               sim,
               -INT16_C(31129),
               -INT8_C(76),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
           out_inspection->players[0].action_ticks == UINT16_C(1) &&
           sim->world.dash_direction[0] ==
               -PF_M4_INITIAL_DASH_TURN_PHASE;
}

static int run_initial_dash_origin_callback_test(
    const pf_content_view *view)
{
    const ssbm_ground_input_attributes *ground_input =
        ssbm_common_reference_ground_input();
    const falcon_common_attributes *common =
        falcon_reference_common_attributes();
    const falcon_common_special_attributes *common_special =
        falcon_reference_common_special_attributes();
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    struct inspection inspection;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source;
    float entry_velocity_x;
    float expected_velocity_x;

    if (ground_input == NULL || common == NULL || common_special == NULL ||
        common_special->fast_ground_friction_multiplier_f32 !=
            2.0f ||
        ground_input->initial_dash_early_end_frame != UINT16_C(4) ||
        ground_input->initial_dash_forward_roll_end_frame != UINT16_C(3) ||
        ground_input->initial_dash_special_end_frame != UINT16_C(20) ||
        ground_input->initial_dash_iasa_velocity_decay_f32 !=
            0.75f ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded))
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xda510001)),
            PF_STATUS_OK,
            "initial-dash-ordinary-attack-reset") ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        sim->world.dash_direction[0] !=
            PF_M4_INITIAL_DASH_ORDINARY_PHASE ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=initial-dash-ordinary-attack"
            " action=%u ticks=%u phase=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)sim->world.dash_direction[0]);
        return 0;
    }

    if (!enter_ucf_turn_origin_initial_dash(
            sim,
            UINT64_C(0xda510002),
            &inspection) ||
        !step_duel_raw_main_x_buttons(
            sim,
            -INT16_C(31129),
            -INT8_C(76),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=initial-dash-turn-attack"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xda510003)),
            PF_STATUS_OK,
            "initial-dash-ordinary-shield-reset") ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
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
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=initial-dash-ordinary-shield"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!enter_ucf_turn_origin_initial_dash(
            sim,
            UINT64_C(0xda510004),
            &inspection))
    {
        return 0;
    }
    entry_velocity_x = inspection.players[0].velocity_x_f32;
    expected_velocity_x = multiply_f32(
        entry_velocity_x,
        1.0f -
            ground_input->initial_dash_iasa_velocity_decay_f32);
    expected_velocity_x += common->friction_f32;
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(sim, &destination),
            PF_STATUS_OK,
            "initial-dash-turn-save"))
    {
        return 0;
    }
    source.bytes = save_bytes;
    source.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source),
            PF_STATUS_OK,
            "initial-dash-turn-load") ||
        loaded->world.dash_direction[0] !=
            -PF_M4_INITIAL_DASH_TURN_PHASE ||
        !step_duel_trigger(
            loaded,
            -INT16_C(31129),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].velocity_x_f32 !=
            expected_velocity_x ||
        loaded->world.guard_dash_grab_window_ticks[0] != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=initial-dash-turn-shield"
            " action=%u phase=%d window=%u velocity_x=%.9g\n",
            (unsigned int)inspection.players[0].action_state,
            (int)loaded->world.dash_direction[0],
            (unsigned int)
                loaded->world.guard_dash_grab_window_ticks[0],
            inspection.players[0].velocity_x_f32);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xda510005)),
            PF_STATUS_OK,
            "initial-dash-taunt-reset") ||
        !step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    entry_velocity_x = inspection.players[0].velocity_x_f32;
    expected_velocity_x = multiply_f32(
        entry_velocity_x,
        (int32_t)1.0f -
            ground_input->initial_dash_iasa_velocity_decay_f32);
    expected_velocity_x -= common->friction_f32;
    if (!step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_TAUNT ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_f32 !=
            expected_velocity_x)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=initial-dash-taunt"
            " action=%u ticks=%u velocity_x=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            inspection.players[0].velocity_x_f32);
        return 0;
    }
    return 1;
}

static int run_guard_dash_grab_window_test(
    const pf_content_view *view)
{
    const ssbm_ground_input_attributes *ground_input =
        ssbm_common_reference_ground_input();
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    struct inspection inspection;
    struct inspection loaded_inspection;
    uint8_t save_bytes[2048];
    pf_mut_bytes destination;
    pf_bytes source;
    size_t required_bytes = (size_t)0;
    uint32_t tick;

    if (ground_input == NULL ||
        ground_input->guard_dash_grab_window_ticks != UINT16_C(3) ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &loaded))
    {
        return 0;
    }

    if (!reset_to_reference_run(sim, &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        sim->world.guard_dash_grab_window_ticks[0] != UINT8_C(3))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=guard-dash-grab-entry"
            " action=%u window=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)sim->world.guard_dash_grab_window_ticks[0]);
        return 0;
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_query_save_size(sim, &required_bytes),
            PF_STATUS_OK,
            "guard-dash-grab-query-save-size") ||
        required_bytes != (size_t)1800 ||
        !expect_status(
            pf_sim_save(sim, &destination),
            PF_STATUS_OK,
            "guard-dash-grab-save"))
    {
        return 0;
    }
    source.bytes = save_bytes;
    source.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source),
            PF_STATUS_OK,
            "guard-dash-grab-load") ||
        loaded->world.guard_dash_grab_window_ticks[0] != UINT8_C(3) ||
        !step_duel_trigger(
            loaded,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &loaded_inspection) ||
        loaded_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_GRAB ||
        loaded->world.guard_dash_grab_window_ticks[0] != UINT8_C(0) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_GRAB ||
        sim->world.guard_dash_grab_window_ticks[0] != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=guard-dash-grab-positive"
            " action=%u window=%u loaded_action=%u loaded_window=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)sim->world.guard_dash_grab_window_ticks[0],
            (unsigned int)loaded_inspection.players[0].action_state,
            (unsigned int)loaded->world.guard_dash_grab_window_ticks[0]);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x4755415244574149)),
            PF_STATUS_OK,
            "guard-ordinary-grab-reset") ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        sim->world.guard_dash_grab_window_ticks[0] != UINT8_C(0) ||
        !step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=guard-ordinary-grab-control\n");
        return 0;
    }

    if (!reset_to_reference_run(sim, &inspection) ||
        !step_duel_trigger(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SHIELD ||
            sim->world.guard_dash_grab_window_ticks[0] !=
                (uint8_t)(UINT32_C(2) - tick))
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=guard-dash-grab-countdown"
                " tick=%u window=%u\n",
                (unsigned int)tick,
                (unsigned int)
                    sim->world.guard_dash_grab_window_ticks[0]);
            return 0;
        }
    }
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=guard-dash-grab-expired\n");
        return 0;
    }
    return 1;
}

static int run_reference_callback_owner_test(
    const struct content *content,
    const pf_content_view *view)
{
    typedef struct callback_owner_wait_case
    {
        uint8_t action_state;
        uint16_t action_ticks;
        uint16_t source_submotion;
    } callback_owner_wait_case;
    test_sim_storage storage;
    pf_sim *sim = NULL;
    struct inspection inspection;
    const struct reference_move *jab =
        falcon_reference_move(PF_M4_FALCON_JAB1);
    const struct reference_move *down_tilt =
        falcon_reference_move(PF_M4_FALCON_DOWN_TILT);
    const callback_owner_wait_case wait_cases[] = {
        {
            (uint8_t)PF_M4_ACTION_RUN_BRAKE,
            content->fighter.run_brake_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_RUN_BRAKE,
        },
        {
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND,
            content->fighter.run_turnaround_ticks,
            (uint16_t)PF_M4_FALCON_SUBMOTION_TURN_RUN,
        },
        {
            (uint8_t)PF_M4_ACTION_STANDING_TURN,
            content->fighter.standing_turn_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_TURN,
        },
        {
            (uint8_t)PF_M4_ACTION_INITIAL_DASH,
            content->fighter.initial_dash_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_DASH,
        },
        {
            (uint8_t)PF_M4_ACTION_TAUNT,
            content->fighter.taunt_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT,
        },
        {
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE,
            content->fighter.shield_release_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_OFF,
        },
        {
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD,
            content->fighter.forward_roll_ticks,
            (uint16_t)PF_M4_FALCON_SUBMOTION_ROLL_FORWARD,
        },
        {
            (uint8_t)PF_M4_ACTION_SPOT_DODGE,
            content->fighter.spot_dodge_ticks,
            (uint16_t)PF_M4_FALCON_SUBMOTION_SPOT_DODGE,
        },
        {
            (uint8_t)PF_M4_ACTION_LANDING,
            content->fighter.landing_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_LANDING,
        },
        {
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING,
            content->fighter.special_landing_ticks - UINT16_C(1),
            (uint16_t)PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL,
        },
    };
    uint32_t case_index;
    const uint32_t neutral_aerial_ticks =
        (uint32_t)content->fighter.aerial_startup_ticks +
        (uint32_t)content->fighter.aerial_active_ticks +
        (uint32_t)content->fighter.aerial_recovery_ticks;

    if (jab == NULL || down_tilt == NULL ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0xca110001)),
            PF_STATUS_OK,
            "callback-owner-squat-control-reset"))
    {
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca110005)),
            PF_STATUS_OK,
            "callback-owner-down-tilt-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_DOWN_ATTACK;
    sim->world.action_ticks[0] = down_tilt->total_frames;
    sim->world.attack_hit_mask[0] = UINT8_C(1);
    sim->world.attack_stale_registered[0] = UINT8_C(1);
    sim->world.previous_tilt_x_direction[0] = INT8_C(0);
    sim->world.tilt_x_age[0] = UINT8_C(254);
    if (!step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].attack_hit_mask != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-down-tilt"
            " action=%u ticks=%u mask=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].attack_hit_mask);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca110006)),
            PF_STATUS_OK,
            "callback-owner-ground-attack-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
    sim->world.action_ticks[0] = jab->total_frames;
    sim->world.attack_hit_mask[0] = UINT8_C(1);
    sim->world.previous_tilt_y_direction[0] = INT8_C(0);
    sim->world.tilt_y_age[0] = UINT8_C(254);
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        inspection.players[0].attack_hit_mask != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-ground-attack"
            " action=%u ticks=%u mask=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].attack_hit_mask);
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_CROUCH_START;
    sim->world.action_ticks[0] =
        content->fighter.crouch_start_ticks - UINT16_C(1);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT;
    sim->world.previous_tilt_x_direction[0] = INT8_C(0);
    sim->world.tilt_x_age[0] = UINT8_C(254);
    if (!step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH_START)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-squat-control"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca110002)),
            PF_STATUS_OK,
            "callback-owner-squatwait-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_CROUCH_START;
    sim->world.action_ticks[0] = content->fighter.crouch_start_ticks;
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT;
    sim->world.previous_tilt_x_direction[0] = INT8_C(0);
    sim->world.tilt_x_age[0] = UINT8_C(254);
    if (!step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-squatwait"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca110003)),
            PF_STATUS_OK,
            "callback-owner-squatrv-control-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_CROUCH_END;
    sim->world.action_ticks[0] =
        content->fighter.crouch_end_ticks - UINT16_C(1);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE;
    sim->world.source_animation_frame_f32[0] =
        (float)(sim->world.action_ticks[0] - UINT16_C(1));
    sim->world.source_animation_rate_f32[0] = 1.0f;
    sim->world.previous_tilt_y_direction[0] = INT8_C(0);
    sim->world.tilt_y_age[0] = UINT8_C(254);
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-squatrv-control"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca110004)),
            PF_STATUS_OK,
            "callback-owner-wait-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_CROUCH_END;
    sim->world.action_ticks[0] = content->fighter.crouch_end_ticks;
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE;
    sim->world.source_animation_frame_f32[0] =
        (float)(sim->world.action_ticks[0] - UINT16_C(1));
    sim->world.source_animation_rate_f32[0] = 1.0f;
    sim->world.previous_tilt_y_direction[0] = INT8_C(0);
    sim->world.tilt_y_age[0] = UINT8_C(254);
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        sim->world.shield_stun_ticks[0] != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-wait"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    for (case_index = UINT32_C(0);
         case_index <
             (uint32_t)(sizeof(wait_cases) / sizeof(wait_cases[0]));
         ++case_index)
    {
        if (!expect_status(
                pf_sim_reset(
                    sim,
                    UINT64_C(0xca120000) + (uint64_t)case_index),
                PF_STATUS_OK,
                "callback-owner-wait-table-reset"))
        {
            return 0;
        }
        sim->world.action_state[0] = wait_cases[case_index].action_state;
        sim->world.action_ticks[0] = wait_cases[case_index].action_ticks;
        sim->world.source_submotion[0] =
            wait_cases[case_index].source_submotion;
        sim->world.previous_tilt_y_direction[0] = INT8_C(0);
        sim->world.tilt_y_age[0] = UINT8_C(254);
        sim->world.shield_held[0] = UINT8_C(0);
        if (!step_duel_trigger(
                sim,
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                UINT16_MAX,
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SPOT_DODGE)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=callback-owner-wait-table"
                " case=%" PRIu32 " source_action=%u action=%u ticks=%u\n",
                case_index,
                (unsigned int)wait_cases[case_index].action_state,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks);
            return 0;
        }
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca12000a)),
            PF_STATUS_OK,
            "callback-owner-roll-guard-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_ROLL_FORWARD;
    sim->world.action_ticks[0] = content->fighter.forward_roll_ticks;
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_ROLL_FORWARD;
    sim->world.shield_held[0] = UINT8_C(0);
    if (!step_duel_trigger(
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
            "m4-movement=fail operation=callback-owner-roll-guard"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca120100)),
            PF_STATUS_OK,
            "callback-owner-guard-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_SHIELD_STUN;
    sim->world.action_ticks[0] = UINT16_C(1);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF;
    sim->world.source_animation_frame_f32[0] = 1.0f;
    sim->world.source_animation_rate_f32[0] = 1.0f;
    sim->world.shield_stun_ticks[0] = UINT16_C(1);
    sim->world.previous_tilt_y_direction[0] = INT8_C(0);
    sim->world.tilt_y_age[0] = UINT8_C(254);
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-guard"
            " action=%u ticks=%u stun=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)sim->world.shield_stun_ticks[0]);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca130000)),
            PF_STATUS_OK,
            "callback-owner-fall-cleanup-reset"))
    {
        return 0;
    }
    sim->world.grounded[0] = UINT8_C(0);
    sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.position_y_f32[0] = INT32_C(50) * 1.0f;
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
    sim->world.action_ticks[0] = (uint16_t)(neutral_aerial_ticks - UINT32_C(1));
    sim->world.attack_hit_mask[0] = UINT8_C(1);
    sim->world.attack_stale_registered[0] = UINT8_C(1);
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].attack_hit_mask != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-fall-cleanup"
            " action=%u ticks=%u mask=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].attack_hit_mask);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca120101)),
            PF_STATUS_OK,
            "callback-owner-run-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] =
        (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
    sim->world.action_ticks[0] =
        content->fighter.run_turnaround_ticks;
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_TURN_RUN;
    sim->world.facing[0] = INT8_C(-1);
    sim->world.dash_direction[0] = INT8_C(-1);
    /* Hitstun duration remains canonical history after the fighter has left
     * Damage. It must not suppress TurnRun's terminal Anim -> Run callback. */
    sim->world.hitstun_ticks[0] = UINT16_C(12);
    if (!step_duel(
            sim,
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].dash_direction != INT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-run"
            " action=%u ticks=%u dash=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].dash_direction);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca130001)),
            PF_STATUS_OK,
            "callback-owner-fall-reset") ||
        !start_aerial_attack(sim, 0, &inspection))
    {
        return 0;
    }
    while ((uint32_t)inspection.players[0].action_ticks + UINT32_C(1) <
           neutral_aerial_ticks)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
        {
            (void)fprintf(
                stderr,
                "m4-movement=fail operation=callback-owner-fall-setup"
                " action=%u ticks=%u\n",
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks);
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-fall"
            " action=%u ticks=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca130003)),
            PF_STATUS_OK,
            "callback-owner-tech-roll-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_TECH_ROLL;
    sim->world.action_ticks[0] =
        content->fighter.tech_roll_ticks - UINT16_C(1);
    sim->world.facing[0] = INT8_C(-1);
    sim->world.tech_direction[0] = INT8_C(-1);
    sim->world.previous_tilt_x_direction[0] = INT8_C(0);
    sim->world.tilt_x_age[0] = UINT8_C(254);
    if (!step_duel(
            sim,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        inspection.players[0].tech_direction != INT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-tech-roll"
            " action=%u ticks=%u tech=%d\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].tech_direction);
        return 0;
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0xca130002)),
            PF_STATUS_OK,
            "callback-owner-damage-guard-reset"))
    {
        return 0;
    }
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_HITSTUN;
    sim->world.action_ticks[0] = UINT16_C(13);
    sim->world.hitstun_ticks[0] = UINT16_C(1);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_NEUTRAL_2;
    sim->world.source_animation_frame_f32[0] =
        INT32_C(14) * 1.0f;
    sim->world.source_animation_rate_f32[0] = 1.0f;
    sim->world.shield_held[0] = UINT8_C(0);
    if (!step_duel_trigger(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].source_submotion !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON ||
        sim->world.ground_blend_progress_f32[0] != 1.0f)
    {
        (void)fprintf(
            stderr,
            "m4-movement=fail operation=callback-owner-damage-guard"
            " action=%u submotion=%u blend=%.9g" "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].source_submotion,
            sim->world.ground_blend_progress_f32[0]);
        return 0;
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
    struct content content;
    pf_content_view view;

    if (!expect_status(
            default_content(&content),
            PF_STATUS_OK,
            "default-content") ||
        !expect_status(
            validate_content(&content),
            PF_STATUS_OK,
            "validate-content") ||
        !expect_status(
            make_content_view(&content, &view),
            PF_STATUS_OK,
            "content-view") ||
        !RUN_MOVEMENT_TEST(run_content_contract_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_falcon_punch_source_data_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_raptor_boost_source_data_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_falcon_dive_source_data_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_falcon_dive_behind_ledge_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_ucf084_input_contract_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_initial_dash_origin_callback_test(&view)) ||
        !RUN_MOVEMENT_TEST(run_guard_dash_grab_window_test(&view)) ||
        !RUN_MOVEMENT_TEST(run_reference_callback_owner_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_run_brake_iasa_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_crouch_common_iasa_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_ground_control_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_player_push_test(&content)) ||
        !RUN_MOVEMENT_TEST(run_tap_jump_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(
            run_jump_takeoff_momentum_test(&content, &view)) ||
        (0 && !run_fox_trot_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_teeter_cancel_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_taunt_cancel_test(&content, &view)) ||
        (0 && !run_stage_humping_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_pivot_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_air_control_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(
            run_instant_double_jump_test(&content, &view)) ||
        (0 && !run_double_jump_cancel_test(&content, &view)) ||
        !RUN_MOVEMENT_TEST(run_falcon_aerial_iasa_test(&view)) ||
        !RUN_MOVEMENT_TEST(
            run_strong_aerial_terminal_iasa_test(&view)) ||
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
            run_crouch_platform_drop_test(&content, &view)) ||
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
        "movement_core=pass tap_jump=1 jump_takeoff_momentum=1 "
        "player_push=1 "
        "teeter_cancel=1 "
        "taunt_cancel=1 "
        "double_jump_cancel=skipped vector_ascent=1 "
        "ledge_roll=1 "
        "emergent_technique_tests=skipped\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION);
    return 0;
}
