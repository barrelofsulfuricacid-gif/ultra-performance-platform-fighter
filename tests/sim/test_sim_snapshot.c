#include "pf/sim.h"
#include "sim_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 2048U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_SAVE_CAPACITY 1024U
#define TEST_TRACE_TICKS UINT64_C(73)

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
            "sim-snapshot=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static pf_content_view make_content(uint8_t salt)
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
            (uint8_t)(byte_index * UINT32_C(11) + (uint32_t)salt);
    }
    return content;
}

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim)
{
    return expect_status(
        pf_sim_init(
            storage->state,
            sizeof(storage->state),
            storage->scratch,
            sizeof(storage->scratch),
            content,
            config,
            out_sim),
        PF_STATUS_OK,
        "init");
}

static void make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(
        inputs,
        0,
        sizeof(*inputs) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < UINT32_C(2);
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }

    inputs[0].main_stick_x =
        (tick % UINT64_C(9)) < UINT64_C(6)
            ? INT16_C(24576)
            : INT16_C(0);
    inputs[1].main_stick_x =
        (tick % UINT64_C(11)) < UINT64_C(7)
            ? INT16_C(-28672)
            : INT16_C(0);
    if (tick == UINT64_C(4) || tick == UINT64_C(31))
    {
        inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
    }
    if (tick == UINT64_C(17) || tick == UINT64_C(52))
    {
        inputs[1].buttons = PF_INPUT_BUTTON_JUMP;
    }
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

static int verify_sha256_standard_vector(void)
{
    static const uint8_t message[3] = {
        UINT8_C(0x61), UINT8_C(0x62), UINT8_C(0x63)};
    static const uint8_t expected[32] = {
        UINT8_C(0xba), UINT8_C(0x78), UINT8_C(0x16), UINT8_C(0xbf),
        UINT8_C(0x8f), UINT8_C(0x01), UINT8_C(0xcf), UINT8_C(0xea),
        UINT8_C(0x41), UINT8_C(0x41), UINT8_C(0x40), UINT8_C(0xde),
        UINT8_C(0x5d), UINT8_C(0xae), UINT8_C(0x22), UINT8_C(0x23),
        UINT8_C(0xb0), UINT8_C(0x03), UINT8_C(0x61), UINT8_C(0xa3),
        UINT8_C(0x96), UINT8_C(0x17), UINT8_C(0x7a), UINT8_C(0x9c),
        UINT8_C(0xb4), UINT8_C(0x10), UINT8_C(0xff), UINT8_C(0x61),
        UINT8_C(0xf2), UINT8_C(0x00), UINT8_C(0x15), UINT8_C(0xad)};
    pf_sha256 hash;
    uint8_t actual[32];

    pf_sha256_init(&hash);
    pf_sha256_update(&hash, message, sizeof(message));
    pf_sha256_finish(&hash, actual);
    if (memcmp(actual, expected, sizeof(expected)) != 0)
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=sha256-standard-vector\n");
        return 0;
    }
    return 1;
}

static int verify_save_digest(
    const uint8_t *save_bytes,
    size_t save_size,
    const pf_state_hash *state_hash)
{
    pf_sha256 hash;
    uint8_t save_digest[32];

    pf_sha256_init(&hash);
    pf_sha256_update(&hash, save_bytes, save_size);
    pf_sha256_finish(&hash, save_digest);
    if (memcmp(
            save_digest,
            state_hash->bytes,
            sizeof(save_digest)) != 0)
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=save-stream-hash\n");
        return 0;
    }
    return 1;
}

static int state_unchanged(
    pf_sim *sim,
    const pf_state_hash *before,
    const char *operation)
{
    pf_state_hash after;

    if (!expect_status(
            pf_sim_hash(sim, &after),
            PF_STATUS_OK,
            operation))
    {
        return 0;
    }
    if (!hash_equal(before, &after))
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=%s-state-mutated\n",
            operation);
        return 0;
    }
    return 1;
}

