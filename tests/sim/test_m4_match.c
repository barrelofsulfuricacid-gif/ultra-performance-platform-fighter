#include "pf/m4.h"
#include "pf/sim.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_SAVE_CAPACITY 1024U
#define TEST_STEP_LIMIT UINT32_C(600)

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static int fail(const char *operation)
{
    (void)fprintf(
        stderr,
        "m4-match=fail operation=%s\n",
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
            "m4-match=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static int make_match_content(
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

    out_content->stage.floor_left_q16 =
        -INT32_C(8) * PF_Q16_ONE;
    out_content->stage.floor_right_q16 =
        INT32_C(8) * PF_Q16_ONE;
    out_content->stage.platform_center_x_q16 =
        INT32_C(5) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 =
        INT32_C(1) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 =
        -PF_Q16_ONE / INT32_C(10);
    out_content->stage.solid_right_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->stage.blast_left_q16 =
        -INT32_C(10) * PF_Q16_ONE;
    out_content->stage.blast_right_q16 =
        INT32_C(10) * PF_Q16_ONE;
    out_content->stage.blast_bottom_q16 =
        INT32_C(34) * PF_Q16_ONE;
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);

    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "match-content-view");
}

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    uint8_t stocks,
    uint16_t respawn_delay,
    uint16_t invulnerability,
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
    config.max_ticks = UINT64_C(5000);
    config.stock_count = stocks;
    config.respawn_delay_ticks = respawn_delay;
    config.respawn_invulnerability_ticks = invulnerability;
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
               pf_sim_reset(*out_sim, UINT64_C(0x4d344d41544348)),
               PF_STATUS_OK,
               "reset");
}

static int inspect(
    const pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return expect_status(
        pf_m4_inspect(sim, out_inspection),
        PF_STATUS_OK,
        "inspect");
}

static int step_players(
    pf_sim *sim,
    uint8_t player_count,
    const int16_t axes[PF_SIM_MAX_PLAYERS],
    const uint64_t buttons[PF_SIM_MAX_PLAYERS],
    pf_tick_result *out_result,
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_m4_inspection before;
    uint32_t player_index;

    if (!inspect(sim, &before))
    {
        return 0;
    }
    (void)memset(inputs, 0, sizeof(inputs));
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)player_count;
         ++player_index)
    {
        inputs[player_index].tick = before.tick;
        inputs[player_index].buttons = buttons[player_index];
        inputs[player_index].main_stick_x = axes[player_index];
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }

    if (!expect_status(
            pf_sim_tick(
                sim,
                inputs,
                (size_t)player_count,
                out_result),
            PF_STATUS_OK,
            "tick"))
    {
        return 0;
    }
    return inspect(sim, out_inspection);
}

static int step_duel(
    pf_sim *sim,
    int16_t player0_axis,
    uint64_t player0_buttons,
    int16_t player1_axis,
    uint64_t player1_buttons,
    pf_tick_result *out_result,
    pf_m4_inspection *out_inspection)
{
    const int16_t axes[PF_SIM_MAX_PLAYERS] = {
        player0_axis,
        player1_axis,
        INT16_C(0),
        INT16_C(0)};
    const uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        player0_buttons,
        player1_buttons,
        UINT64_C(0),
        UINT64_C(0)};

    return step_players(
        sim,
        UINT8_C(2),
        axes,
        buttons,
        out_result,
        out_inspection);
}

static int hash_equal(
    const pf_state_hash *left,
    const pf_state_hash *right)
{
    return left->algorithm == right->algorithm &&
           left->algorithm_version == right->algorithm_version &&
           left->digest_size == right->digest_size &&
           memcmp(
               left->bytes,
               right->bytes,
               PF_SIM_STATE_HASH_BYTES) == 0;
}

static int events_equal(
    const pf_tick_result *left,
    const pf_tick_result *right)
{
    return left->event_count == right->event_count &&
           memcmp(
               left->events,
               right->events,
               sizeof(left->events[0]) *
                   (size_t)left->event_count) == 0;
}

