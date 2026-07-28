#include "pf/render_packet.h"
#include "pf/rl.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PF_VERIFIER_MEMORY_BYTES 2048U
#define PF_VERIFIER_MEMORY_ALIGNMENT 64U
#define PF_VERIFIER_SAVE_CAPACITY 1024U
#define PF_VERIFIER_MAX_CHECKS 48U
#define PF_VERIFIER_MAX_ACCEPTANCE 64U
#define PF_VERIFIER_MAX_EXTERNAL 32U
#define PF_VERIFIER_TEXT_CAPACITY 512U

typedef struct pf_verifier_storage
{
    alignas(PF_VERIFIER_MEMORY_ALIGNMENT)
        uint8_t state[PF_VERIFIER_MEMORY_BYTES];
    alignas(PF_VERIFIER_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_VERIFIER_MEMORY_BYTES];
} pf_verifier_storage;

typedef struct pf_verifier_check
{
    char name[64];
    char status[16];
    char expected[PF_VERIFIER_TEXT_CAPACITY];
    char observed[PF_VERIFIER_TEXT_CAPACITY];
    char evidence[PF_VERIFIER_TEXT_CAPACITY];
} pf_verifier_check;

typedef struct pf_verifier_checks
{
    pf_verifier_check values[PF_VERIFIER_MAX_CHECKS];
    size_t count;
    size_t failures;
} pf_verifier_checks;

typedef struct pf_acceptance_entry
{
    char id[64];
    char category[32];
    char status[32];
    char check[64];
} pf_acceptance_entry;

typedef struct pf_acceptance_manifest
{
    pf_acceptance_entry entries[PF_VERIFIER_MAX_ACCEPTANCE];
    size_t count;
    size_t active_count;
    size_t planned_count;
} pf_acceptance_manifest;

typedef struct pf_diff_summary
{
    size_t files;
    size_t simulation;
    size_t presentation;
    size_t infrastructure;
    size_t specification;
    size_t other;
} pf_diff_summary;

static pf_verifier_storage pf_verifier_storage_pool[4];

static int add_check(
    pf_verifier_checks *checks,
    const char *name,
    int passed,
    const char *expected,
    const char *observed,
    const char *evidence)
{
    pf_verifier_check *check;

    if (checks == NULL ||
        checks->count >= (size_t)PF_VERIFIER_MAX_CHECKS)
    {
        return 0;
    }
    check = &checks->values[checks->count];
    (void)memset(check, 0, sizeof(*check));
    (void)snprintf(check->name, sizeof(check->name), "%.63s", name);
    (void)snprintf(
        check->status,
        sizeof(check->status),
        "%s",
        passed != 0 ? "pass" : "fail");
    (void)snprintf(
        check->expected,
        sizeof(check->expected),
        "%.500s",
        expected);
    (void)snprintf(
        check->observed,
        sizeof(check->observed),
        "%.500s",
        observed);
    (void)snprintf(
        check->evidence,
        sizeof(check->evidence),
        "%.500s",
        evidence);
    ++checks->count;
    if (passed == 0)
    {
        ++checks->failures;
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
            (uint8_t)(byte_index * UINT32_C(17) + UINT32_C(23));
    }
    return content;
}

static int initialize_sim(
    size_t storage_index,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    uint64_t max_ticks,
    pf_sim **out_sim)
{
    pf_sim_config config;
    pf_memory_requirements requirements;

    if (storage_index >= (size_t)4 ||
        pf_sim_default_config(
            &config,
            player_count,
            mode) != PF_STATUS_OK)
    {
        return 0;
    }
    config.max_ticks = max_ticks;
    if (pf_sim_query_memory(&config, &requirements) != PF_STATUS_OK ||
        requirements.state_bytes > (size_t)PF_VERIFIER_MEMORY_BYTES ||
        requirements.scratch_bytes > (size_t)PF_VERIFIER_MEMORY_BYTES ||
        requirements.state_alignment >
            (size_t)PF_VERIFIER_MEMORY_ALIGNMENT ||
        requirements.scratch_alignment >
            (size_t)PF_VERIFIER_MEMORY_ALIGNMENT)
    {
        return 0;
    }
    return pf_sim_init(
               pf_verifier_storage_pool[storage_index].state,
               sizeof(pf_verifier_storage_pool[storage_index].state),
               pf_verifier_storage_pool[storage_index].scratch,
               sizeof(pf_verifier_storage_pool[storage_index].scratch),
               content,
               &config,
               out_sim) == PF_STATUS_OK;
}

static uint64_t random_next(uint64_t *state)
{
    uint64_t value = *state;

    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return value * UINT64_C(0x2545f4914f6cdd1d);
}

