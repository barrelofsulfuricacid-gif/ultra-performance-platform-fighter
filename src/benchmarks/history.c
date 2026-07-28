#include "benchmark.h"

#include "sqlite3.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PF_HISTORY_BOOTSTRAP_ROUNDS 2000U
#define PF_HISTORY_MAX_GRAPH_POINTS 512U

typedef struct pf_history_comparison
{
    int64_t baseline_run_id;
    double relative_change;
    double noise_floor;
    double meaningful_threshold;
    double confidence_low;
    double confidence_high;
    const char *status;
    const char *reason;
    uint8_t has_baseline;
    uint8_t has_confidence;
} pf_history_comparison;

typedef struct pf_graph_point
{
    char commit_hash[41];
    char comparison_status[32];
    double median_rate;
} pf_graph_point;

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

static void set_sqlite_error(
    char error[PF_BENCHMARK_ERROR_CAPACITY],
    sqlite3 *database,
    const char *operation)
{
    (void)snprintf(
        error,
        PF_BENCHMARK_ERROR_CAPACITY,
        "%s: %s",
        operation,
        database != NULL ? sqlite3_errmsg(database) : "no database");
}

static int execute_sql(
    sqlite3 *database,
    const char *sql,
    const char *operation,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    char *sqlite_error = NULL;
    int status = sqlite3_exec(
        database,
        sql,
        NULL,
        NULL,
        &sqlite_error);

    if (status != SQLITE_OK)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "%s: %s",
            operation,
            sqlite_error != NULL ? sqlite_error : sqlite3_errmsg(database));
        sqlite3_free(sqlite_error);
        return 0;
    }
    return 1;
}

static char *read_text_file(
    const char *path,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    FILE *file;
    long length;
    size_t bytes_read;
    char *text;

    file = fopen(path, "rb");
    if (file == NULL)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "cannot open schema: %s",
            path);
        return NULL;
    }
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        (void)fclose(file);
        set_text_error(error, "cannot seek performance schema");
        return NULL;
    }
    length = ftell(file);
    if (length <= 0L || fseek(file, 0L, SEEK_SET) != 0)
    {
        (void)fclose(file);
        set_text_error(error, "performance schema is empty or unreadable");
        return NULL;
    }
    text = (char *)malloc((size_t)length + (size_t)1);
    if (text == NULL)
    {
        (void)fclose(file);
        set_text_error(error, "cannot allocate performance schema buffer");
        return NULL;
    }
    bytes_read = fread(text, (size_t)1, (size_t)length, file);
    if (fclose(file) != 0 || bytes_read != (size_t)length)
    {
        free(text);
        set_text_error(error, "cannot read complete performance schema");
        return NULL;
    }
    text[bytes_read] = '\0';
    return text;
}

static int prepare_statement(
    sqlite3 *database,
    const char *sql,
    sqlite3_stmt **statement,
    const char *operation,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    if (sqlite3_prepare_v2(
            database,
            sql,
            -1,
            statement,
            NULL) != SQLITE_OK)
    {
        set_sqlite_error(error, database, operation);
        return 0;
    }
    return 1;
}

static int bind_text(
    sqlite3 *database,
    sqlite3_stmt *statement,
    int index,
    const char *value,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    if (sqlite3_bind_text(
            statement,
            index,
            value,
            -1,
            SQLITE_TRANSIENT) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "bind SQLite text");
        return 0;
    }
    return 1;
}

static int step_done(
    sqlite3 *database,
    sqlite3_stmt *statement,
    const char *operation,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        set_sqlite_error(error, database, operation);
        return 0;
    }
    return 1;
}

static int verify_schema(
    sqlite3 *database,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3_stmt *statement = NULL;
    int valid = 0;

    if (!prepare_statement(
            database,
            "SELECT value FROM schema_metadata "
            "WHERE key = 'schema_version';",
            &statement,
            "prepare schema-version query",
            error))
    {
        return 0;
    }
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        const unsigned char *value = sqlite3_column_text(statement, 0);
        valid = value != NULL &&
                strcmp((const char *)value, "1") == 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize schema-version query");
        return 0;
    }
    if (!valid)
    {
        set_text_error(error, "performance database schema is not version 1");
        return 0;
    }
    return 1;
}

static int insert_run(
    sqlite3 *database,
    const pf_benchmark_metadata *metadata,
    int64_t *run_id,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    static const char sql[] =
        "INSERT INTO benchmark_runs("
        "commit_hash, dirty_state, run_mode, status, started_utc, "
        "benchmark_schema_version, build_configuration, compiler, "
        "compiler_flags, dependency_hash, content_hash, "
        "machine_fingerprint, os_fingerprint, cpu_fingerprint, "
        "power_metadata, thermal_metadata, executable_hash, "
        "sample_target_ns, repetition_count) "
        "VALUES(?, ?, ?, 'running', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?);";
    sqlite3_stmt *statement = NULL;
    int ok;

    if (!prepare_statement(
            database,
            sql,
            &statement,
            "prepare benchmark run insert",
            error))
    {
        return 0;
    }
    ok = bind_text(database, statement, 1, metadata->commit_hash, error) &&
         sqlite3_bind_int(statement, 2, metadata->dirty_state) == SQLITE_OK &&
         bind_text(database, statement, 3, metadata->run_mode, error) &&
         bind_text(database, statement, 4, metadata->started_utc, error) &&
         sqlite3_bind_int(
             statement,
             5,
             (int)PF_BENCHMARK_SCHEMA_VERSION) == SQLITE_OK &&
         bind_text(
             database,
             statement,
             6,
             metadata->build_configuration,
             error) &&
         bind_text(database, statement, 7, metadata->compiler, error) &&
         bind_text(
             database,
             statement,
             8,
             metadata->compiler_flags,
             error) &&
         bind_text(
             database,
             statement,
             9,
             metadata->dependency_hash,
             error) &&
         bind_text(
             database,
             statement,
             10,
             metadata->content_hash,
             error) &&
         bind_text(
             database,
             statement,
             11,
             metadata->machine_fingerprint,
             error) &&
         bind_text(
             database,
             statement,
             12,
             metadata->os_fingerprint,
             error) &&
         bind_text(
             database,
             statement,
             13,
             metadata->cpu_fingerprint,
             error) &&
         bind_text(
             database,
             statement,
             14,
             metadata->power_metadata,
             error) &&
         bind_text(
             database,
             statement,
             15,
             metadata->thermal_metadata,
             error) &&
         bind_text(
             database,
             statement,
             16,
             metadata->executable_hash,
             error) &&
         sqlite3_bind_int64(
             statement,
             17,
             (sqlite3_int64)metadata->sample_target_ns) == SQLITE_OK &&
         sqlite3_bind_int(
             statement,
             18,
             (int)metadata->repetition_count) == SQLITE_OK;
    if (!ok)
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "bind benchmark run insert");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (!step_done(
            database,
            statement,
            "insert benchmark run",
            error))
    {
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize benchmark run insert");
        return 0;
    }
    *run_id = (int64_t)sqlite3_last_insert_rowid(database);
    return *run_id > INT64_C(0);
}