static int run_player0_to_ko(
    pf_sim *sim,
    pf_m4_inspection *out_inspection,
    pf_tick_result *out_result)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < TEST_STEP_LIMIT; ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_result,
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].active == UINT8_C(0))
        {
            return 1;
        }
    }
    return fail("player0-ko-timeout");
}

static int run_stock_respawn_match_test(
    const pf_content_view *content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_observation observation;
    pf_sim_identity identity;
    pf_tick_result result;
    pf_tick_result loaded_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            UINT8_C(2),
            UINT16_C(3),
            UINT16_C(5),
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            UINT8_C(2),
            UINT16_C(3),
            UINT16_C(5),
            0,
            &loaded) ||
        !inspect(source, &inspection))
    {
        return 0;
    }
    if (inspection.stock_count != UINT8_C(2) ||
        inspection.respawn_delay_ticks != UINT16_C(3) ||
        inspection.respawn_invulnerability_ticks != UINT16_C(5) ||
        inspection.players[0].stocks_remaining != UINT8_C(2) ||
        inspection.players[1].stocks_remaining != UINT8_C(2))
    {
        return fail("initial-stock-contract");
    }
    if (!expect_status(
            pf_sim_query_identity(source, &identity),
            PF_STATUS_OK,
            "query-identity") ||
        identity.schema_version != PF_SIM_IDENTITY_SCHEMA_VERSION ||
        identity.stock_count != UINT8_C(2) ||
        identity.respawn_delay_ticks != UINT16_C(3) ||
        identity.respawn_invulnerability_ticks != UINT16_C(5))
    {
        return fail("identity-stock-contract");
    }

    if (!run_player0_to_ko(source, &inspection, &result) ||
        inspection.players[0].stocks_remaining != UINT8_C(1) ||
        inspection.players[0].respawn_count != UINT16_C(1) ||
        inspection.players[0].respawn_ticks != UINT16_C(3) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RESPAWN_WAIT ||
        result.terminated != UINT8_C(0) ||
        result.event_count != UINT8_C(1) ||
        result.events[0].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[0].target_player != UINT8_C(0) ||
        result.events[0].detail != UINT16_C(1) ||
        result.events[0].flags != UINT16_C(0))
    {
        return fail("first-stock-loss");
    }
    if (!expect_status(
            pf_sim_observe(source, &observation),
            PF_STATUS_OK,
            "observe-respawn") ||
        observation.schema_version !=
            PF_SIM_OBSERVATION_SCHEMA_VERSION ||
        observation.stock_count != UINT8_C(2) ||
        observation.players[0].stocks_remaining != UINT8_C(1) ||
        observation.players[0].respawn_ticks != UINT16_C(3))
    {
        return fail("respawn-observation");
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-save-size") ||
        save_size != (size_t)682)
    {
        return fail("respawn-save-size");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-respawn") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-respawn") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-respawn") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-respawn") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("respawn-save-round-trip");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &result,
                &inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_result,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-respawn-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-respawn-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("respawn-deterministic-continuation");
        }
    }
    if (inspection.players[0].active != UINT8_C(1) ||
        inspection.players[0].respawn_ticks != UINT16_C(0) ||
        inspection.players[0].respawn_invulnerability_ticks !=
            UINT16_C(5) ||
        inspection.players[0].damage_q16 != UINT32_C(0) ||
        result.event_count != UINT8_C(1) ||
        result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_RESPAWN ||
        result.events[0].target_player != UINT8_C(0) ||
        result.events[0].detail != UINT16_C(5) ||
        !events_equal(&result, &loaded_result))
    {
        return fail("respawn-delay-boundary");
    }

    if (!step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &result,
            &inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &result,
            &inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &result,
            &inspection) ||
        inspection.players[0].damage_q16 != UINT32_C(0) ||
        inspection.players[0].respawn_invulnerability_ticks !=
            UINT16_C(2))
    {
        return fail("respawn-invulnerability-rejects-hit");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &result,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].respawn_invulnerability_ticks ==
                UINT16_C(0) &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(40))
    {
        return fail("post-respawn-neutral-timeout");
    }
    if (!step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &result,
            &inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &result,
            &inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &result,
            &inspection) ||
        inspection.players[0].damage_q16 == UINT32_C(0))
    {
        return fail("expired-respawn-invulnerability-accepts-hit");
    }

    if (!run_player0_to_ko(source, &inspection, &result) ||
        inspection.players[0].stocks_remaining != UINT8_C(0) ||
        inspection.players[0].active != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ELIMINATED ||
        inspection.terminated != UINT8_C(1) ||
        inspection.winner_mask != UINT8_C(2) ||
        result.terminated != UINT8_C(1) ||
        result.winner_mask != UINT8_C(2) ||
        result.event_count != UINT8_C(2) ||
        result.events[0].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[0].target_player != UINT8_C(0) ||
        (result.events[0].flags &
         ((uint16_t)PF_SIM_EVENT_FLAG_ELIMINATED |
          (uint16_t)PF_SIM_EVENT_FLAG_LAST_STOCK)) !=
            ((uint16_t)PF_SIM_EVENT_FLAG_ELIMINATED |
             (uint16_t)PF_SIM_EVENT_FLAG_LAST_STOCK) ||
        result.events[0].detail != UINT16_C(0) ||
        result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_MATCH_RESULT ||
        result.events[1].detail != UINT16_C(2) ||
        result.events[1].sequence !=
            result.events[0].sequence + UINT32_C(1))
    {
        return fail("final-stock-match-result");
    }
    return 1;
}

