#include "pf/m4.h"
#include "pf/sim.h"
#include "sim_falcon_frame_data.h"
#include "sim_hsd_pose.h"
#include "sim_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#include "generated/m4_ssbm_falcon_ground_loop_transition_oracle.inc"

#define TEST_ROTATION_TOLERANCE_Q15 INT32_C(8)
#define TEST_TRANSLATION_TOLERANCE_Q16 INT32_C(4)
#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static int32_t absolute_difference_i32(int32_t left, int32_t right)
{
    const int64_t difference = (int64_t)left - (int64_t)right;

    return (int32_t)(difference < INT64_C(0) ? -difference : difference);
}

static int compare_compact_pose(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_compact_pose *actual,
    const pf_m4_hsd_compact_pose *expected,
    uint32_t case_index,
    uint32_t trace_frame,
    const char *operation,
    int32_t rotation_tolerance,
    int32_t translation_tolerance,
    int32_t *maximum_rotation_difference,
    int32_t *maximum_translation_difference)
{
    uint8_t index;

    for (index = UINT8_C(0); index < data->rotation_joint_count; ++index)
    {
        uint8_t component;

        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            const int32_t difference = absolute_difference_i32(
                actual->rotation_q15[index][component],
                expected->rotation_q15[index][component]);

            if (difference > *maximum_rotation_difference)
            {
                *maximum_rotation_difference = difference;
            }
            if (difference > rotation_tolerance)
            {
                (void)fprintf(
                    stderr,
                    "m4-hsd-transition=fail operation=%s-rotation"
                    " case=%" PRIu32 " trace_frame=%" PRIu32
                    " joint=%u component=%u expected=%d actual=%d"
                    " difference=%" PRId32 "\n",
                    operation,
                    case_index,
                    trace_frame,
                    (unsigned int)index,
                    (unsigned int)component,
                    (int)expected->rotation_q15[index][component],
                    (int)actual->rotation_q15[index][component],
                    difference);
                return 0;
            }
        }
    }
    for (index = UINT8_C(0); index < data->translation_joint_count; ++index)
    {
        uint8_t component;

        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            const int32_t difference = absolute_difference_i32(
                actual->translation_q16[index][component],
                expected->translation_q16[index][component]);

            if (difference > *maximum_translation_difference)
            {
                *maximum_translation_difference = difference;
            }
            if (difference > translation_tolerance)
            {
                (void)fprintf(
                    stderr,
                    "m4-hsd-transition=fail operation=%s-translation"
                    " case=%" PRIu32 " trace_frame=%" PRIu32
                    " joint=%u component=%u expected=%" PRId32
                    " actual=%" PRId32 " difference=%" PRId32 "\n",
                    operation,
                    case_index,
                    trace_frame,
                    (unsigned int)index,
                    (unsigned int)component,
                    expected->translation_q16[index][component],
                    actual->translation_q16[index][component],
                    difference);
                return 0;
            }
        }
    }
    return 1;
}

