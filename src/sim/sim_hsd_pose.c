#include "sim_hsd_pose.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define PF_HSD_PI_F 3.14159265358979323846f
#define PF_HSD_TWO_PI_F (2.0f * PF_HSD_PI_F)

typedef struct hsd_matrix
{
    float value[3][4];
} hsd_matrix;

static int hsd_finite(float value)
{
    return isfinite(value) != 0;
}

static float hsd_clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static float hsd_sine_f32(float turns_f32)
{
    return sinf(turns_f32 * PF_HSD_TWO_PI_F);
}

static float hsd_cosine_f32(float turns_f32)
{
    return cosf(turns_f32 * PF_HSD_TWO_PI_F);
}

static int hsd_normalize_quaternion_f32(float quaternion_f32[4])
{
    float magnitude_squared = 0.0f;
    float inverse_magnitude;
    uint8_t component;

    if (quaternion_f32 == NULL)
    {
        return 0;
    }
    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        magnitude_squared += quaternion_f32[component] * quaternion_f32[component];
    }
    if (!(magnitude_squared > 0.0f) || !hsd_finite(magnitude_squared))
    {
        return 0;
    }
    inverse_magnitude = 1.0f / sqrtf(magnitude_squared);
    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        quaternion_f32[component] *= inverse_magnitude;
    }
    return 1;
}

static int hsd_euler_to_quaternion_f32(
    const float rotation_turns_f32[3],
    float out_quaternion_f32[4])
{
    const float half_x = rotation_turns_f32[0] * 0.5f;
    const float half_y = rotation_turns_f32[1] * 0.5f;
    const float half_z = rotation_turns_f32[2] * 0.5f;
    const float sx = hsd_sine_f32(half_x);
    const float sy = hsd_sine_f32(half_y);
    const float sz = hsd_sine_f32(half_z);
    const float cx = hsd_cosine_f32(half_x);
    const float cy = hsd_cosine_f32(half_y);
    const float cz = hsd_cosine_f32(half_z);

    out_quaternion_f32[0] = sx * cy * cz - cx * sy * sz;
    out_quaternion_f32[1] = cx * sy * cz + sx * cy * sz;
    out_quaternion_f32[2] = cx * cy * sz - sx * sy * cz;
    out_quaternion_f32[3] = cx * cy * cz + sx * sy * sz;
    return hsd_normalize_quaternion_f32(out_quaternion_f32);
}

static int hsd_pose_quaternion_f32(
    const hsd_local_pose *pose,
    float out_quaternion_f32[4])
{
    if (pose == NULL || out_quaternion_f32 == NULL)
    {
        return 0;
    }
    if (pose->use_quaternion != UINT8_C(0))
    {
        (void)memcpy(
            out_quaternion_f32,
            pose->rotation_f32,
            sizeof(float) * 4U);
        return hsd_normalize_quaternion_f32(out_quaternion_f32);
    }
    return hsd_euler_to_quaternion_f32(
        pose->rotation_f32,
        out_quaternion_f32);
}

static int hsd_slerp_f32(
    const float target_f32[4],
    const float current_f32[4],
    float current_weight_f32,
    float out_quaternion_f32[4])
{
    float target[4];
    float current[4];
    float dot = 0.0f;
    float target_weight;
    float current_weight;
    uint8_t component;

    if (target_f32 == NULL || current_f32 == NULL ||
        out_quaternion_f32 == NULL || current_weight_f32 < 0.0f ||
        current_weight_f32 > 1.0f)
    {
        return 0;
    }
    (void)memcpy(target, target_f32, sizeof(target));
    (void)memcpy(current, current_f32, sizeof(current));
    if (!hsd_normalize_quaternion_f32(target) ||
        !hsd_normalize_quaternion_f32(current))
    {
        return 0;
    }
    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        dot += target[component] * current[component];
    }
    if (dot < 0.0f)
    {
        dot = -dot;
        for (component = UINT8_C(0); component < UINT8_C(4); ++component)
        {
            current[component] = -current[component];
        }
    }
    dot = hsd_clamp(dot, 0.0f, 1.0f);
    if (1.0f - dot <= 0.0001220703125f)
    {
        target_weight = 1.0f - current_weight_f32;
        current_weight = current_weight_f32;
    }
    else
    {
        const float theta = acosf(dot);
        const float sine_theta = sinf(theta);

        if (!(sine_theta > 0.0f))
        {
            return 0;
        }
        target_weight = sinf((1.0f - current_weight_f32) * theta) /
                        sine_theta;
        current_weight = sinf(current_weight_f32 * theta) / sine_theta;
    }
    for (component = UINT8_C(0); component < UINT8_C(4); ++component)
    {
        out_quaternion_f32[component] =
            target_weight * target[component] +
            current_weight * current[component];
    }
    return hsd_normalize_quaternion_f32(out_quaternion_f32);
}

