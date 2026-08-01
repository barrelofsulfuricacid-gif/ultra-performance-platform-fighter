#include "pf/replay.h"
#include "pf/m4.h"
#include "pf/sim.h"

#include "m2_replay_fixture.h"
#include "sim_sha256.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 2048U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_INPUT_COUNT 720U
#define TEST_HASH_COUNT 181U
#define TEST_REPLAY_CAPACITY 32768U
#define TEST_OPTIONAL_REPLAY_CAPACITY 32816U
#define TEST_INPUT_PAYLOAD_OFFSET 930U

_Static_assert(
    TEST_INPUT_COUNT == PF_M2_REPLAY_TICKS * PF_M2_REPLAY_PLAYERS,
    "replay input capacity must cover the fixture");
_Static_assert(
    TEST_HASH_COUNT == PF_M2_REPLAY_TICKS + UINT64_C(1),
    "replay hash capacity must cover tick zero");

static const char expected_corpus_sha256[] =
    PF_M2_REPLAY_CORPUS_SHA256;
static const char expected_final_sha256[] =
    PF_M2_REPLAY_FINAL_SHA256;
static const char expected_events_sha256[] =
    PF_M2_REPLAY_EVENTS_SHA256;

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static pf_input_frame corpus_inputs[TEST_INPUT_COUNT];
static pf_state_hash corpus_hashes[TEST_HASH_COUNT];
static pf_state_hash mismatch_hashes[TEST_HASH_COUNT];
static uint8_t replay_bytes[TEST_REPLAY_CAPACITY];
static uint8_t modified_replay[TEST_OPTIONAL_REPLAY_CAPACITY];

static int expect_status(
    pf_status actual,
    pf_status expected,
    const char *operation)
{
    if (actual != expected)
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
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

static int state_hash_equal(
    const pf_state_hash *left,
    const pf_state_hash *right)
{
    return left->algorithm == right->algorithm &&
           left->algorithm_version == right->algorithm_version &&
           left->digest_size == right->digest_size &&
           left->reserved == right->reserved &&
           memcmp(left->bytes, right->bytes, sizeof(left->bytes)) == 0;
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

static void write_u64_le(uint8_t *bytes, uint64_t value)
{
    uint32_t byte_index;

    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(8);
         ++byte_index)
    {
        bytes[byte_index] =
            (uint8_t)(value >> (UINT32_C(8) * byte_index));
    }
}

static void digest_hex(
    const uint8_t digest[32],
    char output[65])
{
    static const char digits[] = "0123456789abcdef";
    uint32_t byte_index;

    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(32);
         ++byte_index)
    {
        output[(size_t)byte_index * (size_t)2] =
            digits[digest[byte_index] >> 4U];
        output[(size_t)byte_index * (size_t)2 + (size_t)1] =
            digits[digest[byte_index] & UINT8_C(0x0f)];
    }
    output[64] = '\0';
}

static void sha256_bytes(
    const uint8_t *bytes,
    size_t byte_count,
    uint8_t digest[32])
{
    pf_sha256 hash;

    pf_sha256_init(&hash);
    pf_sha256_update(&hash, bytes, byte_count);
    pf_sha256_finish(&hash, digest);
}

static void hash_tick_events(
    pf_sha256 *hash,
    const pf_tick_result *result)
{
    uint8_t bytes[8];
    uint32_t event_index;

    write_u64_le(bytes, result->completed_tick);
    pf_sha256_update(hash, bytes, (size_t)8);
    bytes[0] = result->event_count;
    pf_sha256_update(hash, bytes, (size_t)1);

    for (event_index = UINT32_C(0);
         event_index < (uint32_t)result->event_count;
         ++event_index)
    {
        const pf_sim_event *event = &result->events[event_index];

        write_u64_le(bytes, event->tick);
        pf_sha256_update(hash, bytes, (size_t)8);
        write_u32_le(bytes, event->sequence);
        pf_sha256_update(hash, bytes, (size_t)4);
        write_u32_le(bytes, event->value_q16);
        pf_sha256_update(hash, bytes, (size_t)4);
        write_u32_le(bytes, (uint32_t)event->velocity_x_q16);
        pf_sha256_update(hash, bytes, (size_t)4);
        write_u32_le(bytes, (uint32_t)event->velocity_y_q16);
        pf_sha256_update(hash, bytes, (size_t)4);
        write_u16_le(bytes, event->type);
        pf_sha256_update(hash, bytes, (size_t)2);
        write_u16_le(bytes, event->flags);
        pf_sha256_update(hash, bytes, (size_t)2);
        write_u16_le(bytes, event->detail);
        pf_sha256_update(hash, bytes, (size_t)2);
        bytes[0] = event->source_player;
        bytes[1] = event->target_player;
        pf_sha256_update(hash, bytes, (size_t)2);
    }
}