static int run_production_transition_cases(
    const pf_m4_hsd_pose_data *data,
    int32_t *production_maximum_rotation_difference,
    int32_t *production_maximum_translation_difference)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim_config config;
    pf_sim *sim = NULL;
    uint32_t case_index;

    if (pf_m4_default_content(&content) != PF_STATUS_OK ||
        pf_m4_make_content_view(&content, &view) != PF_STATUS_OK ||
        pf_sim_default_config(
            &config, UINT8_C(2), PF_SIM_MODE_DUEL) != PF_STATUS_OK ||
        pf_sim_init(
            storage.state,
            sizeof(storage.state),
            storage.scratch,
            sizeof(storage.scratch),
            &view,
            &config,
            &sim) != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr, "m4-hsd-transition=fail operation=production-init\n");
        return 0;
    }
    for (case_index = UINT32_C(0);
         case_index < PF_M4_HSD_TRANSITION_PRODUCTION_CASE_COUNT;
         ++case_index)
    {
        const pf_m4_hsd_transition_production_case *oracle =
            &pf_m4_hsd_transition_production_cases[case_index];
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;
        pf_world_state *world;
        pf_m4_hsd_local_pose previous_local[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_compact_pose previous_compact;
        pf_m4_hsd_local_pose target_local[PF_M4_HSD_POSE_MAX_JOINTS];
        pf_m4_hsd_compact_pose target_compact;
        int32_t reference_maximum_rotation_difference = INT32_C(0);
        int32_t reference_maximum_translation_difference = INT32_C(0);

        if (pf_sim_reset(sim, UINT64_C(0x4853445452414e53)) != PF_STATUS_OK)
        {
            return 0;
        }
        world = &sim->world;
        world->action_state[0] = oracle->previous_action;
        world->action_ticks[0] = oracle->previous_action_ticks;
        world->source_submotion[0] = oracle->previous_submotion;
        world->source_animation_frame_q16[0] = oracle->previous_frame_q16;
        world->source_animation_rate_q16[0] = oracle->previous_rate_q16;
        world->velocity_x_q16[0] = oracle->previous_velocity_x_q16;
        world->ground_blend_progress_q16[0] =
            oracle->previous_progress_q16;
        world->ground_blend_pose[0] = oracle->previous_compact;
        world->facing[0] = INT8_C(1);
        world->dash_direction[0] = INT8_C(1);
        world->previous_tilt_x_direction[0] = INT8_C(1);
        world->tilt_x_age[0] = UINT8_C(10);
        world->horizontal_input_direction[0] = INT8_C(1);
        world->horizontal_input_age[0] = UINT8_C(10);
        (void)memset(inputs, 0, sizeof(inputs));
        inputs[0].tick = world->tick;
        inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[0].player_slot = UINT8_C(0);
        inputs[0].main_stick_x = oracle->input_x;
        inputs[1].tick = world->tick;
        inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[1].player_slot = UINT8_C(1);
        if (oracle->previous_progress_q16 == INT32_C(0) &&
            (!pf_m4_hsd_evaluate_local_pose_q16(
                data,
                oracle->previous_submotion,
                oracle->previous_frame_q16,
                previous_local) ||
            !pf_m4_hsd_pack_compact_pose_q16(
                data, previous_local, &previous_compact) ||
            !compare_compact_pose(
                data,
                &previous_compact,
                &oracle->previous_compact,
                case_index,
                oracle->trace_frame,
                "production-source",
                TEST_ROTATION_TOLERANCE_Q15,
                INT32_C(8),
                &reference_maximum_rotation_difference,
                &reference_maximum_translation_difference)))
        {
            return 0;
        }
        if (!pf_m4_hsd_evaluate_local_pose_q16(
                data,
                oracle->expected_submotion,
                oracle->expected_frame_q16,
                target_local) ||
            !pf_m4_hsd_pack_compact_pose_q16(
                data, target_local, &target_compact) ||
            !compare_compact_pose(
                data,
                &target_compact,
                &oracle->expected_target_compact,
                case_index,
                oracle->trace_frame,
                "production-target",
                TEST_ROTATION_TOLERANCE_Q15,
                TEST_TRANSLATION_TOLERANCE_Q16,
                &reference_maximum_rotation_difference,
                &reference_maximum_translation_difference))
        {
            return 0;
        }
        if (pf_sim_tick(sim, inputs, (size_t)2, &result) != PF_STATUS_OK ||
            world->action_state[0] != oracle->expected_action ||
            world->source_submotion[0] != oracle->expected_submotion ||
            absolute_difference_i32(
                world->source_animation_frame_q16[0],
                oracle->expected_frame_q16) > INT32_C(2) ||
            absolute_difference_i32(
                world->ground_blend_progress_q16[0],
                oracle->expected_progress_q16) > INT32_C(2))
        {
            (void)fprintf(
                stderr,
                "m4-hsd-transition=fail operation=production-state"
                " case=%" PRIu32 " trace_frame=%" PRIu32
                " action=%u/%u submotion=%u/%u frame=%" PRId32
                "/%" PRId32 " progress=%" PRId32 "/%" PRId32 "\n",
                case_index,
                oracle->trace_frame,
                (unsigned int)world->action_state[0],
                (unsigned int)oracle->expected_action,
                (unsigned int)world->source_submotion[0],
                (unsigned int)oracle->expected_submotion,
                world->source_animation_frame_q16[0],
                oracle->expected_frame_q16,
                world->ground_blend_progress_q16[0],
                oracle->expected_progress_q16);
            return 0;
        }
        if (!compare_compact_pose(
                data,
                &world->ground_blend_pose[0],
                &oracle->expected_compact,
                case_index,
                oracle->trace_frame,
                "production",
                TEST_ROTATION_TOLERANCE_Q15,
                TEST_TRANSLATION_TOLERANCE_Q16,
                production_maximum_rotation_difference,
                production_maximum_translation_difference))
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    const pf_m4_hsd_pose_data *data =
        pf_m4_falcon_reference_ground_loop_hsd_data();
    pf_m4_hsd_compact_pose current;
    uint32_t case_index;
    int32_t maximum_rotation_difference = INT32_C(0);
    int32_t maximum_translation_difference = INT32_C(0);
    int32_t production_maximum_rotation_difference = INT32_C(0);
    int32_t production_maximum_translation_difference = INT32_C(0);

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
        if (!compare_compact_pose(
                data,
                &actual,
                &oracle->expected,
                case_index,
                oracle->trace_frame,
                "recurrence",
                TEST_ROTATION_TOLERANCE_Q15,
                TEST_TRANSLATION_TOLERANCE_Q16,
                &maximum_rotation_difference,
                &maximum_translation_difference))
        {
            return 1;
        }
        current = actual;
    }
    if (!run_production_transition_cases(
            data,
            &production_maximum_rotation_difference,
            &production_maximum_translation_difference))
    {
        return 1;
    }
    (void)printf(
        "m4-hsd-transition=pass cases=%" PRIu32
        " production_cases=%" PRIu32
        " rotation_max_q15=%" PRId32 " translation_max_q16=%" PRId32
        " production_rotation_max_q15=%" PRId32
        " production_translation_max_q16=%" PRId32
        " semantic_sha256=%s\n",
        PF_M4_HSD_TRANSITION_ORACLE_CASE_COUNT,
        PF_M4_HSD_TRANSITION_PRODUCTION_CASE_COUNT,
        maximum_rotation_difference,
        maximum_translation_difference,
        production_maximum_rotation_difference,
        production_maximum_translation_difference,
        PF_M4_HSD_TRANSITION_ORACLE_SEMANTIC_SHA256);
    return 0;
}
