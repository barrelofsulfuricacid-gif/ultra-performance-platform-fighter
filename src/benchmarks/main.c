#include "benchmark.h"

#include "pf/sim.h"
#include "sqlite3.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef PF_BENCHMARK_SCHEMA_PATH
#error "PF_BENCHMARK_SCHEMA_PATH must identify the versioned SQL schema"
#endif

#define PF_COMMIT_SAMPLE_TARGET_NS UINT64_C(20000000)
#define PF_MILESTONE_SAMPLE_TARGET_NS UINT64_C(100000000)
#define PF_COMMIT_REPETITIONS UINT32_C(5)
#define PF_MILESTONE_REPETITIONS UINT32_C(15)

static const char pf_sqlite_source_hash[] =
    "bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc";
static const char pf_dependency_hash[] =
    "sqlite-3.53.4:1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d";
static const char pf_content_hash[] =
    "1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526";

static const char *environment_or_default(
    const char *name,
    const char *fallback)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' ? value : fallback;
}

static int require_environment(
    const char *name,
    const char **value,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    *value = getenv(name);
    if (*value == NULL || (*value)[0] == '\0')
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "required environment variable is missing: %s",
            name);
        return 0;
    }
    return 1;
}

static int is_lower_hex_commit(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != (size_t)40)
    {
        return 0;
    }
    for (index = (size_t)0; index < (size_t)40; ++index)
    {
        if (!((value[index] >= '0' && value[index] <= '9') ||
              (value[index] >= 'a' && value[index] <= 'f')))
        {
            return 0;
        }
    }
    return 1;
}

static int parse_u64(
    const char *text,
    uint64_t minimum,
    uint64_t maximum,
    uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || text[0] == '\0' || text[0] == '-')
    {
        return 0;
    }
    parsed = strtoull(text, &end, 10);
    if (end == text || *end != '\0' ||
        parsed < (unsigned long long)minimum ||
        parsed > (unsigned long long)maximum)
    {
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

static int utc_now(
    char output[32],
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    time_t current = time(NULL);
    struct tm utc;

    if (current == (time_t)-1)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "cannot read current UTC time");
        return 0;
    }
#if defined(_WIN32)
    if (gmtime_s(&utc, &current) != 0)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "cannot convert current UTC time");
        return 0;
    }
#else
    if (gmtime_r(&current, &utc) == NULL)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "cannot convert current UTC time");
        return 0;
    }
#endif
    if (strftime(
            output,
            (size_t)32,
            "%Y-%m-%dT%H:%M:%SZ",
            &utc) == (size_t)0)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "cannot format current UTC time");
        return 0;
    }
    return 1;
}

static int sqlite_is_locked_version(void)
{
    return strcmp(sqlite3_libversion(), "3.53.4") == 0 &&
           strstr(sqlite3_sourceid(), pf_sqlite_source_hash) != NULL;
}

static const char *benchmark_process_status(
    const pf_benchmark_history_outcome *outcome)
{
    if (outcome->confirmed_regressions != UINT32_C(0))
    {
        return "regression";
    }
    if (outcome->suspected_regressions != UINT32_C(0))
    {
        return "suspected";
    }
    return "pass";
}

static int benchmark_process_exit_code(
    const pf_benchmark_history_outcome *outcome)
{
    return outcome->confirmed_regressions == UINT32_C(0) ? 0 : 3;
}

static int run_self_test(void)
{
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT];
    pf_benchmark_history_outcome outcome;
    char error[PF_BENCHMARK_ERROR_CAPACITY];

    if (!sqlite_is_locked_version())
    {
        (void)fprintf(
            stderr,
            "benchmarks-self-test=fail reason=sqlite-version\n");
        return 1;
    }
    if (!pf_benchmark_run_self_test(results, error))
    {
        (void)fprintf(
            stderr,
            "benchmarks-self-test=fail reason=%s\n",
            error);
        return 1;
    }
    (void)memset(&outcome, 0, sizeof(outcome));
    if (strcmp(benchmark_process_status(&outcome), "pass") != 0 ||
        benchmark_process_exit_code(&outcome) != 0)
    {
        (void)fprintf(
            stderr,
            "benchmarks-self-test=fail reason=clean-status-policy\n");
        return 1;
    }
    outcome.suspected_regressions = UINT32_C(1);
    if (strcmp(benchmark_process_status(&outcome), "suspected") != 0 ||
        benchmark_process_exit_code(&outcome) != 0)
    {
        (void)fprintf(
            stderr,
            "benchmarks-self-test=fail reason=suspected-status-policy\n");
        return 1;
    }
    outcome.confirmed_regressions = UINT32_C(1);
    if (strcmp(benchmark_process_status(&outcome), "regression") != 0 ||
        benchmark_process_exit_code(&outcome) != 3)
    {
        (void)fprintf(
            stderr,
            "benchmarks-self-test=fail reason=confirmed-status-policy\n");
        return 1;
    }
    (void)printf(
        "benchmarks-self-test=pass scenarios=%u available=9 "
        "sqlite=%s schema=%" PRIu32 "\n",
        (unsigned int)PF_BENCHMARK_SCENARIO_COUNT,
        sqlite3_libversion(),
        PF_BENCHMARK_SCHEMA_VERSION);
    return 0;
}

