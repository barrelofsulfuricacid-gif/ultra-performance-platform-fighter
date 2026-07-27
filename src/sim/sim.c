#include "sim_internal.h"

#include <limits.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(uint8_t) == 1U,
               "pf_sim requires an exact 8-bit uint8_t");
_Static_assert(sizeof(uint16_t) == 2U,
               "pf_sim requires an exact 16-bit uint16_t");
_Static_assert(sizeof(uint32_t) == 4U,
               "pf_sim requires an exact 32-bit uint32_t");
_Static_assert(sizeof(uint64_t) == 8U,
               "pf_sim requires an exact 64-bit uint64_t");
_Static_assert(sizeof(int32_t) == 4U,
               "pf_sim requires an exact 32-bit int32_t");
_Static_assert(PF_SIM_MAX_PLAYERS == UINT32_C(4),
               "M2 state layout assumes four player slots");

static int pf_is_aligned(const void *memory, size_t alignment)
{
    return memory != NULL &&
           ((uintptr_t)memory % (uintptr_t)alignment) == (uintptr_t)0;
}

static int pf_ranges_overlap(
    const void *left_memory,
    size_t left_bytes,
    const void *right_memory,
    size_t right_bytes)
{
    const uintptr_t left_begin = (uintptr_t)left_memory;
    const uintptr_t right_begin = (uintptr_t)right_memory;
    uintptr_t left_end;
    uintptr_t right_end;

    if (left_begin > UINTPTR_MAX - (uintptr_t)left_bytes ||
        right_begin > UINTPTR_MAX - (uintptr_t)right_bytes)
    {
        return 1;
    }

    left_end = left_begin + (uintptr_t)left_bytes;
    right_end = right_begin + (uintptr_t)right_bytes;
    return left_begin < right_end && right_begin < left_end;
}

static int32_t pf_clamp_q16(int64_t value, int32_t minimum, int32_t maximum)
{
    if (value < (int64_t)minimum)
    {
        return minimum;
    }
    if (value > (int64_t)maximum)
    {
        return maximum;
    }
    return (int32_t)value;
}

uint32_t pf_sim_abi_version(void)
{
    return PF_SIM_ABI_VERSION;
}

uint32_t pf_sim_tick_rate_hz(void)
{
    return PF_SIM_TICK_RATE_HZ;
}

const char *pf_status_name(pf_status status)
{
    switch (status)
    {
        case PF_STATUS_OK:
            return "ok";
        case PF_STATUS_INVALID_ARGUMENT:
            return "invalid-argument";
        case PF_STATUS_UNSUPPORTED_VERSION:
            return "unsupported-version";
        case PF_STATUS_BUFFER_TOO_SMALL:
            return "buffer-too-small";
        case PF_STATUS_MISALIGNED_MEMORY:
            return "misaligned-memory";
        case PF_STATUS_INVALID_CONFIG:
            return "invalid-config";
        case PF_STATUS_TICK_MISMATCH:
            return "tick-mismatch";
        case PF_STATUS_EPISODE_DONE:
            return "episode-done";
        case PF_STATUS_INVALID_STATE:
            return "invalid-state";
        case PF_STATUS_DETERMINISTIC_FAULT:
            return "deterministic-fault";
        case PF_STATUS_INCOMPATIBLE_STATE:
            return "incompatible-state";
        case PF_STATUS_CHECKSUM_MISMATCH:
            return "checksum-mismatch";
        default:
            return "unknown-status";
    }
}

