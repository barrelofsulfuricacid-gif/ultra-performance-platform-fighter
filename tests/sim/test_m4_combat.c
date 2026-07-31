#include "pf/m4.h"
#include "pf/replay.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_SAVE_CAPACITY 1024U
#define TEST_DETERMINISTIC_TICKS UINT64_C(20000)
#define TEST_PSC_REPLAY_TICKS UINT64_C(20)
#define TEST_PSC_REPLAY_INPUT_COUNT 40U
#define TEST_PSC_REPLAY_HASH_COUNT 21U
#define TEST_PSC_REPLAY_CAPACITY 8192U
#define TEST_ALC_REPLAY_TICKS UINT64_C(96)
#define TEST_ALC_REPLAY_INPUT_COUNT 192U
#define TEST_ALC_REPLAY_HASH_COUNT 97U
#define TEST_ALC_REPLAY_CAPACITY 16384U

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static pf_tick_result test_last_result;

static int fail(const char *operation)
{
    (void)fprintf(
        stderr,
        "m4-combat=fail operation=%s\n",
        operation);
    return 0;
}

static int expect_status(
    pf_status actual,
    pf_status expected,
    const char *operation)
{
    if (actual != expected)
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static int make_combat_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "combat-content-view");
}

static int make_reaction_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_combat_content(out_content, out_view))
    {
        return 0;
    }

    out_content->fighter.jab_base_knockback_x_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(10);
    out_content->fighter.jab_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.jab_knockback_growth_q16 =
        PF_Q16_ONE / INT32_C(4096);
    out_content->fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(20);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "reaction-content-view");
}

static int make_tech_invulnerability_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_combat_content(out_content, out_view))
    {
        return 0;
    }

    out_content->fighter.jab_base_knockback_x_q16 =
        PF_Q16_ONE / INT32_C(100);
    out_content->fighter.jab_base_knockback_y_q16 =
        (INT32_C(17) * PF_Q16_ONE) / INT32_C(20);
    out_content->fighter.jab_knockback_growth_q16 =
        PF_Q16_ONE / INT32_C(4096);
    out_content->fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(20);
    out_content->stage.platform_center_x_q16 =
        INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 =
        INT32_C(0);
    out_content->stage.solid_left_q16 =
        INT32_C(24) * PF_Q16_ONE;
    out_content->stage.solid_right_q16 =
        INT32_C(30) * PF_Q16_ONE;
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(2) * PF_Q16_ONE) / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "tech-invulnerability-content-view");
}

static int make_floor_attack_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_tech_invulnerability_content(
            out_content,
            out_view))
    {
        return 0;
    }

    out_content->fighter.ground_acceleration_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.turn_acceleration_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.traction_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.walk_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(20);
    out_content->fighter.run_speed_q16 =
        PF_Q16_ONE / INT32_C(5);
    out_content->fighter.initial_dash_speed_q16 =
        PF_Q16_ONE / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "floor-attack-content-view");
}

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    int reset,
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
    if (!expect_status(
            pf_sim_init(
                storage->state,
                sizeof(storage->state),
                storage->scratch,
                sizeof(storage->scratch),
                content,
                &config,
                out_sim),
            PF_STATUS_OK,
            "init"))
    {
        return 0;
    }
    return reset == 0 ||
           expect_status(
               pf_sim_reset(*out_sim, UINT64_C(0x4d34434f4d424154)),
               PF_STATUS_OK,
               "reset");
}

static void make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint8_t player_count,
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(
        inputs,
        0,
        sizeof(*inputs) * (size_t)PF_SIM_MAX_PLAYERS);
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

static int step_players_with_triggers(
    pf_sim *sim,
    uint8_t player_count,
    const int16_t axes_x[PF_SIM_MAX_PLAYERS],
    const int16_t axes_y[PF_SIM_MAX_PLAYERS],
    const uint64_t buttons[PF_SIM_MAX_PLAYERS],
    const uint16_t left_triggers[PF_SIM_MAX_PLAYERS],
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_m4_inspection before;
    uint32_t player_index;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, player_count, before.tick);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)player_count;
         ++player_index)
    {
        inputs[player_index].main_stick_x = axes_x[player_index];
        inputs[player_index].main_stick_y = axes_y[player_index];
        inputs[player_index].buttons = buttons[player_index];
        inputs[player_index].left_trigger =
            left_triggers[player_index];
    }
    return expect_status(
               pf_sim_tick(
                   sim,
                   inputs,
                   (size_t)player_count,
                   &test_last_result),
               PF_STATUS_OK,
               "tick") &&
           expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "inspect-after-step");
}

static int step_players(
    pf_sim *sim,
    uint8_t player_count,
    const int16_t axes_x[PF_SIM_MAX_PLAYERS],
    const int16_t axes_y[PF_SIM_MAX_PLAYERS],
    const uint64_t buttons[PF_SIM_MAX_PLAYERS],
    pf_m4_inspection *out_inspection)
{
    const uint16_t left_triggers[PF_SIM_MAX_PLAYERS] = {
        UINT16_C(0), UINT16_C(0), UINT16_C(0), UINT16_C(0)};

    return step_players_with_triggers(
        sim,
        player_count,
        axes_x,
        axes_y,
        buttons,
        left_triggers,
        out_inspection);
}

static int step_duel(
    pf_sim *sim,
    int16_t player0_x,
    uint64_t player0_buttons,
    int16_t player1_x,
    uint64_t player1_buttons,
    pf_m4_inspection *out_inspection)
{
    const int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        player0_x, player1_x, INT16_C(0), INT16_C(0)};
    const int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    const uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        player0_buttons,
        player1_buttons,
        UINT64_C(0),
        UINT64_C(0)};

    return step_players(
        sim,
        UINT8_C(2),
        axes_x,
        axes_y,
        buttons,
        out_inspection);
}

static int step_reaction_duel(
    pf_sim *sim,
    int16_t player0_x,
    int16_t player0_y,
    uint64_t player0_buttons,
    uint16_t player0_trigger,
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons,
    uint16_t player1_trigger,
    pf_m4_inspection *out_inspection)
{
    const int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        player0_x, player1_x, INT16_C(0), INT16_C(0)};
    const int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        player0_y, player1_y, INT16_C(0), INT16_C(0)};
    const uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        player0_buttons,
        player1_buttons,
        UINT64_C(0),
        UINT64_C(0)};
    const uint16_t left_triggers[PF_SIM_MAX_PLAYERS] = {
        player0_trigger,
        player1_trigger,
        UINT16_C(0),
        UINT16_C(0)};

    return step_players_with_triggers(
        sim,
        UINT8_C(2),
        axes_x,
        axes_y,
        buttons,
        left_triggers,
        out_inspection);
}

static int hash_equal(
    const pf_state_hash *left,
    const pf_state_hash *right)
{
    return left->algorithm == right->algorithm &&
           left->algorithm_version == right->algorithm_version &&
           left->digest_size == right->digest_size &&
           left->reserved == right->reserved &&
           memcmp(left->bytes, right->bytes, sizeof(left->bytes)) == 0;
}

static int run_one_way_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t frozen_x[2];
    int32_t frozen_y[2];
    uint32_t freeze_tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return fail("startup-and-active-schedule");
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.jab_hitlag_ticks ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.jab_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].last_hit_valid != UINT8_C(1) ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[1].last_hit_sequence != UINT32_C(1) ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].last_hit_tick + UINT64_C(1) !=
            inspection.tick ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        test_last_result.events[0].sequence !=
            inspection.players[1].last_hit_sequence ||
        test_last_result.events[0].tick !=
            inspection.players[1].last_hit_tick ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 !=
            content->fighter.jab_damage_q16 ||
        test_last_result.events[0].velocity_x_q16 <= INT32_C(0) ||
        test_last_result.events[0].velocity_y_q16 >= INT32_C(0) ||
        test_last_result.events[0].flags != UINT16_C(0) ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK ||
        (inspection.players[0].attack_hit_mask & UINT8_C(2)) ==
            UINT8_C(0))
    {
        return fail("damage-hitlag-and-event");
    }

    frozen_x[0] = inspection.players[0].position_x_q16;
    frozen_x[1] = inspection.players[1].position_x_q16;
    frozen_y[0] = inspection.players[0].position_y_q16;
    frozen_y[1] = inspection.players[1].position_y_q16;
    for (freeze_tick = UINT32_C(0);
         freeze_tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++freeze_tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
                &inspection) ||
            inspection.players[0].position_x_q16 != frozen_x[0] ||
            inspection.players[1].position_x_q16 != frozen_x[1] ||
            inspection.players[0].position_y_q16 != frozen_y[0] ||
            inspection.players[1].position_y_q16 != frozen_y[1])
        {
            return fail("hitlag-freeze");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[1].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[1].position_x_q16 <= frozen_x[1] ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16)
    {
        return fail("knockback-hitstun-and-single-hit");
    }

    return 1;
}

static int run_aerial_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t frozen_x[2];
    int32_t frozen_y[2];
    uint32_t tick;
    uint32_t hit_sequence;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection))
    {
        return fail("aerial-hit-jump-start");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("aerial-hit-jump-squat");
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("aerial-hit-attack-start");
    }

    for (tick = UINT32_C(0);
         tick <
                 (uint32_t)content->fighter.aerial_startup_ticks +
                     (uint32_t)content->fighter.aerial_active_ticks +
                     UINT32_C(2) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("aerial-hit-active-schedule");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].action_ticks !=
            content->fighter.aerial_startup_ticks ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.aerial_hitlag_ticks ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.aerial_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.aerial_damage_q16 ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.aerial_damage_q16 ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[1].last_hit_valid != UINT8_C(1) ||
        (inspection.players[0].attack_hit_mask & UINT8_C(2)) ==
            UINT8_C(0))
    {
        return fail("aerial-hit-damage-hitlag-and-event");
    }

    hit_sequence = inspection.players[1].last_hit_sequence;
    frozen_x[0] = inspection.players[0].position_x_q16;
    frozen_x[1] = inspection.players[1].position_x_q16;
    frozen_y[0] = inspection.players[0].position_y_q16;
    frozen_y[1] = inspection.players[1].position_y_q16;
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.aerial_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].position_x_q16 != frozen_x[0] ||
            inspection.players[1].position_x_q16 != frozen_x[1] ||
            inspection.players[0].position_y_q16 != frozen_y[0] ||
            inspection.players[1].position_y_q16 != frozen_y[1])
        {
            return fail("aerial-hitlag-freeze");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[1].velocity_y_q16 >= INT32_C(0))
    {
        return fail("aerial-hitlag-resume");
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.aerial_active_ticks +
                 UINT32_C(2);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[1].damage_q16 !=
                content->fighter.aerial_damage_q16 ||
            inspection.players[1].last_hit_sequence !=
                hit_sequence)
        {
            return fail("aerial-single-hit-per-target");
        }
    }
    return 1;
}

