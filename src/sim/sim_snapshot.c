#include "sim_internal.h"
#include "sim_sha256.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PF_SIM_SAVE_HEADER_BYTES ((size_t)140)
#define PF_SIM_SAVE_PAYLOAD_BYTES ((size_t)479)
#define PF_SIM_SAVE_TOTAL_BYTES \
    (PF_SIM_SAVE_HEADER_BYTES + PF_SIM_SAVE_PAYLOAD_BYTES)

typedef struct pf_byte_writer
{
    uint8_t *bytes;
    size_t capacity;
    size_t position;
    pf_sha256 *hash;
    int failed;
} pf_byte_writer;

typedef struct pf_byte_reader
{
    const uint8_t *bytes;
    size_t size;
    size_t position;
    int failed;
} pf_byte_reader;

static const uint8_t pf_save_magic[8] = {
    UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x53), UINT8_C(0x41),
    UINT8_C(0x56), UINT8_C(0x45), UINT8_C(0x31), UINT8_C(0x37)};

static const uint8_t pf_config_hash_domain[8] = {
    UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x43), UINT8_C(0x46),
    UINT8_C(0x47), UINT8_C(0x30), UINT8_C(0x30), UINT8_C(0x31)};

static void pf_writer_bytes(
    pf_byte_writer *writer,
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

static void pf_writer_u8(pf_byte_writer *writer, uint8_t value)
{
    pf_writer_bytes(writer, &value, sizeof(value));
}

static void pf_writer_u16(pf_byte_writer *writer, uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    pf_writer_bytes(writer, bytes, sizeof(bytes));
}

static void pf_writer_u32(pf_byte_writer *writer, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
    pf_writer_bytes(writer, bytes, sizeof(bytes));
}

static void pf_writer_u64(pf_byte_writer *writer, uint64_t value)
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
    pf_writer_bytes(writer, bytes, sizeof(bytes));
}

static void pf_writer_i32(pf_byte_writer *writer, int32_t value)
{
    pf_writer_u32(writer, (uint32_t)value);
}

static void pf_writer_i8(pf_byte_writer *writer, int8_t value)
{
    uint8_t bits;

    (void)memcpy(&bits, &value, sizeof(bits));
    pf_writer_u8(writer, bits);
}

static uint8_t pf_reader_u8(pf_byte_reader *reader)
{
    if (reader->failed != 0 || reader->position >= reader->size)
    {
        reader->failed = 1;
        return UINT8_C(0);
    }
    return reader->bytes[reader->position++];
}

static uint16_t pf_reader_u16(pf_byte_reader *reader)
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
        (uint16_t)((uint16_t)reader->bytes[reader->position + (size_t)1]
                   << 8U);
    reader->position += (size_t)2;
    return value;
}

static uint32_t pf_reader_u32(pf_byte_reader *reader)
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

static uint64_t pf_reader_u64(pf_byte_reader *reader)
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

