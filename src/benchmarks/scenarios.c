#include "benchmark.h"

#include "m2_replay_fixture.h"
#include "pf/m4.h"
#include "pf/profile.h"
#include "pf/replay.h"
#include "pf/rl.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define PF_BENCH_MEMORY_BYTES 4096U
#define PF_BENCH_MEMORY_ALIGNMENT 64U
#define PF_BENCH_STORAGE_COUNT 72U
#define PF_BENCH_RL_ENVIRONMENTS 64U
#define PF_BENCH_SAVE_CAPACITY 1024U
#define PF_BENCH_REPLAY_CAPACITY 65536U
#define PF_BENCH_REPLAY_INPUT_COUNT 960U
#define PF_BENCH_REPLAY_HASH_COUNT 241U
#define PF_BENCH_ROLLBACK_DEPTH UINT64_C(8)
#define PF_BENCH_PREPARE_TICKS UINT64_C(64)
#define PF_BENCH_M4_REPRESENTATIVE_CYCLE_TICKS UINT64_C(240)
#define PF_BENCH_M4_MAXIMUM_CYCLE_TICKS UINT64_C(16)
#define PF_BENCH_MAX_ITERATIONS (UINT64_C(1) << 34U)

typedef struct pf_bench_storage
{
    alignas(PF_BENCH_MEMORY_ALIGNMENT)
        uint8_t state[PF_BENCH_MEMORY_BYTES];
    alignas(PF_BENCH_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_BENCH_MEMORY_BYTES];
} pf_bench_storage;

typedef int (*pf_benchmark_case)(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY]);

enum pf_benchmark_scenario_index
{
    PF_BENCH_EMPTY_TICK = 0,
    PF_BENCH_REPRESENTATIVE_1V1 = 1,
    PF_BENCH_REPRESENTATIVE_2V2 = 2,
    PF_BENCH_MAXIMUM_COMBAT_ENTITIES = 3,
    PF_BENCH_HAZARD_HEAVY = 4,
    PF_BENCH_SNAPSHOT_SAVE = 5,
    PF_BENCH_SNAPSHOT_RESTORE = 6,
    PF_BENCH_ROLLBACK_RESIMULATION = 7,
    PF_BENCH_REPLAY_VERIFICATION = 8,
    PF_BENCH_RL_SINGLE = 9,
    PF_BENCH_RL_BATCH = 10,
    PF_BENCH_DESIGN_DATA_IMPORT = 11,
    PF_BENCH_CLIENT_FRAME = 12
};

static const pf_benchmark_descriptor pf_benchmark_descriptors[
    PF_BENCHMARK_SCENARIO_COUNT] = {
        {
            "empty_tick",
            UINT32_C(1),
            UINT64_C(31001),
            "logical_ticks_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "representative_1v1",
            UINT32_C(2),
            UINT64_C(31002),
            "logical_ticks_per_second",
            "M4",
            NULL,
            NULL,
        },
        {
            "representative_2v2",
            UINT32_C(1),
            UINT64_C(31003),
            "logical_ticks_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "maximum_combat_entities",
            UINT32_C(1),
            UINT64_C(31004),
            "logical_ticks_per_second",
            "M4",
            NULL,
            NULL,
        },
        {
            "hazard_heavy_four_player",
            UINT32_C(1),
            UINT64_C(31005),
            "logical_ticks_per_second",
            "M6",
            "capability-not-implemented",
            "The deterministic stage-hazard framework enters in M6.",
        },
        {
            "snapshot_save",
            UINT32_C(1),
            UINT64_C(31006),
            "operations_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "snapshot_restore",
            UINT32_C(1),
            UINT64_C(31007),
            "operations_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "rollback_resimulation_depth_8",
            UINT32_C(1),
            UINT64_C(31008),
            "logical_ticks_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "replay_verification",
            UINT32_C(1),
            PF_M2_REPLAY_SEED,
            "logical_ticks_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "rl_single_environment_calls",
            UINT32_C(1),
            UINT64_C(31010),
            "logical_ticks_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "rl_batched_step",
            UINT32_C(1),
            UINT64_C(31011),
            "logical_ticks_per_second",
            "M3",
            NULL,
            NULL,
        },
        {
            "design_data_import",
            UINT32_C(1),
            UINT64_C(31012),
            "operations_per_second",
            "M5",
            "capability-not-implemented",
            "Workbook import and packed design data enter in M5.",
        },
        {
            "client_frame",
            UINT32_C(1),
            UINT64_C(31013),
            "frames_per_second",
            "M7",
            "capability-not-implemented",
            "Representative client rendering and frame timing enter in M7.",
        },
};

static pf_bench_storage pf_benchmark_storage[PF_BENCH_STORAGE_COUNT];
static pf_sim *pf_duel_sim;
static pf_sim *pf_restore_sim;
static pf_sim *pf_team_sim;
static pf_sim *pf_m4_duel_sim;
static pf_sim *pf_m4_maximum_sim;
static pf_sim *pf_replay_initial_sim;
static pf_sim *pf_replay_source_sim;
static pf_sim *pf_replay_target_sim;
static pf_sim *pf_rl_sims[PF_BENCH_RL_ENVIRONMENTS];
static uint64_t pf_rl_seeds[PF_BENCH_RL_ENVIRONMENTS];
static pf_rl_action pf_rl_actions[
    PF_BENCH_RL_ENVIRONMENTS * PF_SIM_MAX_PLAYERS];
static pf_rl_transition pf_rl_transitions[PF_BENCH_RL_ENVIRONMENTS];
static uint8_t pf_save_bytes[PF_BENCH_SAVE_CAPACITY];
static size_t pf_save_size;
static pf_input_frame pf_replay_inputs[PF_BENCH_REPLAY_INPUT_COUNT];
static pf_state_hash pf_replay_hashes[PF_BENCH_REPLAY_HASH_COUNT];
static uint8_t pf_replay_bytes[PF_BENCH_REPLAY_CAPACITY];
static size_t pf_replay_size;
static uint64_t pf_state_bytes;
static pf_m4_content pf_benchmark_m4_duel_content;
static pf_m4_content pf_benchmark_m4_maximum_content;
static pf_content_view pf_benchmark_m4_duel_view;
static pf_content_view pf_benchmark_m4_maximum_view;

