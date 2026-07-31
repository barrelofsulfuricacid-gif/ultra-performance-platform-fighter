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

static int pf_m4_action_is_v_cancel_eligible(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
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
    uint8_t action_state;
} pf_m4_attack_runtime;

static int pf_m4_attack_for_action(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    pf_m4_attack_runtime *out_attack)
{
    if (fighter == NULL || out_attack == NULL)
    {
        return 0;
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
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
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
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
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
    int powershield)
{
    int64_t pushback =
        ((int64_t)attack_damage_q16 *
         (int64_t)fighter->shield_defender_pushback_damage_q16) >>
        16U;

    pushback +=
        (int64_t)fighter->shield_defender_pushback_base_q16;
    if (powershield == 0)
    {
        pushback =
            pushback *
            (int64_t)fighter->shield_defender_pushback_scale_q16 /
            (int64_t)PF_Q16_ONE;
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
    const pf_m4_fighter_data *fighter;
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

    fighter = &content->fighter;
    if (!pf_m4_attack_for_action(
            fighter,
            action_state,
            action_ticks,
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
    int v_cancelled;
    int reset;
    uint32_t hit_sequence;
    uint16_t event_flags;
    uint16_t hitstun_ticks;

    if (scratch->action_state[target_index] ==
        (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        scratch->shield_health_q16[target_index] =
            content->fighter.shield_reset_health_q16;
    }
    scratch->damage_q16[target_index] = pf_m4_saturating_damage(
        scratch->damage_q16[target_index],
        damage_q16);
    v_cancelled = pf_m4_player_v_cancelled(
        &content->fighter,
        scratch,
        target_index);
    hitstun_ticks = pf_m4_hitstun_ticks(
        &content->fighter,
        launch_velocity_x_q16,
        launch_velocity_y_q16);
    reset = event_type == PF_SIM_EVENT_HIT &&
            (previous_action == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
             previous_action == (uint8_t)PF_M4_ACTION_RESET_BOUND) &&
            damage_q16 <= content->fighter.reset_max_damage_q16 &&
            hitstun_ticks <=
                content->fighter.reset_max_hitstun_ticks;
    scratch->pending_velocity_x_q16[target_index] =
        reset != 0 ? INT32_C(0) : launch_velocity_x_q16;
    scratch->pending_velocity_y_q16[target_index] =
        reset != 0
            ? -content->fighter.reset_bound_speed_q16
            : launch_velocity_y_q16;
    scratch->hitstun_ticks[target_index] = hitstun_ticks;
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
        reset == 0 &&
                scratch->hitstun_ticks[target_index] >=
                    content->fighter.tumble_hitstun_threshold_ticks
            ? UINT8_C(1)
            : UINT8_C(0);
    scratch->shield_stun_ticks[target_index] = UINT16_C(0);
    scratch->powershield[target_index] = UINT8_C(0);
    scratch->hitlag_ticks[target_index] = hitlag_ticks;
    scratch->hitlag_resume_action[target_index] =
        reset != 0
            ? (uint8_t)PF_M4_ACTION_RESET_BOUND
            : (uint8_t)PF_M4_ACTION_HITSTUN;
    scratch->action_state[target_index] =
        (uint8_t)PF_M4_ACTION_HITLAG;
    scratch->action_ticks[target_index] = UINT16_C(0);
    scratch->dash_direction[target_index] = INT8_C(0);
    scratch->short_hop_latched[target_index] = UINT8_C(0);
    scratch->fast_fall[target_index] = UINT8_C(0);
    scratch->attack_hit_mask[target_index] = UINT8_C(0);
    scratch->sdi_pulse_count[target_index] = UINT8_C(0);
    scratch->sdi_direction_x[target_index] = INT8_C(0);
    scratch->sdi_direction_y[target_index] = INT8_C(0);
    scratch->tech_direction[target_index] = INT8_C(0);

    event_flags =
        scratch->tumble[target_index] != UINT8_C(0)
            ? (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE
            : UINT16_C(0);
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

        for (target_index = UINT32_C(0);
             target_index < (uint32_t)world->player_count;
             ++target_index)
        {
            const uint8_t target_bit =
                (uint8_t)(UINT32_C(1) << target_index);

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
                !pf_m4_hitbox_overlaps_player(
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
                    scratch->action_state[target_index]))
            {
                target_blocked[target_index] = UINT8_C(1);
                attacker_blocked[attacker_index] = UINT8_C(1);
                if (scratch->action_state[target_index] ==
                        (uint8_t)PF_M4_ACTION_SHIELD &&
                    scratch->action_ticks[target_index] <=
                        content->fighter
                            .powershield_window_ticks)
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
        if (!pf_m4_attack_for_action(
                &content->fighter,
                attacker_action[owner],
                scratch->action_ticks[owner],
                &attack))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }

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
                    powershield);

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
                    -knockback_y,
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
            if (!pf_m4_attack_for_action(
                    &content->fighter,
                    attacker_action[attacker_index],
                    scratch->action_ticks[attacker_index],
                    &attack))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
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

    return PF_STATUS_OK;
}
