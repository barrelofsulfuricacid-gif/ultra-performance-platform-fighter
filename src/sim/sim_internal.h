#ifndef PF_SIM_INTERNAL_H
#define PF_SIM_INTERNAL_H

#include "pf/m4.h"
#include "pf/sim.h"

#include <stdint.h>

#define PF_SIM_HANDLE_MAGIC UINT64_C(0x504653494D303032)
#define PF_SIM_MAX_MOTION_SPEED_Q16 INT32_C(262144)
#define PF_SIM_MAX_DAMAGE_Q16 (UINT32_C(999) * UINT32_C(65536))
#define PF_SIM_MAX_SHIELD_HEALTH_Q16 \
    (UINT32_C(100) * UINT32_C(65536))
#define PF_SIM_MAX_HITSTUN_TICKS UINT16_C(600)

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
    uint16_t action_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_count[PF_SIM_MAX_PLAYERS];
    uint8_t team[PF_SIM_MAX_PLAYERS];
    uint8_t grounded[PF_SIM_MAX_PLAYERS];
    uint8_t active[PF_SIM_MAX_PLAYERS];
    uint8_t action_state[PF_SIM_MAX_PLAYERS];
    uint8_t support[PF_SIM_MAX_PLAYERS];
    uint8_t air_jumps_remaining[PF_SIM_MAX_PLAYERS];
    uint8_t short_hop_latched[PF_SIM_MAX_PLAYERS];
    uint8_t platform_drop_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t fast_fall[PF_SIM_MAX_PLAYERS];
    int8_t facing[PF_SIM_MAX_PLAYERS];
    int8_t dash_direction[PF_SIM_MAX_PLAYERS];
    int8_t previous_strong_direction[PF_SIM_MAX_PLAYERS];
    uint32_t damage_q16[PF_SIM_MAX_PLAYERS];
    int32_t pending_velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t pending_velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint32_t last_hit_sequence[PF_SIM_MAX_PLAYERS];
    uint64_t last_hit_tick[PF_SIM_MAX_PLAYERS];
    uint32_t last_hit_damage_q16[PF_SIM_MAX_PLAYERS];
    uint16_t hitlag_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t hitstun_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t tech_window_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t tech_lockout_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t shield_stun_ticks[PF_SIM_MAX_PLAYERS];
    uint32_t shield_health_q16[PF_SIM_MAX_PLAYERS];
    uint8_t hitlag_resume_action[PF_SIM_MAX_PLAYERS];
    uint8_t attack_hit_mask[PF_SIM_MAX_PLAYERS];
    uint8_t last_hit_attacker[PF_SIM_MAX_PLAYERS];
    uint8_t shield_held[PF_SIM_MAX_PLAYERS];
    uint8_t powershield[PF_SIM_MAX_PLAYERS];
    uint8_t tumble[PF_SIM_MAX_PLAYERS];
    uint8_t sdi_pulse_count[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_x[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_y[PF_SIM_MAX_PLAYERS];
    int8_t tech_direction[PF_SIM_MAX_PLAYERS];
    uint32_t combat_event_sequence;
} pf_world_state;

typedef struct pf_sim_scratch
{
    uint64_t previous_buttons[PF_SIM_MAX_PLAYERS];
    int32_t position_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t position_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint16_t action_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_count[PF_SIM_MAX_PLAYERS];
    uint8_t grounded[PF_SIM_MAX_PLAYERS];
    uint8_t action_state[PF_SIM_MAX_PLAYERS];
    uint8_t support[PF_SIM_MAX_PLAYERS];
    uint8_t air_jumps_remaining[PF_SIM_MAX_PLAYERS];
    uint8_t short_hop_latched[PF_SIM_MAX_PLAYERS];
    uint8_t platform_drop_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t fast_fall[PF_SIM_MAX_PLAYERS];
    int8_t facing[PF_SIM_MAX_PLAYERS];
    int8_t dash_direction[PF_SIM_MAX_PLAYERS];
    int8_t previous_strong_direction[PF_SIM_MAX_PLAYERS];
    uint32_t damage_q16[PF_SIM_MAX_PLAYERS];
    int32_t pending_velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t pending_velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint32_t last_hit_sequence[PF_SIM_MAX_PLAYERS];
    uint64_t last_hit_tick[PF_SIM_MAX_PLAYERS];
    uint32_t last_hit_damage_q16[PF_SIM_MAX_PLAYERS];
    uint16_t hitlag_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t hitstun_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t tech_window_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t tech_lockout_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t shield_stun_ticks[PF_SIM_MAX_PLAYERS];
    uint32_t shield_health_q16[PF_SIM_MAX_PLAYERS];
    uint8_t hitlag_resume_action[PF_SIM_MAX_PLAYERS];
    uint8_t attack_hit_mask[PF_SIM_MAX_PLAYERS];
    uint8_t last_hit_attacker[PF_SIM_MAX_PLAYERS];
    uint8_t shield_held[PF_SIM_MAX_PLAYERS];
    uint8_t powershield[PF_SIM_MAX_PLAYERS];
    uint8_t tumble[PF_SIM_MAX_PLAYERS];
    uint8_t sdi_pulse_count[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_x[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_y[PF_SIM_MAX_PLAYERS];
    int8_t tech_direction[PF_SIM_MAX_PLAYERS];
    uint32_t combat_event_sequence;
} pf_sim_scratch;

struct pf_sim
{
    uint64_t magic;
    pf_sim_scratch *scratch;
    uint8_t has_reset;
    uint8_t reserved[7];
    pf_m4_content content;
    pf_world_state world;
};

pf_status pf_sim_validate_config(const pf_sim_config *config);
int pf_sim_is_valid(const pf_sim *sim);
uint64_t pf_sim_rng_next(uint64_t *state);
pf_status pf_m4_content_from_view(
    const pf_content_view *view,
    pf_m4_content *out_content);
int32_t pf_m4_platform_center_x_q16(
    const pf_m4_stage_data *stage,
    uint64_t tick);
void pf_m4_reset_player(
    pf_sim *sim,
    uint32_t player_index,
    int count_respawn);
pf_status pf_m4_step_player(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index);
pf_status pf_m4_resolve_combat(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch);
int pf_m4_attack_hitbox(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    int32_t *out_left_q16,
    int32_t *out_right_q16,
    int32_t *out_top_q16,
    int32_t *out_bottom_q16);
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
