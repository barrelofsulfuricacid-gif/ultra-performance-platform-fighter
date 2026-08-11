#include "sim_hsd_pose.h"
#include "sim_fixed_math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define PF_HSD_Q16_ONE INT32_C(65536)
#define PF_HSD_QUARTER_TURN UINT16_C(16384)
#define PF_HSD_QUARTER_SEGMENTS UINT16_C(64)
#define PF_HSD_QUARTER_SEGMENT_PHASE UINT16_C(256)

typedef struct pf_m4_hsd_matrix
{
    int32_t value[3][4];
} pf_m4_hsd_matrix;

/* Quarter-wave samples at pi/128. Linear interpolation keeps the only
 * approximation inside the simulation's documented Q16.16 boundary. */
static const int32_t pf_m4_hsd_sine_quarter_q16[65] = {
    INT32_C(0), INT32_C(1608), INT32_C(3216), INT32_C(4821),
    INT32_C(6424), INT32_C(8022), INT32_C(9616), INT32_C(11204),
    INT32_C(12785), INT32_C(14359), INT32_C(15924), INT32_C(17479),
    INT32_C(19024), INT32_C(20557), INT32_C(22078), INT32_C(23586),
    INT32_C(25080), INT32_C(26558), INT32_C(28020), INT32_C(29466),
    INT32_C(30893), INT32_C(32303), INT32_C(33692), INT32_C(35062),
    INT32_C(36410), INT32_C(37736), INT32_C(39040), INT32_C(40320),
    INT32_C(41576), INT32_C(42806), INT32_C(44011), INT32_C(45190),
    INT32_C(46341), INT32_C(47464), INT32_C(48559), INT32_C(49624),
    INT32_C(50660), INT32_C(51665), INT32_C(52639), INT32_C(53581),
    INT32_C(54491), INT32_C(55368), INT32_C(56212), INT32_C(57022),
    INT32_C(57798), INT32_C(58538), INT32_C(59244), INT32_C(59914),
    INT32_C(60547), INT32_C(61145), INT32_C(61705), INT32_C(62228),
    INT32_C(62714), INT32_C(63162), INT32_C(63572), INT32_C(63944),
    INT32_C(64277), INT32_C(64571), INT32_C(64827), INT32_C(65043),
    INT32_C(65220), INT32_C(65358), INT32_C(65457), INT32_C(65516),
    INT32_C(65536)};

static int64_t pf_m4_hsd_round_divide(int64_t numerator, int64_t denominator)
{
    if (denominator <= INT64_C(0))
    {
        return INT64_C(0);
    }
    return numerator < INT64_C(0)
               ? -((-numerator + denominator / INT64_C(2)) / denominator)
               : (numerator + denominator / INT64_C(2)) / denominator;
}

static int32_t pf_m4_hsd_q16_multiply(int32_t left, int32_t right)
{
    return (int32_t)pf_m4_hsd_round_divide(
        (int64_t)left * (int64_t)right,
        INT64_C(65536));
}

static int pf_m4_hsd_q16_divide(
    int32_t numerator,
    int32_t denominator,
    int32_t *out_value)
{
    int64_t value;

    if (out_value == NULL || denominator == INT32_C(0))
    {
        return 0;
    }
    value = pf_m4_hsd_round_divide(
        (int64_t)numerator * INT64_C(65536),
        denominator < INT32_C(0)
            ? -(int64_t)denominator
            : (int64_t)denominator);
    if (denominator < INT32_C(0))
    {
        value = -value;
    }
    if (value < (int64_t)INT32_MIN || value > (int64_t)INT32_MAX)
    {
        return 0;
    }
    *out_value = (int32_t)value;
    return 1;
}

static int32_t pf_m4_hsd_sine_q16(int32_t turns_q16)
{
    const uint16_t phase = (uint16_t)turns_q16;
    const uint16_t quadrant = (uint16_t)(phase >> 14U);
    const uint16_t within = (uint16_t)(phase & UINT16_C(0x3FFF));
    const uint16_t coordinate =
        quadrant == UINT16_C(0) || quadrant == UINT16_C(2)
            ? within
            : (uint16_t)(PF_HSD_QUARTER_TURN - within);
    const uint16_t index =
        (uint16_t)(coordinate / PF_HSD_QUARTER_SEGMENT_PHASE);
    const uint16_t fraction =
        (uint16_t)(coordinate % PF_HSD_QUARTER_SEGMENT_PHASE);
    int32_t value;

    if (index >= PF_HSD_QUARTER_SEGMENTS)
    {
        value = pf_m4_hsd_sine_quarter_q16[PF_HSD_QUARTER_SEGMENTS];
    }
    else
    {
        value = (int32_t)pf_m4_hsd_round_divide(
            (int64_t)pf_m4_hsd_sine_quarter_q16[index] *
                    (PF_HSD_QUARTER_SEGMENT_PHASE - fraction) +
                (int64_t)pf_m4_hsd_sine_quarter_q16[index + UINT16_C(1)] *
                    fraction,
            PF_HSD_QUARTER_SEGMENT_PHASE);
    }
    return quadrant >= UINT16_C(2) ? -value : value;
}

static int32_t pf_m4_hsd_cosine_q16(int32_t turns_q16)
{
    return pf_m4_hsd_sine_q16(turns_q16 + INT32_C(16384));
}

static int pf_m4_hsd_normalize_quaternion_q16(int32_t quaternion_q16[4])
{
    uint64_t magnitude_squared = UINT64_C(0);
    uint32_t magnitude_q16;
    uint8_t component;

    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        const int64_t value = quaternion_q16[component];
        magnitude_squared += (uint64_t)(value * value);
    }
    magnitude_q16 = pf_m4_u64_sqrt(magnitude_squared);
    if (magnitude_q16 == UINT32_C(0))
    {
        return 0;
    }
    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        const int64_t scaled =
            (int64_t)quaternion_q16[component] * INT64_C(65536);
        quaternion_q16[component] = (int32_t)pf_m4_hsd_round_divide(
            scaled,
            (int64_t)magnitude_q16);
    }
    return 1;
}

