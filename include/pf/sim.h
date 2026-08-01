#ifndef PF_SIM_H
#define PF_SIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PF_SIM_ABI_VERSION UINT32_C(4)
#define PF_SIM_TICK_RATE_HZ UINT32_C(60)
#define PF_SIM_CONFIG_SCHEMA_VERSION UINT16_C(2)
#define PF_SIM_CONTENT_SCHEMA_VERSION UINT16_C(1)
#define PF_SIM_INPUT_SCHEMA_VERSION UINT16_C(5)
#define PF_SIM_STATE_SCHEMA_VERSION UINT16_C(35)
#define PF_SIM_OBSERVATION_SCHEMA_VERSION UINT16_C(5)
#define PF_SIM_IDENTITY_SCHEMA_VERSION UINT16_C(2)
#define PF_SIM_ARITHMETIC_VERSION UINT16_C(1)
#define PF_SIM_RNG_VERSION UINT16_C(1)
#define PF_SIM_SAVE_FORMAT_VERSION UINT16_C(34)
#define PF_SIM_STATE_HASH_ALGORITHM_SHA256 UINT16_C(1)
#define PF_SIM_STATE_HASH_ALGORITHM_VERSION UINT16_C(1)
#define PF_SIM_STATE_HASH_BYTES UINT16_C(32)
#define PF_SIM_MAX_PLAYERS UINT32_C(4)
#define PF_SIM_MAX_EVENTS_PER_TICK UINT8_C(16)
#define PF_SIM_EVENT_NO_PLAYER UINT8_MAX
#define PF_SIM_MAX_STOCK_COUNT UINT8_C(99)
#define PF_SIM_MAX_RESPAWN_TICKS UINT16_C(3600)
#define PF_SIM_DEFAULT_STOCK_COUNT UINT8_C(4)
#define PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS UINT16_C(60)
#define PF_SIM_DEFAULT_RESPAWN_INVULNERABILITY_TICKS UINT16_C(120)
#define PF_Q16_ONE INT32_C(65536)

#define PF_INPUT_BUTTON_JUMP (UINT64_C(1) << 0U)
#define PF_INPUT_BUTTON_ATTACK (UINT64_C(1) << 1U)
#define PF_INPUT_BUTTON_STRONG_ATTACK (UINT64_C(1) << 2U)
#define PF_INPUT_BUTTON_SPECIAL (UINT64_C(1) << 3U)
#define PF_INPUT_BUTTON_TAUNT (UINT64_C(1) << 4U)
#define PF_INPUT_BUTTON_FORFEIT (UINT64_C(1) << 63U)
#define PF_INPUT_KNOWN_BUTTONS                                             \
    (PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK |                       \
     PF_INPUT_BUTTON_STRONG_ATTACK | PF_INPUT_BUTTON_SPECIAL |             \
     PF_INPUT_BUTTON_TAUNT | PF_INPUT_BUTTON_FORFEIT)

typedef enum pf_status
{
    PF_STATUS_OK = 0,
    PF_STATUS_INVALID_ARGUMENT = 1,
    PF_STATUS_UNSUPPORTED_VERSION = 2,
    PF_STATUS_BUFFER_TOO_SMALL = 3,
    PF_STATUS_MISALIGNED_MEMORY = 4,
    PF_STATUS_INVALID_CONFIG = 5,
    PF_STATUS_TICK_MISMATCH = 6,
    PF_STATUS_EPISODE_DONE = 7,
    PF_STATUS_INVALID_STATE = 8,
    PF_STATUS_DETERMINISTIC_FAULT = 9,
    PF_STATUS_INCOMPATIBLE_STATE = 10,
    PF_STATUS_CHECKSUM_MISMATCH = 11,
    PF_STATUS_REPLAY_MISMATCH = 12
} pf_status;

typedef enum pf_sim_mode
{
    PF_SIM_MODE_DUEL = 1,
    PF_SIM_MODE_TEAMS = 2
} pf_sim_mode;

typedef enum pf_sim_fault
{
    PF_SIM_FAULT_NONE = 0,
    PF_SIM_FAULT_ARITHMETIC = 1 << 0,
    PF_SIM_FAULT_CAPACITY = 1 << 1,
    PF_SIM_FAULT_INVALID_STATE = 1 << 2
} pf_sim_fault;