static float hsd_sample_track_f32(
    const hsd_track *track,
    const hsd_key *keys,
    float frame_f32)
{
    const float requested_frame = frame_f32 + (float)track->start_frame;
    float p0 = 0.0f;
    float p1 = 0.0f;
    float d0 = 0.0f;
    float d1 = 0.0f;
    float t0 = 0.0f;
    float t1 = 0.0f;
    uint8_t interpolation = PF_M4_HSD_INTERPOLATION_CONSTANT;
    uint8_t previous_interpolation = PF_M4_HSD_INTERPOLATION_CONSTANT;
    uint16_t key_index;

    if (track == NULL || keys == NULL || track->key_count == UINT16_C(0) ||
        !hsd_finite(requested_frame))
    {
        return 0.0f;
    }
    frame_f32 = requested_frame;
    if (track->key_count > UINT16_C(1) &&
        frame_f32 >= keys[track->key_count - UINT16_C(1)].frame_f32)
    {
        return keys[track->key_count - UINT16_C(1)].value_f32;
    }
    for (key_index = UINT16_C(0); key_index < track->key_count; ++key_index)
    {
        const hsd_key *key = &keys[key_index];

        previous_interpolation = interpolation;
        interpolation = key->interpolation;
        switch ((hsd_interpolation)interpolation)
        {
            case PF_M4_HSD_INTERPOLATION_CONSTANT:
            case PF_M4_HSD_INTERPOLATION_LINEAR:
                p0 = p1;
                p1 = key->value_f32;
                if (previous_interpolation != PF_M4_HSD_INTERPOLATION_SLOPE)
                {
                    d0 = d1;
                    d1 = 0.0f;
                }
                t0 = t1;
                t1 = key->frame_f32;
                break;
            case PF_M4_HSD_INTERPOLATION_SPLINE_ZERO:
                p0 = p1;
                d0 = d1;
                p1 = key->value_f32;
                d1 = 0.0f;
                t0 = t1;
                t1 = key->frame_f32;
                break;
            case PF_M4_HSD_INTERPOLATION_SPLINE:
                p0 = p1;
                p1 = key->value_f32;
                d0 = d1;
                d1 = key->tangent_f32;
                t0 = t1;
                t1 = key->frame_f32;
                break;
            case PF_M4_HSD_INTERPOLATION_SLOPE:
                d0 = d1;
                d1 = key->tangent_f32;
                break;
            case PF_M4_HSD_INTERPOLATION_KEY:
                p0 = key->value_f32;
                p1 = key->value_f32;
                break;
            default:
                return 0.0f;
        }
        if (t1 > frame_f32 && interpolation != PF_M4_HSD_INTERPOLATION_SLOPE)
        {
            break;
        }
        previous_interpolation = interpolation;
    }
    if (frame_f32 <= t0)
    {
        return p0;
    }
    if (frame_f32 >= t1)
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
        return p0 + (p1 - p0) * ((frame_f32 - t0) / (t1 - t0));
    }
    {
        const float normalized = (frame_f32 - t0) / (t1 - t0);
        const float normalized2 = normalized * normalized;
        const float normalized3 = normalized2 * normalized;
        const float h00 = 2.0f * normalized3 - 3.0f * normalized2 + 1.0f;
        const float h10 = normalized3 - 2.0f * normalized2 + normalized;
        const float h01 = -2.0f * normalized3 + 3.0f * normalized2;
        const float h11 = normalized3 - normalized2;
        const float duration = t1 - t0;

        return h00 * p0 + h10 * duration * d0 + h01 * p1 +
               h11 * duration * d1;
    }
}