static int resolve_scenario_id(
    sqlite3 *database,
    const pf_benchmark_descriptor *descriptor,
    int64_t *scenario_id,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3_stmt *statement = NULL;
    int ok;

    if (!prepare_statement(
            database,
            "INSERT OR IGNORE INTO benchmark_scenarios("
            "name, scenario_version, seed, unit, capability_stage) "
            "VALUES(?, ?, ?, ?, ?);",
            &statement,
            "prepare benchmark scenario insert",
            error))
    {
        return 0;
    }
    ok = bind_text(database, statement, 1, descriptor->name, error) &&
         sqlite3_bind_int(
             statement,
             2,
             (int)descriptor->version) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             3,
             (sqlite3_int64)descriptor->seed) == SQLITE_OK &&
         bind_text(database, statement, 4, descriptor->unit, error) &&
         bind_text(
             database,
             statement,
             5,
             descriptor->capability_stage,
             error);
    if (!ok ||
        !step_done(
            database,
            statement,
            "insert benchmark scenario",
            error))
    {
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize scenario insert");
        return 0;
    }

    if (!prepare_statement(
            database,
            "SELECT id FROM benchmark_scenarios "
            "WHERE name = ? AND scenario_version = ? AND seed = ?;",
            &statement,
            "prepare benchmark scenario query",
            error))
    {
        return 0;
    }
    ok = bind_text(database, statement, 1, descriptor->name, error) &&
         sqlite3_bind_int(
             statement,
             2,
             (int)descriptor->version) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             3,
             (sqlite3_int64)descriptor->seed) == SQLITE_OK;
    if (!ok || sqlite3_step(statement) != SQLITE_ROW)
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "query benchmark scenario");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    *scenario_id = (int64_t)sqlite3_column_int64(statement, 0);
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize scenario query");
        return 0;
    }
    return *scenario_id > INT64_C(0);
}

static int insert_samples(
    sqlite3 *database,
    int64_t run_id,
    int64_t scenario_id,
    const pf_benchmark_result *result,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    static const char sql[] =
        "INSERT INTO benchmark_samples("
        "run_id, scenario_id, sample_index, iterations, logical_ticks, "
        "elapsed_ns, rate_per_second, ns_per_operation, checksum) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *statement = NULL;
    uint32_t sample_index;

    if (!prepare_statement(
            database,
            sql,
            &statement,
            "prepare benchmark sample insert",
            error))
    {
        return 0;
    }
    for (sample_index = UINT32_C(0);
         sample_index < (uint32_t)result->sample_count;
         ++sample_index)
    {
        const pf_benchmark_sample *sample =
            &result->samples[sample_index];
        int ok = sqlite3_bind_int64(
                     statement,
                     1,
                     (sqlite3_int64)run_id) == SQLITE_OK &&
                 sqlite3_bind_int64(
                     statement,
                     2,
                     (sqlite3_int64)scenario_id) == SQLITE_OK &&
                 sqlite3_bind_int(
                     statement,
                     3,
                     (int)sample_index) == SQLITE_OK &&
                 sqlite3_bind_int64(
                     statement,
                     4,
                     (sqlite3_int64)sample->iterations) == SQLITE_OK &&
                 sqlite3_bind_int64(
                     statement,
                     5,
                     (sqlite3_int64)sample->logical_ticks) == SQLITE_OK &&
                 sqlite3_bind_int64(
                     statement,
                     6,
                     (sqlite3_int64)sample->elapsed_ns) == SQLITE_OK &&
                 sqlite3_bind_double(
                     statement,
                     7,
                     sample->rate_per_second) == SQLITE_OK &&
                 sqlite3_bind_double(
                     statement,
                     8,
                     sample->ns_per_operation) == SQLITE_OK &&
                 sqlite3_bind_int64(
                     statement,
                     9,
                     (sqlite3_int64)sample->checksum) == SQLITE_OK;
        if (!ok ||
            !step_done(
                database,
                statement,
                "insert benchmark sample",
                error))
        {
            if (error[0] == '\0')
            {
                set_sqlite_error(error, database, "bind benchmark sample");
            }
            (void)sqlite3_finalize(statement);
            return 0;
        }
        if (sqlite3_reset(statement) != SQLITE_OK ||
            sqlite3_clear_bindings(statement) != SQLITE_OK)
        {
            set_sqlite_error(error, database, "reset benchmark sample insert");
            (void)sqlite3_finalize(statement);
            return 0;
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize benchmark sample insert");
        return 0;
    }
    return 1;
}

static int bind_compatibility(
    sqlite3 *database,
    sqlite3_stmt *statement,
    int first_index,
    const pf_benchmark_metadata *metadata,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    return sqlite3_bind_int(
               statement,
               first_index,
               metadata->dirty_state) == SQLITE_OK &&
           bind_text(
               database,
               statement,
               first_index + 1,
               metadata->run_mode,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 2,
               metadata->build_configuration,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 3,
               metadata->compiler,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 4,
               metadata->compiler_flags,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 5,
               metadata->dependency_hash,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 6,
               metadata->content_hash,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 7,
               metadata->machine_fingerprint,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 8,
               metadata->os_fingerprint,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 9,
               metadata->cpu_fingerprint,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 10,
               metadata->power_metadata,
               error) &&
           bind_text(
               database,
               statement,
               first_index + 11,
               metadata->thermal_metadata,
               error) &&
           sqlite3_bind_int(
               statement,
               first_index + 12,
               (int)PF_BENCHMARK_SCHEMA_VERSION) == SQLITE_OK &&
           sqlite3_bind_int64(
               statement,
               first_index + 13,
               (sqlite3_int64)metadata->sample_target_ns) == SQLITE_OK &&
           sqlite3_bind_int(
               statement,
               first_index + 14,
               (int)metadata->repetition_count) == SQLITE_OK;
}

static int prior_scenario_exists(
    sqlite3 *database,
    int64_t run_id,
    int64_t scenario_id,
    int *exists,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3_stmt *statement = NULL;
    int status;

    if (!prepare_statement(
            database,
            "SELECT 1 FROM benchmark_summaries "
            "WHERE scenario_id = ? AND run_id < ? LIMIT 1;",
            &statement,
            "prepare prior-scenario query",
            error))
    {
        return 0;
    }
    if (sqlite3_bind_int64(
            statement,
            1,
            (sqlite3_int64)scenario_id) != SQLITE_OK ||
        sqlite3_bind_int64(
            statement,
            2,
            (sqlite3_int64)run_id) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "bind prior-scenario query");
        (void)sqlite3_finalize(statement);
        return 0;
    }
    status = sqlite3_step(statement);
    *exists = status == SQLITE_ROW;
    if (status != SQLITE_ROW && status != SQLITE_DONE)
    {
        set_sqlite_error(error, database, "query prior scenario");
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize prior-scenario query");
        return 0;
    }
    return 1;
}

