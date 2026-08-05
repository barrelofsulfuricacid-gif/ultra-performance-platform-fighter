#include "pf/replay.h"

#include "sim_internal.h"
#include "sim_sha256.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PF_REPLAY_CONTAINER_HEADER_BYTES ((size_t)40)
#define PF_REPLAY_CHUNK_HEADER_BYTES ((size_t)48)
#define PF_REPLAY_MATCH_PAYLOAD_BYTES ((size_t)100)
#define PF_REPLAY_INPUT_PREFIX_BYTES ((size_t)8)
#define PF_REPLAY_INPUT_FRAME_BYTES ((size_t)32)
#define PF_REPLAY_HASH_PREFIX_BYTES ((size_t)8)
#define PF_REPLAY_STATE_HASH_BYTES ((size_t)40)
#define PF_REPLAY_RESULT_PAYLOAD_BYTES ((size_t)16)
#define PF_REPLAY_REQUIRED_CHUNK_COUNT UINT32_C(5)

#define PF_REPLAY_CHUNK_MATCH UINT16_C(1)
#define PF_REPLAY_CHUNK_INITIAL_STATE UINT16_C(2)
#define PF_REPLAY_CHUNK_INPUTS UINT16_C(3)
#define PF_REPLAY_CHUNK_STATE_HASHES UINT16_C(4)
#define PF_REPLAY_CHUNK_RESULT UINT16_C(5)
#define PF_REPLAY_CHUNK_VERSION UINT16_C(1)
#define PF_REPLAY_CHUNK_REQUIRED UINT32_C(1)

_Static_assert(SIZE_MAX <= UINT64_MAX,
               "replay wire lengths require size_t no wider than uint64_t");

typedef struct pf_replay_writer
{
    uint8_t *bytes;
    size_t capacity;
    size_t position;
    pf_sha256 *hash;
    int failed;
} pf_replay_writer;

typedef struct pf_replay_reader
{
    const uint8_t *bytes;
    size_t size;
    size_t position;
    int failed;
} pf_replay_reader;

typedef struct pf_replay_slice
{
    const uint8_t *bytes;
    size_t size;
} pf_replay_slice;

typedef struct pf_replay_chunks
{
    pf_replay_slice match;
    pf_replay_slice initial_state;
    pf_replay_slice inputs;
    pf_replay_slice state_hashes;
    pf_replay_slice result;
    uint8_t initial_state_checksum[32];
} pf_replay_chunks;

typedef struct pf_replay_match
{
    uint32_t sim_abi;
    uint16_t state_schema;
    uint16_t input_schema;
    uint16_t arithmetic_version;
    uint16_t rng_version;
    uint8_t content_hash[32];
    uint8_t config_hash[32];
    uint64_t seed;
    uint64_t tick_count;
    uint32_t tick_rate;
    uint8_t player_count;
    uint8_t mode;
} pf_replay_match;

typedef struct pf_replay_layout
{
    size_t initial_state_bytes;
    size_t input_payload_bytes;
    size_t hash_payload_bytes;
    size_t total_bytes;
} pf_replay_layout;

static const uint8_t pf_replay_magic[8] = {
    UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x52), UINT8_C(0x45),
    UINT8_C(0x50), UINT8_C(0x4c), UINT8_C(0x30), UINT8_C(0x31)};

static int pf_replay_add_size(
    size_t left,
    size_t right,
    size_t *out_value)
{
    if (left > SIZE_MAX - right)
    {
        return 0;
    }
    *out_value = left + right;
    return 1;
}

static int pf_replay_multiply_size(
    size_t left,
    size_t right,
    size_t *out_value)
{
    if (left != (size_t)0 && right > SIZE_MAX / left)
    {
        return 0;
    }
    *out_value = left * right;
    return 1;
}

static void pf_replay_writer_bytes(
    pf_replay_writer *writer,
    const uint8_t *bytes,
    size_t byte_count)
{
    if (writer->failed != 0)
    {
        return;
    }
    if (bytes == NULL ||
        writer->position > writer->capacity ||
        byte_count > writer->capacity - writer->position)
    {
        writer->failed = 1;
        return;
    }

    if (writer->bytes != NULL)
    {
        (void)memcpy(
            &writer->bytes[writer->position],
            bytes,
            byte_count);
    }
    if (writer->hash != NULL)
    {
        pf_sha256_update(writer->hash, bytes, byte_count);
    }
    writer->position += byte_count;
}

static void pf_replay_writer_u8(
    pf_replay_writer *writer,
    uint8_t value)
{
    pf_replay_writer_bytes(writer, &value, sizeof(value));
}

static void pf_replay_writer_u16(
    pf_replay_writer *writer,
    uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    pf_replay_writer_bytes(writer, bytes, sizeof(bytes));
}

static void pf_replay_writer_u32(
    pf_replay_writer *writer,
    uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
    pf_replay_writer_bytes(writer, bytes, sizeof(bytes));
}

static void pf_replay_writer_u64(
    pf_replay_writer *writer,
    uint64_t value)
{
    uint8_t bytes[8];
    uint32_t byte_index;

    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(8);
         ++byte_index)
    {
        bytes[byte_index] =
            (uint8_t)(value >> (UINT32_C(8) * byte_index));
    }
    pf_replay_writer_bytes(writer, bytes, sizeof(bytes));
}

static void pf_replay_writer_i16(
    pf_replay_writer *writer,
    int16_t value)
{
    pf_replay_writer_u16(writer, (uint16_t)value);
}

static uint8_t pf_replay_reader_u8(pf_replay_reader *reader)
{
    if (reader->failed != 0 || reader->position >= reader->size)
    {
        reader->failed = 1;
        return UINT8_C(0);
    }
    return reader->bytes[reader->position++];
}

static uint16_t pf_replay_reader_u16(pf_replay_reader *reader)
{
    uint16_t value;

    if (reader->failed != 0 ||
        reader->position > reader->size ||
        (size_t)2 > reader->size - reader->position)
    {
        reader->failed = 1;
        return UINT16_C(0);
    }

    value = (uint16_t)reader->bytes[reader->position];
    value |=
        (uint16_t)((uint16_t)reader->bytes[
                       reader->position + (size_t)1]
                   << 8U);
    reader->position += (size_t)2;
    return value;
}

static uint32_t pf_replay_reader_u32(pf_replay_reader *reader)
{
    uint32_t value;
    uint32_t byte_index;

    if (reader->failed != 0 ||
        reader->position > reader->size ||
        (size_t)4 > reader->size - reader->position)
    {
        reader->failed = 1;
        return UINT32_C(0);
    }

    value = UINT32_C(0);
    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(4);
         ++byte_index)
    {
        value |=
            (uint32_t)reader->bytes[
                reader->position + (size_t)byte_index]
            << (UINT32_C(8) * byte_index);
    }
    reader->position += (size_t)4;
    return value;
}