static int run_profile_workload(void)
{
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT];
    char error[PF_BENCHMARK_ERROR_CAPACITY];

    if (!pf_benchmark_run_all(
            "milestone",
            PF_MILESTONE_SAMPLE_TARGET_NS,
            PF_MILESTONE_REPETITIONS,
            results,
            error))
    {
        (void)fprintf(
            stderr,
            "profile-workload=fail reason=%s\n",
            error);
        return 1;
    }
    (void)printf(
        "profile-workload=pass scenarios=13 available=9 "
        "sample_target_ns=%" PRIu64 " repetitions=%" PRIu32 "\n",
        PF_MILESTONE_SAMPLE_TARGET_NS,
        PF_MILESTONE_REPETITIONS);
    return 0;
}

static void make_synthetic_results(
    uint32_t repetition_count,
    double rate,
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT])
{
    static const double offsets[PF_BENCHMARK_MAX_SAMPLES] = {
        -0.003,
        0.002,
        -0.001,
        0.001,
        0.0,
        0.003,
        -0.002,
        0.0015,
        -0.0015,
        0.0025,
        -0.0025,
        0.0005,
        -0.0005,
        0.001,
        -0.001,
    };
    size_t scenario_index;

    (void)memset(
        results,
        0,
        sizeof(*results) * (size_t)PF_BENCHMARK_SCENARIO_COUNT);
    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        const pf_benchmark_descriptor *descriptor =
            pf_benchmark_descriptor_at(scenario_index);
        uint32_t sample_index;

        results[scenario_index].descriptor = descriptor;
        if (descriptor->unavailable_reason_code != NULL)
        {
            continue;
        }
        results[scenario_index].available = UINT8_C(1);
        results[scenario_index].sample_count =
            (uint8_t)repetition_count;
        results[scenario_index].state_bytes = UINT64_C(512);
        results[scenario_index].snapshot_bytes = UINT64_C(305);
        results[scenario_index].median_rate = rate;
        results[scenario_index].mad_rate = rate * 0.001;
        results[scenario_index].p50_ns = 1000000000.0 / rate;
        results[scenario_index].p95_ns =
            results[scenario_index].p50_ns * 1.003;
        results[scenario_index].p99_ns =
            results[scenario_index].p50_ns * 1.004;
        for (sample_index = UINT32_C(0);
             sample_index < repetition_count;
             ++sample_index)
        {
            pf_benchmark_sample *sample =
                &results[scenario_index].samples[sample_index];
            sample->iterations = UINT64_C(1000);
            sample->logical_ticks = UINT64_C(1000);
            sample->elapsed_ns = UINT64_C(1000000000);
            sample->rate_per_second =
                rate * (1.0 + offsets[sample_index]);
            sample->ns_per_operation =
                1000000000.0 / sample->rate_per_second;
            sample->checksum =
                INT64_C(1000) + (int64_t)scenario_index;
        }
    }
}

static void make_synthetic_metadata(
    const char *commit_hash,
    const char *run_mode,
    uint64_t sample_target_ns,
    uint32_t repetition_count,
    pf_benchmark_metadata *metadata)
{
    (void)memset(metadata, 0, sizeof(*metadata));
    metadata->commit_hash = commit_hash;
    metadata->dirty_state = 0;
    metadata->run_mode = run_mode;
    metadata->started_utc = "2026-07-27T00:00:00Z";
    metadata->finished_utc = "2026-07-27T00:00:01Z";
    metadata->build_configuration = "qualification-release";
    metadata->compiler = "qualification-c17";
    metadata->compiler_flags = "-O3 -std=c17";
    metadata->dependency_hash = pf_dependency_hash;
    metadata->content_hash = pf_content_hash;
    metadata->machine_fingerprint = "qualification-machine";
    metadata->os_fingerprint = "qualification-os";
    metadata->cpu_fingerprint = "qualification-cpu";
    metadata->power_metadata = "qualification-fixed";
    metadata->thermal_metadata = "qualification-fixed";
    metadata->executable_hash = "qualification-executable";
    metadata->sample_target_ns = sample_target_ns;
    metadata->repetition_count = repetition_count;
}

