#ifndef PF_BENCHMARK_H
#define PF_BENCHMARK_H

#include <stddef.h>
#include <stdint.h>

#define PF_BENCHMARK_SCHEMA_VERSION UINT32_C(1)
#define PF_BENCHMARK_SCENARIO_COUNT 13U
#define PF_BENCHMARK_MAX_SAMPLES 15U
#define PF_BENCHMARK_ERROR_CAPACITY 512U

typedef struct pf_benchmark_descriptor
{
    const char *name;
    uint32_t version;
    uint64_t seed;
    const char *unit;
    const char *capability_stage;
    const char *unavailable_reason_code;
    const char *unavailable_details;
} pf_benchmark_descriptor;

typedef struct pf_benchmark_sample
{
    uint64_t iterations;
    uint64_t logical_ticks;
    uint64_t elapsed_ns;
    double rate_per_second;
    double ns_per_operation;
    int64_t checksum;
} pf_benchmark_sample;

typedef struct pf_benchmark_result
{
    const pf_benchmark_descriptor *descriptor;
    uint8_t available;
    uint8_t sample_count;
    uint8_t reserved[6];
    uint64_t state_bytes;
    uint64_t snapshot_bytes;
    pf_benchmark_sample samples[PF_BENCHMARK_MAX_SAMPLES];
    double median_rate;
    double mad_rate;
    double p50_ns;
    double p95_ns;
    double p99_ns;
} pf_benchmark_result;

typedef struct pf_benchmark_metadata
{
    const char *commit_hash;
    int dirty_state;
    const char *run_mode;
    const char *started_utc;
    const char *finished_utc;
    const char *build_configuration;
    const char *compiler;
    const char *compiler_flags;
    const char *dependency_hash;
    const char *content_hash;
    const char *machine_fingerprint;
    const char *os_fingerprint;
    const char *cpu_fingerprint;
    const char *power_metadata;
    const char *thermal_metadata;
    const char *executable_hash;
    uint64_t sample_target_ns;
    uint32_t repetition_count;
} pf_benchmark_metadata;

typedef struct pf_benchmark_history_paths
{
    const char *database_path;
    const char *schema_path;
    const char *graph_directory;
    const char *manifest_path;
} pf_benchmark_history_paths;

typedef struct pf_benchmark_history_outcome
{
    int64_t run_id;
    uint32_t available_count;
    uint32_t unavailable_count;
    uint32_t baseline_count;
    uint32_t compatible_count;
    uint32_t invalid_comparisons;
    uint32_t suspected_regressions;
    uint32_t confirmed_regressions;
} pf_benchmark_history_outcome;

size_t pf_benchmark_descriptor_count(void);
const pf_benchmark_descriptor *pf_benchmark_descriptor_at(size_t index);

int pf_benchmark_run_all(
    const char *run_mode,
    uint64_t sample_target_ns,
    uint32_t repetition_count,
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    char error[PF_BENCHMARK_ERROR_CAPACITY]);

int pf_benchmark_run_self_test(
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    char error[PF_BENCHMARK_ERROR_CAPACITY]);

int pf_benchmark_history_persist(
    const pf_benchmark_metadata *metadata,
    const pf_benchmark_history_paths *paths,
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    pf_benchmark_history_outcome *outcome,
    char error[PF_BENCHMARK_ERROR_CAPACITY]);

#endif