_Static_assert(
    PF_BENCH_REPLAY_INPUT_COUNT ==
        PF_M2_REPLAY_TICKS * PF_M2_REPLAY_PLAYERS,
    "benchmark replay inputs must cover the M2 fixture");
_Static_assert(
    PF_BENCH_REPLAY_HASH_COUNT == PF_M2_REPLAY_TICKS + UINT64_C(1),
    "benchmark replay hashes must include tick zero");

static void set_error(
    char error[PF_BENCHMARK_ERROR_CAPACITY],
    const char *operation,
    pf_status status)
{
    (void)snprintf(
        error,
        PF_BENCHMARK_ERROR_CAPACITY,
        "%s: %s",
        operation,
        pf_status_name(status));
}

static void set_text_error(
    char error[PF_BENCHMARK_ERROR_CAPACITY],
    const char *message)
{
    (void)snprintf(
        error,
        PF_BENCHMARK_ERROR_CAPACITY,
        "%s",
        message);
}

static uint64_t clock_nanoseconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (QueryPerformanceFrequency(&frequency) == 0 ||
        QueryPerformanceCounter(&counter) == 0 ||
        frequency.QuadPart <= 0)
    {
        return UINT64_C(0);
    }
    return ((uint64_t)counter.QuadPart /
            (uint64_t)frequency.QuadPart) *
               UINT64_C(1000000000) +
           (((uint64_t)counter.QuadPart %
             (uint64_t)frequency.QuadPart) *
            UINT64_C(1000000000)) /
               (uint64_t)frequency.QuadPart;
#else
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0 ||
        value.tv_sec < (time_t)0 ||
        value.tv_nsec < 0L)
    {
        return UINT64_C(0);
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
           (uint64_t)value.tv_nsec;
#endif
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
            (uint8_t)(byte_index * UINT32_C(17) + UINT32_C(23));
    }
    return content;
}

static int make_m4_benchmark_content(
    pf_m4_content *content,
    pf_content_view *view,
    int maximum_entities,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_status status;

    status = pf_m4_default_content(content);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "default M4 content", status);
        return 0;
    }
    content->item.enabled = UINT8_C(1);
    content->item.lifetime_ticks = UINT16_C(3600);
    content->projectile.enabled = UINT8_C(1);
    content->projectile.spawn_offset_y_q16 =
        -INT32_C(4) * PF_Q16_ONE;
    content->reflector.enabled = UINT8_C(1);
    content->charge.enabled = UINT8_C(1);
    content->recovery.enabled = UINT8_C(1);

    if (maximum_entities != 0)
    {
        content->stage.spawn_spacing_q16 = PF_Q16_ONE;
        content->stage.platform_center_x_q16 =
            -INT32_C(20) * PF_Q16_ONE;
        content->stage.platform_motion_amplitude_q16 = INT32_C(0);
        content->item.spawn_x_q16 =
            -(INT32_C(3) * PF_Q16_ONE) / INT32_C(2);
        content->item.spawn_y_q16 =
            content->stage.floor_y_q16 -
            content->item.half_height_q16;
        content->projectile.fire_recovery_ticks = UINT16_C(2);
    }
    else
    {
        content->stage.spawn_spacing_q16 = PF_Q16_ONE;
        content->stage.platform_center_x_q16 =
            -INT32_C(20) * PF_Q16_ONE;
        content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    }

    status = pf_m4_make_content_view(content, view);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "make M4 content view", status);
        return 0;
    }
    return 1;
}

static int initialize_sim(
    size_t storage_index,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    pf_sim **out_sim,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_sim_config config;
    pf_memory_requirements requirements;
    pf_status status;

    status = pf_sim_default_config(&config, player_count, mode);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "default config", status);
        return 0;
    }
    config.max_ticks = UINT64_C(1000000000);
    config.stock_count = UINT8_C(0);
    status = pf_sim_query_memory(&config, &requirements);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "query memory", status);
        return 0;
    }
    if (storage_index >= (size_t)PF_BENCH_STORAGE_COUNT ||
        requirements.state_bytes > (size_t)PF_BENCH_MEMORY_BYTES ||
        requirements.scratch_bytes > (size_t)PF_BENCH_MEMORY_BYTES ||
        requirements.state_alignment >
            (size_t)PF_BENCH_MEMORY_ALIGNMENT ||
        requirements.scratch_alignment >
            (size_t)PF_BENCH_MEMORY_ALIGNMENT)
    {
        set_text_error(error, "benchmark simulation storage is insufficient");
        return 0;
    }

    status = pf_sim_init(
        pf_benchmark_storage[storage_index].state,
        sizeof(pf_benchmark_storage[storage_index].state),
        pf_benchmark_storage[storage_index].scratch,
        sizeof(pf_benchmark_storage[storage_index].scratch),
        content,
        &config,
        out_sim);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "initialize simulation", status);
        return 0;
    }
    if (pf_state_bytes == UINT64_C(0))
    {
        pf_state_bytes = (uint64_t)requirements.state_bytes;
    }
    return 1;
}

static void make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint8_t player_count,
    uint64_t tick,
    int representative)
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
        if (representative != 0)
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
    }
}

static void make_m4_representative_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick)
{
    uint32_t player_index;

    make_inputs(inputs, UINT8_C(2), tick, 1);
    for (player_index = UINT32_C(0);
         player_index < UINT32_C(2);
         ++player_index)
    {
        const uint64_t phase =
            tick + (uint64_t)player_index * UINT64_C(13);

        inputs[player_index].secondary_stick_x =
            player_index == UINT32_C(0) ? INT16_MAX : INT16_MIN;
        if (phase % UINT64_C(113) == UINT64_C(7))
        {
            const uint64_t special_route =
                (phase / UINT64_C(113)) % UINT64_C(3);

            inputs[player_index].buttons = PF_INPUT_BUTTON_SPECIAL;
            if (special_route == UINT64_C(1))
            {
                inputs[player_index].main_stick_y = INT16_MAX;
            }
            else if (special_route == UINT64_C(2))
            {
                inputs[player_index].main_stick_y = INT16_MIN;
            }
            else
            {
                inputs[player_index].main_stick_y = INT16_C(0);
            }
        }
        else if (phase % UINT64_C(79) == UINT64_C(5))
        {
            inputs[player_index].buttons =
                PF_INPUT_BUTTON_STRONG_ATTACK;
        }
        else if (phase % UINT64_C(47) == UINT64_C(2))
        {
            inputs[player_index].buttons = PF_INPUT_BUTTON_ATTACK;
        }
        else if (phase % UINT64_C(61) == UINT64_C(4))
        {
            inputs[player_index].buttons = PF_INPUT_BUTTON_JUMP;
        }
        else if (phase % UINT64_C(211) == UINT64_C(19))
        {
            inputs[player_index].buttons = PF_INPUT_BUTTON_TAUNT;
        }
        if (phase % UINT64_C(97) >= UINT64_C(20) &&
            phase % UINT64_C(97) <= UINT64_C(25))
        {
            inputs[player_index].left_trigger = UINT16_MAX;
        }
    }
    if (tick == UINT64_C(0))
    {
        inputs[0].buttons = PF_INPUT_BUTTON_SPECIAL;
        inputs[0].main_stick_y = INT16_C(0);
        inputs[1].buttons = PF_INPUT_BUTTON_ATTACK;
    }
}

