#include "replay_checkpoint.h"

#include "m2_replay_fixture.h"
#include "pf/replay.h"
#include "pf/sim.h"

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

#define PF_WEB_REPLAY_MEMORY_BYTES 2048U
#define PF_WEB_REPLAY_MEMORY_ALIGNMENT 64U
#define PF_WEB_REPLAY_INPUT_COUNT 720U
#define PF_WEB_REPLAY_HASH_COUNT 181U
#define PF_WEB_REPLAY_POSITION_COUNT 1448U
#define PF_WEB_REPLAY_CAPACITY 32768U

_Static_assert(
    PF_WEB_REPLAY_INPUT_COUNT ==
        PF_M2_REPLAY_TICKS * PF_M2_REPLAY_PLAYERS,
    "web replay input capacity must cover the fixture");
_Static_assert(
    PF_WEB_REPLAY_HASH_COUNT == PF_M2_REPLAY_TICKS + UINT64_C(1),
    "web replay hash capacity must cover tick zero");
_Static_assert(
    PF_WEB_REPLAY_POSITION_COUNT ==
        PF_WEB_REPLAY_HASH_COUNT * PF_M2_REPLAY_PLAYERS * UINT64_C(2),
    "web replay position capacity must cover every checkpoint");
_Static_assert(
    sizeof(PF_M2_REPLAY_FINAL_SHA256) ==
        PF_SIM_STATE_HASH_BYTES * (size_t)2 + (size_t)1,
    "fixture hash text must encode exactly one simulation hash");

typedef struct pf_web_replay_storage
{
    alignas(PF_WEB_REPLAY_MEMORY_ALIGNMENT)
        uint8_t state[PF_WEB_REPLAY_MEMORY_BYTES];
    alignas(PF_WEB_REPLAY_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_WEB_REPLAY_MEMORY_BYTES];
} pf_web_replay_storage;

extern void pf_web_replay_inspector(
    const int32_t *positions_q16,
    const uint8_t *hashes,
    int tick_count,
    int player_count,
    int winner_mask,
    const char *final_hash);

static pf_web_replay_storage pf_web_initial_storage;
static pf_web_replay_storage pf_web_source_storage;
static pf_web_replay_storage pf_web_playback_storage;
static pf_input_frame pf_web_replay_inputs[PF_WEB_REPLAY_INPUT_COUNT];
static pf_state_hash pf_web_replay_hashes[PF_WEB_REPLAY_HASH_COUNT];
static int32_t pf_web_replay_positions[PF_WEB_REPLAY_POSITION_COUNT];
static uint8_t pf_web_replay_bytes[PF_WEB_REPLAY_CAPACITY];
static uint8_t pf_web_hash_bytes[
    PF_WEB_REPLAY_HASH_COUNT * PF_SIM_STATE_HASH_BYTES];

static int pf_web_hex_nibble(char value)
{
    if (value >= '0' && value <= '9')
    {
        return (int)(value - '0');
    }
    if (value >= 'a' && value <= 'f')
    {
        return (int)(value - 'a') + 10;
    }
    if (value >= 'A' && value <= 'F')
    {
        return (int)(value - 'A') + 10;
    }
    return -1;
}

static int pf_web_hash_matches_fixture(const pf_state_hash *hash)
{
    static const char expected[] = PF_M2_REPLAY_FINAL_SHA256;
    size_t byte_index;

    if (hash == NULL)
    {
        return 0;
    }
    for (byte_index = (size_t)0;
         byte_index < (size_t)PF_SIM_STATE_HASH_BYTES;
         ++byte_index)
    {
        const int high = pf_web_hex_nibble(expected[byte_index * (size_t)2]);
        const int low =
            pf_web_hex_nibble(expected[byte_index * (size_t)2 + (size_t)1]);
        uint8_t decoded;

        if (high < 0 || low < 0)
        {
            return 0;
        }
        decoded =
            (uint8_t)((unsigned int)high << 4U | (unsigned int)low);
        if (hash->bytes[byte_index] != decoded)
        {
            return 0;
        }
    }
    return 1;
}

static int pf_web_replay_init(
    pf_web_replay_storage *storage,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim)
{
    return pf_sim_init(
               storage->state,
               sizeof(storage->state),
               storage->scratch,
               sizeof(storage->scratch),
               content,
               config,
               out_sim) == PF_STATUS_OK;
}

