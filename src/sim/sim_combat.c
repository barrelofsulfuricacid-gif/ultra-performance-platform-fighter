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

static int pf_m4_action_is_guarding(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN;
}

static int pf_m4_action_is_tech_invulnerable(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    return (action_state ==
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
            action_state ==
                (uint8_t)PF_M4_ACTION_TECH_ROLL) &&
           action_ticks < fighter->tech_invulnerability_ticks;
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
    uint16_t startup_ticks;
    uint16_t active_ticks;
    uint16_t recovery_ticks;
    uint16_t hitlag_ticks;
    uint8_t action_state;
} pf_m4_attack_runtime;

static int pf_m4_attack_for_action(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    pf_m4_attack_runtime *out_attack)
{
    if (fighter == NULL || out_attack == NULL)
    {
        return 0;
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
        out_attack->startup_ticks = fighter->jab_startup_ticks;
        out_attack->active_ticks = fighter->jab_active_ticks;
        out_attack->recovery_ticks = fighter->jab_recovery_ticks;
        out_attack->hitlag_ticks = fighter->jab_hitlag_ticks;
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_STRONG_ATTACK)
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
        out_attack->startup_ticks = fighter->strong_startup_ticks;
        out_attack->active_ticks = fighter->strong_active_ticks;
        out_attack->recovery_ticks =
            fighter->strong_recovery_ticks;
        out_attack->hitlag_ticks = fighter->strong_hitlag_ticks;
        out_attack->action_state =
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK;
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
    uint32_t active_begin;
    uint32_t active_end;
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
            &attack))
    {
        return 0;
    }
    active_begin =
        (uint32_t)attack.startup_ticks + UINT32_C(1);
    active_end =
        (uint32_t)attack.startup_ticks +
        (uint32_t)attack.active_ticks;
    if ((uint32_t)action_ticks < active_begin ||
        (uint32_t)action_ticks > active_end)
    {
        return 0;
    }

    center_x =
        (int64_t)position_x_q16 +
        (int64_t)facing *
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

        if (world->active[attacker_index] == UINT8_C(0) ||
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
                world->active[target_index] == UINT8_C(0) ||
                target_owner[target_index] != UINT8_MAX ||
                scratch->action_state[target_index] ==
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
                pf_m4_action_is_tech_invulnerable(
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
                &attack))
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
            if (scratch->combat_event_sequence != UINT32_MAX)
            {
                ++scratch->combat_event_sequence;
            }
            continue;
        }

        scratch->damage_q16[target_index] = pf_m4_saturating_damage(
            scratch->damage_q16[target_index],
            attack.damage_q16);
        knockback_x = pf_m4_scaled_knockback(
            attack.base_knockback_x_q16,
            attack.knockback_growth_q16,
            scratch->damage_q16[target_index],
            0);
        knockback_y = pf_m4_scaled_knockback(
            attack.base_knockback_y_q16,
            attack.knockback_growth_q16,
            scratch->damage_q16[target_index],
            1);
        scratch->pending_velocity_x_q16[target_index] =
            (int32_t)scratch->facing[owner] * knockback_x;
        scratch->pending_velocity_y_q16[target_index] =
            -knockback_y;
        scratch->hitstun_ticks[target_index] =
            pf_m4_hitstun_ticks(
                &content->fighter,
                scratch->pending_velocity_x_q16[target_index],
                scratch->pending_velocity_y_q16[target_index]);
        scratch->tumble[target_index] =
            scratch->hitstun_ticks[target_index] >=
                    content->fighter.tumble_hitstun_threshold_ticks
                ? UINT8_C(1)
                : UINT8_C(0);
        scratch->shield_stun_ticks[target_index] = UINT16_C(0);
        scratch->powershield[target_index] = UINT8_C(0);
        scratch->hitlag_ticks[target_index] =
            attack.hitlag_ticks;
        scratch->hitlag_resume_action[target_index] =
            (uint8_t)PF_M4_ACTION_HITSTUN;
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

        if (scratch->combat_event_sequence != UINT32_MAX)
        {
            ++scratch->combat_event_sequence;
        }
        scratch->last_hit_sequence[target_index] =
            scratch->combat_event_sequence;
        scratch->last_hit_tick[target_index] = world->tick;
        scratch->last_hit_damage_q16[target_index] =
            attack.damage_q16;
        scratch->last_hit_attacker[target_index] = owner;
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
                    &attack))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (attacker_blocked[attacker_index] != UINT8_C(0))
            {
                scratch->velocity_x_q16[attacker_index] =
                    -(int32_t)scratch->facing[attacker_index] *
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