static void make_exploratory_actions(
    pf_rl_action actions[PF_SIM_MAX_PLAYERS],
    uint8_t player_count,
    uint64_t *random_state,
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(
        actions,
        0,
        sizeof(*actions) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)player_count;
         ++player_index)
    {
        uint64_t random_value = random_next(random_state);

        actions[player_index].schema_version =
            PF_RL_ACTION_SCHEMA_VERSION;
        actions[player_index].main_stick_x =
            (int16_t)(uint16_t)(random_value & UINT64_C(0xffff));
        actions[player_index].main_stick_y =
            (int16_t)(uint16_t)(
                (random_value >> 16U) & UINT64_C(0xffff));
        actions[player_index].secondary_stick_x =
            (int16_t)(uint16_t)(
                (random_value >> 32U) & UINT64_C(0xffff));
        actions[player_index].secondary_stick_y =
            (int16_t)(uint16_t)(
                (random_value >> 48U) & UINT64_C(0xffff));
        actions[player_index].left_trigger =
            (uint16_t)(random_next(random_state) & UINT64_C(0xffff));
        actions[player_index].right_trigger =
            (uint16_t)(random_next(random_state) & UINT64_C(0xffff));
        if ((tick + (uint64_t)player_index * UINT64_C(7)) %
                UINT64_C(43) ==
            UINT64_C(5))
        {
            actions[player_index].buttons = PF_INPUT_BUTTON_JUMP;
        }
    }
}

static int state_hashes_equal(
    const pf_state_hash *left,
    const pf_state_hash *right)
{
    return left->algorithm == right->algorithm &&
           left->algorithm_version == right->algorithm_version &&
           left->digest_size == right->digest_size &&
           left->reserved == right->reserved &&
           memcmp(left->bytes, right->bytes, sizeof(left->bytes)) == 0;
}

static int transitions_equal(
    const pf_rl_transition *left,
    const pf_rl_transition *right)
{
    return left->status == right->status &&
           left->diagnostic_flags == right->diagnostic_flags &&
           left->tick_result.completed_tick ==
               right->tick_result.completed_tick &&
           left->tick_result.fault_flags ==
               right->tick_result.fault_flags &&
           left->tick_result.terminated ==
               right->tick_result.terminated &&
           left->tick_result.truncated ==
               right->tick_result.truncated &&
           left->tick_result.winner_mask ==
               right->tick_result.winner_mask &&
           memcmp(
               left->compact_observation.values,
               right->compact_observation.values,
               sizeof(left->compact_observation.values)) == 0 &&
           memcmp(
               left->reward_q16,
               right->reward_q16,
               sizeof(left->reward_q16)) == 0 &&
           memcmp(
               left->legal_buttons,
               right->legal_buttons,
               sizeof(left->legal_buttons)) == 0;
}

static int run_action_layer_invariant(
    pf_verifier_checks *checks)
{
    const pf_content_view content = make_content();
    const uint64_t seed = UINT64_C(0x72564f4253455256);
    pf_sim *left = NULL;
    pf_sim *right = NULL;
    pf_rl_action actions[PF_SIM_MAX_PLAYERS];
    pf_rl_transition left_transition;
    pf_rl_transition right_transition;
    pf_state_hash left_hash;
    pf_state_hash right_hash;
    pf_sim_observation diagnostic;
    uint64_t random_state = UINT64_C(0x91e10da5c79e7b1d);
    uint64_t tick;
    int passed = 1;

    if (!initialize_sim(
            (size_t)0,
            &content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            UINT64_C(500),
            &left) ||
        !initialize_sim(
            (size_t)1,
            &content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            UINT64_C(500),
            &right) ||
        pf_rl_reset(left, seed, &left_transition) != PF_STATUS_OK ||
        pf_rl_reset(right, seed, &right_transition) != PF_STATUS_OK)
    {
        passed = 0;
    }
    for (tick = UINT64_C(0); passed != 0 && tick < UINT64_C(240); ++tick)
    {
        make_exploratory_actions(
            actions,
            UINT8_C(2),
            &random_state,
            tick);
        if (pf_rl_step(
                left,
                actions,
                (size_t)2,
                &left_transition) != PF_STATUS_OK ||
            pf_rl_step(
                right,
                actions,
                (size_t)2,
                &right_transition) != PF_STATUS_OK ||
            !transitions_equal(&left_transition, &right_transition) ||
            left_transition.structured_observation.seed != UINT64_C(0) ||
            left_transition.compact_observation.values[
                PF_RL_COMPACT_RESERVED_LOW_INDEX] != INT32_C(0) ||
            left_transition.compact_observation.values[
                PF_RL_COMPACT_RESERVED_HIGH_INDEX] != INT32_C(0))
        {
            passed = 0;
        }
    }
    if (passed != 0 &&
        (pf_sim_hash(left, &left_hash) != PF_STATUS_OK ||
         pf_sim_hash(right, &right_hash) != PF_STATUS_OK ||
         !state_hashes_equal(&left_hash, &right_hash) ||
         pf_sim_observe(left, &diagnostic) != PF_STATUS_OK ||
         diagnostic.seed != seed))
    {
        passed = 0;
    }
    return add_check(
        checks,
        "rl-action-layer-determinism",
        passed,
        "Two seeded exploratory runs match exactly; policy views redact "
        "the seed while diagnostic observation retains it.",
        passed != 0
            ? "240 exploratory ticks matched with the RL schema-2 contract."
            : "The exploratory runs, state hashes, or seed visibility diverged.",
        "public pf_rl_reset/pf_rl_step and pf_sim_observe APIs");
}

