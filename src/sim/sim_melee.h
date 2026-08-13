#ifndef PF_SIM_MELEE_H
#define PF_SIM_MELEE_H

#include "pf/m4.h"
#include "sim_ssbm_common_data.h"

#include <limits.h>
#include <stdint.h>

typedef struct pf_m4_melee_knockback_result
{
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    int32_t knockback_q16;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint8_t grounded_launch;
    uint8_t damage_level;
    uint8_t meteor_cancellable;
    uint8_t reserved;
} pf_m4_melee_knockback_result;

/* Q2.30 sin(0..90 degrees), indexed by the integer hitbox angle. */
static const int32_t pf_m4_melee_sin_q30[91] = {
    INT32_C(0), INT32_C(18739379), INT32_C(37473049), INT32_C(56195305),
    INT32_C(74900443), INT32_C(93582766), INT32_C(112236583), INT32_C(130856211),
    INT32_C(149435979), INT32_C(167970228), INT32_C(186453311), INT32_C(204879599),
    INT32_C(223243478), INT32_C(241539355), INT32_C(259761657), INT32_C(277904834),
    INT32_C(295963357), INT32_C(313931728), INT32_C(331804471), INT32_C(349576144),
    INT32_C(367241333), INT32_C(384794656), INT32_C(402230767), INT32_C(419544355),
    INT32_C(436730145), INT32_C(453782903), INT32_C(470697435), INT32_C(487468587),
    INT32_C(504091252), INT32_C(520560366), INT32_C(536870912), INT32_C(553017922),
    INT32_C(568996477), INT32_C(584801711), INT32_C(600428808), INT32_C(615873009),
    INT32_C(631129609), INT32_C(646193961), INT32_C(661061475), INT32_C(675727625),
    INT32_C(690187940), INT32_C(704438018), INT32_C(718473518), INT32_C(732290163),
    INT32_C(745883746), INT32_C(759250125), INT32_C(772385229), INT32_C(785285058),
    INT32_C(797945680), INT32_C(810363241), INT32_C(822533958), INT32_C(834454122),
    INT32_C(846120104), INT32_C(857528349), INT32_C(868675383), INT32_C(879557810),
    INT32_C(890172315), INT32_C(900515665), INT32_C(910584710), INT32_C(920376381),
    INT32_C(929887697), INT32_C(939115760), INT32_C(948057759), INT32_C(956710970),
    INT32_C(965072759), INT32_C(973140576), INT32_C(980911966), INT32_C(988384560),
    INT32_C(995556083), INT32_C(1002424350), INT32_C(1008987269), INT32_C(1015242840),
    INT32_C(1021189159), INT32_C(1026824413), INT32_C(1032146887), INT32_C(1037154959),
    INT32_C(1041847103), INT32_C(1046221891), INT32_C(1050277989), INT32_C(1054014162),
    INT32_C(1057429273), INT32_C(1060522280), INT32_C(1063292242), INT32_C(1065738315),
    INT32_C(1067859754), INT32_C(1069655912), INT32_C(1071126243), INT32_C(1072270298),
    INT32_C(1073087729), INT32_C(1073578288), INT32_C(1073741824)
};

static inline int32_t pf_m4_melee_sin_degrees_q30(uint16_t angle)
{
    const uint16_t normalized = (uint16_t)(angle % UINT16_C(360));
    const uint16_t quadrant = (uint16_t)(normalized / UINT16_C(90));
    const uint16_t offset = (uint16_t)(normalized % UINT16_C(90));

    switch (quadrant)
    {
        case UINT16_C(0):
            return pf_m4_melee_sin_q30[offset];
        case UINT16_C(1):
            return pf_m4_melee_sin_q30[UINT16_C(90) - offset];
        case UINT16_C(2):
            return -pf_m4_melee_sin_q30[offset];
        default:
            return -pf_m4_melee_sin_q30[UINT16_C(90) - offset];
    }
}