static void make_m4_maximum_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(inputs, 0, sizeof(*inputs) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
        inputs[player_index].secondary_stick_x =
            player_index < UINT32_C(2) ? INT16_MAX : INT16_MIN;
    }

    if (tick == UINT64_C(0))
    {
        inputs[0].buttons = PF_INPUT_BUTTON_ATTACK;
        inputs[0].left_trigger = UINT16_MAX;
        inputs[1].buttons = PF_INPUT_BUTTON_SPECIAL;
    }
    else if (tick == UINT64_C(2))
    {
        inputs[0].buttons = PF_INPUT_BUTTON_STRONG_ATTACK;
        for (player_index = UINT32_C(2);
             player_index < (uint32_t)PF_SIM_MAX_PLAYERS;
             ++player_index)
        {
            inputs[player_index].buttons = PF_INPUT_BUTTON_ATTACK;
        }
    }
    else if (tick == UINT64_C(3))
    {
        inputs[1].buttons = PF_INPUT_BUTTON_ATTACK;
    }
    else if (tick == UINT64_C(8))
    {
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)PF_SIM_MAX_PLAYERS;
             ++player_index)
        {
            inputs[player_index].buttons =
                PF_INPUT_BUTTON_STRONG_ATTACK;
        }
    }
    else if (tick >= UINT64_C(11) && tick <= UINT64_C(13))
    {
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)PF_SIM_MAX_PLAYERS;
             ++player_index)
        {
            inputs[player_index].left_trigger = UINT16_MAX;
        }
    }
}

static int64_t hash_checksum(
    pf_sim *sim,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_state_hash hash;
    uint64_t value = UINT64_C(0);
    uint32_t byte_index;
    pf_status status = pf_sim_hash(sim, &hash);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "hash benchmark state", status);
        return INT64_C(-1);
    }
    for (byte_index = UINT32_C(0); byte_index < UINT32_C(8); ++byte_index)
    {
        value |= (uint64_t)hash.bytes[byte_index] << (byte_index * 8U);
    }
    return (int64_t)(value & (uint64_t)INT64_MAX);
}

static int complete_sample(
    pf_benchmark_sample *sample,
    uint64_t iterations,
    uint64_t logical_ticks,
    uint64_t started,
    uint64_t finished,
    int64_t checksum,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    uint64_t rate_count = logical_ticks != UINT64_C(0)
                              ? logical_ticks
                              : iterations;

    if (finished < started ||
        iterations == UINT64_C(0) ||
        rate_count == UINT64_C(0) ||
        checksum < INT64_C(0))
    {
        if (error[0] == '\0')
        {
            set_text_error(error, "invalid benchmark timing or checksum");
        }
        return 0;
    }
    sample->iterations = iterations;
    sample->logical_ticks = logical_ticks;
    sample->elapsed_ns = finished - started;
    /*
     * A one-iteration calibration probe can be shorter than the observable
     * clock resolution. The calibration loop will double its work; completed
     * measurement samples are checked separately and must remain nonzero.
     */
    if (sample->elapsed_ns == UINT64_C(0))
    {
        sample->rate_per_second = 0.0;
        sample->ns_per_operation = 0.0;
        sample->checksum = checksum;
        return 1;
    }
    sample->rate_per_second =
        (double)rate_count * 1000000000.0 /
        (double)sample->elapsed_ns;
    sample->ns_per_operation =
        (double)sample->elapsed_ns / (double)iterations;
    sample->checksum = checksum;
    return 1;
}

static int run_tick_case(
    pf_sim *sim,
    uint64_t seed,
    uint8_t player_count,
    int representative,
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    uint64_t tick;
    uint64_t started;
    uint64_t finished;
    int64_t checksum;
    pf_status status = pf_sim_reset(sim, seed);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset tick scenario", status);
        return 0;
    }
    started = clock_nanoseconds();
    for (tick = UINT64_C(0); tick < iterations; ++tick)
    {
        make_inputs(inputs, player_count, tick, representative);
        status = pf_sim_tick(sim, inputs, (size_t)player_count, &result);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "run tick scenario", status);
            return 0;
        }
    }
    finished = clock_nanoseconds();
    checksum = hash_checksum(sim, error);
    return complete_sample(
        sample,
        iterations,
        iterations,
        started,
        finished,
        checksum,
        error);
}

static int run_m4_tick_case(
    pf_sim *sim,
    uint64_t seed,
    uint8_t player_count,
    int maximum_entities,
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    const uint64_t cycle_ticks =
        maximum_entities != 0
            ? PF_BENCH_M4_MAXIMUM_CYCLE_TICKS
            : PF_BENCH_M4_REPRESENTATIVE_CYCLE_TICKS;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    uint64_t iteration;
    uint64_t started;
    uint64_t finished;
    int64_t checksum;
    pf_status status = pf_sim_reset(sim, seed);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset M4 tick scenario", status);
        return 0;
    }
    started = clock_nanoseconds();
    for (iteration = UINT64_C(0);
         iteration < iterations;
         ++iteration)
    {
        const uint64_t cycle_tick = iteration % cycle_ticks;

        if (iteration != UINT64_C(0) && cycle_tick == UINT64_C(0))
        {
            /* Keep long calibrated samples in the same bounded live trace
             * instead of timing a post-KO or exhausted-resource idle state. */
            status = pf_sim_reset(sim, seed);
            if (status != PF_STATUS_OK)
            {
                set_error(error, "cycle-reset M4 tick scenario", status);
                return 0;
            }
        }
        if (maximum_entities != 0)
        {
            make_m4_maximum_inputs(inputs, cycle_tick);
        }
        else
        {
            make_m4_representative_inputs(inputs, cycle_tick);
        }
        status = pf_sim_tick(
            sim,
            inputs,
            (size_t)player_count,
            &result);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "run M4 tick scenario", status);
            return 0;
        }
    }
    finished = clock_nanoseconds();
    checksum = hash_checksum(sim, error);
    return complete_sample(
        sample,
        iterations,
        iterations,
        started,
        finished,
        checksum,
        error);
}

