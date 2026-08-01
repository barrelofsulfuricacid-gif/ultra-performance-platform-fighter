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
            "sim-world=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static pf_content_view make_content(void)
{
    pf_content_view content;
    uint32_t byte_index;

    (void)memset(&content, 0, sizeof(content));
    content.struct_size = (uint32_t)sizeof(content);
    content.schema_version = PF_SIM_CONTENT_SCHEMA_VERSION;
    for (byte_index = UINT32_C(0);
         byte_index < (uint32_t)sizeof(content.content_hash.bytes);
         ++byte_index)
    {
        content.content_hash.bytes[byte_index] =
            (uint8_t)(byte_index * UINT32_C(7) + UINT32_C(3));
    }
    return content;
}

static void make_inputs(
    pf_input_frame *inputs,
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

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim)
{
    pf_memory_requirements requirements;
    pf_status status;

    status = pf_sim_query_memory(config, &requirements);
    if (!expect_status(status, PF_STATUS_OK, "query-memory"))
    {
        return 0;
    }
    if (requirements.state_bytes > sizeof(storage->state) ||
        requirements.scratch_bytes > sizeof(storage->scratch) ||
        requirements.state_alignment > (size_t)TEST_MEMORY_ALIGNMENT ||
        requirements.scratch_alignment > (size_t)TEST_MEMORY_ALIGNMENT)
    {
        (void)fprintf(stderr, "sim-world=fail operation=memory-capacity\n");
        return 0;
    }

    status = pf_sim_init(
        storage->state,
        sizeof(storage->state),
        storage->scratch,
        sizeof(storage->scratch),
        content,
        config,
        out_sim);
    return expect_status(status, PF_STATUS_OK, "init");
}

static int observations_equal(
    const pf_sim_observation *left,
    const pf_sim_observation *right)
{
    return memcmp(left, right, sizeof(*left)) == 0;
}

static int run_determinism_test(const pf_content_view *content)
{
    test_sim_storage first_storage;
    test_sim_storage second_storage;
    pf_sim_config config;
    pf_sim *first = NULL;
    pf_sim *second = NULL;
    pf_sim_observation first_observation;
    pf_sim_observation second_observation;
    pf_sim_observation before_invalid;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_input_frame invalid_inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_tick_result forfeit_result;
    uint64_t tick;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "default-duel-config"))
    {
        return 0;
    }
    config.max_ticks = UINT64_C(240);

    if (!initialize_sim(&first_storage, content, &config, &first) ||
        !initialize_sim(&second_storage, content, &config, &second))
    {
        return 0;
    }
    if (!expect_status(
            pf_sim_reset(first, UINT64_C(20260727)),
            PF_STATUS_OK,
            "first-reset") ||
        !expect_status(
            pf_sim_reset(second, UINT64_C(20260727)),
            PF_STATUS_OK,
            "second-reset") ||
        !expect_status(
            pf_sim_observe(first, &first_observation),
            PF_STATUS_OK,
            "first-observe") ||
        !expect_status(
            pf_sim_observe(second, &second_observation),
            PF_STATUS_OK,
            "second-observe") ||
        !observations_equal(&first_observation, &second_observation))
    {
        (void)fprintf(stderr, "sim-world=fail operation=seeded-reset\n");
        return 0;
    }

    for (tick = UINT64_C(0); tick < UINT64_C(180); ++tick)
    {
        make_inputs(inputs, UINT8_C(2), tick);
        inputs[0].main_stick_x =
            tick < UINT64_C(90) ? INT16_MAX : INT16_C(0);
        inputs[1].main_stick_x =
            tick < UINT64_C(120) ? INT16_MIN : INT16_C(0);
        if (tick == UINT64_C(12) || tick == UINT64_C(90))
        {
            inputs[0].buttons |= PF_INPUT_BUTTON_JUMP;
        }
        if (tick == UINT64_C(40))
        {
            inputs[1].buttons |= PF_INPUT_BUTTON_JUMP;
        }

        if (tick == UINT64_C(30))
        {
            before_invalid = first_observation;
            (void)memcpy(invalid_inputs, inputs, sizeof(inputs));
            invalid_inputs[0].tick = tick + UINT64_C(1);
            if (!expect_status(
                    pf_sim_tick(
                        first,
                        invalid_inputs,
                        (size_t)2,
                        &result),
                    PF_STATUS_TICK_MISMATCH,
                    "atomic-invalid-tick") ||
                !expect_status(
                    pf_sim_observe(first, &first_observation),
                    PF_STATUS_OK,
                    "observe-after-invalid") ||
                !observations_equal(
                    &before_invalid,
                    &first_observation))
            {
                (void)fprintf(
                    stderr,
                    "sim-world=fail operation=atomicity\n");
                return 0;
            }
        }

        if (!expect_status(
                pf_sim_tick(first, inputs, (size_t)2, &result),
                PF_STATUS_OK,
                "first-tick") ||
            !expect_status(
                pf_sim_tick(second, inputs, (size_t)2, &result),
                PF_STATUS_OK,
                "second-tick") ||
            !expect_status(
                pf_sim_observe(first, &first_observation),
                PF_STATUS_OK,
                "first-post-tick-observe") ||
            !expect_status(
                pf_sim_observe(second, &second_observation),
                PF_STATUS_OK,
                "second-post-tick-observe") ||
            !observations_equal(&first_observation, &second_observation))
        {
            (void)fprintf(
                stderr,
                "sim-world=fail operation=determinism tick=%" PRIu64 "\n",
                tick);
            return 0;
        }
    }

    if (first_observation.tick != UINT64_C(180) ||
        first_observation.players[0].position_x_q16 <= INT32_C(0) ||
        first_observation.players[1].position_x_q16 >= INT32_C(0))
    {
        (void)fprintf(stderr, "sim-world=fail operation=movement\n");
        return 0;
    }

    make_inputs(inputs, UINT8_C(2), UINT64_C(180));
    inputs[0].buttons = PF_INPUT_BUTTON_FORFEIT;
    if (!expect_status(
            pf_sim_tick(first, inputs, (size_t)2, &forfeit_result),
            PF_STATUS_OK,
            "forfeit") ||
        forfeit_result.terminated != UINT8_C(1) ||
        forfeit_result.truncated != UINT8_C(0) ||
        forfeit_result.winner_mask != UINT8_C(2) ||
        forfeit_result.event_count != UINT8_C(2) ||
        forfeit_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_FORFEIT ||
        forfeit_result.events[0].target_player != UINT8_C(0) ||
        forfeit_result.events[0].tick != UINT64_C(180) ||
        forfeit_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_MATCH_RESULT ||
        forfeit_result.events[1].detail != UINT16_C(2) ||
        !expect_status(
            pf_sim_tick(first, inputs, (size_t)2, &result),
            PF_STATUS_EPISODE_DONE,
            "post-terminal-step") ||
        result.completed_tick != UINT64_C(181))
    {
        (void)fprintf(stderr, "sim-world=fail operation=termination\n");
        return 0;
    }

    return 1;
}