static inline int32_t pf_m4_melee_sin_degrees_q16_q30(
    int32_t angle_degrees_q16)
{
    const int32_t full_turn_q16 = INT32_C(360) * INT32_C(65536);
    int32_t normalized_q16 = angle_degrees_q16 % full_turn_q16;
    uint16_t whole_degrees;
    uint16_t next_degrees;
    uint32_t fraction_q16;
    int32_t lower_q30;
    int32_t upper_q30;

    if (normalized_q16 < INT32_C(0))
    {
        normalized_q16 += full_turn_q16;
    }
    whole_degrees = (uint16_t)(normalized_q16 >> 16U);
    next_degrees =
        whole_degrees == UINT16_C(359)
            ? UINT16_C(0)
            : (uint16_t)(whole_degrees + UINT16_C(1));
    fraction_q16 = (uint32_t)normalized_q16 & UINT32_C(0xFFFF);
    lower_q30 = pf_m4_melee_sin_degrees_q30(whole_degrees);
    upper_q30 = pf_m4_melee_sin_degrees_q30(next_degrees);
    return (int32_t)(
        (int64_t)lower_q30 +
        (((int64_t)upper_q30 - (int64_t)lower_q30) *
         (int64_t)fraction_q16 >>
         16U));
}

static inline uint16_t pf_m4_melee_hitlag_ticks(
    uint32_t damage_q16,
    uint8_t target_crouching,
    uint32_t hitlag_multiplier_q16)
{
    const pf_m4_ssbm_damage_response_attributes *common =
        pf_m4_ssbm_common_reference_damage_response();
    const uint32_t damage_count = damage_q16 >> 16U;
    uint32_t hitlag;

    if (common == NULL)
    {
        hitlag = damage_count / UINT32_C(3) + UINT32_C(3);
        return (uint16_t)(
            hitlag < UINT32_C(20) ? hitlag : UINT32_C(20));
    }
    hitlag = (uint32_t)(
        (((uint64_t)damage_count *
           (uint64_t)(uint32_t)common->hitlag_damage_scale_q30) +
          ((uint64_t)common->hitlag_base_ticks << 30U)) >>
         30U);
    hitlag = (uint32_t)(
        ((uint64_t)hitlag * hitlag_multiplier_q16) >> 16U);
    if (target_crouching != UINT8_C(0))
    {
        hitlag = (uint32_t)(
            ((uint64_t)hitlag * common->crouch_hitlag_scale_q16) >> 16U);
    }
    if (hitlag > common->maximum_hitlag_ticks)
    {
        hitlag = common->maximum_hitlag_ticks;
    }
    return (uint16_t)hitlag;
}

