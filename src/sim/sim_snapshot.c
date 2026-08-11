#include "sim_internal.h"
#include "sim_falcon_frame_data.h"
#include "sim_sha256.h"
#include "sim_ssbm_stage_data.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PF_SIM_SAVE_HEADER_BYTES ((size_t)140)
#define PF_SIM_SAVE_PAYLOAD_BYTES ((size_t)1567)
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
    UINT8_C(0x56), UINT8_C(0x45), UINT8_C(0x36), UINT8_C(0x30)};

static const uint8_t pf_config_hash_domain[8] = {
    UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x43), UINT8_C(0x46),
    UINT8_C(0x47), UINT8_C(0x30), UINT8_C(0x30), UINT8_C(0x31)};

static const uint8_t pf_empty_stale_move_queue[
    PF_SIM_STALE_MOVE_QUEUE_CAPACITY] = {UINT8_C(0)};

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

static void pf_writer_i16(pf_byte_writer *writer, int16_t value)
{
    uint16_t bits;

    (void)memcpy(&bits, &value, sizeof(bits));
    pf_writer_u16(writer, bits);
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

static int16_t pf_reader_i16(pf_byte_reader *reader)
{
    const uint16_t bits = pf_reader_u16(reader);
    int16_t value;

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

static uint16_t pf_m4_snapshot_canonical_source_submotion(
    const pf_world_state *world,
    uint32_t player_index)
{
    if (world->active[player_index] == UINT8_C(0))
    {
        return UINT16_C(0);
    }
    return pf_m4_action_retains_source_submotion(
               world->action_state[player_index],
               world->hitlag_resume_action[player_index])
               ? world->source_submotion[player_index]
               : (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
}

static int32_t pf_m4_snapshot_canonical_source_animation_frame_q16(
    const pf_world_state *world,
    uint32_t player_index)
{
    return world->active[player_index] != UINT8_C(0) &&
                   pf_m4_action_uses_source_animation_clock(
                       world->action_state[player_index],
                       world->hitlag_resume_action[player_index])
               ? world->source_animation_frame_q16[player_index]
               : INT32_C(0);
}

static int32_t pf_m4_snapshot_canonical_source_animation_rate_q16(
    const pf_world_state *world,
    uint32_t player_index)
{
    return world->active[player_index] != UINT8_C(0) &&
                   pf_m4_action_uses_source_animation_clock(
                       world->action_state[player_index],
                       world->hitlag_resume_action[player_index])
               ? world->source_animation_rate_q16[player_index]
               : INT32_C(0);
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
        pf_writer_u16(writer, world->match_kos[player_index]);
        pf_writer_u16(writer, world->match_falls[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            world->shield_recoil_x_q16[player_index]);
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
        pf_writer_u16(
            writer,
            pf_m4_snapshot_canonical_source_submotion(
                world,
                player_index));
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const int active =
            world->ground_blend_progress_q16[player_index] > INT32_C(0);
        uint8_t rotation_index;

        pf_writer_i32(
            writer,
            active != 0
                ? world->ground_blend_progress_q16[player_index]
                : INT32_C(0));
        for (rotation_index = UINT8_C(0);
             rotation_index < PF_M4_HSD_COMPACT_ROTATION_CAPACITY;
             ++rotation_index)
        {
            uint8_t component;

            for (component = UINT8_C(0); component < UINT8_C(3); ++component)
            {
                pf_writer_i16(
                    writer,
                    active != 0
                        ? world->ground_blend_pose[player_index]
                              .rotation_q15[rotation_index][component]
                        : INT16_C(0));
            }
        }
        for (uint8_t translation_index = UINT8_C(0);
             translation_index < PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY;
             ++translation_index)
        {
            uint8_t component;

            for (component = UINT8_C(0); component < UINT8_C(3); ++component)
            {
                pf_writer_i32(
                    writer,
                    active != 0
                        ? world->ground_blend_pose[player_index]
                              .translation_q16[translation_index][component]
                        : INT32_C(0));
            }
        }
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            pf_m4_snapshot_canonical_source_animation_frame_q16(
                world,
                player_index));
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            pf_m4_snapshot_canonical_source_animation_rate_q16(
                world,
                player_index));
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
            world->previous_directional_input_flags[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(
            writer,
            world->previous_tilt_x_direction[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(
            writer,
            world->previous_tilt_y_direction[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(
            writer,
            world->mash_stick_x_direction[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(
            writer,
            world->mash_stick_y_direction[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            (uint16_t)world->previous_secondary_stick_x[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            (uint16_t)world->previous_secondary_stick_y[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->tilt_x_age[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->tilt_y_age[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->horizontal_input_age[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i8(
            writer,
            world->horizontal_input_direction[player_index]);
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
            world->knockback_velocity_x_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            world->knockback_velocity_y_q16[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_i32(
            writer,
            world->ground_knockback_velocity_q16[player_index]);
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
    pf_writer_bytes(
        writer,
        world->attack_stale_registered,
        sizeof(world->attack_stale_registered));
    pf_writer_bytes(
        writer,
        world->falcon_kick_hit_count,
        sizeof(world->falcon_kick_hit_count));
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            world->rebound_duration_ticks[player_index]);
    }
    pf_writer_bytes(
        writer,
        world->jab_chain_buffered,
        sizeof(world->jab_chain_buffered));
    pf_writer_bytes(
        writer,
        world->rapid_jab_input_count,
        sizeof(world->rapid_jab_input_count));
    pf_writer_bytes(
        writer,
        world->rapid_jab_continue,
        sizeof(world->rapid_jab_continue));
    pf_writer_bytes(
        writer,
        world->down_tilt_repeat_buffered,
        sizeof(world->down_tilt_repeat_buffered));
    pf_writer_bytes(
        writer,
        world->stale_move_count,
        sizeof(world->stale_move_count));
    pf_writer_bytes(
        writer,
        &world->stale_move_ids[0][0],
        sizeof(world->stale_move_ids));
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
        pf_writer_u8(writer, world->prone_attack_input_age[player_index]);
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
        const int8_t tech_direction =
            world->tech_direction[player_index];
        const uint8_t tech_code =
            tech_direction < INT8_C(0)
                ? UINT8_C(1)
                : (tech_direction > INT8_C(0)
                       ? UINT8_C(2)
                       : UINT8_C(0));

        pf_writer_u8(
            writer,
            (uint8_t)(tech_code |
                      (uint8_t)(world->prone_orientation[player_index]
                                << 2U) |
                      (uint8_t)(
                          world->prone_roll_motion_orientation[player_index]
                          << 4U)));
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->grab_escape_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            world->damage_jump_buffer_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->charge_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(
            writer,
            world->smash_charge_ticks[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->shield_strength[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->shield_angle_turn[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u16(writer, world->shield_magnitude[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(
            writer,
            world->recovery_available[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->grab_target_slot[player_index]);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_writer_u8(writer, world->grab_owner_slot[player_index]);
    }
    pf_writer_i32(writer, world->item_position_x_q16);
    pf_writer_i32(writer, world->item_position_y_q16);
    pf_writer_i32(writer, world->item_velocity_x_q16);
    pf_writer_i32(writer, world->item_velocity_y_q16);
    pf_writer_u16(writer, world->item_lifetime_ticks);
    pf_writer_u16(writer, world->item_respawn_ticks);
    pf_writer_u16(writer, world->item_pickup_lockout_ticks);
    pf_writer_u8(writer, world->item_state);
    pf_writer_u8(writer, world->item_holder_slot);
    pf_writer_u8(writer, world->item_source_slot);
    pf_writer_u8(writer, world->item_hit_mask);
    pf_writer_u8(writer, world->item_stale_registered);
    pf_writer_u8(writer, world->item_throw_direction);
    pf_writer_i32(writer, world->projectile_position_x_q16);
    pf_writer_i32(writer, world->projectile_position_y_q16);
    pf_writer_i32(writer, world->projectile_velocity_x_q16);
    pf_writer_i32(writer, world->projectile_velocity_y_q16);
    pf_writer_u16(writer, world->projectile_lifetime_ticks);
    pf_writer_u8(writer, world->projectile_state);
    pf_writer_u8(writer, world->projectile_owner_slot);
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
        world->match_kos[player_index] = pf_reader_u16(reader);
        world->match_falls[player_index] = pf_reader_u16(reader);
    }
    world->shield_recoil_mask = UINT8_C(0);
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_recoil_x_q16[player_index] =
            pf_reader_i32(reader);
        if (world->shield_recoil_x_q16[player_index] != INT32_C(0))
        {
            world->shield_recoil_mask =
                (uint8_t)(
                    world->shield_recoil_mask |
                    (uint8_t)(UINT8_C(1) << player_index));
        }
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
        world->source_submotion[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->source_animation_frame_q16[player_index] =
            pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->source_animation_rate_q16[player_index] =
            pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        uint8_t rotation_index;

        world->ground_blend_progress_q16[player_index] =
            pf_reader_i32(reader);
        for (rotation_index = UINT8_C(0);
             rotation_index < PF_M4_HSD_COMPACT_ROTATION_CAPACITY;
             ++rotation_index)
        {
            uint8_t component;

            for (component = UINT8_C(0); component < UINT8_C(3); ++component)
            {
                world->ground_blend_pose[player_index]
                    .rotation_q15[rotation_index][component] =
                    pf_reader_i16(reader);
            }
        }
        for (uint8_t translation_index = UINT8_C(0);
             translation_index < PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY;
             ++translation_index)
        {
            uint8_t component;

            for (component = UINT8_C(0); component < UINT8_C(3); ++component)
            {
                world->ground_blend_pose[player_index]
                    .translation_q16[translation_index][component] =
                    pf_reader_i32(reader);
            }
        }
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
        world->previous_directional_input_flags[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_tilt_x_direction[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_tilt_y_direction[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->mash_stick_x_direction[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->mash_stick_y_direction[player_index] =
            pf_reader_i8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_secondary_stick_x[player_index] =
            (int16_t)pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->previous_secondary_stick_y[player_index] =
            (int16_t)pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->tilt_x_age[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->tilt_y_age[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->horizontal_input_age[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->horizontal_input_direction[player_index] =
            pf_reader_i8(reader);
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
        world->knockback_velocity_x_q16[player_index] =
            pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->knockback_velocity_y_q16[player_index] =
            pf_reader_i32(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->ground_knockback_velocity_q16[player_index] =
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
    pf_reader_bytes(
        reader,
        world->attack_stale_registered,
        sizeof(world->attack_stale_registered));
    pf_reader_bytes(
        reader,
        world->falcon_kick_hit_count,
        sizeof(world->falcon_kick_hit_count));
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->rebound_duration_ticks[player_index] =
            pf_reader_u16(reader);
    }
    pf_reader_bytes(
        reader,
        world->jab_chain_buffered,
        sizeof(world->jab_chain_buffered));
    pf_reader_bytes(
        reader,
        world->rapid_jab_input_count,
        sizeof(world->rapid_jab_input_count));
    pf_reader_bytes(
        reader,
        world->rapid_jab_continue,
        sizeof(world->rapid_jab_continue));
    pf_reader_bytes(
        reader,
        world->down_tilt_repeat_buffered,
        sizeof(world->down_tilt_repeat_buffered));
    pf_reader_bytes(
        reader,
        world->stale_move_count,
        sizeof(world->stale_move_count));
    pf_reader_bytes(
        reader,
        &world->stale_move_ids[0][0],
        sizeof(world->stale_move_ids));
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
        world->prone_attack_input_age[player_index] =
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
        const uint8_t packed = pf_reader_u8(reader);
        const uint8_t tech_code = packed & UINT8_C(0x03);
        const uint8_t prone_orientation =
            (packed >> 2U) & UINT8_C(0x03);
        const uint8_t prone_roll_motion_orientation =
            (packed >> 4U) & UINT8_C(0x03);

        if ((packed & UINT8_C(0xc0)) != UINT8_C(0) ||
            tech_code == UINT8_C(3) ||
            prone_orientation > (uint8_t)PF_M4_PRONE_STOMACH ||
            prone_roll_motion_orientation >
                (uint8_t)PF_M4_PRONE_STOMACH)
        {
            reader->failed = 1;
        }
        world->tech_direction[player_index] =
            tech_code == UINT8_C(1)
                ? INT8_C(-1)
                : (tech_code == UINT8_C(2)
                       ? INT8_C(1)
                       : INT8_C(0));
        world->prone_orientation[player_index] =
            prone_orientation;
        world->prone_roll_motion_orientation[player_index] =
            prone_roll_motion_orientation;
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->grab_escape_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->damage_jump_buffer_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->charge_ticks[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->smash_charge_ticks[player_index] =
            pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_strength[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_angle_turn[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->shield_magnitude[player_index] = pf_reader_u16(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->recovery_available[player_index] =
            pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->grab_target_slot[player_index] = pf_reader_u8(reader);
    }
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        world->grab_owner_slot[player_index] = pf_reader_u8(reader);
    }
    world->item_position_x_q16 = pf_reader_i32(reader);
    world->item_position_y_q16 = pf_reader_i32(reader);
    world->item_velocity_x_q16 = pf_reader_i32(reader);
    world->item_velocity_y_q16 = pf_reader_i32(reader);
    world->item_lifetime_ticks = pf_reader_u16(reader);
    world->item_respawn_ticks = pf_reader_u16(reader);
    world->item_pickup_lockout_ticks = pf_reader_u16(reader);
    world->item_state = pf_reader_u8(reader);
    world->item_holder_slot = pf_reader_u8(reader);
    world->item_source_slot = pf_reader_u8(reader);
    world->item_hit_mask = pf_reader_u8(reader);
    world->item_stale_registered = pf_reader_u8(reader);
    world->item_throw_direction = pf_reader_u8(reader);
    world->projectile_position_x_q16 = pf_reader_i32(reader);
    world->projectile_position_y_q16 = pf_reader_i32(reader);
    world->projectile_velocity_x_q16 = pf_reader_i32(reader);
    world->projectile_velocity_y_q16 = pf_reader_i32(reader);
    world->projectile_lifetime_ticks = pf_reader_u16(reader);
    world->projectile_state = pf_reader_u8(reader);
    world->projectile_owner_slot = pf_reader_u8(reader);
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

static int pf_write_save_bytes(
    uint8_t *bytes,
    size_t capacity,
    const pf_world_state *world)
{
    uint8_t config_hash[32];
    uint8_t payload_checksum[32];
    pf_byte_writer header_writer;
    pf_byte_writer payload_writer;
    pf_sha256 hash;

    if (bytes == NULL || capacity < PF_SIM_SAVE_TOTAL_BYTES)
    {
        return 0;
    }
    payload_writer.bytes = &bytes[PF_SIM_SAVE_HEADER_BYTES];
    payload_writer.capacity = PF_SIM_SAVE_PAYLOAD_BYTES;
    payload_writer.position = (size_t)0;
    payload_writer.hash = NULL;
    payload_writer.failed = 0;
    pf_write_payload(&payload_writer, world);
    if (payload_writer.failed != 0 ||
        payload_writer.position != PF_SIM_SAVE_PAYLOAD_BYTES)
    {
        return 0;
    }

    pf_sha256_init(&hash);
    pf_sha256_update(
        &hash,
        &bytes[PF_SIM_SAVE_HEADER_BYTES],
        PF_SIM_SAVE_PAYLOAD_BYTES);
    pf_sha256_finish(&hash, payload_checksum);
    pf_sim_snapshot_config_hash(world, config_hash);
    header_writer.bytes = bytes;
    header_writer.capacity = PF_SIM_SAVE_HEADER_BYTES;
    header_writer.position = (size_t)0;
    header_writer.hash = NULL;
    header_writer.failed = 0;
    pf_write_header(
        &header_writer,
        world,
        config_hash,
        payload_checksum);
    return header_writer.failed == 0 &&
           header_writer.position == PF_SIM_SAVE_HEADER_BYTES;
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

static int pf_m4_snapshot_hitstun_is_memory(
    uint8_t action,
    uint32_t last_hit_sequence)
{
    return last_hit_sequence != UINT32_C(0) &&
           action != (uint8_t)PF_M4_ACTION_HITSTUN &&
           !pf_m4_action_is_damage(action) &&
           action != (uint8_t)PF_M4_ACTION_RESET_BOUND &&
           !pf_m4_snapshot_action_is_surface_bounce(action);
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

static int pf_m4_snapshot_action_is_throw(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_THROW_FORWARD ||
           action == (uint8_t)PF_M4_ACTION_THROW_BACK ||
           action == (uint8_t)PF_M4_ACTION_THROW_UP ||
           action == (uint8_t)PF_M4_ACTION_THROW_DOWN;
}

static int pf_m4_snapshot_action_is_aerial_attack(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_FORWARD_AERIAL ||
           action == (uint8_t)PF_M4_ACTION_BACK_AERIAL ||
           action == (uint8_t)PF_M4_ACTION_UP_AERIAL ||
           action == (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
}

static int pf_m4_snapshot_action_is_ground_attack(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_UP_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_DOWN_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
           pf_m4_action_is_reference_angled_normal(action) ||
           action == (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
           action ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
           action ==
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
           action == (uint8_t)PF_M4_ACTION_JAB_FINAL ||
           action == (uint8_t)PF_M4_ACTION_JAB_THIRD ||
           action == (uint8_t)PF_M4_ACTION_RAPID_JAB_START ||
           action == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP ||
           action == (uint8_t)PF_M4_ACTION_RAPID_JAB_END;
}

static int pf_m4_snapshot_action_is_landing(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_LANDING ||
           action == (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
           action == (uint8_t)PF_M4_ACTION_AERIAL_LANDING ||
           action == (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING ||
           action ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_LANDING ||
           action ==
               (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING ||
           action ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
           action ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING ||
           (action >=
                (uint8_t)PF_M4_ACTION_FORWARD_AERIAL_LANDING &&
            action <=
                (uint8_t)PF_M4_ACTION_DOWN_AERIAL_L_CANCEL_LANDING);
}

static int pf_m4_snapshot_action_is_reference_air_special(uint8_t action)
{
    return action == (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
           action ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
           action ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR ||
           action ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS ||
           action ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND;
}

static int pf_m4_snapshot_action_is_ground_falcon_kick(uint8_t action)
{
    return action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_LANDING ||
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND;
}

static int pf_m4_snapshot_action_is_ground_falcon_dive_start(
    uint8_t action,
    uint16_t action_ticks)
{
    const pf_m4_falcon_up_special_timing *timing =
        pf_m4_falcon_reference_up_special_timing();

    return timing != NULL &&
           action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND &&
           action_ticks <= timing->air_control_begin_frame;
}

static int pf_m4_snapshot_action_is_falcon_dive_capture_holder(
    uint8_t action,
    uint8_t resume_action)
{
    const uint8_t effective_action =
        action == (uint8_t)PF_M4_ACTION_HITLAG
            ? resume_action
            : action;

    return effective_action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
           effective_action ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW;
}

static int pf_m4_snapshot_action_is_grabbed(
    uint8_t action,
    uint8_t resume_action)
{
    return action == (uint8_t)PF_M4_ACTION_GRABBED ||
           (action == (uint8_t)PF_M4_ACTION_HITLAG &&
            resume_action == (uint8_t)PF_M4_ACTION_GRABBED);
}

static int pf_m4_snapshot_action_is_normal_grab_holder(
    uint8_t action,
    uint8_t resume_action)
{
    const uint8_t effective_action =
        action == (uint8_t)PF_M4_ACTION_HITLAG
            ? resume_action
            : action;

    return effective_action == (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
           effective_action == (uint8_t)PF_M4_ACTION_PUMMEL;
}

static int pf_m4_snapshot_action_is_throw_holder(
    uint8_t action,
    uint8_t resume_action)
{
    return pf_m4_snapshot_action_is_throw(
        action == (uint8_t)PF_M4_ACTION_HITLAG
            ? resume_action
            : action);
}

static int pf_m4_snapshot_action_is_smash_charge(uint8_t action)
{
    return action ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
           action == (uint8_t)PF_M4_ACTION_UP_STRONG_CHARGE ||
           action ==
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE ||
           action ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
           action ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW;
}

static int pf_m4_snapshot_action_is_smash_release(uint8_t action)
{
    return pf_m4_action_is_smash_release(action);
}

static uint16_t pf_m4_snapshot_smash_charge_frame(uint8_t action)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;

    switch ((pf_m4_action_state)action)
    {
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE:
            move_index = PF_M4_FALCON_FORWARD_SMASH;
            break;
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH:
            move_index = PF_M4_FALCON_FORWARD_SMASH_HIGH;
            break;
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW:
            move_index = PF_M4_FALCON_FORWARD_SMASH_LOW;
            break;
        case PF_M4_ACTION_UP_STRONG_CHARGE:
            move_index = PF_M4_FALCON_UP_SMASH;
            break;
        case PF_M4_ACTION_DOWN_STRONG_CHARGE:
            move_index = PF_M4_FALCON_DOWN_SMASH;
            break;
        default:
            return UINT16_C(0);
    }
    move = pf_m4_falcon_reference_move(move_index);
    return move != NULL ? move->charge_frame : UINT16_C(0);
}

static const pf_m4_attack_data *pf_m4_snapshot_ground_attack_data(
    const pf_m4_fighter_data *fighter,
    uint8_t action)
{
    switch ((pf_m4_action_state)action)
    {
        case PF_M4_ACTION_UP_ATTACK:
            return &fighter->up_attack;
        case PF_M4_ACTION_DOWN_ATTACK:
            return &fighter->down_attack;
        case PF_M4_ACTION_FORWARD_ATTACK:
        case PF_M4_ACTION_FORWARD_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_LOW:
        case PF_M4_ACTION_FORWARD_ATTACK_LOW:
            return &fighter->forward_attack;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            return &fighter->forward_strong_attack;
        case PF_M4_ACTION_UP_STRONG_ATTACK:
            return &fighter->up_strong_attack;
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
            return &fighter->down_strong_attack;
        default:
            return NULL;
    }
}

static int32_t pf_m4_snapshot_revival_platform_y(
    const pf_m4_stage_data *stage,
    uint16_t action_ticks)
{
    const uint16_t elapsed =
        action_ticks < stage->revival_platform_descent_ticks
            ? action_ticks
            : stage->revival_platform_descent_ticks;
    const int64_t distance =
        (int64_t)stage->revival_platform_end_y_q16 -
        (int64_t)stage->revival_platform_start_y_q16;

    return stage->revival_platform_start_y_q16 +
           (int32_t)(
               distance * (int64_t)elapsed /
               (int64_t)stage->revival_platform_descent_ticks);
}

static int pf_m4_snapshot_source_submotion_valid_for_action(
    uint8_t action,
    uint8_t resume_action,
    uint16_t submotion,
    uint16_t action_ticks,
    uint8_t reference_frame_data_enabled)
{
    const uint8_t effective_action =
        action == (uint8_t)PF_M4_ACTION_HITLAG
            ? resume_action
            : action;
    const pf_m4_falcon_submotion_data *motion =
        pf_m4_falcon_reference_submotion(submotion);
    int identity_valid = 0;

    if (reference_frame_data_enabled == UINT8_C(0) &&
        (pf_m4_action_uses_ledge(effective_action) ||
         effective_action == (uint8_t)PF_M4_ACTION_WALK ||
         effective_action == (uint8_t)PF_M4_ACTION_RUN))
    {
        return submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
    }
    if (effective_action == (uint8_t)PF_M4_ACTION_WALK)
    {
        return motion != NULL &&
               (submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_SLOW ||
                submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_MIDDLE ||
                submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_FAST);
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_RUN)
    {
        return motion != NULL &&
               submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_RUN;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_AIRBORNE ||
        effective_action == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP)
    {
        identity_valid =
            (submotion >=
                 (uint16_t)PF_M4_FALCON_SUBMOTION_JUMP_FORWARD &&
             submotion <=
                 (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD) ||
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_PLATFORM_DROP;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_LEDGE_CATCH)
    {
        identity_valid =
            submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        identity_valid =
            submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TURN;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND)
    {
        identity_valid =
            submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TURN_RUN;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_CROUCH)
    {
        return submotion ==
                   (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT &&
               action_ticks <= UINT16_C(600);
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_TAUNT)
    {
        return (submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT ||
                submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_LEFT) &&
               action_ticks <= UINT16_C(60);
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        identity_valid =
            submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_FLY;
    }
    else if (effective_action ==
             (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN)
    {
        identity_valid =
            submotion ==
                (uint16_t)
                    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_UP ||
            submotion ==
                (uint16_t)
                    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN;
    }
    else if (effective_action ==
             (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
    {
        identity_valid =
            submotion ==
                (uint16_t)
                    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_UP ||
            submotion ==
                (uint16_t)
                    PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN;
    }
    else if (effective_action ==
             (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        /* action_ticks is the source grab/daze countdown here, not the
         * looping FuraFura animation cursor. */
        return submotion ==
                   (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA &&
               action_ticks != UINT16_C(0);
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_TEETER)
    {
        identity_valid =
            submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER ||
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER_WAIT;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_REBOUND)
    {
        identity_valid =
            reference_frame_data_enabled != UINT8_C(0) &&
            submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_REBOUND;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_GRAB_HOLD)
    {
        return submotion ==
                   (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_WAIT &&
               action_ticks <= UINT16_C(600);
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_PUMMEL)
    {
        identity_valid =
            submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_ATTACK;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_GRABBED)
    {
        if (submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_HIGH)
        {
            return action_ticks <= UINT16_C(600);
        }
        identity_valid =
            submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_DAMAGE_HIGH;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_GRAB_RELEASE)
    {
        identity_valid =
            submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT ||
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_CUT;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_LEDGE_HANG)
    {
        return submotion ==
                   (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_WAIT &&
               action_ticks <= UINT16_C(640);
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB)
    {
        identity_valid =
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_SLOW ||
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_QUICK;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        identity_valid =
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW ||
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
    {
        identity_valid =
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_SLOW ||
            submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_QUICK;
    }
    else if (effective_action == (uint8_t)PF_M4_ACTION_LEDGE_JUMP)
    {
        identity_valid =
            submotion >=
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_1 &&
            submotion <=
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2;
    }
    return identity_valid != 0 && motion != NULL &&
           action_ticks < motion->animation_frame_count;
}

static int pf_m4_snapshot_source_animation_clock_valid(
    const pf_m4_fighter_data *fighter,
    uint8_t active,
    uint8_t action,
    uint8_t resume_action,
    uint16_t submotion,
    int32_t frame_q16,
    int32_t rate_q16)
{
    const pf_m4_falcon_submotion_data *motion;

    if (active == UINT8_C(0) ||
        fighter->reference_frame_data_enabled == UINT8_C(0) ||
        !pf_m4_action_uses_source_animation_clock(
            action,
            resume_action))
    {
        return frame_q16 == INT32_C(0) && rate_q16 == INT32_C(0);
    }
    motion = pf_m4_falcon_reference_submotion(submotion);
    return motion != NULL &&
           motion->animation_frame_count != UINT16_C(0) &&
           frame_q16 >= INT32_C(0) &&
           rate_q16 >= INT32_C(0) &&
           (int64_t)frame_q16 <
               (int64_t)motion->animation_frame_count *
                   (int64_t)PF_Q16_ONE;
}

static int pf_m4_snapshot_ground_blend_valid(
    const pf_m4_fighter_data *fighter,
    const pf_world_state *world,
    uint32_t player_index)
{
    const int32_t progress_q16 =
        world->ground_blend_progress_q16[player_index];
    const pf_m4_hsd_compact_pose *compact =
        &world->ground_blend_pose[player_index];
    uint8_t rotation_index;
    uint8_t translation_index;

    if (progress_q16 == INT32_C(0))
    {
        for (rotation_index = UINT8_C(0);
             rotation_index < PF_M4_HSD_COMPACT_ROTATION_CAPACITY;
             ++rotation_index)
        {
            for (uint8_t component = UINT8_C(0);
                 component < UINT8_C(3);
                 ++component)
            {
                if (compact->rotation_q15[rotation_index][component] !=
                    INT16_C(0))
                {
                    return 0;
                }
            }
        }
        for (translation_index = UINT8_C(0);
             translation_index < PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY;
             ++translation_index)
        {
            for (uint8_t component = UINT8_C(0);
                 component < UINT8_C(3);
                 ++component)
            {
                if (compact->translation_q16[translation_index][component] !=
                    INT32_C(0))
                {
                    return 0;
                }
            }
        }
        return 1;
    }
    if (fighter->reference_frame_data_enabled == UINT8_C(0) ||
        world->active[player_index] == UINT8_C(0) ||
        !pf_m4_action_uses_velocity_animation_clock(
            world->action_state[player_index],
            world->hitlag_resume_action[player_index]) ||
        progress_q16 <= INT32_C(0) ||
        progress_q16 >= INT32_C(6) * PF_Q16_ONE)
    {
        return 0;
    }
    {
        const pf_m4_hsd_pose_data *data =
            pf_m4_falcon_reference_hsd_pose_data();
        pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_reference_hurt_capsule
            capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
        uint8_t count;

        return data != NULL &&
               pf_m4_hsd_evaluate_local_pose_q16(
                   data,
                   world->source_submotion[player_index],
                   world->source_animation_frame_q16[player_index],
                   target) &&
               pf_m4_hsd_inflate_compact_pose_q16(
                   data, target, compact, pose) &&
               pf_m4_falcon_reference_dynamic_ground_hurt_capsules_from_local_pose(
                   pose, capsules, &count) &&
               count != UINT8_C(0);
    }
}

static uint32_t pf_m4_snapshot_ledge_attack_ticks(
    const pf_m4_fighter_data *fighter,
    uint16_t source_submotion)
{
    const pf_m4_falcon_ledge_attack_reference *reference_attack =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? pf_m4_falcon_reference_ledge_attack(source_submotion)
            : NULL;

    if (reference_attack != NULL)
    {
        return (uint32_t)reference_attack->total_frames;
    }
    return (uint32_t)fighter->ledge_attack.startup_ticks +
           (uint32_t)fighter->ledge_attack.active_ticks +
           (uint32_t)fighter->ledge_attack.recovery_ticks;
}

static int pf_m4_snapshot_content_state_consistent(
    const pf_m4_content *content,
    const pf_world_state *world)
{
    const pf_m4_charge_data *charge;
    const pf_m4_recovery_data *recovery;
    uint32_t player_index;

    if (content == NULL || world == NULL)
    {
        return 0;
    }
    charge = &content->charge;
    recovery = &content->recovery;
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const uint8_t action = world->action_state[player_index];
        const uint8_t resume_action =
            world->hitlag_resume_action[player_index];
        const uint16_t action_ticks =
            world->action_ticks[player_index];
        const uint16_t source_submotion =
            world->source_submotion[player_index];
        const uint8_t prone_roll_motion_orientation =
            world->prone_roll_motion_orientation[player_index];
        const int charge_action =
            action == (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
            action == (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND ||
            action ==
                (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND;
        const int release_resume =
            resume_action ==
            (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND;
        const uint32_t release_ticks =
            (uint32_t)charge->release_startup_ticks +
            (uint32_t)charge->release_active_ticks +
            (uint32_t)charge->release_recovery_ticks;
        const int recovery_action =
            action == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT;

        if (!pf_m4_ssbm_stage_support_valid(
                content->stage.reference_collision_profile,
                world->support[player_index],
                world->grounded[player_index]))
        {
            return 0;
        }
        if (pf_m4_action_retains_source_submotion(
                action,
                resume_action))
        {
            if (!pf_m4_snapshot_source_submotion_valid_for_action(
                    action,
                    resume_action,
                    source_submotion,
                    action_ticks,
                    content->fighter.reference_frame_data_enabled))
            {
                return 0;
            }
        }
        if (!pf_m4_snapshot_source_animation_clock_valid(
                &content->fighter,
                world->active[player_index],
                action,
                resume_action,
                source_submotion,
                world->source_animation_frame_q16[player_index],
                world->source_animation_rate_q16[player_index]))
        {
            return 0;
        }
        if (!pf_m4_snapshot_ground_blend_valid(
                &content->fighter, world, player_index))
        {
            return 0;
        }
        const uint32_t ledge_attack_ticks =
            pf_m4_snapshot_ledge_attack_ticks(
                &content->fighter,
                source_submotion);
        const pf_m4_attack_data *ground_attack =
            pf_m4_snapshot_ground_attack_data(
                &content->fighter,
                action);
        const pf_m4_attack_data *ground_attack_resume =
            pf_m4_snapshot_ground_attack_data(
                &content->fighter,
                resume_action);
        const pf_m4_reference_timing ground_attack_timing =
            ground_attack != NULL
                ? (pf_m4_reference_timing){
                      ground_attack->startup_ticks,
                      ground_attack->active_ticks,
                      ground_attack->recovery_ticks}
                : (pf_m4_reference_timing){0};
        pf_m4_falcon_move_index ground_attack_move_index;
        const int falcon_ground_attack =
            ground_attack != NULL &&
            (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
                     pf_m4_action_is_reference_angled_normal(action)
                 ? pf_m4_falcon_reference_move_for_action(
                       action,
                       &ground_attack_move_index)
                 : pf_m4_falcon_reference_attack_matches(
                       action,
                       ground_attack_timing,
                       ground_attack->damage_q16));
        const uint16_t smash_charge_ticks =
            world->smash_charge_ticks[player_index];
        const int smash_charge_action =
            pf_m4_snapshot_action_is_smash_charge(action);
        const int smash_release_action =
            pf_m4_snapshot_action_is_smash_release(action);
        const int smash_release_resume =
            pf_m4_snapshot_action_is_smash_release(resume_action);
        const uint16_t source_smash_charge_frame =
            content->fighter.reference_frame_data_enabled != UINT8_C(0)
                ? pf_m4_snapshot_smash_charge_frame(action)
                : UINT16_C(0);
        const uint16_t shield_strength =
            world->shield_strength[player_index];
        const int shield_strength_action =
            action == (uint8_t)PF_M4_ACTION_SHIELD ||
            action == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
            (action == (uint8_t)PF_M4_ACTION_HITLAG &&
             resume_action ==
                 (uint8_t)PF_M4_ACTION_SHIELD_STUN);
        const int revival_action =
            action == (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM;
        const uint32_t revival_ticks =
            (uint32_t)content->stage.revival_platform_descent_ticks +
            (uint32_t)content->stage.revival_platform_hold_ticks;
        const int32_t revival_x =
            ((int32_t)(UINT32_C(2) * player_index + UINT32_C(1)) -
             (int32_t)world->player_count) *
            content->stage.spawn_spacing_q16;
        const int32_t revival_y =
            pf_m4_snapshot_revival_platform_y(
                &content->stage,
                action_ticks) -
            content->fighter.half_height_q16;

        if (world->charge_ticks[player_index] >
                charge->max_charge_ticks ||
            smash_charge_ticks >
                content->fighter.smash_charge_max_ticks ||
            (smash_charge_action &&
             (source_smash_charge_frame != UINT16_C(0)
                  ? (action_ticks == UINT16_C(0) ||
                     action_ticks > source_smash_charge_frame ||
                     (smash_charge_ticks != UINT16_C(0) &&
                      action_ticks != source_smash_charge_frame) ||
                     smash_charge_ticks >=
                         content->fighter.smash_charge_max_ticks)
                  : (smash_charge_ticks == UINT16_C(0) ||
                     smash_charge_ticks != action_ticks ||
                     smash_charge_ticks >=
                         content->fighter.smash_charge_max_ticks))) ||
            (smash_charge_ticks != UINT16_C(0) &&
             !smash_charge_action && !smash_release_action &&
             !smash_release_resume) ||
            ((shield_strength != UINT16_C(0)) !=
             (shield_strength_action != 0)) ||
            (action ==
                 (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN &&
             world->shield_health_q16[player_index] !=
                 content->fighter.shield_reset_health_q16) ||
            ((world->shield_angle_turn[player_index] != UINT16_C(0) ||
              world->shield_magnitude[player_index] != UINT16_C(0)) &&
             shield_strength == UINT16_C(0)) ||
            (world->powershield[player_index] != UINT8_C(0) &&
             shield_strength_action != 0 &&
             shield_strength != UINT16_MAX) ||
            (charge->enabled == UINT8_C(0) &&
             (world->charge_ticks[player_index] != UINT16_C(0) ||
              charge_action || release_resume)) ||
            (action ==
                 (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND &&
             action_ticks >= charge->store_animation_ticks) ||
            (action ==
                 (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND &&
             (uint32_t)action_ticks >= release_ticks) ||
            (release_resume &&
             (action_ticks <
                  charge->release_startup_ticks + UINT16_C(1) ||
              action_ticks >
                  charge->release_startup_ticks +
                      charge->release_active_ticks)) ||
            (action ==
                 (uint8_t)PF_M4_ACTION_MOONWALK_SETUP &&
             (action_ticks == UINT16_C(0) ||
              action_ticks >
                  content->fighter.moonwalk_setup_ticks)) ||
            (action == (uint8_t)PF_M4_ACTION_MOONWALK &&
             (action_ticks == UINT16_C(0) ||
              action_ticks >= content->fighter.initial_dash_ticks)) ||
            (action == (uint8_t)PF_M4_ACTION_TEETER &&
             world->velocity_x_q16[player_index] != INT32_C(0)) ||
            (action == (uint8_t)PF_M4_ACTION_CROUCH_STEP &&
             action_ticks >= content->fighter.crouch_step_ticks) ||
            (action == (uint8_t)PF_M4_ACTION_CROUCH_START &&
             (action_ticks == UINT16_C(0) ||
              action_ticks > content->fighter.crouch_start_ticks)) ||
            (action == (uint8_t)PF_M4_ACTION_CROUCH_END &&
             (action_ticks == UINT16_C(0) ||
              action_ticks > content->fighter.crouch_end_ticks)) ||
            (action == (uint8_t)PF_M4_ACTION_TAUNT &&
             action_ticks >= content->fighter.taunt_ticks) ||
            (action == (uint8_t)PF_M4_ACTION_WALL_JUMP &&
             action_ticks >= content->fighter.wall_jump_ticks) ||
            (action == (uint8_t)PF_M4_ACTION_LEDGE_ROLL &&
             action_ticks >= content->fighter.ledge_roll_ticks) ||
            (action == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK &&
             (uint32_t)action_ticks >= ledge_attack_ticks) ||
            (ground_attack != NULL &&
             (falcon_ground_attack != 0
                  ? (uint32_t)action_ticks >
                        (uint32_t)ground_attack->startup_ticks +
                            (uint32_t)ground_attack->active_ticks +
                            (uint32_t)ground_attack->recovery_ticks
                  : (uint32_t)action_ticks >=
                        (uint32_t)ground_attack->startup_ticks +
                            (uint32_t)ground_attack->active_ticks +
                            (uint32_t)ground_attack->recovery_ticks)) ||
            (resume_action == (uint8_t)PF_M4_ACTION_WALL_JUMP &&
             action_ticks >= content->fighter.wall_jump_ticks) ||
            (resume_action == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK &&
             (uint32_t)action_ticks >= ledge_attack_ticks) ||
            (ground_attack_resume != NULL &&
             (action_ticks <
                  ground_attack_resume->startup_ticks + UINT16_C(1) ||
              action_ticks >
                  ground_attack_resume->startup_ticks +
                      ground_attack_resume->active_ticks)) ||
            (action == (uint8_t)PF_M4_ACTION_PUMMEL &&
              action_ticks > content->fighter.pummel_total_ticks) ||
            (action == (uint8_t)PF_M4_ACTION_GETUP_ROLL &&
             (action_ticks >= content->fighter.getup_roll_ticks ||
              pf_m4_getup_roll_timing_for(
                  &content->fighter,
                  prone_roll_motion_orientation,
                  world->tech_direction[player_index],
                  world->facing[player_index]) == NULL)) ||
            (revival_action &&
             ((uint32_t)action_ticks > revival_ticks ||
              world->position_x_q16[player_index] != revival_x ||
              world->position_y_q16[player_index] != revival_y ||
              world->respawn_invulnerability_ticks[player_index] !=
                  UINT16_C(0))) ||
            (recovery->enabled == UINT8_C(0) &&
             (recovery_action ||
              (player_index < (uint32_t)world->player_count &&
               world->recovery_available[player_index] != UINT8_C(1)))) ||
            (recovery_action &&
             (world->recovery_available[player_index] != UINT8_C(0) ||
              action_ticks >= recovery->ascent_ticks)))
        {
            return 0;
        }
    }
    return 1;
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
               world->shield_recoil_x_q16[player_index] ==
                   INT32_C(0) &&
               world->fast_fall[player_index] == UINT8_C(0) &&
               world->dash_direction[player_index] == INT8_C(0) &&
               world->hitlag_ticks[player_index] == UINT16_C(0) &&
               world->hitstun_ticks[player_index] == UINT16_C(0) &&
               world->shield_stun_ticks[player_index] == UINT16_C(0) &&
               world->hitlag_resume_action[player_index] == UINT8_C(0) &&
               world->knockback_velocity_x_q16[player_index] ==
                   INT32_C(0) &&
               world->knockback_velocity_y_q16[player_index] ==
                   INT32_C(0) &&
               world->respawn_invulnerability_ticks[player_index] ==
                   UINT16_C(0) &&
               world->ledge_invulnerability_ticks[player_index] ==
                   UINT16_C(0) &&
               world->ledge_regrab_lockout_ticks[player_index] ==
                   UINT16_C(0) &&
               world->grab_escape_ticks[player_index] == UINT16_C(0) &&
               world->charge_ticks[player_index] == UINT16_C(0) &&
               world->smash_charge_ticks[player_index] == UINT16_C(0) &&
               world->shield_strength[player_index] == UINT16_C(0) &&
               world->shield_angle_turn[player_index] == UINT16_C(0) &&
               world->shield_magnitude[player_index] == UINT16_C(0) &&
               world->recovery_available[player_index] == UINT8_C(1) &&
               world->grab_target_slot[player_index] == UINT8_C(0) &&
               world->grab_owner_slot[player_index] == UINT8_C(0) &&
               ((waiting &&
                 world->respawn_ticks[player_index] > UINT16_C(0) &&
                 (world->stock_count == UINT8_C(0) ||
                  world->stocks_remaining[player_index] > UINT8_C(0))) ||
                (eliminated &&
                 world->stock_count != UINT8_C(0) &&
                 world->stocks_remaining[player_index] == UINT8_C(0) &&
                 world->respawn_ticks[player_index] == UINT16_C(0)));
    }
    if (action == (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM)
    {
        return grounded == UINT8_C(1) &&
               support ==
                   (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM &&
               world->velocity_x_q16[player_index] == INT32_C(0) &&
               world->velocity_y_q16[player_index] == INT32_C(0) &&
               world->shield_recoil_x_q16[player_index] ==
                   INT32_C(0) &&
               world->fast_fall[player_index] == UINT8_C(0) &&
               world->recovery_available[player_index] == UINT8_C(1);
    }
    if (support == (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM)
    {
        return 0;
    }
    if (grounded != UINT8_C(0))
    {
        const int ground_falcon_dive_start =
            pf_m4_snapshot_action_is_ground_falcon_dive_start(
                action,
                world->action_ticks[player_index]);
        const int falcon_dive_landing =
            action ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING;
        const int falcon_dive_capture =
            pf_m4_snapshot_action_is_falcon_dive_capture_holder(
                action,
                world->hitlag_resume_action[player_index]);
        const int ground_falcon_kick =
            pf_m4_snapshot_action_is_ground_falcon_kick(action);

        return support != (uint8_t)PF_M4_SURFACE_NONE &&
               action != (uint8_t)PF_M4_ACTION_AIRBORNE &&
               action !=
                   (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
               action != (uint8_t)PF_M4_ACTION_WALL_JUMP &&
               action != (uint8_t)PF_M4_ACTION_VECTOR_ASCENT &&
               action != (uint8_t)PF_M4_ACTION_SHIELD_BREAK &&
               action != (uint8_t)PF_M4_ACTION_AIR_DODGE &&
               action != (uint8_t)PF_M4_ACTION_FALL_SPECIAL &&
               !pf_m4_snapshot_action_is_aerial_attack(action) &&
               action !=
                   (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR &&
               (!pf_m4_snapshot_action_is_reference_air_special(action) ||
                ground_falcon_dive_start != 0 ||
                falcon_dive_capture != 0 ||
                ground_falcon_kick != 0) &&
               action != (uint8_t)PF_M4_ACTION_REFLECTOR_AIR &&
               action != (uint8_t)PF_M4_ACTION_LEDGE_CATCH &&
               action != (uint8_t)PF_M4_ACTION_LEDGE_HANG &&
               (world->velocity_y_q16[player_index] == INT32_C(0) ||
                 action == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
                 action == (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
                 action == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
                 action == (uint8_t)PF_M4_ACTION_HITLAG ||
                 (pf_m4_snapshot_action_is_landing(action) &&
                  world->action_ticks[player_index] == UINT16_C(0)) ||
                 ((action == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                   action == (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
                   action == (uint8_t)PF_M4_ACTION_TECH_ROLL) &&
                  world->action_ticks[player_index] == UINT16_C(0)) ||
                 ground_falcon_dive_start != 0) &&
               world->fast_fall[player_index] == UINT8_C(0) &&
               (world->recovery_available[player_index] == UINT8_C(1) ||
                ground_falcon_dive_start != 0 ||
                falcon_dive_capture != 0 ||
                falcon_dive_landing != 0 ||
                ground_falcon_kick != 0);
    }
    if (action == (uint8_t)PF_M4_ACTION_KNOCKDOWN)
    {
        /* DownBound retains its source floor line while the animated ECB has
         * no active floor contact on displayed frames 5 through 22. */
        return support != (uint8_t)PF_M4_SURFACE_NONE &&
               world->velocity_y_q16[player_index] == INT32_C(0) &&
               world->fast_fall[player_index] == UINT8_C(0) &&
               world->recovery_available[player_index] == UINT8_C(1);
    }
    if (support != (uint8_t)PF_M4_SURFACE_NONE)
    {
        return 0;
    }
    if (action == (uint8_t)PF_M4_ACTION_AIRBORNE ||
        action == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        action == (uint8_t)PF_M4_ACTION_WALL_JUMP ||
        action == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
        action == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        action == (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        action == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        pf_m4_snapshot_action_is_aerial_attack(action) ||
        action == (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR ||
        pf_m4_snapshot_action_is_reference_air_special(action) ||
        action == (uint8_t)PF_M4_ACTION_REFLECTOR_AIR ||
        action == (uint8_t)PF_M4_ACTION_LEDGE_JUMP)
    {
        return 1;
    }
    if (action == (uint8_t)PF_M4_ACTION_HITLAG ||
        pf_m4_action_is_damage(action) ||
        action == (uint8_t)PF_M4_ACTION_RESET_BOUND ||
        pf_m4_snapshot_action_is_surface_tech(action) ||
        pf_m4_snapshot_action_is_surface_bounce(action))
    {
        return 1;
    }
    return (action == (uint8_t)PF_M4_ACTION_LEDGE_CATCH ||
            action == (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
            action == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
            action == (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
            action == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK) &&
               world->velocity_x_q16[player_index] == INT32_C(0) &&
               world->velocity_y_q16[player_index] == INT32_C(0) &&
               world->shield_recoil_x_q16[player_index] ==
                   INT32_C(0) &&
           world->fast_fall[player_index] == UINT8_C(0) &&
           world->recovery_available[player_index] == UINT8_C(1);
}

pf_status pf_sim_snapshot_validate_world(const pf_world_state *world)
{
    const pf_m4_falcon_special_attributes *falcon_attributes =
        pf_m4_falcon_reference_special_attributes();
    const uint32_t known_faults =
        (uint32_t)PF_SIM_FAULT_ARITHMETIC |
        (uint32_t)PF_SIM_FAULT_CAPACITY |
        (uint32_t)PF_SIM_FAULT_INVALID_STATE;
    uint32_t player_index;
    uint32_t stale_index;
    uint8_t active_mask;
    uint8_t expected_shield_recoil_mask = UINT8_C(0);
    uint8_t ledge_claims = UINT8_C(0);

    if (world == NULL || falcon_attributes == NULL ||
        falcon_attributes->speciallw_unk2 < INT32_C(0) ||
        falcon_attributes->speciallw_unk2 >= (int32_t)UINT8_MAX ||
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
        world->terminated > UINT8_C(1) ||
        world->truncated > UINT8_C(1))
    {
        return PF_STATUS_INVALID_STATE;
    }

    active_mask =
        (uint8_t)((UINT32_C(1) << world->player_count) - UINT32_C(1));
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        if (world->shield_recoil_x_q16[player_index] != INT32_C(0))
        {
            expected_shield_recoil_mask =
                (uint8_t)(
                    expected_shield_recoil_mask |
                    (uint8_t)(UINT8_C(1) << player_index));
        }
    }
    if ((world->winner_mask & (uint8_t)~active_mask) != UINT8_C(0) ||
        world->shield_recoil_mask != expected_shield_recoil_mask ||
        (world->shield_recoil_mask & (uint8_t)~active_mask) != UINT8_C(0) ||
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

    if (world->item_position_x_q16 <
            -world->arena_half_width_q16 ||
        world->item_position_x_q16 > world->arena_half_width_q16 ||
        !pf_sim_vertical_coordinate_is_within_arena(
            world->item_position_y_q16,
            world->arena_ceiling_q16) ||
        world->item_velocity_x_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->item_velocity_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->item_velocity_y_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->item_velocity_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->item_lifetime_ticks > UINT16_C(3600) ||
        world->item_respawn_ticks > UINT16_C(3600) ||
        world->item_pickup_lockout_ticks > UINT16_C(240) ||
        world->item_state >
            (uint8_t)PF_M4_ITEM_STATE_RESPAWN_WAIT ||
        world->item_holder_slot > world->player_count ||
        world->item_source_slot > world->player_count ||
        (world->item_hit_mask & (uint8_t)~active_mask) != UINT8_C(0) ||
        world->item_stale_registered > UINT8_C(1) ||
        world->item_throw_direction >
            (uint8_t)PF_M4_ITEM_THROW_DOWN ||
        (world->item_source_slot != UINT8_C(0) &&
         (world->item_hit_mask &
          (uint8_t)(UINT32_C(1) <<
                    ((uint32_t)world->item_source_slot -
                     UINT32_C(1)))) != UINT8_C(0)) ||
        (world->item_state == (uint8_t)PF_M4_ITEM_STATE_INACTIVE &&
         (world->item_position_x_q16 != INT32_C(0) ||
          world->item_position_y_q16 != INT32_C(0) ||
          world->item_velocity_x_q16 != INT32_C(0) ||
          world->item_velocity_y_q16 != INT32_C(0) ||
          world->item_lifetime_ticks != UINT16_C(0) ||
          world->item_respawn_ticks != UINT16_C(0) ||
          world->item_pickup_lockout_ticks != UINT16_C(0) ||
          world->item_holder_slot != UINT8_C(0) ||
          world->item_source_slot != UINT8_C(0) ||
          world->item_hit_mask != UINT8_C(0) ||
          world->item_stale_registered != UINT8_C(0) ||
          world->item_throw_direction !=
              (uint8_t)PF_M4_ITEM_THROW_NONE)) ||
        (world->item_state == (uint8_t)PF_M4_ITEM_STATE_GROUND &&
         (world->item_velocity_x_q16 != INT32_C(0) ||
          world->item_velocity_y_q16 != INT32_C(0) ||
          world->item_lifetime_ticks == UINT16_C(0) ||
          world->item_respawn_ticks != UINT16_C(0) ||
          world->item_holder_slot != UINT8_C(0) ||
          world->item_source_slot != UINT8_C(0) ||
          world->item_hit_mask != UINT8_C(0) ||
          world->item_stale_registered != UINT8_C(0) ||
          world->item_throw_direction !=
              (uint8_t)PF_M4_ITEM_THROW_NONE)) ||
        (world->item_state == (uint8_t)PF_M4_ITEM_STATE_HELD &&
         (world->item_velocity_x_q16 != INT32_C(0) ||
          world->item_velocity_y_q16 != INT32_C(0) ||
          world->item_lifetime_ticks == UINT16_C(0) ||
          world->item_respawn_ticks != UINT16_C(0) ||
          world->item_pickup_lockout_ticks != UINT16_C(0) ||
          world->item_holder_slot == UINT8_C(0) ||
          world->item_source_slot != UINT8_C(0) ||
          world->item_hit_mask != UINT8_C(0) ||
          world->item_stale_registered != UINT8_C(0) ||
          world->item_throw_direction !=
              (uint8_t)PF_M4_ITEM_THROW_NONE ||
          world->active[
              (uint32_t)world->item_holder_slot - UINT32_C(1)] ==
              UINT8_C(0))) ||
        (world->item_state ==
             (uint8_t)PF_M4_ITEM_STATE_AIRBORNE &&
         (world->item_lifetime_ticks == UINT16_C(0) ||
          world->item_respawn_ticks != UINT16_C(0) ||
          world->item_holder_slot != UINT8_C(0) ||
          world->item_source_slot == UINT8_C(0) ||
          (world->item_stale_registered != UINT8_C(0) &&
           world->item_hit_mask == UINT8_C(0)))) ||
        (world->item_state ==
             (uint8_t)PF_M4_ITEM_STATE_RESPAWN_WAIT &&
         (world->item_position_x_q16 != INT32_C(0) ||
          world->item_position_y_q16 != INT32_C(0) ||
          world->item_velocity_x_q16 != INT32_C(0) ||
          world->item_velocity_y_q16 != INT32_C(0) ||
          world->item_lifetime_ticks != UINT16_C(0) ||
          world->item_respawn_ticks == UINT16_C(0) ||
          world->item_pickup_lockout_ticks != UINT16_C(0) ||
          world->item_holder_slot != UINT8_C(0) ||
          world->item_source_slot != UINT8_C(0) ||
          world->item_hit_mask != UINT8_C(0) ||
          world->item_stale_registered != UINT8_C(0) ||
          world->item_throw_direction !=
              (uint8_t)PF_M4_ITEM_THROW_NONE)))
    {
        return PF_STATUS_INVALID_STATE;
    }

    if (world->projectile_position_x_q16 <
            -world->arena_half_width_q16 ||
        world->projectile_position_x_q16 >
            world->arena_half_width_q16 ||
        !pf_sim_vertical_coordinate_is_within_arena(
            world->projectile_position_y_q16,
            world->arena_ceiling_q16) ||
        world->projectile_velocity_x_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->projectile_velocity_x_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->projectile_velocity_y_q16 <
            -PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->projectile_velocity_y_q16 >
            PF_SIM_MAX_MOTION_SPEED_Q16 ||
        world->projectile_lifetime_ticks > UINT16_C(3600) ||
        world->projectile_state >
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE ||
        world->projectile_owner_slot > world->player_count ||
        (world->projectile_state ==
             (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE &&
         (world->projectile_position_x_q16 != INT32_C(0) ||
          world->projectile_position_y_q16 != INT32_C(0) ||
          world->projectile_velocity_x_q16 != INT32_C(0) ||
          world->projectile_velocity_y_q16 != INT32_C(0) ||
          world->projectile_lifetime_ticks != UINT16_C(0) ||
          world->projectile_owner_slot != UINT8_C(0))) ||
        (world->projectile_state !=
             (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE &&
         (world->projectile_velocity_x_q16 == INT32_C(0) ||
          world->projectile_velocity_y_q16 != INT32_C(0) ||
          world->projectile_lifetime_ticks == UINT16_C(0) ||
          world->projectile_owner_slot == UINT8_C(0))))
    {
        return PF_STATUS_INVALID_STATE;
    }

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const uint8_t stale_move_count =
            world->stale_move_count[player_index];

        if (stale_move_count > PF_SIM_STALE_MOVE_QUEUE_CAPACITY ||
            world->attack_stale_registered[player_index] >
                UINT8_C(1))
        {
            return PF_STATUS_INVALID_STATE;
        }
        if (stale_move_count == UINT8_C(0))
        {
            if (memcmp(
                    world->stale_move_ids[player_index],
                    pf_empty_stale_move_queue,
                    sizeof(pf_empty_stale_move_queue)) != 0)
            {
                return PF_STATUS_INVALID_STATE;
            }
        }
        else
        {
            for (stale_index = UINT32_C(0);
                 stale_index <
                     (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
                 ++stale_index)
            {
                const uint8_t move_id =
                    world->stale_move_ids[player_index][stale_index];

                if ((stale_index < (uint32_t)stale_move_count &&
                     (move_id == UINT8_C(0) ||
                      pf_m4_stale_move_id_for_action(move_id) !=
                          move_id)) ||
                    (stale_index >= (uint32_t)stale_move_count &&
                     move_id != UINT8_C(0)))
                {
                    return PF_STATUS_INVALID_STATE;
                }
            }
        }
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
            const int hitstun_is_memory =
                hitstun != UINT16_C(0) &&
                pf_m4_snapshot_hitstun_is_memory(
                    action,
                    world->last_hit_sequence[player_index]) &&
                !(action == (uint8_t)PF_M4_ACTION_HITLAG &&
                  (pf_m4_action_is_damage(resume_action) ||
                   resume_action ==
                       (uint8_t)PF_M4_ACTION_RESET_BOUND ||
                   pf_m4_snapshot_action_is_surface_bounce(
                       resume_action)));
            const int hitstun_is_active =
                hitstun != UINT16_C(0) && hitstun_is_memory == 0;
            const uint8_t powershield =
                world->powershield[player_index];
            const uint8_t tumble = world->tumble[player_index];
            const int8_t tech_direction =
                world->tech_direction[player_index];
            const uint8_t prone_orientation =
                world->prone_orientation[player_index];
            const uint8_t prone_roll_motion_orientation =
                world->prone_roll_motion_orientation[player_index];
            const int prone_action =
                action == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                action == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
                action == (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
                action == (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
                action == (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
                (action == (uint8_t)PF_M4_ACTION_HITLAG &&
                 resume_action ==
                     (uint8_t)PF_M4_ACTION_GETUP_ATTACK);
            if (world->active[player_index] > UINT8_C(1) ||
                world->team[player_index] != expected_team ||
                world->grounded[player_index] > UINT8_C(1) ||
                (world->previous_buttons[player_index] &
                 ~PF_INPUT_KNOWN_BUTTONS) != UINT64_C(0) ||
                world->position_x_q16[player_index] <
                    -world->arena_half_width_q16 ||
                world->position_x_q16[player_index] >
                    world->arena_half_width_q16 ||
                !pf_sim_vertical_coordinate_is_within_arena(
                    world->position_y_q16[player_index],
                    world->arena_ceiling_q16) ||
                world->velocity_x_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->velocity_x_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->velocity_y_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->velocity_y_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->ground_knockback_velocity_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->ground_knockback_velocity_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                (world->ground_knockback_velocity_q16[player_index] !=
                     INT32_C(0) &&
                 world->grounded[player_index] == UINT8_C(0)) ||
                world->shield_recoil_x_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->shield_recoil_x_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->action_ticks[player_index] > UINT16_C(640) ||
                world->source_submotion[player_index] >=
                    PF_M4_FALCON_SUBMOTION_COUNT ||
                action >
                    (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW ||
                world->rebound_duration_ticks[player_index] >
                    UINT16_C(640) ||
                world->jab_chain_buffered[player_index] > UINT8_C(1) ||
                world->rapid_jab_continue[player_index] > UINT8_C(1) ||
                world->down_tilt_repeat_buffered[player_index] >
                    UINT8_C(1) ||
                (world->down_tilt_repeat_buffered[player_index] !=
                     UINT8_C(0) &&
                 action != (uint8_t)PF_M4_ACTION_DOWN_ATTACK &&
                 !(action == (uint8_t)PF_M4_ACTION_HITLAG &&
                   world->hitlag_resume_action[player_index] ==
                       (uint8_t)PF_M4_ACTION_DOWN_ATTACK)) ||
                ((action == (uint8_t)PF_M4_ACTION_REBOUND_STOP ||
                  action == (uint8_t)PF_M4_ACTION_REBOUND) &&
                 (world->rebound_duration_ticks[player_index] ==
                      UINT16_C(0) ||
                  world->action_ticks[player_index] >=
                      world->rebound_duration_ticks[player_index])) ||
                (world->rebound_duration_ticks[player_index] !=
                     UINT16_C(0) &&
                 action != (uint8_t)PF_M4_ACTION_REBOUND_STOP &&
                 action != (uint8_t)PF_M4_ACTION_REBOUND) ||
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
                world->support[player_index] == UINT8_MAX ||
                world->air_jumps_remaining[player_index] > UINT8_C(8) ||
                world->recovery_available[player_index] > UINT8_C(1) ||
                world->short_hop_latched[player_index] > UINT8_C(2) ||
                world->platform_drop_ticks[player_index] > UINT8_C(120) ||
                world->fast_fall[player_index] > UINT8_C(1) ||
                (world->facing[player_index] != INT8_C(-1) &&
                 world->facing[player_index] != INT8_C(1)) ||
                world->dash_direction[player_index] < INT8_C(-2) ||
                world->dash_direction[player_index] > INT8_C(2) ||
                world->previous_strong_direction[player_index] <
                    INT8_C(-1) ||
                world->previous_strong_direction[player_index] >
                    INT8_C(1) ||
                (world->previous_directional_input_flags[player_index] |
                 PF_M4_DIRECTIONAL_INPUT_ALL) !=
                    PF_M4_DIRECTIONAL_INPUT_ALL ||
                (((world->previous_directional_input_flags[player_index] &
                   PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL) != UINT8_C(0)) &&
                 action != (uint8_t)PF_M4_ACTION_HITSTUN &&
                 !pf_m4_snapshot_action_is_surface_bounce(action) &&
                 !(action == (uint8_t)PF_M4_ACTION_HITLAG &&
                   resume_action ==
                       (uint8_t)PF_M4_ACTION_HITSTUN)) ||
                world->previous_tilt_x_direction[player_index] <
                    INT8_C(-1) ||
                world->previous_tilt_x_direction[player_index] >
                    INT8_C(1) ||
                world->previous_tilt_y_direction[player_index] <
                    INT8_C(-1) ||
                world->previous_tilt_y_direction[player_index] >
                    INT8_C(1) ||
                world->mash_stick_x_direction[player_index] < INT8_C(-1) ||
                world->mash_stick_x_direction[player_index] > INT8_C(1) ||
                world->mash_stick_y_direction[player_index] < INT8_C(-1) ||
                world->mash_stick_y_direction[player_index] > INT8_C(1) ||
                world->tilt_x_age[player_index] > UINT8_C(254) ||
                world->tilt_y_age[player_index] > UINT8_C(254) ||
                world->horizontal_input_age[player_index] >
                    UINT8_C(254) ||
                world->horizontal_input_direction[player_index] <
                    INT8_C(-1) ||
                world->horizontal_input_direction[player_index] >
                    INT8_C(1) ||
                world->damage_q16[player_index] >
                    PF_SIM_MAX_DAMAGE_Q16 ||
                world->knockback_velocity_x_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->knockback_velocity_x_q16[player_index] >
                    PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->knockback_velocity_y_q16[player_index] <
                    -PF_SIM_MAX_MOTION_SPEED_Q16 ||
                world->knockback_velocity_y_q16[player_index] >
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
                (world->shield_held[player_index] &
                 (uint8_t)~PF_M4_TRIGGER_STATE_MASK) != UINT8_C(0) ||
                ((world->shield_held[player_index] &
                  PF_M4_TRIGGER_STATE_LEFT_DENSE) != UINT8_C(0) &&
                 (world->shield_held[player_index] &
                  PF_M4_TRIGGER_STATE_LEFT_HELD) == UINT8_C(0)) ||
                ((world->shield_held[player_index] &
                  PF_M4_TRIGGER_STATE_RIGHT_DENSE) != UINT8_C(0) &&
                 (world->shield_held[player_index] &
                  PF_M4_TRIGGER_STATE_RIGHT_HELD) == UINT8_C(0)) ||
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
                prone_orientation >
                    (uint8_t)PF_M4_PRONE_STOMACH ||
                prone_roll_motion_orientation >
                    (uint8_t)PF_M4_PRONE_STOMACH ||
                (action == (uint8_t)PF_M4_ACTION_GETUP_ROLL &&
                 prone_roll_motion_orientation ==
                     (uint8_t)PF_M4_PRONE_NONE) ||
                (action != (uint8_t)PF_M4_ACTION_GETUP_ROLL &&
                 prone_roll_motion_orientation !=
                     (uint8_t)PF_M4_PRONE_NONE) ||
                (prone_action != 0 &&
                 prone_orientation ==
                     (uint8_t)PF_M4_PRONE_NONE) ||
                world->grab_escape_ticks[player_index] >
                    PF_SIM_MAX_GRAB_ESCAPE_TICKS ||
                world->damage_jump_buffer_ticks[player_index] >
                    PF_SIM_MAX_HITSTUN_TICKS ||
                world->charge_ticks[player_index] > UINT16_C(600) ||
                world->smash_charge_ticks[player_index] >
                    UINT16_C(600) ||
                world->grab_target_slot[player_index] >
                    world->player_count ||
                world->grab_owner_slot[player_index] >
                    world->player_count ||
                (world->grab_target_slot[player_index] != UINT8_C(0) &&
                 world->grab_owner_slot[player_index] != UINT8_C(0)) ||
                (world->attack_hit_mask[player_index] &
                 (uint8_t)~active_mask) != UINT8_C(0) ||
                (world->attack_hit_mask[player_index] &
                 (uint8_t)(UINT32_C(1) << player_index)) != UINT8_C(0) ||
                (world->active[player_index] == UINT8_C(0) &&
                 world->attack_stale_registered[player_index] !=
                     UINT8_C(0)) ||
                world->falcon_kick_hit_count[player_index] >
                    (uint8_t)(
                        falcon_attributes->speciallw_unk2 + INT32_C(1)) ||
                (world->falcon_kick_hit_count[player_index] !=
                     UINT8_C(0) &&
                 action !=
                     (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND &&
                 action !=
                     (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND &&
                 action !=
                     (uint8_t)
                         PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  (resume_action !=
                       (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND &&
                   resume_action !=
                       (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND))) ||
                ((action ==
                      (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
                  action ==
                      (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
                  action ==
                      (uint8_t)PF_M4_ACTION_STANDING_TURN ||
                  action ==
                      (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
                  action ==
                      (uint8_t)PF_M4_ACTION_MOONWALK) &&
                 world->dash_direction[player_index] == INT8_C(0)) ||
                ((action ==
                      (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
                  action ==
                      (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
                  action ==
                      (uint8_t)PF_M4_ACTION_MOONWALK) &&
                 world->dash_direction[player_index] !=
                     world->facing[player_index]) ||
                (action !=
                     (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                 action !=
                     (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
                 action !=
                     (uint8_t)PF_M4_ACTION_STANDING_TURN &&
                 action !=
                     (uint8_t)PF_M4_ACTION_MOONWALK_SETUP &&
                 action !=
                     (uint8_t)PF_M4_ACTION_MOONWALK &&
                 action !=
                     (uint8_t)PF_M4_ACTION_ROLL_FORWARD &&
                 action !=
                     (uint8_t)PF_M4_ACTION_ROLL_BACKWARD &&
                 action !=
                     (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
                 !(action ==
                       (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                   world->action_ticks[player_index] == UINT16_C(0) &&
                   (world->dash_direction[player_index] == INT8_C(-2) ||
                    world->dash_direction[player_index] == INT8_C(2))) &&
                 world->dash_direction[player_index] != INT8_C(0)) ||
                ((action ==
                       (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
                  action ==
                       (uint8_t)PF_M4_ACTION_ROLL_BACKWARD) &&
                 world->dash_direction[player_index] == INT8_C(0)) ||
                (action ==
                     (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
                 world->dash_direction[player_index] != INT8_C(-1) &&
                 world->dash_direction[player_index] != INT8_C(0) &&
                 world->dash_direction[player_index] != INT8_C(1)) ||
                (world->short_hop_latched[player_index] != UINT8_C(0) &&
                 action !=
                     (uint8_t)PF_M4_ACTION_JUMP_SQUAT) ||
                ((action == (uint8_t)PF_M4_ACTION_HITLAG) !=
                 (hitlag > UINT16_C(0))) ||
                (hitlag > UINT16_C(0) &&
                 !pf_m4_snapshot_action_is_ground_attack(
                     resume_action) &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_LEDGE_ATTACK &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_RESET_BOUND &&
                  resume_action !=
                      (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
                  resume_action !=
                      (uint8_t)PF_M4_ACTION_WALL_JUMP &&
                 !pf_m4_snapshot_action_is_aerial_attack(
                     resume_action) &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_REFLECTOR_AIR &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_PUMMEL &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_FALCON_KICK_LANDING &&
                 !pf_m4_snapshot_action_is_throw(resume_action) &&
                 resume_action != (uint8_t)PF_M4_ACTION_GRABBED &&
                 !pf_m4_action_is_damage(resume_action) &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 resume_action !=
                     (uint8_t)PF_M4_ACTION_SHIELD_BREAK) ||
                (hitlag == UINT16_C(0) &&
                 resume_action != UINT8_C(0)) ||
                ((pf_m4_snapshot_action_is_ground_attack(
                      resume_action) ||
                  pf_m4_snapshot_action_is_aerial_attack(
                      resume_action) ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_REFLECTOR_AIR ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
                  pf_m4_snapshot_action_is_throw(resume_action)) &&
                 (hitstun_is_active != 0 ||
                  (!pf_m4_snapshot_action_is_aerial_attack(
                       resume_action) &&
                   resume_action !=
                       (uint8_t)PF_M4_ACTION_REFLECTOR_AIR &&
                   world->grounded[player_index] == UINT8_C(0)) ||
                  ((pf_m4_snapshot_action_is_aerial_attack(
                        resume_action) ||
                    resume_action ==
                        (uint8_t)PF_M4_ACTION_REFLECTOR_AIR) &&
                   world->grounded[player_index] != UINT8_C(0)))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
                 (hitstun_is_active != 0 ||
                  tumble != UINT8_C(0) ||
                  world->grounded[player_index] != UINT8_C(0) ||
                  world->action_ticks[player_index] >= UINT16_C(120))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_WALL_JUMP &&
                 (hitstun_is_active != 0 ||
                  tumble != UINT8_C(0) ||
                  world->grounded[player_index] != UINT8_C(0))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_LEDGE_ATTACK &&
                 (hitstun_is_active != 0 ||
                  tumble != UINT8_C(0))) ||
                ((pf_m4_action_is_damage(resume_action) ||
                  resume_action ==
                      (uint8_t)PF_M4_ACTION_RESET_BOUND) &&
                 (hitstun == UINT16_C(0) ||
                  (world->knockback_velocity_x_q16[player_index] ==
                       INT32_C(0) &&
                   world->knockback_velocity_y_q16[player_index] ==
                       INT32_C(0)))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 (shield_stun == UINT16_C(0) ||
                  hitstun_is_active != 0 ||
                  world->grounded[player_index] == UINT8_C(0))) ||
                (resume_action ==
                     (uint8_t)PF_M4_ACTION_SHIELD_BREAK &&
                 (shield_stun != UINT16_C(0) ||
                  shield_health != UINT32_C(0) ||
                  hitstun_is_active != 0 ||
                  world->grounded[player_index] == UINT8_C(0))) ||
                (hitlag == UINT16_C(0) &&
                 action == (uint8_t)PF_M4_ACTION_HITSTUN &&
                 hitstun == UINT16_C(0)) ||
                (hitlag == UINT16_C(0) &&
                 hitstun_is_active != 0 &&
                 !pf_m4_action_is_damage(action) &&
                 action != (uint8_t)PF_M4_ACTION_RESET_BOUND &&
                 !pf_m4_snapshot_action_is_surface_bounce(action)) ||
                (action == (uint8_t)PF_M4_ACTION_HITSTUN &&
                 world->grounded[player_index] != UINT8_C(0)) ||
                (pf_m4_action_is_ground_damage(action) &&
                 (world->grounded[player_index] == UINT8_C(0) ||
                  world->support[player_index] ==
                      (uint8_t)PF_M4_SURFACE_NONE ||
                  world->knockback_velocity_y_q16[player_index] !=
                      INT32_C(0) ||
                  tumble != UINT8_C(0))) ||
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
                 action != (uint8_t)PF_M4_ACTION_SHIELD &&
                 action != (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  (resume_action !=
                       (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                   resume_action !=
                       (uint8_t)PF_M4_ACTION_SHIELD_BREAK))) ||
                (pf_m4_snapshot_action_is_shield_break(action) &&
                 (((action ==
                        (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
                       ? shield_health == UINT32_C(0)
                       : shield_health != UINT32_C(0)) ||
                  shield_stun != UINT16_C(0) ||
                  hitlag != UINT16_C(0) ||
                  hitstun_is_active != 0 ||
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
                 (shield_stun != UINT16_C(0) ||
                  hitlag != UINT16_C(0) ||
                  hitstun_is_active != 0 ||
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
                 !pf_m4_action_is_damage(action) &&
                 action != (uint8_t)PF_M4_ACTION_AIRBORNE &&
                 !pf_m4_snapshot_action_is_surface_bounce(action)) ||
                ((world->sdi_direction_x[player_index] != INT8_C(0) ||
                  world->sdi_direction_y[player_index] != INT8_C(0)) &&
                 (action != (uint8_t)PF_M4_ACTION_HITLAG ||
                  (!pf_m4_action_is_damage(resume_action) &&
                   resume_action !=
                       (uint8_t)PF_M4_ACTION_RESET_BOUND &&
                   resume_action !=
                       (uint8_t)PF_M4_ACTION_SHIELD_STUN))) ||
                (action == (uint8_t)PF_M4_ACTION_HITLAG &&
                 resume_action ==
                     (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
                 world->sdi_direction_y[player_index] != INT8_C(0)) ||
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
                      (uint8_t)PF_M4_ACTION_FORCED_GETUP ||
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
                  action == (uint8_t)PF_M4_ACTION_GRAB ||
                  action == (uint8_t)PF_M4_ACTION_DASH_GRAB ||
                  action == (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
                  action == (uint8_t)PF_M4_ACTION_PUMMEL ||
                  action == (uint8_t)PF_M4_ACTION_GRABBED ||
                  action == (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
                  pf_m4_snapshot_action_is_throw(action) ||
                   action == (uint8_t)PF_M4_ACTION_ITEM_THROW ||
                   action ==
                       (uint8_t)PF_M4_ACTION_ITEM_DASH_THROW ||
                   action ==
                       (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND ||
                   action ==
                       (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
                   action ==
                       (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
                   action ==
                       (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND ||
                   action ==
                       (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND ||
                   action ==
                       (uint8_t)PF_M4_ACTION_MOONWALK_SETUP ||
                   action ==
                       (uint8_t)PF_M4_ACTION_MOONWALK ||
                   action == (uint8_t)PF_M4_ACTION_TEETER ||
                   action == (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
                   action == (uint8_t)PF_M4_ACTION_TAUNT ||
                   action == (uint8_t)PF_M4_ACTION_WALL_JUMP ||
                   action == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
                 pf_m4_snapshot_action_is_surface_tech(action)) &&
                 (hitlag != UINT16_C(0) ||
                  hitstun_is_active != 0 ||
                  tumble != UINT8_C(0))) ||
                (pf_m4_snapshot_action_is_surface_bounce(action) &&
                 (hitlag != UINT16_C(0) ||
                  tumble == UINT8_C(0))) ||
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
                (world->damage_jump_buffer_ticks[player_index] !=
                     UINT16_C(0) &&
                 !pf_m4_action_is_damage(action) &&
                 !pf_m4_snapshot_action_is_surface_bounce(action) &&
                 !(action == (uint8_t)PF_M4_ACTION_HITLAG &&
                   (pf_m4_action_is_damage(resume_action) ||
                    pf_m4_snapshot_action_is_surface_bounce(
                        resume_action)))) ||
                !pf_m4_player_state_consistent(world, player_index))
            {
                return PF_STATUS_INVALID_STATE;
            }
            if (world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_CATCH ||
                world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
                world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
                world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
                world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
                world->action_state[player_index] ==
                    (uint8_t)PF_M4_ACTION_LEDGE_JUMP ||
                (world->action_state[player_index] ==
                     (uint8_t)PF_M4_ACTION_HITLAG &&
                 world->hitlag_resume_action[player_index] ==
                     (uint8_t)PF_M4_ACTION_LEDGE_ATTACK))
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
                 world->match_kos[player_index] != UINT16_C(0) ||
                 world->match_falls[player_index] != UINT16_C(0) ||
                 world->shield_recoil_x_q16[player_index] !=
                     INT32_C(0) ||
                 world->action_ticks[player_index] != UINT16_C(0) ||
                 world->source_submotion[player_index] != UINT16_C(0) ||
                 world->source_animation_frame_q16[player_index] !=
                     INT32_C(0) ||
                 world->source_animation_rate_q16[player_index] !=
                     INT32_C(0) ||
                 world->respawn_count[player_index] != UINT16_C(0) ||
                 world->team[player_index] != UINT8_C(0) ||
                 world->grounded[player_index] != UINT8_C(0) ||
                 world->active[player_index] != UINT8_C(0) ||
                 world->action_state[player_index] != UINT8_C(0) ||
                 world->support[player_index] != UINT8_C(0) ||
                 world->air_jumps_remaining[player_index] != UINT8_C(0) ||
                 world->recovery_available[player_index] != UINT8_C(0) ||
                 world->short_hop_latched[player_index] != UINT8_C(0) ||
                 world->platform_drop_ticks[player_index] != UINT8_C(0) ||
                 world->fast_fall[player_index] != UINT8_C(0) ||
                 world->facing[player_index] != INT8_C(0) ||
                 world->dash_direction[player_index] != INT8_C(0) ||
                 world->previous_strong_direction[player_index] !=
                     INT8_C(0) ||
                 world->previous_directional_input_flags[player_index] !=
                     UINT8_C(0) ||
                 world->previous_tilt_x_direction[player_index] !=
                     INT8_C(0) ||
                 world->previous_tilt_y_direction[player_index] !=
                     INT8_C(0) ||
                 world->mash_stick_x_direction[player_index] !=
                     INT8_C(0) ||
                 world->mash_stick_y_direction[player_index] !=
                     INT8_C(0) ||
                 world->previous_secondary_stick_x[player_index] !=
                     INT16_C(0) ||
                 world->previous_secondary_stick_y[player_index] !=
                     INT16_C(0) ||
                 world->tilt_x_age[player_index] != UINT8_C(0) ||
                 world->tilt_y_age[player_index] != UINT8_C(0) ||
                 world->horizontal_input_age[player_index] != UINT8_C(0) ||
                 world->horizontal_input_direction[player_index] !=
                     INT8_C(0) ||
                 world->damage_q16[player_index] != UINT32_C(0) ||
                 world->knockback_velocity_x_q16[player_index] !=
                     INT32_C(0) ||
                 world->knockback_velocity_y_q16[player_index] !=
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
                 world->attack_stale_registered[player_index] !=
                     UINT8_C(0) ||
                 world->rebound_duration_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->jab_chain_buffered[player_index] != UINT8_C(0) ||
                 world->rapid_jab_input_count[player_index] != UINT8_C(0) ||
                 world->rapid_jab_continue[player_index] != UINT8_C(0) ||
                 world->down_tilt_repeat_buffered[player_index] !=
                     UINT8_C(0) ||
                 world->stale_move_count[player_index] != UINT8_C(0) ||
                 world->last_hit_attacker[player_index] != UINT8_C(0) ||
                 world->tech_window_ticks[player_index] != UINT16_C(0) ||
                 world->tech_lockout_ticks[player_index] != UINT16_C(0) ||
                 world->shield_stun_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->shield_health_q16[player_index] !=
                     UINT32_C(0) ||
                 world->shield_held[player_index] != UINT8_C(0) ||
                 world->trigger_input_age[player_index] != UINT8_C(0) ||
                 world->prone_attack_input_age[player_index] !=
                     UINT8_C(0) ||
                 world->powershield[player_index] != UINT8_C(0) ||
                 world->tumble[player_index] != UINT8_C(0) ||
                 world->sdi_pulse_count[player_index] != UINT8_C(0) ||
                 world->sdi_direction_x[player_index] != INT8_C(0) ||
                 world->sdi_direction_y[player_index] != INT8_C(0) ||
                 world->tech_direction[player_index] != INT8_C(0) ||
                 world->prone_orientation[player_index] !=
                     (uint8_t)PF_M4_PRONE_NONE ||
                 world->prone_roll_motion_orientation[player_index] !=
                     (uint8_t)PF_M4_PRONE_NONE ||
                 world->respawn_ticks[player_index] != UINT16_C(0) ||
                 world->respawn_invulnerability_ticks[player_index] !=
                     UINT16_C(0) ||
                 world->ledge_invulnerability_ticks[player_index] !=
                     UINT16_C(0) ||
                  world->ledge_regrab_lockout_ticks[player_index] !=
                      UINT16_C(0) ||
                  world->grab_escape_ticks[player_index] != UINT16_C(0) ||
                  world->damage_jump_buffer_ticks[player_index] !=
                      UINT16_C(0) ||
                  world->charge_ticks[player_index] != UINT16_C(0) ||
                  world->smash_charge_ticks[player_index] !=
                      UINT16_C(0) ||
                   world->shield_strength[player_index] != UINT16_C(0) ||
                   world->shield_angle_turn[player_index] != UINT16_C(0) ||
                   world->shield_magnitude[player_index] != UINT16_C(0) ||
                  world->grab_target_slot[player_index] != UINT8_C(0) ||
                  world->grab_owner_slot[player_index] != UINT8_C(0) ||
                  world->stocks_remaining[player_index] != UINT8_C(0))
        {
            return PF_STATUS_INVALID_STATE;
        }
    }

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const uint8_t target_slot =
            world->grab_target_slot[player_index];
        const uint8_t owner_slot =
            world->grab_owner_slot[player_index];

        if (target_slot != UINT8_C(0))
        {
            const uint32_t target_index =
                (uint32_t)target_slot - UINT32_C(1);

            if (target_index == player_index ||
                world->active[target_index] == UINT8_C(0) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[player_index] == world->team[target_index]) ||
                (!pf_m4_snapshot_action_is_normal_grab_holder(
                     world->action_state[player_index],
                     world->hitlag_resume_action[player_index]) &&
                 !pf_m4_snapshot_action_is_falcon_dive_capture_holder(
                     world->action_state[player_index],
                     world->hitlag_resume_action[player_index]) &&
                 !pf_m4_snapshot_action_is_throw_holder(
                     world->action_state[player_index],
                     world->hitlag_resume_action[player_index])) ||
                world->grab_owner_slot[target_index] !=
                    (uint8_t)(player_index + UINT32_C(1)) ||
                !pf_m4_snapshot_action_is_grabbed(
                    world->action_state[target_index],
                    world->hitlag_resume_action[target_index]) ||
                world->grab_escape_ticks[target_index] == UINT16_C(0))
            {
                return PF_STATUS_INVALID_STATE;
            }
        }
        else if (pf_m4_snapshot_action_is_normal_grab_holder(
                     world->action_state[player_index],
                     world->hitlag_resume_action[player_index]))
        {
            return PF_STATUS_INVALID_STATE;
        }

        if (owner_slot != UINT8_C(0))
        {
            const uint32_t owner_index =
                (uint32_t)owner_slot - UINT32_C(1);

            if (owner_index == player_index ||
                world->active[owner_index] == UINT8_C(0) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[player_index] == world->team[owner_index]) ||
                !pf_m4_snapshot_action_is_grabbed(
                    world->action_state[player_index],
                    world->hitlag_resume_action[player_index]) ||
                (!pf_m4_snapshot_action_is_normal_grab_holder(
                     world->action_state[owner_index],
                     world->hitlag_resume_action[owner_index]) &&
                 !pf_m4_snapshot_action_is_falcon_dive_capture_holder(
                     world->action_state[owner_index],
                     world->hitlag_resume_action[owner_index]) &&
                 !pf_m4_snapshot_action_is_throw_holder(
                     world->action_state[owner_index],
                     world->hitlag_resume_action[owner_index])) ||
                world->grab_target_slot[owner_index] !=
                    (uint8_t)(player_index + UINT32_C(1)) ||
                world->grab_escape_ticks[player_index] == UINT16_C(0))
            {
                return PF_STATUS_INVALID_STATE;
            }
        }
        else if (pf_m4_snapshot_action_is_grabbed(
                     world->action_state[player_index],
                     world->hitlag_resume_action[player_index]) ||
                 world->grab_escape_ticks[player_index] != UINT16_C(0))
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
    if (!pf_m4_snapshot_content_state_consistent(
            &sim->content,
            &sim->world))
    {
        return PF_STATUS_INVALID_STATE;
    }
    if (destination->bytes == NULL ||
        destination->capacity < PF_SIM_SAVE_TOTAL_BYTES)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }

    if (!pf_write_save_bytes(
            destination->bytes,
            destination->capacity,
            &sim->world))
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }
    return PF_STATUS_OK;
}

pf_status pf_sim_hash(
    const pf_sim *sim,
    pf_state_hash *out_hash)
{
    uint8_t save_bytes[PF_SIM_SAVE_TOTAL_BYTES];
    pf_sha256 hash;
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
    if (!pf_m4_snapshot_content_state_consistent(
            &sim->content,
            &sim->world))
    {
        return PF_STATUS_INVALID_STATE;
    }

    if (!pf_write_save_bytes(
            save_bytes,
            sizeof(save_bytes),
            &sim->world))
    {
        return PF_STATUS_INVALID_STATE;
    }
    pf_sha256_init(&hash);
    pf_sha256_update(&hash, save_bytes, sizeof(save_bytes));
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
    if (!pf_m4_snapshot_content_state_consistent(
            &source->content,
            &source->world))
    {
        return PF_STATUS_INVALID_STATE;
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

    for (uint32_t player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        if (candidate.source_submotion[player_index] !=
            pf_m4_snapshot_canonical_source_submotion(
                &candidate,
                player_index) ||
            candidate.source_animation_frame_q16[player_index] !=
                pf_m4_snapshot_canonical_source_animation_frame_q16(
                    &candidate,
                    player_index) ||
            candidate.source_animation_rate_q16[player_index] !=
                pf_m4_snapshot_canonical_source_animation_rate_q16(
                    &candidate,
                    player_index))
        {
            return PF_STATUS_INVALID_STATE;
        }
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
    if (!pf_m4_snapshot_content_state_consistent(
            &sim->content,
            &candidate))
    {
        return PF_STATUS_INVALID_STATE;
    }

    sim->world = candidate;
    (void)memset(sim->scratch, 0, sizeof(*sim->scratch));
    sim->has_reset = UINT8_C(1);
    return PF_STATUS_OK;
}