static int find_compatible_baseline(
    sqlite3 *database,
    const pf_benchmark_metadata *metadata,
    int64_t run_id,
    int64_t scenario_id,
    int64_t *baseline_run_id,
    double *baseline_median,
    double *baseline_mad,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    static const char sql[] =
        "SELECT s.run_id, s.median_rate, s.mad_rate "
        "FROM benchmark_summaries AS s "
        "JOIN benchmark_runs AS r ON r.id = s.run_id "
        "WHERE s.scenario_id = ? AND s.run_id < ? "
        "AND r.dirty_state = ? AND r.run_mode = ? "
        "AND r.build_configuration = ? AND r.compiler = ? "
        "AND r.compiler_flags = ? AND r.dependency_hash = ? "
        "AND r.content_hash = ? AND r.machine_fingerprint = ? "
        "AND r.os_fingerprint = ? AND r.cpu_fingerprint = ? "
        "AND r.power_metadata = ? AND r.thermal_metadata = ? "
        "AND r.benchmark_schema_version = ? "
        "AND r.sample_target_ns = ? AND r.repetition_count = ? "
        "AND r.status = 'pass' "
        "ORDER BY s.run_id DESC LIMIT 1;";
    sqlite3_stmt *statement = NULL;
    int status;
    int ok;

    *baseline_run_id = INT64_C(0);
    if (!prepare_statement(
            database,
            sql,
            &statement,
            "prepare compatible-baseline query",
            error))
    {
        return 0;
    }
    ok = sqlite3_bind_int64(
             statement,
             1,
             (sqlite3_int64)scenario_id) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             2,
             (sqlite3_int64)run_id) == SQLITE_OK &&
         bind_compatibility(database, statement, 3, metadata, error);
    if (!ok)
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "bind baseline query");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    status = sqlite3_step(statement);
    if (status == SQLITE_ROW)
    {
        *baseline_run_id =
            (int64_t)sqlite3_column_int64(statement, 0);
        *baseline_median = sqlite3_column_double(statement, 1);
        *baseline_mad = sqlite3_column_double(statement, 2);
    }
    else if (status != SQLITE_DONE)
    {
        set_sqlite_error(error, database, "query compatible baseline");
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize baseline query");
        return 0;
    }
    return 1;
}