static int validate_m4_maximum_workload(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_m4_inspection inspection;
    uint64_t tick;
    uint32_t maximum_hurtboxes = UINT32_C(0);
    uint32_t maximum_fighter_hitboxes = UINT32_C(0);
    uint32_t maximum_attack_entities = UINT32_C(0);
    uint32_t maximum_events = UINT32_C(0);
    int saw_item_hitbox = 0;
    int saw_projectile_hitbox = 0;
    int saw_item_projectile_overlap = 0;
    pf_status status = pf_sim_reset(
        pf_m4_maximum_sim,
        pf_benchmark_descriptors[PF_BENCH_MAXIMUM_COMBAT_ENTITIES].seed);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset maximum-combat preflight", status);
        return 0;
    }
    for (tick = UINT64_C(0);
         tick < PF_BENCH_M4_MAXIMUM_CYCLE_TICKS;
         ++tick)
    {
        uint32_t player_index;
        uint32_t hurtboxes = UINT32_C(0);
        uint32_t fighter_hitboxes = UINT32_C(0);
        uint32_t attack_entities;

        make_m4_maximum_inputs(inputs, tick);
        status = pf_sim_tick(
            pf_m4_maximum_sim,
            inputs,
            (size_t)PF_SIM_MAX_PLAYERS,
            &result);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "run maximum-combat preflight", status);
            return 0;
        }
        status = pf_m4_inspect(pf_m4_maximum_sim, &inspection);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "inspect maximum-combat preflight", status);
            return 0;
        }
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)PF_SIM_MAX_PLAYERS;
             ++player_index)
        {
            hurtboxes += inspection.players[player_index].active != UINT8_C(0)
                             ? UINT32_C(1)
                             : UINT32_C(0);
            fighter_hitboxes +=
                inspection.players[player_index].hitbox_active != UINT8_C(0)
                    ? UINT32_C(1)
                    : UINT32_C(0);
        }
        saw_item_hitbox =
            saw_item_hitbox != 0 ||
            inspection.item.hitbox_active != UINT8_C(0);
        saw_projectile_hitbox =
            saw_projectile_hitbox != 0 ||
            inspection.projectile.hitbox_active != UINT8_C(0);
        saw_item_projectile_overlap =
            saw_item_projectile_overlap != 0 ||
            (inspection.item.hitbox_active != UINT8_C(0) &&
             inspection.projectile.hitbox_active != UINT8_C(0));
        attack_entities =
            fighter_hitboxes +
            (inspection.item.hitbox_active != UINT8_C(0)
                 ? UINT32_C(1)
                 : UINT32_C(0)) +
            (inspection.projectile.hitbox_active != UINT8_C(0)
                 ? UINT32_C(1)
                 : UINT32_C(0));
        if (hurtboxes > maximum_hurtboxes)
        {
            maximum_hurtboxes = hurtboxes;
        }
        if (fighter_hitboxes > maximum_fighter_hitboxes)
        {
            maximum_fighter_hitboxes = fighter_hitboxes;
        }
        if (attack_entities > maximum_attack_entities)
        {
            maximum_attack_entities = attack_entities;
        }
        if ((uint32_t)result.event_count > maximum_events)
        {
            maximum_events = (uint32_t)result.event_count;
        }
    }

    if (maximum_hurtboxes != (uint32_t)PF_SIM_MAX_PLAYERS ||
        maximum_fighter_hitboxes < UINT32_C(2) ||
        maximum_attack_entities < UINT32_C(4) ||
        maximum_events == UINT32_C(0) ||
        saw_item_hitbox == 0 || saw_projectile_hitbox == 0 ||
        saw_item_projectile_overlap == 0)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "maximum-combat preflight lacked coverage: hurtboxes=%" PRIu32
            " fighter_hitboxes=%" PRIu32 " attack_entities=%" PRIu32
            " events=%" PRIu32 " item=%d projectile=%d overlap=%d",
            maximum_hurtboxes,
            maximum_fighter_hitboxes,
            maximum_attack_entities,
            maximum_events,
            saw_item_hitbox,
            saw_projectile_hitbox,
            saw_item_projectile_overlap);
        return 0;
    }
    return 1;
}

static int validate_m4_representative_workload(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_m4_inspection inspection;
    uint64_t tick;
    uint32_t combat_events = UINT32_C(0);
    int saw_fighter_hitbox = 0;
    int saw_projectile = 0;
    int saw_shield = 0;
    pf_status status = pf_sim_reset(
        pf_m4_duel_sim,
        pf_benchmark_descriptors[PF_BENCH_REPRESENTATIVE_1V1].seed);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset representative-M4 preflight", status);
        return 0;
    }
    for (tick = UINT64_C(0);
         tick < PF_BENCH_M4_REPRESENTATIVE_CYCLE_TICKS;
         ++tick)
    {
        uint32_t event_index;
        uint32_t player_index;

        make_m4_representative_inputs(inputs, tick);
        status = pf_sim_tick(
            pf_m4_duel_sim,
            inputs,
            (size_t)2,
            &result);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "run representative-M4 preflight", status);
            return 0;
        }
        status = pf_m4_inspect(pf_m4_duel_sim, &inspection);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "inspect representative-M4 preflight", status);
            return 0;
        }
        saw_projectile =
            saw_projectile != 0 ||
            inspection.projectile.hitbox_active != UINT8_C(0);
        for (player_index = UINT32_C(0);
             player_index < UINT32_C(2);
             ++player_index)
        {
            saw_fighter_hitbox =
                saw_fighter_hitbox != 0 ||
                inspection.players[player_index].hitbox_active != UINT8_C(0);
            saw_shield =
                saw_shield != 0 ||
                inspection.players[player_index].shield_held != UINT8_C(0);
        }
        for (event_index = UINT32_C(0);
             event_index < (uint32_t)result.event_count;
             ++event_index)
        {
            const uint16_t type = result.events[event_index].type;

            if (type == (uint16_t)PF_SIM_EVENT_HIT ||
                type == (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
                type == (uint16_t)PF_SIM_EVENT_ITEM_HIT ||
                type == (uint16_t)PF_SIM_EVENT_PROJECTILE_HIT)
            {
                ++combat_events;
            }
        }
    }

    if (combat_events == UINT32_C(0) || saw_fighter_hitbox == 0 ||
        saw_projectile == 0 || saw_shield == 0)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "representative-M4 preflight lacked coverage: combat_events=%"
            PRIu32 " fighter_hitbox=%d projectile=%d shield=%d",
            combat_events,
            saw_fighter_hitbox,
            saw_projectile,
            saw_shield);
        return 0;
    }
    return 1;
}

