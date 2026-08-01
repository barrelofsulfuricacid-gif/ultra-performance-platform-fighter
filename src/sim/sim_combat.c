#include "sim_internal.h"

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
    uint16_t action_ticks)
{
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
        return action_ticks <
               fighter->getup_roll_invulnerability_ticks;
    }
    return action_state ==
               (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
           action_ticks <
               fighter->getup_attack_invulnerability_ticks;
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
    int8_t direction;
    int8_t vertical_direction;
    uint8_t action_state;
} pf_m4_attack_runtime;

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
            return &fighter->forward_attack;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
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
        if ((action_state ==
                 (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
             action_state ==
                 (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
             action_state ==
                 (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK) &&
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
        out_attack->active_begin_tick = attack->startup_ticks;
        out_attack->active_end_tick = (uint16_t)(
            (uint32_t)attack->startup_ticks +
            (uint32_t)attack->active_ticks - UINT32_C(1));
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
            fighter->aerial_startup_ticks;
        out_attack->active_end_tick =
            (uint16_t)(
                (uint32_t)fighter->aerial_startup_ticks +
                (uint32_t)fighter->aerial_active_ticks -
                UINT32_C(1));
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

static uint32_t pf_m4_shield_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16)
{
    const uint64_t product =
        (uint64_t)attack_damage_q16 *
        (uint64_t)fighter->shield_damage_multiplier_q16;
    const uint64_t damage = product >> 16U;

    return damage > (uint64_t)UINT32_MAX
               ? UINT32_MAX
               : (uint32_t)damage;
}

static uint16_t pf_m4_shield_stun_ticks(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16)
{
    int64_t stun_q16 =
        ((int64_t)attack_damage_q16 *
         (int64_t)fighter->shield_stun_damage_multiplier_q16) >>
        16U;
    int64_t ticks;

    stun_q16 += (int64_t)fighter->shield_stun_base_q16;
    stun_q16 = stun_q16 * INT64_C(200) / INT64_C(201);
    ticks = stun_q16 >> 16U;
    if (ticks < INT64_C(1))
    {
        ticks = INT64_C(1);
    }
    if (ticks > (int64_t)UINT16_MAX)
    {
        ticks = (int64_t)UINT16_MAX;
    }
    return (uint16_t)ticks;
}

static int32_t pf_m4_shield_defender_pushback_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    int powershield,
    uint16_t shield_strength)
{
    int64_t pushback =
        ((int64_t)attack_damage_q16 *
         (int64_t)fighter->shield_defender_pushback_damage_q16) >>
        16U;

    pushback +=
        (int64_t)fighter->shield_defender_pushback_base_q16;
    if (powershield == 0)
    {
        int64_t light_scale_q16 =
            fighter->light_shield_defender_pushback_scale_q16;

        pushback =
            pushback *
            (int64_t)fighter->shield_defender_pushback_scale_q16 /
            (int64_t)PF_Q16_ONE;
        if (shield_strength >= fighter->digital_trigger_threshold)
        {
            light_scale_q16 = (int64_t)PF_Q16_ONE;
        }
        else if (shield_strength >
                 fighter->light_shield_trigger_threshold)
        {
            light_scale_q16 -=
                ((int64_t)(
                     fighter
                         ->light_shield_defender_pushback_scale_q16 -
                     PF_Q16_ONE) *
                 (int64_t)(
                     shield_strength -
                     fighter->light_shield_trigger_threshold)) /
                (int64_t)(
                    fighter->digital_trigger_threshold -
                    fighter->light_shield_trigger_threshold);
        }
        pushback =
            pushback * light_scale_q16 / (int64_t)PF_Q16_ONE;
    }
    if (pushback > INT64_C(2) * (int64_t)PF_Q16_ONE)
    {
        pushback = INT64_C(2) * (int64_t)PF_Q16_ONE;
    }
    return (int32_t)pushback;
}

static int32_t pf_m4_shield_attacker_pushback_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16)
{
    int64_t pushback =
        ((int64_t)attack_damage_q16 *
         (int64_t)fighter->shield_attacker_pushback_damage_q16) >>
        16U;

    pushback +=
        (int64_t)fighter->shield_attacker_pushback_base_q16;
    if (pushback > (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16)
    {
        pushback = (int64_t)PF_SIM_MAX_MOTION_SPEED_Q16;
    }
    return (int32_t)pushback;
}

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
    int32_t *out_bottom_q16)
{
    pf_m4_attack_runtime attack;
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

    if (!pf_m4_attack_for_action(
            content,
            action_state,
            action_ticks,
            UINT16_C(0),
            UINT16_C(0),
            &attack))
    {
        return 0;
    }
    if (action_ticks < attack.active_begin_tick ||
        action_ticks > attack.active_end_tick)
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
    const uint16_t active_begin =
        content != NULL
            ? (uint16_t)(content->fighter.grab_startup_ticks +
                         UINT16_C(1))
            : UINT16_C(0);
    const uint16_t active_end =
        content != NULL
            ? (uint16_t)(content->fighter.grab_startup_ticks +
                         content->fighter.grab_active_ticks)
            : UINT16_C(0);
    int64_t center_x;
    int64_t center_y;

    if (content == NULL ||
        out_left_q16 == NULL ||
        out_right_q16 == NULL ||
        out_top_q16 == NULL ||
        out_bottom_q16 == NULL ||
        action_state != (uint8_t)PF_M4_ACTION_GRAB ||
        action_ticks < active_begin ||
        action_ticks > active_end)
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

int pf_m4_shield_box(
    const pf_m4_fighter_data *fighter,
    int32_t position_x_q16,
    int32_t position_y_q16,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint32_t shield_health_q16,
    uint16_t shield_strength,
    int16_t shield_tilt_x,
    int16_t shield_tilt_y,
    int32_t *out_left_q16,
    int32_t *out_right_q16,
    int32_t *out_top_q16,
    int32_t *out_bottom_q16)
{
    int32_t density_scale_q16;
    int32_t health_scale_q16;
    int32_t combined_scale_q16;
    int32_t size_scale_q16;
    int32_t half_width_q16;
    int32_t half_height_q16;
    int32_t offset_x_q16;
    int32_t offset_y_q16;
    int64_t center_x_q16;
    int64_t center_y_q16;

    if (fighter == NULL || out_left_q16 == NULL ||
        out_right_q16 == NULL || out_top_q16 == NULL ||
        out_bottom_q16 == NULL || shield_strength == UINT16_C(0) ||
        shield_health_q16 == UINT32_C(0) ||
        !pf_m4_action_has_shield_volume(
            action_state,
            hitlag_resume_action))
    {
        return 0;
    }

    if (shield_strength <= fighter->light_shield_trigger_threshold)
    {
        density_scale_q16 = PF_Q16_ONE;
    }
    else if (shield_strength >= fighter->digital_trigger_threshold)
    {
        density_scale_q16 = fighter->dense_shield_size_scale_q16;
    }
    else
    {
        density_scale_q16 =
            PF_Q16_ONE -
            (int32_t)(
                ((int64_t)(
                     PF_Q16_ONE -
                     fighter->dense_shield_size_scale_q16) *
                 (int64_t)(
                     shield_strength -
                     fighter->light_shield_trigger_threshold)) /
                (int64_t)(
                    fighter->digital_trigger_threshold -
                    fighter->light_shield_trigger_threshold));
    }
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
    half_width_q16 =
        (int32_t)(
            ((int64_t)fighter->shield_half_width_q16 *
             (int64_t)size_scale_q16) /
            (int64_t)PF_Q16_ONE);
    half_height_q16 =
        (int32_t)(
            ((int64_t)fighter->shield_half_height_q16 *
             (int64_t)size_scale_q16) /
            (int64_t)PF_Q16_ONE);
    offset_x_q16 =
        (int32_t)(
            ((int64_t)fighter->shield_tilt_max_x_q16 *
             (int64_t)shield_tilt_x) /
            (shield_tilt_x < INT16_C(0)
                 ? INT64_C(32768)
                 : INT64_C(32767)));
    offset_y_q16 =
        (int32_t)(
            ((int64_t)fighter->shield_tilt_max_y_q16 *
             (int64_t)shield_tilt_y) /
            (shield_tilt_y < INT16_C(0)
                 ? INT64_C(32768)
                 : INT64_C(32767)));
    center_x_q16 = (int64_t)position_x_q16 + offset_x_q16;
    center_y_q16 = (int64_t)position_y_q16 + offset_y_q16;
    *out_left_q16 =
        (int32_t)(center_x_q16 - (int64_t)half_width_q16);
    *out_right_q16 =
        (int32_t)(center_x_q16 + (int64_t)half_width_q16);
    *out_top_q16 =
        (int32_t)(center_y_q16 - (int64_t)half_height_q16);
    *out_bottom_q16 =
        (int32_t)(center_y_q16 + (int64_t)half_height_q16);
    return 1;
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
    int32_t shield_left_q16;
    int32_t shield_right_q16;
    int32_t shield_top_q16;
    int32_t shield_bottom_q16;

    return pf_m4_shield_box(
               fighter,
               scratch->position_x_q16[target_index],
               scratch->position_y_q16[target_index],
               scratch->action_state[target_index],
               scratch->hitlag_resume_action[target_index],
               scratch->shield_health_q16[target_index],
               scratch->shield_strength[target_index],
               scratch->shield_tilt_x[target_index],
               scratch->shield_tilt_y[target_index],
               &shield_left_q16,
               &shield_right_q16,
               &shield_top_q16,
               &shield_bottom_q16) &&
           hitbox_left_q16 <= shield_right_q16 &&
           hitbox_right_q16 >= shield_left_q16 &&
           hitbox_top_q16 <= shield_bottom_q16 &&
           hitbox_bottom_q16 >= shield_top_q16;
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
}

static void pf_m4_release_grab(
    pf_sim_scratch *scratch,
    uint32_t holder_index,
    uint32_t target_index)
{
    pf_m4_clear_grab_links(scratch, holder_index, target_index);
    if (scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        scratch->action_state[holder_index] ==
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        pf_m4_action_is_throw(scratch->action_state[holder_index]))
    {
        scratch->action_state[holder_index] =
            (uint8_t)PF_M4_ACTION_GRAB_RELEASE;
        scratch->action_ticks[holder_index] = UINT16_C(0);
    }
    if (scratch->action_state[target_index] ==
        (uint8_t)PF_M4_ACTION_GRABBED)
    {
        scratch->action_state[target_index] =
            (uint8_t)PF_M4_ACTION_GRAB_RELEASE;
        scratch->action_ticks[target_index] = UINT16_C(0);
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
        pf_m4_release_grab(scratch, owner_index, player_index);
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
        pf_m4_release_grab(scratch, player_index, target_index);
    }
    return PF_STATUS_OK;
}

static uint16_t pf_m4_grab_escape_ticks(
    const pf_m4_fighter_data *fighter,
    uint32_t damage_q16)
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

    launch_velocity_x_q16 = pf_m4_apply_weight_q16(
        launch_velocity_x_q16,
        content->fighter.weight_q16);
    launch_velocity_y_q16 = pf_m4_apply_weight_q16(
        launch_velocity_y_q16,
        content->fighter.weight_q16);

    if (previous_action == (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
        previous_action ==
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND)
    {
        scratch->charge_ticks[target_index] = UINT16_C(0);
    }
    scratch->smash_charge_ticks[target_index] = UINT16_C(0);
    if (scratch->action_state[target_index] ==
        (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        scratch->shield_health_q16[target_index] =
            content->fighter.shield_reset_health_q16;
    }
    scratch->damage_q16[target_index] = pf_m4_saturating_damage(
        scratch->damage_q16[target_index],
        damage_q16);
    hitstun_ticks = pf_m4_hitstun_ticks(
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
        previous_action == (uint8_t)PF_M4_ACTION_CROUCH &&
        scratch->grounded[target_index] != UINT8_C(0) &&
        scratch->damage_q16[target_index] <=
            content->fighter.crouch_cancel_max_damage_q16;
    v_cancelled = armored == 0 && reset == 0 && crouch_cancelled == 0
                      ? pf_m4_player_v_cancelled(
                            &content->fighter,
                            scratch,
                            target_index)
                      : 0;
    scratch->pending_velocity_x_q16[target_index] =
        armored != 0 || reset != 0
            ? INT32_C(0)
            : launch_velocity_x_q16;
    scratch->pending_velocity_y_q16[target_index] =
        armored != 0
            ? INT32_C(0)
            : reset != 0
            ? -content->fighter.reset_bound_speed_q16
            : launch_velocity_y_q16;
    scratch->hitstun_ticks[target_index] =
        armored != 0 ? UINT16_C(0) : hitstun_ticks;
    if (crouch_cancelled != 0)
    {
        scratch->pending_velocity_x_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->pending_velocity_x_q16[target_index],
                content->fighter.crouch_cancel_velocity_scale_q16);
        scratch->pending_velocity_y_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->pending_velocity_y_q16[target_index],
                content->fighter.crouch_cancel_velocity_scale_q16);
        scratch->hitstun_ticks[target_index] =
            pf_m4_scale_hitstun_ticks(
                scratch->hitstun_ticks[target_index],
                content->fighter.crouch_cancel_hitstun_scale_q16);
    }
    if (v_cancelled != 0)
    {
        scratch->pending_velocity_x_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->pending_velocity_x_q16[target_index],
                content->fighter.v_cancel_velocity_scale_q16);
        scratch->pending_velocity_y_q16[target_index] =
            pf_m4_scale_velocity_q16(
                scratch->pending_velocity_y_q16[target_index],
                content->fighter.v_cancel_velocity_scale_q16);
    }
    scratch->tumble[target_index] =
        armored == 0 && reset == 0 &&
                scratch->hitstun_ticks[target_index] >=
                    content->fighter.tumble_hitstun_threshold_ticks
            ? UINT8_C(1)
            : UINT8_C(0);
    scratch->shield_stun_ticks[target_index] = UINT16_C(0);
    scratch->powershield[target_index] = UINT8_C(0);
    scratch->shield_strength[target_index] = UINT16_C(0);
    scratch->shield_tilt_x[target_index] = INT16_C(0);
    scratch->shield_tilt_y[target_index] = INT16_C(0);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    scratch->hitlag_resume_action[target_index] =
        armored != 0
            ? previous_action
            : reset != 0
            ? (uint8_t)PF_M4_ACTION_RESET_BOUND
            : (uint8_t)PF_M4_ACTION_HITSTUN;
    scratch->action_state[target_index] =
        (uint8_t)PF_M4_ACTION_HITLAG;
    if (armored == 0)
    {
        scratch->action_ticks[target_index] = UINT16_C(0);
    }
    scratch->dash_direction[target_index] = INT8_C(0);
    scratch->short_hop_latched[target_index] = UINT8_C(0);
    scratch->fast_fall[target_index] = UINT8_C(0);
    scratch->attack_hit_mask[target_index] = UINT8_C(0);
    scratch->sdi_pulse_count[target_index] = UINT8_C(0);
    scratch->sdi_direction_x[target_index] = INT8_C(0);
    scratch->sdi_direction_y[target_index] = INT8_C(0);
    scratch->tech_direction[target_index] = INT8_C(0);

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
            scratch->pending_velocity_x_q16[target_index],
            scratch->pending_velocity_y_q16[target_index],
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
        const uint8_t holder_action =
            scratch->action_state[holder_index];
        const pf_m4_throw_data *throw_data =
            pf_m4_throw_for_action(&content->fighter, holder_action);
        uint32_t target_index;

        if (target_slot == UINT8_C(0))
        {
            continue;
        }
        target_index = (uint32_t)target_slot - UINT32_C(1);
        if (target_index >= (uint32_t)world->player_count ||
            target_index == holder_index)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (scratch->active[holder_index] == UINT8_C(0) ||
            scratch->active[target_index] == UINT8_C(0) ||
            (holder_action != (uint8_t)PF_M4_ACTION_GRAB_HOLD &&
             holder_action != (uint8_t)PF_M4_ACTION_PUMMEL &&
             throw_data == NULL) ||
            scratch->action_state[target_index] !=
                (uint8_t)PF_M4_ACTION_GRABBED ||
            scratch->grab_owner_slot[target_index] !=
                (uint8_t)(holder_index + UINT32_C(1)))
        {
            pf_m4_release_grab(scratch, holder_index, target_index);
            continue;
        }
        if (scratch->grab_escape_ticks[target_index] == UINT16_C(0))
        {
            pf_m4_release_grab(scratch, holder_index, target_index);
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

            if (action_ticks >= content->fighter.pummel_total_ticks)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (action_ticks == content->fighter.pummel_hit_tick)
            {
                uint32_t pummel_sequence;

                scratch->damage_q16[target_index] =
                    pf_m4_saturating_damage(
                        scratch->damage_q16[target_index],
                        content->fighter.pummel_damage_q16);
                if (pf_sim_push_event(
                        scratch,
                        world->tick,
                        PF_SIM_EVENT_PUMMEL,
                        (uint8_t)holder_index,
                        (uint8_t)target_index,
                        content->fighter.pummel_damage_q16,
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
                    content->fighter.pummel_damage_q16;
                scratch->last_hit_attacker[target_index] =
                    (uint8_t)holder_index;
            }
        }
        if (throw_data != NULL)
        {
            const uint16_t action_ticks =
                scratch->action_ticks[holder_index];

            if (action_ticks > throw_data->release_tick)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (action_ticks == throw_data->release_tick)
            {
                const uint32_t resulting_damage =
                    pf_m4_saturating_damage(
                        scratch->damage_q16[target_index],
                        throw_data->damage_q16);
                const int32_t launch_velocity_x =
                    (int32_t)scratch->facing[holder_index] *
                    pf_m4_scaled_throw_velocity(
                        throw_data->base_velocity_x_q16,
                        throw_data->velocity_growth_x_q16,
                        resulting_damage);
                const int32_t launch_velocity_y =
                    pf_m4_scaled_throw_velocity(
                        throw_data->base_velocity_y_q16,
                        throw_data->velocity_growth_y_q16,
                        resulting_damage);

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
                        throw_data->damage_q16,
                        launch_velocity_x,
                        launch_velocity_y,
                        throw_data->hitlag_ticks,
                        PF_SIM_EVENT_THROW,
                        (uint16_t)holder_action) != PF_STATUS_OK)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                scratch->hitlag_ticks[holder_index] =
                    throw_data->hitlag_ticks;
                scratch->hitlag_resume_action[holder_index] =
                    holder_action;
                scratch->action_state[holder_index] =
                    (uint8_t)PF_M4_ACTION_HITLAG;
            }
        }
    }

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        int32_t grabbox_left;
        int32_t grabbox_right;
        int32_t grabbox_top;
        int32_t grabbox_bottom;
        uint32_t target_index;

        if (scratch->active[attacker_index] == UINT8_C(0) ||
            scratch->grab_target_slot[attacker_index] != UINT8_C(0) ||
            scratch->grab_owner_slot[attacker_index] != UINT8_C(0) ||
            !pf_m4_grabbox(
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
                    scratch->action_ticks[target_index]) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[attacker_index] ==
                     world->team[target_index]) ||
                !pf_m4_hitbox_overlaps_player(
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
                pf_m4_grab_escape_ticks(
                    &content->fighter,
                    scratch->damage_q16[target_index]);
            scratch->action_state[attacker_index] =
                (uint8_t)PF_M4_ACTION_GRAB_HOLD;
            scratch->action_ticks[attacker_index] = UINT16_C(0);
            scratch->velocity_x_q16[attacker_index] = INT32_C(0);
            scratch->action_state[target_index] =
                (uint8_t)PF_M4_ACTION_GRABBED;
            scratch->action_ticks[target_index] = UINT16_C(0);
            scratch->position_x_q16[target_index] =
                scratch->position_x_q16[attacker_index] +
                (int32_t)scratch->facing[attacker_index] *
                    content->fighter.grabbed_offset_x_q16;
            scratch->position_y_q16[target_index] =
                scratch->position_y_q16[attacker_index] +
                content->fighter.grabbed_offset_y_q16;
            scratch->velocity_x_q16[target_index] = INT32_C(0);
            scratch->velocity_y_q16[target_index] = INT32_C(0);
            scratch->pending_velocity_x_q16[target_index] = INT32_C(0);
            scratch->pending_velocity_y_q16[target_index] = INT32_C(0);
            scratch->hitlag_ticks[target_index] = UINT16_C(0);
            scratch->hitstun_ticks[target_index] = UINT16_C(0);
            scratch->shield_stun_ticks[target_index] = UINT16_C(0);
            scratch->shield_strength[target_index] = UINT16_C(0);
            scratch->shield_tilt_x[target_index] = INT16_C(0);
            scratch->shield_tilt_y[target_index] = INT16_C(0);
            scratch->grounded[target_index] =
                scratch->grounded[attacker_index];
            scratch->support[target_index] =
                scratch->support[attacker_index];
            scratch->dash_direction[target_index] = INT8_C(0);
            scratch->short_hop_latched[target_index] = UINT8_C(0);
            scratch->fast_fall[target_index] = UINT8_C(0);
            scratch->attack_hit_mask[target_index] = UINT8_C(0);
            scratch->powershield[target_index] = UINT8_C(0);
            scratch->tumble[target_index] = UINT8_C(0);
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
                    (uint16_t)PF_M4_ACTION_GRAB,
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
                scratch->action_ticks[target_index]) ||
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
            const uint32_t shield_damage =
                pf_m4_shield_damage_q16(
                    &content->fighter,
                    item->damage_q16);
            const int32_t defender_pushback =
                pf_m4_shield_defender_pushback_q16(
                    &content->fighter,
                    item->damage_q16,
                    powershield,
                    scratch->shield_strength[target_index]);

            if (!powershield)
            {
                scratch->shield_health_q16[target_index] =
                    shield_damage >=
                            scratch->shield_health_q16[target_index]
                        ? UINT32_C(0)
                        : scratch->shield_health_q16[target_index] -
                              shield_damage;
            }
            scratch->velocity_x_q16[target_index] =
                (int32_t)launch_direction * defender_pushback;
            scratch->velocity_y_q16[target_index] = INT32_C(0);
            scratch->powershield[target_index] =
                powershield ? UINT8_C(1) : UINT8_C(0);
            scratch->hitlag_ticks[target_index] = item->hitlag_ticks;
            scratch->action_state[target_index] =
                (uint8_t)PF_M4_ACTION_HITLAG;
            scratch->dash_direction[target_index] = INT8_C(0);
            scratch->short_hop_latched[target_index] = UINT8_C(0);
            scratch->fast_fall[target_index] = UINT8_C(0);
            if (scratch->shield_health_q16[target_index] ==
                UINT32_C(0))
            {
                scratch->shield_stun_ticks[target_index] =
                    UINT16_C(0);
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
                scratch->action_ticks[target_index] = UINT16_C(0);
                scratch->shield_strength[target_index] = UINT16_C(0);
                scratch->shield_tilt_x[target_index] = INT16_C(0);
                scratch->shield_tilt_y[target_index] = INT16_C(0);
            }
            else
            {
                scratch->shield_stun_ticks[target_index] =
                    pf_m4_shield_stun_ticks(
                        &content->fighter,
                        item->damage_q16);
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_SHIELD_STUN;
            }
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    powershield
                        ? PF_SIM_EVENT_POWERSHIELD
                        : (scratch->shield_health_q16[target_index] ==
                                   UINT32_C(0)
                               ? PF_SIM_EVENT_SHIELD_BREAK
                               : PF_SIM_EVENT_SHIELD_BLOCK),
                    (uint8_t)source_index,
                    (uint8_t)target_index,
                    powershield ? UINT32_C(0) : shield_damage,
                    scratch->velocity_x_q16[target_index],
                    scratch->velocity_y_q16[target_index],
                    UINT16_C(0),
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
                    item->damage_q16);
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
                    item->damage_q16,
                    (int32_t)launch_direction * knockback_x,
                    -knockback_y,
                    item->hitlag_ticks,
                    PF_SIM_EVENT_ITEM_HIT,
                    (uint16_t)scratch->item_throw_direction) !=
                PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
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
                scratch->action_ticks[target_index]) ||
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
            const uint32_t shield_damage =
                pf_m4_shield_damage_q16(
                    &content->fighter,
                    projectile->damage_q16);
            const int32_t defender_pushback =
                pf_m4_shield_defender_pushback_q16(
                    &content->fighter,
                    projectile->damage_q16,
                    0,
                    scratch->shield_strength[target_index]);
            pf_sim_event_type event_type;

            scratch->shield_health_q16[target_index] =
                shield_damage >=
                        scratch->shield_health_q16[target_index]
                    ? UINT32_C(0)
                    : scratch->shield_health_q16[target_index] -
                          shield_damage;
            scratch->velocity_x_q16[target_index] =
                (int32_t)launch_direction * defender_pushback;
            scratch->velocity_y_q16[target_index] = INT32_C(0);
            scratch->powershield[target_index] = UINT8_C(0);
            scratch->hitlag_ticks[target_index] =
                projectile->hitlag_ticks;
            scratch->action_state[target_index] =
                (uint8_t)PF_M4_ACTION_HITLAG;
            scratch->dash_direction[target_index] = INT8_C(0);
            scratch->short_hop_latched[target_index] = UINT8_C(0);
            scratch->fast_fall[target_index] = UINT8_C(0);
            if (scratch->shield_health_q16[target_index] ==
                UINT32_C(0))
            {
                scratch->shield_stun_ticks[target_index] = UINT16_C(0);
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
                scratch->action_ticks[target_index] = UINT16_C(0);
                scratch->shield_strength[target_index] = UINT16_C(0);
                scratch->shield_tilt_x[target_index] = INT16_C(0);
                scratch->shield_tilt_y[target_index] = INT16_C(0);
                event_type = PF_SIM_EVENT_SHIELD_BREAK;
            }
            else
            {
                scratch->shield_stun_ticks[target_index] =
                    pf_m4_shield_stun_ticks(
                        &content->fighter,
                        projectile->damage_q16);
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_SHIELD_STUN;
                event_type = PF_SIM_EVENT_SHIELD_BLOCK;
            }
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    event_type,
                    (uint8_t)owner_index,
                    (uint8_t)target_index,
                    shield_damage,
                    scratch->velocity_x_q16[target_index],
                    scratch->velocity_y_q16[target_index],
                    UINT16_C(0),
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND,
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            pf_m4_clear_projectile_combat(scratch);
            return PF_STATUS_OK;
        }

        {
            const uint32_t resulting_damage =
                pf_m4_saturating_damage(
                    scratch->damage_q16[target_index],
                    projectile->damage_q16);
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
                    projectile->damage_q16,
                    (int32_t)launch_direction * knockback_x,
                    -knockback_y,
                    projectile->hitlag_ticks,
                    PF_SIM_EVENT_PROJECTILE_HIT,
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND) !=
                PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        pf_m4_clear_projectile_combat(scratch);
        return PF_STATUS_OK;
    }
    return PF_STATUS_OK;
}