static int run_memory_validation_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim_config config;
    pf_sim *sim = NULL;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "memory-validation-config"))
    {
        return 0;
    }

    return expect_status(
        pf_sim_init(
            storage.state,
            sizeof(storage.state),
            storage.state,
            sizeof(storage.state),
            content,
            &config,
            &sim),
        PF_STATUS_INVALID_ARGUMENT,
        "overlapping-memory");
}

static int run_truncation_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    uint64_t tick;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "truncation-config"))
    {
        return 0;
    }
    config.max_ticks = UINT64_C(3);
    if (!initialize_sim(&storage, content, &config, &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(11)),
            PF_STATUS_OK,
            "truncation-reset"))
    {
        return 0;
    }

    for (tick = UINT64_C(0); tick < UINT64_C(3); ++tick)
    {
        make_inputs(inputs, UINT8_C(2), tick);
        if (!expect_status(
                pf_sim_tick(sim, inputs, (size_t)2, &result),
                PF_STATUS_OK,
                "truncation-tick"))
        {
            return 0;
        }
    }

    if (result.completed_tick != UINT64_C(3) ||
        result.terminated != UINT8_C(0) ||
        result.truncated != UINT8_C(1) ||
        result.event_count != UINT8_C(1) ||
        result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_TIME_LIMIT ||
        result.events[0].tick != UINT64_C(2))
    {
        (void)fprintf(stderr, "sim-world=fail operation=truncation\n");
        return 0;
    }
    return 1;
}

static int run_four_player_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_sim_observation observation;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    uint32_t player_index;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(4),
                PF_SIM_MODE_TEAMS),
            PF_STATUS_OK,
            "teams-config") ||
        !initialize_sim(&storage, content, &config, &sim) ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(44)),
            PF_STATUS_OK,
            "teams-reset") ||
        !expect_status(
            pf_sim_observe(sim, &observation),
            PF_STATUS_OK,
            "teams-observe"))
    {
        return 0;
    }

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        if (observation.players[player_index].active != UINT8_C(1) ||
            observation.players[player_index].team !=
                (uint8_t)(player_index & UINT32_C(1)))
        {
            (void)fprintf(
                stderr,
                "sim-world=fail operation=four-player-layout\n");
            return 0;
        }
    }

    make_inputs(inputs, UINT8_C(4), UINT64_C(0));
    return expect_status(
        pf_sim_tick(sim, inputs, (size_t)4, &result),
        PF_STATUS_OK,
        "four-player-tick");
}

int main(void)
{
    pf_content_view content = make_content();

    if (!run_memory_validation_test(&content) ||
        !run_determinism_test(&content) ||
        !run_truncation_test(&content) ||
        !run_four_player_test(&content))
    {
        return 1;
    }

    (void)printf(
        "sim-world=pass players=%" PRIu32 " deterministic_ticks=180\n",
        PF_SIM_MAX_PLAYERS);
    return 0;
}