static int load_baseline_samples(
    sqlite3 *database,
    int64_t baseline_run_id,
    int64_t scenario_id,
    double samples[PF_BENCHMARK_MAX_SAMPLES],
    size_t *sample_count,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3_stmt *statement = NULL;
    int status;

    *sample_count = (size_t)0;
    if (!prepare_statement(
            database,
            "SELECT rate_per_second FROM benchmark_samples "
            "WHERE run_id = ? AND scenario_id = ? "
            "ORDER BY sample_index;",
            &statement,
            "prepare baseline sample query",
            error))
    {
        return 0;
    }
    if (sqlite3_bind_int64(
            statement,
            1,
            (sqlite3_int64)baseline_run_id) != SQLITE_OK ||
        sqlite3_bind_int64(
            statement,
            2,
            (sqlite3_int64)scenario_id) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "bind baseline sample query");
        (void)sqlite3_finalize(statement);
        return 0;
    }
    for (;;)
    {
        status = sqlite3_step(statement);
        if (status == SQLITE_DONE)
        {
            break;
        }
        if (status != SQLITE_ROW ||
            *sample_count >= (size_t)PF_BENCHMARK_MAX_SAMPLES)
        {
            set_sqlite_error(error, database, "query baseline samples");
            (void)sqlite3_finalize(statement);
            return 0;
        }
        samples[*sample_count] = sqlite3_column_double(statement, 0);
        ++(*sample_count);
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize baseline sample query");
        return 0;
    }
    if (*sample_count == (size_t)0)
    {
        set_text_error(error, "compatible baseline has no raw samples");
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

static uint64_t next_random(uint64_t *state)
{
    uint64_t value = *state;

    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return value * UINT64_C(0x2545f4914f6cdd1d);
}

static void bootstrap_interval(
    const pf_benchmark_result *current,
    const double baseline[PF_BENCHMARK_MAX_SAMPLES],
    size_t baseline_count,
    int64_t run_id,
    int64_t baseline_run_id,
    double *confidence_low,
    double *confidence_high)
{
    double changes[PF_HISTORY_BOOTSTRAP_ROUNDS];
    uint64_t random_state =
        (uint64_t)run_id ^
        ((uint64_t)baseline_run_id << 1U) ^
        UINT64_C(0x6a09e667f3bcc909);
    uint32_t round;
    size_t current_count = (size_t)current->sample_count;

    for (round = UINT32_C(0);
         round < (uint32_t)PF_HISTORY_BOOTSTRAP_ROUNDS;
         ++round)
    {
        double current_draw[PF_BENCHMARK_MAX_SAMPLES];
        double baseline_draw[PF_BENCHMARK_MAX_SAMPLES];
        size_t sample_index;
        double current_median;
        double baseline_median;

        for (sample_index = (size_t)0;
             sample_index < current_count;
             ++sample_index)
        {
            size_t selected =
                (size_t)(next_random(&random_state) %
                         (uint64_t)current_count);
            current_draw[sample_index] =
                current->samples[selected].rate_per_second;
        }
        for (sample_index = (size_t)0;
             sample_index < baseline_count;
             ++sample_index)
        {
            size_t selected =
                (size_t)(next_random(&random_state) %
                         (uint64_t)baseline_count);
            baseline_draw[sample_index] = baseline[selected];
        }
        sort_doubles(current_draw, current_count);
        sort_doubles(baseline_draw, baseline_count);
        current_median = quantile(current_draw, current_count, 0.5);
        baseline_median = quantile(baseline_draw, baseline_count, 0.5);
        changes[round] =
            baseline_median > 0.0
                ? (current_median - baseline_median) / baseline_median
                : 0.0;
    }
    sort_doubles(
        changes,
        (size_t)PF_HISTORY_BOOTSTRAP_ROUNDS);
    *confidence_low = quantile(
        changes,
        (size_t)PF_HISTORY_BOOTSTRAP_ROUNDS,
        0.025);
    *confidence_high = quantile(
        changes,
        (size_t)PF_HISTORY_BOOTSTRAP_ROUNDS,
        0.975);
}

static int compare_result(
    sqlite3 *database,
    const pf_benchmark_metadata *metadata,
    int64_t run_id,
    int64_t scenario_id,
    const pf_benchmark_result *result,
    pf_history_comparison *comparison,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    int64_t baseline_run_id;
    double baseline_median = 0.0;
    double baseline_mad = 0.0;
    int has_prior = 0;

    (void)memset(comparison, 0, sizeof(*comparison));
    comparison->status = "baseline";
    comparison->reason = "first-compatible-measurement";

    if (metadata->dirty_state != 0)
    {
        comparison->status = "invalid";
        comparison->reason = "dirty-tree-measurement";
        return 1;
    }
    if (!find_compatible_baseline(
            database,
            metadata,
            run_id,
            scenario_id,
            &baseline_run_id,
            &baseline_median,
            &baseline_mad,
            error))
    {
        return 0;
    }
    if (baseline_run_id == INT64_C(0))
    {
        if (!prior_scenario_exists(
                database,
                run_id,
                scenario_id,
                &has_prior,
                error))
        {
            return 0;
        }
        if (has_prior != 0)
        {
            comparison->status = "invalid";
            comparison->reason =
                "prior-measurements-have-incompatible-metadata";
        }
        return 1;
    }
    if (baseline_median <= 0.0 || result->median_rate <= 0.0)
    {
        comparison->status = "invalid";
        comparison->reason = "non-positive-summary-rate";
        return 1;
    }

    comparison->has_baseline = UINT8_C(1);
    comparison->baseline_run_id = baseline_run_id;
    comparison->relative_change =
        (result->median_rate - baseline_median) / baseline_median;
    comparison->noise_floor =
        result->mad_rate / result->median_rate;
    if (baseline_mad / baseline_median > comparison->noise_floor)
    {
        comparison->noise_floor = baseline_mad / baseline_median;
    }
    comparison->meaningful_threshold =
        3.0 * comparison->noise_floor;
    if (comparison->meaningful_threshold < 0.01)
    {
        comparison->meaningful_threshold = 0.01;
    }
    comparison->status = "compatible";
    comparison->reason = "within-relative-non-regression-envelope";

    if (strcmp(metadata->run_mode, "milestone") == 0)
    {
        double baseline_samples[PF_BENCHMARK_MAX_SAMPLES];
        size_t baseline_count;

        if (!load_baseline_samples(
                database,
                baseline_run_id,
                scenario_id,
                baseline_samples,
                &baseline_count,
                error))
        {
            return 0;
        }
        bootstrap_interval(
            result,
            baseline_samples,
            baseline_count,
            run_id,
            baseline_run_id,
            &comparison->confidence_low,
            &comparison->confidence_high);
        comparison->has_confidence = UINT8_C(1);
        if (comparison->relative_change <=
                -comparison->meaningful_threshold &&
            comparison->confidence_high < 0.0)
        {
            comparison->status = "confirmed_regression";
            comparison->reason =
                "milestone-confidence-interval-excludes-zero";
        }
    }
    else if (comparison->relative_change <=
             -comparison->meaningful_threshold)
    {
        comparison->status = "suspected_regression";
        comparison->reason =
            "commit-change-exceeds-relative-regression-threshold";
    }
    return 1;
}

static int insert_summary(
    sqlite3 *database,
    int64_t run_id,
    int64_t scenario_id,
    const pf_benchmark_result *result,
    const pf_history_comparison *comparison,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    static const char sql[] =
        "INSERT INTO benchmark_summaries("
        "run_id, scenario_id, sample_count, median_rate, mad_rate, "
        "p50_ns, p95_ns, p99_ns, state_bytes, snapshot_bytes, "
        "baseline_run_id, relative_change, relative_noise_floor, "
        "meaningful_threshold, confidence_low, confidence_high, "
        "comparison_status, comparison_reason) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *statement = NULL;
    int ok;

    if (!prepare_statement(
            database,
            sql,
            &statement,
            "prepare benchmark summary insert",
            error))
    {
        return 0;
    }
    ok = sqlite3_bind_int64(
             statement,
             1,
             (sqlite3_int64)run_id) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             2,
             (sqlite3_int64)scenario_id) == SQLITE_OK &&
         sqlite3_bind_int(
             statement,
             3,
             (int)result->sample_count) == SQLITE_OK &&
         sqlite3_bind_double(
             statement,
             4,
             result->median_rate) == SQLITE_OK &&
         sqlite3_bind_double(
             statement,
             5,
             result->mad_rate) == SQLITE_OK &&
         sqlite3_bind_double(statement, 6, result->p50_ns) == SQLITE_OK &&
         sqlite3_bind_double(statement, 7, result->p95_ns) == SQLITE_OK &&
         sqlite3_bind_double(statement, 8, result->p99_ns) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             9,
             (sqlite3_int64)result->state_bytes) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             10,
             (sqlite3_int64)result->snapshot_bytes) == SQLITE_OK;
    if (ok && comparison->has_baseline != UINT8_C(0))
    {
        ok = sqlite3_bind_int64(
                 statement,
                 11,
                 (sqlite3_int64)comparison->baseline_run_id) == SQLITE_OK &&
             sqlite3_bind_double(
                 statement,
                 12,
                 comparison->relative_change) == SQLITE_OK &&
             sqlite3_bind_double(
                 statement,
                 13,
                 comparison->noise_floor) == SQLITE_OK &&
             sqlite3_bind_double(
                 statement,
                 14,
                 comparison->meaningful_threshold) == SQLITE_OK;
    }
    else if (ok)
    {
        ok = sqlite3_bind_null(statement, 11) == SQLITE_OK &&
             sqlite3_bind_null(statement, 12) == SQLITE_OK &&
             sqlite3_bind_null(statement, 13) == SQLITE_OK &&
             sqlite3_bind_null(statement, 14) == SQLITE_OK;
    }
    if (ok && comparison->has_confidence != UINT8_C(0))
    {
        ok = sqlite3_bind_double(
                 statement,
                 15,
                 comparison->confidence_low) == SQLITE_OK &&
             sqlite3_bind_double(
                 statement,
                 16,
                 comparison->confidence_high) == SQLITE_OK;
    }
    else if (ok)
    {
        ok = sqlite3_bind_null(statement, 15) == SQLITE_OK &&
             sqlite3_bind_null(statement, 16) == SQLITE_OK;
    }
    ok = ok &&
         bind_text(
             database,
             statement,
             17,
             comparison->status,
             error) &&
         bind_text(
             database,
             statement,
             18,
             comparison->reason,
             error);
    if (!ok ||
        !step_done(
            database,
            statement,
            "insert benchmark summary",
            error))
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "bind benchmark summary");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize summary insert");
        return 0;
    }
    return 1;
}