static int hsd_make_local_matrix(
    const hsd_local_pose *pose,
    const float *parent_scale_f32,
    hsd_matrix *out_matrix)
{
    float basis[3][3];
    float scale_x2;
    float scale_y2;
    float scale_z2;
    float scale_x1;
    float scale_y1;
    float scale_z1;
    float scale_x;
    float scale_y;
    float scale_z;

    if (pose == NULL || out_matrix == NULL)
    {
        return 0;
    }
    scale_x2 = scale_x1 = scale_x = pose->scale_f32[0];
    scale_y2 = scale_y1 = scale_y = pose->scale_f32[1];
    scale_z2 = scale_z1 = scale_z = pose->scale_f32[2];
    if (pose->use_quaternion != UINT8_C(0))
    {
        float quaternion[4];
        float xx;
        float xy;
        float xz;
        float xw;
        float yy;
        float yz;
        float yw;
        float zz;
        float zw;

        if (!hsd_pose_quaternion_f32(pose, quaternion))
        {
            return 0;
        }
        xx = quaternion[0] * quaternion[0];
        xy = quaternion[0] * quaternion[1];
        xz = quaternion[0] * quaternion[2];
        xw = quaternion[0] * quaternion[3];
        yy = quaternion[1] * quaternion[1];
        yz = quaternion[1] * quaternion[2];
        yw = quaternion[1] * quaternion[3];
        zz = quaternion[2] * quaternion[2];
        zw = quaternion[2] * quaternion[3];
        basis[0][0] = 1.0f - 2.0f * (yy + zz);
        basis[0][1] = 2.0f * (xy - zw);
        basis[0][2] = 2.0f * (xz + yw);
        basis[1][0] = 2.0f * (xy + zw);
        basis[1][1] = 1.0f - 2.0f * (xx + zz);
        basis[1][2] = 2.0f * (yz - xw);
        basis[2][0] = 2.0f * (xz - yw);
        basis[2][1] = 2.0f * (yz + xw);
        basis[2][2] = 1.0f - 2.0f * (xx + yy);
    }
    else
    {
        const float sin_x = hsd_sine_f32(pose->rotation_f32[0]);
        const float cos_x = hsd_cosine_f32(pose->rotation_f32[0]);
        const float sin_y = hsd_sine_f32(pose->rotation_f32[1]);
        const float cos_y = hsd_cosine_f32(pose->rotation_f32[1]);
        const float sin_z = hsd_sine_f32(pose->rotation_f32[2]);
        const float cos_z = hsd_cosine_f32(pose->rotation_f32[2]);
        const float xy = sin_x * sin_y;
        const float cy = cos_x * sin_y;

        basis[0][0] = cos_z * cos_y;
        basis[1][0] = sin_z * cos_y;
        basis[2][0] = -sin_y;
        basis[0][1] = cos_z * xy - cos_x * sin_z;
        basis[1][1] = sin_z * xy + cos_x * cos_z;
        basis[2][1] = cos_y * sin_x;
        basis[0][2] = cos_z * cy + sin_x * sin_z;
        basis[1][2] = sin_z * cy - sin_x * cos_z;
        basis[2][2] = cos_y * cos_x;
    }
    if (parent_scale_f32 != NULL)
    {
        if (parent_scale_f32[0] == 0.0f || parent_scale_f32[1] == 0.0f ||
            parent_scale_f32[2] == 0.0f)
        {
            return 0;
        }
        scale_y2 *= parent_scale_f32[1] / parent_scale_f32[0];
        scale_z2 *= parent_scale_f32[2] / parent_scale_f32[0];
        scale_x1 *= parent_scale_f32[0] / parent_scale_f32[1];
        scale_z1 *= parent_scale_f32[2] / parent_scale_f32[1];
        scale_x *= parent_scale_f32[0] / parent_scale_f32[2];
        scale_y *= parent_scale_f32[1] / parent_scale_f32[2];
    }
    out_matrix->value[0][0] = scale_x2 * basis[0][0];
    out_matrix->value[1][0] = scale_x1 * basis[1][0];
    out_matrix->value[2][0] = scale_x * basis[2][0];
    out_matrix->value[0][1] = scale_y2 * basis[0][1];
    out_matrix->value[1][1] = scale_y1 * basis[1][1];
    out_matrix->value[2][1] = scale_y * basis[2][1];
    out_matrix->value[0][2] = scale_z2 * basis[0][2];
    out_matrix->value[1][2] = scale_z1 * basis[1][2];
    out_matrix->value[2][2] = scale_z * basis[2][2];
    out_matrix->value[0][3] = pose->translation_f32[0];
    out_matrix->value[1][3] = pose->translation_f32[1];
    out_matrix->value[2][3] = pose->translation_f32[2];
    return 1;
}