static int run_empty_tick(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return run_tick_case(
        pf_duel_sim,
        pf_benchmark_descriptors[PF_BENCH_EMPTY_TICK].seed,
        UINT8_C(2),
        0,
        iterations,
        sample,
        error);
}

static int run_representative_1v1(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return run_m4_tick_case(
        pf_m4_duel_sim,
        pf_benchmark_descriptors[PF_BENCH_REPRESENTATIVE_1V1].seed,
        UINT8_C(2),
        0,
        iterations,
        sample,
        error);
}

static int run_maximum_combat_entities(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return run_m4_tick_case(
        pf_m4_maximum_sim,
        pf_benchmark_descriptors[PF_BENCH_MAXIMUM_COMBAT_ENTITIES].seed,
        PF_SIM_MAX_PLAYERS,
        1,
        iterations,
        sample,
        error);
}

static int run_representative_2v2(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return run_tick_case(
        pf_team_sim,
        pf_benchmark_descriptors[PF_BENCH_REPRESENTATIVE_2V2].seed,
        UINT8_C(4),
        1,
        iterations,
        sample,
        error);
}

static int prepare_snapshot(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_mut_bytes destination;
    uint64_t tick;
    pf_status status = pf_sim_reset(
        pf_duel_sim,
        pf_benchmark_descriptors[PF_BENCH_SNAPSHOT_SAVE].seed);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset snapshot source", status);
        return 0;
    }
    for (tick = UINT64_C(0); tick < PF_BENCH_PREPARE_TICKS; ++tick)
    {
        make_inputs(inputs, UINT8_C(2), tick, 1);
        status = pf_sim_tick(
            pf_duel_sim,
            inputs,
            (size_t)2,
            &result);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "prepare snapshot source", status);
            return 0;
        }
    }
    status = pf_sim_query_save_size(pf_duel_sim, &pf_save_size);
    if (status != PF_STATUS_OK ||
        pf_save_size > (size_t)PF_BENCH_SAVE_CAPACITY)
    {
        set_error(error, "query benchmark save size", status);
        return 0;
    }
    destination.bytes = pf_save_bytes;
    destination.capacity = sizeof(pf_save_bytes);
    destination.size = (size_t)0;
    status = pf_sim_save(pf_duel_sim, &destination);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "prepare benchmark save", status);
        return 0;
    }
    pf_save_size = destination.size;
    return 1;
}

static int64_t save_checksum(void)
{
    uint64_t value = UINT64_C(0);
    size_t byte_index;
    size_t limit =
        pf_save_size < (size_t)8 ? pf_save_size : (size_t)8;

    for (byte_index = (size_t)0; byte_index < limit; ++byte_index)
    {
        value |= (uint64_t)pf_save_bytes[byte_index] <<
                 ((uint32_t)byte_index * 8U);
    }
    return (int64_t)(value & (uint64_t)INT64_MAX);
}

static int run_snapshot_save(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_mut_bytes destination;
    uint64_t iteration;
    uint64_t started;
    uint64_t finished;
    pf_status status;

    if (!prepare_snapshot(error))
    {
        return 0;
    }
    destination.bytes = pf_save_bytes;
    destination.capacity = sizeof(pf_save_bytes);
    destination.size = (size_t)0;
    started = clock_nanoseconds();
    for (iteration = UINT64_C(0); iteration < iterations; ++iteration)
    {
        status = pf_sim_save(pf_duel_sim, &destination);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "benchmark snapshot save", status);
            return 0;
        }
    }
    finished = clock_nanoseconds();
    return complete_sample(
        sample,
        iterations,
        UINT64_C(0),
        started,
        finished,
        save_checksum(),
        error);
}

static int run_snapshot_restore(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_bytes source;
    uint64_t iteration;
    uint64_t started;
    uint64_t finished;
    int64_t checksum;
    pf_status status;

    if (!prepare_snapshot(error))
    {
        return 0;
    }
    source.bytes = pf_save_bytes;
    source.size = pf_save_size;
    started = clock_nanoseconds();
    for (iteration = UINT64_C(0); iteration < iterations; ++iteration)
    {
        status = pf_sim_load(pf_restore_sim, source);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "benchmark snapshot restore", status);
            return 0;
        }
    }
    finished = clock_nanoseconds();
    checksum = hash_checksum(pf_restore_sim, error);
    return complete_sample(
        sample,
        iterations,
        UINT64_C(0),
        started,
        finished,
        checksum,
        error);
}

static int run_rollback_resimulation(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_bytes source;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    uint64_t iteration;
    uint64_t depth_tick;
    uint64_t started;
    uint64_t finished;
    uint64_t logical_ticks;
    int64_t checksum;
    pf_status status;

    if (!prepare_snapshot(error) ||
        iterations > UINT64_MAX / PF_BENCH_ROLLBACK_DEPTH)
    {
        set_text_error(error, "rollback iteration count overflow");
        return 0;
    }
    source.bytes = pf_save_bytes;
    source.size = pf_save_size;
    started = clock_nanoseconds();
    for (iteration = UINT64_C(0); iteration < iterations; ++iteration)
    {
        status = pf_sim_load(pf_restore_sim, source);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "rollback restore", status);
            return 0;
        }
        for (depth_tick = UINT64_C(0);
             depth_tick < PF_BENCH_ROLLBACK_DEPTH;
             ++depth_tick)
        {
            make_inputs(
                inputs,
                UINT8_C(2),
                PF_BENCH_PREPARE_TICKS + depth_tick,
                1);
            status = pf_sim_tick(
                pf_restore_sim,
                inputs,
                (size_t)2,
                &result);
            if (status != PF_STATUS_OK)
            {
                set_error(error, "rollback resimulation", status);
                return 0;
            }
        }
    }
    finished = clock_nanoseconds();
    checksum = hash_checksum(pf_restore_sim, error);
    logical_ticks = iterations * PF_BENCH_ROLLBACK_DEPTH;
    return complete_sample(
        sample,
        iterations,
        logical_ticks,
        started,
        finished,
        checksum,
        error);
}