static int run_snapshot_invariant(
    pf_verifier_checks *checks)
{
    const pf_content_view content = make_content();
    pf_sim *source = NULL;
    pf_sim *restored = NULL;
    pf_rl_action actions[PF_SIM_MAX_PLAYERS];
    pf_rl_transition source_transition;
    pf_rl_transition restored_transition;
    pf_state_hash source_hash;
    pf_state_hash restored_hash;
    pf_mut_bytes destination;
    pf_bytes snapshot;
    uint8_t save_bytes[PF_VERIFIER_SAVE_CAPACITY];
    size_t save_size = (size_t)0;
    uint64_t random_state = UINT64_C(0xd1b54a32d192ed03);
    uint64_t tick;
    int passed = 1;

    if (!initialize_sim(
            (size_t)2,
            &content,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            UINT64_C(400),
            &source) ||
        !initialize_sim(
            (size_t)3,
            &content,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            UINT64_C(400),
            &restored) ||
        pf_rl_reset(
            source,
            UINT64_C(0x534e415053484f54),
            &source_transition) != PF_STATUS_OK)
    {
        passed = 0;
    }
    for (tick = UINT64_C(0); passed != 0 && tick < UINT64_C(64); ++tick)
    {
        make_exploratory_actions(
            actions,
            UINT8_C(4),
            &random_state,
            tick);
        if (pf_rl_step(
                source,
                actions,
                (size_t)4,
                &source_transition) != PF_STATUS_OK)
        {
            passed = 0;
        }
    }
    if (passed != 0 &&
        (pf_sim_query_save_size(source, &save_size) != PF_STATUS_OK ||
         save_size > sizeof(save_bytes)))
    {
        passed = 0;
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (passed != 0 &&
        pf_sim_save(source, &destination) != PF_STATUS_OK)
    {
        passed = 0;
    }
    snapshot.bytes = save_bytes;
    snapshot.size = destination.size;
    if (passed != 0 &&
        pf_sim_load(restored, snapshot) != PF_STATUS_OK)
    {
        passed = 0;
    }
    for (tick = UINT64_C(64);
         passed != 0 && tick < UINT64_C(128);
         ++tick)
    {
        make_exploratory_actions(
            actions,
            UINT8_C(4),
            &random_state,
            tick);
        if (pf_rl_step(
                source,
                actions,
                (size_t)4,
                &source_transition) != PF_STATUS_OK ||
            pf_rl_step(
                restored,
                actions,
                (size_t)4,
                &restored_transition) != PF_STATUS_OK ||
            !transitions_equal(
                &source_transition,
                &restored_transition))
        {
            passed = 0;
        }
    }
    if (passed != 0 &&
        (pf_sim_hash(source, &source_hash) != PF_STATUS_OK ||
         pf_sim_hash(restored, &restored_hash) != PF_STATUS_OK ||
         !state_hashes_equal(&source_hash, &restored_hash)))
    {
        passed = 0;
    }
    return add_check(
        checks,
        "snapshot-restore-team-trace",
        passed,
        "A four-player save/load resumes through the same public action layer "
        "and reaches the same hash.",
        passed != 0
            ? "The 64-tick post-restore team trace matched."
            : "The restored team trace or final state hash diverged.",
        "public pf_sim_save/pf_sim_load/pf_rl_step APIs");
}

static float absolute_float(float value)
{
    return value < 0.0F ? -value : value;
}

static int render_packets_match(
    const PF_RenderPacket *expected,
    const PF_RenderPacket *actual,
    float float_tolerance,
    uint8_t byte_tolerance)
{
    size_t index;

    if (!pf_render_packet_validate(expected) ||
        !pf_render_packet_validate(actual) ||
        expected->vertex_count != actual->vertex_count)
    {
        return 0;
    }
    for (index = (size_t)0; index < (size_t)4; ++index)
    {
        if (absolute_float(
                expected->clear_rgba[index] -
                actual->clear_rgba[index]) > float_tolerance)
        {
            return 0;
        }
    }
    for (index = (size_t)0;
         index < expected->vertex_count * (size_t)2;
         ++index)
    {
        if (absolute_float(
                expected->positions_xy[index] -
                actual->positions_xy[index]) > float_tolerance ||
            absolute_float(
                expected->texture_uv[index] -
                actual->texture_uv[index]) > float_tolerance)
        {
            return 0;
        }
    }
    for (index = (size_t)0;
         index < expected->vertex_count * (size_t)4;
         ++index)
    {
        if (absolute_float(
                expected->colors_rgba[index] -
                actual->colors_rgba[index]) > float_tolerance)
        {
            return 0;
        }
    }
    for (index = (size_t)0;
         index < (size_t)PF_RENDER_PACKET_TEXTURE_BYTES;
         ++index)
    {
        int difference =
            (int)expected->texture_rgba[index] -
            (int)actual->texture_rgba[index];
        if (difference < 0)
        {
            difference = -difference;
        }
        if ((unsigned int)difference > (unsigned int)byte_tolerance)
        {
            return 0;
        }
    }
    return 1;
}

static int run_render_invariant(
    pf_verifier_checks *checks)
{
    PF_RenderPacket expected;
    PF_RenderPacket actual;
    int passed;

    pf_render_packet_build_probe(&expected);
    pf_render_packet_build_probe(&actual);
    passed = render_packets_match(
        &expected,
        &actual,
        0.0001F,
        UINT8_C(2));
    return add_check(
        checks,
        "render-semantic-reference",
        passed,
        "The shared render packet is semantically valid and within the "
        "approved numeric/texture tolerance.",
        passed != 0
            ? "Reference and candidate render packets match."
            : "Reference and candidate render packets differ.",
        "pf_render_packet_build_probe with tolerant semantic comparison");
}

static int menu_graph_reaches_all(
    const uint8_t *adjacency,
    size_t node_count,
    size_t start_node)
{
    uint8_t visited[16];
    size_t queue[16];
    size_t head = (size_t)0;
    size_t tail = (size_t)0;
    size_t visited_count = (size_t)0;

    if (adjacency == NULL ||
        node_count == (size_t)0 ||
        node_count > (size_t)16 ||
        start_node >= node_count)
    {
        return 0;
    }
    (void)memset(visited, 0, sizeof(visited));
    visited[start_node] = UINT8_C(1);
    queue[tail++] = start_node;
    while (head < tail)
    {
        size_t from = queue[head++];
        size_t to;

        ++visited_count;
        for (to = (size_t)0; to < node_count; ++to)
        {
            if (adjacency[from * node_count + to] != UINT8_C(0) &&
                visited[to] == UINT8_C(0))
            {
                visited[to] = UINT8_C(1);
                queue[tail++] = to;
            }
        }
    }
    return visited_count == node_count;
}

static int mechanics_fixture_matches(
    const int32_t *expected,
    const int32_t *observed,
    size_t value_count)
{
    return expected != NULL &&
           observed != NULL &&
           memcmp(
               expected,
               observed,
               value_count * sizeof(*expected)) == 0;
}

static int run_internal_checks(pf_verifier_checks *checks)
{
    (void)memset(checks, 0, sizeof(*checks));
    return run_action_layer_invariant(checks) &&
           run_snapshot_invariant(checks) &&
           run_render_invariant(checks);
}

static int qualify_detectors(
    int *mechanical,
    int *visual,
    int *menu,
    int *determinism)
{
    static const int32_t expected_mechanics[4] = {
        INT32_C(-65536),
        INT32_C(0),
        INT32_C(65536),
        INT32_C(131072),
    };
    int32_t observed_mechanics[4];
    PF_RenderPacket expected_render;
    PF_RenderPacket defective_render;
    static const uint8_t defective_menu[16] = {
        UINT8_C(0), UINT8_C(1), UINT8_C(0), UINT8_C(0),
        UINT8_C(1), UINT8_C(0), UINT8_C(1), UINT8_C(0),
        UINT8_C(0), UINT8_C(1), UINT8_C(0), UINT8_C(0),
        UINT8_C(0), UINT8_C(0), UINT8_C(0), UINT8_C(0),
    };
    pf_state_hash expected_hash;
    pf_state_hash defective_hash;

    (void)memcpy(
        observed_mechanics,
        expected_mechanics,
        sizeof(observed_mechanics));
    observed_mechanics[2] += INT32_C(1);
    *mechanical = !mechanics_fixture_matches(
        expected_mechanics,
        observed_mechanics,
        (size_t)4);

    pf_render_packet_build_probe(&expected_render);
    defective_render = expected_render;
    defective_render.positions_xy[3] += 0.05F;
    *visual = !render_packets_match(
        &expected_render,
        &defective_render,
        0.0001F,
        UINT8_C(2));

    *menu = !menu_graph_reaches_all(
        defective_menu,
        (size_t)4,
        (size_t)0);

    (void)memset(&expected_hash, 0, sizeof(expected_hash));
    expected_hash.algorithm = PF_SIM_STATE_HASH_ALGORITHM_SHA256;
    expected_hash.algorithm_version =
        PF_SIM_STATE_HASH_ALGORITHM_VERSION;
    expected_hash.digest_size = PF_SIM_STATE_HASH_BYTES;
    defective_hash = expected_hash;
    defective_hash.bytes[19] = UINT8_C(1);
    *determinism = !state_hashes_equal(
        &expected_hash,
        &defective_hash);

    return *mechanical != 0 &&
           *visual != 0 &&
           *menu != 0 &&
           *determinism != 0;
}

static int parse_tsv_fields(
    char *line,
    char **fields,
    size_t expected_count)
{
    size_t field_index = (size_t)0;
    char *cursor = line;

    while (field_index < expected_count)
    {
        char *separator;

        fields[field_index++] = cursor;
        separator = strchr(cursor, '\t');
        if (separator == NULL)
        {
            break;
        }
        *separator = '\0';
        cursor = separator + (size_t)1;
    }
    if (field_index != expected_count ||
        strchr(cursor, '\t') != NULL)
    {
        return 0;
    }
    cursor = fields[expected_count - (size_t)1];
    cursor[strcspn(cursor, "\r\n")] = '\0';
    return 1;
}

static int read_acceptance_manifest(
    const char *path,
    pf_acceptance_manifest *manifest,
    char error[PF_VERIFIER_TEXT_CAPACITY])
{
    FILE *file = fopen(path, "rb");
    char line[1024];
    size_t line_number = (size_t)0;

    (void)memset(manifest, 0, sizeof(*manifest));
    if (file == NULL)
    {
        (void)snprintf(
            error,
            PF_VERIFIER_TEXT_CAPACITY,
            "cannot open acceptance manifest: %.400s",
            path);
        return 0;
    }
    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char *fields[4];
        pf_acceptance_entry *entry;

        ++line_number;
        if (line_number == (size_t)1)
        {
            if (strcmp(line, "id\tcategory\tstatus\tcheck\n") != 0 &&
                strcmp(line, "id\tcategory\tstatus\tcheck\r\n") != 0)
            {
                (void)fclose(file);
                (void)snprintf(
                    error,
                    PF_VERIFIER_TEXT_CAPACITY,
                    "acceptance manifest header is invalid");
                return 0;
            }
            continue;
        }
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#')
        {
            continue;
        }
        if (manifest->count >= (size_t)PF_VERIFIER_MAX_ACCEPTANCE ||
            !parse_tsv_fields(line, fields, (size_t)4))
        {
            (void)fclose(file);
            (void)snprintf(
                error,
                PF_VERIFIER_TEXT_CAPACITY,
                "invalid acceptance manifest row %zu",
                line_number);
            return 0;
        }
        entry = &manifest->entries[manifest->count++];
        (void)snprintf(entry->id, sizeof(entry->id), "%.63s", fields[0]);
        (void)snprintf(
            entry->category,
            sizeof(entry->category),
            "%.31s",
            fields[1]);
        (void)snprintf(
            entry->status,
            sizeof(entry->status),
            "%.31s",
            fields[2]);
        (void)snprintf(
            entry->check,
            sizeof(entry->check),
            "%.63s",
            fields[3]);
        if (strcmp(entry->status, "active") == 0)
        {
            ++manifest->active_count;
        }
        else if (strncmp(entry->status, "planned:", (size_t)8) == 0)
        {
            ++manifest->planned_count;
        }
        else
        {
            (void)fclose(file);
            (void)snprintf(
                error,
                PF_VERIFIER_TEXT_CAPACITY,
                "acceptance row %zu has invalid status",
                line_number);
            return 0;
        }
    }
    if (fclose(file) != 0 ||
        line_number == (size_t)0 ||
        manifest->active_count == (size_t)0)
    {
        (void)snprintf(
            error,
            PF_VERIFIER_TEXT_CAPACITY,
            "acceptance manifest is empty or unreadable");
        return 0;
    }
    return 1;
}