static int insert_unavailable(
    sqlite3 *database,
    int64_t run_id,
    int64_t scenario_id,
    const pf_benchmark_descriptor *descriptor,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3_stmt *statement = NULL;
    int ok;

    if (!prepare_statement(
            database,
            "INSERT INTO benchmark_unavailable("
            "run_id, scenario_id, reason_code, details) "
            "VALUES(?, ?, ?, ?);",
            &statement,
            "prepare unavailable scenario insert",
            error))
    {
        return 0;
    }
    ok = sqlite3_bind_int64(
             statement,
             1,
             (sqlite3_int64)run_id) == SQLITE_OK &&
         sqlite3_bind_int64(
             statement,
             2,
             (sqlite3_int64)scenario_id) == SQLITE_OK &&
         bind_text(
             database,
             statement,
             3,
             descriptor->unavailable_reason_code,
             error) &&
         bind_text(
             database,
             statement,
             4,
             descriptor->unavailable_details,
             error);
    if (!ok ||
        !step_done(
            database,
            statement,
            "insert unavailable scenario",
            error))
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "bind unavailable scenario");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize unavailable insert");
        return 0;
    }
    return 1;
}

static int update_run_status(
    sqlite3 *database,
    int64_t run_id,
    const char *status,
    const char *finished_utc,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3_stmt *statement = NULL;
    int ok;

    if (!prepare_statement(
            database,
            "UPDATE benchmark_runs "
            "SET status = ?, finished_utc = ? WHERE id = ?;",
            &statement,
            "prepare benchmark run update",
            error))
    {
        return 0;
    }
    ok = bind_text(database, statement, 1, status, error) &&
         bind_text(database, statement, 2, finished_utc, error) &&
         sqlite3_bind_int64(
             statement,
             3,
             (sqlite3_int64)run_id) == SQLITE_OK;
    if (!ok ||
        !step_done(
            database,
            statement,
            "update benchmark run",
            error))
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "bind benchmark run update");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize benchmark run update");
        return 0;
    }
    return 1;
}

static int load_graph_points(
    sqlite3 *database,
    const pf_benchmark_metadata *metadata,
    int64_t scenario_id,
    pf_graph_point points[PF_HISTORY_MAX_GRAPH_POINTS],
    size_t *point_count,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    static const char sql[] =
        "SELECT r.commit_hash, s.median_rate, s.comparison_status "
        "FROM benchmark_summaries AS s "
        "JOIN benchmark_runs AS r ON r.id = s.run_id "
        "WHERE s.scenario_id = ? "
        "AND r.dirty_state = ? AND r.run_mode = ? "
        "AND r.build_configuration = ? AND r.compiler = ? "
        "AND r.compiler_flags = ? AND r.dependency_hash = ? "
        "AND r.content_hash = ? AND r.machine_fingerprint = ? "
        "AND r.os_fingerprint = ? AND r.cpu_fingerprint = ? "
        "AND r.power_metadata = ? AND r.thermal_metadata = ? "
        "AND r.benchmark_schema_version = ? "
        "AND r.sample_target_ns = ? AND r.repetition_count = ? "
        "ORDER BY r.id;";
    sqlite3_stmt *statement = NULL;
    int ok;

    *point_count = (size_t)0;
    if (!prepare_statement(
            database,
            sql,
            &statement,
            "prepare performance graph query",
            error))
    {
        return 0;
    }
    ok = sqlite3_bind_int64(
             statement,
             1,
             (sqlite3_int64)scenario_id) == SQLITE_OK &&
         bind_compatibility(database, statement, 2, metadata, error);
    if (!ok)
    {
        if (error[0] == '\0')
        {
            set_sqlite_error(error, database, "bind graph query");
        }
        (void)sqlite3_finalize(statement);
        return 0;
    }
    for (;;)
    {
        int status = sqlite3_step(statement);
        const unsigned char *commit;
        const unsigned char *comparison_status;
        size_t existing_index;

        if (status == SQLITE_DONE)
        {
            break;
        }
        if (status != SQLITE_ROW)
        {
            set_sqlite_error(error, database, "query performance graph");
            (void)sqlite3_finalize(statement);
            return 0;
        }
        commit = sqlite3_column_text(statement, 0);
        comparison_status = sqlite3_column_text(statement, 2);
        if (commit == NULL || comparison_status == NULL)
        {
            set_text_error(error, "graph query returned null metadata");
            (void)sqlite3_finalize(statement);
            return 0;
        }
        for (existing_index = (size_t)0;
             existing_index < *point_count;
             ++existing_index)
        {
            if (strcmp(
                    points[existing_index].commit_hash,
                    (const char *)commit) == 0)
            {
                break;
            }
        }
        if (existing_index == *point_count)
        {
            if (*point_count >= (size_t)PF_HISTORY_MAX_GRAPH_POINTS)
            {
                set_text_error(error, "performance graph point limit reached");
                (void)sqlite3_finalize(statement);
                return 0;
            }
            ++(*point_count);
        }
        (void)snprintf(
            points[existing_index].commit_hash,
            sizeof(points[existing_index].commit_hash),
            "%.40s",
            (const char *)commit);
        (void)snprintf(
            points[existing_index].comparison_status,
            sizeof(points[existing_index].comparison_status),
            "%.31s",
            (const char *)comparison_status);
        points[existing_index].median_rate =
            sqlite3_column_double(statement, 1);
    }
    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "finalize graph query");
        return 0;
    }
    return 1;
}