static int persist_synthetic_run(
    const pf_benchmark_history_paths *paths,
    const char *commit_hash,
    const char *compiler,
    const char *run_mode,
    uint64_t sample_target_ns,
    uint32_t repetition_count,
    double rate,
    pf_benchmark_history_outcome *outcome,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    pf_benchmark_metadata metadata;
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT];

    make_synthetic_metadata(
        commit_hash,
        run_mode,
        sample_target_ns,
        repetition_count,
        &metadata);
    metadata.compiler = compiler;
    make_synthetic_results(repetition_count, rate, results);
    return pf_benchmark_history_persist(
        &metadata,
        paths,
        results,
        outcome,
        error);
}

static int run_history_qualification(
    const char *database_path,
    const char *graph_directory,
    const char *manifest_path)
{
    static const char baseline_commit[] =
        "1111111111111111111111111111111111111111";
    static const char suspected_commit[] =
        "2222222222222222222222222222222222222222";
    static const char milestone_baseline_commit[] =
        "3333333333333333333333333333333333333333";
    static const char confirmed_commit[] =
        "4444444444444444444444444444444444444444";
    static const char incompatible_commit[] =
        "5555555555555555555555555555555555555555";
    pf_benchmark_history_paths paths;
    pf_benchmark_history_outcome baseline;
    pf_benchmark_history_outcome same_commit_repeat;
    pf_benchmark_history_outcome suspected;
    pf_benchmark_history_outcome milestone_baseline;
    pf_benchmark_history_outcome confirmed;
    pf_benchmark_history_outcome incompatible;
    char error[PF_BENCHMARK_ERROR_CAPACITY];

    paths.database_path = database_path;
    paths.schema_path = PF_BENCHMARK_SCHEMA_PATH;
    paths.graph_directory = graph_directory;
    paths.manifest_path = manifest_path;
    error[0] = '\0';

    if (!persist_synthetic_run(
            &paths,
            baseline_commit,
            "qualification-c17",
            "commit",
            PF_COMMIT_SAMPLE_TARGET_NS,
            PF_COMMIT_REPETITIONS,
            1000000.0,
            &baseline,
            error) ||
        baseline.suspected_regressions != UINT32_C(0) ||
        !persist_synthetic_run(
            &paths,
            baseline_commit,
            "qualification-c17",
            "commit",
            PF_COMMIT_SAMPLE_TARGET_NS,
            PF_COMMIT_REPETITIONS,
            1000000.0,
            &same_commit_repeat,
            error) ||
        same_commit_repeat.compatible_count != UINT32_C(9) ||
        !persist_synthetic_run(
            &paths,
            suspected_commit,
            "qualification-c17",
            "commit",
            PF_COMMIT_SAMPLE_TARGET_NS,
            PF_COMMIT_REPETITIONS,
            900000.0,
            &suspected,
            error) ||
        suspected.suspected_regressions != UINT32_C(9) ||
        !persist_synthetic_run(
            &paths,
            milestone_baseline_commit,
            "qualification-c17",
            "milestone",
            PF_MILESTONE_SAMPLE_TARGET_NS,
            PF_MILESTONE_REPETITIONS,
            1000000.0,
            &milestone_baseline,
            error) ||
        milestone_baseline.confirmed_regressions != UINT32_C(0) ||
        !persist_synthetic_run(
            &paths,
            confirmed_commit,
            "qualification-c17",
            "milestone",
            PF_MILESTONE_SAMPLE_TARGET_NS,
            PF_MILESTONE_REPETITIONS,
            900000.0,
            &confirmed,
            error) ||
        confirmed.confirmed_regressions != UINT32_C(9) ||
        !persist_synthetic_run(
            &paths,
            incompatible_commit,
            "qualification-c18",
            "commit",
            PF_COMMIT_SAMPLE_TARGET_NS,
            PF_COMMIT_REPETITIONS,
            1000000.0,
            &incompatible,
            error) ||
        incompatible.invalid_comparisons != UINT32_C(9))
    {
        (void)fprintf(
            stderr,
            "benchmark-history-qualification=fail reason=%s\n",
            error[0] != '\0' ? error : "unexpected comparison outcome");
        return 1;
    }
    (void)printf(
        "benchmark-history-qualification=pass baseline=%" PRId64
        " same_commit=%" PRIu32
        " suspected=%" PRIu32 " confirmed=%" PRIu32
        " invalid=%" PRIu32 "\n",
        baseline.run_id,
        same_commit_repeat.compatible_count,
        suspected.suspected_regressions,
        confirmed.confirmed_regressions,
        incompatible.invalid_comparisons);
    return 0;
}

