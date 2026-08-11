#ifndef PF_SIM_HSD_POSE_H
#define PF_SIM_HSD_POSE_H

#include <stdint.h>

#define PF_M4_HSD_POSE_MAX_JOINTS UINT8_C(32)
#define PF_M4_HSD_POSE_MAX_CAPSULES UINT8_C(15)

typedef enum pf_m4_hsd_interpolation
{
    PF_M4_HSD_INTERPOLATION_CONSTANT = 1,
    PF_M4_HSD_INTERPOLATION_LINEAR = 2,
    PF_M4_HSD_INTERPOLATION_SPLINE_ZERO = 3,
    PF_M4_HSD_INTERPOLATION_SPLINE = 4,
    PF_M4_HSD_INTERPOLATION_SLOPE = 5,
    PF_M4_HSD_INTERPOLATION_KEY = 6
} pf_m4_hsd_interpolation;

typedef struct pf_m4_hsd_key
{
    int32_t frame_q16;
    int32_t value_q16;
    int32_t tangent_q16;
    uint8_t interpolation;
    uint8_t reserved[3];
} pf_m4_hsd_key;

typedef struct pf_m4_hsd_track
{
    uint16_t key_offset;
    uint16_t key_count;
    int16_t start_frame;
    uint8_t joint_index;
    uint8_t track_type;
} pf_m4_hsd_track;

typedef struct pf_m4_hsd_joint
{
    int32_t rotation_turns_q16[3];
    int32_t scale_q16[3];
    int32_t translation_q16[3];
    int8_t parent_index;
    uint8_t classical_scale;
    uint8_t reserved[2];
} pf_m4_hsd_joint;

typedef struct pf_m4_hsd_hurt_capsule
{
    int32_t offset_a_q16[3];
    int32_t offset_b_q16[3];
    int32_t radius_q16;
    uint8_t joint_index;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
} pf_m4_hsd_hurt_capsule;

typedef struct pf_m4_hsd_motion
{
    uint16_t source_submotion;
    uint16_t track_offset;
    uint16_t track_count;
    uint16_t frame_count;
} pf_m4_hsd_motion;

typedef struct pf_m4_hsd_pose_data
{
    const pf_m4_hsd_joint *joints;
    const pf_m4_hsd_motion *motions;
    const pf_m4_hsd_track *tracks;
    const pf_m4_hsd_key *keys;
    const pf_m4_hsd_hurt_capsule *capsules;
    uint16_t key_count;
    uint16_t track_count;
    uint8_t joint_count;
    uint8_t motion_count;
    uint8_t capsule_count;
    uint8_t reserved;
    int32_t source_to_sim_numerator;
    int32_t source_to_sim_denominator;
    int8_t axis_sign[3];
    uint8_t reserved_conversion;
} pf_m4_hsd_pose_data;

typedef struct pf_m4_hsd_evaluated_capsule
{
    int32_t endpoint_a_q16[3];
    int32_t endpoint_b_q16[3];
    int32_t radius_q16;
    uint8_t hurtbox_id;
    uint8_t height;
    uint8_t grabbable;
    uint8_t reserved;
} pf_m4_hsd_evaluated_capsule;

int pf_m4_hsd_evaluate_hurt_pose(
    const pf_m4_hsd_pose_data *data,
    uint16_t source_submotion,
    int32_t frame_q16,
    pf_m4_hsd_evaluated_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count);

#endif