static int run_simultaneous_ko_sudden_death_test(
    const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_tick_result result;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            UINT8_C(1),
            UINT16_C(3),
            UINT16_C(5),
            1,
            &sim))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < TEST_STEP_LIMIT; ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                UINT64_C(0),
                INT16_MAX,
                UINT64_C(0),
                &result,
                &inspection))
        {
            return 0;
        }
        if (inspection.sudden_death != UINT8_C(0))
        {
            break;
        }
    }
    if (tick == TEST_STEP_LIMIT ||
        inspection.terminated != UINT8_C(0) ||
        inspection.players[0].stocks_remaining != UINT8_C(1) ||
        inspection.players[1].stocks_remaining != UINT8_C(1) ||
        inspection.players[0].damage_q16 !=
            UINT32_C(300) * (uint32_t)PF_Q16_ONE ||
        inspection.players[1].damage_q16 !=
            UINT32_C(300) * (uint32_t)PF_Q16_ONE ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RESPAWN_WAIT ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_RESPAWN_WAIT ||
        result.event_count != UINT8_C(3) ||
        result.events[0].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[0].target_player != UINT8_C(0) ||
        result.events[1].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[1].target_player != UINT8_C(1) ||
        result.events[2].type !=
            (uint16_t)PF_SIM_EVENT_SUDDEN_DEATH ||
        result.events[2].value_q16 !=
            UINT32_C(300) * (uint32_t)PF_Q16_ONE)
    {
        return fail("simultaneous-final-stock-enters-sudden-death");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &result,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].active != UINT8_C(1) ||
        inspection.players[1].active != UINT8_C(1) ||
        inspection.players[0].damage_q16 !=
            UINT32_C(300) * (uint32_t)PF_Q16_ONE ||
        inspection.players[1].damage_q16 !=
            UINT32_C(300) * (uint32_t)PF_Q16_ONE ||
        result.event_count != UINT8_C(2) ||
        result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_RESPAWN ||
        result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_RESPAWN ||
        (result.events[0].flags &
         (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH) == UINT16_C(0) ||
        result.events[0].value_q16 !=
            UINT32_C(300) * (uint32_t)PF_Q16_ONE)
    {
        return fail("sudden-death-spawn-retains-300-percent");
    }

    for (tick = UINT32_C(0); tick < TEST_STEP_LIMIT; ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                UINT64_C(0),
                INT16_MAX,
                UINT64_C(0),
                &result,
                &inspection))
        {
            return 0;
        }
        if (inspection.terminated != UINT8_C(0))
        {
            break;
        }
    }
    if (tick == TEST_STEP_LIMIT ||
        inspection.winner_mask != UINT8_C(1) ||
        result.winner_mask != UINT8_C(1) ||
        inspection.players[0].stocks_remaining != UINT8_C(0) ||
        inspection.players[1].stocks_remaining != UINT8_C(0) ||
        result.event_count != UINT8_C(3) ||
        result.events[0].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[1].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[2].type !=
            (uint16_t)PF_SIM_EVENT_MATCH_RESULT ||
        result.events[2].detail != UINT16_C(1) ||
        (result.events[2].flags &
         (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH) == UINT16_C(0))
    {
        return fail("sudden-death-lowest-port-resolution");
    }
    return 1;
}