static int hsd_concat_matrix(
    const hsd_matrix *left,
    const hsd_matrix *right,
    hsd_matrix *out_matrix)
{
    hsd_matrix result = {{{0.0f}}};
    uint8_t row;
    uint8_t column;

    for (row = UINT8_C(0); row < UINT8_C(3); ++row)
    {
        for (column = UINT8_C(0); column < UINT8_C(4); ++column)
        {
            float value = column == UINT8_C(3) ? left->value[row][3] : 0.0f;
            uint8_t inner;

            for (inner = UINT8_C(0); inner < UINT8_C(3); ++inner)
            {
                value += left->value[row][inner] * right->value[inner][column];
            }
            if (!hsd_finite(value))
            {
                return 0;
            }
            result.value[row][column] = value;
        }
    }
    *out_matrix = result;
    return 1;
}

static int hsd_transform_point(
    const hsd_matrix *matrix,
    const float point_f32[3],
    float out_point_f32[3])
{
    uint8_t row;

    for (row = UINT8_C(0); row < UINT8_C(3); ++row)
    {
        float value = matrix->value[row][3];
        uint8_t column;

        for (column = UINT8_C(0); column < UINT8_C(3); ++column)
        {
            value += matrix->value[row][column] * point_f32[column];
        }
        if (!hsd_finite(value))
        {
            return 0;
        }
        out_point_f32[row] = value;
    }
    return 1;
}

static float hsd_source_scale_to_sim_f32(
    const hsd_pose_data *data,
    float value_f32)
{
    return value_f32 * (float)data->source_to_sim_numerator /
           (float)data->source_to_sim_denominator;
}

static float hsd_source_coordinate_to_sim_f32(
    const hsd_pose_data *data,
    float value_f32,
    uint8_t axis)
{
    return hsd_source_scale_to_sim_f32(data, value_f32) *
           (float)data->axis_sign[axis];
}