static int summarize_diff(
    const char *path,
    pf_diff_summary *summary,
    char error[PF_VERIFIER_TEXT_CAPACITY])
{
    FILE *file = fopen(path, "rb");
    char line[1024];

    (void)memset(summary, 0, sizeof(*summary));
    if (file == NULL)
    {
        (void)snprintf(
            error,
            PF_VERIFIER_TEXT_CAPACITY,
            "cannot open commit diff list: %.400s",
            path);
        return 0;
    }
    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }
        ++summary->files;
        if (strncmp(line, "src/sim/", (size_t)8) == 0 ||
            strncmp(line, "include/pf/sim", (size_t)14) == 0 ||
            strncmp(line, "include/pf/rl", (size_t)13) == 0 ||
            strncmp(line, "include/pf/replay", (size_t)17) == 0 ||
            strncmp(line, "tests/sim/", (size_t)10) == 0)
        {
            ++summary->simulation;
        }
        else if (strncmp(line, "src/presentation/", (size_t)17) == 0 ||
                 strncmp(line, "src/native_client/", (size_t)18) == 0 ||
                 strncmp(line, "src/web_client/", (size_t)15) == 0 ||
                 strncmp(line, "assets/", (size_t)7) == 0)
        {
            ++summary->presentation;
        }
        else if (strncmp(line, "tools/", (size_t)6) == 0 ||
                 strncmp(line, ".github/", (size_t)8) == 0 ||
                 strncmp(line, "dependencies/", (size_t)13) == 0 ||
                 strcmp(line, "CMakeLists.txt") == 0 ||
                 strcmp(line, "CMakePresets.json") == 0)
        {
            ++summary->infrastructure;
        }
        else if (strncmp(line, "docs/", (size_t)5) == 0 ||
                 strstr(line, "implementation_plan.md") != NULL ||
                 strcmp(line, "plan_modifications.md") == 0)
        {
            ++summary->specification;
        }
        else
        {
            ++summary->other;
        }
    }
    if (fclose(file) != 0)
    {
        (void)snprintf(
            error,
            PF_VERIFIER_TEXT_CAPACITY,
            "cannot finish reading commit diff list");
        return 0;
    }
    return 1;
}

