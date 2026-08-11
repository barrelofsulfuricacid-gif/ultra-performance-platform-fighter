#include "sim_internal.h"
#include "sim_collision.h"
#include "sim_falcon_frame_data.h"
#include "sim_melee.h"
#include "sim_ssbm_common_data.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static uint32_t pf_m4_saturating_damage(
    uint32_t current,
    uint32_t added)
{
    if (current >= PF_SIM_MAX_DAMAGE_Q16 ||
        added >= PF_SIM_MAX_DAMAGE_Q16 - current)
    {
        return PF_SIM_MAX_DAMAGE_Q16;
    }
    return current + added;
}

uint8_t pf_m4_stale_move_id_for_action(uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

uint32_t pf_m4_stale_move_multiplier_q16(
    const pf_m4_fighter_data *fighter,
    const uint8_t stale_move_ids[PF_SIM_STALE_MOVE_QUEUE_CAPACITY],
    uint8_t stale_move_count,
    uint8_t move_id)
{
    uint32_t reduction_q16 = UINT32_C(0);
    uint32_t stale_index;

    if (fighter == NULL || stale_move_ids == NULL ||
        stale_move_count > PF_SIM_STALE_MOVE_QUEUE_CAPACITY ||
        move_id == UINT8_C(0))
    {
        return (uint32_t)PF_Q16_ONE;
    }
    for (stale_index = UINT32_C(0);
         stale_index < (uint32_t)stale_move_count;
         ++stale_index)
    {
        if (stale_move_ids[stale_index] == move_id)
        {
            reduction_q16 +=
                (uint32_t)fighter
                    ->stale_move_slot_reduction_q16[stale_index];
        }
    }
    return (uint32_t)PF_Q16_ONE - reduction_q16;
}

static uint32_t pf_m4_stale_scaled_damage_q16(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t move_id,
    uint32_t damage_q16)
{
    uint32_t multiplier_q16;

    if (scratch->stale_move_count[player_index] == UINT8_C(0) ||
        move_id == UINT8_C(0))
    {
        return damage_q16;
    }
    multiplier_q16 =
        pf_m4_stale_move_multiplier_q16(
            fighter,
            scratch->stale_move_ids[player_index],
            scratch->stale_move_count[player_index],
            move_id);
    if (multiplier_q16 == (uint32_t)PF_Q16_ONE)
    {
        return damage_q16;
    }

    return (uint32_t)(
        (uint64_t)damage_q16 * (uint64_t)multiplier_q16 /
        (uint64_t)(uint32_t)PF_Q16_ONE);
}

static uint32_t pf_m4_current_move_stale_scaled_damage_q16(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t move_id,
    uint32_t damage_q16)
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
    const uint32_t multiplier_q16 =
        pf_m4_stale_move_multiplier_q16(
            fighter,
            stale_ids,
            effective_count,
            move_id);

    return (uint32_t)(
        (uint64_t)damage_q16 * (uint64_t)multiplier_q16 /
        (uint64_t)(uint32_t)PF_Q16_ONE);
}

