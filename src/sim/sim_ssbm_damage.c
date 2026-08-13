#include "sim_ssbm_damage.h"

#include "sim_internal.h"
#include "sim_ssbm_common_data.h"

#include <limits.h>
#include <stdint.h>

#define PF_M4_Q30_ONE INT64_C(1073741824)
#define PF_M4_SSBM_VECTOR_EXTRA_SCALE INT64_C(256)

static int64_t pf_m4_ssbm_abs_i64(int64_t value)
{
    return value < INT64_C(0) ? -value : value;
}

static int64_t pf_m4_ssbm_div_round_nearest_i64(
    int64_t numerator,
    int64_t denominator)
{
    return numerator < INT64_C(0)
               ? -((-numerator + denominator / INT64_C(2)) / denominator)
               : (numerator + denominator / INT64_C(2)) / denominator;
}

pf_m4_ssbm_damage_floor_response
pf_m4_ssbm_select_damage_floor_response_q16(
    int32_t knockback_velocity_x_q16,
    int32_t knockback_velocity_y_q16,
    uint8_t force_down_bound)
{
    const pf_m4_ssbm_damage_response_attributes *common =
        pf_m4_ssbm_common_reference_damage_response();
    const int64_t source_x =
        (int64_t)knockback_velocity_x_q16 * INT64_C(115) /
        INT64_C(12);
    const int64_t source_y =
        -(int64_t)knockback_velocity_y_q16 * INT64_C(62) /
        INT64_C(11);
    uint64_t magnitude_squared;
    uint64_t threshold_squared;

    if (force_down_bound != UINT8_C(0) ||
        pf_m4_ssbm_abs_i64(source_x) >=
            common->damage_floor_down_speed_q16 ||
        pf_m4_ssbm_abs_i64(source_y) >=
            common->damage_floor_down_speed_q16)
    {
        return PF_M4_SSBM_DAMAGE_FLOOR_DOWN_BOUND;
    }

    /* ftCo_Damage_Coll compares the isotropic Melee x8c magnitude. Both
     * components are below x1E0 here, so the squared comparison is exact and
     * cannot overflow; no runtime square root or floating point is needed. */
    magnitude_squared =
        (uint64_t)(source_x * source_x) +
        (uint64_t)(source_y * source_y);
    threshold_squared =
        (uint64_t)common->damage_floor_down_speed_q16 *
        (uint64_t)common->damage_floor_down_speed_q16;
    if (magnitude_squared >= threshold_squared)
    {
        return PF_M4_SSBM_DAMAGE_FLOOR_DOWN_BOUND;
    }
    threshold_squared =
        (uint64_t)common->damage_floor_landing_speed_q16 *
        (uint64_t)common->damage_floor_landing_speed_q16;
    return magnitude_squared >= threshold_squared
               ? PF_M4_SSBM_DAMAGE_FLOOR_LANDING
               : PF_M4_SSBM_DAMAGE_FLOOR_KEEP_ACTION;
}

