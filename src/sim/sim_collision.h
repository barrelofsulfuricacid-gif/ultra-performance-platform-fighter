#ifndef PF_SIM_COLLISION_H
#define PF_SIM_COLLISION_H

#include <stdint.h>

typedef struct collision_sphere3_q16
{
    int64_t center_x_q16;
    int64_t center_y_q16;
    int64_t center_z_q16;
    int64_t radius_q16;
} collision_sphere3_q16;

typedef struct collision_capsule3_q16
{
    int64_t endpoint_a_x_q16;
    int64_t endpoint_a_y_q16;
    int64_t endpoint_a_z_q16;
    int64_t endpoint_b_x_q16;
    int64_t endpoint_b_y_q16;
    int64_t endpoint_b_z_q16;
    int64_t radius_q16;
} collision_capsule3_q16;

static inline int64_t collision_dot3(
    int64_t left_x,
    int64_t left_y,
    int64_t left_z,
    int64_t right_x,
    int64_t right_y,
    int64_t right_z)
{
    return left_x * right_x + left_y * right_y + left_z * right_z;
}

static inline uint64_t collision_abs_u64(int64_t value)
{
    return value < INT64_C(0)
               ? (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1)
               : (uint64_t)value;
}

static inline int32_t collision_ratio_q16(
    int64_t numerator,
    int64_t denominator)
{
    uint64_t remainder;
    uint64_t divisor;
    uint32_t ratio_q16;
    uint32_t bit;

    if (numerator <= INT64_C(0) || denominator <= INT64_C(0))
    {
        return INT32_C(0);
    }
    if (numerator >= denominator)
    {
        return INT32_C(65536);
    }
    if (numerator <= INT64_MAX / INT64_C(65536))
    {
        return (int32_t)((numerator * INT64_C(65536)) / denominator);
    }

    /* numerator is smaller than the positive signed divisor, so doubling the
     * unsigned remainder cannot overflow uint64_t. This fixed 16-step divide
     * is the exact floor of numerator * 65536 / denominator without relying
     * on compiler-specific 128-bit integers or signed-overflow behavior. */
    remainder = (uint64_t)numerator;
    divisor = (uint64_t)denominator;
    ratio_q16 = UINT32_C(0);
    for (bit = UINT32_C(0); bit < UINT32_C(16); ++bit)
    {
        remainder <<= UINT32_C(1);
        ratio_q16 <<= UINT32_C(1);
        if (remainder >= divisor)
        {
            remainder -= divisor;
            ratio_q16 |= UINT32_C(1);
        }
    }
    return (int32_t)ratio_q16;
}

/* Integer specialization of the executable's point-to-capsule case. The
 * squared predicate avoids floating point and square roots; the only bounded
 * difference from the source is Q16.16 endpoint/projection quantization. */
static inline int collision_sphere_capsule_overlap_q16(
    const collision_sphere3_q16 *sphere,
    const collision_capsule3_q16 *capsule)
{
    const int64_t segment_x =
        capsule->endpoint_b_x_q16 - capsule->endpoint_a_x_q16;
    const int64_t segment_y =
        capsule->endpoint_b_y_q16 - capsule->endpoint_a_y_q16;
    const int64_t segment_z =
        capsule->endpoint_b_z_q16 - capsule->endpoint_a_z_q16;
    const int64_t sphere_from_a_x =
        sphere->center_x_q16 - capsule->endpoint_a_x_q16;
    const int64_t sphere_from_a_y =
        sphere->center_y_q16 - capsule->endpoint_a_y_q16;
    const int64_t sphere_from_a_z =
        sphere->center_z_q16 - capsule->endpoint_a_z_q16;
    const int64_t segment_length_squared =
        segment_x * segment_x + segment_y * segment_y +
        segment_z * segment_z;
    int64_t projection =
        sphere_from_a_x * segment_x +
        sphere_from_a_y * segment_y +
        sphere_from_a_z * segment_z;
    int64_t nearest_x;
    int64_t nearest_y;
    int64_t nearest_z;
    int64_t delta_x;
    int64_t delta_y;
    int64_t delta_z;
    const int64_t combined_radius =
        sphere->radius_q16 + capsule->radius_q16;

    if (projection < INT64_C(0))
    {
        projection = INT64_C(0);
    }
    else if (projection > segment_length_squared)
    {
        projection = segment_length_squared;
    }
    if (segment_length_squared == INT64_C(0))
    {
        nearest_x = capsule->endpoint_a_x_q16;
        nearest_y = capsule->endpoint_a_y_q16;
        nearest_z = capsule->endpoint_a_z_q16;
    }
    else
    {
        nearest_x = capsule->endpoint_a_x_q16 +
            segment_x * projection / segment_length_squared;
        nearest_y = capsule->endpoint_a_y_q16 +
            segment_y * projection / segment_length_squared;
        nearest_z = capsule->endpoint_a_z_q16 +
            segment_z * projection / segment_length_squared;
    }
    delta_x = sphere->center_x_q16 - nearest_x;
    delta_y = sphere->center_y_q16 - nearest_y;
    delta_z = sphere->center_z_q16 - nearest_z;
    return delta_x * delta_x + delta_y * delta_y +
               delta_z * delta_z <=
           combined_radius * combined_radius;
}