static int pf_m4_hsd_euler_to_quaternion_q16(
    const int32_t rotation_turns_q16[3],
    int32_t out_quaternion_q16[4])
{
    const int32_t half_x = rotation_turns_q16[0] / INT32_C(2);
    const int32_t half_y = rotation_turns_q16[1] / INT32_C(2);
    const int32_t half_z = rotation_turns_q16[2] / INT32_C(2);
    const int32_t sx = pf_m4_hsd_sine_q16(half_x);
    const int32_t sy = pf_m4_hsd_sine_q16(half_y);
    const int32_t sz = pf_m4_hsd_sine_q16(half_z);
    const int32_t cx = pf_m4_hsd_cosine_q16(half_x);
    const int32_t cy = pf_m4_hsd_cosine_q16(half_y);
    const int32_t cz = pf_m4_hsd_cosine_q16(half_z);
    const int32_t ss = pf_m4_hsd_q16_multiply(sy, sz);
    const int32_t cc = pf_m4_hsd_q16_multiply(cy, cz);

    out_quaternion_q16[0] =
        pf_m4_hsd_q16_multiply(sx, cc) -
        pf_m4_hsd_q16_multiply(cx, ss);
    out_quaternion_q16[1] =
        pf_m4_hsd_q16_multiply(cz, pf_m4_hsd_q16_multiply(cx, sy)) +
        pf_m4_hsd_q16_multiply(sz, pf_m4_hsd_q16_multiply(sx, cy));
    out_quaternion_q16[2] =
        pf_m4_hsd_q16_multiply(sz, pf_m4_hsd_q16_multiply(cx, cy)) -
        pf_m4_hsd_q16_multiply(cz, pf_m4_hsd_q16_multiply(sx, sy));
    out_quaternion_q16[3] =
        pf_m4_hsd_q16_multiply(cx, cc) +
        pf_m4_hsd_q16_multiply(sx, ss);
    return pf_m4_hsd_normalize_quaternion_q16(out_quaternion_q16);
}

static int pf_m4_hsd_pose_quaternion_q16(
    const pf_m4_hsd_local_pose *pose,
    int32_t out_quaternion_q16[4])
{
    if (pose->use_quaternion != UINT8_C(0))
    {
        (void)memcpy(
            out_quaternion_q16,
            pose->rotation_q16,
            sizeof(int32_t) * 4U);
        return pf_m4_hsd_normalize_quaternion_q16(out_quaternion_q16);
    }
    return pf_m4_hsd_euler_to_quaternion_q16(
        pose->rotation_q16,
        out_quaternion_q16);
}

static int pf_m4_hsd_slerp_q16(
    const int32_t target_q16[4],
    const int32_t current_q16[4],
    int32_t current_weight_q16,
    int32_t out_quaternion_q16[4])
{
    int32_t target[4];
    int32_t current[4];
    int64_t dot_sum = INT64_C(0);
    int32_t dot_q16;
    int32_t target_weight_q16;
    uint8_t component;

    if (current_weight_q16 < INT32_C(0) ||
        current_weight_q16 > PF_HSD_Q16_ONE)
    {
        return 0;
    }
    (void)memcpy(target, target_q16, sizeof(target));
    (void)memcpy(current, current_q16, sizeof(current));
    if (!pf_m4_hsd_normalize_quaternion_q16(target) ||
        !pf_m4_hsd_normalize_quaternion_q16(current))
    {
        return 0;
    }
    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        dot_sum += (int64_t)target[component] * current[component];
    }
    dot_q16 = (int32_t)pf_m4_hsd_round_divide(dot_sum, INT64_C(65536));
    if (dot_q16 < INT32_C(0))
    {
        dot_q16 = -dot_q16;
        for (component = UINT8_C(0); component < UINT8_C(4); ++component)
        {
            current[component] = -current[component];
        }
    }
    if (dot_q16 > PF_HSD_Q16_ONE)
    {
        dot_q16 = PF_HSD_Q16_ONE;
    }
    target_weight_q16 = PF_HSD_Q16_ONE - current_weight_q16;
    if (PF_HSD_Q16_ONE - dot_q16 <= INT32_C(8))
    {
        for (component = UINT8_C(0); component < UINT8_C(4); ++component)
        {
            out_quaternion_q16[component] =
                pf_m4_hsd_q16_multiply(target_weight_q16, target[component]) +
                pf_m4_hsd_q16_multiply(current_weight_q16, current[component]);
        }
        return pf_m4_hsd_normalize_quaternion_q16(out_quaternion_q16);
    }
    {
        const int32_t dot_squared_q16 =
            pf_m4_hsd_q16_multiply(dot_q16, dot_q16);
        const int32_t sine_theta_q16 = (int32_t)pf_m4_u64_sqrt(
            (uint64_t)(PF_HSD_Q16_ONE - dot_squared_q16) << 16U);
        const uint16_t theta_turn =
            pf_m4_fixed_atan2_turn(sine_theta_q16, dot_q16);
        const int32_t target_sine_q16 = pf_m4_hsd_sine_q16(
            pf_m4_hsd_q16_multiply(target_weight_q16, theta_turn));
        const int32_t current_sine_q16 = pf_m4_hsd_sine_q16(
            pf_m4_hsd_q16_multiply(current_weight_q16, theta_turn));
        int32_t target_coefficient_q16;
        int32_t current_coefficient_q16;

        if (sine_theta_q16 <= INT32_C(0) ||
            !pf_m4_hsd_q16_divide(
                target_sine_q16,
                sine_theta_q16,
                &target_coefficient_q16) ||
            !pf_m4_hsd_q16_divide(
                current_sine_q16,
                sine_theta_q16,
                &current_coefficient_q16))
        {
            return 0;
        }
        for (component = UINT8_C(0); component < UINT8_C(4); ++component)
        {
            out_quaternion_q16[component] =
                pf_m4_hsd_q16_multiply(
                    target_coefficient_q16,
                    target[component]) +
                pf_m4_hsd_q16_multiply(
                    current_coefficient_q16,
                    current[component]);
        }
    }
    return pf_m4_hsd_normalize_quaternion_q16(out_quaternion_q16);
}

