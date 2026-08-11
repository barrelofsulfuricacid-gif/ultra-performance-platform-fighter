#ifndef PF_SIM_INTERNAL_H
#define PF_SIM_INTERNAL_H

#include "pf/m4.h"
#include "pf/sim.h"
#include "sim_fixed_math.h"
#include "sim_hsd_pose.h"

#include <stdint.h>

#define PF_SIM_HANDLE_MAGIC UINT64_C(0x504653494D303032)
#define PF_SIM_MAX_MOTION_SPEED_Q16 INT32_C(262144)

static inline int pf_sim_vertical_coordinate_is_within_arena(
    int32_t coordinate_q16,
    int32_t arena_extent_q16)
{
    return coordinate_q16 >= -arena_extent_q16 &&
           coordinate_q16 <= arena_extent_q16;
}

static inline int32_t pf_m4_total_velocity_q16(
    int32_t self_velocity_q16,
    int32_t knockback_velocity_q16)
{
    /* Both channels are independently bounded by
     * PF_SIM_MAX_MOTION_SPEED_Q16, so their sum is representable in i32. */
    return (int32_t)(
        (int64_t)self_velocity_q16 + (int64_t)knockback_velocity_q16);
}

#define PF_SIM_MAX_DAMAGE_Q16 (UINT32_C(999) * UINT32_C(65536))
#define PF_SIM_MAX_SHIELD_HEALTH_Q16 \
    (UINT32_C(100) * UINT32_C(65536))
#define PF_SIM_MAX_HITSTUN_TICKS UINT16_C(600)
#define PF_SIM_MAX_GRAB_ESCAPE_TICKS UINT16_C(2048)

#define PF_M4_TRIGGER_STATE_LEFT_HELD UINT8_C(1)
#define PF_M4_TRIGGER_STATE_RIGHT_HELD UINT8_C(2)
#define PF_M4_TRIGGER_STATE_LEFT_DENSE UINT8_C(4)
#define PF_M4_TRIGGER_STATE_RIGHT_DENSE UINT8_C(8)
#define PF_M4_TRIGGER_STATE_HELD_MASK UINT8_C(3)
#define PF_M4_TRIGGER_STATE_DENSE_MASK UINT8_C(12)
#define PF_M4_TRIGGER_STATE_MASK UINT8_C(15)

static inline int pf_m4_action_is_ground_damage(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_DAMAGE_LOW_1 ||
           action_state == (uint8_t)PF_M4_ACTION_DAMAGE_LOW_2 ||
           action_state == (uint8_t)PF_M4_ACTION_DAMAGE_LOW_3;
}

static inline int pf_m4_action_is_damage(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_HITSTUN ||
           pf_m4_action_is_ground_damage(action_state);
}

static inline int pf_m4_action_uses_ledge(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_LEDGE_CATCH ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_JUMP;
}

static inline int pf_m4_action_is_reference_jab_extension(
    uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_JAB_THIRD ||
           action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_START ||
           action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP ||
           action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_END;
}

static inline int pf_m4_action_is_reference_jab_chain(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL ||
           pf_m4_action_is_reference_jab_extension(action_state);
}

static inline int pf_m4_action_is_reference_angled_normal(
    uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_MID_LOW ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_LOW ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW;
}

static inline int pf_m4_action_is_forward_smash_release(uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW;
}