static int run_strong_aerial_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
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
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection))
    {
        return fail("strong-aerial-hit-jump-start");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("strong-aerial-hit-jump-squat");
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("strong-aerial-hit-attack-start");
    }

    for (tick = UINT32_C(0);
         tick <
                 (uint32_t)content->fighter.strong_startup_ticks +
                     (uint32_t)content->fighter.strong_active_ticks +
                     UINT32_C(2) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("strong-aerial-hit-active-schedule");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[1].last_hit_valid != UINT8_C(1) ||
        (inspection.players[0].attack_hit_mask & UINT8_C(2)) ==
            UINT8_C(0))
    {
        return fail("strong-aerial-hit-damage-hitlag-and-event");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.strong_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("strong-aerial-hitlag-step");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN)
    {
        return fail("strong-aerial-hitlag-resume");
    }
    return 1;
}

static int run_default_strong_tumble_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;
    int saw_tumble_hitstun = 0;
    int saw_knockdown = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("strong-attack-startup");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.strong_startup_ticks +
                    (uint32_t)content->fighter.strong_active_ticks +
                    UINT32_C(2) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("strong-attack-active-schedule");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].hitstun_ticks <
            content->fighter.tumble_hitstun_threshold_ticks ||
        inspection.players[1].tumble != UINT8_C(1) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        (test_last_result.events[0].flags &
         (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE) == UINT16_C(0) ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_STRONG_ATTACK)
    {
        return fail("default-strong-attack-enters-tumble");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.strong_hitlag_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("strong-hitlag-resume");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].tumble != UINT8_C(1))
    {
        return fail("strong-tumble-launch");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_HITSTUN &&
            inspection.players[1].tumble != UINT8_C(0))
        {
            saw_tumble_hitstun = 1;
        }
        if (inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_KNOCKDOWN)
        {
            saw_knockdown = 1;
            break;
        }
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("strong-tumble-landing");
        }
    }
    return (saw_tumble_hitstun != 0 && saw_knockdown != 0) ||
           fail("default-strong-tumble-route");
}

static int make_surface_tech_content(
    int ceiling_fixture,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "surface-tech-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 =
        ceiling_fixture != 0
            ? INT32_C(0)
            : (INT32_C(23) * PF_Q16_ONE) / INT32_C(10);
    out_content->stage.solid_right_q16 =
        INT32_C(6) * PF_Q16_ONE;
    out_content->stage.solid_top_q16 =
        INT32_C(14) * PF_Q16_ONE;
    out_content->stage.solid_bottom_q16 =
        INT32_C(29) * PF_Q16_ONE;
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "surface-tech-content-view");
}

static int drive_strong_to_surface(
    pf_sim *sim,
    int arm_tech,
    int16_t target_x,
    int16_t target_y,
    uint64_t target_buttons,
    uint8_t expected_action,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("surface-tech-strong-start");
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("surface-tech-strong-active");
        }
    }
    if (inspection.players[1].damage_q16 == UINT32_C(0) ||
        inspection.players[1].tumble != UINT8_C(1))
    {
        return fail("surface-tech-strong-did-not-tumble");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(90); ++tick)
    {
        const uint16_t trigger =
            arm_tech != 0 ? UINT16_MAX : UINT16_C(0);

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                target_y,
                target_buttons,
                trigger,
                &inspection))
        {
            return fail("surface-tech-impact-step");
        }
        if (inspection.players[1].action_state == expected_action)
        {
            *out_inspection = inspection;
            return 1;
        }
    }
    return fail("surface-tech-impact-not-observed");
}

static int run_surface_tech_test(
    const pf_m4_content *wall_content,
    const pf_content_view *wall_view,
    const pf_m4_content *ceiling_content,
    const pf_content_view *ceiling_view)
{
    test_sim_storage wall_storage;
    test_sim_storage wall_jump_storage;
    test_sim_storage wall_bounce_storage;
    test_sim_storage ceiling_storage;
    test_sim_storage ceiling_bounce_storage;
    pf_sim *wall = NULL;
    pf_sim *wall_jump = NULL;
    pf_sim *wall_bounce = NULL;
    pf_sim *ceiling = NULL;
    pf_sim *ceiling_bounce = NULL;
    pf_m4_inspection inspection;
    int32_t bounce_x;
    int32_t bounce_y;
    uint32_t tick;

    if (!initialize_sim(
            &wall_storage,
            wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &wall) ||
        !initialize_sim(
            &wall_jump_storage,
            wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &wall_jump) ||
        !initialize_sim(
            &wall_bounce_storage,
            wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &wall_bounce) ||
        !initialize_sim(
            &ceiling_storage,
            ceiling_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &ceiling) ||
        !initialize_sim(
            &ceiling_bounce_storage,
            ceiling_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &ceiling_bounce))
    {
        return fail("surface-tech-init");
    }

    if (!drive_strong_to_surface(
            wall,
            1,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_WALL_TECH,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        inspection.players[1].tech_window_ticks != UINT16_C(0) ||
        inspection.players[1].facing != INT8_C(-1) ||
        inspection.players[1].velocity_x_q16 != INT32_C(0) ||
        inspection.players[1].velocity_y_q16 != INT32_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("wall-tech-entry");
    }
    for (tick = UINT32_C(1);
         tick <= (uint32_t)wall_content->fighter.wall_tech_stall_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                wall,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("wall-tech-stall-step");
        }
        if (tick < (uint32_t)wall_content->fighter.wall_tech_stall_ticks &&
            (inspection.players[1].velocity_x_q16 != INT32_C(0) ||
             inspection.players[1].velocity_y_q16 != INT32_C(0)))
        {
            return fail("wall-tech-exact-stall");
        }
    }
    if (inspection.players[1].velocity_x_q16 >= INT32_C(0))
    {
        return fail("wall-tech-away-release");
    }

    if (!drive_strong_to_surface(
            wall_jump,
            1,
            INT16_C(0),
            INT16_C(-32767),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP,
            &inspection))
    {
        return fail("wall-tech-jump-entry");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)wall_content->fighter.wall_tech_stall_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                wall_jump,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("wall-tech-jump-stall-step");
        }
    }
    if (inspection.players[1].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[1].velocity_y_q16 >= INT32_C(0))
    {
        return fail("wall-tech-jump-release");
    }

    if (!drive_strong_to_surface(
            wall_bounce,
            0,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_WALL_BOUNCE,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(1) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        inspection.players[1].velocity_x_q16 >= INT32_C(0))
    {
        return fail("wall-bounce-preserves-reaction");
    }
    bounce_x = inspection.players[1].velocity_x_q16;
    bounce_y = inspection.players[1].velocity_y_q16;
    if (bounce_x == INT32_C(0) || bounce_y == INT32_C(0))
    {
        return fail("wall-bounce-reflects-motion");
    }

    if (!drive_strong_to_surface(
            ceiling,
            1,
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_CEILING_TECH,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 != INT32_C(0) ||
        inspection.players[1].velocity_x_q16 !=
            ceiling_content->fighter.ceiling_tech_speed_q16 ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("ceiling-tech-entry");
    }

    if (!drive_strong_to_surface(
            ceiling_bounce,
            0,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_CEILING_BOUNCE,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(1) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 <= INT32_C(0))
    {
        return fail("ceiling-bounce-preserves-reaction");
    }
    return 1;
}

static int run_whiff_and_trade_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_duel(
            sim,
            INT16_C(-13500),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].facing != INT8_C(-1) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("facing-away-whiff");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "trade-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return fail("trade-schedule");
    }

    if (inspection.players[0].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].last_hit_attacker != UINT8_C(1) ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[0].last_hit_sequence == UINT32_C(0) ||
        inspection.players[1].last_hit_sequence == UINT32_C(0) ||
        inspection.players[0].last_hit_sequence ==
            inspection.players[1].last_hit_sequence)
    {
        return fail("simultaneous-trade");
    }
    return 1;
}

static int start_normal_shield_block(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                out_inspection))
        {
            return 0;
        }
    }
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection);
}

static int start_powershield_block(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection);
}