static int verify_wire_prefix(
    const uint8_t *save_bytes,
    size_t save_size)
{
    static const uint8_t expected_magic[8] = {
        UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x53), UINT8_C(0x41),
        UINT8_C(0x56), UINT8_C(0x45), UINT8_C(0x30), UINT8_C(0x37)};

    if (save_size != (size_t)569 ||
        memcmp(save_bytes, expected_magic, sizeof(expected_magic)) != 0 ||
        save_bytes[8] != UINT8_C(7) ||
        save_bytes[9] != UINT8_C(0) ||
        save_bytes[10] != UINT8_C(140) ||
        save_bytes[11] != UINT8_C(0) ||
        save_bytes[12] != UINT8_C(2) ||
        save_bytes[13] != UINT8_C(0) ||
        save_bytes[14] != UINT8_C(0) ||
        save_bytes[15] != UINT8_C(0) ||
        save_bytes[16] != UINT8_C(8) ||
        save_bytes[17] != UINT8_C(0) ||
        save_bytes[22] != UINT8_C(3) ||
        save_bytes[23] != UINT8_C(0) ||
        save_bytes[92] != (uint8_t)TEST_TRACE_TICKS ||
        save_bytes[140] != (uint8_t)TEST_TRACE_TICKS)
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=wire-prefix\n");
        return 0;
    }
    return 1;
}