static int configure_metadata(
    const char *run_mode,
    pf_benchmark_metadata *metadata,
    pf_benchmark_history_paths *paths,
    char started_utc[32],
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    const char *dirty_text;
    const char *target_override;
    const char *repetition_override;
    uint64_t parsed;

    (void)memset(metadata, 0, sizeof(*metadata));
    (void)memset(paths, 0, sizeof(*paths));
    if (!utc_now(started_utc, error) ||
        !require_environment(
            "PF_PERF_COMMIT",
            &metadata->commit_hash,
            error) ||
        !require_environment(
            "PF_PERF_DIRTY",
            &dirty_text,
            error) ||
        !require_environment(
            "PF_PERF_COMPILER",
            &metadata->compiler,
            error) ||
        !require_environment(
            "PF_PERF_COMPILER_FLAGS",
            &metadata->compiler_flags,
            error) ||
        !require_environment(
            "PF_PERF_MACHINE_FINGERPRINT",
            &metadata->machine_fingerprint,
            error) ||
        !require_environment(
            "PF_PERF_OS_FINGERPRINT",
            &metadata->os_fingerprint,
            error) ||
        !require_environment(
            "PF_PERF_CPU_FINGERPRINT",
            &metadata->cpu_fingerprint,
            error) ||
        !require_environment(
            "PF_PERF_EXECUTABLE_HASH",
            &metadata->executable_hash,
            error) ||
        !require_environment(
            "PF_PERF_DATABASE",
            &paths->database_path,
            error) ||
        !require_environment(
            "PF_PERF_GRAPH_DIRECTORY",
            &paths->graph_directory,
            error) ||
        !require_environment(
            "PF_PERF_MANIFEST",
            &paths->manifest_path,
            error))
    {
        return 0;
    }
    if (!is_lower_hex_commit(metadata->commit_hash) ||
        (strcmp(dirty_text, "0") != 0 && strcmp(dirty_text, "1") != 0))
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "commit hash or dirty-state metadata is invalid");
        return 0;
    }

    metadata->dirty_state = dirty_text[0] == '1' ? 1 : 0;
    metadata->run_mode = run_mode;
    metadata->started_utc = started_utc;
    metadata->build_configuration = environment_or_default(
        "PF_PERF_BUILD_CONFIGURATION",
        "benchmark-release");
    metadata->dependency_hash = environment_or_default(
        "PF_PERF_DEPENDENCY_HASH",
        pf_dependency_hash);
    metadata->content_hash = environment_or_default(
        "PF_PERF_CONTENT_HASH",
        pf_content_hash);
    metadata->power_metadata = environment_or_default(
        "PF_PERF_POWER_METADATA",
        "unavailable:not-supplied");
    metadata->thermal_metadata = environment_or_default(
        "PF_PERF_THERMAL_METADATA",
        "unavailable:not-supplied");
    paths->schema_path = environment_or_default(
        "PF_PERF_SCHEMA",
        PF_BENCHMARK_SCHEMA_PATH);

    metadata->sample_target_ns =
        strcmp(run_mode, "milestone") == 0
            ? PF_MILESTONE_SAMPLE_TARGET_NS
            : PF_COMMIT_SAMPLE_TARGET_NS;
    metadata->repetition_count =
        strcmp(run_mode, "milestone") == 0
            ? PF_MILESTONE_REPETITIONS
            : PF_COMMIT_REPETITIONS;

    target_override = getenv("PF_PERF_SAMPLE_TARGET_NS");
    if (target_override != NULL && target_override[0] != '\0')
    {
        if (!parse_u64(
                target_override,
                UINT64_C(1000000),
                UINT64_C(10000000000),
                &metadata->sample_target_ns))
        {
            (void)snprintf(
                error,
                PF_BENCHMARK_ERROR_CAPACITY,
                "PF_PERF_SAMPLE_TARGET_NS is invalid");
            return 0;
        }
    }
    repetition_override = getenv("PF_PERF_REPETITIONS");
    if (repetition_override != NULL &&
        repetition_override[0] != '\0')
    {
        if (!parse_u64(
                repetition_override,
                UINT64_C(3),
                (uint64_t)PF_BENCHMARK_MAX_SAMPLES,
                &parsed))
        {
            (void)snprintf(
                error,
                PF_BENCHMARK_ERROR_CAPACITY,
                "PF_PERF_REPETITIONS is invalid");
            return 0;
        }
        metadata->repetition_count = (uint32_t)parsed;
    }
    return 1;
}