static int advance_block_to_release(
    const pf_m4_content *content,
    pf_sim *sim,
    int powershield,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if ((powershield != 0
             ? !start_powershield_block(sim, out_inspection)
             : !start_normal_shield_block(sim, out_inspection)))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(600); ++tick)
    {
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
        {
            return 1;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int run_powershield_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage early_storage;
    test_sim_storage cancel_storage;
    test_sim_storage strong_cancel_storage;
    test_sim_storage normal_storage;
    pf_sim *early = NULL;
    pf_sim *cancel = NULL;
    pf_sim *strong_cancel = NULL;
    pf_sim *normal = NULL;
    pf_m4_inspection inspection;

    if (!initialize_sim(
            &early_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &early) ||
        !initialize_sim(
            &cancel_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &cancel) ||
        !initialize_sim(
            &normal_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &normal) ||
        !initialize_sim(
            &strong_cancel_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &strong_cancel) ||
        !advance_block_to_release(
            content,
            early,
            1,
            &inspection) ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(1))
    {
        return fail("powershield-cancel-opportunity");
    }
    if (!step_reaction_duel(
            early,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        inspection.players[1].action_ticks !=
            content->fighter.powershield_cancel_delay_ticks ||
        inspection.players[1].powershield != UINT8_C(1))
    {
        return fail("powershield-cancel-frame-one-rejected");
    }

    if (!advance_block_to_release(
            content,
            cancel,
            1,
            &inspection) ||
        !step_reaction_duel(
            cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_ticks !=
            content->fighter.powershield_cancel_delay_ticks ||
        !step_reaction_duel(
            cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-frame-two-attack");
    }

    if (!advance_block_to_release(
            content,
            strong_cancel,
            1,
            &inspection) ||
        !step_reaction_duel(
            strong_cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            strong_cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-frame-two-strong-attack");
    }

    if (!advance_block_to_release(
            content,
            normal,
            0,
            &inspection) ||
        inspection.players[1].powershield != UINT8_C(0) ||
        !step_reaction_duel(
            normal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            normal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        inspection.players[1].action_ticks != UINT16_C(2))
    {
        return fail("normal-shield-release-not-canceled");
    }
    return 1;
}

static int run_powershield_cancel_replay_test(
    const pf_content_view *view)
{
    test_sim_storage initial_storage;
    test_sim_storage source_storage;
    test_sim_storage playback_storage;
    pf_sim *initial = NULL;
    pf_sim *source = NULL;
    pf_sim *playback = NULL;
    pf_input_frame inputs[TEST_PSC_REPLAY_INPUT_COUNT];
    pf_state_hash hashes[TEST_PSC_REPLAY_HASH_COUNT];
    pf_input_frame tick_inputs[PF_SIM_MAX_PLAYERS];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    pf_tick_result result;
    pf_mut_bytes destination;
    pf_bytes replay;
    pf_m4_inspection inspection;
    uint8_t replay_bytes[TEST_PSC_REPLAY_CAPACITY];
    size_t replay_size = (size_t)0;
    uint64_t tick;
    int saw_powershield = 0;
    int saw_cancel_release = 0;
    int saw_cancel_attack = 0;

    if (!initialize_sim(
            &initial_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &initial) ||
        !initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &source) ||
        !initialize_sim(
            &playback_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &playback) ||
        !expect_status(
            pf_sim_clone(source, initial),
            PF_STATUS_OK,
            "powershield-cancel-replay-clone") ||
        !expect_status(
            pf_sim_hash(initial, &hashes[0]),
            PF_STATUS_OK,
            "powershield-cancel-replay-initial-hash"))
    {
        return fail("powershield-cancel-replay-init");
    }

    for (tick = UINT64_C(0);
         tick < TEST_PSC_REPLAY_TICKS;
         ++tick)
    {
        pf_input_frame *stored =
            &inputs[(size_t)tick * (size_t)UINT8_C(2)];

        make_inputs(tick_inputs, UINT8_C(2), tick);
        if (tick == UINT64_C(0))
        {
            tick_inputs[0].buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (tick == UINT64_C(2))
        {
            tick_inputs[1].left_trigger = UINT16_MAX;
        }
        if (tick == UINT64_C(12))
        {
            tick_inputs[1].buttons = PF_INPUT_BUTTON_ATTACK;
        }
        (void)memcpy(
            stored,
            tick_inputs,
            sizeof(*stored) * (size_t)UINT8_C(2));
        if (!expect_status(
                pf_sim_tick(
                    source,
                    tick_inputs,
                    (size_t)UINT8_C(2),
                    &result),
                PF_STATUS_OK,
                "powershield-cancel-replay-tick") ||
            !expect_status(
                pf_sim_hash(
                    source,
                    &hashes[(size_t)tick + (size_t)1]),
                PF_STATUS_OK,
                "powershield-cancel-replay-hash") ||
            !expect_status(
                pf_m4_inspect(source, &inspection),
                PF_STATUS_OK,
                "powershield-cancel-replay-inspect"))
        {
            return fail("powershield-cancel-replay-record");
        }
        if (inspection.players[1].powershield != UINT8_C(0))
        {
            saw_powershield = 1;
        }
        if (inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
            inspection.players[1].powershield != UINT8_C(0))
        {
            saw_cancel_release = 1;
        }
        if (tick == UINT64_C(12) &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
        {
            saw_cancel_attack = 1;
        }
    }

    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = inputs;
    replay_source.input_frame_count =
        (size_t)TEST_PSC_REPLAY_INPUT_COUNT;
    replay_source.state_hashes = hashes;
    replay_source.state_hash_count =
        (size_t)TEST_PSC_REPLAY_HASH_COUNT;
    replay_source.tick_count = TEST_PSC_REPLAY_TICKS;
    replay_source.final_result = result;
    destination.bytes = replay_bytes;
    destination.capacity = sizeof(replay_bytes);
    replay.bytes = replay_bytes;

    if (saw_powershield == 0 ||
        saw_cancel_release == 0 ||
        saw_cancel_attack == 0 ||
        !expect_status(
            pf_replay_query_size(&replay_source, &replay_size),
            PF_STATUS_OK,
            "powershield-cancel-replay-query") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("powershield-cancel-replay-trace");
    }
    destination.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_OK,
            "powershield-cancel-replay-encode"))
    {
        return fail("powershield-cancel-replay-encode");
    }
    replay.size = destination.size;
    if (!expect_status(
            pf_replay_verify(
                playback,
                replay,
                &verification),
            PF_STATUS_OK,
            "powershield-cancel-replay-verify") ||
        verification.verified_ticks !=
            TEST_PSC_REPLAY_TICKS ||
        !expect_status(
            pf_m4_inspect(playback, &inspection),
            PF_STATUS_OK,
            "powershield-cancel-replay-final-inspect") ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-replay-result");
    }
    return 1;
}

static int run_aerial_l_cancel_replay_test(void)
{
    test_sim_storage initial_storage;
    test_sim_storage source_storage;
    test_sim_storage playback_storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim *initial = NULL;
    pf_sim *source = NULL;
    pf_sim *playback = NULL;
    pf_input_frame inputs[TEST_ALC_REPLAY_INPUT_COUNT];
    pf_state_hash hashes[TEST_ALC_REPLAY_HASH_COUNT];
    pf_input_frame tick_inputs[PF_SIM_MAX_PLAYERS];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    pf_tick_result result;
    pf_mut_bytes destination;
    pf_bytes replay;
    pf_m4_inspection inspection;
    uint8_t replay_bytes[TEST_ALC_REPLAY_CAPACITY];
    size_t replay_size = (size_t)0;
    uint64_t tick;
    int attack_started = 0;
    int trigger_pressed = 0;
    int saw_aerial = 0;
    int saw_fast_fall = 0;
    int saw_l_cancel_eligible = 0;
    int saw_l_cancel_landing = 0;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "aerial-replay-default-content") ||
        !expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "aerial-replay-content-view") ||
        !initialize_sim(
            &initial_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &initial) ||
        !initialize_sim(
            &source_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &source) ||
        !initialize_sim(
            &playback_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &playback) ||
        !expect_status(
            pf_sim_clone(source, initial),
            PF_STATUS_OK,
            "aerial-replay-clone") ||
        !expect_status(
            pf_sim_hash(initial, &hashes[0]),
            PF_STATUS_OK,
            "aerial-replay-initial-hash") ||
        !expect_status(
            pf_m4_inspect(source, &inspection),
            PF_STATUS_OK,
            "aerial-replay-initial-inspect"))
    {
        return fail("aerial-replay-init");
    }

    for (tick = UINT64_C(0);
         tick < TEST_ALC_REPLAY_TICKS;
         ++tick)
    {
        pf_input_frame *stored =
            &inputs[(size_t)tick * (size_t)UINT8_C(2)];

        make_inputs(tick_inputs, UINT8_C(2), tick);
        if (tick == UINT64_C(0))
        {
            tick_inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
        }
        else if (
            attack_started == 0 &&
            inspection.players[0].grounded == UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE)
        {
            tick_inputs[0].buttons = PF_INPUT_BUTTON_ATTACK;
            attack_started = 1;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
        {
            tick_inputs[0].main_stick_y = INT16_MAX;
            if (trigger_pressed == 0 &&
                inspection.players[0].velocity_y_q16 >= INT32_C(0))
            {
                tick_inputs[0].left_trigger = UINT16_MAX;
                trigger_pressed = 1;
            }
        }

        (void)memcpy(
            stored,
            tick_inputs,
            sizeof(*stored) * (size_t)UINT8_C(2));
        if (!expect_status(
                pf_sim_tick(
                    source,
                    tick_inputs,
                    (size_t)UINT8_C(2),
                    &result),
                PF_STATUS_OK,
                "aerial-replay-tick") ||
            !expect_status(
                pf_sim_hash(
                    source,
                    &hashes[(size_t)tick + (size_t)1]),
                PF_STATUS_OK,
                "aerial-replay-hash") ||
            !expect_status(
                pf_m4_inspect(source, &inspection),
                PF_STATUS_OK,
                "aerial-replay-inspect"))
        {
            return fail("aerial-replay-record");
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
        {
            saw_aerial = 1;
        }
        if (inspection.players[0].fast_fall != UINT8_C(0))
        {
            saw_fast_fall = 1;
        }
        if (inspection.players[0].l_cancel_eligible != UINT8_C(0))
        {
            saw_l_cancel_eligible = 1;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING)
        {
            saw_l_cancel_landing = 1;
        }
    }

    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = inputs;
    replay_source.input_frame_count =
        (size_t)TEST_ALC_REPLAY_INPUT_COUNT;
    replay_source.state_hashes = hashes;
    replay_source.state_hash_count =
        (size_t)TEST_ALC_REPLAY_HASH_COUNT;
    replay_source.tick_count = TEST_ALC_REPLAY_TICKS;
    replay_source.final_result = result;
    destination.bytes = replay_bytes;
    destination.capacity = sizeof(replay_bytes);
    replay.bytes = replay_bytes;

    if (saw_aerial == 0 ||
        saw_fast_fall == 0 ||
        saw_l_cancel_eligible == 0 ||
        saw_l_cancel_landing == 0 ||
        !expect_status(
            pf_replay_query_size(&replay_source, &replay_size),
            PF_STATUS_OK,
            "aerial-replay-query") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("aerial-replay-trace");
    }
    destination.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_OK,
            "aerial-replay-encode"))
    {
        return fail("aerial-replay-encode");
    }
    replay.size = destination.size;
    if (!expect_status(
            pf_replay_verify(
                playback,
                replay,
                &verification),
            PF_STATUS_OK,
            "aerial-replay-verify") ||
        verification.verified_ticks != TEST_ALC_REPLAY_TICKS ||
        !expect_status(
            pf_m4_inspect(playback, &inspection),
            PF_STATUS_OK,
            "aerial-replay-final-inspect") ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].grounded != UINT8_C(1))
    {
        return fail("aerial-replay-result");
    }
    return 1;
}

static int run_shield_state_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t run_velocity;
    uint32_t shield_health;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return fail("shield-state-init");
    }
    if (!step_reaction_duel(
            sim,
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "shield-state-reset"))
    {
        return fail("initial-dash-cannot-shield");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(11); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(32767),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("shield-stop-run-setup");
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        run_velocity <= INT32_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_x_q16 >= run_velocity ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 -
                content->fighter.shield_hold_depletion_q16)
    {
        return fail("shield-stop-and-entry");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("shield-minimum-hold-release");
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
        {
            break;
        }
    }
    if (inspection.players[0].action_state !=
        (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
    {
        return fail("shield-release-entry");
    }
    shield_health = inspection.players[0].shield_health_q16;
    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].shield_health_q16 <= shield_health ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        return fail("shield-regeneration-and-jump-cancel");
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "long-shield-reset"))
    {
        return fail("long-shield-reset");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("long-shield-hold");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].action_ticks !=
            content->fighter.shield_minimum_hold_ticks)
    {
        return fail("shield-action-timer-saturates");
    }
    return 1;
}