int hsd_evaluate_local_pose_f32(
    const hsd_pose_data *data,
    uint16_t source_submotion,
    float frame_f32,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    const hsd_motion *motion = NULL;
    uint8_t motion_index;
    uint8_t joint_index;
    uint16_t track_index;

    if (data == NULL || out_pose == NULL || data->joints == NULL ||
        data->motions == NULL || data->tracks == NULL || data->keys == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        data->source_to_sim_numerator <= INT32_C(0) ||
        data->source_to_sim_denominator <= INT32_C(0) ||
        (data->axis_sign[0] != INT8_C(-1) && data->axis_sign[0] != INT8_C(1)) ||
        (data->axis_sign[1] != INT8_C(-1) && data->axis_sign[1] != INT8_C(1)) ||
        (data->axis_sign[2] != INT8_C(-1) && data->axis_sign[2] != INT8_C(1)) ||
        frame_f32 < 0.0f || !hsd_finite(frame_f32))
    {
        return 0;
    }
    for (motion_index = UINT8_C(0); motion_index < data->motion_count; ++motion_index)
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
    for (joint_index = UINT8_C(0); joint_index < data->joint_count; ++joint_index)
    {
        (void)memset(&out_pose[joint_index], 0, sizeof(out_pose[joint_index]));
        (void)memcpy(out_pose[joint_index].rotation_f32,
                     data->joints[joint_index].rotation_turns_f32,
                     sizeof(data->joints[joint_index].rotation_turns_f32));
        (void)memcpy(out_pose[joint_index].scale_f32,
                     data->joints[joint_index].scale_f32,
                     sizeof(out_pose[joint_index].scale_f32));
        (void)memcpy(out_pose[joint_index].translation_f32,
                     data->joints[joint_index].translation_f32,
                     sizeof(out_pose[joint_index].translation_f32));
    }
    for (track_index = motion->track_offset;
         track_index < motion->track_offset + motion->track_count;
         ++track_index)
    {
        const hsd_track *track = &data->tracks[track_index];
        float *destination;

        if (track->joint_index >= data->joint_count ||
            track->key_offset > data->key_count ||
            track->key_count > data->key_count - track->key_offset)
        {
            return 0;
        }
        if (track->track_type >= UINT8_C(1) && track->track_type <= UINT8_C(3))
        {
            destination = &out_pose[track->joint_index]
                               .rotation_f32[track->track_type - UINT8_C(1)];
        }
        else if (track->track_type >= UINT8_C(5) &&
                 track->track_type <= UINT8_C(7))
        {
            destination = &out_pose[track->joint_index]
                               .translation_f32[track->track_type - UINT8_C(5)];
        }
        else if (track->track_type >= UINT8_C(8) &&
                 track->track_type <= UINT8_C(10))
        {
            destination = &out_pose[track->joint_index]
                               .scale_f32[track->track_type - UINT8_C(8)];
        }
        else
        {
            return 0;
        }
        *destination = hsd_sample_track_f32(
            track, &data->keys[track->key_offset], frame_f32);
    }
    return 1;
}

int hsd_blend_local_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS],
    const hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS],
    float current_weight_f32,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    uint8_t joint_index;

    if (data == NULL || target == NULL || current == NULL || out_pose == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        current_weight_f32 < 0.0f || current_weight_f32 > 1.0f)
    {
        return 0;
    }
    for (joint_index = UINT8_C(0); joint_index < data->joint_count; ++joint_index)
    {
        float target_quaternion[4];
        float current_quaternion[4];
        int copy_target = joint_index == UINT8_C(0);
        uint8_t copy_index;
        uint8_t axis;

        out_pose[joint_index] = target[joint_index];
        for (copy_index = UINT8_C(0);
             copy_index < data->copy_target_joint_count;
             ++copy_index)
        {
            copy_target |= data->copy_target_joint_indices[copy_index] == joint_index;
        }
        if (copy_target != 0)
        {
            continue;
        }
        if (target[joint_index].use_quaternion == UINT8_C(0) &&
            current[joint_index].use_quaternion == UINT8_C(0) &&
            memcmp(target[joint_index].rotation_f32,
                   current[joint_index].rotation_f32,
                   sizeof(target[joint_index].rotation_f32)) == 0)
        {
            (void)memcpy(out_pose[joint_index].rotation_f32,
                         target[joint_index].rotation_f32,
                         sizeof(out_pose[joint_index].rotation_f32));
            out_pose[joint_index].use_quaternion = UINT8_C(0);
        }
        else if (!hsd_pose_quaternion_f32(&target[joint_index], target_quaternion) ||
                 !hsd_pose_quaternion_f32(&current[joint_index], current_quaternion) ||
                 !hsd_slerp_f32(target_quaternion,
                                current_quaternion,
                                current_weight_f32,
                                out_pose[joint_index].rotation_f32))
        {
            return 0;
        }
        else
        {
            out_pose[joint_index].use_quaternion = UINT8_C(1);
        }
        for (axis = UINT8_C(0); axis < UINT8_C(3); ++axis)
        {
            out_pose[joint_index].translation_f32[axis] =
                (1.0f - current_weight_f32) * target[joint_index].translation_f32[axis] +
                current_weight_f32 * current[joint_index].translation_f32[axis];
            out_pose[joint_index].scale_f32[axis] =
                (1.0f - current_weight_f32) * target[joint_index].scale_f32[axis] +
                current_weight_f32 * current[joint_index].scale_f32[axis];
        }
    }
    return 1;
}

