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

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

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
    pf_tick_result result;
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
                   &result),
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
        inspection.players[1].last_hit_tick + UINT64_C(1) !=
            inspection.tick ||
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
    test_sim_storage normal_storage;
    pf_sim *early = NULL;
    pf_sim *cancel = NULL;
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
        normal_inspection.players[1].velocity_x_q16 <= INT32_C(0))
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
            normal_pushback)
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
    out_content->fighter.shield_break_ticks = UINT16_C(20);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "shield-break-content-view");
}

static int run_shield_break_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t break_elapsed = UINT32_C(0);
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !start_normal_shield_block(sim, &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].shield_health_q16 != UINT32_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
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
                &inspection))
        {
            return fail("shield-break-hitlag");
        }
    }
    if (inspection.players[1].action_state !=
        (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        return fail("shield-break-state");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
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
                &inspection))
        {
            return fail("shield-break-lockout");
        }
        ++break_elapsed;
        if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            return fail("shield-break-ended-before-rehit");
        }
    }
    if (inspection.players[0].action_state !=
        (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("shield-break-rehit-attacker-ready");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return fail("shield-break-rehit-step");
        }
        ++break_elapsed;
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[1].shield_health_q16 != UINT32_C(0))
    {
        return fail("shield-break-placeholder-rehit-lockout");
    }
    while (break_elapsed <
           (uint32_t)content->fighter.shield_break_ticks)
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
            return fail("shield-break-lockout-completion");
        }
        ++break_elapsed;
        if (break_elapsed <
                (uint32_t)content->fighter.shield_break_ticks &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            return fail("shield-break-lockout-duration");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[1].shield_health_q16 !=
            content->fighter.shield_reset_health_q16)
    {
        return fail("shield-break-reset-health");
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
                    PF_Q16_ONE >=
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

static int run_knockdown_and_tech_test(
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
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(1) ||
        roll_inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        roll_inspection.players[1].tumble != UINT8_C(0))
    {
        return fail("missed-tech-in-place-and-roll");
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
        save_size != (size_t)569)
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
        save_size != (size_t)569)
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
    pf_m4_content invalid_shield_content;
    pf_m4_content invalid_cancel_content;
    pf_m4_content reaction_content;
    pf_m4_content shield_break_content;
    pf_content_view view;
    pf_content_view reaction_view;
    pf_content_view shield_break_view;

    if (!make_combat_content(&content, &view) ||
        !make_reaction_content(
            &reaction_content,
            &reaction_view) ||
        !make_shield_break_content(
            &shield_break_content,
            &shield_break_view))
    {
        return 1;
    }
    invalid_content = content;
    invalid_content.fighter.jab_knockback_growth_q16 =
        INT32_C(4) * PF_Q16_ONE;
    invalid_shield_content = content;
    invalid_shield_content.fighter.shield_release_ticks =
        UINT16_C(0);
    invalid_cancel_content = content;
    invalid_cancel_content.fighter.powershield_cancel_delay_ticks =
        invalid_cancel_content.fighter.shield_release_ticks;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-knockback") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_cancel_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-powershield-cancel-data") ||
        !run_one_way_hit_test(&content, &view) ||
        !run_whiff_and_trade_test(&content, &view) ||
        !run_shield_state_test(&content, &view) ||
        !run_shield_block_test(&content, &view) ||
        !run_powershield_cancel_test(&content, &view) ||
        !run_powershield_cancel_replay_test(&view) ||
        !run_shield_break_test(
            &shield_break_content,
            &shield_break_view) ||
        !run_di_and_sdi_test(
            &reaction_content,
            &reaction_view) ||
        !run_knockdown_and_tech_test(&reaction_view) ||
        !run_hitlag_snapshot_test(&view) ||
        !run_shield_hitlag_snapshot_test(&view) ||
        !run_deterministic_trace(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-combat=pass content_schema=%u deterministic_ticks=%" PRIu64
        " combat_invariants=51\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        TEST_DETERMINISTIC_TICKS);
    return 0;
}