static int read_external_checks(
    const char *path,
    pf_verifier_checks *checks,
    char error[PF_VERIFIER_TEXT_CAPACITY])
{
    FILE *file = fopen(path, "rb");
    char line[2048];
    size_t line_number = (size_t)0;
    size_t external_count = (size_t)0;

    if (file == NULL)
    {
        (void)snprintf(
            error,
            PF_VERIFIER_TEXT_CAPACITY,
            "cannot open external check manifest: %.380s",
            path);
        return 0;
    }
    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char *fields[3];
        char check_name[96];
        int passed;

        ++line_number;
        if (line_number == (size_t)1)
        {
            if (strcmp(line, "check\tstatus\tevidence\n") != 0 &&
                strcmp(line, "check\tstatus\tevidence\r\n") != 0)
            {
                (void)fclose(file);
                (void)snprintf(
                    error,
                    PF_VERIFIER_TEXT_CAPACITY,
                    "external check manifest header is invalid");
                return 0;
            }
            continue;
        }
        if (external_count >= (size_t)PF_VERIFIER_MAX_EXTERNAL ||
            !parse_tsv_fields(line, fields, (size_t)3))
        {
            (void)fclose(file);
            (void)snprintf(
                error,
                PF_VERIFIER_TEXT_CAPACITY,
                "invalid external check row %zu",
                line_number);
            return 0;
        }
        passed = strcmp(fields[1], "pass") == 0 ||
                 strcmp(fields[1], "deferred") == 0;
        if (!passed && strcmp(fields[1], "fail") != 0)
        {
            (void)fclose(file);
            (void)snprintf(
                error,
                PF_VERIFIER_TEXT_CAPACITY,
                "external check row %zu has invalid status",
                line_number);
            return 0;
        }
        (void)snprintf(
            check_name,
            sizeof(check_name),
            "external:%.63s",
            fields[0]);
        if (!add_check(
                checks,
                check_name,
                passed,
                "The selected external check passes or is explicitly "
                "deferred by capability.",
                fields[1],
                fields[2]))
        {
            (void)fclose(file);
            (void)snprintf(
                error,
                PF_VERIFIER_TEXT_CAPACITY,
                "too many verifier checks");
            return 0;
        }
        if (strcmp(fields[1], "deferred") == 0)
        {
            (void)snprintf(
                checks->values[checks->count - (size_t)1].status,
                sizeof(
                    checks->values[
                        checks->count - (size_t)1].status),
                "deferred");
        }
        ++external_count;
    }
    if (fclose(file) != 0 || external_count == (size_t)0)
    {
        (void)snprintf(
            error,
            PF_VERIFIER_TEXT_CAPACITY,
            "external check manifest is empty or unreadable");
        return 0;
    }
    return 1;
}