static int pf_web_capture_checkpoint(
    const pf_sim *sim,
    size_t checkpoint_index)
{
    pf_sim_observation observation;
    pf_state_hash *state_hash = &pf_web_replay_hashes[checkpoint_index];
    uint32_t player_index;

    if (pf_sim_observe(sim, &observation) != PF_STATUS_OK ||
        pf_sim_hash(sim, state_hash) != PF_STATUS_OK)
    {
        return 0;
    }
    (void)memcpy(
        &pf_web_hash_bytes[
            checkpoint_index * (size_t)PF_SIM_STATE_HASH_BYTES],
        state_hash->bytes,
        (size_t)PF_SIM_STATE_HASH_BYTES);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)PF_M2_REPLAY_PLAYERS;
         ++player_index)
    {
        const size_t position_index =
            (checkpoint_index * (size_t)PF_M2_REPLAY_PLAYERS +
             (size_t)player_index) *
            (size_t)2;
        pf_web_replay_positions[position_index] =
            observation.players[player_index].position_x_q16;
        pf_web_replay_positions[position_index + (size_t)1] =
            observation.players[player_index].position_y_q16;
    }
    return 1;
}

int pf_web_run_replay_checkpoint(void)
{
    const pf_content_view content = pf_m2_replay_make_content();
    pf_sim_config config;
    pf_sim *initial = NULL;
    pf_sim *source = NULL;
    pf_sim *playback = NULL;
    pf_tick_result result;
    pf_replay_source replay_source;
    pf_replay_verification verification;
    pf_mut_bytes destination;
    pf_bytes replay;
    size_t replay_size;
    uint64_t tick;
    int passed = 0;

    if (pf_sim_default_config(
            &config,
            PF_M2_REPLAY_PLAYERS,
            PF_SIM_MODE_TEAMS) != PF_STATUS_OK)
    {
        return 0;
    }
    config.max_ticks = UINT64_C(500);
    if (!pf_web_replay_init(
            &pf_web_initial_storage,
            &content,
            &config,
            &initial) ||
        !pf_web_replay_init(
            &pf_web_source_storage,
            &content,
            &config,
            &source) ||
        !pf_web_replay_init(
            &pf_web_playback_storage,
            &content,
            &config,
            &playback) ||
        pf_sim_reset(initial, PF_M2_REPLAY_SEED) != PF_STATUS_OK ||
        pf_sim_clone(source, initial) != PF_STATUS_OK ||
        !pf_web_capture_checkpoint(source, (size_t)0))
    {
        goto cleanup;
    }

    for (tick = UINT64_C(0); tick < PF_M2_REPLAY_TICKS; ++tick)
    {
        pf_input_frame *inputs =
            &pf_web_replay_inputs[
                (size_t)tick * (size_t)PF_M2_REPLAY_PLAYERS];
        pf_m2_replay_make_tick_inputs(inputs, tick);
        if (pf_sim_tick(
                source,
                inputs,
                (size_t)PF_M2_REPLAY_PLAYERS,
                &result) != PF_STATUS_OK ||
            !pf_web_capture_checkpoint(source, (size_t)tick + (size_t)1))
        {
            goto cleanup;
        }
    }
    if (result.completed_tick != PF_M2_REPLAY_TICKS ||
        result.terminated != UINT8_C(1) ||
        result.truncated != UINT8_C(0) ||
        result.winner_mask != UINT8_C(5) ||
        !pf_web_hash_matches_fixture(
            &pf_web_replay_hashes[
                PF_WEB_REPLAY_HASH_COUNT - (size_t)1]))
    {
        goto cleanup;
    }

    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = pf_web_replay_inputs;
    replay_source.input_frame_count = PF_WEB_REPLAY_INPUT_COUNT;
    replay_source.state_hashes = pf_web_replay_hashes;
    replay_source.state_hash_count = PF_WEB_REPLAY_HASH_COUNT;
    replay_source.tick_count = PF_M2_REPLAY_TICKS;
    replay_source.final_result = result;
    if (pf_replay_query_size(&replay_source, &replay_size) !=
            PF_STATUS_OK ||
        replay_size > sizeof(pf_web_replay_bytes))
    {
        goto cleanup;
    }
    destination.bytes = pf_web_replay_bytes;
    destination.capacity = sizeof(pf_web_replay_bytes);
    destination.size = (size_t)0;
    if (pf_replay_encode(&replay_source, &destination) != PF_STATUS_OK)
    {
        goto cleanup;
    }
    replay.bytes = pf_web_replay_bytes;
    replay.size = destination.size;
    if (pf_replay_verify(playback, replay, &verification) != PF_STATUS_OK ||
        verification.status != (uint32_t)PF_STATUS_OK ||
        verification.verified_ticks != PF_M2_REPLAY_TICKS ||
        verification.first_mismatch_tick != UINT64_MAX)
    {
        goto cleanup;
    }

    pf_web_replay_inspector(
        pf_web_replay_positions,
        pf_web_hash_bytes,
        (int)PF_M2_REPLAY_TICKS,
        (int)PF_M2_REPLAY_PLAYERS,
        (int)result.winner_mask,
        PF_M2_REPLAY_FINAL_SHA256);
    passed = 1;

cleanup:
    if (playback != NULL)
    {
        (void)pf_sim_deinit(playback);
    }
    if (source != NULL)
    {
        (void)pf_sim_deinit(source);
    }
    if (initial != NULL)
    {
        (void)pf_sim_deinit(initial);
    }
    return passed;
}