static int32_t pf_m4_hsd_sample_track_q16(
    const pf_m4_hsd_track *track,
    const pf_m4_hsd_key *keys,
    int32_t frame_q16)
{
    const int64_t requested_frame_q16 =
        (int64_t)frame_q16 +
        (int64_t)track->start_frame * (int64_t)PF_HSD_Q16_ONE;
    int32_t p0 = INT32_C(0);
    int32_t p1 = INT32_C(0);
    int32_t d0 = INT32_C(0);
    int32_t d1 = INT32_C(0);
    int32_t t0 = INT32_C(0);
    int32_t t1 = INT32_C(0);
    uint8_t interpolation = PF_M4_HSD_INTERPOLATION_CONSTANT;
    uint8_t previous_interpolation = PF_M4_HSD_INTERPOLATION_CONSTANT;
    uint16_t key_index;

    if (requested_frame_q16 < (int64_t)INT32_MIN)
    {
        frame_q16 = INT32_MIN;
    }
    else if (requested_frame_q16 > (int64_t)INT32_MAX)
    {
        frame_q16 = INT32_MAX;
    }
    else
    {
        frame_q16 = (int32_t)requested_frame_q16;
    }
    if (track->key_count == UINT16_C(0))
    {
        return INT32_C(0);
    }
    if (track->key_count > UINT16_C(1) &&
        frame_q16 >= keys[track->key_count - UINT16_C(1)].frame_q16)
    {
        return keys[track->key_count - UINT16_C(1)].value_q16;
    }
    for (key_index = UINT16_C(0);
         key_index < track->key_count;
         ++key_index)
    {
        const pf_m4_hsd_key *key = &keys[key_index];

        previous_interpolation = interpolation;
        interpolation = key->interpolation;
        switch ((pf_m4_hsd_interpolation)interpolation)
        {
            case PF_M4_HSD_INTERPOLATION_CONSTANT:
            case PF_M4_HSD_INTERPOLATION_LINEAR:
                p0 = p1;
                p1 = key->value_q16;
                if (previous_interpolation != PF_M4_HSD_INTERPOLATION_SLOPE)
                {
                    d0 = d1;
                    d1 = INT32_C(0);
                }
                t0 = t1;
                t1 = key->frame_q16;
                break;
            case PF_M4_HSD_INTERPOLATION_SPLINE_ZERO:
                p0 = p1;
                d0 = d1;
                p1 = key->value_q16;
                d1 = INT32_C(0);
                t0 = t1;
                t1 = key->frame_q16;
                break;
            case PF_M4_HSD_INTERPOLATION_SPLINE:
                p0 = p1;
                p1 = key->value_q16;
                d0 = d1;
                d1 = key->tangent_q16;
                t0 = t1;
                t1 = key->frame_q16;
                break;
            case PF_M4_HSD_INTERPOLATION_SLOPE:
                d0 = d1;
                d1 = key->tangent_q16;
                break;
            case PF_M4_HSD_INTERPOLATION_KEY:
                p0 = key->value_q16;
                p1 = key->value_q16;
                break;
            default:
                return INT32_C(0);
        }
        if (t1 > frame_q16 &&
            interpolation != PF_M4_HSD_INTERPOLATION_SLOPE)
        {
            break;
        }
        previous_interpolation = interpolation;
    }
    if (frame_q16 <= t0)
    {
        return p0;
    }
    if (frame_q16 >= t1)
    {
        return p1;
    }
    if (t0 == t1 ||
        previous_interpolation == PF_M4_HSD_INTERPOLATION_CONSTANT ||
        previous_interpolation == PF_M4_HSD_INTERPOLATION_KEY)
    {
        return p0;
    }
    if (previous_interpolation == PF_M4_HSD_INTERPOLATION_LINEAR)
    {
        return p0 + (int32_t)pf_m4_hsd_round_divide(
                        (int64_t)(p1 - p0) * (int64_t)(frame_q16 - t0),
                        (int64_t)(t1 - t0));
    }
    {
        int32_t normalized_q16;
        int32_t normalized2_q16;
        int32_t normalized3_q16;
        int32_t h00_q16;
        int32_t h10_q16;
        int32_t h01_q16;
        int32_t h11_q16;
        int32_t duration_d0_q16;
        int32_t duration_d1_q16;
        int64_t result;

        if (!pf_m4_hsd_q16_divide(
                frame_q16 - t0,
                t1 - t0,
                &normalized_q16))
        {
            return p0;
        }
        normalized2_q16 =
            pf_m4_hsd_q16_multiply(normalized_q16, normalized_q16);
        normalized3_q16 =
            pf_m4_hsd_q16_multiply(normalized2_q16, normalized_q16);
        h00_q16 = INT32_C(2) * normalized3_q16 -
                  INT32_C(3) * normalized2_q16 + PF_HSD_Q16_ONE;
        h10_q16 = normalized3_q16 - INT32_C(2) * normalized2_q16 +
                  normalized_q16;
        h01_q16 = -INT32_C(2) * normalized3_q16 +
                  INT32_C(3) * normalized2_q16;
        h11_q16 = normalized3_q16 - normalized2_q16;
        duration_d0_q16 = pf_m4_hsd_q16_multiply(t1 - t0, d0);
        duration_d1_q16 = pf_m4_hsd_q16_multiply(t1 - t0, d1);
        result = (int64_t)pf_m4_hsd_q16_multiply(h00_q16, p0) +
                 (int64_t)pf_m4_hsd_q16_multiply(h10_q16, duration_d0_q16) +
                 (int64_t)pf_m4_hsd_q16_multiply(h01_q16, p1) +
                 (int64_t)pf_m4_hsd_q16_multiply(h11_q16, duration_d1_q16);
        return result < (int64_t)INT32_MIN
                   ? INT32_MIN
                   : result > (int64_t)INT32_MAX
                         ? INT32_MAX
                         : (int32_t)result;
    }
}

