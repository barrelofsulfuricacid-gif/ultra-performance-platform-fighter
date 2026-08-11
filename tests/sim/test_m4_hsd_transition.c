#include "sim_falcon_frame_data.h"
#include "sim_hsd_pose.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "generated/m4_ssbm_falcon_ground_loop_transition_oracle.inc"

#define TEST_ROTATION_TOLERANCE_Q15 INT32_C(8)
#define TEST_TRANSLATION_TOLERANCE_Q16 INT32_C(4)

static int32_t absolute_difference_i32(int32_t left, int32_t right)
{
    const int64_t difference = (int64_t)left - (int64_t)right;

    return (int32_t)(difference < INT64_C(0) ? -difference : difference);
}

int main(void)
{
    const pf_m4_hsd_pose_data *data =
        pf_m4_falcon_reference_ground_loop_hsd_data();
    pf_m4_hsd_compact_pose current;
    uint32_t case_index;
    int32_t maximum_rotation_difference = INT32_C(0);
    int32_t maximum_translation_difference = INT32_C(0);

    if (data == NULL ||
        data->rotation_joint_count != PF_M4_HSD_COMPACT_ROTATION_CAPACITY ||
        data->translation_joint_count !=
            PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY)
    {
        (void)fprintf(stderr, "m4-hsd-transition=fail operation=data\n");
        return 1;
    }
    (void)memset(&current, 0, sizeof(current));
    for (case_index = UINT32_C(0);
         case_index < PF_M4_HSD_TRANSITION_ORACLE_CASE_COUNT;
         ++case_index)
    {
        const pf_m4_hsd_transition_oracle_case *oracle =
            &pf_m4_hsd_transition_oracle_cases[case_index];
        pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_local_pose source[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_local_pose result[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_compact_pose actual;
        uint8_t index;

        if (oracle->reset != UINT8_C(0))
        {
            current = oracle->prior;
        }
        if (!pf_m4_hsd_evaluate_local_pose_q16(
                data,
                oracle->source_submotion,
                oracle->frame_q16,
                target) ||
            !pf_m4_hsd_inflate_compact_pose_q16(
                data, target, &current, source) ||
            !pf_m4_hsd_blend_local_pose_q16(
                data,
                target,
                source,
                oracle->current_weight_q16,
                result) ||
            !pf_m4_hsd_pack_compact_pose_q16(data, result, &actual))
        {
            (void)fprintf(
                stderr,
                "m4-hsd-transition=fail operation=evaluate case=%" PRIu32
                " trace_frame=%" PRIu32 "\n",
                case_index,
                oracle->trace_frame);
            return 1;
        }
        for (index = UINT8_C(0);
             index < data->rotation_joint_count;
             ++index)
        {
            uint8_t component;

            for (component = UINT8_C(0); component < UINT8_C(3); ++component)
            {
                const int32_t difference = absolute_difference_i32(
                    actual.rotation_q15[index][component],
                    oracle->expected.rotation_q15[index][component]);

                if (difference > maximum_rotation_difference)
                {
                    maximum_rotation_difference = difference;
                }
                if (difference > TEST_ROTATION_TOLERANCE_Q15)
                {
                    (void)fprintf(
                        stderr,
                        "m4-hsd-transition=fail operation=rotation case=%" PRIu32
                        " trace_frame=%" PRIu32 " joint=%u component=%u"
                        " expected=%d actual=%d difference=%" PRId32 "\n",
                        case_index,
                        oracle->trace_frame,
                        (unsigned int)index,
                        (unsigned int)component,
                        (int)oracle->expected.rotation_q15[index][component],
                        (int)actual.rotation_q15[index][component],
                        difference);
                    return 1;
                }
            }
        }
        for (index = UINT8_C(0);
             index < data->translation_joint_count;
             ++index)
        {
            uint8_t component;

            for (component = UINT8_C(0); component < UINT8_C(3); ++component)
            {
                const int32_t difference = absolute_difference_i32(
                    actual.translation_q16[index][component],
                    oracle->expected.translation_q16[index][component]);

                if (difference > maximum_translation_difference)
                {
                    maximum_translation_difference = difference;
                }
                if (difference > TEST_TRANSLATION_TOLERANCE_Q16)
                {
                    (void)fprintf(
                        stderr,
                        "m4-hsd-transition=fail operation=translation case=%" PRIu32
                        " trace_frame=%" PRIu32 " joint=%u component=%u"
                        " expected=%" PRId32 " actual=%" PRId32
                        " difference=%" PRId32 "\n",
                        case_index,
                        oracle->trace_frame,
                        (unsigned int)index,
                        (unsigned int)component,
                        oracle->expected.translation_q16[index][component],
                        actual.translation_q16[index][component],
                        difference);
                    return 1;
                }
            }
        }
        current = actual;
    }
    (void)printf(
        "m4-hsd-transition=pass cases=%" PRIu32
        " rotation_max_q15=%" PRId32 " translation_max_q16=%" PRId32
        " semantic_sha256=%s\n",
        PF_M4_HSD_TRANSITION_ORACLE_CASE_COUNT,
        maximum_rotation_difference,
        maximum_translation_difference,
        PF_M4_HSD_TRANSITION_ORACLE_SEMANTIC_SHA256);
    return 0;
}