typedef enum pf_sim_event_type
{
    PF_SIM_EVENT_NONE = 0,
    PF_SIM_EVENT_HIT = 1,
    PF_SIM_EVENT_SHIELD_BLOCK = 2,
    PF_SIM_EVENT_POWERSHIELD = 3,
    PF_SIM_EVENT_SHIELD_BREAK = 4,
    PF_SIM_EVENT_KO = 5,
    PF_SIM_EVENT_RESPAWN = 6,
    PF_SIM_EVENT_SUDDEN_DEATH = 7,
    PF_SIM_EVENT_MATCH_RESULT = 8,
    PF_SIM_EVENT_FORFEIT = 9,
    PF_SIM_EVENT_TIME_LIMIT = 10,
    PF_SIM_EVENT_GRAB = 11,
    PF_SIM_EVENT_GRAB_ESCAPE = 12,
    PF_SIM_EVENT_THROW = 13,
    PF_SIM_EVENT_ITEM_PICKUP = 14,
    PF_SIM_EVENT_ITEM_DROP = 15,
    PF_SIM_EVENT_ITEM_THROW = 16,
    PF_SIM_EVENT_ITEM_HIT = 17,
    PF_SIM_EVENT_ITEM_RESET = 18,
    PF_SIM_EVENT_PROJECTILE_FIRE = 19,
    PF_SIM_EVENT_PROJECTILE_HIT = 20,
    PF_SIM_EVENT_PROJECTILE_REFLECT = 21
} pf_sim_event_type;

typedef enum pf_sim_event_flag
{
    PF_SIM_EVENT_FLAG_NONE = 0,
    PF_SIM_EVENT_FLAG_TUMBLE = 1 << 0,
    PF_SIM_EVENT_FLAG_ELIMINATED = 1 << 1,
    PF_SIM_EVENT_FLAG_LAST_STOCK = 1 << 2,
    PF_SIM_EVENT_FLAG_SUDDEN_DEATH = 1 << 3
} pf_sim_event_flag;

typedef struct pf_sim_event
{
    uint64_t tick;
    uint32_t sequence;
    uint32_t value_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint16_t type;
    uint16_t flags;
    uint16_t detail;
    uint8_t source_player;
    uint8_t target_player;
} pf_sim_event;

typedef struct pf_hash256
{
    uint8_t bytes[32];
} pf_hash256;

typedef struct pf_bytes
{
    const uint8_t *bytes;
    size_t size;
} pf_bytes;

typedef struct pf_mut_bytes
{
    uint8_t *bytes;
    size_t capacity;
    size_t size;
} pf_mut_bytes;

typedef struct pf_state_hash
{
    uint16_t algorithm;
    uint16_t algorithm_version;
    uint16_t digest_size;
    uint16_t reserved;
    uint8_t bytes[PF_SIM_STATE_HASH_BYTES];
} pf_state_hash;

typedef struct pf_content_view
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    const void *bytes;
    size_t byte_count;
    pf_hash256 content_hash;
} pf_content_view;

typedef struct pf_sim_identity
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint16_t reserved;
    uint32_t sim_abi_version;
    uint32_t tick_rate_hz;
    uint16_t config_schema_version;
    uint16_t content_schema_version;
    uint16_t input_schema_version;
    uint16_t state_schema_version;
    uint16_t observation_schema_version;
    uint16_t arithmetic_version;
    uint16_t rng_version;
    uint16_t save_format_version;
    uint8_t player_count;
    uint8_t mode;
    uint16_t reserved2;
    uint64_t max_ticks;
    int32_t arena_half_width_q16;
    int32_t arena_ceiling_q16;
    uint8_t stock_count;
    uint8_t reserved3;
    uint16_t respawn_delay_ticks;
    uint16_t respawn_invulnerability_ticks;
    uint16_t reserved4;
    pf_hash256 content_hash;
    pf_hash256 config_hash;
} pf_sim_identity;

typedef struct pf_sim_config
{
    uint32_t struct_size;
    uint16_t schema_version;
    uint8_t player_count;
    uint8_t mode;
    uint64_t max_ticks;
    int32_t arena_half_width_q16;
    int32_t arena_ceiling_q16;
    uint8_t stock_count;
    uint8_t reserved2;
    uint16_t respawn_delay_ticks;
    uint16_t respawn_invulnerability_ticks;
    uint16_t reserved3;
} pf_sim_config;