static int run_dashing_shield_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage tap_storage;
    test_sim_storage held_storage;
    test_sim_storage loaded_storage;
    test_sim_storage idle_storage;
    pf_sim *tap = NULL;
    pf_sim *held = NULL;
    pf_sim *loaded = NULL;
    pf_sim *idle = NULL;
    pf_m4_inspection tap_inspection;
    pf_m4_inspection held_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection idle_inspection;
    pf_state_hash tap_hash;
    pf_state_hash held_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    int32_t run_start_x;
    uint32_t tick;

    if (!initialize_sim(
            &tap_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &tap) ||
        !initialize_sim(
            &held_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &held) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &idle_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &idle))
    {
        return fail("dashing-shield-init");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.initial_dash_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                tap,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &tap_inspection) ||
            !step_reaction_duel(
                held,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &held_inspection))
        {
            return fail("dashing-shield-run-setup");
        }
    }
    run_start_x = tap_inspection.players[0].position_x_q16;
    if (tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        held_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        tap_inspection.players[0].velocity_x_q16 !=
            held_inspection.players[0].velocity_x_q16 ||
        tap_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        !step_reaction_duel(
            tap,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &tap_inspection) ||
        !step_reaction_duel(
            held,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &held_inspection) ||
        tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        tap_inspection.players[0].action_ticks != UINT16_C(1) ||
        tap_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        tap_inspection.players[0].position_x_q16 <= run_start_x ||
        !expect_status(
            pf_sim_hash(tap, &tap_hash),
            PF_STATUS_OK,
            "dashing-shield-tap-hash") ||
        !expect_status(
            pf_sim_hash(held, &held_hash),
            PF_STATUS_OK,
            "dashing-shield-held-hash") ||
        !hash_equal(&tap_hash, &held_hash) ||
        !expect_status(
            pf_sim_query_save_size(tap, &save_size),
            PF_STATUS_OK,
            "dashing-shield-query-save-size") ||
        save_size != (size_t)611)
    {
        return fail("dashing-shield-entry");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(tap, &destination),
            PF_STATUS_OK,
            "dashing-shield-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "dashing-shield-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.shield_minimum_hold_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                tap,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &tap_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !step_reaction_duel(
                held,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &held_inspection) ||
            !expect_status(
                pf_sim_hash(tap, &tap_hash),
                PF_STATUS_OK,
                "dashing-shield-release-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "dashing-shield-loaded-release-hash") ||
            !hash_equal(&tap_hash, &loaded_hash))
        {
            return fail("dashing-shield-minimum-hold");
        }
    }
    if (tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        tap_inspection.players[0].action_ticks != UINT16_C(0) ||
        held_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        held_inspection.players[0].action_ticks !=
            content->fighter.shield_minimum_hold_ticks ||
        tap_inspection.players[0].position_x_q16 !=
            held_inspection.players[0].position_x_q16 ||
        tap_inspection.players[0].position_x_q16 <= run_start_x ||
        tap_inspection.players[0].velocity_x_q16 !=
            held_inspection.players[0].velocity_x_q16)
    {
        return fail("dashing-shield-tap-versus-held");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.shield_release_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                tap,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &tap_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !step_reaction_duel(
                held,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &held_inspection) ||
            !expect_status(
                pf_sim_hash(tap, &tap_hash),
                PF_STATUS_OK,
                "dashing-shield-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "dashing-shield-loaded-future-hash") ||
            !hash_equal(&tap_hash, &loaded_hash))
        {
            return fail("dashing-shield-release-duration");
        }
    }
    if (tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        held_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        tap_inspection.players[0].position_x_q16 !=
            held_inspection.players[0].position_x_q16 ||
        tap_inspection.players[0].position_x_q16 <= run_start_x ||
        tap_inspection.players[0].shield_health_q16 <=
            held_inspection.players[0].shield_health_q16)
    {
        return fail("dashing-shield-recovery-versus-held");
    }

    if (!expect_status(
            pf_m4_inspect(idle, &idle_inspection),
            PF_STATUS_OK,
            "dashing-shield-idle-inspect"))
    {
        return 0;
    }
    run_start_x = idle_inspection.players[0].position_x_q16;
    if (!step_reaction_duel(
            idle,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &idle_inspection) ||
        idle_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        idle_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        idle_inspection.players[0].position_x_q16 != run_start_x)
    {
        return fail("dashing-shield-idle-negative");
    }
    return 1;
}

static int run_shield_block_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage normal_storage;
    test_sim_storage power_storage;
    pf_sim *normal = NULL;
    pf_sim *power = NULL;
    pf_m4_inspection normal_inspection;
    pf_m4_inspection power_inspection;
    const uint32_t shield_damage =
        (uint32_t)(((uint64_t)content->fighter.jab_damage_q16 *
                    (uint64_t)content->fighter
                        .shield_damage_multiplier_q16) >>
                   16U);
    const uint32_t normal_expected_health =
        content->fighter.shield_health_q16 -
        UINT32_C(8) *
            content->fighter.shield_hold_depletion_q16 -
        shield_damage;
    const uint32_t power_expected_health =
        content->fighter.shield_health_q16 -
        content->fighter.shield_hold_depletion_q16;
    int32_t normal_pushback;
    uint16_t normal_shield_stun;
    uint32_t tick;

    if (!initialize_sim(
            &normal_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &normal) ||
        !initialize_sim(
            &power_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &power) ||
        !start_normal_shield_block(normal, &normal_inspection))
    {
        return fail("normal-shield-block-setup");
    }
    if (normal_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        normal_inspection.players[1].damage_q16 != UINT32_C(0) ||
        normal_inspection.players[1].last_hit_valid != UINT8_C(0) ||
        normal_inspection.players[1].powershield != UINT8_C(0) ||
        normal_inspection.players[1].shield_stun_ticks ==
            UINT16_C(0) ||
        normal_inspection.players[1].shield_health_q16 !=
            normal_expected_health ||
        normal_inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        normal_inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 != shield_damage ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return fail("normal-shield-damage-stun-pushback");
    }
    normal_pushback =
        normal_inspection.players[1].velocity_x_q16;
    normal_shield_stun =
        normal_inspection.players[1].shield_stun_ticks;

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                normal,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &normal_inspection))
        {
            return fail("shield-hitlag-step");
        }
    }
    if (normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        normal_inspection.players[1].hitlag_ticks != UINT16_C(0))
    {
        return fail("shield-hitlag-resume");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_reaction_duel(
                normal,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &normal_inspection))
        {
            return fail("shield-stun-step");
        }
        if (normal_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD)
        {
            break;
        }
    }
    if (normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        normal_inspection.players[1].shield_stun_ticks !=
            UINT16_C(0))
    {
        return fail("shield-stun-duration");
    }

    if (!start_powershield_block(power, &power_inspection) ||
        power_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        power_inspection.players[1].damage_q16 != UINT32_C(0) ||
        power_inspection.players[1].powershield != UINT8_C(1) ||
        power_inspection.players[1].shield_health_q16 !=
            power_expected_health ||
        power_inspection.players[1].shield_stun_ticks !=
            normal_shield_stun ||
        power_inspection.players[1].velocity_x_q16 <=
            normal_pushback ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_POWERSHIELD ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 != UINT32_C(0))
    {
        return fail("powershield-window-and-zero-damage");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                power,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &power_inspection))
        {
            return fail("powershield-hitlag-step");
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_reaction_duel(
                power,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &power_inspection))
        {
            return fail("powershield-stun-step");
        }
        if (power_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD)
        {
            break;
        }
    }
    if (power_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        power_inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-indicator-clears");
    }
    return 1;
}

static int make_shield_break_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "shield-break-default-content"))
    {
        return 0;
    }
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.shield_health_q16 =
        UINT32_C(4) * UINT32_C(65536);
    out_content->fighter.shield_reset_health_q16 =
        UINT32_C(2) * UINT32_C(65536);
    out_content->fighter.shield_hold_depletion_q16 =
        UINT32_C(655);
    out_content->fighter.shield_attacker_pushback_damage_q16 =
        INT32_C(1);
    out_content->fighter.shield_attacker_pushback_base_q16 =
        INT32_C(1);
    out_content->fighter.gravity_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.fall_speed_q16 =
        (INT32_C(2) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.shield_break_launch_speed_q16 =
        PF_Q16_ONE / INT32_C(2);
    out_content->fighter.shield_break_stun_ticks = UINT16_C(20);
    out_content->fighter.shield_break_minimum_stun_ticks =
        UINT16_C(8);
    out_content->fighter.shield_break_down_ticks = UINT16_C(12);
    out_content->fighter.shield_break_stand_ticks = UINT16_C(12);
    out_content->fighter.shield_break_mash_reduction_ticks =
        UINT16_C(3);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "shield-break-content-view");
}

static int advance_shield_break_to_stun(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection,
    int test_early_invulnerability)
{
    int tested_early_invulnerability = 0;
    int saw_down = 0;
    int saw_stand = 0;
    uint32_t tick;

    if (!start_normal_shield_block(sim, out_inspection) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        out_inspection->players[1].shield_health_q16 !=
            UINT32_C(0) ||
        out_inspection->players[1].powershield != UINT8_C(0) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BREAK ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1))
    {
        return fail("shield-break-hit-setup");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                out_inspection))
        {
            return fail("shield-break-hitlag");
        }
    }
    if (out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        out_inspection->players[1].grounded != UINT8_C(0) ||
        out_inspection->players[1].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        out_inspection->players[1].velocity_x_q16 != INT32_C(0) ||
        out_inspection->players[1].velocity_y_q16 !=
            -content->fighter.shield_break_launch_speed_q16 ||
        out_inspection->players[1].invulnerable != UINT8_C(1))
    {
        return fail("shield-break-launch");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(256); ++tick)
    {
        uint32_t attack_tick;

        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            break;
        }
        if (test_early_invulnerability != 0 &&
            tested_early_invulnerability == 0 &&
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            for (attack_tick = UINT32_C(0);
                 attack_tick < UINT32_C(3);
                 ++attack_tick)
            {
                if (!step_reaction_duel(
                        sim,
                        INT16_C(0),
                        INT16_C(0),
                        attack_tick == UINT32_C(0)
                            ? PF_INPUT_BUTTON_ATTACK
                            : UINT64_C(0),
                        UINT16_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        UINT16_C(0),
                        out_inspection) ||
                    out_inspection->players[1].damage_q16 !=
                        UINT32_C(0) ||
                    out_inspection->players[1].shield_health_q16 !=
                        UINT32_C(0) ||
                    out_inspection->players[1].action_state ==
                        (uint8_t)PF_M4_ACTION_HITLAG ||
                    out_inspection->players[1].invulnerable !=
                        UINT8_C(1))
                {
                    return fail(
                        "shield-break-early-invulnerability");
                }
            }
            tested_early_invulnerability = 1;
            continue;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return fail("shield-break-sequence-step");
        }
        if (out_inspection->players[1].shield_health_q16 !=
            UINT32_C(0))
        {
            return fail("shield-break-sequence-health");
        }
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            if (out_inspection->players[1].grounded != UINT8_C(0) ||
                out_inspection->players[1].velocity_x_q16 !=
                    INT32_C(0) ||
                out_inspection->players[1].invulnerable != UINT8_C(1))
            {
                return fail("shield-break-flight");
            }
        }
        else if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN)
        {
            saw_down = 1;
            if (out_inspection->players[1].grounded != UINT8_C(1) ||
                out_inspection->players[1].invulnerable != UINT8_C(1))
            {
                return fail("shield-break-down");
            }
        }
        else if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
        {
            saw_stand = 1;
            if (saw_down == 0 ||
                out_inspection->players[1].grounded != UINT8_C(1) ||
                out_inspection->players[1].invulnerable != UINT8_C(1))
            {
                return fail("shield-break-stand");
            }
        }
        else if (out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            return fail("shield-break-phase-order");
        }
    }

    if (tick == UINT32_C(256) ||
        saw_down == 0 ||
        saw_stand == 0 ||
        (test_early_invulnerability != 0 &&
         tested_early_invulnerability == 0) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
        out_inspection->players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks ||
        out_inspection->players[1].grounded != UINT8_C(1) ||
        out_inspection->players[1].invulnerable != UINT8_C(0))
    {
        return fail("shield-break-stun-entry");
    }
    return 1;
}