static int pf_m4_hsd_make_local_matrix(
    const pf_m4_hsd_local_pose *pose,
    const int32_t *parent_scale_q16,
    pf_m4_hsd_matrix *out_matrix)
{
    int32_t basis_q16[3][3];
    int32_t scale_x2;
    int32_t scale_y2;
    int32_t scale_z2;
    int32_t scale_x1;
    int32_t scale_y1;
    int32_t scale_z1;
    int32_t scale_x;
    int32_t scale_y;
    int32_t scale_z;
    int32_t ratio;

    if (pose == NULL || out_matrix == NULL)
    {
        return 0;
    }
    scale_x2 = scale_x1 = scale_x = pose->scale_q16[0];
    scale_y2 = scale_y1 = scale_y = pose->scale_q16[1];
    scale_z2 = scale_z1 = scale_z = pose->scale_q16[2];
    if (pose->use_quaternion != UINT8_C(0))
    {
        int32_t quaternion_q16[4];
        int32_t xx;
        int32_t xy;
        int32_t xz;
        int32_t xw;
        int32_t yy;
        int32_t yz;
        int32_t yw;
        int32_t zz;
        int32_t zw;

        if (!pf_m4_hsd_pose_quaternion_q16(pose, quaternion_q16))
        {
            return 0;
        }
        xx = pf_m4_hsd_q16_multiply(quaternion_q16[0], quaternion_q16[0]);
        xy = pf_m4_hsd_q16_multiply(quaternion_q16[0], quaternion_q16[1]);
        xz = pf_m4_hsd_q16_multiply(quaternion_q16[0], quaternion_q16[2]);
        xw = pf_m4_hsd_q16_multiply(quaternion_q16[0], quaternion_q16[3]);
        yy = pf_m4_hsd_q16_multiply(quaternion_q16[1], quaternion_q16[1]);
        yz = pf_m4_hsd_q16_multiply(quaternion_q16[1], quaternion_q16[2]);
        yw = pf_m4_hsd_q16_multiply(quaternion_q16[1], quaternion_q16[3]);
        zz = pf_m4_hsd_q16_multiply(quaternion_q16[2], quaternion_q16[2]);
        zw = pf_m4_hsd_q16_multiply(quaternion_q16[2], quaternion_q16[3]);
        basis_q16[0][0] = PF_HSD_Q16_ONE - INT32_C(2) * (yy + zz);
        basis_q16[0][1] = INT32_C(2) * (xy - zw);
        basis_q16[0][2] = INT32_C(2) * (xz + yw);
        basis_q16[1][0] = INT32_C(2) * (xy + zw);
        basis_q16[1][1] = PF_HSD_Q16_ONE - INT32_C(2) * (xx + zz);
        basis_q16[1][2] = INT32_C(2) * (yz - xw);
        basis_q16[2][0] = INT32_C(2) * (xz - yw);
        basis_q16[2][1] = INT32_C(2) * (yz + xw);
        basis_q16[2][2] = PF_HSD_Q16_ONE - INT32_C(2) * (xx + yy);
    }
    else
    {
        const int32_t sin_x = pf_m4_hsd_sine_q16(pose->rotation_q16[0]);
        const int32_t cos_x = pf_m4_hsd_cosine_q16(pose->rotation_q16[0]);
        const int32_t sin_y = pf_m4_hsd_sine_q16(pose->rotation_q16[1]);
        const int32_t cos_y = pf_m4_hsd_cosine_q16(pose->rotation_q16[1]);
        const int32_t sin_z = pf_m4_hsd_sine_q16(pose->rotation_q16[2]);
        const int32_t cos_z = pf_m4_hsd_cosine_q16(pose->rotation_q16[2]);
        const int32_t xy = pf_m4_hsd_q16_multiply(sin_x, sin_y);
        const int32_t cy = pf_m4_hsd_q16_multiply(cos_x, sin_y);

        basis_q16[0][0] = pf_m4_hsd_q16_multiply(cos_z, cos_y);
        basis_q16[1][0] = pf_m4_hsd_q16_multiply(sin_z, cos_y);
        basis_q16[2][0] = -sin_y;
        basis_q16[0][1] =
            pf_m4_hsd_q16_multiply(cos_z, xy) -
            pf_m4_hsd_q16_multiply(cos_x, sin_z);
        basis_q16[1][1] =
            pf_m4_hsd_q16_multiply(sin_z, xy) +
            pf_m4_hsd_q16_multiply(cos_x, cos_z);
        basis_q16[2][1] = pf_m4_hsd_q16_multiply(cos_y, sin_x);
        basis_q16[0][2] =
            pf_m4_hsd_q16_multiply(cos_z, cy) +
            pf_m4_hsd_q16_multiply(sin_x, sin_z);
        basis_q16[1][2] =
            pf_m4_hsd_q16_multiply(sin_z, cy) -
            pf_m4_hsd_q16_multiply(sin_x, cos_z);
        basis_q16[2][2] = pf_m4_hsd_q16_multiply(cos_y, cos_x);
    }
    if (parent_scale_q16 != NULL)
    {
#define PF_HSD_APPLY_SCALE_RATIO(destination, numerator, denominator) \
        do                                                               \
        {                                                                \
            if (!pf_m4_hsd_q16_divide(                                  \
                    parent_scale_q16[(numerator)],                       \
                    parent_scale_q16[(denominator)],                     \
                    &ratio))                                             \
            {                                                            \
                return 0;                                                \
            }                                                            \
            (destination) = pf_m4_hsd_q16_multiply((destination), ratio);\
        } while (0)
        PF_HSD_APPLY_SCALE_RATIO(scale_y2, 1, 0);
        PF_HSD_APPLY_SCALE_RATIO(scale_z2, 2, 0);
        PF_HSD_APPLY_SCALE_RATIO(scale_x1, 0, 1);
        PF_HSD_APPLY_SCALE_RATIO(scale_z1, 2, 1);
        PF_HSD_APPLY_SCALE_RATIO(scale_x, 0, 2);
        PF_HSD_APPLY_SCALE_RATIO(scale_y, 1, 2);
#undef PF_HSD_APPLY_SCALE_RATIO
    }
    out_matrix->value[0][0] =
        pf_m4_hsd_q16_multiply(scale_x2, basis_q16[0][0]);
    out_matrix->value[1][0] =
        pf_m4_hsd_q16_multiply(scale_x1, basis_q16[1][0]);
    out_matrix->value[2][0] =
        pf_m4_hsd_q16_multiply(scale_x, basis_q16[2][0]);
    out_matrix->value[0][1] =
        pf_m4_hsd_q16_multiply(scale_y2, basis_q16[0][1]);
    out_matrix->value[1][1] =
        pf_m4_hsd_q16_multiply(scale_y1, basis_q16[1][1]);
    out_matrix->value[2][1] =
        pf_m4_hsd_q16_multiply(scale_y, basis_q16[2][1]);
    out_matrix->value[0][2] =
        pf_m4_hsd_q16_multiply(scale_z2, basis_q16[0][2]);
    out_matrix->value[1][2] =
        pf_m4_hsd_q16_multiply(scale_z1, basis_q16[1][2]);
    out_matrix->value[2][2] =
        pf_m4_hsd_q16_multiply(scale_z, basis_q16[2][2]);
    out_matrix->value[0][3] = pose->translation_q16[0];
    out_matrix->value[1][3] = pose->translation_q16[1];
    out_matrix->value[2][3] = pose->translation_q16[2];
    return 1;
}