pf_status pf_m4_resolve_combat(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch)
{
    uint8_t target_owner[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_action[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_hit[PF_SIM_MAX_PLAYERS];
    uint8_t attacker_blocked[PF_SIM_MAX_PLAYERS];
    uint8_t target_blocked[PF_SIM_MAX_PLAYERS];
    uint8_t target_powershield[PF_SIM_MAX_PLAYERS];
    pf_m4_attack_runtime attacker_attack[PF_SIM_MAX_PLAYERS];
    uint32_t attacker_index;
    uint32_t target_index;

    if (content == NULL || world == NULL || scratch == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    if (pf_m4_resolve_grabs(content, world, scratch) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    (void)memset(target_owner, UINT8_MAX, sizeof(target_owner));
    (void)memset(attacker_action, UINT8_MAX, sizeof(attacker_action));
    (void)memset(attacker_hit, 0, sizeof(attacker_hit));
    (void)memset(attacker_blocked, 0, sizeof(attacker_blocked));
    (void)memset(target_blocked, 0, sizeof(target_blocked));
    (void)memset(
        target_powershield,
        0,
        sizeof(target_powershield));

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
                &hitbox_left,
                &hitbox_right,
                &hitbox_top,
                &hitbox_bottom))
        {
            continue;
        }
        attacker_action[attacker_index] =
            scratch->action_state[attacker_index];
        if (!pf_m4_attack_for_action(
                content,
                attacker_action[attacker_index],
                scratch->action_ticks[attacker_index],
                scratch->charge_ticks[attacker_index],
                scratch->smash_charge_ticks[attacker_index],
                &attacker_attack[attacker_index]))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }

        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            const uint8_t target_bit =
                (uint8_t)(UINT32_C(1) << target_index);
            const int shield_overlap =
                pf_m4_hitbox_overlaps_shield(
                    &content->fighter,
                    scratch,
                    target_index,
                    hitbox_left,
                    hitbox_right,
                    hitbox_top,
                    hitbox_bottom);

            if (target_index == attacker_index ||
                scratch->active[target_index] == UINT8_C(0) ||
                scratch
                        ->respawn_invulnerability_ticks[target_index] !=
                    UINT16_C(0) ||
                scratch->ledge_invulnerability_ticks[target_index] !=
                    UINT16_C(0) ||
                target_owner[target_index] != UINT8_MAX ||
                pf_m4_action_is_recovery_invulnerable(
                    &content->fighter,
                    scratch->action_state[target_index],
                    scratch->action_ticks[target_index]) ||
                (scratch->attack_hit_mask[attacker_index] &
                 target_bit) != UINT8_C(0) ||
                scratch->hitlag_ticks[target_index] != UINT16_C(0) ||
                (world->mode == (uint8_t)PF_SIM_MODE_TEAMS &&
                 world->team[attacker_index] ==
                     world->team[target_index]) ||
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

            target_owner[target_index] = (uint8_t)attacker_index;
            attacker_hit[attacker_index] = UINT8_C(1);
            if (pf_m4_action_is_guarding(
                    scratch->action_state[target_index]) &&
                shield_overlap != 0)
            {
                target_blocked[target_index] = UINT8_C(1);
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

    for (target_index = UINT32_C(0);
         target_index < (uint32_t)world->player_count;
         ++target_index)
    {
        const uint8_t owner = target_owner[target_index];
        pf_m4_attack_runtime attack;
        int32_t knockback_x;
        int32_t knockback_y;

        if (owner == UINT8_MAX)
        {
            continue;
        }
        attack = attacker_attack[owner];

        if (pf_m4_break_player_grab_links(
                world,
                scratch,
                target_index) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }

        scratch->attack_hit_mask[owner] |=
            (uint8_t)(UINT32_C(1) << target_index);
        if (target_blocked[target_index] != UINT8_C(0))
        {
            const int powershield =
                target_powershield[target_index] != UINT8_C(0);
            const uint32_t shield_damage =
                pf_m4_shield_damage_q16(
                    &content->fighter,
                    attack.damage_q16);
            const int32_t defender_pushback =
                pf_m4_shield_defender_pushback_q16(
                    &content->fighter,
                    attack.damage_q16,
                    powershield,
                    scratch->shield_strength[target_index]);

            if (!powershield)
            {
                if (shield_damage >=
                    scratch->shield_health_q16[target_index])
                {
                    scratch->shield_health_q16[target_index] =
                        UINT32_C(0);
                }
                else
                {
                    scratch->shield_health_q16[target_index] -=
                        shield_damage;
                }
            }
            scratch->velocity_x_q16[target_index] =
                (int32_t)scratch->facing[owner] *
                (int32_t)attack.direction *
                defender_pushback;
            scratch->velocity_y_q16[target_index] =
                INT32_C(0);
            scratch->powershield[target_index] =
                powershield ? UINT8_C(1) : UINT8_C(0);
            scratch->hitlag_ticks[target_index] =
                attack.hitlag_ticks;
            scratch->action_state[target_index] =
                (uint8_t)PF_M4_ACTION_HITLAG;
            scratch->dash_direction[target_index] = INT8_C(0);
            scratch->short_hop_latched[target_index] =
                UINT8_C(0);
            scratch->fast_fall[target_index] = UINT8_C(0);
            if (scratch->shield_health_q16[target_index] ==
                UINT32_C(0))
            {
                scratch->shield_stun_ticks[target_index] =
                    UINT16_C(0);
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
                scratch->action_ticks[target_index] =
                    UINT16_C(0);
                scratch->shield_strength[target_index] = UINT16_C(0);
                scratch->shield_tilt_x[target_index] = INT16_C(0);
                scratch->shield_tilt_y[target_index] = INT16_C(0);
            }
            else
            {
                scratch->shield_stun_ticks[target_index] =
                    pf_m4_shield_stun_ticks(
                        &content->fighter,
                        attack.damage_q16);
                scratch->hitlag_resume_action[target_index] =
                    (uint8_t)PF_M4_ACTION_SHIELD_STUN;
            }
            if (pf_sim_push_event(
                    scratch,
                    world->tick,
                    powershield
                        ? PF_SIM_EVENT_POWERSHIELD
                        : (scratch->shield_health_q16[target_index] ==
                                   UINT32_C(0)
                               ? PF_SIM_EVENT_SHIELD_BREAK
                               : PF_SIM_EVENT_SHIELD_BLOCK),
                    owner,
                    (uint8_t)target_index,
                    powershield ? UINT32_C(0) : shield_damage,
                    scratch->velocity_x_q16[target_index],
                    scratch->velocity_y_q16[target_index],
                    UINT16_C(0),
                    (uint16_t)attacker_action[owner],
                    NULL) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            continue;
        }

        {
            const uint32_t resulting_damage =
                pf_m4_saturating_damage(
                    scratch->damage_q16[target_index],
                    attack.damage_q16);

            knockback_x = pf_m4_scaled_knockback(
                attack.base_knockback_x_q16,
                attack.knockback_growth_q16,
                resulting_damage,
                0);
            knockback_y = pf_m4_scaled_knockback(
                attack.base_knockback_y_q16,
                attack.knockback_growth_q16,
                resulting_damage,
                1);
            if (pf_m4_apply_hit_reaction(
                    content,
                    world,
                    scratch,
                    owner,
                    target_index,
                    attack.damage_q16,
                    (int32_t)scratch->facing[owner] *
                        (int32_t)attack.direction * knockback_x,
                    (int32_t)attack.vertical_direction * knockback_y,
                    attack.hitlag_ticks,
                    PF_SIM_EVENT_HIT,
                    (uint16_t)attacker_action[owner]) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
    }

    for (attacker_index = UINT32_C(0);
         attacker_index < (uint32_t)world->player_count;
         ++attacker_index)
    {
        pf_m4_attack_runtime attack;

        if (attacker_hit[attacker_index] != UINT8_C(0) &&
            target_owner[attacker_index] == UINT8_MAX)
        {
            attack = attacker_attack[attacker_index];
            if (attacker_blocked[attacker_index] != UINT8_C(0))
            {
                scratch->velocity_x_q16[attacker_index] =
                    -(int32_t)scratch->facing[attacker_index] *
                    (int32_t)attack.direction *
                    pf_m4_shield_attacker_pushback_q16(
                        &content->fighter,
                        attack.damage_q16);
            }
            scratch->hitlag_ticks[attacker_index] =
                attack.hitlag_ticks;
            scratch->hitlag_resume_action[attacker_index] =
                attack.action_state;
            scratch->action_state[attacker_index] =
                (uint8_t)PF_M4_ACTION_HITLAG;
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