static const char *point_color(const char *status)
{
    if (strcmp(status, "confirmed_regression") == 0)
    {
        return "#dc2626";
    }
    if (strcmp(status, "suspected_regression") == 0)
    {
        return "#f97316";
    }
    if (strcmp(status, "invalid") == 0)
    {
        return "#94a3b8";
    }
    return "#2563eb";
}

static int write_graph(
    const char *graph_directory,
    const pf_benchmark_descriptor *descriptor,
    const pf_graph_point points[PF_HISTORY_MAX_GRAPH_POINTS],
    size_t point_count,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    char path[1024];
    FILE *file;
    double minimum = 0.0;
    double maximum = 0.0;
    size_t point_index;

    if (snprintf(
            path,
            sizeof(path),
            "%s/%s.svg",
            graph_directory,
            descriptor->name) < 0)
    {
        set_text_error(error, "cannot format performance graph path");
        return 0;
    }
    file = fopen(path, "wb");
    if (file == NULL)
    {
        (void)snprintf(
            error,
            PF_BENCHMARK_ERROR_CAPACITY,
            "cannot create graph: %.400s",
            path);
        return 0;
    }
    if (point_count > (size_t)0)
    {
        minimum = points[0].median_rate;
        maximum = points[0].median_rate;
        for (point_index = (size_t)1;
             point_index < point_count;
             ++point_index)
        {
            if (points[point_index].median_rate < minimum)
            {
                minimum = points[point_index].median_rate;
            }
            if (points[point_index].median_rate > maximum)
            {
                maximum = points[point_index].median_rate;
            }
        }
        if (maximum <= minimum)
        {
            maximum = minimum + (minimum > 0.0 ? minimum * 0.05 : 1.0);
            minimum = minimum > 0.0 ? minimum * 0.95 : 0.0;
        }
    }

    (void)fprintf(
        file,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"960\" height=\"480\" viewBox=\"0 0 960 480\">\n"
        "<title>%s performance evolution</title>\n"
        "<desc>SQLite-backed compatible measurements keyed by commit.</desc>\n"
        "<rect width=\"960\" height=\"480\" fill=\"#f8fafc\"/>\n"
        "<text x=\"48\" y=\"38\" font-family=\"sans-serif\" "
        "font-size=\"22\" fill=\"#0f172a\">%s</text>\n"
        "<text x=\"48\" y=\"62\" font-family=\"sans-serif\" "
        "font-size=\"13\" fill=\"#475569\">%s</text>\n"
        "<line x1=\"72\" y1=\"410\" x2=\"930\" y2=\"410\" "
        "stroke=\"#64748b\"/>\n"
        "<line x1=\"72\" y1=\"84\" x2=\"72\" y2=\"410\" "
        "stroke=\"#64748b\"/>\n",
        descriptor->name,
        descriptor->name,
        descriptor->unit);
    if (point_count == (size_t)0)
    {
        const char *message =
            descriptor->unavailable_details != NULL
                ? descriptor->unavailable_details
                : "No compatible measurements are recorded yet.";
        (void)fprintf(
            file,
            "<text x=\"110\" y=\"240\" font-family=\"sans-serif\" "
            "font-size=\"16\" fill=\"#64748b\">%s</text>\n",
            message);
    }
    else
    {
        (void)fprintf(
            file,
            "<text x=\"10\" y=\"94\" font-family=\"monospace\" "
            "font-size=\"11\" fill=\"#475569\">%.3g</text>\n"
            "<text x=\"10\" y=\"410\" font-family=\"monospace\" "
            "font-size=\"11\" fill=\"#475569\">%.3g</text>\n"
            "<polyline fill=\"none\" stroke=\"#2563eb\" "
            "stroke-width=\"2\" points=\"",
            maximum,
            minimum);
        for (point_index = (size_t)0;
             point_index < point_count;
             ++point_index)
        {
            const double x =
                point_count == (size_t)1
                    ? 501.0
                    : 72.0 +
                          858.0 * (double)point_index /
                              (double)(point_count - (size_t)1);
            const double y =
                410.0 -
                326.0 *
                    (points[point_index].median_rate - minimum) /
                    (maximum - minimum);
            (void)fprintf(file, "%.2f,%.2f ", x, y);
        }
        (void)fprintf(file, "\"/>\n");
        for (point_index = (size_t)0;
             point_index < point_count;
             ++point_index)
        {
            const double x =
                point_count == (size_t)1
                    ? 501.0
                    : 72.0 +
                          858.0 * (double)point_index /
                              (double)(point_count - (size_t)1);
            const double y =
                410.0 -
                326.0 *
                    (points[point_index].median_rate - minimum) /
                    (maximum - minimum);
            (void)fprintf(
                file,
                "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"4\" fill=\"%s\">"
                "<title>%.40s %.6g %s</title></circle>\n",
                x,
                y,
                point_color(points[point_index].comparison_status),
                points[point_index].commit_hash,
                points[point_index].median_rate,
                descriptor->unit);
        }
        (void)fprintf(
            file,
            "<text x=\"72\" y=\"438\" font-family=\"monospace\" "
            "font-size=\"11\" fill=\"#475569\">%.8s</text>\n"
            "<text x=\"865\" y=\"438\" font-family=\"monospace\" "
            "font-size=\"11\" fill=\"#475569\">%.8s</text>\n",
            points[0].commit_hash,
            points[point_count - (size_t)1].commit_hash);
    }
    (void)fprintf(file, "</svg>\n");
    if (fclose(file) != 0)
    {
        set_text_error(error, "cannot finish performance graph");
        return 0;
    }
    return 1;
}

