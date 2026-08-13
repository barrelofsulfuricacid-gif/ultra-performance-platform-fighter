#include "sim_ssbm_damage.h"

#include "sim_internal.h"
#include "sim_ssbm_common_data.h"

#include <math.h>
#include <stdint.h>

#define PF_M4_Q30_SCALE 1073741824.0f

static float ssbm_source_x(float world_x)
{
    return world_x * (115.0f / 12.0f);
}

static float ssbm_source_y_math(float world_y)
{
    return -world_y * (62.0f / 11.0f);
}

static float ssbm_world_x(float source_x)
{
    return source_x * (12.0f / 115.0f);
}

static float ssbm_world_y(float source_y_math)
{
    return -source_y_math * (11.0f / 62.0f);
}

ssbm_damage_floor_response ssbm_select_damage_floor_response_f32(
    float knockback_velocity_x_f32,
    float knockback_velocity_y_f32,
    uint8_t force_down_bound)
{
    const ssbm_damage_response_attributes *common =
        ssbm_common_reference_damage_response();
    const float source_x = ssbm_source_x(knockback_velocity_x_f32);
    const float source_y = ssbm_source_y_math(knockback_velocity_y_f32);
    const float magnitude_squared = source_x * source_x + source_y * source_y;

    if (force_down_bound != UINT8_C(0) ||
        fabsf(source_x) >= common->damage_floor_down_speed_f32 ||
        fabsf(source_y) >= common->damage_floor_down_speed_f32 ||
        magnitude_squared >= common->damage_floor_down_speed_f32 *
                                 common->damage_floor_down_speed_f32)
    {
        return PF_M4_SSBM_DAMAGE_FLOOR_DOWN_BOUND;
    }
    return magnitude_squared >= common->damage_floor_landing_speed_f32 *
                                    common->damage_floor_landing_speed_f32
               ? PF_M4_SSBM_DAMAGE_FLOOR_LANDING
               : PF_M4_SSBM_DAMAGE_FLOOR_KEEP_ACTION;
}

pf_status ssbm_resolve_ground_damage_launch_f32(
    float source_normal_x_f32,
    float source_normal_y_f32,
    float ground_projection_x_f32,
    float ground_projection_y_f32,
    uint8_t damage_level,
    float *velocity_x_f32,
    float *velocity_y_f32,
    float *ground_scalar_f32,
    uint8_t *launch_grounded)
{
    const ssbm_damage_response_attributes *common =
        ssbm_common_reference_damage_response();
    float source_x;
    float source_y;
    float dot;

    if (velocity_x_f32 == NULL || velocity_y_f32 == NULL ||
        ground_scalar_f32 == NULL || launch_grounded == NULL ||
        common == NULL || damage_level > UINT8_C(3) ||
        (source_normal_x_f32 == 0.0f && source_normal_y_f32 == 0.0f) ||
        fabsf(source_normal_x_f32) > 1.0f ||
        fabsf(source_normal_y_f32) > 1.0f)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    source_x = ssbm_source_x(*velocity_x_f32);
    source_y = ssbm_source_y_math(*velocity_y_f32);
    dot = source_x * source_normal_x_f32 +
          source_y * source_normal_y_f32;
    *ground_scalar_f32 = 0.0f;
    *launch_grounded = UINT8_C(0);
    if (dot > 0.0f)
    {
        return PF_STATUS_OK;
    }
    if (damage_level != UINT8_C(3))
    {
        *ground_scalar_f32 = *velocity_x_f32;
        *velocity_x_f32 *= ground_projection_x_f32;
        *velocity_y_f32 =
            *ground_scalar_f32 * ground_projection_y_f32;
        *launch_grounded = UINT8_C(1);
        return isfinite(*velocity_x_f32) && isfinite(*velocity_y_f32)
                   ? PF_STATUS_OK
                   : PF_STATUS_DETERMINISTIC_FAULT;
    }

    {
        const float magnitude = sqrtf(source_x * source_x + source_y * source_y);
        const float normal_magnitude = sqrtf(
            source_normal_x_f32 * source_normal_x_f32 +
            source_normal_y_f32 * source_normal_y_f32);

        if (-dot > magnitude * normal_magnitude *
                       common->ground_damage_steep_angle_sine_f32)
        {
            *velocity_y_f32 = -*velocity_y_f32 *
                              common->ground_damage_vertical_reflection_f32;
        }
    }
    return isfinite(*velocity_y_f32) ? PF_STATUS_OK
                                     : PF_STATUS_DETERMINISTIC_FAULT;
}