static int prepare_replay(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_content_view content = pf_m2_replay_make_content();
    pf_tick_result result;
    pf_replay_source source;
    pf_mut_bytes destination;
    uint64_t tick;
    pf_status status = pf_sim_reset(
        pf_replay_initial_sim,
        PF_M2_REPLAY_SEED);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset replay initial state", status);
        return 0;
    }
    status = pf_sim_clone(
        pf_replay_source_sim,
        pf_replay_initial_sim);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "clone replay source", status);
        return 0;
    }
    status = pf_sim_hash(
        pf_replay_initial_sim,
        &pf_replay_hashes[0]);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "hash replay initial state", status);
        return 0;
    }
    for (tick = UINT64_C(0); tick < PF_M2_REPLAY_TICKS; ++tick)
    {
        pf_input_frame *inputs =
            &pf_replay_inputs[
                (size_t)tick * (size_t)PF_M2_REPLAY_PLAYERS];
        pf_m2_replay_make_tick_inputs(inputs, tick);
        status = pf_sim_tick(
            pf_replay_source_sim,
            inputs,
            (size_t)PF_M2_REPLAY_PLAYERS,
            &result);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "build replay trace", status);
            return 0;
        }
        status = pf_sim_hash(
            pf_replay_source_sim,
            &pf_replay_hashes[(size_t)tick + (size_t)1]);
        if (status != PF_STATUS_OK)
        {
            set_error(error, "hash replay trace", status);
            return 0;
        }
    }

    (void)memset(&source, 0, sizeof(source));
    source.struct_size = (uint32_t)sizeof(source);
    source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    source.initial_state = pf_replay_initial_sim;
    source.input_frames = pf_replay_inputs;
    source.input_frame_count = PF_BENCH_REPLAY_INPUT_COUNT;
    source.state_hashes = pf_replay_hashes;
    source.state_hash_count = PF_BENCH_REPLAY_HASH_COUNT;
    source.tick_count = PF_M2_REPLAY_TICKS;
    source.final_result = result;

    status = pf_replay_query_size(&source, &pf_replay_size);
    if (status != PF_STATUS_OK ||
        pf_replay_size > (size_t)PF_BENCH_REPLAY_CAPACITY)
    {
        set_error(error, "query benchmark replay size", status);
        return 0;
    }
    destination.bytes = pf_replay_bytes;
    destination.capacity = sizeof(pf_replay_bytes);
    destination.size = (size_t)0;
    status = pf_replay_encode(&source, &destination);
    if (status != PF_STATUS_OK)
    {
        set_error(error, "encode benchmark replay", status);
        return 0;
    }
    pf_replay_size = destination.size;
    (void)content;
    return 1;
}

static int run_replay_verification(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_bytes replay;
    pf_replay_verification verification;
    uint64_t iteration;
    uint64_t started;
    uint64_t finished;
    uint64_t logical_ticks;
    int64_t checksum;
    pf_status status;

    if (iterations > UINT64_MAX / PF_M2_REPLAY_TICKS)
    {
        set_text_error(error, "replay iteration count overflow");
        return 0;
    }
    replay.bytes = pf_replay_bytes;
    replay.size = pf_replay_size;
    started = clock_nanoseconds();
    for (iteration = UINT64_C(0); iteration < iterations; ++iteration)
    {
        status = pf_replay_verify(
            pf_replay_target_sim,
            replay,
            &verification);
        if (status != PF_STATUS_OK ||
            verification.verified_ticks != PF_M2_REPLAY_TICKS)
        {
            set_error(error, "benchmark replay verification", status);
            return 0;
        }
    }
    finished = clock_nanoseconds();
    checksum = hash_checksum(pf_replay_target_sim, error);
    logical_ticks = iterations * PF_M2_REPLAY_TICKS;
    return complete_sample(
        sample,
        iterations,
        logical_ticks,
        started,
        finished,
        checksum,
        error);
}

static int reset_rl_environments(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_status status = pf_rl_reset_batch(
        pf_rl_sims,
        pf_rl_seeds,
        (size_t)PF_BENCH_RL_ENVIRONMENTS,
        pf_rl_transitions);

    if (status != PF_STATUS_OK)
    {
        set_error(error, "reset RL environments", status);
        return 0;
    }
    return 1;
}

static int64_t rl_checksum(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    uint64_t checksum = UINT64_C(0);
    size_t environment_index;

    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_BENCH_RL_ENVIRONMENTS;
         ++environment_index)
    {
        int64_t value = hash_checksum(
            pf_rl_sims[environment_index],
            error);
        if (value < INT64_C(0))
        {
            return INT64_C(-1);
        }
        checksum ^= (uint64_t)value +
                    (uint64_t)environment_index *
                        UINT64_C(0x9e3779b97f4a7c15);
    }
    return (int64_t)(checksum & (uint64_t)INT64_MAX);
}

static int run_rl(
    uint64_t iterations,
    int batched,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    uint64_t iteration;
    uint64_t started;
    uint64_t finished;
    uint64_t logical_ticks;
    int64_t checksum;
    pf_status status;

    if (iterations >
        UINT64_MAX / (uint64_t)PF_BENCH_RL_ENVIRONMENTS)
    {
        set_text_error(error, "RL iteration count overflow");
        return 0;
    }
    if (!reset_rl_environments(error))
    {
        return 0;
    }
    started = clock_nanoseconds();
    for (iteration = UINT64_C(0); iteration < iterations; ++iteration)
    {
        if (batched != 0)
        {
            status = pf_rl_step_batch(
                pf_rl_sims,
                (size_t)PF_BENCH_RL_ENVIRONMENTS,
                pf_rl_actions,
                (size_t)PF_SIM_MAX_PLAYERS,
                pf_rl_transitions);
            if (status != PF_STATUS_OK)
            {
                set_error(error, "benchmark batched RL step", status);
                return 0;
            }
        }
        else
        {
            size_t environment_index;

            for (environment_index = (size_t)0;
                 environment_index <
                     (size_t)PF_BENCH_RL_ENVIRONMENTS;
                 ++environment_index)
            {
                const size_t action_base =
                    environment_index *
                    (size_t)PF_SIM_MAX_PLAYERS;
                status = pf_rl_step(
                    pf_rl_sims[environment_index],
                    &pf_rl_actions[action_base],
                    (size_t)2,
                    &pf_rl_transitions[environment_index]);
                if (status != PF_STATUS_OK)
                {
                    set_error(error, "benchmark single RL step", status);
                    return 0;
                }
            }
        }
    }
    finished = clock_nanoseconds();
    checksum = rl_checksum(error);
    logical_ticks =
        iterations * (uint64_t)PF_BENCH_RL_ENVIRONMENTS;
    return complete_sample(
        sample,
        iterations,
        logical_ticks,
        started,
        finished,
        checksum,
        error);
}