static int32_t pf_reader_i32(pf_byte_reader *reader)
{
    const uint32_t bits = pf_reader_u32(reader);
    int32_t value;

    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static int8_t pf_reader_i8(pf_byte_reader *reader)
{
    const uint8_t bits = pf_reader_u8(reader);
    int8_t value;

    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static void pf_reader_bytes(
    pf_byte_reader *reader,
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

static void pf_write_payload(
    pf_byte_writer *writer,
    const pf_world_state *world)
{
    uint32_t player_index;

    pf_writer_u64(writer, world->tick);
    pf_writer_u64(writer, world->seed);
    pf_writer_u64(writer, world->rng_state);
    pf_writer_u64(writer, world->max_ticks);
    pf_writer_u32(writer, world->fault_flags);
    pf_writer_u16(writer, world->state_schema_version);
    pf_writer_u16(writer, world->arithmetic_version);
    pf_writer_u16(writer, world->rng_version);
    pf_writer_u16(writer, world->input_schema_version);
    pf_writer_i32(writer, world->arena_half_width_q16);
    pf_writer_i32(writer, world->arena_ceiling_q16);
    pf_writer_u16(writer, world->respawn_delay_config_ticks);
    pf_writer_u16(
        writer,
        world->respawn_invulnerability_config_ticks);
    pf_writer_u8(writer, world->player_count);
    pf_writer_u8(writer, world->mode);
    pf_writer_u8(writer, world->stock_count);
    pf_writer_u8(writer, world->sudden_death);
    pf_writer_u8(writer, world->terminated);
    pf_writer_u8(writer, world->truncated);
    pf_writer_u8(writer, world->winner_mask);

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u64(writer, world->previous_buttons[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(writer, world->position_x_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(writer, world->position_y_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(writer, world->velocity_x_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(writer, world->velocity_y_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->action_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->respawn_count[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->respawn_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            world->respawn_invulnerability_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            world->ledge_invulnerability_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            world->ledge_regrab_lockout_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->team[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->grounded[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->active[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->stocks_remaining[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->action_state[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->support[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->air_jumps_remaining[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->short_hop_latched[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->platform_drop_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->fast_fall[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(writer, world->facing[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(writer, world->dash_direction[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(
            writer,
            world->previous_strong_direction[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->previous_dodge_down[player_index]);
    }
    pf_writer_u32(writer, world->combat_event_sequence);
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u32(writer, world->damage_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            world->pending_velocity_x_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            world->pending_velocity_y_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u32(writer, world->last_hit_sequence[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u64(writer, world->last_hit_tick[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u32(
            writer,
            world->last_hit_damage_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->hitlag_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->hitstun_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->hitlag_resume_action[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->attack_hit_mask[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->last_hit_attacker[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->tech_window_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->tech_lockout_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->shield_stun_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u32(writer, world->shield_health_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->shield_held[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->trigger_input_age[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->powershield[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->tumble[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->sdi_pulse_count[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(writer, world->sdi_direction_x[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(writer, world->sdi_direction_y[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(writer, world->tech_direction[player_index]);
    }
}

static void pf_read_payload(
    pf_byte_reader *reader,
    pf_world_state *world)
{
    uint32_t player_index;

    world->tick = pf_reader_u64(reader);
    world->seed = pf_reader_u64(reader);
    world->rng_state = pf_reader_u64(reader);
    world->max_ticks = pf_reader_u64(reader);
    world->fault_flags = pf_reader_u32(reader);
    world->state_schema_version = pf_reader_u16(reader);
    world->arithmetic_version = pf_reader_u16(reader);
    world->rng_version = pf_reader_u16(reader);
    world->input_schema_version = pf_reader_u16(reader);
    world->arena_half_width_q16 = pf_reader_i32(reader);
    world->arena_ceiling_q16 = pf_reader_i32(reader);
    world->respawn_delay_config_ticks = pf_reader_u16(reader);
    world->respawn_invulnerability_config_ticks =
        pf_reader_u16(reader);
    world->player_count = pf_reader_u8(reader);
    world->mode = pf_reader_u8(reader);
    world->stock_count = pf_reader_u8(reader);
    world->sudden_death = pf_reader_u8(reader);
    world->terminated = pf_reader_u8(reader);
    world->truncated = pf_reader_u8(reader);
    world->winner_mask = pf_reader_u8(reader);

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_buttons[player_index] = pf_reader_u64(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->position_x_q16[player_index] = pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->position_y_q16[player_index] = pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->velocity_x_q16[player_index] = pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->velocity_y_q16[player_index] = pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->action_ticks[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->respawn_count[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->respawn_ticks[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->respawn_invulnerability_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->ledge_invulnerability_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->ledge_regrab_lockout_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->team[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->grounded[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->active[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->stocks_remaining[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->action_state[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->support[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->air_jumps_remaining[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->short_hop_latched[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->platform_drop_ticks[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->fast_fall[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->facing[player_index] = pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->dash_direction[player_index] = pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_strong_direction[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_dodge_down[player_index] =
            pf_reader_u8(reader);
    }
    world->combat_event_sequence = pf_reader_u32(reader);
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->damage_q16[player_index] = pf_reader_u32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->pending_velocity_x_q16[player_index] =
            pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->pending_velocity_y_q16[player_index] =
            pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->last_hit_sequence[player_index] =
            pf_reader_u32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->last_hit_tick[player_index] = pf_reader_u64(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->last_hit_damage_q16[player_index] =
            pf_reader_u32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->hitlag_ticks[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->hitstun_ticks[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->hitlag_resume_action[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->attack_hit_mask[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->last_hit_attacker[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->tech_window_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->tech_lockout_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_stun_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_health_q16[player_index] =
            pf_reader_u32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_held[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->trigger_input_age[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->powershield[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->tumble[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->sdi_pulse_count[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->sdi_direction_x[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->sdi_direction_y[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->tech_direction[player_index] =
            pf_reader_i8(reader);
    }
}

static void pf_hash_payload(
    const pf_world_state *world,
    uint8_t digest[32])
{
    pf_sha256 hash;
    pf_byte_writer writer;

    pf_sha256_init(&hash);
    writer.bytes = NULL;
    writer.capacity = PF_SIM_SAVE_PAYLOAD_BYTES;
    writer.position = (size_t)0;
    writer.hash = &hash;
    writer.failed = 0;
    pf_write_payload(&writer, world);
    pf_sha256_finish(&hash, digest);
}

void pf_sim_snapshot_config_hash(
    const pf_world_state *world,
    uint8_t digest[32])
{
    pf_sha256 hash;
    pf_byte_writer writer;

    pf_sha256_init(&hash);
    writer.bytes = NULL;
    writer.capacity = (size_t)34;
    writer.position = (size_t)0;
    writer.hash = &hash;
    writer.failed = 0;
    pf_writer_bytes(
        &writer,
        pf_config_hash_domain,
        sizeof(pf_config_hash_domain));
    pf_writer_u16(&writer, PF_SIM_CONFIG_SCHEMA_VERSION);
    pf_writer_u8(&writer, world->player_count);
    pf_writer_u8(&writer, world->mode);
    pf_writer_u64(&writer, world->max_ticks);
    pf_writer_i32(&writer, world->arena_half_width_q16);
    pf_writer_i32(&writer, world->arena_ceiling_q16);
    pf_writer_u8(&writer, world->stock_count);
    pf_writer_u8(&writer, UINT8_C(0));
    pf_writer_u16(&writer, world->respawn_delay_config_ticks);
    pf_writer_u16(
        &writer,
        world->respawn_invulnerability_config_ticks);
    pf_sha256_finish(&hash, digest);
}

static void pf_write_header(
    pf_byte_writer *writer,
    const pf_world_state *world,
    const uint8_t config_hash[32],
    const uint8_t payload_checksum[32])
{
    pf_writer_bytes(writer, pf_save_magic, sizeof(pf_save_magic));
    pf_writer_u16(writer, PF_SIM_SAVE_FORMAT_VERSION);
    pf_writer_u16(writer, (uint16_t)PF_SIM_SAVE_HEADER_BYTES);
    pf_writer_u32(writer, PF_SIM_ABI_VERSION);
    pf_writer_u16(writer, PF_SIM_STATE_SCHEMA_VERSION);
    pf_writer_u16(writer, PF_SIM_ARITHMETIC_VERSION);
    pf_writer_u16(writer, PF_SIM_RNG_VERSION);
    pf_writer_u16(writer, PF_SIM_INPUT_SCHEMA_VERSION);
    pf_writer_u32(writer, PF_SIM_TICK_RATE_HZ);
    pf_writer_bytes(
        writer,
        world->content_hash.bytes,
        sizeof(world->content_hash.bytes));
    pf_writer_bytes(writer, config_hash, (size_t)32);
    pf_writer_u64(writer, world->tick);
    pf_writer_u32(writer, (uint32_t)PF_SIM_SAVE_PAYLOAD_BYTES);
    pf_writer_u16(writer, PF_SIM_STATE_HASH_ALGORITHM_SHA256);
    pf_writer_u16(writer, PF_SIM_STATE_HASH_ALGORITHM_VERSION);
    pf_writer_bytes(writer, payload_checksum, (size_t)32);
}

static void pf_write_save_stream(
    pf_byte_writer *writer,
    const pf_world_state *world)
{
    uint8_t config_hash[32];
    uint8_t payload_checksum[32];

    pf_sim_snapshot_config_hash(world, config_hash);
    pf_hash_payload(world, payload_checksum);
    pf_write_header(writer, world, config_hash, payload_checksum);
    pf_write_payload(writer, world);
}

static int pf_hash_equal(
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

static int pf_world_identity_equal(
    const pf_world_state *left,
    const pf_world_state *right)
{
    return pf_hash_equal(
               left->content_hash.bytes,
               right->content_hash.bytes) &&
           left->max_ticks == right->max_ticks &&
           left->state_schema_version == right->state_schema_version &&
           left->arithmetic_version == right->arithmetic_version &&
           left->rng_version == right->rng_version &&
           left->input_schema_version == right->input_schema_version &&
           left->arena_half_width_q16 == right->arena_half_width_q16 &&
           left->arena_ceiling_q16 == right->arena_ceiling_q16 &&
           left->stock_count == right->stock_count &&
           left->respawn_delay_config_ticks ==
               right->respawn_delay_config_ticks &&
           left->respawn_invulnerability_config_ticks ==
               right->respawn_invulnerability_config_ticks &&
           left->player_count == right->player_count &&
           left->mode == right->mode;
}

static int pf_m4_snapshot_action_is_surface_tech(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_WALL_TECH ||
           action == (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP ||
           action == (uint8_t)PF_M4_ACTION_CEILING_TECH;
}

static int pf_m4_snapshot_action_is_surface_bounce(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_WALL_BOUNCE ||
           action == (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
}

static int pf_m4_snapshot_action_is_shield_break(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
           action ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
           action ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND ||
           action ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN;
}

static int pf_m4_player_state_consistent(
    const pf_world_state *world,
    uint32_t player_index)
{
    const uint8_t grounded = world->grounded[player_index];
    const uint8_t action = world->action_state[player_index];
    const uint8_t support = world->support[player_index];

    if (world->active[player_index] == UINT8_C(0))
    {
        const int waiting =
            action == (uint8_t)PF_M4_ACTION_RESPAWN_WAIT;
        const int eliminated =
            action == (uint8_t)PF_M4_ACTION_ELIMINATED;

        return (waiting || eliminated) &&
               grounded == UINT8_C(0) &&
               support == (uint8_t)PF_M4_SURFACE_NONE &&
               world->velocity_x_q16[player_index] == INT32_C(0) &&
               world->velocity_y_q16[player_index] == INT32_C(0) &&
               world->fast_fall[player_index] == UINT8_C(0) &&
               world->dash_direction[player_index] == INT8_C(0) &&
               world->hitlag_ticks[player_index] == UINT16_C(0) &&
               world->hitstun_ticks[player_index] == UINT16_C(0) &&
               world->shield_stun_ticks[player_index] == UINT16_C(0) &&
               world->hitlag_resume_action[player_index] == UINT8_C(0) &&
               world->pending_velocity_x_q16[player_index] ==
                   INT32_C(0) &&
               world->pending_velocity_y_q16[player_index] ==
                   INT32_C(0) &&
               world->respawn_invulnerability_ticks[player_index] ==
                   UINT16_C(0) &&
               world->ledge_invulnerability_ticks[player_index] ==
                   UINT16_C(0) &&
               world->ledge_regrab_lockout_ticks[player_index] ==
                   UINT16_C(0) &&
               ((waiting &&
                 world->respawn_ticks[player_index] > UINT16_C(0) &&
                 (world->stock_count == UINT8_C(0) ||
                  world->stocks_remaining[player_index] > UINT8_C(0))) ||
                (eliminated &&
                 world->stock_count != UINT8_C(0) &&
                 world->stocks_remaining[player_index] == UINT8_C(0) &&
                 world->respawn_ticks[player_index] == UINT16_C(0)));
    }
    if (grounded != UINT8_C(0))
    {
        return support != (uint8_t)PF_M4_SURFACE_NONE &&
               action != (uint8_t)PF_M4_ACTION_AIRBORNE &&
               action != (uint8_t)PF_M4_ACTION_SHIELD_BREAK &&
               action != (uint8_t)PF_M4_ACTION_AIR_DODGE &&
               action != (uint8_t)PF_M4_ACTION_FALL_SPECIAL &&
               action != (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
               action !=
                   (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
               action != (uint8_t)PF_M4_ACTION_LEDGE_HANG &&
               action != (uint8_t)PF_M4_ACTION_LEDGE_CLIMB &&
               world->velocity_y_q16[player_index] == INT32_C(0) &&
               world->fast_fall[player_index] == UINT8_C(0);
    }
    if (support != (uint8_t)PF_M4_SURFACE_NONE)
    {
        return 0;
    }
    if (action == (uint8_t)PF_M4_ACTION_AIRBORNE ||
        action == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        action == (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        action == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        action == (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        action == (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK)
    {
        return 1;
    }
    if (action == (uint8_t)PF_M4_ACTION_HITLAG ||
        action == (uint8_t)PF_M4_ACTION_HITSTUN ||
        pf_m4_snapshot_action_is_surface_tech(action) ||
        pf_m4_snapshot_action_is_surface_bounce(action))
    {
        return 1;
    }
    return (action == (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
            action == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB) &&
           world->velocity_x_q16[player_index] == INT32_C(0) &&
           world->velocity_y_q16[player_index] == INT32_C(0) &&
           world->fast_fall[player_index] == UINT8_C(0);
}

pf_status pf_sim_snapshot_validate_world(const pf_world_state *world)
{
    const uint32_t known_faults =
        (uint32_t)PF_SIM_FAULT_ARITHMETIC |
        (uint32_t)PF_SIM_FAULT_CAPACITY |
        (uint32_t)PF_SIM_FAULT_INVALID_STATE;
    uint32_t player_index;
    uint8_t active_mask;
    uint8_t ledge_claims = UINT8_C(0);

    if (world == NULL ||
        world->state_schema_version != PF_SIM_STATE_SCHEMA_VERSION ||
        world->arithmetic_version != PF_SIM_ARITHMETIC_VERSION ||
        world->rng_version != PF_SIM_RNG_VERSION ||
        world->input_schema_version != PF_SIM_INPUT_SCHEMA_VERSION ||
        (world->fault_flags & ~known_faults) != UINT32_C(0) ||
        world->max_ticks == UINT64_C(0) ||
        world->max_ticks == UINT64_MAX ||
        world->tick > world->max_ticks ||
        (world->mode == (uint8_t)PF_SIM_MODE_DUEL &&
         world->player_count != UINT8_C(2)) ||
        (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
         world->player_count != UINT8_C(4)) ||
        (world->mode != (uint8_t)PF_SIM_MODE_DUEL &&
         world->mode != (uint8_t)PF_SIM_MODE_TEAMS) ||
        world->arena_half_width_q16 < INT32_C(16) * PF_Q16_ONE ||
        world->arena_half_width_q16 > INT32_C(4096) * PF_Q16_ONE ||
        world->arena_ceiling_q16 < INT32_C(16) * PF_Q16_ONE ||
        world->arena_ceiling_q16 > INT32_C(4096) * PF_Q16_ONE ||
        world->stock_count > PF_SIM_MAX_STOCK_COUNT ||
        world->respawn_delay_config_ticks > PF_SIM_MAX_RESPAWN_TICKS ||
        world->respawn_invulnerability_config_ticks >
            PF_SIM_MAX_RESPAWN_TICKS ||
        world->sudden_death > UINT8_C(1) ||
        (world->sudden_death != UINT8_C(0) &&
         world->stock_count == UINT8_C(0)) ||
        world->reserved != UINT8_C(0) ||
        world->terminated > UINT8_C(1) ||
        world->truncated > UINT8_C(1))
    {
        return PF_STATUS_INVALID_STATE;
    }

    active_mask =
        (uint8_t)((UINT32_C(1) << world->player_count) - UINT32_C(1));
    if ((world->winner_mask & (uint8_t)~active_mask) != UINT8_C(0) ||
        (world->terminated == UINT8_C(0) &&
         world->winner_mask != UINT8_C(0)) ||
        (world->terminated != UINT8_C(0) &&
         world->truncated != UINT8_C(0)) ||
        (world->tick < world->max_ticks &&
         world->truncated != UINT8_C(0)) ||
        (world->tick == world->max_ticks &&
         world->terminated == UINT8_C(0) &&
         world->truncated != UINT8_C(1)))
    {
        return PF_STATUS_INVALID_STATE;
    }

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        if (player_index < (uint32_t)world->player_count)
        {
            const uint8_t expected_team =
                world->mode == (uint8_t)PF_SIM_MODE_TEAMS
                    ? (uint8_t)(player_index & UINT32_C(1))
                    : (uint8_t)player_index;
            const uint8_t action =
                world->action_state[player_index];
            const uint16_t hitlag =
                world->hitlag_ticks[player_index];
            const uint16_t hitstun =
                world->hitstun_ticks[player_index];
            const uint16_t tech_window =
                world->tech_window_ticks[player_index];
            const uint16_t tech_lockout =
                world->tech_lockout_ticks[player_index];
            const uint16_t shield_stun =
                world->shield_stun_ticks[player_index];
            const uint32_t shield_health =
                world->shield_health_q16[player_index];
            const uint8_t resume_action =
                world->hitlag_resume_action[player_index];
            const uint8_t powershield =
                world->powershield[player_index];
            const uint8_t tumble = world->tumble[player_index];
            const int8_t tech_direction =
                world->tech_direction[player_index];
            if (world->active[player_index] > UINT8_C(1) ||
                world->team[player_index] != expected_team ||
                world->grounded[player_index] > UINT8_C(1) ||
                (world->previous_buttons[player_index] &
                 ~PF_INPUT_KNOWN_BUTTONS) != UINT64_C(0) ||
                world->position_x_q16[player_index] <
                    -world->arena_half_width_q16 ||
                world->position_x_q16[player_index] >
                    world->arena_half_width_q16 ||
                world->position_y_q16[player_index] < INT32_C(0) ||
                world->position_y_q16[player_index] >
                    world->arena_ceiling_q16 ||
                world->velocity_x_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->velocity_x_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->velocity_y_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->velocity_y_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->action_ticks[player_index] > UINT16_C(600) ||
                action >
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
                world->respawn_ticks[player_index] >
                    (world->respawn_delay_config_ticks != UINT16_C(0)
                         ? world->respawn_delay_config_ticks
                         : UINT16_C(1)) ||
                world->respawn_invulnerability_ticks[player_index] >
                    world->respawn_invulnerability_config_ticks ||
                world->ledge_invulnerability_ticks[player_index] >
                    UINT16_C(600) ||
                world->ledge_regrab_lockout_ticks[player_index] >
                    UINT16_C(600) ||
                (world->active[player_index] != UINT8_C(0) &&
                 world->respawn_ticks[player_index] != UINT16_C(0)) ||
                (world->stock_count == UINT8_C(0) &&
                 world->stocks_remaining[player_index] != UINT8_C(0)) ||
                (world->stock_count != UINT8_C(0) &&
                 world->sudden_death == UINT8_C(0) &&
                 world->stocks_remaining[player_index] >
                     world->stock_count) ||
                (world->sudden_death != UINT8_C(0) &&
                 world->stocks_remaining[player_index] > UINT8_C(1)) ||
                (world->active[player_index] != UINT8_C(0) &&
                 world->stock_count != UINT8_C(0) &&
                 world->stocks_remaining[player_index] == UINT8_C(0)) ||
                world->support[player_index] >
                    (uint8_t)PF_M4_SURFACE_SOLID_TOP ||
                world->air_jumps_remaining[player_index] > UINT8_C(8) ||
                world->short_hop_latched[player_index] > UINT8_C(1) ||
                world->platform_drop_ticks[player_index] > UINT8_C(120) ||
                world->fast_fall[player_index] > UINT8_C(1) ||
                (world->facing[player_index] != INT8_C(-1) &&
                 world->facing[player_index] != INT8_C(1)) ||
                world->dash_direction[player_index] < INT8_C(-1) ||
                world->dash_direction[player_index] > INT8_C(1) ||
                world->previous_strong_direction[player_index] <
                    INT8_C(-1) ||
                world->previous_strong_direction[player_index] >
                    INT8_C(1) ||
                world->previous_dodge_down[player_index] >
                    UINT8_C(1) ||
                world->damage_q16[player_index] >
                    PF_SIM_MAX_DAMAGE_Q16 ||
                world->pending_velocity_x_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->pending_velocity_x_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->pending_velocity_y_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->pending_velocity_y_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->last_hit_sequence[player_index] >
                    world->combat_event_sequence ||
                world->last_hit_damage_q16[player_index] >
                    PF_SIM_MAX_DAMAGE_Q16 ||
                hitlag > UINT16_C(120) ||
                hitstun > PF_SIM_MAX_HITSTUN_TICKS ||
                tech_window > UINT16_C(120) ||
                tech_lockout > UINT16_C(240) ||
                tech_window > tech_lockout ||
                shield_stun > UINT16_C(600) ||
                shield_health >
                    PF_SIM_MAX_SHIELD_HEALTH_Q16 ||
                world->shield_held[player_index] > UINT8_C(1) ||
                powershield > UINT8_C(1) ||
                tumble > UINT8_C(1) ||
                world->sdi_pulse_count[player_index] >
                    UINT8_C(120) ||
                world->sdi_direction_x[player_index] < INT8_C(-1) ||
                world->sdi_direction_x[player_index] > INT8_C(1) ||
                world->sdi_direction_y[player_index] < INT8_C(-1) ||
                world->sdi_direction_y[player_index] > INT8_C(1) ||
                tech_direction < INT8_C(-1) ||
                tech_direction > INT8_C(1) ||
                (world->attack_hit_mask[player_index] &
                 (uint8_t)~active_mask) != UINT8_C(0) ||
                (world->attack_hit_mask[player_index] &
                 (uint8_t)(UINT32_C(1) << player_index)) != UINT8_C(0) ||
                ((action ==
                      (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
                  action ==
                      (uint8_t)PF_M4_ACTION_RUN_TURNAROUND) &&
                 world->dash_direction[player_index] == INT8_C(0)) ||
                ((action ==
                      (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
                  action ==
                      (uint8_t)PF_M4_ACTION_RUN_TURNAROUND) &&
                 world->dash_direction[player_index] !=
                     world->facing[player_index]) ||
                (action !=
                     (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                 action !=
                     (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
                 world->dash_direction[player_index] != INT8_C(0)) ||
                (world->short_hop_latched[player_index] != UINT8_C(0) &&
                 action !=
                     (uint8_t)PF_M4_ACTION_JUMP_SQUAT) ||
                ((action == (uint8_t)PF_M4_ACTION_HITLAG) !=
                 (hitlag > UINT16_C(0))) ||
                (hitlag > UINT16_C(0) &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_STRONG_ATTACK &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
                 resume_action != (uint8_t)PF_M4_ACTION_HITSTUN &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_SHIELD_BREAK) ||
                (hitlag == UINT16_C(0) &&
                 resume_action != UINT8_C(0)) ||
                ((resume_action ==
                      (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_GETUP_ATTACK) &&
                 (hitstun != UINT16_C(0) ||
                  (resume_action !=
                       (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
                   resume_action !=
                       (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
                   world->grounded[player_index] == UINT8_C(0)) ||
                  ((resume_action ==
                        (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
                    resume_action ==
                        (uint8_t)
                            PF_M4_ACTION_STRONG_AERIAL_ATTACK) &&
                   world->grounded[player_index] != UINT8_C(0)) ||
                  world->pending_velocity_x_q16[player_index] !=
                      INT32_C(0) ||
                  world->pending_velocity_y_q16[player_index] !=
                      INT32_C(0))) ||
                (resume_action == (uint8_t)PF_M4_ACTION_HITSTUN &&
                 (hitstun == UINT16_C(0) ||
                  (world->pending_velocity_x_q16[player_index] ==
                       INT32_C(0) &&
                   world->pending_velocity_y_q16[player_index] ==
                       INT32_C(0)))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 (shield_stun == UINT16_C(0) ||
                  shield_health == UINT32_C(0) ||
                  hitstun != UINT16_C(0) ||
                  world->grounded[player_index] == UINT8_C(0) ||
                  world->pending_velocity_x_q16[player_index] !=
                      INT32_C(0) ||
                  world->pending_velocity_y_q16[player_index] !=
                      INT32_C(0))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_SHIELD_BREAK &&
                 (shield_stun != UINT16_C(0) ||
                  shield_health != UINT32_C(0) ||
                  hitstun != UINT16_C(0) ||
                  world->grounded[player_index] == UINT8_C(0) ||
                  world->pending_velocity_x_q16[player_index] !=
                      INT32_C(0) ||
                  world->pending_velocity_y_q16[player_index] !=
                      INT32_C(0))) ||
                (hitlag == UINT16_C(0) &&
                 action == (uint8_t)PF_M4_ACTION_HITSTUN &&
                 hitstun == UINT16_C(0)) ||
                (hitlag == UINT16_C(0) &&
                 hitstun > UINT16_C(0) &&
                 action != (uint8_t)PF_M4_ACTION_HITSTUN &&
                 !pf_m4_snapshot_action_is_surface_bounce(action)) ||
                (action == (uint8_t)PF_M4_ACTION_HITSTUN &&
                 world->grounded[player_index] != UINT8_C(0)) ||
                (hitlag == UINT16_C(0) &&
                 ((action ==
                       (uint8_t)PF_M4_ACTION_SHIELD_STUN) !=
                  (shield_stun > UINT16_C(0)))) ||
                (shield_stun > UINT16_C(0) &&
                 action != (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  resume_action !=
                      (uint8_t)PF_M4_ACTION_SHIELD_STUN)) ||
                (shield_health == UINT32_C(0) &&
                 !pf_m4_snapshot_action_is_shield_break(action) &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  resume_action !=
                      (uint8_t)PF_M4_ACTION_SHIELD_BREAK)) ||
                (pf_m4_snapshot_action_is_shield_break(action) &&
                 (shield_health != UINT32_C(0) ||
                  shield_stun != UINT16_C(0) ||
                  hitlag != UINT16_C(0) ||
                  hitstun != UINT16_C(0) ||
                  (action ==
                       (uint8_t)PF_M4_ACTION_SHIELD_BREAK
                       ? world->grounded[player_index] !=
                             UINT8_C(0)
                       : world->grounded[player_index] ==
                             UINT8_C(0)) ||
                  world->velocity_x_q16[player_index] !=
                      INT32_C(0))) ||
                ((action == (uint8_t)PF_M4_ACTION_SHIELD ||
                  action ==
                      (uint8_t)PF_M4_ACTION_SHIELD_RELEASE) &&
                 (shield_health == UINT32_C(0) ||
                  shield_stun != UINT16_C(0) ||
                  hitlag != UINT16_C(0) ||
                  hitstun != UINT16_C(0) ||
                  world->grounded[player_index] == UINT8_C(0))) ||
                (powershield != UINT8_C(0) &&
                 action != (uint8_t)PF_M4_ACTION_SHIELD &&
                 action != (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 action !=
                     (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  resume_action !=
                      (uint8_t)PF_M4_ACTION_SHIELD_STUN)) ||
                (tumble != UINT8_C(0) &&
                 action != (uint8_t)PF_M4_ACTION_HITLAG &&
                 action != (uint8_t)PF_M4_ACTION_HITSTUN &&
                 action != (uint8_t)PF_M4_ACTION_AIRBORNE &&
                 !pf_m4_snapshot_action_is_surface_bounce(action)) ||
                ((world->sdi_direction_x[player_index] != INT8_C(0) ||
                  world->sdi_direction_y[player_index] != INT8_C(0)) &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  resume_action !=
                      (uint8_t)PF_M4_ACTION_HITSTUN)) ||
                (((action == (uint8_t)PF_M4_ACTION_TECH_ROLL ||
                   action == (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
                   action == (uint8_t)PF_M4_ACTION_WALL_TECH ||
                   action ==
                       (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP)) !=
                 (tech_direction != INT8_C(0))) ||
                ((action == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                  action ==
                      (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
                  action == (uint8_t)PF_M4_ACTION_TECH_ROLL ||
                  action == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
                  action ==
                      (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
                  action == (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
                  action ==
                      (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
                  action ==
                      (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
                  action ==
                      (uint8_t)PF_M4_ACTION_ROLL_BACKWARD ||
                  action ==
                      (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
                  action ==
                      (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
                  action ==
                      (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND ||
                  action ==
                      (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
                  pf_m4_snapshot_action_is_surface_tech(action)) &&
                 (hitlag != UINT16_C(0) ||
                  hitstun != UINT16_C(0) ||
                  tumble != UINT8_C(0))) ||
                (pf_m4_snapshot_action_is_surface_bounce(action) &&
                 (hitlag != UINT16_C(0) ||
                  tumble == UINT8_C(0))) ||
                (hitlag == UINT16_C(0) &&
                 (world->pending_velocity_x_q16[player_index] !=
                      INT32_C(0) ||
                  world->pending_velocity_y_q16[player_index] !=
                      INT32_C(0))) ||
                (world->last_hit_sequence[player_index] ==
                     UINT32_C(0) &&
                 (world->last_hit_tick[player_index] != UINT64_C(0) ||
                  world->last_hit_damage_q16[player_index] !=
                      UINT32_C(0) ||
                  world->last_hit_attacker[player_index] !=
                      UINT8_C(0))) ||
                (world->last_hit_sequence[player_index] !=
                     UINT32_C(0) &&
                 (world->last_hit_tick[player_index] >= world->tick ||
                  world->last_hit_damage_q16[player_index] ==
                      UINT32_C(0) ||
                  world->last_hit_attacker[player_index] >=
                      world->player_count ||
                  world->last_hit_attacker[player_index] ==
                      (uint8_t)player_index)) ||
                !pf_m4_player_state_consistent(world, player_index))
            {
                return PF_STATUS_INVALID_STATE;
            }
            if (world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
                world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_CLIMB)
            {
                const uint8_t ledge_bit =
                    world->facing[player_index] == INT8_C(1)
                        ? UINT8_C(1)
                        : UINT8_C(2);

                if ((ledge_claims & ledge_bit) != UINT8_C(0))
                {
                    return PF_STATUS_INVALID_STATE;
                }
                ledge_claims |= ledge_bit;
            }
        }
        else if (world->previous_buttons[player_index] != UINT64_C(0) ||
                 world->position_x_q16[player_index] != INT32_C(0) ||
                 world->position_y_q16[player_index] != INT32_C(0) ||
                 world->velocity_x_q16[player_index] != INT32_C(0) ||
                 world->velocity_y_q16[player_index] != INT32_C(0) ||
                 world->action_ticks[player_index] != UINT16_C(0) ||
                 world->respawn_count[player_index] != UINT16_C(0) ||
                 world->team[player_index] != UINT8_C(0) ||
                 world->grounded[player_index] != UINT8_C(0) ||
                 world->active[player_index] != UINT8_C(0) ||
                 world->action_state[player_index] != UINT8_C(0) ||
                 world->support[player_index] != UINT8_C(0) ||
                 world->air_jumps_remaining[player_index] != UINT8_C(0) ||
                 world->short_hop_latched[player_index] != UINT8_C(0) ||
                 world->platform_drop_ticks[player_index] != UINT8_C(0) ||
                 world->fast_fall[player_index] != UINT8_C(0) ||
                 world->facing[player_index] != INT8_C(0) ||
                 world->dash_direction[player_index] != INT8_C(0) ||
                 world->previous_strong_direction[player_index] !=
                     INT8_C(0) ||
                 world->previous_dodge_down[player_index] !=
                     UINT8_C(0) ||
                 world->damage_q16[player_index] != UINT32_C(0) ||
                 world->pending_velocity_x_q16[player_index] !=
                     INT32_C(0) ||
                 world->pending_velocity_y_q16[player_index] !=
                     INT32_C(0) ||
                 world->last_hit_sequence[player_index] != UINT32_C(0) ||
                 world->last_hit_tick[player_index] != UINT64_C(0) ||
                 world->last_hit_damage_q16[player_index] !=
                     UINT32_C(0) ||
                 world->hitlag_ticks[player_index] != UINT16_C(0) ||
                 world->hitstun_ticks[player_index] != UINT16_C(0) ||
                 world->hitlag_resume_action[player_index] !=
                     UINT8_C(0) ||
                 world->attack_hit_mask[player_index] != UINT8_C(0) ||
                 world->last_hit_attacker[player_index] != UINT8_C(0) ||
                 world->tech_window_ticks[player_index] != UINT16_C(0) ||
                 world->tech_lockout_ticks[player_index] != UINT16_C(0) ||
                 world->shield_stun_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->shield_health_q16[player_index] !=
                     UINT32_C(0) ||
                 world->shield_held[player_index] != UINT8_C(0) ||
                 world->trigger_input_age[player_index] != UINT8_C(0) ||
                 world->powershield[player_index] != UINT8_C(0) ||
                 world->tumble[player_index] != UINT8_C(0) ||
                 world->sdi_pulse_count[player_index] != UINT8_C(0) ||
                 world->sdi_direction_x[player_index] != INT8_C(0) ||
                 world->sdi_direction_y[player_index] != INT8_C(0) ||
                 world->tech_direction[player_index] != INT8_C(0) ||
                 world->respawn_ticks[player_index] != UINT16_C(0) ||
                 world->respawn_invulnerability_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->ledge_invulnerability_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->ledge_regrab_lockout_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->stocks_remaining[player_index] != UINT8_C(0))
        {
            return PF_STATUS_INVALID_STATE;
        }
    }

    return PF_STATUS_OK;
}

pf_status pf_sim_query_save_size(
    const pf_sim *sim,
    size_t *out_save_bytes)
{
    if (out_save_bytes == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (!pf_sim_is_valid(sim))
    {
        return PF_STATUS_INVALID_STATE;
    }

    *out_save_bytes = PF_SIM_SAVE_TOTAL_BYTES;
    return PF_STATUS_OK;
}

pf_status pf_sim_query_identity(
    const pf_sim *sim,
    pf_sim_identity *out_identity)
{
    if (out_identity == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_identity, 0, sizeof(*out_identity));
    if (!pf_sim_is_valid(sim))
    {
        return PF_STATUS_INVALID_STATE;
    }

    out_identity->struct_size = (uint32_t)sizeof(*out_identity);
    out_identity->schema_version = PF_SIM_IDENTITY_SCHEMA_VERSION;
    out_identity->sim_abi_version = PF_SIM_ABI_VERSION;
    out_identity->tick_rate_hz = PF_SIM_TICK_RATE_HZ;
    out_identity->config_schema_version =
        PF_SIM_CONFIG_SCHEMA_VERSION;
    out_identity->content_schema_version =
        PF_SIM_CONTENT_SCHEMA_VERSION;
    out_identity->input_schema_version =
        sim->world.input_schema_version;
    out_identity->state_schema_version =
        sim->world.state_schema_version;
    out_identity->observation_schema_version =
        PF_SIM_OBSERVATION_SCHEMA_VERSION;
    out_identity->arithmetic_version =
        sim->world.arithmetic_version;
    out_identity->rng_version = sim->world.rng_version;
    out_identity->save_format_version =
        PF_SIM_SAVE_FORMAT_VERSION;
    out_identity->player_count = sim->world.player_count;
    out_identity->mode = sim->world.mode;
    out_identity->stock_count = sim->world.stock_count;
    out_identity->respawn_delay_ticks =
        sim->world.respawn_delay_config_ticks;
    out_identity->respawn_invulnerability_ticks =
        sim->world.respawn_invulnerability_config_ticks;
    out_identity->max_ticks = sim->world.max_ticks;
    out_identity->arena_half_width_q16 =
        sim->world.arena_half_width_q16;
    out_identity->arena_ceiling_q16 =
        sim->world.arena_ceiling_q16;
    out_identity->content_hash = sim->world.content_hash;
    pf_sim_snapshot_config_hash(
        &sim->world,
        out_identity->config_hash.bytes);
    return PF_STATUS_OK;
}

pf_status pf_sim_save(
    const pf_sim *sim,
    pf_mut_bytes *destination)
{
    pf_byte_writer writer;
    pf_status status;

    if (destination == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    destination->size = PF_SIM_SAVE_TOTAL_BYTES;
    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    status = pf_sim_snapshot_validate_world(&sim->world);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (destination->bytes == NULL ||
        destination->capacity < PF_SIM_SAVE_TOTAL_BYTES)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }

    writer.bytes = destination->bytes;
    writer.capacity = destination->capacity;
    writer.position = (size_t)0;
    writer.hash = NULL;
    writer.failed = 0;
    pf_write_save_stream(&writer, &sim->world);
    if (writer.failed != 0 ||
        writer.position != PF_SIM_SAVE_TOTAL_BYTES)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }
    return PF_STATUS_OK;
}

pf_status pf_sim_hash(
    const pf_sim *sim,
    pf_state_hash *out_hash)
{
    pf_sha256 hash;
    pf_byte_writer writer;
    pf_status status;

    if (out_hash == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_hash, 0, sizeof(*out_hash));
    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    status = pf_sim_snapshot_validate_world(&sim->world);
    if (status != PF_STATUS_OK)
    {
        return status;
    }

    pf_sha256_init(&hash);
    writer.bytes = NULL;
    writer.capacity = PF_SIM_SAVE_TOTAL_BYTES;
    writer.position = (size_t)0;
    writer.hash = &hash;
    writer.failed = 0;
    pf_write_save_stream(&writer, &sim->world);
    if (writer.failed != 0 ||
        writer.position != PF_SIM_SAVE_TOTAL_BYTES)
    {
        return PF_STATUS_INVALID_STATE;
    }
    pf_sha256_finish(&hash, out_hash->bytes);
    out_hash->algorithm = PF_SIM_STATE_HASH_ALGORITHM_SHA256;
    out_hash->algorithm_version =
        PF_SIM_STATE_HASH_ALGORITHM_VERSION;
    out_hash->digest_size = PF_SIM_STATE_HASH_BYTES;
    return PF_STATUS_OK;
}

pf_status pf_sim_clone(
    pf_sim *destination,
    const pf_sim *source)
{
    pf_status status;

    if (!pf_sim_is_valid(destination) ||
        !pf_sim_is_valid(source) ||
        source->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    status = pf_sim_snapshot_validate_world(&source->world);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (!pf_world_identity_equal(
            &destination->world,
            &source->world))
    {
        return PF_STATUS_INCOMPATIBLE_STATE;
    }
    if (destination == source)
    {
        return PF_STATUS_OK;
    }

    destination->world = source->world;
    (void)memset(destination->scratch, 0, sizeof(*destination->scratch));
    destination->has_reset = UINT8_C(1);
    return PF_STATUS_OK;
}

pf_status pf_sim_load(
    pf_sim *sim,
    pf_bytes source)
{
    pf_byte_reader header;
    pf_byte_reader payload;
    pf_world_state candidate;
    uint8_t magic[8];
    uint8_t content_hash[32];
    uint8_t stored_config_hash[32];
    uint8_t live_config_hash[32];
    uint8_t stored_payload_hash[32];
    uint8_t computed_payload_hash[32];
    uint16_t format_version;
    uint16_t header_bytes;
    uint32_t sim_abi;
    uint16_t state_schema;
    uint16_t arithmetic_version;
    uint16_t rng_version;
    uint16_t input_schema;
    uint32_t tick_rate;
    uint64_t header_tick;
    uint32_t payload_bytes;
    uint16_t hash_algorithm;
    uint16_t hash_version;
    pf_sha256 hash;
    pf_status status;

    if (!pf_sim_is_valid(sim))
    {
        return PF_STATUS_INVALID_STATE;
    }
    if (source.bytes == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (source.size < PF_SIM_SAVE_HEADER_BYTES)
    {
        return PF_STATUS_INVALID_STATE;
    }

    header.bytes = source.bytes;
    header.size = source.size;
    header.position = (size_t)0;
    header.failed = 0;
    pf_reader_bytes(&header, magic, sizeof(magic));
    format_version = pf_reader_u16(&header);
    header_bytes = pf_reader_u16(&header);
    sim_abi = pf_reader_u32(&header);
    state_schema = pf_reader_u16(&header);
    arithmetic_version = pf_reader_u16(&header);
    rng_version = pf_reader_u16(&header);
    input_schema = pf_reader_u16(&header);
    tick_rate = pf_reader_u32(&header);
    pf_reader_bytes(&header, content_hash, sizeof(content_hash));
    pf_reader_bytes(
        &header,
        stored_config_hash,
        sizeof(stored_config_hash));
    header_tick = pf_reader_u64(&header);
    payload_bytes = pf_reader_u32(&header);
    hash_algorithm = pf_reader_u16(&header);
    hash_version = pf_reader_u16(&header);
    pf_reader_bytes(
        &header,
        stored_payload_hash,
        sizeof(stored_payload_hash));

    if (header.failed != 0 ||
        header.position != PF_SIM_SAVE_HEADER_BYTES ||
        memcmp(magic, pf_save_magic, sizeof(magic)) != 0)
    {
        return PF_STATUS_INVALID_STATE;
    }
    if (format_version != PF_SIM_SAVE_FORMAT_VERSION ||
        header_bytes != (uint16_t)PF_SIM_SAVE_HEADER_BYTES ||
        sim_abi != PF_SIM_ABI_VERSION ||
        state_schema != PF_SIM_STATE_SCHEMA_VERSION ||
        arithmetic_version != PF_SIM_ARITHMETIC_VERSION ||
        rng_version != PF_SIM_RNG_VERSION ||
        input_schema != PF_SIM_INPUT_SCHEMA_VERSION ||
        tick_rate != PF_SIM_TICK_RATE_HZ ||
        hash_algorithm != PF_SIM_STATE_HASH_ALGORITHM_SHA256 ||
        hash_version != PF_SIM_STATE_HASH_ALGORITHM_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (payload_bytes != (uint32_t)PF_SIM_SAVE_PAYLOAD_BYTES ||
        source.size !=
            PF_SIM_SAVE_HEADER_BYTES + (size_t)payload_bytes)
    {
        return PF_STATUS_INVALID_STATE;
    }

    pf_sim_snapshot_config_hash(&sim->world, live_config_hash);
    if (!pf_hash_equal(
            content_hash,
            sim->world.content_hash.bytes) ||
        !pf_hash_equal(stored_config_hash, live_config_hash))
    {
        return PF_STATUS_INCOMPATIBLE_STATE;
    }

    pf_sha256_init(&hash);
    pf_sha256_update(
        &hash,
        &source.bytes[PF_SIM_SAVE_HEADER_BYTES],
        (size_t)payload_bytes);
    pf_sha256_finish(&hash, computed_payload_hash);
    if (!pf_hash_equal(
            stored_payload_hash,
            computed_payload_hash))
    {
        return PF_STATUS_CHECKSUM_MISMATCH;
    }

    (void)memset(&candidate, 0, sizeof(candidate));
    (void)memcpy(
        candidate.content_hash.bytes,
        content_hash,
        sizeof(candidate.content_hash.bytes));
    payload.bytes = &source.bytes[PF_SIM_SAVE_HEADER_BYTES];
    payload.size = (size_t)payload_bytes;
    payload.position = (size_t)0;
    payload.failed = 0;
    pf_read_payload(&payload, &candidate);
    if (payload.failed != 0 ||
        payload.position != payload.size ||
        candidate.tick != header_tick)
    {
        return PF_STATUS_INVALID_STATE;
    }

    status = pf_sim_snapshot_validate_world(&candidate);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (!pf_world_identity_equal(&sim->world, &candidate))
    {
        return PF_STATUS_INCOMPATIBLE_STATE;
    }

    sim->world = candidate;
    (void)memset(sim->scratch, 0, sizeof(*sim->scratch));
    sim->has_reset = UINT8_C(1);
    return PF_STATUS_OK;
}