static int run_team_result_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_tick_result result;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            content,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            UINT8_C(1),
            UINT16_C(3),
            UINT16_C(5),
            1,
            &sim))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < TEST_STEP_LIMIT; ++tick)
    {
        const int16_t axes[PF_SIM_MAX_PLAYERS] = {
            INT16_MIN,
            INT16_C(0),
            INT16_MAX,
            INT16_C(0)};
        const uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
            UINT64_C(0),
            UINT64_C(0),
            UINT64_C(0),
            UINT64_C(0)};

        if (!step_players(
                sim,
                UINT8_C(4),
                axes,
                buttons,
                &result,
                &inspection))
        {
            return 0;
        }
        if (inspection.terminated != UINT8_C(0))
        {
            break;
        }
    }
    if (tick == TEST_STEP_LIMIT ||
        inspection.winner_mask != UINT8_C(10) ||
        result.winner_mask != UINT8_C(10) ||
        inspection.players[0].stocks_remaining != UINT8_C(0) ||
        inspection.players[2].stocks_remaining != UINT8_C(0) ||
        inspection.players[1].stocks_remaining != UINT8_C(1) ||
        inspection.players[3].stocks_remaining != UINT8_C(1) ||
        result.event_count != UINT8_C(2) ||
        result.events[0].type != (uint16_t)PF_SIM_EVENT_KO ||
        result.events[0].target_player != UINT8_C(2) ||
        result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_MATCH_RESULT ||
        result.events[1].detail != UINT16_C(10))
    {
        return fail("team-stock-result");
    }
    return 1;
}

static int run_invalid_config_test(void)
{
    pf_sim_config config;
    pf_memory_requirements requirements;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "default-invalid-config-base") ||
        config.stock_count != PF_SIM_DEFAULT_STOCK_COUNT ||
        config.respawn_delay_ticks !=
            PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS ||
        config.respawn_invulnerability_ticks !=
            PF_SIM_DEFAULT_RESPAWN_INVULNERABILITY_TICKS)
    {
        return fail("default-stock-config");
    }

    config.stock_count =
        (uint8_t)(PF_SIM_MAX_STOCK_COUNT + UINT8_C(1));
    if (!expect_status(
            pf_sim_query_memory(&config, &requirements),
            PF_STATUS_INVALID_CONFIG,
            "reject-too-many-stocks"))
    {
        return 0;
    }
    config.stock_count = PF_SIM_DEFAULT_STOCK_COUNT;
    config.respawn_delay_ticks =
        (uint16_t)(PF_SIM_MAX_RESPAWN_TICKS + UINT16_C(1));
    if (!expect_status(
            pf_sim_query_memory(&config, &requirements),
            PF_STATUS_INVALID_CONFIG,
            "reject-respawn-delay"))
    {
        return 0;
    }
    config.respawn_delay_ticks =
        PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS;
    config.respawn_invulnerability_ticks =
        (uint16_t)(PF_SIM_MAX_RESPAWN_TICKS + UINT16_C(1));
    if (!expect_status(
            pf_sim_query_memory(&config, &requirements),
            PF_STATUS_INVALID_CONFIG,
            "reject-respawn-invulnerability"))
    {
        return 0;
    }
    return 1;
}

int main(void)
{
    pf_m4_content content;
    pf_content_view view;

    if (!make_match_content(&content, &view) ||
        !run_invalid_config_test() ||
        !run_stock_respawn_match_test(&view) ||
        !run_simultaneous_ko_sudden_death_test(&view) ||
        !run_team_result_test(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-match=pass stocks=4 respawn_delay=60 "
        "respawn_invulnerability=120 sudden_death=1 "
        "team_result=1 invariants=24 journal_invariants=44\n");
    return 0;
}