static int pf_m4_hsd_concat_matrix(
    const pf_m4_hsd_matrix *left,
    const pf_m4_hsd_matrix *right,
    pf_m4_hsd_matrix *out_matrix)
{
    pf_m4_hsd_matrix result = {{{INT32_C(0)}}};
    uint8_t row;
    uint8_t column;

    for (row = UINT8_C(0); row < UINT8_C(3); ++row)
    {
        for (column = UINT8_C(0); column < UINT8_C(4); ++column)
        {
            int64_t value = column == UINT8_C(3)
                                ? (int64_t)left->value[row][3]
                                : INT64_C(0);
            uint8_t inner;

            for (inner = UINT8_C(0); inner < UINT8_C(3); ++inner)
            {
                value += pf_m4_hsd_q16_multiply(
                    left->value[row][inner],
                    right->value[inner][column]);
            }
            if (value < (int64_t)INT32_MIN || value > (int64_t)INT32_MAX)
            {
                return 0;
            }
            result.value[row][column] = (int32_t)value;
        }
    }
    *out_matrix = result;
    return 1;
}

static int pf_m4_hsd_transform_point(
    const pf_m4_hsd_matrix *matrix,
    const int32_t point_q16[3],
    int32_t out_point_q16[3])
{
    uint8_t row;

    for (row = UINT8_C(0); row < UINT8_C(3); ++row)
    {
        int64_t value = matrix->value[row][3];
        uint8_t column;

        for (column = UINT8_C(0); column < UINT8_C(3); ++column)
        {
            value += pf_m4_hsd_q16_multiply(
                matrix->value[row][column],
                point_q16[column]);
        }
        if (value < (int64_t)INT32_MIN || value > (int64_t)INT32_MAX)
        {
            return 0;
        }
        out_point_q16[row] = (int32_t)value;
    }
    return 1;
}

static int32_t pf_m4_hsd_source_scale_to_sim_q16(
    const pf_m4_hsd_pose_data *data,
    int32_t value_q16)
{
    return (int32_t)pf_m4_hsd_round_divide(
        (int64_t)value_q16 * (int64_t)data->source_to_sim_numerator,
        (int64_t)data->source_to_sim_denominator);
}

static int32_t pf_m4_hsd_source_coordinate_to_sim_q16(
    const pf_m4_hsd_pose_data *data,
    int32_t value_q16,
    uint8_t axis)
{
    return pf_m4_hsd_source_scale_to_sim_q16(data, value_q16) *
           (int32_t)data->axis_sign[axis];
}

int pf_m4_hsd_evaluate_local_pose_q16(
    const pf_m4_hsd_pose_data *data,
    uint16_t source_submotion,
    int32_t frame_q16,
    pf_m4_hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    const pf_m4_hsd_motion *motion = NULL;
    uint8_t motion_index;
    uint8_t joint_index;
    uint16_t track_index;
    if (data == NULL || out_pose == NULL ||
        data->joints == NULL || data->motions == NULL ||
        data->tracks == NULL || data->keys == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        data->source_to_sim_numerator <= INT32_C(0) ||
        data->source_to_sim_denominator <= INT32_C(0) ||
        (data->axis_sign[0] != INT8_C(-1) &&
         data->axis_sign[0] != INT8_C(1)) ||
        (data->axis_sign[1] != INT8_C(-1) &&
         data->axis_sign[1] != INT8_C(1)) ||
        (data->axis_sign[2] != INT8_C(-1) &&
         data->axis_sign[2] != INT8_C(1)) ||
        frame_q16 < INT32_C(0))
    {
        return 0;
    }
    for (motion_index = UINT8_C(0);
         motion_index < data->motion_count;
         ++motion_index)
    {
        if (data->motions[motion_index].source_submotion == source_submotion)
        {
            motion = &data->motions[motion_index];
            break;
        }
    }
    if (motion == NULL || motion->track_offset > data->track_count ||
        motion->track_count > data->track_count - motion->track_offset)
    {
        return 0;
    }
    for (joint_index = UINT8_C(0);
         joint_index < data->joint_count;
         ++joint_index)
    {
        (void)memset(&out_pose[joint_index], 0, sizeof(out_pose[joint_index]));
        (void)memcpy(
            out_pose[joint_index].rotation_q16,
            data->joints[joint_index].rotation_turns_q16,
            sizeof(data->joints[joint_index].rotation_turns_q16));
        (void)memcpy(
            out_pose[joint_index].scale_q16,
            data->joints[joint_index].scale_q16,
            sizeof(out_pose[joint_index].scale_q16));
        (void)memcpy(
            out_pose[joint_index].translation_q16,
            data->joints[joint_index].translation_q16,
            sizeof(out_pose[joint_index].translation_q16));
    }
    for (track_index = motion->track_offset;
         track_index < motion->track_offset + motion->track_count;
         ++track_index)
    {
        const pf_m4_hsd_track *track = &data->tracks[track_index];
        const pf_m4_hsd_key *keys;
        int32_t value_q16;
        int32_t *destination;

        if (track->joint_index >= data->joint_count ||
            track->key_offset > data->key_count ||
            track->key_count > data->key_count - track->key_offset)
        {
            return 0;
        }
        keys = &data->keys[track->key_offset];
        value_q16 = pf_m4_hsd_sample_track_q16(track, keys, frame_q16);
        if (track->track_type >= UINT8_C(1) &&
            track->track_type <= UINT8_C(3))
        {
            destination = &out_pose[track->joint_index].rotation_q16
                                             [track->track_type - UINT8_C(1)];
        }
        else if (track->track_type >= UINT8_C(5) &&
                 track->track_type <= UINT8_C(7))
        {
            destination = &out_pose[track->joint_index].translation_q16
                                             [track->track_type - UINT8_C(5)];
        }
        else if (track->track_type >= UINT8_C(8) &&
                 track->track_type <= UINT8_C(10))
        {
            destination = &out_pose[track->joint_index].scale_q16
                                             [track->track_type - UINT8_C(8)];
        }
        else
        {
            return 0;
        }
        *destination = value_q16;
    }
    return 1;
}

