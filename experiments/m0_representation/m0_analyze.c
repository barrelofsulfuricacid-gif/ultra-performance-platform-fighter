#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CASES 64u
#define MAX_ROUNDS 64u
#define MAX_LINE 1024u

typedef struct {
    char family[64];
    char candidate[96];
    char unit[48];
    size_t state_bytes;
    double samples[MAX_ROUNDS];
    bool present[MAX_ROUNDS];
    size_t count;
} CaseStats;

static int compare_double(const void *left, const void *right)
{
    const double a = *(const double *)left;
    const double b = *(const double *)right;
    return (a > b) - (a < b);
}

static int compare_case(const void *left, const void *right)
{
    const CaseStats *a = (const CaseStats *)left;
    const CaseStats *b = (const CaseStats *)right;
    const int family_order = strcmp(a->family, b->family);
    return family_order != 0 ? family_order
                             : strcmp(a->candidate, b->candidate);
}

static double median(const double *values, size_t count)
{
    double copy[MAX_ROUNDS];
    if (count == 0u || count > MAX_ROUNDS) {
        return 0.0;
    }
    memcpy(copy, values, count * sizeof(copy[0]));
    qsort(copy, count, sizeof(copy[0]), compare_double);
    if ((count & 1u) != 0u) {
        return copy[count / 2u];
    }
    return (copy[count / 2u - 1u] + copy[count / 2u]) * 0.5;
}

static double median_absolute_deviation(const double *values, size_t count,
                                        double center)
{
    double deviations[MAX_ROUNDS];
    for (size_t i = 0; i < count; ++i) {
        deviations[i] = fabs(values[i] - center);
    }
    return median(deviations, count);
}

static const char *baseline_candidate(const char *family)
{
    if (strcmp(family, "numeric_motion") == 0) {
        return "float32";
    }
    if (strcmp(family, "world_resolution") == 0) {
        return "world_256_u8";
    }
    if (strncmp(family, "broadphase_", 11u) == 0) {
        return "naive";
    }
    if (strcmp(family, "layout_update") == 0) {
        return "aos_with_cold";
    }
    if (strcmp(family, "state_dispatch") == 0) {
        return "switch";
    }
    if (strcmp(family, "snapshot_64k") == 0) {
        return "full_copy_restore";
    }
    return NULL;
}