static uint64_t pf_replay_reader_u64(pf_replay_reader *reader)
{
    uint64_t value;
    uint32_t byte_index;

    if (reader->failed != 0 ||
        reader->position > reader->size ||
        (size_t)8 > reader->size - reader->position)
    {
        reader->failed = 1;
        return UINT64_C(0);
    }

    value = UINT64_C(0);
    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(8);
         ++byte_index)
    {
        value |=
            (uint64_t)reader->bytes[
                reader->position + (size_t)byte_index]
            << (UINT32_C(8) * byte_index);
    }
    reader->position += (size_t)8;
    return value;
}

static int16_t pf_replay_reader_i16(pf_replay_reader *reader)
{
    const uint16_t bits = pf_replay_reader_u16(reader);
    int16_t value;

    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static void pf_replay_reader_bytes(
    pf_replay_reader *reader,
    uint8_t *destination,
    size_t byte_count)
{
    if (reader->failed != 0 ||
        destination == NULL ||
        reader->position > reader->size ||
        byte_count > reader->size - reader->position)
    {
        reader->failed = 1;
        return;
    }

    (void)memcpy(
        destination,
        &reader->bytes[reader->position],
        byte_count);
    reader->position += byte_count;
}

static int pf_replay_hash_equal(
    const uint8_t left[32],
    const uint8_t right[32])
{
    uint32_t byte_index;
    uint8_t difference = UINT8_C(0);

    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(32);
         ++byte_index)
    {
        difference |= (uint8_t)(left[byte_index] ^ right[byte_index]);
    }
    return difference == UINT8_C(0);
}

static void pf_replay_hash_slice(
    pf_replay_slice slice,
    uint8_t digest[32])
{
    pf_sha256 hash;

    pf_sha256_init(&hash);
    pf_sha256_update(&hash, slice.bytes, slice.size);
    pf_sha256_finish(&hash, digest);
}

static int pf_replay_state_hash_valid(const pf_state_hash *hash)
{
    return hash != NULL &&
           hash->algorithm == PF_SIM_STATE_HASH_ALGORITHM_SHA256 &&
           hash->algorithm_version ==
               PF_SIM_STATE_HASH_ALGORITHM_VERSION &&
           hash->digest_size == PF_SIM_STATE_HASH_BYTES &&
           hash->reserved == UINT16_C(0);
}

static int pf_replay_state_hash_equal(
    const pf_state_hash *left,
    const pf_state_hash *right)
{
    return left->algorithm == right->algorithm &&
           left->algorithm_version == right->algorithm_version &&
           left->digest_size == right->digest_size &&
           left->reserved == right->reserved &&
           pf_replay_hash_equal(left->bytes, right->bytes);
}

static void pf_replay_write_input(
    pf_replay_writer *writer,
    const pf_input_frame *input)
{
    pf_replay_writer_u64(writer, input->tick);
    pf_replay_writer_u64(writer, input->buttons);
    pf_replay_writer_i16(writer, input->main_stick_x);
    pf_replay_writer_i16(writer, input->main_stick_y);
    pf_replay_writer_i16(writer, input->secondary_stick_x);
    pf_replay_writer_i16(writer, input->secondary_stick_y);
    pf_replay_writer_u16(writer, input->left_trigger);
    pf_replay_writer_u16(writer, input->right_trigger);
    pf_replay_writer_u16(writer, input->schema_version);
    pf_replay_writer_u8(writer, input->player_slot);
    pf_replay_writer_u8(writer, input->reserved);
}

static pf_input_frame pf_replay_read_input(pf_replay_reader *reader)
{
    pf_input_frame input;

    (void)memset(&input, 0, sizeof(input));
    input.tick = pf_replay_reader_u64(reader);
    input.buttons = pf_replay_reader_u64(reader);
    input.main_stick_x = pf_replay_reader_i16(reader);
    input.main_stick_y = pf_replay_reader_i16(reader);
    input.secondary_stick_x = pf_replay_reader_i16(reader);
    input.secondary_stick_y = pf_replay_reader_i16(reader);
    input.left_trigger = pf_replay_reader_u16(reader);
    input.right_trigger = pf_replay_reader_u16(reader);
    input.schema_version = pf_replay_reader_u16(reader);
    input.player_slot = pf_replay_reader_u8(reader);
    input.reserved = pf_replay_reader_u8(reader);
    return input;
}

static void pf_replay_write_state_hash(
    pf_replay_writer *writer,
    const pf_state_hash *hash)
{
    pf_replay_writer_u16(writer, hash->algorithm);
    pf_replay_writer_u16(writer, hash->algorithm_version);
    pf_replay_writer_u16(writer, hash->digest_size);
    pf_replay_writer_u16(writer, hash->reserved);
    pf_replay_writer_bytes(writer, hash->bytes, sizeof(hash->bytes));
}

static pf_state_hash pf_replay_read_state_hash(
    pf_replay_reader *reader)
{
    pf_state_hash hash;

    (void)memset(&hash, 0, sizeof(hash));
    hash.algorithm = pf_replay_reader_u16(reader);
    hash.algorithm_version = pf_replay_reader_u16(reader);
    hash.digest_size = pf_replay_reader_u16(reader);
    hash.reserved = pf_replay_reader_u16(reader);
    pf_replay_reader_bytes(reader, hash.bytes, sizeof(hash.bytes));
    return hash;
}

static void pf_replay_write_result(
    pf_replay_writer *writer,
    const pf_tick_result *result)
{
    pf_replay_writer_u64(writer, result->completed_tick);
    pf_replay_writer_u32(writer, result->fault_flags);
    pf_replay_writer_u8(writer, result->terminated);
    pf_replay_writer_u8(writer, result->truncated);
    pf_replay_writer_u8(writer, result->winner_mask);
    pf_replay_writer_u8(writer, result->reserved);
}

static pf_tick_result pf_replay_read_result(
    pf_replay_reader *reader)
{
    pf_tick_result result;

    (void)memset(&result, 0, sizeof(result));
    result.completed_tick = pf_replay_reader_u64(reader);
    result.fault_flags = pf_replay_reader_u32(reader);
    result.terminated = pf_replay_reader_u8(reader);
    result.truncated = pf_replay_reader_u8(reader);
    result.winner_mask = pf_replay_reader_u8(reader);
    result.reserved = pf_replay_reader_u8(reader);
    return result;
}

static int pf_replay_result_equal(
    const pf_tick_result *left,
    const pf_tick_result *right)
{
    return left->completed_tick == right->completed_tick &&
           left->fault_flags == right->fault_flags &&
           left->terminated == right->terminated &&
           left->truncated == right->truncated &&
           left->winner_mask == right->winner_mask &&
           left->reserved == right->reserved;
}

static int pf_replay_player_mask_valid(
    uint16_t mask,
    uint8_t player_count)
{
    const uint16_t player_mask =
        (uint16_t)((UINT16_C(1) << player_count) - UINT16_C(1));

    return mask != UINT16_C(0) &&
           (mask & (uint16_t)~player_mask) == UINT16_C(0);
}