int pf_m4_hsd_blend_local_pose_q16(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS],
    const pf_m4_hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS],
    int32_t current_weight_q16,
    pf_m4_hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    uint8_t joint_index;

    if (data == NULL || target == NULL || current == NULL || out_pose == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        current_weight_q16 < INT32_C(0) ||
        current_weight_q16 > PF_HSD_Q16_ONE)
    {
        return 0;
    }
    for (joint_index = UINT8_C(0);
         joint_index < data->joint_count;
         ++joint_index)
    {
        int32_t target_quaternion_q16[4];
        int32_t current_quaternion_q16[4];
        int copy_target = joint_index == UINT8_C(0);
        uint8_t copy_index;
        uint8_t axis;

        out_pose[joint_index] = target[joint_index];
        for (copy_index = UINT8_C(0);
             copy_index < data->copy_target_joint_count;
             ++copy_index)
        {
            copy_target |=
                data->copy_target_joint_indices[copy_index] == joint_index;
        }
        if (copy_target != 0)
        {
            continue;
        }
        if (target[joint_index].use_quaternion == UINT8_C(0) &&
            current[joint_index].use_quaternion == UINT8_C(0) &&
            memcmp(
                target[joint_index].rotation_q16,
                current[joint_index].rotation_q16,
                sizeof(target[joint_index].rotation_q16)) == 0)
        {
            (void)memcpy(
                out_pose[joint_index].rotation_q16,
                target[joint_index].rotation_q16,
                sizeof(out_pose[joint_index].rotation_q16));
            out_pose[joint_index].use_quaternion = UINT8_C(0);
        }
        else if (!pf_m4_hsd_pose_quaternion_q16(
                     &target[joint_index], target_quaternion_q16) ||
                 !pf_m4_hsd_pose_quaternion_q16(
                     &current[joint_index], current_quaternion_q16) ||
                 !pf_m4_hsd_slerp_q16(
                     target_quaternion_q16,
                     current_quaternion_q16,
                     current_weight_q16,
                     out_pose[joint_index].rotation_q16))
        {
            return 0;
        }
        else
        {
            out_pose[joint_index].use_quaternion = UINT8_C(1);
        }
        for (axis = UINT8_C(0); axis < UINT8_C(3); ++axis)
        {
            out_pose[joint_index].translation_q16[axis] =
                pf_m4_hsd_q16_multiply(
                    PF_HSD_Q16_ONE - current_weight_q16,
                    target[joint_index].translation_q16[axis]) +
                pf_m4_hsd_q16_multiply(
                    current_weight_q16,
                    current[joint_index].translation_q16[axis]);
            out_pose[joint_index].scale_q16[axis] =
                pf_m4_hsd_q16_multiply(
                    PF_HSD_Q16_ONE - current_weight_q16,
                    target[joint_index].scale_q16[axis]) +
                pf_m4_hsd_q16_multiply(
                    current_weight_q16,
                    current[joint_index].scale_q16[axis]);
        }
    }
    return 1;
}

int pf_m4_hsd_pack_compact_pose_q16(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    pf_m4_hsd_compact_pose *out_compact)
{
    uint8_t index;

    if (data == NULL || pose == NULL || out_compact == NULL ||
        data->rotation_joint_indices == NULL ||
        data->translation_joint_indices == NULL ||
        data->rotation_joint_count > PF_M4_HSD_COMPACT_ROTATION_CAPACITY ||
        data->translation_joint_count >
            PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY)
    {
        return 0;
    }
    (void)memset(out_compact, 0, sizeof(*out_compact));
    for (index = UINT8_C(0);
         index < data->rotation_joint_count;
         ++index)
    {
        const uint8_t joint_index = data->rotation_joint_indices[index];
        int32_t quaternion_q16[4];
        uint8_t component;

        if (joint_index >= data->joint_count ||
            !pf_m4_hsd_pose_quaternion_q16(
                &pose[joint_index], quaternion_q16))
        {
            return 0;
        }
        if (quaternion_q16[3] < INT32_C(0))
        {
            for (component = UINT8_C(0); component < UINT8_C(4); ++component)
            {
                quaternion_q16[component] = -quaternion_q16[component];
            }
        }
        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            int64_t value = pf_m4_hsd_round_divide(
                (int64_t)quaternion_q16[component] * INT64_C(32767),
                INT64_C(65536));

            if (value < (int64_t)INT16_MIN)
            {
                value = INT16_MIN;
            }
            else if (value > (int64_t)INT16_MAX)
            {
                value = INT16_MAX;
            }
            out_compact->rotation_q15[index][component] = (int16_t)value;
        }
    }
    for (index = UINT8_C(0);
         index < data->translation_joint_count;
         ++index)
    {
        const uint8_t joint_index = data->translation_joint_indices[index];

        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        (void)memcpy(
            out_compact->translation_q16[index],
            pose[joint_index].translation_q16,
            sizeof(out_compact->translation_q16[index]));
    }
    return 1;
}