pf_status pf_m4_ssbm_resolve_ground_damage_launch_q16(
    int32_t source_normal_x_q16,
    int32_t source_normal_y_q16,
    int32_t ground_projection_x_q16,
    int32_t ground_projection_y_q16,
    uint8_t damage_level,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16,
    int32_t *ground_scalar_q16,
    uint8_t *launch_grounded)
{
    const pf_m4_ssbm_damage_response_attributes *common =
        pf_m4_ssbm_common_reference_damage_response();
    int64_t source_x;
    int64_t source_y_math;
    int64_t maximum_component;
    int64_t dot;
    uint64_t magnitude_squared;
    uint32_t magnitude;
    uint32_t normal_magnitude;

    if (velocity_x_q16 == NULL || velocity_y_q16 == NULL ||
        ground_scalar_q16 == NULL || launch_grounded == NULL ||
        common == NULL || damage_level > UINT8_C(3) ||
        (source_normal_x_q16 == INT32_C(0) &&
         source_normal_y_q16 == INT32_C(0)) ||
        pf_m4_ssbm_abs_i64(source_normal_x_q16) > PF_Q16_ONE ||
        pf_m4_ssbm_abs_i64(source_normal_y_q16) > PF_Q16_ONE)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    /* ftCo_8008DCE0 compares the launch vector with the current floor in
     * Melee's isotropic positive-Y-up coordinates. Undo the world scale,
     * then reduce only extreme, unreachable API inputs until squaring is
     * safe; the common factor leaves every angle test unchanged. */
    source_x =
        (int64_t)*velocity_x_q16 * INT64_C(115) / INT64_C(12);
    source_y_math =
        -(int64_t)*velocity_y_q16 * INT64_C(62) / INT64_C(11);
    maximum_component = pf_m4_ssbm_abs_i64(source_x);
    if (pf_m4_ssbm_abs_i64(source_y_math) > maximum_component)
    {
        maximum_component = pf_m4_ssbm_abs_i64(source_y_math);
    }
    while (maximum_component > (INT64_C(1) << 30U))
    {
        source_x /= INT64_C(2);
        source_y_math /= INT64_C(2);
        maximum_component /= INT64_C(2);
    }
    dot =
        source_x * (int64_t)source_normal_x_q16 +
        source_y_math * (int64_t)source_normal_y_q16;

    *ground_scalar_q16 = INT32_C(0);
    *launch_grounded = UINT8_C(0);
    if (dot > INT64_C(0))
    {
        return PF_STATUS_OK;
    }
    if (damage_level != UINT8_C(3))
    {
        const int64_t projected_x =
            (int64_t)*velocity_x_q16 * ground_projection_x_q16 /
            (int64_t)PF_Q16_ONE;
        const int64_t projected_y =
            (int64_t)*velocity_x_q16 * ground_projection_y_q16 /
            (int64_t)PF_Q16_ONE;

        if (projected_x < (int64_t)INT32_MIN ||
            projected_x > (int64_t)INT32_MAX ||
            projected_y < (int64_t)INT32_MIN ||
            projected_y > (int64_t)INT32_MAX)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *ground_scalar_q16 = *velocity_x_q16;
        *velocity_x_q16 = (int32_t)projected_x;
        *velocity_y_q16 = (int32_t)projected_y;
        *launch_grounded = UINT8_C(1);
        return PF_STATUS_OK;
    }

    magnitude_squared =
        (uint64_t)(source_x * source_x) +
        (uint64_t)(source_y_math * source_y_math);
    magnitude = pf_m4_u64_sqrt(magnitude_squared);
    normal_magnitude = pf_m4_u64_sqrt(
        (uint64_t)((int64_t)source_normal_x_q16 *
                   source_normal_x_q16) +
        (uint64_t)((int64_t)source_normal_y_q16 *
                   source_normal_y_q16));
    /* The source comparison is strict: angle > 90 degrees + x1E8. For a
     * unit floor normal this is -dot > |velocity| * sin(x1E8). */
    if (-dot >
        (int64_t)magnitude * (int64_t)normal_magnitude *
            common->ground_damage_steep_angle_sine_q16 /
            (int64_t)PF_Q16_ONE)
    {
        const int64_t reflected_y =
            -(int64_t)*velocity_y_q16 *
            common->ground_damage_vertical_reflection_q16 /
            (int64_t)PF_Q16_ONE;

        if (reflected_y < (int64_t)INT32_MIN ||
            reflected_y > (int64_t)INT32_MAX)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *velocity_y_q16 = (int32_t)reflected_y;
    }
    return PF_STATUS_OK;
}

