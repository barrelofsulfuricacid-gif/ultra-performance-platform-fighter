#include "pf/rl.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>
#include <time.h>

#define PF_HEADLESS_MEMORY_BYTES 2048U
#define PF_HEADLESS_MEMORY_ALIGNMENT 64U
#define PF_HEADLESS_ENVIRONMENTS 64U
#define PF_HEADLESS_MAX_SAMPLES 7U

typedef struct pf_headless_storage
{
    alignas(PF_HEADLESS_MEMORY_ALIGNMENT)
        uint8_t state[PF_HEADLESS_MEMORY_BYTES];
    alignas(PF_HEADLESS_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_HEADLESS_MEMORY_BYTES];
} pf_headless_storage;

static pf_headless_storage pf_headless_storage_pool[
    PF_HEADLESS_ENVIRONMENTS];
static pf_sim *pf_headless_sims[PF_HEADLESS_ENVIRONMENTS];
static uint64_t pf_headless_seeds[PF_HEADLESS_ENVIRONMENTS];
static pf_rl_action pf_headless_actions[
    PF_HEADLESS_ENVIRONMENTS * PF_SIM_MAX_PLAYERS];
static pf_rl_transition pf_headless_transitions[
    PF_HEADLESS_ENVIRONMENTS];
static pf_state_hash pf_headless_single_hashes[
    PF_HEADLESS_ENVIRONMENTS];

static double monotonic_sample_seconds(void)
{
    struct timespec value;

    if (timespec_get(&value, TIME_UTC) != TIME_UTC)
    {
        return 0.0;
    }
    return (double)value.tv_sec +
           (double)value.tv_nsec / 1000000000.0;
}

static pf_content_view make_benchmark_content(void)
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

static int initialize_throughput_environments(void)
{
    const pf_content_view content = make_benchmark_content();
    pf_sim_config config;
    pf_memory_requirements requirements;
    size_t environment_index;
    size_t action_index;

    if (pf_sim_default_config(
            &config,
            UINT8_C(2),
            PF_SIM_MODE_DUEL) != PF_STATUS_OK)
    {
        return 0;
    }
    config.max_ticks = UINT64_C(1000000000);
    if (pf_sim_query_memory(&config, &requirements) != PF_STATUS_OK ||
        requirements.state_bytes > (size_t)PF_HEADLESS_MEMORY_BYTES ||
        requirements.scratch_bytes > (size_t)PF_HEADLESS_MEMORY_BYTES ||
        requirements.state_alignment >
            (size_t)PF_HEADLESS_MEMORY_ALIGNMENT ||
        requirements.scratch_alignment >
            (size_t)PF_HEADLESS_MEMORY_ALIGNMENT)
    {
        return 0;
    }

    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_HEADLESS_ENVIRONMENTS;
         ++environment_index)
    {
        pf_headless_sims[environment_index] = NULL;
        pf_headless_seeds[environment_index] =
            UINT64_C(9000) + (uint64_t)environment_index;
        if (pf_sim_init(
                pf_headless_storage_pool[environment_index].state,
                sizeof(
                    pf_headless_storage_pool[environment_index].state),
                pf_headless_storage_pool[environment_index].scratch,
                sizeof(
                    pf_headless_storage_pool[environment_index].scratch),
                &content,
                &config,
                &pf_headless_sims[environment_index]) != PF_STATUS_OK)
        {
            return 0;
        }
    }

    (void)memset(
        pf_headless_actions,
        0,
        sizeof(pf_headless_actions));
    for (action_index = (size_t)0;
         action_index <
             (size_t)PF_HEADLESS_ENVIRONMENTS *
                 (size_t)PF_SIM_MAX_PLAYERS;
         ++action_index)
    {
        pf_headless_actions[action_index].schema_version =
            PF_RL_ACTION_SCHEMA_VERSION;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_HEADLESS_ENVIRONMENTS;
         ++environment_index)
    {
        const size_t action_base =
            environment_index * (size_t)PF_SIM_MAX_PLAYERS;
        pf_headless_actions[action_base].main_stick_x =
            INT16_C(24576);
        pf_headless_actions[action_base + (size_t)1].main_stick_x =
            INT16_C(-24576);
    }
    return 1;
}