pf_status ssbm_select_damage_motion(
    uint8_t launch_grounded,
    uint8_t damage_level,
    float resulting_damage_f32,
    float launch_velocity_x_f32,
    float launch_velocity_y_f32,
    uint64_t *rng_state,
    ssbm_damage_motion_kind *out_motion)
{
    const ssbm_damage_response_attributes *common =
        ssbm_common_reference_damage_response();

    if (rng_state == NULL || out_motion == NULL || common == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    *out_motion = PF_M4_SSBM_DAMAGE_MOTION_ORDINARY;
    if (launch_grounded != UINT8_C(0) || damage_level != UINT8_C(3))
    {
        return PF_STATUS_OK;
    }
    if (launch_velocity_y_f32 < 0.0f)
    {
        const float horizontal = fabsf(ssbm_source_x(launch_velocity_x_f32));
        const float vertical = ssbm_source_y_math(launch_velocity_y_f32);

        if (horizontal < vertical * common->damage_fly_top_horizontal_ratio_f32)
        {
            *out_motion = PF_M4_SSBM_DAMAGE_MOTION_FLY_TOP;
            return PF_STATUS_OK;
        }
    }
    if (resulting_damage_f32 >=
        (float)common->damage_fly_roll_damage_threshold)
    {
        const uint16_t random = pf_sim_hsd_random_u16(rng_state);

        if (random < common->damage_fly_roll_random_threshold_u16)
        {
            *out_motion = PF_M4_SSBM_DAMAGE_MOTION_FLY_ROLL;
        }
    }
    return PF_STATUS_OK;
}

static float ssbm_stick_axis(int16_t axis)
{
    return axis == INT16_MIN ? -32767.0f : (float)axis;
}

int ssbm_stick_meets_radial_threshold(
    int16_t stick_x,
    int16_t stick_y,
    uint16_t threshold)
{
    const float x = ssbm_stick_axis(stick_x);
    const float y = ssbm_stick_axis(stick_y);
    const float limit = (float)threshold;

    return x * x + y * y >= limit * limit;
}

float ssbm_analog_displacement_f32(
    int16_t stick_axis,
    float maximum_distance_f32)
{
    return ssbm_stick_axis(stick_axis) * maximum_distance_f32 / 32767.0f;
}

pf_status ssbm_decay_air_knockback_f32(
    float decay_f32,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    float source_x;
    float source_y;
    float magnitude;
    float remaining;

    if (decay_f32 < 0.0f || velocity_x_f32 == NULL ||
        velocity_y_f32 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    source_x = ssbm_source_x(*velocity_x_f32);
    source_y = ssbm_source_y_math(*velocity_y_f32);
    magnitude = sqrtf(source_x * source_x + source_y * source_y);
    if (!(magnitude > decay_f32))
    {
        *velocity_x_f32 = 0.0f;
        *velocity_y_f32 = 0.0f;
        return PF_STATUS_OK;
    }
    remaining = (magnitude - decay_f32) / magnitude;
    *velocity_x_f32 = ssbm_world_x(source_x * remaining);
    *velocity_y_f32 = ssbm_world_y(source_y * remaining);
    return isfinite(*velocity_x_f32) && isfinite(*velocity_y_f32)
               ? PF_STATUS_OK
               : PF_STATUS_DETERMINISTIC_FAULT;
}

pf_status ssbm_mirror_velocity_f32(
    float source_normal_x_f32,
    float source_normal_y_f32,
    float multiplier_f32,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    float source_x;
    float source_y;
    float dot;

    if (multiplier_f32 < 0.0f || velocity_x_f32 == NULL ||
        velocity_y_f32 == NULL ||
        (source_normal_x_f32 == 0.0f && source_normal_y_f32 == 0.0f))
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    source_x = ssbm_source_x(*velocity_x_f32);
    source_y = ssbm_source_y_math(*velocity_y_f32);
    dot = source_x * source_normal_x_f32 +
          source_y * source_normal_y_f32;
    source_x = (source_x - 2.0f * dot * source_normal_x_f32) *
               multiplier_f32;
    source_y = (source_y - 2.0f * dot * source_normal_y_f32) *
               multiplier_f32;
    *velocity_x_f32 = ssbm_world_x(source_x);
    *velocity_y_f32 = ssbm_world_y(source_y);
    return isfinite(*velocity_x_f32) && isfinite(*velocity_y_f32)
               ? PF_STATUS_OK
               : PF_STATUS_DETERMINISTIC_FAULT;
}

pf_status ssbm_apply_di_f32(
    int32_t max_angle_radians_q30,
    int16_t stick_x,
    int16_t stick_y,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    float source_x;
    float source_y;
    float speed;
    float cross;
    float projection;
    float turn_fraction;
    float angle;
    float sine;
    float cosine;

    if (max_angle_radians_q30 < INT32_C(0) || velocity_x_f32 == NULL ||
        velocity_y_f32 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    source_x = ssbm_source_x(*velocity_x_f32);
    source_y = ssbm_source_y_math(*velocity_y_f32);
    speed = sqrtf(source_x * source_x + source_y * source_y);
    if (!(speed > 0.0f) ||
        (stick_x == INT16_C(0) && stick_y == INT16_C(0)))
    {
        return PF_STATUS_OK;
    }
    cross = source_x * -ssbm_stick_axis(stick_y) -
            source_y * ssbm_stick_axis(stick_x);
    projection = cross / speed / 32767.0f;
    turn_fraction = copysignf(projection * projection, cross);
    angle = ((float)max_angle_radians_q30 / PF_M4_Q30_SCALE) *
            turn_fraction;
    sine = sinf(angle);
    cosine = cosf(angle);
    {
        const float rotated_x = source_x * cosine - source_y * sine;
        const float rotated_y = source_x * sine + source_y * cosine;

        *velocity_x_f32 = ssbm_world_x(rotated_x);
        *velocity_y_f32 = ssbm_world_y(rotated_y);
    }
    return isfinite(*velocity_x_f32) && isfinite(*velocity_y_f32)
               ? PF_STATUS_OK
               : PF_STATUS_DETERMINISTIC_FAULT;
}