static int run_benchmarks(const char *run_mode)
{
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT];
    pf_benchmark_metadata metadata;
    pf_benchmark_history_paths paths;
    pf_benchmark_history_outcome outcome;
    char started_utc[32];
    char finished_utc[32];
    char error[PF_BENCHMARK_ERROR_CAPACITY];
    size_t scenario_index;

    error[0] = '\0';
    if (!sqlite_is_locked_version())
    {
        (void)fprintf(
            stderr,
            "benchmarks=fail reason=sqlite-version\n");
        return 1;
    }
    if (!configure_metadata(
            run_mode,
            &metadata,
            &paths,
            started_utc,
            error))
    {
        (void)fprintf(stderr, "benchmarks=fail reason=%s\n", error);
        return 1;
    }
    if (!pf_benchmark_run_all(
            run_mode,
            metadata.sample_target_ns,
            metadata.repetition_count,
            results,
            error))
    {
        (void)fprintf(stderr, "benchmarks=fail reason=%s\n", error);
        return 1;
    }
    if (!utc_now(finished_utc, error))
    {
        (void)fprintf(stderr, "benchmarks=fail reason=%s\n", error);
        return 1;
    }
    metadata.finished_utc = finished_utc;
    if (!pf_benchmark_history_persist(
            &metadata,
            &paths,
            results,
            &outcome,
            error))
    {
        (void)fprintf(stderr, "benchmarks=fail reason=%s\n", error);
        return 1;
    }

    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        if (results[scenario_index].available != UINT8_C(0))
        {
            (void)printf(
                "scenario=%s median_rate=%.0f mad_rate=%.0f "
                "p95_ns=%.3f\n",
                results[scenario_index].descriptor->name,
                results[scenario_index].median_rate,
                results[scenario_index].mad_rate,
                results[scenario_index].p95_ns);
        }
        else
        {
            (void)printf(
                "scenario=%s unavailable=%s\n",
                results[scenario_index].descriptor->name,
                results[scenario_index]
                    .descriptor
                    ->unavailable_reason_code);
        }
    }
    (void)printf(
        "benchmarks=%s schema=%" PRIu32 " run_id=%" PRId64
        " available=%" PRIu32 " unavailable=%" PRIu32
        " invalid_comparisons=%" PRIu32
        " suspected_regressions=%" PRIu32
        " confirmed_regressions=%" PRIu32 "\n",
        benchmark_process_status(&outcome),
        PF_BENCHMARK_SCHEMA_VERSION,
        outcome.run_id,
        outcome.available_count,
        outcome.unavailable_count,
        outcome.invalid_comparisons,
        outcome.suspected_regressions,
        outcome.confirmed_regressions);
    return benchmark_process_exit_code(&outcome);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 &&
        strcmp(arguments[1], "--smoke") == 0)
    {
        (void)printf(
            "benchmarks-smoke=pass sim_abi=%" PRIu32
            " tick_hz=%" PRIu32 "\n",
            pf_sim_abi_version(),
            pf_sim_tick_rate_hz());
        return 0;
    }
    if (argument_count == 2 &&
        strcmp(arguments[1], "--self-test") == 0)
    {
        return run_self_test();
    }
    if (argument_count == 2 &&
        strcmp(arguments[1], "--profile-workload") == 0)
    {
        return run_profile_workload();
    }
    if (argument_count == 3 &&
        strcmp(arguments[1], "--run") == 0 &&
        (strcmp(arguments[2], "commit") == 0 ||
         strcmp(arguments[2], "milestone") == 0))
    {
        return run_benchmarks(arguments[2]);
    }
    if (argument_count == 5 &&
        strcmp(arguments[1], "--qualify-history") == 0)
    {
        return run_history_qualification(
            arguments[2],
            arguments[3],
            arguments[4]);
    }

    (void)fprintf(
        stderr,
        "usage: pf_benchmarks --self-test|--profile-workload|"
        "--run commit|milestone|"
        "--qualify-history DATABASE GRAPH_DIR MANIFEST\n");
    return 2;
}