static int run_shield_break_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage punish_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *punish = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection punish_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !advance_shield_break_to_stun(
            content,
            source,
            &source_inspection,
            1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-shield-break-save-size") ||
        save_size != (size_t)611)
    {
        return fail("shield-break-snapshot-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-shield-break-stun") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-shield-break-stun") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-shield-break") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-shield-break") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("shield-break-snapshot-round-trip");
    }

    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &source_inspection) ||
        !step_reaction_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &loaded_inspection) ||
        source_inspection.players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks -
                UINT16_C(1) -
                content->fighter
                    .shield_break_mash_reduction_ticks ||
        source_inspection.players[1].tech_window_ticks !=
            UINT16_C(0) ||
        source_inspection.players[1].tech_lockout_ticks !=
            UINT16_C(0) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-shield-break-mash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-shield-break-mash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("shield-break-fresh-mash");
    }
    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &source_inspection) ||
        !step_reaction_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &loaded_inspection) ||
        source_inspection.players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks -
                UINT16_C(2) -
                content->fighter
                    .shield_break_mash_reduction_ticks)
    {
        return fail("shield-break-held-input-no-remash");
    }
    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        !step_reaction_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &loaded_inspection) ||
        source_inspection.players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks -
                UINT16_C(3) -
                UINT16_C(2) *
                    content->fighter
                        .shield_break_mash_reduction_ticks ||
        source_inspection.players[1].tech_window_ticks !=
            UINT16_C(0) ||
        source_inspection.players[1].tech_lockout_ticks !=
            UINT16_C(0))
    {
        return fail("shield-break-trigger-mash-no-tech-buffer");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        const uint64_t mash =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                mash,
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                mash,
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-shield-break-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-shield-break-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail(
                "shield-break-deterministic-continuation");
        }
        if (source_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(32) ||
        source_inspection.players[1].shield_health_q16 !=
            content->fighter.shield_reset_health_q16 ||
        loaded_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("shield-break-mash-recovery");
    }

    if (!initialize_sim(
            &punish_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &punish) ||
        !advance_shield_break_to_stun(
            content,
            punish,
            &punish_inspection,
            0) ||
        punish_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("shield-break-punish-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                punish,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &punish_inspection))
        {
            return fail("shield-break-punish-step");
        }
    }
    if (punish_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        punish_inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        punish_inspection.players[1].shield_health_q16 !=
            content->fighter.shield_reset_health_q16 ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT)
    {
        return fail("shield-break-vulnerable-stun-punish");
    }
    return 1;
}

static int start_reaction_hit(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_HITLAG;
}