static inline pf_m4_melee_knockback_result pf_m4_melee_knockback_for_state(
    const pf_m4_melee_knockback_data *hit,
    uint16_t target_weight,
    uint32_t knockback_damage_q16,
    uint32_t hitlag_damage_q16,
    uint32_t knockback_percent_q16,
    uint8_t target_grounded,
    uint8_t target_crouching,
    uint8_t target_smash_charging,
    uint32_t target_hitlag_multiplier_q16)
{
    const int64_t one_q16 = INT64_C(65536);
    const uint32_t damage_count = knockback_damage_q16 >> 16U;
    const int64_t weight_factor_q16 =
        (INT64_C(200) * one_q16) /
        (INT64_C(100) + (int64_t)target_weight);
    const int64_t inner_q16 =
        hit->weight_set != UINT16_C(0)
            ? one_q16 +
                  ((int64_t)hit->weight_set * one_q16) / INT64_C(2)
            : ((int64_t)knockback_percent_q16 *
               (INT64_C(2) + (int64_t)damage_count)) /
                  INT64_C(20);
    const int64_t weighted_inner_q16 =
        (((weight_factor_q16 * inner_q16) >> 16U) * INT64_C(7)) /
        INT64_C(5);
    const int64_t base_term_q16 =
        INT64_C(18) * one_q16 + weighted_inner_q16;
    int64_t knockback_q16 =
        (int64_t)hit->base * one_q16 +
        ((int64_t)hit->growth * base_term_q16) / INT64_C(100);
    int64_t launch_q16;
    const pf_m4_ssbm_damage_response_attributes *common =
        pf_m4_ssbm_common_reference_damage_response();
    int32_t launch_angle_degrees_q16 =
        (int32_t)hit->angle_degrees * INT32_C(65536);
    int32_t scaled_knockback_q16;
    int32_t sin_q30;
    int32_t cos_q30;
    pf_m4_melee_knockback_result result;
    uint32_t hitstun;

    if (knockback_q16 > INT64_C(2500) * one_q16)
    {
        knockback_q16 = INT64_C(2500) * one_q16;
    }
    if (common != NULL && target_crouching != UINT8_C(0))
    {
        knockback_q16 =
            (knockback_q16 * common->crouch_knockback_scale_q16) >> 16U;
    }
    if (common != NULL && target_smash_charging != UINT8_C(0))
    {
        knockback_q16 =
            (knockback_q16 * common->smash_charge_knockback_scale_q16) >>
            16U;
    }
    if (hit->angle_degrees == UINT16_C(361) && common != NULL)
    {
        if (target_grounded == UINT8_C(0))
        {
            launch_angle_degrees_q16 =
                common->sakurai_air_angle_degrees_q16;
        }
        else if (knockback_q16 < common->sakurai_low_knockback_q16)
        {
            launch_angle_degrees_q16 = INT32_C(0);
        }
        else
        {
            const int64_t range_q16 =
                (int64_t)common->sakurai_high_knockback_q16 -
                (int64_t)common->sakurai_low_knockback_q16;
            int64_t angle_q16 =
                range_q16 > INT64_C(0)
                    ? INT64_C(65536) +
                          ((knockback_q16 -
                            (int64_t)common->sakurai_low_knockback_q16) *
                           common->sakurai_max_ground_angle_degrees_q16) /
                              range_q16
                    : common->sakurai_max_ground_angle_degrees_q16;
            if (angle_q16 >
                common->sakurai_max_ground_angle_degrees_q16)
            {
                angle_q16 =
                    common->sakurai_max_ground_angle_degrees_q16;
            }
            launch_angle_degrees_q16 = (int32_t)angle_q16;
        }
    }
    sin_q30 =
        pf_m4_melee_sin_degrees_q16_q30(launch_angle_degrees_q16);
    cos_q30 = pf_m4_melee_sin_degrees_q16_q30(
        launch_angle_degrees_q16 + INT32_C(90) * INT32_C(65536));
    launch_q16 = (knockback_q16 * INT64_C(3)) / INT64_C(100);
    result.velocity_x_q16 = (int32_t)(
        ((((launch_q16 * (int64_t)cos_q30) >> 30U) * INT64_C(12)) /
         INT64_C(115)));
    result.velocity_y_q16 = (int32_t)(
        ((((launch_q16 * (int64_t)sin_q30) >> 30U) * INT64_C(11)) /
         INT64_C(62)));
    result.knockback_q16 = (int32_t)knockback_q16;
    scaled_knockback_q16 =
        common != NULL
            ? (int32_t)((knockback_q16 *
                         common->hitstun_per_knockback_q16) >>
                        16U)
            : (int32_t)((knockback_q16 * INT64_C(2)) / INT64_C(5));
    result.grounded_launch =
        target_grounded != UINT8_C(0) &&
                result.velocity_y_q16 <= INT32_C(0) && common != NULL &&
                scaled_knockback_q16 < common->grounded_damage_max_level_q16
            ? UINT8_C(1)
            : UINT8_C(0);
    result.damage_level =
        common == NULL || scaled_knockback_q16 <
                              common->damage_level_1_threshold_q16
            ? UINT8_C(0)
            : scaled_knockback_q16 <
                      common->damage_level_2_threshold_q16
            ? UINT8_C(1)
            : scaled_knockback_q16 <
                      common->grounded_damage_max_level_q16
            ? UINT8_C(2)
            : UINT8_C(3);
    result.meteor_cancellable =
        common != NULL &&
                hit->angle_degrees != UINT16_C(361) &&
                hit->angle_degrees >= common->meteor_angle_min_degrees &&
                hit->angle_degrees <= common->meteor_angle_max_degrees
            ? UINT8_C(1)
            : UINT8_C(0);
    result.reserved = UINT8_C(0);
    if (result.grounded_launch != UINT8_C(0))
    {
        result.velocity_y_q16 = INT32_C(0);
    }
    hitstun = (uint32_t)(
        ((uint64_t)knockback_q16 * UINT64_C(2) / UINT64_C(5)) >>
        16U);
    if (hitstun > UINT32_C(65535))
    {
        hitstun = UINT32_C(65535);
    }
    result.hitlag_ticks = pf_m4_melee_hitlag_ticks(
        hitlag_damage_q16,
        target_crouching,
        target_hitlag_multiplier_q16);
    result.hitstun_ticks = (uint16_t)hitstun;
    return result;
}

static inline pf_m4_melee_knockback_result pf_m4_melee_knockback(
    const pf_m4_melee_knockback_data *hit,
    uint16_t target_weight,
    uint32_t damage_q16,
    uint32_t knockback_percent_q16)
{
    return pf_m4_melee_knockback_for_state(
        hit,
        target_weight,
        damage_q16,
        damage_q16,
        knockback_percent_q16,
        UINT8_C(0),
        UINT8_C(0),
        UINT8_C(0),
        UINT32_C(65536));
}

#endif