static int16_t hsd_float_to_q15(float value)
{
    float scaled = hsd_clamp(value, -1.0f, 1.0f) * 32767.0f;
    int32_t rounded = scaled < 0.0f ? (int32_t)(scaled - 0.5f)
                                    : (int32_t)(scaled + 0.5f);

    if (rounded < INT16_MIN)
    {
        rounded = INT16_MIN;
    }
    if (rounded > INT16_MAX)
    {
        rounded = INT16_MAX;
    }
    return (int16_t)rounded;
}

int hsd_pack_compact_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    hsd_compact_pose *out_compact)
{
    uint8_t index;

    if (data == NULL || pose == NULL || out_compact == NULL ||
        data->rotation_joint_indices == NULL ||
        data->translation_joint_indices == NULL ||
        data->rotation_joint_count > PF_M4_HSD_COMPACT_ROTATION_CAPACITY ||
        data->translation_joint_count > PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY)
    {
        return 0;
    }
    (void)memset(out_compact, 0, sizeof(*out_compact));
    for (index = UINT8_C(0); index < data->rotation_joint_count; ++index)
    {
        const uint8_t joint_index = data->rotation_joint_indices[index];
        float quaternion[4];
        uint8_t component;

        if (joint_index >= data->joint_count ||
            !hsd_pose_quaternion_f32(&pose[joint_index], quaternion))
        {
            return 0;
        }
        if (quaternion[3] < 0.0f)
        {
            for (component = UINT8_C(0); component < UINT8_C(4); ++component)
            {
                quaternion[component] = -quaternion[component];
            }
        }
        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            out_compact->rotation_q15[index][component] =
                hsd_float_to_q15(quaternion[component]);
        }
    }
    for (index = UINT8_C(0); index < data->translation_joint_count; ++index)
    {
        const uint8_t joint_index = data->translation_joint_indices[index];
        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        (void)memcpy(out_compact->translation_f32[index],
                     pose[joint_index].translation_f32,
                     sizeof(out_compact->translation_f32[index]));
    }
    return 1;
}

int hsd_inflate_compact_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose *target,
    const hsd_compact_pose *compact,
    hsd_local_pose *out_pose)
{
    uint8_t index;

    if (data == NULL || target == NULL || compact == NULL || out_pose == NULL ||
        data->rotation_joint_indices == NULL ||
        data->translation_joint_indices == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        data->rotation_joint_count > PF_M4_HSD_COMPACT_ROTATION_CAPACITY ||
        data->translation_joint_count > PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY ||
        compact->mode != (uint8_t)PF_M4_HSD_COMPACT_POSE_PACKED)
    {
        return 0;
    }
    (void)memcpy(out_pose, target, sizeof(*out_pose) * data->joint_count);
    for (index = UINT8_C(0); index < data->rotation_joint_count; ++index)
    {
        const uint8_t joint_index = data->rotation_joint_indices[index];
        float vector_squared = 0.0f;
        uint8_t component;

        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            const float value =
                (float)compact->rotation_q15[index][component] / 32767.0f;
            out_pose[joint_index].rotation_f32[component] = value;
            vector_squared += value * value;
        }
        out_pose[joint_index].rotation_f32[3] =
            vector_squared >= 1.0f ? 0.0f : sqrtf(1.0f - vector_squared);
        if (!hsd_normalize_quaternion_f32(out_pose[joint_index].rotation_f32))
        {
            return 0;
        }
        out_pose[joint_index].use_quaternion = UINT8_C(1);
    }
    for (index = UINT8_C(0); index < data->translation_joint_count; ++index)
    {
        const uint8_t joint_index = data->translation_joint_indices[index];
        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        (void)memcpy(out_pose[joint_index].translation_f32,
                     compact->translation_f32[index],
                     sizeof(out_pose[joint_index].translation_f32));
    }
    return 1;
}