static int run_di_and_sdi_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage sdi_storage;
    test_sim_storage neutral_storage;
    test_sim_storage di_storage;
    pf_sim *sdi_sim = NULL;
    pf_sim *neutral_sim = NULL;
    pf_sim *di_sim = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection neutral_inspection;
    pf_m4_inspection di_inspection;
    int32_t hit_x;
    int32_t hit_y;
    int32_t first_pulse_x;
    uint32_t freeze_tick;
    int64_t neutral_speed_squared;
    int64_t di_speed_squared;
    int64_t speed_difference;

    if (!initialize_sim(
            &sdi_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sdi_sim) ||
        !initialize_sim(
            &neutral_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &neutral_sim) ||
        !initialize_sim(
            &di_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &di_sim) ||
        !start_reaction_hit(sdi_sim, &inspection))
    {
        return fail("reaction-init");
    }

    hit_x = inspection.players[1].position_x_q16;
    hit_y = inspection.players[1].position_y_q16;
    if (!step_reaction_duel(
            sdi_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
        inspection.players[1].position_x_q16 <= hit_x ||
        inspection.players[1].position_y_q16 != hit_y)
    {
        return fail("sdi-first-component");
    }
    first_pulse_x = inspection.players[1].position_x_q16;

    if (!step_reaction_duel(
            sdi_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
        inspection.players[1].position_x_q16 != first_pulse_x ||
        !step_reaction_duel(
            sdi_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(-32767),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(2) ||
        inspection.players[1].position_x_q16 <= first_pulse_x ||
        inspection.players[1].position_y_q16 >= hit_y)
    {
        return fail("sdi-hold-and-quarter-circle");
    }

    if (!start_reaction_hit(neutral_sim, &neutral_inspection) ||
        !start_reaction_hit(di_sim, &di_inspection))
    {
        return fail("di-hit-setup");
    }
    for (freeze_tick = UINT32_C(0);
         freeze_tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++freeze_tick)
    {
        const int16_t di_y =
            freeze_tick + UINT32_C(1) ==
                    (uint32_t)content->fighter.jab_hitlag_ticks
                ? INT16_C(-32767)
                : INT16_C(0);

        if (!step_reaction_duel(
                neutral_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &neutral_inspection) ||
            !step_reaction_duel(
                di_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                di_y,
                UINT64_C(0),
                UINT16_C(0),
                &di_inspection))
        {
            return fail("di-freeze-step");
        }
    }

    neutral_speed_squared =
        (int64_t)neutral_inspection.players[1].velocity_x_q16 *
            (int64_t)neutral_inspection.players[1].velocity_x_q16 +
        (int64_t)neutral_inspection.players[1].velocity_y_q16 *
            (int64_t)neutral_inspection.players[1].velocity_y_q16;
    di_speed_squared =
        (int64_t)di_inspection.players[1].velocity_x_q16 *
            (int64_t)di_inspection.players[1].velocity_x_q16 +
        (int64_t)di_inspection.players[1].velocity_y_q16 *
            (int64_t)di_inspection.players[1].velocity_y_q16;
    speed_difference =
        di_speed_squared >= neutral_speed_squared
            ? di_speed_squared - neutral_speed_squared
            : neutral_speed_squared - di_speed_squared;
    if (neutral_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        di_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        neutral_inspection.players[1].tumble != UINT8_C(1) ||
        di_inspection.players[1].velocity_y_q16 >=
            neutral_inspection.players[1].velocity_y_q16 ||
        di_inspection.players[1].velocity_x_q16 >=
            neutral_inspection.players[1].velocity_x_q16 ||
        speed_difference > neutral_speed_squared / INT64_C(100))
    {
        return fail("trajectory-di-angle-and-magnitude");
    }
    return 1;
}

static int run_until_reaction_landing(
    pf_sim *sim,
    int tech_mode,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    int trigger_sent = 0;

    if (!start_reaction_hit(sim, out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const pf_m4_player_inspection *target =
            &out_inspection->players[1];
        const int should_trigger =
            tech_mode != 0 &&
            trigger_sent == 0 &&
            target->action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            target->velocity_y_q16 > INT32_C(0) &&
            target->position_y_q16 +
                    INT32_C(4) * PF_Q16_ONE >=
                INT32_C(32) * PF_Q16_ONE;
        const int16_t target_x =
            tech_mode > 1 &&
                    (should_trigger || trigger_sent != 0)
                ? INT16_C(32767)
                : INT16_C(0);

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                INT16_C(0),
                UINT64_C(0),
                should_trigger
                    ? UINT16_MAX
                    : UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (should_trigger)
        {
            trigger_sent = 1;
        }
        if (out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_ROLL)
        {
            return tech_mode == 0 || trigger_sent != 0;
        }
    }
    return 0;
}

static int advance_missed_tech_to_down_wait(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint16_t knockdown_tick;

    if (!run_until_reaction_landing(
            sim,
            0,
            out_inspection) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
        out_inspection->players[1].action_ticks != UINT16_C(0) ||
        out_inspection->players[1].invulnerable != UINT8_C(0))
    {
        return 0;
    }

    for (knockdown_tick = UINT16_C(1);
         knockdown_tick <= content->fighter.knockdown_ticks;
         ++knockdown_tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (knockdown_tick < content->fighter.knockdown_ticks)
        {
            if (out_inspection->players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                out_inspection->players[1].action_ticks !=
                    knockdown_tick ||
                out_inspection->players[1].invulnerable !=
                    UINT8_C(0))
            {
                return 0;
            }
        }
    }
    return out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_WAIT &&
           out_inspection->players[1].action_ticks == UINT16_C(0) &&
           out_inspection->players[1].invulnerable == UINT8_C(0);
}

static int run_knockdown_and_tech_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage missed_storage;
    test_sim_storage in_place_storage;
    test_sim_storage roll_storage;
    pf_sim *missed = NULL;
    pf_sim *in_place = NULL;
    pf_sim *roll = NULL;
    pf_m4_inspection missed_inspection;
    pf_m4_inspection in_place_inspection;
    pf_m4_inspection roll_inspection;

    if (!initialize_sim(
            &missed_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &missed) ||
        !initialize_sim(
            &in_place_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &in_place) ||
        !initialize_sim(
            &roll_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &roll) ||
        !run_until_reaction_landing(
            missed,
            0,
            &missed_inspection) ||
        !run_until_reaction_landing(
            in_place,
            1,
            &in_place_inspection) ||
        !run_until_reaction_landing(
            roll,
            2,
            &roll_inspection))
    {
        return fail("tech-landing-setup");
    }

    if (missed_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
        missed_inspection.players[1].grounded != UINT8_C(1) ||
        in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        in_place_inspection.players[1].tech_direction != INT8_C(0) ||
        in_place_inspection.players[1].tech_window_ticks !=
            UINT16_C(0) ||
        in_place_inspection.players[1].tech_lockout_ticks ==
            UINT16_C(0) ||
        in_place_inspection.players[1].invulnerable != UINT8_C(1) ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(1) ||
        roll_inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        roll_inspection.players[1].invulnerable != UINT8_C(1) ||
        missed_inspection.players[1].invulnerable != UINT8_C(0) ||
        roll_inspection.players[1].tumble != UINT8_C(0))
    {
        return fail("missed-tech-in-place-and-roll");
    }
    while (in_place_inspection.players[1].action_ticks <
           content->fighter.tech_invulnerability_ticks)
    {
        if (!step_reaction_duel(
                in_place,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &in_place_inspection))
        {
            return fail("tech-invulnerability-duration-step");
        }
        if (in_place_inspection.players[1].action_ticks <
                content->fighter.tech_invulnerability_ticks &&
            in_place_inspection.players[1].invulnerable !=
                UINT8_C(1))
        {
            return fail("tech-invulnerability-ended-early");
        }
    }
    if (in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        in_place_inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("tech-invulnerability-exact-end");
    }
    return 1;
}

static int16_t tech_chase_axis(
    const pf_m4_inspection *inspection)
{
    const int32_t delta =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;

    if (delta >
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return INT16_MAX;
    }
    if (delta > PF_Q16_ONE / INT32_C(2))
    {
        return INT16_C(13500);
    }
    if (delta <
        -(INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return -INT16_MAX;
    }
    if (delta < -PF_Q16_ONE / INT32_C(2))
    {
        return -INT16_C(13500);
    }
    return INT16_C(0);
}

static int tech_chase_jab_in_range(
    const pf_m4_content *content,
    const pf_m4_inspection *inspection)
{
    const pf_m4_player_inspection *attacker =
        &inspection->players[0];
    const pf_m4_player_inspection *target =
        &inspection->players[1];
    const int64_t delta =
        (int64_t)target->position_x_q16 -
        (int64_t)attacker->position_x_q16;
    const int8_t direction =
        delta > INT64_C(0) ? INT8_C(1) : INT8_C(-1);
    const int64_t distance =
        delta >= INT64_C(0) ? delta : -delta;
    const int64_t reach =
        (int64_t)content->fighter.jab_hitbox_offset_x_q16 +
        (int64_t)content->fighter.jab_hitbox_half_width_q16 +
        (int64_t)content->fighter.half_width_q16;

    return delta != INT64_C(0) &&
           attacker->facing == direction &&
           distance <= reach;
}

static int run_until_tech_chase_landing(
    pf_sim *sim,
    int tech_mode,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    int trigger_sent = 0;

    if (!start_reaction_hit(sim, out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const pf_m4_player_inspection *target =
            &out_inspection->players[1];
        const int should_trigger =
            trigger_sent == 0 &&
            target->action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            target->velocity_y_q16 > INT32_C(0) &&
            target->position_y_q16 +
                    INT32_C(4) * PF_Q16_ONE >=
                INT32_C(32) * PF_Q16_ONE;
        const int16_t target_x =
            tech_mode > 1 &&
                    (should_trigger || trigger_sent != 0)
                ? INT16_MAX
                : INT16_C(0);

        if (!step_reaction_duel(
                sim,
                tech_chase_axis(out_inspection),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                INT16_C(0),
                UINT64_C(0),
                should_trigger
                    ? UINT16_MAX
                    : UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (should_trigger)
        {
            trigger_sent = 1;
        }
        if (out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_ROLL)
        {
            return trigger_sent != 0;
        }
    }
    return 0;
}

static int run_tech_chase_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage in_place_storage;
    test_sim_storage roll_storage;
    test_sim_storage loaded_storage;
    test_sim_storage miss_storage;
    pf_sim *in_place = NULL;
    pf_sim *roll = NULL;
    pf_sim *loaded = NULL;
    pf_sim *miss = NULL;
    pf_m4_inspection in_place_inspection;
    pf_m4_inspection roll_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection miss_inspection;
    pf_state_hash roll_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t initial_damage;
    uint32_t tick;
    uint16_t attack_action_tick = UINT16_MAX;
    int attack_sent = 0;
    int saw_hit = 0;

    if (!initialize_sim(
            &in_place_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &in_place) ||
        !initialize_sim(
            &roll_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &roll) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &miss_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &miss) ||
        !run_until_tech_chase_landing(
            in_place,
            1,
            &in_place_inspection) ||
        !run_until_tech_chase_landing(
            roll,
            2,
            &roll_inspection) ||
        !run_until_tech_chase_landing(
            miss,
            2,
            &miss_inspection))
    {
        return fail("tech-chase-init");
    }
    if (in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        miss_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(1) ||
        in_place_inspection.players[1].damage_q16 !=
            roll_inspection.players[1].damage_q16 ||
        roll_inspection.players[1].damage_q16 !=
            miss_inspection.players[1].damage_q16)
    {
        return fail("tech-chase-outcome-setup");
    }

    initial_damage = in_place_inspection.players[1].damage_q16;
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        int16_t chaser_x = tech_chase_axis(&in_place_inspection);
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            in_place_inspection.players[1].action_ticks >=
                content->fighter.tech_invulnerability_ticks &&
            tech_chase_jab_in_range(
                content,
                &in_place_inspection))
        {
            attack_sent = 1;
            attack_action_tick =
                in_place_inspection.players[1].action_ticks;
            chaser_x = INT16_C(0);
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                in_place,
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &in_place_inspection))
        {
            return fail("tech-chase-in-place-step");
        }
        if (in_place_inspection.players[1].damage_q16 >
            initial_damage)
        {
            saw_hit = 1;
            break;
        }
    }
    if (attack_sent == 0 || saw_hit == 0 ||
        attack_action_tick + content->fighter.jab_startup_ticks >=
            content->fighter.tech_in_place_ticks ||
        in_place_inspection.players[1].damage_q16 !=
            initial_damage + content->fighter.jab_damage_q16 ||
        in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        in_place_inspection.players[1].last_hit_attacker !=
            UINT8_C(0) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT)
    {
        return fail("tech-chase-in-place-punish");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                roll,
                tech_chase_axis(&roll_inspection),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &roll_inspection))
        {
            return fail("tech-chase-roll-presnapshot");
        }
    }
    if (roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].invulnerable != UINT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(roll, &save_size),
            PF_STATUS_OK,
            "tech-chase-query-save-size") ||
        save_size != (size_t)611)
    {
        return fail("tech-chase-roll-snapshot-boundary");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(roll, &destination),
            PF_STATUS_OK,
            "tech-chase-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "tech-chase-load") ||
        !expect_status(
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "tech-chase-loaded-inspect"))
    {
        return 0;
    }

    initial_damage = roll_inspection.players[1].damage_q16;
    attack_action_tick = UINT16_MAX;
    attack_sent = 0;
    saw_hit = 0;
    for (tick = UINT32_C(0); tick < UINT32_C(100); ++tick)
    {
        int16_t chaser_x = tech_chase_axis(&roll_inspection);
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            roll_inspection.players[1].action_ticks >=
                content->fighter.tech_invulnerability_ticks &&
            tech_chase_jab_in_range(content, &roll_inspection))
        {
            attack_sent = 1;
            attack_action_tick =
                roll_inspection.players[1].action_ticks;
            chaser_x = INT16_C(0);
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                roll,
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &roll_inspection) ||
            !step_reaction_duel(
                loaded,
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(roll, &roll_hash),
                PF_STATUS_OK,
                "tech-chase-roll-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "tech-chase-loaded-hash") ||
            !hash_equal(&roll_hash, &loaded_hash))
        {
            return fail("tech-chase-roll-continuation");
        }
        if (roll_inspection.players[1].damage_q16 >
            initial_damage)
        {
            saw_hit = 1;
            break;
        }
    }
    if (attack_sent == 0 || saw_hit == 0 ||
        attack_action_tick + content->fighter.jab_startup_ticks >=
            content->fighter.tech_roll_ticks ||
        roll_inspection.players[1].damage_q16 !=
            initial_damage + content->fighter.jab_damage_q16 ||
        loaded_inspection.players[1].damage_q16 !=
            roll_inspection.players[1].damage_q16 ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return fail("tech-chase-roll-punish");
    }

    initial_damage = miss_inspection.players[1].damage_q16;
    attack_sent = 0;
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            miss_inspection.players[1].action_ticks >=
                content->fighter.tech_invulnerability_ticks)
        {
            if (tech_chase_jab_in_range(content, &miss_inspection))
            {
                return fail("tech-chase-static-negative-spacing");
            }
            attack_sent = 1;
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                miss,
                INT16_C(0),
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &miss_inspection))
        {
            return fail("tech-chase-static-negative-step");
        }
    }
    if (attack_sent == 0 ||
        miss_inspection.players[1].damage_q16 != initial_damage ||
        miss_inspection.players[1].last_hit_sequence != UINT32_C(1))
    {
        return fail("tech-chase-static-negative-result");
    }
    return 1;
}