static int run_rl_single(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return run_rl(iterations, 0, sample, error);
}

static int run_rl_batch(
    uint64_t iterations,
    pf_benchmark_sample *sample,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return run_rl(iterations, 1, sample, error);
}

static pf_benchmark_case benchmark_case_for_index(size_t index)
{
    switch (index)
    {
        case PF_BENCH_EMPTY_TICK:
            return run_empty_tick;
        case PF_BENCH_REPRESENTATIVE_1V1:
            return run_representative_1v1;
        case PF_BENCH_REPRESENTATIVE_2V2:
            return run_representative_2v2;
        case PF_BENCH_MAXIMUM_COMBAT_ENTITIES:
            return run_maximum_combat_entities;
        case PF_BENCH_SNAPSHOT_SAVE:
            return run_snapshot_save;
        case PF_BENCH_SNAPSHOT_RESTORE:
            return run_snapshot_restore;
        case PF_BENCH_ROLLBACK_RESIMULATION:
            return run_rollback_resimulation;
        case PF_BENCH_REPLAY_VERIFICATION:
            return run_replay_verification;
        case PF_BENCH_RL_SINGLE:
            return run_rl_single;
        case PF_BENCH_RL_BATCH:
            return run_rl_batch;
        default:
            return NULL;
    }
}

static int initialize_benchmark_worlds(
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    const pf_content_view content = make_content();
    const pf_content_view replay_content = pf_m2_replay_make_content();
    size_t environment_index;
    size_t action_index;

    pf_state_bytes = UINT64_C(0);
    if (!make_m4_benchmark_content(
            &pf_benchmark_m4_duel_content,
            &pf_benchmark_m4_duel_view,
            0,
            error) ||
        !make_m4_benchmark_content(
            &pf_benchmark_m4_maximum_content,
            &pf_benchmark_m4_maximum_view,
            1,
            error) ||
        !initialize_sim(
            (size_t)0,
            &content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &pf_duel_sim,
            error) ||
        !initialize_sim(
            (size_t)1,
            &content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &pf_restore_sim,
            error) ||
        !initialize_sim(
            (size_t)2,
            &content,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            &pf_team_sim,
            error) ||
        !initialize_sim(
            (size_t)3,
            &replay_content,
            PF_M2_REPLAY_PLAYERS,
            PF_SIM_MODE_TEAMS,
            &pf_replay_initial_sim,
            error) ||
        !initialize_sim(
            (size_t)4,
            &replay_content,
            PF_M2_REPLAY_PLAYERS,
            PF_SIM_MODE_TEAMS,
            &pf_replay_source_sim,
            error) ||
        !initialize_sim(
            (size_t)5,
            &replay_content,
            PF_M2_REPLAY_PLAYERS,
            PF_SIM_MODE_TEAMS,
            &pf_replay_target_sim,
            error) ||
        !initialize_sim(
            (size_t)70,
            &pf_benchmark_m4_duel_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            &pf_m4_duel_sim,
            error) ||
        !initialize_sim(
            (size_t)71,
            &pf_benchmark_m4_maximum_view,
            PF_SIM_MAX_PLAYERS,
            PF_SIM_MODE_TEAMS,
            &pf_m4_maximum_sim,
            error))
    {
        return 0;
    }

    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_BENCH_RL_ENVIRONMENTS;
         ++environment_index)
    {
        if (!initialize_sim(
                environment_index + (size_t)6,
                &content,
                UINT8_C(2),
                PF_SIM_MODE_DUEL,
                &pf_rl_sims[environment_index],
                error))
        {
            return 0;
        }
        pf_rl_seeds[environment_index] =
            pf_benchmark_descriptors[PF_BENCH_RL_BATCH].seed +
            (uint64_t)environment_index;
    }

    (void)memset(pf_rl_actions, 0, sizeof(pf_rl_actions));
    for (action_index = (size_t)0;
         action_index <
             (size_t)PF_BENCH_RL_ENVIRONMENTS *
                 (size_t)PF_SIM_MAX_PLAYERS;
         ++action_index)
    {
        pf_rl_actions[action_index].schema_version =
            PF_RL_ACTION_SCHEMA_VERSION;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_BENCH_RL_ENVIRONMENTS;
         ++environment_index)
    {
        const size_t action_base =
            environment_index * (size_t)PF_SIM_MAX_PLAYERS;
        pf_rl_actions[action_base].main_stick_x = INT16_C(24576);
        pf_rl_actions[action_base + (size_t)1].main_stick_x =
            INT16_C(-24576);
    }

    if (!prepare_snapshot(error) || !prepare_replay(error) ||
        !validate_m4_representative_workload(error) ||
        !validate_m4_maximum_workload(error))
    {
        return 0;
    }
    return 1;
}

static void sort_doubles(double *values, size_t count)
{
    size_t value_index;

    for (value_index = (size_t)1;
         value_index < count;
         ++value_index)
    {
        const double value = values[value_index];
        size_t insertion_index = value_index;

        while (insertion_index > (size_t)0 &&
               values[insertion_index - (size_t)1] > value)
        {
            values[insertion_index] =
                values[insertion_index - (size_t)1];
            --insertion_index;
        }
        values[insertion_index] = value;
    }
}

static double quantile(const double *sorted, size_t count, double q)
{
    const double position = q * (double)(count - (size_t)1);
    const size_t lower = (size_t)position;
    const size_t upper =
        lower + (size_t)1 < count ? lower + (size_t)1 : lower;
    const double fraction = position - (double)lower;

    return sorted[lower] +
           (sorted[upper] - sorted[lower]) * fraction;
}