pf_status pf_m4_ssbm_select_damage_motion(
    uint8_t launch_grounded,
    uint8_t damage_level,
    uint32_t resulting_damage_q16,
    int32_t launch_velocity_x_q16,
    int32_t launch_velocity_y_q16,
    uint64_t *rng_state,
    pf_m4_ssbm_damage_motion_kind *out_motion)
{
    const pf_m4_ssbm_damage_response_attributes *common =
        pf_m4_ssbm_common_reference_damage_response();

    if (rng_state == NULL || out_motion == NULL || common == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    *out_motion = PF_M4_SSBM_DAMAGE_MOTION_ORDINARY;
    if (launch_grounded != UINT8_C(0) || damage_level != UINT8_C(3))
    {
        return PF_STATUS_OK;
    }

    if (launch_velocity_y_q16 < INT32_C(0))
    {
        const uint64_t source_horizontal =
            (uint64_t)(launch_velocity_x_q16 < INT32_C(0)
                           ? -(int64_t)launch_velocity_x_q16
                           : (int64_t)launch_velocity_x_q16) *
            UINT64_C(115) * UINT64_C(11) * UINT64_C(65536);
        const uint64_t source_vertical_limit =
            (uint64_t)(-(int64_t)launch_velocity_y_q16) *
            UINT64_C(62) * UINT64_C(12) *
            (uint64_t)(uint32_t)
                common->damage_fly_top_horizontal_ratio_q16;

        /* ftCo_8008DCE0 uses strict 70/110-degree bounds. Comparing the
         * source-space horizontal/vertical ratio avoids libm and preserves
         * the project's anisotropic world conversion. */
        if (source_horizontal < source_vertical_limit)
        {
            *out_motion = PF_M4_SSBM_DAMAGE_MOTION_FLY_TOP;
            return PF_STATUS_OK;
        }
    }
    if (resulting_damage_q16 >=
        (uint32_t)common->damage_fly_roll_damage_threshold *
            UINT32_C(65536))
    {
        const uint16_t random = pf_sim_hsd_random_u16(rng_state);

        if (random < common->damage_fly_roll_random_threshold_u16)
        {
            *out_motion = PF_M4_SSBM_DAMAGE_MOTION_FLY_ROLL;
        }
    }
    return PF_STATUS_OK;
}

static int32_t pf_m4_ssbm_clamp_stick_axis(int16_t axis)
{
    return axis == INT16_MIN ? INT32_C(-32767) : (int32_t)axis;
}

static int64_t pf_m4_ssbm_mul_q30(int64_t left, int64_t right)
{
    return (left * right) / PF_M4_Q30_ONE;
}

static void pf_m4_ssbm_sin_cos_q30(
    int64_t angle_q30,
    int64_t *out_sin_q30,
    int64_t *out_cos_q30)
{
    const int64_t angle_2 =
        pf_m4_ssbm_mul_q30(angle_q30, angle_q30);
    const int64_t angle_3 =
        pf_m4_ssbm_mul_q30(angle_2, angle_q30);
    const int64_t angle_4 =
        pf_m4_ssbm_mul_q30(angle_2, angle_2);
    const int64_t angle_5 =
        pf_m4_ssbm_mul_q30(angle_4, angle_q30);
    const int64_t angle_6 =
        pf_m4_ssbm_mul_q30(angle_4, angle_2);

    /* |angle| is source-bounded to 18 degrees for legal controller input.
     * These fixed Taylor terms stay below one Q16.16 velocity unit of the
     * corresponding libm rotation over that interval. */
    *out_sin_q30 =
        angle_q30 - angle_3 / INT64_C(6) + angle_5 / INT64_C(120);
    *out_cos_q30 =
        PF_M4_Q30_ONE - angle_2 / INT64_C(2) +
        angle_4 / INT64_C(24) - angle_6 / INT64_C(720);
}

int pf_m4_ssbm_stick_meets_radial_threshold(
    int16_t stick_x,
    int16_t stick_y,
    uint16_t threshold)
{
    const int64_t x = (int64_t)pf_m4_ssbm_clamp_stick_axis(stick_x);
    const int64_t y = (int64_t)pf_m4_ssbm_clamp_stick_axis(stick_y);
    const int64_t threshold_64 = (int64_t)threshold;

    return x * x + y * y >= threshold_64 * threshold_64;
}

int32_t pf_m4_ssbm_analog_displacement_q16(
    int16_t stick_axis,
    int32_t maximum_distance_q16)
{
    return (int32_t)(
        (int64_t)pf_m4_ssbm_clamp_stick_axis(stick_axis) *
        (int64_t)maximum_distance_q16 /
        INT64_C(32767));
}

pf_status pf_m4_ssbm_decay_air_knockback_q16(
    int32_t decay_q16,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    /* ftCommon_8007D494 subtracts a scalar from the knockback magnitude in
     * Melee coordinates. Preserve eight guard bits while undoing the
     * project's anisotropic world conversion, then convert the shortened
     * vector back only once. */
    int64_t source_x;
    int64_t source_y;
    uint64_t magnitude_squared;
    uint32_t magnitude;
    int64_t guarded_decay;
    int64_t remaining;
    int64_t decayed_source_x;
    int64_t decayed_source_y;
    int64_t decayed_x;
    int64_t decayed_y;

    if (decay_q16 < INT32_C(0) || velocity_x_q16 == NULL ||
        velocity_y_q16 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    source_x = pf_m4_ssbm_div_round_nearest_i64(
        (int64_t)*velocity_x_q16 * INT64_C(115) *
            PF_M4_SSBM_VECTOR_EXTRA_SCALE,
        INT64_C(12));
    source_y = pf_m4_ssbm_div_round_nearest_i64(
        (int64_t)*velocity_y_q16 * INT64_C(62) *
            PF_M4_SSBM_VECTOR_EXTRA_SCALE,
        INT64_C(11));
    magnitude_squared =
        (uint64_t)(source_x * source_x) +
        (uint64_t)(source_y * source_y);
    magnitude = pf_m4_u64_sqrt(magnitude_squared);
    guarded_decay =
        (int64_t)decay_q16 * PF_M4_SSBM_VECTOR_EXTRA_SCALE;
    if (magnitude == UINT32_C(0) ||
        (int64_t)magnitude <= guarded_decay)
    {
        *velocity_x_q16 = INT32_C(0);
        *velocity_y_q16 = INT32_C(0);
        return PF_STATUS_OK;
    }

    remaining = (int64_t)magnitude - guarded_decay;
    decayed_source_x = pf_m4_ssbm_div_round_nearest_i64(
        source_x * remaining,
        (int64_t)magnitude);
    decayed_source_y = pf_m4_ssbm_div_round_nearest_i64(
        source_y * remaining,
        (int64_t)magnitude);
    decayed_x = pf_m4_ssbm_div_round_nearest_i64(
        decayed_source_x * INT64_C(12),
        INT64_C(115) * PF_M4_SSBM_VECTOR_EXTRA_SCALE);
    decayed_y = pf_m4_ssbm_div_round_nearest_i64(
        decayed_source_y * INT64_C(11),
        INT64_C(62) * PF_M4_SSBM_VECTOR_EXTRA_SCALE);
    if (decayed_x < (int64_t)INT32_MIN ||
        decayed_x > (int64_t)INT32_MAX ||
        decayed_y < (int64_t)INT32_MIN ||
        decayed_y > (int64_t)INT32_MAX)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *velocity_x_q16 = (int32_t)decayed_x;
    *velocity_y_q16 = (int32_t)decayed_y;
    return PF_STATUS_OK;
}

pf_status pf_m4_ssbm_mirror_velocity_q16(
    int32_t source_normal_x_q16,
    int32_t source_normal_y_q16,
    int32_t multiplier_q16,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    int64_t source_x;
    int64_t source_y_math;
    int64_t dot;
    int64_t mirrored_source_x;
    int64_t mirrored_source_y_math;
    int64_t mirrored_x;
    int64_t mirrored_y;

    if (multiplier_q16 < INT32_C(0) || velocity_x_q16 == NULL ||
        velocity_y_q16 == NULL ||
        (source_normal_x_q16 == INT32_C(0) &&
         source_normal_y_q16 == INT32_C(0)))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    /* Axis-aligned surfaces need no coordinate conversion. Besides avoiding
     * needless divisions, this preserves the already live-qualified integer
     * result for ordinary vertical walls and horizontal ceilings. */
    if (source_normal_y_q16 == INT32_C(0))
    {
        mirrored_x =
            -(int64_t)*velocity_x_q16 * (int64_t)multiplier_q16 /
            (int64_t)PF_Q16_ONE;
        mirrored_y =
            (int64_t)*velocity_y_q16 * (int64_t)multiplier_q16 /
            (int64_t)PF_Q16_ONE;
        goto store_result;
    }
    if (source_normal_x_q16 == INT32_C(0))
    {
        mirrored_x =
            (int64_t)*velocity_x_q16 * (int64_t)multiplier_q16 /
            (int64_t)PF_Q16_ONE;
        mirrored_y =
            -(int64_t)*velocity_y_q16 * (int64_t)multiplier_q16 /
            (int64_t)PF_Q16_ONE;
        goto store_result;
    }

    /* lbVector_Mirror operates in Melee's isotropic, positive-Y-up physics
     * coordinates. Preserve eight guard bits while undoing this project's
     * anisotropic world conversion, mirror around the generated source-space
     * unit normal, apply common-data x1BC, and convert back once. */
    source_x =
        (int64_t)*velocity_x_q16 * INT64_C(115) *
        PF_M4_SSBM_VECTOR_EXTRA_SCALE / INT64_C(12);
    source_y_math =
        -(int64_t)*velocity_y_q16 * INT64_C(62) *
        PF_M4_SSBM_VECTOR_EXTRA_SCALE / INT64_C(11);
    dot =
        (source_x * (int64_t)source_normal_x_q16 +
         source_y_math * (int64_t)source_normal_y_q16) /
        (int64_t)PF_Q16_ONE;
    mirrored_source_x =
        source_x -
        INT64_C(2) * dot * (int64_t)source_normal_x_q16 /
            (int64_t)PF_Q16_ONE;
    mirrored_source_y_math =
        source_y_math -
        INT64_C(2) * dot * (int64_t)source_normal_y_q16 /
            (int64_t)PF_Q16_ONE;
    mirrored_source_x =
        mirrored_source_x * (int64_t)multiplier_q16 /
        (int64_t)PF_Q16_ONE;
    mirrored_source_y_math =
        mirrored_source_y_math * (int64_t)multiplier_q16 /
        (int64_t)PF_Q16_ONE;
    mirrored_x =
        mirrored_source_x * INT64_C(12) /
        (INT64_C(115) * PF_M4_SSBM_VECTOR_EXTRA_SCALE);
    mirrored_y =
        -mirrored_source_y_math * INT64_C(11) /
        (INT64_C(62) * PF_M4_SSBM_VECTOR_EXTRA_SCALE);
store_result:
    if (mirrored_x < (int64_t)INT32_MIN ||
        mirrored_x > (int64_t)INT32_MAX ||
        mirrored_y < (int64_t)INT32_MIN ||
        mirrored_y > (int64_t)INT32_MAX)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *velocity_x_q16 = (int32_t)mirrored_x;
    *velocity_y_q16 = (int32_t)mirrored_y;
    return PF_STATUS_OK;
}

pf_status pf_m4_ssbm_apply_di_q16(
    int32_t max_angle_radians_q30,
    int16_t stick_x,
    int16_t stick_y,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    /* Keep eight fractional guard bits while undoing the project's
     * anisotropic Melee-to-world coordinate conversion. DI rotates in
     * Melee's source coordinate system, not in screen-scaled world units. */
    int64_t source_velocity_x;
    int64_t source_velocity_y_math;
    const int64_t stick_x_64 =
        (int64_t)pf_m4_ssbm_clamp_stick_axis(stick_x);
    const int64_t stick_y_math =
        -(int64_t)pf_m4_ssbm_clamp_stick_axis(stick_y);
    uint64_t speed_squared;
    uint32_t speed;
    int64_t cross;
    int64_t projected_q16;
    int64_t projection_ratio_q16;
    int64_t turn_fraction_q16;
    int64_t angle_q30;
    int64_t sin_q30;
    int64_t cos_q30;
    int64_t influenced_source_x;
    int64_t influenced_source_y_math;
    int64_t influenced_x;
    int64_t influenced_y;

    if (max_angle_radians_q30 < INT32_C(0) ||
        velocity_x_q16 == NULL || velocity_y_q16 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    source_velocity_x =
        (int64_t)*velocity_x_q16 * INT64_C(115) *
        PF_M4_SSBM_VECTOR_EXTRA_SCALE /
        INT64_C(12);
    source_velocity_y_math =
        -(int64_t)*velocity_y_q16 * INT64_C(62) *
        PF_M4_SSBM_VECTOR_EXTRA_SCALE /
        INT64_C(11);
    speed_squared =
        (uint64_t)(source_velocity_x * source_velocity_x) +
        (uint64_t)(source_velocity_y_math * source_velocity_y_math);
    speed = pf_m4_u64_sqrt(speed_squared);

    if (speed == UINT32_C(0) ||
        (stick_x == INT16_C(0) && stick_y == INT16_C(0)))
    {
        return PF_STATUS_OK;
    }

    /* Melee squares the signed perpendicular stick projection rather than
     * linearly interpolating the maximum angle. Keep the quotient and
     * remainder separate so the Q16 projection cannot overflow int64_t. */
    cross =
        source_velocity_x * stick_y_math -
        source_velocity_y_math * stick_x_64;
    projected_q16 =
        (cross / (int64_t)speed) * (int64_t)PF_Q16_ONE +
        ((cross % (int64_t)speed) * (int64_t)PF_Q16_ONE) /
            (int64_t)speed;
    projection_ratio_q16 = projected_q16 / INT64_C(32767);
    turn_fraction_q16 =
        (projection_ratio_q16 * projection_ratio_q16) /
        (int64_t)PF_Q16_ONE;
    if (cross < INT64_C(0))
    {
        turn_fraction_q16 = -turn_fraction_q16;
    }
    angle_q30 =
        (int64_t)max_angle_radians_q30 * turn_fraction_q16 /
        (int64_t)PF_Q16_ONE;
    if (angle_q30 == INT64_C(0))
    {
        return PF_STATUS_OK;
    }
    pf_m4_ssbm_sin_cos_q30(angle_q30, &sin_q30, &cos_q30);

    influenced_source_x =
        (source_velocity_x * cos_q30 -
         source_velocity_y_math * sin_q30) /
        PF_M4_Q30_ONE;
    influenced_source_y_math =
        (source_velocity_x * sin_q30 +
         source_velocity_y_math * cos_q30) /
        PF_M4_Q30_ONE;
    influenced_x =
        influenced_source_x * INT64_C(12) /
        (INT64_C(115) * PF_M4_SSBM_VECTOR_EXTRA_SCALE);
    influenced_y =
        -influenced_source_y_math * INT64_C(11) /
        (INT64_C(62) * PF_M4_SSBM_VECTOR_EXTRA_SCALE);
    if (influenced_x < (int64_t)INT32_MIN ||
        influenced_x > (int64_t)INT32_MAX ||
        influenced_y < (int64_t)INT32_MIN ||
        influenced_y > (int64_t)INT32_MAX)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *velocity_x_q16 = (int32_t)influenced_x;
    *velocity_y_q16 = (int32_t)influenced_y;
    return PF_STATUS_OK;
}