static int run_floor_getup_option_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage up_storage;
    test_sim_storage shield_storage;
    test_sim_storage roll_storage;
    test_sim_storage auto_storage;
    test_sim_storage attack_storage;
    pf_sim *up = NULL;
    pf_sim *shield = NULL;
    pf_sim *roll = NULL;
    pf_sim *automatic = NULL;
    pf_sim *attack = NULL;
    pf_m4_inspection up_inspection;
    pf_m4_inspection shield_inspection;
    pf_m4_inspection roll_inspection;
    pf_m4_inspection auto_inspection;
    pf_m4_inspection attack_inspection;
    uint16_t tick;

    if (!initialize_sim(
            &up_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &up) ||
        !initialize_sim(
            &shield_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &shield) ||
        !initialize_sim(
            &roll_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &roll) ||
        !initialize_sim(
            &auto_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &automatic) ||
        !initialize_sim(
            &attack_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &attack) ||
        !advance_missed_tech_to_down_wait(
            content,
            up,
            &up_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            shield,
            &shield_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            roll,
            &roll_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            automatic,
            &auto_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            attack,
            &attack_inspection))
    {
        return fail("floor-getup-down-wait-setup");
    }

    if (!step_reaction_duel(
            up,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(-32767),
            UINT64_C(0),
            UINT16_C(0),
            &up_inspection) ||
        up_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        up_inspection.players[1].action_ticks != UINT16_C(0) ||
        up_inspection.players[1].invulnerable != UINT8_C(1) ||
        !step_reaction_duel(
            shield,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &shield_inspection) ||
        shield_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        shield_inspection.players[1].tech_window_ticks !=
            content->fighter.tech_window_ticks ||
        shield_inspection.players[1].tech_lockout_ticks !=
            content->fighter.tech_lockout_ticks ||
        !step_reaction_duel(
            roll,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &roll_inspection) ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(-1) ||
        roll_inspection.players[1].velocity_x_q16 >= INT32_C(0) ||
        roll_inspection.players[1].invulnerable != UINT8_C(1) ||
        !step_reaction_duel(
            attack,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &attack_inspection) ||
        attack_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
        attack_inspection.players[1].action_ticks != UINT16_C(0) ||
        attack_inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("floor-getup-input-routing");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_neutral_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                up,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &up_inspection))
        {
            return fail("neutral-getup-duration-step");
        }
        if (tick < content->fighter.getup_neutral_ticks &&
            (up_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
             up_inspection.players[1].invulnerable !=
                 (tick <
                          content->fighter
                              .getup_neutral_invulnerability_ticks
                      ? UINT8_C(1)
                      : UINT8_C(0))))
        {
            return fail("neutral-getup-duration-or-invulnerability");
        }
    }
    if (up_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        up_inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("neutral-getup-exact-end");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_roll_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                roll,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &roll_inspection))
        {
            return fail("getup-roll-duration-step");
        }
        if (tick < content->fighter.getup_roll_ticks &&
            (roll_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
             roll_inspection.players[1].invulnerable !=
                 (tick <
                          content->fighter
                              .getup_roll_invulnerability_ticks
                      ? UINT8_C(1)
                      : UINT8_C(0))))
        {
            return fail("getup-roll-duration-or-invulnerability");
        }
    }
    if (roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        roll_inspection.players[1].tech_direction != INT8_C(0))
    {
        return fail("getup-roll-exact-end");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.down_wait_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                automatic,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &auto_inspection))
        {
            return fail("automatic-neutral-getup-step");
        }
        if (tick < content->fighter.down_wait_ticks &&
            (auto_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
             auto_inspection.players[1].action_ticks != tick))
        {
            return fail("down-wait-persistence");
        }
    }
    if (auto_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        auto_inspection.players[1].action_ticks != UINT16_C(0) ||
        auto_inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("automatic-neutral-getup");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_attack_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                attack,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &attack_inspection))
        {
            return fail("getup-attack-duration-step");
        }
        if (tick < content->fighter.getup_attack_ticks &&
            (attack_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
             attack_inspection.players[1].invulnerable !=
                 (tick <
                          content->fighter
                              .getup_attack_invulnerability_ticks
                      ? UINT8_C(1)
                      : UINT8_C(0))))
        {
            return fail("getup-attack-duration-or-invulnerability");
        }
    }
    if (attack_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        attack_inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("getup-attack-exact-end");
    }
    return 1;
}

static int run_getup_attack_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage front_storage;
    test_sim_storage back_storage;
    pf_sim *front = NULL;
    pf_sim *back = NULL;
    pf_m4_inspection front_inspection;
    pf_m4_inspection back_inspection;
    uint16_t tick;
    int front_hit = 0;
    int back_hit = 0;

    if (!initialize_sim(
            &front_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &front) ||
        !initialize_sim(
            &back_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &back) ||
        !advance_missed_tech_to_down_wait(
            content,
            front,
            &front_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            back,
            &back_inspection) ||
        !step_reaction_duel(
            front,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &front_inspection))
    {
        return fail("getup-attack-hit-setup");
    }

    for (tick = UINT16_C(1);
         tick <=
             content->fighter.getup_attack_front_active_end_tick;
         ++tick)
    {
        if (!step_reaction_duel(
                front,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &front_inspection))
        {
            return fail("getup-attack-front-step");
        }
        if (front_inspection.players[0].damage_q16 != UINT32_C(0))
        {
            front_hit = 1;
            break;
        }
    }
    if (front_hit == 0 ||
        front_inspection.players[0].damage_q16 !=
            content->fighter.getup_attack_damage_q16 ||
        front_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        front_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        (uint32_t)front_inspection.players[1].action_ticks +
                UINT32_C(1) <
            (uint32_t)content->fighter
                .getup_attack_front_active_begin_tick ||
        (uint32_t)front_inspection.players[1].action_ticks +
                UINT32_C(1) >
            (uint32_t)content->fighter
                .getup_attack_front_active_end_tick)
    {
        return fail("getup-attack-front-hit");
    }

    for (tick = UINT16_C(0); tick < UINT16_C(8); ++tick)
    {
        if (!step_reaction_duel(
                back,
                INT16_C(32767),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &back_inspection))
        {
            return fail("getup-attack-back-position");
        }
    }
    for (tick = UINT16_C(0); tick < UINT16_C(2); ++tick)
    {
        if (!step_reaction_duel(
                back,
                INT16_C(0),
                INT16_C(32767),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &back_inspection))
        {
            return fail("getup-attack-back-stop");
        }
    }
    if (back_inspection.players[0].position_x_q16 <=
            back_inspection.players[1].position_x_q16 +
                content->fighter.half_width_q16 ||
        !step_reaction_duel(
            back,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &back_inspection))
    {
        return fail("getup-attack-back-side-setup");
    }

    for (tick = UINT16_C(1);
         tick <=
             content->fighter.getup_attack_back_active_end_tick;
         ++tick)
    {
        if (!step_reaction_duel(
                back,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &back_inspection))
        {
            return fail("getup-attack-back-step");
        }
        if (back_inspection.players[0].damage_q16 != UINT32_C(0))
        {
            if (tick <
                content->fighter
                    .getup_attack_back_active_begin_tick -
                    UINT16_C(1))
            {
                return fail("getup-attack-back-hit-too-early");
            }
            back_hit = 1;
            break;
        }
    }
    if (back_hit == 0 ||
        back_inspection.players[0].damage_q16 !=
            content->fighter.getup_attack_damage_q16 ||
        back_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        back_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        (uint32_t)back_inspection.players[1].action_ticks +
                UINT32_C(1) <
            (uint32_t)content->fighter
                .getup_attack_back_active_begin_tick ||
        (uint32_t)back_inspection.players[1].action_ticks +
                UINT32_C(1) >
            (uint32_t)content->fighter
                .getup_attack_back_active_end_tick)
    {
        return fail("getup-attack-back-hit");
    }
    return 1;
}

static int run_floor_recovery_snapshot_test(
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
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint16_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !advance_missed_tech_to_down_wait(
            content,
            source,
            &source_inspection) ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        source_inspection.players[1].tech_direction != INT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-floor-recovery-save-size") ||
        save_size != (size_t)611)
    {
        return fail("floor-recovery-snapshot-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-floor-recovery") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-floor-recovery") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-floor-recovery") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-floor-recovery") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("floor-recovery-snapshot-round-trip");
    }

    for (tick = UINT16_C(0);
         tick <
             content->fighter.getup_roll_ticks + UINT16_C(5);
         ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-floor-recovery-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-floor-recovery-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("floor-recovery-deterministic-continuation");
        }
    }
    return source_inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                   loaded_inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE
               ? 1
               : fail("floor-recovery-snapshot-exact-end");
}

static int run_tech_invulnerability_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    const uint32_t initial_damage =
        content->fighter.jab_damage_q16;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !run_until_reaction_landing(sim, 1, &inspection) ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].damage_q16 != initial_damage ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug tech-invulnerability "
            "p0_action=%u p0_hitbox=%u p0_x=%" PRId32
            " p1_action=%u p1_ticks=%u p1_invulnerable=%u "
            "p1_damage=%" PRIu32 " p1_x=%" PRId32
            " p1_y=%" PRId32 " p1_vy=%" PRId32
            " tech_window=%u tech_lockout=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].hitbox_active,
            inspection.players[0].position_x_q16,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16,
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            inspection.players[1].velocity_y_q16,
            (unsigned int)inspection.players[1].tech_window_ticks,
            (unsigned int)inspection.players[1].tech_lockout_ticks);
        return fail("tech-invulnerability-rejects-hit");
    }

    while (inspection.players[1].action_ticks <
               content->fighter.tech_invulnerability_ticks ||
           inspection.players[0].action_state !=
               (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("tech-vulnerability-window-setup");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[1].damage_q16 !=
            initial_damage + content->fighter.jab_damage_q16 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return fail("tech-vulnerability-restores-hit");
    }
    return 1;
}

static int run_air_dodge_invulnerability_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
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
            1,
            &sim))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                &inspection))
        {
            return fail("air-dodge-invulnerability-jump");
        }
        if (inspection.players[1].grounded == UINT8_C(0))
        {
            break;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[1].action_ticks !=
            (uint16_t)(
                content->fighter
                    .air_dodge_invulnerability_begin_tick +
                UINT16_C(1)) ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug air-dodge-invulnerability"
            " p0_action=%u p0_ticks=%u p0_hitbox=%u"
            " p0_y=%" PRId32 " p1_action=%u p1_ticks=%u"
            " p1_invulnerable=%u p1_damage=%" PRIu32
            " p1_y=%" PRId32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active,
            inspection.players[0].position_y_q16,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16,
            inspection.players[1].position_y_q16);
        return fail("air-dodge-invulnerability-rejects-hit");
    }

    while (inspection.players[1].action_ticks <
               (uint16_t)(
                   content->fighter
                       .air_dodge_invulnerability_end_tick -
                   UINT16_C(3)) ||
           inspection.players[0].action_state !=
               (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return fail("air-dodge-vulnerability-window-setup");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[1].action_ticks !=
            (uint16_t)(
                content->fighter
                    .air_dodge_invulnerability_end_tick -
                UINT16_C(3)) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug air-dodge-expired"
            " p0_action=%u p0_ticks=%u p0_hitbox=%u"
            " p1_action=%u p1_ticks=%u p1_invulnerable=%u"
            " p1_damage=%" PRIu32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16);
        return fail("air-dodge-expired-window-accepts-hit");
    }
    return 1;
}

static int run_ground_dodge_invulnerability_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        inspection.players[1].action_ticks != UINT16_C(1) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16)
    {
        return fail("spot-dodge-startup-accepts-hit");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5d07d0d6e)),
            PF_STATUS_OK,
            "spot-dodge-invulnerable-reset") ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        inspection.players[1].action_ticks !=
            content->fighter.spot_dodge_invulnerability_begin_tick ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("spot-dodge-invulnerability-rejects-hit");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5d07d0d6f)),
            PF_STATUS_OK,
            "spot-dodge-expired-reset") ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    while (inspection.players[1].action_ticks <
           (uint16_t)(
               content->fighter
                   .spot_dodge_invulnerability_end_tick -
               UINT16_C(3)))
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("spot-dodge-expired-window-setup");
        }
    }
    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug spot-dodge-expired"
            " action=%u ticks=%u invulnerable=%u damage=%" PRIu32 "\n",
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16);
        return fail("spot-dodge-expired-window-accepts-hit");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5d07d0d70)),
            PF_STATUS_OK,
            "roll-invulnerable-reset") ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.players[1].action_ticks !=
            content->fighter.roll_invulnerability_begin_tick ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("roll-invulnerability-rejects-hit");
    }
    return 1;
}