static uint64_t bootstrap_random(uint64_t *state)
{
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static bool paired_ratio(const CaseStats *candidate,
                         const CaseStats *baseline, double *ratio,
                         double *lower, double *upper, size_t *sample_count)
{
    double ratios[MAX_ROUNDS];
    size_t count = 0u;
    for (size_t round = 0; round < MAX_ROUNDS; ++round) {
        if (candidate->present[round] && baseline->present[round] &&
            candidate->samples[round] > 0.0 &&
            baseline->samples[round] > 0.0) {
            ratios[count++] =
                candidate->samples[round] / baseline->samples[round];
        }
    }
    *sample_count = count;
    if (count == 0u) {
        return false;
    }
    *ratio = median(ratios, count);

    if (count == 1u) {
        *lower = *ratio;
        *upper = *ratio;
        return true;
    }

    enum { BOOTSTRAP_SAMPLES = 20000 };
    double bootstrap[BOOTSTRAP_SAMPLES];
    uint64_t random_state = UINT64_C(0x243f6a8885a308d3);
    for (size_t sample = 0; sample < BOOTSTRAP_SAMPLES; ++sample) {
        double resampled[MAX_ROUNDS];
        for (size_t i = 0; i < count; ++i) {
            const size_t index =
                (size_t)(bootstrap_random(&random_state) % count);
            resampled[i] = ratios[index];
        }
        bootstrap[sample] = median(resampled, count);
    }
    qsort(bootstrap, BOOTSTRAP_SAMPLES, sizeof(bootstrap[0]),
          compare_double);
    *lower = bootstrap[(BOOTSTRAP_SAMPLES * 25u) / 1000u];
    *upper = bootstrap[(BOOTSTRAP_SAMPLES * 975u) / 1000u - 1u];
    return true;
}

static CaseStats *find_or_add(CaseStats *cases, size_t *case_count,
                              const char *family, const char *candidate,
                              const char *unit, size_t state_bytes)
{
    for (size_t i = 0; i < *case_count; ++i) {
        if (strcmp(cases[i].family, family) == 0 &&
            strcmp(cases[i].candidate, candidate) == 0) {
            return &cases[i];
        }
    }
    if (*case_count >= MAX_CASES) {
        fprintf(stderr, "too many benchmark cases\n");
        exit(EXIT_FAILURE);
    }
    CaseStats *stats = &cases[(*case_count)++];
    memset(stats, 0, sizeof(*stats));
    (void)snprintf(stats->family, sizeof(stats->family), "%s", family);
    (void)snprintf(stats->candidate, sizeof(stats->candidate), "%s",
                   candidate);
    (void)snprintf(stats->unit, sizeof(stats->unit), "%s", unit);
    stats->state_bytes = state_bytes;
    return stats;
}

static CaseStats *find_case(CaseStats *cases, size_t case_count,
                            const char *family, const char *candidate)
{
    for (size_t i = 0; i < case_count; ++i) {
        if (strcmp(cases[i].family, family) == 0 &&
            strcmp(cases[i].candidate, candidate) == 0) {
            return &cases[i];
        }
    }
    return NULL;
}

static void print_rate(double rate)
{
    if (rate >= 1000000000.0) {
        printf("%.3f G", rate / 1000000000.0);
    } else if (rate >= 1000000.0) {
        printf("%.3f M", rate / 1000000.0);
    } else if (rate >= 1000.0) {
        printf("%.3f k", rate / 1000.0);
    } else {
        printf("%.3f", rate);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s results.csv\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *input = fopen(argv[1], "r");
    if (input == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    char line[MAX_LINE];
    if (fgets(line, sizeof(line), input) == NULL ||
        strncmp(line, "family,candidate,round,", 23u) != 0) {
        fprintf(stderr, "unexpected benchmark CSV header\n");
        (void)fclose(input);
        return EXIT_FAILURE;
    }

    CaseStats cases[MAX_CASES];
    memset(cases, 0, sizeof(cases));
    size_t case_count = 0u;
    size_t row_count = 0u;
    while (fgets(line, sizeof(line), input) != NULL) {
        char *fields[10];
        size_t field_count = 0u;
        char *save = NULL;
        for (char *token = strtok_r(line, ",\n", &save);
             token != NULL && field_count < 10u;
             token = strtok_r(NULL, ",\n", &save)) {
            fields[field_count++] = token;
        }
        if (field_count != 10u) {
            fprintf(stderr, "malformed benchmark CSV row %zu\n",
                    row_count + 2u);
            (void)fclose(input);
            return EXIT_FAILURE;
        }

        char *round_end = NULL;
        char *rate_end = NULL;
        char *bytes_end = NULL;
        const unsigned long round = strtoul(fields[2], &round_end, 10);
        const double rate = strtod(fields[6], &rate_end);
        const unsigned long long state_bytes =
            strtoull(fields[8], &bytes_end, 10);
        if (*round_end != '\0' || *rate_end != '\0' ||
            *bytes_end != '\0' || round >= MAX_ROUNDS || rate <= 0.0) {
            fprintf(stderr, "invalid benchmark value on row %zu\n",
                    row_count + 2u);
            (void)fclose(input);
            return EXIT_FAILURE;
        }

        CaseStats *stats =
            find_or_add(cases, &case_count, fields[0], fields[1],
                        fields[9], (size_t)state_bytes);
        if (stats->present[round]) {
            fprintf(stderr, "duplicate case/round on row %zu\n",
                    row_count + 2u);
            (void)fclose(input);
            return EXIT_FAILURE;
        }
        stats->samples[round] = rate;
        stats->present[round] = true;
        ++stats->count;
        ++row_count;
    }
    if (fclose(input) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }
    if (case_count == 0u) {
        fprintf(stderr, "benchmark CSV contains no cases\n");
        return EXIT_FAILURE;
    }

    qsort(cases, case_count, sizeof(cases[0]), compare_case);
    printf("# M0 representation benchmark summary\n\n");
    printf("Source: `%s`  \n", argv[1]);
    printf("Cases: %zu; samples: %zu. Higher throughput is better. "
           "The relative column is the median paired ratio to the named "
           "family baseline with a deterministic 20,000-resample, "
           "two-sided 95%% bootstrap confidence interval.\n\n",
           case_count, row_count);
    printf("| Family | Candidate | Median throughput | MAD | Relative to "
           "baseline (95%% CI) | State bytes | Unit |\n");
    printf("|---|---|---:|---:|---:|---:|---|\n");

    for (size_t i = 0; i < case_count; ++i) {
        double compact[MAX_ROUNDS];
        size_t compact_count = 0u;
        for (size_t round = 0; round < MAX_ROUNDS; ++round) {
            if (cases[i].present[round]) {
                compact[compact_count++] = cases[i].samples[round];
            }
        }
        const double center = median(compact, compact_count);
        const double mad =
            median_absolute_deviation(compact, compact_count, center);
        const char *baseline_name = baseline_candidate(cases[i].family);
        CaseStats *baseline =
            baseline_name == NULL
                ? NULL
                : find_case(cases, case_count, cases[i].family,
                            baseline_name);

        printf("| %s | %s | ", cases[i].family, cases[i].candidate);
        print_rate(center);
        printf(" | %.2f%% | ", center == 0.0 ? 0.0 : mad * 100.0 / center);
        if (baseline == NULL) {
            printf("n/a");
        } else {
            double ratio;
            double lower;
            double upper;
            size_t paired_count;
            if (!paired_ratio(&cases[i], baseline, &ratio, &lower, &upper,
                              &paired_count)) {
                printf("n/a");
            } else if (paired_count == 1u) {
                printf("%.3fx (1 sample)", ratio);
            } else {
                printf("%.3fx [%.3f, %.3f]", ratio, lower, upper);
            }
        }
        printf(" | %zu | %s |\n", cases[i].state_bytes, cases[i].unit);
    }

    return EXIT_SUCCESS;
}
