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
#define TEST_SAVE_CAPACITY 512U
#define TEST_DETERMINISTIC_TICKS UINT64_C(20000)

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

static int step_players(
    pf_sim *sim,
    uint8_t player_count,
    const int16_t axes_x[PF_SIM_MAX_PLAYERS],
    const int16_t axes_y[PF_SIM_MAX_PLAYERS],
    const uint64_t buttons[PF_SIM_MAX_PLAYERS],
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
                INT16_C(32767),
                PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
                INT16_C(-32767),
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
        save_size != (size_t)501)
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
            }
        }
    }
    return saw_hit != 0 || fail("trace-did-not-exercise-combat");
}

int main(void)
{
    pf_m4_content content;
    pf_m4_content invalid_content;
    pf_content_view view;

    if (!make_combat_content(&content, &view))
    {
        return 1;
    }
    invalid_content = content;
    invalid_content.fighter.jab_knockback_growth_q16 =
        INT32_C(4) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-knockback") ||
        !run_one_way_hit_test(&content, &view) ||
        !run_whiff_and_trade_test(&content, &view) ||
        !run_hitlag_snapshot_test(&view) ||
        !run_deterministic_trace(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-combat=pass content_schema=%u deterministic_ticks=%" PRIu64
        " combat_invariants=14\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        TEST_DETERMINISTIC_TICKS);
    return 0;
}