static int generate_graphs(
    sqlite3 *database,
    const pf_benchmark_metadata *metadata,
    const char *graph_directory,
    const int64_t scenario_ids[PF_BENCHMARK_SCENARIO_COUNT],
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    char index_path[1024];
    FILE *index_file;
    size_t scenario_index;

    if (snprintf(
            index_path,
            sizeof(index_path),
            "%s/index.md",
            graph_directory) < 0)
    {
        set_text_error(error, "cannot format graph index path");
        return 0;
    }
    index_file = fopen(index_path, "wb");
    if (index_file == NULL)
    {
        set_text_error(error, "cannot create performance graph index");
        return 0;
    }
    (void)fprintf(
        index_file,
        "# Local performance graphs\n\n"
        "Commit: `%s`\n\n"
        "Compatibility series: `%s`, `%s`, `%s`\n\n",
        metadata->commit_hash,
        metadata->build_configuration,
        metadata->machine_fingerprint,
        metadata->compiler);

    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        pf_graph_point points[PF_HISTORY_MAX_GRAPH_POINTS];
        size_t point_count;
        const pf_benchmark_descriptor *descriptor =
            pf_benchmark_descriptor_at(scenario_index);

        if (!load_graph_points(
                database,
                metadata,
                scenario_ids[scenario_index],
                points,
                &point_count,
                error) ||
            !write_graph(
                graph_directory,
                descriptor,
                points,
                point_count,
                error))
        {
            (void)fclose(index_file);
            return 0;
        }
        (void)fprintf(
            index_file,
            "- [%s](%s.svg) — %zu compatible commit%s\n",
            descriptor->name,
            descriptor->name,
            point_count,
            point_count == (size_t)1 ? "" : "s");
    }
    if (fclose(index_file) != 0)
    {
        set_text_error(error, "cannot finish performance graph index");
        return 0;
    }
    return 1;
}

static int write_manifest(
    const pf_benchmark_metadata *metadata,
    const pf_benchmark_history_paths *paths,
    const pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    const pf_history_comparison comparisons[PF_BENCHMARK_SCENARIO_COUNT],
    const pf_benchmark_history_outcome *outcome,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    FILE *file = fopen(paths->manifest_path, "wb");
    size_t scenario_index;

    if (file == NULL)
    {
        set_text_error(error, "cannot create performance manifest");
        return 0;
    }
    (void)fprintf(
        file,
        "schema=%" PRIu32 "\n"
        "run_id=%" PRId64 "\n"
        "commit=%s\n"
        "dirty=%d\n"
        "mode=%s\n"
        "database=%s\n"
        "graphs=%s\n"
        "available=%" PRIu32 "\n"
        "unavailable=%" PRIu32 "\n"
        "baselines=%" PRIu32 "\n"
        "compatible_comparisons=%" PRIu32 "\n"
        "invalid_comparisons=%" PRIu32 "\n"
        "suspected_regressions=%" PRIu32 "\n"
        "confirmed_regressions=%" PRIu32 "\n",
        PF_BENCHMARK_SCHEMA_VERSION,
        outcome->run_id,
        metadata->commit_hash,
        metadata->dirty_state,
        metadata->run_mode,
        paths->database_path,
        paths->graph_directory,
        outcome->available_count,
        outcome->unavailable_count,
        outcome->baseline_count,
        outcome->compatible_count,
        outcome->invalid_comparisons,
        outcome->suspected_regressions,
        outcome->confirmed_regressions);
    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        const pf_benchmark_result *result = &results[scenario_index];

        if (result->available != UINT8_C(0))
        {
            (void)fprintf(
                file,
                "scenario.%s=measured median_rate=%.9g "
                "mad_rate=%.9g comparison=%s reason=%s\n",
                result->descriptor->name,
                result->median_rate,
                result->mad_rate,
                comparisons[scenario_index].status,
                comparisons[scenario_index].reason);
        }
        else
        {
            (void)fprintf(
                file,
                "scenario.%s=unavailable reason=%s details=%s\n",
                result->descriptor->name,
                result->descriptor->unavailable_reason_code,
                result->descriptor->unavailable_details);
        }
    }
    if (fclose(file) != 0)
    {
        set_text_error(error, "cannot finish performance manifest");
        return 0;
    }
    return 1;
}

static int validate_arguments(
    const pf_benchmark_metadata *metadata,
    const pf_benchmark_history_paths *paths,
    const pf_benchmark_result *results,
    pf_benchmark_history_outcome *outcome,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    const char *metadata_strings[] = {
        metadata != NULL ? metadata->commit_hash : NULL,
        metadata != NULL ? metadata->run_mode : NULL,
        metadata != NULL ? metadata->started_utc : NULL,
        metadata != NULL ? metadata->finished_utc : NULL,
        metadata != NULL ? metadata->build_configuration : NULL,
        metadata != NULL ? metadata->compiler : NULL,
        metadata != NULL ? metadata->compiler_flags : NULL,
        metadata != NULL ? metadata->dependency_hash : NULL,
        metadata != NULL ? metadata->content_hash : NULL,
        metadata != NULL ? metadata->machine_fingerprint : NULL,
        metadata != NULL ? metadata->os_fingerprint : NULL,
        metadata != NULL ? metadata->cpu_fingerprint : NULL,
        metadata != NULL ? metadata->power_metadata : NULL,
        metadata != NULL ? metadata->thermal_metadata : NULL,
        metadata != NULL ? metadata->executable_hash : NULL,
    };
    const char *path_strings[] = {
        paths != NULL ? paths->database_path : NULL,
        paths != NULL ? paths->schema_path : NULL,
        paths != NULL ? paths->graph_directory : NULL,
        paths != NULL ? paths->manifest_path : NULL,
    };
    size_t index;

    if (metadata == NULL || paths == NULL || results == NULL ||
        outcome == NULL || error == NULL)
    {
        if (error != NULL)
        {
            set_text_error(error, "invalid performance history arguments");
        }
        return 0;
    }
    for (index = (size_t)0;
         index < sizeof(metadata_strings) / sizeof(metadata_strings[0]);
         ++index)
    {
        if (metadata_strings[index] == NULL ||
            metadata_strings[index][0] == '\0')
        {
            set_text_error(error, "performance metadata field is empty");
            return 0;
        }
    }
    for (index = (size_t)0;
         index < sizeof(path_strings) / sizeof(path_strings[0]);
         ++index)
    {
        if (path_strings[index] == NULL ||
            path_strings[index][0] == '\0')
        {
            set_text_error(error, "performance history path is empty");
            return 0;
        }
    }
    if (strlen(metadata->commit_hash) != (size_t)40 ||
        metadata->dirty_state < 0 ||
        metadata->dirty_state > 1 ||
        metadata->sample_target_ns == UINT64_C(0) ||
        metadata->repetition_count == UINT32_C(0))
    {
        set_text_error(error, "performance metadata values are invalid");
        return 0;
    }
    return 1;
}