static void summarize_result(pf_benchmark_result *result)
{
    double rates[PF_BENCHMARK_MAX_SAMPLES];
    double deviations[PF_BENCHMARK_MAX_SAMPLES];
    double times[PF_BENCHMARK_MAX_SAMPLES];
    size_t sample_index;
    size_t count = (size_t)result->sample_count;

    for (sample_index = (size_t)0;
         sample_index < count;
         ++sample_index)
    {
        rates[sample_index] =
            result->samples[sample_index].rate_per_second;
        times[sample_index] =
            result->samples[sample_index].ns_per_operation;
    }
    sort_doubles(rates, count);
    sort_doubles(times, count);
    result->median_rate = quantile(rates, count, 0.5);
    for (sample_index = (size_t)0;
         sample_index < count;
         ++sample_index)
    {
        const double difference =
            result->samples[sample_index].rate_per_second -
            result->median_rate;
        deviations[sample_index] =
            difference < 0.0 ? -difference : difference;
    }
    sort_doubles(deviations, count);
    result->mad_rate = quantile(deviations, count, 0.5);
    result->p50_ns = quantile(times, count, 0.5);
    result->p95_ns = quantile(times, count, 0.95);
    result->p99_ns = quantile(times, count, 0.99);
}

static int run_measured_scenario(
    pf_benchmark_case benchmark_case,
    uint64_t sample_target_ns,
    uint32_t repetition_count,
    pf_benchmark_result *result,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_benchmark_sample warmup;
    uint64_t iterations = UINT64_C(1);
    uint32_t sample_index;

    for (;;)
    {
        if (!benchmark_case(iterations, &warmup, error))
        {
            return 0;
        }
        if (warmup.elapsed_ns >= sample_target_ns)
        {
            break;
        }
        if (iterations > PF_BENCH_MAX_ITERATIONS / UINT64_C(2))
        {
            set_text_error(error, "benchmark calibration overflow");
            return 0;
        }
        iterations *= UINT64_C(2);
    }

    result->available = UINT8_C(1);
    result->sample_count = (uint8_t)repetition_count;
    result->state_bytes = pf_state_bytes;
    result->snapshot_bytes = (uint64_t)pf_save_size;
    for (sample_index = UINT32_C(0);
         sample_index < repetition_count;
         ++sample_index)
    {
        if (!benchmark_case(
                iterations,
                &result->samples[sample_index],
                error))
        {
            return 0;
        }
        if (result->samples[sample_index].elapsed_ns == UINT64_C(0))
        {
            set_text_error(
                error,
                "measured benchmark sample has zero duration");
            return 0;
        }
    }
    summarize_result(result);
    return 1;
}

size_t pf_benchmark_descriptor_count(void)
{
    return (size_t)PF_BENCHMARK_SCENARIO_COUNT;
}

const pf_benchmark_descriptor *pf_benchmark_descriptor_at(size_t index)
{
    if (index >= pf_benchmark_descriptor_count())
    {
        return NULL;
    }
    return &pf_benchmark_descriptors[index];
}

int pf_benchmark_run_all(
    const char *run_mode,
    uint64_t sample_target_ns,
    uint32_t repetition_count,
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    size_t scenario_index;

    if (run_mode == NULL ||
        (strcmp(run_mode, "commit") != 0 &&
         strcmp(run_mode, "milestone") != 0) ||
        sample_target_ns == UINT64_C(0) ||
        repetition_count == UINT32_C(0) ||
        repetition_count > (uint32_t)PF_BENCHMARK_MAX_SAMPLES ||
        results == NULL ||
        error == NULL)
    {
        if (error != NULL)
        {
            set_text_error(error, "invalid benchmark run arguments");
        }
        return 0;
    }
    error[0] = '\0';
    (void)memset(
        results,
        0,
        sizeof(*results) * (size_t)PF_BENCHMARK_SCENARIO_COUNT);
    if (!initialize_benchmark_worlds(error))
    {
        return 0;
    }

    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        pf_benchmark_case benchmark_case =
            benchmark_case_for_index(scenario_index);
        int scenario_succeeded;

        results[scenario_index].descriptor =
            &pf_benchmark_descriptors[scenario_index];
        if (benchmark_case == NULL)
        {
            results[scenario_index].available = UINT8_C(0);
            continue;
        }
        PF_PROFILE_ZONE_BEGIN(
            scenario_zone,
            "canonical benchmark scenario");
        PF_PROFILE_ZONE_TEXT(
            scenario_zone,
            results[scenario_index].descriptor->name,
            strlen(results[scenario_index].descriptor->name));
        scenario_succeeded = run_measured_scenario(
                benchmark_case,
                sample_target_ns,
                repetition_count,
                &results[scenario_index],
                error);
        PF_PROFILE_ZONE_END(scenario_zone);
        PF_PROFILE_FRAME_MARK();
        if (scenario_succeeded == 0)
        {
            char detail[PF_BENCHMARK_ERROR_CAPACITY];

            (void)snprintf(detail, sizeof(detail), "%s", error);
            (void)snprintf(
                error,
                PF_BENCHMARK_ERROR_CAPACITY,
                "%.48s: %.180s",
                results[scenario_index].descriptor->name,
                detail);
            return 0;
        }
    }
    return 1;
}

int pf_benchmark_run_self_test(
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    size_t scenario_index;
    uint32_t available_count = UINT32_C(0);

    if (!pf_benchmark_run_all(
            "commit",
            UINT64_C(1000000),
            UINT32_C(3),
            results,
            error))
    {
        return 0;
    }
    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        if (results[scenario_index].available != UINT8_C(0))
        {
            if (results[scenario_index].sample_count != UINT8_C(3) ||
                results[scenario_index].median_rate <= 0.0 ||
                results[scenario_index].p99_ns <= 0.0)
            {
                set_text_error(
                    error,
                    "self-test produced an invalid scenario summary");
                return 0;
            }
            ++available_count;
        }
        else if (results[scenario_index]
                     .descriptor
                     ->unavailable_reason_code == NULL)
        {
            set_text_error(
                error,
                "self-test scenario lacks an availability reason");
            return 0;
        }
    }
    if (available_count != UINT32_C(10))
    {
        set_text_error(error, "self-test available scenario count changed");
        return 0;
    }
    return 1;
}