int hsd_resolve_compact_pose_f32(
    const hsd_pose_data *data,
    uint16_t target_submotion,
    float target_frame_f32,
    float progress_f32,
    const hsd_compact_pose *compact,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];

    if (data == NULL || compact == NULL || out_pose == NULL ||
        !hsd_evaluate_local_pose_f32(data, target_submotion, target_frame_f32, target))
    {
        return 0;
    }
    if (compact->mode == (uint8_t)PF_M4_HSD_COMPACT_POSE_PACKED)
    {
        return hsd_inflate_compact_pose_f32(data, target, compact, out_pose);
    }
    if (compact->mode == (uint8_t)PF_M4_HSD_COMPACT_POSE_REPLAY)
    {
        hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS];
        hsd_local_pose next[PF_M4_HSD_POSE_MAX_JOINTS];
        const float step = compact->replay.target_step_f32;
        const float blend_frames = compact->replay.blend_frames_f32;
        float old_progress = 0.0f;
        float step_progress;

        if (step != 1.0f || blend_frames != 6.0f || progress_f32 <= 0.0f ||
            progress_f32 >= blend_frames ||
            progress_f32 != (float)(int32_t)progress_f32 ||
            target_frame_f32 != compact->replay.target_entry_frame_f32 +
                                    (progress_f32 - 1.0f) ||
            !hsd_evaluate_local_pose_f32(data,
                                         compact->replay.source_submotion,
                                         compact->replay.source_frame_f32,
                                         current))
        {
            return 0;
        }
        for (step_progress = step;
             step_progress <= progress_f32;
             step_progress += step)
        {
            const float remaining = blend_frames - old_progress;
            const float current_weight =
                (blend_frames - step_progress) / remaining;
            const float step_frame = compact->replay.target_entry_frame_f32 +
                                     (step_progress - 1.0f);

            if (!hsd_evaluate_local_pose_f32(data,
                                             target_submotion,
                                             step_frame,
                                             target) ||
                !hsd_blend_local_pose_f32(data,
                                          target,
                                          current,
                                          current_weight,
                                          next))
            {
                return 0;
            }
            (void)memcpy(current, next, sizeof(*next) * data->joint_count);
            old_progress = step_progress;
        }
        (void)memcpy(out_pose, current, sizeof(*current) * data->joint_count);
        return 1;
    }
    return 0;
}

static int hsd_evaluate_joint_matrices_from_local_pose(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    hsd_matrix matrices[PF_M4_HSD_POSE_MAX_JOINTS])
{
    float cumulative_scale[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t has_cumulative_scale[PF_M4_HSD_POSE_MAX_JOINTS] = {UINT8_C(0)};
    uint8_t joint_index;

    if (data == NULL || pose == NULL || matrices == NULL || data->joints == NULL ||
        data->joint_count == UINT8_C(0) ||
        data->joint_count > PF_M4_HSD_POSE_MAX_JOINTS)
    {
        return 0;
    }
    for (joint_index = UINT8_C(0); joint_index < data->joint_count; ++joint_index)
    {
        const hsd_joint *joint = &data->joints[joint_index];
        const int parent = joint->parent_index;
        hsd_matrix local;
        const float *parent_scale =
            parent >= 0 && has_cumulative_scale[parent] != UINT8_C(0)
                ? cumulative_scale[parent]
                : NULL;
        uint8_t axis;

        if (parent >= (int)joint_index || parent < -1 ||
            !hsd_make_local_matrix(&pose[joint_index], parent_scale, &local))
        {
            return 0;
        }
        if (parent >= 0)
        {
            if (!hsd_concat_matrix(&matrices[parent], &local, &matrices[joint_index]))
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
                (void)memcpy(cumulative_scale[joint_index],
                             cumulative_scale[parent],
                             sizeof(cumulative_scale[joint_index]));
            }
        }
        else
        {
            has_cumulative_scale[joint_index] = UINT8_C(1);
            for (axis = UINT8_C(0); axis < UINT8_C(3); ++axis)
            {
                cumulative_scale[joint_index][axis] =
                    parent_scale == NULL
                        ? pose[joint_index].scale_f32[axis]
                        : pose[joint_index].scale_f32[axis] * parent_scale[axis];
            }
        }
    }
    return 1;
}