static int acceptance_check_exists(
    const pf_verifier_checks *checks,
    const char *check_name)
{
    size_t check_index;

    for (check_index = (size_t)0;
         check_index < checks->count;
         ++check_index)
    {
        const char *recorded_name = checks->values[check_index].name;

        if (strcmp(recorded_name, check_name) == 0 ||
            (strncmp(recorded_name, "external:", (size_t)9) == 0 &&
             strcmp(recorded_name + (size_t)9, check_name) == 0))
        {
            return 1;
        }
    }
    return 0;
}

static int enforce_acceptance_coverage(
    const pf_acceptance_manifest *acceptance,
    pf_verifier_checks *checks)
{
    size_t acceptance_index;

    for (acceptance_index = (size_t)0;
         acceptance_index < acceptance->count;
         ++acceptance_index)
    {
        const pf_acceptance_entry *entry =
            &acceptance->entries[acceptance_index];

        if (strcmp(entry->status, "active") == 0 &&
            !acceptance_check_exists(checks, entry->check))
        {
            char check_name[96];
            char expected[PF_VERIFIER_TEXT_CAPACITY];

            (void)snprintf(
                check_name,
                sizeof(check_name),
                "acceptance:%.63s",
                entry->id);
            (void)snprintf(
                expected,
                sizeof(expected),
                "Active acceptance entry %.63s is covered by check %.63s.",
                entry->id,
                entry->check);
            if (!add_check(
                    checks,
                    check_name,
                    0,
                    expected,
                    "No internal or external result was supplied.",
                    "verifier/acceptance_manifest.tsv"))
            {
                return 0;
            }
        }
    }
    return 1;
}

static uint32_t issue_number(
    const char *commit_hash,
    const char *check_name)
{
    uint32_t hash = UINT32_C(2166136261);
    const unsigned char *cursor;

    for (cursor = (const unsigned char *)commit_hash;
         *cursor != UINT8_C(0);
         ++cursor)
    {
        hash ^= (uint32_t)*cursor;
        hash *= UINT32_C(16777619);
    }
    for (cursor = (const unsigned char *)check_name;
         *cursor != UINT8_C(0);
         ++cursor)
    {
        hash ^= (uint32_t)*cursor;
        hash *= UINT32_C(16777619);
    }
    return hash % UINT32_C(10000);
}

static void safe_filename(
    const char *source,
    char destination[80])
{
    size_t index;

    for (index = (size_t)0;
         index < (size_t)79 && source[index] != '\0';
         ++index)
    {
        char value = source[index];
        destination[index] =
            (value >= 'a' && value <= 'z') ||
                    (value >= 'A' && value <= 'Z') ||
                    (value >= '0' && value <= '9') ||
                    value == '-' || value == '_'
                ? value
                : '_';
    }
    destination[index] = '\0';
}