static int run_hitlag_snapshot_test(const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !step_duel(
            source,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-combat-save-size") ||
        save_size != (size_t)611)
    {
        return fail("mid-hitlag-save-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-mid-hitlag") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-mid-hitlag") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-mid-hitlag") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-mid-hitlag") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("mid-hitlag-round-trip");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        const uint64_t player0_buttons =
            tick == UINT32_C(20) ? PF_INPUT_BUTTON_ATTACK : UINT64_C(0);
        const uint64_t player1_buttons =
            tick == UINT32_C(38) ? PF_INPUT_BUTTON_ATTACK : UINT64_C(0);

        if (!step_duel(
                source,
                INT16_C(0),
                player0_buttons,
                INT16_C(0),
                player1_buttons,
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                player0_buttons,
                INT16_C(0),
                player1_buttons,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("mid-hitlag-deterministic-continuation");
        }
    }
    return 1;
}

static int run_shield_hitlag_snapshot_test(
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
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !start_normal_shield_block(source, &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        source_inspection.players[1].shield_stun_ticks ==
            UINT16_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-shield-save-size") ||
        save_size != (size_t)611)
    {
        return fail("mid-shield-hitlag-save-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-mid-shield-hitlag") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-mid-shield-hitlag") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-mid-shield-hitlag") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-mid-shield-hitlag") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("mid-shield-hitlag-round-trip");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-shield-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-shield-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail(
                "mid-shield-hitlag-deterministic-continuation");
        }
    }
    return 1;
}

static void make_trace_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick)
{
    uint32_t player_index;

    make_inputs(inputs, UINT8_C(4), tick);
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const uint64_t phase =
            (tick / UINT64_C(29) + (uint64_t)player_index) %
            UINT64_C(4);
        const uint64_t attack_period =
            UINT64_C(23) + UINT64_C(2) * (uint64_t)player_index;
        const uint64_t jump_period =
            UINT64_C(71) + UINT64_C(3) * (uint64_t)player_index;

        if (phase == UINT64_C(0))
        {
            inputs[player_index].main_stick_x = INT16_C(32767);
        }
        else if (phase == UINT64_C(2))
        {
            inputs[player_index].main_stick_x = INT16_C(-32767);
        }
        if (tick % attack_period == (uint64_t)player_index)
        {
            inputs[player_index].buttons |= PF_INPUT_BUTTON_ATTACK;
        }
        if (tick % jump_period ==
            UINT64_C(5) + (uint64_t)player_index)
        {
            inputs[player_index].buttons |= PF_INPUT_BUTTON_JUMP;
        }
        if (tick % UINT64_C(97) ==
            UINT64_C(11) + (uint64_t)player_index)
        {
            inputs[player_index].main_stick_y = INT16_C(32767);
        }
        if ((tick + UINT64_C(5) * (uint64_t)player_index) %
                UINT64_C(127) <
            UINT64_C(12))
        {
            inputs[player_index].left_trigger = UINT16_MAX;
        }
    }
}

static int run_deterministic_trace(const pf_content_view *view)
{
    test_sim_storage left_storage;
    test_sim_storage right_storage;
    pf_sim *left = NULL;
    pf_sim *right = NULL;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result left_result;
    pf_tick_result right_result;
    pf_state_hash left_hash;
    pf_state_hash right_hash;
    pf_m4_inspection inspection;
    uint64_t tick;
    int saw_hit = 0;
    int saw_shield = 0;

    if (!initialize_sim(
            &left_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &left) ||
        !initialize_sim(
            &right_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &right))
    {
        return fail("trace-init");
    }

    for (tick = UINT64_C(0);
         tick < TEST_DETERMINISTIC_TICKS;
         ++tick)
    {
        make_trace_inputs(inputs, tick);
        if (!expect_status(
                pf_sim_tick(
                    left,
                    inputs,
                    (size_t)PF_SIM_MAX_PLAYERS,
                    &left_result),
                PF_STATUS_OK,
                "trace-left-tick") ||
            !expect_status(
                pf_sim_tick(
                    right,
                    inputs,
                    (size_t)PF_SIM_MAX_PLAYERS,
                    &right_result),
                PF_STATUS_OK,
                "trace-right-tick") ||
            !expect_status(
                pf_sim_hash(left, &left_hash),
                PF_STATUS_OK,
                "trace-left-hash") ||
            !expect_status(
                pf_sim_hash(right, &right_hash),
                PF_STATUS_OK,
                "trace-right-hash") ||
            !hash_equal(&left_hash, &right_hash))
        {
            return fail("deterministic-combat-trace");
        }

        if ((tick % UINT64_C(113)) == UINT64_C(0) &&
            expect_status(
                pf_m4_inspect(left, &inspection),
                PF_STATUS_OK,
                "trace-inspect"))
        {
            uint32_t player_index;

            for (player_index = UINT32_C(0);
                 player_index < PF_SIM_MAX_PLAYERS;
                 ++player_index)
            {
                if (inspection.players[player_index].last_hit_valid !=
                    UINT8_C(0))
                {
                    saw_hit = 1;
                }
                if (inspection.players[player_index].action_state ==
                        (uint8_t)PF_M4_ACTION_SHIELD ||
                    inspection.players[player_index]
                            .shield_health_q16 <
                        UINT32_C(60) * UINT32_C(65536))
                {
                    saw_shield = 1;
                }
            }
        }
    }
    return (saw_hit != 0 && saw_shield != 0) ||
           fail("trace-did-not-exercise-combat-and-shield");
}

int main(void)
{
    pf_m4_content content;
    pf_m4_content invalid_content;
    pf_m4_content invalid_strong_content;
    pf_m4_content invalid_tech_content;
    pf_m4_content invalid_getup_content;
    pf_m4_content invalid_shield_content;
    pf_m4_content invalid_shield_break_content;
    pf_m4_content invalid_cancel_content;
    pf_m4_content invalid_surface_content;
    pf_m4_content reaction_content;
    pf_m4_content tech_invulnerability_content;
    pf_m4_content floor_attack_content;
    pf_m4_content shield_break_content;
    pf_m4_content wall_tech_content;
    pf_m4_content ceiling_tech_content;
    pf_content_view view;
    pf_content_view reaction_view;
    pf_content_view tech_invulnerability_view;
    pf_content_view floor_attack_view;
    pf_content_view shield_break_view;
    pf_content_view wall_tech_view;
    pf_content_view ceiling_tech_view;

    if (!make_combat_content(&content, &view) ||
        !make_reaction_content(
            &reaction_content,
            &reaction_view) ||
        !make_tech_invulnerability_content(
            &tech_invulnerability_content,
            &tech_invulnerability_view) ||
        !make_floor_attack_content(
            &floor_attack_content,
            &floor_attack_view) ||
        !make_shield_break_content(
            &shield_break_content,
            &shield_break_view) ||
        !make_surface_tech_content(
            0,
            &wall_tech_content,
            &wall_tech_view) ||
        !make_surface_tech_content(
            1,
            &ceiling_tech_content,
            &ceiling_tech_view))
    {
        return 1;
    }
    invalid_content = content;
    invalid_content.fighter.jab_knockback_growth_q16 =
        INT32_C(4) * PF_Q16_ONE;
    invalid_strong_content = content;
    invalid_strong_content.fighter.strong_knockback_growth_q16 =
        INT32_C(4) * PF_Q16_ONE;
    invalid_tech_content = content;
    invalid_tech_content.fighter.tech_invulnerability_ticks =
        (uint16_t)(
            invalid_tech_content.fighter.tech_in_place_ticks +
            UINT16_C(1));
    invalid_getup_content = content;
    invalid_getup_content.fighter
        .getup_attack_front_active_end_tick =
        invalid_getup_content.fighter
            .getup_attack_back_active_begin_tick;
    invalid_shield_content = content;
    invalid_shield_content.fighter.shield_release_ticks =
        UINT16_C(0);
    invalid_shield_break_content = content;
    invalid_shield_break_content.fighter
        .shield_break_launch_speed_q16 =
        invalid_shield_break_content.fighter.gravity_q16;
    invalid_cancel_content = content;
    invalid_cancel_content.fighter.powershield_cancel_delay_ticks =
        invalid_cancel_content.fighter.shield_release_ticks;
    invalid_surface_content = content;
    invalid_surface_content.stage.solid_bottom_q16 =
        invalid_surface_content.stage.solid_top_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-knockback") ||
        !expect_status(
            pf_m4_validate_content(&invalid_strong_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-strong-knockback") ||
        !expect_status(
            pf_m4_validate_content(&invalid_tech_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-tech-invulnerability") ||
        !expect_status(
            pf_m4_validate_content(&invalid_getup_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overlapping-getup-attack-windows") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_break_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-break-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_cancel_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-powershield-cancel-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_surface_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-solid-geometry") ||
        !run_one_way_hit_test(&content, &view) ||
        !run_aerial_hit_test(&content, &view) ||
        !run_strong_aerial_hit_test(&content, &view) ||
        !run_default_strong_tumble_test(&content, &view) ||
        !run_surface_tech_test(
            &wall_tech_content,
            &wall_tech_view,
            &ceiling_tech_content,
            &ceiling_tech_view) ||
        !run_whiff_and_trade_test(&content, &view) ||
        !run_shield_state_test(&content, &view) ||
        !run_dashing_shield_test(&content, &view) ||
        !run_shield_block_test(&content, &view) ||
        !run_powershield_cancel_test(&content, &view) ||
        !run_powershield_cancel_replay_test(&view) ||
        !run_aerial_l_cancel_replay_test() ||
        !run_shield_break_test(
            &shield_break_content,
            &shield_break_view) ||
        !run_di_and_sdi_test(
            &reaction_content,
            &reaction_view) ||
        !run_knockdown_and_tech_test(
            &reaction_content,
            &reaction_view) ||
        !run_tech_chase_test(
            &tech_invulnerability_content,
            &tech_invulnerability_view) ||
        !run_floor_getup_option_test(
            &reaction_content,
            &reaction_view) ||
        !run_getup_attack_hit_test(
            &floor_attack_content,
            &floor_attack_view) ||
        !run_floor_recovery_snapshot_test(
            &reaction_content,
            &reaction_view) ||
        !run_tech_invulnerability_hit_test(
            &tech_invulnerability_content,
            &tech_invulnerability_view) ||
        !run_air_dodge_invulnerability_hit_test(
            &content,
            &view) ||
        !run_ground_dodge_invulnerability_hit_test(
            &content,
            &view) ||
        !run_hitlag_snapshot_test(&view) ||
        !run_shield_hitlag_snapshot_test(&view) ||
        !run_deterministic_trace(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-combat=pass content_schema=%u deterministic_ticks=%" PRIu64
        " combat_invariants=149 journal_invariants=30\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        TEST_DETERMINISTIC_TICKS);
    return 0;
}