static int reset_throughput_environments(void)
{
    return pf_rl_reset_batch(
               pf_headless_sims,
               pf_headless_seeds,
               (size_t)PF_HEADLESS_ENVIRONMENTS,
               pf_headless_transitions) == PF_STATUS_OK;
}

static double run_throughput_case(size_t rounds, int batched)
{
    const double started = monotonic_sample_seconds();
    double finished;
    size_t round_index;

    if (started == 0.0)
    {
        return 0.0;
    }
    for (round_index = (size_t)0;
         round_index < rounds;
         ++round_index)
    {
        if (batched != 0)
        {
            if (pf_rl_step_batch(
                    pf_headless_sims,
                    (size_t)PF_HEADLESS_ENVIRONMENTS,
                    pf_headless_actions,
                    (size_t)PF_SIM_MAX_PLAYERS,
                    pf_headless_transitions) != PF_STATUS_OK)
            {
                return 0.0;
            }
        }
        else
        {
            size_t environment_index;

            for (environment_index = (size_t)0;
                 environment_index <
                     (size_t)PF_HEADLESS_ENVIRONMENTS;
                 ++environment_index)
            {
                const size_t action_base =
                    environment_index *
                    (size_t)PF_SIM_MAX_PLAYERS;
                if (pf_rl_step(
                        pf_headless_sims[environment_index],
                        &pf_headless_actions[action_base],
                        (size_t)2,
                        &pf_headless_transitions[environment_index]) !=
                    PF_STATUS_OK)
                {
                    return 0.0;
                }
            }
        }
    }

    finished = monotonic_sample_seconds();
    if (finished <= started)
    {
        return 0.0;
    }
    return finished - started;
}

static void sort_samples(double *samples, size_t sample_count)
{
    size_t sample_index;

    for (sample_index = (size_t)1;
         sample_index < sample_count;
         ++sample_index)
    {
        const double value = samples[sample_index];
        size_t insertion_index = sample_index;

        while (insertion_index > (size_t)0 &&
               samples[insertion_index - (size_t)1] > value)
        {
            samples[insertion_index] =
                samples[insertion_index - (size_t)1];
            --insertion_index;
        }
        samples[insertion_index] = value;
    }
}

static int throughput_states_match(size_t rounds)
{
    size_t environment_index;

    if (!reset_throughput_environments() ||
        run_throughput_case(rounds, 0) <= 0.0)
    {
        return 0;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_HEADLESS_ENVIRONMENTS;
         ++environment_index)
    {
        if (pf_sim_hash(
                pf_headless_sims[environment_index],
                &pf_headless_single_hashes[environment_index]) !=
            PF_STATUS_OK)
        {
            return 0;
        }
    }

    if (!reset_throughput_environments() ||
        run_throughput_case(rounds, 1) <= 0.0)
    {
        return 0;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)PF_HEADLESS_ENVIRONMENTS;
         ++environment_index)
    {
        pf_state_hash batch_hash;

        if (pf_sim_hash(
                pf_headless_sims[environment_index],
                &batch_hash) != PF_STATUS_OK ||
            memcmp(
                pf_headless_single_hashes[environment_index].bytes,
                batch_hash.bytes,
                sizeof(batch_hash.bytes)) != 0)
        {
            return 0;
        }
    }
    return 1;
}