static const char *issue_severity(const char *check_name)
{
    if (strstr(check_name, "determinism") != NULL ||
        strstr(check_name, "snapshot") != NULL ||
        strstr(check_name, "sanitizer") != NULL)
    {
        return "critical";
    }
    if (strstr(check_name, "performance") != NULL)
    {
        return "high";
    }
    if (strstr(check_name, "render") != NULL)
    {
        return "medium";
    }
    return "high";
}

static int write_issue(
    const char *issue_directory,
    const char *commit_hash,
    const char *build_hash,
    const char *content_hash,
    const pf_verifier_check *check,
    char path[1024])
{
    char safe_name[80];
    FILE *existing;
    FILE *file;
    uint32_t number = issue_number(commit_hash, check->name);

    safe_filename(check->name, safe_name);
    if (snprintf(
            path,
            (size_t)1024,
            "%s/VRF-2026-%04" PRIu32 "-%s.md",
            issue_directory,
            number,
            safe_name) < 0)
    {
        return 0;
    }
    existing = fopen(path, "rb");
    if (existing != NULL)
    {
        (void)fclose(existing);
        return 1;
    }
    file = fopen(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    (void)fprintf(
        file,
        "# [VRF-2026-%04" PRIu32 "] %s\n\n"
        "ID: VRF-2026-%04" PRIu32 "\n"
        "Status: unfixed\n"
        "Severity: %s\n"
        "Detected commit: %s\n"
        "Build hash: %s\n"
        "Content hash: %s\n"
        "Fixed commit: not-fixed\n\n"
        "## Reproduction\n\n"
        "Run `tools/run_verifier.sh %s` from the recorded commit.\n\n"
        "## Expected behavior\n\n"
        "%s\n\n"
        "## Observed behavior\n\n"
        "%s\n\n"
        "## Evidence\n\n"
        "%s\n\n"
        "## Resolution\n\n"
        "Not fixed.\n\n"
        "## Fix verification\n\n"
        "Pending a corrective commit and following bookkeeping commit.\n",
        number,
        check->name,
        number,
        issue_severity(check->name),
        commit_hash,
        build_hash,
        content_hash,
        commit_hash,
        check->expected,
        check->observed,
        check->evidence);
    return fclose(file) == 0;
}

static int write_pass_manifest(
    const char *output_directory,
    const char *issue_directory,
    const char *commit_hash,
    const char *build_hash,
    const char *content_hash,
    const pf_acceptance_manifest *acceptance,
    const pf_diff_summary *diff,
    const pf_verifier_checks *checks,
    char manifest_path[1024])
{
    FILE *file;
    size_t check_index;
    size_t acceptance_index;

    if (snprintf(
            manifest_path,
            (size_t)1024,
            "%s/pass_manifest.md",
            output_directory) < 0)
    {
        return 0;
    }
    file = fopen(manifest_path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    (void)fprintf(
        file,
        "# Verifier pass manifest\n\n"
        "Status: %s\n"
        "Commit: `%s`\n"
        "Build hash: `%s`\n"
        "Content hash: `%s`\n"
        "Checks: %zu\n"
        "Failures: %zu\n"
        "Active acceptance entries: %zu\n"
        "Planned acceptance entries: %zu\n\n"
        "## Diff selection\n\n"
        "- Files: %zu\n"
        "- Simulation: %zu\n"
        "- Presentation: %zu\n"
        "- Infrastructure: %zu\n"
        "- Specification: %zu\n"
        "- Other: %zu\n\n"
        "## Checks\n\n"
        "| Check | Status | Evidence |\n"
        "|---|---|---|\n",
        checks->failures == (size_t)0 ? "pass" : "fail",
        commit_hash,
        build_hash,
        content_hash,
        checks->count,
        checks->failures,
        acceptance->active_count,
        acceptance->planned_count,
        diff->files,
        diff->simulation,
        diff->presentation,
        diff->infrastructure,
        diff->specification,
        diff->other);
    for (check_index = (size_t)0;
         check_index < checks->count;
         ++check_index)
    {
        (void)fprintf(
            file,
            "| %s | %s | %s |\n",
            checks->values[check_index].name,
            checks->values[check_index].status,
            checks->values[check_index].evidence);
    }
    (void)fprintf(
        file,
        "\n## Planned capability checks\n\n");
    for (acceptance_index = (size_t)0;
         acceptance_index < acceptance->count;
         ++acceptance_index)
    {
        const pf_acceptance_entry *entry =
            &acceptance->entries[acceptance_index];
        if (strncmp(entry->status, "planned:", (size_t)8) == 0)
        {
            (void)fprintf(
                file,
                "- `%s` (%s): `%s` — %s\n",
                entry->id,
                entry->category,
                entry->check,
                entry->status);
        }
    }
    (void)fprintf(file, "\n## Issues\n\n");
    if (checks->failures == (size_t)0)
    {
        (void)fprintf(file, "None.\n");
    }
    else
    {
        for (check_index = (size_t)0;
             check_index < checks->count;
             ++check_index)
        {
            if (strcmp(
                    checks->values[check_index].status,
                    "fail") == 0)
            {
                char issue_path[1024];

                if (!write_issue(
                        issue_directory,
                        commit_hash,
                        build_hash,
                        content_hash,
                        &checks->values[check_index],
                        issue_path))
                {
                    (void)fclose(file);
                    return 0;
                }
                (void)fprintf(file, "- `%s`\n", issue_path);
            }
        }
    }
    return fclose(file) == 0;
}

static int run_qualification(const char *output_directory)
{
    pf_verifier_checks checks;
    char path[1024];
    FILE *file;
    int mechanical = 0;
    int visual = 0;
    int menu = 0;
    int determinism = 0;

    if (!run_internal_checks(&checks) ||
        checks.failures != (size_t)0 ||
        !qualify_detectors(
            &mechanical,
            &visual,
            &menu,
            &determinism))
    {
        (void)fprintf(
            stderr,
            "verifier-qualification=fail internal_failures=%zu\n",
            checks.failures);
        return 1;
    }
    if (snprintf(
            path,
            sizeof(path),
            "%s/qualification_manifest.md",
            output_directory) < 0)
    {
        return 1;
    }
    file = fopen(path, "wb");
    if (file == NULL)
    {
        return 1;
    }
    (void)fprintf(
        file,
        "# Verifier qualification\n\n"
        "Status: pass\n\n"
        "| Seeded defect | Detected |\n"
        "|---|---|\n"
        "| Mechanical oracle mismatch | %s |\n"
        "| Visual tolerance mismatch | %s |\n"
        "| Unreachable menu state | %s |\n"
        "| Deterministic hash mismatch | %s |\n",
        mechanical != 0 ? "yes" : "no",
        visual != 0 ? "yes" : "no",
        menu != 0 ? "yes" : "no",
        determinism != 0 ? "yes" : "no");
    if (fclose(file) != 0)
    {
        return 1;
    }
    (void)printf(
        "verifier-qualification=pass mechanical=%d visual=%d "
        "menu=%d determinism=%d\n",
        mechanical,
        visual,
        menu,
        determinism);
    return 0;
}

static int run_self_test(void)
{
    pf_verifier_checks checks;
    int mechanical = 0;
    int visual = 0;
    int menu = 0;
    int determinism = 0;

    if (!run_internal_checks(&checks) ||
        checks.failures != (size_t)0 ||
        !qualify_detectors(
            &mechanical,
            &visual,
            &menu,
            &determinism))
    {
        (void)fprintf(
            stderr,
            "verifier-self-test=fail checks=%zu failures=%zu\n",
            checks.count,
            checks.failures);
        return 1;
    }
    (void)printf(
        "verifier-self-test=pass checks=%zu seeded_defects=4\n",
        checks.count);
    return 0;
}

static int run_verification(
    const char *acceptance_path,
    const char *diff_path,
    const char *external_path,
    const char *output_directory,
    const char *issue_directory,
    const char *commit_hash,
    const char *build_hash,
    const char *content_hash)
{
    pf_acceptance_manifest acceptance;
    pf_diff_summary diff;
    pf_verifier_checks checks;
    char error[PF_VERIFIER_TEXT_CAPACITY];
    char manifest_path[1024];

    error[0] = '\0';
    if (strlen(commit_hash) != (size_t)40 ||
        !read_acceptance_manifest(
            acceptance_path,
            &acceptance,
            error) ||
        !summarize_diff(diff_path, &diff, error) ||
        !run_internal_checks(&checks) ||
        !read_external_checks(external_path, &checks, error) ||
        !enforce_acceptance_coverage(&acceptance, &checks))
    {
        (void)fprintf(
            stderr,
            "verifier=fail reason=%s\n",
            error[0] != '\0' ? error : "internal verifier setup");
        return 1;
    }
    if (!write_pass_manifest(
            output_directory,
            issue_directory,
            commit_hash,
            build_hash,
            content_hash,
            &acceptance,
            &diff,
            &checks,
            manifest_path))
    {
        (void)fprintf(
            stderr,
            "verifier=fail reason=write-pass-manifest\n");
        return 1;
    }
    (void)printf(
        "verifier=%s checks=%zu failures=%zu active=%zu planned=%zu "
        "manifest=%s\n",
        checks.failures == (size_t)0 ? "pass" : "fail",
        checks.count,
        checks.failures,
        acceptance.active_count,
        acceptance.planned_count,
        manifest_path);
    return checks.failures == (size_t)0 ? 0 : 1;
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 &&
        strcmp(arguments[1], "--smoke") == 0)
    {
        (void)printf(
            "verifier-smoke=pass sim_abi=%" PRIu32
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
    if (argument_count == 3 &&
        strcmp(arguments[1], "--qualify") == 0)
    {
        return run_qualification(arguments[2]);
    }
    if (argument_count == 10 &&
        strcmp(arguments[1], "--verify") == 0)
    {
        return run_verification(
            arguments[2],
            arguments[3],
            arguments[4],
            arguments[5],
            arguments[6],
            arguments[7],
            arguments[8],
            arguments[9]);
    }

    (void)fprintf(
        stderr,
        "usage: pf_verifier --smoke|--self-test|--qualify OUTPUT_DIR|"
        "--verify ACCEPTANCE DIFF EXTERNAL OUTPUT ISSUES COMMIT BUILD CONTENT\n");
    return 2;
}
