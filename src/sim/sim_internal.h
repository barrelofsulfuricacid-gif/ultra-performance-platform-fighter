#ifndef PF_SIM_INTERNAL_H
#define PF_SIM_INTERNAL_H

#include "pf/sim.h"

#include <stdint.h>

#define PF_SIM_HANDLE_MAGIC UINT64_C(0x504653494D303032)
#define PF_M2_MAX_HORIZONTAL_SPEED_Q16 INT32_C(131072)
#define PF_M2_JUMP_SPEED_Q16 INT32_C(196608)

typedef struct pf_world_state
{
    pf_hash256 content_hash;
    uint64_t tick;
    uint64_t seed;
    uint64_t rng_state;
    uint64_t max_ticks;
    uint32_t fault_flags;
    uint16_t state_schema_version;
    uint16_t arithmetic_version;
    uint16_t rng_version;
    uint16_t input_schema_version;
    int32_t arena_half_width_q16;
    int32_t arena_ceiling_q16;
    uint8_t player_count;
    uint8_t mode;
    uint8_t terminated;
    uint8_t truncated;
    uint8_t winner_mask;
    uint8_t reserved[3];
    uint64_t previous_buttons[PF_SIM_MAX_PLAYERS];
    int32_t position_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t position_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint8_t team[PF_SIM_MAX_PLAYERS];
    uint8_t grounded[PF_SIM_MAX_PLAYERS];
    uint8_t active[PF_SIM_MAX_PLAYERS];
} pf_world_state;

typedef struct pf_sim_scratch
{
    uint64_t previous_buttons[PF_SIM_MAX_PLAYERS];
    int32_t position_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t position_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint8_t grounded[PF_SIM_MAX_PLAYERS];
} pf_sim_scratch;

struct pf_sim
{
    uint64_t magic;
    pf_sim_scratch *scratch;
    uint8_t has_reset;
    uint8_t reserved[7];
    pf_world_state world;
};

pf_status pf_sim_validate_config(const pf_sim_config *config);
int pf_sim_is_valid(const pf_sim *sim);
uint64_t pf_sim_rng_next(uint64_t *state);
pf_status pf_sim_tick_impl(
    pf_sim *sim,
    const pf_input_frame *inputs,
    size_t player_count,
    pf_tick_result *out_result);
pf_status pf_sim_snapshot_validate_world(const pf_world_state *world);
void pf_sim_snapshot_config_hash(
    const pf_world_state *world,
    uint8_t digest[32]);

#endif