static int run_throughput(int smoke)
{
    const double calibration_target = smoke != 0 ? 0.01 : 0.10;
    const size_t sample_count =
        smoke != 0 ? (size_t)3 : (size_t)PF_HEADLESS_MAX_SAMPLES;
    double single_samples[PF_HEADLESS_MAX_SAMPLES];
    double batch_samples[PF_HEADLESS_MAX_SAMPLES];
    double calibration_elapsed;
    double single_seconds;
    double batch_seconds;
    double single_tps;
    double batch_tps;
    double speedup;
    size_t rounds = (size_t)16;
    size_t sample_index;
    uint64_t total_ticks;

    if (!initialize_throughput_environments())
    {
        (void)fprintf(
            stderr,
            "headless-throughput=fail reason=initialize\n");
        return 1;
    }

    for (;;)
    {
        if (!reset_throughput_environments())
        {
            return 1;
        }
        calibration_elapsed = run_throughput_case(rounds, 1);
        if (calibration_elapsed >= calibration_target)
        {
            break;
        }
        if (calibration_elapsed <= 0.0 ||
            rounds > SIZE_MAX / (size_t)2)
        {
            (void)fprintf(
                stderr,
                "headless-throughput=fail reason=calibration\n");
            return 1;
        }
        rounds *= (size_t)2;
    }

    for (sample_index = (size_t)0;
         sample_index < sample_count;
         ++sample_index)
    {
        if (!reset_throughput_environments())
        {
            return 1;
        }
        single_samples[sample_index] =
            run_throughput_case(rounds, 0);
        if (!reset_throughput_environments())
        {
            return 1;
        }
        batch_samples[sample_index] =
            run_throughput_case(rounds, 1);
        if (single_samples[sample_index] <= 0.0 ||
            batch_samples[sample_index] <= 0.0)
        {
            return 1;
        }
    }
    sort_samples(single_samples, sample_count);
    sort_samples(batch_samples, sample_count);
    single_seconds = single_samples[sample_count / (size_t)2];
    batch_seconds = batch_samples[sample_count / (size_t)2];

    if (rounds >
        (size_t)(UINT64_MAX /
                 (uint64_t)PF_HEADLESS_ENVIRONMENTS))
    {
        return 1;
    }
    total_ticks =
        (uint64_t)rounds * (uint64_t)PF_HEADLESS_ENVIRONMENTS;
    single_tps = (double)total_ticks / single_seconds;
    batch_tps = (double)total_ticks / batch_seconds;
    speedup = batch_tps / single_tps;

    if (!throughput_states_match(rounds))
    {
        (void)fprintf(
            stderr,
            "headless-throughput=fail reason=state-mismatch\n");
        return 1;
    }

    (void)printf(
        "headless-throughput=pass environments=%u rounds=%zu"
        " single_tps=%.0f batch_tps=%.0f"
        " boundary_speedup=%.4f state_match=1\n",
        (unsigned int)PF_HEADLESS_ENVIRONMENTS,
        rounds,
        single_tps,
        batch_tps,
        speedup);
    return 0;
}

static int run_smoke(void)
{
    uint32_t abi_version = pf_sim_abi_version();
    uint32_t tick_rate_hz = pf_sim_tick_rate_hz();

    if (abi_version != PF_SIM_ABI_VERSION ||
        tick_rate_hz != PF_SIM_TICK_RATE_HZ)
    {
        (void)fprintf(stderr, "headless-smoke=fail\n");
        return 1;
    }

    (void)printf(
        "headless-smoke=pass sim_abi=%" PRIu32 " tick_hz=%" PRIu32 "\n",
        abi_version,
        tick_rate_hz);
    return 0;
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && strcmp(arguments[1], "--smoke") == 0)
    {
        return run_smoke();
    }
    if (argument_count == 2 &&
        strcmp(arguments[1], "--throughput-smoke") == 0)
    {
        return run_throughput(1);
    }
    if (argument_count == 2 &&
        strcmp(arguments[1], "--throughput") == 0)
    {
        return run_throughput(0);
    }

    (void)fprintf(
        stderr,
        "usage: headless --smoke|--throughput-smoke|--throughput\n");
    return 2;
}