typedef struct pf_memory_requirements
{
    size_t state_bytes;
    size_t state_alignment;
    size_t scratch_bytes;
    size_t scratch_alignment;
} pf_memory_requirements;

typedef struct pf_input_frame
{
    uint64_t tick;
    uint64_t buttons;
    int16_t main_stick_x;
    int16_t main_stick_y;
    int16_t secondary_stick_x;
    int16_t secondary_stick_y;
    uint16_t left_trigger;
    uint16_t right_trigger;
    uint16_t schema_version;
    uint8_t player_slot;
    uint8_t reserved;
} pf_input_frame;

typedef struct pf_tick_result
{
    uint64_t completed_tick;
    uint32_t fault_flags;
    uint8_t terminated;
    uint8_t truncated;
    uint8_t winner_mask;
    uint8_t reserved;
    uint8_t event_count;
    uint8_t reserved2;
    uint16_t reserved3;
    pf_sim_event events[PF_SIM_MAX_EVENTS_PER_TICK];
} pf_tick_result;

typedef struct pf_player_observation
{
    uint64_t previous_buttons;
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint8_t player_slot;
    uint8_t team;
    uint8_t grounded;
    uint8_t active;
    uint8_t stocks_remaining;
    uint8_t reserved;
    uint16_t respawn_ticks;
    uint16_t respawn_invulnerability_ticks;
    uint16_t charge_ticks;
} pf_player_observation;

typedef struct pf_item_observation
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint16_t lifetime_ticks;
    uint16_t respawn_ticks;
    uint16_t pickup_lockout_ticks;
    uint8_t state;
    uint8_t holder_slot;
    uint8_t source_slot;
    uint8_t throw_direction;
    uint8_t hit_mask;
    uint8_t reserved[3];
} pf_item_observation;

typedef struct pf_projectile_observation
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint16_t lifetime_ticks;
    uint8_t state;
    uint8_t owner_slot;
} pf_projectile_observation;

typedef struct pf_sim_observation
{
    uint64_t tick;
    uint64_t seed;
    uint32_t fault_flags;
    uint16_t schema_version;
    uint8_t player_count;
    uint8_t mode;
    uint8_t terminated;
    uint8_t truncated;
    uint8_t winner_mask;
    uint8_t sudden_death;
    uint8_t stock_count;
    uint8_t reserved;
    pf_item_observation item;
    pf_projectile_observation projectile;
    pf_player_observation players[PF_SIM_MAX_PLAYERS];
} pf_sim_observation;

typedef struct pf_sim pf_sim;

uint32_t pf_sim_abi_version(void);
uint32_t pf_sim_tick_rate_hz(void);
const char *pf_status_name(pf_status status);

pf_status pf_sim_default_config(
    pf_sim_config *out_config,
    uint8_t player_count,
    pf_sim_mode mode);

pf_status pf_sim_query_memory(
    const pf_sim_config *config,
    pf_memory_requirements *out_requirements);

pf_status pf_sim_init(
    void *state_memory,
    size_t state_bytes,
    void *scratch_memory,
    size_t scratch_bytes,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim);

pf_status pf_sim_deinit(pf_sim *sim);

pf_status pf_sim_reset(pf_sim *sim, uint64_t seed);

pf_status pf_sim_tick(
    pf_sim *sim,
    const pf_input_frame *inputs,
    size_t player_count,
    pf_tick_result *out_result);

pf_status pf_sim_observe(
    const pf_sim *sim,
    pf_sim_observation *out_observation);

pf_status pf_sim_query_identity(
    const pf_sim *sim,
    pf_sim_identity *out_identity);

pf_status pf_sim_query_save_size(
    const pf_sim *sim,
    size_t *out_save_bytes);

pf_status pf_sim_save(
    const pf_sim *sim,
    pf_mut_bytes *destination);

pf_status pf_sim_load(
    pf_sim *sim,
    pf_bytes source);

pf_status pf_sim_clone(
    pf_sim *destination,
    const pf_sim *source);

pf_status pf_sim_hash(
    const pf_sim *sim,
    pf_state_hash *out_hash);

#ifdef __cplusplus
}
#endif

#endif