int pf_benchmark_history_persist(
    const pf_benchmark_metadata *metadata,
    const pf_benchmark_history_paths *paths,
    pf_benchmark_result results[PF_BENCHMARK_SCENARIO_COUNT],
    pf_benchmark_history_outcome *outcome,
    char error[PF_BENCHMARK_ERROR_CAPACITY])
{
    sqlite3 *database = NULL;
    char *schema_text = NULL;
    int64_t scenario_ids[PF_BENCHMARK_SCENARIO_COUNT];
    pf_history_comparison comparisons[PF_BENCHMARK_SCENARIO_COUNT];
    size_t scenario_index;
    int transaction_open = 0;
    int ok = 0;
    const char *final_status;

    if (!validate_arguments(
            metadata,
            paths,
            results,
            outcome,
            error))
    {
        return 0;
    }
    (void)memset(outcome, 0, sizeof(*outcome));
    (void)memset(comparisons, 0, sizeof(comparisons));
    error[0] = '\0';

    if (strcmp(sqlite3_libversion(), "3.53.4") != 0 ||
        strstr(
            sqlite3_sourceid(),
            "bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc") ==
            NULL)
    {
        set_text_error(error, "linked SQLite is not locked version 3.53.4");
        return 0;
    }
    if (sqlite3_open_v2(
            paths->database_path,
            &database,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL) != SQLITE_OK)
    {
        set_sqlite_error(error, database, "open performance database");
        if (database != NULL)
        {
            (void)sqlite3_close(database);
        }
        return 0;
    }

    schema_text = read_text_file(paths->schema_path, error);
    if (schema_text == NULL ||
        !execute_sql(
            database,
            schema_text,
            "apply performance schema",
            error) ||
        !verify_schema(database, error) ||
        !execute_sql(
            database,
            "BEGIN IMMEDIATE;",
            "begin performance transaction",
            error))
    {
        goto cleanup;
    }
    transaction_open = 1;
    if (!insert_run(database, metadata, &outcome->run_id, error))
    {
        goto cleanup;
    }

    for (scenario_index = (size_t)0;
         scenario_index < pf_benchmark_descriptor_count();
         ++scenario_index)
    {
        pf_benchmark_result *result = &results[scenario_index];

        if (!resolve_scenario_id(
                database,
                result->descriptor,
                &scenario_ids[scenario_index],
                error))
        {
            goto cleanup;
        }
        if (result->available != UINT8_C(0))
        {
            if (!insert_samples(
                    database,
                    outcome->run_id,
                    scenario_ids[scenario_index],
                    result,
                    error) ||
                !compare_result(
                    database,
                    metadata,
                    outcome->run_id,
                    scenario_ids[scenario_index],
                    result,
                    &comparisons[scenario_index],
                    error) ||
                !insert_summary(
                    database,
                    outcome->run_id,
                    scenario_ids[scenario_index],
                    result,
                    &comparisons[scenario_index],
                    error))
            {
                goto cleanup;
            }
            ++outcome->available_count;
            if (strcmp(
                    comparisons[scenario_index].status,
                    "baseline") == 0)
            {
                ++outcome->baseline_count;
            }
            if (strcmp(
                    comparisons[scenario_index].status,
                    "compatible") == 0)
            {
                ++outcome->compatible_count;
            }
            if (strcmp(
                    comparisons[scenario_index].status,
                    "invalid") == 0)
            {
                ++outcome->invalid_comparisons;
            }
            if (strcmp(
                    comparisons[scenario_index].status,
                    "suspected_regression") == 0)
            {
                ++outcome->suspected_regressions;
            }
            if (strcmp(
                    comparisons[scenario_index].status,
                    "confirmed_regression") == 0)
            {
                ++outcome->confirmed_regressions;
            }
        }
        else
        {
            if (!insert_unavailable(
                    database,
                    outcome->run_id,
                    scenario_ids[scenario_index],
                    result->descriptor,
                    error))
            {
                goto cleanup;
            }
            ++outcome->unavailable_count;
        }
    }

    final_status =
        outcome->suspected_regressions != UINT32_C(0) ||
                outcome->confirmed_regressions != UINT32_C(0)
            ? "regression"
            : "pass";
    if (!update_run_status(
            database,
            outcome->run_id,
            final_status,
            metadata->finished_utc,
            error) ||
        !execute_sql(
            database,
            "COMMIT;",
            "commit performance transaction",
            error))
    {
        goto cleanup;
    }
    transaction_open = 0;

    if (!generate_graphs(
            database,
            metadata,
            paths->graph_directory,
            scenario_ids,
            error) ||
        !write_manifest(
            metadata,
            paths,
            results,
            comparisons,
            outcome,
            error))
    {
        goto cleanup;
    }
    ok = 1;

cleanup:
    if (transaction_open != 0)
    {
        (void)sqlite3_exec(database, "ROLLBACK;", NULL, NULL, NULL);
    }
    free(schema_text);
    if (database != NULL && sqlite3_close(database) != SQLITE_OK && ok != 0)
    {
        set_text_error(error, "cannot close performance database");
        ok = 0;
    }
    return ok;
}