static int pf_replay_forfeit_event_valid(
    const pf_sim_event *event,
    uint8_t player_count)
{
    return event->source_player == PF_SIM_EVENT_NO_PLAYER &&
           event->target_player == PF_SIM_EVENT_NO_PLAYER &&
           event->value_q16 == UINT32_C(0) &&
           event->velocity_x_q16 == INT32_C(0) &&
           event->velocity_y_q16 == INT32_C(0) &&
           event->flags == UINT16_C(0) &&
           pf_replay_player_mask_valid(event->detail, player_count);
}

static int pf_replay_action_transition_event_valid(
    const pf_sim_event *event,
    uint8_t player_count)
{
    const uint32_t previous_actions =
        (uint32_t)event->velocity_x_q16;
    uint32_t player_index;

    if (event->source_player != PF_SIM_EVENT_NO_PLAYER ||
        event->target_player != PF_SIM_EVENT_NO_PLAYER ||
        event->velocity_y_q16 != INT32_C(0) ||
        event->flags != UINT16_C(0) ||
        !pf_replay_player_mask_valid(event->detail, player_count))
    {
        return 0;
    }

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const uint32_t shift = player_index * UINT32_C(8);
        const uint8_t previous_action =
            (uint8_t)(previous_actions >> shift);
        const uint8_t next_action =
            (uint8_t)(event->value_q16 >> shift);
        const int changed = previous_action != next_action;

        if (player_index < (uint32_t)player_count)
        {
            if (previous_action >
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
                next_action >
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
                changed !=
                    ((event->detail &
                      (uint16_t)(UINT16_C(1) << player_index)) !=
                     UINT16_C(0)))
            {
                return 0;
            }
        }
        else if (previous_action != UINT8_C(0) ||
                 next_action != UINT8_C(0))
        {
            return 0;
        }
    }
    return 1;
}

