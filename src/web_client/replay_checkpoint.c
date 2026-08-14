#include "replay_checkpoint.h"

#include "m2_replay_fixture.h"
#include "pf/replay.h"
#include "pf/sim.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

#define PF_WEB_REPLAY_MEMORY_BYTES 4096U
#define PF_WEB_REPLAY_MEMORY_ALIGNMENT 64U
#define PF_WEB_REPLAY_INPUT_COUNT 960U
#define PF_WEB_REPLAY_HASH_COUNT 241U
#define PF_WEB_REPLAY_CAPACITY 65536U
#define PF_WEB_REPLAY_MAX_TICKS 500U
#define PF_WEB_REPLAY_MAX_CHECKPOINTS (PF_WEB_REPLAY_MAX_TICKS + 1U)
#define PF_WEB_REPLAY_POSITION_COUNT                                \
    (PF_WEB_REPLAY_MAX_CHECKPOINTS * PF_SIM_MAX_PLAYERS * 2U)
#define PF_WEB_REPLAY_EVENT_STRIDE 10U
#define PF_WEB_REPLAY_EVENT_VALUE_COUNT                             \
    (PF_WEB_REPLAY_MAX_CHECKPOINTS *                                \
     PF_SIM_MAX_EVENTS_PER_TICK * PF_WEB_REPLAY_EVENT_STRIDE)

_Static_assert(
    PF_WEB_REPLAY_INPUT_COUNT ==
        PF_M2_REPLAY_TICKS * PF_M2_REPLAY_PLAYERS,
    "web replay input capacity must cover the fixture");
_Static_assert(
    PF_WEB_REPLAY_HASH_COUNT == PF_M2_REPLAY_TICKS + UINT64_C(1),
    "web replay hash capacity must cover tick zero");
_Static_assert(
    PF_WEB_REPLAY_MAX_TICKS == UINT64_C(500),
    "web replay visualization capacity must match the fixture config");
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
    const float *positions_f32,
    const uint8_t *hashes,
    const int32_t *event_counts,
    const float *event_values,
    const uint8_t *replay_bytes,
    int replay_size,
    int tick_count,
    int player_count,
    int winner_mask,
    const char *final_hash,
    int imported);

static pf_web_replay_storage pf_web_initial_storage;
static pf_web_replay_storage pf_web_source_storage;
static pf_web_replay_storage pf_web_playback_storage;
static pf_input_frame pf_web_replay_inputs[PF_WEB_REPLAY_INPUT_COUNT];
static pf_state_hash pf_web_replay_hashes[PF_WEB_REPLAY_HASH_COUNT];
static float pf_web_replay_positions[PF_WEB_REPLAY_POSITION_COUNT];
static uint8_t pf_web_replay_bytes[PF_WEB_REPLAY_CAPACITY];
static uint8_t pf_web_hash_bytes[
    PF_WEB_REPLAY_MAX_CHECKPOINTS * PF_SIM_STATE_HASH_BYTES];
static int32_t pf_web_replay_event_counts[PF_WEB_REPLAY_MAX_CHECKPOINTS];
static float pf_web_replay_event_values[PF_WEB_REPLAY_EVENT_VALUE_COUNT];
static char pf_web_replay_final_hash[65];

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

static int pf_web_capture_source_hash(
    const pf_sim *sim,
    size_t checkpoint_index)
{
    pf_state_hash *state_hash = &pf_web_replay_hashes[checkpoint_index];

    return pf_sim_hash(sim, state_hash) == PF_STATUS_OK;
}

typedef struct pf_web_replay_observer_context
{
    uint64_t checkpoint_count;
    uint64_t replay_tick_count;
    uint8_t player_count;
} pf_web_replay_observer_context;

static void pf_web_replay_hash_hex(
    const uint8_t hash[PF_SIM_STATE_HASH_BYTES],
    char output[65])
{
    static const char digits[] = "0123456789abcdef";
    uint32_t byte_index;

    for (byte_index = UINT32_C(0);
         byte_index < (uint32_t)PF_SIM_STATE_HASH_BYTES;
         ++byte_index)
    {
        output[(size_t)byte_index * (size_t)2] =
            digits[hash[byte_index] >> 4U];
        output[(size_t)byte_index * (size_t)2 + (size_t)1] =
            digits[hash[byte_index] & UINT8_C(0x0f)];
    }
    output[64] = '\0';
}

