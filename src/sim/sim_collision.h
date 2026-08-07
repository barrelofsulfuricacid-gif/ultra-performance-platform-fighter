#ifndef PF_SIM_COLLISION_H
#define PF_SIM_COLLISION_H

#include <stdint.h>

typedef struct pf_m4_collision_sphere3_q16
{
    int64_t center_x_q16;
    int64_t center_y_q16;
    int64_t center_z_q16;
    int64_t radius_q16;
} pf_m4_collision_sphere3_q16;

typedef struct pf_m4_collision_capsule3_q16
{
    int64_t endpoint_a_x_q16;
    int64_t endpoint_a_y_q16;
    int64_t endpoint_a_z_q16;
    int64_t endpoint_b_x_q16;
    int64_t endpoint_b_y_q16;
    int64_t endpoint_b_z_q16;
    int64_t radius_q16;
} pf_m4_collision_capsule3_q16;

/* Integer specialization of the executable's point-to-capsule case. The
 * squared predicate avoids floating point and square roots; the only bounded
 * difference from the source is Q16.16 endpoint/projection quantization. */
static inline int pf_m4_collision_sphere_capsule_overlap_q16(
    const pf_m4_collision_sphere3_q16 *sphere,
    const pf_m4_collision_capsule3_q16 *capsule)
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

#endif