int main(void)
{
    test_sim_storage source_storage;
    test_sim_storage load_storage;
    test_sim_storage clone_storage;
    test_sim_storage content_mismatch_storage;
    test_sim_storage config_mismatch_storage;
    test_sim_storage unreset_storage;
    pf_content_view content = make_content(UINT8_C(5));
    pf_content_view different_content = make_content(UINT8_C(9));
    pf_sim_config config;
    pf_sim_config different_config;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *clone = NULL;
    pf_sim *content_mismatch = NULL;
    pf_sim *config_mismatch = NULL;
    pf_sim *unreset = NULL;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result tick_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_state_hash clone_hash;
    pf_state_hash before_invalid;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    uint8_t damaged_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t required_bytes = (size_t)0;
    uint64_t tick;

    if (!verify_sha256_standard_vector() ||
        !expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "default-config"))
    {
        return 1;
    }
    config.max_ticks = UINT64_C(400);
    different_config = config;
    different_config.max_ticks = UINT64_C(401);

    if (!initialize_sim(&source_storage, &content, &config, &source) ||
        !initialize_sim(&load_storage, &content, &config, &loaded) ||
        !initialize_sim(&clone_storage, &content, &config, &clone) ||
        !initialize_sim(
            &content_mismatch_storage,
            &different_content,
            &config,
            &content_mismatch) ||
        !initialize_sim(
            &config_mismatch_storage,
            &content,
            &different_config,
            &config_mismatch) ||
        !initialize_sim(
            &unreset_storage,
            &content,
            &config,
            &unreset) ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x1020304050607080)),
            PF_STATUS_OK,
            "source-reset"))
    {
        return 1;
    }

    for (tick = UINT64_C(0); tick < TEST_TRACE_TICKS; ++tick)
    {
        make_inputs(inputs, tick);
        if (!expect_status(
                pf_sim_tick(
                    source,
                    inputs,
                    (size_t)2,
                    &tick_result),
                PF_STATUS_OK,
                "trace-tick"))
        {
            return 1;
        }
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &required_bytes),
            PF_STATUS_OK,
            "query-save-size") ||
        required_bytes != (size_t)569)
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=save-size value=%zu\n",
            required_bytes);
        return 1;
    }

    destination.bytes = save_bytes;
    destination.capacity = required_bytes - (size_t)1;
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_BUFFER_TOO_SMALL,
            "small-save-buffer") ||
        destination.size != required_bytes)
    {
        return 1;
    }

    destination.capacity = sizeof(save_bytes);
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save") ||
        !verify_wire_prefix(save_bytes, destination.size) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "source-hash") ||
        source_hash.algorithm != PF_SIM_STATE_HASH_ALGORITHM_SHA256 ||
        source_hash.algorithm_version !=
            PF_SIM_STATE_HASH_ALGORITHM_VERSION ||
        source_hash.digest_size != PF_SIM_STATE_HASH_BYTES ||
        source_hash.reserved != UINT16_C(0) ||
        !verify_save_digest(
            save_bytes,
            destination.size,
            &source_hash))
    {
        return 1;
    }

    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "load-into-unreset") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash) ||
        !expect_status(
            pf_sim_clone(clone, source),
            PF_STATUS_OK,
            "clone-into-unreset") ||
        !expect_status(
            pf_sim_hash(clone, &clone_hash),
            PF_STATUS_OK,
            "clone-hash") ||
        !hash_equal(&source_hash, &clone_hash) ||
        !expect_status(
            pf_sim_clone(source, source),
            PF_STATUS_OK,
            "self-clone"))
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=round-trip-equality\n");
        return 1;
    }

    before_invalid = loaded_hash;
    (void)memcpy(damaged_bytes, save_bytes, destination.size);
    damaged_bytes[destination.size - (size_t)1] ^= UINT8_C(1);
    source_bytes.bytes = damaged_bytes;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_CHECKSUM_MISMATCH,
            "payload-corruption") ||
        !state_unchanged(
            loaded,
            &before_invalid,
            "hash-after-payload-corruption"))
    {
        return 1;
    }

    (void)memcpy(damaged_bytes, save_bytes, destination.size);
    damaged_bytes[92] ^= UINT8_C(1);
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_INVALID_STATE,
            "header-tick-corruption") ||
        !state_unchanged(
            loaded,
            &before_invalid,
            "hash-after-header-tick-corruption"))
    {
        return 1;
    }

    (void)memcpy(damaged_bytes, save_bytes, destination.size);
    damaged_bytes[8] = UINT8_C(8);
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_UNSUPPORTED_VERSION,
            "unsupported-save-version") ||
        !state_unchanged(
            loaded,
            &before_invalid,
            "hash-after-version-corruption"))
    {
        return 1;
    }

    (void)memcpy(damaged_bytes, save_bytes, destination.size);
    damaged_bytes[28] ^= UINT8_C(1);
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_INCOMPATIBLE_STATE,
            "content-header-mismatch") ||
        !state_unchanged(
            loaded,
            &before_invalid,
            "hash-after-content-mismatch"))
    {
        return 1;
    }

    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size + (size_t)1;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_INVALID_STATE,
            "trailing-save-data") ||
        !state_unchanged(
            loaded,
            &before_invalid,
            "hash-after-trailing-data"))
    {
        return 1;
    }

    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(content_mismatch, source_bytes),
            PF_STATUS_INCOMPATIBLE_STATE,
            "load-content-mismatch") ||
        !expect_status(
            pf_sim_load(config_mismatch, source_bytes),
            PF_STATUS_INCOMPATIBLE_STATE,
            "load-config-mismatch") ||
        !expect_status(
            pf_sim_clone(content_mismatch, source),
            PF_STATUS_INCOMPATIBLE_STATE,
            "clone-content-mismatch") ||
        !expect_status(
            pf_sim_clone(config_mismatch, source),
            PF_STATUS_INCOMPATIBLE_STATE,
            "clone-config-mismatch"))
    {
        return 1;
    }

    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(unreset, &destination),
            PF_STATUS_INVALID_STATE,
            "save-before-reset") ||
        destination.size != required_bytes)
    {
        return 1;
    }

    make_inputs(inputs, TEST_TRACE_TICKS);
    if (!expect_status(
            pf_sim_tick(source, inputs, (size_t)2, &tick_result),
            PF_STATUS_OK,
            "source-post-clone-tick") ||
        !expect_status(
            pf_sim_tick(clone, inputs, (size_t)2, &tick_result),
            PF_STATUS_OK,
            "clone-post-clone-tick") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "source-post-clone-hash") ||
        !expect_status(
            pf_sim_hash(clone, &clone_hash),
            PF_STATUS_OK,
            "clone-post-clone-hash") ||
        !hash_equal(&source_hash, &clone_hash))
    {
        (void)fprintf(
            stderr,
            "sim-snapshot=fail operation=clone-future-equality\n");
        return 1;
    }

    (void)printf(
        "sim-snapshot=pass bytes=%zu hash_algorithm=sha256\n",
        required_bytes);
    return 0;
}