static pf_status pf_web_capture_verified_checkpoint(
    void *user_data,
    const pf_sim *sim,
    uint64_t replay_tick_count,
    const pf_tick_result *tick_result,
    const pf_state_hash *state_hash)
{
    pf_web_replay_observer_context *context =
        (pf_web_replay_observer_context *)user_data;
    pf_sim_observation observation;
    size_t checkpoint_index;
    uint32_t player_index;
    uint32_t event_index;

    if (context == NULL || sim == NULL || tick_result == NULL ||
        state_hash == NULL ||
        replay_tick_count > (uint64_t)PF_WEB_REPLAY_MAX_TICKS ||
        tick_result->completed_tick != context->checkpoint_count ||
        context->checkpoint_count > replay_tick_count ||
        pf_sim_observe(sim, &observation) != PF_STATUS_OK ||
        observation.tick != tick_result->completed_tick ||
        observation.player_count == UINT8_C(0) ||
        observation.player_count > (uint8_t)PF_SIM_MAX_PLAYERS)
    {
        return PF_STATUS_INVALID_STATE;
    }

    checkpoint_index = (size_t)context->checkpoint_count;
    if (checkpoint_index >= (size_t)PF_WEB_REPLAY_MAX_CHECKPOINTS)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }
    if (checkpoint_index == (size_t)0)
    {
        context->replay_tick_count = replay_tick_count;
        context->player_count = observation.player_count;
    }
    else if (context->replay_tick_count != replay_tick_count ||
             context->player_count != observation.player_count)
    {
        return PF_STATUS_INVALID_STATE;
    }

    (void)memcpy(
        &pf_web_hash_bytes[
            checkpoint_index * (size_t)PF_SIM_STATE_HASH_BYTES],
        state_hash->bytes,
        (size_t)PF_SIM_STATE_HASH_BYTES);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)observation.player_count;
         ++player_index)
    {
        const size_t position_index =
            (checkpoint_index * (size_t)observation.player_count +
             (size_t)player_index) *
            (size_t)2;

        pf_web_replay_positions[position_index] =
            observation.players[player_index].position_x_f32;
        pf_web_replay_positions[position_index + (size_t)1] =
            observation.players[player_index].position_y_f32;
    }

    pf_web_replay_event_counts[checkpoint_index] =
        (int32_t)tick_result->event_count;
    for (event_index = UINT32_C(0);
         event_index < (uint32_t)tick_result->event_count;
         ++event_index)
    {
        const pf_sim_event *event = &tick_result->events[event_index];
        const size_t base =
            (checkpoint_index * (size_t)PF_SIM_MAX_EVENTS_PER_TICK +
             (size_t)event_index) *
            (size_t)PF_WEB_REPLAY_EVENT_STRIDE;

        if (event->tick > (uint64_t)INT32_MAX ||
            event->sequence > (uint32_t)INT32_MAX)
        {
            return PF_STATUS_BUFFER_TOO_SMALL;
        }
        pf_web_replay_event_values[base] = (float)event->sequence;
        pf_web_replay_event_values[base + (size_t)1] =
            (float)event->tick;
        pf_web_replay_event_values[base + (size_t)2] =
            (float)event->type;
        pf_web_replay_event_values[base + (size_t)3] =
            (float)event->source_player;
        pf_web_replay_event_values[base + (size_t)4] =
            (float)event->target_player;
        pf_web_replay_event_values[base + (size_t)5] =
            event->value_f32;
        pf_web_replay_event_values[base + (size_t)6] =
            event->velocity_x_f32;
        pf_web_replay_event_values[base + (size_t)7] =
            event->velocity_y_f32;
        pf_web_replay_event_values[base + (size_t)8] =
            (float)event->flags;
        pf_web_replay_event_values[base + (size_t)9] =
            (float)event->detail;
    }

    context->checkpoint_count += UINT64_C(1);
    return PF_STATUS_OK;
}