static void pf_m4_register_stale_move(
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

static int32_t pf_m4_scaled_knockback(
    int32_t base_q16,
    int32_t growth_q16,
    uint32_t damage_q16,
    int divide_growth)
{
    int64_t growth =
        ((int64_t)growth_q16 * (int64_t)damage_q16) >>
        16U;
    int64_t result;

    if (divide_growth != 0)
    {
        growth /= INT64_C(2);
    }
    result = (int64_t)base_q16 + growth;
    if (result > (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        return PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    return (int32_t)result;
}

static uint16_t pf_m4_hitstun_ticks(
    const pf_m4_fighter_data *fighter,
    int32_t velocity_x_q16,
    int32_t velocity_y_q16)
{
    const int64_t horizontal =
        velocity_x_q16 < INT32_C(0)
            ? -(int64_t)velocity_x_q16
            : (int64_t)velocity_x_q16;
    const int64_t vertical =
        velocity_y_q16 < INT32_C(0)
            ? -(int64_t)velocity_y_q16
            : (int64_t)velocity_y_q16;
    const int64_t divisor =
        (int64_t)fighter->hitstun_velocity_per_tick_q16;
    int64_t ticks = (horizontal + vertical + divisor - INT64_C(1)) /
                    divisor;

    if (ticks < INT64_C(1))
    {
        ticks = INT64_C(1);
    }
    if (ticks > (int64_t)PF_SIM_MAX_HITSTUN_TICKS)
    {
        ticks = (int64_t)PF_SIM_MAX_HITSTUN_TICKS;
    }
    return (uint16_t)ticks;
}

static int32_t pf_m4_scale_velocity_q16(
    int32_t velocity_q16,
    int32_t scale_q16)
{
    return (int32_t)(
        ((int64_t)velocity_q16 * (int64_t)scale_q16) /
        (int64_t)PF_Q16_ONE);
}

static int32_t pf_m4_apply_weight_q16(
    int32_t velocity_q16,
    int32_t weight_q16)
{
    int64_t weighted_velocity =
        (int64_t)velocity_q16 * (int64_t)PF_Q16_ONE /
        (int64_t)weight_q16;

    if (weighted_velocity > (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        return PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    if (weighted_velocity < -(int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        return -PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    return (int32_t)weighted_velocity;
}

static uint16_t pf_m4_scale_hitstun_ticks(
    uint16_t hitstun_ticks,
    int32_t scale_q16)
{
    uint32_t scaled_ticks =
        (uint32_t)(
            ((uint64_t)hitstun_ticks * (uint64_t)(uint32_t)scale_q16) /
            (uint64_t)(uint32_t)PF_Q16_ONE);

    if (hitstun_ticks != UINT16_C(0) && scaled_ticks == UINT32_C(0))
    {
        scaled_ticks = UINT32_C(1);
    }
    return (uint16_t)scaled_ticks;
}

static int pf_m4_action_is_v_cancel_eligible(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_WALL_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
           action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
           action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE;
}

static int pf_m4_player_v_cancelled(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const uint8_t input_age =
        scratch->trigger_input_age[player_index];

    if (scratch->grounded[player_index] != UINT8_C(0) ||
        !pf_m4_action_is_v_cancel_eligible(
            scratch->action_state[player_index]) ||
        input_age >= fighter->v_cancel_window_ticks ||
        (uint16_t)input_age > fighter->tech_lockout_ticks)
    {
        return 0;
    }
    return scratch->tech_lockout_ticks[player_index] ==
           (uint16_t)(fighter->tech_lockout_ticks - input_age);
}

static int pf_m4_action_is_guarding(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN;
}

static int pf_m4_action_has_shield_volume(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return pf_m4_action_is_guarding(action_state) ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
            hitlag_resume_action ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN);
}

static int pf_m4_action_is_recovery_invulnerable(
    const pf_m4_fighter_data *fighter,
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
        const pf_m4_getup_roll_timing *timing =
            pf_m4_getup_roll_timing_for(
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
               pf_m4_getup_attack_invulnerability_ticks_for(
                   fighter,
                   prone_orientation);
}

typedef struct pf_m4_attack_runtime
{
    int32_t hitbox_offset_x_q16;
    int32_t hitbox_offset_y_q16;
    int32_t hitbox_half_width_q16;
    int32_t hitbox_half_height_q16;
    uint32_t damage_q16;
    int32_t base_knockback_x_q16;
    int32_t base_knockback_y_q16;
    int32_t knockback_growth_q16;
    uint16_t active_begin_tick;
    uint16_t active_end_tick;
    uint16_t hitlag_ticks;
    int16_t shield_damage;
    uint32_t target_hitlag_multiplier_q16;
    pf_m4_melee_knockback_data reference_melee_knockback;
    const pf_m4_melee_knockback_data *melee_knockback;
    int8_t direction;
    int8_t vertical_direction;
    uint8_t action_state;
} pf_m4_attack_runtime;

static int pf_m4_action_is_throw(uint8_t action_state);

static void pf_m4_copy_attack_state(
    pf_m4_attack_runtime *destination,
    const pf_m4_attack_runtime *source)
{
    *destination = *source;
    if (source->melee_knockback == &source->reference_melee_knockback)
    {
        destination->melee_knockback =
            &destination->reference_melee_knockback;
    }
}

static const pf_m4_attack_data *pf_m4_ground_attack_data(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

static uint32_t pf_m4_smash_charged_damage(
    const pf_m4_fighter_data *fighter,
    uint32_t base_damage_q16,
    uint16_t charge_ticks)
{
    const uint16_t clamped_ticks =
        charge_ticks < fighter->smash_charge_max_ticks
            ? charge_ticks
            : fighter->smash_charge_max_ticks;
    const uint64_t bonus_damage =
        (uint64_t)base_damage_q16 *
        (uint64_t)fighter->smash_charge_damage_bonus_q16 *
        (uint64_t)clamped_ticks /
        ((uint64_t)UINT32_C(65536) *
         (uint64_t)fighter->smash_charge_max_ticks);

    return base_damage_q16 + (uint32_t)bonus_damage;
}

static uint32_t pf_m4_authored_base_damage_for_action(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    const pf_m4_attack_data *ground_attack =
        pf_m4_ground_attack_data(fighter, action_state);

    if (ground_attack != NULL)
    {
        return ground_attack->damage_q16;
    }
    switch ((pf_m4_action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
            return fighter->jab_damage_q16;
        case PF_M4_ACTION_JAB_FINAL:
            return fighter->jab_final_damage_q16;
        case PF_M4_ACTION_DASH_ATTACK:
            return fighter->dash_attack_damage_q16;
        case PF_M4_ACTION_STRONG_ATTACK:
            return fighter->strong_damage_q16;
        case PF_M4_ACTION_AERIAL_ATTACK:
            return fighter->aerial_damage_q16;
        case PF_M4_ACTION_FORWARD_AERIAL:
            return fighter->forward_aerial.damage_q16;
        case PF_M4_ACTION_BACK_AERIAL:
            return fighter->back_aerial.damage_q16;
        case PF_M4_ACTION_UP_AERIAL:
            return fighter->up_aerial.damage_q16;
        case PF_M4_ACTION_DOWN_AERIAL:
            return fighter->down_aerial.damage_q16;
        case PF_M4_ACTION_PUMMEL:
            return fighter->pummel_damage_q16;
        default:
            return UINT32_C(0);
    }
}

static int pf_m4_reference_knockback_matches(
    const pf_m4_melee_knockback_data *knockback,
    const pf_m4_reference_hit_effect *effect)
{
    return knockback != NULL && effect != NULL &&
           knockback->enabled != UINT8_C(0) &&
           knockback->angle_degrees == effect->angle_degrees &&
           knockback->growth == effect->growth &&
           knockback->weight_set == effect->weight_set &&
           knockback->base == effect->base;
}

static void pf_m4_apply_falcon_reference_effect(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t smash_charge_ticks,
    const pf_m4_reference_hit_effect *effect,
    pf_m4_attack_runtime *attack)
{
    uint32_t damage_q16 =
        (uint32_t)effect->damage * UINT32_C(65536);

    if (pf_m4_action_is_smash_release(action_state) &&
        smash_charge_ticks != UINT16_C(0))
    {
        damage_q16 = pf_m4_smash_charged_damage(
            fighter,
            damage_q16,
            smash_charge_ticks);
    }
    attack->damage_q16 = damage_q16;
    attack->shield_damage = (int16_t)effect->shield_damage;
    attack->hitlag_ticks = pf_m4_melee_hitlag_ticks(
        damage_q16,
        UINT8_C(0),
        UINT32_C(65536));
    attack->target_hitlag_multiplier_q16 = UINT32_C(65536);
    if (effect->element == (uint8_t)PF_M4_REFERENCE_HIT_ELECTRIC)
    {
        const pf_m4_ssbm_damage_response_attributes *common =
            pf_m4_ssbm_common_reference_damage_response();

        if (common != NULL)
        {
            attack->target_hitlag_multiplier_q16 =
                common->electric_hitlag_scale_q16;
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

static void pf_m4_apply_stale_scaled_reference_damage(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint8_t action_state,
    pf_m4_attack_runtime *attack)
{
    const int uses_reference_effect =
        attack->melee_knockback == &attack->reference_melee_knockback;

    attack->damage_q16 = pf_m4_stale_scaled_damage_q16(
        fighter,
        scratch,
        attacker_index,
        pf_m4_stale_move_id_for_action(action_state),
        attack->damage_q16);
    if (uses_reference_effect != 0)
    {
        attack->hitlag_ticks = pf_m4_melee_hitlag_ticks(
            attack->damage_q16,
            UINT8_C(0),
            UINT32_C(65536));
    }
}

/*
 * Returns 1 when the generated Falcon route supplied this frame's effect,
 * 0 when authored content does not match that route, and -1 for an inactive
 * frame inside an otherwise table-backed action. This keeps custom content
 * authored while making the default fighter's disjoint hit phases exact.
 */
static int pf_m4_apply_falcon_reference_frame(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_frame,
    uint16_t smash_charge_ticks,
    pf_m4_attack_runtime *attack)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_hit_effect *primary_effect;
    const pf_m4_reference_hit_effect *frame_effect;
    pf_m4_reference_timing timing;
    int reference_special;
    int reference_source_only_action;

    if (fighter == NULL || attack == NULL ||
        !pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    primary_effect = pf_m4_falcon_reference_primary_effect(move_index);
    timing = pf_m4_falcon_reference_timing(move_index);
    reference_special =
        move_index >= PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND;
    reference_source_only_action =
        pf_m4_action_is_reference_jab_extension(action_state) ||
        pf_m4_action_is_reference_angled_normal(action_state);
    if (primary_effect == NULL || timing.active_ticks == UINT16_C(0) ||
        (reference_special != 0 &&
         fighter->reference_frame_data_enabled == UINT8_C(0)) ||
        (reference_special == 0 && reference_source_only_action == 0 &&
         action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
         !pf_m4_reference_knockback_matches(
             &fighter->jab_melee_knockback,
             primary_effect)) ||
        (reference_special == 0 && reference_source_only_action == 0 &&
         action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL &&
         !pf_m4_reference_knockback_matches(
             &fighter->jab_final_melee_knockback,
             primary_effect)) ||
        attack->active_begin_tick !=
            timing.startup_ticks + UINT16_C(1) ||
        attack->active_end_tick !=
            timing.startup_ticks + timing.active_ticks ||
        (reference_special == 0 && reference_source_only_action == 0 &&
         pf_m4_authored_base_damage_for_action(fighter, action_state) !=
             (uint32_t)primary_effect->damage * UINT32_C(65536)))
    {
        return 0;
    }
    frame_effect = pf_m4_falcon_reference_effect_at_frame(
        move_index,
        action_frame);
    if (frame_effect == NULL)
    {
        return -1;
    }

    pf_m4_apply_falcon_reference_effect(
        fighter,
        action_state,
        smash_charge_ticks,
        frame_effect,
        attack);
    return 1;
}

static int pf_m4_action_is_falcon_dive_capture_phase(
    uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH;
}

static int pf_m4_attack_for_action(
    const pf_m4_content *content,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t charge_ticks,
    uint16_t smash_charge_ticks,
    pf_m4_attack_runtime *out_attack)
{
    const pf_m4_fighter_data *fighter;

    if (content == NULL || out_attack == NULL)
    {
        return 0;
    }
    fighter = &content->fighter;
    out_attack->melee_knockback = NULL;
    out_attack->shield_damage = INT16_C(0);
    out_attack->target_hitlag_multiplier_q16 = UINT32_C(65536);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        !pf_m4_action_is_falcon_dive_capture_phase(action_state))
    {
        pf_m4_falcon_move_index reference_move_index;

        if (pf_m4_falcon_reference_move_for_action(
                action_state,
                &reference_move_index) &&
            (reference_move_index >=
                 PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND ||
             pf_m4_action_is_throw(action_state) ||
             action_state == (uint8_t)PF_M4_ACTION_PUMMEL ||
             pf_m4_action_is_reference_jab_extension(action_state) ||
             pf_m4_action_is_reference_angled_normal(action_state)))
        {
            const pf_m4_reference_hit_effect *effect =
                pf_m4_falcon_reference_primary_effect(
                    reference_move_index);
            const pf_m4_reference_timing timing =
                pf_m4_falcon_reference_timing(
                    reference_move_index);

            if (effect == NULL || timing.active_ticks == UINT16_C(0))
            {
                return 0;
            }
            (void)memset(out_attack, 0, sizeof(*out_attack));
            if (action_state == (uint8_t)PF_M4_ACTION_JAB_THIRD)
            {
                out_attack->hitbox_offset_x_q16 =
                    fighter->jab_final_hitbox_offset_x_q16;
                out_attack->hitbox_offset_y_q16 =
                    fighter->jab_final_hitbox_offset_y_q16;
                out_attack->hitbox_half_width_q16 =
                    fighter->jab_final_hitbox_half_width_q16;
                out_attack->hitbox_half_height_q16 =
                    fighter->jab_final_hitbox_half_height_q16;
            }
            else if (action_state ==
                         (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP)
            {
                /* Custom content still needs a rectangular fallback. The
                 * generated Falcon path selects the executable Attack100
                 * spheres before this envelope reaches collision. */
                out_attack->hitbox_offset_x_q16 =
                    fighter->jab_hitbox_offset_x_q16;
                out_attack->hitbox_offset_y_q16 =
                    fighter->jab_hitbox_offset_y_q16;
                out_attack->hitbox_half_width_q16 =
                    fighter->jab_hitbox_half_width_q16;
                out_attack->hitbox_half_height_q16 =
                    fighter->jab_hitbox_half_height_q16;
            }
            else if (pf_m4_action_is_reference_angled_normal(action_state))
            {
                const pf_m4_attack_data *fallback =
                    pf_m4_ground_attack_data(fighter, action_state);

                if (fallback != NULL)
                {
                    out_attack->hitbox_offset_x_q16 =
                        fallback->hitbox_offset_x_q16;
                    out_attack->hitbox_offset_y_q16 =
                        fallback->hitbox_offset_y_q16;
                    out_attack->hitbox_half_width_q16 =
                        fallback->hitbox_half_width_q16;
                    out_attack->hitbox_half_height_q16 =
                        fallback->hitbox_half_height_q16;
                }
            }
            out_attack->active_begin_tick =
                timing.startup_ticks + UINT16_C(1);
            out_attack->active_end_tick =
                timing.startup_ticks + timing.active_ticks;
            out_attack->direction = INT8_C(1);
            out_attack->vertical_direction = INT8_C(-1);
            out_attack->action_state = action_state;
            pf_m4_apply_falcon_reference_effect(
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
        out_attack->hitbox_offset_x_q16 =
            fighter->dash_attack_hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            fighter->dash_attack_hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            fighter->dash_attack_hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            fighter->dash_attack_hitbox_half_height_q16;
        out_attack->damage_q16 = fighter->dash_attack_damage_q16;
        out_attack->base_knockback_x_q16 =
            fighter->dash_attack_base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            fighter->dash_attack_base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            fighter->dash_attack_knockback_growth_q16;
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
        out_attack->hitbox_offset_x_q16 =
            fighter->jab_final_hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            fighter->jab_final_hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            fighter->jab_final_hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            fighter->jab_final_hitbox_half_height_q16;
        out_attack->damage_q16 = fighter->jab_final_damage_q16;
        out_attack->base_knockback_x_q16 =
            fighter->jab_final_base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            fighter->jab_final_base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            fighter->jab_final_knockback_growth_q16;
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
        out_attack->hitbox_offset_x_q16 =
            fighter->jab_hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            fighter->jab_hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            fighter->jab_hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            fighter->jab_hitbox_half_height_q16;
        out_attack->damage_q16 = fighter->jab_damage_q16;
        out_attack->base_knockback_x_q16 =
            fighter->jab_base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            fighter->jab_base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            fighter->jab_knockback_growth_q16;
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
    if (pf_m4_ground_attack_data(fighter, action_state) != NULL)
    {
        const pf_m4_attack_data *attack =
            pf_m4_ground_attack_data(fighter, action_state);

        out_attack->hitbox_offset_x_q16 =
            attack->hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            attack->hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            attack->hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            attack->hitbox_half_height_q16;
        out_attack->damage_q16 = attack->damage_q16;
        if (pf_m4_action_is_smash_release(action_state) &&
            smash_charge_ticks != UINT16_C(0))
        {
            out_attack->damage_q16 = pf_m4_smash_charged_damage(
                fighter,
                attack->damage_q16,
                smash_charge_ticks);
        }
        out_attack->base_knockback_x_q16 =
            attack->base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            attack->base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            attack->knockback_growth_q16;
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
        const pf_m4_attack_data *attack = &fighter->ledge_attack;

        out_attack->hitbox_offset_x_q16 =
            attack->hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            attack->hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            attack->hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            attack->hitbox_half_height_q16;
        out_attack->damage_q16 = attack->damage_q16;
        out_attack->base_knockback_x_q16 =
            attack->base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            attack->base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            attack->knockback_growth_q16;
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
        const pf_m4_attack_data *attack;

        switch ((pf_m4_action_state)action_state)
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

        out_attack->hitbox_offset_x_q16 =
            attack->hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            attack->hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            attack->hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            attack->hitbox_half_height_q16;
        out_attack->damage_q16 = attack->damage_q16;
        out_attack->base_knockback_x_q16 =
            attack->base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            attack->base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            attack->knockback_growth_q16;
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
        out_attack->hitbox_offset_x_q16 =
            fighter->strong_hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            fighter->strong_hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            fighter->strong_hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            fighter->strong_hitbox_half_height_q16;
        out_attack->damage_q16 = fighter->strong_damage_q16;
        out_attack->base_knockback_x_q16 =
            fighter->strong_base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            fighter->strong_base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            fighter->strong_knockback_growth_q16;
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
        out_attack->hitbox_offset_x_q16 =
            fighter->aerial_hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            fighter->aerial_hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            fighter->aerial_hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            fighter->aerial_hitbox_half_height_q16;
        out_attack->damage_q16 = fighter->aerial_damage_q16;
        out_attack->base_knockback_x_q16 =
            fighter->aerial_base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            fighter->aerial_base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            fighter->aerial_knockback_growth_q16;
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
        const pf_m4_reflector_data *reflector = &content->reflector;

        if (reflector->enabled == UINT8_C(0))
        {
            return 0;
        }
        out_attack->hitbox_offset_x_q16 =
            reflector->hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            reflector->hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            reflector->hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            reflector->hitbox_half_height_q16;
        out_attack->damage_q16 = reflector->damage_q16;
        out_attack->base_knockback_x_q16 =
            reflector->base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            reflector->base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            reflector->knockback_growth_q16;
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
        const pf_m4_charge_data *charge = &content->charge;
        const uint16_t bounded_charge =
            charge_ticks < charge->max_charge_ticks
                ? charge_ticks
                : charge->max_charge_ticks;

        if (charge->enabled == UINT8_C(0))
        {
            return 0;
        }
        out_attack->hitbox_offset_x_q16 =
            charge->hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            charge->hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            charge->hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            charge->hitbox_half_height_q16;
        out_attack->damage_q16 =
            charge->base_damage_q16 +
            (uint32_t)(
                (uint64_t)charge->bonus_damage_q16 *
                (uint64_t)bounded_charge /
                (uint64_t)charge->max_charge_ticks);
        out_attack->base_knockback_x_q16 =
            charge->base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            charge->base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            charge->knockback_growth_q16;
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

        out_attack->hitbox_offset_x_q16 =
            fighter->getup_attack_hitbox_offset_x_q16;
        out_attack->hitbox_offset_y_q16 =
            fighter->getup_attack_hitbox_offset_y_q16;
        out_attack->hitbox_half_width_q16 =
            fighter->getup_attack_hitbox_half_width_q16;
        out_attack->hitbox_half_height_q16 =
            fighter->getup_attack_hitbox_half_height_q16;
        out_attack->damage_q16 =
            fighter->getup_attack_damage_q16;
        out_attack->base_knockback_x_q16 =
            fighter->getup_attack_base_knockback_x_q16;
        out_attack->base_knockback_y_q16 =
            fighter->getup_attack_base_knockback_y_q16;
        out_attack->knockback_growth_q16 =
            fighter->getup_attack_knockback_growth_q16;
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

static int pf_m4_attack_for_state(
    const pf_m4_content *content,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t source_submotion,
    uint16_t charge_ticks,
    uint16_t smash_charge_ticks,
    pf_m4_attack_runtime *out_attack)
{
    const pf_m4_falcon_ledge_attack_reference *ledge_attack;

    if (!pf_m4_attack_for_action(
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
        pf_m4_falcon_reference_ledge_attack(source_submotion);
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
    pf_m4_apply_falcon_reference_effect(
        &content->fighter,
        action_state,
        smash_charge_ticks,
        &ledge_attack->effect,
        out_attack);
    return 1;
}

typedef struct pf_m4_shield_hit_response
{
    uint32_t damage_q16;
    uint16_t stun_ticks;
    int32_t defender_pushback_q16;
    int32_t attacker_pushback_q16;
} pf_m4_shield_hit_response;

static inline int32_t pf_m4_shield_pressure_lerp_q16(
    int32_t light_q16,
    int32_t dense_q16,
    uint16_t shield_strength)
{
    return light_q16 -
           (int32_t)(
               ((int64_t)(light_q16 - dense_q16) *
                (int64_t)shield_strength) /
               (int64_t)UINT16_MAX);
}

static inline uint32_t pf_m4_shield_hit_damage_q16(
    uint32_t attack_damage_q16)
{
    uint32_t integer_damage = attack_damage_q16 >> 16U;

    if (attack_damage_q16 != UINT32_C(0) &&
        integer_damage == UINT32_C(0))
    {
        integer_damage = UINT32_C(1);
    }
    return integer_damage << 16U;
}

static pf_m4_shield_hit_response pf_m4_shield_hit_response_for(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    int16_t shield_damage,
    uint16_t shield_strength,
    int powershield)
{
    pf_m4_shield_hit_response response;
    const uint32_t shield_hit_damage_q16 =
        pf_m4_shield_hit_damage_q16(attack_damage_q16);
    const int64_t shield_health_damage_q16_signed =
        (int64_t)shield_hit_damage_q16 +
        (int64_t)shield_damage * INT64_C(65536);
    const uint32_t shield_health_damage_q16 =
        shield_health_damage_q16_signed > INT64_C(0)
            ? (uint32_t)shield_health_damage_q16_signed
            : UINT32_C(0);
    const int32_t damage_multiplier_q16 =
        pf_m4_shield_pressure_lerp_q16(
            (int32_t)fighter->light_shield_damage_multiplier_q16,
            (int32_t)fighter->dense_shield_damage_multiplier_q16,
            shield_strength);
    const int32_t stun_damage_multiplier_q16 =
        pf_m4_shield_pressure_lerp_q16(
            fighter->light_shield_stun_damage_multiplier_q16,
            fighter->dense_shield_stun_damage_multiplier_q16,
            shield_strength);
    const int64_t stun_duration_q16 =
        (((int64_t)shield_hit_damage_q16 *
          (int64_t)stun_damage_multiplier_q16) >>
         16U) +
        (int64_t)fighter->shield_stun_base_q16;
    const uint64_t pressure_damage_q16 =
        ((uint64_t)shield_hit_damage_q16 *
         (uint64_t)shield_strength) /
        (uint64_t)UINT16_MAX;
    int64_t ticks =
        (stun_duration_q16 * INT64_C(200) / INT64_C(201)) >> 16U;
    int64_t defender_pushback_q16 =
        (stun_duration_q16 *
         (int64_t)fighter->shield_defender_pushback_stun_scale_q16) >>
        16U;
    int64_t attacker_pushback_q16 =
        ((int64_t)pressure_damage_q16 *
         (int64_t)fighter->shield_attacker_pushback_damage_q16) >>
        16U;

    response.damage_q16 =
        (uint32_t)(((uint64_t)shield_health_damage_q16 *
                    (uint64_t)(uint32_t)damage_multiplier_q16) >>
                   16U);
    if (ticks < INT64_C(1))
    {
        ticks = INT64_C(1);
    }
    if (ticks > (int64_t)UINT16_MAX)
    {
        ticks = (int64_t)UINT16_MAX;
    }
    response.stun_ticks = (uint16_t)ticks;

    if (powershield == 0)
    {
        defender_pushback_q16 =
            (defender_pushback_q16 *
             (int64_t)fighter
                 ->shield_defender_pushback_normal_scale_q16) >>
            16U;
    }
    if (defender_pushback_q16 >
        (int64_t)fighter->shield_defender_pushback_max_q16)
    {
        defender_pushback_q16 =
            (int64_t)fighter->shield_defender_pushback_max_q16;
    }
    response.defender_pushback_q16 =
        (int32_t)defender_pushback_q16;

    attacker_pushback_q16 +=
        (int64_t)fighter->shield_attacker_pushback_base_q16;
    if (attacker_pushback_q16 >
        (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        attacker_pushback_q16 =
            (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    response.attacker_pushback_q16 =
        (int32_t)attacker_pushback_q16;
    return response;
}

static pf_status pf_m4_apply_shield_hit(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_m4_fighter_data *fighter,
    uint32_t source_index,
    uint32_t target_index,
    uint32_t attack_damage_q16,
    int16_t shield_damage,
    uint16_t hitlag_ticks,
    int32_t horizontal_direction,
    int powershield,
    uint16_t source_action,
    pf_m4_shield_hit_response *out_response)
{
    const pf_m4_shield_hit_response response =
        pf_m4_shield_hit_response_for(
            fighter,
            attack_damage_q16,
            shield_damage,
            scratch->shield_strength[target_index],
            powershield);
    pf_sim_event_type event_type;
    const int shield_broken =
        powershield == 0 &&
        response.damage_q16 >
            scratch->shield_health_q16[target_index];

    if (powershield == 0)
    {
        scratch->shield_health_q16[target_index] =
            response.damage_q16 >=
                    scratch->shield_health_q16[target_index]
                ? UINT32_C(0)
                : scratch->shield_health_q16[target_index] -
                      response.damage_q16;
    }
    scratch->velocity_x_q16[target_index] =
        horizontal_direction * response.defender_pushback_q16;
    scratch->velocity_y_q16[target_index] = INT32_C(0);
    scratch->powershield[target_index] =
        powershield ? UINT8_C(1) : UINT8_C(0);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    pf_m4_set_action_state(
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
        scratch->shield_health_q16[target_index] =
            fighter->shield_reset_health_q16;
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
        scratch->shield_stun_ticks[target_index] = response.stun_ticks;
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
        powershield ? UINT32_C(0) : response.damage_q16,
        scratch->velocity_x_q16[target_index],
        scratch->velocity_y_q16[target_index],
        UINT16_C(0),
        source_action,
        NULL);
}

static int32_t pf_m4_reference_origin_y_q16(
    const pf_m4_fighter_data *fighter,
    int32_t position_y_q16)
{
    return position_y_q16 + fighter->half_height_q16;
}

static uint8_t pf_m4_reference_hit_spheres_at_world(
    const pf_m4_fighter_data *fighter,
    pf_m4_falcon_move_index move_index,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    uint16_t action_ticks,
    pf_m4_hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY])
{
    const pf_m4_reference_hit_sphere *source;
    const int32_t reference_origin_y_q16 =
        pf_m4_reference_origin_y_q16(fighter, position_y_q16);
    uint8_t sphere_count;
    uint8_t sphere_index;

    if (out_spheres == NULL)
    {
        return UINT8_C(0);
    }
    source = pf_m4_falcon_reference_hit_spheres_at_frame(
        move_index,
        action_ticks,
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
        out_spheres[sphere_index].center_x_q16 =
            position_x_q16 +
            (int32_t)facing * source[sphere_index].offset_x_q16;
        out_spheres[sphere_index].center_y_q16 =
            reference_origin_y_q16 + source[sphere_index].offset_y_q16;
        out_spheres[sphere_index].center_z_q16 =
            (int32_t)facing * source[sphere_index].offset_z_q16;
        out_spheres[sphere_index].radius_q16 =
            source[sphere_index].radius_q16;
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

static int pf_m4_falcon_geometry_move_for_attack(
    const pf_m4_content *content,
    uint8_t action_state,
    pf_m4_falcon_move_index *out_move_index)
{
    pf_m4_falcon_move_index move_index;
    pf_m4_attack_runtime attack;
    int reference_special;

    if (content == NULL ||
        content->fighter.reference_frame_data_enabled == UINT8_C(0) ||
        pf_m4_action_is_falcon_dive_capture_phase(action_state) ||
        !pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index) ||
        !pf_m4_falcon_reference_has_hit_geometry(move_index) ||
        !pf_m4_attack_for_action(
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
        !pf_m4_action_is_throw(action_state) &&
        !pf_m4_action_is_reference_angled_normal(action_state) &&
        !pf_m4_falcon_reference_attack_matches(
            action_state,
            (pf_m4_reference_timing){
                (uint16_t)(attack.active_begin_tick - UINT16_C(1)),
                (uint16_t)(
                    attack.active_end_tick -
                    attack.active_begin_tick +
                    UINT16_C(1)),
                (uint16_t)(
                    pf_m4_falcon_reference_move(move_index)->total_frames -
                    attack.active_end_tick)},
            pf_m4_authored_base_damage_for_action(
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

uint8_t pf_m4_attack_hit_spheres(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    pf_m4_hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY])
{
    pf_m4_falcon_move_index move_index;

    if (out_spheres == NULL ||
        !pf_m4_falcon_geometry_move_for_attack(
            content,
            action_state,
            &move_index))
    {
        return UINT8_C(0);
    }
    return pf_m4_reference_hit_spheres_at_world(
        &content->fighter,
        move_index,
        position_x_q16,
        position_y_q16,
        facing,
        action_ticks,
        out_spheres);
}

static int pf_m4_hit_sphere_bounds(
    const pf_m4_hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t sphere_count,
    int32_t *out_left_q16,
    int32_t *out_right_q16,
    int32_t *out_top_q16,
    int32_t *out_bottom_q16)
{
    uint8_t sphere_index;

    if (spheres == NULL || sphere_count == UINT8_C(0) ||
        sphere_count > UINT8_C(PF_M4_INSPECTION_HIT_SPHERE_CAPACITY) ||
        out_left_q16 == NULL || out_right_q16 == NULL ||
        out_top_q16 == NULL || out_bottom_q16 == NULL)
    {
        return 0;
    }
    *out_left_q16 = spheres[0].center_x_q16 - spheres[0].radius_q16;
    *out_right_q16 = spheres[0].center_x_q16 + spheres[0].radius_q16;
    *out_top_q16 = spheres[0].center_y_q16 - spheres[0].radius_q16;
    *out_bottom_q16 = spheres[0].center_y_q16 + spheres[0].radius_q16;
    for (sphere_index = UINT8_C(1);
         sphere_index < sphere_count;
         ++sphere_index)
    {
        const int32_t left =
            spheres[sphere_index].center_x_q16 -
            spheres[sphere_index].radius_q16;
        const int32_t right =
            spheres[sphere_index].center_x_q16 +
            spheres[sphere_index].radius_q16;
        const int32_t top =
            spheres[sphere_index].center_y_q16 -
            spheres[sphere_index].radius_q16;
        const int32_t bottom =
            spheres[sphere_index].center_y_q16 +
            spheres[sphere_index].radius_q16;

        if (left < *out_left_q16)
        {
            *out_left_q16 = left;
        }
        if (right > *out_right_q16)
        {
            *out_right_q16 = right;
        }
        if (top < *out_top_q16)
        {
            *out_top_q16 = top;
        }
        if (bottom > *out_bottom_q16)
        {
            *out_bottom_q16 = bottom;
        }
    }
    return 1;
}

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
    int32_t *out_bottom_q16)
{
    pf_m4_attack_runtime attack;
    pf_m4_falcon_move_index geometry_move_index;
    pf_m4_falcon_move_index reference_move_index;
    pf_m4_hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    uint8_t sphere_count;
    uint16_t effective_action_ticks = action_ticks;
    int64_t center_x;
    int64_t center_y;

    if (content == NULL ||
        out_left_q16 == NULL ||
        out_right_q16 == NULL ||
        out_top_q16 == NULL ||
        out_bottom_q16 == NULL)
    {
        return 0;
    }

    if (pf_m4_falcon_geometry_move_for_attack(
            content,
            action_state,
            &geometry_move_index))
    {
        sphere_count = pf_m4_reference_hit_spheres_at_world(
            &content->fighter,
            geometry_move_index,
            position_x_q16,
            position_y_q16,
            facing,
            action_ticks,
            spheres);
        if (sphere_count == UINT8_C(0))
        {
            return 0;
        }
        return pf_m4_hit_sphere_bounds(
            spheres,
            sphere_count,
            out_left_q16,
            out_right_q16,
            out_top_q16,
            out_bottom_q16);
    }

    if (!pf_m4_attack_for_state(
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
    if (pf_m4_falcon_reference_move_for_action(
            action_state,
            &reference_move_index))
    {
        effective_action_ticks =
            pf_m4_falcon_reference_effective_hit_frame(
                reference_move_index,
                action_ticks);
    }
    if (pf_m4_apply_falcon_reference_frame(
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

    center_x =
        (int64_t)position_x_q16 +
        (int64_t)facing *
            (int64_t)attack.direction *
            (int64_t)attack.hitbox_offset_x_q16;
    center_y =
        (int64_t)position_y_q16 +
        (int64_t)attack.hitbox_offset_y_q16;
    *out_left_q16 =
        (int32_t)(center_x -
                  (int64_t)attack.hitbox_half_width_q16);
    *out_right_q16 =
        (int32_t)(center_x +
                  (int64_t)attack.hitbox_half_width_q16);
    *out_top_q16 =
        (int32_t)(center_y -
                  (int64_t)attack.hitbox_half_height_q16);
    *out_bottom_q16 =
        (int32_t)(center_y +
                  (int64_t)attack.hitbox_half_height_q16);
    return 1;
}

static int pf_m4_event_is_physical_hit(pf_sim_event_type event_type)
{
    return event_type == PF_SIM_EVENT_HIT ||
           event_type == PF_SIM_EVENT_ITEM_HIT ||
           event_type == PF_SIM_EVENT_PROJECTILE_HIT;
}

static int pf_m4_falcon_geometry_move_for_grab(
    const pf_m4_content *content,
    uint8_t action_state,
    pf_m4_falcon_move_index *out_move_index)
{
    pf_m4_falcon_move_index move_index;
    pf_m4_reference_timing reference;
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
        if (!pf_m4_falcon_reference_has_hit_geometry(move_index))
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
    reference = pf_m4_falcon_reference_timing(move_index);
    if (!pf_m4_falcon_reference_has_hit_geometry(move_index) ||
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

static uint8_t pf_m4_grab_hit_spheres(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing,
    uint8_t action_state,
    uint16_t action_ticks,
    pf_m4_hit_sphere_inspection
        out_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY])
{
    pf_m4_falcon_move_index move_index;

    if (!pf_m4_falcon_geometry_move_for_grab(
            content,
            action_state,
            &move_index))
    {
        return UINT8_C(0);
    }
    return pf_m4_reference_hit_spheres_at_world(
        &content->fighter,
        move_index,
        position_x_q16,
        position_y_q16,
        facing,
        action_ticks,
        out_spheres);
}

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
    int32_t *out_bottom_q16)
{
    const pf_m4_fighter_data *fighter;
    pf_m4_falcon_move_index geometry_move_index;
    pf_m4_hit_sphere_inspection
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
    int64_t center_x;
    int64_t center_y;
    if (content == NULL ||
        out_left_q16 == NULL ||
        out_right_q16 == NULL ||
        out_top_q16 == NULL ||
        out_bottom_q16 == NULL)
    {
        return 0;
    }

    if (pf_m4_falcon_geometry_move_for_grab(
            content,
            action_state,
            &geometry_move_index))
    {
        sphere_count = pf_m4_reference_hit_spheres_at_world(
            &content->fighter,
            geometry_move_index,
            position_x_q16,
            position_y_q16,
            facing,
            action_ticks,
            spheres);
        return pf_m4_hit_sphere_bounds(
            spheres,
            sphere_count,
            out_left_q16,
            out_right_q16,
            out_top_q16,
            out_bottom_q16);
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
    center_x =
        (int64_t)position_x_q16 +
        (int64_t)facing * (int64_t)fighter->grabbox_offset_x_q16;
    center_y =
        (int64_t)position_y_q16 +
        (int64_t)fighter->grabbox_offset_y_q16;
    *out_left_q16 =
        (int32_t)(center_x -
                  (int64_t)fighter->grabbox_half_width_q16);
    *out_right_q16 =
        (int32_t)(center_x +
                  (int64_t)fighter->grabbox_half_width_q16);
    *out_top_q16 =
        (int32_t)(center_y -
                  (int64_t)fighter->grabbox_half_height_q16);
    *out_bottom_q16 =
        (int32_t)(center_y +
                  (int64_t)fighter->grabbox_half_height_q16);
    return 1;
}

typedef struct pf_m4_shield_volume
{
    int32_t center_x_q16;
    int32_t center_y_q16;
    int32_t radius_x_q16;
    int32_t radius_y_q16;
} pf_m4_shield_volume;

/* Falcon's GALE01 guard-direction animation sampled at 45-degree keys. The
 * values are local TransN y and (z - 1) in Q16.16; interpolation between keys
 * is linear in the original animation. */
static const int32_t pf_m4_shield_animation_y_q16[9] = {
    INT32_C(0), INT32_C(163840), INT32_C(294912), INT32_C(163840),
    INT32_C(0), INT32_C(-65536), INT32_C(-117952), INT32_C(-65536),
    INT32_C(0)};
static const int32_t pf_m4_shield_animation_z_q16[9] = {
    INT32_C(196608), INT32_C(131072), INT32_C(65536), INT32_C(-13112),
    INT32_C(-65536), INT32_C(-13112), INT32_C(65536), INT32_C(131072),
    INT32_C(196608)};

static int32_t pf_m4_shield_animation_sample(
    const int32_t values_q16[9],
    uint16_t angle_turn)
{
    const uint32_t key = (uint32_t)angle_turn >> 13U;
    const uint32_t fraction = (uint32_t)angle_turn & UINT32_C(8191);
    const int64_t delta =
        (int64_t)values_q16[key + UINT32_C(1)] -
        (int64_t)values_q16[key];

    return values_q16[key] +
           (int32_t)((delta * (int64_t)fraction) / INT64_C(8192));
}

static int32_t pf_m4_scale_shield_animation_q16(
    int32_t animation_scale_q16,
    int32_t animation_value_q16,
    uint16_t magnitude)
{
    return (int32_t)(
        ((int64_t)animation_scale_q16 *
         (int64_t)animation_value_q16 * (int64_t)magnitude) /
        (INT64_C(65536) * INT64_C(65535)));
}

static int pf_m4_shield_volume_for_player(
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
    pf_m4_shield_volume *out_volume)
{
    int32_t density_scale_q16;
    int32_t health_scale_q16;
    int32_t combined_scale_q16;
    int32_t size_scale_q16;
    int32_t animation_x_q16;
    int32_t animation_y_q16;
    int32_t center_forward_q16;
    int32_t center_up_q16;

    if (fighter == NULL || out_volume == NULL ||
        shield_strength == UINT16_C(0) ||
        shield_health_q16 == UINT32_C(0) ||
        !pf_m4_action_has_shield_volume(
            action_state,
            hitlag_resume_action))
    {
        return 0;
    }

    density_scale_q16 =
        PF_Q16_ONE -
        (int32_t)(
            ((int64_t)(
                 PF_Q16_ONE -
                 fighter->dense_shield_size_scale_q16) *
             (int64_t)shield_strength) /
            (int64_t)UINT16_MAX);
    health_scale_q16 =
        (int32_t)(
            ((uint64_t)shield_health_q16 *
             (uint64_t)(uint32_t)PF_Q16_ONE) /
            (uint64_t)fighter->shield_health_q16);
    combined_scale_q16 =
        (int32_t)(
            ((int64_t)health_scale_q16 *
             (int64_t)density_scale_q16) /
            (int64_t)PF_Q16_ONE);
    size_scale_q16 =
        fighter->shield_minimum_size_scale_q16 +
        (int32_t)(
            ((int64_t)(
                 PF_Q16_ONE -
                 fighter->shield_minimum_size_scale_q16) *
             (int64_t)combined_scale_q16) /
            (int64_t)PF_Q16_ONE);
    out_volume->radius_x_q16 =
        (int32_t)(
            ((int64_t)fighter->shield_radius_x_q16 *
             (int64_t)size_scale_q16) /
            (int64_t)PF_Q16_ONE);
    out_volume->radius_y_q16 =
        (int32_t)(
            ((int64_t)fighter->shield_radius_y_q16 *
             (int64_t)size_scale_q16) /
            (int64_t)PF_Q16_ONE);
    animation_x_q16 = pf_m4_scale_shield_animation_q16(
        fighter->shield_animation_scale_x_q16,
        pf_m4_shield_animation_sample(
            pf_m4_shield_animation_z_q16,
            shield_angle_turn),
        shield_magnitude);
    animation_y_q16 = pf_m4_scale_shield_animation_q16(
        fighter->shield_animation_scale_y_q16,
        pf_m4_shield_animation_sample(
            pf_m4_shield_animation_y_q16,
            shield_angle_turn),
        shield_magnitude);
    center_forward_q16 =
        fighter->shield_center_forward_q16 + animation_x_q16;
    center_up_q16 = fighter->shield_center_up_q16 + animation_y_q16;
    out_volume->center_x_q16 =
        position_x_q16 + (int32_t)facing * center_forward_q16;
    out_volume->center_y_q16 = position_y_q16 - center_up_q16;
    return 1;
}

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
    int32_t *out_bottom_q16)
{
    pf_m4_shield_volume volume;

    if (out_left_q16 == NULL || out_right_q16 == NULL ||
        out_top_q16 == NULL || out_bottom_q16 == NULL ||
        !pf_m4_shield_volume_for_player(
            fighter,
            position_x_q16,
            position_y_q16,
            action_state,
            hitlag_resume_action,
            shield_health_q16,
            shield_strength,
            facing,
            shield_angle_turn,
            shield_magnitude,
            &volume))
    {
        return 0;
    }
    *out_left_q16 = volume.center_x_q16 - volume.radius_x_q16;
    *out_right_q16 = volume.center_x_q16 + volume.radius_x_q16;
    *out_top_q16 = volume.center_y_q16 - volume.radius_y_q16;
    *out_bottom_q16 = volume.center_y_q16 + volume.radius_y_q16;
    return 1;
}

typedef pf_m4_collision_capsule3_q16 pf_m4_world_hurt_capsule;

static inline int8_t pf_m4_reference_hurt_pose_facing(
    const pf_m4_fighter_data *fighter,
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

static inline uint16_t pf_m4_reference_common_hurt_frame(
    uint8_t action_state,
    uint16_t action_ticks)
{
    /* Dash, RunBrake, and squat transitions enter the movement scratch at
     * source frame 1.  The remaining imported common motions enter at tick 0,
     * so their source frame is one-based relative to the stored tick. */
    if (action_ticks == UINT16_MAX)
    {
        return UINT16_MAX;
    }
    switch ((pf_m4_action_state)action_state)
    {
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

static const pf_m4_reference_hurt_capsule *pf_m4_reference_hurt_pose(
    const pf_m4_fighter_data *fighter,
    uint8_t grounded,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    uint16_t action_ticks,
    pf_m4_reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    pf_m4_falcon_move_index move_index;

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
         action_state == (uint8_t)PF_M4_ACTION_RUN))
    {
        return dynamic_capsules != NULL &&
                       pf_m4_falcon_reference_dynamic_ground_hurt_capsules(
                           source_submotion,
                           source_animation_frame_q16,
                           dynamic_capsules,
                           out_count)
                   ? dynamic_capsules
                   : NULL;
    }
    if (grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return pf_m4_falcon_reference_standing_hurt_capsules(out_count);
    }
    {
        const pf_m4_reference_hurt_capsule *common_pose =
            pf_m4_falcon_reference_common_hurt_capsules_for_submotion_at_frame(
                action_state,
                source_submotion,
                pf_m4_reference_common_hurt_frame(
                    action_state,
                    action_ticks),
                out_count);

        if (common_pose != NULL)
        {
            return common_pose;
        }
    }
    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        switch ((pf_m4_action_state)action_state)
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
    return pf_m4_falcon_reference_hurt_capsules_at_frame(
        move_index,
        action_ticks,
        out_count);
}

static pf_m4_world_hurt_capsule pf_m4_world_hurt_capsule_from_reference(
    const pf_m4_fighter_data *fighter,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int8_t facing_value,
    const pf_m4_reference_hurt_capsule *source)
{
    const int64_t position_x = (int64_t)position_x_q16;
    const int64_t position_y =
        (int64_t)pf_m4_reference_origin_y_q16(
            fighter,
            position_y_q16);
    const int64_t facing = (int64_t)facing_value;
    const pf_m4_world_hurt_capsule world = {
        position_x + facing * (int64_t)source->endpoint_a_x_q16,
        position_y + (int64_t)source->endpoint_a_y_q16,
        facing * (int64_t)source->endpoint_a_z_q16,
        position_x + facing * (int64_t)source->endpoint_b_x_q16,
        position_y + (int64_t)source->endpoint_b_y_q16,
        facing * (int64_t)source->endpoint_b_z_q16,
        (int64_t)source->radius_q16};

    return world;
}

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
    pf_m4_hurt_capsule_inspection
        out_capsules[PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY])
{
    uint8_t capsule_count;
    pf_m4_reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const pf_m4_reference_hurt_capsule *source_capsules;
    uint8_t capsule_index;
    int8_t pose_facing;

    if (fighter == NULL || out_capsules == NULL)
    {
        return UINT8_C(0);
    }
    source_capsules = pf_m4_reference_hurt_pose(
        fighter,
        grounded,
        action_state,
        hitlag_resume_action,
        source_submotion,
        source_animation_frame_q16,
        action_ticks,
        dynamic_capsules,
        &capsule_count);
    if (source_capsules == NULL ||
        capsule_count > UINT8_C(PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY))
    {
        return UINT8_C(0);
    }
    pose_facing = pf_m4_reference_hurt_pose_facing(
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
        const pf_m4_reference_hurt_capsule *source =
            &source_capsules[capsule_index];
        const pf_m4_world_hurt_capsule world =
            pf_m4_world_hurt_capsule_from_reference(
                fighter,
                position_x_q16,
                position_y_q16,
                pose_facing,
                source);
        pf_m4_hurt_capsule_inspection *destination =
            &out_capsules[capsule_index];

        if (world.endpoint_a_x_q16 < (int64_t)INT32_MIN ||
            world.endpoint_a_x_q16 > (int64_t)INT32_MAX ||
            world.endpoint_a_y_q16 < (int64_t)INT32_MIN ||
            world.endpoint_a_y_q16 > (int64_t)INT32_MAX ||
            world.endpoint_a_z_q16 < (int64_t)INT32_MIN ||
            world.endpoint_a_z_q16 > (int64_t)INT32_MAX ||
            world.endpoint_b_x_q16 < (int64_t)INT32_MIN ||
            world.endpoint_b_x_q16 > (int64_t)INT32_MAX ||
            world.endpoint_b_y_q16 < (int64_t)INT32_MIN ||
            world.endpoint_b_y_q16 > (int64_t)INT32_MAX ||
            world.endpoint_b_z_q16 < (int64_t)INT32_MIN ||
            world.endpoint_b_z_q16 > (int64_t)INT32_MAX ||
            world.radius_q16 < (int64_t)INT32_MIN ||
            world.radius_q16 > (int64_t)INT32_MAX)
        {
            return UINT8_C(0);
        }
        destination->endpoint_a_x_q16 =
            (int32_t)world.endpoint_a_x_q16;
        destination->endpoint_a_y_q16 =
            (int32_t)world.endpoint_a_y_q16;
        destination->endpoint_a_z_q16 =
            (int32_t)world.endpoint_a_z_q16;
        destination->endpoint_b_x_q16 =
            (int32_t)world.endpoint_b_x_q16;
        destination->endpoint_b_y_q16 =
            (int32_t)world.endpoint_b_y_q16;
        destination->endpoint_b_z_q16 =
            (int32_t)world.endpoint_b_z_q16;
        destination->radius_q16 = (int32_t)world.radius_q16;
        destination->hurtbox_id = source->hurtbox_id;
        destination->height = source->height;
        destination->grabbable = source->grabbable;
        destination->reserved = UINT8_C(0);
    }
    return capsule_count;
}

static int pf_m4_hitbox_overlaps_player(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    int32_t hitbox_left_q16,
    int32_t hitbox_right_q16,
    int32_t hitbox_top_q16,
    int32_t hitbox_bottom_q16)
{
    uint8_t capsule_count;
    pf_m4_reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const pf_m4_reference_hurt_capsule *capsules =
        pf_m4_reference_hurt_pose(
            fighter,
            scratch->grounded[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_q16[target_index],
            scratch->action_ticks[target_index],
            dynamic_capsules,
            &capsule_count);

    if (capsules != NULL)
    {
        const int8_t pose_facing = pf_m4_reference_hurt_pose_facing(
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
            const pf_m4_world_hurt_capsule capsule =
                pf_m4_world_hurt_capsule_from_reference(
                    fighter,
                    scratch->position_x_q16[target_index],
                    scratch->position_y_q16[target_index],
                    pose_facing,
                    &capsules[capsule_index]);
            const int64_t capsule_left =
                (capsule.endpoint_a_x_q16 < capsule.endpoint_b_x_q16
                     ? capsule.endpoint_a_x_q16
                     : capsule.endpoint_b_x_q16) -
                capsule.radius_q16;
            const int64_t capsule_right =
                (capsule.endpoint_a_x_q16 > capsule.endpoint_b_x_q16
                     ? capsule.endpoint_a_x_q16
                     : capsule.endpoint_b_x_q16) +
                capsule.radius_q16;
            const int64_t capsule_top =
                (capsule.endpoint_a_y_q16 < capsule.endpoint_b_y_q16
                     ? capsule.endpoint_a_y_q16
                     : capsule.endpoint_b_y_q16) -
                capsule.radius_q16;
            const int64_t capsule_bottom =
                (capsule.endpoint_a_y_q16 > capsule.endpoint_b_y_q16
                     ? capsule.endpoint_a_y_q16
                     : capsule.endpoint_b_y_q16) +
                capsule.radius_q16;

            if (hitbox_left_q16 <= capsule_right &&
                hitbox_right_q16 >= capsule_left &&
                hitbox_top_q16 <= capsule_bottom &&
                hitbox_bottom_q16 >= capsule_top)
            {
                return 1;
            }
        }
        return 0;
    }
    const int32_t hurtbox_left =
        scratch->position_x_q16[target_index] -
        fighter->half_width_q16;
    const int32_t hurtbox_right =
        scratch->position_x_q16[target_index] +
        fighter->half_width_q16;
    const int32_t hurtbox_top =
        scratch->position_y_q16[target_index] -
        fighter->half_height_q16;
    const int32_t hurtbox_bottom =
        scratch->position_y_q16[target_index] +
        fighter->half_height_q16;

    return hitbox_left_q16 <= hurtbox_right &&
           hitbox_right_q16 >= hurtbox_left &&
           hitbox_top_q16 <= hurtbox_bottom &&
           hitbox_bottom_q16 >= hurtbox_top;
}

static int pf_m4_hitbox_overlaps_shield(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    int32_t hitbox_left_q16,
    int32_t hitbox_right_q16,
    int32_t hitbox_top_q16,
    int32_t hitbox_bottom_q16)
{
    pf_m4_shield_volume volume;
    int32_t nearest_x_q16;
    int32_t nearest_y_q16;
    int64_t normalized_x_q16;
    int64_t normalized_y_q16;

    if (!pf_m4_shield_volume_for_player(
            fighter,
            scratch->position_x_q16[target_index],
            scratch->position_y_q16[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->shield_health_q16[target_index],
            scratch->shield_strength[target_index],
            scratch->facing[target_index],
            scratch->shield_angle_turn[target_index],
            scratch->shield_magnitude[target_index],
            &volume))
    {
        return 0;
    }
    nearest_x_q16 = volume.center_x_q16 < hitbox_left_q16
                        ? hitbox_left_q16
                        : volume.center_x_q16 > hitbox_right_q16
                              ? hitbox_right_q16
                              : volume.center_x_q16;
    nearest_y_q16 = volume.center_y_q16 < hitbox_top_q16
                        ? hitbox_top_q16
                        : volume.center_y_q16 > hitbox_bottom_q16
                              ? hitbox_bottom_q16
                              : volume.center_y_q16;
    normalized_x_q16 =
        ((int64_t)nearest_x_q16 - (int64_t)volume.center_x_q16) *
        (int64_t)PF_Q16_ONE / (int64_t)volume.radius_x_q16;
    normalized_y_q16 =
        ((int64_t)nearest_y_q16 - (int64_t)volume.center_y_q16) *
        (int64_t)PF_Q16_ONE / (int64_t)volume.radius_y_q16;
    return normalized_x_q16 * normalized_x_q16 +
               normalized_y_q16 * normalized_y_q16 <=
           (int64_t)PF_Q16_ONE * (int64_t)PF_Q16_ONE;
}

static int pf_m4_hitbox_overlaps_player_or_shield(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    int32_t hitbox_left_q16,
    int32_t hitbox_right_q16,
    int32_t hitbox_top_q16,
    int32_t hitbox_bottom_q16)
{
    return pf_m4_hitbox_overlaps_player(
               fighter,
               scratch,
               target_index,
               hitbox_left_q16,
               hitbox_right_q16,
               hitbox_top_q16,
               hitbox_bottom_q16) ||
           pf_m4_hitbox_overlaps_shield(
               fighter,
               scratch,
               target_index,
               hitbox_left_q16,
               hitbox_right_q16,
               hitbox_top_q16,
               hitbox_bottom_q16);
}

static int pf_m4_hit_sphere_overlaps_reference_pose(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    const pf_m4_hit_sphere_inspection *previous_sphere,
    const pf_m4_hit_sphere_inspection *sphere,
    const pf_m4_reference_hurt_capsule *capsules,
    uint8_t capsule_count,
    int grabbable_only)
{
    const int8_t pose_facing = pf_m4_reference_hurt_pose_facing(
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
        const pf_m4_world_hurt_capsule capsule =
            pf_m4_world_hurt_capsule_from_reference(
                fighter,
                scratch->position_x_q16[target_index],
                scratch->position_y_q16[target_index],
                pose_facing,
                &capsules[capsule_index]);
        const pf_m4_collision_capsule3_q16 collision_hit = {
            (int64_t)previous_sphere->center_x_q16,
            (int64_t)previous_sphere->center_y_q16,
            (int64_t)previous_sphere->center_z_q16,
            (int64_t)sphere->center_x_q16,
            (int64_t)sphere->center_y_q16,
            (int64_t)sphere->center_z_q16,
            (int64_t)sphere->radius_q16};

        if (grabbable_only != 0 &&
            capsules[capsule_index].grabbable == UINT8_C(0))
        {
            continue;
        }
        if (pf_m4_collision_capsule_capsule_overlap_q16(
                &collision_hit,
                &capsule))
        {
            return 1;
        }
    }
    return 0;
}

static int pf_m4_hit_sphere_overlaps_player(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    const pf_m4_hit_sphere_inspection *previous_sphere,
    const pf_m4_hit_sphere_inspection *sphere)
{
    uint8_t capsule_count;
    pf_m4_reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const pf_m4_reference_hurt_capsule *capsules =
        pf_m4_reference_hurt_pose(
            fighter,
            scratch->grounded[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_q16[target_index],
            scratch->action_ticks[target_index],
            dynamic_capsules,
            &capsule_count);

    if (capsules != NULL)
    {
        return pf_m4_hit_sphere_overlaps_reference_pose(
            fighter,
            scratch,
            target_index,
            previous_sphere,
            sphere,
            capsules,
            capsule_count,
            0);
    }
    const int32_t hurtbox_left =
        scratch->position_x_q16[target_index] -
        fighter->half_width_q16;
    const int32_t hurtbox_right =
        scratch->position_x_q16[target_index] +
        fighter->half_width_q16;
    const int32_t hurtbox_top =
        scratch->position_y_q16[target_index] -
        fighter->half_height_q16;
    const int32_t hurtbox_bottom =
        scratch->position_y_q16[target_index] +
        fighter->half_height_q16;
    const int32_t nearest_x =
        sphere->center_x_q16 < hurtbox_left
            ? hurtbox_left
            : sphere->center_x_q16 > hurtbox_right
                  ? hurtbox_right
                  : sphere->center_x_q16;
    const int32_t nearest_y =
        sphere->center_y_q16 < hurtbox_top
            ? hurtbox_top
            : sphere->center_y_q16 > hurtbox_bottom
                  ? hurtbox_bottom
                  : sphere->center_y_q16;
    const int64_t delta_x =
        (int64_t)sphere->center_x_q16 - (int64_t)nearest_x;
    const int64_t delta_y =
        (int64_t)sphere->center_y_q16 - (int64_t)nearest_y;

    return delta_x * delta_x + delta_y * delta_y <=
           (int64_t)sphere->radius_q16 *
               (int64_t)sphere->radius_q16;
}

static int pf_m4_grab_sphere_overlaps_player(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t target_index,
    const pf_m4_hit_sphere_inspection *previous_sphere,
    const pf_m4_hit_sphere_inspection *sphere)
{
    uint8_t capsule_count;
    pf_m4_reference_hurt_capsule
        dynamic_capsules[PF_M4_HSD_POSE_MAX_CAPSULES];
    const pf_m4_reference_hurt_capsule *capsules =
        pf_m4_reference_hurt_pose(
            fighter,
            scratch->grounded[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->source_submotion[target_index],
            scratch->source_animation_frame_q16[target_index],
            scratch->action_ticks[target_index],
            dynamic_capsules,
            &capsule_count);

    if (capsules != NULL)
    {
        return pf_m4_hit_sphere_overlaps_reference_pose(
            fighter,
            scratch,
            target_index,
            previous_sphere,
            sphere,
            capsules,
            capsule_count,
            1);
    }
    return pf_m4_hit_sphere_overlaps_player(
        fighter,
        scratch,
        target_index,
        previous_sphere,
        sphere);
}

static int pf_m4_hit_sphere_overlaps_shield(
    const pf_m4_fighter_data *fighter,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    int32_t previous_attacker_position_y_q16,
    const pf_m4_hit_sphere_inspection *previous_sphere,
    const pf_m4_hit_sphere_inspection *sphere)
{
    pf_m4_shield_volume volume;
    const int64_t previous_attacker_reference_origin_y =
        (int64_t)pf_m4_reference_origin_y_q16(
            fighter,
            previous_attacker_position_y_q16);
    const int64_t attacker_reference_origin_y =
        (int64_t)pf_m4_reference_origin_y_q16(
            fighter,
            scratch->position_y_q16[attacker_index]);
    int64_t previous_center_y;
    int64_t current_center_y;

    if (!pf_m4_shield_volume_for_player(
            fighter,
            scratch->position_x_q16[target_index],
            scratch->position_y_q16[target_index],
            scratch->action_state[target_index],
            scratch->hitlag_resume_action[target_index],
            scratch->shield_health_q16[target_index],
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
        (int64_t)volume.center_y_q16 +
        (previous_attacker_reference_origin_y -
         (int64_t)volume.center_y_q16) *
            (int64_t)fighter->shield_radius_x_q16 /
            (int64_t)fighter->shield_radius_y_q16 +
        ((int64_t)previous_sphere->center_y_q16 -
         previous_attacker_reference_origin_y);
    current_center_y =
        (int64_t)volume.center_y_q16 +
        (attacker_reference_origin_y -
         (int64_t)volume.center_y_q16) *
            (int64_t)fighter->shield_radius_x_q16 /
            (int64_t)fighter->shield_radius_y_q16 +
        ((int64_t)sphere->center_y_q16 -
         attacker_reference_origin_y);
    {
        const pf_m4_collision_capsule3_q16 hit = {
            (int64_t)previous_sphere->center_x_q16,
            previous_center_y,
            (int64_t)previous_sphere->center_z_q16,
            (int64_t)sphere->center_x_q16,
            current_center_y,
            (int64_t)sphere->center_z_q16,
            (int64_t)sphere->radius_q16};
        const pf_m4_collision_capsule3_q16 shield = {
            (int64_t)volume.center_x_q16,
            (int64_t)volume.center_y_q16,
            INT64_C(0),
            (int64_t)volume.center_x_q16,
            (int64_t)volume.center_y_q16,
            INT64_C(0),
            (int64_t)volume.radius_x_q16};

        return pf_m4_collision_capsule_capsule_overlap_q16(
            &hit,
            &shield);
    }
}

static const pf_m4_hit_sphere_inspection *
pf_m4_previous_matching_hit_sphere(
    const pf_m4_hit_sphere_inspection *sphere,
    const pf_m4_hit_sphere_inspection
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

static int pf_m4_reference_hit_sphere_can_continue(
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

static int pf_m4_boxes_overlap(
    int32_t left_a_q16,
    int32_t right_a_q16,
    int32_t top_a_q16,
    int32_t bottom_a_q16,
    int32_t left_b_q16,
    int32_t right_b_q16,
    int32_t top_b_q16,
    int32_t bottom_b_q16)
{
    return left_a_q16 <= right_b_q16 &&
           right_a_q16 >= left_b_q16 &&
           top_a_q16 <= bottom_b_q16 &&
           bottom_a_q16 >= top_b_q16;
}

static int pf_m4_action_is_throw(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_THROW_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_BACK ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_UP ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_DOWN;
}

static const pf_m4_throw_data *pf_m4_throw_for_action(
    const pf_m4_fighter_data *fighter,
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

static int32_t pf_m4_scaled_throw_velocity(
    int32_t base_q16,
    int32_t growth_q16,
    uint32_t damage_q16)
{
    int64_t result =
        (int64_t)base_q16 +
        (((int64_t)growth_q16 * (int64_t)damage_q16) >> 16U);

    if (result > (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        result = (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    else if (result < -(int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        result = -(int64_t)PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    return (int32_t)result;
}

static void pf_m4_clear_grab_links(
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

static void pf_m4_release_grab(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index)
{
    const int throw_edge_drop =
        scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_AIRBORNE &&
        pf_m4_action_is_throw(world->action_state[holder_index]);

    pf_m4_clear_grab_links(scratch, holder_index, target_index);
    if (scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        pf_m4_action_is_throw(scratch->action_state[holder_index]))
    {
        pf_m4_set_action_state(
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
            pf_m4_set_action_state(
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
            pf_m4_set_action_state(
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

static pf_status pf_m4_break_player_grab_links(
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
        pf_m4_release_grab(
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
        pf_m4_release_grab(
            world,
            scratch,
            player_index,
            target_index);
    }
    return PF_STATUS_OK;
}

static uint32_t pf_m4_stock_subscore(
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

static int pf_m4_player_stock_standing_is_better(
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
    return pf_m4_stock_subscore(world, scratch, left_index) >
           pf_m4_stock_subscore(world, scratch, right_index);
}

static uint32_t pf_m4_capture_rank(
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
                pf_m4_player_stock_standing_is_better(
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
                    pf_m4_stock_subscore(world, scratch, other_index);
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
                        pf_m4_stock_subscore(
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

static uint16_t pf_m4_grab_escape_ticks(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const uint32_t damage_q16 = scratch->damage_q16[player_index];

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const pf_m4_ssbm_mash_attributes *mash =
            pf_m4_ssbm_common_reference_mash();
        const uint32_t rank =
            pf_m4_capture_rank(world, scratch, player_index);
        const uint32_t rank_slot = rank + UINT32_C(1);
        const int64_t damage_term_q16 =
            ((int64_t)damage_q16 *
             (int64_t)mash->capture_damage_scale_q16) >>
            16U;
        const int64_t handicap_term_q16 =
            (int64_t)mash->capture_handicap_scale_q16 *
            ((int64_t)mash->capture_handicap_reference_q16 /
                 (int64_t)PF_Q16_ONE -
             INT64_C(9));
        const int64_t rank_term_q16 =
            (int64_t)mash->capture_rank_scale_q16 *
            ((int64_t)mash->capture_rank_reference_q16 /
                 (int64_t)PF_Q16_ONE -
             (int64_t)rank_slot);
        const int64_t timer_q16 =
            (int64_t)mash->capture_base_q16 +
            handicap_term_q16 + rank_term_q16 + damage_term_q16;
        const uint64_t decrement_q16 =
            (uint64_t)mash->capture_tick_decrement *
            (uint64_t)PF_Q16_ONE;
        uint64_t ticks =
            timer_q16 > INT64_C(0)
                ? ((uint64_t)timer_q16 + decrement_q16 - UINT64_C(1)) /
                      decrement_q16
                : UINT64_C(1);

        if (ticks > (uint64_t)PF_SIM_MAX_GRAB_ESCAPE_TICKS)
        {
            ticks = (uint64_t)PF_SIM_MAX_GRAB_ESCAPE_TICKS;
        }
        return (uint16_t)ticks;
    }

    {
    const uint64_t scaled =
        (uint64_t)damage_q16 *
        (uint64_t)(uint32_t)fighter->grab_escape_damage_ticks_q16;
    uint32_t ticks =
        (uint32_t)fighter->grab_escape_base_ticks +
        (uint32_t)(scaled >> 32U);

    if (ticks > (uint32_t)fighter->grab_escape_max_ticks)
    {
        ticks = (uint32_t)fighter->grab_escape_max_ticks;
    }
    return (uint16_t)ticks;
    }
}

static int32_t pf_m4_merge_damage_velocity_component(
    int32_t current_q16,
    int32_t incoming_q16)
{
    const int opposite_directions =
        (current_q16 < INT32_C(0) && incoming_q16 > INT32_C(0)) ||
        (current_q16 > INT32_C(0) && incoming_q16 < INT32_C(0));
    const uint32_t current_magnitude =
        (uint32_t)(current_q16 < INT32_C(0)
                       ? -(int64_t)current_q16
                       : (int64_t)current_q16);
    const uint32_t incoming_magnitude =
        (uint32_t)(incoming_q16 < INT32_C(0)
                       ? -(int64_t)incoming_q16
                       : (int64_t)incoming_q16);

    if (opposite_directions != 0)
    {
        /* Opposite-signed int32_t operands cannot overflow their sum. */
        return current_q16 + incoming_q16;
    }
    return incoming_magnitude > current_magnitude
               ? incoming_q16
               : current_q16;
}

static pf_status pf_m4_apply_hit_reaction(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint8_t source_player,
    uint32_t target_index,
    uint32_t damage_q16,
    int32_t launch_velocity_x_q16,
    int32_t launch_velocity_y_q16,
    uint16_t hitlag_ticks,
    uint16_t resolved_hitstun_ticks,
    int velocity_is_weighted,
    int launch_grounded,
    uint8_t damage_level,
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
    uint16_t hitstun_ticks;
    const int32_t previous_knockback_velocity_x_q16 =
        scratch->knockback_velocity_x_q16[target_index];
    const int32_t previous_knockback_velocity_y_q16 =
        scratch->knockback_velocity_y_q16[target_index];

    if (velocity_is_weighted == 0)
    {
        launch_velocity_x_q16 = pf_m4_apply_weight_q16(
            launch_velocity_x_q16,
            content->fighter.weight_q16);
        launch_velocity_y_q16 = pf_m4_apply_weight_q16(
            launch_velocity_y_q16,
            content->fighter.weight_q16);
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
        scratch->shield_health_q16[target_index] =
            content->fighter.shield_reset_health_q16;
    }
    scratch->damage_q16[target_index] = pf_m4_saturating_damage(
        scratch->damage_q16[target_index],
        damage_q16);
    hitstun_ticks =
        resolved_hitstun_ticks != UINT16_MAX
            ? resolved_hitstun_ticks
            : pf_m4_hitstun_ticks(
                  &content->fighter,
                  launch_velocity_x_q16,
                  launch_velocity_y_q16);
    armored = pf_m4_event_is_physical_hit(event_type) &&
              previous_action ==
                  (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
              content->fighter
                      .double_jump_armor_max_hitstun_ticks !=
                  UINT16_C(0) &&
              hitstun_ticks <=
                  content->fighter
                      .double_jump_armor_max_hitstun_ticks;
    reset = armored == 0 &&
            pf_m4_event_is_physical_hit(event_type) &&
            (previous_action == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
             previous_action == (uint8_t)PF_M4_ACTION_RESET_BOUND) &&
            damage_q16 <= content->fighter.reset_max_damage_q16 &&
            hitstun_ticks <=
                content->fighter.reset_max_hitstun_ticks;
    crouch_cancelled =
        armored == 0 && reset == 0 &&
        pf_m4_event_is_physical_hit(event_type) &&
        (previous_action == (uint8_t)PF_M4_ACTION_CROUCH_START ||
         previous_action == (uint8_t)PF_M4_ACTION_CROUCH) &&
        scratch->grounded[target_index] != UINT8_C(0) &&
        scratch->damage_q16[target_index] <=
            content->fighter.crouch_cancel_max_damage_q16;
    v_cancelled = armored == 0 && reset == 0 && crouch_cancelled == 0
                      ? pf_m4_player_v_cancelled(
                            &content->fighter,
                            scratch,
                            target_index)
                      : 0;
    scratch->knockback_velocity_x_q16[target_index] =
        armored != 0 || reset != 0
            ? INT32_C(0)
            : launch_velocity_x_q16;
    scratch->knockback_velocity_y_q16[target_index] =
        armored != 0
            ? INT32_C(0)
            : reset != 0
            ? -content->fighter.reset_bound_speed_q16
            : launch_velocity_y_q16;
    scratch->ground_knockback_velocity_q16[target_index] =
        armored != 0 || reset != 0 || launch_grounded == 0
            ? INT32_C(0)
            : launch_velocity_x_q16;
    scratch->hitstun_ticks[target_index] =
        armored != 0 ? UINT16_C(0) : hitstun_ticks;
    if (armored == 0 && pf_m4_event_is_physical_hit(event_type))
    {
        /* ftCo_Damage clears self/gr_vel when ordinary damage motion starts;
         * separately tracked knockback owns displacement until damage motion
         * releases control. */
        scratch->velocity_x_q16[target_index] = INT32_C(0);
        scratch->velocity_y_q16[target_index] = INT32_C(0);

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
                    scratch->position_x_q16[target_index] >
                            scratch->position_x_q16[source_player]
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
        scratch->knockback_velocity_x_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->knockback_velocity_x_q16[target_index],
                content->fighter.crouch_cancel_velocity_scale_q16);
        scratch->knockback_velocity_y_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->knockback_velocity_y_q16[target_index],
                content->fighter.crouch_cancel_velocity_scale_q16);
        scratch->hitstun_ticks[target_index] =
            pf_m4_scale_hitstun_ticks(
                scratch->hitstun_ticks[target_index],
                content->fighter.crouch_cancel_hitstun_scale_q16);
    }
    if (v_cancelled != 0)
    {
        scratch->knockback_velocity_x_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->knockback_velocity_x_q16[target_index],
                content->fighter.v_cancel_velocity_scale_q16);
        scratch->knockback_velocity_y_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->knockback_velocity_y_q16[target_index],
            content->fighter.v_cancel_velocity_scale_q16);
    }
    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        armored == 0 && reset == 0 &&
        scratch->last_hit_sequence[target_index] != UINT32_C(0))
    {
        const pf_m4_ssbm_damage_response_attributes *damage_response =
            pf_m4_ssbm_common_reference_damage_response();
        const uint64_t ticks_since_hit =
            world->tick - scratch->last_hit_tick[target_index];

        /* ftCo_Damage_CalcVel replaces both axes inside xFC frames.  Once
         * that window has elapsed it adds opposing components and otherwise
         * preserves whichever same-direction component has greater
         * magnitude. xF0_ground_kb_vel remains the incoming grounded scalar
         * and therefore intentionally is not merged here. */
        if (damage_response != NULL &&
            ticks_since_hit >=
                damage_response->damage_velocity_replace_window_ticks)
        {
            scratch->knockback_velocity_x_q16[target_index] =
                pf_m4_merge_damage_velocity_component(
                    previous_knockback_velocity_x_q16,
                    scratch->knockback_velocity_x_q16[target_index]);
            scratch->knockback_velocity_y_q16[target_index] =
                pf_m4_merge_damage_velocity_component(
                    previous_knockback_velocity_y_q16,
                    scratch->knockback_velocity_y_q16[target_index]);
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
    scratch->shield_recoil_x_q16[target_index] = INT32_C(0);
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
    /* DamageFly is airborne from its entry frame, including the hitlag-held
     * rows before its physics callback first advances.  Ground damage keeps
     * the source floor only when the imported knockback result selected a
     * grounded launch. */
    if (armored == 0 && reset == 0 && launch_grounded == 0)
    {
        scratch->grounded[target_index] = UINT8_C(0);
    }
    /* DownBound can retain its floor line while its animated ECB reports no
     * contact. Once a hit replaces that action with airborne damage, the
     * retained line is no longer an active support constraint. */
    if (scratch->grounded[target_index] == UINT8_C(0))
    {
        scratch->support[target_index] = (uint8_t)PF_M4_SURFACE_NONE;
    }
    pf_m4_set_action_state(
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
            damage_q16,
            scratch->knockback_velocity_x_q16[target_index],
            scratch->knockback_velocity_y_q16[target_index],
            event_flags,
            event_detail,
            &hit_sequence) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    scratch->last_hit_sequence[target_index] = hit_sequence;
    scratch->last_hit_tick[target_index] = world->tick;
    scratch->last_hit_damage_q16[target_index] = damage_q16;
    scratch->last_hit_attacker[target_index] = source_player;
    return PF_STATUS_OK;
}

static pf_status pf_m4_resolve_falcon_dive_capture(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
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
        scratch->position_x_q16[target_index] =
            scratch->position_x_q16[holder_index];
        scratch->position_y_q16[target_index] =
            scratch->position_y_q16[holder_index];
        scratch->velocity_x_q16[target_index] = INT32_C(0);
        scratch->velocity_y_q16[target_index] = INT32_C(0);
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
        const pf_m4_reference_hit_effect *effect =
            pf_m4_falcon_reference_primary_effect(
                PF_M4_FALCON_UP_SPECIAL_CATCH);
        pf_m4_melee_knockback_data hit;
        pf_m4_melee_knockback_result result;
        uint32_t damage_q16;
        uint32_t hit_sequence;

        if (effect == NULL || effect->damage == UINT8_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        damage_q16 = pf_m4_stale_scaled_damage_q16(
            &content->fighter,
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH,
            (uint32_t)effect->damage * UINT32_C(65536));
        hit.angle_degrees = effect->angle_degrees;
        hit.growth = effect->growth;
        hit.weight_set = effect->weight_set;
        hit.base = effect->base;
        hit.enabled = UINT8_C(1);
        (void)memset(hit.reserved, 0, sizeof(hit.reserved));
        result = pf_m4_melee_knockback(
            &hit,
            content->fighter.knockback_weight,
            damage_q16,
            pf_m4_saturating_damage(
                scratch->damage_q16[target_index],
                damage_q16));
        scratch->damage_q16[target_index] = pf_m4_saturating_damage(
            scratch->damage_q16[target_index],
            damage_q16);
        if (pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_HIT,
                (uint8_t)holder_index,
                (uint8_t)target_index,
                damage_q16,
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
        scratch->last_hit_damage_q16[target_index] = damage_q16;
        scratch->last_hit_attacker[target_index] =
            (uint8_t)holder_index;
        pf_m4_register_stale_move(
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH);
        scratch->attack_stale_registered[holder_index] = UINT8_C(1);
        scratch->hitlag_ticks[holder_index] = result.hitlag_ticks;
        scratch->hitlag_resume_action[holder_index] =
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH;
        pf_m4_set_action_state(
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
        const pf_m4_reference_throw *source =
            pf_m4_falcon_reference_throw(
                PF_M4_FALCON_UP_SPECIAL_THROW);
        const pf_m4_falcon_up_special_timing *timing =
            pf_m4_falcon_reference_up_special_timing();
        uint32_t damage_q16;

        if (source == NULL || timing == NULL ||
            scratch->action_ticks[holder_index] != UINT16_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        damage_q16 = pf_m4_stale_scaled_damage_q16(
            &content->fighter,
            scratch,
            holder_index,
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH,
            (uint32_t)source->damage * UINT32_C(65536));
        pf_m4_clear_grab_links(scratch, holder_index, target_index);
        scratch->grounded[target_index] = UINT8_C(0);
        scratch->support[target_index] =
            (uint8_t)PF_M4_SURFACE_NONE;
        /* CaptureCaptain computes the victim's damage-state duration from the
         * imported throw, then deliberately clears applied launch velocity in
         * ftCo_800DE7C0. Ordinary air gravity starts on the release sample. */
        if (pf_m4_apply_hit_reaction(
                content,
                world,
                scratch,
                (uint8_t)holder_index,
                target_index,
                damage_q16,
                INT32_C(0),
                INT32_C(0),
                UINT16_C(0),
                timing->victim_release_hitstun_ticks,
                1,
                0,
                UINT8_C(0),
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
        scratch->velocity_x_q16[target_index] = INT32_C(0);
        scratch->velocity_y_q16[target_index] =
            content->fighter.gravity_q16;
        scratch->knockback_velocity_y_q16[target_index] =
            content->fighter.gravity_q16;
    }
    return PF_STATUS_OK;
}

static pf_status pf_m4_resolve_throw_capture_hit(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index,
    uint8_t holder_action,
    int *out_hit)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_hit_effect *effect;
    const uint8_t target_bit =
        (uint8_t)(UINT32_C(1) << target_index);
    const uint8_t move_id = holder_action;
    uint32_t damage_q16;
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
        !pf_m4_falcon_reference_move_for_action(
            holder_action,
            &move_index) ||
        !pf_m4_falcon_reference_has_hit_geometry(move_index))
    {
        return PF_STATUS_OK;
    }
    effect = pf_m4_falcon_reference_effect_at_frame(
        move_index,
        scratch->action_ticks[holder_index]);
    if (effect == NULL)
    {
        return PF_STATUS_OK;
    }
    damage_q16 = pf_m4_current_move_stale_scaled_damage_q16(
        &content->fighter,
        scratch,
        holder_index,
        move_id,
        (uint32_t)effect->damage * UINT32_C(65536));
    hitlag_ticks = pf_m4_melee_hitlag_ticks(
        damage_q16,
        UINT8_C(0),
        UINT32_C(65536));
    scratch->damage_q16[target_index] =
        pf_m4_saturating_damage(
            scratch->damage_q16[target_index],
            damage_q16);
    if (pf_sim_push_event(
            scratch,
            world->tick,
            PF_SIM_EVENT_HIT,
            (uint8_t)holder_index,
            (uint8_t)target_index,
            damage_q16,
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
    scratch->last_hit_damage_q16[target_index] = damage_q16;
    scratch->last_hit_attacker[target_index] =
        (uint8_t)holder_index;
    scratch->attack_hit_mask[holder_index] |= target_bit;
    if (scratch->attack_stale_registered[holder_index] ==
        UINT8_C(0))
    {
        pf_m4_register_stale_move(
            scratch,
            holder_index,
            move_id);
        scratch->attack_stale_registered[holder_index] =
            UINT8_C(1);
    }
    scratch->hitlag_ticks[holder_index] = hitlag_ticks;
    scratch->hitlag_resume_action[holder_index] = holder_action;
    pf_m4_set_action_state(
        world,
        scratch,
        holder_index,
        (uint8_t)PF_M4_ACTION_HITLAG);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    scratch->hitlag_resume_action[target_index] =
        (uint8_t)PF_M4_ACTION_GRABBED;
    pf_m4_set_action_state(
        world,
        scratch,
        target_index,
        (uint8_t)PF_M4_ACTION_HITLAG);
    *out_hit = 1;
    return PF_STATUS_OK;
}

static pf_status pf_m4_resolve_grabs(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
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
        const pf_m4_throw_data *throw_data;
        int falcon_dive_capture = 0;
        uint32_t target_index;

        if (target_slot == UINT8_C(0))
        {
            continue;
        }
        if (holder_action == (uint8_t)PF_M4_ACTION_HITLAG &&
            (pf_m4_action_is_throw(
                 scratch->hitlag_resume_action[holder_index]) ||
             scratch->hitlag_resume_action[holder_index] ==
                 (uint8_t)PF_M4_ACTION_PUMMEL))
        {
            holder_action =
                scratch->hitlag_resume_action[holder_index];
        }
        throw_data =
            pf_m4_throw_for_action(&content->fighter, holder_action);
        target_index = (uint32_t)target_slot - UINT32_C(1);
        if (target_index >= (uint32_t)world->player_count ||
            target_index == holder_index)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (pf_m4_resolve_falcon_dive_capture(
                content,
                world,
                scratch,
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
            pf_m4_release_grab(
                world,
                scratch,
                holder_index,
                target_index);
            continue;
        }
        if (scratch->grab_escape_ticks[target_index] == UINT16_C(0))
        {
            pf_m4_release_grab(
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
                    scratch->damage_q16[target_index],
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

        scratch->position_x_q16[target_index] =
            scratch->position_x_q16[holder_index] +
            (int32_t)scratch->facing[holder_index] *
                content->fighter.grabbed_offset_x_q16;
        scratch->position_y_q16[target_index] =
            scratch->position_y_q16[holder_index] +
            content->fighter.grabbed_offset_y_q16;
        scratch->velocity_x_q16[target_index] = INT32_C(0);
        scratch->velocity_y_q16[target_index] = INT32_C(0);
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
                const uint32_t damage_q16 =
                    pf_m4_stale_scaled_damage_q16(
                        &content->fighter,
                        scratch,
                        holder_index,
                        move_id,
                        content->fighter.pummel_damage_q16);
                const uint16_t hitlag_ticks =
                    pf_m4_melee_hitlag_ticks(
                        damage_q16,
                        UINT8_C(0),
                        UINT32_C(65536));
                uint32_t pummel_sequence;

                scratch->damage_q16[target_index] =
                    pf_m4_saturating_damage(
                        scratch->damage_q16[target_index],
                        damage_q16);
                if (pf_sim_push_event(
                        scratch,
                        world->tick,
                        PF_SIM_EVENT_PUMMEL,
                        (uint8_t)holder_index,
                        (uint8_t)target_index,
                        damage_q16,
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
                scratch->last_hit_damage_q16[target_index] =
                    damage_q16;
                scratch->last_hit_attacker[target_index] =
                    (uint8_t)holder_index;
                scratch->attack_hit_mask[holder_index] |= target_bit;
                if (scratch->attack_stale_registered[holder_index] ==
                    UINT8_C(0))
                {
                    pf_m4_register_stale_move(
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
                pf_m4_set_action_state(
                    world,
                    scratch,
                    holder_index,
                    (uint8_t)PF_M4_ACTION_HITLAG);
                scratch->action_ticks[target_index] = UINT16_C(0);
                scratch->source_submotion[target_index] =
                    (uint16_t)
                        PF_M4_FALCON_SUBMOTION_CAPTURE_DAMAGE_HIGH;
                if (scratch->hitlag_ticks[target_index] < hitlag_ticks)
                {
                    scratch->hitlag_ticks[target_index] = hitlag_ticks;
                }
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_GRABBED;
                pf_m4_set_action_state(
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
            if (pf_m4_resolve_throw_capture_hit(
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
                const uint32_t damage_q16 =
                    pf_m4_current_move_stale_scaled_damage_q16(
                        &content->fighter,
                        scratch,
                        holder_index,
                        move_id,
                        throw_data->damage_q16);
                const uint32_t resulting_damage =
                    pf_m4_saturating_damage(
                        scratch->damage_q16[target_index],
                        damage_q16);
                int32_t launch_velocity_x;
                int32_t launch_velocity_y;
                uint16_t resolved_hitlag_ticks =
                    throw_data->hitlag_ticks;
                uint16_t resolved_hitstun_ticks = UINT16_MAX;
                int velocity_is_weighted = 0;
                int launch_grounded = 0;
                uint8_t damage_level = UINT8_C(0);
                uint8_t meteor_cancellable = UINT8_C(0);

                if (throw_data->melee_knockback.enabled != UINT8_C(0))
                {
                    const pf_m4_melee_knockback_result result =
                        pf_m4_melee_knockback_for_state(
                            &throw_data->melee_knockback,
                            content->fighter.knockback_weight,
                            damage_q16,
                            resulting_damage,
                            scratch->grounded[target_index],
                            UINT8_C(0),
                            UINT8_C(0),
                            UINT32_C(65536));

                    launch_velocity_x =
                        (int32_t)scratch->facing[holder_index] *
                        result.velocity_x_q16;
                    launch_velocity_y = -result.velocity_y_q16;
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
                        pf_m4_scaled_throw_velocity(
                            throw_data->base_velocity_x_q16,
                            throw_data->velocity_growth_x_q16,
                            resulting_damage);
                    launch_velocity_y =
                        pf_m4_scaled_throw_velocity(
                            throw_data->base_velocity_y_q16,
                            throw_data->velocity_growth_y_q16,
                            resulting_damage);
                }

                pf_m4_clear_grab_links(
                    scratch,
                    holder_index,
                    target_index);
                if (pf_m4_apply_hit_reaction(
                        content,
                        world,
                        scratch,
                        (uint8_t)holder_index,
                        target_index,
                        damage_q16,
                        launch_velocity_x,
                        launch_velocity_y,
                        resolved_hitlag_ticks,
                        resolved_hitstun_ticks,
                        velocity_is_weighted,
                        launch_grounded,
                        damage_level,
                        meteor_cancellable,
                        PF_SIM_EVENT_THROW,
                        (uint16_t)holder_action) != PF_STATUS_OK)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (resolved_hitlag_ticks != UINT16_C(0))
                {
                    scratch->hitlag_ticks[holder_index] =
                        resolved_hitlag_ticks;
                    scratch->hitlag_resume_action[holder_index] =
                        holder_action;
                    pf_m4_set_action_state(
                        world,
                        scratch,
                        holder_index,
                        (uint8_t)PF_M4_ACTION_HITLAG);
                }
                else
                {
                    scratch->velocity_x_q16[target_index] =
                        scratch->knockback_velocity_x_q16[target_index];
                    scratch->velocity_y_q16[target_index] =
                        scratch->knockback_velocity_y_q16[target_index];
                    scratch->knockback_velocity_x_q16[target_index] =
                        INT32_C(0);
                    scratch->knockback_velocity_y_q16[target_index] =
                        INT32_C(0);
                    scratch->grounded[target_index] = UINT8_C(0);
                    scratch->support[target_index] =
                        (uint8_t)PF_M4_SURFACE_NONE;
                    scratch->hitlag_ticks[target_index] =
                        UINT16_C(0);
                    scratch->hitlag_resume_action[target_index] =
                        UINT8_C(0);
                    pf_m4_set_action_state(
                        world,
                        scratch,
                        target_index,
                        (uint8_t)PF_M4_ACTION_HITSTUN);
                }
                if (scratch->attack_stale_registered[holder_index] ==
                    UINT8_C(0))
                {
                    pf_m4_register_stale_move(
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
        int32_t grabbox_left = INT32_C(0);
        int32_t grabbox_right = INT32_C(0);
        int32_t grabbox_top = INT32_C(0);
        int32_t grabbox_bottom = INT32_C(0);
        pf_m4_falcon_move_index geometry_move_index;
        pf_m4_hit_sphere_inspection
            grab_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
        pf_m4_hit_sphere_inspection
            previous_grab_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
        uint8_t grab_sphere_count = UINT8_C(0);
        uint8_t previous_grab_sphere_count = UINT8_C(0);
        const int falcon_dive_grab =
            scratch->action_state[attacker_index] ==
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
            scratch->action_state[attacker_index] ==
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR;
        const int reference_grab_geometry =
            pf_m4_falcon_geometry_move_for_grab(
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
            grab_sphere_count = pf_m4_grab_hit_spheres(
                content,
                scratch->position_x_q16[attacker_index],
                scratch->position_y_q16[attacker_index],
                scratch->facing[attacker_index],
                scratch->action_state[attacker_index],
                scratch->action_ticks[attacker_index],
                grab_spheres);
            if (grab_sphere_count == UINT8_C(0))
            {
                continue;
            }
            if (pf_m4_reference_hit_sphere_can_continue(
                    world,
                    scratch,
                    attacker_index))
            {
                previous_grab_sphere_count = pf_m4_grab_hit_spheres(
                    content,
                    world->position_x_q16[attacker_index],
                    world->position_y_q16[attacker_index],
                    world->facing[attacker_index],
                    world->action_state[attacker_index],
                    world->action_ticks[attacker_index],
                    previous_grab_spheres);
            }
        }
        else if (!pf_m4_grabbox(
                     content,
                     scratch->position_x_q16[attacker_index],
                     scratch->position_y_q16[attacker_index],
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
                pf_m4_action_is_recovery_invulnerable(
                    &content->fighter,
                    scratch->action_state[target_index],
                    scratch->action_ticks[target_index],
                    scratch->prone_orientation[target_index],
                    scratch->tech_direction[target_index],
                    scratch->facing[target_index]) ||
                (content->fighter.reference_frame_data_enabled !=
                     UINT8_C(0) &&
                 pf_m4_falcon_reference_body_invulnerable(
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
                    const pf_m4_hit_sphere_inspection *previous_sphere =
                        pf_m4_previous_matching_hit_sphere(
                            &grab_spheres[sphere_index],
                            previous_grab_spheres,
                            previous_grab_sphere_count);

                    if (pf_m4_grab_sphere_overlaps_player(
                            &content->fighter,
                            scratch,
                            target_index,
                            previous_sphere,
                            &grab_spheres[sphere_index]))
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
            else if (!pf_m4_hitbox_overlaps_player(
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

            scratch->grab_target_slot[attacker_index] =
                (uint8_t)(target_index + UINT32_C(1));
            scratch->grab_owner_slot[target_index] =
                (uint8_t)(attacker_index + UINT32_C(1));
            scratch->grab_escape_ticks[target_index] =
                falcon_dive_grab != 0
                    ? UINT16_C(1)
                    : pf_m4_grab_escape_ticks(
                          content,
                          world,
                          scratch,
                          target_index);
            scratch->mash_stick_x_direction[target_index] = INT8_C(0);
            scratch->mash_stick_y_direction[target_index] = INT8_C(0);
            pf_m4_set_action_state(
                world,
                scratch,
                attacker_index,
                falcon_dive_grab != 0
                    ? (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH
                    : (uint8_t)PF_M4_ACTION_GRAB_HOLD);
            scratch->action_ticks[attacker_index] = UINT16_C(0);
            scratch->velocity_x_q16[attacker_index] = INT32_C(0);
            pf_m4_set_action_state(
                world,
                scratch,
                target_index,
                (uint8_t)PF_M4_ACTION_GRABBED);
            scratch->action_ticks[target_index] = UINT16_C(0);
            scratch->source_submotion[attacker_index] =
                (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_WAIT;
            scratch->source_submotion[target_index] =
                (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_HIGH;
            scratch->charge_ticks[target_index] = UINT16_C(0);
            scratch->smash_charge_ticks[target_index] = UINT16_C(0);
            if (falcon_dive_grab != 0 &&
                scratch->grounded[target_index] != UINT8_C(0))
            {
                scratch->position_x_q16[attacker_index] =
                    scratch->position_x_q16[target_index];
                scratch->position_y_q16[attacker_index] =
                    scratch->position_y_q16[target_index];
                scratch->grounded[attacker_index] = UINT8_C(1);
                scratch->support[attacker_index] =
                    scratch->support[target_index];
            }
            scratch->position_x_q16[target_index] =
                falcon_dive_grab != 0
                    ? scratch->position_x_q16[attacker_index]
                    : scratch->position_x_q16[attacker_index] +
                          (int32_t)scratch->facing[attacker_index] *
                              content->fighter.grabbed_offset_x_q16;
            scratch->position_y_q16[target_index] =
                falcon_dive_grab != 0
                    ? scratch->position_y_q16[attacker_index]
                    : scratch->position_y_q16[attacker_index] +
                          content->fighter.grabbed_offset_y_q16;
            scratch->velocity_x_q16[target_index] = INT32_C(0);
            scratch->velocity_y_q16[target_index] = INT32_C(0);
            scratch->knockback_velocity_x_q16[target_index] = INT32_C(0);
            scratch->knockback_velocity_y_q16[target_index] = INT32_C(0);
            scratch->ground_knockback_velocity_q16[target_index] = INT32_C(0);
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
                    scratch->damage_q16[target_index],
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

static pf_status pf_m4_resolve_item_combat(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    const pf_m4_item_data *item = &content->item;
    const uint8_t source_slot = scratch->item_source_slot;
    const uint32_t source_index =
        (uint32_t)source_slot - UINT32_C(1);
    const int32_t hitbox_left =
        scratch->item_position_x_q16 - item->hitbox_half_width_q16;
    const int32_t hitbox_right =
        scratch->item_position_x_q16 + item->hitbox_half_width_q16;
    const int32_t hitbox_top =
        scratch->item_position_y_q16 - item->hitbox_half_height_q16;
    const int32_t hitbox_bottom =
        scratch->item_position_y_q16 + item->hitbox_half_height_q16;
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
            scratch->item_velocity_x_q16 < INT32_C(0)
                ? INT8_C(-1)
                : scratch->item_velocity_x_q16 > INT32_C(0)
                ? INT8_C(1)
                : scratch->facing[source_index];
        const int shield_overlap =
            pf_m4_hitbox_overlaps_shield(
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
            (scratch->item_hit_mask & target_bit) != UINT8_C(0) ||
            scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
            pf_m4_action_is_recovery_invulnerable(
                &content->fighter,
                scratch->action_state[target_index],
                scratch->action_ticks[target_index],
                scratch->prone_orientation[target_index],
                scratch->tech_direction[target_index],
                scratch->facing[target_index]) ||
            (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_falcon_reference_body_invulnerable(
                 scratch->source_submotion[target_index],
                 scratch->action_ticks[target_index])) ||
            (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
             world->team[source_index] == world->team[target_index]) ||
            !pf_m4_hitbox_overlaps_player_or_shield(
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

        if (pf_m4_break_player_grab_links(
                world,
                scratch,
                target_index) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        scratch->item_hit_mask |= target_bit;
        scratch->item_velocity_y_q16 =
            item->hit_bounce_velocity_y_q16;

        const uint8_t move_id =
            (uint8_t)PF_M4_ACTION_ITEM_THROW;
        const uint32_t damage_q16 =
            pf_m4_stale_scaled_damage_q16(
                &content->fighter,
                scratch,
                source_index,
                move_id,
                item->damage_q16);

        if (pf_m4_action_is_guarding(
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
            if (pf_m4_apply_shield_hit(
                    world,
                    scratch,
                    &content->fighter,
                    source_index,
                    target_index,
                    damage_q16,
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
            const uint32_t resulting_damage =
                pf_m4_saturating_damage(
                    scratch->damage_q16[target_index],
                    damage_q16);
            const int32_t knockback_x = pf_m4_scaled_knockback(
                item->base_knockback_x_q16,
                item->knockback_growth_q16,
                resulting_damage,
                0);
            const int32_t knockback_y = pf_m4_scaled_knockback(
                item->base_knockback_y_q16,
                item->knockback_growth_q16,
                resulting_damage,
                1);

            if (pf_m4_apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    (uint8_t)source_index,
                    target_index,
                    damage_q16,
                    (int32_t)launch_direction * knockback_x,
                    -knockback_y,
                    item->hitlag_ticks,
                    UINT16_MAX,
                    0,
                    0,
                    UINT8_C(0),
                    UINT8_C(0),
                    PF_SIM_EVENT_ITEM_HIT,
                    (uint16_t)scratch->item_throw_direction) !=
                PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (scratch->item_stale_registered == UINT8_C(0))
            {
                pf_m4_register_stale_move(
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

static void pf_m4_clear_projectile_combat(pf_sim_scratch *scratch)
{
    scratch->projectile_position_x_q16 = INT32_C(0);
    scratch->projectile_position_y_q16 = INT32_C(0);
    scratch->projectile_velocity_x_q16 = INT32_C(0);
    scratch->projectile_velocity_y_q16 = INT32_C(0);
    scratch->projectile_lifetime_ticks = UINT16_C(0);
    scratch->projectile_state =
        (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE;
    scratch->projectile_owner_slot = UINT8_C(0);
}

static pf_status pf_m4_resolve_projectile_combat(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    const pf_m4_projectile_data *projectile = &content->projectile;
    const uint8_t owner_slot = scratch->projectile_owner_slot;
    const uint32_t owner_index =
        (uint32_t)owner_slot - UINT32_C(1);
    const int32_t hitbox_left =
        scratch->projectile_position_x_q16 -
        projectile->half_width_q16;
    const int32_t hitbox_right =
        scratch->projectile_position_x_q16 +
        projectile->half_width_q16;
    const int32_t hitbox_top =
        scratch->projectile_position_y_q16 -
        projectile->half_height_q16;
    const int32_t hitbox_bottom =
        scratch->projectile_position_y_q16 +
        projectile->half_height_q16;
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
            scratch->projectile_velocity_x_q16 < INT32_C(0)
                ? INT8_C(-1)
                : INT8_C(1);
        int32_t reflector_left;
        int32_t reflector_right;
        int32_t reflector_top;
        int32_t reflector_bottom;
        const int reflector_active =
            (scratch->action_state[target_index] ==
                 (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
             scratch->action_state[target_index] ==
                 (uint8_t)PF_M4_ACTION_REFLECTOR_AIR) &&
            pf_m4_attack_hitbox(
                content,
                scratch->position_x_q16[target_index],
                scratch->position_y_q16[target_index],
                scratch->facing[target_index],
                scratch->action_state[target_index],
                scratch->action_ticks[target_index],
                scratch->source_submotion[target_index],
                &reflector_left,
                &reflector_right,
                &reflector_top,
                &reflector_bottom) &&
            pf_m4_boxes_overlap(
                hitbox_left,
                hitbox_right,
                hitbox_top,
                hitbox_bottom,
                reflector_left,
                reflector_right,
                reflector_top,
                reflector_bottom);
        const int shield_overlap =
            pf_m4_hitbox_overlaps_shield(
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
            scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
            pf_m4_action_is_recovery_invulnerable(
                &content->fighter,
                scratch->action_state[target_index],
                scratch->action_ticks[target_index],
                scratch->prone_orientation[target_index],
                scratch->tech_direction[target_index],
                scratch->facing[target_index]) ||
            (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_falcon_reference_body_invulnerable(
                 scratch->source_submotion[target_index],
                 scratch->action_ticks[target_index])) ||
            (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
             world->team[owner_index] == world->team[target_index]) ||
            (reflector_active == 0 &&
             !pf_m4_hitbox_overlaps_player_or_shield(
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
            const int32_t reflected_velocity_x =
                -scratch->projectile_velocity_x_q16;

            scratch->projectile_velocity_x_q16 =
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
                    scratch->projectile_velocity_y_q16,
                    UINT16_C(0),
                    (uint16_t)scratch->action_state[target_index],
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            return PF_STATUS_OK;
        }

        if (pf_m4_action_is_guarding(
                scratch->action_state[target_index]) &&
            shield_overlap != 0 &&
            scratch->action_state[target_index] ==
                (uint8_t)PF_M4_ACTION_SHIELD &&
            scratch->action_ticks[target_index] <=
                projectile->powershield_reflect_window_ticks &&
            scratch->shield_strength[target_index] >=
                content->fighter.digital_trigger_threshold)
        {
            const int32_t reflected_velocity_x =
                -scratch->projectile_velocity_x_q16;

            scratch->projectile_velocity_x_q16 =
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
                    scratch->projectile_velocity_y_q16,
                    UINT16_C(0),
                    (uint16_t)PF_M4_ACTION_SHIELD,
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            return PF_STATUS_OK;
        }

        if (pf_m4_break_player_grab_links(
                world,
                scratch,
                target_index) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (pf_m4_action_is_guarding(
                scratch->action_state[target_index]) &&
            shield_overlap != 0)
        {
            const uint8_t move_id =
                (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND;
            const uint32_t damage_q16 =
                pf_m4_stale_scaled_damage_q16(
                    &content->fighter,
                    scratch,
                    owner_index,
                    move_id,
                    projectile->damage_q16);
            if (pf_m4_apply_shield_hit(
                    world,
                    scratch,
                    &content->fighter,
                    owner_index,
                    target_index,
                    damage_q16,
                    INT16_C(0),
                    projectile->hitlag_ticks,
                    (int32_t)launch_direction,
                    0,
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND,
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            pf_m4_clear_projectile_combat(scratch);
            return PF_STATUS_OK;
        }

        {
            const uint8_t move_id =
                (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND;
            const uint32_t damage_q16 =
                pf_m4_stale_scaled_damage_q16(
                    &content->fighter,
                    scratch,
                    owner_index,
                    move_id,
                    projectile->damage_q16);
            const uint32_t resulting_damage =
                pf_m4_saturating_damage(
                    scratch->damage_q16[target_index],
                    damage_q16);
            const int32_t knockback_x = pf_m4_scaled_knockback(
                projectile->base_knockback_x_q16,
                projectile->knockback_growth_q16,
                resulting_damage,
                0);
            const int32_t knockback_y = pf_m4_scaled_knockback(
                projectile->base_knockback_y_q16,
                projectile->knockback_growth_q16,
                resulting_damage,
                1);

            if (pf_m4_apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    (uint8_t)owner_index,
                    target_index,
                    damage_q16,
                    (int32_t)launch_direction * knockback_x,
                    -knockback_y,
                    projectile->hitlag_ticks,
                    UINT16_MAX,
                    0,
                    0,
                    UINT8_C(0),
                    UINT8_C(0),
                    PF_SIM_EVENT_PROJECTILE_HIT,
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND) !=
                PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            pf_m4_register_stale_move(
                scratch,
                owner_index,
                move_id);
        }
        pf_m4_clear_projectile_combat(scratch);
        return PF_STATUS_OK;
    }
    return PF_STATUS_OK;
}

static pf_status pf_m4_resolve_falcon_side_special_searches(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    const pf_m4_falcon_side_special_timing *timing =
        pf_m4_falcon_reference_side_special_timing();
    const pf_m4_falcon_special_attributes *attributes =
        pf_m4_falcon_reference_special_attributes();
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
        const pf_m4_reference_search_sphere *source_spheres;
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
            pf_m4_falcon_reference_side_special_search_spheres(
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
                const pf_m4_reference_search_sphere *source =
                    &source_spheres[sphere_index];
                const pf_m4_hit_sphere_inspection sphere = {
                    .center_x_q16 =
                        scratch->position_x_q16[attacker_index] +
                        (int32_t)scratch->facing[attacker_index] *
                            source->offset_x_q16,
                    .center_y_q16 =
                        scratch->position_y_q16[attacker_index] +
                        source->offset_y_q16,
                    .center_z_q16 =
                        (int32_t)scratch->facing[attacker_index] *
                        source->offset_z_q16,
                    .radius_q16 = source->radius_q16,
                    .effect_index = UINT8_C(0),
                    .hitbox_id = sphere_index,
                    .group_id = UINT8_C(0),
                    .collision_state = UINT8_C(2)};

                if (pf_m4_hit_sphere_overlaps_player(
                        &content->fighter,
                        scratch,
                        target_index,
                        &sphere,
                        &sphere))
                {
                    detected = 1;
                    break;
                }
            }
            if (detected != 0)
            {
                pf_m4_set_action_state(
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
                    scratch->velocity_x_q16[attacker_index] =
                        pf_m4_multiply_q16(
                            scratch->velocity_x_q16[attacker_index],
                            attributes->specials_gr_vel_x_q16);
                    scratch->velocity_y_q16[attacker_index] = INT32_C(0);
                }
                break;
            }
        }
    }
    return PF_STATUS_OK;
}

static int pf_m4_select_reference_target_hit(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t attacker_index,
    uint32_t target_index,
    uint8_t attacker_action,
    pf_m4_falcon_move_index geometry_move,
    const pf_m4_attack_runtime *base_attack,
    const pf_m4_hit_sphere_inspection
        spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t sphere_count,
    const pf_m4_hit_sphere_inspection
        previous_spheres[PF_M4_INSPECTION_HIT_SPHERE_CAPACITY],
    uint8_t previous_sphere_count,
    uint16_t cancelled_group_mask,
    pf_m4_attack_runtime *out_attack,
    uint8_t *out_group,
    int *out_shield_overlap)
{
    uint8_t sphere_index;

    if (content == NULL || world == NULL || scratch == NULL ||
        base_attack == NULL || spheres == NULL ||
        previous_spheres == NULL || out_attack == NULL ||
        out_group == NULL || out_shield_overlap == NULL)
    {
        return -1;
    }
    for (sphere_index = UINT8_C(0);
         sphere_index < sphere_count;
         ++sphere_index)
    {
        const pf_m4_hit_sphere_inspection *sphere =
            &spheres[sphere_index];
        const pf_m4_hit_sphere_inspection *previous_sphere;
        const pf_m4_reference_hit_effect *effect;
        int32_t previous_attacker_position_y_q16;
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
        previous_sphere = pf_m4_previous_matching_hit_sphere(
            sphere,
            previous_spheres,
            previous_sphere_count);
        previous_attacker_position_y_q16 =
            previous_sphere == sphere
                ? scratch->position_y_q16[attacker_index]
                : world->position_y_q16[attacker_index];
        shield_overlap = pf_m4_hit_sphere_overlaps_shield(
            &content->fighter,
            scratch,
            attacker_index,
            target_index,
            previous_attacker_position_y_q16,
            previous_sphere,
            sphere);
        player_overlap = pf_m4_hit_sphere_overlaps_player(
            &content->fighter,
            scratch,
            target_index,
            previous_sphere,
            sphere);
        effect = pf_m4_falcon_reference_effect(
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
        pf_m4_copy_attack_state(out_attack, base_attack);
        pf_m4_apply_falcon_reference_effect(
            &content->fighter,
            attacker_action,
            scratch->smash_charge_ticks[attacker_index],
            effect,
            out_attack);
        pf_m4_apply_stale_scaled_reference_damage(
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

pf_status pf_m4_resolve_combat(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
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
    int32_t attacker_shield_pushback_q16[PF_SIM_MAX_PLAYERS];
    pf_m4_attack_runtime attacker_attack[PF_SIM_MAX_PLAYERS];
    pf_m4_attack_runtime attacker_base_attack[PF_SIM_MAX_PLAYERS];
    pf_m4_attack_runtime
        target_attack[PF_SIM_MAX_PLAYERS][PF_SIM_MAX_PLAYERS];
    uint8_t
        target_attack_group[PF_SIM_MAX_PLAYERS][PF_SIM_MAX_PLAYERS];
    pf_m4_hit_sphere_inspection
        attacker_spheres[PF_SIM_MAX_PLAYERS]
                        [PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    pf_m4_hit_sphere_inspection
        attacker_previous_spheres[PF_SIM_MAX_PLAYERS]
                                 [PF_M4_INSPECTION_HIT_SPHERE_CAPACITY];
    pf_m4_falcon_move_index geometry_move[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_sphere_count[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_previous_sphere_count[PF_SIM_MAX_PLAYERS];
    uint32_t attacker_index;
    uint32_t target_index;

    if (content == NULL || world == NULL || scratch == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    if (content->fighter.reference_frame_data_enabled != UINT8_C(0) &&
        pf_m4_resolve_falcon_side_special_searches(
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
        attacker_shield_pushback_q16,
        0,
        sizeof(attacker_shield_pushback_q16));
    (void)memset(
        target_powershield,
        0,
        sizeof(target_powershield));
    (void)memset(rebound_damage, 0, sizeof(rebound_damage));
    (void)memset(rebound_source, UINT8_MAX, sizeof(rebound_source));
    (void)memset(cancelled_group_mask, 0, sizeof(cancelled_group_mask));
    (void)memset(target_attack_group, UINT8_MAX, sizeof(target_attack_group));
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
        int32_t hitbox_left;
        int32_t hitbox_right;
        int32_t hitbox_top;
        int32_t hitbox_bottom;

        if (scratch->active[attacker_index] == UINT8_C(0) ||
            !pf_m4_attack_hitbox(
                content,
                scratch->position_x_q16[attacker_index],
                scratch->position_y_q16[attacker_index],
                scratch->facing[attacker_index],
                scratch->action_state[attacker_index],
                scratch->action_ticks[attacker_index],
                scratch->source_submotion[attacker_index],
                &hitbox_left,
                &hitbox_right,
                &hitbox_top,
                &hitbox_bottom))
        {
            continue;
        }
        attacker_action[attacker_index] =
            scratch->action_state[attacker_index];
        if (!pf_m4_attack_for_state(
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
            pf_m4_attack_hit_spheres(
                content,
                scratch->position_x_q16[attacker_index],
                scratch->position_y_q16[attacker_index],
                scratch->facing[attacker_index],
                scratch->action_state[attacker_index],
                scratch->action_ticks[attacker_index],
                attacker_spheres[attacker_index]);
        if (attacker_sphere_count[attacker_index] != UINT8_C(0) &&
            pf_m4_reference_hit_sphere_can_continue(
                world,
                scratch,
                attacker_index))
        {
            attacker_previous_sphere_count[attacker_index] =
                pf_m4_attack_hit_spheres(
                    content,
                    world->position_x_q16[attacker_index],
                    world->position_y_q16[attacker_index],
                    world->facing[attacker_index],
                    world->action_state[attacker_index],
                    world->action_ticks[attacker_index],
                    attacker_previous_spheres[attacker_index]);
        }
        if (attacker_sphere_count[attacker_index] != UINT8_C(0))
        {
            if (!pf_m4_falcon_reference_move_for_action(
                    attacker_action[attacker_index],
                    &geometry_move[attacker_index]))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        else if (pf_m4_apply_falcon_reference_frame(
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
            pf_m4_apply_stale_scaled_reference_damage(
                &content->fighter,
                scratch,
                attacker_index,
                attacker_action[attacker_index],
                &attacker_attack[attacker_index]);
        }
        pf_m4_copy_attack_state(
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
                    : pf_m4_hitbox_overlaps_shield(
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
                pf_m4_action_is_recovery_invulnerable(
                    &content->fighter,
                    scratch->action_state[target_index],
                    scratch->action_ticks[target_index],
                    scratch->prone_orientation[target_index],
                    scratch->tech_direction[target_index],
                    scratch->facing[target_index]) ||
                (content->fighter.reference_frame_data_enabled !=
                     UINT8_C(0) &&
                 pf_m4_falcon_reference_body_invulnerable(
                     scratch->source_submotion[target_index],
                     scratch->action_ticks[target_index])) ||
                (scratch->attack_hit_mask[attacker_index] &
                 target_bit) != UINT8_C(0) ||
                scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[attacker_index] ==
                     world->team[target_index]) ||
                (uses_reference_spheres == 0 &&
                 !pf_m4_hitbox_overlaps_player_or_shield(
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
                const int selection = pf_m4_select_reference_target_hit(
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
                    pf_m4_copy_attack_state(
                        &attacker_attack[attacker_index],
                        &target_attack[target_index][attacker_index]);
                }
            }
            else
            {
                pf_m4_copy_attack_state(
                    &target_attack[target_index][attacker_index],
                    &attacker_attack[attacker_index]);
            }

            target_hit_mask[target_index] |=
                (uint8_t)(UINT32_C(1) << attacker_index);
            attacker_hit[attacker_index] = UINT8_C(1);
            if (pf_m4_action_is_guarding(
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
        const pf_m4_ssbm_clank_attributes *clank =
            pf_m4_ssbm_common_reference_clank();
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
                    const pf_m4_hit_sphere_inspection *first_sphere =
                        &attacker_spheres[first_index][first_sphere_index];
                    const pf_m4_reference_hit_effect *first_effect =
                        pf_m4_falcon_reference_effect(
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
                        const pf_m4_hit_sphere_inspection *second_sphere =
                            &attacker_spheres[second_index]
                                             [second_sphere_index];
                        const pf_m4_reference_hit_effect *second_effect =
                            pf_m4_falcon_reference_effect(
                                geometry_move[second_index],
                                second_sphere->effect_index);
                        const pf_m4_hit_sphere_inspection *first_previous;
                        const pf_m4_hit_sphere_inspection *second_previous;
                        pf_m4_collision_capsule3_q16 first_capsule;
                        pf_m4_collision_capsule3_q16 second_capsule;
                        pf_m4_attack_runtime first_selected;
                        pf_m4_attack_runtime second_selected;
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
                        first_previous = pf_m4_previous_matching_hit_sphere(
                            first_sphere,
                            attacker_previous_spheres[first_index],
                            attacker_previous_sphere_count[first_index]);
                        second_previous = pf_m4_previous_matching_hit_sphere(
                            second_sphere,
                            attacker_previous_spheres[second_index],
                            attacker_previous_sphere_count[second_index]);
                        first_capsule = (pf_m4_collision_capsule3_q16){
                            first_previous->center_x_q16,
                            first_previous->center_y_q16,
                            first_previous->center_z_q16,
                            first_sphere->center_x_q16,
                            first_sphere->center_y_q16,
                            first_sphere->center_z_q16,
                            first_sphere->radius_q16};
                        second_capsule = (pf_m4_collision_capsule3_q16){
                            second_previous->center_x_q16,
                            second_previous->center_y_q16,
                            second_previous->center_z_q16,
                            second_sphere->center_x_q16,
                            second_sphere->center_y_q16,
                            second_sphere->center_z_q16,
                            second_sphere->radius_q16};
                        if (!pf_m4_collision_capsule_capsule_overlap_q16(
                                &first_capsule,
                                &second_capsule))
                        {
                            continue;
                        }
                        pf_m4_copy_attack_state(
                            &first_selected,
                            &attacker_base_attack[first_index]);
                        pf_m4_apply_falcon_reference_effect(
                            &content->fighter,
                            attacker_action[first_index],
                            scratch->smash_charge_ticks[first_index],
                            first_effect,
                            &first_selected);
                        pf_m4_apply_stale_scaled_reference_damage(
                            &content->fighter,
                            scratch,
                            first_index,
                            attacker_action[first_index],
                            &first_selected);
                        pf_m4_copy_attack_state(
                            &second_selected,
                            &attacker_base_attack[second_index]);
                        pf_m4_apply_falcon_reference_effect(
                            &content->fighter,
                            attacker_action[second_index],
                            scratch->smash_charge_ticks[second_index],
                            second_effect,
                            &second_selected);
                        pf_m4_apply_stale_scaled_reference_damage(
                            &content->fighter,
                            scratch,
                            second_index,
                            attacker_action[second_index],
                            &second_selected);
                        first_damage = (uint16_t)(first_selected.damage_q16 >> 16U);
                        second_damage = (uint16_t)(second_selected.damage_q16 >> 16U);
                        if (first_selected.damage_q16 != UINT32_C(0) &&
                            first_damage == UINT16_C(0))
                        {
                            first_damage = UINT16_C(1);
                        }
                        if (second_selected.damage_q16 != UINT32_C(0) &&
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
                selection = pf_m4_select_reference_target_hit(
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
                    &shield_overlap);
                if (selection < 0)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (selection != 0)
                {
                    target_hit_mask[target_index] |= bit;
                    if (pf_m4_action_is_guarding(
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
                pf_m4_copy_attack_state(
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
                    attacker_hit[attacker_index] = UINT8_C(1);
                    if (target_attack[target_index][attacker_index]
                            .hitlag_ticks >
                        attacker_attack[attacker_index].hitlag_ticks)
                    {
                        pf_m4_copy_attack_state(
                            &attacker_attack[attacker_index],
                            &target_attack[target_index][attacker_index]);
                    }
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

        if (pf_m4_break_player_grab_links(
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
                const pf_m4_attack_runtime *attack =
                    &target_attack[target_index][attacker_index];
                const int powershield =
                    target_powershield[target_index] != UINT8_C(0);
                const uint16_t target_shield_hitlag_ticks =
                    attack->target_hitlag_multiplier_q16 !=
                            UINT32_C(65536)
                        ? pf_m4_melee_hitlag_ticks(
                              attack->damage_q16,
                              UINT8_C(0),
                              attack->target_hitlag_multiplier_q16)
                        : attack->hitlag_ticks;
                pf_m4_shield_hit_response response;

                if (pf_m4_apply_shield_hit(
                        world,
                        scratch,
                        &content->fighter,
                        attacker_index,
                        target_index,
                        attack->damage_q16,
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
                if (attacker_shield_pushback_q16[attacker_index] ==
                    INT32_C(0))
                {
                    attacker_shield_pushback_q16[attacker_index] =
                        response.attacker_pushback_q16;
                }
            }
        }

        if (hurt_mask != UINT8_C(0))
        {
            uint32_t total_damage_q16 = UINT32_C(0);
            uint32_t resulting_damage;
            uint32_t best_owner = UINT32_MAX;
            int32_t best_knockback_q16 = INT32_C(-1);
            int32_t launch_velocity_x = INT32_C(0);
            int32_t launch_velocity_y = INT32_C(0);
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
                    total_damage_q16 = pf_m4_saturating_damage(
                        total_damage_q16,
                        target_attack[target_index][attacker_index]
                            .damage_q16);
                }
            }
            resulting_damage = pf_m4_saturating_damage(
                scratch->damage_q16[target_index],
                total_damage_q16);

            for (attacker_index = UINT32_C(0);
                 attacker_index < (uint32_t)world->player_count;
                 ++attacker_index)
            {
                const uint8_t attacker_bit =
                    (uint8_t)(UINT32_C(1) << attacker_index);
                const pf_m4_attack_runtime *attack;
                int32_t candidate_velocity_x;
                int32_t candidate_velocity_y;
                int32_t candidate_knockback_q16;
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
                    const pf_m4_falcon_special_attributes *attributes =
                        pf_m4_falcon_reference_special_attributes();

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
                    const pf_m4_melee_knockback_result result =
                        pf_m4_melee_knockback_for_state(
                            attack->melee_knockback,
                            content->fighter.knockback_weight,
                            attack->damage_q16,
                            resulting_damage,
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
                            attack->target_hitlag_multiplier_q16);

                    candidate_velocity_x =
                        (int32_t)scratch->facing[attacker_index] *
                        (int32_t)attack->direction * result.velocity_x_q16;
                    candidate_velocity_y = -result.velocity_y_q16;
                    candidate_knockback_q16 = result.knockback_q16;
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
                    const int32_t knockback_x = pf_m4_scaled_knockback(
                        attack->base_knockback_x_q16,
                        attack->knockback_growth_q16,
                        resulting_damage,
                        0);
                    const int32_t knockback_y = pf_m4_scaled_knockback(
                        attack->base_knockback_y_q16,
                        attack->knockback_growth_q16,
                        resulting_damage,
                        1);

                    candidate_velocity_x =
                        (int32_t)scratch->facing[attacker_index] *
                        (int32_t)attack->direction * knockback_x;
                    candidate_velocity_y =
                        (int32_t)attack->vertical_direction * knockback_y;
                    candidate_knockback_q16 =
                        knockback_x > knockback_y ? knockback_x : knockback_y;
                }
                if (candidate_knockback_q16 > best_knockback_q16)
                {
                    best_owner = attacker_index;
                    best_knockback_q16 = candidate_knockback_q16;
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
                    pf_m4_register_stale_move(
                        scratch,
                        attacker_index,
                        pf_m4_stale_move_id_for_action(
                            attacker_action[attacker_index]));
                    scratch->attack_stale_registered[attacker_index] =
                        UINT8_C(1);
                }
            }
            if (best_owner == UINT32_MAX ||
                pf_m4_apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    (uint8_t)best_owner,
                    target_index,
                    total_damage_q16,
                    launch_velocity_x,
                    launch_velocity_y,
                    resolved_hitlag_ticks,
                    resolved_hitstun_ticks,
                    velocity_is_weighted,
                    launch_grounded,
                    damage_level,
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
            const pf_m4_ssbm_clank_attributes *clank =
                pf_m4_ssbm_common_reference_clank();
            const pf_m4_falcon_common_attributes *common =
                pf_m4_falcon_reference_common_attributes();
            int32_t strength_q16;
            int32_t speed_q16;
            int32_t animation_speed_q16;
            uint32_t rebound_duration_ticks;
            const uint8_t source = rebound_source[attacker_index];

            if (clank == NULL || common == NULL ||
                source >= world->player_count)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            strength_q16 =
                (int32_t)rebound_damage[attacker_index] *
                    clank->rebound_strength_damage_scale_q16 +
                clank->rebound_strength_base_q16;
            if (strength_q16 <= INT32_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            speed_q16 =
                pf_m4_multiply_q16(
                    strength_q16,
                    clank->rebound_velocity_strength_scale_q16) +
                clank->rebound_velocity_base_q16;
            animation_speed_q16 =
                (int32_t)(((int64_t)(
                    common->rebound_animation_length_q16 +
                    INT32_C(6554)) << 16U) /
                    (int64_t)strength_q16);
            if (animation_speed_q16 <= INT32_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            rebound_duration_ticks =
                ((uint32_t)common->rebound_animation_length_q16 +
                 (uint32_t)animation_speed_q16 - UINT32_C(1)) /
                (uint32_t)animation_speed_q16;
            if (rebound_duration_ticks == UINT32_C(0) ||
                rebound_duration_ticks > UINT16_MAX)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            scratch->velocity_x_q16[attacker_index] =
                scratch->position_x_q16[source] >
                        scratch->position_x_q16[attacker_index]
                    ? -speed_q16
                    : speed_q16;
            scratch->velocity_y_q16[attacker_index] = INT32_C(0);
            scratch->action_ticks[attacker_index] = UINT16_C(0);
            scratch->attack_hit_mask[attacker_index] = UINT8_C(0);
            scratch->attack_stale_registered[attacker_index] = UINT8_C(0);
            scratch->rebound_duration_ticks[attacker_index] =
                (uint16_t)rebound_duration_ticks;
            pf_m4_set_action_state(
                world,
                scratch,
                attacker_index,
                (uint8_t)PF_M4_ACTION_REBOUND_STOP);
        }
    }

    if (pf_m4_resolve_grabs(content, world, scratch) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        pf_m4_attack_runtime attack;

        if (attacker_hit[attacker_index] != UINT8_C(0) &&
            target_hit_mask[attacker_index] == UINT8_C(0) &&
            scratch->action_state[attacker_index] !=
                (uint8_t)PF_M4_ACTION_REBOUND_STOP &&
            scratch->action_state[attacker_index] !=
                (uint8_t)PF_M4_ACTION_REBOUND)
        {
            attack = attacker_attack[attacker_index];
            if (attacker_blocked[attacker_index] != UINT8_C(0))
            {
                scratch->shield_recoil_x_q16[attacker_index] =
                    -(int32_t)scratch->facing[attacker_index] *
                    (int32_t)attack.direction *
                    attacker_shield_pushback_q16[attacker_index];
                scratch->shield_recoil_mask =
                    (uint8_t)(
                        scratch->shield_recoil_mask |
                        (uint8_t)(UINT8_C(1) << attacker_index));
            }
            scratch->hitlag_ticks[attacker_index] =
                attack.hitlag_ticks;
            scratch->hitlag_resume_action[attacker_index] =
                attack.action_state;
            pf_m4_set_action_state(
                world,
                scratch,
                attacker_index,
                (uint8_t)PF_M4_ACTION_HITLAG);
        }
    }

    if (pf_m4_resolve_projectile_combat(
            content,
            world,
            scratch) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    return pf_m4_resolve_item_combat(content, world, scratch);
}