typedef struct test_replay_observer_context
{
    pf_sha256 events_hash;
    uint64_t checkpoint_count;
    uint64_t event_count;
} test_replay_observer_context;

static pf_status observe_replay_checkpoint(
    void *user_data,
    const pf_sim *sim,
    uint64_t replay_tick_count,
    const pf_tick_result *tick_result,
    const pf_state_hash *state_hash)
{
    test_replay_observer_context *context =
        (test_replay_observer_context *)user_data;
    pf_sim_observation observation;
    uint64_t checkpoint = context != NULL
                              ? context->checkpoint_count
                              : UINT64_MAX;

    if (context == NULL || sim == NULL || tick_result == NULL ||
        state_hash == NULL ||
        replay_tick_count != PF_M2_REPLAY_TICKS ||
        checkpoint > PF_M2_REPLAY_TICKS ||
        tick_result->completed_tick != checkpoint ||
        !state_hash_equal(
            state_hash,
            &corpus_hashes[(size_t)checkpoint]) ||
        pf_sim_observe(sim, &observation) != PF_STATUS_OK ||
        observation.tick != checkpoint)
    {
        return PF_STATUS_INVALID_STATE;
    }
    if (checkpoint != UINT64_C(0))
    {
        hash_tick_events(&context->events_hash, tick_result);
        context->event_count += (uint64_t)tick_result->event_count;
    }
    context->checkpoint_count += UINT64_C(1);
    return PF_STATUS_OK;
}

static int verify_unmodified_hash(
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
    if (!state_hash_equal(before, &after))
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=%s-state-mutated\n",
            operation);
        return 0;
    }
    return 1;
}

static int verify_optional_chunk(
    pf_sim *sim,
    size_t replay_size)
{
    static const uint8_t empty_sha256[32] = {
        UINT8_C(0xe3), UINT8_C(0xb0), UINT8_C(0xc4), UINT8_C(0x42),
        UINT8_C(0x98), UINT8_C(0xfc), UINT8_C(0x1c), UINT8_C(0x14),
        UINT8_C(0x9a), UINT8_C(0xfb), UINT8_C(0xf4), UINT8_C(0xc8),
        UINT8_C(0x99), UINT8_C(0x6f), UINT8_C(0xb9), UINT8_C(0x24),
        UINT8_C(0x27), UINT8_C(0xae), UINT8_C(0x41), UINT8_C(0xe4),
        UINT8_C(0x64), UINT8_C(0x9b), UINT8_C(0x93), UINT8_C(0x4c),
        UINT8_C(0xa4), UINT8_C(0x95), UINT8_C(0x99), UINT8_C(0x1b),
        UINT8_C(0x78), UINT8_C(0x52), UINT8_C(0xb8), UINT8_C(0x55)};
    pf_replay_verification verification;
    pf_bytes replay;
    uint8_t *chunk;

    (void)memcpy(modified_replay, replay_bytes, replay_size);
    write_u64_le(
        &modified_replay[24],
        (uint64_t)(replay_size + (size_t)48));
    write_u32_le(&modified_replay[32], UINT32_C(6));
    chunk = &modified_replay[replay_size];
    chunk[0] = UINT8_C(99);
    chunk[1] = UINT8_C(0);
    chunk[2] = UINT8_C(1);
    chunk[3] = UINT8_C(0);
    write_u32_le(&chunk[4], UINT32_C(0));
    write_u64_le(&chunk[8], UINT64_C(0));
    (void)memcpy(&chunk[16], empty_sha256, sizeof(empty_sha256));

    replay.bytes = modified_replay;
    replay.size = replay_size + (size_t)48;
    return expect_status(
               pf_replay_verify(sim, replay, &verification),
               PF_STATUS_OK,
               "unknown-optional-chunk") &&
           verification.verified_ticks == PF_M2_REPLAY_TICKS;
}

