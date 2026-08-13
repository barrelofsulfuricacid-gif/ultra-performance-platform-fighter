#ifndef PF_SIM_COLLISION_H
#define PF_SIM_COLLISION_H

typedef struct collision_sphere3_f32
{
    float center_x_f32;
    float center_y_f32;
    float center_z_f32;
    float radius_f32;
} collision_sphere3_f32;

typedef struct collision_capsule3_f32
{
    float endpoint_a_x_f32;
    float endpoint_a_y_f32;
    float endpoint_a_z_f32;
    float endpoint_b_x_f32;
    float endpoint_b_y_f32;
    float endpoint_b_z_f32;
    float radius_f32;
} collision_capsule3_f32;

static inline float collision_dot3(
    float left_x,
    float left_y,
    float left_z,
    float right_x,
    float right_y,
    float right_z)
{
    return left_x * right_x + left_y * right_y + left_z * right_z;
}

static inline float collision_clamp01(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

static inline int collision_sphere_capsule_overlap_f32(
    const collision_sphere3_f32 *sphere,
    const collision_capsule3_f32 *capsule)
{
    const float segment_x =
        capsule->endpoint_b_x_f32 - capsule->endpoint_a_x_f32;
    const float segment_y =
        capsule->endpoint_b_y_f32 - capsule->endpoint_a_y_f32;
    const float segment_z =
        capsule->endpoint_b_z_f32 - capsule->endpoint_a_z_f32;
    const float sphere_from_a_x =
        sphere->center_x_f32 - capsule->endpoint_a_x_f32;
    const float sphere_from_a_y =
        sphere->center_y_f32 - capsule->endpoint_a_y_f32;
    const float sphere_from_a_z =
        sphere->center_z_f32 - capsule->endpoint_a_z_f32;
    const float segment_length_squared = collision_dot3(
        segment_x,
        segment_y,
        segment_z,
        segment_x,
        segment_y,
        segment_z);
    const float parameter =
        segment_length_squared > 0.0f
            ? collision_clamp01(
                  collision_dot3(
                      sphere_from_a_x,
                      sphere_from_a_y,
                      sphere_from_a_z,
                      segment_x,
                      segment_y,
                      segment_z) /
                  segment_length_squared)
            : 0.0f;
    const float delta_x = sphere->center_x_f32 -
        (capsule->endpoint_a_x_f32 + segment_x * parameter);
    const float delta_y = sphere->center_y_f32 -
        (capsule->endpoint_a_y_f32 + segment_y * parameter);
    const float delta_z = sphere->center_z_f32 -
        (capsule->endpoint_a_z_f32 + segment_z * parameter);
    const float combined_radius =
        sphere->radius_f32 + capsule->radius_f32;

    return collision_dot3(
               delta_x,
               delta_y,
               delta_z,
               delta_x,
               delta_y,
               delta_z) <=
           combined_radius * combined_radius;
}

/* Closest points between two line segments, specialized for the moving hit
 * capsule and hurt capsule. All arithmetic stays in binary32 and preserves
 * source expression order; compiler contraction is disabled project-wide. */
static inline int collision_capsule_capsule_overlap_f32(
    const collision_capsule3_f32 *hit,
    const collision_capsule3_f32 *hurt)
{
    const float hit_x = hit->endpoint_b_x_f32 - hit->endpoint_a_x_f32;
    const float hit_y = hit->endpoint_b_y_f32 - hit->endpoint_a_y_f32;
    const float hit_z = hit->endpoint_b_z_f32 - hit->endpoint_a_z_f32;
    const float hurt_x =
        hurt->endpoint_b_x_f32 - hurt->endpoint_a_x_f32;
    const float hurt_y =
        hurt->endpoint_b_y_f32 - hurt->endpoint_a_y_f32;
    const float hurt_z =
        hurt->endpoint_b_z_f32 - hurt->endpoint_a_z_f32;
    const float start_x = hit->endpoint_a_x_f32 - hurt->endpoint_a_x_f32;
    const float start_y = hit->endpoint_a_y_f32 - hurt->endpoint_a_y_f32;
    const float start_z = hit->endpoint_a_z_f32 - hurt->endpoint_a_z_f32;
    const float hit_length_squared = collision_dot3(
        hit_x, hit_y, hit_z, hit_x, hit_y, hit_z);
    const float segment_dot = collision_dot3(
        hit_x, hit_y, hit_z, hurt_x, hurt_y, hurt_z);
    const float hurt_length_squared = collision_dot3(
        hurt_x, hurt_y, hurt_z, hurt_x, hurt_y, hurt_z);
    const float hit_start_dot = collision_dot3(
        hit_x, hit_y, hit_z, start_x, start_y, start_z);
    const float hurt_start_dot = collision_dot3(
        hurt_x, hurt_y, hurt_z, start_x, start_y, start_z);
    const float denominator =
        hit_length_squared * hurt_length_squared -
        segment_dot * segment_dot;
    float hit_parameter;
    float hurt_parameter;
    float hurt_numerator;

    if (hit_length_squared <= 0.0f)
    {
        const collision_sphere3_f32 sphere = {
            hit->endpoint_a_x_f32,
            hit->endpoint_a_y_f32,
            hit->endpoint_a_z_f32,
            hit->radius_f32};
        return collision_sphere_capsule_overlap_f32(&sphere, hurt);
    }
    if (hurt_length_squared <= 0.0f)
    {
        const collision_sphere3_f32 sphere = {
            hurt->endpoint_a_x_f32,
            hurt->endpoint_a_y_f32,
            hurt->endpoint_a_z_f32,
            hurt->radius_f32};
        return collision_sphere_capsule_overlap_f32(&sphere, hit);
    }

    hit_parameter = denominator > 0.0f
                        ? collision_clamp01(
                              (segment_dot * hurt_start_dot -
                               hurt_length_squared * hit_start_dot) /
                              denominator)
                        : 0.0f;
    hurt_numerator = segment_dot * hit_parameter + hurt_start_dot;
    if (hurt_numerator <= 0.0f)
    {
        hurt_parameter = 0.0f;
        hit_parameter = collision_clamp01(-hit_start_dot /
                                          hit_length_squared);
    }
    else if (hurt_numerator >= hurt_length_squared)
    {
        hurt_parameter = 1.0f;
        hit_parameter = collision_clamp01(
            (segment_dot - hit_start_dot) / hit_length_squared);
    }
    else
    {
        hurt_parameter = hurt_numerator / hurt_length_squared;
    }

    {
        const float delta_x =
            hit->endpoint_a_x_f32 + hit_x * hit_parameter -
            (hurt->endpoint_a_x_f32 + hurt_x * hurt_parameter);
        const float delta_y =
            hit->endpoint_a_y_f32 + hit_y * hit_parameter -
            (hurt->endpoint_a_y_f32 + hurt_y * hurt_parameter);
        const float delta_z =
            hit->endpoint_a_z_f32 + hit_z * hit_parameter -
            (hurt->endpoint_a_z_f32 + hurt_z * hurt_parameter);
        const float combined_radius = hit->radius_f32 + hurt->radius_f32;

        return collision_dot3(
                   delta_x,
                   delta_y,
                   delta_z,
                   delta_x,
                   delta_y,
                   delta_z) <=
               combined_radius * combined_radius;
    }
}

#endif