int pf_m4_hsd_inflate_compact_pose_q16(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS],
    const pf_m4_hsd_compact_pose *compact,
    pf_m4_hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    uint8_t index;

    if (data == NULL || target == NULL || compact == NULL ||
        out_pose == NULL || data->rotation_joint_indices == NULL ||
        data->translation_joint_indices == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        data->rotation_joint_count > PF_M4_HSD_COMPACT_ROTATION_CAPACITY ||
        data->translation_joint_count >
            PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY ||
        compact->mode != (uint8_t)PF_M4_HSD_COMPACT_POSE_PACKED)
    {
        return 0;
    }
    (void)memcpy(
        out_pose,
        target,
        sizeof(*out_pose) * data->joint_count);
    for (index = UINT8_C(0);
         index < data->rotation_joint_count;
         ++index)
    {
        const uint8_t joint_index = data->rotation_joint_indices[index];
        uint64_t vector_squared = UINT64_C(0);
        uint8_t component;

        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            const int32_t value_q16 = (int32_t)pf_m4_hsd_round_divide(
                (int64_t)compact->rotation_q15[index][component] *
                    INT64_C(65536),
                INT64_C(32767));

            out_pose[joint_index].rotation_q16[component] = value_q16;
            vector_squared +=
                (uint64_t)((int64_t)value_q16 * (int64_t)value_q16);
        }
        out_pose[joint_index].rotation_q16[3] =
            vector_squared >= (UINT64_C(1) << 32U)
                ? INT32_C(0)
                : (int32_t)pf_m4_u64_sqrt(
                      (UINT64_C(1) << 32U) - vector_squared);
        if (!pf_m4_hsd_normalize_quaternion_q16(
                out_pose[joint_index].rotation_q16))
        {
            return 0;
        }
        out_pose[joint_index].use_quaternion = UINT8_C(1);
    }
    for (index = UINT8_C(0);
         index < data->translation_joint_count;
         ++index)
    {
        const uint8_t joint_index = data->translation_joint_indices[index];

        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        (void)memcpy(
            out_pose[joint_index].translation_q16,
            compact->translation_q16[index],
            sizeof(out_pose[joint_index].translation_q16));
    }
    return 1;
}

int pf_m4_hsd_resolve_compact_pose_q16(
    const pf_m4_hsd_pose_data *data,
    uint16_t target_submotion,
    int32_t target_frame_q16,
    int32_t progress_q16,
    const pf_m4_hsd_compact_pose *compact,
    pf_m4_hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];

    if (data == NULL || compact == NULL || out_pose == NULL ||
        !pf_m4_hsd_evaluate_local_pose_q16(
            data, target_submotion, target_frame_q16, target))
    {
        return 0;
    }
    if (compact->mode == (uint8_t)PF_M4_HSD_COMPACT_POSE_PACKED)
    {
        return pf_m4_hsd_inflate_compact_pose_q16(
            data, target, compact, out_pose);
    }
    if (compact->mode == (uint8_t)PF_M4_HSD_COMPACT_POSE_REPLAY)
    {
        pf_m4_hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_local_pose next[PF_M4_HSD_POSE_MAX_JOINTS];
        const int32_t step_q16 = compact->replay.target_step_q16;
        const int32_t blend_frames_q16 =
            compact->replay.blend_frames_q16;
        int32_t old_progress_q16 = INT32_C(0);
        int32_t step_progress_q16;

        if (step_q16 != PF_HSD_Q16_ONE ||
            blend_frames_q16 != INT32_C(6) * PF_HSD_Q16_ONE ||
            progress_q16 <= INT32_C(0) ||
            progress_q16 >= blend_frames_q16 ||
            progress_q16 % step_q16 != INT32_C(0) ||
            (int64_t)target_frame_q16 !=
                (int64_t)compact->replay.target_entry_frame_q16 +
                    (int64_t)(progress_q16 / step_q16 - INT32_C(1)) *
                        step_q16 ||
            !pf_m4_hsd_evaluate_local_pose_q16(
                data,
                compact->replay.source_submotion,
                compact->replay.source_frame_q16,
                current))
        {
            return 0;
        }
        for (step_progress_q16 = step_q16;
             step_progress_q16 <= progress_q16;
             step_progress_q16 += step_q16)
        {
            const int32_t remaining_q16 =
                blend_frames_q16 - old_progress_q16;
            const int32_t current_weight_q16 =
                (int32_t)(
                    ((int64_t)(blend_frames_q16 - step_progress_q16) *
                         PF_HSD_Q16_ONE +
                     remaining_q16 / INT32_C(2)) /
                    remaining_q16);
            const int32_t step_frame_q16 =
                compact->replay.target_entry_frame_q16 +
                (step_progress_q16 / step_q16 - INT32_C(1)) * step_q16;

            if (!pf_m4_hsd_evaluate_local_pose_q16(
                    data, target_submotion, step_frame_q16, target) ||
                !pf_m4_hsd_blend_local_pose_q16(
                    data, target, current, current_weight_q16, next))
            {
                return 0;
            }
            (void)memcpy(
                current, next, sizeof(*next) * data->joint_count);
            old_progress_q16 = step_progress_q16;
        }
        (void)memcpy(
            out_pose, current, sizeof(*current) * data->joint_count);
        return 1;
    }
    return 0;
}