int main(void)
{
    test_sim_storage initial_storage;
    test_sim_storage source_storage;
    test_sim_storage playback_storage;
    test_sim_storage malformed_storage;
    test_sim_storage mismatch_storage;
    test_sim_storage incompatible_storage;
    pf_content_view content = pf_m2_replay_make_content();
    pf_content_view different_content = content;
    pf_sim_config config;
    pf_sim *initial = NULL;
    pf_sim *source_sim = NULL;
    pf_sim *playback = NULL;
    pf_sim *malformed_target = NULL;
    pf_sim *mismatch_target = NULL;
    pf_sim *incompatible = NULL;
    pf_tick_result result;
    pf_replay_source replay_source;
    pf_replay_verification verification;
    pf_replay_observer observer;
    test_replay_observer_context observer_context;
    pf_mut_bytes destination;
    pf_bytes replay;
    pf_state_hash playback_hash;
    pf_state_hash malformed_before;
    pf_m4_inspection combat_inspection;
    uint8_t replay_digest[32];
    uint8_t events_digest[32];
    uint8_t observed_events_digest[32];
    char replay_digest_hex[65];
    char final_digest_hex[65];
    char events_digest_hex[65];
    char observed_events_digest_hex[65];
    pf_sha256 events_hash;
    size_t replay_size = (size_t)0;
    uint64_t tick;
    int sdi_observed = 0;
    int tech_window_observed = 0;
    int air_dodge_observed = 0;
    int special_landing_observed = 0;
    int grounded_roll_observed = 0;
    int spot_dodge_observed = 0;
    int hit_observed = 0;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                PF_M2_REPLAY_PLAYERS,
                PF_SIM_MODE_TEAMS),
            PF_STATUS_OK,
            "default-config"))
    {
        return 1;
    }
    config.max_ticks = UINT64_C(500);
    different_content.content_hash.bytes[0] ^= UINT8_C(0xff);
    if (!initialize_sim(
            &initial_storage,
            &content,
            &config,
            &initial) ||
        !initialize_sim(
            &source_storage,
            &content,
            &config,
            &source_sim) ||
        !initialize_sim(
            &playback_storage,
            &content,
            &config,
            &playback) ||
        !initialize_sim(
            &malformed_storage,
            &content,
            &config,
            &malformed_target) ||
        !initialize_sim(
            &mismatch_storage,
            &content,
            &config,
            &mismatch_target) ||
        !initialize_sim(
            &incompatible_storage,
            &different_content,
            &config,
            &incompatible) ||
        !expect_status(
            pf_sim_reset(
                initial,
                PF_M2_REPLAY_SEED),
            PF_STATUS_OK,
            "initial-reset") ||
        !expect_status(
            pf_sim_clone(source_sim, initial),
            PF_STATUS_OK,
            "source-clone") ||
        !expect_status(
            pf_sim_hash(initial, &corpus_hashes[0]),
            PF_STATUS_OK,
            "initial-hash"))
    {
        return 1;
    }
    pf_sha256_init(&events_hash);
    pf_sha256_update(
        &events_hash,
        (const uint8_t *)"PFEVT001",
        (size_t)8);

    for (tick = UINT64_C(0); tick < PF_M2_REPLAY_TICKS; ++tick)
    {
        pf_input_frame *tick_inputs =
            &corpus_inputs[
                (size_t)tick * (size_t)PF_M2_REPLAY_PLAYERS];
        pf_m2_replay_make_tick_inputs(tick_inputs, tick);
        if (!expect_status(
                pf_sim_tick(
                    source_sim,
                    tick_inputs,
                    (size_t)PF_M2_REPLAY_PLAYERS,
                    &result),
                PF_STATUS_OK,
                "source-tick") ||
            !expect_status(
                pf_sim_hash(
                    source_sim,
                    &corpus_hashes[(size_t)tick + (size_t)1]),
                PF_STATUS_OK,
                "source-tick-hash") ||
            !expect_status(
                pf_m4_inspect(source_sim, &combat_inspection),
                PF_STATUS_OK,
                "source-tick-inspection"))
        {
            return 1;
        }
        hash_tick_events(&events_hash, &result);
        {
            uint32_t player_index;

            for (player_index = UINT32_C(0);
                 player_index < (uint32_t)PF_M2_REPLAY_PLAYERS;
                 ++player_index)
            {
                if (combat_inspection.players[player_index].
                        sdi_pulse_count > UINT8_C(0))
                {
                    sdi_observed = 1;
                }
                if (combat_inspection.players[player_index].
                        last_hit_valid != UINT8_C(0))
                {
                    hit_observed = 1;
                }
                if (combat_inspection.players[player_index].
                        tech_window_ticks > UINT16_C(0))
                {
                    tech_window_observed = 1;
                }
                if (combat_inspection.players[player_index].
                        action_state ==
                    (uint8_t)PF_M4_ACTION_AIR_DODGE)
                {
                    air_dodge_observed = 1;
                }
                if (combat_inspection.players[player_index].
                        action_state ==
                    (uint8_t)PF_M4_ACTION_SPECIAL_LANDING)
                {
                    special_landing_observed = 1;
                }
                if (combat_inspection.players[player_index].
                        action_state ==
                        (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
                    combat_inspection.players[player_index].
                        action_state ==
                        (uint8_t)PF_M4_ACTION_ROLL_BACKWARD)
                {
                    grounded_roll_observed = 1;
                }
                if (combat_inspection.players[player_index].
                        action_state ==
                    (uint8_t)PF_M4_ACTION_SPOT_DODGE)
                {
                    spot_dodge_observed = 1;
                }
            }
        }
    }
    if (!expect_status(
            pf_m4_inspect(source_sim, &combat_inspection),
            PF_STATUS_OK,
            "source-combat-inspection"))
    {
        return 1;
    }
    if (result.completed_tick != PF_M2_REPLAY_TICKS ||
        result.terminated != UINT8_C(1) ||
        result.truncated != UINT8_C(0) ||
        result.winner_mask != UINT8_C(5) ||
        sdi_observed == 0 ||
        tech_window_observed == 0 ||
        air_dodge_observed == 0 ||
        special_landing_observed == 0 ||
        grounded_roll_observed == 0 ||
        spot_dodge_observed == 0 ||
        hit_observed == 0)
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=source-result completed=%" PRIu64
            " terminated=%u truncated=%u winner=%u hit=%u%u%u%u seen=%d"
            " sdi=%d tech=%d air_dodge=%d special_landing=%d"
            " grounded_roll=%d spot_dodge=%d\n",
            result.completed_tick,
            (unsigned int)result.terminated,
            (unsigned int)result.truncated,
            (unsigned int)result.winner_mask,
            (unsigned int)combat_inspection.players[0].last_hit_valid,
            (unsigned int)combat_inspection.players[1].last_hit_valid,
            (unsigned int)combat_inspection.players[2].last_hit_valid,
            (unsigned int)combat_inspection.players[3].last_hit_valid,
            hit_observed,
            sdi_observed,
            tech_window_observed,
            air_dodge_observed,
            special_landing_observed,
            grounded_roll_observed,
            spot_dodge_observed);
        return 1;
    }

    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = corpus_inputs;
    replay_source.input_frame_count = TEST_INPUT_COUNT;
    replay_source.state_hashes = corpus_hashes;
    replay_source.state_hash_count = TEST_HASH_COUNT;
    replay_source.tick_count = PF_M2_REPLAY_TICKS;
    replay_source.final_result = result;

    if (!expect_status(
            pf_replay_query_size(&replay_source, &replay_size),
            PF_STATUS_OK,
            "query-replay-size") ||
        replay_size != (size_t)31386)
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=replay-size value=%zu\n",
            replay_size);
        return 1;
    }

    destination.bytes = replay_bytes;
    destination.capacity = replay_size - (size_t)1;
    destination.size = (size_t)0;
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_BUFFER_TOO_SMALL,
            "small-replay-buffer") ||
        destination.size != replay_size)
    {
        return 1;
    }
    destination.capacity = sizeof(replay_bytes);
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_OK,
            "encode-replay") ||
        destination.size != replay_size)
    {
        return 1;
    }

    replay.bytes = replay_bytes;
    replay.size = replay_size;
    (void)memset(&observer, 0, sizeof(observer));
    observer.struct_size = (uint32_t)sizeof(observer);
    observer.schema_version = PF_REPLAY_OBSERVER_SCHEMA_VERSION;
    observer.checkpoint = observe_replay_checkpoint;
    observer.user_data = &observer_context;
    (void)memset(&observer_context, 0, sizeof(observer_context));
    pf_sha256_init(&observer_context.events_hash);
    pf_sha256_update(
        &observer_context.events_hash,
        (const uint8_t *)"PFEVT001",
        (size_t)8);
    if (!expect_status(
            pf_replay_verify_observed(
                playback,
                replay,
                &observer,
                &verification),
            PF_STATUS_OK,
            "verify-observed-replay") ||
        verification.status != (uint32_t)PF_STATUS_OK ||
        verification.expected_ticks != PF_M2_REPLAY_TICKS ||
        verification.verified_ticks != PF_M2_REPLAY_TICKS ||
        verification.first_mismatch_tick != UINT64_MAX ||
        observer_context.checkpoint_count !=
            PF_M2_REPLAY_TICKS + UINT64_C(1) ||
        observer_context.event_count == UINT64_C(0) ||
        !expect_status(
            pf_sim_hash(playback, &playback_hash),
            PF_STATUS_OK,
            "playback-final-hash") ||
        !state_hash_equal(
            &playback_hash,
            &corpus_hashes[TEST_HASH_COUNT - 1U]))
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=verified-playback\n");
        return 1;
    }

    if (!expect_status(
            pf_sim_reset(
                malformed_target,
                UINT64_C(0x9988776655443322)),
            PF_STATUS_OK,
            "malformed-target-reset") ||
        !expect_status(
            pf_sim_hash(malformed_target, &malformed_before),
            PF_STATUS_OK,
            "malformed-before-hash"))
    {
        return 1;
    }
    (void)memcpy(modified_replay, replay_bytes, replay_size);
    modified_replay[TEST_INPUT_PAYLOAD_OFFSET + (size_t)19] ^=
        UINT8_C(1);
    replay.bytes = modified_replay;
    if (!expect_status(
            pf_replay_verify(
                malformed_target,
                replay,
                &verification),
            PF_STATUS_CHECKSUM_MISMATCH,
            "corrupt-input-checksum") ||
        !verify_unmodified_hash(
            malformed_target,
            &malformed_before,
            "malformed-after-hash"))
    {
        return 1;
    }

    (void)memcpy(
        mismatch_hashes,
        corpus_hashes,
        sizeof(mismatch_hashes));
    mismatch_hashes[51].bytes[0] ^= UINT8_C(1);
    replay_source.state_hashes = mismatch_hashes;
    destination.bytes = modified_replay;
    destination.capacity = sizeof(modified_replay);
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_OK,
            "encode-mismatch-replay"))
    {
        return 1;
    }
    replay.bytes = modified_replay;
    replay.size = destination.size;
    if (!expect_status(
            pf_replay_verify(
                mismatch_target,
                replay,
                &verification),
            PF_STATUS_REPLAY_MISMATCH,
            "detect-tick-hash-mismatch") ||
        verification.first_mismatch_tick != UINT64_C(51) ||
        verification.verified_ticks != UINT64_C(50))
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=mismatch-localization tick=%" PRIu64
            " verified=%" PRIu64 "\n",
            verification.first_mismatch_tick,
            verification.verified_ticks);
        return 1;
    }

    replay.bytes = replay_bytes;
    replay.size = replay_size;
    if (!expect_status(
            pf_replay_verify(
                incompatible,
                replay,
                &verification),
            PF_STATUS_INCOMPATIBLE_STATE,
            "incompatible-content"))
    {
        return 1;
    }

    (void)memcpy(modified_replay, replay_bytes, replay_size);
    modified_replay[40] = UINT8_C(99);
    modified_replay[41] = UINT8_C(0);
    replay.bytes = modified_replay;
    if (!expect_status(
            pf_replay_verify(
                malformed_target,
                replay,
                &verification),
            PF_STATUS_UNSUPPORTED_VERSION,
            "unknown-required-chunk"))
    {
        return 1;
    }

    if (!verify_optional_chunk(malformed_target, replay_size))
    {
        return 1;
    }

    sha256_bytes(replay_bytes, replay_size, replay_digest);
    digest_hex(replay_digest, replay_digest_hex);
    digest_hex(
        corpus_hashes[TEST_HASH_COUNT - 1U].bytes,
        final_digest_hex);
    pf_sha256_finish(&events_hash, events_digest);
    pf_sha256_finish(
        &observer_context.events_hash,
        observed_events_digest);
    digest_hex(events_digest, events_digest_hex);
    digest_hex(
        observed_events_digest,
        observed_events_digest_hex);
    if (strcmp(replay_digest_hex, expected_corpus_sha256) != 0 ||
        strcmp(final_digest_hex, expected_final_sha256) != 0 ||
        strcmp(events_digest_hex, expected_events_sha256) != 0 ||
        strcmp(observed_events_digest_hex, expected_events_sha256) != 0)
    {
        (void)fprintf(
            stderr,
            "sim-replay=fail operation=golden-corpus"
            " corpus=%s final=%s events=%s observed_events=%s\n",
            replay_digest_hex,
            final_digest_hex,
            events_digest_hex,
            observed_events_digest_hex);
        return 1;
    }
    (void)printf(
        "sim-replay=pass ticks=%" PRIu64
        " players=%u bytes=%zu corpus_sha256=%s final_sha256=%s"
        " events_sha256=%s\n",
        PF_M2_REPLAY_TICKS,
        (unsigned int)PF_M2_REPLAY_PLAYERS,
        replay_size,
        replay_digest_hex,
        final_digest_hex,
        events_digest_hex);
    return 0;
}