static pf_status pf_web_verify_and_render_replay(
    pf_bytes replay,
    int imported)
{
    const pf_content_view content = pf_m2_replay_make_content();
    pf_sim_config config;
    pf_sim *playback = NULL;
    pf_replay_verification verification;
    pf_replay_observer observer;
    pf_web_replay_observer_context context;
    pf_status status;

    if (replay.bytes == NULL || replay.size == (size_t)0 ||
        replay.size > (size_t)INT_MAX)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    status = pf_sim_default_config(
        &config,
        PF_M2_REPLAY_PLAYERS,
        PF_SIM_MODE_TEAMS);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    config.max_ticks = (uint64_t)PF_WEB_REPLAY_MAX_TICKS;
    if (!pf_web_replay_init(
            &pf_web_playback_storage,
            &content,
            &config,
            &playback))
    {
        return PF_STATUS_INVALID_STATE;
    }

    (void)memset(pf_web_replay_positions, 0, sizeof(pf_web_replay_positions));
    (void)memset(pf_web_hash_bytes, 0, sizeof(pf_web_hash_bytes));
    (void)memset(
        pf_web_replay_event_counts,
        0,
        sizeof(pf_web_replay_event_counts));
    (void)memset(
        pf_web_replay_event_values,
        0,
        sizeof(pf_web_replay_event_values));
    (void)memset(&context, 0, sizeof(context));
    (void)memset(&observer, 0, sizeof(observer));
    observer.struct_size = (uint32_t)sizeof(observer);
    observer.schema_version = PF_REPLAY_OBSERVER_SCHEMA_VERSION;
    observer.checkpoint = pf_web_capture_verified_checkpoint;
    observer.user_data = &context;

    status = pf_replay_verify_observed(
        playback,
        replay,
        &observer,
        &verification);
    if (status == PF_STATUS_OK &&
        (verification.status != (uint32_t)PF_STATUS_OK ||
         verification.expected_ticks != context.replay_tick_count ||
         verification.verified_ticks != context.replay_tick_count ||
         verification.first_mismatch_tick != UINT64_MAX ||
         context.checkpoint_count !=
             context.replay_tick_count + UINT64_C(1) ||
         context.replay_tick_count > (uint64_t)INT_MAX))
    {
        status = PF_STATUS_INVALID_STATE;
    }
    if (status == PF_STATUS_OK)
    {
        pf_web_replay_hash_hex(
            verification.actual_hash.bytes,
            pf_web_replay_final_hash);
        pf_web_replay_inspector(
            pf_web_replay_positions,
            pf_web_hash_bytes,
            pf_web_replay_event_counts,
            pf_web_replay_event_values,
            replay.bytes,
            (int)replay.size,
            (int)context.replay_tick_count,
            (int)context.player_count,
            (int)verification.actual_result.winner_mask,
            pf_web_replay_final_hash,
            imported != 0 ? 1 : 0);
    }

    (void)pf_sim_deinit(playback);
    return status;
}

int pf_web_replay_import(
    const uint8_t *replay_bytes,
    size_t replay_size)
{
    pf_bytes replay;

    replay.bytes = replay_bytes;
    replay.size = replay_size;
    return (int)pf_web_verify_and_render_replay(replay, 1);
}

int pf_web_run_replay_checkpoint(void)
{
    const pf_content_view content = pf_m2_replay_make_content();
    pf_sim_config config;
    pf_sim *initial = NULL;
    pf_sim *source = NULL;
    pf_tick_result result;
    pf_replay_source replay_source;
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
        pf_sim_reset(initial, PF_M2_REPLAY_SEED) != PF_STATUS_OK ||
        pf_sim_clone(source, initial) != PF_STATUS_OK ||
        !pf_web_capture_source_hash(source, (size_t)0))
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
            !pf_web_capture_source_hash(
                source,
                (size_t)tick + (size_t)1))
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
    if (pf_web_verify_and_render_replay(replay, 0) != PF_STATUS_OK)
    {
        goto cleanup;
    }
    passed = 1;

cleanup:
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