static int pf_m4_hsd_evaluate_joint_matrices_from_local_pose(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    pf_m4_hsd_matrix matrices[PF_M4_HSD_POSE_MAX_JOINTS])
{
    int32_t cumulative_scale_q16[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t has_cumulative_scale[PF_M4_HSD_POSE_MAX_JOINTS] = {UINT8_C(0)};
    uint8_t joint_index;

    if (data == NULL || pose == NULL || matrices == NULL ||
        data->joints == NULL || data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS)
    {
        return 0;
    }
    for (joint_index = UINT8_C(0);
         joint_index < data->joint_count;
         ++joint_index)
    {
        const pf_m4_hsd_joint *joint = &data->joints[joint_index];
        const int parent = joint->parent_index;
        pf_m4_hsd_matrix local;
        const int32_t *parent_scale =
            parent >= 0 && has_cumulative_scale[parent] != UINT8_C(0)
                ? cumulative_scale_q16[parent]
                : NULL;
        uint8_t axis;

        if (parent >= (int)joint_index || parent < -1 ||
            !pf_m4_hsd_make_local_matrix(
                &pose[joint_index], parent_scale, &local))
        {
            return 0;
        }
        if (parent >= 0)
        {
            if (!pf_m4_hsd_concat_matrix(
                    &matrices[parent], &local, &matrices[joint_index]))
            {
                return 0;
            }
        }
        else
        {
            matrices[joint_index] = local;
        }
        if (joint->classical_scale != UINT8_C(0))
        {
            has_cumulative_scale[joint_index] =
                parent >= 0 ? has_cumulative_scale[parent] : UINT8_C(0);
            if (has_cumulative_scale[joint_index] != UINT8_C(0))
            {
                (void)memcpy(
                    cumulative_scale_q16[joint_index],
                    cumulative_scale_q16[parent],
                    sizeof(cumulative_scale_q16[joint_index]));
            }
        }
        else
        {
            has_cumulative_scale[joint_index] = UINT8_C(1);
            for (axis = UINT8_C(0); axis < UINT8_C(3); ++axis)
            {
                cumulative_scale_q16[joint_index][axis] =
                    parent_scale == NULL
                        ? pose[joint_index].scale_q16[axis]
                        : pf_m4_hsd_q16_multiply(
                              pose[joint_index].scale_q16[axis],
                              parent_scale[axis]);
            }
        }
    }
    return 1;
}

int pf_m4_hsd_evaluate_joint_origins_source_q16(
    const pf_m4_hsd_pose_data *data,
    uint16_t source_submotion,
    int32_t frame_q16,
    const uint8_t *joint_indices,
    uint8_t joint_count,
    int32_t out_origins_q16[PF_M4_HSD_POSE_MAX_JOINTS][3])
{
    pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];

    if (!pf_m4_hsd_evaluate_local_pose_q16(
            data, source_submotion, frame_q16, pose))
    {
        return 0;
    }
    return pf_m4_hsd_evaluate_joint_origins_from_local_pose_q16(
        data, pose, joint_indices, joint_count, out_origins_q16);
}

int pf_m4_hsd_evaluate_joint_origins_from_local_pose_q16(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    const uint8_t *joint_indices,
    uint8_t joint_count,
    int32_t out_origins_q16[PF_M4_HSD_POSE_MAX_JOINTS][3])
{
    pf_m4_hsd_matrix matrices[PF_M4_HSD_POSE_MAX_JOINTS];
    uint8_t output_index;

    if (joint_indices == NULL || out_origins_q16 == NULL ||
        joint_count == UINT8_C(0) ||
        joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        !pf_m4_hsd_evaluate_joint_matrices_from_local_pose(
            data, pose, matrices))
    {
        return 0;
    }
    for (output_index = UINT8_C(0);
         output_index < joint_count;
         ++output_index)
    {
        const uint8_t joint_index = joint_indices[output_index];

        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        out_origins_q16[output_index][0] = matrices[joint_index].value[0][3];
        out_origins_q16[output_index][1] = matrices[joint_index].value[1][3];
        out_origins_q16[output_index][2] = matrices[joint_index].value[2][3];
    }
    return 1;
}

int pf_m4_hsd_evaluate_hurt_pose(
    const pf_m4_hsd_pose_data *data,
    uint16_t source_submotion,
    int32_t frame_q16,
    pf_m4_hsd_evaluated_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];

    if (!pf_m4_hsd_evaluate_local_pose_q16(
            data, source_submotion, frame_q16, pose))
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return 0;
    }
    return pf_m4_hsd_evaluate_hurt_pose_from_local_pose(
        data, pose, out_capsules, out_count);
}

int pf_m4_hsd_evaluate_hurt_pose_from_local_pose(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    pf_m4_hsd_evaluated_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    pf_m4_hsd_matrix matrices[PF_M4_HSD_POSE_MAX_JOINTS];
    uint8_t capsule_index;

    if (out_count != NULL)
    {
        *out_count = UINT8_C(0);
    }
    if (data == NULL || out_capsules == NULL || out_count == NULL ||
        data->capsules == NULL || data->capsule_count == UINT8_C(0) ||
        data->capsule_count > PF_M4_HSD_POSE_MAX_CAPSULES ||
        !pf_m4_hsd_evaluate_joint_matrices_from_local_pose(
            data, pose, matrices))
    {
        return 0;
    }
    for (capsule_index = UINT8_C(0);
         capsule_index < data->capsule_count;
         ++capsule_index)
    {
        const pf_m4_hsd_hurt_capsule *source =
            &data->capsules[capsule_index];
        pf_m4_hsd_evaluated_capsule *destination =
            &out_capsules[capsule_index];
        int32_t endpoint_a_q16[3];
        int32_t endpoint_b_q16[3];
        uint8_t axis;

        if (source->joint_index >= data->joint_count ||
            !pf_m4_hsd_transform_point(
                &matrices[source->joint_index],
                source->offset_a_q16,
                endpoint_a_q16) ||
            !pf_m4_hsd_transform_point(
                &matrices[source->joint_index],
                source->offset_b_q16,
                endpoint_b_q16))
        {
            return 0;
        }
        for (axis = UINT8_C(0); axis < UINT8_C(3); ++axis)
        {
            destination->endpoint_a_q16[axis] =
                pf_m4_hsd_source_coordinate_to_sim_q16(
                    data,
                    endpoint_a_q16[axis],
                    axis);
            destination->endpoint_b_q16[axis] =
                pf_m4_hsd_source_coordinate_to_sim_q16(
                    data,
                    endpoint_b_q16[axis],
                    axis);
        }
        destination->radius_q16 =
            pf_m4_hsd_source_scale_to_sim_q16(
                data,
                source->radius_q16);
        destination->hurtbox_id = source->hurtbox_id;
        destination->height = source->height;
        destination->grabbable = source->grabbable;
        destination->reserved = UINT8_C(0);
    }
    *out_count = data->capsule_count;
    return 1;
}