pf_status pf_sim_default_config(
    pf_sim_config *out_config,
    uint8_t player_count,
    pf_sim_mode mode)
{
    if (out_config == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(out_config, 0, sizeof(*out_config));
    out_config->struct_size = (uint32_t)sizeof(*out_config);
    out_config->schema_version = PF_SIM_CONFIG_SCHEMA_VERSION;
    out_config->player_count = player_count;
    out_config->mode = (uint8_t)mode;
    out_config->max_ticks = UINT64_C(3600);
    out_config->arena_half_width_q16 = INT32_C(64) * PF_Q16_ONE;
    out_config->arena_ceiling_q16 = INT32_C(64) * PF_Q16_ONE;

    return pf_sim_validate_config(out_config);
}

pf_status pf_sim_validate_config(const pf_sim_config *config)
{
    const int32_t minimum_arena_q16 = INT32_C(16) * PF_Q16_ONE;
    const int32_t maximum_arena_q16 = INT32_C(4096) * PF_Q16_ONE;

    if (config == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (config->struct_size != (uint32_t)sizeof(*config) ||
        config->schema_version != PF_SIM_CONFIG_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if ((config->mode == (uint8_t)PF_SIM_MODE_DUEL &&
         config->player_count != UINT8_C(2)) ||
        (config->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
         config->player_count != UINT8_C(4)) ||
        (config->mode != (uint8_t)PF_SIM_MODE_DUEL &&
         config->mode != (uint8_t)PF_SIM_MODE_TEAMS))
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    if (config->max_ticks == UINT64_C(0) ||
        config->max_ticks == UINT64_MAX)
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    if (config->arena_half_width_q16 < minimum_arena_q16 ||
        config->arena_half_width_q16 > maximum_arena_q16 ||
        config->arena_ceiling_q16 < minimum_arena_q16 ||
        config->arena_ceiling_q16 > maximum_arena_q16)
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    return PF_STATUS_OK;
}

pf_status pf_sim_query_memory(
    const pf_sim_config *config,
    pf_memory_requirements *out_requirements)
{
    pf_status status;

    if (out_requirements == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    status = pf_sim_validate_config(config);
    if (status != PF_STATUS_OK)
    {
        return status;
    }

    out_requirements->state_bytes = sizeof(pf_sim);
    out_requirements->state_alignment = alignof(pf_sim);
    out_requirements->scratch_bytes = sizeof(pf_sim_scratch);
    out_requirements->scratch_alignment = alignof(pf_sim_scratch);
    return PF_STATUS_OK;
}

int pf_sim_is_valid(const pf_sim *sim)
{
    return sim != NULL && sim->magic == PF_SIM_HANDLE_MAGIC &&
           sim->scratch != NULL;
}

pf_status pf_sim_init(
    void *state_memory,
    size_t state_bytes,
    void *scratch_memory,
    size_t scratch_bytes,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim)
{
    pf_memory_requirements requirements;
    pf_status status;
    pf_sim *sim;

    if (out_sim == NULL || content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    *out_sim = NULL;

    if (content->struct_size != (uint32_t)sizeof(*content) ||
        content->schema_version != PF_SIM_CONTENT_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (content->reserved != UINT16_C(0) ||
        (content->byte_count != (size_t)0 && content->bytes == NULL))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    status = pf_sim_query_memory(config, &requirements);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (state_bytes < requirements.state_bytes ||
        scratch_bytes < requirements.scratch_bytes)
    {
        return PF_STATUS_BUFFER_TOO_SMALL;
    }
    if (!pf_is_aligned(state_memory, requirements.state_alignment) ||
        !pf_is_aligned(scratch_memory, requirements.scratch_alignment))
    {
        return PF_STATUS_MISALIGNED_MEMORY;
    }
    if (pf_ranges_overlap(
            state_memory,
            requirements.state_bytes,
            scratch_memory,
            requirements.scratch_bytes))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    sim = (pf_sim *)state_memory;
    (void)memset(sim, 0, sizeof(*sim));
    (void)memset(scratch_memory, 0, sizeof(pf_sim_scratch));

    sim->magic = PF_SIM_HANDLE_MAGIC;
    sim->scratch = (pf_sim_scratch *)scratch_memory;
    sim->world.content_hash = content->content_hash;
    sim->world.max_ticks = config->max_ticks;
    sim->world.state_schema_version = PF_SIM_STATE_SCHEMA_VERSION;
    sim->world.arithmetic_version = PF_SIM_ARITHMETIC_VERSION;
    sim->world.rng_version = PF_SIM_RNG_VERSION;
    sim->world.input_schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
    sim->world.arena_half_width_q16 = config->arena_half_width_q16;
    sim->world.arena_ceiling_q16 = config->arena_ceiling_q16;
    sim->world.player_count = config->player_count;
    sim->world.mode = config->mode;

    *out_sim = sim;
    return PF_STATUS_OK;
}

uint64_t pf_sim_rng_next(uint64_t *state)
{
    uint64_t value;

    *state += UINT64_C(0x9E3779B97F4A7C15);
    value = *state;
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

pf_status pf_sim_reset(pf_sim *sim, uint64_t seed)
{
    pf_world_state preserved;
    uint32_t player_index;

    if (!pf_sim_is_valid(sim))
    {
        return PF_STATUS_INVALID_STATE;
    }

    preserved = sim->world;
    (void)memset(&sim->world, 0, sizeof(sim->world));
    sim->world.content_hash = preserved.content_hash;
    sim->world.max_ticks = preserved.max_ticks;
    sim->world.state_schema_version = preserved.state_schema_version;
    sim->world.arithmetic_version = preserved.arithmetic_version;
    sim->world.rng_version = preserved.rng_version;
    sim->world.input_schema_version = preserved.input_schema_version;
    sim->world.arena_half_width_q16 = preserved.arena_half_width_q16;
    sim->world.arena_ceiling_q16 = preserved.arena_ceiling_q16;
    sim->world.player_count = preserved.player_count;
    sim->world.mode = preserved.mode;
    sim->world.seed = seed;
    sim->world.rng_state = seed;

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)sim->world.player_count;
         ++player_index)
    {
        const int32_t centered_slot =
            (int32_t)(UINT32_C(2) * player_index + UINT32_C(1)) -
            (int32_t)sim->world.player_count;
        const int32_t base_position_q16 =
            centered_slot * INT32_C(8) * PF_Q16_ONE;
        const uint64_t random_value =
            pf_sim_rng_next(&sim->world.rng_state);
        const int32_t jitter_q16 =
            (int32_t)((random_value >> 48U) & UINT64_C(0xFFFF)) -
            INT32_C(32768);

        sim->world.position_x_q16[player_index] = pf_clamp_q16(
            (int64_t)base_position_q16 + (int64_t)jitter_q16,
            -sim->world.arena_half_width_q16,
            sim->world.arena_half_width_q16);
        sim->world.position_y_q16[player_index] = INT32_C(0);
        sim->world.velocity_x_q16[player_index] = INT32_C(0);
        sim->world.velocity_y_q16[player_index] = INT32_C(0);
        sim->world.team[player_index] =
            sim->world.mode == (uint8_t)PF_SIM_MODE_TEAMS
                ? (uint8_t)(player_index & UINT32_C(1))
                : (uint8_t)player_index;
        sim->world.grounded[player_index] = UINT8_C(1);
        sim->world.active[player_index] = UINT8_C(1);
    }

    (void)memset(sim->scratch, 0, sizeof(*sim->scratch));
    sim->has_reset = UINT8_C(1);
    return PF_STATUS_OK;
}

pf_status pf_sim_tick(
    pf_sim *sim,
    const pf_input_frame *inputs,
    size_t player_count,
    pf_tick_result *out_result)
{
    return pf_sim_tick_impl(sim, inputs, player_count, out_result);
}

pf_status pf_sim_observe(
    const pf_sim *sim,
    pf_sim_observation *out_observation)
{
    uint32_t player_index;

    if (out_observation == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    (void)memset(out_observation, 0, sizeof(*out_observation));
    out_observation->tick = sim->world.tick;
    out_observation->seed = sim->world.seed;
    out_observation->fault_flags = sim->world.fault_flags;
    out_observation->schema_version =
        PF_SIM_OBSERVATION_SCHEMA_VERSION;
    out_observation->player_count = sim->world.player_count;
    out_observation->mode = sim->world.mode;
    out_observation->terminated = sim->world.terminated;
    out_observation->truncated = sim->world.truncated;
    out_observation->winner_mask = sim->world.winner_mask;

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_player_observation *player =
            &out_observation->players[player_index];
        player->previous_buttons =
            sim->world.previous_buttons[player_index];
        player->position_x_q16 =
            sim->world.position_x_q16[player_index];
        player->position_y_q16 =
            sim->world.position_y_q16[player_index];
        player->velocity_x_q16 =
            sim->world.velocity_x_q16[player_index];
        player->velocity_y_q16 =
            sim->world.velocity_y_q16[player_index];
        player->player_slot = (uint8_t)player_index;
        player->team = sim->world.team[player_index];
        player->grounded = sim->world.grounded[player_index];
        player->active = sim->world.active[player_index];
    }

    return PF_STATUS_OK;
}
