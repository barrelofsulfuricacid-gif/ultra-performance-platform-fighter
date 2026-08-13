#ifndef PF_SIM_MELEE_H
#define PF_SIM_MELEE_H

#include "pf/m4.h"
#include "sim_ssbm_common_data.h"

#include <math.h>
#include <stdint.h>

#define PF_MELEE_PI_F 3.14159265358979323846f

typedef struct melee_knockback_result
{
    float velocity_x_f32;
    float velocity_y_f32;
    float knockback_f32;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint8_t grounded_launch;
    uint8_t damage_level;
    uint8_t meteor_cancellable;
    uint8_t reserved;
} melee_knockback_result;

static inline uint16_t melee_hitlag_ticks(
    float damage_f32,
    uint8_t target_crouching,
    float hitlag_multiplier_f32)
{
    const ssbm_damage_response_attributes *common =
        ssbm_common_reference_damage_response();
    const uint32_t damage_count =
        damage_f32 > 0.0f ? (uint32_t)damage_f32 : UINT32_C(0);
    float hitlag_f32;
    uint32_t hitlag;

    if (common == NULL)
    {
        hitlag = damage_count / UINT32_C(3) + UINT32_C(3);
        return (uint16_t)(hitlag < UINT32_C(20) ? hitlag : UINT32_C(20));
    }
    hitlag_f32 =
        (float)damage_count *
            ((float)common->hitlag_damage_scale_q30 / 1073741824.0f) +
        (float)common->hitlag_base_ticks;
    hitlag_f32 *= hitlag_multiplier_f32;
    if (target_crouching != UINT8_C(0))
    {
        hitlag_f32 *= common->crouch_hitlag_scale_f32;
    }
    hitlag = hitlag_f32 > 0.0f ? (uint32_t)hitlag_f32 : UINT32_C(0);
    if (hitlag > common->maximum_hitlag_ticks)
    {
        hitlag = common->maximum_hitlag_ticks;
    }
    return (uint16_t)hitlag;
}

static inline melee_knockback_result melee_knockback_for_state(
    const melee_knockback_data *hit,
    uint16_t target_weight,
    float knockback_damage_f32,
    float hitlag_damage_f32,
    float knockback_percent_f32,
    uint8_t target_grounded,
    uint8_t target_crouching,
    uint8_t target_smash_charging,
    float target_hitlag_multiplier_f32)
{
    const uint32_t damage_count =
        knockback_damage_f32 > 0.0f
            ? (uint32_t)knockback_damage_f32
            : UINT32_C(0);
    const float weight_factor = 200.0f / (100.0f + (float)target_weight);
    const float inner =
        hit->weight_set != UINT16_C(0)
            ? 1.0f + (float)hit->weight_set * 0.5f
            : knockback_percent_f32 * (2.0f + (float)damage_count) / 20.0f;
    const float base_term = 18.0f + weight_factor * inner * 1.4f;
    const ssbm_damage_response_attributes *common =
        ssbm_common_reference_damage_response();
    float knockback =
        (float)hit->base + (float)hit->growth * base_term / 100.0f;
    float launch_angle_degrees = (float)hit->angle_degrees;
    float scaled_knockback;
    float radians;
    melee_knockback_result result;
    uint32_t hitstun;

    if (knockback > 2500.0f)
    {
        knockback = 2500.0f;
    }
    if (common != NULL && target_crouching != UINT8_C(0))
    {
        knockback *= common->crouch_knockback_scale_f32;
    }
    if (common != NULL && target_smash_charging != UINT8_C(0))
    {
        knockback *= common->smash_charge_knockback_scale_f32;
    }
    if (hit->angle_degrees == UINT16_C(361) && common != NULL)
    {
        if (target_grounded == UINT8_C(0))
        {
            launch_angle_degrees = common->sakurai_air_angle_degrees_f32;
        }
        else if (knockback < common->sakurai_low_knockback_f32)
        {
            launch_angle_degrees = 0.0f;
        }
        else
        {
            const float range = common->sakurai_high_knockback_f32 -
                                common->sakurai_low_knockback_f32;
            float angle =
                range > 0.0f
                    ? 1.0f +
                          (knockback - common->sakurai_low_knockback_f32) *
                              common->sakurai_max_ground_angle_degrees_f32 /
                              range
                    : common->sakurai_max_ground_angle_degrees_f32;
            if (angle > common->sakurai_max_ground_angle_degrees_f32)
            {
                angle = common->sakurai_max_ground_angle_degrees_f32;
            }
            launch_angle_degrees = angle;
        }
    }
    radians = launch_angle_degrees * (PF_MELEE_PI_F / 180.0f);
    result.velocity_x_f32 =
        knockback * 0.03f * cosf(radians) * (12.0f / 115.0f);
    result.velocity_y_f32 =
        knockback * 0.03f * sinf(radians) * (11.0f / 62.0f);
    result.knockback_f32 = knockback;
    scaled_knockback = common != NULL
                           ? knockback * common->hitstun_per_knockback_f32
                           : knockback * 0.4f;
    result.grounded_launch =
        target_grounded != UINT8_C(0) && result.velocity_y_f32 <= 0.0f &&
                common != NULL &&
                scaled_knockback < common->grounded_damage_max_level_f32
            ? UINT8_C(1)
            : UINT8_C(0);
    result.damage_level =
        common == NULL || scaled_knockback < common->damage_level_1_threshold_f32
            ? UINT8_C(0)
            : scaled_knockback < common->damage_level_2_threshold_f32
                  ? UINT8_C(1)
                  : scaled_knockback < common->grounded_damage_max_level_f32
                        ? UINT8_C(2)
                        : UINT8_C(3);
    result.meteor_cancellable =
        common != NULL && hit->angle_degrees != UINT16_C(361) &&
                hit->angle_degrees >= common->meteor_angle_min_degrees &&
                hit->angle_degrees <= common->meteor_angle_max_degrees
            ? UINT8_C(1)
            : UINT8_C(0);
    result.reserved = UINT8_C(0);
    if (result.grounded_launch != UINT8_C(0))
    {
        result.velocity_y_f32 = 0.0f;
    }
    hitstun = knockback > 0.0f ? (uint32_t)(knockback * 0.4f) : UINT32_C(0);
    if (hitstun > UINT32_C(65535))
    {
        hitstun = UINT32_C(65535);
    }
    result.hitlag_ticks = melee_hitlag_ticks(
        hitlag_damage_f32,
        target_crouching,
        target_hitlag_multiplier_f32);
    result.hitstun_ticks = (uint16_t)hitstun;
    return result;
}

static inline melee_knockback_result melee_knockback(
    const melee_knockback_data *hit,
    uint16_t target_weight,
    float damage_f32,
    float knockback_percent_f32)
{
    return melee_knockback_for_state(
        hit,
        target_weight,
        damage_f32,
        damage_f32,
        knockback_percent_f32,
        UINT8_C(0),
        UINT8_C(0),
        UINT8_C(0),
        1.0f);
}

#endif