static int pf_replay_tick_events_valid(
    const pf_tick_result *result,
    uint8_t player_count)
{
    const uint16_t known_flags =
        (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE |
        (uint16_t)PF_SIM_EVENT_FLAG_ELIMINATED |
        (uint16_t)PF_SIM_EVENT_FLAG_LAST_STOCK |
        (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH |
        (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL;
    uint32_t event_index;
    uint32_t previous_sequence = UINT32_C(0);

    if (result->reserved2 != UINT8_C(0) ||
        result->reserved3 != UINT16_C(0) ||
        result->event_count > PF_SIM_MAX_EVENTS_PER_TICK ||
        (result->completed_tick == UINT64_C(0) &&
         result->event_count != UINT8_C(0)))
    {
        return 0;
    }
    for (event_index = UINT32_C(0);
         event_index < (uint32_t)result->event_count;
         ++event_index)
    {
        const pf_sim_event *event = &result->events[event_index];

        if (event->tick != result->completed_tick - UINT64_C(1) ||
            event->sequence == UINT32_C(0) ||
            (event_index != UINT32_C(0) &&
             event->sequence != previous_sequence + UINT32_C(1)) ||
            event->type <= (uint16_t)PF_SIM_EVENT_NONE ||
            event->type >
                (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
            (event->flags & (uint16_t)~known_flags) != UINT16_C(0) ||
            (event->source_player != PF_SIM_EVENT_NO_PLAYER &&
             event->source_player >= player_count) ||
            (event->target_player != PF_SIM_EVENT_NO_PLAYER &&
             event->target_player >= player_count) ||
            (event->type == (uint16_t)PF_SIM_EVENT_FORFEIT &&
             !pf_replay_forfeit_event_valid(event, player_count)) ||
            (event->type ==
                 (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS &&
             !pf_replay_action_transition_event_valid(
                 event,
                 player_count)))
        {
            return 0;
        }
        previous_sequence = event->sequence;
    }
    return 1;
}

static void pf_replay_world_result(
    const pf_world_state *world,
    pf_tick_result *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->completed_tick = world->tick;
    result->fault_flags = world->fault_flags;
    result->terminated = world->terminated;
    result->truncated = world->truncated;
    result->winner_mask = world->winner_mask;
}

static int pf_replay_input_valid(
    const pf_input_frame *input,
    uint64_t tick,
    uint8_t player_slot)
{
    return input->tick == tick &&
           input->buttons ==
               (input->buttons & PF_INPUT_KNOWN_BUTTONS) &&
           input->schema_version == PF_SIM_INPUT_SCHEMA_VERSION &&
           input->player_slot == player_slot &&
           input->reserved == UINT8_C(0);
}

static pf_status pf_replay_calculate_layout(
    const pf_replay_source *source,
    pf_replay_layout *out_layout)
{
    const uint32_t known_faults =
        (uint32_t)PF_SIM_FAULT_ARITHMETIC |
        (uint32_t)PF_SIM_FAULT_CAPACITY |
        (uint32_t)PF_SIM_FAULT_INVALID_STATE;
    pf_replay_layout layout;
    pf_state_hash initial_hash;
    size_t tick_count;
    size_t expected_input_count;
    size_t expected_hash_count;
    size_t payload_bytes;
    size_t total_bytes;
    size_t frame_index;
    size_t hash_index;
    pf_status status;

    if (source == NULL || out_layout == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (source->struct_size != (uint32_t)sizeof(*source) ||
        source->schema_version != PF_REPLAY_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (source->flags != PF_REPLAY_FLAG_PER_TICK_HASHES ||
        !pf_sim_is_valid(source->initial_state) ||
        source->initial_state->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (source->initial_state->world.tick != UINT64_C(0) ||
        source->initial_state->world.fault_flags != UINT32_C(0) ||
        source->initial_state->world.terminated != UINT8_C(0) ||
        source->initial_state->world.truncated != UINT8_C(0) ||
        source->tick_count > source->initial_state->world.max_ticks ||
        source->tick_count == UINT64_MAX)
    {
        return PF_STATUS_INVALID_STATE;
    }

    status = pf_sim_snapshot_validate_world(
        &source->initial_state->world);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    status = pf_sim_query_save_size(
        source->initial_state,
        &layout.initial_state_bytes);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (source->tick_count > (uint64_t)SIZE_MAX)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    tick_count = (size_t)source->tick_count;
    if (!pf_replay_multiply_size(
            tick_count,
            (size_t)source->initial_state->world.player_count,
            &expected_input_count) ||
        !pf_replay_add_size(tick_count, (size_t)1, &expected_hash_count))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (source->input_frame_count != expected_input_count ||
        source->state_hash_count != expected_hash_count ||
        (expected_input_count != (size_t)0 &&
         source->input_frames == NULL) ||
        source->state_hashes == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    status = pf_sim_hash(source->initial_state, &initial_hash);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (!pf_replay_state_hash_valid(&source->state_hashes[0]) ||
        !pf_replay_state_hash_equal(
            &initial_hash,
            &source->state_hashes[0]))
    {
        return PF_STATUS_INVALID_STATE;
    }

    for (frame_index = (size_t)0;
         frame_index < expected_input_count;
         ++frame_index)
    {
        const uint64_t tick =
            (uint64_t)(frame_index /
                       (size_t)source->initial_state->world.player_count);
        const uint8_t player_slot =
            (uint8_t)(frame_index %
                      (size_t)source->initial_state->world.player_count);
        if (!pf_replay_input_valid(
                &source->input_frames[frame_index],
                tick,
                player_slot))
        {
            return PF_STATUS_INVALID_ARGUMENT;
        }
    }
    for (hash_index = (size_t)0;
         hash_index < expected_hash_count;
         ++hash_index)
    {
        if (!pf_replay_state_hash_valid(
                &source->state_hashes[hash_index]))
        {
            return PF_STATUS_INVALID_ARGUMENT;
        }
    }

    if (source->final_result.completed_tick != source->tick_count ||
        source->final_result.reserved != UINT8_C(0) ||
        !pf_replay_tick_events_valid(
            &source->final_result,
            source->initial_state->world.player_count) ||
        source->final_result.terminated > UINT8_C(1) ||
        source->final_result.truncated > UINT8_C(1) ||
        (source->final_result.fault_flags & ~known_faults) !=
            UINT32_C(0) ||
        (source->final_result.terminated == UINT8_C(0) &&
         source->final_result.winner_mask != UINT8_C(0)) ||
        (source->final_result.winner_mask &
         (uint8_t)~((UINT32_C(1) <<
                     source->initial_state->world.player_count) -
                    UINT32_C(1))) != UINT8_C(0) ||
        (source->tick_count ==
             source->initial_state->world.max_ticks &&
         source->final_result.truncated == UINT8_C(0)) ||
        (source->tick_count <
             source->initial_state->world.max_ticks &&
         source->final_result.truncated != UINT8_C(0)))
    {
        return PF_STATUS_INVALID_STATE;
    }

    if (!pf_replay_multiply_size(
            expected_input_count,
            PF_REPLAY_INPUT_FRAME_BYTES,
            &payload_bytes) ||
        !pf_replay_add_size(
            PF_REPLAY_INPUT_PREFIX_BYTES,
            payload_bytes,
            &layout.input_payload_bytes) ||
        !pf_replay_multiply_size(
            expected_hash_count,
            PF_REPLAY_STATE_HASH_BYTES,
            &payload_bytes) ||
        !pf_replay_add_size(
            PF_REPLAY_HASH_PREFIX_BYTES,
            payload_bytes,
            &layout.hash_payload_bytes) ||
        !pf_replay_multiply_size(
            (size_t)PF_REPLAY_REQUIRED_CHUNK_COUNT,
            PF_REPLAY_CHUNK_HEADER_BYTES,
            &total_bytes) ||
        !pf_replay_add_size(
            PF_REPLAY_CONTAINER_HEADER_BYTES,
            total_bytes,
            &total_bytes) ||
        !pf_replay_add_size(
            total_bytes,
            PF_REPLAY_MATCH_PAYLOAD_BYTES,
            &total_bytes) ||
        !pf_replay_add_size(
            total_bytes,
            layout.initial_state_bytes,
            &total_bytes) ||
        !pf_replay_add_size(
            total_bytes,
            layout.input_payload_bytes,
            &total_bytes) ||
        !pf_replay_add_size(
            total_bytes,
            layout.hash_payload_bytes,
            &total_bytes) ||
        !pf_replay_add_size(
            total_bytes,
            PF_REPLAY_RESULT_PAYLOAD_BYTES,
            &total_bytes))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    layout.total_bytes = total_bytes;
    *out_layout = layout;
    return PF_STATUS_OK;
}

static void pf_replay_write_container_header(
    pf_replay_writer *writer,
    const pf_replay_layout *layout)
{
    pf_replay_writer_bytes(
        writer,
        pf_replay_magic,
        sizeof(pf_replay_magic));
    pf_replay_writer_u16(writer, PF_REPLAY_FORMAT_VERSION);
    pf_replay_writer_u16(
        writer,
        (uint16_t)PF_REPLAY_CONTAINER_HEADER_BYTES);
    pf_replay_writer_u32(writer, PF_SIM_ABI_VERSION);
    pf_replay_writer_u32(writer, PF_SIM_TICK_RATE_HZ);
    pf_replay_writer_u32(
        writer,
        (uint32_t)PF_REPLAY_FLAG_PER_TICK_HASHES);
    pf_replay_writer_u64(writer, (uint64_t)layout->total_bytes);
    pf_replay_writer_u32(writer, PF_REPLAY_REQUIRED_CHUNK_COUNT);
    pf_replay_writer_u32(writer, UINT32_C(0));
}

static void pf_replay_write_chunk_header(
    pf_replay_writer *writer,
    uint16_t chunk_type,
    size_t payload_bytes,
    const uint8_t checksum[32])
{
    pf_replay_writer_u16(writer, chunk_type);
    pf_replay_writer_u16(writer, PF_REPLAY_CHUNK_VERSION);
    pf_replay_writer_u32(writer, PF_REPLAY_CHUNK_REQUIRED);
    pf_replay_writer_u64(writer, (uint64_t)payload_bytes);
    pf_replay_writer_bytes(writer, checksum, (size_t)32);
}

static void pf_replay_write_match(
    pf_replay_writer *writer,
    const pf_replay_source *source)
{
    uint8_t config_hash[32];

    pf_sim_snapshot_config_hash(
        &source->initial_state->world,
        config_hash);
    pf_replay_writer_u32(writer, PF_SIM_ABI_VERSION);
    pf_replay_writer_u16(writer, PF_SIM_STATE_SCHEMA_VERSION);
    pf_replay_writer_u16(writer, PF_SIM_INPUT_SCHEMA_VERSION);
    pf_replay_writer_u16(writer, PF_SIM_ARITHMETIC_VERSION);
    pf_replay_writer_u16(writer, PF_SIM_RNG_VERSION);
    pf_replay_writer_bytes(
        writer,
        source->initial_state->world.content_hash.bytes,
        sizeof(source->initial_state->world.content_hash.bytes));
    pf_replay_writer_bytes(writer, config_hash, sizeof(config_hash));
    pf_replay_writer_u64(writer, source->initial_state->world.seed);
    pf_replay_writer_u64(writer, source->tick_count);
    pf_replay_writer_u32(writer, PF_SIM_TICK_RATE_HZ);
    pf_replay_writer_u8(
        writer,
        source->initial_state->world.player_count);
    pf_replay_writer_u8(writer, source->initial_state->world.mode);
    pf_replay_writer_u16(writer, UINT16_C(0));
}

static void pf_replay_write_inputs(
    pf_replay_writer *writer,
    const pf_replay_source *source)
{
    size_t frame_index;

    pf_replay_writer_u64(
        writer,
        (uint64_t)source->input_frame_count);
    for (frame_index = (size_t)0;
         frame_index < source->input_frame_count;
         ++frame_index)
    {
        pf_replay_write_input(
            writer,
            &source->input_frames[frame_index]);
    }
}

static void pf_replay_write_hashes(
    pf_replay_writer *writer,
    const pf_replay_source *source)
{
    size_t hash_index;

    pf_replay_writer_u64(
        writer,
        (uint64_t)source->state_hash_count);
    for (hash_index = (size_t)0;
         hash_index < source->state_hash_count;
         ++hash_index)
    {
        pf_replay_write_state_hash(
            writer,
            &source->state_hashes[hash_index]);
    }
}

static void pf_replay_hash_generated_payload(
    const pf_replay_source *source,
    size_t payload_bytes,
    uint16_t chunk_type,
    uint8_t digest[32])
{
    pf_sha256 hash;
    pf_replay_writer writer;

    pf_sha256_init(&hash);
    writer.bytes = NULL;
    writer.capacity = payload_bytes;
    writer.position = (size_t)0;
    writer.hash = &hash;
    writer.failed = 0;

    switch (chunk_type)
    {
        case PF_REPLAY_CHUNK_MATCH:
            pf_replay_write_match(&writer, source);
            break;
        case PF_REPLAY_CHUNK_INPUTS:
            pf_replay_write_inputs(&writer, source);
            break;
        case PF_REPLAY_CHUNK_STATE_HASHES:
            pf_replay_write_hashes(&writer, source);
            break;
        case PF_REPLAY_CHUNK_RESULT:
            pf_replay_write_result(
                &writer,
                &source->final_result);
            break;
        default:
            writer.failed = 1;
            break;
    }

    pf_sha256_finish(&hash, digest);
}

pf_status pf_replay_query_size(
    const pf_replay_source *source,
    size_t *out_replay_bytes)
{
    pf_replay_layout layout;
    pf_status status;

    if (out_replay_bytes == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    status = pf_replay_calculate_layout(source, &layout);
    if (status != PF_STATUS_OK)
    {
        return status;
    }

    *out_replay_bytes = layout.total_bytes;
    return PF_STATUS_OK;
}

pf_status pf_replay_encode(
    const pf_replay_source *source,
    pf_mut_bytes *destination)
{
    pf_replay_layout layout;
    pf_replay_writer writer;
    pf_mut_bytes save_destination;
    pf_state_hash initial_state_hash;
    uint8_t match_checksum[32];
    uint8_t inputs_checksum[32];
    uint8_t hashes_checksum[32];
    uint8_t result_checksum[32];
    pf_status status;

    if (destination == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    destination->size = (size_t)0;
    status = pf_replay_calculate_layout(source, &layout);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    destination->size = layout.total_bytes;
    if (destination->bytes == NULL ||
        destination->capacity < layout.total_bytes)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }

    status = pf_sim_hash(
        source->initial_state,
        &initial_state_hash);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    pf_replay_hash_generated_payload(
        source,
        PF_REPLAY_MATCH_PAYLOAD_BYTES,
        PF_REPLAY_CHUNK_MATCH,
        match_checksum);
    pf_replay_hash_generated_payload(
        source,
        layout.input_payload_bytes,
        PF_REPLAY_CHUNK_INPUTS,
        inputs_checksum);
    pf_replay_hash_generated_payload(
        source,
        layout.hash_payload_bytes,
        PF_REPLAY_CHUNK_STATE_HASHES,
        hashes_checksum);
    pf_replay_hash_generated_payload(
        source,
        PF_REPLAY_RESULT_PAYLOAD_BYTES,
        PF_REPLAY_CHUNK_RESULT,
        result_checksum);

    writer.bytes = destination->bytes;
    writer.capacity = destination->capacity;
    writer.position = (size_t)0;
    writer.hash = NULL;
    writer.failed = 0;
    pf_replay_write_container_header(&writer, &layout);

    pf_replay_write_chunk_header(
        &writer,
        PF_REPLAY_CHUNK_MATCH,
        PF_REPLAY_MATCH_PAYLOAD_BYTES,
        match_checksum);
    pf_replay_write_match(&writer, source);

    pf_replay_write_chunk_header(
        &writer,
        PF_REPLAY_CHUNK_INITIAL_STATE,
        layout.initial_state_bytes,
        initial_state_hash.bytes);
    if (writer.failed != 0 ||
        writer.position > writer.capacity)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }
    save_destination.bytes = &writer.bytes[writer.position];
    save_destination.capacity = writer.capacity - writer.position;
    save_destination.size = (size_t)0;
    status = pf_sim_save(
        source->initial_state,
        &save_destination);
    if (status != PF_STATUS_OK ||
        save_destination.size != layout.initial_state_bytes)
    {
        return status;
    }
    writer.position += save_destination.size;

    pf_replay_write_chunk_header(
        &writer,
        PF_REPLAY_CHUNK_INPUTS,
        layout.input_payload_bytes,
        inputs_checksum);
    pf_replay_write_inputs(&writer, source);

    pf_replay_write_chunk_header(
        &writer,
        PF_REPLAY_CHUNK_STATE_HASHES,
        layout.hash_payload_bytes,
        hashes_checksum);
    pf_replay_write_hashes(&writer, source);

    pf_replay_write_chunk_header(
        &writer,
        PF_REPLAY_CHUNK_RESULT,
        PF_REPLAY_RESULT_PAYLOAD_BYTES,
        result_checksum);
    pf_replay_write_result(&writer, &source->final_result);

    if (writer.failed != 0 ||
        writer.position != layout.total_bytes)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_read_container_header(
    pf_bytes replay,
    pf_replay_reader *reader,
    uint32_t *out_chunk_count)
{
    uint8_t magic[8];
    uint16_t format_version;
    uint16_t header_bytes;
    uint32_t sim_abi;
    uint32_t tick_rate;
    uint32_t flags;
    uint64_t total_bytes;
    uint32_t chunk_count;
    uint32_t reserved;

    if (replay.bytes == NULL ||
        replay.size < PF_REPLAY_CONTAINER_HEADER_BYTES)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    reader->bytes = replay.bytes;
    reader->size = replay.size;
    reader->position = (size_t)0;
    reader->failed = 0;
    pf_replay_reader_bytes(reader, magic, sizeof(magic));
    format_version = pf_replay_reader_u16(reader);
    header_bytes = pf_replay_reader_u16(reader);
    sim_abi = pf_replay_reader_u32(reader);
    tick_rate = pf_replay_reader_u32(reader);
    flags = pf_replay_reader_u32(reader);
    total_bytes = pf_replay_reader_u64(reader);
    chunk_count = pf_replay_reader_u32(reader);
    reserved = pf_replay_reader_u32(reader);

    if (reader->failed != 0 ||
        reader->position != PF_REPLAY_CONTAINER_HEADER_BYTES ||
        memcmp(magic, pf_replay_magic, sizeof(magic)) != 0 ||
        total_bytes != (uint64_t)replay.size ||
        reserved != UINT32_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }
    if (format_version != PF_REPLAY_FORMAT_VERSION ||
        header_bytes !=
            (uint16_t)PF_REPLAY_CONTAINER_HEADER_BYTES ||
        sim_abi != PF_SIM_ABI_VERSION ||
        tick_rate != PF_SIM_TICK_RATE_HZ ||
        flags != (uint32_t)PF_REPLAY_FLAG_PER_TICK_HASHES)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (chunk_count < PF_REPLAY_REQUIRED_CHUNK_COUNT)
    {
        return PF_STATUS_INVALID_STATE;
    }

    *out_chunk_count = chunk_count;
    return PF_STATUS_OK;
}

static pf_status pf_replay_store_chunk(
    pf_replay_chunks *chunks,
    uint16_t chunk_type,
    pf_replay_slice payload,
    const uint8_t checksum[32])
{
    pf_replay_slice *destination;

    switch (chunk_type)
    {
        case PF_REPLAY_CHUNK_MATCH:
            destination = &chunks->match;
            break;
        case PF_REPLAY_CHUNK_INITIAL_STATE:
            destination = &chunks->initial_state;
            break;
        case PF_REPLAY_CHUNK_INPUTS:
            destination = &chunks->inputs;
            break;
        case PF_REPLAY_CHUNK_STATE_HASHES:
            destination = &chunks->state_hashes;
            break;
        case PF_REPLAY_CHUNK_RESULT:
            destination = &chunks->result;
            break;
        default:
            return PF_STATUS_OK;
    }

    if (destination->bytes != NULL)
    {
        return PF_STATUS_INVALID_STATE;
    }
    *destination = payload;
    if (chunk_type == PF_REPLAY_CHUNK_INITIAL_STATE)
    {
        (void)memcpy(
            chunks->initial_state_checksum,
            checksum,
            sizeof(chunks->initial_state_checksum));
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_scan_chunks(
    pf_bytes replay,
    pf_replay_chunks *out_chunks)
{
    pf_replay_reader reader;
    uint32_t chunk_count;
    uint32_t chunk_index;
    pf_status status;

    (void)memset(out_chunks, 0, sizeof(*out_chunks));
    status = pf_replay_read_container_header(
        replay,
        &reader,
        &chunk_count);
    if (status != PF_STATUS_OK)
    {
        return status;
    }

    for (chunk_index = UINT32_C(0);
         chunk_index < chunk_count;
         ++chunk_index)
    {
        uint16_t chunk_type;
        uint16_t chunk_version;
        uint32_t chunk_flags;
        uint64_t payload_bytes_u64;
        uint8_t stored_checksum[32];
        uint8_t computed_checksum[32];
        pf_replay_slice payload;
        int known_chunk;

        if (reader.position > reader.size ||
            PF_REPLAY_CHUNK_HEADER_BYTES >
                reader.size - reader.position)
        {
            return PF_STATUS_INVALID_STATE;
        }
        chunk_type = pf_replay_reader_u16(&reader);
        chunk_version = pf_replay_reader_u16(&reader);
        chunk_flags = pf_replay_reader_u32(&reader);
        payload_bytes_u64 = pf_replay_reader_u64(&reader);
        pf_replay_reader_bytes(
            &reader,
            stored_checksum,
            sizeof(stored_checksum));
        if (reader.failed != 0 ||
            payload_bytes_u64 > (uint64_t)SIZE_MAX)
        {
            return PF_STATUS_INVALID_STATE;
        }

        payload.size = (size_t)payload_bytes_u64;
        if (reader.position > reader.size ||
            payload.size > reader.size - reader.position)
        {
            return PF_STATUS_INVALID_STATE;
        }
        payload.bytes = &reader.bytes[reader.position];
        reader.position += payload.size;
        pf_replay_hash_slice(payload, computed_checksum);
        if (!pf_replay_hash_equal(
                stored_checksum,
                computed_checksum))
        {
            return PF_STATUS_CHECKSUM_MISMATCH;
        }

        known_chunk =
            chunk_type >= PF_REPLAY_CHUNK_MATCH &&
            chunk_type <= PF_REPLAY_CHUNK_RESULT;
        if ((chunk_flags & ~PF_REPLAY_CHUNK_REQUIRED) != UINT32_C(0))
        {
            return PF_STATUS_UNSUPPORTED_VERSION;
        }
        if (known_chunk != 0)
        {
            if (chunk_version != PF_REPLAY_CHUNK_VERSION ||
                chunk_flags != PF_REPLAY_CHUNK_REQUIRED)
            {
                return PF_STATUS_UNSUPPORTED_VERSION;
            }
            status = pf_replay_store_chunk(
                out_chunks,
                chunk_type,
                payload,
                stored_checksum);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
        else if ((chunk_flags & PF_REPLAY_CHUNK_REQUIRED) != UINT32_C(0))
        {
            return PF_STATUS_UNSUPPORTED_VERSION;
        }
    }

    if (reader.failed != 0 || reader.position != reader.size ||
        out_chunks->match.bytes == NULL ||
        out_chunks->initial_state.bytes == NULL ||
        out_chunks->inputs.bytes == NULL ||
        out_chunks->state_hashes.bytes == NULL ||
        out_chunks->result.bytes == NULL)
    {
        return PF_STATUS_INVALID_STATE;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_parse_match(
    pf_replay_slice payload,
    pf_replay_match *out_match)
{
    pf_replay_reader reader;
    uint16_t reserved;

    if (payload.size != PF_REPLAY_MATCH_PAYLOAD_BYTES)
    {
        return PF_STATUS_INVALID_STATE;
    }
    (void)memset(out_match, 0, sizeof(*out_match));
    reader.bytes = payload.bytes;
    reader.size = payload.size;
    reader.position = (size_t)0;
    reader.failed = 0;
    out_match->sim_abi = pf_replay_reader_u32(&reader);
    out_match->state_schema = pf_replay_reader_u16(&reader);
    out_match->input_schema = pf_replay_reader_u16(&reader);
    out_match->arithmetic_version = pf_replay_reader_u16(&reader);
    out_match->rng_version = pf_replay_reader_u16(&reader);
    pf_replay_reader_bytes(
        &reader,
        out_match->content_hash,
        sizeof(out_match->content_hash));
    pf_replay_reader_bytes(
        &reader,
        out_match->config_hash,
        sizeof(out_match->config_hash));
    out_match->seed = pf_replay_reader_u64(&reader);
    out_match->tick_count = pf_replay_reader_u64(&reader);
    out_match->tick_rate = pf_replay_reader_u32(&reader);
    out_match->player_count = pf_replay_reader_u8(&reader);
    out_match->mode = pf_replay_reader_u8(&reader);
    reserved = pf_replay_reader_u16(&reader);

    if (reader.failed != 0 ||
        reader.position != reader.size ||
        reserved != UINT16_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }
    if (out_match->sim_abi != PF_SIM_ABI_VERSION ||
        out_match->state_schema != PF_SIM_STATE_SCHEMA_VERSION ||
        out_match->input_schema != PF_SIM_INPUT_SCHEMA_VERSION ||
        out_match->arithmetic_version != PF_SIM_ARITHMETIC_VERSION ||
        out_match->rng_version != PF_SIM_RNG_VERSION ||
        out_match->tick_rate != PF_SIM_TICK_RATE_HZ)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_validate_match_identity(
    const pf_sim *sim,
    const pf_replay_match *match)
{
    uint8_t live_config_hash[32];

    pf_sim_snapshot_config_hash(&sim->world, live_config_hash);
    if (!pf_replay_hash_equal(
            sim->world.content_hash.bytes,
            match->content_hash) ||
        !pf_replay_hash_equal(
            live_config_hash,
            match->config_hash) ||
        sim->world.player_count != match->player_count ||
        sim->world.mode != match->mode)
    {
        return PF_STATUS_INCOMPATIBLE_STATE;
    }
    if (match->tick_count > sim->world.max_ticks ||
        match->tick_count == UINT64_MAX)
    {
        return PF_STATUS_INVALID_STATE;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_validate_inputs(
    pf_replay_slice payload,
    const pf_replay_match *match)
{
    pf_replay_reader reader;
    uint64_t frame_count;
    uint64_t expected_frame_count;
    uint64_t frame_index;

    if (match->tick_count >
        UINT64_MAX / (uint64_t)match->player_count)
    {
        return PF_STATUS_INVALID_STATE;
    }
    expected_frame_count =
        match->tick_count * (uint64_t)match->player_count;
    reader.bytes = payload.bytes;
    reader.size = payload.size;
    reader.position = (size_t)0;
    reader.failed = 0;
    frame_count = pf_replay_reader_u64(&reader);
    if (frame_count != expected_frame_count)
    {
        return PF_STATUS_INVALID_STATE;
    }

    for (frame_index = UINT64_C(0);
         frame_index < frame_count;
         ++frame_index)
    {
        const uint64_t tick =
            frame_index / (uint64_t)match->player_count;
        const uint8_t slot =
            (uint8_t)(frame_index %
                      (uint64_t)match->player_count);
        const pf_input_frame input =
            pf_replay_read_input(&reader);
        if (reader.failed != 0 ||
            !pf_replay_input_valid(&input, tick, slot))
        {
            return PF_STATUS_INVALID_STATE;
        }
    }
    if (reader.position != reader.size)
    {
        return PF_STATUS_INVALID_STATE;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_validate_hashes(
    pf_replay_slice payload,
    const pf_replay_match *match,
    const uint8_t initial_state_checksum[32])
{
    pf_replay_reader reader;
    uint64_t hash_count;
    uint64_t hash_index;

    reader.bytes = payload.bytes;
    reader.size = payload.size;
    reader.position = (size_t)0;
    reader.failed = 0;
    hash_count = pf_replay_reader_u64(&reader);
    if (hash_count != match->tick_count + UINT64_C(1))
    {
        return PF_STATUS_INVALID_STATE;
    }

    for (hash_index = UINT64_C(0);
         hash_index < hash_count;
         ++hash_index)
    {
        const pf_state_hash hash =
            pf_replay_read_state_hash(&reader);
        if (reader.failed != 0 ||
            !pf_replay_state_hash_valid(&hash) ||
            (hash_index == UINT64_C(0) &&
             !pf_replay_hash_equal(
                 hash.bytes,
                 initial_state_checksum)))
        {
            return PF_STATUS_INVALID_STATE;
        }
    }
    if (reader.position != reader.size)
    {
        return PF_STATUS_INVALID_STATE;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_parse_result(
    pf_replay_slice payload,
    const pf_replay_match *match,
    pf_tick_result *out_result)
{
    const uint32_t known_faults =
        (uint32_t)PF_SIM_FAULT_ARITHMETIC |
        (uint32_t)PF_SIM_FAULT_CAPACITY |
        (uint32_t)PF_SIM_FAULT_INVALID_STATE;
    pf_replay_reader reader;
    uint8_t active_mask;

    if (payload.size != PF_REPLAY_RESULT_PAYLOAD_BYTES)
    {
        return PF_STATUS_INVALID_STATE;
    }
    reader.bytes = payload.bytes;
    reader.size = payload.size;
    reader.position = (size_t)0;
    reader.failed = 0;
    *out_result = pf_replay_read_result(&reader);
    active_mask =
        (uint8_t)((UINT32_C(1) << match->player_count) - UINT32_C(1));

    if (reader.failed != 0 ||
        reader.position != reader.size ||
        out_result->completed_tick != match->tick_count ||
        (out_result->fault_flags & ~known_faults) != UINT32_C(0) ||
        out_result->terminated > UINT8_C(1) ||
        out_result->truncated > UINT8_C(1) ||
        out_result->reserved != UINT8_C(0) ||
        (out_result->winner_mask & (uint8_t)~active_mask) != UINT8_C(0) ||
        (out_result->terminated == UINT8_C(0) &&
         out_result->winner_mask != UINT8_C(0)))
    {
        return PF_STATUS_INVALID_STATE;
    }
    return PF_STATUS_OK;
}

static void pf_replay_verification_init(
    pf_replay_verification *verification)
{
    (void)memset(verification, 0, sizeof(*verification));
    verification->struct_size = (uint32_t)sizeof(*verification);
    verification->schema_version =
        PF_REPLAY_VERIFICATION_SCHEMA_VERSION;
    verification->first_mismatch_tick = UINT64_MAX;
}

static pf_status pf_replay_verification_return(
    pf_replay_verification *verification,
    pf_status status)
{
    verification->status = (uint32_t)status;
    return status;
}

static pf_status pf_replay_report_mismatch(
    pf_replay_verification *verification,
    uint64_t tick,
    const pf_state_hash *expected,
    const pf_state_hash *actual)
{
    verification->first_mismatch_tick = tick;
    if (expected != NULL)
    {
        verification->expected_hash = *expected;
    }
    if (actual != NULL)
    {
        verification->actual_hash = *actual;
    }
    return pf_replay_verification_return(
        verification,
        PF_STATUS_REPLAY_MISMATCH);
}

static pf_status pf_replay_observer_validate(
    const pf_replay_observer *observer)
{
    if (observer == NULL)
    {
        return PF_STATUS_OK;
    }
    if (observer->struct_size != (uint32_t)sizeof(*observer) ||
        observer->schema_version != PF_REPLAY_OBSERVER_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (observer->reserved != UINT16_C(0) ||
        observer->checkpoint == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    return PF_STATUS_OK;
}

static pf_status pf_replay_observe_checkpoint(
    const pf_replay_observer *observer,
    const pf_sim *sim,
    uint64_t replay_tick_count,
    const pf_tick_result *tick_result,
    const pf_state_hash *state_hash)
{
    if (observer == NULL)
    {
        return PF_STATUS_OK;
    }
    return observer->checkpoint(
        observer->user_data,
        sim,
        replay_tick_count,
        tick_result,
        state_hash);
}

static pf_status pf_replay_verify_internal(
    pf_sim *sim,
    pf_bytes replay,
    const pf_replay_observer *observer,
    pf_replay_verification *out_verification)
{
    pf_replay_chunks chunks;
    pf_replay_match match;
    pf_tick_result expected_result;
    pf_tick_result actual_result;
    pf_replay_reader input_reader;
    pf_replay_reader hash_reader;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_state_hash expected_hash;
    pf_state_hash actual_hash;
    pf_bytes initial_state;
    size_t expected_initial_state_bytes;
    uint64_t tick_index;
    uint32_t player_index;
    pf_status status;

    if (out_verification == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    pf_replay_verification_init(out_verification);
    status = pf_replay_observer_validate(observer);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    if (!pf_sim_is_valid(sim))
    {
        return pf_replay_verification_return(
            out_verification,
            PF_STATUS_INVALID_STATE);
    }

    status = pf_replay_scan_chunks(replay, &chunks);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    status = pf_replay_parse_match(chunks.match, &match);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    out_verification->expected_ticks = match.tick_count;
    status = pf_replay_validate_match_identity(sim, &match);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    status = pf_sim_query_save_size(
        sim,
        &expected_initial_state_bytes);
    if (status != PF_STATUS_OK ||
        chunks.initial_state.size != expected_initial_state_bytes)
    {
        return pf_replay_verification_return(
            out_verification,
            status == PF_STATUS_OK
                ? PF_STATUS_INVALID_STATE
                : status);
    }
    status = pf_replay_validate_inputs(chunks.inputs, &match);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    status = pf_replay_validate_hashes(
        chunks.state_hashes,
        &match,
        chunks.initial_state_checksum);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    status = pf_replay_parse_result(
        chunks.result,
        &match,
        &expected_result);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }

    initial_state.bytes = chunks.initial_state.bytes;
    initial_state.size = chunks.initial_state.size;
    status = pf_sim_load(sim, initial_state);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    if (sim->world.tick != UINT64_C(0) ||
        sim->world.seed != match.seed)
    {
        pf_replay_world_result(&sim->world, &actual_result);
        out_verification->actual_result = actual_result;
        return pf_replay_report_mismatch(
            out_verification,
            UINT64_C(0),
            NULL,
            NULL);
    }

    input_reader.bytes = chunks.inputs.bytes;
    input_reader.size = chunks.inputs.size;
    input_reader.position = (size_t)0;
    input_reader.failed = 0;
    (void)pf_replay_reader_u64(&input_reader);
    hash_reader.bytes = chunks.state_hashes.bytes;
    hash_reader.size = chunks.state_hashes.size;
    hash_reader.position = (size_t)0;
    hash_reader.failed = 0;
    (void)pf_replay_reader_u64(&hash_reader);

    expected_hash = pf_replay_read_state_hash(&hash_reader);
    status = pf_sim_hash(sim, &actual_hash);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }
    if (!pf_replay_state_hash_equal(
            &expected_hash,
            &actual_hash))
    {
        pf_replay_world_result(&sim->world, &actual_result);
        out_verification->actual_result = actual_result;
        return pf_replay_report_mismatch(
            out_verification,
            UINT64_C(0),
            &expected_hash,
            &actual_hash);
    }
    pf_replay_world_result(&sim->world, &actual_result);
    status = pf_replay_observe_checkpoint(
        observer,
        sim,
        match.tick_count,
        &actual_result,
        &actual_hash);
    if (status != PF_STATUS_OK)
    {
        return pf_replay_verification_return(
            out_verification,
            status);
    }

    for (tick_index = UINT64_C(0);
         tick_index < match.tick_count;
         ++tick_index)
    {
        for (player_index = UINT32_C(0);
             player_index < (uint32_t)match.player_count;
             ++player_index)
        {
            inputs[player_index] =
                pf_replay_read_input(&input_reader);
        }
        expected_hash =
            pf_replay_read_state_hash(&hash_reader);
        status = pf_sim_tick(
            sim,
            inputs,
            (size_t)match.player_count,
            &actual_result);
        out_verification->actual_result = actual_result;
        if (status != PF_STATUS_OK)
        {
            if (pf_sim_hash(sim, &actual_hash) != PF_STATUS_OK)
            {
                (void)memset(&actual_hash, 0, sizeof(actual_hash));
            }
            return pf_replay_report_mismatch(
                out_verification,
                tick_index + UINT64_C(1),
                &expected_hash,
                &actual_hash);
        }

        status = pf_sim_hash(sim, &actual_hash);
        if (status != PF_STATUS_OK)
        {
            return pf_replay_verification_return(
                out_verification,
                status);
        }
        if (!pf_replay_state_hash_equal(
                &expected_hash,
                &actual_hash))
        {
            return pf_replay_report_mismatch(
                out_verification,
                tick_index + UINT64_C(1),
                &expected_hash,
                &actual_hash);
        }
        out_verification->verified_ticks =
            tick_index + UINT64_C(1);
        status = pf_replay_observe_checkpoint(
            observer,
            sim,
            match.tick_count,
            &actual_result,
            &actual_hash);
        if (status != PF_STATUS_OK)
        {
            return pf_replay_verification_return(
                out_verification,
                status);
        }
    }

    if (input_reader.failed != 0 ||
        input_reader.position != input_reader.size ||
        hash_reader.failed != 0 ||
        hash_reader.position != hash_reader.size)
    {
        return pf_replay_verification_return(
            out_verification,
            PF_STATUS_INVALID_STATE);
    }

    pf_replay_world_result(&sim->world, &actual_result);
    out_verification->actual_result = actual_result;
    out_verification->expected_hash = expected_hash;
    out_verification->actual_hash = actual_hash;
    if (!pf_replay_result_equal(
            &expected_result,
            &actual_result))
    {
        return pf_replay_report_mismatch(
            out_verification,
            match.tick_count,
            &expected_hash,
            &actual_hash);
    }

    return pf_replay_verification_return(
        out_verification,
        PF_STATUS_OK);
}

pf_status pf_replay_verify(
    pf_sim *sim,
    pf_bytes replay,
    pf_replay_verification *out_verification)
{
    return pf_replay_verify_internal(
        sim,
        replay,
        NULL,
        out_verification);
}

pf_status pf_replay_verify_observed(
    pf_sim *sim,
    pf_bytes replay,
    const pf_replay_observer *observer,
    pf_replay_verification *out_verification)
{
    return pf_replay_verify_internal(
        sim,
        replay,
        observer,
        out_verification);
}
