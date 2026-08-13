#include "sim_internal.h"

#include "sim_collision.h"
#include "sim_falcon_frame_data.h"
#include "sim_melee.h"
#include "sim_ssbm_common_data.h"
#include "sim_ssbm_damage.h"
#include "sim_ssbm_stage_data.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static inline void reset_ordinary_tilt_x_age(
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    scratch->tilt_x_age[player_index] = UINT8_C(254);
}

static inline void reset_ordinary_tilt_ages(
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    reset_ordinary_tilt_x_age(scratch, player_index);
    scratch->tilt_y_age[player_index] = UINT8_C(254);
}

static float lerp_f32_steps(
    float start,
    float end,
    uint16_t step,
    uint16_t step_count)
{
    return start + (end - start) * (float)step / (float)step_count;
}

static pf_status reference_throw_release_sweep(
    const struct content *content,
    const pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index,
    float direct_x_f32,
    float direct_y_f32,
    float *out_x_f32,
    float *out_y_f32)
{
    falcon_ecb_pose_f32 holder_ecb;
    falcon_ecb_pose_f32 current_ecb;
    falcon_ecb_pose_f32 desired_ecb;
    float holder_mid_y_f32;
    float start_x_f32;
    float start_y_f32;
    float prior_x_f32;
    float prior_y_f32;
    uint16_t step_count;
    uint32_t step;

    if (content == NULL || scratch == NULL || out_x_f32 == NULL ||
        out_y_f32 == NULL || holder_index >= PF_SIM_MAX_PLAYERS ||
        target_index >= PF_SIM_MAX_PLAYERS ||
        content->stage.reference_collision_profile ==
            (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED ||
        !falcon_reference_hsd_ecb_pose(
            scratch->source_submotion[holder_index],
            scratch->source_animation_frame_f32[holder_index],
            scratch->grounded[holder_index] != UINT8_C(0),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
            &holder_ecb) ||
        !falcon_reference_hsd_ecb_pose(
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_f32[target_index],
            scratch->grounded[target_index] != UINT8_C(0),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
            &current_ecb))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    desired_ecb = current_ecb;
    holder_mid_y_f32 =
        (holder_ecb.top_y_from_origin_f32 +
         holder_ecb.bottom_y_from_origin_f32) * 0.5f;
    start_x_f32 = scratch->position_x_f32[holder_index];
    start_y_f32 = scratch->position_y_f32[holder_index] - holder_mid_y_f32;
    if (!falcon_reference_collision_sweep_step_count_f32(
            direct_x_f32 - start_x_f32,
            direct_y_f32 - start_y_f32,
            &current_ecb,
            &desired_ecb,
            &step_count) ||
        step_count == UINT16_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    prior_x_f32 = start_x_f32;
    prior_y_f32 =
        start_y_f32 + content->fighter.half_height_f32 -
        current_ecb.bottom_y_from_origin_f32;
    *out_x_f32 = direct_x_f32;
    *out_y_f32 = direct_y_f32;
    for (step = UINT32_C(1); step <= (uint32_t)step_count; ++step)
    {
        const float body_x_f32 = lerp_f32_steps(
            start_x_f32,
            direct_x_f32,
            (uint16_t)step,
            step_count);
        const float body_y_f32 = lerp_f32_steps(
            start_y_f32,
            direct_y_f32,
            (uint16_t)step,
            step_count);
        const float bottom_x_f32 = lerp_f32_steps(
            current_ecb.bottom_x_from_origin_f32,
            desired_ecb.bottom_x_from_origin_f32,
            (uint16_t)step,
            step_count);
        const float bottom_y_f32 = lerp_f32_steps(
            current_ecb.bottom_y_from_origin_f32,
            desired_ecb.bottom_y_from_origin_f32,
            (uint16_t)step,
            step_count);
        const float point_x_f32 =
            body_x_f32 + (float)scratch->facing[target_index] * bottom_x_f32;
        const float point_y_f32 =
            body_y_f32 + content->fighter.half_height_f32 -
            bottom_y_f32;
        float fraction_f32;
        float floor_y_f32;
        uint8_t support;

        if (!isfinite(point_x_f32) || !isfinite(point_y_f32))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (ssbm_reference_stage_find_floor_point_contact(
                content->stage.reference_collision_profile,
                prior_x_f32,
                prior_y_f32,
                point_x_f32,
                point_y_f32,
                &fraction_f32,
                &floor_y_f32,
                &support))
        {
            const float snapped_y_f32 =
                floor_y_f32 - content->fighter.half_height_f32 +
                bottom_y_f32;
            (void)fraction_f32;
            (void)support;
            if (!isfinite(snapped_y_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *out_x_f32 = body_x_f32;
            *out_y_f32 = snapped_y_f32;
            return PF_STATUS_OK;
        }
        prior_x_f32 = point_x_f32;
        prior_y_f32 = point_y_f32;
    }
    return PF_STATUS_OK;
}

static pf_status reference_zero_hitlag_throw_damage_step(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t target_index)
{
    float next_position_x_f32;
    float next_position_y_f32;

    if (content == NULL || world == NULL || scratch == NULL ||
        target_index >= PF_SIM_MAX_PLAYERS)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    /* Throw's release flag is consumed by Throw_Anim before Fighter_procUpdate.
     * ftCo_800DDDE4 therefore enters Damage early enough for the new DamageFly
     * physics callback, the global x8c decay, and position integration to all
     * execute on the release update. Ordinary attack hits are resolved later
     * by Fighter_ProcessHit and deliberately do not take this extra step. */
    scratch->grounded[target_index] = UINT8_C(0);
    scratch->support[target_index] = (uint8_t)PF_M4_SURFACE_NONE;
    scratch->ground_knockback_velocity_f32[target_index] = 0.0f;
    scratch->hitlag_ticks[target_index] = UINT16_C(0);
    scratch->hitlag_resume_action[target_index] = UINT8_C(0);
    set_action_state(
        world,
        scratch,
        target_index,
        (uint8_t)PF_M4_ACTION_HITSTUN);

    /* The release flag is consumed from the thrower's priority-0 Anim
     * callback.  A later-created victim has not run its own priority-0
     * callback yet, so the newly installed DamageFly Anim callback consumes
     * one frame of mv.co.damage.x0 on this same update.  This decrement is
     * distinct from the later physics/global-knockback step below. */
    if (scratch->hitstun_ticks[target_index] > UINT16_C(0))
    {
        --scratch->hitstun_ticks[target_index];
    }

    /* Throw_Anim runs before the priority-3 controller refresh, so Damage's
     * entry-angle calculation consumes the previously processed main stick.
     * This is observably different from using the release pre-frame sample. */
    if (ssbm_apply_di_f32(
            content->fighter.di_max_angle_radians_q30,
            world->previous_main_stick_x[target_index],
            world->previous_main_stick_y[target_index],
            &scratch->knockback_velocity_x_f32[target_index],
            &scratch->knockback_velocity_y_f32[target_index]) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    /* ftCo_8008DCE0 cleared self velocity. DamageFly's locked-physics branch
     * applies gravity plus aerial friction (a no-op from zero X), then the
     * common fighter update shortens the independent knockback vector. */
    scratch->velocity_x_f32[target_index] = 0.0f;
    scratch->velocity_y_f32[target_index] = content->fighter.gravity_f32;
    if (scratch->velocity_y_f32[target_index] >
        content->fighter.fall_speed_f32)
    {
        scratch->velocity_y_f32[target_index] =
            content->fighter.fall_speed_f32;
    }
    if (ssbm_decay_air_knockback_f32(
            content->fighter.air_knockback_decay_f32,
            &scratch->knockback_velocity_x_f32[target_index],
            &scratch->knockback_velocity_y_f32[target_index]) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    next_position_x_f32 =
        scratch->position_x_f32[target_index] +
        scratch->velocity_x_f32[target_index] +
        scratch->knockback_velocity_x_f32[target_index];
    next_position_y_f32 =
        scratch->position_y_f32[target_index] +
        scratch->velocity_y_f32[target_index] +
        scratch->knockback_velocity_y_f32[target_index];
    if (!isfinite(next_position_x_f32) || !isfinite(next_position_y_f32))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->position_x_f32[target_index] = next_position_x_f32;
    scratch->position_y_f32[target_index] = next_position_y_f32;
    return PF_STATUS_OK;
}

static float saturating_damage(float current, float added)
{
    if (current >= PF_SIM_MAX_DAMAGE_F32 ||
        added >= PF_SIM_MAX_DAMAGE_F32 - current)
    {
        return PF_SIM_MAX_DAMAGE_F32;
    }
    return current + added;
}

uint8_t stale_move_id_for_action(uint8_t action_state)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
        case PF_M4_ACTION_STRONG_ATTACK:
        case PF_M4_ACTION_AERIAL_ATTACK:
        case PF_M4_ACTION_STRONG_AERIAL_ATTACK:
        case PF_M4_ACTION_GETUP_ATTACK:
        case PF_M4_ACTION_THROW_FORWARD:
        case PF_M4_ACTION_THROW_BACK:
        case PF_M4_ACTION_THROW_UP:
        case PF_M4_ACTION_THROW_DOWN:
        case PF_M4_ACTION_DASH_ATTACK:
        case PF_M4_ACTION_JAB_FINAL:
        case PF_M4_ACTION_JAB_THIRD:
        case PF_M4_ACTION_PUMMEL:
        case PF_M4_ACTION_UP_ATTACK:
        case PF_M4_ACTION_DOWN_ATTACK:
        case PF_M4_ACTION_FORWARD_AERIAL:
        case PF_M4_ACTION_BACK_AERIAL:
        case PF_M4_ACTION_UP_AERIAL:
        case PF_M4_ACTION_DOWN_AERIAL:
        case PF_M4_ACTION_LEDGE_ATTACK:
        case PF_M4_ACTION_FORWARD_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        case PF_M4_ACTION_UP_STRONG_ATTACK:
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
        case PF_M4_ACTION_CHARGE_RELEASE_GROUND:
            return action_state;
        case PF_M4_ACTION_FORWARD_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_LOW:
        case PF_M4_ACTION_FORWARD_ATTACK_LOW:
            return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK;
        case PF_M4_ACTION_RAPID_JAB_START:
        case PF_M4_ACTION_RAPID_JAB_LOOP:
        case PF_M4_ACTION_RAPID_JAB_END:
            return (uint8_t)PF_M4_ACTION_RAPID_JAB_START;
        case PF_M4_ACTION_ITEM_THROW:
        case PF_M4_ACTION_ITEM_DASH_THROW:
            return (uint8_t)PF_M4_ACTION_ITEM_THROW;
        case PF_M4_ACTION_PROJECTILE_FIRE_GROUND:
        case PF_M4_ACTION_PROJECTILE_FIRE_AIR:
            return (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND;
        case PF_M4_ACTION_REFLECTOR_GROUND:
        case PF_M4_ACTION_REFLECTOR_AIR:
            return (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND;
        case PF_M4_ACTION_FALCON_PUNCH_GROUND:
        case PF_M4_ACTION_FALCON_PUNCH_AIR:
            return (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND;
        case PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND:
        case PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR:
            return (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND;
        case PF_M4_ACTION_FALCON_DIVE_CATCH:
        case PF_M4_ACTION_FALCON_DIVE_THROW:
            return (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH;
        case PF_M4_ACTION_FALCON_KICK_START_GROUND:
        case PF_M4_ACTION_FALCON_KICK_START_AIR:
        case PF_M4_ACTION_FALCON_KICK_LANDING:
            return (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND;
        default:
            return UINT8_C(0);
    }
}

float stale_move_multiplier_f32(
    const fighter_data *fighter,
    const uint8_t stale_move_ids[PF_SIM_STALE_MOVE_QUEUE_CAPACITY],
    uint8_t stale_move_count,
    uint8_t move_id)
{
    float reduction_f32 = 0.0f;
    uint32_t stale_index;

    if (fighter == NULL || stale_move_ids == NULL ||
        stale_move_count > PF_SIM_STALE_MOVE_QUEUE_CAPACITY ||
        move_id == UINT8_C(0))
    {
        return 1.0f;
    }
    for (stale_index = UINT32_C(0);
         stale_index < (uint32_t)stale_move_count;
         ++stale_index)
    {
        if (stale_move_ids[stale_index] == move_id)
        {
            reduction_f32 +=
                fighter->stale_move_slot_reduction_f32[stale_index];
        }
    }
    return 1.0f - reduction_f32;
}

static float stale_scaled_damage_f32(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t move_id,
    float damage_f32)
{
    float multiplier_f32;

    if (scratch->stale_move_count[player_index] == UINT8_C(0) ||
        move_id == UINT8_C(0))
    {
        return damage_f32;
    }
    multiplier_f32 =
        stale_move_multiplier_f32(
            fighter,
            scratch->stale_move_ids[player_index],
            scratch->stale_move_count[player_index],
            move_id);
    if (multiplier_f32 == 1.0f)
    {
        return damage_f32;
    }

    return damage_f32 * multiplier_f32;
}

static float current_move_stale_scaled_damage_f32(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t move_id,
    float damage_f32)
{
    const uint8_t stale_count =
        scratch->stale_move_count[player_index];
    const uint8_t skip_current =
        scratch->attack_stale_registered[player_index] != UINT8_C(0) &&
        stale_count != UINT8_C(0) &&
        scratch->stale_move_ids[player_index][0] == move_id;
    const uint8_t *stale_ids =
        &scratch->stale_move_ids[player_index][skip_current];
    const uint8_t effective_count =
        (uint8_t)(stale_count - skip_current);
    const float multiplier_f32 =
        stale_move_multiplier_f32(
            fighter,
            stale_ids,
            effective_count,
            move_id);

    return damage_f32 * multiplier_f32;
}

static void register_stale_move(
    pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t move_id)
{
    uint32_t stale_index;
    uint32_t stale_count =
        (uint32_t)scratch->stale_move_count[player_index];

    if (stale_count <
        (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY)
    {
        ++stale_count;
        scratch->stale_move_count[player_index] =
            (uint8_t)stale_count;
    }
    for (stale_index = stale_count - UINT32_C(1);
         stale_index != UINT32_C(0);
         --stale_index)
    {
        scratch->stale_move_ids[player_index][stale_index] =
            scratch->stale_move_ids[player_index]
                                   [stale_index - UINT32_C(1)];
    }
    scratch->stale_move_ids[player_index][0] = move_id;
    scratch->stale_move_dirty_mask |=
        (uint8_t)(UINT32_C(1) << player_index);
}

static float scaled_knockback(
    float base_f32,
    float growth_f32,
    float damage_f32,
    int divide_growth)
{
    float growth = growth_f32 * damage_f32;
    float result;

    if (divide_growth != 0)
    {
        growth *= 0.5f;
    }
    result = base_f32 + growth;
    if (result > PF_SIM_MAX_MOTION_SPEED_F32)
    {
        return PF_SIM_MAX_MOTION_SPEED_F32;
    }
    return result;
}

static uint16_t hitstun_ticks(
    const fighter_data *fighter,
    float velocity_x_f32,
    float velocity_y_f32)
{
    const float horizontal = fabsf(velocity_x_f32);
    const float vertical = fabsf(velocity_y_f32);
    const float divisor = fighter->hitstun_velocity_per_tick_f32;
    uint32_t ticks = (uint32_t)ceilf((horizontal + vertical) / divisor);

    if (ticks < UINT32_C(1))
    {
        ticks = UINT32_C(1);
    }
    if (ticks > (uint32_t)PF_SIM_MAX_HITSTUN_TICKS)
    {
        ticks = (uint32_t)PF_SIM_MAX_HITSTUN_TICKS;
    }
    return (uint16_t)ticks;
}

static float scale_velocity_f32(
    float velocity_f32,
    float scale_f32)
{
    return velocity_f32 * scale_f32;
}

static float apply_weight_f32(
    float velocity_f32,
    float weight_f32)
{
    float weighted_velocity = velocity_f32 / weight_f32;

    if (weighted_velocity > PF_SIM_MAX_MOTION_SPEED_F32)
    {
        return PF_SIM_MAX_MOTION_SPEED_F32;
    }
    if (weighted_velocity < -PF_SIM_MAX_MOTION_SPEED_F32)
    {
        return -PF_SIM_MAX_MOTION_SPEED_F32;
    }
    return weighted_velocity;
}

static uint16_t scale_hitstun_ticks(
    uint16_t hitstun_ticks_value,
    float scale_f32)
{
    uint32_t scaled_ticks =
        (uint32_t)((float)hitstun_ticks_value * scale_f32);

    if (hitstun_ticks_value != UINT16_C(0) && scaled_ticks == UINT32_C(0))
    {
        scaled_ticks = UINT32_C(1);
    }
    return (uint16_t)scaled_ticks;
}

static int action_is_v_cancel_eligible(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_WALL_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
           action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
           action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE;
}

static int player_v_cancelled(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const uint8_t input_age =
        scratch->trigger_input_age[player_index];

    if (scratch->grounded[player_index] != UINT8_C(0) ||
        !action_is_v_cancel_eligible(
            scratch->action_state[player_index]) ||
        input_age >= fighter->v_cancel_window_ticks ||
        (uint16_t)input_age > fighter->tech_lockout_ticks)
    {
        return 0;
    }
    return scratch->tech_lockout_ticks[player_index] ==
           (uint16_t)(fighter->tech_lockout_ticks - input_age);
}

static int action_is_guarding(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN;
}

static int action_has_shield_volume(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return action_is_guarding(action_state) ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
            hitlag_resume_action ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN);
}

static int action_is_recovery_invulnerable(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t prone_orientation,
    int8_t tech_direction,
    int8_t facing)
{
    if (action_state == (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM)
    {
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
    {
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        return action_ticks >=
                   fighter->air_dodge_invulnerability_begin_tick &&
               action_ticks <
                   fighter->air_dodge_invulnerability_end_tick;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD)
    {
        return action_ticks >=
                   fighter->roll_invulnerability_begin_tick &&
               action_ticks <
                   fighter->roll_invulnerability_end_tick;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        return action_ticks >=
                   fighter->spot_dodge_invulnerability_begin_tick &&
               action_ticks <
                   fighter->spot_dodge_invulnerability_end_tick;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
    {
        return action_ticks <
               fighter->ledge_roll_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        return action_ticks <
               fighter->ledge_attack_invulnerability_ticks;
    }
    if (action_state ==
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        action_state == (uint8_t)PF_M4_ACTION_WALL_TECH ||
        action_state == (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP ||
        action_state == (uint8_t)PF_M4_ACTION_CEILING_TECH)
    {
        return action_ticks <
               fighter->tech_invulnerability_ticks;
    }
    if (action_state ==
        (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL)
    {
        return action_ticks <
               fighter->getup_neutral_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL)
    {
        const getup_roll_timing *timing =
            getup_roll_timing_for(
                fighter,
                prone_orientation,
                tech_direction,
                facing);
        const uint16_t action_frame =
            action_ticks != UINT16_MAX
                ? (uint16_t)(action_ticks + UINT16_C(1))
                : UINT16_MAX;

        return timing != NULL &&
               action_frame >= timing->invulnerability_begin_tick &&
               action_frame <= timing->invulnerability_end_tick;
    }
    return action_state ==
               (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
           action_ticks <
               getup_attack_invulnerability_ticks_for(
                   fighter,
                   prone_orientation);
}

typedef struct attack_runtime
{
    float hitbox_offset_x_f32;
    float hitbox_offset_y_f32;
    float hitbox_half_width_f32;
    float hitbox_half_height_f32;
    float damage_f32;
    float base_knockback_x_f32;
    float base_knockback_y_f32;
    float knockback_growth_f32;
    uint16_t active_begin_tick;
    uint16_t active_end_tick;
    uint16_t hitlag_ticks;
    int16_t shield_damage;
    float target_hitlag_multiplier_f32;
    float knockback_damage_f32;
    melee_knockback_data reference_melee_knockback;
    const melee_knockback_data *melee_knockback;
    int8_t direction;
    int8_t vertical_direction;
    uint8_t action_state;
} attack_runtime;

static int action_is_throw(uint8_t action_state);

static void copy_attack_state(
    attack_runtime *destination,
    const attack_runtime *source)
{
    *destination = *source;
    if (source->melee_knockback == &source->reference_melee_knockback)
    {
        destination->melee_knockback =
            &destination->reference_melee_knockback;
    }
}

static const attack_data *ground_attack_data(
    const fighter_data *fighter,
    uint8_t action_state)
{
    switch ((enum action_state)action_state)
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

static float smash_charged_damage(
    const fighter_data *fighter,
    float base_damage_f32,
    uint16_t charge_ticks)
{
    const uint16_t clamped_ticks =
        charge_ticks < fighter->smash_charge_max_ticks
            ? charge_ticks
            : fighter->smash_charge_max_ticks;
    const float bonus_damage =
        base_damage_f32 * fighter->smash_charge_damage_bonus_f32 *
        (float)clamped_ticks / (float)fighter->smash_charge_max_ticks;

    return base_damage_f32 + bonus_damage;
}

static float authored_base_damage_for_action(
    const fighter_data *fighter,
    uint8_t action_state)
{
    const attack_data *ground_attack =
        ground_attack_data(fighter, action_state);

    if (ground_attack != NULL)
    {
        return ground_attack->damage_f32;
    }
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
            return fighter->jab_damage_f32;
        case PF_M4_ACTION_JAB_FINAL:
            return fighter->jab_final_damage_f32;
        case PF_M4_ACTION_DASH_ATTACK:
            return fighter->dash_attack_damage_f32;
        case PF_M4_ACTION_STRONG_ATTACK:
            return fighter->strong_damage_f32;
        case PF_M4_ACTION_AERIAL_ATTACK:
            return fighter->aerial_damage_f32;
        case PF_M4_ACTION_FORWARD_AERIAL:
            return fighter->forward_aerial.damage_f32;
        case PF_M4_ACTION_BACK_AERIAL:
            return fighter->back_aerial.damage_f32;
        case PF_M4_ACTION_UP_AERIAL:
            return fighter->up_aerial.damage_f32;
        case PF_M4_ACTION_DOWN_AERIAL:
            return fighter->down_aerial.damage_f32;
        case PF_M4_ACTION_PUMMEL:
            return fighter->pummel_damage_f32;
        default:
            return 0.0f;
    }
}

static int reference_knockback_matches(
    const melee_knockback_data *knockback,
    const reference_hit_effect *effect)
{
    return knockback != NULL && effect != NULL &&
           knockback->enabled != UINT8_C(0) &&
           knockback->angle_degrees == effect->angle_degrees &&
           knockback->growth == effect->growth &&
           knockback->weight_set == effect->weight_set &&
           knockback->base == effect->base;
}

static void apply_falcon_reference_effect(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t smash_charge_ticks,
    const reference_hit_effect *effect,
    attack_runtime *attack)
{
    float damage_f32 = (float)effect->damage;

    if (action_is_smash_release(action_state) &&
        smash_charge_ticks != UINT16_C(0))
    {
        damage_f32 = smash_charged_damage(
            fighter,
            damage_f32,
            smash_charge_ticks);
    }
    attack->damage_f32 = damage_f32;
    attack->knockback_damage_f32 = damage_f32;
    attack->shield_damage = (int16_t)effect->shield_damage;
    attack->hitlag_ticks = melee_hitlag_ticks(
        damage_f32,
        UINT8_C(0),
        1.0f);
    attack->target_hitlag_multiplier_f32 = 1.0f;
    if (effect->element == (uint8_t)PF_M4_REFERENCE_HIT_ELECTRIC)
    {
        const ssbm_damage_response_attributes *common =
            ssbm_common_reference_damage_response();

        if (common != NULL)
        {
            attack->target_hitlag_multiplier_f32 =
                common->electric_hitlag_scale_f32;
        }
    }
    attack->reference_melee_knockback.angle_degrees =
        effect->angle_degrees;
    attack->reference_melee_knockback.growth = effect->growth;
    attack->reference_melee_knockback.weight_set = effect->weight_set;
    attack->reference_melee_knockback.base = effect->base;
    attack->reference_melee_knockback.enabled = UINT8_C(1);
    (void)memset(
        attack->reference_melee_knockback.reserved,
        0,
        sizeof(attack->reference_melee_knockback.reserved));
    attack->melee_knockback = &attack->reference_melee_knockback;
}

static void apply_stale_scaled_reference_damage(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint8_t action_state,
    attack_runtime *attack)
{
    const int uses_reference_effect =
        attack->melee_knockback == &attack->reference_melee_knockback;

    attack->damage_f32 = stale_scaled_damage_f32(
        fighter,
        scratch,
        attacker_index,
        stale_move_id_for_action(action_state),
        attack->damage_f32);
    if (uses_reference_effect != 0)
    {
        attack->hitlag_ticks = melee_hitlag_ticks(
            attack->damage_f32,
            UINT8_C(0),
            1.0f);
    }
}

/*
 * Returns 1 when the generated Falcon route supplied this frame's effect,
 * 0 when authored content does not match that route, and -1 for an inactive
 * frame inside an otherwise table-backed action. This keeps custom content
 * authored while making the default fighter's disjoint hit phases exact.
 */
static int apply_falcon_reference_frame(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_frame,
    uint16_t smash_charge_ticks,
    attack_runtime *attack)
{
    falcon_move_index move_index;
    const reference_hit_effect *primary_effect;
    const reference_hit_effect *frame_effect;
    reference_timing timing;
    int reference_special;
    int reference_source_only_action;

    if (fighter == NULL || attack == NULL ||
        !falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    primary_effect = falcon_reference_primary_effect(move_index);
    timing = falcon_reference_timing(move_index);
    reference_special =
        move_index >= PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND;
    reference_source_only_action =
        action_is_reference_jab_extension(action_state) ||
        action_is_reference_angled_normal(action_state);
    if (primary_effect == NULL || timing.active_ticks == UINT16_C(0) ||
        (reference_special != 0 &&
         fighter->reference_frame_data_enabled == UINT8_C(0)) ||
        (reference_special == 0 && reference_source_only_action == 0 &&
         action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
         !reference_knockback_matches(
             &fighter->jab_melee_knockback,
             primary_effect)) ||
        (reference_special == 0 && reference_source_only_action == 0 &&
         action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL &&
         !reference_knockback_matches(
             &fighter->jab_final_melee_knockback,
             primary_effect)) ||
        attack->active_begin_tick !=
            timing.startup_ticks + UINT16_C(1) ||
        attack->active_end_tick !=
            timing.startup_ticks + timing.active_ticks ||
        (reference_special == 0 && reference_source_only_action == 0 &&
         authored_base_damage_for_action(fighter, action_state) !=
             (float)primary_effect->damage))
    {
        return 0;
    }
    frame_effect = falcon_reference_effect_at_frame(
        move_index,
        action_frame);
    if (frame_effect == NULL)
    {
        return -1;
    }

    apply_falcon_reference_effect(
        fighter,
        action_state,
        smash_charge_ticks,
        frame_effect,
        attack);
    return 1;
}

static int action_is_falcon_dive_capture_phase(
    uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH;
}

static int attack_for_action(
    const struct content *content,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t charge_ticks,
    uint16_t smash_charge_ticks,
    attack_runtime *out_attack)
{
    const fighter_data *fighter;

    if (content == NULL || out_attack == NULL)
    {
        return 0;
    }
    fighter = &content->fighter;
    out_attack->melee_knockback = NULL;
    out_attack->shield_damage = INT16_C(0);
    out_attack->target_hitlag_multiplier_f32 = 1.0f;

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        !action_is_falcon_dive_capture_phase(action_state))
    {
        falcon_move_index reference_move_index;

        if (falcon_reference_move_for_action(
                action_state,
                &reference_move_index) &&
            (reference_move_index >=
                 PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND ||
             action_is_throw(action_state) ||
             action_state == (uint8_t)PF_M4_ACTION_PUMMEL ||
             action_is_reference_jab_extension(action_state) ||
             action_is_reference_angled_normal(action_state)))
        {
            const reference_hit_effect *effect =
                falcon_reference_primary_effect(
                    reference_move_index);
            const reference_timing timing =
                falcon_reference_timing(
                    reference_move_index);

            if (effect == NULL || timing.active_ticks == UINT16_C(0))
            {
                return 0;
            }
            (void)memset(out_attack, 0, sizeof(*out_attack));
            if (action_state == (uint8_t)PF_M4_ACTION_JAB_THIRD)
            {
                out_attack->hitbox_offset_x_f32 =
                    fighter->jab_final_hitbox_offset_x_f32;
                out_attack->hitbox_offset_y_f32 =
                    fighter->jab_final_hitbox_offset_y_f32;
                out_attack->hitbox_half_width_f32 =
                    fighter->jab_final_hitbox_half_width_f32;
                out_attack->hitbox_half_height_f32 =
                    fighter->jab_final_hitbox_half_height_f32;
            }
            else if (action_state ==
                         (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP)
            {
                /* Custom content still needs a rectangular fallback. The
                 * generated Falcon path selects the executable Attack100
                 * spheres before this envelope reaches collision. */
                out_attack->hitbox_offset_x_f32 =
                    fighter->jab_hitbox_offset_x_f32;
                out_attack->hitbox_offset_y_f32 =
                    fighter->jab_hitbox_offset_y_f32;
                out_attack->hitbox_half_width_f32 =
                    fighter->jab_hitbox_half_width_f32;
                out_attack->hitbox_half_height_f32 =
                    fighter->jab_hitbox_half_height_f32;
            }
            else if (action_is_reference_angled_normal(action_state))
            {
                const attack_data *fallback =
                    ground_attack_data(fighter, action_state);

                if (fallback != NULL)
                {
                    out_attack->hitbox_offset_x_f32 =
                        fallback->hitbox_offset_x_f32;
                    out_attack->hitbox_offset_y_f32 =
                        fallback->hitbox_offset_y_f32;
                    out_attack->hitbox_half_width_f32 =
                        fallback->hitbox_half_width_f32;
                    out_attack->hitbox_half_height_f32 =
                        fallback->hitbox_half_height_f32;
                }
            }
            out_attack->active_begin_tick =
                timing.startup_ticks + UINT16_C(1);
            out_attack->active_end_tick =
                timing.startup_ticks + timing.active_ticks;
            out_attack->direction = INT8_C(1);
            out_attack->vertical_direction = INT8_C(-1);
            out_attack->action_state = action_state;
            apply_falcon_reference_effect(
                fighter,
                action_state,
                smash_charge_ticks,
                effect,
                out_attack);
            return 1;
        }
    }

    if (action_state == (uint8_t)PF_M4_ACTION_DASH_ATTACK)
    {
        out_attack->hitbox_offset_x_f32 =
            fighter->dash_attack_hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            fighter->dash_attack_hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            fighter->dash_attack_hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            fighter->dash_attack_hitbox_half_height_f32;
        out_attack->damage_f32 = fighter->dash_attack_damage_f32;
        out_attack->base_knockback_x_f32 =
            fighter->dash_attack_base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            fighter->dash_attack_base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            fighter->dash_attack_knockback_growth_f32;
        out_attack->active_begin_tick =
            fighter->dash_attack_startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            fighter->dash_attack_startup_ticks +
            fighter->dash_attack_active_ticks;
        out_attack->hitlag_ticks =
            fighter->dash_attack_hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_DASH_ATTACK;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL)
    {
        out_attack->hitbox_offset_x_f32 =
            fighter->jab_final_hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            fighter->jab_final_hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            fighter->jab_final_hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            fighter->jab_final_hitbox_half_height_f32;
        out_attack->damage_f32 = fighter->jab_final_damage_f32;
        out_attack->base_knockback_x_f32 =
            fighter->jab_final_base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            fighter->jab_final_base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            fighter->jab_final_knockback_growth_f32;
        out_attack->active_begin_tick =
            fighter->jab_final_startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            fighter->jab_final_startup_ticks +
            fighter->jab_final_active_ticks;
        out_attack->hitlag_ticks = fighter->jab_final_hitlag_ticks;
        out_attack->melee_knockback =
            fighter->jab_final_melee_knockback.enabled != UINT8_C(0)
                ? &fighter->jab_final_melee_knockback
                : NULL;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state = (uint8_t)PF_M4_ACTION_JAB_FINAL;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        out_attack->hitbox_offset_x_f32 =
            fighter->jab_hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            fighter->jab_hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            fighter->jab_hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            fighter->jab_hitbox_half_height_f32;
        out_attack->damage_f32 = fighter->jab_damage_f32;
        out_attack->base_knockback_x_f32 =
            fighter->jab_base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            fighter->jab_base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            fighter->jab_knockback_growth_f32;
        out_attack->active_begin_tick =
            fighter->jab_startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            fighter->jab_startup_ticks +
            fighter->jab_active_ticks;
        out_attack->hitlag_ticks = fighter->jab_hitlag_ticks;
        out_attack->melee_knockback =
            fighter->jab_melee_knockback.enabled != UINT8_C(0)
                ? &fighter->jab_melee_knockback
                : NULL;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
        return 1;
    }
    if (ground_attack_data(fighter, action_state) != NULL)
    {
        const attack_data *attack =
            ground_attack_data(fighter, action_state);

        out_attack->hitbox_offset_x_f32 =
            attack->hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            attack->hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            attack->hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            attack->hitbox_half_height_f32;
        out_attack->damage_f32 = attack->damage_f32;
        if (action_is_smash_release(action_state) &&
            smash_charge_ticks != UINT16_C(0))
        {
            out_attack->damage_f32 = smash_charged_damage(
                fighter,
                attack->damage_f32,
                smash_charge_ticks);
        }
        out_attack->base_knockback_x_f32 =
            attack->base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            attack->base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            attack->knockback_growth_f32;
        out_attack->active_begin_tick =
            attack->startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            attack->startup_ticks + attack->active_ticks;
        out_attack->hitlag_ticks = attack->hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction =
            action_state == (uint8_t)PF_M4_ACTION_DOWN_ATTACK
                ? INT8_C(1)
                : INT8_C(-1);
        out_attack->action_state = action_state;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        const attack_data *attack = &fighter->ledge_attack;

        out_attack->hitbox_offset_x_f32 =
            attack->hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            attack->hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            attack->hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            attack->hitbox_half_height_f32;
        out_attack->damage_f32 = attack->damage_f32;
        out_attack->base_knockback_x_f32 =
            attack->base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            attack->base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            attack->knockback_growth_f32;
        out_attack->active_begin_tick =
            attack->startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            attack->startup_ticks + attack->active_ticks;
        out_attack->hitlag_ticks = attack->hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state = action_state;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_FORWARD_AERIAL ||
        action_state == (uint8_t)PF_M4_ACTION_BACK_AERIAL ||
        action_state == (uint8_t)PF_M4_ACTION_UP_AERIAL ||
        action_state == (uint8_t)PF_M4_ACTION_DOWN_AERIAL)
    {
        const attack_data *attack;

        switch ((enum action_state)action_state)
        {
            case PF_M4_ACTION_FORWARD_AERIAL:
                attack = &fighter->forward_aerial;
                break;
            case PF_M4_ACTION_BACK_AERIAL:
                attack = &fighter->back_aerial;
                break;
            case PF_M4_ACTION_UP_AERIAL:
                attack = &fighter->up_aerial;
                break;
            case PF_M4_ACTION_DOWN_AERIAL:
                attack = &fighter->down_aerial;
                break;
            default:
                return 0;
        }

        out_attack->hitbox_offset_x_f32 =
            attack->hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            attack->hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            attack->hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            attack->hitbox_half_height_f32;
        out_attack->damage_f32 = attack->damage_f32;
        out_attack->base_knockback_x_f32 =
            attack->base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            attack->base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            attack->knockback_growth_f32;
        out_attack->active_begin_tick =
            attack->startup_ticks + UINT16_C(1);
        out_attack->active_end_tick = (uint16_t)(
            (uint32_t)attack->startup_ticks +
            (uint32_t)attack->active_ticks);
        out_attack->hitlag_ticks = attack->hitlag_ticks;
        out_attack->direction =
            action_state == (uint8_t)PF_M4_ACTION_BACK_AERIAL
                ? INT8_C(-1)
                : INT8_C(1);
        out_attack->vertical_direction =
            action_state == (uint8_t)PF_M4_ACTION_DOWN_AERIAL
                ? INT8_C(1)
                : INT8_C(-1);
        out_attack->action_state = action_state;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        action_state ==
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK)
    {
        out_attack->hitbox_offset_x_f32 =
            fighter->strong_hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            fighter->strong_hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            fighter->strong_hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            fighter->strong_hitbox_half_height_f32;
        out_attack->damage_f32 = fighter->strong_damage_f32;
        out_attack->base_knockback_x_f32 =
            fighter->strong_base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            fighter->strong_base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            fighter->strong_knockback_growth_f32;
        out_attack->active_begin_tick =
            fighter->strong_startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            fighter->strong_startup_ticks +
            fighter->strong_active_ticks;
        out_attack->hitlag_ticks = fighter->strong_hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state = action_state;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
    {
        out_attack->hitbox_offset_x_f32 =
            fighter->aerial_hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            fighter->aerial_hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            fighter->aerial_hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            fighter->aerial_hitbox_half_height_f32;
        out_attack->damage_f32 = fighter->aerial_damage_f32;
        out_attack->base_knockback_x_f32 =
            fighter->aerial_base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            fighter->aerial_base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            fighter->aerial_knockback_growth_f32;
        out_attack->active_begin_tick =
            fighter->aerial_startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            (uint16_t)(
                (uint32_t)fighter->aerial_startup_ticks +
                (uint32_t)fighter->aerial_active_ticks);
        out_attack->hitlag_ticks = fighter->aerial_hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
        action_state == (uint8_t)PF_M4_ACTION_REFLECTOR_AIR)
    {
        const reflector_data *reflector = &content->reflector;

        if (reflector->enabled == UINT8_C(0))
        {
            return 0;
        }
        out_attack->hitbox_offset_x_f32 =
            reflector->hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            reflector->hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            reflector->hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            reflector->hitbox_half_height_f32;
        out_attack->damage_f32 = reflector->damage_f32;
        out_attack->base_knockback_x_f32 =
            reflector->base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            reflector->base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            reflector->knockback_growth_f32;
        out_attack->active_begin_tick =
            reflector->startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            reflector->startup_ticks + reflector->active_ticks;
        out_attack->hitlag_ticks = reflector->hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(1);
        out_attack->action_state = action_state;
        return 1;
    }
    if (action_state ==
        (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND)
    {
        const charge_data *charge = &content->charge;
        const uint16_t bounded_charge =
            charge_ticks < charge->max_charge_ticks
                ? charge_ticks
                : charge->max_charge_ticks;

        if (charge->enabled == UINT8_C(0))
        {
            return 0;
        }
        out_attack->hitbox_offset_x_f32 =
            charge->hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            charge->hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            charge->hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            charge->hitbox_half_height_f32;
        out_attack->damage_f32 =
            charge->base_damage_f32 +
            charge->bonus_damage_f32 * (float)bounded_charge /
                (float)charge->max_charge_ticks;
        out_attack->base_knockback_x_f32 =
            charge->base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            charge->base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            charge->knockback_growth_f32;
        out_attack->active_begin_tick =
            charge->release_startup_ticks + UINT16_C(1);
        out_attack->active_end_tick =
            charge->release_startup_ticks +
            charge->release_active_ticks;
        out_attack->hitlag_ticks = charge->release_hitlag_ticks;
        out_attack->direction = INT8_C(1);
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state = action_state;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ATTACK)
    {
        const uint32_t action_frame =
            (uint32_t)action_ticks + UINT32_C(1);
        int8_t direction;
        uint16_t active_begin;
        uint16_t active_end;

        if (action_frame >=
                fighter->getup_attack_front_active_begin_tick &&
            action_frame <=
                fighter->getup_attack_front_active_end_tick)
        {
            direction = INT8_C(1);
            active_begin =
                fighter->getup_attack_front_active_begin_tick -
                UINT16_C(1);
            active_end =
                fighter->getup_attack_front_active_end_tick -
                UINT16_C(1);
        }
        else if (
            action_frame >=
                fighter->getup_attack_back_active_begin_tick &&
            action_frame <=
                fighter->getup_attack_back_active_end_tick)
        {
            direction = INT8_C(-1);
            active_begin =
                fighter->getup_attack_back_active_begin_tick -
                UINT16_C(1);
            active_end =
                fighter->getup_attack_back_active_end_tick -
                UINT16_C(1);
        }
        else
        {
            return 0;
        }

        out_attack->hitbox_offset_x_f32 =
            fighter->getup_attack_hitbox_offset_x_f32;
        out_attack->hitbox_offset_y_f32 =
            fighter->getup_attack_hitbox_offset_y_f32;
        out_attack->hitbox_half_width_f32 =
            fighter->getup_attack_hitbox_half_width_f32;
        out_attack->hitbox_half_height_f32 =
            fighter->getup_attack_hitbox_half_height_f32;
        out_attack->damage_f32 =
            fighter->getup_attack_damage_f32;
        out_attack->base_knockback_x_f32 =
            fighter->getup_attack_base_knockback_x_f32;
        out_attack->base_knockback_y_f32 =
            fighter->getup_attack_base_knockback_y_f32;
        out_attack->knockback_growth_f32 =
            fighter->getup_attack_knockback_growth_f32;
        out_attack->active_begin_tick = active_begin;
        out_attack->active_end_tick = active_end;
        out_attack->hitlag_ticks =
            fighter->getup_attack_hitlag_ticks;
        out_attack->direction = direction;
        out_attack->vertical_direction = INT8_C(-1);
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_GETUP_ATTACK;
        return 1;
    }
    return 0;
}

static int attack_for_state(
    const struct content *content,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t source_submotion,
    uint16_t charge_ticks,
    uint16_t smash_charge_ticks,
    attack_runtime *out_attack)
{
    const falcon_ledge_attack_reference *ledge_attack;

    if (!attack_for_action(
            content,
            action_state,
            action_ticks,
            charge_ticks,
            smash_charge_ticks,
            out_attack))
    {
        return 0;
    }
    if (action_state != (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
        content->fighter.reference_frame_data_enabled == UINT8_C(0))
    {
        return 1;
    }
    ledge_attack =
        falcon_reference_ledge_attack(source_submotion);
    if (ledge_attack == NULL ||
        ledge_attack->first_active_frame == UINT16_C(0) ||
        ledge_attack->last_active_frame <
            ledge_attack->first_active_frame)
    {
        return 0;
    }
    out_attack->active_begin_tick =
        ledge_attack->first_active_frame - UINT16_C(1);
    out_attack->active_end_tick =
        ledge_attack->last_active_frame - UINT16_C(1);
    apply_falcon_reference_effect(
        &content->fighter,
        action_state,
        smash_charge_ticks,
        &ledge_attack->effect,
        out_attack);
    return 1;
}

typedef struct shield_hit_response
{
    float damage_f32;
    uint16_t stun_ticks;
    float stun_duration_f32;
    float defender_pushback_f32;
    float attacker_pushback_f32;
} shield_hit_response;

static float guard_setoff_animation_rate_f32(
    const fighter_data *fighter,
    float stun_duration_f32)
{
    const falcon_submotion_data *motion;
    float endpoint_f32;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) ||
        stun_duration_f32 <= 0.0f)
    {
        return 0.0f;
    }
    motion = falcon_reference_submotion(
        (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF);
    if (motion == NULL || motion->animation_frame_count == UINT16_C(0))
    {
        return 0.0f;
    }
    /* ftCo_80092F2C sets (lbGetJObjEndFrame() + 0.1) / shield_stun.
     * The animation endpoint is imported per fighter; the 0.1 bias belongs
     * to the common callback and is rounded once to binary32. */
    endpoint_f32 = (float)motion->animation_frame_count + 0.1f;
    return endpoint_f32 / stun_duration_f32;
}

static inline float shield_pressure_lerp_f32(
    float light_f32,
    float dense_f32,
    uint16_t shield_strength)
{
    return light_f32 -
           (light_f32 - dense_f32) *
               (float)shield_strength / (float)UINT16_MAX;
}

static inline float shield_hit_damage_f32(
    float attack_damage_f32)
{
    float integer_damage = floorf(attack_damage_f32);

    if (attack_damage_f32 != 0.0f && integer_damage == 0.0f)
    {
        integer_damage = 1.0f;
    }
    return integer_damage;
}

static shield_hit_response shield_hit_response_for(
    const fighter_data *fighter,
    float attack_damage_f32,
    int16_t shield_damage,
    uint16_t shield_strength,
    int powershield)
{
    shield_hit_response response;
    const float shield_hit_damage_f32_value =
        shield_hit_damage_f32(attack_damage_f32);
    const float shield_health_damage_f32_signed =
        shield_hit_damage_f32_value + (float)shield_damage;
    const float shield_health_damage_f32 =
        shield_health_damage_f32_signed > 0.0f
            ? shield_health_damage_f32_signed
            : 0.0f;
    const float damage_multiplier_f32 =
        shield_pressure_lerp_f32(
            fighter->light_shield_damage_multiplier_f32,
            fighter->dense_shield_damage_multiplier_f32,
            shield_strength);
    const float stun_damage_multiplier_f32 =
        shield_pressure_lerp_f32(
            fighter->light_shield_stun_damage_multiplier_f32,
            fighter->dense_shield_stun_damage_multiplier_f32,
            shield_strength);
    const float stun_duration_f32 =
        shield_hit_damage_f32_value * stun_damage_multiplier_f32 +
        fighter->shield_stun_base_f32;
    const float pressure_damage_f32 =
        shield_hit_damage_f32_value * (float)shield_strength /
        (float)UINT16_MAX;
    uint32_t ticks = (uint32_t)floorf(stun_duration_f32 * 200.0f / 201.0f);
    float defender_pushback_f32 =
        stun_duration_f32 * fighter->shield_defender_pushback_stun_scale_f32;
    float attacker_pushback_f32 =
        pressure_damage_f32 * fighter->shield_attacker_pushback_damage_f32;

    response.damage_f32 = shield_health_damage_f32 * damage_multiplier_f32;
    if (ticks < UINT32_C(1))
    {
        ticks = UINT32_C(1);
    }
    if (ticks > (uint32_t)UINT16_MAX)
    {
        ticks = (uint32_t)UINT16_MAX;
    }
    response.stun_ticks = (uint16_t)ticks;
    response.stun_duration_f32 = stun_duration_f32;

    if (powershield == 0)
    {
        defender_pushback_f32 *=
            fighter->shield_defender_pushback_normal_scale_f32;
    }
    if (defender_pushback_f32 >
        fighter->shield_defender_pushback_max_f32)
    {
        defender_pushback_f32 =
            fighter->shield_defender_pushback_max_f32;
    }
    response.defender_pushback_f32 =
        defender_pushback_f32;

    attacker_pushback_f32 +=
        fighter->shield_attacker_pushback_base_f32;
    if (attacker_pushback_f32 >
        PF_SIM_MAX_MOTION_SPEED_F32)
    {
        attacker_pushback_f32 =
            PF_SIM_MAX_MOTION_SPEED_F32;
    }
    response.attacker_pushback_f32 =
        attacker_pushback_f32;
    return response;
}

static pf_status apply_shield_hit(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const fighter_data *fighter,
    uint32_t source_index,
    uint32_t target_index,
    float attack_damage_f32,
    int16_t shield_damage,
    uint16_t hitlag_ticks,
    int32_t horizontal_direction,
    int powershield,
    uint16_t source_action,
    shield_hit_response *out_response)
{
    const shield_hit_response response =
        shield_hit_response_for(
            fighter,
            attack_damage_f32,
            shield_damage,
            scratch->shield_strength[target_index],
            powershield);
    pf_sim_event_type event_type;
    const int shield_broken =
        powershield == 0 &&
        response.damage_f32 >
            scratch->shield_health_f32[target_index];

    if (powershield == 0)
    {
        scratch->shield_health_f32[target_index] =
            response.damage_f32 >=
                    scratch->shield_health_f32[target_index]
                ? UINT32_C(0)
                : scratch->shield_health_f32[target_index] -
                      response.damage_f32;
    }
    scratch->velocity_x_f32[target_index] =
        (float)horizontal_direction * response.defender_pushback_f32;
    scratch->velocity_y_f32[target_index] = INT32_C(0);
    scratch->powershield[target_index] =
        powershield ? UINT8_C(1) : UINT8_C(0);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    set_action_state(
        world,
        scratch,
        target_index,
        (uint8_t)PF_M4_ACTION_HITLAG);
    scratch->dash_direction[target_index] = INT8_C(0);
    scratch->short_hop_latched[target_index] = UINT8_C(0);
    scratch->fast_fall[target_index] = UINT8_C(0);

    /* Fighter_ProcessHit breaks only after the floating shield value crosses
     * below zero. Equality leaves a zero-health GuardSetOff that will break
     * on the next positive drain or shield hit. Preserve that strict boundary
     * despite the canonical unsigned, saturating health representation. */
    if (shield_broken != 0)
    {
        /* Fighter_ProcessHit assigns ftCommonData.x280 before entering
         * ShieldBreakFly. Passive Guard depletion instead enters the same
         * action from zero health; keep the two source paths distinct. */
        scratch->shield_health_f32[target_index] =
            fighter->shield_reset_health_f32;
        scratch->shield_stun_ticks[target_index] = UINT16_C(0);
        scratch->hitlag_resume_action[target_index] =
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
        scratch->action_ticks[target_index] = UINT16_C(0);
        scratch->shield_strength[target_index] = UINT16_C(0);
        scratch->shield_angle_turn[target_index] = UINT16_C(0);
        scratch->shield_magnitude[target_index] = UINT16_C(0);
        event_type = PF_SIM_EVENT_SHIELD_BREAK;
    }
    else
    {
        /* ftCo_80092F2C enters GuardSetOff with x670 = 0xFE. Its
         * shield-SDI callback consumes only the ordinary horizontal age;
         * UCF's continuous x673 age remains independent. */
        reset_ordinary_tilt_x_age(scratch, target_index);
        scratch->shield_stun_ticks[target_index] = response.stun_ticks;
        scratch->source_animation_frame_f32[target_index] = INT32_C(0);
        scratch->source_animation_rate_f32[target_index] =
            guard_setoff_animation_rate_f32(
                fighter, response.stun_duration_f32);
        scratch->hitlag_resume_action[target_index] =
            (uint8_t)PF_M4_ACTION_SHIELD_STUN;
        event_type = powershield ? PF_SIM_EVENT_POWERSHIELD
                                 : PF_SIM_EVENT_SHIELD_BLOCK;
    }

    if (out_response != NULL)
    {
        *out_response = response;
    }
    return pf_sim_push_event(
        scratch,
        world->tick,
        event_type,
        (uint8_t)source_index,
        (uint8_t)target_index,
        powershield ? UINT32_C(0) : response.damage_f32,
        scratch->velocity_x_f32[target_index],
        scratch->velocity_y_f32[target_index],
        UINT16_C(0),
        source_action,
        NULL);
}

static float reference_origin_y_f32(
    const fighter_data *fighter,
    float position_y_f32)
{
    return position_y_f32 + fighter->half_height_f32;
}

static uint16_t reference_move_action_frame(
    falcon_move_index move_index,
    uint16_t action_ticks)
{
    if (move_index >= PF_M4_FALCON_NEUTRAL_AERIAL &&
        move_index <= PF_M4_FALCON_DOWN_AERIAL &&
        action_ticks < UINT16_MAX)
    {
        /* The canonical aerial states store zero on displayed frame 1;
         * imported hit/hurt tracks are indexed by Melee's displayed frame. */
        return (uint16_t)(action_ticks + UINT16_C(1));
    }
    return action_ticks;
}

static uint8_t reference_hit_spheres_at_world(
    const fighter_data *fighter,
    falcon_move_index move_index,
    float position_x_f32,
    float position_y_f32,
    int8_t facing,
    uint16_t action_ticks,
    hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY])
{
    const reference_hit_sphere *source;
    const float reference_origin_y_f32_value =
        reference_origin_y_f32(fighter, position_y_f32);
    uint8_t sphere_count;
    uint8_t sphere_index;

    if (out_spheres == NULL)
    {
        return UINT8_C(0);
    }
    source = falcon_reference_hit_spheres_at_frame(
        move_index,
        reference_move_action_frame(move_index, action_ticks),
        &sphere_count);
    if (source == NULL || sphere_count == UINT8_C(0) ||
        sphere_count > UINT8_C(PF_M4_INSPECTION_HIT_SPHERE_CAPACITY))
    {
        return UINT8_C(0);
    }
    for (sphere_index = UINT8_C(0);
         sphere_index < sphere_count;
         ++sphere_index)
    {
        out_spheres[sphere_index].center_x_f32 =
            position_x_f32 +
            (int32_t)facing * source[sphere_index].offset_x_f32;
        out_spheres[sphere_index].center_y_f32 =
            reference_origin_y_f32_value + source[sphere_index].offset_y_f32;
        out_spheres[sphere_index].center_z_f32 =
            (int32_t)facing * source[sphere_index].offset_z_f32;
        out_spheres[sphere_index].radius_f32 =
            source[sphere_index].radius_f32;
        out_spheres[sphere_index].effect_index =
            source[sphere_index].effect_index;
        out_spheres[sphere_index].hitbox_id =
            source[sphere_index].hitbox_id;
        out_spheres[sphere_index].group_id =
            source[sphere_index].group_id;
        out_spheres[sphere_index].collision_state =
            source[sphere_index].collision_state;
    }
    return sphere_count;
}

static int falcon_geometry_move_for_attack(
    const struct content *content,
    uint8_t action_state,
    falcon_move_index *out_move_index)
{
    falcon_move_index move_index;
    attack_runtime attack;
    int reference_special;

    if (content == NULL ||
        content->fighter.reference_frame_data_enabled == UINT8_C(0) ||
        action_is_falcon_dive_capture_phase(action_state) ||
        !falcon_reference_move_for_action(
            action_state,
            &move_index) ||
        !falcon_reference_has_hit_geometry(move_index) ||
        !attack_for_action(
            content,
            action_state,
            UINT16_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &attack))
    {
        return 0;
    }
    reference_special =
        move_index >= PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND;
    if (reference_special == 0 &&
        !action_is_throw(action_state) &&
        !action_is_reference_angled_normal(action_state) &&
        !falcon_reference_attack_matches(
            action_state,
            (reference_timing){
                (uint16_t)(attack.active_begin_tick - UINT16_C(1)),
                (uint16_t)(
                    attack.active_end_tick -
                    attack.active_begin_tick +
                    UINT16_C(1)),
                (uint16_t)(
                    falcon_reference_move(move_index)->total_frames -
                    attack.active_end_tick)},
            authored_base_damage_for_action(
                &content->fighter,
                action_state)))
    {
        return 0;
    }
    if (out_move_index != NULL)
    {
        *out_move_index = move_index;
    }
    return 1;
}

uint8_t attack_hit_spheres(
    const struct content *content,
    float position_x_f32,
    float position_y_f32,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY])
{
    falcon_move_index move_index;

    if (out_spheres == NULL ||
        !falcon_geometry_move_for_attack(
            content,
            action_state,
            &move_index))
    {
        return UINT8_C(0);
    }
    return reference_hit_spheres_at_world(
        &content->fighter,
        move_index,
        position_x_f32,
        position_y_f32,
        facing,
        action_ticks,
        out_spheres);
}

static int hit_sphere_bounds(
    const hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t sphere_count,
    float *out_left_f32,
    float *out_right_f32,
    float *out_top_f32,
    float *out_bottom_f32)
{
    uint8_t sphere_index;

    if (spheres == NULL || sphere_count == UINT8_C(0) ||
        sphere_count > UINT8_C(PF_M4_INSPECTION_HIT_SPHERE_CAPACITY) ||
        out_left_f32 == NULL || out_right_f32 == NULL ||
        out_top_f32 == NULL || out_bottom_f32 == NULL)
    {
        return 0;
    }
    *out_left_f32 = spheres[0].center_x_f32 - spheres[0].radius_f32;
    *out_right_f32 = spheres[0].center_x_f32 + spheres[0].radius_f32;
    *out_top_f32 = spheres[0].center_y_f32 - spheres[0].radius_f32;
    *out_bottom_f32 = spheres[0].center_y_f32 + spheres[0].radius_f32;
    for (sphere_index = UINT8_C(1);
         sphere_index < sphere_count;
         ++sphere_index)
    {
        const float left =
            spheres[sphere_index].center_x_f32 -
            spheres[sphere_index].radius_f32;
        const float right =
            spheres[sphere_index].center_x_f32 +
            spheres[sphere_index].radius_f32;
        const float top =
            spheres[sphere_index].center_y_f32 -
            spheres[sphere_index].radius_f32;
        const float bottom =
            spheres[sphere_index].center_y_f32 +
            spheres[sphere_index].radius_f32;

        if (left < *out_left_f32)
        {
            *out_left_f32 = left;
        }
        if (right > *out_right_f32)
        {
            *out_right_f32 = right;
        }
        if (top < *out_top_f32)
        {
            *out_top_f32 = top;
        }
        if (bottom > *out_bottom_f32)
        {
            *out_bottom_f32 = bottom;
        }
    }
    return 1;
}

int attack_hitbox(
    const struct content *content,
    float position_x_f32,
    float position_y_f32,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t source_submotion,
    float *out_left_f32,
    float *out_right_f32,
    float *out_top_f32,
    float *out_bottom_f32)
{
    attack_runtime attack;
    falcon_move_index geometry_move_index;
    falcon_move_index reference_move_index;
    hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    uint8_t sphere_count;
    uint16_t effective_action_ticks = action_ticks;
    float center_x;
    float center_y;

    if (content == NULL ||
        out_left_f32 == NULL ||
        out_right_f32 == NULL ||
        out_top_f32 == NULL ||
        out_bottom_f32 == NULL)
    {
        return 0;
    }

    if (falcon_geometry_move_for_attack(
            content,
            action_state,
            &geometry_move_index))
    {
        sphere_count = reference_hit_spheres_at_world(
            &content->fighter,
            geometry_move_index,
            position_x_f32,
            position_y_f32,
            facing,
            action_ticks,
            spheres);
        if (sphere_count == UINT8_C(0))
        {
            return 0;
        }
        return hit_sphere_bounds(
            spheres,
            sphere_count,
            out_left_f32,
            out_right_f32,
            out_top_f32,
            out_bottom_f32);
    }

    if (!attack_for_state(
            content,
            action_state,
            action_ticks,
            source_submotion,
            UINT16_C(0),
            UINT16_C(0),
            &attack))
    {
        return 0;
    }
    if (falcon_reference_move_for_action(
            action_state,
            &reference_move_index))
    {
        effective_action_ticks =
            falcon_reference_effective_hit_frame(
                reference_move_index,
                action_ticks);
    }
    if (apply_falcon_reference_frame(
            &content->fighter,
            action_state,
            action_ticks,
            UINT16_C(0),
            &attack) < 0 ||
        effective_action_ticks < attack.active_begin_tick ||
        effective_action_ticks > attack.active_end_tick)
    {
        return 0;
    }

    center_x = position_x_f32 + (float)facing * (float)attack.direction *
        attack.hitbox_offset_x_f32;
    center_y = position_y_f32 + attack.hitbox_offset_y_f32;
    *out_left_f32 = center_x - attack.hitbox_half_width_f32;
    *out_right_f32 = center_x + attack.hitbox_half_width_f32;
    *out_top_f32 = center_y - attack.hitbox_half_height_f32;
    *out_bottom_f32 = center_y + attack.hitbox_half_height_f32;
    return 1;
}

static int event_is_physical_hit(pf_sim_event_type event_type)
{
    return event_type == PF_SIM_EVENT_HIT ||
           event_type == PF_SIM_EVENT_THROW ||
           event_type == PF_SIM_EVENT_ITEM_HIT ||
           event_type == PF_SIM_EVENT_PROJECTILE_HIT;
}

static int falcon_geometry_move_for_grab(
    const struct content *content,
    uint8_t action_state,
    falcon_move_index *out_move_index)
{
    falcon_move_index move_index;
    reference_timing reference;
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;

    if (content == NULL ||
        content->fighter.reference_frame_data_enabled == UINT8_C(0))
    {
        return 0;
    }
    if (action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
    {
        move_index =
            action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND
                ? PF_M4_FALCON_UP_SPECIAL_GROUND
                : PF_M4_FALCON_UP_SPECIAL_AIR;
        if (!falcon_reference_has_hit_geometry(move_index))
        {
            return 0;
        }
        if (out_move_index != NULL)
        {
            *out_move_index = move_index;
        }
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GRAB)
    {
        move_index = PF_M4_FALCON_GRAB;
        startup_ticks = content->fighter.grab_startup_ticks;
        active_ticks = content->fighter.grab_active_ticks;
        recovery_ticks = content->fighter.grab_recovery_ticks;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB)
    {
        move_index = PF_M4_FALCON_DASH_GRAB;
        startup_ticks = content->fighter.dash_grab_startup_ticks;
        active_ticks = content->fighter.dash_grab_active_ticks;
        recovery_ticks = content->fighter.dash_grab_recovery_ticks;
    }
    else
    {
        return 0;
    }
    reference = falcon_reference_timing(move_index);
    if (!falcon_reference_has_hit_geometry(move_index) ||
        startup_ticks != reference.startup_ticks ||
        active_ticks != reference.active_ticks ||
        recovery_ticks != reference.recovery_ticks)
    {
        return 0;
    }
    if (out_move_index != NULL)
    {
        *out_move_index = move_index;
    }
    return 1;
}

static uint8_t grab_hit_spheres(
    const struct content *content,
    float position_x_f32,
    float position_y_f32,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY])
{
    falcon_move_index move_index;

    if (!falcon_geometry_move_for_grab(
            content,
            action_state,
            &move_index))
    {
        return UINT8_C(0);
    }
    return reference_hit_spheres_at_world(
        &content->fighter,
        move_index,
        position_x_f32,
        position_y_f32,
        facing,
        action_ticks,
        out_spheres);
}

int grabbox(
    const struct content *content,
    float position_x_f32,
    float position_y_f32,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    float *out_left_f32,
    float *out_right_f32,
    float *out_top_f32,
    float *out_bottom_f32)
{
    const fighter_data *fighter;
    falcon_move_index geometry_move_index;
    hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    uint8_t sphere_count;
    const int dash_grab =
        action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB;
    const uint16_t startup_ticks =
        content == NULL
            ? UINT16_C(0)
            : (dash_grab != 0
                   ? content->fighter.dash_grab_startup_ticks
                   : content->fighter.grab_startup_ticks);
    const uint16_t active_ticks =
        content == NULL
            ? UINT16_C(0)
            : (dash_grab != 0
                   ? content->fighter.dash_grab_active_ticks
                   : content->fighter.grab_active_ticks);
    const uint16_t active_begin =
        (uint16_t)(startup_ticks + UINT16_C(1));
    const uint16_t active_end =
        (uint16_t)(startup_ticks + active_ticks);
    float center_x;
    float center_y;
    if (content == NULL ||
        out_left_f32 == NULL ||
        out_right_f32 == NULL ||
        out_top_f32 == NULL ||
        out_bottom_f32 == NULL)
    {
        return 0;
    }

    if (falcon_geometry_move_for_grab(
            content,
            action_state,
            &geometry_move_index))
    {
        sphere_count = reference_hit_spheres_at_world(
            &content->fighter,
            geometry_move_index,
            position_x_f32,
            position_y_f32,
            facing,
            action_ticks,
            spheres);
        return hit_sphere_bounds(
            spheres,
            sphere_count,
            out_left_f32,
            out_right_f32,
            out_top_f32,
            out_bottom_f32);
    }
    if (action_state != (uint8_t)PF_M4_ACTION_GRAB &&
        action_state != (uint8_t)PF_M4_ACTION_DASH_GRAB)
    {
        return 0;
    }
    if (action_ticks < active_begin || action_ticks > active_end)
    {
        return 0;
    }

    fighter = &content->fighter;
    center_x = position_x_f32 + (float)facing * fighter->grabbox_offset_x_f32;
    center_y = position_y_f32 + fighter->grabbox_offset_y_f32;
    *out_left_f32 = center_x - fighter->grabbox_half_width_f32;
    *out_right_f32 = center_x + fighter->grabbox_half_width_f32;
    *out_top_f32 = center_y - fighter->grabbox_half_height_f32;
    *out_bottom_f32 = center_y + fighter->grabbox_half_height_f32;
    return 1;
}

typedef struct shield_volume
{
    float center_x_f32;
    float center_y_f32;
    float radius_x_f32;
    float radius_y_f32;
} shield_volume;

/* Falcon's GALE01 guard-direction animation sampled at 45-degree keys. The
 * values are local TransN y and (z - 1) in float32; interpolation between keys
 * is linear in the original animation. */
static const float shield_animation_y_f32[9] = {
    0.0f, 2.5f, 4.5f, 2.5f, 0.0f, -1.0f, -1.7998046875f, -1.0f, 0.0f};
static const float shield_animation_z_f32[9] = {
    3.0f, 2.0f, 1.0f, -0.2000732421875f, -1.0f,
    -0.2000732421875f, 1.0f, 2.0f, 3.0f};

static float shield_animation_sample(
    const float values_f32[9],
    uint16_t angle_turn)
{
    const uint32_t key = (uint32_t)angle_turn >> 13U;
    const uint32_t fraction = (uint32_t)angle_turn & UINT32_C(8191);
    const float delta =
        values_f32[key + UINT32_C(1)] - values_f32[key];

    return values_f32[key] + delta * (float)fraction / 8192.0f;
}

static float scale_shield_animation_f32(
    float animation_scale_f32,
    float animation_value_f32,
    uint16_t magnitude)
{
    return animation_scale_f32 * animation_value_f32 *
           (float)magnitude / (float)UINT16_MAX;
}

static int shield_volume_for_player(
    const fighter_data *fighter,
    float position_x_f32,
    float position_y_f32,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    float shield_health_f32,
    uint16_t shield_strength,
    int8_t facing,
    uint16_t shield_angle_turn,
    uint16_t shield_magnitude,
    shield_volume *out_volume)
{
    float density_scale_f32;
    float health_scale_f32;
    float combined_scale_f32;
    float size_scale_f32;
    float animation_x_f32;
    float animation_y_f32;
    float center_forward_f32;
    float center_up_f32;

    if (fighter == NULL || out_volume == NULL ||
        shield_strength == UINT16_C(0) ||
        shield_health_f32 == 0.0f ||
        !action_has_shield_volume(
            action_state,
            hitlag_resume_action))
    {
        return 0;
    }

    density_scale_f32 =
        1.0f -
        (1.0f - fighter->dense_shield_size_scale_f32) *
            (float)shield_strength / (float)UINT16_MAX;
    health_scale_f32 = shield_health_f32 / fighter->shield_health_f32;
    combined_scale_f32 = health_scale_f32 * density_scale_f32;
    size_scale_f32 =
        fighter->shield_minimum_size_scale_f32 +
        (1.0f - fighter->shield_minimum_size_scale_f32) *
            combined_scale_f32;
    out_volume->radius_x_f32 = fighter->shield_radius_x_f32 * size_scale_f32;
    out_volume->radius_y_f32 = fighter->shield_radius_y_f32 * size_scale_f32;
    animation_x_f32 = scale_shield_animation_f32(
        fighter->shield_animation_scale_x_f32,
        shield_animation_sample(
            shield_animation_z_f32,
            shield_angle_turn),
        shield_magnitude);
    animation_y_f32 = scale_shield_animation_f32(
        fighter->shield_animation_scale_y_f32,
        shield_animation_sample(
            shield_animation_y_f32,
            shield_angle_turn),
        shield_magnitude);
    center_forward_f32 =
        fighter->shield_center_forward_f32 + animation_x_f32;
    center_up_f32 = fighter->shield_center_up_f32 + animation_y_f32;
    out_volume->center_x_f32 =
        position_x_f32 + (int32_t)facing * center_forward_f32;
    out_volume->center_y_f32 = position_y_f32 - center_up_f32;
    return 1;
}

int shield_box(
    const fighter_data *fighter,
    float position_x_f32,
    float position_y_f32,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    float shield_health_f32,
    uint16_t shield_strength,
    int8_t facing,
    uint16_t shield_angle_turn,
    uint16_t shield_magnitude,
    float *out_left_f32,
    float *out_right_f32,
    float *out_top_f32,
    float *out_bottom_f32)
{
    shield_volume volume;

    if (out_left_f32 == NULL || out_right_f32 == NULL ||
        out_top_f32 == NULL || out_bottom_f32 == NULL ||
        !shield_volume_for_player(
            fighter,
            position_x_f32,
            position_y_f32,
            action_state,
            hitlag_resume_action,
            shield_health_f32,
            shield_strength,
            facing,
            shield_angle_turn,
            shield_magnitude,
            &volume))
    {
        return 0;
    }
    *out_left_f32 = volume.center_x_f32 - volume.radius_x_f32;
    *out_right_f32 = volume.center_x_f32 + volume.radius_x_f32;
    *out_top_f32 = volume.center_y_f32 - volume.radius_y_f32;
    *out_bottom_f32 = volume.center_y_f32 + volume.radius_y_f32;
    return 1;
}

typedef collision_capsule3_f32 world_hurt_capsule;

static inline int8_t reference_hurt_pose_facing(
    const fighter_data *fighter,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t action_ticks,
    int8_t gameplay_facing,
    int8_t dash_direction)
{
    const uint8_t effective_action =
        action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
                hitlag_resume_action != UINT8_C(0)
            ? hitlag_resume_action
            : action_state;

    /* TurnRun's animation callback flips gameplay facing when the frozen
     * source frame 9 resumes.  The display bones used by collision retain
     * their preceding facing until the next animation step.  This tuple is
     * unique in the production state machine, so preserve the source phase
     * without another rollback/snapshot field. */
    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        effective_action == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        action_ticks == UINT16_C(10) &&
        dash_direction != INT8_C(0) &&
        gameplay_facing == dash_direction)
    {
        return (int8_t)-gameplay_facing;
    }
    return gameplay_facing;
}

static inline uint16_t reference_common_hurt_frame(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks)
{
    /* Dash, RunBrake, and squat transitions enter the movement scratch at
     * source frame 1.  The remaining imported common motions enter at tick 0,
     * so their source frame is one-based relative to the stored tick. */
    if (action_ticks == UINT16_MAX)
    {
        return UINT16_MAX;
    }
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_SHIELD:
            return source_submotion ==
                           (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON
                       ? action_ticks
                       : UINT16_C(0);
        case PF_M4_ACTION_SHIELD_RELEASE:
            return action_ticks;
        case PF_M4_ACTION_RUN_TURNAROUND:
            return action_ticks == UINT16_C(0)
                       ? UINT16_C(0)
                       : (uint16_t)(action_ticks - UINT16_C(1));
        case PF_M4_ACTION_INITIAL_DASH:
        case PF_M4_ACTION_RUN_BRAKE:
        case PF_M4_ACTION_CROUCH_START:
        case PF_M4_ACTION_CROUCH_END:
            return action_ticks;
        default:
            return (uint16_t)(action_ticks + UINT16_C(1));
    }
}

static const reference_hurt_capsule *reference_hurt_pose(
    const fighter_data *fighter,
    uint8_t grounded,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    uint16_t action_ticks,
    int8_t facing,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    const hsd_local_pose *ground_loop_pose,
    const hsd_compact_pose *ground_loop_compact,
    float ground_loop_progress_f32,
    reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    falcon_move_index move_index;

    if (out_count != NULL)
    {
        *out_count = UINT8_C(0);
    }
    if (fighter->reference_frame_data_enabled == UINT8_C(0))
    {
        return NULL;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
        hitlag_resume_action != UINT8_C(0))
    {
        action_state = hitlag_resume_action;
    }
    if (grounded != UINT8_C(0) &&
        (action_state == (uint8_t)PF_M4_ACTION_WALK ||
         action_state == (uint8_t)PF_M4_ACTION_RUN ||
         (action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
          (ground_loop_pose != NULL || ground_loop_compact != NULL)) ||
         (action_state == (uint8_t)PF_M4_ACTION_SHIELD &&
          source_submotion ==
              (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON &&
          (ground_loop_pose != NULL || ground_loop_compact != NULL))))
    {
        int evaluated = 0;

        if (dynamic_capsules != NULL && ground_loop_pose != NULL)
        {
            evaluated =
                falcon_reference_hsd_hurt_capsules_from_local_pose(
                    ground_loop_pose, dynamic_capsules, out_count);
        }
        else if (dynamic_capsules != NULL && ground_loop_compact != NULL)
        {
            const hsd_pose_data *data =
                falcon_reference_hsd_pose_data();
            hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];

            evaluated = data != NULL &&
                falcon_resolve_compact_hsd_pose(
                    source_submotion, source_animation_frame_f32,
                    ground_loop_progress_f32, ground_loop_compact, pose) &&
                falcon_reference_hsd_hurt_capsules_from_local_pose(
                    pose, dynamic_capsules, out_count);
        }
        else if (dynamic_capsules != NULL)
        {
            evaluated = falcon_reference_hsd_hurt_capsules(
                source_submotion,
                source_animation_frame_f32,
                dynamic_capsules,
                out_count);
        }
        return evaluated != 0 ? dynamic_capsules : NULL;
    }
    if (grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        if (dynamic_capsules != NULL &&
            falcon_wait_hsd_pose_is_direct(
                source_submotion,
                source_animation_frame_f32) &&
            falcon_reference_hsd_hurt_capsules(
                source_submotion,
                source_animation_frame_f32,
                dynamic_capsules,
                out_count))
        {
            return dynamic_capsules;
        }
        return falcon_reference_standing_hurt_capsules(out_count);
    }
    if (dynamic_capsules != NULL &&
        source_submotion >=
            (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_HIGH_1 &&
        source_submotion <=
            (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL &&
        falcon_reference_damage_hsd_hurt_capsules(
            source_submotion,
            source_animation_frame_f32,
            facing,
            total_velocity_x_f32,
            total_velocity_y_f32,
            dynamic_capsules,
            out_count))
    {
        return dynamic_capsules;
    }
    if (dynamic_capsules != NULL &&
        falcon_reference_retained_hsd_hurt_capsules(
            action_state,
            source_submotion,
            action_ticks,
            source_animation_frame_f32,
            dynamic_capsules,
            out_count))
    {
        return dynamic_capsules;
    }
    {
        const reference_hurt_capsule *common_pose =
            falcon_reference_common_hurt_capsules_for_submotion_at_frame(
                action_state,
                source_submotion,
                reference_common_hurt_frame(
                    action_state,
                    source_submotion,
                    action_ticks),
                out_count);

        if (common_pose != NULL)
        {
            return common_pose;
        }
    }
    if (!falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        switch ((enum action_state)action_state)
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
                return NULL;
        }
    }
    return falcon_reference_hurt_capsules_at_frame(
        move_index,
        reference_move_action_frame(move_index, action_ticks),
        out_count);
}

static world_hurt_capsule world_hurt_capsule_from_reference(
    const fighter_data *fighter,
    float position_x_f32,
    float position_y_f32,
    int8_t facing_value,
    const reference_hurt_capsule *source)
{
    const float position_x = position_x_f32;
    const float position_y = reference_origin_y_f32(fighter, position_y_f32);
    const float facing = (float)facing_value;
    const world_hurt_capsule world = {
        position_x + facing * source->endpoint_a_x_f32,
        position_y + source->endpoint_a_y_f32,
        facing * source->endpoint_a_z_f32,
        position_x + facing * source->endpoint_b_x_f32,
        position_y + source->endpoint_b_y_f32,
        facing * source->endpoint_b_z_f32,
        source->radius_f32};

    return world;
}

uint8_t reference_world_hurt_capsules(
    const fighter_data *fighter,
    float position_x_f32,
    float position_y_f32,
    int8_t facing,
    int8_t dash_direction,
    uint8_t grounded,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    uint16_t action_ticks,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    const hsd_local_pose *ground_loop_pose,
    hurt_capsule_inspection
        out_capsules[PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY])
{
    uint8_t capsule_count;
    reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const reference_hurt_capsule *source_capsules;
    uint8_t capsule_index;
    int8_t pose_facing;

    if (fighter == NULL || out_capsules == NULL)
    {
        return UINT8_C(0);
    }
    source_capsules = reference_hurt_pose(
        fighter,
        grounded,
        action_state,
        hitlag_resume_action,
        source_submotion,
        source_animation_frame_f32,
        action_ticks,
        facing,
        total_velocity_x_f32,
        total_velocity_y_f32,
        ground_loop_pose,
        NULL,
        INT32_C(0),
        dynamic_capsules,
        &capsule_count);
    if (source_capsules == NULL ||
        capsule_count > UINT8_C(PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY))
    {
        return UINT8_C(0);
    }
    pose_facing = reference_hurt_pose_facing(
        fighter,
        action_state,
        hitlag_resume_action,
        action_ticks,
        facing,
        dash_direction);

    for (capsule_index = UINT8_C(0);
         capsule_index < capsule_count;
         ++capsule_index)
    {
        const reference_hurt_capsule *source =
            &source_capsules[capsule_index];
        const world_hurt_capsule world =
            world_hurt_capsule_from_reference(
                fighter,
                position_x_f32,
                position_y_f32,
                pose_facing,
                source);
        hurt_capsule_inspection *destination =
            &out_capsules[capsule_index];

        if (!isfinite(world.endpoint_a_x_f32) ||
            !isfinite(world.endpoint_a_y_f32) ||
            !isfinite(world.endpoint_a_z_f32) ||
            !isfinite(world.endpoint_b_x_f32) ||
            !isfinite(world.endpoint_b_y_f32) ||
            !isfinite(world.endpoint_b_z_f32) ||
            !isfinite(world.radius_f32))
        {
            return UINT8_C(0);
        }
        destination->endpoint_a_x_f32 =
            world.endpoint_a_x_f32;
        destination->endpoint_a_y_f32 =
            world.endpoint_a_y_f32;
        destination->endpoint_a_z_f32 =
            world.endpoint_a_z_f32;
        destination->endpoint_b_x_f32 =
            world.endpoint_b_x_f32;
        destination->endpoint_b_y_f32 =
            world.endpoint_b_y_f32;
        destination->endpoint_b_z_f32 =
            world.endpoint_b_z_f32;
        destination->radius_f32 = world.radius_f32;
        destination->hurtbox_id = source->hurtbox_id;
        destination->height = source->height;
        destination->grabbable = source->grabbable;
        destination->reserved = UINT8_C(0);
    }
    return capsule_count;
}

static int hitbox_overlaps_player(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    float hitbox_left_f32,
    float hitbox_right_f32,
    float hitbox_top_f32,
    float hitbox_bottom_f32)
{
    uint8_t capsule_count;
    reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const reference_hurt_capsule *capsules =
        reference_hurt_pose(
            fighter,
            scratch->grounded[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_f32[target_index],
            scratch->action_ticks[target_index],
            scratch->facing[target_index],
            total_velocity_f32(
                scratch->velocity_x_f32[target_index],
                scratch->knockback_velocity_x_f32[target_index]),
            total_velocity_f32(
                scratch->velocity_y_f32[target_index],
                scratch->knockback_velocity_y_f32[target_index]),
            NULL,
            scratch->ground_blend_progress_f32[target_index] > 0.0f
                ? &scratch->ground_blend_pose[target_index]
                : NULL,
            scratch->ground_blend_progress_f32[target_index],
            dynamic_capsules,
            &capsule_count);

    if (capsules != NULL)
    {
        const int8_t pose_facing = reference_hurt_pose_facing(
            fighter,
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->action_ticks[target_index],
            scratch->facing[target_index],
            scratch->dash_direction[target_index]);
        uint8_t capsule_index;

        for (capsule_index = UINT8_C(0);
             capsule_index < capsule_count;
             ++capsule_index)
        {
            const world_hurt_capsule capsule =
                world_hurt_capsule_from_reference(
                    fighter,
                    scratch->position_x_f32[target_index],
                    scratch->position_y_f32[target_index],
                    pose_facing,
                    &capsules[capsule_index]);
            const float capsule_left =
                (capsule.endpoint_a_x_f32 < capsule.endpoint_b_x_f32
                     ? capsule.endpoint_a_x_f32
                     : capsule.endpoint_b_x_f32) -
                capsule.radius_f32;
            const float capsule_right =
                (capsule.endpoint_a_x_f32 > capsule.endpoint_b_x_f32
                     ? capsule.endpoint_a_x_f32
                     : capsule.endpoint_b_x_f32) +
                capsule.radius_f32;
            const float capsule_top =
                (capsule.endpoint_a_y_f32 < capsule.endpoint_b_y_f32
                     ? capsule.endpoint_a_y_f32
                     : capsule.endpoint_b_y_f32) -
                capsule.radius_f32;
            const float capsule_bottom =
                (capsule.endpoint_a_y_f32 > capsule.endpoint_b_y_f32
                     ? capsule.endpoint_a_y_f32
                     : capsule.endpoint_b_y_f32) +
                capsule.radius_f32;

            if (hitbox_left_f32 <= capsule_right &&
                hitbox_right_f32 >= capsule_left &&
                hitbox_top_f32 <= capsule_bottom &&
                hitbox_bottom_f32 >= capsule_top)
            {
                return 1;
            }
        }
        return 0;
    }
    const float hurtbox_left =
        scratch->position_x_f32[target_index] -
        fighter->half_width_f32;
    const float hurtbox_right =
        scratch->position_x_f32[target_index] +
        fighter->half_width_f32;
    const float hurtbox_top =
        scratch->position_y_f32[target_index] -
        fighter->half_height_f32;
    const float hurtbox_bottom =
        scratch->position_y_f32[target_index] +
        fighter->half_height_f32;

    return hitbox_left_f32 <= hurtbox_right &&
           hitbox_right_f32 >= hurtbox_left &&
           hitbox_top_f32 <= hurtbox_bottom &&
           hitbox_bottom_f32 >= hurtbox_top;
}

static int hitbox_overlaps_shield(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    float hitbox_left_f32,
    float hitbox_right_f32,
    float hitbox_top_f32,
    float hitbox_bottom_f32)
{
    shield_volume volume;
    float nearest_x_f32;
    float nearest_y_f32;
    float normalized_x_f32;
    float normalized_y_f32;

    if (!shield_volume_for_player(
            fighter,
            scratch->position_x_f32[target_index],
            scratch->position_y_f32[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->shield_health_f32[target_index],
            scratch->shield_strength[target_index],
            scratch->facing[target_index],
            scratch->shield_angle_turn[target_index],
            scratch->shield_magnitude[target_index],
            &volume))
    {
        return 0;
    }
    nearest_x_f32 = volume.center_x_f32 < hitbox_left_f32
                        ? hitbox_left_f32
                        : volume.center_x_f32 > hitbox_right_f32
                              ? hitbox_right_f32
                              : volume.center_x_f32;
    nearest_y_f32 = volume.center_y_f32 < hitbox_top_f32
                        ? hitbox_top_f32
                        : volume.center_y_f32 > hitbox_bottom_f32
                              ? hitbox_bottom_f32
                              : volume.center_y_f32;
    normalized_x_f32 =
        (nearest_x_f32 - volume.center_x_f32) / volume.radius_x_f32;
    normalized_y_f32 =
        (nearest_y_f32 - volume.center_y_f32) / volume.radius_y_f32;
    return normalized_x_f32 * normalized_x_f32 +
               normalized_y_f32 * normalized_y_f32 <=
           1.0f;
}

static int hitbox_overlaps_player_or_shield(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    float hitbox_left_f32,
    float hitbox_right_f32,
    float hitbox_top_f32,
    float hitbox_bottom_f32)
{
    return hitbox_overlaps_player(
               fighter,
               scratch,
               target_index,
               hitbox_left_f32,
               hitbox_right_f32,
               hitbox_top_f32,
               hitbox_bottom_f32) ||
           hitbox_overlaps_shield(
               fighter,
               scratch,
               target_index,
               hitbox_left_f32,
               hitbox_right_f32,
               hitbox_top_f32,
               hitbox_bottom_f32);
}

static int hit_sphere_overlaps_reference_pose(
    const fighter_data *fighter,
    float target_position_x_f32,
    float target_position_y_f32,
    uint8_t target_action_state,
    uint8_t target_hitlag_resume_action,
    uint16_t target_action_ticks,
    int8_t target_facing,
    int8_t target_dash_direction,
    float previous_attacker_position_y_f32,
    float attacker_position_y_f32,
    const hit_sphere_inspection *previous_sphere,
    const hit_sphere_inspection *sphere,
    const reference_hurt_capsule *capsules,
    uint8_t capsule_count,
    int grabbable_only,
    uint8_t *out_hurtbox_height)
{
    const float target_origin_y =
        reference_origin_y_f32(fighter, target_position_y_f32);
    const float previous_attacker_origin_y =
        reference_origin_y_f32(fighter, previous_attacker_position_y_f32);
    const float attacker_origin_y =
        reference_origin_y_f32(fighter, attacker_position_y_f32);
    const float previous_center_y =
        target_origin_y +
        (previous_attacker_origin_y - target_origin_y) *
            fighter->shield_radius_x_f32 /
            fighter->shield_radius_y_f32 +
        (previous_sphere->center_y_f32 -
         previous_attacker_origin_y);
    const float center_y =
        target_origin_y +
        (attacker_origin_y - target_origin_y) *
            fighter->shield_radius_x_f32 /
            fighter->shield_radius_y_f32 +
        (sphere->center_y_f32 - attacker_origin_y);
    const int8_t pose_facing = reference_hurt_pose_facing(
        fighter,
        target_action_state,
        target_hitlag_resume_action,
        target_action_ticks,
        target_facing,
        target_dash_direction);
    uint8_t capsule_index;

    for (capsule_index = UINT8_C(0);
         capsule_index < capsule_count;
         ++capsule_index)
    {
        const world_hurt_capsule capsule =
            world_hurt_capsule_from_reference(
                fighter,
                target_position_x_f32,
                target_position_y_f32,
                pose_facing,
                &capsules[capsule_index]);
        const collision_capsule3_f32 collision_hit = {
            previous_sphere->center_x_f32,
            previous_center_y,
            previous_sphere->center_z_f32,
            sphere->center_x_f32,
            center_y,
            sphere->center_z_f32,
            sphere->radius_f32};

        if (grabbable_only != 0 &&
            capsules[capsule_index].grabbable == UINT8_C(0))
        {
            continue;
        }
        if (collision_capsule_capsule_overlap_f32(
                &collision_hit,
                &capsule))
        {
            if (out_hurtbox_height != NULL)
            {
                *out_hurtbox_height = capsules[capsule_index].height;
            }
            return 1;
        }
    }
    return 0;
}

static int hit_sphere_overlaps_player_with_height(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    float previous_attacker_position_y_f32,
    const hit_sphere_inspection *previous_sphere,
    const hit_sphere_inspection *sphere,
    uint8_t *out_hurtbox_height)
{
    uint8_t capsule_count;
    reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const reference_hurt_capsule *capsules =
        reference_hurt_pose(
            fighter,
            scratch->grounded[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_f32[target_index],
            scratch->action_ticks[target_index],
            scratch->facing[target_index],
            total_velocity_f32(
                scratch->velocity_x_f32[target_index],
                scratch->knockback_velocity_x_f32[target_index]),
            total_velocity_f32(
                scratch->velocity_y_f32[target_index],
                scratch->knockback_velocity_y_f32[target_index]),
            NULL,
            scratch->ground_blend_progress_f32[target_index] > INT32_C(0)
                ? &scratch->ground_blend_pose[target_index]
                : NULL,
            scratch->ground_blend_progress_f32[target_index],
            dynamic_capsules,
            &capsule_count);

    if (capsules != NULL)
    {
        return hit_sphere_overlaps_reference_pose(
            fighter,
            scratch->position_x_f32[target_index],
            scratch->position_y_f32[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->action_ticks[target_index],
            scratch->facing[target_index],
            scratch->dash_direction[target_index],
            previous_attacker_position_y_f32,
            scratch->position_y_f32[attacker_index],
            previous_sphere,
            sphere,
            capsules,
            capsule_count,
            0,
            out_hurtbox_height);
    }
    const float hurtbox_left =
        scratch->position_x_f32[target_index] -
        fighter->half_width_f32;
    const float hurtbox_right =
        scratch->position_x_f32[target_index] +
        fighter->half_width_f32;
    const float hurtbox_top =
        scratch->position_y_f32[target_index] -
        fighter->half_height_f32;
    const float hurtbox_bottom =
        scratch->position_y_f32[target_index] +
        fighter->half_height_f32;
    const float nearest_x =
        sphere->center_x_f32 < hurtbox_left
            ? hurtbox_left
            : sphere->center_x_f32 > hurtbox_right
                  ? hurtbox_right
                  : sphere->center_x_f32;
    const float nearest_y =
        sphere->center_y_f32 < hurtbox_top
            ? hurtbox_top
            : sphere->center_y_f32 > hurtbox_bottom
                  ? hurtbox_bottom
                  : sphere->center_y_f32;
    const float delta_x = sphere->center_x_f32 - nearest_x;
    const float delta_y = sphere->center_y_f32 - nearest_y;

    if (delta_x * delta_x + delta_y * delta_y <=
        sphere->radius_f32 * sphere->radius_f32)
    {
        if (out_hurtbox_height != NULL)
        {
            *out_hurtbox_height = UINT8_C(1);
        }
        return 1;
    }
    return 0;
}

static int hit_sphere_overlaps_player(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    float previous_attacker_position_y_f32,
    const hit_sphere_inspection *previous_sphere,
    const hit_sphere_inspection *sphere)
{
    return hit_sphere_overlaps_player_with_height(
        fighter,
        scratch,
        attacker_index,
        target_index,
        previous_attacker_position_y_f32,
        previous_sphere,
        sphere,
        NULL);
}

static int grab_sphere_overlaps_player(
    const fighter_data *fighter,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    float previous_attacker_position_y_f32,
    const hit_sphere_inspection *previous_sphere,
    const hit_sphere_inspection *sphere,
    int target_before_step)
{
    const float target_position_x_f32 = target_before_step != 0
        ? world->position_x_f32[target_index]
        : scratch->position_x_f32[target_index];
    const float target_position_y_f32 = target_before_step != 0
        ? world->position_y_f32[target_index]
        : scratch->position_y_f32[target_index];
    const uint8_t target_grounded = target_before_step != 0
        ? world->grounded[target_index]
        : scratch->grounded[target_index];
    const uint8_t target_action_state = target_before_step != 0
        ? world->action_state[target_index]
        : scratch->action_state[target_index];
    const uint8_t target_hitlag_resume_action = target_before_step != 0
        ? world->hitlag_resume_action[target_index]
        : scratch->hitlag_resume_action[target_index];
    const uint16_t target_source_submotion = target_before_step != 0
        ? world->source_submotion[target_index]
        : scratch->source_submotion[target_index];
    const float target_source_frame_f32 = target_before_step != 0
        ? world->source_animation_frame_f32[target_index]
        : scratch->source_animation_frame_f32[target_index];
    const uint16_t target_action_ticks = target_before_step != 0
        ? world->action_ticks[target_index]
        : scratch->action_ticks[target_index];
    const int8_t target_facing = target_before_step != 0
        ? world->facing[target_index]
        : scratch->facing[target_index];
    const int8_t target_dash_direction = target_before_step != 0
        ? world->dash_direction[target_index]
        : scratch->dash_direction[target_index];
    const float target_velocity_x_f32 = target_before_step != 0
        ? total_velocity_f32(
              world->velocity_x_f32[target_index],
              world->knockback_velocity_x_f32[target_index])
        : total_velocity_f32(
              scratch->velocity_x_f32[target_index],
              scratch->knockback_velocity_x_f32[target_index]);
    const float target_velocity_y_f32 = target_before_step != 0
        ? total_velocity_f32(
              world->velocity_y_f32[target_index],
              world->knockback_velocity_y_f32[target_index])
        : total_velocity_f32(
              scratch->velocity_y_f32[target_index],
              scratch->knockback_velocity_y_f32[target_index]);
    const float target_blend_progress_f32 = target_before_step != 0
        ? world->ground_blend_progress_f32[target_index]
        : scratch->ground_blend_progress_f32[target_index];
    const hsd_compact_pose *target_blend_pose =
        target_blend_progress_f32 > INT32_C(0)
            ? (target_before_step != 0
                   ? &world->ground_blend_pose[target_index]
                   : &scratch->ground_blend_pose[target_index])
            : NULL;
    uint8_t capsule_count;
    reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const reference_hurt_capsule *capsules =
        reference_hurt_pose(
            fighter,
            target_grounded,
            target_action_state,
            target_hitlag_resume_action,
            target_source_submotion,
            target_source_frame_f32,
            target_action_ticks,
            target_facing,
            target_velocity_x_f32,
            target_velocity_y_f32,
            NULL,
            target_blend_pose,
            target_blend_progress_f32,
            dynamic_capsules,
            &capsule_count);

    if (capsules != NULL)
    {
        return hit_sphere_overlaps_reference_pose(
            fighter,
            target_position_x_f32,
            target_position_y_f32,
            target_action_state,
            target_hitlag_resume_action,
            target_action_ticks,
            target_facing,
            target_dash_direction,
            previous_attacker_position_y_f32,
            scratch->position_y_f32[attacker_index],
            previous_sphere,
            sphere,
            capsules,
            capsule_count,
            1,
            NULL);
    }
    {
        const float hurtbox_left =
            target_position_x_f32 - fighter->half_width_f32;
        const float hurtbox_right =
            target_position_x_f32 + fighter->half_width_f32;
        const float hurtbox_top =
            target_position_y_f32 - fighter->half_height_f32;
        const float hurtbox_bottom =
            target_position_y_f32 + fighter->half_height_f32;
        const float nearest_x = sphere->center_x_f32 < hurtbox_left
            ? hurtbox_left
            : sphere->center_x_f32 > hurtbox_right
                  ? hurtbox_right
                  : sphere->center_x_f32;
        const float nearest_y = sphere->center_y_f32 < hurtbox_top
            ? hurtbox_top
            : sphere->center_y_f32 > hurtbox_bottom
                  ? hurtbox_bottom
                  : sphere->center_y_f32;
        const float delta_x = sphere->center_x_f32 - nearest_x;
        const float delta_y = sphere->center_y_f32 - nearest_y;

        (void)attacker_index;
        (void)previous_attacker_position_y_f32;
        (void)previous_sphere;
        return delta_x * delta_x + delta_y * delta_y <=
               sphere->radius_f32 * sphere->radius_f32;
    }
}

static int hit_sphere_overlaps_shield(
    const fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    float previous_attacker_position_y_f32,
    const hit_sphere_inspection *previous_sphere,
    const hit_sphere_inspection *sphere)
{
    shield_volume volume;
    const float previous_attacker_reference_origin_y =
        reference_origin_y_f32(
            fighter,
            previous_attacker_position_y_f32);
    const float attacker_reference_origin_y =
        reference_origin_y_f32(
            fighter,
            scratch->position_y_f32[attacker_index]);
    float previous_center_y;
    float current_center_y;

    if (!shield_volume_for_player(
            fighter,
            scratch->position_x_f32[target_index],
            scratch->position_y_f32[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->shield_health_f32[target_index],
            scratch->shield_strength[target_index],
            scratch->facing[target_index],
            scratch->shield_angle_turn[target_index],
            scratch->shield_magnitude[target_index],
            &volume))
    {
        return 0;
    }

    /* The executable tests two spheres in Melee's uniform spatial metric.
     * World Y uses the project's independent movement scale, while imported
     * hit-sphere offsets/radii already use the spatial X scale. Convert only
     * each fighter origin and the shield joint back into that metric; keeping
     * the hit offset separate avoids scaling it twice. */
    previous_center_y =
        volume.center_y_f32 +
        (previous_attacker_reference_origin_y -
         volume.center_y_f32) *
            fighter->shield_radius_x_f32 /
            fighter->shield_radius_y_f32 +
        (previous_sphere->center_y_f32 -
         previous_attacker_reference_origin_y);
    current_center_y =
        volume.center_y_f32 +
        (attacker_reference_origin_y -
         volume.center_y_f32) *
            fighter->shield_radius_x_f32 /
            fighter->shield_radius_y_f32 +
        (sphere->center_y_f32 -
         attacker_reference_origin_y);
    {
        const collision_capsule3_f32 hit = {
            previous_sphere->center_x_f32,
            previous_center_y,
            previous_sphere->center_z_f32,
            sphere->center_x_f32,
            current_center_y,
            sphere->center_z_f32,
            sphere->radius_f32};
        const collision_capsule3_f32 shield = {
            volume.center_x_f32,
            volume.center_y_f32,
            0.0f,
            volume.center_x_f32,
            volume.center_y_f32,
            0.0f,
            volume.radius_x_f32};

        return collision_capsule_capsule_overlap_f32(
            &hit,
            &shield);
    }
}

static const hit_sphere_inspection *
previous_matching_hit_sphere(
    const hit_sphere_inspection *sphere,
    const hit_sphere_inspection
        previous_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t previous_sphere_count)
{
    uint8_t previous_index;

    if (sphere->collision_state != UINT8_C(3))
    {
        return sphere;
    }
    for (previous_index = UINT8_C(0);
         previous_index < previous_sphere_count;
         ++previous_index)
    {
        if (previous_spheres[previous_index].hitbox_id ==
            sphere->hitbox_id)
        {
            return &previous_spheres[previous_index];
        }
    }
    return sphere;
}

static int reference_hit_sphere_can_continue(
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index)
{
    const uint16_t previous_tick = world->action_ticks[attacker_index];
    const uint16_t current_tick = scratch->action_ticks[attacker_index];

    return world->action_state[attacker_index] ==
               scratch->action_state[attacker_index] &&
           current_tick >= previous_tick &&
           (uint16_t)(current_tick - previous_tick) <= UINT16_C(1);
}

static int boxes_overlap(
    float left_a_f32,
    float right_a_f32,
    float top_a_f32,
    float bottom_a_f32,
    float left_b_f32,
    float right_b_f32,
    float top_b_f32,
    float bottom_b_f32)
{
    return left_a_f32 <= right_b_f32 &&
           right_a_f32 >= left_b_f32 &&
           top_a_f32 <= bottom_b_f32 &&
           bottom_a_f32 >= top_b_f32;
}

static int action_is_throw(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_THROW_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_BACK ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_UP ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_DOWN;
}

static const struct throw_data *throw_for_action(
    const fighter_data *fighter,
    uint8_t action_state)
{
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_FORWARD)
    {
        return &fighter->forward_throw;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_BACK)
    {
        return &fighter->back_throw;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_UP)
    {
        return &fighter->up_throw;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_DOWN)
    {
        return &fighter->down_throw;
    }
    return NULL;
}

static float scaled_throw_velocity(
    float base_f32,
    float growth_f32,
    float damage_f32)
{
    float result = base_f32 + growth_f32 * damage_f32;

    if (result > PF_SIM_MAX_MOTION_SPEED_F32)
    {
        result = PF_SIM_MAX_MOTION_SPEED_F32;
    }
    else if (result < -PF_SIM_MAX_MOTION_SPEED_F32)
    {
        result = -PF_SIM_MAX_MOTION_SPEED_F32;
    }
    return result;
}

static void clear_grab_links(
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index)
{
    scratch->grab_target_slot[holder_index] = UINT8_C(0);
    scratch->grab_owner_slot[target_index] = UINT8_C(0);
    scratch->grab_escape_ticks[target_index] = UINT16_C(0);
    scratch->mash_stick_x_direction[target_index] = INT8_C(0);
    scratch->mash_stick_y_direction[target_index] = INT8_C(0);
}

static void release_grab(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index)
{
    const int throw_edge_drop =
        scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_AIRBORNE &&
        action_is_throw(world->action_state[holder_index]);

    clear_grab_links(scratch, holder_index, target_index);
    if (scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        action_is_throw(scratch->action_state[holder_index]))
    {
        set_action_state(
            world,
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_GRAB_RELEASE);
        scratch->action_ticks[holder_index] = UINT16_C(0);
        scratch->source_submotion[holder_index] =
            (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT;
        scratch->attack_stale_registered[holder_index] = UINT8_C(0);
    }
    if (scratch->action_state[target_index] ==
        (uint8_t)PF_M4_ACTION_GRABBED)
    {
        if (throw_edge_drop != 0)
        {
            /* Throw_Coll's grounded edge callback calls ftCo_800DC920 and
             * then enters Fall for both fighters. It does not run
             * CaptureCut and therefore applies no grab-release impulse. */
            set_action_state(
                world,
                scratch,
                target_index,
                (uint8_t)PF_M4_ACTION_AIRBORNE);
            scratch->action_ticks[target_index] = UINT16_C(0);
            scratch->source_submotion[target_index] =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            scratch->grounded[target_index] = UINT8_C(0);
            scratch->support[target_index] =
                (uint8_t)PF_M4_SURFACE_NONE;
        }
        else
        {
            set_action_state(
                world,
                scratch,
                target_index,
                (uint8_t)PF_M4_ACTION_GRAB_RELEASE);
            scratch->action_ticks[target_index] = UINT16_C(0);
            scratch->source_submotion[target_index] =
                (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_CUT;
        }
    }
}

static pf_status break_player_grab_links(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const uint8_t owner_slot = scratch->grab_owner_slot[player_index];
    const uint8_t target_slot = scratch->grab_target_slot[player_index];

    if (owner_slot != UINT8_C(0))
    {
        const uint32_t owner_index =
            (uint32_t)owner_slot - UINT32_C(1);

        if (owner_index >= (uint32_t)world->player_count ||
            scratch->grab_target_slot[owner_index] !=
                (uint8_t)(player_index + UINT32_C(1)))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        release_grab(
            world,
            scratch,
            owner_index,
            player_index);
    }
    if (target_slot != UINT8_C(0))
    {
        const uint32_t target_index =
            (uint32_t)target_slot - UINT32_C(1);

        if (target_index >= (uint32_t)world->player_count ||
            scratch->grab_owner_slot[target_index] !=
                (uint8_t)(player_index + UINT32_C(1)))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        release_grab(
            world,
            scratch,
            player_index,
            target_index);
    }
    return PF_STATUS_OK;
}

static uint32_t stock_subscore(
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index)
{
    uint32_t fewer_kos = UINT32_C(0);
    uint32_t more_falls = UINT32_C(0);
    uint32_t other_index;

    for (other_index = UINT32_C(0);
         other_index < (uint32_t)world->player_count;
         ++other_index)
    {
        if (scratch->match_kos[player_index] >
            scratch->match_kos[other_index])
        {
            ++fewer_kos;
        }
        if (scratch->match_falls[player_index] <
            scratch->match_falls[other_index])
        {
            ++more_falls;
        }
    }
    /* fn_801656A8 packs KO rank, fall rank, then the six-slot port tiebreak. */
    return ((fewer_kos * UINT32_C(16) + more_falls) * UINT32_C(16)) +
           (UINT32_C(6) - player_index);
}

static int player_stock_standing_is_better(
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t left_index,
    uint32_t right_index)
{
    if (scratch->stocks_remaining[left_index] !=
        scratch->stocks_remaining[right_index])
    {
        return scratch->stocks_remaining[left_index] >
               scratch->stocks_remaining[right_index];
    }
    return stock_subscore(world, scratch, left_index) >
           stock_subscore(world, scratch, right_index);
}

static uint32_t capture_rank(
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index)
{
    uint32_t rank = UINT32_C(0);
    uint32_t other_index;

    if (world->mode != (uint8_t)PF_SIM_MODE_TEAMS)
    {
        for (other_index = UINT32_C(0);
             other_index < (uint32_t)world->player_count;
             ++other_index)
        {
            if (other_index != player_index &&
                player_stock_standing_is_better(
                    world,
                    scratch,
                    other_index,
                    player_index))
            {
                ++rank;
            }
        }
        return rank;
    }

    {
        const uint8_t player_team = world->team[player_index];
        uint32_t player_team_score = UINT32_C(0);
        uint32_t player_team_subscore = UINT32_C(0);

        for (other_index = UINT32_C(0);
             other_index < (uint32_t)world->player_count;
             ++other_index)
        {
            if (world->team[other_index] == player_team)
            {
                player_team_score +=
                    (uint32_t)scratch->stocks_remaining[other_index];
                player_team_subscore +=
                    stock_subscore(world, scratch, other_index);
            }
        }
        for (other_index = UINT32_C(0);
             other_index < (uint32_t)world->player_count;
             ++other_index)
        {
            const uint8_t other_team = world->team[other_index];
            uint32_t other_team_score = UINT32_C(0);
            uint32_t other_team_subscore = UINT32_C(0);
            uint32_t member_index;
            uint32_t preceding_index;
            int already_counted = 0;

            if (other_team == player_team)
            {
                continue;
            }
            for (preceding_index = UINT32_C(0);
                 preceding_index < other_index;
                 ++preceding_index)
            {
                if (world->team[preceding_index] == other_team)
                {
                    already_counted = 1;
                    break;
                }
            }
            if (already_counted != 0)
            {
                continue;
            }
            for (member_index = UINT32_C(0);
                 member_index < (uint32_t)world->player_count;
                 ++member_index)
            {
                if (world->team[member_index] == other_team)
                {
                    other_team_score +=
                        (uint32_t)scratch->stocks_remaining[member_index];
                    other_team_subscore +=
                        stock_subscore(
                            world,
                            scratch,
                            member_index);
                }
            }
            if (other_team_score > player_team_score ||
                (other_team_score == player_team_score &&
                 other_team_subscore > player_team_subscore))
            {
                ++rank;
            }
        }
    }
    return rank;
}

static uint16_t grab_escape_ticks(
    const struct content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const fighter_data *fighter = &content->fighter;
    const float damage_f32 = scratch->damage_f32[player_index];

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const ssbm_mash_attributes *mash =
            ssbm_common_reference_mash();
        const uint32_t rank =
            capture_rank(world, scratch, player_index);
        const uint32_t rank_slot = rank + UINT32_C(1);
        const float damage_term_f32 =
            damage_f32 * mash->capture_damage_scale_f32;
        const float handicap_term_f32 =
            mash->capture_handicap_scale_f32 *
            (mash->capture_handicap_reference_f32 - 9.0f);
        const float rank_term_f32 =
            mash->capture_rank_scale_f32 *
            (mash->capture_rank_reference_f32 - (float)rank_slot);
        const float timer_f32 =
            mash->capture_base_f32 +
            handicap_term_f32 + rank_term_f32 + damage_term_f32;
        const float decrement_f32 = (float)mash->capture_tick_decrement;
        uint32_t ticks =
            timer_f32 > 0.0f
                ? (uint32_t)ceilf(timer_f32 / decrement_f32)
                : UINT32_C(1);

        if (ticks > (uint32_t)PF_SIM_MAX_GRAB_ESCAPE_TICKS)
        {
            ticks = (uint32_t)PF_SIM_MAX_GRAB_ESCAPE_TICKS;
        }
        return (uint16_t)ticks;
    }

    {
    uint32_t ticks =
        (uint32_t)fighter->grab_escape_base_ticks +
        (uint32_t)(damage_f32 * fighter->grab_escape_damage_ticks_f32);

    if (ticks > (uint32_t)fighter->grab_escape_max_ticks)
    {
        ticks = (uint32_t)fighter->grab_escape_max_ticks;
    }
    return (uint16_t)ticks;
    }
}

static float merge_damage_velocity_component(
    float current_f32,
    float incoming_f32)
{
    const int opposite_directions =
        (current_f32 < INT32_C(0) && incoming_f32 > INT32_C(0)) ||
        (current_f32 > INT32_C(0) && incoming_f32 < INT32_C(0));
    const float current_magnitude = fabsf(current_f32);
    const float incoming_magnitude = fabsf(incoming_f32);

    if (opposite_directions != 0)
    {
        /* Opposite-signed int32_t operands cannot overflow their sum. */
        return current_f32 + incoming_f32;
    }
    return incoming_magnitude > current_magnitude
               ? incoming_f32
               : current_f32;
}

static pf_status apply_hit_reaction(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t *rng_state,
    uint8_t source_player,
    uint32_t target_index,
    float damage_f32,
    float launch_velocity_x_f32,
    float launch_velocity_y_f32,
    uint16_t hitlag_ticks,
    uint16_t resolved_hitstun_ticks,
    int velocity_is_weighted,
    int launch_grounded,
    uint8_t damage_level,
    uint8_t hurtbox_height,
    uint8_t meteor_cancellable,
    pf_sim_event_type event_type,
    uint16_t event_detail)
{
    const uint8_t previous_action =
        scratch->action_state[target_index];
    int armored;
    int crouch_cancelled;
    int v_cancelled;
    int reset;
    uint32_t hit_sequence;
    uint16_t event_flags;
    uint16_t hitstun_ticks_value;
    float motion_velocity_x_f32;
    float motion_velocity_y_f32;
    float ground_knockback_scalar_f32;
    const float previous_knockback_velocity_x_f32 =
        scratch->knockback_velocity_x_f32[target_index];
    const float previous_knockback_velocity_y_f32 =
        scratch->knockback_velocity_y_f32[target_index];
    const uint8_t damage_source_grounded =
        scratch->grounded[target_index];

    if (velocity_is_weighted == 0)
    {
        launch_velocity_x_f32 = apply_weight_f32(
            launch_velocity_x_f32,
            content->fighter.weight_f32);
        launch_velocity_y_f32 = apply_weight_f32(
            launch_velocity_y_f32,
            content->fighter.weight_f32);
    }

    /* Damage motion selection owns the original launch angle. The floor
     * response below may project or reflect the physical velocity, but the
     * source chooses DamageFlyTop/Roll before that mutation. */
    motion_velocity_x_f32 = launch_velocity_x_f32;
    motion_velocity_y_f32 = launch_velocity_y_f32;
    ground_knockback_scalar_f32 =
        launch_grounded != 0 ? launch_velocity_x_f32 : INT32_C(0);
    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        damage_source_grounded != UINT8_C(0) &&
        event_is_physical_hit(event_type))
    {
        const ssbm_stage_collision_line *floor =
            ssbm_reference_stage_line(
                content->stage.reference_collision_profile,
                scratch->support[target_index]);
        uint8_t source_launch_grounded = UINT8_C(0);

        if (floor != NULL)
        {
            if (ssbm_resolve_ground_damage_launch_f32(
                    floor->source_normal_x_f32,
                    floor->source_normal_y_f32,
                    floor->ground_projection_x_f32,
                    floor->ground_projection_y_f32,
                    damage_level,
                    &launch_velocity_x_f32,
                    &launch_velocity_y_f32,
                    &ground_knockback_scalar_f32,
                    &source_launch_grounded) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            launch_grounded = source_launch_grounded != UINT8_C(0);
        }
    }

    if (previous_action == (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
        previous_action ==
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND)
    {
        scratch->charge_ticks[target_index] = UINT16_C(0);
    }
    scratch->smash_charge_ticks[target_index] = UINT16_C(0);
    scratch->damage_jump_buffer_ticks[target_index] = UINT16_C(0);
    if (scratch->action_state[target_index] ==
        (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        scratch->shield_health_f32[target_index] =
            content->fighter.shield_reset_health_f32;
    }
    scratch->damage_f32[target_index] = saturating_damage(
        scratch->damage_f32[target_index],
        damage_f32);
    hitstun_ticks_value =
        resolved_hitstun_ticks != UINT16_MAX
            ? resolved_hitstun_ticks
            : hitstun_ticks(
                  &content->fighter,
                  motion_velocity_x_f32,
                  motion_velocity_y_f32);
    armored = event_is_physical_hit(event_type) &&
              previous_action ==
                  (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
              content->fighter
                      .double_jump_armor_max_hitstun_ticks !=
                  UINT16_C(0) &&
              hitstun_ticks_value <=
                  content->fighter
                      .double_jump_armor_max_hitstun_ticks;
    if (armored == 0)
    {
        /* ftCo_8008DCE0 resets x670/x671 on every actual Damage entry.
         * ResetBound takes that same entry route; the armor branch instead
         * preserves its existing action and therefore keeps both ages. UCF
         * x673/x674 are deliberately separate continuous histories. */
        reset_ordinary_tilt_ages(scratch, target_index);
    }
    reset = armored == 0 &&
            event_is_physical_hit(event_type) &&
            (previous_action == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
             previous_action == (uint8_t)PF_M4_ACTION_RESET_BOUND) &&
            damage_f32 <= content->fighter.reset_max_damage_f32 &&
            hitstun_ticks_value <=
                content->fighter.reset_max_hitstun_ticks;
    crouch_cancelled =
        armored == 0 && reset == 0 &&
        event_is_physical_hit(event_type) &&
        (previous_action == (uint8_t)PF_M4_ACTION_CROUCH_START ||
         previous_action == (uint8_t)PF_M4_ACTION_CROUCH) &&
        scratch->grounded[target_index] != UINT8_C(0) &&
        scratch->damage_f32[target_index] <=
            content->fighter.crouch_cancel_max_damage_f32;
    v_cancelled = armored == 0 && reset == 0 && crouch_cancelled == 0
                      ? player_v_cancelled(
                            &content->fighter,
                            scratch,
                            target_index)
                      : 0;
    scratch->knockback_velocity_x_f32[target_index] =
        armored != 0 || reset != 0
            ? INT32_C(0)
            : launch_velocity_x_f32;
    scratch->knockback_velocity_y_f32[target_index] =
        armored != 0
            ? INT32_C(0)
            : reset != 0
            ? -content->fighter.reset_bound_speed_f32
            : launch_velocity_y_f32;
    scratch->ground_knockback_velocity_f32[target_index] =
        armored != 0 || reset != 0 || launch_grounded == 0
            ? INT32_C(0)
            : ground_knockback_scalar_f32;
    scratch->hitstun_ticks[target_index] =
        armored != 0 ? UINT16_C(0) : hitstun_ticks_value;
    if (armored == 0 && event_is_physical_hit(event_type))
    {
        /* ftCo_Damage clears self/gr_vel when ordinary damage motion starts;
         * separately tracked knockback owns displacement until damage motion
         * releases control. */
        scratch->velocity_x_f32[target_index] = INT32_C(0);
        scratch->velocity_y_f32[target_index] = INT32_C(0);

        /* ftColl_8007A06C makes an ordinary fighter victim face the attacker
         * selected by the strongest damage-log entry. ftCo_Throw instead
         * seeds dmg.facing_dir_1 opposite the thrower's facing before entering
         * the same damage routine. Item/environment facing has separate
         * velocity and collision-point rules and is intentionally left to
         * those content paths. None of Falcon's imported capsules uses the
         * special 362-degree collision-point angle. */
        if (source_player < world->player_count)
        {
            if (event_type == PF_SIM_EVENT_HIT)
            {
                scratch->facing[target_index] =
                    scratch->position_x_f32[target_index] >
                            scratch->position_x_f32[source_player]
                        ? INT8_C(-1)
                        : INT8_C(1);
            }
            else if (event_type == PF_SIM_EVENT_THROW)
            {
                scratch->facing[target_index] =
                    (int8_t)-scratch->facing[source_player];
            }
        }
    }
    if (crouch_cancelled != 0 && velocity_is_weighted == 0)
    {
        scratch->knockback_velocity_x_f32[target_index] =
            scale_velocity_f32(
                scratch->knockback_velocity_x_f32[target_index],
                content->fighter.crouch_cancel_velocity_scale_f32);
        scratch->knockback_velocity_y_f32[target_index] =
            scale_velocity_f32(
                scratch->knockback_velocity_y_f32[target_index],
                content->fighter.crouch_cancel_velocity_scale_f32);
        scratch->hitstun_ticks[target_index] =
            scale_hitstun_ticks(
                scratch->hitstun_ticks[target_index],
                content->fighter.crouch_cancel_hitstun_scale_f32);
    }
    if (v_cancelled != 0)
    {
        scratch->knockback_velocity_x_f32[target_index] =
            scale_velocity_f32(
                scratch->knockback_velocity_x_f32[target_index],
                content->fighter.v_cancel_velocity_scale_f32);
        scratch->knockback_velocity_y_f32[target_index] =
            scale_velocity_f32(
                scratch->knockback_velocity_y_f32[target_index],
            content->fighter.v_cancel_velocity_scale_f32);
    }
    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        armored == 0 && reset == 0 &&
        scratch->last_hit_sequence[target_index] != UINT32_C(0))
    {
        const ssbm_damage_response_attributes *damage_response =
            ssbm_common_reference_damage_response();
        /* ftCo_Damage_CalcVel replaces both axes inside xFC frames.  Once
         * that window has elapsed it adds opposing components and otherwise
         * preserves whichever same-direction component has greater
         * magnitude. Melee's x18ac timer pauses during hitlag, so wall-clock
         * simulation ticks are not equivalent here. xF0_ground_kb_vel
         * remains the incoming grounded scalar and therefore intentionally
         * is not merged here. */
        if (damage_response != NULL &&
            scratch->damage_time_since_hit_ticks[target_index] >=
                damage_response->damage_velocity_replace_window_ticks)
        {
            scratch->knockback_velocity_x_f32[target_index] =
                merge_damage_velocity_component(
                    previous_knockback_velocity_x_f32,
                    scratch->knockback_velocity_x_f32[target_index]);
            scratch->knockback_velocity_y_f32[target_index] =
                merge_damage_velocity_component(
                    previous_knockback_velocity_y_f32,
                    scratch->knockback_velocity_y_f32[target_index]);
        }
    }
    scratch->tumble[target_index] =
        armored == 0 && reset == 0 &&
                scratch->hitstun_ticks[target_index] >=
                    content->fighter.tumble_hitstun_threshold_ticks
            ? UINT8_C(1)
            : UINT8_C(0);
    scratch->previous_directional_input_flags[target_index] =
        (uint8_t)(
            (scratch->previous_directional_input_flags[target_index] &
             (uint8_t)~PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL) |
            (meteor_cancellable != UINT8_C(0) && armored == 0 &&
                     reset == 0 && launch_grounded == 0
                 ? PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL
                 : UINT8_C(0)));
    scratch->shield_stun_ticks[target_index] = UINT16_C(0);
    scratch->shield_recoil_x_f32[target_index] = INT32_C(0);
    scratch->shield_recoil_mask =
        (uint8_t)(
            scratch->shield_recoil_mask &
            (uint8_t)~(UINT8_C(1) << target_index));
    scratch->powershield[target_index] = UINT8_C(0);
    scratch->shield_strength[target_index] = UINT16_C(0);
    scratch->shield_angle_turn[target_index] = UINT16_C(0);
    scratch->shield_magnitude[target_index] = UINT16_C(0);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    scratch->hitlag_resume_action[target_index] =
        armored != 0
            ? previous_action
            : reset != 0
            ? (uint8_t)PF_M4_ACTION_RESET_BOUND
            : launch_grounded != 0
            ? (uint8_t)((uint8_t)PF_M4_ACTION_DAMAGE_LOW_1 +
                        (damage_level < UINT8_C(3)
                             ? damage_level
                             : UINT8_C(2)))
            : (uint8_t)PF_M4_ACTION_HITSTUN;
    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        armored == 0 && reset == 0)
    {
        ssbm_damage_motion_kind damage_motion;
        uint16_t damage_submotion;

        if (ssbm_select_damage_motion(
                (uint8_t)launch_grounded,
                damage_level,
                scratch->damage_f32[target_index],
                motion_velocity_x_f32,
                motion_velocity_y_f32,
                rng_state,
                &damage_motion) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (damage_motion == PF_M4_SSBM_DAMAGE_MOTION_FLY_TOP)
        {
            damage_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_TOP;
        }
        else if (damage_motion == PF_M4_SSBM_DAMAGE_MOTION_FLY_ROLL)
        {
            damage_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL;
        }
        else if (!falcon_reference_damage_submotion(
                     damage_source_grounded,
                     damage_level,
                     hurtbox_height,
                     &damage_submotion))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        scratch->source_submotion[target_index] = damage_submotion;
        scratch->source_animation_frame_f32[target_index] = 1.0f;
        scratch->source_animation_rate_f32[target_index] = 1.0f;
    }
    /* DamageFly is airborne from its entry frame, including the hitlag-held
     * rows before its physics callback first advances.  Ground damage keeps
     * the source floor only when the imported knockback result selected a
     * grounded launch. */
    if (armored == 0 && reset == 0 && launch_grounded == 0)
    {
        if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
            damage_source_grounded != UINT8_C(0))
        {
            /* ftCo_8008DCE0 calls ftCommon_8007D5D4 before changing a
             * ground-origin damage launch to GA_Air. A grounded desired ECB
             * has bottom zero in root space; retain that exact source value
             * through hitlag so the first released collision callback can
             * recontact a downhill floor. */
            scratch->ecb_bottom_lock_ticks[target_index] =
                PF_M4_COMMON_AIR_ENTRY_ECB_LOCK_TICKS;
            scratch->ecb_locked_bottom_y_f32[target_index] = 0.0f;
        }
        scratch->grounded[target_index] = UINT8_C(0);
    }
    /* DownBound can retain its floor line while its animated ECB reports no
     * contact. Once a hit replaces that action with airborne damage, the
     * retained line is no longer an active support constraint. */
    if (scratch->grounded[target_index] == UINT8_C(0))
    {
        scratch->support[target_index] = (uint8_t)PF_M4_SURFACE_NONE;
    }
    set_action_state(
        world,
        scratch,
        target_index,
        (uint8_t)PF_M4_ACTION_HITLAG);
    if (armored == 0)
    {
        scratch->action_ticks[target_index] = UINT16_C(0);
    }
    scratch->dash_direction[target_index] = INT8_C(0);
    scratch->short_hop_latched[target_index] = UINT8_C(0);
    scratch->fast_fall[target_index] = UINT8_C(0);
    scratch->attack_hit_mask[target_index] = UINT8_C(0);
    scratch->attack_stale_registered[target_index] = UINT8_C(0);
    scratch->rebound_duration_ticks[target_index] = UINT16_C(0);
    scratch->jab_chain_buffered[target_index] = UINT8_C(0);
    scratch->rapid_jab_input_count[target_index] = UINT8_C(0);
    scratch->rapid_jab_continue[target_index] = UINT8_C(0);
    scratch->down_tilt_repeat_buffered[target_index] = UINT8_C(0);
    scratch->sdi_pulse_count[target_index] = UINT8_C(0);
    scratch->sdi_direction_x[target_index] = INT8_C(0);
    scratch->sdi_direction_y[target_index] = INT8_C(0);
    scratch->tech_direction[target_index] = INT8_C(0);
    scratch->prone_orientation[target_index] =
        (uint8_t)PF_M4_PRONE_NONE;

    event_flags = UINT16_C(0);
    if (scratch->tumble[target_index] != UINT8_C(0))
    {
        event_flags |= (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE;
    }
    if (crouch_cancelled != 0)
    {
        event_flags |= (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL;
    }
    if (pf_sim_push_event(
            scratch,
            world->tick,
            event_type,
            source_player,
            (uint8_t)target_index,
            damage_f32,
            scratch->knockback_velocity_x_f32[target_index],
            scratch->knockback_velocity_y_f32[target_index],
            event_flags,
            event_detail,
            &hit_sequence) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->last_hit_sequence[target_index] = hit_sequence;
    scratch->last_hit_tick[target_index] = world->tick;
    scratch->last_hit_damage_f32[target_index] = damage_f32;
    scratch->last_hit_attacker[target_index] = source_player;
    if (armored == 0)
    {
        scratch->damage_time_since_hit_ticks[target_index] = UINT8_C(0);
    }
    return PF_STATUS_OK;
}

static pf_status resolve_falcon_dive_capture(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t *rng_state,
    uint32_t holder_index,
    uint32_t target_index,
    int *out_handled)
{
    uint8_t holder_action = scratch->action_state[holder_index];

    if (out_handled == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (holder_action == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        holder_action =
            scratch->hitlag_resume_action[holder_index];
    }
    *out_handled =
        holder_action ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
        holder_action ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW;
    if (*out_handled == 0)
    {
        return PF_STATUS_OK;
    }
    if (scratch->grab_owner_slot[target_index] !=
            (uint8_t)(holder_index + UINT32_C(1)) ||
        scratch->action_state[target_index] !=
            (uint8_t)PF_M4_ACTION_GRABBED)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    if (holder_action ==
        (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH)
    {
        scratch->position_x_f32[target_index] =
            scratch->position_x_f32[holder_index];
        scratch->position_y_f32[target_index] =
            scratch->position_y_f32[holder_index];
        scratch->velocity_x_f32[target_index] = INT32_C(0);
        scratch->velocity_y_f32[target_index] = INT32_C(0);
        scratch->grounded[target_index] =
            scratch->grounded[holder_index];
        scratch->support[target_index] = scratch->support[holder_index];
    }

    if (holder_action ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH &&
        scratch->action_state[holder_index] !=
            (uint8_t)PF_M4_ACTION_HITLAG &&
        scratch->action_ticks[holder_index] == UINT16_C(1) &&
        scratch->attack_stale_registered[holder_index] == UINT8_C(0))
    {
        const reference_hit_effect *effect =
            falcon_reference_primary_effect(
                PF_M4_FALCON_UP_SPECIAL_CATCH);
        melee_knockback_data hit;
        melee_knockback_result result;
        float damage_f32;
        uint32_t hit_sequence;

        if (effect == NULL || effect->damage == UINT8_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        damage_f32 = stale_scaled_damage_f32(
            &content->fighter,
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH,
            (float)effect->damage);
        hit.angle_degrees = effect->angle_degrees;
        hit.growth = effect->growth;
        hit.weight_set = effect->weight_set;
        hit.base = effect->base;
        hit.enabled = UINT8_C(1);
        (void)memset(hit.reserved, 0, sizeof(hit.reserved));
        result = melee_knockback(
            &hit,
            content->fighter.knockback_weight,
            damage_f32,
            saturating_damage(
                scratch->damage_f32[target_index],
                damage_f32));
        scratch->damage_f32[target_index] = saturating_damage(
            scratch->damage_f32[target_index],
            damage_f32);
        if (pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_HIT,
                (uint8_t)holder_index,
                (uint8_t)target_index,
                damage_f32,
                INT32_C(0),
                INT32_C(0),
                UINT16_C(0),
                (uint16_t)PF_M4_ACTION_FALCON_DIVE_CATCH,
                &hit_sequence) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        scratch->last_hit_sequence[target_index] = hit_sequence;
        scratch->last_hit_tick[target_index] = world->tick;
        scratch->last_hit_damage_f32[target_index] = damage_f32;
        scratch->last_hit_attacker[target_index] =
            (uint8_t)holder_index;
        register_stale_move(
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH);
        scratch->attack_stale_registered[holder_index] = UINT8_C(1);
        scratch->hitlag_ticks[holder_index] = result.hitlag_ticks;
        scratch->hitlag_resume_action[holder_index] =
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH;
        set_action_state(
            world,
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_HITLAG);
        return PF_STATUS_OK;
    }

    if (holder_action ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW &&
        scratch->action_state[holder_index] !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        const reference_throw *source =
            falcon_reference_throw(
                PF_M4_FALCON_UP_SPECIAL_THROW);
        const falcon_up_special_timing *timing =
            falcon_reference_up_special_timing();
        float damage_f32;

        if (source == NULL || timing == NULL ||
            scratch->action_ticks[holder_index] != UINT16_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        damage_f32 = stale_scaled_damage_f32(
            &content->fighter,
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH,
            (float)source->damage);
        clear_grab_links(scratch, holder_index, target_index);
        scratch->grounded[target_index] = UINT8_C(0);
        scratch->support[target_index] =
            (uint8_t)PF_M4_SURFACE_NONE;
        /* CaptureCaptain computes the victim's damage-state duration from the
         * imported throw, then deliberately clears applied launch velocity in
         * ftCo_800DE7C0. Ordinary air gravity starts on the release sample. */
        if (apply_hit_reaction(
                content,
                world,
                scratch,
                rng_state,
                (uint8_t)holder_index,
                target_index,
                damage_f32,
                INT32_C(0),
                INT32_C(0),
                UINT16_C(0),
                timing->victim_release_hitstun_ticks,
                1,
                0,
                UINT8_C(0),
                UINT8_C(1),
                UINT8_C(0),
                PF_SIM_EVENT_THROW,
                (uint16_t)PF_M4_ACTION_FALCON_DIVE_THROW) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        /* Combat resolves after this tick's movement phase. Seed both the
         * visible and zero-hitlag-resume channels with the source's first
         * gravity step so the next movement tick continues the same sequence
         * without introducing launch knockback. */
        scratch->velocity_x_f32[target_index] = INT32_C(0);
        scratch->velocity_y_f32[target_index] =
            content->fighter.gravity_f32;
        scratch->knockback_velocity_y_f32[target_index] =
            content->fighter.gravity_f32;
    }
    return PF_STATUS_OK;
}

static pf_status resolve_throw_capture_hit(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index,
    uint8_t holder_action,
    int *out_hit)
{
    falcon_move_index move_index;
    const reference_hit_effect *effect;
    const uint8_t target_bit =
        (uint8_t)(UINT32_C(1) << target_index);
    const uint8_t move_id = holder_action;
    float damage_f32;
    uint32_t hit_sequence;
    uint16_t hitlag_ticks;

    if (out_hit == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    *out_hit = 0;
    if (content->fighter.reference_frame_data_enabled == UINT8_C(0) ||
        (scratch->attack_hit_mask[holder_index] & target_bit) !=
            UINT8_C(0) ||
        !falcon_reference_move_for_action(
            holder_action,
            &move_index) ||
        !falcon_reference_has_hit_geometry(move_index))
    {
        return PF_STATUS_OK;
    }
    effect = falcon_reference_effect_at_frame(
        move_index,
        reference_move_action_frame(
            move_index,
            scratch->action_ticks[holder_index]));
    if (effect == NULL)
    {
        return PF_STATUS_OK;
    }
    damage_f32 = current_move_stale_scaled_damage_f32(
        &content->fighter,
        scratch,
        holder_index,
        move_id,
        (float)effect->damage);
    hitlag_ticks = melee_hitlag_ticks(
        damage_f32,
        UINT8_C(0),
        1.0f);
    scratch->damage_f32[target_index] =
        saturating_damage(
            scratch->damage_f32[target_index],
            damage_f32);
    if (pf_sim_push_event(
            scratch,
            world->tick,
            PF_SIM_EVENT_HIT,
            (uint8_t)holder_index,
            (uint8_t)target_index,
            damage_f32,
            INT32_C(0),
            INT32_C(0),
            UINT16_C(0),
            (uint16_t)holder_action,
            &hit_sequence) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->last_hit_sequence[target_index] = hit_sequence;
    scratch->last_hit_tick[target_index] = world->tick;
    scratch->last_hit_damage_f32[target_index] = damage_f32;
    scratch->last_hit_attacker[target_index] =
        (uint8_t)holder_index;
    scratch->attack_hit_mask[holder_index] |= target_bit;
    if (scratch->attack_stale_registered[holder_index] ==
        UINT8_C(0))
    {
        register_stale_move(
            scratch,
            holder_index,
            move_id);
        scratch->attack_stale_registered[holder_index] =
            UINT8_C(1);
    }
    scratch->hitlag_ticks[holder_index] = hitlag_ticks;
    scratch->hitlag_resume_action[holder_index] = holder_action;
    set_action_state(
        world,
        scratch,
        holder_index,
        (uint8_t)PF_M4_ACTION_HITLAG);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    scratch->hitlag_resume_action[target_index] =
        (uint8_t)PF_M4_ACTION_GRABBED;
    set_action_state(
        world,
        scratch,
        target_index,
        (uint8_t)PF_M4_ACTION_HITLAG);
    *out_hit = 1;
    return PF_STATUS_OK;
}

static pf_status apply_reference_capture_constraint(
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index,
    int constrain_y)
{
    float offset_x_f32;
    float offset_y_f32;
    float position_x_f32;
    float position_y_f32;

    if (!falcon_reference_capture_constraint_f32(
            scratch->source_submotion[holder_index],
            scratch->source_animation_frame_f32[holder_index],
            scratch->facing[holder_index],
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_f32[target_index],
            scratch->facing[target_index],
            &offset_x_f32,
            &offset_y_f32))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    position_x_f32 = scratch->position_x_f32[holder_index] + offset_x_f32;
    position_y_f32 = scratch->position_y_f32[holder_index] + offset_y_f32;
    if (!isfinite(position_x_f32) ||
        (constrain_y != 0 && !isfinite(position_y_f32)))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->position_x_f32[target_index] = position_x_f32;
    if (constrain_y != 0)
    {
        scratch->position_y_f32[target_index] = position_y_f32;
    }
    return PF_STATUS_OK;
}

static pf_status resolve_grabs(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t *rng_state)
{
    uint32_t holder_index;
    uint32_t attacker_index;

    for (holder_index = UINT32_C(0);
         holder_index < (uint32_t)world->player_count;
         ++holder_index)
    {
        const uint8_t target_slot =
            scratch->grab_target_slot[holder_index];
        uint8_t holder_action =
            scratch->action_state[holder_index];
        const struct throw_data *throw_data;
        int falcon_dive_capture = 0;
        uint32_t target_index;

        if (target_slot == UINT8_C(0))
        {
            continue;
        }
        if (holder_action == (uint8_t)PF_M4_ACTION_HITLAG &&
            (action_is_throw(
                 scratch->hitlag_resume_action[holder_index]) ||
             scratch->hitlag_resume_action[holder_index] ==
                 (uint8_t)PF_M4_ACTION_PUMMEL))
        {
            holder_action =
                scratch->hitlag_resume_action[holder_index];
        }
        throw_data =
            throw_for_action(&content->fighter, holder_action);
        target_index = (uint32_t)target_slot - UINT32_C(1);
        if (target_index >= (uint32_t)world->player_count ||
            target_index == holder_index)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (resolve_falcon_dive_capture(
                content,
                world,
                scratch,
                rng_state,
                holder_index,
                target_index,
                &falcon_dive_capture) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (falcon_dive_capture != 0)
        {
            continue;
        }
        if (scratch->active[holder_index] == UINT8_C(0) ||
            scratch->active[target_index] == UINT8_C(0) ||
            (holder_action != (uint8_t)PF_M4_ACTION_GRAB_HOLD &&
             holder_action != (uint8_t)PF_M4_ACTION_PUMMEL &&
             throw_data == NULL) ||
            (scratch->action_state[target_index] !=
                 (uint8_t)PF_M4_ACTION_GRABBED &&
             !(scratch->action_state[target_index] ==
                   (uint8_t)PF_M4_ACTION_HITLAG &&
               scratch->hitlag_resume_action[target_index] ==
                   (uint8_t)PF_M4_ACTION_GRABBED)) ||
            scratch->grab_owner_slot[target_index] !=
                (uint8_t)(holder_index + UINT32_C(1)))
        {
            release_grab(
                world,
                scratch,
                holder_index,
                target_index);
            continue;
        }
        if (scratch->grab_escape_ticks[target_index] == UINT16_C(0))
        {
            release_grab(
                world,
                scratch,
                holder_index,
                target_index);
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_GRAB_ESCAPE,
                    (uint8_t)target_index,
                    (uint8_t)holder_index,
                    scratch->damage_f32[target_index],
                    INT32_C(0),
                    INT32_C(0),
                    UINT16_C(0),
                    UINT16_C(0),
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            continue;
        }

        if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
            ((holder_action == (uint8_t)PF_M4_ACTION_GRAB_HOLD &&
              (scratch->source_submotion[target_index] == (uint16_t)
                   PF_M4_FALCON_SUBMOTION_CAPTURE_PULLED_LOW ||
               scratch->source_submotion[target_index] == (uint16_t)
                   PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_LOW)) ||
             throw_data != NULL))
        {
            if (scratch->source_submotion[holder_index] == (uint16_t)
                    PF_M4_FALCON_SUBMOTION_CATCH_WAIT &&
                scratch->source_submotion[target_index] == (uint16_t)
                    PF_M4_FALCON_SUBMOTION_CAPTURE_PULLED_LOW)
            {
                /* Catch's throw-flag callback enters CatchWait before IASA;
                 * install the paired CaptureWaitLw endpoint after every
                 * fighter has stepped so player iteration order cannot
                 * expose one half of the transition. */
                scratch->source_submotion[target_index] = (uint16_t)
                    PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_LOW;
                scratch->source_animation_frame_f32[target_index] =
                    INT32_C(0);
                scratch->source_animation_rate_f32[target_index] =
                    1.0f;
                scratch->action_ticks[target_index] = UINT16_C(0);
            }
            if (throw_data != NULL)
            {
                uint16_t expected_holder_submotion;
                uint16_t victim_submotion;
                float animation_rate_f32;

                if (!falcon_reference_throw_motions(
                        holder_action,
                        &expected_holder_submotion,
                        &victim_submotion,
                        &animation_rate_f32) ||
                    scratch->source_submotion[holder_index] !=
                        expected_holder_submotion ||
                    scratch->source_animation_rate_f32[holder_index] !=
                        animation_rate_f32)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                /* Throw/Thrown are installed as one paired transition after
                 * every fighter has stepped. This mirrors ftCo_800DD398 and
                 * makes the result independent of player iteration order. */
                scratch->source_submotion[target_index] = victim_submotion;
                scratch->source_animation_frame_f32[target_index] =
                    scratch->source_animation_frame_f32[holder_index];
                scratch->source_animation_rate_f32[target_index] =
                    animation_rate_f32;
                scratch->facing[target_index] =
                    scratch->facing[holder_index];
            }
            if (apply_reference_capture_constraint(
                    scratch,
                    holder_index,
                    target_index,
                    throw_data != NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        else
        {
            scratch->position_x_f32[target_index] =
                scratch->position_x_f32[holder_index] +
                (int32_t)scratch->facing[holder_index] *
                    content->fighter.grabbed_offset_x_f32;
            scratch->position_y_f32[target_index] =
                scratch->position_y_f32[holder_index] +
                content->fighter.grabbed_offset_y_f32;
        }
        scratch->velocity_x_f32[target_index] = INT32_C(0);
        scratch->velocity_y_f32[target_index] = INT32_C(0);
        scratch->grounded[target_index] =
            scratch->grounded[holder_index];
        scratch->support[target_index] =
            scratch->support[holder_index];
        if (holder_action == (uint8_t)PF_M4_ACTION_PUMMEL)
        {
            const uint16_t action_ticks =
                scratch->action_ticks[holder_index];
            const uint8_t target_bit =
                (uint8_t)(UINT32_C(1) << target_index);

            if (action_ticks > content->fighter.pummel_total_ticks)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (action_ticks == content->fighter.pummel_hit_tick &&
                (scratch->attack_hit_mask[holder_index] & target_bit) ==
                    UINT8_C(0))
            {
                const uint8_t move_id =
                    (uint8_t)PF_M4_ACTION_PUMMEL;
                const float damage_f32 =
                    stale_scaled_damage_f32(
                        &content->fighter,
                        scratch,
                        holder_index,
                        move_id,
                        content->fighter.pummel_damage_f32);
                const uint16_t hitlag_ticks =
                    melee_hitlag_ticks(
                        damage_f32,
                        UINT8_C(0),
                        1.0f);
                uint32_t pummel_sequence;

                scratch->damage_f32[target_index] =
                    saturating_damage(
                        scratch->damage_f32[target_index],
                        damage_f32);
                if (pf_sim_push_event(
                        scratch,
                        world->tick,
                        PF_SIM_EVENT_PUMMEL,
                        (uint8_t)holder_index,
                        (uint8_t)target_index,
                        damage_f32,
                        INT32_C(0),
                        INT32_C(0),
                        UINT16_C(0),
                        (uint16_t)PF_M4_ACTION_PUMMEL,
                        &pummel_sequence) != PF_STATUS_OK)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                scratch->last_hit_sequence[target_index] =
                    pummel_sequence;
                scratch->last_hit_tick[target_index] = world->tick;
                scratch->last_hit_damage_f32[target_index] =
                    damage_f32;
                scratch->last_hit_attacker[target_index] =
                    (uint8_t)holder_index;
                scratch->attack_hit_mask[holder_index] |= target_bit;
                if (scratch->attack_stale_registered[holder_index] ==
                    UINT8_C(0))
                {
                    register_stale_move(
                        scratch,
                        holder_index,
                        move_id);
                    scratch->attack_stale_registered[holder_index] =
                        UINT8_C(1);
                }
                /* CatchAttack's script creates an ordinary damaging hitbox.
                 * The captured target stays linked and receives no launch,
                 * but both participants still enter the hit's synchronized
                 * hitlag before resuming CatchAttack/CaptureDamage. */
                if (scratch->hitlag_ticks[holder_index] < hitlag_ticks)
                {
                    scratch->hitlag_ticks[holder_index] = hitlag_ticks;
                }
                scratch->hitlag_resume_action[holder_index] =
                    (uint8_t)PF_M4_ACTION_PUMMEL;
                set_action_state(
                    world,
                    scratch,
                    holder_index,
                    (uint8_t)PF_M4_ACTION_HITLAG);
                scratch->action_ticks[target_index] = UINT16_C(0);
                scratch->source_submotion[target_index] =
                    (uint16_t)
                        PF_M4_FALCON_SUBMOTION_CAPTURE_DAMAGE_HIGH;
                scratch->source_animation_frame_f32[target_index] = 0.0f;
                scratch->source_animation_rate_f32[target_index] = 1.0f;
                if (scratch->hitlag_ticks[target_index] < hitlag_ticks)
                {
                    scratch->hitlag_ticks[target_index] = hitlag_ticks;
                }
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_GRABBED;
                set_action_state(
                    world,
                    scratch,
                    target_index,
                    (uint8_t)PF_M4_ACTION_HITLAG);
            }
        }
        if (throw_data != NULL)
        {
            const uint16_t action_ticks =
                scratch->action_ticks[holder_index];
            int captured_hit = 0;

            if (action_ticks > throw_data->release_tick)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (resolve_throw_capture_hit(
                    content,
                    world,
                    scratch,
                    holder_index,
                    target_index,
                    holder_action,
                    &captured_hit) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (captured_hit != 0)
            {
                continue;
            }
            if (action_ticks == throw_data->release_tick)
            {
                const uint8_t move_id = holder_action;
                const float damage_f32 =
                    current_move_stale_scaled_damage_f32(
                        &content->fighter,
                        scratch,
                        holder_index,
                        move_id,
                        throw_data->damage_f32);
                const float resulting_damage =
                    saturating_damage(
                        scratch->damage_f32[target_index],
                        damage_f32);
                const float knockback_percent_f32 =
                    saturating_damage(
                        floorf(scratch->damage_f32[target_index]),
                        damage_f32);
                float launch_velocity_x;
                float launch_velocity_y;
                uint16_t resolved_hitlag_ticks =
                    throw_data->hitlag_ticks;
                uint16_t resolved_hitstun_ticks = UINT16_MAX;
                int velocity_is_weighted = 0;
                int launch_grounded = 0;
                uint8_t damage_level = UINT8_C(0);
                uint8_t meteor_cancellable = UINT8_C(0);

                if (content->fighter.reference_frame_data_enabled !=
                    UINT8_C(0))
                {
                    float release_offset_x_f32;
                    float release_offset_y_f32;
                    float release_frame_f32;
                    float release_position_x_f32;
                    float release_position_y_f32;
                    const float release_frame =
                        scratch->source_animation_frame_f32[holder_index] +
                        scratch->source_animation_rate_f32[holder_index];

                    /* ftCo_800DDDE4 samples the thrower's TransN2 at the
                     * release-frame pose, adds the victim's base XRotN-to-root
                     * offset with the pre-Damage facing, and only then removes
                     * the capture constraint and enters common Damage.  The
                     * source animation process advances before its Throw Anim
                     * callback; combat runs before our later pose advance, so
                     * sample that same next imported frame explicitly. */
                    if (!isfinite(release_frame))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    release_frame_f32 = release_frame;
                    if (!falcon_reference_capture_constraint_f32(
                            scratch->source_submotion[holder_index],
                            release_frame_f32,
                            scratch->facing[holder_index],
                            scratch->source_submotion[target_index],
                            scratch->source_animation_frame_f32[target_index],
                            scratch->facing[target_index],
                            &release_offset_x_f32,
                            &release_offset_y_f32))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    release_position_x_f32 =
                        scratch->position_x_f32[holder_index] +
                        release_offset_x_f32;
                    release_position_y_f32 =
                        scratch->position_y_f32[holder_index] +
                        release_offset_y_f32;
                    if (!isfinite(release_position_x_f32) ||
                        !isfinite(release_position_y_f32))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    scratch->position_x_f32[target_index] =
                        release_position_x_f32;
                    scratch->position_y_f32[target_index] =
                        release_position_y_f32;
                    if (content->stage.reference_collision_profile !=
                            (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED &&
                        reference_throw_release_sweep(
                                content,
                                scratch,
                                holder_index,
                                target_index,
                                release_position_x_f32,
                                release_position_y_f32,
                                &scratch->position_x_f32[target_index],
                                &scratch->position_y_f32[target_index]) !=
                            PF_STATUS_OK)
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                }

                if (throw_data->melee_knockback.enabled != UINT8_C(0))
                {
                    const melee_knockback_result result =
                        melee_knockback_for_state(
                            &throw_data->melee_knockback,
                            content->fighter.knockback_weight,
                            damage_f32,
                            damage_f32,
                            knockback_percent_f32,
                            scratch->grounded[target_index],
                            UINT8_C(0),
                            UINT8_C(0),
                            1.0f);

                    launch_velocity_x =
                        (float)scratch->facing[holder_index] *
                        result.velocity_x_f32;
                    launch_velocity_y = -result.velocity_y_f32;
                    resolved_hitstun_ticks = result.hitstun_ticks;
                    velocity_is_weighted = 1;
                    launch_grounded = result.grounded_launch != UINT8_C(0);
                    damage_level = result.damage_level;
                    meteor_cancellable = result.meteor_cancellable;
                }
                else
                {
                    launch_velocity_x =
                        (int32_t)scratch->facing[holder_index] *
                        scaled_throw_velocity(
                            throw_data->base_velocity_x_f32,
                            throw_data->velocity_growth_x_f32,
                            resulting_damage);
                    launch_velocity_y =
                        scaled_throw_velocity(
                            throw_data->base_velocity_y_f32,
                            throw_data->velocity_growth_y_f32,
                            resulting_damage);
                }

                clear_grab_links(
                    scratch,
                    holder_index,
                    target_index);
                if (apply_hit_reaction(
                        content,
                        world,
                        scratch,
                        rng_state,
                        (uint8_t)holder_index,
                        target_index,
                        damage_f32,
                        launch_velocity_x,
                        launch_velocity_y,
                        resolved_hitlag_ticks,
                        resolved_hitstun_ticks,
                        velocity_is_weighted,
                        launch_grounded,
                        damage_level,
                        UINT8_C(1),
                        meteor_cancellable,
                        PF_SIM_EVENT_THROW,
                        (uint16_t)holder_action) != PF_STATUS_OK)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (content->fighter.reference_frame_data_enabled !=
                        UINT8_C(0) &&
                    holder_action == (uint8_t)PF_M4_ACTION_THROW_DOWN)
                {
                    /* ftCo_800DD724 passes motion_id == ThrowLw into
                     * ftCo_800DE7C0. That helper supplies the explicit common
                     * motion id 0x5A to ftCo_8008DCE0, forcing DamageFlyTop;
                     * the other three throws pass -1 and use ordinary damage
                     * motion selection. DI still runs afterward. */
                    scratch->source_submotion[target_index] =
                        (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_TOP;
                }
                if (resolved_hitlag_ticks != UINT16_C(0))
                {
                    scratch->hitlag_ticks[holder_index] =
                        resolved_hitlag_ticks;
                    scratch->hitlag_resume_action[holder_index] =
                        holder_action;
                    set_action_state(
                        world,
                        scratch,
                        holder_index,
                        (uint8_t)PF_M4_ACTION_HITLAG);
                }
                else
                {
                    if (content->fighter.reference_frame_data_enabled !=
                            UINT8_C(0) &&
                        target_index > holder_index)
                    {
                        if (reference_zero_hitlag_throw_damage_step(
                                content,
                                world,
                                scratch,
                                target_index) != PF_STATUS_OK)
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                    }
                    else if (content->fighter.reference_frame_data_enabled !=
                             UINT8_C(0))
                    {
                        /* Fighter objects run in player order.  An earlier
                         * victim already completed its Anim/Phys/Map update
                         * before the later thrower released it. DamageFly is
                         * entered immediately, but its newly installed
                         * callbacks do not execute until the following
                         * update. */
                        scratch->hitlag_ticks[target_index] = UINT16_C(0);
                        scratch->hitlag_resume_action[target_index] =
                            UINT8_C(0);
                        set_action_state(
                            world,
                            scratch,
                            target_index,
                            (uint8_t)PF_M4_ACTION_HITSTUN);
                    }
                    else
                    {
                        scratch->velocity_x_f32[target_index] =
                            scratch->knockback_velocity_x_f32[target_index];
                        scratch->velocity_y_f32[target_index] =
                            scratch->knockback_velocity_y_f32[target_index];
                        scratch->knockback_velocity_x_f32[target_index] =
                            INT32_C(0);
                        scratch->knockback_velocity_y_f32[target_index] =
                            INT32_C(0);
                        scratch->grounded[target_index] = UINT8_C(0);
                        scratch->support[target_index] =
                            (uint8_t)PF_M4_SURFACE_NONE;
                        scratch->hitlag_ticks[target_index] = UINT16_C(0);
                        scratch->hitlag_resume_action[target_index] =
                            UINT8_C(0);
                        set_action_state(
                            world,
                            scratch,
                            target_index,
                            (uint8_t)PF_M4_ACTION_HITSTUN);
                    }
                }
                if (scratch->attack_stale_registered[holder_index] ==
                    UINT8_C(0))
                {
                    register_stale_move(
                        scratch,
                        holder_index,
                        move_id);
                    scratch->attack_stale_registered[holder_index] =
                        UINT8_C(1);
                }
            }
        }
    }

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        float grabbox_left = 0.0f;
        float grabbox_right = 0.0f;
        float grabbox_top = 0.0f;
        float grabbox_bottom = 0.0f;
        falcon_move_index geometry_move_index;
        hit_sphere_inspection
            grab_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
        hit_sphere_inspection
            previous_grab_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
        uint8_t grab_sphere_count = UINT8_C(0);
        uint8_t previous_grab_sphere_count = UINT8_C(0);
        const int falcon_dive_grab =
            scratch->action_state[attacker_index] ==
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
            scratch->action_state[attacker_index] ==
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR;
        const int reference_grab_geometry =
            falcon_geometry_move_for_grab(
                content,
                scratch->action_state[attacker_index],
                &geometry_move_index);
        uint32_t target_index;

        if (scratch->active[attacker_index] == UINT8_C(0) ||
            scratch->grab_target_slot[attacker_index] != UINT8_C(0) ||
            scratch->grab_owner_slot[attacker_index] != UINT8_C(0))
        {
            continue;
        }
        if (reference_grab_geometry != 0)
        {
            (void)geometry_move_index;
            grab_sphere_count = grab_hit_spheres(
                content,
                scratch->position_x_f32[attacker_index],
                scratch->position_y_f32[attacker_index],
                scratch->facing[attacker_index],
                scratch->action_state[attacker_index],
                scratch->action_ticks[attacker_index],
                grab_spheres);
            if (grab_sphere_count == UINT8_C(0))
            {
                continue;
            }
            if (reference_hit_sphere_can_continue(
                    world,
                    scratch,
                    attacker_index))
            {
                previous_grab_sphere_count = grab_hit_spheres(
                    content,
                    world->position_x_f32[attacker_index],
                    world->position_y_f32[attacker_index],
                    world->facing[attacker_index],
                    world->action_state[attacker_index],
                    world->action_ticks[attacker_index],
                    previous_grab_spheres);
            }
        }
        else if (!grabbox(
                     content,
                     scratch->position_x_f32[attacker_index],
                     scratch->position_y_f32[attacker_index],
                     scratch->facing[attacker_index],
                     scratch->action_state[attacker_index],
                     scratch->action_ticks[attacker_index],
                     &grabbox_left,
                     &grabbox_right,
                     &grabbox_top,
                     &grabbox_bottom))
        {
            continue;
        }

        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            if (target_index == attacker_index ||
                scratch->active[target_index] == UINT8_C(0) ||
                scratch->grab_owner_slot[target_index] != UINT8_C(0) ||
                scratch->grab_target_slot[target_index] != UINT8_C(0) ||
                scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
                scratch->respawn_invulnerability_ticks[target_index] !=
                    UINT16_C(0) ||
                scratch->ledge_invulnerability_ticks[target_index] !=
                    UINT16_C(0) ||
                action_is_match_entry(
                    scratch->action_state[target_index]) ||
                action_is_recovery_invulnerable(
                    &content->fighter,
                    scratch->action_state[target_index],
                    scratch->action_ticks[target_index],
                    scratch->prone_orientation[target_index],
                    scratch->tech_direction[target_index],
                    scratch->facing[target_index]) ||
                (content->fighter.reference_frame_data_enabled !=
                     UINT8_C(0) &&
                 falcon_reference_body_invulnerable(
                     scratch->source_submotion[target_index],
                     scratch->action_ticks[target_index])) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[attacker_index] ==
                     world->team[target_index]))
            {
                continue;
            }
            if (reference_grab_geometry != 0)
            {
                uint8_t sphere_index;
                int overlaps_target = 0;

                for (sphere_index = UINT8_C(0);
                     sphere_index < grab_sphere_count;
                     ++sphere_index)
                {
                    const hit_sphere_inspection *previous_sphere =
                        previous_matching_hit_sphere(
                            &grab_spheres[sphere_index],
                            previous_grab_spheres,
                            previous_grab_sphere_count);
                    if (grab_sphere_overlaps_player(
                            &content->fighter,
                            world,
                            scratch,
                            attacker_index,
                            target_index,
                            previous_sphere == &grab_spheres[sphere_index]
                                ? scratch->position_y_f32[attacker_index]
                                : world->position_y_f32[attacker_index],
                            previous_sphere,
                            &grab_spheres[sphere_index],
                            target_index > attacker_index))
                    {
                        overlaps_target = 1;
                        break;
                    }
                }
                if (overlaps_target == 0)
                {
                    continue;
                }
            }
            else if (!hitbox_overlaps_player(
                         &content->fighter,
                         scratch,
                         target_index,
                         grabbox_left,
                         grabbox_right,
                         grabbox_top,
                         grabbox_bottom))
            {
                continue;
            }

            /* Fighter collision resolves in player order. A later target has
             * not run its physics callback yet, so a grab on the terminal
             * KneeBend update still owns the pre-step grounded pose. */
            const uint8_t target_collision_grounded =
                target_index > attacker_index
                    ? world->grounded[target_index]
                    : scratch->grounded[target_index];
            const int reference_ground_capture =
                falcon_dive_grab == 0 &&
                content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
                scratch->grounded[attacker_index] != UINT8_C(0) &&
                target_collision_grounded != UINT8_C(0);
            const uint16_t catch_frame =
                scratch->action_ticks[attacker_index] != UINT16_C(0)
                    ? (uint16_t)(scratch->action_ticks[attacker_index] -
                                 UINT16_C(1))
                    : UINT16_C(0);

            scratch->grab_target_slot[attacker_index] =
                (uint8_t)(target_index + UINT32_C(1));
            scratch->grab_owner_slot[target_index] =
                (uint8_t)(attacker_index + UINT32_C(1));
            scratch->grab_escape_ticks[target_index] =
                falcon_dive_grab != 0
                    ? UINT16_C(1)
                    : grab_escape_ticks(
                          content,
                          world,
                          scratch,
                          target_index);
            scratch->mash_stick_x_direction[target_index] = INT8_C(0);
            scratch->mash_stick_y_direction[target_index] = INT8_C(0);
            set_action_state(
                world,
                scratch,
                attacker_index,
                falcon_dive_grab != 0
                    ? (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH
                    : (uint8_t)PF_M4_ACTION_GRAB_HOLD);
            scratch->action_ticks[attacker_index] = UINT16_C(0);
            scratch->velocity_x_f32[attacker_index] = 0.0f;
            set_action_state(
                world,
                scratch,
                target_index,
                (uint8_t)PF_M4_ACTION_GRABBED);
            scratch->action_ticks[target_index] = UINT16_C(0);
            scratch->source_submotion[attacker_index] =
                reference_ground_capture != 0
                    ? (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH
                    : (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_WAIT;
            scratch->source_submotion[target_index] =
                reference_ground_capture != 0
                    ? (uint16_t)
                          PF_M4_FALCON_SUBMOTION_CAPTURE_PULLED_LOW
                    : (uint16_t)
                          PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_HIGH;
            scratch->source_animation_frame_f32[attacker_index] =
                reference_ground_capture != 0
                    ? (float)catch_frame
                    : 0.0f;
            scratch->source_animation_frame_f32[target_index] =
                reference_ground_capture != 0 ? 1.0f : 0.0f;
            scratch->source_animation_rate_f32[attacker_index] =
                reference_ground_capture != 0 ? 1.0f : 0.0f;
            scratch->source_animation_rate_f32[target_index] =
                reference_ground_capture != 0 ? 1.0f : 0.0f;
            scratch->charge_ticks[target_index] = UINT16_C(0);
            scratch->smash_charge_ticks[target_index] = UINT16_C(0);
            if (falcon_dive_grab != 0 &&
                scratch->grounded[target_index] != UINT8_C(0))
            {
                scratch->position_x_f32[attacker_index] =
                    scratch->position_x_f32[target_index];
                scratch->position_y_f32[attacker_index] =
                    scratch->position_y_f32[target_index];
                scratch->grounded[attacker_index] = UINT8_C(1);
                scratch->support[attacker_index] =
                    scratch->support[target_index];
            }
            if (reference_ground_capture != 0)
            {
                scratch->facing[target_index] =
                    (int8_t)-scratch->facing[attacker_index];
                /* A later target still receives its fighter update after the
                 * grab callback, so frame-one CapturePulled physics constrains
                 * X on the acquisition row. An earlier target has already run
                 * and begins constraining on the next update. Ground collision
                 * owns Y separately in either order. */
                if (target_index > attacker_index)
                {
                    scratch->position_y_f32[target_index] =
                        world->position_y_f32[target_index];
                    if (apply_reference_capture_constraint(
                            scratch,
                            attacker_index,
                            target_index,
                            0) != PF_STATUS_OK)
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                }
            }
            else
            {
                scratch->position_x_f32[target_index] =
                    falcon_dive_grab != 0
                        ? scratch->position_x_f32[attacker_index]
                        : scratch->position_x_f32[attacker_index] +
                              (int32_t)scratch->facing[attacker_index] *
                                  content->fighter.grabbed_offset_x_f32;
                scratch->position_y_f32[target_index] =
                    falcon_dive_grab != 0
                        ? scratch->position_y_f32[attacker_index]
                        : scratch->position_y_f32[attacker_index] +
                              content->fighter.grabbed_offset_y_f32;
            }
            scratch->velocity_x_f32[target_index] = INT32_C(0);
            scratch->velocity_y_f32[target_index] = INT32_C(0);
            scratch->knockback_velocity_x_f32[target_index] = INT32_C(0);
            scratch->knockback_velocity_y_f32[target_index] = INT32_C(0);
            scratch->ground_knockback_velocity_f32[target_index] = INT32_C(0);
            scratch->hitlag_ticks[target_index] = UINT16_C(0);
            scratch->hitstun_ticks[target_index] = UINT16_C(0);
            scratch->shield_stun_ticks[target_index] = UINT16_C(0);
            scratch->shield_strength[target_index] = UINT16_C(0);
            scratch->shield_angle_turn[target_index] = UINT16_C(0);
            scratch->shield_magnitude[target_index] = UINT16_C(0);
            scratch->grounded[target_index] =
                scratch->grounded[attacker_index];
            scratch->support[target_index] =
                scratch->support[attacker_index];
            scratch->dash_direction[target_index] = INT8_C(0);
            scratch->short_hop_latched[target_index] = UINT8_C(0);
            scratch->fast_fall[target_index] = UINT8_C(0);
            scratch->attack_hit_mask[target_index] = UINT8_C(0);
            scratch->attack_stale_registered[target_index] =
                UINT8_C(0);
            scratch->powershield[target_index] = UINT8_C(0);
            scratch->tumble[target_index] = UINT8_C(0);
            scratch->tech_direction[target_index] = INT8_C(0);
            scratch->prone_orientation[target_index] =
                (uint8_t)PF_M4_PRONE_NONE;
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_GRAB,
                    (uint8_t)attacker_index,
                    (uint8_t)target_index,
                    scratch->damage_f32[target_index],
                    INT32_C(0),
                    INT32_C(0),
                    UINT16_C(0),
                    (uint16_t)scratch->action_state[attacker_index],
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            break;
        }
    }

    return PF_STATUS_OK;
}

static pf_status resolve_item_combat(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t *rng_state)
{
    const item_data *item = &content->item;
    const uint8_t source_slot = scratch->item_source_slot;
    const uint32_t source_index =
        (uint32_t)source_slot - UINT32_C(1);
    const float hitbox_left =
        scratch->item_position_x_f32 - item->hitbox_half_width_f32;
    const float hitbox_right =
        scratch->item_position_x_f32 + item->hitbox_half_width_f32;
    const float hitbox_top =
        scratch->item_position_y_f32 - item->hitbox_half_height_f32;
    const float hitbox_bottom =
        scratch->item_position_y_f32 + item->hitbox_half_height_f32;
    uint32_t target_index;

    if (item->enabled == UINT8_C(0) ||
        scratch->item_state !=
            (uint8_t)PF_M4_ITEM_STATE_AIRBORNE ||
        source_slot == UINT8_C(0))
    {
        return PF_STATUS_OK;
    }
    if (source_index >= (uint32_t)world->player_count)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    for (target_index = UINT32_C(0);
         target_index < (uint32_t)world->player_count;
         ++target_index)
    {
        const uint8_t target_bit =
            (uint8_t)(UINT32_C(1) << target_index);
        const int8_t launch_direction =
            scratch->item_velocity_x_f32 < INT32_C(0)
                ? INT8_C(-1)
                : scratch->item_velocity_x_f32 > INT32_C(0)
                ? INT8_C(1)
                : scratch->facing[source_index];
        const int shield_overlap =
            hitbox_overlaps_shield(
                &content->fighter,
                scratch,
                target_index,
                hitbox_left,
                hitbox_right,
                hitbox_top,
                hitbox_bottom);

        if (target_index == source_index ||
            scratch->active[target_index] == UINT8_C(0) ||
            scratch->respawn_invulnerability_ticks[target_index] !=
                UINT16_C(0) ||
            scratch->ledge_invulnerability_ticks[target_index] !=
                UINT16_C(0) ||
            action_is_match_entry(
                scratch->action_state[target_index]) ||
            (scratch->item_hit_mask & target_bit) != UINT8_C(0) ||
            scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
            action_is_recovery_invulnerable(
                &content->fighter,
                scratch->action_state[target_index],
                scratch->action_ticks[target_index],
                scratch->prone_orientation[target_index],
                scratch->tech_direction[target_index],
                scratch->facing[target_index]) ||
            (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
             falcon_reference_body_invulnerable(
                 scratch->source_submotion[target_index],
                 scratch->action_ticks[target_index])) ||
            (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
             world->team[source_index] == world->team[target_index]) ||
            !hitbox_overlaps_player_or_shield(
                &content->fighter,
                scratch,
                target_index,
                hitbox_left,
                hitbox_right,
                hitbox_top,
                hitbox_bottom))
        {
            continue;
        }

        if (break_player_grab_links(
                world,
                scratch,
                target_index) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        scratch->item_hit_mask |= target_bit;
        scratch->item_velocity_y_f32 =
            item->hit_bounce_velocity_y_f32;

        const uint8_t move_id =
            (uint8_t)PF_M4_ACTION_ITEM_THROW;
        const float damage_f32 =
            stale_scaled_damage_f32(
                &content->fighter,
                scratch,
                source_index,
                move_id,
                item->damage_f32);

        if (action_is_guarding(
                scratch->action_state[target_index]) &&
            shield_overlap != 0)
        {
            const int powershield =
                scratch->action_state[target_index] ==
                        (uint8_t)PF_M4_ACTION_SHIELD &&
                scratch->action_ticks[target_index] <=
                    content->fighter.powershield_window_ticks &&
                scratch->shield_strength[target_index] >=
                    content->fighter.digital_trigger_threshold;
            if (apply_shield_hit(
                    world,
                    scratch,
                    &content->fighter,
                    source_index,
                    target_index,
                    damage_f32,
                    INT16_C(0),
                    item->hitlag_ticks,
                    (int32_t)launch_direction,
                    powershield,
                    (uint16_t)PF_M4_ACTION_ITEM_THROW,
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            return PF_STATUS_OK;
        }

        {
            const float resulting_damage =
                saturating_damage(
                    scratch->damage_f32[target_index],
                    damage_f32);
            const float knockback_x = scaled_knockback(
                item->base_knockback_x_f32,
                item->knockback_growth_f32,
                resulting_damage,
                0);
            const float knockback_y = scaled_knockback(
                item->base_knockback_y_f32,
                item->knockback_growth_f32,
                resulting_damage,
                1);

            if (apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    rng_state,
                    (uint8_t)source_index,
                    target_index,
                    damage_f32,
                    (float)launch_direction * knockback_x,
                    -knockback_y,
                    item->hitlag_ticks,
                    UINT16_MAX,
                    0,
                    0,
                    UINT8_C(0),
                    UINT8_C(1),
                    UINT8_C(0),
                    PF_SIM_EVENT_ITEM_HIT,
                    (uint16_t)scratch->item_throw_direction) !=
                PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (scratch->item_stale_registered == UINT8_C(0))
            {
                register_stale_move(
                    scratch,
                    source_index,
                    move_id);
                scratch->item_stale_registered = UINT8_C(1);
            }
        }
        return PF_STATUS_OK;
    }
    return PF_STATUS_OK;
}

static void clear_projectile_combat(pf_sim_scratch *scratch)
{
    scratch->projectile_position_x_f32 = 0.0f;
    scratch->projectile_position_y_f32 = 0.0f;
    scratch->projectile_velocity_x_f32 = 0.0f;
    scratch->projectile_velocity_y_f32 = 0.0f;
    scratch->projectile_lifetime_ticks = UINT16_C(0);
    scratch->projectile_state =
        (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE;
    scratch->projectile_owner_slot = UINT8_C(0);
}

static pf_status resolve_projectile_combat(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t *rng_state)
{
    const projectile_data *projectile = &content->projectile;
    const uint8_t owner_slot = scratch->projectile_owner_slot;
    const uint32_t owner_index =
        (uint32_t)owner_slot - UINT32_C(1);
    const float hitbox_left =
        scratch->projectile_position_x_f32 -
        projectile->half_width_f32;
    const float hitbox_right =
        scratch->projectile_position_x_f32 +
        projectile->half_width_f32;
    const float hitbox_top =
        scratch->projectile_position_y_f32 -
        projectile->half_height_f32;
    const float hitbox_bottom =
        scratch->projectile_position_y_f32 +
        projectile->half_height_f32;
    uint32_t target_index;

    if (projectile->enabled == UINT8_C(0) ||
        scratch->projectile_state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE)
    {
        return PF_STATUS_OK;
    }
    if (owner_slot == UINT8_C(0) ||
        owner_index >= (uint32_t)world->player_count)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    for (target_index = UINT32_C(0);
         target_index < (uint32_t)world->player_count;
         ++target_index)
    {
        const int8_t launch_direction =
            scratch->projectile_velocity_x_f32 < INT32_C(0)
                ? INT8_C(-1)
                : INT8_C(1);
        float reflector_left;
        float reflector_right;
        float reflector_top;
        float reflector_bottom;
        const int reflector_active =
            (scratch->action_state[target_index] ==
                 (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
             scratch->action_state[target_index] ==
                 (uint8_t)PF_M4_ACTION_REFLECTOR_AIR) &&
            attack_hitbox(
                content,
                scratch->position_x_f32[target_index],
                scratch->position_y_f32[target_index],
                scratch->facing[target_index],
                scratch->action_state[target_index],
                scratch->action_ticks[target_index],
                scratch->source_submotion[target_index],
                &reflector_left,
                &reflector_right,
                &reflector_top,
                &reflector_bottom) &&
            boxes_overlap(
                hitbox_left,
                hitbox_right,
                hitbox_top,
                hitbox_bottom,
                reflector_left,
                reflector_right,
                reflector_top,
                reflector_bottom);
        const int shield_overlap =
            hitbox_overlaps_shield(
                &content->fighter,
                scratch,
                target_index,
                hitbox_left,
                hitbox_right,
                hitbox_top,
                hitbox_bottom);

        if (target_index == owner_index ||
            scratch->active[target_index] == UINT8_C(0) ||
            scratch->respawn_invulnerability_ticks[target_index] !=
                UINT16_C(0) ||
            scratch->ledge_invulnerability_ticks[target_index] !=
                UINT16_C(0) ||
            action_is_match_entry(
                scratch->action_state[target_index]) ||
            scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
            action_is_recovery_invulnerable(
                &content->fighter,
                scratch->action_state[target_index],
                scratch->action_ticks[target_index],
                scratch->prone_orientation[target_index],
                scratch->tech_direction[target_index],
                scratch->facing[target_index]) ||
            (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
             falcon_reference_body_invulnerable(
                 scratch->source_submotion[target_index],
                 scratch->action_ticks[target_index])) ||
            (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
             world->team[owner_index] == world->team[target_index]) ||
            (reflector_active == 0 &&
             !hitbox_overlaps_player_or_shield(
                  &content->fighter,
                  scratch,
                  target_index,
                 hitbox_left,
                 hitbox_right,
                 hitbox_top,
                 hitbox_bottom)))
        {
            continue;
        }

        if (reflector_active != 0)
        {
            const float reflected_velocity_x =
                -scratch->projectile_velocity_x_f32;

            scratch->projectile_velocity_x_f32 =
                reflected_velocity_x;
            scratch->projectile_owner_slot =
                (uint8_t)(target_index + UINT32_C(1));
            scratch->powershield[target_index] = UINT8_C(0);
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_PROJECTILE_REFLECT,
                    (uint8_t)target_index,
                    (uint8_t)owner_index,
                    UINT32_C(0),
                    reflected_velocity_x,
                    scratch->projectile_velocity_y_f32,
                    UINT16_C(0),
                    (uint16_t)scratch->action_state[target_index],
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            return PF_STATUS_OK;
        }

        if (action_is_guarding(
                scratch->action_state[target_index]) &&
            shield_overlap != 0 &&
            scratch->action_state[target_index] ==
                (uint8_t)PF_M4_ACTION_SHIELD &&
            scratch->action_ticks[target_index] <=
                projectile->powershield_reflect_window_ticks &&
            scratch->shield_strength[target_index] >=
                content->fighter.digital_trigger_threshold)
        {
            const float reflected_velocity_x =
                -scratch->projectile_velocity_x_f32;

            scratch->projectile_velocity_x_f32 =
                reflected_velocity_x;
            scratch->projectile_owner_slot =
                (uint8_t)(target_index + UINT32_C(1));
            scratch->powershield[target_index] = UINT8_C(1);
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_PROJECTILE_REFLECT,
                    (uint8_t)target_index,
                    (uint8_t)owner_index,
                    UINT32_C(0),
                    reflected_velocity_x,
                    scratch->projectile_velocity_y_f32,
                    UINT16_C(0),
                    (uint16_t)PF_M4_ACTION_SHIELD,
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            return PF_STATUS_OK;
        }

        if (break_player_grab_links(
                world,
                scratch,
                target_index) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_is_guarding(
                scratch->action_state[target_index]) &&
            shield_overlap != 0)
        {
            const uint8_t move_id =
                (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND;
            const float damage_f32 =
                stale_scaled_damage_f32(
                    &content->fighter,
                    scratch,
                    owner_index,
                    move_id,
                    projectile->damage_f32);
            if (apply_shield_hit(
                    world,
                    scratch,
                    &content->fighter,
                    owner_index,
                    target_index,
                    damage_f32,
                    INT16_C(0),
                    projectile->hitlag_ticks,
                    (int32_t)launch_direction,
                    0,
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND,
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            clear_projectile_combat(scratch);
            return PF_STATUS_OK;
        }

        {
            const uint8_t move_id =
                (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND;
            const float damage_f32 =
                stale_scaled_damage_f32(
                    &content->fighter,
                    scratch,
                    owner_index,
                    move_id,
                    projectile->damage_f32);
            const float resulting_damage =
                saturating_damage(
                    scratch->damage_f32[target_index],
                    damage_f32);
            const float knockback_x = scaled_knockback(
                projectile->base_knockback_x_f32,
                projectile->knockback_growth_f32,
                resulting_damage,
                0);
            const float knockback_y = scaled_knockback(
                projectile->base_knockback_y_f32,
                projectile->knockback_growth_f32,
                resulting_damage,
                1);

            if (apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    rng_state,
                    (uint8_t)owner_index,
                    target_index,
                    damage_f32,
                    (int32_t)launch_direction * knockback_x,
                    -knockback_y,
                    projectile->hitlag_ticks,
                    UINT16_MAX,
                    0,
                    0,
                    UINT8_C(0),
                    UINT8_C(1),
                    UINT8_C(0),
                    PF_SIM_EVENT_PROJECTILE_HIT,
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND) !=
                PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            register_stale_move(
                scratch,
                owner_index,
                move_id);
        }
        clear_projectile_combat(scratch);
        return PF_STATUS_OK;
    }
    return PF_STATUS_OK;
}

static pf_status resolve_falcon_side_special_searches(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    const falcon_side_special_timing *timing =
        falcon_reference_side_special_timing();
    const falcon_special_attributes *attributes =
        falcon_reference_special_attributes();
    uint32_t attacker_index;

    if (timing == NULL || attributes == NULL)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        const uint8_t action = scratch->action_state[attacker_index];
        const int airborne =
            action == (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR;
        const uint16_t action_frame = scratch->action_ticks[attacker_index];
        const uint16_t first_frame = airborne != 0
                                         ? timing->air_search_begin_frame
                                         : timing->ground_search_begin_frame;
        const uint16_t last_frame = airborne != 0
                                        ? timing->air_search_end_frame
                                        : timing->ground_search_end_frame;
        const reference_search_sphere *source_spheres;
        uint8_t sphere_count;
        uint32_t target_index;

        if (scratch->active[attacker_index] == UINT8_C(0) ||
            (action !=
                 (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND &&
             airborne == 0) ||
            action_frame < first_frame || action_frame > last_frame)
        {
            continue;
        }
        source_spheres =
            falcon_reference_side_special_search_spheres(
                airborne,
                &sphere_count);
        if (source_spheres == NULL || sphere_count == UINT8_C(0) ||
            sphere_count > UINT8_C(PF_M4_INSPECTION_HIT_SPHERE_CAPACITY))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            uint8_t sphere_index;
            int detected = 0;

            if (target_index == attacker_index ||
                scratch->active[target_index] == UINT8_C(0) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[attacker_index] == world->team[target_index]))
            {
                continue;
            }
            for (sphere_index = UINT8_C(0);
                 sphere_index < sphere_count;
                 ++sphere_index)
            {
                const reference_search_sphere *source =
                    &source_spheres[sphere_index];
                const hit_sphere_inspection sphere = {
                    .center_x_f32 =
                        scratch->position_x_f32[attacker_index] +
                        (int32_t)scratch->facing[attacker_index] *
                            source->offset_x_f32,
                    .center_y_f32 =
                        scratch->position_y_f32[attacker_index] +
                        source->offset_y_f32,
                    .center_z_f32 =
                        (int32_t)scratch->facing[attacker_index] *
                        source->offset_z_f32,
                    .radius_f32 = source->radius_f32,
                    .effect_index = UINT8_C(0),
                    .hitbox_id = sphere_index,
                    .group_id = UINT8_C(0),
                    .collision_state = UINT8_C(2)};

                if (hit_sphere_overlaps_player(
                        &content->fighter,
                        scratch,
                        attacker_index,
                        target_index,
                        scratch->position_y_f32[attacker_index],
                        &sphere,
                        &sphere))
                {
                    detected = 1;
                    break;
                }
            }
            if (detected != 0)
            {
                set_action_state(
                    world,
                    scratch,
                    attacker_index,
                    airborne != 0
                        ? (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR
                        : (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND);
                scratch->action_ticks[attacker_index] = UINT16_C(0);
                scratch->attack_hit_mask[attacker_index] = UINT8_C(0);
                scratch->attack_stale_registered[attacker_index] =
                    UINT8_C(0);
                if (airborne == 0)
                {
                    scratch->velocity_x_f32[attacker_index] =
                        multiply_f32(
                            scratch->velocity_x_f32[attacker_index],
                            attributes->specials_gr_vel_x_f32);
                    scratch->velocity_y_f32[attacker_index] = INT32_C(0);
                }
                break;
            }
        }
    }
    return PF_STATUS_OK;
}

static int select_reference_target_hit(
    const struct content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    uint8_t attacker_action,
    falcon_move_index geometry_move,
    const attack_runtime *base_attack,
    const hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t sphere_count,
    const hit_sphere_inspection
        previous_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t previous_sphere_count,
    uint16_t cancelled_group_mask,
    attack_runtime *out_attack,
    uint8_t *out_group,
    uint8_t *out_hurtbox_height,
    int *out_shield_overlap)
{
    uint8_t sphere_index;

    if (content == NULL || world == NULL || scratch == NULL ||
        base_attack == NULL || spheres == NULL ||
        previous_spheres == NULL || out_attack == NULL ||
        out_group == NULL || out_hurtbox_height == NULL ||
        out_shield_overlap == NULL)
    {
        return -1;
    }
    for (sphere_index = UINT8_C(0);
         sphere_index < sphere_count;
         ++sphere_index)
    {
        const hit_sphere_inspection *sphere =
            &spheres[sphere_index];
        const hit_sphere_inspection *previous_sphere;
        const reference_hit_effect *effect;
        float previous_attacker_position_y_f32;
        int shield_overlap;
        int player_overlap;

        if (sphere->group_id >= UINT8_C(16))
        {
            return -1;
        }
        if ((cancelled_group_mask &
             (uint16_t)(UINT16_C(1) << sphere->group_id)) != UINT16_C(0))
        {
            continue;
        }
        previous_sphere = previous_matching_hit_sphere(
            sphere,
            previous_spheres,
            previous_sphere_count);
        previous_attacker_position_y_f32 =
            previous_sphere == sphere
                ? scratch->position_y_f32[attacker_index]
                : world->position_y_f32[attacker_index];
        shield_overlap = hit_sphere_overlaps_shield(
            &content->fighter,
            scratch,
            attacker_index,
            target_index,
            previous_attacker_position_y_f32,
            previous_sphere,
            sphere);
        player_overlap = hit_sphere_overlaps_player_with_height(
            &content->fighter,
            scratch,
            attacker_index,
            target_index,
            previous_attacker_position_y_f32,
            previous_sphere,
            sphere,
            out_hurtbox_height);
        effect = falcon_reference_effect(
            geometry_move,
            sphere->effect_index);
        if (effect == NULL)
        {
            return -1;
        }
        if ((player_overlap == 0 && shield_overlap == 0) ||
            (shield_overlap == 0 &&
             ((scratch->grounded[target_index] != UINT8_C(0) &&
               effect->hits_grounded == UINT8_C(0)) ||
              (scratch->grounded[target_index] == UINT8_C(0) &&
               effect->hits_airborne == UINT8_C(0)))))
        {
            continue;
        }
        copy_attack_state(out_attack, base_attack);
        apply_falcon_reference_effect(
            &content->fighter,
            attacker_action,
            scratch->smash_charge_ticks[attacker_index],
            effect,
            out_attack);
        apply_stale_scaled_reference_damage(
            &content->fighter,
            scratch,
            attacker_index,
            attacker_action,
            out_attack);
        *out_group = sphere->group_id;
        *out_shield_overlap = shield_overlap;
        return 1;
    }
    return 0;
}

pf_status resolve_combat(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint64_t *rng_state)
{
    uint8_t target_hit_mask[PF_SIM_MAX_PLAYERS];
    uint8_t target_shield_hit_mask[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_action[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_hit[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_blocked[PF_SIM_MAX_PLAYERS];
    uint8_t target_powershield[PF_SIM_MAX_PLAYERS];
    uint16_t rebound_damage[PF_SIM_MAX_PLAYERS];
    uint8_t rebound_source[PF_SIM_MAX_PLAYERS];
    uint16_t cancelled_group_mask[PF_SIM_MAX_PLAYERS];
    float attacker_shield_pushback_f32[PF_SIM_MAX_PLAYERS];
    attack_runtime attacker_attack[PF_SIM_MAX_PLAYERS];
    attack_runtime attacker_base_attack[PF_SIM_MAX_PLAYERS];
    attack_runtime
        target_attack[PF_SIM_MAX_PLAYERS][PF_SIM_MAX_PLAYERS];
    uint8_t
        target_attack_group[PF_SIM_MAX_PLAYERS][PF_SIM_MAX_PLAYERS];
    uint8_t
        target_hurtbox_height[PF_SIM_MAX_PLAYERS][PF_SIM_MAX_PLAYERS];
    hit_sphere_inspection
        attacker_spheres[PF_SIM_MAX_PLAYERS]
                        [PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    hit_sphere_inspection
        attacker_previous_spheres[PF_SIM_MAX_PLAYERS]
                                 [PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    falcon_move_index geometry_move[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_sphere_count[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_previous_sphere_count[PF_SIM_MAX_PLAYERS];
    uint32_t attacker_index;
    uint32_t target_index;

    if (content == NULL || world == NULL || scratch == NULL ||
        rng_state == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        resolve_falcon_side_special_searches(
            content,
            world,
            scratch) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    (void)memset(target_hit_mask, 0, sizeof(target_hit_mask));
    (void)memset(
        target_shield_hit_mask,
        0,
        sizeof(target_shield_hit_mask));
    (void)memset(attacker_action, UINT8_MAX, sizeof(attacker_action));
    (void)memset(attacker_hit, 0, sizeof(attacker_hit));
    (void)memset(attacker_blocked, 0, sizeof(attacker_blocked));
    (void)memset(
        attacker_shield_pushback_f32,
        0,
        sizeof(attacker_shield_pushback_f32));
    (void)memset(
        target_powershield,
        0,
        sizeof(target_powershield));
    (void)memset(rebound_damage, 0, sizeof(rebound_damage));
    (void)memset(rebound_source, UINT8_MAX, sizeof(rebound_source));
    (void)memset(cancelled_group_mask, 0, sizeof(cancelled_group_mask));
    (void)memset(target_attack_group, UINT8_MAX, sizeof(target_attack_group));
    (void)memset(target_hurtbox_height, 1, sizeof(target_hurtbox_height));
    (void)memset(
        attacker_sphere_count,
        0,
        sizeof(attacker_sphere_count));
    (void)memset(
        attacker_previous_sphere_count,
        0,
        sizeof(attacker_previous_sphere_count));

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        float hitbox_left;
        float hitbox_right;
        float hitbox_top;
        float hitbox_bottom;
        const uint8_t current_action =
            scratch->action_state[attacker_index];

        if (scratch->active[attacker_index] == UINT8_C(0))
        {
            continue;
        }
        if (!attack_hitbox(
                content,
                scratch->position_x_f32[attacker_index],
                scratch->position_y_f32[attacker_index],
                scratch->facing[attacker_index],
                current_action,
                scratch->action_ticks[attacker_index],
                scratch->source_submotion[attacker_index],
                &hitbox_left,
                &hitbox_right,
                &hitbox_top,
                &hitbox_bottom))
        {
            falcon_move_index inactive_move;

            /* Attack scripts clear their victim records with their hitboxes.
             * A later create in the same action may hit the same victim again
             * (Falcon Nair's early and late hits are the canonical case).
             * HITLAG is represented by a distinct action, so frozen active
             * hitboxes never pass through this inactive-phase reset. */
            if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
                falcon_reference_move_for_action(
                    current_action,
                    &inactive_move) &&
                falcon_reference_has_hit_geometry(inactive_move))
            {
                scratch->attack_hit_mask[attacker_index] = UINT8_C(0);
            }
            continue;
        }
        attacker_action[attacker_index] = current_action;
        if (!attack_for_state(
                content,
                attacker_action[attacker_index],
                scratch->action_ticks[attacker_index],
                scratch->source_submotion[attacker_index],
                scratch->charge_ticks[attacker_index],
                scratch->smash_charge_ticks[attacker_index],
                &attacker_attack[attacker_index]))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        attacker_sphere_count[attacker_index] =
            attack_hit_spheres(
                content,
                scratch->position_x_f32[attacker_index],
                scratch->position_y_f32[attacker_index],
                scratch->facing[attacker_index],
                scratch->action_state[attacker_index],
                scratch->action_ticks[attacker_index],
                attacker_spheres[attacker_index]);
        if (attacker_sphere_count[attacker_index] != UINT8_C(0) &&
            reference_hit_sphere_can_continue(
                world,
                scratch,
                attacker_index))
        {
            attacker_previous_sphere_count[attacker_index] =
                attack_hit_spheres(
                    content,
                    world->position_x_f32[attacker_index],
                    world->position_y_f32[attacker_index],
                    world->facing[attacker_index],
                    world->action_state[attacker_index],
                    world->action_ticks[attacker_index],
                    attacker_previous_spheres[attacker_index]);
        }
        if (attacker_sphere_count[attacker_index] != UINT8_C(0))
        {
            if (!falcon_reference_move_for_action(
                    attacker_action[attacker_index],
                    &geometry_move[attacker_index]))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        else if (apply_falcon_reference_frame(
                     &content->fighter,
                     attacker_action[attacker_index],
                     scratch->action_ticks[attacker_index],
                     scratch->smash_charge_ticks[attacker_index],
                     &attacker_attack[attacker_index]) < 0)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (attacker_sphere_count[attacker_index] == UINT8_C(0))
        {
            apply_stale_scaled_reference_damage(
                &content->fighter,
                scratch,
                attacker_index,
                attacker_action[attacker_index],
                &attacker_attack[attacker_index]);
        }
        copy_attack_state(
            &attacker_base_attack[attacker_index],
            &attacker_attack[attacker_index]);

        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            const uint8_t target_bit =
                (uint8_t)(UINT32_C(1) << target_index);
            const int uses_reference_spheres =
                attacker_sphere_count[attacker_index] != UINT8_C(0);
            int shield_overlap =
                uses_reference_spheres != 0
                    ? 0
                    : hitbox_overlaps_shield(
                          &content->fighter,
                          scratch,
                          target_index,
                          hitbox_left,
                          hitbox_right,
                          hitbox_top,
                          hitbox_bottom);

            if (target_index == attacker_index ||
                scratch->grab_target_slot[attacker_index] ==
                    (uint8_t)(target_index + UINT32_C(1)) ||
                scratch->active[target_index] == UINT8_C(0) ||
                scratch
                        ->respawn_invulnerability_ticks[target_index] !=
                    UINT16_C(0) ||
                scratch->ledge_invulnerability_ticks[target_index] !=
                    UINT16_C(0) ||
                action_is_match_entry(
                    scratch->action_state[target_index]) ||
                action_is_recovery_invulnerable(
                    &content->fighter,
                    scratch->action_state[target_index],
                    scratch->action_ticks[target_index],
                    scratch->prone_orientation[target_index],
                    scratch->tech_direction[target_index],
                    scratch->facing[target_index]) ||
                (content->fighter.reference_frame_data_enabled !=
                     UINT8_C(0) &&
                 falcon_reference_body_invulnerable(
                     scratch->source_submotion[target_index],
                     scratch->action_ticks[target_index])) ||
                (scratch->attack_hit_mask[attacker_index] &
                 target_bit) != UINT8_C(0) ||
                scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[attacker_index] ==
                     world->team[target_index]) ||
                (uses_reference_spheres == 0 &&
                 !hitbox_overlaps_player_or_shield(
                     &content->fighter,
                     scratch,
                     target_index,
                     hitbox_left,
                     hitbox_right,
                     hitbox_top,
                     hitbox_bottom)))
            {
                continue;
            }

            if (attacker_sphere_count[attacker_index] != UINT8_C(0))
            {
                const int selection = select_reference_target_hit(
                    content,
                    world,
                    scratch,
                    attacker_index,
                    target_index,
                    attacker_action[attacker_index],
                    geometry_move[attacker_index],
                    &attacker_base_attack[attacker_index],
                    attacker_spheres[attacker_index],
                    attacker_sphere_count[attacker_index],
                    attacker_previous_spheres[attacker_index],
                    attacker_previous_sphere_count[attacker_index],
                    UINT16_C(0),
                    &target_attack[target_index][attacker_index],
                    &target_attack_group[target_index][attacker_index],
                    &target_hurtbox_height[target_index][attacker_index],
                    &shield_overlap);

                if (selection < 0)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (selection == 0)
                {
                    continue;
                }
                if (attacker_hit[attacker_index] == UINT8_C(0) ||
                    target_attack[target_index][attacker_index]
                            .hitlag_ticks >
                        attacker_attack[attacker_index].hitlag_ticks)
                {
                    copy_attack_state(
                        &attacker_attack[attacker_index],
                        &target_attack[target_index][attacker_index]);
                }
            }
            else
            {
                copy_attack_state(
                    &target_attack[target_index][attacker_index],
                    &attacker_attack[attacker_index]);
            }

            target_hit_mask[target_index] |=
                (uint8_t)(UINT32_C(1) << attacker_index);
            attacker_hit[attacker_index] = UINT8_C(1);
            if (action_is_guarding(
                    scratch->action_state[target_index]) &&
                shield_overlap != 0)
            {
                target_shield_hit_mask[target_index] |=
                    (uint8_t)(UINT32_C(1) << attacker_index);
                attacker_blocked[attacker_index] = UINT8_C(1);
                if (scratch->action_state[target_index] ==
                        (uint8_t)PF_M4_ACTION_SHIELD &&
                    scratch->action_ticks[target_index] <=
                        content->fighter
                            .powershield_window_ticks &&
                    scratch->shield_strength[target_index] >=
                        content->fighter
                            .digital_trigger_threshold)
                {
                    target_powershield[target_index] =
                        UINT8_C(1);
                }
            }
        }
    }

    /* Fighter hitboxes clank only while both owners are grounded. The DAT
     * packs clank in interaction bit 1 and rebound in bit 0. Resolve the
     * imported moving capsules before hurt/shield delivery, matching
     * ftColl_8007699C's strict nine-damage comparison. */
    if (content->fighter.reference_frame_data_enabled != UINT8_C(0))
    {
        const ssbm_clank_attributes *clank =
            ssbm_common_reference_clank();
        uint32_t first_index;

        if (clank == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        for (first_index = UINT32_C(0);
             first_index < (uint32_t)world->player_count;
             ++first_index)
        {
            uint32_t second_index;

            if (attacker_sphere_count[first_index] == UINT8_C(0) ||
                scratch->grounded[first_index] == UINT8_C(0))
            {
                continue;
            }
            for (second_index = first_index + UINT32_C(1);
                 second_index < (uint32_t)world->player_count;
                 ++second_index)
            {
                uint8_t first_sphere_index;

                if (attacker_sphere_count[second_index] == UINT8_C(0) ||
                    scratch->grounded[second_index] == UINT8_C(0))
                {
                    continue;
                }
                for (first_sphere_index = UINT8_C(0);
                     first_sphere_index <
                         attacker_sphere_count[first_index];
                     ++first_sphere_index)
                {
                    const hit_sphere_inspection *first_sphere =
                        &attacker_spheres[first_index][first_sphere_index];
                    const reference_hit_effect *first_effect =
                        falcon_reference_effect(
                            geometry_move[first_index],
                            first_sphere->effect_index);
                    uint8_t second_sphere_index;

                    if (first_effect == NULL ||
                        first_sphere->group_id >= UINT8_C(16))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    if ((cancelled_group_mask[first_index] &
                         (uint16_t)(UINT16_C(1) <<
                                    first_sphere->group_id)) != UINT16_C(0))
                    {
                        continue;
                    }
                    if ((first_effect->interaction & UINT8_C(2)) ==
                            UINT8_C(0) ||
                        first_effect->element ==
                            (uint8_t)PF_M4_REFERENCE_HIT_EMPTY ||
                        first_effect->element ==
                            (uint8_t)PF_M4_REFERENCE_HIT_GRAB)
                    {
                        continue;
                    }
                    for (second_sphere_index = UINT8_C(0);
                         second_sphere_index <
                             attacker_sphere_count[second_index];
                         ++second_sphere_index)
                    {
                        const hit_sphere_inspection *second_sphere =
                            &attacker_spheres[second_index]
                                             [second_sphere_index];
                        const reference_hit_effect *second_effect =
                            falcon_reference_effect(
                                geometry_move[second_index],
                                second_sphere->effect_index);
                        const hit_sphere_inspection *first_previous;
                        const hit_sphere_inspection *second_previous;
                        collision_capsule3_f32 first_capsule;
                        collision_capsule3_f32 second_capsule;
                        attack_runtime first_selected;
                        attack_runtime second_selected;
                        uint16_t first_damage;
                        uint16_t second_damage;
                        int cancel_first;
                        int cancel_second;

                        if (second_effect == NULL ||
                            second_sphere->group_id >= UINT8_C(16))
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                        if ((cancelled_group_mask[second_index] &
                             (uint16_t)(UINT16_C(1) <<
                                        second_sphere->group_id)) !=
                            UINT16_C(0))
                        {
                            continue;
                        }
                        if ((second_effect->interaction & UINT8_C(2)) ==
                                UINT8_C(0) ||
                            second_effect->element ==
                                (uint8_t)PF_M4_REFERENCE_HIT_EMPTY ||
                            second_effect->element ==
                                (uint8_t)PF_M4_REFERENCE_HIT_GRAB)
                        {
                            continue;
                        }
                        first_previous = previous_matching_hit_sphere(
                            first_sphere,
                            attacker_previous_spheres[first_index],
                            attacker_previous_sphere_count[first_index]);
                        second_previous = previous_matching_hit_sphere(
                            second_sphere,
                            attacker_previous_spheres[second_index],
                            attacker_previous_sphere_count[second_index]);
                        first_capsule = (collision_capsule3_f32){
                            first_previous->center_x_f32,
                            first_previous->center_y_f32,
                            first_previous->center_z_f32,
                            first_sphere->center_x_f32,
                            first_sphere->center_y_f32,
                            first_sphere->center_z_f32,
                            first_sphere->radius_f32};
                        second_capsule = (collision_capsule3_f32){
                            second_previous->center_x_f32,
                            second_previous->center_y_f32,
                            second_previous->center_z_f32,
                            second_sphere->center_x_f32,
                            second_sphere->center_y_f32,
                            second_sphere->center_z_f32,
                            second_sphere->radius_f32};
                        if (!collision_capsule_capsule_overlap_f32(
                                &first_capsule,
                                &second_capsule))
                        {
                            continue;
                        }
                        copy_attack_state(
                            &first_selected,
                            &attacker_base_attack[first_index]);
                        apply_falcon_reference_effect(
                            &content->fighter,
                            attacker_action[first_index],
                            scratch->smash_charge_ticks[first_index],
                            first_effect,
                            &first_selected);
                        apply_stale_scaled_reference_damage(
                            &content->fighter,
                            scratch,
                            first_index,
                            attacker_action[first_index],
                            &first_selected);
                        copy_attack_state(
                            &second_selected,
                            &attacker_base_attack[second_index]);
                        apply_falcon_reference_effect(
                            &content->fighter,
                            attacker_action[second_index],
                            scratch->smash_charge_ticks[second_index],
                            second_effect,
                            &second_selected);
                        apply_stale_scaled_reference_damage(
                            &content->fighter,
                            scratch,
                            second_index,
                            attacker_action[second_index],
                            &second_selected);
                        first_damage = (uint16_t)floorf(first_selected.damage_f32);
                        second_damage = (uint16_t)floorf(second_selected.damage_f32);
                        if (first_selected.damage_f32 != UINT32_C(0) &&
                            first_damage == UINT16_C(0))
                        {
                            first_damage = UINT16_C(1);
                        }
                        if (second_selected.damage_f32 != UINT32_C(0) &&
                            second_damage == UINT16_C(0))
                        {
                            second_damage = UINT16_C(1);
                        }
                        cancel_first =
                            (int32_t)first_damage -
                                (int32_t)clank->damage_margin <
                            (int32_t)second_damage;
                        cancel_second =
                            (int32_t)second_damage -
                                (int32_t)clank->damage_margin <
                            (int32_t)first_damage;
                        if (cancel_first != 0)
                        {
                            if (first_sphere->group_id >= UINT8_C(16))
                            {
                                return PF_STATUS_DETERMINISTIC_FAULT;
                            }
                            cancelled_group_mask[first_index] |=
                                (uint16_t)(UINT16_C(1) <<
                                           first_sphere->group_id);
                            if ((first_effect->interaction & UINT8_C(1)) !=
                                    UINT8_C(0) &&
                                first_damage > rebound_damage[first_index])
                            {
                                rebound_damage[first_index] = first_damage;
                                rebound_source[first_index] =
                                    (uint8_t)second_index;
                            }
                        }
                        if (cancel_second != 0)
                        {
                            if (second_sphere->group_id >= UINT8_C(16))
                            {
                                return PF_STATUS_DETERMINISTIC_FAULT;
                            }
                            cancelled_group_mask[second_index] |=
                                (uint16_t)(UINT16_C(1) <<
                                           second_sphere->group_id);
                            if ((second_effect->interaction & UINT8_C(1)) !=
                                    UINT8_C(0) &&
                                second_damage > rebound_damage[second_index])
                            {
                                rebound_damage[second_index] = second_damage;
                                rebound_source[second_index] =
                                    (uint8_t)first_index;
                            }
                        }
                    }
                }
            }
        }
        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            for (attacker_index = UINT32_C(0);
                 attacker_index < (uint32_t)world->player_count;
                 ++attacker_index)
            {
                const uint8_t bit =
                    (uint8_t)(UINT8_C(1) << attacker_index);
                const uint8_t group =
                    target_attack_group[target_index][attacker_index];
                int shield_overlap = 0;
                int selection;

                if ((target_hit_mask[target_index] & bit) == UINT8_C(0) ||
                    attacker_sphere_count[attacker_index] == UINT8_C(0) ||
                    group >= UINT8_C(16) ||
                    (cancelled_group_mask[attacker_index] &
                     (uint16_t)(UINT16_C(1) << group)) == UINT16_C(0))
                {
                    continue;
                }
                target_hit_mask[target_index] &= (uint8_t)~bit;
                target_shield_hit_mask[target_index] &= (uint8_t)~bit;
                target_attack_group[target_index][attacker_index] =
                    UINT8_MAX;
                selection = select_reference_target_hit(
                    content,
                    world,
                    scratch,
                    attacker_index,
                    target_index,
                    attacker_action[attacker_index],
                    geometry_move[attacker_index],
                    &attacker_base_attack[attacker_index],
                    attacker_spheres[attacker_index],
                    attacker_sphere_count[attacker_index],
                    attacker_previous_spheres[attacker_index],
                    attacker_previous_sphere_count[attacker_index],
                    cancelled_group_mask[attacker_index],
                    &target_attack[target_index][attacker_index],
                    &target_attack_group[target_index][attacker_index],
                    &target_hurtbox_height[target_index][attacker_index],
                    &shield_overlap);
                if (selection < 0)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (selection != 0)
                {
                    target_hit_mask[target_index] |= bit;
                    if (action_is_guarding(
                            scratch->action_state[target_index]) &&
                        shield_overlap != 0)
                    {
                        target_shield_hit_mask[target_index] |= bit;
                    }
                }
            }
        }
        (void)memset(attacker_hit, 0, sizeof(attacker_hit));
        (void)memset(attacker_blocked, 0, sizeof(attacker_blocked));
        for (attacker_index = UINT32_C(0);
             attacker_index < (uint32_t)world->player_count;
             ++attacker_index)
        {
            if (attacker_action[attacker_index] != UINT8_MAX)
            {
                copy_attack_state(
                    &attacker_attack[attacker_index],
                    &attacker_base_attack[attacker_index]);
            }
        }
        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            for (attacker_index = UINT32_C(0);
                 attacker_index < (uint32_t)world->player_count;
                 ++attacker_index)
            {
                const uint8_t bit =
                    (uint8_t)(UINT8_C(1) << attacker_index);
                if ((target_hit_mask[target_index] & bit) != UINT8_C(0))
                {
                    if (attacker_hit[attacker_index] == UINT8_C(0) ||
                        target_attack[target_index][attacker_index]
                            .hitlag_ticks >
                        attacker_attack[attacker_index].hitlag_ticks)
                    {
                        copy_attack_state(
                            &attacker_attack[attacker_index],
                            &target_attack[target_index][attacker_index]);
                    }
                    attacker_hit[attacker_index] = UINT8_C(1);
                }
                if ((target_shield_hit_mask[target_index] & bit) !=
                    UINT8_C(0))
                {
                    attacker_blocked[attacker_index] = UINT8_C(1);
                }
            }
        }
    }

    for (target_index = UINT32_C(0);
         target_index < (uint32_t)world->player_count;
         ++target_index)
    {
        const uint8_t hit_mask = target_hit_mask[target_index];
        const uint8_t hurt_mask =
            (uint8_t)(hit_mask &
                      (uint8_t)~target_shield_hit_mask[target_index]);

        if (hit_mask == UINT8_C(0))
        {
            continue;
        }

        if (break_player_grab_links(
                world,
                scratch,
                target_index) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }

        for (attacker_index = UINT32_C(0);
             attacker_index < (uint32_t)world->player_count;
             ++attacker_index)
        {
            const uint8_t attacker_bit =
                (uint8_t)(UINT32_C(1) << attacker_index);

            if ((hit_mask & attacker_bit) == UINT8_C(0))
            {
                continue;
            }
            scratch->attack_hit_mask[attacker_index] |=
                (uint8_t)(UINT32_C(1) << target_index);
            if ((target_shield_hit_mask[target_index] & attacker_bit) !=
                UINT8_C(0))
            {
                const attack_runtime *attack =
                    &target_attack[target_index][attacker_index];
                const int powershield =
                    target_powershield[target_index] != UINT8_C(0);
                const uint16_t target_shield_hitlag_ticks =
                    attack->target_hitlag_multiplier_f32 !=
                            1.0f
                        ? melee_hitlag_ticks(
                              attack->damage_f32,
                              UINT8_C(0),
                              attack->target_hitlag_multiplier_f32)
                        : attack->hitlag_ticks;
                shield_hit_response response;

                if (apply_shield_hit(
                        world,
                        scratch,
                        &content->fighter,
                        attacker_index,
                        target_index,
                        attack->damage_f32,
                        attack->shield_damage,
                        target_shield_hitlag_ticks,
                        (int32_t)scratch->facing[attacker_index] *
                            (int32_t)attack->direction,
                        powershield,
                        (uint16_t)attacker_action[attacker_index],
                        &response) != PF_STATUS_OK)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (attacker_shield_pushback_f32[attacker_index] ==
                    INT32_C(0))
                {
                    attacker_shield_pushback_f32[attacker_index] =
                        response.attacker_pushback_f32;
                }
            }
        }

        if (hurt_mask != UINT8_C(0))
        {
            float total_damage_f32 = UINT32_C(0);
            float resulting_damage;
            uint32_t best_owner = UINT32_MAX;
            float best_knockback_f32 = INT32_C(-1);
            float launch_velocity_x = 0.0f;
            float launch_velocity_y = 0.0f;
            uint16_t resolved_hitlag_ticks = UINT16_C(0);
            uint16_t resolved_hitstun_ticks = UINT16_MAX;
            int velocity_is_weighted = 0;
            int launch_grounded = 0;
            uint8_t damage_level = UINT8_C(0);
            uint8_t meteor_cancellable = UINT8_C(0);

            for (attacker_index = UINT32_C(0);
                 attacker_index < (uint32_t)world->player_count;
                 ++attacker_index)
            {
                const uint8_t attacker_bit =
                    (uint8_t)(UINT32_C(1) << attacker_index);

                if ((hurt_mask & attacker_bit) != UINT8_C(0))
                {
                    total_damage_f32 = saturating_damage(
                        total_damage_f32,
                        target_attack[target_index][attacker_index]
                            .damage_f32);
                }
            }
            resulting_damage = saturating_damage(
                scratch->damage_f32[target_index],
                total_damage_f32);
            /* ftColl uses (s32) x1830_percent plus the current collision
             * batch's x1838_percentTemp.  Keep displayed damage fractional,
             * but deliberately truncate the pre-hit percent for knockback. */
            const float knockback_percent_f32 =
                saturating_damage(
                    floorf(scratch->damage_f32[target_index]),
                    total_damage_f32);

            for (attacker_index = UINT32_C(0);
                 attacker_index < (uint32_t)world->player_count;
                 ++attacker_index)
            {
                const uint8_t attacker_bit =
                    (uint8_t)(UINT32_C(1) << attacker_index);
                const attack_runtime *attack;
                float candidate_velocity_x;
                float candidate_velocity_y;
                float candidate_knockback_f32;
                uint16_t candidate_hitlag_ticks;
                uint16_t candidate_hitstun_ticks = UINT16_MAX;
                int candidate_velocity_is_weighted = 0;
                int candidate_launch_grounded = 0;
                uint8_t candidate_damage_level = UINT8_C(0);
                uint8_t candidate_meteor_cancellable = UINT8_C(0);

                if ((hurt_mask & attacker_bit) == UINT8_C(0))
                {
                    continue;
                }
                attack = &target_attack[target_index][attacker_index];
                candidate_hitlag_ticks = attack->hitlag_ticks;
                if (attacker_action[attacker_index] ==
                        (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
                    attacker_action[attacker_index] ==
                        (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND)
                {
                    const falcon_special_attributes *attributes =
                        falcon_reference_special_attributes();

                    if (attributes == NULL ||
                        attributes->speciallw_unk2 < INT32_C(0))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    if ((int32_t)scratch
                            ->falcon_kick_hit_count[attacker_index] <=
                        attributes->speciallw_unk2)
                    {
                        ++scratch
                              ->falcon_kick_hit_count[attacker_index];
                    }
                }
                if (attack->melee_knockback != NULL)
                {
                    const melee_knockback_result result =
                        melee_knockback_for_state(
                            attack->melee_knockback,
                            content->fighter.knockback_weight,
                            attack->knockback_damage_f32 != UINT32_C(0)
                                ? attack->knockback_damage_f32
                                : attack->damage_f32,
                            attack->damage_f32,
                            knockback_percent_f32,
                            scratch->grounded[target_index],
                            (uint8_t)(
                                scratch->action_state[target_index] ==
                                    (uint8_t)PF_M4_ACTION_CROUCH_START ||
                                scratch->action_state[target_index] ==
                                    (uint8_t)PF_M4_ACTION_CROUCH),
                            (uint8_t)(
                                scratch->smash_charge_ticks[target_index] !=
                                    UINT16_C(0) &&
                                (scratch->action_state[target_index] ==
                                     (uint8_t)
                                         PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
                                 scratch->action_state[target_index] ==
                                     (uint8_t)
                                         PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
                                 scratch->action_state[target_index] ==
                                     (uint8_t)
                                         PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW ||
                                 scratch->action_state[target_index] ==
                                     (uint8_t)
                                         PF_M4_ACTION_UP_STRONG_CHARGE ||
                                 scratch->action_state[target_index] ==
                                     (uint8_t)
                                         PF_M4_ACTION_DOWN_STRONG_CHARGE)),
                            attack->target_hitlag_multiplier_f32);

                    /* ftColl selects a fighter hit's horizontal damage
                     * direction from the victim/attacker spatial relation,
                     * then Damage applies velocity away from the attacker.
                     * It is intentionally independent of attacker facing:
                     * crossed-up aerials must launch through the attacker. */
                    candidate_velocity_x =
                        scratch->position_x_f32[target_index] >
                                scratch->position_x_f32[attacker_index]
                            ? result.velocity_x_f32
                            : -result.velocity_x_f32;
                    candidate_velocity_y = -result.velocity_y_f32;
                    candidate_knockback_f32 = result.knockback_f32;
                    candidate_hitlag_ticks = result.hitlag_ticks;
                    candidate_hitstun_ticks = result.hitstun_ticks;
                    candidate_velocity_is_weighted = 1;
                    candidate_launch_grounded =
                        result.grounded_launch != UINT8_C(0);
                    candidate_damage_level = result.damage_level;
                    candidate_meteor_cancellable =
                        result.meteor_cancellable;
                }
                else
                {
                    const float knockback_x = scaled_knockback(
                        attack->base_knockback_x_f32,
                        attack->knockback_growth_f32,
                        resulting_damage,
                        0);
                    const float knockback_y = scaled_knockback(
                        attack->base_knockback_y_f32,
                        attack->knockback_growth_f32,
                        resulting_damage,
                        1);

                    candidate_velocity_x =
                        (float)scratch->facing[attacker_index] *
                        (float)attack->direction * knockback_x;
                    candidate_velocity_y =
                        (float)attack->vertical_direction * knockback_y;
                    candidate_knockback_f32 =
                        knockback_x > knockback_y ? knockback_x : knockback_y;
                }
                if (candidate_knockback_f32 > best_knockback_f32)
                {
                    best_owner = attacker_index;
                    best_knockback_f32 = candidate_knockback_f32;
                    launch_velocity_x = candidate_velocity_x;
                    launch_velocity_y = candidate_velocity_y;
                    resolved_hitlag_ticks = candidate_hitlag_ticks;
                    resolved_hitstun_ticks = candidate_hitstun_ticks;
                    velocity_is_weighted = candidate_velocity_is_weighted;
                    launch_grounded = candidate_launch_grounded;
                    damage_level = candidate_damage_level;
                    meteor_cancellable = candidate_meteor_cancellable;
                }
                if (scratch->attack_stale_registered[attacker_index] ==
                    UINT8_C(0))
                {
                    register_stale_move(
                        scratch,
                        attacker_index,
                        stale_move_id_for_action(
                            attacker_action[attacker_index]));
                    scratch->attack_stale_registered[attacker_index] =
                        UINT8_C(1);
                }
            }
            if (best_owner == UINT32_MAX ||
                apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    rng_state,
                    (uint8_t)best_owner,
                    target_index,
                    total_damage_f32,
                    launch_velocity_x,
                    launch_velocity_y,
                    resolved_hitlag_ticks,
                    resolved_hitstun_ticks,
                    velocity_is_weighted,
                    launch_grounded,
                    damage_level,
                    target_hurtbox_height[target_index][best_owner],
                    meteor_cancellable,
                    PF_SIM_EVENT_HIT,
                    (uint16_t)attacker_action[best_owner]) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
    }

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        if (rebound_damage[attacker_index] != UINT16_C(0) &&
            target_hit_mask[attacker_index] == UINT8_C(0))
        {
            const ssbm_clank_attributes *clank =
                ssbm_common_reference_clank();
            const falcon_common_attributes *common =
                falcon_reference_common_attributes();
            float strength_f32;
            float speed_f32;
            float animation_speed_f32;
            uint32_t rebound_duration_ticks;
            const uint8_t source = rebound_source[attacker_index];

            if (clank == NULL || common == NULL ||
                source >= world->player_count)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            strength_f32 =
                (float)rebound_damage[attacker_index] *
                    clank->rebound_strength_damage_scale_f32 +
                clank->rebound_strength_base_f32;
            if (strength_f32 <= 0.0f)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            speed_f32 =
                multiply_f32(
                    strength_f32,
                    clank->rebound_velocity_strength_scale_f32) +
                clank->rebound_velocity_base_f32;
            animation_speed_f32 =
                (common->rebound_animation_length_f32 + 0.1f) /
                strength_f32;
            if (animation_speed_f32 <= 0.0f)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            rebound_duration_ticks = (uint32_t)ceilf(
                common->rebound_animation_length_f32 /
                animation_speed_f32);
            if (rebound_duration_ticks == UINT32_C(0) ||
                rebound_duration_ticks > UINT16_MAX)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            scratch->velocity_x_f32[attacker_index] =
                scratch->position_x_f32[source] >
                        scratch->position_x_f32[attacker_index]
                    ? -speed_f32
                    : speed_f32;
            scratch->velocity_y_f32[attacker_index] = 0.0f;
            scratch->action_ticks[attacker_index] = UINT16_C(0);
            scratch->attack_hit_mask[attacker_index] = UINT8_C(0);
            scratch->attack_stale_registered[attacker_index] = UINT8_C(0);
            scratch->rebound_duration_ticks[attacker_index] =
                (uint16_t)rebound_duration_ticks;
            set_action_state(
                world,
                scratch,
                attacker_index,
                (uint8_t)PF_M4_ACTION_REBOUND_STOP);
        }
    }

    if (resolve_grabs(content, world, scratch, rng_state) !=
        PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        attack_runtime attack;

        if (attacker_hit[attacker_index] != UINT8_C(0) &&
            target_hit_mask[attacker_index] == UINT8_C(0) &&
            scratch->action_state[attacker_index] !=
                (uint8_t)PF_M4_ACTION_REBOUND_STOP &&
            scratch->action_state[attacker_index] !=
                (uint8_t)PF_M4_ACTION_REBOUND)
        {
            attack = attacker_attack[attacker_index];
            if (attacker_blocked[attacker_index] != UINT8_C(0) &&
                scratch->grounded[attacker_index] != UINT8_C(0))
            {
                scratch->shield_recoil_x_f32[attacker_index] =
                    -(float)scratch->facing[attacker_index] *
                    (float)attack.direction *
                    attacker_shield_pushback_f32[attacker_index];
                scratch->shield_recoil_mask =
                    (uint8_t)(
                        scratch->shield_recoil_mask |
                        (uint8_t)(UINT8_C(1) << attacker_index));
            }
            scratch->hitlag_ticks[attacker_index] =
                attack.hitlag_ticks;
            scratch->hitlag_resume_action[attacker_index] =
                attack.action_state;
            set_action_state(
                world,
                scratch,
                attacker_index,
                (uint8_t)PF_M4_ACTION_HITLAG);
        }
    }

    if (resolve_projectile_combat(
            content,
            world,
            scratch,
            rng_state) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    return resolve_item_combat(
        content, world, scratch, rng_state);
}