int hsd_evaluate_joint_origins_source_f32(
    const hsd_pose_data *data,
    uint16_t source_submotion,
    float frame_f32,
    const uint8_t *joint_indices,
    uint8_t joint_count,
    float out_origins_f32[PF_M4_HSD_POSE_MAX_JOINTS][3])
{
    hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];
    if (!hsd_evaluate_local_pose_f32(data, source_submotion, frame_f32, pose))
    {
        return 0;
    }
    return hsd_evaluate_joint_origins_from_local_pose_f32(
        data, pose, joint_indices, joint_count, out_origins_f32);
}

int hsd_evaluate_joint_origins_from_local_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    const uint8_t *joint_indices,
    uint8_t joint_count,
    float out_origins_f32[PF_M4_HSD_POSE_MAX_JOINTS][3])
{
    hsd_matrix matrices[PF_M4_HSD_POSE_MAX_JOINTS];
    uint8_t output_index;

    if (joint_indices == NULL || out_origins_f32 == NULL ||
        joint_count == UINT8_C(0) || joint_count > PF_M4_HSD_POSE_MAX_JOINTS ||
        !hsd_evaluate_joint_matrices_from_local_pose(data, pose, matrices))
    {
        return 0;
    }
    for (output_index = UINT8_C(0); output_index < joint_count; ++output_index)
    {
        const uint8_t joint_index = joint_indices[output_index];
        if (joint_index >= data->joint_count)
        {
            return 0;
        }
        out_origins_f32[output_index][0] = matrices[joint_index].value[0][3];
        out_origins_f32[output_index][1] = matrices[joint_index].value[1][3];
        out_origins_f32[output_index][2] = matrices[joint_index].value[2][3];
    }
    return 1;
}

int hsd_evaluate_hurt_pose(
    const hsd_pose_data *data,
    uint16_t source_submotion,
    float frame_f32,
    hsd_evaluated_capsule out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];

    if (!hsd_evaluate_local_pose_f32(data, source_submotion, frame_f32, pose))
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return 0;
    }
    return hsd_evaluate_hurt_pose_from_local_pose(data, pose, out_capsules, out_count);
}

int hsd_evaluate_hurt_pose_from_local_pose(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    hsd_evaluated_capsule out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    hsd_matrix matrices[PF_M4_HSD_POSE_MAX_JOINTS];
    uint8_t capsule_index;

    if (out_count != NULL)
    {
        *out_count = UINT8_C(0);
    }
    if (data == NULL || out_capsules == NULL || out_count == NULL ||
        data->capsules == NULL || data->capsule_count == UINT8_C(0) ||
        data->capsule_count > PF_M4_HSD_POSE_MAX_CAPSULES ||
        !hsd_evaluate_joint_matrices_from_local_pose(data, pose, matrices))
    {
        return 0;
    }
    for (capsule_index = UINT8_C(0); capsule_index < data->capsule_count; ++capsule_index)
    {
        const hsd_hurt_capsule *source = &data->capsules[capsule_index];
        hsd_evaluated_capsule *destination = &out_capsules[capsule_index];
        float endpoint_a[3];
        float endpoint_b[3];
        uint8_t axis;

        if (source->joint_index >= data->joint_count ||
            !hsd_transform_point(&matrices[source->joint_index],
                                 source->offset_a_f32,
                                 endpoint_a) ||
            !hsd_transform_point(&matrices[source->joint_index],
                                 source->offset_b_f32,
                                 endpoint_b))
        {
            return 0;
        }
        for (axis = UINT8_C(0); axis < UINT8_C(3); ++axis)
        {
            destination->endpoint_a_f32[axis] =
                hsd_source_coordinate_to_sim_f32(data, endpoint_a[axis], axis);
            destination->endpoint_b_f32[axis] =
                hsd_source_coordinate_to_sim_f32(data, endpoint_b[axis], axis);
        }
        destination->radius_f32 = hsd_source_scale_to_sim_f32(data, source->radius_f32);
        destination->hurtbox_id = source->hurtbox_id;
        destination->height = source->height;
        destination->grabbable = source->grabbable;
        destination->reserved = UINT8_C(0);
    }
    *out_count = data->capsule_count;
    return 1;
}