static inline int pf_m4_action_is_smash_release(uint8_t action_state)
{
    return pf_m4_action_is_forward_smash_release(action_state) ||
           action_state == (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
}

static inline int pf_m4_action_uses_direct_hsd_pose(uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
           action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
           action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW;
}

static inline int pf_m4_action_uses_fall_special_pose(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT ||
           action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL;
}

static inline int pf_m4_action_retains_source_submotion(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    const int action_owns_submotion =
        action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        action_state == (uint8_t)PF_M4_ACTION_WALK ||
        action_state == (uint8_t)PF_M4_ACTION_RUN ||
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
        action_state == (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
        action_state == (uint8_t)PF_M4_ACTION_TAUNT ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND ||
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
        action_state == (uint8_t)PF_M4_ACTION_TEETER ||
        action_state == (uint8_t)PF_M4_ACTION_REBOUND ||
        action_state == (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        action_state == (uint8_t)PF_M4_ACTION_PUMMEL ||
        action_state == (uint8_t)PF_M4_ACTION_GRABBED ||
        action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
        pf_m4_action_uses_ledge(action_state) ||
        pf_m4_action_uses_fall_special_pose(action_state) ||
        pf_m4_action_uses_direct_hsd_pose(action_state);
    const int resume_owns_submotion =
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_WALK ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_RUN ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_AIRBORNE ||
        hitlag_resume_action ==
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_STANDING_TURN ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_CROUCH ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_TAUNT ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_SHIELD ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        hitlag_resume_action ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
        hitlag_resume_action ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND ||
        hitlag_resume_action ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_TEETER ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_REBOUND ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_PUMMEL ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_GRABBED ||
        hitlag_resume_action == (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
        pf_m4_action_uses_ledge(hitlag_resume_action) ||
        pf_m4_action_uses_fall_special_pose(hitlag_resume_action) ||
        pf_m4_action_uses_direct_hsd_pose(hitlag_resume_action);

    return action_owns_submotion ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
           resume_owns_submotion);
}

static inline uint8_t pf_m4_effective_action_state(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return action_state == (uint8_t)PF_M4_ACTION_HITLAG
               ? hitlag_resume_action
               : action_state;
}

static inline int pf_m4_action_uses_velocity_animation_clock(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    const uint8_t effective_action = pf_m4_effective_action_state(
        action_state,
        hitlag_resume_action);

    return effective_action == (uint8_t)PF_M4_ACTION_WALK ||
           effective_action == (uint8_t)PF_M4_ACTION_RUN;
}

static inline int pf_m4_action_uses_ground_animation_clock(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return pf_m4_effective_action_state(
               action_state,
               hitlag_resume_action) ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           pf_m4_action_uses_velocity_animation_clock(
               action_state,
               hitlag_resume_action);
}

static inline int pf_m4_action_uses_source_animation_clock(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    const uint8_t effective_action = pf_m4_effective_action_state(
        action_state,
        hitlag_resume_action);

    return pf_m4_action_uses_ground_animation_clock(
               action_state,
               hitlag_resume_action) ||
           effective_action == (uint8_t)PF_M4_ACTION_CROUCH ||
           effective_action == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           effective_action ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
           pf_m4_action_uses_fall_special_pose(effective_action) ||
           pf_m4_action_uses_direct_hsd_pose(effective_action);
}

static inline int32_t pf_m4_multiply_q16(
    int32_t value_q16,
    int32_t multiplier_q16)
{
    return (int32_t)(
        ((int64_t)value_q16 * (int64_t)multiplier_q16) /
        (int64_t)PF_Q16_ONE);
}

static inline int32_t pf_m4_axis_q16(int16_t axis)
{
    const int64_t denominator =
        axis < INT16_C(0) ? INT64_C(32768) : INT64_C(32767);
    const int64_t product = (int64_t)axis * (int64_t)PF_Q16_ONE;

    return product < INT64_C(0)
               ? (int32_t)(
                     -((-product + denominator / INT64_C(2)) /
                       denominator))
               : (int32_t)(
                     (product + denominator / INT64_C(2)) /
                     denominator);
}

static inline uint8_t pf_m4_input_trigger_state(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input)
{
    uint8_t state = UINT8_C(0);

    if (input->left_trigger >= fighter->light_shield_trigger_threshold)
    {
        state |= PF_M4_TRIGGER_STATE_LEFT_HELD;
    }
    if (input->right_trigger >= fighter->light_shield_trigger_threshold)
    {
        state |= PF_M4_TRIGGER_STATE_RIGHT_HELD;
    }
    if (input->left_trigger >= fighter->digital_trigger_threshold)
    {
        state |= PF_M4_TRIGGER_STATE_LEFT_DENSE;
    }
    if (input->right_trigger >= fighter->digital_trigger_threshold)
    {
        state |= PF_M4_TRIGGER_STATE_RIGHT_DENSE;
    }
    return state;
}

enum
{
    PF_M4_DIRECTIONAL_INPUT_DODGE_DOWN = UINT8_C(1) << 0,
    PF_M4_DIRECTIONAL_INPUT_C_UP = UINT8_C(1) << 1,
    PF_M4_DIRECTIONAL_INPUT_C_LEFT = UINT8_C(1) << 2,
    PF_M4_DIRECTIONAL_INPUT_C_RIGHT = UINT8_C(1) << 3,
    PF_M4_DIRECTIONAL_INPUT_LEDGE_READY = UINT8_C(1) << 4,
    PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ATTACK = UINT8_C(1) << 5,
    PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ROLL_INWARD = UINT8_C(1) << 6,
    /* Compact damage-state work bit. It is not an input edge: combat sets it
     * only for the decomp's 260..280 degree meteor-cancellable launch range,
     * and movement preserves it until damage ends or the cancel succeeds. */
    PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL = UINT8_C(1) << 7,
    PF_M4_DIRECTIONAL_INPUT_ALL =
        PF_M4_DIRECTIONAL_INPUT_DODGE_DOWN |
        PF_M4_DIRECTIONAL_INPUT_C_UP |
        PF_M4_DIRECTIONAL_INPUT_C_LEFT |
        PF_M4_DIRECTIONAL_INPUT_C_RIGHT |
        PF_M4_DIRECTIONAL_INPUT_LEDGE_READY |
        PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ATTACK |
        PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ROLL_INWARD |
        PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL
};

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
    uint16_t respawn_delay_config_ticks;
    uint16_t respawn_invulnerability_config_ticks;
    uint8_t player_count;
    uint8_t mode;
    uint8_t stock_count;
    uint8_t sudden_death;
    uint8_t terminated;
    uint8_t truncated;
    uint8_t winner_mask;
    uint8_t shield_recoil_mask;
    uint64_t previous_buttons[PF_SIM_MAX_PLAYERS];
    int32_t position_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t position_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint16_t match_kos[PF_SIM_MAX_PLAYERS];
    uint16_t match_falls[PF_SIM_MAX_PLAYERS];
    uint16_t action_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t source_submotion[PF_SIM_MAX_PLAYERS];
    int32_t source_animation_frame_q16[PF_SIM_MAX_PLAYERS];
    int32_t source_animation_rate_q16[PF_SIM_MAX_PLAYERS];
    int32_t fall_animation_blend_q16[PF_SIM_MAX_PLAYERS];
    uint8_t fall_animation_target_switched[PF_SIM_MAX_PLAYERS];
    int32_t ecb_locked_bottom_y_q16[PF_SIM_MAX_PLAYERS];
    pf_m4_hsd_compact_pose ground_blend_pose[PF_SIM_MAX_PLAYERS];
    int32_t ground_blend_progress_q16[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_count[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_invulnerability_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t ledge_invulnerability_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t ledge_regrab_lockout_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t grab_escape_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t damage_jump_buffer_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t charge_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t smash_charge_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t shield_strength[PF_SIM_MAX_PLAYERS];
    uint16_t shield_angle_turn[PF_SIM_MAX_PLAYERS];
    uint16_t shield_magnitude[PF_SIM_MAX_PLAYERS];
    uint8_t team[PF_SIM_MAX_PLAYERS];
    uint8_t grounded[PF_SIM_MAX_PLAYERS];
    uint8_t active[PF_SIM_MAX_PLAYERS];
    uint8_t stocks_remaining[PF_SIM_MAX_PLAYERS];
    uint8_t grab_target_slot[PF_SIM_MAX_PLAYERS];
    uint8_t grab_owner_slot[PF_SIM_MAX_PLAYERS];
    uint8_t action_state[PF_SIM_MAX_PLAYERS];
    uint8_t support[PF_SIM_MAX_PLAYERS];
    uint8_t air_jumps_remaining[PF_SIM_MAX_PLAYERS];
    uint8_t recovery_available[PF_SIM_MAX_PLAYERS];
    uint8_t short_hop_latched[PF_SIM_MAX_PLAYERS];
    uint8_t platform_drop_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t ecb_bottom_lock_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t fast_fall[PF_SIM_MAX_PLAYERS];
    int8_t facing[PF_SIM_MAX_PLAYERS];
    int8_t dash_direction[PF_SIM_MAX_PLAYERS];
    int8_t previous_strong_direction[PF_SIM_MAX_PLAYERS];
    uint8_t previous_directional_input_flags[PF_SIM_MAX_PLAYERS];
    int8_t previous_tilt_x_direction[PF_SIM_MAX_PLAYERS];
    int8_t previous_tilt_y_direction[PF_SIM_MAX_PLAYERS];
    int8_t mash_stick_x_direction[PF_SIM_MAX_PLAYERS];
    int8_t mash_stick_y_direction[PF_SIM_MAX_PLAYERS];
    int16_t previous_secondary_stick_x[PF_SIM_MAX_PLAYERS];
    int16_t previous_secondary_stick_y[PF_SIM_MAX_PLAYERS];
    uint8_t tilt_x_age[PF_SIM_MAX_PLAYERS];
    uint8_t tilt_y_age[PF_SIM_MAX_PLAYERS];
    uint8_t horizontal_input_age[PF_SIM_MAX_PLAYERS];
    int8_t horizontal_input_direction[PF_SIM_MAX_PLAYERS];
    uint32_t damage_q16[PF_SIM_MAX_PLAYERS];
    int32_t knockback_velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t knockback_velocity_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t ground_knockback_velocity_q16[PF_SIM_MAX_PLAYERS];
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
    uint8_t attack_stale_registered[PF_SIM_MAX_PLAYERS];
    uint8_t falcon_kick_hit_count[PF_SIM_MAX_PLAYERS];
    uint16_t rebound_duration_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t jab_chain_buffered[PF_SIM_MAX_PLAYERS];
    uint8_t rapid_jab_input_count[PF_SIM_MAX_PLAYERS];
    uint8_t rapid_jab_continue[PF_SIM_MAX_PLAYERS];
    uint8_t down_tilt_repeat_buffered[PF_SIM_MAX_PLAYERS];
    uint8_t stale_move_count[PF_SIM_MAX_PLAYERS];
    uint8_t stale_move_ids[PF_SIM_MAX_PLAYERS]
                          [PF_SIM_STALE_MOVE_QUEUE_CAPACITY];
    uint8_t last_hit_attacker[PF_SIM_MAX_PLAYERS];
    uint8_t shield_held[PF_SIM_MAX_PLAYERS];
    uint8_t trigger_input_age[PF_SIM_MAX_PLAYERS];
    uint8_t prone_attack_input_age[PF_SIM_MAX_PLAYERS];
    uint8_t powershield[PF_SIM_MAX_PLAYERS];
    uint8_t tumble[PF_SIM_MAX_PLAYERS];
    uint8_t sdi_pulse_count[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_x[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_y[PF_SIM_MAX_PLAYERS];
    int8_t tech_direction[PF_SIM_MAX_PLAYERS];
    uint8_t prone_orientation[PF_SIM_MAX_PLAYERS];
    uint8_t prone_roll_motion_orientation[PF_SIM_MAX_PLAYERS];
    int32_t item_position_x_q16;
    int32_t item_position_y_q16;
    int32_t item_velocity_x_q16;
    int32_t item_velocity_y_q16;
    uint16_t item_lifetime_ticks;
    uint16_t item_respawn_ticks;
    uint16_t item_pickup_lockout_ticks;
    uint8_t item_state;
    uint8_t item_holder_slot;
    uint8_t item_source_slot;
    uint8_t item_hit_mask;
    uint8_t item_stale_registered;
    uint8_t item_throw_direction;
    int32_t projectile_position_x_q16;
    int32_t projectile_position_y_q16;
    int32_t projectile_velocity_x_q16;
    int32_t projectile_velocity_y_q16;
    uint16_t projectile_lifetime_ticks;
    uint8_t projectile_state;
    uint8_t projectile_owner_slot;
    uint32_t combat_event_sequence;
    int32_t shield_recoil_x_q16[PF_SIM_MAX_PLAYERS];
} pf_world_state;

typedef struct pf_sim_scratch
{
    uint64_t previous_buttons[PF_SIM_MAX_PLAYERS];
    int32_t position_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t position_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t velocity_y_q16[PF_SIM_MAX_PLAYERS];
    uint16_t match_kos[PF_SIM_MAX_PLAYERS];
    uint16_t match_falls[PF_SIM_MAX_PLAYERS];
    uint16_t action_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t source_submotion[PF_SIM_MAX_PLAYERS];
    int32_t source_animation_frame_q16[PF_SIM_MAX_PLAYERS];
    int32_t source_animation_rate_q16[PF_SIM_MAX_PLAYERS];
    int32_t fall_animation_blend_q16[PF_SIM_MAX_PLAYERS];
    uint8_t fall_animation_target_switched[PF_SIM_MAX_PLAYERS];
    int32_t ecb_locked_bottom_y_q16[PF_SIM_MAX_PLAYERS];
    pf_m4_hsd_compact_pose ground_blend_pose[PF_SIM_MAX_PLAYERS];
    int32_t ground_blend_progress_q16[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_count[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t respawn_invulnerability_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t ledge_invulnerability_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t ledge_regrab_lockout_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t grab_escape_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t damage_jump_buffer_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t charge_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t smash_charge_ticks[PF_SIM_MAX_PLAYERS];
    uint16_t shield_strength[PF_SIM_MAX_PLAYERS];
    uint16_t shield_angle_turn[PF_SIM_MAX_PLAYERS];
    uint16_t shield_magnitude[PF_SIM_MAX_PLAYERS];
    uint8_t grounded[PF_SIM_MAX_PLAYERS];
    uint8_t active[PF_SIM_MAX_PLAYERS];
    uint8_t stocks_remaining[PF_SIM_MAX_PLAYERS];
    uint8_t grab_target_slot[PF_SIM_MAX_PLAYERS];
    uint8_t grab_owner_slot[PF_SIM_MAX_PLAYERS];
    uint8_t action_state[PF_SIM_MAX_PLAYERS];
    uint8_t support[PF_SIM_MAX_PLAYERS];
    uint8_t air_jumps_remaining[PF_SIM_MAX_PLAYERS];
    uint8_t recovery_available[PF_SIM_MAX_PLAYERS];
    uint8_t short_hop_latched[PF_SIM_MAX_PLAYERS];
    uint8_t platform_drop_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t ecb_bottom_lock_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t fast_fall[PF_SIM_MAX_PLAYERS];
    int8_t facing[PF_SIM_MAX_PLAYERS];
    int8_t dash_direction[PF_SIM_MAX_PLAYERS];
    int8_t previous_strong_direction[PF_SIM_MAX_PLAYERS];
    uint8_t previous_directional_input_flags[PF_SIM_MAX_PLAYERS];
    int8_t previous_tilt_x_direction[PF_SIM_MAX_PLAYERS];
    int8_t previous_tilt_y_direction[PF_SIM_MAX_PLAYERS];
    int8_t mash_stick_x_direction[PF_SIM_MAX_PLAYERS];
    int8_t mash_stick_y_direction[PF_SIM_MAX_PLAYERS];
    int16_t previous_secondary_stick_x[PF_SIM_MAX_PLAYERS];
    int16_t previous_secondary_stick_y[PF_SIM_MAX_PLAYERS];
    uint8_t tilt_x_age[PF_SIM_MAX_PLAYERS];
    uint8_t tilt_y_age[PF_SIM_MAX_PLAYERS];
    uint8_t horizontal_input_age[PF_SIM_MAX_PLAYERS];
    int8_t horizontal_input_direction[PF_SIM_MAX_PLAYERS];
    uint32_t damage_q16[PF_SIM_MAX_PLAYERS];
    int32_t knockback_velocity_x_q16[PF_SIM_MAX_PLAYERS];
    int32_t knockback_velocity_y_q16[PF_SIM_MAX_PLAYERS];
    int32_t ground_knockback_velocity_q16[PF_SIM_MAX_PLAYERS];
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
    uint8_t attack_stale_registered[PF_SIM_MAX_PLAYERS];
    uint8_t falcon_kick_hit_count[PF_SIM_MAX_PLAYERS];
    uint16_t rebound_duration_ticks[PF_SIM_MAX_PLAYERS];
    uint8_t jab_chain_buffered[PF_SIM_MAX_PLAYERS];
    uint8_t rapid_jab_input_count[PF_SIM_MAX_PLAYERS];
    uint8_t rapid_jab_continue[PF_SIM_MAX_PLAYERS];
    uint8_t down_tilt_repeat_buffered[PF_SIM_MAX_PLAYERS];
    uint8_t stale_move_count[PF_SIM_MAX_PLAYERS];
    uint8_t stale_move_ids[PF_SIM_MAX_PLAYERS]
                          [PF_SIM_STALE_MOVE_QUEUE_CAPACITY];
    uint8_t last_hit_attacker[PF_SIM_MAX_PLAYERS];
    uint8_t shield_held[PF_SIM_MAX_PLAYERS];
    uint8_t trigger_input_age[PF_SIM_MAX_PLAYERS];
    uint8_t prone_attack_input_age[PF_SIM_MAX_PLAYERS];
    uint8_t powershield[PF_SIM_MAX_PLAYERS];
    uint8_t tumble[PF_SIM_MAX_PLAYERS];
    uint8_t sdi_pulse_count[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_x[PF_SIM_MAX_PLAYERS];
    int8_t sdi_direction_y[PF_SIM_MAX_PLAYERS];
    int8_t tech_direction[PF_SIM_MAX_PLAYERS];
    uint8_t prone_orientation[PF_SIM_MAX_PLAYERS];
    uint8_t prone_roll_motion_orientation[PF_SIM_MAX_PLAYERS];
    int32_t item_position_x_q16;
    int32_t item_position_y_q16;
    int32_t item_velocity_x_q16;
    int32_t item_velocity_y_q16;
    uint16_t item_lifetime_ticks;
    uint16_t item_respawn_ticks;
    uint16_t item_pickup_lockout_ticks;
    uint8_t item_state;
    uint8_t item_holder_slot;
    uint8_t item_source_slot;
    uint8_t item_hit_mask;
    uint8_t item_stale_registered;
    uint8_t item_throw_direction;
    int32_t projectile_position_x_q16;
    int32_t projectile_position_y_q16;
    int32_t projectile_velocity_x_q16;
    int32_t projectile_velocity_y_q16;
    uint16_t projectile_lifetime_ticks;
    uint8_t projectile_state;
    uint8_t projectile_owner_slot;
    uint32_t combat_event_sequence;
    uint8_t combat_event_count;
    uint8_t stale_move_sync_valid;
    uint8_t stale_move_dirty_mask;
    uint8_t action_transition_mask;
    uint8_t shield_recoil_mask;
    pf_sim_event combat_events[PF_SIM_MAX_EVENTS_PER_TICK];
    int32_t shield_recoil_x_q16[PF_SIM_MAX_PLAYERS];
} pf_sim_scratch;

static inline void pf_m4_track_action_transition(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const uint8_t player_mask =
        (uint8_t)(UINT32_C(1) << player_index);

    if (scratch->action_state[player_index] !=
        world->action_state[player_index])
    {
        scratch->action_transition_mask =
            (uint8_t)(scratch->action_transition_mask | player_mask);
    }
    else
    {
        scratch->action_transition_mask =
            (uint8_t)(
                scratch->action_transition_mask &
                (uint8_t)(~player_mask));
    }
}

static inline void pf_m4_set_action_state(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t action_state)
{
    scratch->action_state[player_index] = action_state;
    pf_m4_track_action_transition(world, scratch, player_index);
}

struct pf_sim
{
    uint64_t magic;
    pf_sim_scratch *scratch;
    uint8_t has_reset;
    uint8_t reserved[7];
    pf_m4_content content;
    pf_world_state world;
};

typedef enum pf_m4_item_input_intent
{
    PF_M4_ITEM_INPUT_NONE = 0,
    PF_M4_ITEM_INPUT_PICKUP = 1,
    PF_M4_ITEM_INPUT_DROP = 2,
    PF_M4_ITEM_INPUT_THROW = 3,
    PF_M4_ITEM_INPUT_GLIDE_TOSS = 4,
    PF_M4_ITEM_INPUT_JUMP_CANCEL_THROW = 5,
    PF_M4_ITEM_INPUT_DASH_THROW = 6
} pf_m4_item_input_intent;

typedef enum pf_m4_projectile_input_intent
{
    PF_M4_PROJECTILE_INPUT_NONE = 0,
    PF_M4_PROJECTILE_INPUT_FIRE = 1
} pf_m4_projectile_input_intent;

void pf_m4_prepare_reflector_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input);
void pf_m4_prepare_charge_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input);

pf_status pf_sim_validate_config(const pf_sim_config *config);
int pf_sim_is_valid(const pf_sim *sim);
uint64_t pf_sim_rng_next(uint64_t *state);
pf_status pf_sim_push_event(
    pf_sim_scratch *scratch,
    uint64_t tick,
    pf_sim_event_type type,
    uint8_t source_player,
    uint8_t target_player,
    uint32_t value_q16,
    int32_t velocity_x_q16,
    int32_t velocity_y_q16,
    uint16_t flags,
    uint16_t detail,
    uint32_t *out_sequence);
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
void pf_m4_reset_item(pf_sim *sim);
void pf_m4_reset_projectile(pf_sim *sim);
void pf_m4_begin_item_tick(
    const pf_world_state *world,
    pf_sim_scratch *scratch);
pf_m4_item_input_intent pf_m4_prepare_item_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input);
pf_status pf_m4_apply_item_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_m4_item_input_intent intent);
pf_status pf_m4_step_item(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch);
void pf_m4_begin_projectile_tick(
    const pf_world_state *world,
    pf_sim_scratch *scratch);
pf_m4_projectile_input_intent pf_m4_prepare_projectile_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    pf_input_frame *effective_input);
pf_status pf_m4_apply_projectile_input(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    pf_m4_projectile_input_intent intent);
pf_status pf_m4_step_projectile(
    const pf_m4_content *content,
    pf_sim_scratch *scratch);
pf_status pf_m4_step_player(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    const pf_input_frame *raw_input,
    uint32_t player_index,
    int32_t player_nudge_x_q16);
pf_status pf_m4_resolve_combat(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch);
uint8_t pf_m4_stale_move_id_for_action(uint8_t action_state);
uint32_t pf_m4_stale_move_multiplier_q16(
    const pf_m4_fighter_data *fighter,
    const uint8_t stale_move_ids[PF_SIM_STALE_MOVE_QUEUE_CAPACITY],
    uint8_t stale_move_count,
    uint8_t move_id);
const pf_m4_getup_roll_timing *pf_m4_getup_roll_timing_for(
    const pf_m4_fighter_data *fighter,
    uint8_t prone_orientation,
    int8_t roll_direction,
    int8_t facing);
uint16_t pf_m4_getup_roll_submotion_for(
    uint8_t prone_orientation,
    int8_t roll_direction,
    int8_t facing);
uint16_t pf_m4_getup_attack_invulnerability_ticks_for(
    const pf_m4_fighter_data *fighter,
    uint8_t prone_orientation);
int pf_m4_attack_hitbox(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t source_submotion,
    int32_t *out_left_q16,
    int32_t *out_right_q16,
    int32_t *out_top_q16,
    int32_t *out_bottom_q16);
int pf_m4_grabbox(
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
uint8_t pf_m4_attack_hit_spheres(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    pf_m4_hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY]);
uint8_t pf_m4_reference_world_hurt_capsules(
    const pf_m4_fighter_data *fighter,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    int8_t dash_direction,
    uint8_t grounded,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    uint16_t action_ticks,
    const pf_m4_hsd_local_pose *ground_loop_pose,
    pf_m4_hurt_capsule_inspection
        out_capsules[PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY]);
int pf_m4_shield_box(
    const pf_m4_fighter_data *fighter,
    int32_t position_x_q16,
    int32_t position_y_q16,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint32_t shield_health_q16,
    uint16_t shield_strength,
    int8_t facing,
    uint16_t shield_angle_turn,
    uint16_t shield_magnitude,
    int32_t *out_left_q16,
    int32_t *out_right_q16,
    int32_t *out_top_q16,
    int32_t *out_bottom_q16);
void pf_m4_shield_tilt_axes(
    uint16_t angle_turn,
    uint16_t magnitude,
    int8_t facing,
    int16_t *out_x,
    int16_t *out_y);
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
