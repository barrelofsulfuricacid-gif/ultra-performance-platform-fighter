#ifndef PF_SIM_HSD_POSE_H
#define PF_SIM_HSD_POSE_H

#include <stdint.h>

#define PF_M4_HSD_POSE_MAX_JOINTS UINT8_C(32)
#define PF_M4_HSD_POSE_MAX_CAPSULES UINT8_C(15)
#define PF_M4_HSD_COMPACT_ROTATION_CAPACITY UINT8_C(19)
#define PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY UINT8_C(6)
#define PF_M4_HSD_FIGHTER_ANIMATION_TRANSLATION_FLAG UINT32_C(0x80000000)

typedef enum hsd_interpolation
{
    PF_M4_HSD_INTERPOLATION_CONSTANT = 1,
    PF_M4_HSD_INTERPOLATION_LINEAR = 2,
    PF_M4_HSD_INTERPOLATION_SPLINE_ZERO = 3,
    PF_M4_HSD_INTERPOLATION_SPLINE = 4,
    PF_M4_HSD_INTERPOLATION_SLOPE = 5,
    PF_M4_HSD_INTERPOLATION_KEY = 6
} hsd_interpolation;

typedef struct hsd_key
{
    float frame_f32;
    float value_f32;
    float tangent_f32;
    uint8_t interpolation;
    uint8_t reserved[3];
} hsd_key;

typedef struct hsd_track
{
    uint16_t key_offset;
    uint16_t key_count;
    int16_t start_frame;
    uint8_t joint_index;
    uint8_t track_type;
} hsd_track;

typedef struct hsd_joint
{
    float rotation_turns_f32[3];
    float scale_f32[3];
    float translation_f32[3];
    int8_t parent_index;
    uint8_t classical_scale;
    uint8_t reserved[2];
} hsd_joint;

typedef struct hsd_hurt_capsule
{
    float offset_a_f32[3];
    float offset_b_f32[3];
    float radius_f32;
    uint8_t joint_index;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
} hsd_hurt_capsule;

typedef struct hsd_motion
{
    uint16_t source_submotion;
    uint16_t track_offset;
    uint16_t track_count;
    uint16_t frame_count;
} hsd_motion;

typedef struct hsd_wait_animation
{
    uint16_t source_submotion;
    uint8_t weight;
    uint8_t blend_frames;
    uint8_t blend_parameter;
    uint8_t reserved[3];
} hsd_wait_animation;

typedef struct hsd_pose_data
{
    const hsd_joint *joints;
    const hsd_motion *motions;
    const hsd_track *tracks;
    const hsd_key *keys;
    const hsd_hurt_capsule *capsules;
    const uint8_t *rotation_joint_indices;
    const uint8_t *translation_joint_indices;
    const uint8_t *copy_target_joint_indices;
    uint16_t key_count;
    uint16_t track_count;
    uint8_t joint_count;
    uint8_t motion_count;
    uint8_t capsule_count;
    uint8_t rotation_joint_count;
    uint8_t translation_joint_count;
    uint8_t copy_target_joint_count;
    uint8_t reserved;
    int32_t source_to_sim_numerator;
    int32_t source_to_sim_denominator;
    int8_t axis_sign[3];
    uint8_t reserved_conversion;
} hsd_pose_data;

typedef struct hsd_local_pose
{
    float rotation_f32[4];
    float scale_f32[3];
    float translation_f32[3];
    uint8_t use_quaternion;
    uint8_t reserved[3];
} hsd_local_pose;

typedef struct hsd_compact_pose
{
    int16_t rotation_q15[PF_M4_HSD_COMPACT_ROTATION_CAPACITY][3];
    union
    {
        float translation_f32[PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY][3];
        struct
        {
            float source_frame_f32;
            float target_entry_frame_f32;
            float target_step_f32;
            float blend_frames_f32;
            uint16_t source_submotion;
            uint16_t reserved;
        } replay;
    };
    uint8_t mode;
    uint8_t reserved[3];
} hsd_compact_pose;

typedef enum hsd_compact_pose_mode
{
    PF_M4_HSD_COMPACT_POSE_PACKED = 0,
    PF_M4_HSD_COMPACT_POSE_REPLAY = 1
} hsd_compact_pose_mode;

typedef struct hsd_evaluated_capsule
{
    float endpoint_a_f32[3];
    float endpoint_b_f32[3];
    float radius_f32;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
    uint8_t reserved;
} hsd_evaluated_capsule;

int hsd_evaluate_local_pose_f32(
    const hsd_pose_data *data,
    uint16_t source_submotion,
    float frame_f32,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS]);

int hsd_blend_local_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS],
    const hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS],
    float current_weight_f32,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS]);

int hsd_pack_compact_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    hsd_compact_pose *out_compact);

int hsd_inflate_compact_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose *target,
    const hsd_compact_pose *compact,
    hsd_local_pose *out_pose);

int hsd_resolve_compact_pose_f32(
    const hsd_pose_data *data,
    uint16_t target_submotion,
    float target_frame_f32,
    float progress_f32,
    const hsd_compact_pose *compact,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS]);

int hsd_evaluate_joint_origins_from_local_pose_f32(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    const uint8_t *joint_indices,
    uint8_t joint_count,
    float out_origins_f32[PF_M4_HSD_POSE_MAX_JOINTS][3]);

int hsd_evaluate_hurt_pose_from_local_pose(
    const hsd_pose_data *data,
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    hsd_evaluated_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

int hsd_evaluate_joint_origins_source_f32(
    const hsd_pose_data *data,
    uint16_t source_submotion,
    float frame_f32,
    const uint8_t *joint_indices,
    uint8_t joint_count,
    float out_origins_f32[PF_M4_HSD_POSE_MAX_JOINTS][3]);

int hsd_evaluate_hurt_pose(
    const hsd_pose_data *data,
    uint16_t source_submotion,
    float frame_f32,
    hsd_evaluated_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

#endif