/* Integer specialization of the executable's moving-hit-sphere path. Melee
 * treats the previous and current hit-sphere centers as one capsule axis and
 * finds its closest points against the hurt/shield capsule axis. Dot products
 * are reduced to 23 significant bits before the determinant, matching or
 * exceeding the source float precision while keeping every intermediate in
 * portable C17 int64_t. Final contact coordinates remain Q16.16. */
static inline int collision_capsule_capsule_overlap_q16(
    const collision_capsule3_q16 *hit,
    const collision_capsule3_q16 *hurt)
{
    const int64_t combined_radius = hit->radius_q16 + hurt->radius_q16;
    const int64_t hit_min_x =
        hit->endpoint_a_x_q16 < hit->endpoint_b_x_q16
            ? hit->endpoint_a_x_q16
            : hit->endpoint_b_x_q16;
    const int64_t hit_max_x =
        hit->endpoint_a_x_q16 > hit->endpoint_b_x_q16
            ? hit->endpoint_a_x_q16
            : hit->endpoint_b_x_q16;
    const int64_t hit_min_y =
        hit->endpoint_a_y_q16 < hit->endpoint_b_y_q16
            ? hit->endpoint_a_y_q16
            : hit->endpoint_b_y_q16;
    const int64_t hit_max_y =
        hit->endpoint_a_y_q16 > hit->endpoint_b_y_q16
            ? hit->endpoint_a_y_q16
            : hit->endpoint_b_y_q16;
    const int64_t hit_min_z =
        hit->endpoint_a_z_q16 < hit->endpoint_b_z_q16
            ? hit->endpoint_a_z_q16
            : hit->endpoint_b_z_q16;
    const int64_t hit_max_z =
        hit->endpoint_a_z_q16 > hit->endpoint_b_z_q16
            ? hit->endpoint_a_z_q16
            : hit->endpoint_b_z_q16;
    const int64_t hurt_min_x =
        hurt->endpoint_a_x_q16 < hurt->endpoint_b_x_q16
            ? hurt->endpoint_a_x_q16
            : hurt->endpoint_b_x_q16;
    const int64_t hurt_max_x =
        hurt->endpoint_a_x_q16 > hurt->endpoint_b_x_q16
            ? hurt->endpoint_a_x_q16
            : hurt->endpoint_b_x_q16;
    const int64_t hurt_min_y =
        hurt->endpoint_a_y_q16 < hurt->endpoint_b_y_q16
            ? hurt->endpoint_a_y_q16
            : hurt->endpoint_b_y_q16;
    const int64_t hurt_max_y =
        hurt->endpoint_a_y_q16 > hurt->endpoint_b_y_q16
            ? hurt->endpoint_a_y_q16
            : hurt->endpoint_b_y_q16;
    const int64_t hurt_min_z =
        hurt->endpoint_a_z_q16 < hurt->endpoint_b_z_q16
            ? hurt->endpoint_a_z_q16
            : hurt->endpoint_b_z_q16;
    const int64_t hurt_max_z =
        hurt->endpoint_a_z_q16 > hurt->endpoint_b_z_q16
            ? hurt->endpoint_a_z_q16
            : hurt->endpoint_b_z_q16;
    const int64_t hit_x =
        hit->endpoint_b_x_q16 - hit->endpoint_a_x_q16;
    const int64_t hit_y =
        hit->endpoint_b_y_q16 - hit->endpoint_a_y_q16;
    const int64_t hit_z =
        hit->endpoint_b_z_q16 - hit->endpoint_a_z_q16;
    const int64_t hurt_x =
        hurt->endpoint_b_x_q16 - hurt->endpoint_a_x_q16;
    const int64_t hurt_y =
        hurt->endpoint_b_y_q16 - hurt->endpoint_a_y_q16;
    const int64_t hurt_z =
        hurt->endpoint_b_z_q16 - hurt->endpoint_a_z_q16;
    const int64_t start_x =
        hit->endpoint_a_x_q16 - hurt->endpoint_a_x_q16;
    const int64_t start_y =
        hit->endpoint_a_y_q16 - hurt->endpoint_a_y_q16;
    const int64_t start_z =
        hit->endpoint_a_z_q16 - hurt->endpoint_a_z_q16;
    int64_t hit_length_squared;
    int64_t segment_dot;
    int64_t hurt_length_squared;
    int64_t hit_start_dot;
    int64_t hurt_start_dot;
    uint64_t maximum_dot;
    int64_t dot_scale = INT64_C(1);
    int64_t denominator;
    int64_t hit_numerator;
    int64_t hit_denominator;
    int64_t hurt_numerator;
    int64_t hurt_denominator;
    int32_t hit_parameter_q16;
    int32_t hurt_parameter_q16;
    int64_t hit_closest_x;
    int64_t hit_closest_y;
    int64_t hit_closest_z;
    int64_t hurt_closest_x;
    int64_t hurt_closest_y;
    int64_t hurt_closest_z;
    int64_t delta_x;
    int64_t delta_y;
    int64_t delta_z;

    if (hit_max_x + combined_radius < hurt_min_x ||
        hit_min_x - combined_radius > hurt_max_x ||
        hit_max_y + combined_radius < hurt_min_y ||
        hit_min_y - combined_radius > hurt_max_y ||
        hit_max_z + combined_radius < hurt_min_z ||
        hit_min_z - combined_radius > hurt_max_z)
    {
        return 0;
    }

    hit_length_squared = collision_dot3(
        hit_x, hit_y, hit_z, hit_x, hit_y, hit_z);
    segment_dot = collision_dot3(
        hit_x, hit_y, hit_z, hurt_x, hurt_y, hurt_z);
    hurt_length_squared = collision_dot3(
        hurt_x, hurt_y, hurt_z, hurt_x, hurt_y, hurt_z);
    hit_start_dot = collision_dot3(
        hit_x, hit_y, hit_z, start_x, start_y, start_z);
    hurt_start_dot = collision_dot3(
        hurt_x, hurt_y, hurt_z, start_x, start_y, start_z);

    if (hit_length_squared == INT64_C(0))
    {
        const collision_sphere3_q16 sphere = {
            hit->endpoint_a_x_q16,
            hit->endpoint_a_y_q16,
            hit->endpoint_a_z_q16,
            hit->radius_q16};
        return collision_sphere_capsule_overlap_q16(&sphere, hurt);
    }
    if (hurt_length_squared == INT64_C(0))
    {
        const collision_sphere3_q16 sphere = {
            hurt->endpoint_a_x_q16,
            hurt->endpoint_a_y_q16,
            hurt->endpoint_a_z_q16,
            hurt->radius_q16};
        return collision_sphere_capsule_overlap_q16(&sphere, hit);
    }

    maximum_dot = collision_abs_u64(hit_length_squared);
#define PF_M4_COLLISION_ACCUMULATE_MAX(value)                              \
    do                                                                     \
    {                                                                      \
        const uint64_t candidate = collision_abs_u64(value);         \
        if (candidate > maximum_dot)                                       \
        {                                                                  \
            maximum_dot = candidate;                                      \
        }                                                                  \
    } while (0)
    PF_M4_COLLISION_ACCUMULATE_MAX(segment_dot);
    PF_M4_COLLISION_ACCUMULATE_MAX(hurt_length_squared);
    PF_M4_COLLISION_ACCUMULATE_MAX(hit_start_dot);
    PF_M4_COLLISION_ACCUMULATE_MAX(hurt_start_dot);
#undef PF_M4_COLLISION_ACCUMULATE_MAX
    while (maximum_dot > (UINT64_C(1) << 22))
    {
        maximum_dot >>= 1;
        dot_scale <<= 1;
    }
    hit_length_squared /= dot_scale;
    segment_dot /= dot_scale;
    hurt_length_squared /= dot_scale;
    hit_start_dot /= dot_scale;
    hurt_start_dot /= dot_scale;

    if (hit_length_squared == INT64_C(0))
    {
        const collision_sphere3_q16 sphere = {
            hit->endpoint_a_x_q16,
            hit->endpoint_a_y_q16,
            hit->endpoint_a_z_q16,
            hit->radius_q16};
        return collision_sphere_capsule_overlap_q16(&sphere, hurt);
    }
    if (hurt_length_squared == INT64_C(0))
    {
        const collision_sphere3_q16 sphere = {
            hurt->endpoint_a_x_q16,
            hurt->endpoint_a_y_q16,
            hurt->endpoint_a_z_q16,
            hurt->radius_q16};
        return collision_sphere_capsule_overlap_q16(&sphere, hit);
    }

    denominator =
        hit_length_squared * hurt_length_squared -
        segment_dot * segment_dot;
    hit_denominator = denominator;
    hurt_denominator = denominator;
    if (denominator <= INT64_C(0))
    {
        hit_numerator = INT64_C(0);
        hit_denominator = INT64_C(1);
        hurt_numerator = hurt_start_dot;
        hurt_denominator = hurt_length_squared;
    }
    else
    {
        hit_numerator =
            segment_dot * hurt_start_dot -
            hurt_length_squared * hit_start_dot;
        hurt_numerator =
            hit_length_squared * hurt_start_dot -
            segment_dot * hit_start_dot;
        if (hit_numerator < INT64_C(0))
        {
            hit_numerator = INT64_C(0);
            hurt_numerator = hurt_start_dot;
            hurt_denominator = hurt_length_squared;
        }
        else if (hit_numerator > hit_denominator)
        {
            hit_numerator = hit_denominator;
            hurt_numerator = hurt_start_dot + segment_dot;
            hurt_denominator = hurt_length_squared;
        }
    }
    if (hurt_numerator < INT64_C(0))
    {
        hurt_numerator = INT64_C(0);
        if (-hit_start_dot < INT64_C(0))
        {
            hit_numerator = INT64_C(0);
        }
        else if (-hit_start_dot > hit_length_squared)
        {
            hit_numerator = hit_denominator;
        }
        else
        {
            hit_numerator = -hit_start_dot;
            hit_denominator = hit_length_squared;
        }
    }
    else if (hurt_numerator > hurt_denominator)
    {
        hurt_numerator = hurt_denominator;
        if (-hit_start_dot + segment_dot < INT64_C(0))
        {
            hit_numerator = INT64_C(0);
        }
        else if (-hit_start_dot + segment_dot > hit_length_squared)
        {
            hit_numerator = hit_denominator;
        }
        else
        {
            hit_numerator = -hit_start_dot + segment_dot;
            hit_denominator = hit_length_squared;
        }
    }

    hit_parameter_q16 = collision_ratio_q16(
        hit_numerator,
        hit_denominator);
    hurt_parameter_q16 = collision_ratio_q16(
        hurt_numerator,
        hurt_denominator);
    hit_closest_x = hit->endpoint_a_x_q16 +
        hit_x * (int64_t)hit_parameter_q16 / INT64_C(65536);
    hit_closest_y = hit->endpoint_a_y_q16 +
        hit_y * (int64_t)hit_parameter_q16 / INT64_C(65536);
    hit_closest_z = hit->endpoint_a_z_q16 +
        hit_z * (int64_t)hit_parameter_q16 / INT64_C(65536);
    hurt_closest_x = hurt->endpoint_a_x_q16 +
        hurt_x * (int64_t)hurt_parameter_q16 / INT64_C(65536);
    hurt_closest_y = hurt->endpoint_a_y_q16 +
        hurt_y * (int64_t)hurt_parameter_q16 / INT64_C(65536);
    hurt_closest_z = hurt->endpoint_a_z_q16 +
        hurt_z * (int64_t)hurt_parameter_q16 / INT64_C(65536);
    delta_x = hit_closest_x - hurt_closest_x;
    delta_y = hit_closest_y - hurt_closest_y;
    delta_z = hit_closest_z - hurt_closest_z;
    return delta_x * delta_x + delta_y * delta_y + delta_z * delta_z <=
           combined_radius * combined_radius;
}

#endif
