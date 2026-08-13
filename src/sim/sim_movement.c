#include "sim_internal.h"
#include "sim_falcon_frame_data.h"
#include "sim_ssbm_common_data.h"
#include "sim_ssbm_damage.h"
#include "sim_ssbm_stage_data.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>


static float approach(float value, float target, float amount)
{
    if (value < target)
    {
        const float next = value + amount;
        return next > target ? target : next;
    }
    if (value > target)
    {
        const float next = value - amount;
        return next < target ? target : next;
    }
    return value;
}

static int update_mash_stick_direction(
    int16_t axis,
    uint16_t threshold,
    int8_t *direction)
{
    const int8_t previous = *direction;

    if (axis < -(int16_t)threshold)
    {
        *direction = INT8_C(-1);
    }
    if (axis > (int16_t)threshold)
    {
        *direction = INT8_C(1);
    }
    return *direction != previous;
}

static uint32_t grab_mash_pulses(
    const fighter_data *fighter,
    const pf_input_frame *input,
    uint64_t previous_buttons,
    int dense_shield_pressed,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const uint64_t digital_mask =
        PF_INPUT_BUTTON_ATTACK |
        PF_INPUT_BUTTON_SPECIAL |
        PF_INPUT_BUTTON_JUMP;
    const int digital_pulse =
        (((input->buttons & digital_mask) & ~previous_buttons) !=
         UINT64_C(0)) ||
        dense_shield_pressed != 0;
    const int stick_pulse =
        update_mash_stick_direction(
            input->main_stick_x,
            fighter->mash_stick_axis_threshold,
            &scratch->mash_stick_x_direction[player_index]) |
        update_mash_stick_direction(
            input->main_stick_y,
            fighter->mash_stick_axis_threshold,
            &scratch->mash_stick_y_direction[player_index]);

    return (uint32_t)digital_pulse + (uint32_t)stick_pulse;
}

static uint16_t falcon_jump_submotion_from_x(
    int16_t main_stick_x,
    int8_t facing,
    int aerial)
{
    const int32_t relative_axis =
        (int32_t)main_stick_x * (int32_t)facing;
    const int32_t backward_threshold =
        -(int32_t)
            ssbm_common_reference_jump_backward_axis_threshold();
    const int backward = relative_axis <= backward_threshold;

    if (aerial != 0)
    {
        return backward != 0
                   ? (uint16_t)
                         PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD
                   : (uint16_t)
                         PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD;
    }
    return backward != 0
               ? (uint16_t)PF_M4_FALCON_SUBMOTION_JUMP_BACKWARD
               : (uint16_t)PF_M4_FALCON_SUBMOTION_JUMP_FORWARD;
}

static uint16_t falcon_jump_submotion(
    const pf_input_frame *input,
    int8_t facing,
    int aerial)
{
    return falcon_jump_submotion_from_x(
        input->main_stick_x,
        facing,
        aerial);
}

static int falcon_direct_hsd_locked_bottom_f32(
    uint8_t action_state,
    float source_animation_frame_f32,
    uint8_t grounded,
    float *out_locked_bottom_y_f32)
{
    falcon_ecb_pose_f32 source_pose;

    if (out_locked_bottom_y_f32 == NULL)
    {
        return 0;
    }
    *out_locked_bottom_y_f32 =
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32;
    if (action_state == (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR &&
        source_animation_frame_f32 <
            5.0f)
    {
        *out_locked_bottom_y_f32 = 0.0f;
    }
    else if (action_state ==
                 (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND &&
             grounded == UINT8_C(0) &&
             source_animation_frame_f32 >=
                 14.0f &&
             source_animation_frame_f32 <=
                 17.0f)
    {
        *out_locked_bottom_y_f32 = 0.0f;
    }
    else if (action_state ==
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
    {
        if (source_animation_frame_f32 <=
            5.0f)
        {
            *out_locked_bottom_y_f32 = 0.0f;
        }
        else if (source_animation_frame_f32 >=
                     14.0f &&
                 source_animation_frame_f32 <=
                     17.0f)
        {
            if (!falcon_reference_hsd_ecb_pose(
                    (uint16_t)
                        PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR,
                    13.0f,
                    0,
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
                    &source_pose))
            {
                return 0;
            }
            *out_locked_bottom_y_f32 =
                source_pose.bottom_y_from_origin_f32;
        }
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW &&
             source_animation_frame_f32 >= 1.0f)
    {
        if (!falcon_reference_hsd_ecb_pose(
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_THROW,
                0.0f,
                0,
                PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
                &source_pose))
        {
            return 0;
        }
        *out_locked_bottom_y_f32 =
            source_pose.bottom_y_from_origin_f32;
    }
    return 1;
}

static int falcon_walk_submotion(uint16_t source_submotion)
{
    return source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_SLOW ||
           source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_MIDDLE ||
           source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_FAST;
}

static uint16_t falcon_walk_submotion_for_velocity(
    const fighter_data *fighter,
    float ground_velocity_f32)
{
    const float speed_f32 =
        ground_velocity_f32 < 0.0f
            ? -ground_velocity_f32
            : ground_velocity_f32;
    const float middle_threshold_f32 = multiply_f32(
        fighter->walk_speed_f32,
        fighter->walk_middle_speed_ratio_f32);
    const float fast_threshold_f32 = multiply_f32(
        fighter->walk_speed_f32,
        fighter->walk_fast_speed_ratio_f32);

    if (speed_f32 >= fast_threshold_f32)
    {
        return (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_FAST;
    }
    if (speed_f32 >= middle_threshold_f32)
    {
        return (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_MIDDLE;
    }
    return (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_SLOW;
}

static float falcon_ground_animation_rate_f32(
    float ground_velocity_f32,
    int8_t facing,
    float animation_scaling_f32)
{
    if (ground_velocity_f32 * (float)facing <= 0.0f)
    {
        return 0.0f;
    }
    return fabsf(ground_velocity_f32) / animation_scaling_f32;
}

static float falcon_walk_animation_scaling_f32(
    const fighter_data *fighter,
    uint16_t source_submotion)
{
    switch (source_submotion)
    {
        case PF_M4_FALCON_SUBMOTION_WALK_SLOW:
            return fighter->slow_walk_animation_scaling_f32;
        case PF_M4_FALCON_SUBMOTION_WALK_MIDDLE:
            return fighter->middle_walk_animation_scaling_f32;
        case PF_M4_FALCON_SUBMOTION_WALK_FAST:
            return fighter->fast_walk_animation_scaling_f32;
        default:
            return 0.0f;
    }
}

static float falcon_advance_loop_animation_f32(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    float source_animation_rate_f32,
    float *out_frame_f32)
{
    const falcon_submotion_data *motion =
        falcon_reference_submotion(source_submotion);
    float length_f32;
    float next_frame_f32;

    if (motion == NULL || motion->animation_frame_count == UINT16_C(0) ||
        source_animation_frame_f32 < 0.0f ||
        source_animation_rate_f32 < 0.0f)
    {
        return 0;
    }
    length_f32 = (float)motion->animation_frame_count;
    next_frame_f32 = source_animation_frame_f32 + source_animation_rate_f32;
    *out_frame_f32 = fmodf(next_frame_f32, length_f32);
    return 1;
}

static float falcon_remap_walk_animation_f32(
    uint16_t old_submotion,
    uint16_t new_submotion,
    float advanced_frame_f32,
    float *out_frame_f32)
{
    const falcon_submotion_data *old_motion =
        falcon_reference_submotion(old_submotion);
    const falcon_submotion_data *new_motion =
        falcon_reference_submotion(new_submotion);
    float old_length_f32;
    int64_t remapped_integer_frame;

    if (old_motion == NULL || new_motion == NULL ||
        old_motion->animation_frame_count == UINT16_C(0) ||
        new_motion->animation_frame_count == UINT16_C(0) ||
        advanced_frame_f32 < 0.0f)
    {
        return 0;
    }
    old_length_f32 = (float)old_motion->animation_frame_count;
    remapped_integer_frame =
        (int64_t)((float)new_motion->animation_frame_count *
                  fmodf(advanced_frame_f32, old_length_f32) /
                  old_length_f32);
    remapped_integer_frame =
        (remapped_integer_frame + INT64_C(1)) %
        (int64_t)new_motion->animation_frame_count;
    *out_frame_f32 = (float)remapped_integer_frame;
    return 1;
}

static const hsd_wait_animation *
falcon_select_wait_animation(
    uint64_t *rng_state,
    uint16_t current_submotion)
{
    uint8_t animation_count;
    const hsd_wait_animation *animations =
        falcon_reference_wait_animations(&animation_count);

    for (;;)
    {
        const uint32_t selection =
            pf_sim_hsd_random_bounded(rng_state, UINT32_C(100)) +
            UINT32_C(1);
        uint32_t cumulative_weight = UINT32_C(0);
        uint8_t animation_index;

        for (animation_index = UINT8_C(0);
             animation_index < animation_count;
             ++animation_index)
        {
            cumulative_weight += animations[animation_index].weight;
            if (selection <= cumulative_weight)
            {
                const hsd_wait_animation *selected =
                    &animations[animation_index];

                /* ftwaitanim.c repeats a secondary idle selection when it
                 * chose the currently playing secondary idle. Base Wait may
                 * select itself and restart through its ordinary blend. */
                if (current_submotion !=
                        (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT &&
                    selected->source_submotion == current_submotion)
                {
                    break;
                }
                return selected;
            }
        }
        if (animation_index == animation_count)
        {
            return NULL;
        }
    }
}

static pf_status update_falcon_ground_animation_clock(
    const fighter_data *fighter,
    uint64_t *rng_state,
    uint8_t previous_action,
    uint8_t previous_resume_action,
    uint16_t previous_submotion,
    float previous_frame_f32,
    float previous_rate_f32,
    float previous_ground_velocity_f32,
    int8_t previous_facing,
    uint8_t action,
    uint8_t resume_action,
    uint16_t *source_submotion,
    float *source_animation_frame_f32,
    float *source_animation_rate_f32)
{
    const uint8_t effective_previous_action =
        previous_action == (uint8_t)PF_M4_ACTION_HITLAG
            ? previous_resume_action
            : previous_action;
    const uint8_t effective_action =
        action == (uint8_t)PF_M4_ACTION_HITLAG
            ? resume_action
            : action;

    if (effective_action != (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
        effective_action != (uint8_t)PF_M4_ACTION_WALK &&
        effective_action != (uint8_t)PF_M4_ACTION_RUN)
    {
        *source_animation_frame_f32 = INT32_C(0);
        *source_animation_rate_f32 = INT32_C(0);
        return PF_STATUS_OK;
    }
    if (action == (uint8_t)PF_M4_ACTION_HITLAG ||
        previous_action == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        *source_submotion = previous_submotion;
        *source_animation_frame_f32 = previous_frame_f32;
        *source_animation_rate_f32 = previous_rate_f32;
        return PF_STATUS_OK;
    }
    if (effective_action == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        const falcon_submotion_data *wait =
            falcon_reference_submotion(previous_submotion);
        const float terminal_frame_f32 =
            wait != NULL && wait->animation_frame_count != UINT16_C(0)
                ? (int32_t)(wait->animation_frame_count - UINT16_C(1)) *
                      1.0f
                : INT32_C(-1);

        if (terminal_frame_f32 < INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (effective_previous_action !=
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            *source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
            *source_animation_frame_f32 = INT32_C(0);
            *source_animation_rate_f32 = 1.0f;
        }
        else if (previous_frame_f32 >= terminal_frame_f32)
        {
            const hsd_wait_animation *selected =
                falcon_select_wait_animation(
                    rng_state, previous_submotion);

            if (selected == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *source_submotion = selected->source_submotion;
            *source_animation_frame_f32 = INT32_C(0);
            *source_animation_rate_f32 = 1.0f;
        }
        else
        {
            *source_submotion = previous_submotion;
            *source_animation_frame_f32 =
                previous_frame_f32 + previous_rate_f32;
            *source_animation_rate_f32 = 1.0f;
        }
        return PF_STATUS_OK;
    }
    if (effective_action == (uint8_t)PF_M4_ACTION_WALK)
    {
        const uint16_t selected_submotion =
            falcon_walk_submotion_for_velocity(
                fighter,
                previous_ground_velocity_f32);

        if (effective_previous_action != (uint8_t)PF_M4_ACTION_WALK ||
            !falcon_walk_submotion(previous_submotion))
        {
            const falcon_submotion_data *motion =
                falcon_reference_submotion(selected_submotion);

            if (motion == NULL ||
                motion->animation_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *source_submotion = selected_submotion;
            *source_animation_frame_f32 =
                motion->animation_frame_count > UINT16_C(1)
                    ? 1.0f
                    : INT32_C(0);
            *source_animation_rate_f32 = 1.0f;
            return PF_STATUS_OK;
        }
        if (!falcon_advance_loop_animation_f32(
                previous_submotion,
                previous_frame_f32,
                previous_rate_f32,
                source_animation_frame_f32))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *source_submotion = selected_submotion;
        if (selected_submotion != previous_submotion)
        {
            if (!falcon_remap_walk_animation_f32(
                    previous_submotion,
                    selected_submotion,
                    *source_animation_frame_f32,
                    source_animation_frame_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *source_animation_rate_f32 = 1.0f;
        }
        else
        {
            *source_animation_rate_f32 =
                falcon_ground_animation_rate_f32(
                    previous_ground_velocity_f32,
                    previous_facing,
                    falcon_walk_animation_scaling_f32(
                        fighter,
                        selected_submotion));
        }
        return PF_STATUS_OK;
    }

    *source_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_RUN;
    if (effective_previous_action != (uint8_t)PF_M4_ACTION_RUN ||
        previous_submotion != (uint16_t)PF_M4_FALCON_SUBMOTION_RUN)
    {
        *source_animation_frame_f32 = INT32_C(0);
        *source_animation_rate_f32 = 1.0f;
        return PF_STATUS_OK;
    }
    if (!falcon_advance_loop_animation_f32(
            previous_submotion,
            previous_frame_f32,
            previous_rate_f32,
            source_animation_frame_f32))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *source_animation_rate_f32 =
        falcon_ground_animation_rate_f32(
            previous_ground_velocity_f32,
            previous_facing,
            fighter->run_animation_scaling_f32);
    return PF_STATUS_OK;
}

static int falcon_fall_animation_motions(
    uint8_t action,
    uint16_t submotion,
    uint16_t *out_neutral,
    uint16_t *out_forward,
    uint16_t *out_backward)
{
    if (action_uses_fall_special_pose(action))
    {
        *out_neutral =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL;
        *out_forward =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_FORWARD;
        *out_backward =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_BACKWARD;
        return 1;
    }
    if (action != (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        return 0;
    }
    if (submotion >= (uint16_t)PF_M4_FALCON_SUBMOTION_FALL &&
        submotion <= (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_BACKWARD)
    {
        *out_neutral = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
        *out_forward = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_FORWARD;
        *out_backward = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_BACKWARD;
        return 1;
    }
    if (submotion >= (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL &&
        submotion <=
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD)
    {
        *out_neutral = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL;
        *out_forward =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL_FORWARD;
        *out_backward =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD;
        return 1;
    }
    return 0;
}

static pf_status update_falcon_fall_animation_clock(
    const fighter_data *fighter,
    uint8_t previous_action,
    uint8_t previous_resume_action,
    uint16_t previous_submotion,
    float previous_frame_f32,
    float previous_rate_f32,
    float previous_blend_f32,
    uint8_t previous_target_switched,
    float previous_velocity_x_f32,
    int8_t previous_facing,
    uint8_t action,
    uint8_t resume_action,
    uint16_t *source_submotion,
    float *source_animation_frame_f32,
    float *source_animation_rate_f32,
    float *fall_animation_blend_f32,
    uint8_t *fall_animation_target_switched)
{
    const ssbm_fall_animation_attributes *common =
        ssbm_common_reference_fall_animation();
    const uint8_t effective_previous_action =
        effective_action_state(previous_action, previous_resume_action);
    const uint8_t effective_action =
        effective_action_state(action, resume_action);
    const falcon_submotion_data *neutral;
    float air_drift_fraction_f32;
    float magnitude_f32;
    float target_blend_f32 = 0.0f;
    float next_blend_f32;
    uint16_t neutral_submotion;
    uint16_t forward_submotion;
    uint16_t backward_submotion;
    uint16_t previous_neutral_submotion;
    uint16_t previous_forward_submotion;
    uint16_t previous_backward_submotion;
    uint16_t selected_submotion;

    if (!falcon_fall_animation_motions(
            effective_action,
            *source_submotion,
            &neutral_submotion,
            &forward_submotion,
            &backward_submotion))
    {
        *fall_animation_blend_f32 = 0.0f;
        *fall_animation_target_switched = UINT8_C(0);
        return PF_STATUS_OK;
    }
    neutral = falcon_reference_submotion(neutral_submotion);
    selected_submotion = neutral_submotion;
    if (common == NULL || neutral == NULL ||
        neutral->animation_frame_count == UINT16_C(0) ||
        fighter->air_speed_f32 <= 0.0f)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (action == (uint8_t)PF_M4_ACTION_HITLAG ||
        previous_action == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        *source_submotion = previous_submotion;
        *source_animation_frame_f32 = previous_frame_f32;
        *source_animation_rate_f32 = previous_rate_f32;
        *fall_animation_blend_f32 = previous_blend_f32;
        *fall_animation_target_switched = previous_target_switched;
        return PF_STATUS_OK;
    }
    if (!falcon_fall_animation_motions(
            effective_previous_action,
            previous_submotion,
            &previous_neutral_submotion,
            &previous_forward_submotion,
            &previous_backward_submotion) ||
        previous_neutral_submotion != neutral_submotion)
    {
        *source_submotion = neutral_submotion;
        *source_animation_frame_f32 = 0.0f;
        *source_animation_rate_f32 = 1.0f;
        *fall_animation_blend_f32 = 0.0f;
        *fall_animation_target_switched = UINT8_C(0);
        return PF_STATUS_OK;
    }

    *source_animation_frame_f32 = previous_frame_f32 + previous_rate_f32;
    while (*source_animation_frame_f32 >=
           (float)neutral->animation_frame_count)
    {
        *source_animation_frame_f32 -=
            (float)neutral->animation_frame_count;
    }
    *source_animation_rate_f32 = 1.0f;

    air_drift_fraction_f32 = previous_velocity_x_f32 / fighter->air_speed_f32;
    if (air_drift_fraction_f32 > 1.0f)
    {
        air_drift_fraction_f32 = 1.0f;
    }
    else if (air_drift_fraction_f32 < -1.0f)
    {
        air_drift_fraction_f32 = -1.0f;
    }
    magnitude_f32 = air_drift_fraction_f32 < 0.0f
                        ? -air_drift_fraction_f32
                        : air_drift_fraction_f32;
    if (magnitude_f32 > common->direction_threshold_f32)
    {
        target_blend_f32 =
            (magnitude_f32 - common->direction_threshold_f32) /
            (1.0f - common->direction_threshold_f32);
        selected_submotion =
            (air_drift_fraction_f32 * (float)previous_facing > 0.0f)
                ? forward_submotion
                : backward_submotion;
    }
    next_blend_f32 = previous_blend_f32 +
                     multiply_f32(
                         target_blend_f32 - previous_blend_f32,
                         common->blend_rate_f32);
    *fall_animation_blend_f32 = next_blend_f32;
    *fall_animation_target_switched =
        next_blend_f32 != 0.0f &&
                selected_submotion != previous_submotion
            ? UINT8_C(1)
            : UINT8_C(0);
    *source_submotion = next_blend_f32 != 0.0f
                            ? selected_submotion
                            : previous_submotion;
    return PF_STATUS_OK;
}

static float ground_blend_weight_f32(
    float old_progress_f32,
    float new_progress_f32)
{
    const float old_remaining_f32 =
        6.0f - old_progress_f32;
    const float new_remaining_f32 =
        6.0f - new_progress_f32;

    return old_remaining_f32 > 0.0f
               ? new_remaining_f32 / old_remaining_f32
               : 0.0f;
}

static int falcon_continue_ground_blend_pose(
    const hsd_pose_data *data,
    const hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS],
    const hsd_compact_pose *previous_compact,
    float previous_progress_f32,
    float frame_delta_f32,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS],
    float *out_progress_f32)
{
    hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS];
    const float progress_f32 = previous_progress_f32 + frame_delta_f32;

    if (progress_f32 >= 6.0f)
    {
        (void)memcpy(
            out_pose, target, sizeof(*target) * data->joint_count);
    }
    else if (!hsd_inflate_compact_pose_f32(
                 data, target, previous_compact, current) ||
             !hsd_blend_local_pose_f32(
                 data,
                 target,
                 current,
                 ground_blend_weight_f32(
                     previous_progress_f32, progress_f32),
                 out_pose))
    {
        return 0;
    }
    *out_progress_f32 = progress_f32;
    return 1;
}

static int falcon_ground_blend_source_pose(
    const pf_world_state *world,
    uint32_t player_index,
    const hsd_pose_data *data,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    const uint8_t previous_action = effective_action_state(
        world->action_state[player_index],
        world->hitlag_resume_action[player_index]);
    falcon_move_index previous_move_index;
    const int previous_reference_move =
        falcon_reference_move_for_action(
            previous_action,
            &previous_move_index);
    uint16_t source_submotion;
    float source_frame_f32;

    if (previous_action == (uint8_t)PF_M4_ACTION_WALK ||
        previous_action == (uint8_t)PF_M4_ACTION_RUN)
    {
        hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        float ignored_progress_f32;

        source_submotion = world->source_submotion[player_index];
        if (!falcon_advance_loop_animation_f32(
                source_submotion,
                world->source_animation_frame_f32[player_index],
                world->source_animation_rate_f32[player_index],
                &source_frame_f32) ||
            !hsd_evaluate_local_pose_f32(
                data, source_submotion, source_frame_f32, target))
        {
            return 0;
        }
        if (world->ground_blend_progress_f32[player_index] <= INT32_C(0))
        {
            (void)memcpy(
                out_pose, target, sizeof(*target) * data->joint_count);
            return 1;
        }
        return falcon_continue_ground_blend_pose(
            data,
            target,
            &world->ground_blend_pose[player_index],
            world->ground_blend_progress_f32[player_index],
            world->source_animation_rate_f32[player_index],
            out_pose,
            &ignored_progress_f32);
    }
    else if (previous_action == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        float ignored_progress_f32;

        if (falcon_reference_wait_animation(
                world->source_submotion[player_index]) == NULL)
        {
            return 0;
        }
        source_submotion = world->source_submotion[player_index];
        source_frame_f32 =
            world->source_animation_frame_f32[player_index] +
            world->source_animation_rate_f32[player_index];
        if (!hsd_evaluate_local_pose_f32(
                data, source_submotion, source_frame_f32, target))
        {
            return 0;
        }
        if (world->ground_blend_progress_f32[player_index] <= INT32_C(0))
        {
            (void)memcpy(
                out_pose, target, sizeof(*target) * data->joint_count);
            return 1;
        }
        return falcon_continue_ground_blend_pose(
            data,
            target,
            &world->ground_blend_pose[player_index],
            world->ground_blend_progress_f32[player_index],
            world->source_animation_rate_f32[player_index],
            out_pose,
            &ignored_progress_f32);
    }
    else if (previous_action == (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        const falcon_submotion_data *dash =
            falcon_reference_submotion(
                PF_M4_FALCON_SUBMOTION_DASH);

        if (dash == NULL || dash->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        source_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_DASH;
        source_frame_f32 = (int32_t)(
            world->action_ticks[player_index] + UINT16_C(1) <
                    dash->animation_frame_count
                ? world->action_ticks[player_index] + UINT16_C(1)
                : dash->animation_frame_count - UINT16_C(1)) *
            1.0f;
    }
    else if (previous_action == (uint8_t)PF_M4_ACTION_LANDING)
    {
        const falcon_submotion_data *landing =
            falcon_reference_submotion(
                PF_M4_FALCON_SUBMOTION_LANDING);

        if (landing == NULL || landing->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        source_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_LANDING;
        source_frame_f32 = (int32_t)(
            world->action_ticks[player_index] + UINT16_C(1) <
                    landing->animation_frame_count
                ? world->action_ticks[player_index] + UINT16_C(1)
                : landing->animation_frame_count - UINT16_C(1)) *
            1.0f;
    }
    else if (previous_action == (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        const falcon_submotion_data *turn =
            falcon_reference_submotion(
                PF_M4_FALCON_SUBMOTION_TURN);

        if (turn == NULL || turn->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        source_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_TURN;
        source_frame_f32 = (float)(
            world->action_ticks[player_index] + UINT16_C(1) <
                    turn->animation_frame_count
                ? world->action_ticks[player_index] + UINT16_C(1)
                : turn->animation_frame_count - UINT16_C(1));
    }
    else if (action_is_damage(previous_action) &&
             world->source_animation_rate_f32[player_index] > INT32_C(0))
    {
        const falcon_submotion_data *damage_motion =
            falcon_reference_submotion(
                world->source_submotion[player_index]);
        float next_frame_f32;
        float terminal_frame_f32;

        /* Damage_Anim advances before Damage_IASA delegates to Wait/Fall.
         * A released grounded Damage can therefore acquire GuardOn in the
         * same update; its blend source is the just-advanced Damage pose,
         * clamped exactly like the retained Damage animation clock. */
        if (damage_motion == NULL ||
            world->source_submotion[player_index] <
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_HIGH_1 ||
            world->source_submotion[player_index] >
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL ||
            damage_motion->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        source_submotion = world->source_submotion[player_index];
        next_frame_f32 =
            world->source_animation_frame_f32[player_index] +
            world->source_animation_rate_f32[player_index];
        terminal_frame_f32 =
            (float)(damage_motion->animation_frame_count - UINT16_C(1));
        source_frame_f32 =
            next_frame_f32 < terminal_frame_f32
                ? next_frame_f32
                : terminal_frame_f32;
    }
    else if (previous_action ==
                 (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
             world->source_submotion[player_index] ==
                 (uint16_t)PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL &&
             world->source_animation_rate_f32[player_index] > INT32_C(0))
    {
        const falcon_submotion_data *landing =
            falcon_reference_submotion(
                (uint16_t)
                    PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL);
        const float next_frame_f32 =
            world->source_animation_frame_f32[player_index] +
            world->source_animation_rate_f32[player_index];
        float terminal_frame_f32;

        if (landing == NULL ||
            landing->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        terminal_frame_f32 =
            (float)(landing->animation_frame_count - UINT16_C(1));
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL;
        source_frame_f32 =
            next_frame_f32 < terminal_frame_f32
                ? next_frame_f32
                : terminal_frame_f32;
    }
    else if ((previous_action == (uint8_t)PF_M4_ACTION_CROUCH_END ||
              action_uses_direct_hsd_pose(previous_action)) &&
             world->source_animation_rate_f32[player_index] > INT32_C(0))
    {
        source_submotion = world->source_submotion[player_index];
        source_frame_f32 =
            world->source_animation_frame_f32[player_index] +
            world->source_animation_rate_f32[player_index];
    }
    else if (previous_reference_move != 0)
    {
        /* Ground attacks and Catch can finish in Anim, install Wait, and
         * enter GuardOn in this same update. The Guard blend starts from the
         * just-advanced terminal move pose, not from an invented Wait row. */
        const struct reference_move *move =
            falcon_reference_move(previous_move_index);
        const float final_frame_f32 =
            move != NULL && move->present != UINT8_C(0)
                ? (float)move->total_frames
                : -1.0f;

        if (final_frame_f32 < INT32_C(0) ||
            !falcon_reference_action_hsd_source(
                previous_action,
                world->action_ticks[player_index],
                &source_submotion,
                &source_frame_f32))
        {
            return 0;
        }
        if (source_frame_f32 > final_frame_f32)
        {
            source_frame_f32 = final_frame_f32;
        }
    }
    else
    {
        return 0;
    }
    return hsd_evaluate_local_pose_f32(
        data, source_submotion, source_frame_f32, out_pose);
}

static pf_status evaluate_falcon_ground_blend_pose(
    const pf_world_state *world,
    uint32_t player_index,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    uint16_t action_ticks,
    hsd_compact_pose *out_pose,
    float *out_progress_f32)
{
    const hsd_pose_data *data =
        falcon_reference_hsd_pose_data();
    const uint8_t previous_action = effective_action_state(
        world->action_state[player_index],
        world->hitlag_resume_action[player_index]);
    const uint8_t action = effective_action_state(
        action_state, hitlag_resume_action);
    hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
    hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS];
    hsd_local_pose result[PF_M4_HSD_POSE_MAX_JOINTS];
    float progress_f32 = 0.0f;
    int transition_steps = 0;
    const int guard_on =
        action == (uint8_t)PF_M4_ACTION_SHIELD &&
        source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON;

    if (out_pose == NULL || out_progress_f32 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (data == NULL ||
        (!guard_on &&
        !action_uses_ground_animation_clock(
            action_state, hitlag_resume_action)))
    {
        (void)memset(out_pose, 0, sizeof(*out_pose));
        *out_progress_f32 = 0.0f;
        return PF_STATUS_OK;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        *out_pose = world->ground_blend_pose[player_index];
        *out_progress_f32 = world->ground_blend_progress_f32[player_index];
        return PF_STATUS_OK;
    }
    if (guard_on)
    {
        const hsd_local_pose *guard_target =
            falcon_reference_guard_target_hsd_pose();

        if (guard_target == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (previous_action != (uint8_t)PF_M4_ACTION_SHIELD ||
            world->source_submotion[player_index] !=
                (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON)
        {
            if (!falcon_ground_blend_source_pose(
                    world, player_index, data, result))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        else
        {
            const uint16_t frame =
                action_ticks < PF_M4_FALCON_GUARD_ON_FRAME_COUNT
                    ? action_ticks
                    : PF_M4_FALCON_GUARD_ON_FRAME_COUNT;
            const float current_weight_f32 =
                (float)(PF_M4_FALCON_GUARD_ON_FRAME_COUNT - frame) /
                (float)PF_M4_FALCON_GUARD_ON_FRAME_COUNT;

            if (!hsd_inflate_compact_pose_f32(
                    data,
                    guard_target,
                    &world->ground_blend_pose[player_index],
                    current) ||
                !hsd_blend_local_pose_f32(
                    data,
                    guard_target,
                    current,
                    current_weight_f32,
                    result))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        if (!hsd_pack_compact_pose_f32(data, result, out_pose))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *out_progress_f32 = (float)action_ticks + 1.0f;
        return PF_STATUS_OK;
    }
    if (!hsd_evaluate_local_pose_f32(
            data,
            source_submotion,
            source_animation_frame_f32,
            target))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (source_submotion != world->source_submotion[player_index] ||
        (action == (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
         action == previous_action &&
         source_animation_frame_f32 <
             world->source_animation_frame_f32[player_index]) ||
        action != previous_action)
    {
        if (action == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            transition_steps = 1;
        }
        else if (action == (uint8_t)PF_M4_ACTION_WALK)
        {
            transition_steps =
                previous_action == (uint8_t)PF_M4_ACTION_WALK ? 3 : 2;
        }
        else
        {
            transition_steps = 1;
        }
    }
    if (transition_steps > 0 &&
        action == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        const hsd_wait_animation *wait_animation =
            falcon_reference_wait_animation(source_submotion);

        if (wait_animation != NULL &&
            wait_animation->blend_frames == UINT8_C(0))
        {
            (void)memset(out_pose, 0, sizeof(*out_pose));
            *out_progress_f32 = INT32_C(0);
            return PF_STATUS_OK;
        }
        if (wait_animation != NULL &&
            wait_animation->blend_frames != UINT8_C(6))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
    }
    if (transition_steps == 1 &&
        action == (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
        (previous_action == (uint8_t)PF_M4_ACTION_CROUCH_END ||
         previous_action == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
         action_uses_direct_hsd_pose(previous_action)) &&
        world->source_animation_rate_f32[player_index] == 1.0f)
    {
        (void)memset(out_pose, 0, sizeof(*out_pose));
        out_pose->replay.source_submotion =
            world->source_submotion[player_index];
        out_pose->replay.source_frame_f32 =
            world->source_animation_frame_f32[player_index] + 1.0f;
        out_pose->replay.target_entry_frame_f32 =
            source_animation_frame_f32;
        out_pose->replay.target_step_f32 = 1.0f;
        out_pose->replay.blend_frames_f32 =
            INT32_C(6) * 1.0f;
        out_pose->mode = (uint8_t)PF_M4_HSD_COMPACT_POSE_REPLAY;
        *out_progress_f32 = 1.0f;
        return PF_STATUS_OK;
    }
    if (transition_steps > 0 &&
        falcon_ground_blend_source_pose(
            world, player_index, data, current))
    {
        int step;

        for (step = 1; step <= transition_steps; ++step)
        {
            hsd_local_pose step_target[PF_M4_HSD_POSE_MAX_JOINTS];
            hsd_local_pose next[PF_M4_HSD_POSE_MAX_JOINTS];
            float step_frame_f32 =
                source_animation_frame_f32 -
                (transition_steps - step) * 1.0f;
            const falcon_submotion_data *motion =
                falcon_reference_submotion(
                    source_submotion);

            if (motion == NULL || motion->animation_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            while (step_frame_f32 < INT32_C(0))
            {
                step_frame_f32 +=
                    (int32_t)motion->animation_frame_count * 1.0f;
            }
            if (!hsd_evaluate_local_pose_f32(
                    data,
                    source_submotion,
                    step_frame_f32,
                    step_target) ||
                !hsd_blend_local_pose_f32(
                    data,
                    step_target,
                    current,
                    (int32_t)(INT32_C(6) - step) * 1.0f /
                        (int32_t)(INT32_C(7) - step),
                    next))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            (void)memcpy(current, next, sizeof(*next) * data->joint_count);
        }
        progress_f32 = transition_steps * 1.0f;
    }
    else if (world->ground_blend_progress_f32[player_index] > INT32_C(0) &&
             action == previous_action &&
             source_submotion == world->source_submotion[player_index])
    {
        float frame_delta_f32 =
            source_animation_frame_f32 -
            world->source_animation_frame_f32[player_index];
        const falcon_submotion_data *motion =
            falcon_reference_submotion(
                source_submotion);

        if (motion == NULL || motion->animation_frame_count == UINT16_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (frame_delta_f32 < INT32_C(0))
        {
            frame_delta_f32 +=
                (int32_t)motion->animation_frame_count * 1.0f;
        }
        progress_f32 = world->ground_blend_progress_f32[player_index] +
                       frame_delta_f32;
        if (progress_f32 < INT32_C(6) * 1.0f &&
            world->ground_blend_pose[player_index].mode ==
                (uint8_t)PF_M4_HSD_COMPACT_POSE_REPLAY)
        {
            *out_pose = world->ground_blend_pose[player_index];
            *out_progress_f32 = progress_f32;
            return PF_STATUS_OK;
        }
        else if (progress_f32 < INT32_C(6) * 1.0f)
        {
            if (!falcon_continue_ground_blend_pose(
                    data,
                    target,
                    &world->ground_blend_pose[player_index],
                    world->ground_blend_progress_f32[player_index],
                    frame_delta_f32,
                    result,
                    &progress_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
    }
    if (progress_f32 > INT32_C(0) &&
        progress_f32 < INT32_C(6) * 1.0f)
    {
        if (transition_steps > 0)
        {
            (void)memcpy(
                result,
                current,
                sizeof(*current) * data->joint_count);
        }
        if (!hsd_pack_compact_pose_f32(
                data,
                result,
                out_pose))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *out_progress_f32 = progress_f32;
    }
    else
    {
        (void)memset(out_pose, 0, sizeof(*out_pose));
        *out_progress_f32 = INT32_C(0);
    }
    return PF_STATUS_OK;
}

static int advance_falcon_source_submotion(
    uint16_t *submotion_index,
    uint16_t *action_ticks)
{
    const falcon_submotion_data *submotion =
        falcon_reference_submotion(*submotion_index);
    uint16_t next_ticks;

    if (submotion == NULL || submotion->animation_frame_count == UINT16_C(0))
    {
        return 0;
    }
    next_ticks = (uint16_t)(*action_ticks + UINT16_C(1));
    if (next_ticks < submotion->animation_frame_count)
    {
        *action_ticks = next_ticks;
        return 1;
    }

    switch (*submotion_index)
    {
    case PF_M4_FALCON_SUBMOTION_JUMP_FORWARD:
    case PF_M4_FALCON_SUBMOTION_JUMP_BACKWARD:
        *submotion_index = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
        break;
    case PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD:
    case PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD:
        *submotion_index =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL;
        break;
    case PF_M4_FALCON_SUBMOTION_PLATFORM_DROP:
        *submotion_index = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
        break;
    case PF_M4_FALCON_SUBMOTION_FALL:
    case PF_M4_FALCON_SUBMOTION_FALL_FORWARD:
    case PF_M4_FALCON_SUBMOTION_FALL_BACKWARD:
    case PF_M4_FALCON_SUBMOTION_FALL_AERIAL:
    case PF_M4_FALCON_SUBMOTION_FALL_AERIAL_FORWARD:
    case PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD:
        break;
    default:
        return 0;
    }
    *action_ticks = UINT16_C(0);
    return 1;
}

static uint16_t ground_damage_submotion(uint8_t action_state)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_DAMAGE_LOW_1:
            return PF_M4_FALCON_SUBMOTION_DAMAGE_LOW_1;
        case PF_M4_ACTION_DAMAGE_LOW_2:
            return (uint16_t)(PF_M4_FALCON_SUBMOTION_DAMAGE_LOW_1 +
                              UINT16_C(1));
        case PF_M4_ACTION_DAMAGE_LOW_3:
            return (uint16_t)(PF_M4_FALCON_SUBMOTION_DAMAGE_LOW_1 +
                              UINT16_C(2));
        default:
            return UINT16_MAX;
    }
}

static pf_status advance_ground_damage_animation(
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t hitstun_ticks_value,
    float *ground_knockback_velocity_f32)
{
    const uint16_t submotion_index =
        ground_damage_submotion(*action_state);
    const falcon_submotion_data *motion =
        submotion_index != UINT16_MAX
            ? falcon_reference_submotion(submotion_index)
            : NULL;

    if (motion == NULL || motion->gameplay_frame_count == UINT16_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (*action_ticks < UINT16_MAX)
    {
        ++*action_ticks;
    }
    if (*action_ticks >= motion->gameplay_frame_count &&
        hitstun_ticks_value == UINT16_C(0))
    {
        *action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        *action_ticks = UINT16_C(0);
        *ground_knockback_velocity_f32 = INT32_C(0);
    }
    return PF_STATUS_OK;
}

static pf_status advance_retained_damage_animation(
    uint16_t source_submotion,
    uint8_t grounded,
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t hitstun_ticks_value,
    uint16_t *next_submotion)
{
    const falcon_submotion_data *motion =
        falcon_reference_submotion(source_submotion);

    if (action_state == NULL || action_ticks == NULL ||
        next_submotion == NULL ||
        source_submotion <
            (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_HIGH_1 ||
        source_submotion >
            (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL ||
        motion == NULL || motion->gameplay_frame_count == UINT16_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (*action_ticks < UINT16_MAX)
    {
        ++*action_ticks;
    }
    if (*action_ticks >= motion->gameplay_frame_count &&
        hitstun_ticks_value == UINT16_C(0))
    {
        *action_state =
            grounded != UINT8_C(0)
                ? (uint8_t)PF_M4_ACTION_GROUND_IDLE
                : (uint8_t)PF_M4_ACTION_AIRBORNE;
        *action_ticks = UINT16_C(0);
        *next_submotion =
            grounded != UINT8_C(0)
                ? (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT
                : (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    }
    return PF_STATUS_OK;
}

static float clamp_f32(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static float shield_health_add(
    float health_f32,
    float amount_f32,
    float maximum_f32)
{
    if (health_f32 >= maximum_f32 ||
        amount_f32 >= maximum_f32 - health_f32)
    {
        return maximum_f32;
    }
    return health_f32 + amount_f32;
}

static float shield_health_subtract(
    float health_f32,
    float amount_f32)
{
    return amount_f32 >= health_f32
               ? 0.0f
               : health_f32 - amount_f32;
}

static uint16_t input_shield_strength(
    const fighter_data *fighter,
    const pf_input_frame *input)
{
    const uint16_t raw_strength =
        input->left_trigger >= input->right_trigger
            ? input->left_trigger
            : input->right_trigger;
    const uint32_t dead_zone =
        (uint32_t)fighter->light_shield_trigger_threshold - UINT32_C(1);

    if (raw_strength < fighter->light_shield_trigger_threshold)
    {
        return UINT16_C(0);
    }
    return (uint16_t)(
        ((uint32_t)raw_strength - dead_zone) * (uint32_t)UINT16_MAX /
        ((uint32_t)UINT16_MAX - dead_zone));
}

static float lerp_f32(
    float low,
    float high,
    uint16_t value,
    uint16_t low_value,
    uint16_t high_value)
{
    if (value <= low_value || high <= low)
    {
        return low;
    }
    if (value >= high_value)
    {
        return high;
    }
    return low + (high - low) *
                     (float)(value - low_value) /
                     (float)(high_value - low_value);
}

static float shield_hold_depletion_f32(
    const fighter_data *fighter,
    uint16_t shield_strength)
{
    return lerp_f32(
        fighter->light_shield_hold_depletion_f32,
        fighter->shield_hold_depletion_f32,
        shield_strength,
        UINT16_C(0),
        UINT16_MAX);
}

static int action_retains_shield_strength(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
            hitlag_resume_action ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN);
}

static int action_freezes_shield_strength(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
            hitlag_resume_action ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN);
}

static uint16_t shield_break_stun_ticks(
    const fighter_data *fighter,
    float damage_f32)
{
    const uint32_t damage_percent =
        damage_f32 > 0.0f ? (uint32_t)floorf(damage_f32) : UINT32_C(0);
    const uint32_t maximum_reduction =
        (uint32_t)fighter->shield_break_stun_ticks -
        (uint32_t)fighter->shield_break_minimum_stun_ticks;

    return damage_percent >= maximum_reduction
               ? fighter->shield_break_minimum_stun_ticks
               : (uint16_t)(
                     (uint32_t)fighter->shield_break_stun_ticks -
                     damage_percent);
}

static float scale_axis_f32(
    int16_t axis,
    float magnitude_f32)
{
    const float denominator =
        axis < INT16_C(0) ? 32768.0f : 32767.0f;

    return (float)axis * magnitude_f32 / denominator;
}

static uint16_t axis_magnitude(int16_t axis)
{
    if (axis == INT16_MIN)
    {
        return UINT16_C(32768);
    }
    if (axis < INT16_C(0))
    {
        return (uint16_t)(-axis);
    }
    return (uint16_t)axis;
}

static const int16_t sine_q15_table[65] = {
    INT16_C(0), INT16_C(804), INT16_C(1608), INT16_C(2410),
    INT16_C(3212), INT16_C(4011), INT16_C(4808), INT16_C(5602),
    INT16_C(6393), INT16_C(7179), INT16_C(7962), INT16_C(8739),
    INT16_C(9512), INT16_C(10278), INT16_C(11039), INT16_C(11793),
    INT16_C(12539), INT16_C(13279), INT16_C(14010), INT16_C(14732),
    INT16_C(15446), INT16_C(16151), INT16_C(16846), INT16_C(17530),
    INT16_C(18204), INT16_C(18868), INT16_C(19519), INT16_C(20159),
    INT16_C(20787), INT16_C(21403), INT16_C(22005), INT16_C(22594),
    INT16_C(23170), INT16_C(23731), INT16_C(24279), INT16_C(24811),
    INT16_C(25329), INT16_C(25832), INT16_C(26319), INT16_C(26790),
    INT16_C(27245), INT16_C(27683), INT16_C(28105), INT16_C(28510),
    INT16_C(28898), INT16_C(29268), INT16_C(29621), INT16_C(29956),
    INT16_C(30273), INT16_C(30571), INT16_C(30852), INT16_C(31113),
    INT16_C(31356), INT16_C(31580), INT16_C(31785), INT16_C(31971),
    INT16_C(32137), INT16_C(32285), INT16_C(32412), INT16_C(32521),
    INT16_C(32609), INT16_C(32678), INT16_C(32728), INT16_C(32757),
    INT16_C(32767)};

static int32_t sine_q15(uint16_t angle_turn)
{
    const uint32_t quadrant = (uint32_t)angle_turn >> 14U;
    uint32_t quarter_turn = (uint32_t)angle_turn & UINT32_C(16383);
    uint32_t position;
    uint32_t index;
    uint32_t fraction;
    int32_t lower;
    int32_t upper;
    int32_t value;

    if ((quadrant & UINT32_C(1)) != UINT32_C(0))
    {
        quarter_turn = UINT32_C(16384) - quarter_turn;
    }
    position = quarter_turn << 2U;
    index = position >> 10U;
    fraction = position & UINT32_C(1023);
    lower = sine_q15_table[index];
    upper = sine_q15_table[
        index < UINT32_C(64) ? index + UINT32_C(1) : index];
    value = lower +
            (int32_t)(((int64_t)(upper - lower) * fraction) /
                      INT64_C(1024));
    return quadrant >= UINT32_C(2) ? -value : value;
}

static float falcon_source_velocity_to_sim_f32(
    float source_velocity_f32,
    int32_t numerator,
    int32_t denominator)
{
    return source_velocity_f32 * (float)numerator / (float)denominator;
}

static void falcon_punch_launch_velocity(
    const falcon_special_attributes *attributes,
    int16_t stick_y,
    int8_t facing,
    float *out_velocity_x_f32,
    float *out_velocity_y_f32)
{
    const float stick_magnitude_f32 =
        (float)axis_magnitude(stick_y) / 32768.0f;
    const float bounded_stick_f32 =
        stick_magnitude_f32 >
                attributes->specialn_stick_range_y_pos_f32
            ? attributes->specialn_stick_range_y_pos_f32
            : stick_magnitude_f32;
    const float angle_input_f32 =
        bounded_stick_f32 >
                attributes->specialn_stick_range_y_neg_f32
            ? bounded_stick_f32 -
                  attributes->specialn_stick_range_y_neg_f32
            : 0.0f;
    const float angle_range_f32 =
        attributes->specialn_stick_range_y_pos_f32 -
        attributes->specialn_stick_range_y_neg_f32;
    const float angle_degrees_f32 =
        angle_range_f32 > 0.0f
            ? angle_input_f32 * attributes->specialn_angle_diff_f32 /
                  angle_range_f32
            : 0.0f;
    uint16_t angle_turn = (uint16_t)(
        angle_degrees_f32 * 65536.0f / 360.0f);
    const float source_x_f32 =
        falcon_source_velocity_to_sim_f32(
            attributes->specialn_vel_x_f32,
            INT32_C(12),
            INT32_C(115));
    const float source_y_f32 =
        falcon_source_velocity_to_sim_f32(
            attributes->specialn_vel_x_f32,
            INT32_C(11),
            INT32_C(62));

    if (stick_y < INT16_C(0))
    {
        angle_turn = (uint16_t)(UINT16_C(0) - angle_turn);
    }
    *out_velocity_x_f32 =
        (float)facing * source_x_f32 *
        (float)sine_q15(
            (uint16_t)(angle_turn + UINT16_C(16384))) /
        32767.0f;
    *out_velocity_y_f32 =
        -source_y_f32 * (float)sine_q15(angle_turn) / 32767.0f;
}

void shield_tilt_axes(
    uint16_t angle_turn,
    uint16_t magnitude,
    int8_t facing,
    int16_t *out_x,
    int16_t *out_y)
{
    const int32_t local_x_q15 =
        sine_q15((uint16_t)(angle_turn + UINT16_C(16384)));
    const int32_t local_y_q15 = sine_q15(angle_turn);

    *out_x = (int16_t)(
        ((int64_t)local_x_q15 * (int64_t)magnitude * (int64_t)facing) /
        INT64_C(65535));
    *out_y = (int16_t)(
        -((int64_t)local_y_q15 * (int64_t)magnitude) /
        INT64_C(65535));
}

static uint16_t atan2_turn(int32_t y, int32_t x)
{
    const float radians = atan2f((float)y, (float)x);
    const float positive_radians =
        radians < 0.0f ? radians + 6.28318530717958647692f : radians;
    const uint32_t angle = (uint32_t)(
        positive_radians * (65536.0f / 6.28318530717958647692f) +
        0.5f);

    /* GALE01 clamps angles in the final degree to 359 before smoothing. */
    return (uint16_t)(angle > UINT32_C(65354) ? UINT32_C(65354) : angle);
}

static int32_t half_nearest(int32_t value)
{
    return value < INT32_C(0)
               ? -((-value + INT32_C(1)) / INT32_C(2))
               : (value + INT32_C(1)) / INT32_C(2);
}

static uint16_t shield_target_magnitude(const pf_input_frame *input)
{
    float magnitude = hypotf(
        (float)input->main_stick_x,
        (float)input->main_stick_y);

    if (magnitude > 32768.0f)
    {
        magnitude = 32768.0f;
    }
    return (uint16_t)(magnitude * (65535.0f / 32768.0f) + 0.5f);
}

static void update_shield_tilt(
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    int8_t facing)
{
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        const uint16_t target_angle = atan2_turn(
            -(int32_t)input->main_stick_y,
            (int32_t)input->main_stick_x * (int32_t)facing);
        const uint16_t current_angle =
            scratch->shield_angle_turn[player_index];
        int32_t angle_delta =
            (int32_t)target_angle - (int32_t)current_angle;
        const uint16_t target_magnitude =
            shield_target_magnitude(input);

        if (angle_delta > INT32_C(32768))
        {
            angle_delta -= INT32_C(65536);
        }
        else if (angle_delta < INT32_C(-32768))
        {
            angle_delta += INT32_C(65536);
        }
        scratch->shield_angle_turn[player_index] = (uint16_t)(
            (uint32_t)((int32_t)current_angle +
                       half_nearest(angle_delta)) &
            UINT32_C(65535));
        scratch->shield_magnitude[player_index] = (uint16_t)(
            ((uint32_t)scratch->shield_magnitude[player_index] +
             (uint32_t)target_magnitude + UINT32_C(1)) /
            UINT32_C(2));
    }
    else if (!action_retains_shield_strength(
                 action_state,
                 hitlag_resume_action))
    {
        scratch->shield_angle_turn[player_index] = UINT16_C(0);
        scratch->shield_magnitude[player_index] = UINT16_C(0);
    }
}

static int8_t axis_direction(int16_t axis, uint16_t dead_zone)
{
    if (axis > (int16_t)dead_zone)
    {
        return INT8_C(1);
    }
    if (axis < -(int16_t)dead_zone)
    {
        return INT8_C(-1);
    }
    return INT8_C(0);
}

static pf_status enter_air_dodge(
    const fighter_data *fighter,
    int16_t stick_x,
    int16_t stick_y,
    float *velocity_x,
    float *velocity_y)
{
    const uint16_t magnitude_x = axis_magnitude(stick_x);
    const uint16_t magnitude_y = axis_magnitude(stick_y);
    float stick_magnitude;

    if (magnitude_x < fighter->air_dodge_dead_zone &&
        magnitude_y < fighter->air_dodge_dead_zone)
    {
        *velocity_x = 0.0f;
        *velocity_y = 0.0f;
        return PF_STATUS_OK;
    }

    stick_magnitude = hypotf((float)stick_x, (float)stick_y);
    if (stick_magnitude == 0.0f)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    *velocity_x = (float)stick_x * fighter->air_dodge_speed_x_f32 /
                  (float)stick_magnitude;
    *velocity_y = (float)stick_y * fighter->air_dodge_speed_y_f32 /
                  (float)stick_magnitude;
    return PF_STATUS_OK;
}

static int8_t strong_direction(
    int16_t axis,
    uint16_t threshold)
{
    if (axis >= (int16_t)threshold)
    {
        return INT8_C(1);
    }
    if (axis <= -(int16_t)threshold)
    {
        return INT8_C(-1);
    }
    return INT8_C(0);
}

static float ground_input_acceleration(
    const fighter_data *fighter,
    int16_t stick_x,
    float velocity_x,
    float target_x,
    int movement_mode)
{
    const int walk = movement_mode == 1;
    const int run = movement_mode == 2;
    const float stick_acceleration_f32 =
        walk != 0
            ? fighter->walk_initial_velocity_f32
            : fighter->ground_acceleration_f32;
    const float base_acceleration_f32 =
        walk != 0
            ? fighter->walk_acceleration_f32
            : fighter->dash_run_base_acceleration_f32;
    const float taper_f32 =
        walk != 0
            ? fighter->walk_acceleration_taper_f32
            : fighter->run_acceleration_taper_f32;
    float acceleration =
        scale_axis_f32(stick_x, stick_acceleration_f32) +
        (stick_x < INT16_C(0)
             ? -base_acceleration_f32
             : base_acceleration_f32);

    if (walk == 0 &&
        (stick_x >= INT16_C(32767) ||
         stick_x <= INT16_C(-32767)))
    {
        acceleration =
            stick_x < INT16_C(0)
                ? -fighter->turn_acceleration_f32
                : fighter->turn_acceleration_f32;
    }

    if ((walk != 0 || run != 0) &&
        target_x != 0.0f && velocity_x * target_x > 0.0f &&
        ((target_x > 0.0f && velocity_x < target_x) ||
         (target_x < 0.0f && velocity_x > target_x)))
    {
        const float remaining =
            target_x > velocity_x
                ? target_x - velocity_x
                : velocity_x - target_x;
        const float target_magnitude = fabsf(target_x);
        const float factor_f32 =
            remaining * taper_f32 / target_magnitude;

        acceleration *= factor_f32;
    }
    return acceleration;
}

static float apply_ground_input(
    const fighter_data *fighter,
    float velocity_x,
    int16_t stick_x,
    float maximum_speed_f32,
    int movement_mode)
{
    const float target_x =
        scale_axis_f32(stick_x, maximum_speed_f32);
    float acceleration;
    float next;

    if (target_x == 0.0f)
    {
        return approach(
            velocity_x,
            0.0f,
            fighter->traction_f32);
    }
    acceleration = ground_input_acceleration(
        fighter,
        stick_x,
        velocity_x,
        target_x,
        movement_mode);
    next = velocity_x + acceleration;

    if (velocity_x * acceleration >= 0.0f)
    {
        if (acceleration > 0.0f && next > target_x)
        {
            next = velocity_x - fighter->traction_f32;
            if (next < target_x)
            {
                next = target_x;
            }
            if (next > fighter->ground_max_horizontal_speed_f32)
            {
                next = fighter->ground_max_horizontal_speed_f32;
            }
        }
        else if (
            acceleration < 0.0f && next < target_x)
        {
            next = velocity_x + fighter->traction_f32;
            if (next > target_x)
            {
                next = target_x;
            }
            if (next < -fighter->ground_max_horizontal_speed_f32)
            {
                next = -fighter->ground_max_horizontal_speed_f32;
            }
        }
    }
    return next;
}

static float enter_initial_dash_velocity(
    const fighter_data *fighter,
    float velocity_x,
    int8_t direction)
{
    const float impulse =
        (float)direction * fighter->initial_dash_speed_f32;

    if (velocity_x * (float)direction < 0.0f)
    {
        return velocity_x + impulse;
    }
    return impulse;
}

static float apply_air_input(
    const fighter_data *fighter,
    float velocity_x,
    int16_t stick_x,
    float maximum_speed_f32)
{
    const falcon_common_attributes *source_character =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? falcon_reference_common_attributes()
            : NULL;
    const int8_t direction =
        stick_x < INT16_C(0)
            ? INT8_C(-1)
            : (stick_x > INT16_C(0) ? INT8_C(1) : INT8_C(0));
    const float target_x =
        scale_axis_f32(stick_x, maximum_speed_f32);
    float acceleration;
    float next;

    if (source_character != NULL && direction != INT8_C(0))
    {
        acceleration =
            axis_f32(stick_x) *
                source_character->air_mobility_a_f32 +
            (float)direction *
                source_character->air_mobility_b_f32;
    }
    else
    {
        acceleration =
            scale_axis_f32(
                stick_x,
                fighter->air_acceleration_f32) +
            (float)direction * fighter->air_base_acceleration_f32;
    }

    if (direction == INT8_C(0))
    {
        return approach(
            velocity_x,
            0.0f,
            fighter->air_friction_f32);
    }

    next = velocity_x + acceleration;
    if (velocity_x * acceleration >= 0.0f)
    {
        if (acceleration > 0.0f && next > target_x)
        {
            next = velocity_x - fighter->air_friction_f32;
            if (next < target_x)
            {
                next = target_x;
            }
            if (next > fighter->air_max_horizontal_speed_f32)
            {
                next = fighter->air_max_horizontal_speed_f32;
            }
        }
        else if (
            acceleration < 0.0f && next < target_x)
        {
            next = velocity_x + fighter->air_friction_f32;
            if (next > target_x)
            {
                next = target_x;
            }
            if (next < -fighter->air_max_horizontal_speed_f32)
            {
                next = -fighter->air_max_horizontal_speed_f32;
            }
        }
    }
    return next;
}

static float falcon_dive_air_control(
    const fighter_data *fighter,
    const falcon_common_special_attributes *common,
    const falcon_special_attributes *special,
    float axis_f32_value,
    float internal_x_f32,
    float maximum_f32)
{
    float acceleration_f32 = 0.0f;
    float target_f32 = 0.0f;
    float candidate;

    if (internal_x_f32 > maximum_f32 ||
        internal_x_f32 < -maximum_f32)
    {
        return approach(
            internal_x_f32,
            0.0f,
            common->air_drift_over_maximum_deceleration_f32);
    }
    if (axis_f32_value >= common->air_drift_dead_zone_f32 ||
        axis_f32_value <= -common->air_drift_dead_zone_f32)
    {
        acceleration_f32 = multiply_f32(
            axis_f32_value,
            multiply_f32(
                fighter->air_acceleration_f32,
                special->specialhi_air_friction_mul_f32));
        target_f32 = multiply_f32(axis_f32_value, maximum_f32);
    }
    if (target_f32 == 0.0f)
    {
        return 0.0f;
    }
    candidate = internal_x_f32 + acceleration_f32;
    if (internal_x_f32 * acceleration_f32 >= 0.0f)
    {
        if ((acceleration_f32 > 0.0f && candidate > target_f32) ||
            (acceleration_f32 < 0.0f && candidate < target_f32))
        {
            return target_f32;
        }
    }
    return candidate;
}

static pf_status falcon_dive_start_velocity(
    const fighter_data *fighter,
    const pf_input_frame *input,
    uint8_t action_state,
    uint16_t action_ticks,
    int8_t *facing,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    const falcon_common_special_attributes *common;
    const falcon_special_attributes *special;
    const falcon_up_special_timing *timing;
    uint16_t displayed_frame;
    int8_t previous_facing;
    float axis_f32_value;
    float maximum_f32;
    float previous_root_x_f32 = INT32_C(0);
    float root_x_f32;
    float root_y_f32;
    float internal_x_f32;

    if (fighter == NULL || input == NULL || facing == NULL ||
        velocity_x_f32 == NULL || velocity_y_f32 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    common = falcon_reference_common_special_attributes();
    special = falcon_reference_special_attributes();
    timing = falcon_reference_up_special_timing();
    displayed_frame = (uint16_t)(action_ticks + UINT16_C(1));
    previous_facing = *facing;
    axis_f32_value = axis_f32(input->main_stick_x);
    maximum_f32 =
        special != NULL
            ? multiply_f32(
                  fighter->air_speed_f32,
                  special->specialhi_horz_vel_f32)
            : INT32_C(0);
    if (common == NULL || special == NULL || timing == NULL ||
        maximum_f32 <= INT32_C(0) ||
        !falcon_reference_motion_x_f32(
            action_state,
            displayed_frame,
            &root_x_f32) ||
        !falcon_reference_motion_y_f32(
            action_state,
            displayed_frame,
            &root_y_f32) ||
        (action_ticks != UINT16_C(0) &&
         !falcon_reference_motion_x_f32(
             action_state,
             action_ticks,
             &previous_root_x_f32)))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    internal_x_f32 =
        action_ticks == UINT16_C(0)
            ? INT32_C(0)
            : *velocity_x_f32 -
                  (float)previous_facing * previous_root_x_f32;
    /* ftCa_SpecialHiAir_IASA consumes and clears the action-script command
     * variable once. Direction may change on that exact gate frame only;
     * later air-control samples do not repeatedly turn Falcon. */
    if (displayed_frame == timing->air_control_begin_frame &&
        (axis_f32_value > special->specialhi_input_var_f32 ||
         axis_f32_value < -special->specialhi_input_var_f32))
    {
        *facing = axis_f32_value < INT32_C(0) ? INT8_C(-1) : INT8_C(1);
    }
    *velocity_x_f32 =
        (float)*facing * root_x_f32 +
        falcon_dive_air_control(
            fighter,
            common,
            special,
            axis_f32_value,
            internal_x_f32,
            maximum_f32);
    *velocity_y_f32 = root_y_f32;
    return PF_STATUS_OK;
}

static pf_status falcon_dive_throw_velocity(
    const fighter_data *fighter,
    const pf_input_frame *input,
    uint16_t action_ticks,
    int8_t facing,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    const falcon_common_special_attributes *common =
        falcon_reference_common_special_attributes();
    const falcon_special_attributes *special =
        falcon_reference_special_attributes();
    const falcon_up_special_timing *timing =
        falcon_reference_up_special_timing();
    const uint16_t displayed_frame =
        (uint16_t)(action_ticks + UINT16_C(1));
    float root_x_f32;
    float root_y_f32;
    float previous_root_x_f32 = INT32_C(0);
    float previous_root_y_f32 = INT32_C(0);
    float internal_x_f32 = INT32_C(0);
    float internal_y_f32 = INT32_C(0);
    float maximum_f32;

    if (fighter == NULL || input == NULL || velocity_x_f32 == NULL ||
        velocity_y_f32 == NULL || common == NULL || special == NULL ||
        timing == NULL ||
        !falcon_reference_motion_x_f32(
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
            displayed_frame,
            &root_x_f32) ||
        !falcon_reference_motion_y_f32(
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
            displayed_frame,
            &root_y_f32) ||
        (action_ticks != UINT16_C(0) &&
         (!falcon_reference_motion_x_f32(
              (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
              action_ticks,
              &previous_root_x_f32) ||
          !falcon_reference_motion_y_f32(
              (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
              action_ticks,
              &previous_root_y_f32))))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    maximum_f32 = multiply_f32(
        fighter->air_speed_f32,
        special->specialhi_horz_vel_f32);
    if (maximum_f32 <= INT32_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (displayed_frame >= timing->throw_gravity_begin_frame)
    {
        internal_x_f32 =
            *velocity_x_f32 - (float)facing * previous_root_x_f32;
        internal_y_f32 = *velocity_y_f32 - previous_root_y_f32;
        internal_x_f32 = falcon_dive_air_control(
            fighter,
            common,
            special,
            axis_f32(input->main_stick_x),
            internal_x_f32,
            maximum_f32);
        internal_y_f32 = approach(
            internal_y_f32,
            fighter->fall_speed_f32,
            falcon_source_velocity_to_sim_f32(
                special->specialhi_catch_grav_f32,
                INT32_C(11),
                INT32_C(62)));
    }
    *velocity_x_f32 = (float)facing * root_x_f32 + internal_x_f32;
    *velocity_y_f32 = root_y_f32 + internal_y_f32;
    if (*velocity_y_f32 > fighter->fall_speed_f32)
    {
        *velocity_y_f32 = fighter->fall_speed_f32;
    }
    return PF_STATUS_OK;
}

static const struct reference_move *falcon_move_for_action(
    uint8_t action_state)
{
    falcon_move_index move_index;

    return falcon_reference_move_for_action(
               action_state,
               &move_index) != 0
               ? falcon_reference_move(move_index)
               : NULL;
}

static pf_status falcon_kick_root_velocity(
    uint8_t action_state,
    uint16_t action_ticks,
    int8_t facing,
    int include_vertical,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    const uint16_t displayed_frame =
        (uint16_t)(action_ticks + UINT16_C(1));
    float root_x_f32;
    float root_y_f32 = INT32_C(0);

    if (velocity_x_f32 == NULL || velocity_y_f32 == NULL ||
        !falcon_reference_motion_x_f32(
            action_state,
            displayed_frame,
            &root_x_f32) ||
        (include_vertical != 0 &&
         !falcon_reference_motion_y_f32(
             action_state,
             displayed_frame,
             &root_y_f32)))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *velocity_x_f32 = (float)facing * root_x_f32;
    *velocity_y_f32 = root_y_f32;
    return PF_STATUS_OK;
}

static float falcon_kick_hit_velocity_scale(
    const falcon_special_attributes *attributes,
    uint8_t hit_count)
{
    float scale_f32 = 1.0f;

    while (hit_count != UINT8_C(0))
    {
        scale_f32 = multiply_f32(
            scale_f32,
            attributes->speciallw_on_hit_spd_modifier_f32);
        --hit_count;
    }
    return scale_f32;
}

static float falcon_source_ground_friction(
    const falcon_common_attributes *common,
    const falcon_common_special_attributes *common_special,
    float velocity_x_f32)
{
    const float speed_f32 =
        velocity_x_f32 < INT32_C(0)
            ? -velocity_x_f32
            : velocity_x_f32;

    return speed_f32 > common->walk_maximum_velocity_f32
               ? multiply_f32(
                     common->friction_f32,
                     common_special
                         ->fast_ground_friction_multiplier_f32)
               : common->friction_f32;
}

static float stationary_ground_friction(
    const fighter_data *fighter,
    float velocity_x_f32)
{
    const float speed_f32 = velocity_x_f32 < INT32_C(0)
                                  ? -velocity_x_f32
                                  : velocity_x_f32;

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const falcon_common_special_attributes *common_special =
            falcon_reference_common_special_attributes();

        if (common_special != NULL)
        {
            /* ft_80084F3C/ft_80084FA8 share this exact stationary-ground
             * primitive for Guard, GuardOn/Off/SetOff and Appeal. */
            return speed_f32 > fighter->walk_speed_f32
                       ? multiply_f32(
                             fighter->traction_f32,
                             common_special
                                 ->fast_ground_friction_multiplier_f32)
                       : fighter->traction_f32;
        }
    }
    return speed_f32 > fighter->walk_speed_f32
               ? fighter->turn_acceleration_f32
               : fighter->traction_f32;
}

static float apply_initial_dash_iasa_tail(
    const ssbm_ground_input_attributes *ground_input,
    float velocity_x_f32)
{
    return multiply_f32(
        velocity_x_f32,
        1.0f - ground_input->initial_dash_iasa_velocity_decay_f32);
}

static float falcon_kick_parallel_velocity(
    float unscaled_velocity_f32,
    float applied_friction_f32,
    float hit_scale_f32)
{
    return multiply_f32(unscaled_velocity_f32, hit_scale_f32) -
           multiply_f32(
               applied_friction_f32,
               1.0f - hit_scale_f32);
}

static pf_status falcon_kick_ground_end_velocity(
    uint16_t action_ticks,
    int8_t facing,
    uint8_t hit_count,
    int is_entry_frame,
    float *velocity_x_f32)
{
    const struct reference_move *start_move =
        falcon_move_for_action(
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND);
    const falcon_common_attributes *common =
        falcon_reference_common_attributes();
    const falcon_common_special_attributes *common_special =
        falcon_reference_common_special_attributes();
    const falcon_special_attributes *attributes =
        falcon_reference_special_attributes();
    const falcon_down_special_timing *timing =
        falcon_reference_down_special_timing();
    float root_velocity_f32;
    float ignored_velocity_y_f32;
    float unscaled_velocity_f32;
    float applied_friction_f32;
    float hit_scale_f32;
    uint16_t step;

    if (velocity_x_f32 == NULL || start_move == NULL || common == NULL ||
        common_special == NULL || attributes == NULL || timing == NULL ||
        start_move->total_frames == UINT16_C(0) ||
        falcon_kick_root_velocity(
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND,
            (uint16_t)(start_move->total_frames - UINT16_C(1)),
            1,
            0,
            &root_velocity_f32,
            &ignored_velocity_y_f32) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    unscaled_velocity_f32 = multiply_f32(
        root_velocity_f32,
        timing->ground_end_entry_velocity_scale_f32);
    hit_scale_f32 = falcon_kick_hit_velocity_scale(
        attributes,
        hit_count);
    /* Melee advances gr_vel and self_vel in parallel here. Reconstruct the
     * bounded ground channel from imported root motion and friction instead
     * of serializing a duplicate runtime velocity. */
    if (is_entry_frame != 0)
    {
        applied_friction_f32 = falcon_source_ground_friction(
            common,
            common_special,
            root_velocity_f32);
    }
    else
    {
        applied_friction_f32 = INT32_C(0);
        for (step = UINT16_C(0); step <= action_ticks; ++step)
        {
            const uint16_t displayed_frame =
                (uint16_t)(step + UINT16_C(2));
            const float friction_f32 =
                displayed_frame >= timing->ground_end_traction_begin_frame &&
                        displayed_frame <=
                            timing->ground_end_traction_end_frame
                    ? multiply_f32(
                          common->friction_f32,
                          attributes->speciallw_ground_traction_f32)
                    : falcon_source_ground_friction(
                          common,
                          common_special,
                          unscaled_velocity_f32);
            const float next_velocity_f32 = approach(
                unscaled_velocity_f32,
                INT32_C(0),
                friction_f32);

            applied_friction_f32 =
                unscaled_velocity_f32 - next_velocity_f32;
            unscaled_velocity_f32 = next_velocity_f32;
        }
    }
    *velocity_x_f32 = (int32_t)facing *
        falcon_kick_parallel_velocity(
            unscaled_velocity_f32,
            applied_friction_f32,
            hit_scale_f32);
    return PF_STATUS_OK;
}

static void falcon_source_air_physics(
    const falcon_common_attributes *common,
    float *velocity_x_f32,
    float *velocity_y_f32)
{
    *velocity_x_f32 = approach(
        *velocity_x_f32,
        INT32_C(0),
        common->air_friction_f32);
    *velocity_y_f32 = approach(
        *velocity_y_f32,
        common->terminal_velocity_f32,
        common->gravity_f32);
    if (*velocity_y_f32 > common->terminal_velocity_f32)
    {
        *velocity_y_f32 = common->terminal_velocity_f32;
    }
}

static int body_overlaps_horizontal_interval(
    float position_x,
    float half_width,
    float interval_left,
    float interval_right)
{
    return position_x + half_width > interval_left &&
           position_x - half_width < interval_right;
}

float platform_center_x_f32(
    const stage_data *stage,
    uint64_t tick)
{
    const uint64_t period =
        (uint64_t)stage->platform_motion_period_ticks;
    const uint64_t half_period = period / UINT64_C(2);
    const uint64_t phase = tick % period;
    float offset;

    if (phase <= half_period)
    {
        offset = -stage->platform_motion_amplitude_f32 +
                 2.0f * stage->platform_motion_amplitude_f32 *
                     (float)phase / (float)half_period;
    }
    else
    {
        const uint64_t descending_phase = phase - half_period;
        offset = stage->platform_motion_amplitude_f32 -
                 2.0f * stage->platform_motion_amplitude_f32 *
                     (float)descending_phase / (float)half_period;
    }
    return stage->platform_center_x_f32 + offset;
}

static int find_drop_cancel_platform(
    const stage_data *stage,
    const fighter_data *fighter,
    uint64_t tick,
    float position_x_f32,
    float position_y_f32,
    float *out_surface_y_f32,
    uint8_t *out_support)
{
    const float platform_center =
        platform_center_x_f32(stage, tick);
    const float platform_left =
        platform_center - stage->platform_half_width_f32;
    const float platform_right =
        platform_center + stage->platform_half_width_f32;
    const float upper_left =
        stage->upper_platform_center_x_f32 -
        stage->upper_platform_half_width_f32;
    const float upper_right =
        stage->upper_platform_center_x_f32 +
        stage->upper_platform_half_width_f32;
    const float body_bottom = position_y_f32 + fighter->half_height_f32;
    float best_distance = INFINITY;

    if (position_x_f32 >= platform_left &&
        position_x_f32 <= platform_right)
    {
        const float distance = body_bottom - stage->platform_y_f32;

        if (distance >= 0.0f &&
            distance <= fighter->drop_cancel_snap_distance_f32)
        {
            best_distance = distance;
            *out_surface_y_f32 = stage->platform_y_f32;
            *out_support = (uint8_t)PF_M4_SURFACE_PLATFORM;
        }
    }
    if (position_x_f32 >= upper_left &&
        position_x_f32 <= upper_right)
    {
        const float distance = body_bottom - stage->upper_platform_y_f32;

        if (distance >= 0.0f &&
            distance <= fighter->drop_cancel_snap_distance_f32 &&
            distance < best_distance)
        {
            best_distance = distance;
            *out_surface_y_f32 = stage->upper_platform_y_f32;
            *out_support = (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
        }
    }
    return isfinite(best_distance);
}

static float surface_y_f32(
    const struct content *content,
    uint8_t support,
    float position_x_f32)
{
    const ssbm_stage_collision_line *line =
        ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        return ssbm_stage_line_y_f32(line, position_x_f32);
    }
    if (support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        return content->stage.platform_y_f32;
    }
    if (support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
    {
        return content->stage.solid_top_f32;
    }
    if (support == (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM)
    {
        return content->stage.upper_platform_y_f32;
    }
    return content->stage.floor_y_f32;
}

static void surface_ground_projection_f32(
    const struct content *content,
    uint8_t support,
    float *out_x_f32,
    float *out_y_f32)
{
    const ssbm_stage_collision_line *line =
        ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        *out_x_f32 = line->ground_projection_x_f32;
        *out_y_f32 = line->ground_projection_y_f32;
        return;
    }
    *out_x_f32 = 1.0f;
    *out_y_f32 = INT32_C(0);
}

static void project_ground_scalar_f32(
    const struct content *content,
    uint8_t support,
    float scalar_f32,
    float *out_x_f32,
    float *out_y_f32)
{
    float projection_x_f32;
    float projection_y_f32;

    surface_ground_projection_f32(
        content,
        support,
        &projection_x_f32,
        &projection_y_f32);
    *out_x_f32 = multiply_f32(scalar_f32, projection_x_f32);
    *out_y_f32 = multiply_f32(scalar_f32, projection_y_f32);
}

static uint8_t stage_spawn_support(const stage_data *stage)
{
    if (stage->reference_collision_profile !=
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        return (uint8_t)(stage->reference_spawn_line + UINT16_C(1));
    }
    return (uint8_t)PF_M4_SURFACE_FLOOR;
}

static uint16_t damage_fly_ecb_frame_index(uint16_t action_ticks)
{
    return action_ticks < PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT
               ? action_ticks
               : (uint16_t)(
                     PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT -
                     UINT16_C(1));
}

static uint16_t clamped_pose_index(
    uint16_t action_ticks,
    uint16_t frame_count)
{
    return action_ticks < frame_count
               ? action_ticks
               : (uint16_t)(frame_count - UINT16_C(1));
}

static int reference_ecb_pose_f32(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    float inherited_locked_bottom_y_f32,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    float fall_animation_blend_f32,
    uint8_t fall_animation_target_switched,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    float ground_loop_progress_f32,
    const hsd_compact_pose *ground_loop_compact,
    falcon_ecb_pose_f32 *out_pose)
{
    const falcon_collision_pose *pose =
        falcon_reference_collision_pose();
    const falcon_ecb_pose_f32 *prone_pose;
    const falcon_ecb_pose_f32 *guard_pose;
    const falcon_ecb_pose_f32 *airborne_pose;
    uint16_t frame_index;
    float locked_bottom_y_f32 =
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32;
    float retained_hsd_frame_f32;
    int retained_hsd_pose;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) || pose == NULL ||
        out_pose == NULL)
    {
        return 0;
    }
    if (ground_loop_compact != NULL)
    {
        const hsd_pose_data *data =
            falcon_reference_hsd_pose_data();
        hsd_local_pose blended[PF_M4_HSD_POSE_MAX_JOINTS];

        if (data == NULL ||
            !falcon_resolve_compact_hsd_pose(
                source_submotion, source_animation_frame_f32,
                ground_loop_progress_f32, ground_loop_compact, blended) ||
            !falcon_reference_hsd_ground_ecb_pose_from_local_pose(
                blended, out_pose))
        {
            return 0;
        }
        return 1;
    }
    retained_hsd_pose = falcon_reference_retained_hsd_pose(
            action_state,
            source_submotion,
            action_ticks,
            source_animation_frame_f32,
            &retained_hsd_frame_f32);
    if (retained_hsd_pose)
    {
        source_animation_frame_f32 = retained_hsd_frame_f32;
    }
    if (!falcon_direct_hsd_locked_bottom_f32(
            action_state,
            source_animation_frame_f32,
            grounded,
            &locked_bottom_y_f32))
    {
        return 0;
    }
    if (locked_bottom_y_f32 ==
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32 &&
        inherited_locked_bottom_y_f32 !=
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32)
    {
        locked_bottom_y_f32 = inherited_locked_bottom_y_f32;
    }
    if (source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL &&
        falcon_reference_damage_hsd_ecb_pose(
            source_submotion,
            source_animation_frame_f32,
            facing,
            total_velocity_x_f32,
            total_velocity_y_f32,
            grounded != UINT8_C(0),
            locked_bottom_y_f32,
            out_pose))
    {
        return 1;
    }
    if ((action_uses_fall_special_pose(action_state) ||
         (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
          fall_animation_blend_f32 != INT32_C(0))) &&
        falcon_reference_hsd_fall_ecb_pose(
            source_submotion,
            source_animation_frame_f32,
            fall_animation_blend_f32,
            fall_animation_target_switched,
            locked_bottom_y_f32,
            out_pose))
    {
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        frame_index = clamped_pose_index(
            action_ticks > UINT16_C(0)
                ? (uint16_t)(action_ticks - UINT16_C(1))
                : UINT16_C(0),
            PF_M4_FALCON_SHIELD_BREAK_FLY_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_fly[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN)
    {
        frame_index = clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_SHIELD_BREAK_DOWN_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_down_down[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN)
    {
        frame_index = clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_SHIELD_BREAK_STAND_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_stand_down[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA)
    {
        frame_index = clamped_pose_index(
            (uint16_t)(
            source_animation_frame_f32 > 0.0f
                    ? source_animation_frame_f32
                    : INT32_C(0)),
            PF_M4_FALCON_SHIELD_BREAK_STUN_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_stun[frame_index];
        return 1;
    }
    guard_pose = falcon_reference_guard_ecb_pose(
        action_state, source_submotion, action_ticks);
    if (guard_pose != NULL)
    {
        *out_pose = *guard_pose;
        return 1;
    }
    if (falcon_reference_action_hsd_ecb_pose(
            action_state,
            action_ticks,
            grounded,
            locked_bottom_y_f32,
            out_pose))
    {
        return 1;
    }
    if (!(action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
          source_submotion >=
              (uint16_t)PF_M4_FALCON_SUBMOTION_FALL &&
          source_submotion <=
              (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD) &&
        falcon_reference_hsd_ecb_pose(
            source_submotion,
            action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                    !falcon_wait_hsd_pose_is_direct(
                        source_submotion,
                        source_animation_frame_f32)
                ? INT32_C(0)
                : source_animation_frame_f32,
            (action_uses_direct_hsd_pose(action_state) ||
             retained_hsd_pose)
                ? grounded != UINT8_C(0)
                : 1,
            locked_bottom_y_f32,
            out_pose))
    {
        return 1;
    }
    airborne_pose =
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE
            ? falcon_reference_airborne_ecb_pose(
                  source_submotion,
                  action_ticks)
            : NULL;
    if (airborne_pose != NULL)
    {
        *out_pose = *airborne_pose;
        return falcon_reference_ecb_apply_bottom_lock_f32(
            inherited_locked_bottom_y_f32,
            out_pose);
    }
    prone_pose = falcon_reference_prone_ecb_pose(
        action_state,
        action_ticks,
        prone_orientation,
        prone_roll_motion_orientation,
        tech_direction,
        facing);
    if (prone_pose != NULL)
    {
        *out_pose = *prone_pose;
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
    {
        frame_index = damage_fly_ecb_frame_index(action_ticks);
        out_pose->top_x_from_origin_f32 = INT32_C(0);
        out_pose->top_y_from_origin_f32 =
            pose->damage_fly_top_y_from_origin_f32[frame_index];
        out_pose->bottom_x_from_origin_f32 = INT32_C(0);
        out_pose->bottom_y_from_origin_f32 =
            pose->damage_fly_bottom_y_from_origin_f32[frame_index];
        out_pose->right_x_from_origin_f32 =
            pose->damage_fly_side_x_from_origin_f32[frame_index];
        out_pose->right_y_from_origin_f32 =
            pose->damage_fly_side_y_from_origin_f32[frame_index];
        out_pose->left_x_from_origin_f32 =
            -pose->damage_fly_side_x_from_origin_f32[frame_index];
        out_pose->left_y_from_origin_f32 =
            pose->damage_fly_side_y_from_origin_f32[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_CEILING_BOUNCE)
    {
        frame_index = clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT);
        *out_pose = pose->ceiling_bounce[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_WALL_BOUNCE)
    {
        frame_index = clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT);
        *out_pose = pose->wall_bounce[frame_index];
        return 1;
    }
    return 0;
}

static void ecb_world_wall_side_f32(
    const falcon_ecb_pose_f32 *pose,
    float body_center_from_source_root_f32,
    int8_t facing,
    int moving_right,
    float *out_x_from_origin_f32,
    float *out_y_from_origin_f32)
{
    if (moving_right != 0)
    {
        if (facing >= INT8_C(0))
        {
            *out_x_from_origin_f32 = pose->right_x_from_origin_f32;
            *out_y_from_origin_f32 =
                pose->right_y_from_origin_f32 -
                body_center_from_source_root_f32;
        }
        else
        {
            *out_x_from_origin_f32 = -pose->left_x_from_origin_f32;
            *out_y_from_origin_f32 =
                pose->left_y_from_origin_f32 -
                body_center_from_source_root_f32;
        }
    }
    else if (facing >= INT8_C(0))
    {
        *out_x_from_origin_f32 = pose->left_x_from_origin_f32;
        *out_y_from_origin_f32 =
            pose->left_y_from_origin_f32 -
            body_center_from_source_root_f32;
    }
    else
    {
        *out_x_from_origin_f32 = -pose->right_x_from_origin_f32;
        *out_y_from_origin_f32 =
            pose->right_y_from_origin_f32 -
            body_center_from_source_root_f32;
    }
}

static float line_x_at_y_f32(
    const ssbm_stage_collision_line *line,
    float y_f32)
{
    const float dx = line->end_x_f32 - line->start_x_f32;
    const float dy = line->end_y_f32 - line->start_y_f32;

    return dy == 0.0f
               ? line->start_x_f32
               : line->start_x_f32 +
                     (y_f32 - line->start_y_f32) * dx / dy;
}

static float ecb_wall_boundary_x_at_y_f32(
    float top_y_f32,
    float side_x_f32,
    float side_y_f32,
    float bottom_y_f32,
    float y_f32)
{
    if (y_f32 <= side_y_f32)
    {
        const float height = side_y_f32 - top_y_f32;
        return height == 0.0f
                   ? side_x_f32
                   : side_x_f32 * (y_f32 - top_y_f32) / height;
    }
    {
        const float height = bottom_y_f32 - side_y_f32;
        return height == 0.0f
                   ? side_x_f32
                   : side_x_f32 -
                         side_x_f32 * (y_f32 - side_y_f32) / height;
    }
}

static float reference_resolve_wall_ecb_f32(
    uint16_t profile_id,
    float previous_position_x_f32,
    float previous_position_y_f32,
    const falcon_ecb_pose_f32 *previous_pose,
    float position_y_f32,
    const falcon_ecb_pose_f32 *pose,
    float body_center_from_source_root_f32,
    int8_t facing,
    int moving_right,
    float *position_x_f32,
    uint8_t *out_support,
    int8_t *out_away_direction)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    float previous_side_x_f32;
    float previous_side_y_from_origin_f32;
    float side_x_f32;
    float side_y_from_origin_f32;
    float previous_top_y_f32;
    float previous_bottom_y_f32;
    float top_y_f32;
    float side_y_f32;
    float bottom_y_f32;
    float swept_left_f32;
    float swept_right_f32;
    float swept_top_f32;
    float swept_bottom_f32;
    float nearest_constraint_f32 =
        moving_right != 0 ? INFINITY : -INFINITY;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t range_start;
    uint16_t range_count;
    uint16_t offset;

    if (profile == NULL || previous_pose == NULL || pose == NULL ||
        position_x_f32 == NULL || out_support == NULL ||
        out_away_direction == NULL)
    {
        return 0;
    }
    ecb_world_wall_side_f32(
        previous_pose,
        body_center_from_source_root_f32,
        facing,
        moving_right,
        &previous_side_x_f32,
        &previous_side_y_from_origin_f32);
    ecb_world_wall_side_f32(
        pose,
        body_center_from_source_root_f32,
        facing,
        moving_right,
        &side_x_f32,
        &side_y_from_origin_f32);
    previous_top_y_f32 =
        previous_position_y_f32 -
        (previous_pose->top_y_from_origin_f32 -
         body_center_from_source_root_f32);
    previous_bottom_y_f32 =
        previous_position_y_f32 +
        body_center_from_source_root_f32 -
        previous_pose->bottom_y_from_origin_f32;
    top_y_f32 =
        position_y_f32 -
        (pose->top_y_from_origin_f32 -
         body_center_from_source_root_f32);
    side_y_f32 = position_y_f32 - side_y_from_origin_f32;
    bottom_y_f32 =
        position_y_f32 + body_center_from_source_root_f32 -
        pose->bottom_y_from_origin_f32;
    swept_left_f32 = previous_position_x_f32;
    swept_right_f32 = previous_position_x_f32;
    if (previous_position_x_f32 + previous_side_x_f32 < swept_left_f32)
    {
        swept_left_f32 =
            previous_position_x_f32 + previous_side_x_f32;
    }
    if (previous_position_x_f32 + previous_side_x_f32 > swept_right_f32)
    {
        swept_right_f32 =
            previous_position_x_f32 + previous_side_x_f32;
    }
    if (*position_x_f32 < swept_left_f32)
    {
        swept_left_f32 = *position_x_f32;
    }
    if (*position_x_f32 > swept_right_f32)
    {
        swept_right_f32 = *position_x_f32;
    }
    if (*position_x_f32 + side_x_f32 < swept_left_f32)
    {
        swept_left_f32 = *position_x_f32 + side_x_f32;
    }
    if (*position_x_f32 + side_x_f32 > swept_right_f32)
    {
        swept_right_f32 = *position_x_f32 + side_x_f32;
    }
    swept_top_f32 =
        previous_top_y_f32 < top_y_f32
            ? previous_top_y_f32
            : top_y_f32;
    swept_bottom_f32 =
        previous_bottom_y_f32 > bottom_y_f32
            ? previous_bottom_y_f32
            : bottom_y_f32;
    range_start =
        moving_right != 0
            ? profile->left_wall_start
            : profile->right_wall_start;
    range_count =
        moving_right != 0
            ? profile->left_wall_count
            : profile->right_wall_count;

    for (offset = UINT16_C(0); offset < range_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(range_start + offset);
        const ssbm_stage_collision_line *line;
        const uint8_t expected_kind =
            moving_right != 0
                ? (uint8_t)PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL
                : (uint8_t)PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL;
        float line_left_f32;
        float line_right_f32;
        float line_top_f32;
        float line_bottom_f32;
        float candidate_y_f32[5];
        float line_constraint_f32 =
            moving_right != 0 ? INFINITY : -INFINITY;
        uint8_t candidate_index;
        int has_constraint = 0;

        if (line_index >= profile->line_count ||
            line_index >= UINT8_MAX)
        {
            return 0;
        }
        line = &profile->lines[line_index];
        if (line->kind != expected_kind ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0))
        {
            continue;
        }
        line_left_f32 =
            line->start_x_f32 < line->end_x_f32
                ? line->start_x_f32
                : line->end_x_f32;
        line_right_f32 =
            line->start_x_f32 > line->end_x_f32
                ? line->start_x_f32
                : line->end_x_f32;
        line_top_f32 =
            line->start_y_f32 < line->end_y_f32
                ? line->start_y_f32
                : line->end_y_f32;
        line_bottom_f32 =
            line->start_y_f32 > line->end_y_f32
                ? line->start_y_f32
                : line->end_y_f32;
        if (line_right_f32 < swept_left_f32 ||
            line_left_f32 > swept_right_f32 ||
            line_bottom_f32 < swept_top_f32 ||
            line_top_f32 > swept_bottom_f32)
        {
            continue;
        }
        candidate_y_f32[0] = top_y_f32;
        candidate_y_f32[1] = side_y_f32;
        candidate_y_f32[2] = bottom_y_f32;
        candidate_y_f32[3] = line->start_y_f32;
        candidate_y_f32[4] = line->end_y_f32;
        for (candidate_index = UINT8_C(0);
             candidate_index < UINT8_C(5);
             ++candidate_index)
        {
            const float y_f32 = candidate_y_f32[candidate_index];
            float constraint_f32;

            if (y_f32 < line_top_f32 ||
                y_f32 > line_bottom_f32 ||
                y_f32 < top_y_f32 ||
                y_f32 > bottom_y_f32)
            {
                continue;
            }
            constraint_f32 =
                line_x_at_y_f32(line, y_f32) -
                ecb_wall_boundary_x_at_y_f32(
                    top_y_f32,
                    side_x_f32,
                    side_y_f32,
                    bottom_y_f32,
                    y_f32);
            if (!has_constraint ||
                (moving_right != 0 &&
                 constraint_f32 < line_constraint_f32) ||
                (moving_right == 0 &&
                 constraint_f32 > line_constraint_f32))
            {
                line_constraint_f32 = constraint_f32;
                has_constraint = 1;
            }
        }
        if (has_constraint &&
            ((moving_right != 0 &&
              *position_x_f32 > line_constraint_f32) ||
             (moving_right == 0 &&
              *position_x_f32 < line_constraint_f32)) &&
            (nearest_support == UINT8_C(0) ||
             (moving_right != 0 &&
              line_constraint_f32 < nearest_constraint_f32) ||
             (moving_right == 0 &&
              line_constraint_f32 > nearest_constraint_f32)))
        {
            nearest_constraint_f32 = line_constraint_f32;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *position_x_f32 = nearest_constraint_f32;
    *out_support = nearest_support;
    *out_away_direction =
        moving_right != 0 ? INT8_C(-1) : INT8_C(1);
    return 1;
}

static float floor_contact_bottom_extent_f32(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    float inherited_locked_bottom_y_f32,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    float fall_animation_blend_f32,
    uint8_t fall_animation_target_switched,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    int *out_exact_reference_pose)
{
    const falcon_collision_pose *pose =
        falcon_reference_collision_pose();
    float bottom_y_from_origin_f32 = INT32_C(0);
    int has_reference_pose = 0;
    int exact_reference_pose = 0;
    falcon_ecb_pose_f32 action_pose;
    const falcon_ecb_pose_f32 *airborne_pose = NULL;

    *out_exact_reference_pose = 0;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) || pose == NULL)
    {
        return fighter->half_height_f32;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        airborne_pose = falcon_reference_airborne_ecb_pose(
            source_submotion,
            action_ticks);
    }
    if (reference_ecb_pose_f32(
            fighter,
            action_state,
            action_ticks,
            grounded,
            inherited_locked_bottom_y_f32,
            source_submotion,
            source_animation_frame_f32,
            fall_animation_blend_f32,
            fall_animation_target_switched,
            prone_orientation,
            prone_roll_motion_orientation,
            tech_direction,
            facing,
            total_velocity_x_f32,
            total_velocity_y_f32,
            INT32_C(0),
            NULL,
            &action_pose) != 0)
    {
        bottom_y_from_origin_f32 = action_pose.bottom_y_from_origin_f32;
        has_reference_pose = 1;
        exact_reference_pose = 1;
    }
    else if (airborne_pose != NULL)
    {
        bottom_y_from_origin_f32 = airborne_pose->bottom_y_from_origin_f32;
        has_reference_pose = 1;
        exact_reference_pose = 1;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
             source_submotion ==
                 (uint16_t)PF_M4_FALCON_SUBMOTION_PLATFORM_DROP)
    {
        const uint16_t frame_index =
            action_ticks < PF_M4_FALCON_PLATFORM_DROP_ECB_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(
                      PF_M4_FALCON_PLATFORM_DROP_ECB_FRAME_COUNT -
                      UINT16_C(1));

        bottom_y_from_origin_f32 =
            pose->platform_drop_bottom_y_from_origin_f32[frame_index];
        has_reference_pose = 1;
        exact_reference_pose = 1;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
             action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW)
    {
        bottom_y_from_origin_f32 = pose->falling_bottom_y_from_origin_f32;
        has_reference_pose = 1;
    }
    if (has_reference_pose != 0 &&
        bottom_y_from_origin_f32 >= INT32_C(0))
    {
        *out_exact_reference_pose = exact_reference_pose;
        return fighter->half_height_f32 - bottom_y_from_origin_f32;
    }
    return fighter->half_height_f32;
}

static float reference_ecb_lock_bottom_from_world_f32(
    const fighter_data *fighter,
    const pf_world_state *world,
    uint32_t player_index,
    float previous_locked_bottom_y_f32)
{
    int previous_exact_pose = 0;

    if (previous_locked_bottom_y_f32 !=
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32)
    {
        return previous_locked_bottom_y_f32;
    }

    /* ftCommon_8007D5D4 preserves CollData.desired_ecb.bottom from the
     * preceding map update. Canonical positions use centre space, so convert
     * the prior sweep extent back to the source root-space ordinate. */
    const float previous_floor_contact_bottom_extent_f32 =
        floor_contact_bottom_extent_f32(
            fighter,
            effective_action_state(
                world->action_state[player_index],
                world->hitlag_resume_action[player_index]),
            world->action_ticks[player_index],
            world->grounded[player_index],
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
            world->source_submotion[player_index],
            world->source_animation_frame_f32[player_index],
            world->fall_animation_blend_f32[player_index],
            world->fall_animation_target_switched[player_index],
            world->prone_orientation[player_index],
            world->prone_roll_motion_orientation[player_index],
            world->tech_direction[player_index],
            world->facing[player_index],
            total_velocity_f32(
                world->velocity_x_f32[player_index],
                world->knockback_velocity_x_f32[player_index]),
            total_velocity_f32(
                world->velocity_y_f32[player_index],
                world->knockback_velocity_y_f32[player_index]),
            &previous_exact_pose);

    (void)previous_exact_pose;
    return fighter->half_height_f32 -
           previous_floor_contact_bottom_extent_f32;
}

static uint8_t down_bound_floor_contact(
    uint8_t prone_orientation,
    uint16_t action_ticks)
{
    const falcon_collision_pose *pose =
        falcon_reference_collision_pose();
    uint32_t contact_mask;
    const uint16_t displayed_frame = action_ticks + UINT16_C(1);

    if (pose == NULL ||
        displayed_frame > PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT)
    {
        return UINT8_C(1);
    }
    contact_mask = pose->down_bound_floor_contact_mask[
        prone_orientation == (uint8_t)PF_M4_PRONE_BACK
            ? UINT16_C(0)
            : UINT16_C(1)];
    return (contact_mask &
            (UINT32_C(1) << (displayed_frame - UINT16_C(1)))) != UINT32_C(0)
               ? UINT8_C(1)
               : UINT8_C(0);
}

static int surface_is_pass_through(
    const struct content *content,
    uint8_t support)
{
    const ssbm_stage_collision_line *line =
        ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        const uint8_t property_flags =
            (uint8_t)(line->property_material_flags >> UINT16_C(8));
        return (property_flags & UINT8_C(1)) != UINT8_C(0);
    }
    return support == (uint8_t)PF_M4_SURFACE_PLATFORM ||
           support == (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
}

static void surface_bounds_f32(
    const struct content *content,
    uint8_t support,
    uint64_t tick,
    float *out_left,
    float *out_right)
{
    const ssbm_stage_collision_line *line =
        ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        *out_left = line->start_x_f32 < line->end_x_f32
                        ? line->start_x_f32
                        : line->end_x_f32;
        *out_right = line->start_x_f32 > line->end_x_f32
                         ? line->start_x_f32
                         : line->end_x_f32;
        return;
    }
    if (support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        const float center =
            platform_center_x_f32(&content->stage, tick);
        *out_left = center - content->stage.platform_half_width_f32;
        *out_right = center + content->stage.platform_half_width_f32;
    }
    else if (support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
    {
        *out_left = content->stage.solid_left_f32;
        *out_right = content->stage.solid_right_f32;
    }
    else if (support == (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM)
    {
        *out_left =
            content->stage.upper_platform_center_x_f32 -
            content->stage.upper_platform_half_width_f32;
        *out_right =
            content->stage.upper_platform_center_x_f32 +
            content->stage.upper_platform_half_width_f32;
    }
    else
    {
        *out_left = content->stage.floor_left_f32;
        *out_right = content->stage.floor_right_f32;
    }
}

static int reference_connected_floor_support(
    uint16_t profile_id,
    uint8_t initial_support,
    float position_x_f32,
    uint8_t *out_support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    uint16_t line_index = (uint16_t)initial_support - UINT16_C(1);
    uint16_t traversal_count;

    if (profile == NULL || initial_support == UINT8_C(0) ||
        out_support == NULL || line_index >= profile->line_count)
    {
        return 0;
    }
    for (traversal_count = UINT16_C(0);
         traversal_count < profile->line_count;
         ++traversal_count)
    {
        const ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const float left =
            line->start_x_f32 < line->end_x_f32
                ? line->start_x_f32
                : line->end_x_f32;
        const float right =
            line->start_x_f32 > line->end_x_f32
                ? line->start_x_f32
                : line->end_x_f32;
        const int crossed_start =
            position_x_f32 < left
                ? line->start_x_f32 <= line->end_x_f32
                : line->start_x_f32 >= line->end_x_f32;
        const int16_t next_line =
            crossed_start != 0
                ? line->previous_line
                : line->next_line;

        if (line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0))
        {
            return 0;
        }
        if (position_x_f32 >= left && position_x_f32 <= right)
        {
            *out_support = (uint8_t)(line_index + UINT16_C(1));
            return 1;
        }
        if (next_line < INT16_C(0) ||
            (uint16_t)next_line >= profile->line_count ||
            (uint16_t)next_line == line_index)
        {
            return 0;
        }
        line_index = (uint16_t)next_line;
    }
    return 0;
}

typedef enum pass_through_floor_sweep_policy
{
    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT = 0,
    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DEFERRED = 1,
    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT_OR_DEFERRED = 2
} pass_through_floor_sweep_policy;

static int floor_sweep_crosses_surface(
    float previous_floor_contact_f32,
    float new_floor_contact_f32,
    float surface_y_f32_value,
    int is_pass_through,
    uint8_t fast_fall,
    pass_through_floor_sweep_policy pass_through_policy)
{
    const int direct_crossing =
        previous_floor_contact_f32 <= surface_y_f32_value &&
        new_floor_contact_f32 >= surface_y_f32_value;

    if (is_pass_through != 0 && fast_fall == UINT8_C(0) &&
        pass_through_policy != PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT)
    {
        const float previous_overshoot =
            previous_floor_contact_f32 - surface_y_f32_value;
        const float current_displacement =
            new_floor_contact_f32 - previous_floor_contact_f32;

        const int immediately_previous_crossing =
            previous_overshoot > 0.0f &&
            current_displacement > 0.0f &&
            previous_overshoot <= current_displacement;

        /* Melee reports some one-way-platform contacts on the update after
         * the swept ECB bottom first crosses the line.  The approximate
         * ordinary-airborne adapter requires that deferred result, while an
         * imported animated ECB can legitimately report either the direct or
         * immediately previous crossing.  Bounding the latter by one current
         * displacement prevents already-below fighters from teleporting up. */
        return pass_through_policy ==
                       PF_M4_PASS_THROUGH_FLOOR_SWEEP_DEFERRED
                   ? immediately_previous_crossing
                   : direct_crossing || immediately_previous_crossing;
    }
    return direct_crossing;
}

static int reference_stage_find_floor_landing(
    const struct content *content,
    float position_x_f32,
    float previous_floor_contact_f32,
    float new_floor_contact_f32,
    uint8_t fast_fall,
    pass_through_floor_sweep_policy pass_through_policy,
    int pass_through_allowed,
    uint8_t platform_drop_ticks,
    float *out_surface_y_f32,
    uint8_t *out_support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(
            content->stage.reference_collision_profile);
    float best_surface_y_f32 = INFINITY;
    uint16_t floor_offset;

    if (profile == NULL || out_surface_y_f32 == NULL ||
        out_support == NULL)
    {
        return 0;
    }
    for (floor_offset = UINT16_C(0);
         floor_offset < profile->floor_count;
         ++floor_offset)
    {
        const uint16_t line_index =
            profile->floor_start + floor_offset;
        const ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const uint8_t support = (uint8_t)(line_index + UINT16_C(1));
        const float left =
            line->start_x_f32 < line->end_x_f32
                ? line->start_x_f32
                : line->end_x_f32;
        const float right =
            line->start_x_f32 > line->end_x_f32
                ? line->start_x_f32
                : line->end_x_f32;
        const int is_pass_through =
            surface_is_pass_through(content, support);
        float surface_y_f32_value;
        int crosses_surface;

        if ((line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0) ||
            line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
            position_x_f32 < left || position_x_f32 > right ||
            (is_pass_through != 0 &&
             (pass_through_allowed == 0 ||
              platform_drop_ticks != UINT8_C(0))))
        {
            continue;
        }
        surface_y_f32_value =
            surface_y_f32(content, support, position_x_f32);
        crosses_surface = floor_sweep_crosses_surface(
            previous_floor_contact_f32,
            new_floor_contact_f32,
            surface_y_f32_value,
            is_pass_through,
            fast_fall,
            pass_through_policy);
        if (crosses_surface != 0 &&
            surface_y_f32_value < best_surface_y_f32)
        {
            best_surface_y_f32 = surface_y_f32_value;
            *out_support = support;
        }
    }
    if (*out_support == (uint8_t)PF_M4_SURFACE_NONE)
    {
        return 0;
    }
    *out_surface_y_f32 = best_surface_y_f32;
    return 1;
}

static int body_sweep_hits_solid(
    const struct content *content,
    float previous_position_x_f32,
    float previous_position_y_f32,
    float current_position_x_f32,
    float current_position_y_f32)
{
    const fighter_data *fighter = &content->fighter;
    const stage_data *stage = &content->stage;
    const float left = current_position_x_f32 - fighter->half_width_f32;
    const float right = current_position_x_f32 + fighter->half_width_f32;
    const float top = current_position_y_f32 - fighter->half_height_f32;
    const float bottom = current_position_y_f32 + fighter->half_height_f32;

    if (stage->reference_collision_profile !=
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        const float previous_top =
            previous_position_y_f32 - fighter->half_height_f32;
        const float previous_bottom =
            previous_position_y_f32 + fighter->half_height_f32;
        const float swept_top = previous_top < top ? previous_top : top;
        const float swept_bottom =
            previous_bottom > bottom ? previous_bottom : bottom;
        float contact_position_x_f32;
        float contact_y_f32;
        uint8_t contact_support;
        int8_t away_direction;

        if (ssbm_reference_stage_find_wall_contact(
                stage->reference_collision_profile,
                previous_position_x_f32,
                current_position_x_f32,
                swept_top,
                swept_bottom,
                fighter->half_width_f32,
                &contact_position_x_f32,
                &contact_support,
                &away_direction))
        {
            return 1;
        }
        if (current_position_y_f32 < previous_position_y_f32 &&
            ssbm_reference_stage_find_ceiling_contact(
                stage->reference_collision_profile,
                current_position_x_f32,
                previous_top,
                top,
                &contact_y_f32,
                &contact_support))
        {
            return 1;
        }
        if (current_position_y_f32 > previous_position_y_f32)
        {
            contact_support = (uint8_t)PF_M4_SURFACE_NONE;
            if (reference_stage_find_floor_landing(
                    content,
                    current_position_x_f32,
                    previous_bottom,
                    bottom,
                    UINT8_C(0),
                    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT,
                    1,
                    UINT8_C(0),
                    &contact_y_f32,
                    &contact_support))
            {
                return 1;
            }
        }
        return 0;
    }

    return right > stage->solid_left_f32 &&
           left < stage->solid_right_f32 &&
           bottom > stage->solid_top_f32 &&
           top < stage->solid_bottom_f32;
}

static int8_t wall_contact_away_direction(
    const struct content *content,
    float position_x_f32,
    float position_y_f32)
{
    const fighter_data *fighter = &content->fighter;
    const stage_data *stage = &content->stage;
    const float body_top = position_y_f32 - fighter->half_height_f32;
    const float body_bottom = position_y_f32 + fighter->half_height_f32;
    const int vertical_overlap =
        body_bottom > stage->solid_top_f32 &&
        body_top < stage->solid_bottom_f32;

    if (stage->reference_collision_profile !=
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        float contact_position_x_f32;
        uint8_t contact_support;
        int8_t away_direction;

        if (isfinite(position_x_f32) &&
            ssbm_reference_stage_find_wall_contact(
                stage->reference_collision_profile,
                position_x_f32 - 1.0f,
                position_x_f32,
                body_top,
                body_bottom,
                fighter->half_width_f32,
                &contact_position_x_f32,
                &contact_support,
                &away_direction) &&
            contact_position_x_f32 == position_x_f32)
        {
            return away_direction;
        }
        if (isfinite(position_x_f32) &&
            ssbm_reference_stage_find_wall_contact(
                stage->reference_collision_profile,
                position_x_f32 + 1.0f,
                position_x_f32,
                body_top,
                body_bottom,
                fighter->half_width_f32,
                &contact_position_x_f32,
                &contact_support,
                &away_direction) &&
            contact_position_x_f32 == position_x_f32)
        {
            return away_direction;
        }
        return INT8_C(0);
    }

    if (!vertical_overlap)
    {
        return INT8_C(0);
    }
    if (position_x_f32 + fighter->half_width_f32 ==
        stage->solid_left_f32)
    {
        return INT8_C(-1);
    }
    if (position_x_f32 - fighter->half_width_f32 ==
        stage->solid_right_f32)
    {
        return INT8_C(1);
    }
    return INT8_C(0);
}

static pf_status apply_hitlag_shift(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int16_t stick_x,
    int16_t stick_y,
    float maximum_distance_x_f32,
    float maximum_distance_y_f32,
    int preserve_ground_support,
    int defer_solid_resolution)
{
    const fighter_data *fighter = &content->fighter;
    const stage_data *stage = &content->stage;
    const float old_x = scratch->position_x_f32[player_index];
    const float old_y = scratch->position_y_f32[player_index];
    float next_x;
    float next_y;

    next_x = old_x + ssbm_analog_displacement_f32(
        stick_x, maximum_distance_x_f32);
    next_y = old_y + ssbm_analog_displacement_f32(
        stick_y, maximum_distance_y_f32);
    if (defer_solid_resolution == 0 &&
        body_sweep_hits_solid(
            content,
            old_x,
            old_y,
            next_x,
            next_y))
    {
        const float requested_x = next_x;
        const float requested_y = next_y;

        /* Hitlag displacement resolves each axis independently.  A grounded
         * diagonal SDI input slides along the floor in Melee; cancelling the
         * entire vector when only its vertical component enters the floor
         * incorrectly discards the horizontal pulse. */
        next_x = body_sweep_hits_solid(
                     content,
                     old_x,
                     old_y,
                     requested_x,
                     old_y)
                     ? old_x
                     : requested_x;
        next_y = body_sweep_hits_solid(
                     content,
                     old_x,
                     old_y,
                     old_x,
                     requested_y)
                     ? old_y
                     : requested_y;
    }

    if (scratch->grounded[player_index] != UINT8_C(0))
    {
        const uint8_t support = scratch->support[player_index];
        const float surface_y =
            surface_y_f32(content, support, next_x);
        const float standing_y =
            surface_y - fighter->half_height_f32;
        float surface_left;
        float surface_right;

        if (next_y > standing_y)
        {
            next_y = standing_y;
        }
        surface_bounds_f32(
            content,
            support,
            world->tick,
            &surface_left,
            &surface_right);
        if (preserve_ground_support != 0)
        {
            if (next_x < surface_left)
            {
                next_x = surface_left;
            }
            else if (next_x > surface_right)
            {
                next_x = surface_right;
            }
            next_y = standing_y;
        }
        else if (next_y < standing_y ||
                 next_x < surface_left ||
                 next_x > surface_right)
        {
            scratch->grounded[player_index] = UINT8_C(0);
            scratch->support[player_index] =
                (uint8_t)PF_M4_SURFACE_NONE;
        }
    }
    else if (defer_solid_resolution == 0 && next_y > old_y)
    {
        const float old_bottom =
            old_y + fighter->half_height_f32;
        const float next_bottom =
            next_y + fighter->half_height_f32;
        const float platform_center =
            platform_center_x_f32(stage, world->tick);
        const float platform_left =
            platform_center - stage->platform_half_width_f32;
        const float platform_right =
            platform_center + stage->platform_half_width_f32;
        const int crosses_platform =
            next_x >= platform_left &&
            next_x <= platform_right &&
            old_bottom <= stage->platform_y_f32 &&
            next_bottom >= stage->platform_y_f32;
        const int crosses_floor =
            next_x >= stage->floor_left_f32 &&
            next_x <= stage->floor_right_f32 &&
            old_bottom <= stage->floor_y_f32 &&
            next_bottom >= stage->floor_y_f32;

        if (crosses_platform || crosses_floor)
        {
            next_y = old_y;
        }
    }

    scratch->position_x_f32[player_index] = next_x;
    scratch->position_y_f32[player_index] = next_y;
    return PF_STATUS_OK;
}

static int action_is_throw(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_THROW_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_BACK ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_UP ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_DOWN;
}

static const struct throw_data *throw_for_action(
    const fighter_data *fighter,
    uint8_t action_state)
{
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_FORWARD)
    {
        return &fighter->forward_throw;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_BACK)
    {
        return &fighter->back_throw;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_UP)
    {
        return &fighter->up_throw;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_THROW_DOWN)
    {
        return &fighter->down_throw;
    }
    return NULL;
}

static uint8_t grab_action_for_input(
    const fighter_data *fighter,
    const pf_input_frame *input,
    uint8_t main_stick_x_age,
    uint8_t main_stick_y_age,
    int16_t previous_secondary_stick_x,
    int16_t previous_secondary_stick_y,
    int8_t facing)
{
    const int16_t threshold =
        (int16_t)fighter->tilt_axis_threshold;
    int16_t stick_x = INT16_C(0);

    if (main_stick_x_age == UINT8_C(0))
    {
        stick_x = input->main_stick_x;
    }
    else if ((previous_secondary_stick_x < threshold &&
              input->secondary_stick_x >= threshold) ||
             (previous_secondary_stick_x > -threshold &&
              input->secondary_stick_x <= -threshold))
    {
        stick_x = input->secondary_stick_x;
    }
    if (stick_x != INT16_C(0))
    {
        return (stick_x < INT16_C(0) ? INT8_C(-1) : INT8_C(1)) ==
                       facing
                   ? (uint8_t)PF_M4_ACTION_THROW_FORWARD
                   : (uint8_t)PF_M4_ACTION_THROW_BACK;
    }
    if ((main_stick_y_age == UINT8_C(0) &&
         input->main_stick_y <= -threshold) ||
        (previous_secondary_stick_y > -threshold &&
         input->secondary_stick_y <= -threshold))
    {
        return (uint8_t)PF_M4_ACTION_THROW_UP;
    }
    if ((main_stick_y_age == UINT8_C(0) &&
         input->main_stick_y >= threshold) ||
        /* ftCo_800DF878 deliberately accepts held C-down: both the
         * previous and current source Y samples are at/below xB0. */
        (previous_secondary_stick_y >= threshold &&
         input->secondary_stick_y >= threshold))
    {
        return (uint8_t)PF_M4_ACTION_THROW_DOWN;
    }
    return (uint8_t)PF_M4_ACTION_GRAB_HOLD;
}

static int action_locks_ground_control(uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM ||
           action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
           action_state == (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
           action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
           action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND ||
           action_state == (uint8_t)PF_M4_ACTION_FORCED_GETUP ||
           action_state == (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
           action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
           action_state == (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD ||
           action_state == (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
           action_state == (uint8_t)PF_M4_ACTION_GRAB ||
           action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB ||
           action_state == (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
           action_state == (uint8_t)PF_M4_ACTION_PUMMEL ||
           action_state == (uint8_t)PF_M4_ACTION_GRABBED ||
           action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
           action_is_throw(action_state) ||
           action_state == (uint8_t)PF_M4_ACTION_ITEM_THROW ||
           action_state ==
               (uint8_t)PF_M4_ACTION_ITEM_DASH_THROW ||
           action_state ==
               (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_LANDING ||
           action_state == (uint8_t)PF_M4_ACTION_REBOUND_STOP ||
           action_state == (uint8_t)PF_M4_ACTION_REBOUND ||
           action_state == (uint8_t)PF_M4_ACTION_TAUNT;
}

static int action_is_reference_special_locked(uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND;
}

static int action_is_falcon_kick(uint8_t action_state)
{
    return action_state >=
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND &&
           action_state <=
               (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND;
}

static int action_is_shield_break(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
           action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN;
}

static int action_is_wall_tech(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_WALL_TECH ||
           action_state == (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP;
}

static int action_is_surface_tech(uint8_t action_state)
{
    return action_is_wall_tech(action_state) ||
           action_state == (uint8_t)PF_M4_ACTION_CEILING_TECH;
}

static int action_is_surface_bounce(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_WALL_BOUNCE ||
           action_state == (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
}

static int action_is_shield(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
           action_is_shield_break(action_state);
}

static int action_is_ground_attack(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_UP_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
           action_is_reference_angled_normal(action_state) ||
           action_state == (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_UP_STRONG_CHARGE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW ||
           action_state == (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL ||
           action_state == (uint8_t)PF_M4_ACTION_JAB_THIRD ||
           action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_START ||
           action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP ||
           action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_END;
}

static int action_is_forward_ground_attack(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
           action_is_reference_angled_normal(action_state) ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW;
}

static int action_allows_fresh_fast_fall(
    uint8_t action_state,
    uint16_t action_ticks)
{
    const falcon_neutral_special_timing *timing;

    if (action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW &&
        !action_is_falcon_kick(action_state))
    {
        return 1;
    }
    if (action_state !=
        (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR)
    {
        return 0;
    }
    timing = falcon_reference_neutral_special_timing();
    return timing != NULL &&
           action_ticks >= timing->ordinary_air_physics_begin_frame;
}

static int action_is_smash_charge(uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_UP_STRONG_CHARGE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW;
}

static uint8_t smash_release_action(uint8_t charge_action)
{
    switch ((enum action_state)charge_action)
    {
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK;
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH;
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW;
        case PF_M4_ACTION_UP_STRONG_CHARGE:
            return (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK;
        case PF_M4_ACTION_DOWN_STRONG_CHARGE:
            return (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
        default:
            return (uint8_t)PF_M4_ACTION_GROUND_IDLE;
    }
}

static uint8_t smash_charge_action_for_release(uint8_t release_action)
{
    switch ((enum action_state)release_action)
    {
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        default:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE;
    }
}

static const attack_data *directional_ground_data(
    const fighter_data *fighter,
    uint8_t action_state)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_UP_ATTACK:
            return &fighter->up_attack;
        case PF_M4_ACTION_DOWN_ATTACK:
            return &fighter->down_attack;
        case PF_M4_ACTION_FORWARD_ATTACK:
        case PF_M4_ACTION_FORWARD_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_LOW:
        case PF_M4_ACTION_FORWARD_ATTACK_LOW:
            return &fighter->forward_attack;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            return &fighter->forward_strong_attack;
        case PF_M4_ACTION_UP_STRONG_ATTACK:
            return &fighter->up_strong_attack;
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
            return &fighter->down_strong_attack;
        default:
            return NULL;
    }
}

static reference_timing ground_attack_timing(
    const fighter_data *fighter,
    uint8_t action_state)
{
    reference_timing timing = {0};
    const attack_data *attack =
        directional_ground_data(fighter, action_state);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        (action_is_reference_jab_extension(action_state) ||
         action_is_reference_angled_normal(action_state)))
    {
        falcon_move_index move_index;

        if (falcon_reference_move_for_action(
                action_state,
                &move_index))
        {
            return falcon_reference_timing(move_index);
        }
        return timing;
    }
    if (attack != NULL)
    {
        timing.startup_ticks = attack->startup_ticks;
        timing.active_ticks = attack->active_ticks;
        timing.recovery_ticks = attack->recovery_ticks;
        return timing;
    }
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_DASH_ATTACK:
            timing.startup_ticks = fighter->dash_attack_startup_ticks;
            timing.active_ticks = fighter->dash_attack_active_ticks;
            timing.recovery_ticks = fighter->dash_attack_recovery_ticks;
            break;
        case PF_M4_ACTION_JAB_FINAL:
            timing.startup_ticks = fighter->jab_final_startup_ticks;
            timing.active_ticks = fighter->jab_final_active_ticks;
            timing.recovery_ticks = fighter->jab_final_recovery_ticks;
            break;
        case PF_M4_ACTION_STRONG_ATTACK:
            timing.startup_ticks = fighter->strong_startup_ticks;
            timing.active_ticks = fighter->strong_active_ticks;
            timing.recovery_ticks = fighter->strong_recovery_ticks;
            break;
        case PF_M4_ACTION_GROUND_ATTACK:
            timing.startup_ticks = fighter->jab_startup_ticks;
            timing.active_ticks = fighter->jab_active_ticks;
            timing.recovery_ticks = fighter->jab_recovery_ticks;
            break;
        default:
            break;
    }
    return timing;
}

static float ground_attack_damage_f32(
    const fighter_data *fighter,
    uint8_t action_state)
{
    const attack_data *attack =
        directional_ground_data(fighter, action_state);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_is_reference_jab_extension(action_state))
    {
        falcon_move_index move_index;
        const reference_hit_effect *effect;

        if (!falcon_reference_move_for_action(
                action_state,
                &move_index) ||
            (effect = falcon_reference_primary_effect(move_index)) ==
                NULL)
        {
            return 0.0f;
        }
        return (float)effect->damage;
    }
    if (attack != NULL)
    {
        return attack->damage_f32;
    }
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_DASH_ATTACK:
            return fighter->dash_attack_damage_f32;
        case PF_M4_ACTION_JAB_FINAL:
            return fighter->jab_final_damage_f32;
        case PF_M4_ACTION_STRONG_ATTACK:
            return fighter->strong_damage_f32;
        case PF_M4_ACTION_GROUND_ATTACK:
            return fighter->jab_damage_f32;
        default:
            return 0.0f;
    }
}

static const struct reference_move *falcon_ground_reference_attack(
    const fighter_data *fighter,
    uint8_t action_state)
{
    const reference_timing authored =
        ground_attack_timing(fighter, action_state);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_is_reference_angled_normal(action_state))
    {
        falcon_move_index move_index;

        return falcon_reference_move_for_action(
                   action_state,
                   &move_index)
                   ? falcon_reference_move(move_index)
                   : NULL;
    }

    return falcon_reference_attack(
        action_state,
        authored,
        ground_attack_damage_f32(fighter, action_state));
}

static int falcon_ground_reference_matches(
    const fighter_data *fighter,
    uint8_t action_state)
{
    return falcon_ground_reference_attack(
               fighter,
               action_state) != NULL;
}

enum
{
    PF_M4_FALCON_IASA_COMMON_MOVEMENT = 1U << 0U,
    PF_M4_FALCON_IASA_SPECIAL = 1U << 1U,
    PF_M4_FALCON_IASA_GRAB = 1U << 2U,
    PF_M4_FALCON_IASA_ATTACK = 1U << 3U,
    PF_M4_FALCON_IASA_ESCAPE = 1U << 4U,
    PF_M4_FALCON_IASA_GUARD = 1U << 5U,
    PF_M4_FALCON_IASA_TAUNT = 1U << 6U
};

static uint8_t falcon_ground_iasa_capabilities(
    reference_iasa_policy policy)
{
    const uint8_t common =
        (uint8_t)(PF_M4_FALCON_IASA_COMMON_MOVEMENT |
                  PF_M4_FALCON_IASA_ATTACK);

    switch (policy)
    {
        case PF_M4_REFERENCE_IASA_JAB_CHAIN:
        case PF_M4_REFERENCE_IASA_DOWN_TILT:
            return common;
        case PF_M4_REFERENCE_IASA_WAIT:
            return (uint8_t)(
                common |
                PF_M4_FALCON_IASA_SPECIAL |
                PF_M4_FALCON_IASA_GRAB |
                PF_M4_FALCON_IASA_ESCAPE |
                PF_M4_FALCON_IASA_GUARD |
                PF_M4_FALCON_IASA_TAUNT);
        case PF_M4_REFERENCE_IASA_FORWARD_SMASH:
            return (uint8_t)(
                common |
                PF_M4_FALCON_IASA_SPECIAL |
                PF_M4_FALCON_IASA_GRAB |
                PF_M4_FALCON_IASA_GUARD |
                PF_M4_FALCON_IASA_TAUNT);
        case PF_M4_REFERENCE_IASA_NONE:
        default:
            return UINT8_C(0);
    }
}

static int reference_calls_direct_escape_n(
    uint8_t source_action_state,
    uint8_t ground_iasa_capabilities)
{
    /* Wait owns direct EscapeN. Released grounded Damage delegates to
     * Wait_IASA, as do imported attacks with the Wait callback policy.
     * Falcon's Appeal scripts never enable their nominal IASA callback. */
    return source_action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           (action_is_ground_attack(source_action_state) &&
           (ground_iasa_capabilities &
             PF_M4_FALCON_IASA_ESCAPE) != UINT8_C(0)) ||
           action_is_damage(source_action_state);
}

static int8_t source_forward_angle_band(
    int16_t stick_x,
    int16_t stick_y,
    float outer_angle_tan_f32,
    float inner_angle_tan_f32)
{
    const int32_t source_y = -(int32_t)stick_y;
    const uint16_t horizontal_magnitude = axis_magnitude(stick_x);
    const float angle_numerator =
        (float)(source_y < INT32_C(0) ? -source_y : source_y);

    if (angle_numerator >
        (float)horizontal_magnitude * outer_angle_tan_f32)
    {
        return source_y > INT32_C(0) ? INT8_C(2) : INT8_C(-2);
    }
    if (angle_numerator >
        (float)horizontal_magnitude * inner_angle_tan_f32)
    {
        return source_y > INT32_C(0) ? INT8_C(1) : INT8_C(-1);
    }
    return INT8_C(0);
}

static uint8_t select_ground_light_attack_action(
    const fighter_data *fighter,
    const ssbm_ground_input_attributes *source_ground_input,
    int8_t facing,
    int16_t stick_x,
    int16_t stick_y)
{
    const uint16_t horizontal_magnitude =
        axis_magnitude(stick_x);
    if (source_ground_input != NULL)
    {
        const int32_t source_y = -(int32_t)stick_y;
        const float angle_numerator =
            (float)(source_y < INT32_C(0) ? -source_y : source_y);
        const float direction_boundary =
            (float)horizontal_magnitude *
            source_ground_input->tilt_direction_angle_tan_f32;

        /* Wait_IASA checks S3 before Hi3/Lw3 before Attack11. Match those
         * independent source predicates rather than choosing the dominant
         * Cartesian axis. */
        if (horizontal_magnitude >=
                source_ground_input->forward_tilt_axis_threshold &&
            axis_direction(
                stick_x,
                source_ground_input->forward_tilt_axis_threshold) == facing &&
            angle_numerator < direction_boundary)
        {
            const int8_t angle_band = source_forward_angle_band(
                stick_x,
                stick_y,
                source_ground_input->forward_tilt_outer_angle_tan_f32,
                source_ground_input->forward_tilt_inner_angle_tan_f32);

            if (angle_band == INT8_C(2))
            {
                return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_HIGH;
            }
            if (angle_band == INT8_C(1))
            {
                return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH;
            }
            if (angle_band == INT8_C(-2))
            {
                return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_LOW;
            }
            if (angle_band == INT8_C(-1))
            {
                return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_MID_LOW;
            }
            return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK;
        }
        if (source_y >=
                (int32_t)source_ground_input->vertical_tilt_axis_threshold &&
            angle_numerator > direction_boundary)
        {
            return (uint8_t)PF_M4_ACTION_UP_ATTACK;
        }
        if (source_y <=
                -(int32_t)source_ground_input->vertical_tilt_axis_threshold &&
            angle_numerator > direction_boundary)
        {
            return (uint8_t)PF_M4_ACTION_DOWN_ATTACK;
        }
        return (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
    }

    if (axis_magnitude(stick_y) >= fighter->axis_dead_zone &&
        axis_magnitude(stick_y) > horizontal_magnitude)
    {
        return stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_ATTACK
                   : (uint8_t)PF_M4_ACTION_DOWN_ATTACK;
    }
    if (horizontal_magnitude >= fighter->axis_dead_zone &&
        horizontal_magnitude >= axis_magnitude(stick_y) &&
        axis_direction(stick_x, fighter->axis_dead_zone) == facing)
    {
        return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK;
    }
    return (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
}

static int8_t reference_turn_callback_facing(
    const ssbm_ground_input_attributes *source_ground_input,
    uint8_t action_state,
    int8_t facing,
    int8_t turn_direction)
{
    if (source_ground_input != NULL &&
        action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN &&
        turn_direction != INT8_C(0))
    {
        const int8_t target_facing =
            turn_direction < INT8_C(0) ? INT8_C(-1) : INT8_C(1);

        /* ftCo_Turn_IASA temporarily exposes facing_after to the ordered
         * special/grab/attack callbacks until the basic turn's physical
         * facing flip. Consumers retain it; Guard/Taunt/Jump run only after
         * the source restores the old facing. */
        return target_facing;
    }
    return facing;
}

static uint8_t select_ground_strong_attack_action(
    const fighter_data *fighter,
    const ssbm_ground_input_attributes *source_ground_input,
    int16_t stick_x,
    int16_t stick_y)
{
    const uint16_t horizontal_magnitude =
        axis_magnitude(stick_x);
    const uint16_t vertical_magnitude =
        axis_magnitude(stick_y);

    if (source_ground_input != NULL)
    {
        if (horizontal_magnitude >=
            source_ground_input->c_stick_horizontal_smash_threshold)
        {
            const int8_t angle_band = source_forward_angle_band(
                stick_x,
                stick_y,
                source_ground_input->forward_smash_outer_angle_tan_f32,
                source_ground_input->forward_smash_inner_angle_tan_f32);

            /* Falcon has no AttackS4HiS/LwS subactions. The decomp tests
             * those pointers before selecting the inner-angle variants, so
             * only the outer high/low states differ from neutral. */
            if (angle_band == INT8_C(2))
            {
                return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH;
            }
            if (angle_band == INT8_C(-2))
            {
                return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW;
            }
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK;
        }
        if (stick_y <=
            -(int16_t)source_ground_input->vertical_smash_axis_threshold)
        {
            return (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK;
        }
        if (stick_y >=
            (int16_t)source_ground_input->vertical_smash_axis_threshold)
        {
            return (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
        }
        return (uint8_t)PF_M4_ACTION_STRONG_ATTACK;
    }

    if (vertical_magnitude >= fighter->axis_dead_zone &&
        vertical_magnitude > horizontal_magnitude)
    {
        return stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK
                   : (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
    }
    if (horizontal_magnitude >= fighter->axis_dead_zone &&
        horizontal_magnitude >= vertical_magnitude)
    {
        return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK;
    }
    return (uint8_t)PF_M4_ACTION_STRONG_ATTACK;
}

static uint8_t select_ground_strong_input_action(
    const fighter_data *fighter,
    const ssbm_ground_input_attributes *source_ground_input,
    uint8_t reference_c_stick_action,
    int16_t stick_x,
    int16_t stick_y)
{
    if (reference_c_stick_action != UINT8_MAX &&
        reference_c_stick_action !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
        reference_c_stick_action !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK)
    {
        return reference_c_stick_action;
    }
    return select_ground_strong_attack_action(
        fighter,
        source_ground_input,
        stick_x,
        stick_y);
}

static int action_is_light_aerial(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_AERIAL ||
           action_state == (uint8_t)PF_M4_ACTION_BACK_AERIAL ||
           action_state == (uint8_t)PF_M4_ACTION_UP_AERIAL ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
}

static const attack_data *directional_aerial_data(
    const fighter_data *fighter,
    uint8_t action_state)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_FORWARD_AERIAL:
            return &fighter->forward_aerial;
        case PF_M4_ACTION_BACK_AERIAL:
            return &fighter->back_aerial;
        case PF_M4_ACTION_UP_AERIAL:
            return &fighter->up_aerial;
        case PF_M4_ACTION_DOWN_AERIAL:
            return &fighter->down_aerial;
        default:
            return NULL;
    }
}

static uint32_t light_aerial_ticks(
    const fighter_data *fighter,
    uint8_t action_state)
{
    const attack_data *attack =
        directional_aerial_data(fighter, action_state);

    if (attack == NULL)
    {
        return (uint32_t)fighter->aerial_startup_ticks +
               (uint32_t)fighter->aerial_active_ticks +
               (uint32_t)fighter->aerial_recovery_ticks;
    }
    return (uint32_t)attack->startup_ticks +
           (uint32_t)attack->active_ticks +
           (uint32_t)attack->recovery_ticks;
}

static uint16_t aerial_landing_lag_for_action(
    const fighter_data *fighter,
    uint8_t action_state)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_FORWARD_AERIAL:
            return fighter->forward_aerial_landing_lag_ticks;
        case PF_M4_ACTION_BACK_AERIAL:
            return fighter->back_aerial_landing_lag_ticks;
        case PF_M4_ACTION_UP_AERIAL:
            return fighter->up_aerial_landing_lag_ticks;
        case PF_M4_ACTION_DOWN_AERIAL:
            return fighter->down_aerial_landing_lag_ticks;
        default:
            return fighter->aerial_landing_lag_ticks;
    }
}

static int falcon_aerial_reference_matches(
    const fighter_data *fighter,
    uint8_t action_state)
{
    falcon_move_index move_index;
    const struct reference_move *move;
    const reference_hit_effect *effect;
    const attack_data *attack =
        directional_aerial_data(fighter, action_state);
    const float damage_f32 =
        attack != NULL ? attack->damage_f32 : fighter->aerial_damage_f32;

    if (!falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = falcon_reference_move(move_index);
    effect = falcon_reference_primary_effect(move_index);
    return move != NULL && effect != NULL &&
           move->landing_lag != UINT16_C(0) &&
           light_aerial_ticks(fighter, action_state) ==
               (uint32_t)move->total_frames &&
           damage_f32 == (float)effect->damage &&
           aerial_landing_lag_for_action(
               fighter,
               action_state) == move->landing_lag;
}

static int light_aerial_landing_lag_active(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_frame)
{
    const int reference_lag_active =
        falcon_aerial_reference_matches(fighter, action_state)
            ? falcon_reference_landing_lag_active(
                  action_state,
                  action_frame)
            : -1;

    return reference_lag_active >= 0
               ? reference_lag_active
               : (action_frame >=
                      fighter->aerial_landing_lag_begin_tick &&
                  action_frame <
                      fighter->aerial_landing_lag_end_tick);
}

static uint8_t select_light_aerial_action(
    const fighter_data *fighter,
    int16_t stick_x,
    int16_t stick_y,
    int8_t facing)
{
    const uint16_t horizontal_magnitude =
        axis_magnitude(stick_x);
    const uint16_t vertical_magnitude =
        axis_magnitude(stick_y);

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const ssbm_ground_input_attributes *ground_input =
            ssbm_common_reference_ground_input();

        if (ground_input != NULL)
        {
            const int32_t source_y = -(int32_t)stick_y;
            const float direction_height =
                (float)(source_y < INT32_C(0) ? -source_y : source_y);
            const float direction_width =
                (float)horizontal_magnitude *
                ground_input->aerial_direction_angle_tan_f32;

            if (horizontal_magnitude <
                    ground_input->aerial_neutral_x_threshold &&
                vertical_magnitude <
                    ground_input->aerial_neutral_y_threshold)
            {
                return (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
            }
            if (source_y > INT32_C(0) &&
                direction_height > direction_width)
            {
                return (uint8_t)PF_M4_ACTION_UP_AERIAL;
            }
            if (source_y < INT32_C(0) &&
                direction_height > direction_width)
            {
                return (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
            }
            if (stick_x == INT16_C(0))
            {
                return source_y > INT32_C(0)
                           ? (uint8_t)PF_M4_ACTION_UP_AERIAL
                           : (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
            }
            return (stick_x < INT16_C(0) ? INT8_C(-1) : INT8_C(1)) ==
                           facing
                       ? (uint8_t)PF_M4_ACTION_FORWARD_AERIAL
                       : (uint8_t)PF_M4_ACTION_BACK_AERIAL;
        }
    }

    if (vertical_magnitude >= fighter->dash_axis_threshold &&
        vertical_magnitude > horizontal_magnitude)
    {
        return stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_AERIAL
                   : (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
    }
    if (horizontal_magnitude >= fighter->dash_axis_threshold &&
        horizontal_magnitude >= vertical_magnitude)
    {
        const int8_t input_direction =
            stick_x < INT16_C(0) ? INT8_C(-1) : INT8_C(1);

        return input_direction == facing
                   ? (uint8_t)PF_M4_ACTION_FORWARD_AERIAL
                   : (uint8_t)PF_M4_ACTION_BACK_AERIAL;
    }
    return (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
}

static uint8_t select_aerial_attack_action(
    const fighter_data *fighter,
    const pf_input_frame *input,
    int8_t facing,
    int strong_attack_pressed)
{
    if (strong_attack_pressed != 0)
    {
        const ssbm_ground_input_attributes *ground_input =
            fighter->reference_frame_data_enabled != UINT8_C(0)
                ? ssbm_common_reference_ground_input()
                : NULL;
        const int secondary_stick_active =
            axis_magnitude(input->secondary_stick_x) >=
                (ground_input != NULL
                     ? ground_input->aerial_neutral_x_threshold
                     : fighter->axis_dead_zone) ||
            axis_magnitude(input->secondary_stick_y) >=
                (ground_input != NULL
                     ? ground_input->aerial_neutral_y_threshold
                     : fighter->axis_dead_zone);
        const uint8_t c_stick_action =
            select_light_aerial_action(
                fighter,
                input->secondary_stick_x,
                input->secondary_stick_y,
                facing);

        /* Melee's C-stick is an alternate directional input for the same
         * aerial scripts. Retain the authored strong-aerial extension for
         * custom content that does not exactly match the Falcon catalog. */
        if (secondary_stick_active != 0 &&
            falcon_aerial_reference_matches(
                fighter,
                c_stick_action))
        {
            return c_stick_action;
        }
        return (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK;
    }
    return select_light_aerial_action(
        fighter,
        input->main_stick_x,
        input->main_stick_y,
        facing);
}

static uint8_t reference_c_stick_attack_action(
    const fighter_data *fighter,
    const pf_input_frame *input,
    int16_t previous_x,
    int16_t previous_y,
    int grounded)
{
    const ssbm_ground_input_attributes *ground_input;
    const uint16_t current_x =
        axis_magnitude(input->secondary_stick_x);
    const uint16_t previous_x_magnitude =
        axis_magnitude(previous_x);

    if (fighter->reference_frame_data_enabled == UINT8_C(0))
    {
        return UINT8_MAX;
    }
    ground_input = ssbm_common_reference_ground_input();
    if (ground_input == NULL)
    {
        return UINT8_MAX;
    }
    if (grounded == 0)
    {
        return (previous_x_magnitude <
                        ground_input->aerial_neutral_x_threshold &&
                    current_x >=
                        ground_input->aerial_neutral_x_threshold) ||
                       (axis_magnitude(previous_y) <
                            ground_input->aerial_neutral_y_threshold &&
                        axis_magnitude(input->secondary_stick_y) >=
                            ground_input->aerial_neutral_y_threshold)
                   ? (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK
                   : UINT8_MAX;
    }
    /* Wait_IASA checks S4 before Hi4 before Lw4. Preserve that priority
     * when a diagonal C-stick transition crosses multiple source gates. */
    if (previous_x_magnitude <
            ground_input->c_stick_horizontal_smash_threshold &&
        current_x >= ground_input->c_stick_horizontal_smash_threshold)
    {
        return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK;
    }
    if (previous_y >
            -(int16_t)ground_input->c_stick_up_smash_threshold &&
        input->secondary_stick_y <=
            -(int16_t)ground_input->c_stick_up_smash_threshold)
    {
        return (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK;
    }
    if (previous_y <
            (int16_t)ground_input->c_stick_down_smash_threshold &&
        input->secondary_stick_y >=
            (int16_t)ground_input->c_stick_down_smash_threshold)
    {
        return (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
    }
    return UINT8_MAX;
}

static int action_can_enter_teeter(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
           action_state == (uint8_t)PF_M4_ACTION_LANDING;
}

static int action_is_aerial_landing(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AERIAL_LANDING ||
           action_state == (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_AERIAL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_BACK_AERIAL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_UP_AERIAL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_AERIAL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_FORWARD_AERIAL_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_BACK_AERIAL_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_UP_AERIAL_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_DOWN_AERIAL_L_CANCEL_LANDING;
}

static int action_is_grounded_landing(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_LANDING ||
           action_state == (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING ||
           action_is_aerial_landing(action_state);
}

static int action_is_l_cancel_landing(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_FORWARD_AERIAL_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_BACK_AERIAL_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_UP_AERIAL_L_CANCEL_LANDING ||
           action_state ==
               (uint8_t)
                   PF_M4_ACTION_DOWN_AERIAL_L_CANCEL_LANDING;
}

static uint8_t aerial_landing_action(
    uint8_t aerial_action,
    int l_cancelled)
{
    switch ((enum action_state)aerial_action)
    {
        case PF_M4_ACTION_FORWARD_AERIAL:
            return l_cancelled != 0
                       ? (uint8_t)
                             PF_M4_ACTION_FORWARD_AERIAL_L_CANCEL_LANDING
                       : (uint8_t)PF_M4_ACTION_FORWARD_AERIAL_LANDING;
        case PF_M4_ACTION_BACK_AERIAL:
            return l_cancelled != 0
                       ? (uint8_t)
                             PF_M4_ACTION_BACK_AERIAL_L_CANCEL_LANDING
                       : (uint8_t)PF_M4_ACTION_BACK_AERIAL_LANDING;
        case PF_M4_ACTION_UP_AERIAL:
            return l_cancelled != 0
                       ? (uint8_t)
                             PF_M4_ACTION_UP_AERIAL_L_CANCEL_LANDING
                       : (uint8_t)PF_M4_ACTION_UP_AERIAL_LANDING;
        case PF_M4_ACTION_DOWN_AERIAL:
            return l_cancelled != 0
                       ? (uint8_t)
                             PF_M4_ACTION_DOWN_AERIAL_L_CANCEL_LANDING
                       : (uint8_t)PF_M4_ACTION_DOWN_AERIAL_LANDING;
        default:
            return l_cancelled != 0
                       ? (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING
                       : (uint8_t)PF_M4_ACTION_AERIAL_LANDING;
    }
}

static uint16_t aerial_landing_ticks(
    const fighter_data *fighter,
    uint8_t landing_action)
{
    falcon_move_index move_index = PF_M4_FALCON_MOVE_COUNT;
    uint8_t aerial_action = UINT8_MAX;
    uint16_t authored_ticks;
    int l_cancelled = action_is_l_cancel_landing(landing_action);

    switch ((enum action_state)landing_action)
    {
        case PF_M4_ACTION_FORWARD_AERIAL_LANDING:
        case PF_M4_ACTION_FORWARD_AERIAL_L_CANCEL_LANDING:
            aerial_action = (uint8_t)PF_M4_ACTION_FORWARD_AERIAL;
            authored_ticks = fighter->forward_aerial_landing_lag_ticks;
            break;
        case PF_M4_ACTION_BACK_AERIAL_LANDING:
        case PF_M4_ACTION_BACK_AERIAL_L_CANCEL_LANDING:
            aerial_action = (uint8_t)PF_M4_ACTION_BACK_AERIAL;
            authored_ticks = fighter->back_aerial_landing_lag_ticks;
            break;
        case PF_M4_ACTION_UP_AERIAL_LANDING:
        case PF_M4_ACTION_UP_AERIAL_L_CANCEL_LANDING:
            aerial_action = (uint8_t)PF_M4_ACTION_UP_AERIAL;
            authored_ticks = fighter->up_aerial_landing_lag_ticks;
            break;
        case PF_M4_ACTION_DOWN_AERIAL_LANDING:
        case PF_M4_ACTION_DOWN_AERIAL_L_CANCEL_LANDING:
            aerial_action = (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
            authored_ticks = fighter->down_aerial_landing_lag_ticks;
            break;
        case PF_M4_ACTION_STRONG_AERIAL_LANDING:
        case PF_M4_ACTION_STRONG_L_CANCEL_LANDING:
            authored_ticks = fighter->strong_aerial_landing_lag_ticks;
            break;
        default:
            aerial_action = (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
            authored_ticks = fighter->aerial_landing_lag_ticks;
            break;
    }
    if (aerial_action != UINT8_MAX &&
        falcon_aerial_reference_matches(fighter, aerial_action) &&
        falcon_reference_move_for_action(
            aerial_action,
            &move_index))
    {
        const struct reference_move *move =
            falcon_reference_move(move_index);

        if (move != NULL)
        {
            return l_cancelled != 0
                       ? move->l_cancelled_landing_lag
                       : move->landing_lag;
        }
    }
    if (l_cancelled != 0)
    {
        authored_ticks =
            (uint16_t)(authored_ticks / fighter->l_cancel_divisor);
        if (authored_ticks == UINT16_C(0))
        {
            authored_ticks = UINT16_C(1);
        }
    }
    return authored_ticks;
}

static int action_is_recovery_invulnerable(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing)
{
    if (action_state == (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM)
    {
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
    {
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        return action_ticks >=
                   fighter->air_dodge_invulnerability_begin_tick &&
               action_ticks <
                   fighter->air_dodge_invulnerability_end_tick;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_WALL_JUMP)
    {
        return action_ticks <
               fighter->wall_jump_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD)
    {
        return action_ticks >=
                   fighter->roll_invulnerability_begin_tick &&
               action_ticks <
                   fighter->roll_invulnerability_end_tick;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        return action_ticks >=
                   fighter->spot_dodge_invulnerability_begin_tick &&
               action_ticks <
                   fighter->spot_dodge_invulnerability_end_tick;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
    {
        return action_ticks <
               fighter->ledge_roll_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        return action_ticks <
               fighter->ledge_attack_invulnerability_ticks;
    }
    if (action_state ==
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL)
    {
        return action_ticks <
               fighter->tech_invulnerability_ticks;
    }
    if (action_is_wall_tech(action_state))
    {
        return action_ticks <
               fighter->wall_tech_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_CEILING_TECH)
    {
        return action_ticks < fighter->ceiling_tech_control_tick;
    }
    if (action_is_surface_bounce(action_state))
    {
        return action_ticks <
               fighter->surface_bounce_invulnerability_ticks;
    }
    if (action_state ==
        (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL)
    {
        return action_ticks <
               fighter->getup_neutral_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL)
    {
        const getup_roll_timing *timing =
            getup_roll_timing_for(
                fighter,
                prone_roll_motion_orientation,
                tech_direction,
                facing);
        const uint16_t action_frame =
            action_ticks != UINT16_MAX
                ? (uint16_t)(action_ticks + UINT16_C(1))
                : UINT16_MAX;

        return timing != NULL &&
               action_frame >= timing->invulnerability_begin_tick &&
               action_frame <= timing->invulnerability_end_tick;
    }
    return action_state ==
               (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
           action_ticks <
               getup_attack_invulnerability_ticks_for(
                   fighter,
                   prone_orientation);
}

static uint8_t ledge_from_state(
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    int8_t facing)
{
    if (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
        action_uses_ledge(hitlag_resume_action))
    {
        action_state = hitlag_resume_action;
    }
    if (!action_uses_ledge(action_state))
    {
        return (uint8_t)PF_M4_LEDGE_NONE;
    }
    return facing == INT8_C(1)
               ? (uint8_t)PF_M4_LEDGE_LEFT
               : (uint8_t)PF_M4_LEDGE_RIGHT;
}

static int8_t ledge_inward_direction(uint8_t ledge)
{
    return ledge == (uint8_t)PF_M4_LEDGE_LEFT
               ? INT8_C(1)
               : INT8_C(-1);
}

static float ledge_x_f32(
    const stage_data *stage,
    uint8_t ledge)
{
    return ledge == (uint8_t)PF_M4_LEDGE_LEFT
               ? stage->floor_left_f32
               : stage->floor_right_f32;
}

static uint16_t ledge_transition_ticks(
    const fighter_data *fighter)
{
    return fighter->ledge_transition_ticks;
}

static void ledge_hang_position(
    const fighter_data *fighter,
    const stage_data *stage,
    uint8_t ledge,
    float *out_x,
    float *out_y)
{
    const int8_t inward = ledge_inward_direction(ledge);

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const falcon_ledge_root_positions *roots =
            falcon_reference_ledge_root_positions();

        if (roots != NULL)
        {
            *out_x =
                ledge_x_f32(stage, ledge) +
                (float)inward * roots->wait_frame_one_x_f32;
            *out_y =
                stage->floor_y_f32 + roots->wait_frame_one_y_f32 -
                fighter->half_height_f32;
            return;
        }
    }

    *out_x =
        ledge_x_f32(stage, ledge) -
        (float)inward * fighter->half_width_f32;
    *out_y =
        stage->floor_y_f32 + fighter->half_height_f32 / INT32_C(2);
}

static int reference_ledge_direction_option(
    int16_t stick_x,
    int16_t stick_y,
    int8_t facing,
    const ssbm_ledge_response_attributes *attributes)
{
    const uint32_t magnitude_x = axis_magnitude(stick_x);
    const uint32_t magnitude_y = axis_magnitude(stick_y);
    const int32_t source_y = -(int32_t)stick_y;
    const float angle_left =
        (float)(source_y < INT32_C(0) ? -source_y : source_y);
    const float angle_right =
        (float)magnitude_x * attributes->direction_angle_tan_f32;
    const int above_upper =
        source_y > INT32_C(0) && angle_left > angle_right;
    const int at_or_below_lower =
        source_y < INT32_C(0) && angle_left >= angle_right;

    if (magnitude_x < attributes->stick_axis_threshold &&
        magnitude_y < attributes->stick_axis_threshold)
    {
        return 0;
    }
    if (above_upper != 0 ||
        (at_or_below_lower == 0 &&
         (int32_t)stick_x * (int32_t)facing >= INT32_C(0)))
    {
        return 1;
    }
    return -1;
}

static uint16_t reference_ledge_option_submotion(
    uint8_t action_state,
    int quick)
{
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB)
    {
        return quick != 0
                   ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_QUICK
                   : (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_SLOW;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
    {
        return quick != 0
                   ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_QUICK
                   : (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_SLOW;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        return quick != 0
                   ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK
                   : (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW;
    }
    return quick != 0
               ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_1
               : (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_1;
}

static int enter_reference_ledge_option(
    const fighter_data *fighter,
    const stage_data *stage,
    uint8_t ledge,
    uint8_t next_action,
    int quick,
    float *position_x,
    float *position_y,
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t *source_submotion)
{
    float anchor_x_f32;
    float anchor_y_f32;
    const uint16_t submotion =
        reference_ledge_option_submotion(next_action, quick);

    if (!falcon_reference_ledge_option_anchor_f32(
            submotion,
            &anchor_x_f32,
            &anchor_y_f32))
    {
        return 0;
    }
    *position_x =
        ledge_x_f32(stage, ledge) +
        (float)ledge_inward_direction(ledge) * anchor_x_f32;
    *position_y =
        stage->floor_y_f32 + anchor_y_f32 - fighter->half_height_f32;
    *action_state = next_action;
    *action_ticks = UINT16_C(0);
    *source_submotion = submotion;
    return 1;
}

static int ledge_catch_position(
    const fighter_data *fighter,
    const stage_data *stage,
    uint8_t ledge,
    float *out_x,
    float *out_y)
{
    const falcon_ledge_root_positions *roots =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? falcon_reference_ledge_root_positions()
            : NULL;

    if (roots == NULL)
    {
        return 0;
    }
    *out_x =
        ledge_x_f32(stage, ledge) +
        (float)ledge_inward_direction(ledge) *
            roots->catch_frame_one_x_f32;
    *out_y =
        stage->floor_y_f32 + roots->catch_frame_one_y_f32 -
        fighter->half_height_f32;
    return 1;
}

static int ledge_occupied(
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    uint8_t ledge)
{
    uint32_t other_index;

    for (other_index = UINT32_C(0);
         other_index < (uint32_t)world->player_count;
         ++other_index)
    {
        if (other_index != player_index &&
            ledge_from_state(
                world->action_state[other_index],
                world->hitlag_resume_action[other_index],
                world->facing[other_index]) == ledge)
        {
            return 1;
        }
    }

    for (other_index = UINT32_C(0);
         other_index < player_index;
         ++other_index)
    {
        if (ledge_from_state(
                scratch->action_state[other_index],
                scratch->hitlag_resume_action[other_index],
                scratch->facing[other_index]) == ledge)
        {
            return 1;
        }
    }
    return 0;
}

enum
{
    PF_M4_LEDGE_PROBE_RIGHT = -1,
    PF_M4_LEDGE_PROBE_BOTH = 0,
    PF_M4_LEDGE_PROBE_LEFT = 1,
    PF_M4_LEDGE_PROBE_NONE = 2
};

static int8_t ledge_probe_direction(
    uint8_t action_state,
    uint16_t action_ticks,
    int8_t facing)
{
    if (action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
    {
        const falcon_up_special_timing *timing =
            falcon_reference_up_special_timing();

        return timing != NULL &&
                       action_ticks >= timing->air_control_begin_frame
                   ? (int8_t)PF_M4_LEDGE_PROBE_BOTH
                   : (int8_t)PF_M4_LEDGE_PROBE_NONE;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
        action_state ==
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        action_state == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL)
    {
        return facing;
    }
    return (int8_t)PF_M4_LEDGE_PROBE_NONE;
}

static int try_grab_ledge(
    const struct content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    float *position_x,
    float *position_y,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *air_jumps_remaining,
    uint8_t *short_hop_latched,
    uint8_t *fast_fall,
    uint16_t *ledge_invulnerability_ticks,
    uint16_t ledge_regrab_lockout_ticks,
    uint8_t action_state_before_catch,
    uint16_t action_ticks_before_catch,
    float previous_position_x,
    int8_t ledge_probe_direction_value,
    int8_t *facing,
    int16_t main_stick_y,
    const ssbm_ledge_response_attributes *reference_ledge_response,
    int8_t *dash_direction)
{
    const fighter_data *fighter = &content->fighter;
    const stage_data *stage = &content->stage;
    float horizontal_reach =
        fighter->half_width_f32 + fighter->air_speed_f32;
    float catch_top =
        stage->floor_y_f32 - fighter->half_height_f32;
    float catch_bottom =
        stage->floor_y_f32 + fighter->half_height_f32;
    float melee_bottom_extent_f32 = INT32_C(0);
    float left_probe_position_x = *position_x;
    float right_probe_position_x = *position_x;
    int use_melee_ledge_probe = 0;
    uint8_t ledge = (uint8_t)PF_M4_LEDGE_NONE;

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const falcon_ledge_attributes *ledge_attributes =
            falcon_reference_ledge_attributes();

        if (ledge_attributes != NULL)
        {
            horizontal_reach =
                ledge_attributes->snap_x_f32 + fighter->half_width_f32;
            catch_top =
                stage->floor_y_f32 + ledge_attributes->snap_y_f32 -
                ledge_attributes->snap_height_f32 / INT32_C(2) -
                fighter->half_height_f32;
            catch_bottom =
                stage->floor_y_f32 + ledge_attributes->snap_y_f32 +
                ledge_attributes->snap_height_f32 / INT32_C(2) -
                fighter->half_height_f32;
            left_probe_position_x =
                previous_position_x > *position_x
                    ? previous_position_x
                    : *position_x;
            right_probe_position_x =
                previous_position_x < *position_x
                    ? previous_position_x
                    : *position_x;
            use_melee_ledge_probe = 1;

            if (action_state_before_catch ==
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
                action_state_before_catch ==
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
            {
                uint16_t dive_submotion = UINT16_C(0);
                float source_frame_f32 = INT32_C(0);
                float locked_bottom_y_f32 =
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32;
                falcon_ecb_pose_f32 dive_pose;

                if (falcon_reference_direct_hsd_pose(
                        action_state_before_catch,
                        action_ticks_before_catch,
                        UINT8_C(0),
                        &dive_submotion,
                        &source_frame_f32) &&
                    falcon_direct_hsd_locked_bottom_f32(
                        action_state_before_catch,
                        source_frame_f32,
                        UINT8_C(0),
                        &locked_bottom_y_f32) &&
                    falcon_reference_hsd_ecb_pose(
                        dive_submotion,
                        source_frame_f32,
                        0,
                        locked_bottom_y_f32,
                        &dive_pose))
                {
                    horizontal_reach =
                        ledge_attributes->snap_x_f32 +
                        dive_pose.right_x_from_origin_f32;
                    melee_bottom_extent_f32 =
                        dive_pose.bottom_y_from_origin_f32;
                }
            }
        }
    }

    if (ledge_regrab_lockout_ticks != UINT16_C(0) ||
        (reference_ledge_response != NULL &&
         main_stick_y >=
             (int16_t)reference_ledge_response->grab_down_axis_threshold) ||
        *velocity_y < INT32_C(0) ||
        (use_melee_ledge_probe != 0 && *velocity_y == INT32_C(0)) ||
        *position_y < catch_top ||
        (use_melee_ledge_probe == 0 && *position_y > catch_bottom) ||
        (use_melee_ledge_probe != 0 &&
         *position_y - *velocity_y > catch_bottom) ||
        (use_melee_ledge_probe != 0 &&
         *position_y + fighter->half_height_f32 -
                 melee_bottom_extent_f32 <=
             stage->floor_y_f32))
    {
        return 0;
    }

    if (*position_x < stage->floor_left_f32 &&
        (ledge_probe_direction_value == (int8_t)PF_M4_LEDGE_PROBE_BOTH ||
         ledge_probe_direction_value == (int8_t)PF_M4_LEDGE_PROBE_LEFT) &&
         stage->floor_left_f32 - left_probe_position_x <=
            horizontal_reach)
    {
        ledge = (uint8_t)PF_M4_LEDGE_LEFT;
    }
    else if (*position_x > stage->floor_right_f32 &&
             (ledge_probe_direction_value ==
                  (int8_t)PF_M4_LEDGE_PROBE_BOTH ||
              ledge_probe_direction_value ==
                  (int8_t)PF_M4_LEDGE_PROBE_RIGHT) &&
              right_probe_position_x - stage->floor_right_f32 <=
                 horizontal_reach)
    {
        ledge = (uint8_t)PF_M4_LEDGE_RIGHT;
    }

    if (ledge == (uint8_t)PF_M4_LEDGE_NONE ||
        ledge_occupied(
            world,
            scratch,
            player_index,
            ledge))
    {
        return 0;
    }

    if (!ledge_catch_position(
            fighter,
            stage,
            ledge,
            position_x,
            position_y))
    {
        ledge_hang_position(
            fighter,
            stage,
            ledge,
            position_x,
            position_y);
    }
    *velocity_x = INT32_C(0);
    *velocity_y = INT32_C(0);
    *action_ticks = UINT16_C(0);
    *source_submotion =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH
            : UINT16_C(0);
    *grounded = UINT8_C(0);
    *action_state =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? (uint8_t)PF_M4_ACTION_LEDGE_CATCH
            : (uint8_t)PF_M4_ACTION_LEDGE_HANG;
    *support = (uint8_t)PF_M4_SURFACE_NONE;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *facing = ledge_inward_direction(ledge);
    *ledge_invulnerability_ticks =
        fighter->ledge_invulnerability_ticks;
    *dash_direction = INT8_C(0);
    return 1;
}

void reset_player(
    pf_sim *sim,
    uint32_t player_index,
    int count_respawn)
{
    const fighter_data *fighter = &sim->content.fighter;
    const stage_data *stage = &sim->content.stage;
    const int32_t centered_slot =
        (int32_t)(UINT32_C(2) * player_index + UINT32_C(1)) -
        (int32_t)sim->world.player_count;
    const uint16_t respawn_count =
        count_respawn != 0
            ? (sim->world.respawn_count[player_index] != UINT16_MAX
                   ? (uint16_t)(
                         sim->world.respawn_count[player_index] +
                         UINT16_C(1))
                   : UINT16_MAX)
            : UINT16_C(0);
    const ssbm_stage_spawn_point *reference_spawn =
        ssbm_reference_stage_spawn_point(
            stage->reference_collision_profile,
            (uint8_t)player_index);
    const uint8_t spawn_support =
        reference_spawn != NULL
            ? reference_spawn->support
            : stage_spawn_support(stage);
    uint32_t other_index;

    for (other_index = UINT32_C(0);
         other_index < (uint32_t)sim->world.player_count;
         ++other_index)
    {
        if (sim->world.grab_target_slot[other_index] ==
            (uint8_t)(player_index + UINT32_C(1)))
        {
            sim->world.grab_target_slot[other_index] = UINT8_C(0);
            if (sim->world.action_state[other_index] ==
                    (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
                sim->world.action_state[other_index] ==
                    (uint8_t)PF_M4_ACTION_PUMMEL ||
                action_is_throw(
                    sim->world.action_state[other_index]))
            {
                sim->world.action_state[other_index] =
                    (uint8_t)PF_M4_ACTION_GRAB_RELEASE;
                sim->world.action_ticks[other_index] = UINT16_C(0);
                sim->world.source_submotion[other_index] =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT;
                sim->world.source_animation_frame_f32[other_index] =
                    INT32_C(0);
                sim->world.source_animation_rate_f32[other_index] =
                    INT32_C(0);
            }
        }
        if (sim->world.grab_owner_slot[other_index] ==
            (uint8_t)(player_index + UINT32_C(1)))
        {
            sim->world.grab_owner_slot[other_index] = UINT8_C(0);
            sim->world.grab_escape_ticks[other_index] = UINT16_C(0);
            if (sim->world.action_state[other_index] ==
                (uint8_t)PF_M4_ACTION_GRABBED)
            {
                sim->world.action_state[other_index] =
                    (uint8_t)PF_M4_ACTION_GRAB_RELEASE;
                sim->world.action_ticks[other_index] = UINT16_C(0);
                sim->world.source_submotion[other_index] =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_CUT;
                sim->world.source_animation_frame_f32[other_index] =
                    INT32_C(0);
                sim->world.source_animation_rate_f32[other_index] =
                    INT32_C(0);
            }
        }
    }

    sim->world.previous_buttons[player_index] = UINT64_C(0);
    sim->world.position_x_f32[player_index] =
        reference_spawn != NULL
            ? reference_spawn->position_x_f32
            : centered_slot * stage->spawn_spacing_f32 +
                  (stage->reference_collision_profile !=
                           (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED
                       ? stage->reference_spawn_x_f32
                       : INT32_C(0));
    sim->world.position_y_f32[player_index] =
        surface_y_f32(
            &sim->content,
            spawn_support,
            sim->world.position_x_f32[player_index]) -
        fighter->half_height_f32;
    sim->world.velocity_x_f32[player_index] = INT32_C(0);
    sim->world.velocity_y_f32[player_index] = INT32_C(0);
    sim->world.match_kos[player_index] = UINT16_C(0);
    sim->world.match_falls[player_index] = UINT16_C(0);
    sim->world.shield_recoil_x_f32[player_index] = INT32_C(0);
    sim->world.shield_recoil_mask =
        (uint8_t)(
            sim->world.shield_recoil_mask &
            (uint8_t)~(UINT8_C(1) << player_index));
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.source_submotion[player_index] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
    sim->world.source_animation_frame_f32[player_index] = INT32_C(0);
    sim->world.source_animation_rate_f32[player_index] = INT32_C(0);
    sim->world.fall_animation_blend_f32[player_index] = INT32_C(0);
    sim->world.fall_animation_target_switched[player_index] = UINT8_C(0);
    sim->world.ecb_bottom_lock_ticks[player_index] = UINT8_C(0);
    sim->world.ecb_locked_bottom_y_f32[player_index] = INT32_C(0);
    sim->world.respawn_count[player_index] = respawn_count;
    sim->world.respawn_ticks[player_index] = UINT16_C(0);
    sim->world.respawn_invulnerability_ticks[player_index] =
        UINT16_C(0);
    sim->world.ledge_invulnerability_ticks[player_index] =
        UINT16_C(0);
    sim->world.ledge_regrab_lockout_ticks[player_index] =
        UINT16_C(0);
    sim->world.grab_escape_ticks[player_index] = UINT16_C(0);
    sim->world.damage_jump_buffer_ticks[player_index] = UINT16_C(0);
    sim->world.charge_ticks[player_index] = UINT16_C(0);
    sim->world.smash_charge_ticks[player_index] = UINT16_C(0);
    sim->world.shield_strength[player_index] = UINT16_C(0);
    sim->world.shield_angle_turn[player_index] = UINT16_C(0);
    sim->world.shield_magnitude[player_index] = UINT16_C(0);
    sim->world.grab_target_slot[player_index] = UINT8_C(0);
    sim->world.grab_owner_slot[player_index] = UINT8_C(0);
    sim->world.grounded[player_index] = UINT8_C(1);
    sim->world.active[player_index] = UINT8_C(1);
    sim->world.stocks_remaining[player_index] =
        sim->world.stock_count;
    sim->world.action_state[player_index] =
        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
    sim->world.support[player_index] =
        spawn_support;
    sim->world.air_jumps_remaining[player_index] =
        fighter->air_jump_count;
    sim->world.recovery_available[player_index] = UINT8_C(1);
    sim->world.short_hop_latched[player_index] = UINT8_C(0);
    sim->world.platform_drop_ticks[player_index] = UINT8_C(0);
    sim->world.fast_fall[player_index] = UINT8_C(0);
    sim->world.facing[player_index] =
        centered_slot <= INT32_C(0) ? INT8_C(1) : INT8_C(-1);
    sim->world.dash_direction[player_index] = INT8_C(0);
    sim->world.previous_strong_direction[player_index] = INT8_C(0);
    sim->world.previous_directional_input_flags[player_index] = UINT8_C(0);
    sim->world.previous_tilt_x_direction[player_index] = INT8_C(0);
    sim->world.previous_tilt_y_direction[player_index] = INT8_C(0);
    sim->world.mash_stick_x_direction[player_index] = INT8_C(0);
    sim->world.mash_stick_y_direction[player_index] = INT8_C(0);
    sim->world.previous_secondary_stick_x[player_index] = INT16_C(0);
    sim->world.previous_secondary_stick_y[player_index] = INT16_C(0);
    sim->world.previous_main_stick_x[player_index] = INT16_C(0);
    sim->world.previous_main_stick_y[player_index] = INT16_C(0);
    sim->world.tilt_x_age[player_index] = UINT8_C(254);
    sim->world.tilt_y_age[player_index] = UINT8_C(254);
    sim->world.ucf_tilt_x_age[player_index] = UINT8_C(254);
    sim->world.ucf_tilt_y_age[player_index] = UINT8_C(254);
    sim->world.raw_main_t2_x[player_index] = INT8_C(0);
    sim->world.raw_main_t2_y[player_index] = INT8_C(0);
    sim->world.ucf_pad_buffer_count[player_index] = UINT8_C(0);
    sim->world.horizontal_input_age[player_index] = UINT8_C(254);
    sim->world.horizontal_input_direction[player_index] = INT8_C(0);
    sim->world.damage_f32[player_index] = UINT32_C(0);
    sim->world.knockback_velocity_x_f32[player_index] = INT32_C(0);
    sim->world.knockback_velocity_y_f32[player_index] = INT32_C(0);
    sim->world.ground_knockback_velocity_f32[player_index] = INT32_C(0);
    sim->world.last_hit_sequence[player_index] = UINT32_C(0);
    sim->world.last_hit_tick[player_index] = UINT64_C(0);
    sim->world.last_hit_damage_f32[player_index] = UINT32_C(0);
    sim->world.damage_time_since_hit_ticks[player_index] = UINT8_C(0);
    sim->world.hitlag_ticks[player_index] = UINT16_C(0);
    sim->world.hitstun_ticks[player_index] = UINT16_C(0);
    sim->world.tech_window_ticks[player_index] = UINT16_C(0);
    sim->world.tech_lockout_ticks[player_index] = UINT16_C(0);
    sim->world.shield_stun_ticks[player_index] = UINT16_C(0);
    sim->world.shield_health_f32[player_index] =
        fighter->shield_health_f32;
    sim->world.hitlag_resume_action[player_index] = UINT8_C(0);
    sim->world.attack_hit_mask[player_index] = UINT8_C(0);
    sim->world.attack_stale_registered[player_index] = UINT8_C(0);
    sim->world.falcon_kick_hit_count[player_index] = UINT8_C(0);
    sim->world.rebound_duration_ticks[player_index] = UINT16_C(0);
    sim->world.jab_chain_buffered[player_index] = UINT8_C(0);
    sim->world.rapid_jab_input_count[player_index] = UINT8_C(0);
    sim->world.rapid_jab_continue[player_index] = UINT8_C(0);
    sim->world.down_tilt_repeat_buffered[player_index] = UINT8_C(0);
    sim->world.stale_move_count[player_index] = UINT8_C(0);
    (void)memset(
        sim->world.stale_move_ids[player_index],
        0,
        sizeof(sim->world.stale_move_ids[player_index]));
    sim->world.last_hit_attacker[player_index] = UINT8_C(0);
    sim->world.shield_held[player_index] = UINT8_C(0);
    sim->world.trigger_input_age[player_index] = UINT8_MAX;
    sim->world.prone_attack_input_age[player_index] = UINT8_MAX;
    sim->world.up_special_input_age[player_index] = UINT8_MAX;
    sim->world.powershield[player_index] = UINT8_C(0);
    sim->world.guard_dash_grab_window_ticks[player_index] = UINT8_C(0);
    sim->world.tumble[player_index] = UINT8_C(0);
    sim->world.sdi_pulse_count[player_index] = UINT8_C(0);
    sim->world.sdi_direction_x[player_index] = INT8_C(0);
    sim->world.sdi_direction_y[player_index] = INT8_C(0);
    sim->world.tech_direction[player_index] = INT8_C(0);
    sim->world.prone_orientation[player_index] =
        (uint8_t)PF_M4_PRONE_NONE;
    sim->world.prone_roll_motion_orientation[player_index] =
        (uint8_t)PF_M4_PRONE_NONE;
}

static void land(
    const fighter_data *fighter,
    float surface_y_f32_value,
    uint8_t surface,
    float *position_y,
    float *velocity_y,
    uint16_t *action_ticks,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *air_jumps_remaining,
    uint8_t *short_hop_latched,
    uint8_t *fast_fall,
    int8_t *dash_direction)
{
    (void)velocity_y;
    *position_y = surface_y_f32_value - fighter->half_height_f32;
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(1);
    *action_state = (uint8_t)PF_M4_ACTION_LANDING;
    *support = surface;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
}

static void enter_shield_break_launch(
    const fighter_data *fighter,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *short_hop_latched,
    uint8_t *fast_fall,
    int8_t *dash_direction)
{
    *velocity_x = INT32_C(0);
    *velocity_y = -fighter->shield_break_launch_speed_f32;
    *action_ticks = UINT16_C(0);
    *source_submotion =
        (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_FLY;
    *grounded = UINT8_C(0);
    *action_state = (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
    *support = (uint8_t)PF_M4_SURFACE_NONE;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
    scratch->shield_stun_ticks[player_index] = UINT16_C(0);
    scratch->powershield[player_index] = UINT8_C(0);
    scratch->shield_strength[player_index] = UINT16_C(0);
    scratch->shield_angle_turn[player_index] = UINT16_C(0);
    scratch->shield_magnitude[player_index] = UINT16_C(0);
    scratch->tech_window_ticks[player_index] = UINT16_C(0);
    scratch->tech_lockout_ticks[player_index] = UINT16_C(0);
    scratch->tumble[player_index] = UINT8_C(0);
    scratch->tech_direction[player_index] = INT8_C(0);
    scratch->attack_hit_mask[player_index] = UINT8_C(0);
    scratch->attack_stale_registered[player_index] = UINT8_C(0);
}

static void initialize_ground_knockback_from_air(
    const struct content *content,
    uint8_t surface,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const ssbm_damage_response_attributes *common =
        ssbm_common_reference_damage_response();
    float scalar =
        scratch->knockback_velocity_x_f32[player_index];

    if (common == NULL)
    {
        scalar = 0.0f;
    }
    else if (scalar > common->ground_knockback_max_speed_f32)
    {
        scalar = common->ground_knockback_max_speed_f32;
    }
    else if (scalar < -common->ground_knockback_max_speed_f32)
    {
        scalar = -common->ground_knockback_max_speed_f32;
    }
    scratch->ground_knockback_velocity_f32[player_index] = scalar;
    project_ground_scalar_f32(
        content,
        surface,
        scalar,
        &scratch->knockback_velocity_x_f32[player_index],
        &scratch->knockback_velocity_y_f32[player_index]);
}

static void land_from_air(
    const struct content *content,
    float surface_y_f32_value,
    uint8_t surface,
    int16_t horizontal_input,
    int8_t facing,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    float *position_y,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *air_jumps_remaining,
    uint8_t *short_hop_latched,
    uint8_t *fast_fall,
    int8_t *dash_direction)
{
    const fighter_data *fighter = &content->fighter;
    ssbm_damage_floor_response damage_floor_response =
        PF_M4_SSBM_DAMAGE_FLOOR_LANDING;
    int force_down_bound = 0;
    const int8_t roll_direction =
        axis_direction(
            horizontal_input,
            fighter->tech_roll_axis_threshold);
    const float incoming_velocity_x = total_velocity_f32(
        *velocity_x,
        scratch->knockback_velocity_x_f32[player_index]);

    scratch->prone_orientation[player_index] =
        (uint8_t)PF_M4_PRONE_NONE;

    if (*action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *velocity_x = INT32_C(0);
        /* ftCommon_8007D7FC transfers the horizontal component to gr_vel but
         * leaves self_vel.y observable on ShieldBreakDown frame 1. The first
         * grounded physics callback clears it on the following frame. */
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state =
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN;
        *source_submotion =
            falcon_reference_shield_break_down_submotion();
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        scratch->tech_lockout_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
    {
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        *grounded = UINT8_C(1);
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND)
    {
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *velocity_y = INT32_C(0);
        *grounded = UINT8_C(1);
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR)
    {
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state =
            (uint8_t)PF_M4_ACTION_FALCON_KICK_LANDING;
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state ==
        (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND)
    {
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state ==
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT)
    {
        const int hit =
            *action_state ==
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR ||
            *action_state ==
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT;

        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state =
            hit != 0
                ? (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT
                : (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS;
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW ||
        ((*action_state ==
              (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
          *action_state ==
              (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR) &&
         *action_ticks >=
             falcon_reference_up_special_timing()
                 ->air_control_begin_frame))
    {
        const int preserve_fall_special_velocity =
            *action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL;

        *position_y = surface_y_f32_value - fighter->half_height_f32;
        if (preserve_fall_special_velocity == 0)
        {
            *velocity_y = INT32_C(0);
        }
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state =
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING;
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE)
    {
        /* CatchCut/CaptureCut collision converts air movement to ground
         * movement without changing the motion state or restarting its
         * animation. Both source friction multipliers (x64/x36C) are 1.0
         * in NTSC 1.02, so the ordinary grounded release branch can resume
         * with the same clock and horizontal velocity. */
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *velocity_y = INT32_C(0);
        *grounded = UINT8_C(1);
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        return;
    }

    if (*action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        *action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        *action_state == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT)
    {
        *position_y = surface_y_f32_value - fighter->half_height_f32;
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state = (uint8_t)PF_M4_ACTION_SPECIAL_LANDING;
        *support = surface;
        *air_jumps_remaining = fighter->air_jump_count;
        *short_hop_latched = UINT8_C(0);
        *fast_fall = UINT8_C(0);
        *dash_direction = INT8_C(0);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (action_is_light_aerial(*action_state) ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK)
    {
        const uint8_t aerial_action = *action_state;
        const int strong_aerial =
            *action_state ==
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK;
        const int landing_lag_active =
            strong_aerial != 0 ||
            light_aerial_landing_lag_active(
                fighter,
                aerial_action,
                *action_ticks);

        land(
            fighter,
            surface_y_f32_value,
            surface,
            position_y,
            velocity_y,
            action_ticks,
            grounded,
            action_state,
            support,
            air_jumps_remaining,
            short_hop_latched,
            fast_fall,
            dash_direction);
        if (landing_lag_active)
        {
            const int l_cancelled =
                scratch->trigger_input_age[player_index] <
                fighter->l_cancel_window_ticks;

            if (strong_aerial != 0)
            {
                *action_state =
                    l_cancelled != 0
                        ? (uint8_t)
                              PF_M4_ACTION_STRONG_L_CANCEL_LANDING
                        : (uint8_t)
                              PF_M4_ACTION_STRONG_AERIAL_LANDING;
            }
            else
            {
                *action_state = aerial_landing_action(
                    aerial_action,
                    l_cancelled);
            }
        }
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (*action_state == (uint8_t)PF_M4_ACTION_REFLECTOR_AIR)
    {
        land(
            fighter,
            surface_y_f32_value,
            surface,
            position_y,
            velocity_y,
            action_ticks,
            grounded,
            action_state,
            support,
            air_jumps_remaining,
            short_hop_latched,
            fast_fall,
            dash_direction);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    if (scratch->tumble[player_index] == UINT8_C(0))
    {
        if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
            *action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
        {
            damage_floor_response =
                ssbm_select_damage_floor_response_f32(
                    scratch->knockback_velocity_x_f32[player_index],
                    scratch->knockback_velocity_y_f32[player_index],
                    UINT8_C(0));
        }
        if (damage_floor_response ==
            PF_M4_SSBM_DAMAGE_FLOOR_KEEP_ACTION)
        {
            /* ftCommon_8007D7FC changes only the kinetic state. Damage_Coll
             * retains the current damage motion, clock, hitstun timer, self
             * velocity, and full x8c vector below common-data x1E4. */
            *position_y = surface_y_f32_value - fighter->half_height_f32;
            *grounded = UINT8_C(1);
            *support = surface;
            *air_jumps_remaining = fighter->air_jump_count;
            *short_hop_latched = UINT8_C(0);
            *fast_fall = UINT8_C(0);
            *dash_direction = INT8_C(0);
            scratch->tech_direction[player_index] = INT8_C(0);
            return;
        }
        force_down_bound =
            damage_floor_response ==
            PF_M4_SSBM_DAMAGE_FLOOR_DOWN_BOUND;
        if (force_down_bound == 0)
        {
            /* ftCo_Landing_Enter_Basic changes ground/air state without
             * rewriting x8c_kb_vel or initializing xF0_ground_kb_vel.
             * Preserve both entry channels; the following global knockback
             * update derives xF0, decays it, and projects x8c onto the live
             * floor tangent. */
            scratch->hitstun_ticks[player_index] = UINT16_C(0);
            land(
                fighter,
                surface_y_f32_value,
                surface,
                position_y,
                velocity_y,
                action_ticks,
                grounded,
                action_state,
                support,
                air_jumps_remaining,
                short_hop_latched,
                fast_fall,
                dash_direction);
            scratch->tech_direction[player_index] = INT8_C(0);
            return;
        }
    }

    *position_y = surface_y_f32_value - fighter->half_height_f32;
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(1);
    *support = surface;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
    scratch->tumble[player_index] = UINT8_C(0);
    scratch->ground_knockback_velocity_f32[player_index] = INT32_C(0);

    if (force_down_bound == 0 &&
        scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        if (roll_direction == INT8_C(0))
        {
            *velocity_x = INT32_C(0);
            *action_state =
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE;
            scratch->tech_direction[player_index] = INT8_C(0);
            /* Passive (neutral tech) calls ftCommon_8007CCE8 on entry. */
            initialize_ground_knockback_from_air(
                content,
                surface,
                scratch,
                player_index);
        }
        else
        {
            *velocity_x = INT32_C(0);
            *action_state = (uint8_t)PF_M4_ACTION_TECH_ROLL;
            scratch->tech_direction[player_index] = roll_direction;
            /* PassiveStandF/B deliberately omit ftCommon_8007CCE8. Their
             * entry row retains the complete air x8c vector with xF0 zero. */
        }
    }
    else
    {
        *velocity_x = INT32_C(0);
        *action_state = (uint8_t)PF_M4_ACTION_KNOCKDOWN;
        scratch->tech_direction[player_index] = INT8_C(0);
        scratch->prone_orientation[player_index] =
            incoming_velocity_x != INT32_C(0) &&
                    ((incoming_velocity_x > INT32_C(0)) ==
                     (facing > INT8_C(0)))
                ? (uint8_t)PF_M4_PRONE_STOMACH
                : (uint8_t)PF_M4_PRONE_BACK;
        /* DownBound applies ftCommon_8007CCE8 after its motion change. */
        initialize_ground_knockback_from_air(
            content,
            surface,
            scratch,
            player_index);
    }
}

static int action_can_start_grab(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
           action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN ||
           action_state == (uint8_t)PF_M4_ACTION_TEETER ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
           action_is_damage(action_state);
}

static int action_can_start_dash_attack(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    return action_state == (uint8_t)PF_M4_ACTION_RUN ||
           (action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
            action_ticks >= fighter->forward_smash_input_window_ticks);
}

static int action_can_start_taunt(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_END ||
           action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN ||
           action_state == (uint8_t)PF_M4_ACTION_TEETER ||
           action_is_damage(action_state);
}

static int normal_landing_is_interruptible(
    const fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    return action_state != (uint8_t)PF_M4_ACTION_LANDING ||
           (uint32_t)action_ticks + UINT32_C(1) >=
               (uint32_t)fighter->landing_interruptible_tick;
}

static int drop_cancel_hitlag_is_eligible(
    const fighter_data *fighter,
    uint16_t action_ticks,
    uint16_t hitlag_ticks,
    uint8_t hitlag_resume_action,
    uint8_t platform_drop_ticks)
{
    const int32_t expected_timer_delta =
        (int32_t)fighter->platform_drop_ticks -
        (int32_t)fighter->aerial_startup_ticks - INT32_C(1) -
        (int32_t)fighter->aerial_hitlag_ticks;
    const int32_t timer_delta =
        (int32_t)platform_drop_ticks - (int32_t)hitlag_ticks;

    return hitlag_resume_action ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           action_ticks == fighter->aerial_startup_ticks &&
           timer_delta == expected_timer_delta;
}

static pf_status enter_wall_impact(
    const fighter_data *fighter,
    int wall_tech_jump,
    int8_t away_direction,
    float source_normal_x_f32,
    float source_normal_y_f32,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint8_t *action_state,
    uint8_t *fast_fall,
    int8_t *facing)
{
    const float total_velocity_x = total_velocity_f32(
        *velocity_x,
        scratch->knockback_velocity_x_f32[player_index]);
    const float total_velocity_y = total_velocity_f32(
        *velocity_y,
        scratch->knockback_velocity_y_f32[player_index]);
    *action_ticks = UINT16_C(0);
    *fast_fall = UINT8_C(0);
    *facing = away_direction;
    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_f32[player_index] = INT32_C(0);
        scratch->knockback_velocity_y_f32[player_index] = INT32_C(0);
        scratch->ground_knockback_velocity_f32[player_index] = INT32_C(0);
        *action_state =
            wall_tech_jump != 0
                ? (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP
                : (uint8_t)PF_M4_ACTION_WALL_TECH;
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        scratch->tech_direction[player_index] = away_direction;
    }
    else
    {
        float reflected_velocity_x = total_velocity_x;
        float reflected_velocity_y = total_velocity_y;
        const pf_status status = ssbm_mirror_velocity_f32(
            source_normal_x_f32,
            source_normal_y_f32,
            fighter->surface_bounce_multiplier_f32,
            &reflected_velocity_x,
            &reflected_velocity_y);

        if (status != PF_STATUS_OK)
        {
            return status;
        }
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_f32[player_index] =
            reflected_velocity_x;
        scratch->knockback_velocity_y_f32[player_index] =
            reflected_velocity_y;
        *action_state = (uint8_t)PF_M4_ACTION_WALL_BOUNCE;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    return PF_STATUS_OK;
}

static pf_status enter_ceiling_impact(
    const fighter_data *fighter,
    float source_normal_x_f32,
    float source_normal_y_f32,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint8_t *action_state,
    uint8_t *fast_fall)
{
    *action_ticks = UINT16_C(0);
    *fast_fall = UINT8_C(0);
    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_f32[player_index] = INT32_C(0);
        scratch->knockback_velocity_y_f32[player_index] = INT32_C(0);
        scratch->ground_knockback_velocity_f32[player_index] = INT32_C(0);
        *action_state = (uint8_t)PF_M4_ACTION_CEILING_TECH;
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    else
    {
        float total_velocity_x = total_velocity_f32(
            *velocity_x,
            scratch->knockback_velocity_x_f32[player_index]);
        float total_velocity_y = total_velocity_f32(
            *velocity_y,
            scratch->knockback_velocity_y_f32[player_index]);
        const pf_status status = ssbm_mirror_velocity_f32(
            source_normal_x_f32,
            source_normal_y_f32,
            fighter->surface_bounce_multiplier_f32,
            &total_velocity_x,
            &total_velocity_y);

        if (status != PF_STATUS_OK)
        {
            return status;
        }

        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_f32[player_index] =
            total_velocity_x;
        scratch->knockback_velocity_y_f32[player_index] =
            total_velocity_y;
        *action_state = (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    return PF_STATUS_OK;
}

static void write_scratch(
    pf_sim_scratch *scratch,
    uint32_t player_index,
    const pf_input_frame *input,
    float position_x,
    float position_y,
    float velocity_x,
    float velocity_y,
    uint16_t action_ticks,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    float source_animation_rate_f32,
    uint16_t respawn_count,
    uint8_t grounded,
    uint8_t action_state,
    uint8_t support,
    uint8_t air_jumps_remaining,
    uint8_t recovery_available,
    uint8_t short_hop_latched,
    uint8_t platform_drop_ticks,
    uint8_t fast_fall,
    int8_t facing,
    int8_t dash_direction,
    int8_t previous_strong_direction,
    uint8_t directional_input_flags,
    int8_t tilt_x_direction,
    int8_t tilt_y_direction,
    uint8_t tilt_x_age,
    uint8_t tilt_y_age)
{
    scratch->previous_buttons[player_index] = input->buttons;
    scratch->previous_secondary_stick_x[player_index] =
        input->secondary_stick_x;
    scratch->previous_secondary_stick_y[player_index] =
        input->secondary_stick_y;
    scratch->previous_main_stick_x[player_index] = input->main_stick_x;
    scratch->previous_main_stick_y[player_index] = input->main_stick_y;
    scratch->position_x_f32[player_index] = position_x;
    scratch->position_y_f32[player_index] = position_y;
    scratch->velocity_x_f32[player_index] = velocity_x;
    scratch->velocity_y_f32[player_index] = velocity_y;
    scratch->action_ticks[player_index] = action_ticks;
    scratch->source_submotion[player_index] = source_submotion;
    scratch->source_animation_frame_f32[player_index] =
        source_animation_frame_f32;
    scratch->source_animation_rate_f32[player_index] =
        source_animation_rate_f32;
    scratch->respawn_count[player_index] = respawn_count;
    scratch->grounded[player_index] = grounded;
    scratch->action_state[player_index] = action_state;
    scratch->support[player_index] = support;
    scratch->air_jumps_remaining[player_index] =
        air_jumps_remaining;
    scratch->recovery_available[player_index] =
        grounded != UINT8_C(0) &&
                action_state !=
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND
            ? UINT8_C(1)
            : recovery_available;
    scratch->short_hop_latched[player_index] = short_hop_latched;
    scratch->platform_drop_ticks[player_index] =
        platform_drop_ticks;
    scratch->fast_fall[player_index] = fast_fall;
    scratch->facing[player_index] = facing;
    scratch->dash_direction[player_index] = dash_direction;
    scratch->previous_strong_direction[player_index] =
        previous_strong_direction;
    scratch->previous_directional_input_flags[player_index] =
        directional_input_flags;
    scratch->previous_tilt_x_direction[player_index] =
        tilt_x_direction;
    scratch->previous_tilt_y_direction[player_index] =
        tilt_y_direction;
    scratch->tilt_x_age[player_index] = tilt_x_age;
    scratch->tilt_y_age[player_index] = tilt_y_age;
}

static void copy_combat_scratch(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    scratch->mash_stick_x_direction[player_index] =
        world->mash_stick_x_direction[player_index];
    scratch->mash_stick_y_direction[player_index] =
        world->mash_stick_y_direction[player_index];
    scratch->active[player_index] =
        world->active[player_index];
    scratch->stocks_remaining[player_index] =
        world->stocks_remaining[player_index];
    scratch->respawn_ticks[player_index] =
        world->respawn_ticks[player_index];
    scratch->respawn_invulnerability_ticks[player_index] =
        world->respawn_invulnerability_ticks[player_index];
    scratch->ledge_invulnerability_ticks[player_index] =
        world->ledge_invulnerability_ticks[player_index];
    scratch->ledge_regrab_lockout_ticks[player_index] =
        world->ledge_regrab_lockout_ticks[player_index];
    scratch->grab_escape_ticks[player_index] =
        world->grab_escape_ticks[player_index];
    scratch->damage_jump_buffer_ticks[player_index] =
        world->damage_jump_buffer_ticks[player_index];
    scratch->charge_ticks[player_index] =
        world->charge_ticks[player_index];
    scratch->smash_charge_ticks[player_index] =
        world->smash_charge_ticks[player_index];
    scratch->shield_strength[player_index] =
        world->shield_strength[player_index];
    scratch->shield_angle_turn[player_index] =
        world->shield_angle_turn[player_index];
    scratch->shield_magnitude[player_index] =
        world->shield_magnitude[player_index];
    scratch->grab_target_slot[player_index] =
        world->grab_target_slot[player_index];
    scratch->grab_owner_slot[player_index] =
        world->grab_owner_slot[player_index];
    scratch->damage_f32[player_index] =
        world->damage_f32[player_index];
    scratch->knockback_velocity_x_f32[player_index] =
        world->knockback_velocity_x_f32[player_index];
    scratch->knockback_velocity_y_f32[player_index] =
        world->knockback_velocity_y_f32[player_index];
    scratch->ground_knockback_velocity_f32[player_index] =
        world->ground_knockback_velocity_f32[player_index];
    scratch->last_hit_sequence[player_index] =
        world->last_hit_sequence[player_index];
    scratch->last_hit_tick[player_index] =
        world->last_hit_tick[player_index];
    scratch->last_hit_damage_f32[player_index] =
        world->last_hit_damage_f32[player_index];
    scratch->damage_time_since_hit_ticks[player_index] =
        world->damage_time_since_hit_ticks[player_index];
    scratch->hitlag_ticks[player_index] =
        world->hitlag_ticks[player_index];
    scratch->hitstun_ticks[player_index] =
        world->hitstun_ticks[player_index];
    scratch->tech_window_ticks[player_index] =
        world->tech_window_ticks[player_index];
    scratch->tech_lockout_ticks[player_index] =
        world->tech_lockout_ticks[player_index];
    scratch->shield_stun_ticks[player_index] =
        world->shield_stun_ticks[player_index];
    scratch->shield_health_f32[player_index] =
        world->shield_health_f32[player_index];
    scratch->hitlag_resume_action[player_index] =
        world->hitlag_resume_action[player_index];
    scratch->attack_hit_mask[player_index] =
        world->attack_hit_mask[player_index];
    scratch->attack_stale_registered[player_index] =
        world->attack_stale_registered[player_index];
    scratch->falcon_kick_hit_count[player_index] =
        world->falcon_kick_hit_count[player_index];
    scratch->rebound_duration_ticks[player_index] =
        world->rebound_duration_ticks[player_index];
    scratch->jab_chain_buffered[player_index] =
        world->jab_chain_buffered[player_index];
    scratch->rapid_jab_input_count[player_index] =
        world->rapid_jab_input_count[player_index];
    scratch->rapid_jab_continue[player_index] =
        world->rapid_jab_continue[player_index];
    scratch->down_tilt_repeat_buffered[player_index] =
        world->down_tilt_repeat_buffered[player_index];
    scratch->last_hit_attacker[player_index] =
        world->last_hit_attacker[player_index];
    scratch->shield_held[player_index] =
        world->shield_held[player_index];
    scratch->trigger_input_age[player_index] =
        world->trigger_input_age[player_index];
    scratch->prone_attack_input_age[player_index] =
        world->prone_attack_input_age[player_index];
    scratch->up_special_input_age[player_index] =
        world->up_special_input_age[player_index];
    scratch->powershield[player_index] =
        world->powershield[player_index];
    scratch->guard_dash_grab_window_ticks[player_index] =
        world->guard_dash_grab_window_ticks[player_index];
    scratch->tumble[player_index] =
        world->tumble[player_index];
    scratch->sdi_pulse_count[player_index] =
        world->sdi_pulse_count[player_index];
    scratch->sdi_direction_x[player_index] =
        world->sdi_direction_x[player_index];
    scratch->sdi_direction_y[player_index] =
        world->sdi_direction_y[player_index];
    scratch->tech_direction[player_index] =
        world->tech_direction[player_index];
    scratch->prone_orientation[player_index] =
        world->prone_orientation[player_index];
    scratch->prone_roll_motion_orientation[player_index] =
        world->prone_roll_motion_orientation[player_index];
}

static float revival_platform_y(
    const stage_data *stage,
    uint16_t action_ticks)
{
    const uint16_t descent_ticks =
        stage->revival_platform_descent_ticks;
    const uint16_t elapsed =
        action_ticks < descent_ticks ? action_ticks : descent_ticks;
    const float distance =
        stage->revival_platform_end_y_f32 -
        stage->revival_platform_start_y_f32;

    return stage->revival_platform_start_y_f32 +
           distance * (float)elapsed / (float)descent_ticks;
}

static float revival_platform_x(
    const stage_data *stage,
    const pf_world_state *world,
    uint32_t player_index)
{
    return ssbm_revival_platform_x_f32(
        stage->reference_collision_profile,
        world->player_count,
        (uint8_t)player_index,
        stage->spawn_spacing_f32);
}

static void prepare_spawn(
    const fighter_data *fighter,
    const stage_data *stage,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    float *position_x,
    float *position_y,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *air_jumps_remaining,
    uint8_t *short_hop_latched,
    uint8_t *platform_drop_ticks,
    uint8_t *fast_fall,
    int8_t *facing,
    int8_t *dash_direction,
    int8_t *previous_strong_direction,
    uint8_t *directional_input_flags)
{
    *position_x = revival_platform_x(
        stage,
        world,
        player_index);
    /* Rebirth physics advances once on the entry update while its displayed
     * action counter is still zero. */
    *position_y = revival_platform_y(stage, UINT16_C(1)) -
                  fighter->half_height_f32;
    *velocity_x = INT32_C(0);
    *velocity_y = INT32_C(0);
    *action_ticks = UINT16_C(0);
    *source_submotion =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT
            : UINT16_C(0);
    /* Slippi/HSD classify Rebirth as airborne even while its accessory
     * platform supplies the scripted support trajectory. */
    *grounded = UINT8_C(0);
    *action_state = (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM;
    *support = (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *platform_drop_ticks = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *facing =
        *position_x <= INT32_C(0) ? INT8_C(1) : INT8_C(-1);
    *dash_direction = INT8_C(0);
    *previous_strong_direction = INT8_C(0);
    *directional_input_flags = UINT8_C(0);
    scratch->damage_f32[player_index] = UINT32_C(0);
    scratch->knockback_velocity_x_f32[player_index] = INT32_C(0);
    scratch->knockback_velocity_y_f32[player_index] = INT32_C(0);
    scratch->ground_knockback_velocity_f32[player_index] = INT32_C(0);
    scratch->last_hit_sequence[player_index] = UINT32_C(0);
    scratch->last_hit_tick[player_index] = UINT64_C(0);
    scratch->last_hit_damage_f32[player_index] = UINT32_C(0);
    scratch->damage_time_since_hit_ticks[player_index] = UINT8_C(0);
    scratch->hitlag_ticks[player_index] = UINT16_C(0);
    scratch->hitstun_ticks[player_index] = UINT16_C(0);
    scratch->tech_window_ticks[player_index] = UINT16_C(0);
    scratch->tech_lockout_ticks[player_index] = UINT16_C(0);
    scratch->shield_stun_ticks[player_index] = UINT16_C(0);
    scratch->shield_recoil_x_f32[player_index] = INT32_C(0);
    scratch->shield_recoil_mask =
        (uint8_t)(
            scratch->shield_recoil_mask &
            (uint8_t)~(UINT8_C(1) << player_index));
    scratch->shield_health_f32[player_index] =
        fighter->shield_health_f32;
    scratch->hitlag_resume_action[player_index] = UINT8_C(0);
    scratch->attack_hit_mask[player_index] = UINT8_C(0);
    scratch->attack_stale_registered[player_index] = UINT8_C(0);
    scratch->falcon_kick_hit_count[player_index] = UINT8_C(0);
    scratch->rebound_duration_ticks[player_index] = UINT16_C(0);
    scratch->jab_chain_buffered[player_index] = UINT8_C(0);
    scratch->rapid_jab_input_count[player_index] = UINT8_C(0);
    scratch->rapid_jab_continue[player_index] = UINT8_C(0);
    scratch->down_tilt_repeat_buffered[player_index] = UINT8_C(0);
    scratch->last_hit_attacker[player_index] = UINT8_C(0);
    scratch->shield_held[player_index] = UINT8_C(0);
    scratch->trigger_input_age[player_index] = UINT8_MAX;
    scratch->prone_attack_input_age[player_index] = UINT8_MAX;
    scratch->up_special_input_age[player_index] = UINT8_MAX;
    scratch->powershield[player_index] = UINT8_C(0);
    scratch->guard_dash_grab_window_ticks[player_index] = UINT8_C(0);
    scratch->tumble[player_index] = UINT8_C(0);
    scratch->sdi_pulse_count[player_index] = UINT8_C(0);
    scratch->sdi_direction_x[player_index] = INT8_C(0);
    scratch->sdi_direction_y[player_index] = INT8_C(0);
    scratch->tech_direction[player_index] = INT8_C(0);
    scratch->ledge_invulnerability_ticks[player_index] =
        UINT16_C(0);
    scratch->ledge_regrab_lockout_ticks[player_index] =
        UINT16_C(0);
    scratch->grab_escape_ticks[player_index] = UINT16_C(0);
    scratch->damage_jump_buffer_ticks[player_index] = UINT16_C(0);
    scratch->charge_ticks[player_index] = UINT16_C(0);
    scratch->smash_charge_ticks[player_index] = UINT16_C(0);
    scratch->shield_strength[player_index] = UINT16_C(0);
    scratch->shield_angle_turn[player_index] = UINT16_C(0);
    scratch->shield_magnitude[player_index] = UINT16_C(0);
    scratch->grab_target_slot[player_index] = UINT8_C(0);
    scratch->grab_owner_slot[player_index] = UINT8_C(0);
}

static void enter_wall_jump(
    const fighter_data *fighter,
    int8_t away_direction,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint8_t *action_state,
    uint8_t *fast_fall,
    int8_t *facing)
{
    *velocity_x =
        (float)away_direction * fighter->wall_jump_speed_x_f32;
    *velocity_y = -fighter->wall_jump_speed_y_f32;
    *action_ticks = UINT16_C(0);
    *action_state = (uint8_t)PF_M4_ACTION_WALL_JUMP;
    *fast_fall = UINT8_C(0);
    *facing = away_direction;
}

static void enter_double_jump(
    const fighter_data *fighter,
    const pf_input_frame *input,
    float *velocity_x,
    float *velocity_y,
    uint8_t *air_jumps_remaining,
    uint8_t *fast_fall,
    uint8_t *tilt_y_age,
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    int8_t facing)
{
    *velocity_x = scale_axis_f32(
        input->main_stick_x,
        fighter->double_jump_horizontal_speed_f32);
    /* Melee enters JumpAerial during IASA, then executes its ordinary aerial
     * physics callback on that same fighter update. Apply air control to the
     * newly assigned jump velocity here; applying it before entry is lost when
     * the entry callback replaces self_vel.x. */
    *velocity_x = apply_air_input(
        fighter,
        *velocity_x,
        input->main_stick_x,
        fighter->air_speed_f32);
    *velocity_y = -fighter->double_jump_speed_f32;
    --*air_jumps_remaining;
    *fast_fall = UINT8_C(0);
    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        /* ftCo_JumpAerial_Enter resets x671, but UCF's independent x674
         * continuity is intentionally retained by the caller. */
        *tilt_y_age = UINT8_C(254);
    }
    *action_state = fighter->double_jump_cancel_ticks > UINT16_C(0)
                        ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
                        : (uint8_t)PF_M4_ACTION_AIRBORNE;
    *action_ticks = UINT16_C(0);
    *source_submotion =
        falcon_jump_submotion(input, facing, 1);
}

static void enter_ground_jump(
    const fighter_data *fighter,
    int16_t entry_main_stick_x,
    uint8_t short_hop_latched,
    int8_t facing,
    float *velocity_x,
    float *velocity_y,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *short_hop_latched_out,
    uint8_t *fast_fall,
    uint8_t *tilt_y_age)
{
    const float carried_velocity_x = multiply_f32(
        *velocity_x,
        fighter->jump_horizontal_momentum_multiplier_f32);
    const float input_velocity_x = scale_axis_f32(
        entry_main_stick_x,
        fighter->jump_horizontal_input_speed_f32);
    const float requested_velocity_x =
        carried_velocity_x + input_velocity_x;

    if (requested_velocity_x <
        -fighter->jump_horizontal_max_speed_f32)
    {
        *velocity_x = -fighter->jump_horizontal_max_speed_f32;
    }
    else if (requested_velocity_x >
             fighter->jump_horizontal_max_speed_f32)
    {
        *velocity_x = fighter->jump_horizontal_max_speed_f32;
    }
    else
    {
        *velocity_x = requested_velocity_x;
    }
    *velocity_y =
        -(short_hop_latched == UINT8_C(1)
              ? fighter->short_hop_speed_f32
              : fighter->full_hop_speed_f32);
    *grounded = UINT8_C(0);
    *support = (uint8_t)PF_M4_SURFACE_NONE;
    *action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
    *action_ticks = UINT16_C(0);
    *source_submotion =
        falcon_jump_submotion_from_x(
            entry_main_stick_x,
            facing,
            0);
    *short_hop_latched_out = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        /* ftCo_Jump_Enter resets x671 but not UCF's x674 timer. */
        *tilt_y_age = UINT8_C(254);
    }
}

static void enter_platform_pass(
    const fighter_data *fighter,
    float *position_y,
    float *velocity_y,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *platform_drop_ticks,
    uint8_t *fast_fall)
{
    *grounded = UINT8_C(0);
    *support = (uint8_t)PF_M4_SURFACE_NONE;
    *action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
    *action_ticks = UINT16_C(0);
    *source_submotion =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? (uint16_t)PF_M4_FALCON_SUBMOTION_PLATFORM_DROP
            : (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    *platform_drop_ticks = (uint8_t)fighter->platform_drop_ticks;
    if (fighter->reference_frame_data_enabled == UINT8_C(0))
    {
        *position_y += fighter->platform_drop_nudge_f32;
    }
    *velocity_y = fighter->platform_drop_speed_y_f32;
    *fast_fall = UINT8_C(0);
}

static int action_can_start_vector_ascent(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
           action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
           action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_END ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
           action_is_damage(action_state);
}

typedef struct reference_callback_owner
{
    uint8_t action_state;
    uint16_t action_ticks;
    uint16_t source_submotion;
    float source_animation_frame_f32;
    float source_animation_rate_f32;
    uint8_t entered_this_tick;
} reference_callback_owner;

static reference_callback_owner
reference_project_callback_owner(
    const fighter_data *fighter,
    const ssbm_match_entry_attributes *match_entry,
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t source_submotion,
    float source_animation_frame_f32,
    float source_animation_rate_f32,
    uint16_t hitlag_ticks,
    uint16_t hitstun_ticks_value,
    uint16_t shield_stun_ticks,
    int shield_held,
    int16_t main_stick_x,
    int8_t run_target_direction)
{
    reference_callback_owner owner = {
        action_state,
        action_ticks,
        source_submotion,
        source_animation_frame_f32,
        source_animation_rate_f32,
        UINT8_C(0),
    };
    falcon_move_index move_index;
    const struct reference_move *move;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) ||
        hitlag_ticks != UINT16_C(0) ||
        (hitstun_ticks_value != UINT16_C(0) &&
         action_state != (uint8_t)PF_M4_ACTION_TECH_IN_PLACE &&
         action_state != (uint8_t)PF_M4_ACTION_TECH_ROLL))
    {
        return owner;
    }

    /* Fighter animation callbacks run before IASA. When Squat or SquatRv
     * reaches its terminal frame, ChangeMotionState installs SquatWait or
     * Wait immediately, so that successor owns the input callback later in
     * the same fighter update. Keep this projection stack-local: it changes
     * callback ownership without adding rollback state or another timer. */
    if (action_state == (uint8_t)PF_M4_ACTION_MATCH_ENTRY_END &&
        match_entry != NULL &&
        (uint32_t)action_ticks + UINT32_C(1) >=
            (uint32_t)match_entry->descent_ticks)
    {
        /* EntryEnd_Anim installs Fall before priority-3 input and ordinary
         * air physics. The terminal update therefore already owns Fall's
         * callbacks and gravity; there is no intermediate EntryEnd row. */
        owner.action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_CROUCH_START &&
        action_ticks >= fighter->crouch_start_ticks)
    {
        owner.action_state = (uint8_t)PF_M4_ACTION_CROUCH;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
             (uint32_t)action_ticks + UINT32_C(1) >=
                 (uint32_t)fighter->jump_squat_ticks)
    {
        /* KneeBend_Anim installs JumpF/B before IASA. Jump's aerial option
         * table therefore owns the takeoff update, including EscapeAir for
         * a frame-perfect wavedash. */
        owner.action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_CROUCH_END &&
             action_ticks >= fighter->crouch_end_ticks)
    {
        owner.action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_PUMMEL &&
             action_ticks >= fighter->pummel_total_ticks)
    {
        /* CatchAttack_Anim installs CatchWait before priority-3 IASA on the
         * terminal update. CatchWait can therefore consume a pummel or throw
         * input without exposing an intermediate CatchWait row. */
        owner.action_state = (uint8_t)PF_M4_ACTION_GRAB_HOLD;
        owner.action_ticks = UINT16_C(0);
        owner.source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_WAIT;
        owner.source_animation_frame_f32 = INT32_C(0);
        owner.source_animation_rate_f32 = 1.0f;
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_is_throw(action_state))
    {
        const falcon_submotion_data *throw_motion;
        uint16_t holder_submotion;
        uint16_t victim_submotion;
        float expected_rate_f32;

        if (falcon_reference_throw_motions(
                action_state,
                &holder_submotion,
                &victim_submotion,
                &expected_rate_f32) &&
            source_submotion == holder_submotion &&
            source_animation_rate_f32 == expected_rate_f32 &&
            source_animation_rate_f32 > INT32_C(0) &&
            (throw_motion =
                 falcon_reference_submotion(source_submotion)) != NULL &&
            throw_motion->animation_frame_count != UINT16_C(0) &&
            source_animation_frame_f32 + source_animation_rate_f32 >=
                (float)throw_motion->animation_frame_count)
        {
            /* Throw_Anim advances the weight-scaled fractional animation
             * clock before testing ftAnim_IsFramesRemaining. On the update
             * that crosses the raw FigaTree endpoint it installs Wait before
             * priority-3 input, so Wait can acquire Dash without an idle row.
             * The victim submotion is checked by the ordinary throw path. */
            (void)victim_submotion;
            owner.action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            owner.action_ticks = UINT16_C(0);
            owner.entered_this_tick = UINT8_C(1);
        }
    }
    else if ((action_state == (uint8_t)PF_M4_ACTION_LANDING &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->landing_ticks) ||
             (action_is_aerial_landing(action_state) &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)aerial_landing_ticks(
                      fighter,
                      action_state)) ||
             (action_state == (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->special_landing_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->run_brake_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->standing_turn_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->initial_dash_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_TAUNT &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->taunt_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->shield_release_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_TECH_IN_PLACE &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->tech_in_place_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL &&
              (uint32_t)action_ticks + UINT32_C(1) >=
                  (uint32_t)fighter->tech_roll_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD &&
              action_ticks >= fighter->forward_roll_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD &&
              action_ticks >= fighter->backward_roll_ticks) ||
             (action_state == (uint8_t)PF_M4_ACTION_SPOT_DODGE &&
              action_ticks >= fighter->spot_dodge_ticks))
    {
        owner.action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_is_light_aerial(action_state) &&
             (uint32_t)action_ticks + UINT32_C(1) >=
                 light_aerial_ticks(fighter, action_state))
    {
        owner.action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state ==
                 (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
             (uint32_t)action_ticks + UINT32_C(1) >=
                 (uint32_t)fighter->strong_startup_ticks +
                     (uint32_t)fighter->strong_active_ticks +
                     (uint32_t)fighter->strong_recovery_ticks)
    {
        /* AttackAir_Anim installs Fall before AttackAir IASA on the
         * terminal frame. Strong aerials use the shared AttackAir callback
         * in Melee, so they must expose Fall's EscapeAir/SpecialAir table on
         * this update just like the imported light-aerial motions above. */
        owner.action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (((action_is_ground_attack(action_state) &&
               !action_is_smash_charge(action_state) &&
               action_state != (uint8_t)PF_M4_ACTION_RAPID_JAB_START &&
               action_state != (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP) ||
              action_state == (uint8_t)PF_M4_ACTION_GRAB ||
              action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB) &&
             falcon_reference_move_for_action(
                 action_state,
                 &move_index) &&
             (move = falcon_reference_move(move_index)) != NULL &&
             (uint32_t)action_ticks >=
                 (uint32_t)move->total_frames + UINT32_C(1))
    {
        /* AttackLw3_Anim enters SquatWait; the other represented Falcon
         * ground attacks and both Catch variants enter Wait. The newly
         * installed callback owns this update, exactly like the common
         * locomotion transitions above. */
        owner.action_state =
            action_state == (uint8_t)PF_M4_ACTION_DOWN_ATTACK
                ? (uint8_t)PF_M4_ACTION_CROUCH
                : (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
             shield_stun_ticks <= UINT16_C(1))
    {
        owner.action_state =
            shield_held != 0
                ? (uint8_t)PF_M4_ACTION_SHIELD
                : (uint8_t)PF_M4_ACTION_SHIELD_RELEASE;
        owner.action_ticks =
            shield_held != 0
                ? fighter->shield_minimum_hold_ticks
                : UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
             action_ticks >= fighter->run_turnaround_ticks)
    {
        const int target_held =
            axis_direction(
                main_stick_x,
                fighter->axis_dead_zone) == run_target_direction &&
            axis_magnitude(main_stick_x) >=
                fighter->run_continue_axis_threshold;

        owner.action_state =
            target_held != 0
                ? (uint8_t)PF_M4_ACTION_RUN
                : (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        owner.action_ticks = UINT16_C(0);
        owner.entered_this_tick = UINT8_C(1);
    }

    return owner;
}

enum
{
    PF_M4_REFERENCE_SPECIAL_SIDE = 1U << 0U,
    PF_M4_REFERENCE_SPECIAL_UP = 1U << 1U,
    PF_M4_REFERENCE_SPECIAL_NEUTRAL = 1U << 2U,
    PF_M4_REFERENCE_SPECIAL_DOWN = 1U << 3U,
    PF_M4_REFERENCE_SPECIAL_ALL =
        PF_M4_REFERENCE_SPECIAL_SIDE |
        PF_M4_REFERENCE_SPECIAL_UP |
        PF_M4_REFERENCE_SPECIAL_NEUTRAL |
        PF_M4_REFERENCE_SPECIAL_DOWN
};

static uint8_t reference_action_special_capabilities(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    int normal_landing_interruptible,
    int powershield_release_cancel_ready,
    uint8_t ground_iasa_capabilities,
    uint16_t initial_dash_special_end_frame)
{
    if (grounded == UINT8_C(0))
    {
        /* Fall/Jump IASA routes SpecialAir first. DamageFall also exposes
         * SpecialAir once damage lockout has ended; EscapeAir, passive-wall
         * states, and FallSpecial use narrower callback tables. */
        return (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
                action_is_damage(action_state))
                   ? (uint8_t)PF_M4_REFERENCE_SPECIAL_ALL
                   : UINT8_C(0);
    }
    if (action_is_ground_attack(action_state))
    {
        return (ground_iasa_capabilities &
                PF_M4_FALCON_IASA_SPECIAL) != UINT8_C(0)
                   ? (uint8_t)PF_M4_REFERENCE_SPECIAL_ALL
                   : UINT8_C(0);
    }
    /* KneeBend calls only SpecialHi before its grab/up-smash callbacks.
     * SquatWait and SquatRv call SpecialLw, then SpecialHi. Turn calls
     * SpecialS, SpecialLw, then SpecialHi, deliberately omitting SpecialN.
     * Ottotto and OttottoWait share the full common dispatcher. Powershield GuardOff
     * exposes the full table while its x1C cancel window remains live.
     * Ordinary GuardOff, Guard, RunBrake, TurnRun, and recovery states have
     * no special dispatcher. Dash exposes only SpecialS through its imported
     * x4C frame boundary. Every remaining listed state owns the full common
     * table. */
    if (action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
        action_state == (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        return (uint8_t)(PF_M4_REFERENCE_SPECIAL_UP |
                         PF_M4_REFERENCE_SPECIAL_DOWN);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        return (uint8_t)PF_M4_REFERENCE_SPECIAL_UP;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        return (uint8_t)(PF_M4_REFERENCE_SPECIAL_SIDE |
                         PF_M4_REFERENCE_SPECIAL_UP |
                         PF_M4_REFERENCE_SPECIAL_DOWN);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        return action_ticks <= initial_dash_special_end_frame
                   ? (uint8_t)PF_M4_REFERENCE_SPECIAL_SIDE
                   : UINT8_C(0);
    }
    return (action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
            action_state == (uint8_t)PF_M4_ACTION_WALK ||
            action_state == (uint8_t)PF_M4_ACTION_RUN ||
            action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
            action_state == (uint8_t)PF_M4_ACTION_TEETER ||
            action_is_damage(action_state) ||
            (action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
             powershield_release_cancel_ready != 0) ||
            (action_state == (uint8_t)PF_M4_ACTION_LANDING &&
             normal_landing_interruptible != 0))
               ? (uint8_t)PF_M4_REFERENCE_SPECIAL_ALL
               : UINT8_C(0);
}

typedef enum prone_option
{
    PF_M4_PRONE_OPTION_NONE = 0,
    PF_M4_PRONE_OPTION_ATTACK = 1,
    PF_M4_PRONE_OPTION_ROLL = 2,
    PF_M4_PRONE_OPTION_NEUTRAL = 3
} prone_option;

static int axis_is_in_prone_horizontal_wedge(
    int16_t axis_x,
    int16_t axis_y,
    uint16_t threshold,
    float angle_tan_f32)
{
    const uint32_t magnitude_x = axis_magnitude(axis_x);
    const uint32_t magnitude_y = axis_magnitude(axis_y);

    return magnitude_x >= threshold &&
           (float)magnitude_y < (float)magnitude_x * angle_tan_f32;
}

static prone_option select_prone_option(
    const fighter_data *fighter,
    const pf_input_frame *input,
    uint8_t previous_directional_input_flags,
    uint8_t prone_attack_input_age,
    int shield_pressed,
    int8_t *out_roll_direction)
{
    const int previous_c_up =
        (previous_directional_input_flags &
         PF_M4_DIRECTIONAL_INPUT_C_UP) != UINT8_C(0);
    const int8_t previous_c_roll_direction =
        (previous_directional_input_flags &
         PF_M4_DIRECTIONAL_INPUT_C_LEFT) != UINT8_C(0)
            ? INT8_C(-1)
            : ((previous_directional_input_flags &
                PF_M4_DIRECTIONAL_INPUT_C_RIGHT) != UINT8_C(0)
                   ? INT8_C(1)
                   : INT8_C(0));
    /* A one-tick C-stick pulse on the terminal DownBound source frame is
     * visible through the prior controller sample when the response callback
     * runs. Accepting either threshold transition preserves that edge without
     * adding a second buffered-input channel; an entry edge in DownWait is
     * still consumed immediately, so its later release cannot select again. */
    const int c_up_pressed =
        (previous_c_up == 0 &&
         input->secondary_stick_y <=
             -(int16_t)fighter->down_c_up_axis_threshold) ||
        (previous_c_up != 0 &&
         input->secondary_stick_y >
             -(int16_t)fighter->down_c_up_axis_threshold);
    const int current_c_roll =
        axis_is_in_prone_horizontal_wedge(
            input->secondary_stick_x,
            input->secondary_stick_y,
            fighter->down_horizontal_axis_threshold,
            fighter->down_horizontal_angle_tan_f32);
    const int c_roll_pressed =
        (previous_c_roll_direction == INT8_C(0) && current_c_roll != 0) ||
        (previous_c_roll_direction != INT8_C(0) && current_c_roll == 0);
    const int main_roll_held =
        axis_is_in_prone_horizontal_wedge(
            input->main_stick_x,
            input->main_stick_y,
            fighter->down_horizontal_axis_threshold,
            fighter->down_horizontal_angle_tan_f32);
    const uint32_t main_up_magnitude =
        input->main_stick_y < INT16_C(0)
            ? (uint32_t)(-(int32_t)input->main_stick_y)
            : UINT32_C(0);
    const int main_up_held =
        main_up_magnitude >= fighter->down_up_axis_threshold &&
        (float)main_up_magnitude >=
            (float)axis_magnitude(input->main_stick_x) *
                fighter->down_horizontal_angle_tan_f32;

    *out_roll_direction = INT8_C(0);
    if (prone_attack_input_age <
            fighter->down_attack_input_window_ticks ||
        c_up_pressed != 0)
    {
        return PF_M4_PRONE_OPTION_ATTACK;
    }
    if (c_roll_pressed != 0 || main_roll_held != 0)
    {
        *out_roll_direction = c_roll_pressed != 0
                                  ? (current_c_roll != 0
                                         ? (input->secondary_stick_x <
                                                    INT16_C(0)
                                                ? INT8_C(-1)
                                                : INT8_C(1))
                                         : previous_c_roll_direction)
                                  : (input->main_stick_x < INT16_C(0)
                                         ? INT8_C(-1)
                                         : INT8_C(1));
        return PF_M4_PRONE_OPTION_ROLL;
    }
    if (main_up_held != 0 || shield_pressed != 0)
    {
        return PF_M4_PRONE_OPTION_NEUTRAL;
    }
    return PF_M4_PRONE_OPTION_NONE;
}

static pf_status enter_prone_option(
    prone_option option,
    int8_t roll_direction,
    uint8_t prone_orientation,
    int from_knockdown,
    int8_t facing,
    float *velocity_x,
    uint8_t *action_state,
    uint16_t *action_ticks,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    *action_ticks = UINT16_C(0);
    *velocity_x = INT32_C(0);
    scratch->tech_direction[player_index] = INT8_C(0);
    scratch->prone_roll_motion_orientation[player_index] =
        (uint8_t)PF_M4_PRONE_NONE;
    if (option == PF_M4_PRONE_OPTION_ATTACK)
    {
        *action_state = (uint8_t)PF_M4_ACTION_GETUP_ATTACK;
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        return PF_STATUS_OK;
    }
    if (option == PF_M4_PRONE_OPTION_ROLL)
    {
        /* ftCo_Down_CheckInput keys the U/D roll motion from DownWaitU only.
         * A roll selected directly by DownBound_Anim therefore takes the D
         * motion even when the current bound motion is U. */
        const uint8_t motion_orientation =
            from_knockdown != 0
                ? (uint8_t)PF_M4_PRONE_STOMACH
                : prone_orientation;
        const uint16_t submotion_index =
            getup_roll_submotion_for(
                motion_orientation,
                roll_direction,
                facing);
        float translation_x_f32;

        if (submotion_index == UINT16_MAX ||
            !falcon_reference_translation_f32(
                submotion_index,
                UINT16_C(1),
                &translation_x_f32,
                NULL))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *action_state = (uint8_t)PF_M4_ACTION_GETUP_ROLL;
        *velocity_x = (float)facing * translation_x_f32;
        scratch->tech_direction[player_index] = roll_direction;
        scratch->prone_roll_motion_orientation[player_index] =
            motion_orientation;
        return PF_STATUS_OK;
    }
    if (option == PF_M4_PRONE_OPTION_NEUTRAL)
    {
        *action_state = (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL;
        return PF_STATUS_OK;
    }
    return PF_STATUS_INVALID_ARGUMENT;
}

pf_status step_player(
    const struct content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    const pf_input_frame *raw_input,
    uint32_t player_index,
    float player_nudge_x_f32_value,
    uint64_t *rng_state)
{
    const fighter_data *fighter = &content->fighter;
    const stage_data *stage = &content->stage;
    const falcon_common_attributes *source_character =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? falcon_reference_common_attributes()
            : NULL;
    const ssbm_match_entry_attributes *match_entry =
        source_character != NULL
            ? ssbm_common_reference_match_entry()
            : NULL;
    const reference_callback_owner callback_owner =
        reference_project_callback_owner(
            fighter,
            match_entry,
            world->action_state[player_index],
            world->action_ticks[player_index],
            world->source_submotion[player_index],
            world->source_animation_frame_f32[player_index],
            world->source_animation_rate_f32[player_index],
            world->hitlag_ticks[player_index],
            world->hitstun_ticks[player_index],
            world->shield_stun_ticks[player_index],
            (input_trigger_state(fighter, input) &
             PF_M4_TRIGGER_STATE_HELD_MASK) != UINT8_C(0),
            input->main_stick_x,
            world->dash_direction[player_index]);
    const uint8_t previous_action_state =
        world->action_state[player_index];
    const uint8_t previous_hitlag_resume_action =
        world->hitlag_resume_action[player_index];
    const uint16_t previous_source_submotion =
        world->source_submotion[player_index];
    const float previous_source_animation_frame_f32 =
        world->source_animation_frame_f32[player_index];
    const float previous_source_animation_rate_f32 =
        world->source_animation_rate_f32[player_index];
    const float previous_fall_animation_blend_f32 =
        world->fall_animation_blend_f32[player_index];
    const uint8_t previous_fall_animation_target_switched =
        world->fall_animation_target_switched[player_index];
    const float previous_ground_velocity_f32 =
        world->velocity_x_f32[player_index];
    const int8_t previous_facing = world->facing[player_index];

    if (rng_state == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (!ssbm_stage_support_valid(
            stage->reference_collision_profile,
            world->support[player_index],
            world->grounded[player_index]))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    const uint64_t previous_buttons =
        world->previous_buttons[player_index];
    const int raw_attack_pressed =
        (raw_input->buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_ATTACK) == UINT64_C(0);
    const int raw_special_pressed =
        (raw_input->buttons & PF_INPUT_BUTTON_SPECIAL) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_SPECIAL) == UINT64_C(0);
    const pf_input_raw_pad ucf_raw = pf_input_get_raw_pad(raw_input);
    const int ucf_raw_delta_x =
        (int)ucf_raw.main_stick_x -
        (int)world->raw_main_t2_x[player_index];
    const int ucf_raw_delta_y =
        (int)ucf_raw.main_stick_y -
        (int)world->raw_main_t2_y[player_index];
    const int ucf084_enabled =
        content->gameplay_ruleset ==
        (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084;
    const int8_t input_tilt_x_direction =
        scratch->previous_tilt_x_direction[player_index];
    const int8_t input_tilt_y_direction =
        scratch->previous_tilt_y_direction[player_index];
    const uint8_t input_tilt_x_age = scratch->tilt_x_age[player_index];
    const uint8_t input_tilt_y_age = scratch->tilt_y_age[player_index];
    const int button_jump_pressed =
        (input->buttons & PF_INPUT_BUTTON_JUMP) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_JUMP) == UINT64_C(0);
    const int tap_jump_pressed =
        input->main_stick_y <=
            -(int16_t)fighter->tap_jump_axis_threshold &&
        input_tilt_y_age < fighter->tap_jump_input_window_ticks;
    const int jump_pressed =
        button_jump_pressed != 0 || tap_jump_pressed != 0;
    const int main_jump_up_held =
        input->main_stick_y <=
        -(int16_t)fighter->tap_jump_axis_threshold;
    const int light_attack_pressed =
        (input->buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_ATTACK) == UINT64_C(0);
    const int light_attack_held =
        (input->buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0);
    const int light_attack_released =
        (input->buttons & PF_INPUT_BUTTON_ATTACK) == UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0);
    const uint8_t reference_c_stick_attack_action_value =
        reference_c_stick_attack_action(
            fighter,
            input,
            world->previous_secondary_stick_x[player_index],
            world->previous_secondary_stick_y[player_index],
            world->grounded[player_index] != UINT8_C(0));
    const int strong_attack_pressed =
        ((input->buttons & PF_INPUT_BUTTON_STRONG_ATTACK) !=
             UINT64_C(0) &&
         (previous_buttons & PF_INPUT_BUTTON_STRONG_ATTACK) ==
             UINT64_C(0)) ||
        reference_c_stick_attack_action_value != UINT8_MAX;
    const int special_pressed =
        (input->buttons & PF_INPUT_BUTTON_SPECIAL) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_SPECIAL) == UINT64_C(0);
    const int taunt_pressed =
        (input->buttons & PF_INPUT_BUTTON_TAUNT) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_TAUNT) == UINT64_C(0);
    const uint16_t input_shield_strength_value =
        input_shield_strength(fighter, input);
    const uint8_t input_trigger_state_value =
        input_trigger_state(fighter, input);
    const uint8_t previous_trigger_state =
        world->shield_held[player_index];
    const int shield_held =
        (input_trigger_state_value & PF_M4_TRIGGER_STATE_HELD_MASK) !=
        UINT8_C(0);
    const int dense_shield_pressed =
        (input_trigger_state_value & PF_M4_TRIGGER_STATE_DENSE_MASK &
         (uint8_t)~previous_trigger_state) != UINT8_C(0);
    const int shield_pressed =
        ((input_trigger_state_value & PF_M4_TRIGGER_STATE_HELD_MASK &
          (uint8_t)~previous_trigger_state) != UINT8_C(0)) ||
        dense_shield_pressed != 0;
    const int grab_pressed =
        shield_held != 0 && light_attack_pressed != 0;
    const int powershield_release_cancel_ready =
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        scratch->powershield[player_index] != UINT8_C(0) &&
        fighter->powershield_cancel_enabled != UINT8_C(0) &&
        callback_owner.action_ticks >=
            fighter->powershield_cancel_delay_ticks;
    const int grab_blocks_attack =
        grab_pressed != 0 &&
        (action_can_start_grab(
             callback_owner.action_state) ||
         powershield_release_cancel_ready != 0);
    const int grab_fallback_attack_pressed =
        grab_pressed != 0 && grab_blocks_attack == 0;
    const int boost_grab_pressed =
        world->grounded[player_index] != UINT8_C(0) &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_DASH_ATTACK &&
        callback_owner.action_ticks >=
            fighter->boost_grab_cancel_begin_tick &&
        callback_owner.action_ticks <=
            fighter->boost_grab_cancel_end_tick &&
        shield_held != 0 &&
        (light_attack_pressed != 0 ||
         (light_attack_held != 0 && shield_pressed != 0));
    const int jab_combo_window = source_character == NULL &&
        world->grounded[player_index] != UINT8_C(0) &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
        callback_owner.action_ticks >=
            fighter->jab_combo_input_begin_tick &&
        callback_owner.action_ticks <=
            fighter->jab_combo_input_end_tick;
    const int jab_cancel_pressed =
        jab_combo_window != 0 && shield_pressed != 0;
    const int jab_final_pressed =
        jab_combo_window != 0 && shield_held == 0 &&
        light_attack_pressed != 0;
    const int was_shielding =
        callback_owner.entered_this_tick == UINT8_C(0) &&
        (callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_SHIELD ||
         callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_SHIELD_STUN);
    const uint16_t horizontal_magnitude =
        axis_magnitude(input->main_stick_x);
    const uint16_t vertical_magnitude =
        axis_magnitude(input->main_stick_y);
    const int8_t horizontal_direction =
        axis_direction(
            input->main_stick_x,
            fighter->axis_dead_zone);
    const int8_t ground_horizontal_direction =
        axis_direction(
            input->main_stick_x,
            fighter->walk_axis_threshold);
    const int8_t strong_direction_value =
        strong_direction(
            input->main_stick_x,
            fighter->dash_axis_threshold);
    const uint16_t secondary_horizontal_magnitude =
        axis_magnitude(input->secondary_stick_x);
    const uint16_t secondary_vertical_magnitude =
        axis_magnitude(input->secondary_stick_y);
    const ssbm_ground_input_attributes *source_ground_input =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? ssbm_common_reference_ground_input()
            : NULL;
    const uint16_t special_vertical_axis_threshold =
        source_ground_input != NULL
            ? source_ground_input->special_vertical_axis_threshold
            : fighter->dash_axis_threshold;
    const int raw_up_special_input_pressed =
        raw_special_pressed != 0 &&
        input->main_stick_y <=
            -(int16_t)special_vertical_axis_threshold;
    const int up_special_repress_allowed =
        source_ground_input == NULL ||
        raw_up_special_input_pressed == 0 ||
        world->up_special_input_age[player_index] >=
            source_ground_input->up_special_repress_interval_ticks;
    const uint16_t escape_axis_threshold =
        source_ground_input != NULL
            ? source_ground_input->escape_axis_threshold
            : fighter->dash_axis_threshold;
    const uint16_t escape_tilt_window_ticks =
        source_ground_input != NULL
            ? source_ground_input->escape_tilt_window_ticks
            : UINT16_C(1);
    const uint16_t initial_dash_early_end_frame =
        source_ground_input != NULL
            ? source_ground_input->initial_dash_early_end_frame
            : UINT16_C(0);
    const uint16_t initial_dash_forward_roll_end_frame =
        source_ground_input != NULL
            ? source_ground_input->initial_dash_forward_roll_end_frame
            : UINT16_C(0);
    const uint16_t initial_dash_special_end_frame =
        source_ground_input != NULL
            ? source_ground_input->initial_dash_special_end_frame
            : UINT16_C(0);
    /* ftCo_Dash_IASA observes the animation frame being processed now,
     * whereas action_ticks is the last completed frame exposed by the sim.
     * Keep that clock conversion explicit so the imported x44/x4C branch
     * boundaries retain their source meaning. */
    const uint32_t reference_current_anim_frame =
        (uint32_t)callback_owner.action_ticks + UINT32_C(1);
    const int8_t reference_initial_dash_direction =
        signed_phase_direction(
            world->dash_direction[player_index]);
    const int reference_initial_dash_turn_origin =
        source_ground_input != NULL &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        (world->dash_direction[player_index] ==
             (int8_t)(reference_initial_dash_direction *
                      PF_M4_INITIAL_DASH_TURN_PHASE));
    const int reference_initial_dash_ordinary_origin =
        source_ground_input != NULL &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        reference_initial_dash_turn_origin == 0;
    const int secondary_stick_active =
        secondary_horizontal_magnitude >= fighter->axis_dead_zone ||
        secondary_vertical_magnitude >= fighter->axis_dead_zone;
    const int16_t strong_attack_stick_x =
        secondary_stick_active != 0
            ? input->secondary_stick_x
            : input->main_stick_x;
    const int16_t strong_attack_stick_y =
        secondary_stick_active != 0
            ? input->secondary_stick_y
            : input->main_stick_y;
    const int8_t strong_attack_horizontal_direction =
        axis_direction(
            strong_attack_stick_x,
            fighter->axis_dead_zone);
    const uint16_t forward_smash_axis_threshold =
        source_ground_input != NULL
            ? source_ground_input->c_stick_horizontal_smash_threshold
            : fighter->dash_axis_threshold;
    const uint16_t forward_smash_window_ticks =
        source_ground_input != NULL
            ? source_ground_input->forward_smash_input_window_ticks
            : fighter->forward_smash_input_window_ticks;
    const int8_t forward_smash_direction =
        strong_direction(
            input->main_stick_x,
            forward_smash_axis_threshold);
    const int8_t reference_turn_callback_facing_value =
        reference_turn_callback_facing(
            source_ground_input,
            callback_owner.action_state,
            world->facing[player_index],
            world->dash_direction[player_index]);
    const int reference_full_ground_attack_callbacks =
        source_ground_input != NULL &&
        (callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_STANDING_TURN ||
         callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_CROUCH_START ||
         callback_owner.action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
         callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_CROUCH_END);
    const int forward_smash_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        forward_smash_direction != INT8_C(0) &&
        (((callback_owner.action_state ==
                   (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           callback_owner.action_state ==
                   (uint8_t)PF_M4_ACTION_WALK ||
           reference_full_ground_attack_callbacks != 0) &&
          input_tilt_x_age < forward_smash_window_ticks) ||
         (callback_owner.action_state ==
              (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          forward_smash_direction == world->facing[player_index]));
    const int vertical_smash_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        (callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
         callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_WALK ||
         reference_full_ground_attack_callbacks != 0) &&
        vertical_magnitude >=
            (source_ground_input != NULL
                 ? source_ground_input->vertical_smash_axis_threshold
                 : fighter->dash_axis_threshold) &&
        input_tilt_y_age <
            (source_ground_input != NULL
                 ? source_ground_input->vertical_smash_input_window_ticks
                 : fighter->forward_smash_input_window_ticks);
    const int ground_smash_charge_pressed =
        forward_smash_pressed != 0 || vertical_smash_pressed != 0;
    const uint8_t forward_smash_release_action =
        select_ground_strong_attack_action(
            fighter,
            source_ground_input,
            input->main_stick_x,
            input->main_stick_y);
    const uint8_t ground_smash_charge_action =
        forward_smash_pressed != 0
            ? smash_charge_action_for_release(
                  forward_smash_release_action)
            : (input->main_stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_STRONG_CHARGE
                   : (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE);
    const int ground_strong_attack_pressed =
        grab_blocks_attack == 0 && strong_attack_pressed != 0;
    const uint8_t ground_light_attack_action =
        select_ground_light_attack_action(
            fighter,
            source_ground_input,
            reference_turn_callback_facing_value,
            input->main_stick_x,
            input->main_stick_y);
    const uint8_t ground_strong_attack_action =
        select_ground_strong_input_action(
            fighter,
            source_ground_input,
            reference_c_stick_attack_action_value,
            strong_attack_stick_x,
            strong_attack_stick_y);
    const int dash_attack_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        ground_smash_charge_pressed == 0 &&
        action_can_start_dash_attack(
            fighter,
            callback_owner.action_state,
            callback_owner.action_ticks);
    const int reference_initial_dash_forward_smash =
        reference_initial_dash_ordinary_origin != 0 &&
        reference_current_anim_frame <=
            initial_dash_early_end_frame &&
        (forward_smash_pressed != 0 ||
         reference_c_stick_attack_action_value ==
             (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK);
    const int reference_initial_dash_dash_attack =
        source_ground_input != NULL &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        (reference_initial_dash_turn_origin != 0 ||
         reference_current_anim_frame >
             initial_dash_early_end_frame) &&
        reference_current_anim_frame <=
            initial_dash_special_end_frame &&
        grab_blocks_attack == 0 && light_attack_pressed != 0;
    const int reference_initial_dash_attack_allowed =
        source_ground_input == NULL ||
        callback_owner.action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        reference_initial_dash_forward_smash != 0 ||
        reference_initial_dash_dash_attack != 0;
    const int attack_pressed =
        grab_blocks_attack == 0 &&
        (light_attack_pressed != 0 || strong_attack_pressed != 0);
    const int jump_cancel_attack_pressed =
        world->grounded[player_index] != UINT8_C(0) &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        attack_pressed != 0 &&
        input->main_stick_y <=
            -(int16_t)fighter->dash_axis_threshold;
    const int dodge_down_held =
        input->main_stick_y >=
        (int16_t)fighter->crouch_axis_threshold;
    const int secondary_jump_up_buffered =
        input->secondary_stick_y <=
        -(int16_t)fighter->dash_axis_threshold;
    const int secondary_jump_up_held =
        input->secondary_stick_y <=
        -(int16_t)fighter->crouch_axis_threshold;
    const int ucf_shield_drop_spot_dodge_suppressed =
        ucf084_enabled != 0 &&
        source_ground_input != NULL &&
        input->secondary_stick_y <
            (int16_t)PF_M4_UCF084_SHIELD_DROP_C_UPPER_AXIS &&
        input_tilt_x_age >= source_ground_input->escape_tilt_window_ticks &&
        input->main_stick_y <
            (int16_t)PF_M4_UCF084_SHIELD_DROP_MAIN_UPPER_AXIS &&
        surface_is_pass_through(
            content,
            world->support[player_index]) != 0 &&
        ucf084_adjusted_radial_qualifies(
            input->main_stick_x,
            input->main_stick_y);
    const int main_stick_spot_dodge_pressed =
        shield_held != 0 &&
        input->main_stick_y >= (int16_t)escape_axis_threshold &&
        input_tilt_y_age < escape_tilt_window_ticks &&
        ucf_shield_drop_spot_dodge_suppressed == 0;
    const int secondary_stick_spot_dodge_buffered =
        shield_held != 0 &&
        input->secondary_stick_y >= (int16_t)escape_axis_threshold &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD;
    const int spot_dodge_pressed =
        main_stick_spot_dodge_pressed != 0 ||
        secondary_stick_spot_dodge_buffered != 0;
    const int main_stick_roll_pressed =
        shield_held != 0 &&
        horizontal_magnitude >= escape_axis_threshold &&
        input_tilt_x_age < escape_tilt_window_ticks;
    const int secondary_stick_roll_buffered =
        shield_held != 0 &&
        secondary_horizontal_magnitude >= escape_axis_threshold &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD;
    const int roll_pressed =
        main_stick_roll_pressed != 0 ||
        secondary_stick_roll_buffered != 0;
    const int8_t roll_direction =
        main_stick_roll_pressed != 0
            ? axis_direction(
                  input->main_stick_x,
                  escape_axis_threshold)
            : axis_direction(
                  input->secondary_stick_x,
                  escape_axis_threshold);
    const int shield_jump_pressed =
        jump_pressed != 0 ||
        (shield_held != 0 &&
         secondary_jump_up_buffered != 0 &&
         callback_owner.action_state ==
             (uint8_t)PF_M4_ACTION_SHIELD);
    const int shield_release_spot_dodge_pressed =
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        ((input->main_stick_y >= (int16_t)escape_axis_threshold &&
          input_tilt_y_age < escape_tilt_window_ticks) ||
         input->secondary_stick_y >=
             (int16_t)escape_axis_threshold) &&
        ucf_shield_drop_spot_dodge_suppressed == 0;
    const int shield_release_jump_pressed =
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        (jump_pressed != 0 || secondary_jump_up_buffered != 0);
    const int shield_platform_drop_requested =
        shield_held != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        surface_is_pass_through(
            content,
            world->support[player_index]) != 0 &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD &&
        (source_ground_input != NULL
             ? (input_tilt_y_age <
                    source_ground_input->platform_drop_tilt_window_ticks &&
                (input->main_stick_y >=
                     (int16_t)source_ground_input
                         ->platform_drop_axis_threshold ||
                 (ucf084_enabled != 0 &&
                  scratch->ucf_pad_buffer_count[player_index] >
                      UINT8_C(1))))
             : input->main_stick_y >=
                   (int16_t)fighter->shield_drop_axis_threshold);
    const struct reference_move *ground_reference_attack =
        world->grounded[player_index] != UINT8_C(0) &&
        action_is_ground_attack(
            callback_owner.action_state)
            ? falcon_ground_reference_attack(
                  fighter,
                  callback_owner.action_state)
            : NULL;
    const reference_iasa_policy ground_iasa_policy =
        ground_reference_attack != NULL
            ? falcon_reference_iasa_policy_for_action(
                  callback_owner.action_state)
            : PF_M4_REFERENCE_IASA_NONE;
    const int ground_attack_iasa =
        ground_reference_attack != NULL &&
        falcon_reference_iasa_active(
            callback_owner.action_state,
            (uint32_t)callback_owner.action_ticks + UINT32_C(1));
    const uint8_t ground_iasa_capabilities =
        ground_attack_iasa != 0
            ? falcon_ground_iasa_capabilities(
                  ground_iasa_policy)
            : UINT8_C(0);
    const int ground_common_iasa_input =
        jump_pressed != 0 ||
        horizontal_magnitude >= fighter->walk_axis_threshold ||
        input->main_stick_y >=
            (int16_t)fighter->crouch_axis_threshold;
    const int ground_common_iasa_unclaimed =
        attack_pressed == 0 && grab_pressed == 0 &&
        shield_held == 0 && special_pressed == 0 &&
        taunt_pressed == 0;
    const int reference_jab_chain_ready =
        source_character != NULL &&
        world->jab_chain_buffered[player_index] != UINT8_C(0) &&
        ((callback_owner.action_state ==
              (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
          (uint32_t)callback_owner.action_ticks + UINT32_C(1) >=
              source_character->jab_1_combo_enable_frame) ||
         (callback_owner.action_state ==
              (uint8_t)PF_M4_ACTION_JAB_FINAL &&
          (uint32_t)callback_owner.action_ticks + UINT32_C(1) >=
              source_character->jab_2_combo_enable_frame));
    const int reference_rapid_jab_ready =
        source_character != NULL &&
        callback_owner.action_state ==
            (uint8_t)PF_M4_ACTION_JAB_THIRD &&
        (uint32_t)callback_owner.action_ticks + UINT32_C(1) >=
            source_character->jab_3_rapid_enable_frame &&
        (uint16_t)world->rapid_jab_input_count[player_index] +
                (uint16_t)(light_attack_pressed != 0 ||
                           light_attack_released != 0) >=
            source_character->rapid_jab_input_count;
    float position_x = world->position_x_f32[player_index];
    float position_y = world->position_y_f32[player_index];
    float velocity_x = world->velocity_x_f32[player_index];
    float velocity_y = world->velocity_y_f32[player_index];
    uint16_t action_ticks = callback_owner.action_ticks;
    uint16_t source_submotion =
        callback_owner.source_submotion;
    float source_animation_frame_f32 =
        callback_owner.source_animation_frame_f32;
    float fall_animation_blend_f32 =
        world->fall_animation_blend_f32[player_index];
    uint8_t fall_animation_target_switched =
        world->fall_animation_target_switched[player_index];
    float source_animation_rate_f32 =
        callback_owner.source_animation_rate_f32;
    uint8_t ecb_bottom_lock_ticks =
        world->ecb_bottom_lock_ticks[player_index];
    float ecb_locked_bottom_y_f32 =
        world->ecb_locked_bottom_y_f32[player_index];
    const float previous_locked_bottom_y_f32 =
        ecb_bottom_lock_ticks != UINT8_C(0)
            ? ecb_locked_bottom_y_f32
            : PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32;
    float inherited_locked_bottom_y_f32 =
        ecb_bottom_lock_ticks > UINT8_C(1)
            ? ecb_locked_bottom_y_f32
            : PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32;
    int ecb_lock_entered_during_map = 0;

    scratch->ecb_bottom_lock_ticks[player_index] =
        ecb_bottom_lock_ticks;
    scratch->ecb_locked_bottom_y_f32[player_index] =
        ecb_locked_bottom_y_f32;
    uint16_t respawn_count = world->respawn_count[player_index];
    uint8_t grounded = world->grounded[player_index];
    uint8_t action_state = callback_owner.action_state;
    const int running_tap_jump_pressed =
        source_ground_input != NULL &&
        input->main_stick_y <=
            -(int16_t)
                source_ground_input->running_jump_axis_threshold &&
        input_tilt_y_age < fighter->tap_jump_input_window_ticks;
    uint8_t support = world->support[player_index];
    uint8_t air_jumps_remaining =
        world->air_jumps_remaining[player_index];
    uint8_t recovery_available =
        world->recovery_available[player_index];
    uint8_t short_hop_latched =
        world->short_hop_latched[player_index];
    uint8_t platform_drop_ticks =
        world->platform_drop_ticks[player_index];
    scratch->crouch_pass_pending_ticks[player_index] =
        world->crouch_pass_pending_ticks[player_index];
    uint8_t fast_fall = world->fast_fall[player_index];
    int8_t facing = world->facing[player_index];
    const ssbm_ledge_response_attributes *reference_ledge_response =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? ssbm_common_reference_ledge_response()
            : NULL;
    const int ledge_c_attack_held =
        reference_ledge_response != NULL &&
        input->secondary_stick_y <=
            -(int16_t)reference_ledge_response->c_attack_axis_threshold;
    const int ledge_c_roll_inward_held =
        reference_ledge_response != NULL &&
        (int32_t)facing * (int32_t)input->secondary_stick_x >=
            (int32_t)reference_ledge_response->c_roll_axis_threshold;
    int8_t dash_direction = world->dash_direction[player_index];
    int8_t previous_strong_direction =
        world->previous_strong_direction[player_index];
    uint8_t directional_input_flags =
        (uint8_t)(
            (dodge_down_held != 0
                 ? PF_M4_DIRECTIONAL_INPUT_DODGE_DOWN
                 : UINT8_C(0)) |
            (input->secondary_stick_y <=
                     -(int16_t)fighter->down_c_up_axis_threshold
                 ? PF_M4_DIRECTIONAL_INPUT_C_UP
                 : UINT8_C(0)) |
            (axis_is_in_prone_horizontal_wedge(
                 input->secondary_stick_x,
                 input->secondary_stick_y,
                 fighter->down_horizontal_axis_threshold,
                 fighter->down_horizontal_angle_tan_f32)
                 ? (input->secondary_stick_x < INT16_C(0)
                        ? PF_M4_DIRECTIONAL_INPUT_C_LEFT
                        : PF_M4_DIRECTIONAL_INPUT_C_RIGHT)
                 : UINT8_C(0)) |
            (ledge_c_attack_held != 0
                 ? PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ATTACK
                 : UINT8_C(0)) |
            (ledge_c_roll_inward_held != 0
                 ? PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ROLL_INWARD
                 : UINT8_C(0)) |
            (world->previous_directional_input_flags[player_index] &
             PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL));
    const int ledge_c_attack_pressed =
        ledge_c_attack_held != 0 &&
        (world->previous_directional_input_flags[player_index] &
         PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ATTACK) == UINT8_C(0);
    const int ledge_c_roll_inward_pressed =
        ledge_c_roll_inward_held != 0 &&
        (world->previous_directional_input_flags[player_index] &
         PF_M4_DIRECTIONAL_INPUT_LEDGE_C_ROLL_INWARD) == UINT8_C(0);
    int8_t tilt_x_direction = input_tilt_x_direction;
    int8_t tilt_y_direction = input_tilt_y_direction;
    uint8_t tilt_x_age = input_tilt_x_age;
    uint8_t tilt_y_age = input_tilt_y_age;
    int launched_this_tick = 0;
    int ground_jump_entry_this_tick = 0;
    int dropped_platform_this_tick = 0;
    int ledge_motion_handled = 0;
    int released_ledge_this_tick = 0;
    int initial_dash_entered_this_tick = 0;
    int resumed_hitlag_motion_this_tick = 0;
    int revival_drop_this_tick = 0;
    int damage_fall_wiggle_this_tick = 0;
    int damage_released_jump_requested = 0;
    int guard_dash_grab_window_entered_this_tick = 0;
    int exact_wall_response_this_tick = 0;
    float exact_wall_contact_position_y_f32 = INT32_C(0);
    float initial_dash_entry_motion_velocity_x = velocity_x;
    float animation_motion_x_f32 = INT32_C(0);
    float animation_motion_y_f32 = INT32_C(0);
    float integrated_self_x_f32;
    float integrated_self_y_f32;
    float integrated_animation_x_f32;
    float integrated_animation_y_f32;
    int hitstun_locked;
    float previous_position_x;
    float next_position;
    pf_status status;

    if (grounded != UINT8_C(0))
    {
        velocity_y = INT32_C(0);
    }

    if (world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
        world->action_ticks[player_index] == UINT16_C(0) &&
        (world->dash_direction[player_index] == INT8_C(-2) ||
         world->dash_direction[player_index] == INT8_C(2)))
    {
        animation_motion_x_f32 =
            (world->dash_direction[player_index] < INT8_C(0)
                 ? -1.0f
                 : 1.0f) *
            0x1.006p-5f;
        dash_direction = INT8_C(0);
    }

    copy_combat_scratch(world, scratch, player_index);
    const int guard_dash_grab_available =
        source_ground_input != NULL &&
        callback_owner.action_state == (uint8_t)PF_M4_ACTION_SHIELD &&
        scratch->guard_dash_grab_window_ticks[player_index] != UINT8_C(0);
    if (callback_owner.entered_this_tick != UINT8_C(0))
    {
        if (previous_action_state ==
                (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
            previous_action_state ==
                (uint8_t)PF_M4_ACTION_STANDING_TURN ||
            previous_action_state ==
                (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
            previous_action_state ==
                (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
            previous_action_state ==
                (uint8_t)PF_M4_ACTION_ROLL_BACKWARD)
        {
            dash_direction = INT8_C(0);
        }
        if (action_is_ground_attack(previous_action_state) ||
            action_is_light_aerial(previous_action_state) ||
            action_is_throw(previous_action_state) ||
            previous_action_state == (uint8_t)PF_M4_ACTION_GRAB ||
            previous_action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB)
        {
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] = UINT8_C(0);
            scratch->smash_charge_ticks[player_index] = UINT16_C(0);
        }
        if (previous_action_state ==
            (uint8_t)PF_M4_ACTION_RAPID_JAB_END)
        {
            scratch->rapid_jab_input_count[player_index] = UINT8_C(0);
            scratch->rapid_jab_continue[player_index] = UINT8_C(0);
        }
        if (previous_action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_STUN)
        {
            scratch->shield_stun_ticks[player_index] = UINT16_C(0);
            if (callback_owner.action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD)
            {
                scratch->powershield[player_index] = UINT8_C(0);
                scratch->shield_strength[player_index] =
                    input_shield_strength_value;
            }
        }
        if (previous_action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
        {
            scratch->powershield[player_index] = UINT8_C(0);
        }
    }
    if (callback_owner.entered_this_tick != UINT8_C(0) &&
        previous_action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        callback_owner.action_state == (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        /* Anim changed KneeBend to Jump before this update's IASA. Apply the
         * Jump entry state now so SpecialAir/EscapeAir/AttackAir observe the
         * same takeoff velocity and airborne collision owner. */
        enter_ground_jump(
            fighter,
            world->previous_main_stick_x[player_index],
            short_hop_latched,
            facing,
            &velocity_x,
            &velocity_y,
            &action_ticks,
            &source_submotion,
            &grounded,
            &action_state,
            &support,
            &short_hop_latched,
            &fast_fall,
            &tilt_y_age);
        launched_this_tick = 1;
        ground_jump_entry_this_tick = 1;
    }
    scratch->horizontal_input_age[player_index] =
        world->horizontal_input_age[player_index] < UINT8_C(254)
            ? (uint8_t)(world->horizontal_input_age[player_index] +
                        UINT8_C(1))
            : UINT8_C(254);
    scratch->horizontal_input_direction[player_index] =
        world->horizontal_input_direction[player_index];
    if (input_tilt_x_direction != INT8_C(0) &&
        input_tilt_x_direction !=
            world->previous_tilt_x_direction[player_index])
    {
        /* ftInput updates x676/x2228_b7 on a horizontal threshold
         * crossing. Unlike the generic tilt-age helper, returning the stick
         * to neutral does not discard the remembered direction. */
        scratch->horizontal_input_age[player_index] = UINT8_C(0);
        scratch->horizontal_input_direction[player_index] =
            input_tilt_x_direction;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN &&
        dash_direction != INT8_C(0) &&
        ((dash_direction >= INT8_C(-1) &&
          dash_direction <= INT8_C(1)) ||
         action_ticks + UINT16_C(1) >=
             fighter->standing_turn_facing_tick))
    {
        facing = dash_direction < INT8_C(0)
                     ? INT8_C(-1)
                     : INT8_C(1);
    }
    if (scratch->ledge_invulnerability_ticks[player_index] >
        UINT16_C(0))
    {
        --scratch->ledge_invulnerability_ticks[player_index];
    }
    if (scratch->ledge_regrab_lockout_ticks[player_index] >
        UINT16_C(0))
    {
        --scratch->ledge_regrab_lockout_ticks[player_index];
    }
    if (world->active[player_index] == UINT8_C(0))
    {
        if (world->stock_count != UINT8_C(0) &&
            scratch->stocks_remaining[player_index] == UINT8_C(0))
        {
            action_state = (uint8_t)PF_M4_ACTION_ELIMINATED;
            action_ticks = UINT16_C(0);
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            velocity_x = INT32_C(0);
            velocity_y = INT32_C(0);
        }
        else
        {
            if (scratch->respawn_ticks[player_index] > UINT16_C(0))
            {
                --scratch->respawn_ticks[player_index];
            }
            if (scratch->respawn_ticks[player_index] == UINT16_C(0))
            {
                prepare_spawn(
                    fighter,
                    stage,
                    world,
                    scratch,
                    player_index,
                    &position_x,
                    &position_y,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &source_submotion,
                    &grounded,
                    &action_state,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &platform_drop_ticks,
                    &fast_fall,
                    &facing,
                    &dash_direction,
                    &previous_strong_direction,
                    &directional_input_flags);
                scratch->active[player_index] = UINT8_C(1);
                scratch->respawn_invulnerability_ticks[player_index] =
                    UINT16_C(0);
                if (world->sudden_death != UINT8_C(0))
                {
                    scratch->damage_f32[player_index] =
                        UINT32_C(300) * (uint32_t)1.0f;
                }
                status = pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_RESPAWN,
                    PF_SIM_EVENT_NO_PLAYER,
                    (uint8_t)player_index,
                    scratch->damage_f32[player_index],
                    velocity_x,
                    velocity_y,
                    world->sudden_death != UINT8_C(0)
                        ? (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH
                        : UINT16_C(0),
                    world->respawn_invulnerability_config_ticks,
                    NULL);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
            }
            else
            {
                action_state = (uint8_t)PF_M4_ACTION_RESPAWN_WAIT;
                action_ticks = UINT16_C(0);
                grounded = UINT8_C(0);
                support = (uint8_t)PF_M4_SURFACE_NONE;
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
            }
        }

        update_shield_tilt(
            scratch,
            input,
            player_index,
            action_state,
            scratch->hitlag_resume_action[player_index],
            facing);
        write_scratch(
            scratch,
            player_index,
            input,
            position_x,
            position_y,
            velocity_x,
            velocity_y,
            action_ticks,
            source_submotion,
            INT32_C(0),
            INT32_C(0),
            respawn_count,
            grounded,
            action_state,
            support,
            air_jumps_remaining,
            recovery_available,
            short_hop_latched,
            platform_drop_ticks,
            fast_fall,
            facing,
            dash_direction,
            previous_strong_direction,
            directional_input_flags,
            tilt_x_direction,
            tilt_y_direction,
            tilt_x_age,
            tilt_y_age);
        return PF_STATUS_OK;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM)
    {
        const uint32_t total_ticks =
            (uint32_t)stage->revival_platform_descent_ticks +
            (uint32_t)stage->revival_platform_hold_ticks;
        const int descent_complete =
            (uint32_t)action_ticks + UINT32_C(1) >=
            stage->revival_platform_descent_ticks;
        const int priority_input =
            special_pressed != 0 || light_attack_pressed != 0 ||
            strong_attack_pressed != 0 || dense_shield_pressed != 0 ||
            jump_pressed != 0;
        const int fall_input =
            shield_held != 0 || taunt_pressed != 0 ||
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold ||
            (int32_t)facing * (int32_t)input->main_stick_x <=
                -(int32_t)fighter->teeter_turn_axis_threshold ||
            (int32_t)facing * (int32_t)input->main_stick_x >=
                (int32_t)fighter->walk_axis_threshold;
        const int input_drop =
            descent_complete != 0 &&
            (priority_input != 0 || fall_input != 0);
        const int automatic_drop =
            (uint32_t)action_ticks >= total_ticks;

        position_x = revival_platform_x(
            stage,
            world,
            player_index);
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        grounded = UINT8_C(0);
        support = (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM;
        fast_fall = UINT8_C(0);
        if (input_drop != 0 || automatic_drop != 0)
        {
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            position_y = stage->revival_platform_end_y_f32 -
                         fighter->half_height_f32;
            scratch->respawn_invulnerability_ticks[player_index] =
                world->respawn_invulnerability_config_ticks;
            revival_drop_this_tick = 1;
            status = pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_REVIVAL_DROP,
                PF_SIM_EVENT_NO_PLAYER,
                (uint8_t)player_index,
                UINT32_C(0),
                velocity_x,
                velocity_y,
                UINT16_C(0),
                automatic_drop != 0 ? UINT16_C(1) : UINT16_C(0),
                NULL);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
        else
        {
            ++action_ticks;
            position_y =
                revival_platform_y(
                    stage,
                    (uint16_t)(action_ticks + UINT16_C(1))) -
                fighter->half_height_f32;
        }

        if (input_drop == 0 && automatic_drop == 0)
        {
            update_shield_tilt(
                scratch,
                input,
                player_index,
                action_state,
                scratch->hitlag_resume_action[player_index],
                facing);
            write_scratch(
                scratch,
                player_index,
                input,
                position_x,
                position_y,
                velocity_x,
                velocity_y,
                action_ticks,
                source_submotion,
                INT32_C(0),
                INT32_C(0),
                respawn_count,
                grounded,
                action_state,
                support,
                air_jumps_remaining,
                recovery_available,
                short_hop_latched,
                platform_drop_ticks,
                fast_fall,
                facing,
                dash_direction,
                previous_strong_direction,
                directional_input_flags,
                tilt_x_direction,
                tilt_y_direction,
                tilt_x_age,
                tilt_y_age);
            return PF_STATUS_OK;
        }
    }
    if (shield_held != 0 &&
        !action_freezes_shield_strength(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        scratch->shield_strength[player_index] =
            input_shield_strength_value;
    }
    if (revival_drop_this_tick == 0 &&
        scratch->respawn_invulnerability_ticks[player_index] > UINT16_C(0))
    {
        --scratch->respawn_invulnerability_ticks[player_index];
    }
    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        --scratch->tech_window_ticks[player_index];
    }
    if (scratch->tech_lockout_ticks[player_index] > UINT16_C(0))
    {
        --scratch->tech_lockout_ticks[player_index];
    }
    if (shield_pressed != 0 &&
        !action_is_shield_break(action_state) &&
        scratch->tech_lockout_ticks[player_index] == UINT16_C(0))
    {
        scratch->tech_window_ticks[player_index] =
            fighter->tech_window_ticks;
        scratch->tech_lockout_ticks[player_index] =
            fighter->tech_lockout_ticks;
    }
    scratch->shield_held[player_index] = input_trigger_state_value;
    if (shield_pressed != 0)
    {
        scratch->trigger_input_age[player_index] = UINT8_C(0);
    }
    else if (!(scratch->hitlag_ticks[player_index] > UINT16_C(0) &&
               scratch->trigger_input_age[player_index] == UINT8_C(0)) &&
             scratch->trigger_input_age[player_index] < UINT8_MAX)
    {
        /* Fighter_Spaghetti_8006AD10 accumulates x668 button edges while
         * x2219_b5 is set.  The LR edge therefore keeps x67F at zero for
         * every hitlag input sample, including samples after the physical
         * trigger was released. */
        ++scratch->trigger_input_age[player_index];
    }
    if (raw_attack_pressed != 0 || raw_special_pressed != 0)
    {
        scratch->prone_attack_input_age[player_index] = UINT8_C(0);
    }
    else if (scratch->prone_attack_input_age[player_index] < UINT8_MAX)
    {
        ++scratch->prone_attack_input_age[player_index];
    }
    if (source_ground_input != NULL &&
        scratch->hitlag_ticks[player_index] == UINT16_C(0))
    {
        if (raw_up_special_input_pressed != 0)
        {
            scratch->up_special_input_age[player_index] = UINT8_C(0);
        }
        else if (scratch->up_special_input_age[player_index] < UINT8_MAX)
        {
            ++scratch->up_special_input_age[player_index];
        }
    }

    if (action_is_match_entry(action_state))
    {
        const ssbm_stage_collision_profile *profile =
            ssbm_reference_stage_collision(
                stage->reference_collision_profile);
        const ssbm_stage_spawn_point *spawn;
        uint32_t elapsed_ticks;
        float rise_f32;

        if (match_entry == NULL || source_character == NULL ||
            profile == NULL || profile->spawn_point_count < UINT8_C(4) ||
            match_entry->ascent_ticks == UINT16_C(0) ||
            match_entry->descent_ticks == UINT16_C(0) ||
            source_character->match_entry_rise_f32 <= INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        spawn = &profile->spawn_points[player_index + UINT32_C(2)];
        position_x = spawn->position_x_f32;
        /* StageInfo spawn points are Fighter root coordinates. Canonical
         * simulation position is the authored body center, so use the same
         * root-to-center conversion as ordinary reference-stage spawning. */
        position_y = spawn->position_y_f32 - fighter->half_height_f32;
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        grounded = UINT8_C(0);
        support = (uint8_t)PF_M4_SURFACE_NONE;
        fast_fall = UINT8_C(0);
        dash_direction = INT8_C(0);

        if (action_state == (uint8_t)PF_M4_ACTION_MATCH_ENTRY)
        {
            const uint32_t delay_ticks =
                (uint32_t)match_entry->player_delay_stride_ticks *
                (player_index + UINT32_C(1));

            if ((uint32_t)action_ticks < delay_ticks)
            {
                ++action_ticks;
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_MATCH_ENTRY_START;
                action_ticks = UINT16_C(1);
                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_ENTRY_START;
                source_animation_frame_f32 = INT32_C(0);
                source_animation_rate_f32 = 1.0f;
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_MATCH_ENTRY_START)
        {
            if ((uint32_t)action_ticks <
                (uint32_t)match_entry->ascent_ticks - UINT32_C(1))
            {
                ++action_ticks;
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_MATCH_ENTRY_END;
                action_ticks = UINT16_C(0);
                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
                source_animation_frame_f32 = INT32_C(0);
                source_animation_rate_f32 = INT32_C(0);
            }
        }
        else
        {
            ++action_ticks;
        }

        if (action_state ==
            (uint8_t)PF_M4_ACTION_MATCH_ENTRY_START)
        {
            elapsed_ticks = (uint32_t)action_ticks;
            rise_f32 = source_character->match_entry_rise_f32 *
                       (float)elapsed_ticks /
                       (float)match_entry->ascent_ticks;
            position_y -= rise_f32;
            source_animation_frame_f32 =
                (int32_t)(action_ticks - UINT16_C(1)) * 1.0f;
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_MATCH_ENTRY_END)
        {
            elapsed_ticks =
                (uint32_t)match_entry->descent_ticks -
                (uint32_t)action_ticks;
            rise_f32 = source_character->match_entry_rise_f32 *
                       (float)elapsed_ticks /
                       (float)match_entry->descent_ticks;
            position_y -= rise_f32;
        }

        write_scratch(
            scratch,
            player_index,
            input,
            position_x,
            position_y,
            velocity_x,
            velocity_y,
            action_ticks,
            source_submotion,
            source_animation_frame_f32,
            source_animation_rate_f32,
            respawn_count,
            grounded,
            action_state,
            support,
            air_jumps_remaining,
            recovery_available,
            short_hop_latched,
            platform_drop_ticks,
            fast_fall,
            facing,
            dash_direction,
            previous_strong_direction,
            directional_input_flags,
            tilt_x_direction,
            tilt_y_direction,
            tilt_x_age,
            tilt_y_age);
        return PF_STATUS_OK;
    }

    if (scratch->hitlag_ticks[player_index] > UINT16_C(0) ||
        (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
         scratch->hitlag_resume_action[player_index] != UINT8_C(0)))
    {
        const int resolving_zero_hitlag =
            scratch->hitlag_ticks[player_index] == UINT16_C(0);
        const int drop_cancel_eligible =
            resolving_zero_hitlag == 0 &&
            drop_cancel_hitlag_is_eligible(
                fighter,
                action_ticks,
                scratch->hitlag_ticks[player_index],
                scratch->hitlag_resume_action[player_index],
                platform_drop_ticks);

        scratch->position_x_f32[player_index] = position_x;
        scratch->position_y_f32[player_index] = position_y;
        scratch->grounded[player_index] = grounded;
        scratch->support[player_index] = support;
        if (platform_drop_ticks > UINT8_C(0))
        {
            --platform_drop_ticks;
        }
        if (drop_cancel_eligible != 0 &&
            platform_drop_ticks == UINT8_C(0) &&
            scratch->hitlag_ticks[player_index] == UINT16_C(1))
        {
            float drop_cancel_surface_y_f32 = INT32_C(0);
            uint8_t drop_cancel_support =
                (uint8_t)PF_M4_SURFACE_NONE;

            if (find_drop_cancel_platform(
                    stage,
                    fighter,
                    world->tick + UINT64_C(1),
                    position_x,
                    position_y,
                    &drop_cancel_surface_y_f32,
                    &drop_cancel_support))
            {
                uint8_t landing_action =
                    scratch->hitlag_resume_action[player_index];

                land_from_air(
                    content,
                    drop_cancel_surface_y_f32,
                    drop_cancel_support,
                    input->main_stick_x,
                    facing,
                    scratch,
                    player_index,
                    &position_y,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &source_submotion,
                    &grounded,
                    &landing_action,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &fast_fall,
                    &dash_direction);
                scratch->hitlag_resume_action[player_index] =
                    landing_action;
                scratch->position_y_f32[player_index] = position_y;
                scratch->grounded[player_index] = grounded;
                scratch->support[player_index] = support;
            }
        }
        /* A source post-frame that reports one hitlag frame remaining has
         * already run OnEveryHitlag for its input sample.  Before the next
         * controller refresh, the engine clears hitlag and invokes the
         * post-hitlag callback.  The target row that resolves one to zero
         * must therefore not consume its new input as an extra SDI pulse. */
        if (scratch->hitlag_ticks[player_index] > UINT16_C(1) &&
            (action_is_damage(
                 scratch->hitlag_resume_action[player_index]) ||
            scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_RESET_BOUND ||
            scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN))
        {
            const int shield_sdi =
                scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN;
            const int sdi_stick_active =
                shield_sdi != 0
                    ? axis_magnitude(input->main_stick_x) >=
                          fighter->sdi_stick_threshold
                    : ssbm_stick_meets_radial_threshold(
                          input->main_stick_x,
                          input->main_stick_y,
                          fighter->sdi_stick_threshold);
            const int fresh_sdi_tilt =
                shield_sdi != 0
                    ? tilt_x_age < fighter->sdi_stick_window_ticks
                    : tilt_x_age < fighter->sdi_stick_window_ticks ||
                          tilt_y_age < fighter->sdi_stick_window_ticks;
            const int64_t previous_sdi_x =
                (int64_t)world->previous_main_stick_x[player_index];
            const int64_t previous_sdi_y =
                (int64_t)world->previous_main_stick_y[player_index];
            const int previous_sdi_stick_active =
                shield_sdi != 0
                    ? world->previous_main_stick_x[player_index] >=
                          (int16_t)fighter->sdi_stick_threshold
                    : previous_sdi_x * previous_sdi_x +
                              previous_sdi_y * previous_sdi_y >
                          (int64_t)fighter->sdi_stick_threshold *
                              (int64_t)fighter->sdi_stick_threshold;
            const int ucf_sdi_tilt =
                ucf084_enabled != 0 &&
                sdi_stick_active != 0 &&
                fresh_sdi_tilt == 0 &&
                (shield_sdi != 0
                     ? scratch->ucf_tilt_x_age[player_index] <= UINT8_C(1) &&
                           (previous_sdi_stick_active != 0 ||
                            ucf_raw_delta_x * ucf_raw_delta_x > 62 * 62)
                     : (scratch->ucf_tilt_x_age[player_index] <= UINT8_C(1) ||
                        scratch->ucf_tilt_y_age[player_index] <= UINT8_C(1)) &&
                           previous_sdi_stick_active == 0 &&
                           ucf_raw_delta_x * ucf_raw_delta_x +
                                   ucf_raw_delta_y * ucf_raw_delta_y >
                               62 * 62);
            const int8_t sdi_x =
                sdi_stick_active != 0
                    ? axis_direction(
                          input->main_stick_x,
                          UINT16_C(0))
                    : INT8_C(0);

            const int8_t sdi_y =
                sdi_stick_active != 0 && shield_sdi == 0
                    ? axis_direction(
                          input->main_stick_y,
                          UINT16_C(0))
                    : INT8_C(0);

            if (sdi_stick_active != 0 &&
                (fresh_sdi_tilt != 0 || ucf_sdi_tilt != 0))
            {
                status = apply_hitlag_shift(
                    content,
                    world,
                    scratch,
                    player_index,
                    input->main_stick_x,
                    shield_sdi != 0
                        ? INT16_C(0)
                        : input->main_stick_y,
                    shield_sdi != 0
                        ? multiply_f32(
                              fighter->sdi_distance_x_f32,
                              fighter->shield_sdi_scale_f32)
                        : fighter->sdi_distance_x_f32,
                    shield_sdi != 0
                        ? INT32_C(0)
                        : fighter->sdi_distance_y_f32,
                    shield_sdi,
                    0);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                if (scratch->sdi_pulse_count[player_index] !=
                    UINT8_MAX)
                {
                    ++scratch->sdi_pulse_count[player_index];
                }
                tilt_x_age = UINT8_C(254);
                if (shield_sdi == 0)
                {
                    tilt_y_age = UINT8_C(254);
                }
            }
            scratch->sdi_direction_x[player_index] = sdi_x;
            scratch->sdi_direction_y[player_index] = sdi_y;
        }

        if (resolving_zero_hitlag == 0)
        {
            --scratch->hitlag_ticks[player_index];
        }
        action_state = (uint8_t)PF_M4_ACTION_HITLAG;
        if (scratch->hitlag_ticks[player_index] == UINT16_C(0))
        {
            action_state =
                scratch->hitlag_resume_action[player_index];
            scratch->hitlag_resume_action[player_index] = UINT8_C(0);
            if (action_is_damage(action_state) ||
                action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
            {
                /* Fighter_8006D10C invokes Damage's post-hitlag callback
                 * after the final frozen update but before the next fighter
                 * controller refresh.  Consequently ASDI and DI consume the
                 * processed main/C-stick sample retained from the preceding
                 * pre-frame, not the input being prepared for this update. */
                const int c_stick_asdi =
                    ssbm_stick_meets_radial_threshold(
                        world->previous_secondary_stick_x[player_index],
                        world->previous_secondary_stick_y[player_index],
                        fighter->sdi_stick_threshold);
                const int main_stick_asdi =
                    ssbm_stick_meets_radial_threshold(
                        world->previous_main_stick_x[player_index],
                        world->previous_main_stick_y[player_index],
                        fighter->sdi_stick_threshold);

                if (c_stick_asdi != 0 || main_stick_asdi != 0)
                {
                    status = apply_hitlag_shift(
                        content,
                        world,
                        scratch,
                        player_index,
                        c_stick_asdi != 0
                            ? world->previous_secondary_stick_x[player_index]
                            : world->previous_main_stick_x[player_index],
                        c_stick_asdi != 0
                            ? world->previous_secondary_stick_y[player_index]
                            : world->previous_main_stick_y[player_index],
                        fighter->asdi_distance_x_f32,
                        fighter->asdi_distance_y_f32,
                        0,
                        1);
                    if (status != PF_STATUS_OK)
                    {
                        return status;
                    }
                }
                status = ssbm_apply_di_f32(
                    fighter->di_max_angle_radians_q30,
                    world->previous_main_stick_x[player_index],
                    world->previous_main_stick_y[player_index],
                    &scratch
                         ->knockback_velocity_x_f32[player_index],
                    &scratch
                         ->knockback_velocity_y_f32[player_index]);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                /* Melee keeps launch knockback in x8c_kb_vel. Ground damage
                 * additionally owns xF0_ground_kb_vel, which projects the
                 * launch onto the floor tangent after friction. */
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
                if (scratch->grounded[player_index] != UINT8_C(0) &&
                    scratch->ground_knockback_velocity_f32[player_index] !=
                        INT32_C(0))
                {
                    scratch->knockback_velocity_x_f32[player_index] =
                        scratch->ground_knockback_velocity_f32[player_index];
                    scratch->knockback_velocity_y_f32[player_index] =
                        INT32_C(0);
                }
                else
                {
                    grounded = UINT8_C(0);
                    support = (uint8_t)PF_M4_SURFACE_NONE;
                    scratch->grounded[player_index] = UINT8_C(0);
                    scratch->support[player_index] =
                        (uint8_t)PF_M4_SURFACE_NONE;
                    scratch->ground_knockback_velocity_f32[player_index] =
                        INT32_C(0);
                }
                fast_fall = UINT8_C(0);
                dash_direction = INT8_C(0);
            }
            else if (
                action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN)
            {
                if (axis_magnitude(input->main_stick_x) >=
                    fighter->sdi_stick_threshold)
                {
                    status = apply_hitlag_shift(
                        content,
                        world,
                        scratch,
                        player_index,
                        input->main_stick_x,
                        INT16_C(0),
                        multiply_f32(
                            fighter->asdi_distance_x_f32,
                            fighter->shield_sdi_scale_f32),
                        INT32_C(0),
                        1,
                        0);
                    if (status != PF_STATUS_OK)
                    {
                        return status;
                    }
                }
            }
            else if (
                action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
            {
                enter_shield_break_launch(
                    fighter,
                    scratch,
                    player_index,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &source_submotion,
                    &grounded,
                    &action_state,
                    &support,
                    &short_hop_latched,
                    &fast_fall,
                    &dash_direction);
                scratch->grounded[player_index] = grounded;
                scratch->support[player_index] = support;
            }
            /* Melee resumes the restored action on the sample where hitlag
             * reaches zero; it does not spend an extra frozen simulation
             * tick displaying that action at its pre-hitlag frame. */
            resumed_hitlag_motion_this_tick = 1;
            scratch->sdi_direction_x[player_index] = INT8_C(0);
            scratch->sdi_direction_y[player_index] = INT8_C(0);
        }
        position_x = scratch->position_x_f32[player_index];
        position_y = scratch->position_y_f32[player_index];
        grounded = scratch->grounded[player_index];
        support = scratch->support[player_index];
        if (!action_retains_shield_strength(
                action_state,
                scratch->hitlag_resume_action[player_index]))
        {
            scratch->shield_strength[player_index] = UINT16_C(0);
        }
        update_shield_tilt(
            scratch,
            input,
            player_index,
            action_state,
            scratch->hitlag_resume_action[player_index],
            facing);
        if (resumed_hitlag_motion_this_tick == 0)
        {
            write_scratch(
                scratch,
                player_index,
                input,
                position_x,
                position_y,
                velocity_x,
                velocity_y,
                action_ticks,
                source_submotion,
                source_animation_frame_f32,
                source_animation_rate_f32,
                respawn_count,
                grounded,
                action_state,
                support,
                air_jumps_remaining,
                recovery_available,
                short_hop_latched,
                platform_drop_ticks,
                fast_fall,
                facing,
                dash_direction,
                previous_strong_direction,
                directional_input_flags,
                tilt_x_direction,
                tilt_y_direction,
                tilt_x_age,
                tilt_y_age);
            return PF_STATUS_OK;
        }
    }

    {
        hitstun_locked =
        action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND ||
        ((action_is_damage(action_state) ||
          action_is_surface_bounce(action_state)) &&
         scratch->hitstun_ticks[player_index] > UINT16_C(0));
    }

    if (hitstun_locked &&
        action_state == (uint8_t)PF_M4_ACTION_HITSTUN &&
        grounded == UINT8_C(0) &&
        (directional_input_flags &
         PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL) != UINT8_C(0))
    {
        const ssbm_damage_response_attributes *damage_response =
            ssbm_common_reference_damage_response();
        const int lockout_elapsed =
            damage_response != NULL &&
            (uint32_t)action_ticks + UINT32_C(1) >=
                damage_response->meteor_cancel_lockout_ticks;
        const int falling_from_meteor =
            scratch->knockback_velocity_y_f32[player_index] > INT32_C(0);
        const int up_special_cancel =
            lockout_elapsed != 0 && falling_from_meteor != 0 &&
            special_pressed != 0 && up_special_repress_allowed != 0 &&
            input->main_stick_y <=
                -(int16_t)fighter->dash_axis_threshold;
        const int double_jump_cancel =
            up_special_cancel == 0 && lockout_elapsed != 0 &&
            falling_from_meteor != 0 && jump_pressed != 0 &&
            air_jumps_remaining > UINT8_C(0);

            if (up_special_cancel != 0 || double_jump_cancel != 0)
            {
            scratch->knockback_velocity_x_f32[player_index] =
                INT32_C(0);
            scratch->knockback_velocity_y_f32[player_index] =
                INT32_C(0);
            scratch->ground_knockback_velocity_f32[player_index] =
                INT32_C(0);
            scratch->hitstun_ticks[player_index] = UINT16_C(0);
            scratch->tumble[player_index] = UINT8_C(0);
            scratch->damage_jump_buffer_ticks[player_index] =
                UINT16_C(0);
            directional_input_flags = (uint8_t)(
                directional_input_flags &
                (uint8_t)~PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL);
            if (up_special_cancel != 0)
            {
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
                action_state =
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR;
                action_ticks = UINT16_C(0);
                fast_fall = UINT8_C(0);
            }
            else
            {
                enter_double_jump(
                    fighter,
                    input,
                    &velocity_x,
                    &velocity_y,
                    &air_jumps_remaining,
                    &fast_fall,
                    &tilt_y_age,
                    &action_state,
                    &action_ticks,
                    &source_submotion,
                    facing);
            }
            if (scratch->ledge_invulnerability_ticks[player_index] <
                damage_response->meteor_cancel_invulnerability_ticks)
            {
                scratch->ledge_invulnerability_ticks[player_index] =
                    damage_response
                        ->meteor_cancel_invulnerability_ticks;
            }
            hitstun_locked = 0;
        }
    }

    if (action_is_damage(action_state) ||
        action_is_surface_bounce(action_state))
    {
        const ssbm_damage_response_attributes *damage_response =
            ssbm_common_reference_damage_response();

        /* Damage IASA runs after Damage_Anim decrements mv.damage.x0. A
         * jump edge during locked damage stores that remaining timer; once
         * hitstun releases, values at or below common x1D0 synthesize X/Y
         * for the ordinary Wait/Fall IASA route. */
        if (scratch->hitstun_ticks[player_index] > UINT16_C(0) &&
            jump_pressed != 0)
        {
            scratch->damage_jump_buffer_ticks[player_index] =
                (uint16_t)(
                    scratch->hitstun_ticks[player_index] -
                    UINT16_C(1));
        }
        if (scratch->hitstun_ticks[player_index] <= UINT16_C(1))
        {
            const int buffered_jump =
                damage_response != NULL &&
                scratch->damage_jump_buffer_ticks[player_index] >
                    UINT16_C(0) &&
                scratch->damage_jump_buffer_ticks[player_index] <=
                    damage_response->damage_jump_buffer_window_ticks;
            const int requested_jump =
                jump_pressed != 0 ||
                buffered_jump != 0;

            scratch->hitstun_ticks[player_index] = UINT16_C(0);
            scratch->damage_jump_buffer_ticks[player_index] =
                UINT16_C(0);
            if (fighter->reference_frame_data_enabled == UINT8_C(0) &&
                action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
            {
                action_state =
                    grounded != UINT8_C(0)
                        ? (uint8_t)PF_M4_ACTION_GROUND_IDLE
                        : (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
            }
            /* Damage_IASA synthesizes X/Y from the stored x14 timer and
             * then enters the ordinary Wait/Fall IASA table. Preserve that
             * request until those tables reach their jump callback. A
             * neutral sample retains the Damage action and sourced animation
             * until Damage_Anim itself runs out of frames. */
            damage_released_jump_requested = requested_jump;
            hitstun_locked = 0;
        }
    }

    if (!hitstun_locked &&
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
        grounded == UINT8_C(0) &&
        scratch->tumble[player_index] != UINT8_C(0) &&
        special_pressed == 0 &&
        !((jump_pressed != 0 ||
           damage_released_jump_requested != 0) &&
          air_jumps_remaining > UINT8_C(0)))
    {
        const ssbm_damage_response_attributes *damage_response =
            ssbm_common_reference_damage_response();

        if (damage_response != NULL &&
            horizontal_magnitude >=
                damage_response->damage_fall_wiggle_axis_threshold &&
            (tilt_x_age <
                 damage_response->damage_fall_wiggle_tilt_window_ticks ||
             (ucf084_enabled != 0 &&
              tilt_x_age == UINT8_C(1) &&
              axis_magnitude(
                  world->previous_main_stick_x[player_index]) <
                  damage_response->damage_fall_wiggle_axis_threshold &&
              ucf_raw_delta_x * ucf_raw_delta_x > 75 * 75)))
        {
            scratch->tumble[player_index] = UINT8_C(0);
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            action_ticks = UINT16_C(0);
            damage_fall_wiggle_this_tick = 1;
        }
    }

    if (platform_drop_ticks > UINT8_C(0))
    {
        --platform_drop_ticks;
    }

    if (action_uses_ledge(action_state))
    {
        const uint8_t ledge =
            ledge_from_state(
                action_state,
                scratch->hitlag_resume_action[player_index],
                facing);
        const int8_t inward =
            ledge_inward_direction(ledge);
        const int8_t outward = (int8_t)-inward;
        float hang_x = position_x;
        float hang_y = position_y;

        if (fighter->reference_frame_data_enabled != UINT8_C(0))
        {
            const ssbm_ledge_response_attributes *ledge_response =
                reference_ledge_response;
            const falcon_common_attributes *common =
                falcon_reference_common_attributes();
            const int quick =
                ledge_response != NULL &&
                scratch->damage_f32[player_index] <
                    (float)ledge_response->damage_threshold_percent;
            const int ledge_jump_phase_two =
                action_state == (uint8_t)PF_M4_ACTION_LEDGE_JUMP &&
                (source_submotion ==
                     (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_2 ||
                 source_submotion ==
                     (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2);

            if (ledge_response == NULL || common == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (ledge_jump_phase_two == 0)
            {
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
            }
            short_hop_latched = UINT8_C(0);
            fast_fall = UINT8_C(0);
            dash_direction = INT8_C(0);

            if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_CATCH)
            {
                const falcon_submotion_data *catch_motion =
                    falcon_reference_submotion(
                        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH);
                float translation_x_f32;
                float translation_y_f32;

                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH;
                grounded = UINT8_C(0);
                support = (uint8_t)PF_M4_SURFACE_NONE;
                if (catch_motion == NULL ||
                    catch_motion->gameplay_frame_count == UINT16_C(0))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if ((uint32_t)action_ticks + UINT32_C(1) >=
                    (uint32_t)catch_motion->gameplay_frame_count)
                {
                    action_state = (uint8_t)PF_M4_ACTION_LEDGE_HANG;
                    action_ticks = UINT16_C(0);
                    source_submotion =
                        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_WAIT;
                    directional_input_flags = (uint8_t)(
                        directional_input_flags &
                        (uint8_t)~PF_M4_DIRECTIONAL_INPUT_LEDGE_READY);
                    scratch->ledge_invulnerability_ticks[player_index] =
                        ledge_response->wait_invulnerability_ticks;
                    ledge_hang_position(
                        fighter,
                        stage,
                        ledge,
                        &position_x,
                        &position_y);
                }
                else if (!falcon_reference_translation_f32(
                             (uint16_t)
                                 PF_M4_FALCON_SUBMOTION_LEDGE_CATCH,
                             (uint16_t)(action_ticks + UINT16_C(2)),
                             &translation_x_f32,
                             &translation_y_f32))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                else
                {
                    position_x += (float)facing * translation_x_f32;
                    position_y += translation_y_f32;
                    ++action_ticks;
                }
                ledge_motion_handled = 1;
            }
            else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG)
            {
                const int main_option =
                    reference_ledge_direction_option(
                        input->main_stick_x,
                        input->main_stick_y,
                        facing,
                        ledge_response);
                const int c_option =
                    reference_ledge_direction_option(
                        input->secondary_stick_x,
                        input->secondary_stick_y,
                        facing,
                        ledge_response);
                const int ledge_ready =
                    (world->previous_directional_input_flags[player_index] &
                     PF_M4_DIRECTIONAL_INPUT_LEDGE_READY) != UINT8_C(0);
                int direction_option = 0;
                const uint16_t wait_ticks =
                    quick != 0 ? ledge_response->quick_wait_ticks
                               : ledge_response->slow_wait_ticks;
                uint8_t next_action = UINT8_C(0);

                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_WAIT;
                grounded = UINT8_C(0);
                support = (uint8_t)PF_M4_SURFACE_NONE;
                ledge_hang_position(
                    fighter,
                    stage,
                    ledge,
                    &position_x,
                    &position_y);
                /* ftCo_8009AA0C gives an active main stick exclusive
                 * priority. C-stick up/inward can never climb, C-stick
                 * down/outward may drop, and readiness arms only after both
                 * sticks return below the source threshold. */
                if (main_option != 0)
                {
                    direction_option =
                        ledge_ready != 0 ? main_option : 0;
                }
                else if (c_option != 0)
                {
                    direction_option =
                        ledge_ready != 0 && c_option < 0 ? -1 : 0;
                }
                else
                {
                    directional_input_flags |=
                        PF_M4_DIRECTIONAL_INPUT_LEDGE_READY;
                }
                /* CliffWait IASA order in the source is attack, roll, jump,
                 * then the climb/drop router. The C-stick predicates are
                 * rising edges using ftCommonData x7F8/x7FC. */
                if (light_attack_pressed != 0 ||
                    special_pressed != 0 ||
                    ledge_c_attack_pressed != 0)
                {
                    next_action = (uint8_t)PF_M4_ACTION_LEDGE_ATTACK;
                }
                else if (shield_pressed != 0 ||
                         ledge_c_roll_inward_pressed != 0)
                {
                    next_action = (uint8_t)PF_M4_ACTION_LEDGE_ROLL;
                }
                else if (jump_pressed != 0)
                {
                    next_action = (uint8_t)PF_M4_ACTION_LEDGE_JUMP;
                }
                else if (direction_option > 0)
                {
                    next_action = (uint8_t)PF_M4_ACTION_LEDGE_CLIMB;
                }

                if (next_action != UINT8_C(0))
                {
                    if (!enter_reference_ledge_option(
                            fighter,
                            stage,
                            ledge,
                            next_action,
                            quick,
                            &position_x,
                            &position_y,
                            &action_state,
                            &action_ticks,
                            &source_submotion))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    directional_input_flags = (uint8_t)(
                        directional_input_flags &
                        (uint8_t)~PF_M4_DIRECTIONAL_INPUT_LEDGE_READY);
                    if (next_action ==
                        (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
                    {
                        scratch->attack_hit_mask[player_index] = UINT8_C(0);
                        scratch->attack_stale_registered[player_index] =
                            UINT8_C(0);
                    }
                }
                else if (direction_option < 0 ||
                         action_ticks + UINT16_C(1) >= wait_ticks)
                {
                    const int timed_out = direction_option >= 0;

                    action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    source_submotion =
                        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
                    scratch->ledge_regrab_lockout_ticks[player_index] =
                        fighter->ledge_regrab_lockout_ticks;
                    if (timed_out != 0)
                    {
                        scratch->tumble[player_index] = UINT8_C(1);
                    }
                    directional_input_flags = (uint8_t)(
                        directional_input_flags &
                        (uint8_t)~PF_M4_DIRECTIONAL_INPUT_LEDGE_READY);
                    released_ledge_this_tick = 1;
                }
                else
                {
                    ++action_ticks;
                }
                ledge_motion_handled =
                    released_ledge_this_tick == 0;
            }
            else if (ledge_jump_phase_two != 0)
            {
                /* Jump phase two uses ordinary air physics while retaining
                 * the source action clock and animation identity. */
                ledge_motion_handled = 0;
            }
            else
            {
                const falcon_submotion_data *motion =
                    falcon_reference_submotion(source_submotion);
                const uint16_t ground_frame =
                    falcon_reference_ledge_option_ground_frame(
                        source_submotion);
                const int will_ground =
                    grounded == UINT8_C(0) &&
                    ground_frame != UINT16_C(0) &&
                    action_ticks + UINT16_C(2) >= ground_frame;
                const uint16_t translation_frame =
                    (uint16_t)(
                        action_ticks + UINT16_C(2));
                const int was_grounded =
                    world->grounded[player_index] != UINT8_C(0);
                const float previous_option_y_f32 = position_y;
                float translation_x_f32;
                float translation_y_f32;
                float jump_x_from_wait_f32 = INT32_C(0);
                float jump_y_from_wait_f32 = INT32_C(0);
                const int uses_hyrule_jump_path =
                    stage->reference_collision_profile ==
                        (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE &&
                    falcon_reference_hyrule_ledge_jump_position_f32(
                        source_submotion,
                        translation_frame,
                        &jump_x_from_wait_f32,
                        &jump_y_from_wait_f32);

                if (motion == NULL ||
                    motion->gameplay_frame_count == UINT16_C(0))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if ((uint32_t)action_ticks + UINT32_C(1) >=
                    (uint32_t)motion->gameplay_frame_count)
                {
                    if (action_state ==
                        (uint8_t)PF_M4_ACTION_LEDGE_JUMP)
                    {
                        source_submotion =
                            source_submotion ==
                                    (uint16_t)
                                        PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_1
                                ? (uint16_t)
                                      PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2
                                : (uint16_t)
                                      PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_2;
                        action_ticks = UINT16_C(0);
                        grounded = UINT8_C(0);
                        support = (uint8_t)PF_M4_SURFACE_NONE;
                        velocity_x =
                            (int32_t)inward *
                            common->ledge_jump_horizontal_velocity_f32;
                        velocity_y =
                            -common->ledge_jump_vertical_velocity_f32;
                        if (stage->reference_collision_profile ==
                                (uint16_t)
                                    PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE &&
                            falcon_reference_hyrule_ledge_jump_position_f32(
                                source_submotion,
                                UINT16_C(1),
                                &jump_x_from_wait_f32,
                                &jump_y_from_wait_f32))
                        {
                            ledge_hang_position(
                                fighter,
                                stage,
                                ledge,
                                &hang_x,
                                &hang_y);
                            position_x =
                                hang_x -
                                (float)inward * jump_x_from_wait_f32;
                            position_y = hang_y + jump_y_from_wait_f32;
                        }
                        else
                        {
                            position_x += velocity_x;
                            position_y += velocity_y;
                        }
                        launched_this_tick = 1;
                    }
                    else
                    {
                        action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                        action_ticks = UINT16_C(0);
                        source_submotion =
                            (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
                        velocity_x = INT32_C(0);
                        velocity_y = INT32_C(0);
                        scratch->attack_hit_mask[player_index] = UINT8_C(0);
                        scratch->attack_stale_registered[player_index] =
                            UINT8_C(0);
                    }
                }
                else
                {
                    if (uses_hyrule_jump_path != 0)
                    {
                        ledge_hang_position(
                            fighter,
                            stage,
                            ledge,
                            &hang_x,
                            &hang_y);
                        position_x =
                            hang_x - (float)inward * jump_x_from_wait_f32;
                        position_y = hang_y + jump_y_from_wait_f32;
                    }
                    else
                    {
                        if (translation_frame > motion->translation_count)
                        {
                            translation_x_f32 = INT32_C(0);
                            translation_y_f32 = INT32_C(0);
                        }
                        else if (!falcon_reference_translation_f32(
                                     source_submotion,
                                     translation_frame,
                                     &translation_x_f32,
                                     &translation_y_f32))
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                        position_x += (float)inward * translation_x_f32;
                        if (grounded == UINT8_C(0))
                        {
                            position_y += translation_y_f32;
                            if (will_ground != 0)
                            {
                                position_x +=
                                    (float)inward * translation_x_f32;
                                support =
                                    ssbm_reference_stage_ledge_support(
                                        stage->reference_collision_profile,
                                        ledge_x_f32(stage, ledge));
                                if (support ==
                                    (uint8_t)PF_M4_SURFACE_NONE)
                                {
                                    return PF_STATUS_DETERMINISTIC_FAULT;
                                }
                                grounded = UINT8_C(1);
                            }
                        }
                        if (grounded != UINT8_C(0))
                        {
                            position_y =
                                surface_y_f32(
                                    content,
                                    support,
                                    position_x) -
                                fighter->half_height_f32;
                            if (was_grounded != 0)
                            {
                                velocity_x =
                                    (float)inward * translation_x_f32;
                                velocity_y =
                                    position_y - previous_option_y_f32;
                            }
                            else
                            {
                                velocity_x =
                                    (float)inward * translation_x_f32;
                                velocity_y = INT32_C(0);
                            }
                        }
                    }
                    ++action_ticks;
                }
                ledge_motion_handled = 1;
            }
        }
        else
        {
        if (action_state != (uint8_t)PF_M4_ACTION_LEDGE_CATCH)
        {
            ledge_hang_position(
                fighter,
                stage,
                ledge,
                &hang_x,
                &hang_y);
            position_x = hang_x;
            position_y = hang_y;
        }
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        grounded = UINT8_C(0);
        support = (uint8_t)PF_M4_SURFACE_NONE;
        short_hop_latched = UINT8_C(0);
        fast_fall = UINT8_C(0);
        dash_direction = INT8_C(0);

        if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_CATCH)
        {
            const falcon_submotion_data *catch_motion =
                falcon_reference_submotion(
                    (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH);
            float translation_x_f32;
            float translation_y_f32;

            if (catch_motion == NULL ||
                catch_motion->gameplay_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if ((uint32_t)action_ticks + UINT32_C(1) >=
                (uint32_t)catch_motion->gameplay_frame_count)
            {
                action_state = (uint8_t)PF_M4_ACTION_LEDGE_HANG;
                action_ticks = UINT16_C(0);
                ledge_hang_position(
                    fighter,
                    stage,
                    ledge,
                    &position_x,
                    &position_y);
            }
            else if (!falcon_reference_translation_f32(
                         (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH,
                         (uint16_t)(action_ticks + UINT16_C(2)),
                         &translation_x_f32,
                         &translation_y_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            else
            {
                position_x += (float)facing * translation_x_f32;
                position_y += translation_y_f32;
                ++action_ticks;
            }
            ledge_motion_handled = 1;
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            const uint16_t catch_ticks =
                ledge_transition_ticks(fighter);
            const int down_held =
                input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;

            if (action_ticks < catch_ticks)
            {
                ++action_ticks;
            }

            if (action_ticks >= catch_ticks &&
                (light_attack_pressed || strong_attack_pressed))
            {
                action_ticks = UINT16_C(0);
                action_state =
                    (uint8_t)PF_M4_ACTION_LEDGE_ATTACK;
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                ledge_motion_handled = 1;
            }
            else if (action_ticks >= catch_ticks && shield_pressed)
            {
                action_ticks = UINT16_C(0);
                action_state =
                    (uint8_t)PF_M4_ACTION_LEDGE_ROLL;
                ledge_motion_handled = 1;
            }
            else if (action_ticks >= catch_ticks && jump_pressed)
            {
                velocity_x =
                    (float)inward * fighter->ledge_jump_speed_x_f32;
                velocity_y = -fighter->ledge_jump_speed_y_f32;
                action_ticks = UINT16_C(0);
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                launched_this_tick = 1;
                released_ledge_this_tick = 1;
            }
            else if (action_ticks >= catch_ticks &&
                     (down_held ||
                      horizontal_direction == outward))
            {
                velocity_x =
                    horizontal_direction == outward
                        ? (float)outward *
                              fighter->air_speed_f32
                        : (float)outward *
                              fighter->platform_drop_nudge_f32;
                velocity_y = fighter->gravity_f32;
                action_ticks = UINT16_C(0);
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                released_ledge_this_tick = 1;
            }
            else if (action_ticks >= catch_ticks &&
                     horizontal_direction == inward)
            {
                action_ticks = UINT16_C(0);
                action_state =
                    (uint8_t)PF_M4_ACTION_LEDGE_CLIMB;
                ledge_motion_handled = 1;
            }
            else
            {
                ledge_motion_handled = 1;
            }
            if (released_ledge_this_tick != 0)
            {
                scratch->ledge_regrab_lockout_ticks[player_index] =
                    fighter->ledge_regrab_lockout_ticks;
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB)
        {
            const uint16_t climb_ticks =
                ledge_transition_ticks(fighter);
            const float target_x =
                ledge_x_f32(stage, ledge) +
                (float)inward *
                    (fighter->half_width_f32 +
                     fighter->platform_drop_nudge_f32);
            const float target_y =
                stage->floor_y_f32 - fighter->half_height_f32;

            ++action_ticks;
            if (action_ticks >= climb_ticks)
            {
                position_x = target_x;
                land(
                    fighter,
                    stage->floor_y_f32,
                    (uint8_t)PF_M4_SURFACE_FLOOR,
                    &position_y,
                    &velocity_y,
                    &action_ticks,
                    &grounded,
                    &action_state,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &fast_fall,
                    &dash_direction);
            }
            else
            {
                position_x = hang_x + (target_x - hang_x) *
                                          (float)action_ticks /
                                          (float)climb_ticks;
                position_y = hang_y + (target_y - hang_y) *
                                          (float)action_ticks /
                                          (float)climb_ticks;
            }
            ledge_motion_handled = 1;
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
        {
            const float target_x =
                ledge_x_f32(stage, ledge) +
                (float)inward * fighter->ledge_roll_distance_f32;
            const float target_y =
                stage->floor_y_f32 - fighter->half_height_f32;
            const uint16_t movement_ticks =
                fighter->ledge_roll_movement_ticks;

            ++action_ticks;
            if (action_ticks >= fighter->ledge_roll_ticks)
            {
                position_x = target_x;
                land(
                    fighter,
                    stage->floor_y_f32,
                    (uint8_t)PF_M4_SURFACE_FLOOR,
                    &position_y,
                    &velocity_y,
                    &action_ticks,
                    &grounded,
                    &action_state,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &fast_fall,
                    &dash_direction);
            }
            else
            {
                const uint16_t progress_ticks =
                    action_ticks < movement_ticks
                        ? action_ticks
                        : movement_ticks;

                position_x = hang_x + (target_x - hang_x) *
                                          (float)progress_ticks /
                                          (float)movement_ticks;
                position_y = hang_y + (target_y - hang_y) *
                                          (float)progress_ticks /
                                          (float)movement_ticks;
            }
            ledge_motion_handled = 1;
        }
        else
        {
            const attack_data *attack = &fighter->ledge_attack;
            const uint32_t total_ticks =
                (uint32_t)attack->startup_ticks +
                (uint32_t)attack->active_ticks +
                (uint32_t)attack->recovery_ticks;
            const float target_x =
                ledge_x_f32(stage, ledge) +
                (float)inward *
                    (fighter->half_width_f32 +
                     fighter->platform_drop_nudge_f32);
            const float target_y =
                stage->floor_y_f32 - fighter->half_height_f32;

            ++action_ticks;
            if ((uint32_t)action_ticks >= total_ticks)
            {
                position_x = target_x;
                land(
                    fighter,
                    stage->floor_y_f32,
                    (uint8_t)PF_M4_SURFACE_FLOOR,
                    &position_y,
                    &velocity_y,
                    &action_ticks,
                    &grounded,
                    &action_state,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &fast_fall,
                    &dash_direction);
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
            else
            {
                const uint16_t movement_ticks = attack->startup_ticks;
                const uint16_t progress_ticks =
                    action_ticks < movement_ticks
                        ? action_ticks
                        : movement_ticks;

                position_x = hang_x + (target_x - hang_x) *
                                          (float)progress_ticks /
                                          (float)movement_ticks;
                position_y = hang_y + (target_y - hang_y) *
                                          (float)progress_ticks /
                                          (float)movement_ticks;
            }
            ledge_motion_handled = 1;
        }
        }
    }

    if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        const float previous_platform_x =
            platform_center_x_f32(stage, world->tick);
        const float next_platform_x =
            platform_center_x_f32(
                stage,
                world->tick + UINT64_C(1));
        next_position = position_x + next_platform_x - previous_platform_x;
        position_x = next_position;
    }

    const uint8_t reference_special_capabilities =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? reference_action_special_capabilities(
                  action_state,
                  action_ticks,
                  grounded,
                  normal_landing_is_interruptible(
                      fighter,
                      action_state,
                      action_ticks),
                  powershield_release_cancel_ready,
                  ground_iasa_capabilities,
                  initial_dash_special_end_frame)
            : UINT8_C(0);

    if (!ledge_motion_handled &&
        released_ledge_this_tick == 0 &&
        !hitstun_locked &&
        grounded == UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
        fighter->wall_jump_enabled != UINT8_C(0) &&
        strong_direction_value != INT8_C(0) &&
        strong_direction_value != previous_strong_direction &&
        strong_direction_value == wall_contact_away_direction(
                                content,
                                position_x,
                                position_y))
    {
        enter_wall_jump(
            fighter,
            strong_direction_value,
            &velocity_x,
            &velocity_y,
            &action_ticks,
            &action_state,
            &fast_fall,
            &facing);
        launched_this_tick = 1;
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        action_state ==
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND &&
        shield_held == 0 &&
        action_ticks < content->charge.store_animation_ticks)
    {
        action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        action_ticks = UINT16_C(0);
    }

    /*
     * Every mapped Melee ground-attack IASA callback exposes jump plus the
     * dash/crouch/turn/walk subset. Re-enter the existing zero-allocation
     * common ground path for that proven intersection only when the default
     * authored move still matches the extracted Falcon row and its displayed
     * IASA frame has arrived. The extracted action's callback class supplies
     * the exact attack/item/defense capability mask used by the handlers below.
     */
    if (!ledge_motion_handled && !hitstun_locked &&
        reference_jab_chain_ready == 0 &&
        reference_rapid_jab_ready == 0 &&
        (ground_iasa_capabilities &
         PF_M4_FALCON_IASA_COMMON_MOVEMENT) != UINT8_C(0) &&
        ground_common_iasa_input != 0 &&
        ground_common_iasa_unclaimed != 0 &&
        action_is_ground_attack(action_state))
    {
        if (jump_pressed == 0 &&
            input->main_stick_y <
                (int16_t)fighter->crouch_axis_threshold &&
            horizontal_magnitude >= fighter->walk_axis_threshold &&
            tilt_x_age >= fighter->dash_input_window_ticks)
        {
            /* Walk_CheckInput follows Dash_CheckInput in every one of these
             * callbacks, so an aged held axis enters Walk directly. */
            action_state = (uint8_t)PF_M4_ACTION_WALK;
            action_ticks = fighter->dash_input_window_ticks;
        }
        else
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
    }

    if (!ledge_motion_handled &&
        released_ledge_this_tick == 0 &&
        !hitstun_locked &&
        reference_rapid_jab_ready == 0 &&
        !action_is_reference_special_locked(action_state) &&
        action_state != (uint8_t)PF_M4_ACTION_WALL_JUMP &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        !(fighter->reference_frame_data_enabled != UINT8_C(0) &&
          action_state == (uint8_t)PF_M4_ACTION_WALK &&
          grab_pressed != 0) &&
        (fighter->reference_frame_data_enabled == UINT8_C(0) ||
         powershield_release_cancel_ready != 0 ||
         reference_special_capabilities != UINT8_C(0)) &&
        (!action_is_ground_attack(action_state) ||
         ground_reference_attack == NULL ||
         (ground_iasa_capabilities &
          PF_M4_FALCON_IASA_SPECIAL) != UINT8_C(0)) &&
        special_pressed != 0)
    {
        const falcon_common_special_attributes *
            common_special_attributes =
                falcon_reference_common_special_attributes();
        const int raw_up_special_requested =
            input->main_stick_y <=
            -(int16_t)special_vertical_axis_threshold;
        const int raw_down_special_requested =
            input->main_stick_y >=
            (int16_t)special_vertical_axis_threshold;
        const float special_stick_x_f32 =
            axis_f32(input->main_stick_x);
        const int raw_side_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            common_special_attributes != NULL &&
            (special_stick_x_f32 >=
                 common_special_attributes
                     ->side_special_stick_threshold_f32 ||
             special_stick_x_f32 <=
                 -common_special_attributes
                      ->side_special_stick_threshold_f32);
        const int reference_side_special_enabled =
            fighter->reference_frame_data_enabled == UINT8_C(0) ||
            (reference_special_capabilities &
             PF_M4_REFERENCE_SPECIAL_SIDE) != UINT8_C(0);
        const int reference_up_special_enabled =
            fighter->reference_frame_data_enabled == UINT8_C(0) ||
            (reference_special_capabilities &
             PF_M4_REFERENCE_SPECIAL_UP) != UINT8_C(0);
        const int reference_neutral_special_enabled =
            fighter->reference_frame_data_enabled == UINT8_C(0) ||
            (reference_special_capabilities &
             PF_M4_REFERENCE_SPECIAL_NEUTRAL) != UINT8_C(0);
        const int reference_down_special_enabled =
            fighter->reference_frame_data_enabled == UINT8_C(0) ||
            (reference_special_capabilities &
             PF_M4_REFERENCE_SPECIAL_DOWN) != UINT8_C(0);
        /* Ground common IASA checks SpecialS before Hi/N/Lw. SpecialAir
         * instead checks Hi, Lw, S, then N. */
        const int up_special_requested =
            raw_up_special_requested != 0 &&
            up_special_repress_allowed != 0 &&
            reference_up_special_enabled != 0 &&
            (grounded == UINT8_C(0) ||
             raw_side_special_requested == 0 ||
             reference_side_special_enabled == 0);
        const int charge_requested =
            fighter->reference_frame_data_enabled == UINT8_C(0) &&
            content->charge.enabled != UINT8_C(0) &&
            grounded != UINT8_C(0) &&
            up_special_requested != 0 &&
            light_attack_held != 0;
        const int vector_ascent_requested =
            up_special_requested != 0 && charge_requested == 0;
        const int falcon_down_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            raw_down_special_requested != 0 &&
            reference_down_special_enabled != 0 &&
            (grounded == UINT8_C(0) ||
             raw_side_special_requested == 0 ||
             reference_side_special_enabled == 0);
        const int reflector_requested =
            fighter->reference_frame_data_enabled == UINT8_C(0) &&
            content->reflector.enabled != UINT8_C(0) &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;
        const int falcon_side_special_requested =
            raw_side_special_requested != 0 &&
            reference_side_special_enabled != 0 &&
            (grounded != UINT8_C(0) ||
             (up_special_requested == 0 &&
              falcon_down_special_requested == 0)) &&
            reflector_requested == 0;
        const int falcon_neutral_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            reference_neutral_special_enabled != 0 &&
            !(raw_up_special_requested != 0 &&
              up_special_repress_allowed == 0) &&
            up_special_requested == 0 &&
            falcon_down_special_requested == 0 &&
            reflector_requested == 0 &&
            falcon_side_special_requested == 0;
        const int reference_special_input_blocked =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            up_special_requested == 0 &&
            falcon_down_special_requested == 0 &&
            falcon_side_special_requested == 0 &&
            falcon_neutral_special_requested == 0;

        if (reference_special_input_blocked == 0)
        {
            facing = reference_turn_callback_facing_value;
        }

        if (vector_ascent_requested != 0)
        {
            /* reference_special_capabilities is the state-specific decomp
             * callback table. It is authoritative for Falcon; the authored
             * Vector Ascent allowlist intentionally describes fewer states. */
            if ((fighter->reference_frame_data_enabled != UINT8_C(0) ||
                 (content->recovery.enabled != UINT8_C(0) &&
                  recovery_available != UINT8_C(0))) &&
                (fighter->reference_frame_data_enabled != UINT8_C(0) ||
                 action_can_start_vector_ascent(action_state)))
            {
                if (fighter->reference_frame_data_enabled != UINT8_C(0))
                {
                    velocity_x = INT32_C(0);
                    velocity_y = INT32_C(0);
                    /* ftCa_SpecialLw_800E49FC is the shared grounded/air
                     * Falcon Dive initialization callback. It writes
                     * x1968_jumpsUsed=max_jumps before either motion begins. */
                    air_jumps_remaining = UINT8_C(0);
                    action_state =
                        grounded != UINT8_C(0)
                            ? (uint8_t)
                                  PF_M4_ACTION_FALCON_DIVE_START_GROUND
                            : (uint8_t)
                                  PF_M4_ACTION_FALCON_DIVE_START_AIR;
                }
                else
                {
                    velocity_x = scale_axis_f32(
                        input->main_stick_x,
                        content->recovery.horizontal_speed_f32);
                    velocity_y = -content->recovery.vertical_speed_f32;
                    action_state =
                        (uint8_t)PF_M4_ACTION_VECTOR_ASCENT;
                    grounded = UINT8_C(0);
                    support = (uint8_t)PF_M4_SURFACE_NONE;
                    launched_this_tick = 1;
                }
                action_ticks = UINT16_C(0);
                if (fighter->reference_frame_data_enabled == UINT8_C(0))
                {
                    recovery_available = UINT8_C(0);
                }
                fast_fall = UINT8_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
            }
        }
        else if (reference_special_input_blocked == 0)
        {
            if (falcon_side_special_requested != 0)
            {
                if (special_stick_x_f32 * (int32_t)facing <
                    -common_special_attributes
                         ->side_special_turn_threshold_f32)
                {
                    facing = (int8_t)-facing;
                }
                action_state = grounded != UINT8_C(0)
                                   ? (uint8_t)
                                         PF_M4_ACTION_RAPTOR_BOOST_START_GROUND
                                   : (uint8_t)
                                         PF_M4_ACTION_RAPTOR_BOOST_START_AIR;
                /* setupAirStart calls ftCommon_8007D60C, whose compact
                 * jump-count side effect is to consume every remaining
                 * jump. Ground Raptor Boost keeps its ordinary count until
                 * its source collision/animation route enters special fall. */
                if (grounded == UINT8_C(0))
                {
                    air_jumps_remaining = UINT8_C(0);
                }
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
            }
            else if (falcon_down_special_requested != 0)
            {
                action_state =
                    grounded != UINT8_C(0)
                        ? (uint8_t)
                              PF_M4_ACTION_FALCON_KICK_START_GROUND
                        : (uint8_t)
                              PF_M4_ACTION_FALCON_KICK_START_AIR;
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
                scratch->falcon_kick_hit_count[player_index] =
                    UINT8_C(0);
            }
            else
            {
                action_state =
                    falcon_neutral_special_requested != 0
                    ? (grounded != UINT8_C(0)
                           ? (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND
                           : (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR)
                    : charge_requested != 0
                    ? (uint8_t)PF_M4_ACTION_CHARGE_GROUND
                    : grounded != UINT8_C(0)
                    ? (reflector_requested != 0
                           ? (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND
                           : (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND)
                    : (reflector_requested != 0
                           ? (uint8_t)PF_M4_ACTION_REFLECTOR_AIR
                           : (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR);
                if (falcon_neutral_special_requested != 0 &&
                    grounded == UINT8_C(0) &&
                    source_ground_input != NULL &&
                    scratch->horizontal_input_age[player_index] <
                        source_ground_input
                            ->neutral_special_turn_window_ticks &&
                    scratch->horizontal_input_direction[player_index] ==
                        (int8_t)-facing)
                {
                    /* SpecialAirN uses x676/x2228_b7 to honor a recent
                     * opposite horizontal tilt even though the axis did not
                     * reach the side-special gate. */
                    facing = (int8_t)-facing;
                }
            }
            action_ticks = UINT16_C(0);
        }
        if (reference_special_input_blocked == 0 &&
            (vector_ascent_requested == 0 ||
             action_state == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
             action_state ==
                 (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
             action_state ==
                 (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR))
        {
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
            short_hop_latched = UINT8_C(0);
            dash_direction = INT8_C(0);
            if (action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR)
            {
                fast_fall = UINT8_C(0);
            }
            scratch->powershield[player_index] = UINT8_C(0);
        }
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_CHARGE_GROUND &&
        shield_pressed != 0)
    {
        action_state =
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND;
        action_ticks = UINT16_C(0);
        velocity_x = INT32_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_CHARGE_GROUND &&
        special_pressed == 0 &&
        (attack_pressed != 0 ||
         (light_attack_held != 0 &&
          (input->buttons & PF_INPUT_BUTTON_SPECIAL) == UINT64_C(0) &&
          (previous_buttons & PF_INPUT_BUTTON_SPECIAL) != UINT64_C(0))))
    {
        action_state =
            (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND;
        action_ticks = UINT16_C(0);
        velocity_x = INT32_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        special_pressed == 0 &&
        jump_cancel_attack_pressed != 0)
    {
        action_state = ground_strong_attack_action;
        action_ticks = UINT16_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
        if (source_character != NULL &&
            action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
        {
            scratch->jab_chain_buffered[player_index] = UINT8_C(0);
            scratch->rapid_jab_input_count[player_index] = UINT8_C(0);
            scratch->rapid_jab_continue[player_index] = UINT8_C(0);
        }
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_LANDING &&
        normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        input->main_stick_y >=
            (int16_t)fighter->crouch_axis_threshold &&
        (uint32_t)action_ticks + UINT32_C(1) ==
            (uint32_t)fighter->landing_interruptible_tick &&
        jump_pressed == 0 && attack_pressed == 0 &&
        grab_pressed == 0 && shield_held == 0 &&
        special_pressed == 0 && taunt_pressed == 0 &&
        !(strong_direction_value != INT8_C(0) &&
          tilt_x_age < fighter->dash_input_window_ticks))
    {
        action_state = (uint8_t)PF_M4_ACTION_CROUCH;
        action_ticks = UINT16_C(0);
    }
    else if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_LANDING &&
        normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        (horizontal_magnitude >= fighter->walk_axis_threshold ||
         jump_pressed != 0 || attack_pressed != 0 ||
         grab_pressed != 0 || shield_held != 0 ||
         special_pressed != 0 || taunt_pressed != 0))
    {
        action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        action_ticks = UINT16_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        reference_rapid_jab_ready == 0 &&
        grounded != UINT8_C(0) &&
        ((grab_pressed != 0 &&
          ((source_ground_input != NULL &&
            action_state != (uint8_t)PF_M4_ACTION_SHIELD) ||
           (spot_dodge_pressed == 0 && roll_pressed == 0)) &&
          (action_can_start_grab(action_state) ||
           (action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
            powershield_release_cancel_ready != 0) ||
           (action_is_ground_attack(action_state) &&
            (ground_iasa_capabilities &
             PF_M4_FALCON_IASA_GRAB) != UINT8_C(0)))) ||
         boost_grab_pressed != 0) &&
        scratch->grab_target_slot[player_index] == UINT8_C(0) &&
        scratch->grab_owner_slot[player_index] == UINT8_C(0))
    {
        facing = reference_turn_callback_facing_value;
        action_state =
            boost_grab_pressed != 0 ||
                    guard_dash_grab_available != 0 ||
                    callback_owner.action_state ==
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
                    callback_owner.action_state ==
                        (uint8_t)PF_M4_ACTION_RUN
                ? (uint8_t)PF_M4_ACTION_DASH_GRAB
                : (uint8_t)PF_M4_ACTION_GRAB;
        action_ticks = UINT16_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        source_character != NULL &&
        grounded != UINT8_C(0) &&
        (action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
         action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL ||
         action_state == (uint8_t)PF_M4_ACTION_JAB_THIRD ||
         action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP))
    {
        const int attack_edge =
            light_attack_pressed != 0 || light_attack_released != 0;
        const uint16_t displayed_frame =
            (uint16_t)(action_ticks + UINT16_C(1));
        const uint16_t buffer_window =
            action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK
                ? source_character->jab_2_input_window_ticks
                : source_character->jab_3_input_window_ticks;

        if (action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP)
        {
            const uint16_t decision_offset =
                displayed_frame >=
                        source_character->rapid_jab_first_decision_frame
                    ? (uint16_t)(
                          displayed_frame -
                          source_character->rapid_jab_first_decision_frame)
                    : UINT16_MAX;
            const int decision_frame =
                displayed_frame <=
                    source_character->rapid_jab_last_decision_frame &&
                decision_offset != UINT16_MAX &&
                (displayed_frame ==
                     source_character->rapid_jab_last_decision_frame ||
                 decision_offset %
                         source_character->rapid_jab_decision_interval ==
                     UINT16_C(0));

            /* Attack100Loop_Anim consumes the previous IASA callback's A
             * edge at each script throw-flag decision, then clears it if the
             * loop continues. The current frame's IASA edge is recorded only
             * after that animation decision. */
            if (decision_frame != 0)
            {
                if (scratch->rapid_jab_continue[player_index] == UINT8_C(0))
                {
                    action_state = (uint8_t)PF_M4_ACTION_RAPID_JAB_END;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                }
                else
                {
                    scratch->rapid_jab_continue[player_index] = UINT8_C(0);
                }
            }
            if (action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP &&
                attack_edge != 0)
            {
                scratch->rapid_jab_continue[player_index] = UINT8_C(1);
            }
        }
        else if (attack_edge != 0 &&
            scratch->rapid_jab_input_count[player_index] != UINT8_MAX)
        {
            ++scratch->rapid_jab_input_count[player_index];
        }
        if ((action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
             action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL) &&
            light_attack_pressed != 0 && action_ticks <= buffer_window)
        {
            scratch->jab_chain_buffered[player_index] = UINT8_C(1);
        }
        if (action_state == (uint8_t)PF_M4_ACTION_JAB_THIRD &&
            displayed_frame >=
                source_character->jab_3_rapid_enable_frame &&
            scratch->rapid_jab_input_count[player_index] >=
                source_character->rapid_jab_input_count)
        {
            action_state = (uint8_t)PF_M4_ACTION_RAPID_JAB_START;
            action_ticks = UINT16_C(0);
            scratch->jab_chain_buffered[player_index] = UINT8_C(0);
            scratch->rapid_jab_continue[player_index] = UINT8_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] = UINT8_C(0);
        }
        else if (scratch->jab_chain_buffered[player_index] != UINT8_C(0) &&
                 ground_smash_charge_pressed == 0 &&
                 ground_strong_attack_pressed == 0 &&
                 (light_attack_pressed == 0 ||
                  ground_light_attack_action ==
                      (uint8_t)PF_M4_ACTION_GROUND_ATTACK) &&
                 ((action_state ==
                       (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
                   displayed_frame >=
                       source_character->jab_1_combo_enable_frame) ||
                  (action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL &&
                   displayed_frame >=
                       source_character->jab_2_combo_enable_frame)))
        {
            action_state =
                action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK
                    ? (uint8_t)PF_M4_ACTION_JAB_FINAL
                    : (uint8_t)PF_M4_ACTION_JAB_THIRD;
            action_ticks = UINT16_C(0);
            scratch->jab_chain_buffered[player_index] = UINT8_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] = UINT8_C(0);
        }
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        source_character != NULL &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_DOWN_ATTACK)
    {
        /* AttackLw3_IASA buffers a fresh A edge before script frame 29 and
         * enters a new AttackLw3 immediately once cmd_var0 is enabled. */
        if (light_attack_pressed != 0)
        {
            scratch->down_tilt_repeat_buffered[player_index] = UINT8_C(1);
        }
        if (scratch->down_tilt_repeat_buffered[player_index] != UINT8_C(0) &&
            (uint32_t)action_ticks + UINT32_C(1) >=
                source_character->down_tilt_repeat_enable_frame)
        {
            action_ticks = UINT16_C(0);
            scratch->down_tilt_repeat_buffered[player_index] = UINT8_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] = UINT8_C(0);
        }
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        (jab_cancel_pressed != 0 || jab_final_pressed != 0))
    {
        action_state =
            jab_cancel_pressed != 0
                ? (uint8_t)PF_M4_ACTION_SHIELD
                : (uint8_t)PF_M4_ACTION_JAB_FINAL;
        action_ticks = UINT16_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        (spot_dodge_pressed != 0 || roll_pressed != 0) &&
        (was_shielding == 0 ||
         scratch->shield_health_f32[player_index] >
             shield_hold_depletion_f32(
                 fighter,
                 scratch->shield_strength[player_index])) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        if (was_shielding)
        {
            scratch->shield_health_f32[player_index] =
                shield_health_subtract(
                    scratch->shield_health_f32[player_index],
                    shield_hold_depletion_f32(
                        fighter,
                        scratch->shield_strength[player_index]));
        }
        if (spot_dodge_pressed != 0)
        {
            action_state = (uint8_t)PF_M4_ACTION_SPOT_DODGE;
            dash_direction = INT8_C(0);
        }
        else
        {
            action_state =
                roll_direction == facing
                    ? (uint8_t)PF_M4_ACTION_ROLL_FORWARD
                    : (uint8_t)PF_M4_ACTION_ROLL_BACKWARD;
            dash_direction = roll_direction;
        }
        action_ticks = UINT16_C(0);
        velocity_x = INT32_C(0);
        short_hop_latched = UINT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        source_ground_input != NULL &&
        action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        reference_initial_dash_ordinary_origin != 0 &&
        reference_current_anim_frame <=
            initial_dash_forward_roll_end_frame &&
        shield_held != 0 &&
        attack_pressed == 0)
    {
        /* Ordinary ftCo_Dash x4 routes held LR through ftCo_80099264 on
         * frames <= x48. Turn-origin Dash has x4 clear and reaches Guard. */
        action_state = (uint8_t)PF_M4_ACTION_ROLL_FORWARD;
        action_ticks = UINT16_C(0);
        velocity_x = INT32_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = facing;
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        shield_held != 0 &&
        grab_fallback_attack_pressed == 0 &&
        (source_ground_input == NULL || attack_pressed == 0) &&
        !action_is_shield(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          (source_ground_input == NULL ||
           (reference_initial_dash_ordinary_origin != 0 &&
            reference_current_anim_frame <=
                initial_dash_early_end_frame))) &&
        (!action_is_ground_attack(action_state) ||
         (ground_iasa_capabilities &
          PF_M4_FALCON_IASA_GUARD) != UINT8_C(0)) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        !action_is_aerial_landing(action_state) &&
        !action_locks_ground_control(action_state))
    {
        const int reference_escape_allowed =
            source_ground_input != NULL &&
            main_stick_spot_dodge_pressed != 0 &&
            reference_calls_direct_escape_n(
                callback_owner.action_state,
                ground_iasa_capabilities);

        if (reference_escape_allowed != 0)
        {
            action_state = (uint8_t)PF_M4_ACTION_SPOT_DODGE;
            dash_direction = INT8_C(0);
            velocity_x = INT32_C(0);
        }
        else
        {
            action_state = (uint8_t)PF_M4_ACTION_SHIELD;
            dash_direction = INT8_C(0);
            if (source_ground_input != NULL &&
                callback_owner.action_state ==
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH)
            {
                /* Successful Guard acquisition falls through ftCo_Dash_IASA
                 * and applies x54 before the newly installed Guard physics. */
                velocity_x = apply_initial_dash_iasa_tail(
                    source_ground_input,
                    velocity_x);
            }
            if (source_ground_input != NULL &&
                ((callback_owner.action_state ==
                         (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                     reference_current_anim_frame >
                         initial_dash_special_end_frame) ||
                 callback_owner.action_state ==
                     (uint8_t)PF_M4_ACTION_RUN))
            {
                scratch->guard_dash_grab_window_ticks[player_index] =
                    (uint8_t)
                        source_ground_input->guard_dash_grab_window_ticks;
                guard_dash_grab_window_entered_this_tick = 1;
            }
            else
            {
                scratch->guard_dash_grab_window_ticks[player_index] =
                    UINT8_C(0);
            }
        }
        action_ticks = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        !action_is_reference_special_locked(action_state) &&
        normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        !action_is_aerial_landing(action_state) &&
        (!action_is_ground_attack(action_state) ||
         ((ground_iasa_capabilities &
           PF_M4_FALCON_IASA_ATTACK) != UINT8_C(0) &&
          (ground_iasa_policy !=
               PF_M4_REFERENCE_IASA_JAB_CHAIN ||
           ground_smash_charge_pressed != 0 ||
           ground_strong_attack_pressed != 0 ||
           ground_light_attack_action !=
               (uint8_t)PF_M4_ACTION_GROUND_ATTACK))) &&
        !action_is_shield(action_state) &&
        !action_locks_ground_control(action_state) &&
        reference_initial_dash_attack_allowed != 0 &&
        attack_pressed)
    {
        uint8_t next_attack_action;

        facing = reference_turn_callback_facing_value;

        if (reference_initial_dash_dash_attack != 0 ||
            dash_attack_pressed != 0)
        {
            next_attack_action = (uint8_t)PF_M4_ACTION_DASH_ATTACK;
        }
        else if (reference_initial_dash_forward_smash != 0)
        {
            next_attack_action =
                reference_c_stick_attack_action_value ==
                        (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK
                    ? ground_strong_attack_action
                    : ground_smash_charge_action;
        }
        else if (ground_smash_charge_pressed != 0)
        {
            next_attack_action = ground_smash_charge_action;
        }
        else if (ground_strong_attack_pressed != 0)
        {
            next_attack_action = ground_strong_attack_action;
        }
        else
        {
            next_attack_action = ground_light_attack_action;
        }
        action_state = next_attack_action;
        action_ticks = UINT16_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        if (dash_attack_pressed != 0 ||
            reference_initial_dash_dash_attack != 0)
        {
            velocity_x =
                (float)facing * fighter->dash_attack_speed_f32;
        }
        if ((ground_strong_attack_pressed != 0
                 ? strong_attack_horizontal_direction
                 : horizontal_direction) != INT8_C(0) &&
            action_is_forward_ground_attack(action_state))
        {
            facing = ground_strong_attack_pressed != 0
                         ? strong_attack_horizontal_direction
                         : horizontal_direction;
        }
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        !action_is_aerial_landing(action_state) &&
        !action_is_ground_attack(action_state) &&
        !action_is_shield(action_state) &&
        !action_locks_ground_control(action_state) &&
        taunt_pressed == 0 &&
        (button_jump_pressed != 0 ||
         damage_released_jump_requested != 0 ||
         ((action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
           action_state == (uint8_t)PF_M4_ACTION_RUN_BRAKE)
              ? running_tap_jump_pressed
              : tap_jump_pressed)))
    {
        action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
        action_ticks = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        taunt_pressed != 0 &&
        (action_can_start_taunt(action_state) ||
         (action_is_ground_attack(action_state) &&
          (ground_iasa_capabilities &
           PF_M4_FALCON_IASA_TAUNT) != UINT8_C(0))))
    {
        action_state = (uint8_t)PF_M4_ACTION_TAUNT;
        action_ticks = UINT16_C(0);
        if (source_ground_input != NULL &&
            callback_owner.action_state ==
                (uint8_t)PF_M4_ACTION_INITIAL_DASH)
        {
            /* Appeal succeeds without returning from ftCo_Dash_IASA, so x54
             * is applied before Appeal's first physics callback. */
            velocity_x = apply_initial_dash_iasa_tail(
                source_ground_input,
                velocity_x);
        }
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    /* Melee's air-to-ground transition preserves self_vel.y on the entry
     * frame. The first grounded physics callback then projects self velocity
     * onto the selected floor; the compact stage domain is flat today. */
    if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_ticks == UINT16_C(0) &&
        action_is_grounded_landing(action_state))
    {
        velocity_y = INT32_C(0);
    }

    if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        const float shield_hold_depletion_f32_value =
            shield_hold_depletion_f32(
                fighter,
                scratch->shield_strength[player_index]);
        const float shield_health_before_depletion =
            scratch->shield_health_f32[player_index];
        const float depleted_shield_health =
            shield_hold_depletion_f32_value >=
                    shield_health_before_depletion
                ? shield_health_before_depletion
                : shield_hold_depletion_f32_value;
        const int shield_broken_by_depletion =
            was_shielding &&
            shield_hold_depletion_f32_value >
                shield_health_before_depletion;

        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        if (was_shielding)
        {
            scratch->shield_health_f32[player_index] =
                shield_health_subtract(
                    scratch->shield_health_f32[player_index],
                    shield_hold_depletion_f32_value);
        }
        if (shield_broken_by_depletion != 0)
        {
            enter_shield_break_launch(
                fighter,
                scratch,
                player_index,
                &velocity_x,
                &velocity_y,
                &action_ticks,
                &source_submotion,
                &grounded,
                &action_state,
                &support,
                &short_hop_latched,
                &fast_fall,
                &dash_direction);
            status = pf_sim_push_event(
                scratch,
                world->tick,
                PF_SIM_EVENT_SHIELD_BREAK,
                PF_SIM_EVENT_NO_PLAYER,
                (uint8_t)player_index,
                depleted_shield_health,
                velocity_x,
                velocity_y,
                UINT16_C(0),
                UINT16_C(0),
                NULL);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
        else if (was_shielding && shield_jump_pressed)
        {
            action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
            action_ticks = UINT16_C(0);
            short_hop_latched =
                secondary_jump_up_buffered != 0 && jump_pressed == 0
                    ? UINT8_C(2)
                    : UINT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
        }
        else if (shield_platform_drop_requested != 0)
        {
            enter_platform_pass(
                fighter,
                &position_y,
                &velocity_y,
                &action_ticks,
                &source_submotion,
                &grounded,
                &action_state,
                &support,
                &platform_drop_ticks,
                &fast_fall);
            short_hop_latched = UINT8_C(0);
            dash_direction = INT8_C(0);
            if (source_ground_input != NULL)
            {
                tilt_y_age = UINT8_C(254);
            }
            dropped_platform_this_tick = 1;
            scratch->powershield[player_index] = UINT8_C(0);
            scratch->shield_stun_ticks[player_index] =
                UINT16_C(0);
        }
        else
        {
            if (was_shielding && action_ticks <
                fighter->shield_minimum_hold_ticks)
            {
                ++action_ticks;
            }
            if (shield_held == 0 &&
                action_ticks >=
                    fighter->shield_minimum_hold_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_SHIELD_RELEASE;
                action_ticks = UINT16_C(0);
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            stationary_ground_friction(fighter, velocity_x));
        if (resumed_hitlag_motion_this_tick == 0 &&
            scratch->shield_stun_ticks[player_index] >
            UINT16_C(0))
        {
            --scratch->shield_stun_ticks[player_index];
        }
        if (scratch->shield_stun_ticks[player_index] ==
            UINT16_C(0))
        {
            if (shield_held != 0)
            {
                scratch->powershield[player_index] = UINT8_C(0);
                scratch->shield_strength[player_index] =
                    input_shield_strength_value;
                action_state = (uint8_t)PF_M4_ACTION_SHIELD;
                action_ticks =
                    fighter->shield_minimum_hold_ticks;
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_SHIELD_RELEASE;
                action_ticks = UINT16_C(0);
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            stationary_ground_friction(fighter, velocity_x));
        if (scratch->powershield[player_index] != UINT8_C(0) &&
            fighter->powershield_cancel_enabled != UINT8_C(0) &&
            action_ticks >=
                fighter->powershield_cancel_delay_ticks &&
            attack_pressed)
        {
            action_state =
                strong_attack_pressed
                    ? ground_strong_attack_action
                    : ground_light_attack_action;
            /* GuardOff's powershield attack callbacks enter the selected
             * attack at displayed frame 1 after its ordinary update branch
             * has already passed for this tick. */
            action_ticks = UINT16_C(1);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
            short_hop_latched = UINT8_C(0);
            dash_direction = INT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
            if ((strong_attack_pressed != 0
                     ? strong_attack_horizontal_direction
                     : horizontal_direction) != INT8_C(0) &&
                action_is_forward_ground_attack(action_state))
            {
                facing = strong_attack_pressed != 0
                             ? strong_attack_horizontal_direction
                             : horizontal_direction;
            }
        }
        else if (shield_release_spot_dodge_pressed != 0)
        {
            action_state = (uint8_t)PF_M4_ACTION_SPOT_DODGE;
            /* EscapeN begins at displayed frame 1 on the GuardOff IASA
             * acquisition tick. Its ordinary update branch has already run
             * earlier in this function, so commit that frame explicitly. */
            action_ticks = UINT16_C(1);
            velocity_x = INT32_C(0);
            short_hop_latched = UINT8_C(0);
            dash_direction = INT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
        }
        else if (shield_release_jump_pressed != 0)
        {
            action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
            action_ticks = UINT16_C(0);
            short_hop_latched =
                secondary_jump_up_buffered != 0 && jump_pressed == 0
                    ? UINT8_C(2)
                    : UINT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
        }
        else if (!(callback_owner.entered_this_tick != UINT8_C(0) &&
                   previous_action_state ==
                       (uint8_t)PF_M4_ACTION_SHIELD_STUN))
        {
            ++action_ticks;
            if (action_ticks >= fighter->shield_release_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                scratch->powershield[player_index] =
                    UINT8_C(0);
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN)
    {
        if (action_ticks == UINT16_C(0))
        {
            velocity_y = INT32_C(0);
        }
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= fighter->shield_break_down_ticks)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND;
            action_ticks = UINT16_C(0);
            source_submotion =
                source_submotion ==
                        (uint16_t)
                            PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN
                    ? (uint16_t)
                          PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN
                    : (uint16_t)
                          PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_UP;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= fighter->shield_break_stand_ticks)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN;
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA;
            action_ticks = shield_break_stun_ticks(
                fighter,
                scratch->damage_f32[player_index]);
            scratch->mash_stick_x_direction[player_index] = INT8_C(0);
            scratch->mash_stick_y_direction[player_index] = INT8_C(0);
            scratch->shield_health_f32[player_index] =
                fighter->shield_reset_health_f32;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        const uint32_t mash_pulses = grab_mash_pulses(
            fighter,
            raw_input,
            previous_buttons,
            dense_shield_pressed,
            scratch,
            player_index);
        uint32_t elapsed_ticks =
            (uint32_t)fighter->shield_break_stun_tick_decrement;

        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        scratch->shield_health_f32[player_index] =
            fighter->shield_reset_health_f32;
        elapsed_ticks +=
            mash_pulses *
            (uint32_t)
                fighter->shield_break_mash_reduction_ticks;
        if ((uint32_t)action_ticks <= elapsed_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->shield_health_f32[player_index] =
                fighter->shield_reset_health_f32;
        }
        else
        {
            action_ticks =
                (uint16_t)(
                    (uint32_t)action_ticks - elapsed_ticks);
        }
    }
    else if (!ledge_motion_handled &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH)
    {
        const struct reference_move *catch_move =
            falcon_reference_move(
                PF_M4_FALCON_UP_SPECIAL_CATCH);

        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        if (catch_move == NULL ||
            action_ticks > catch_move->total_frames)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        ++action_ticks;
        if (action_ticks > catch_move->total_frames)
        {
            const falcon_up_special_timing *timing =
                falcon_reference_up_special_timing();

            if (timing == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (grounded != UINT8_C(0))
            {
                position_x +=
                    (int32_t)facing *
                    timing->grounded_throw_reposition_x_f32;
                position_y +=
                    timing->grounded_throw_reposition_y_f32;
            }
            action_state =
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW;
            action_ticks = UINT16_C(0);
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            launched_this_tick = 1;
            fast_fall = UINT8_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        ((grounded != UINT8_C(0) &&
          (action_state == (uint8_t)PF_M4_ACTION_GRAB ||
           action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB ||
           action_state == (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
           action_state == (uint8_t)PF_M4_ACTION_PUMMEL ||
           action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
           action_is_throw(action_state))) ||
         action_state == (uint8_t)PF_M4_ACTION_GRABBED))
    {
        if (action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE &&
            action_ticks == UINT16_C(0))
        {
            velocity_x =
                -(float)facing * fighter->grab_release_speed_x_f32;
        }
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        velocity_y = INT32_C(0);
        if (action_state == (uint8_t)PF_M4_ACTION_GRAB ||
            action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB)
        {
            const int dash_grab =
                action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB;
            falcon_move_index reference_move_index;
            const struct reference_move *reference_move = NULL;
            const uint32_t grab_ticks =
                (uint32_t)(dash_grab != 0
                               ? fighter->dash_grab_startup_ticks
                               : fighter->grab_startup_ticks) +
                (uint32_t)(dash_grab != 0
                               ? fighter->dash_grab_active_ticks
                               : fighter->grab_active_ticks) +
                (uint32_t)(dash_grab != 0
                               ? fighter->dash_grab_recovery_ticks
                               : fighter->grab_recovery_ticks);

            if (fighter->reference_frame_data_enabled != UINT8_C(0))
            {
                if (!falcon_reference_move_for_action(
                        action_state,
                        &reference_move_index) ||
                    (reference_move = falcon_reference_move(
                         reference_move_index)) == NULL ||
                    reference_move->subaction_index !=
                        (dash_grab != 0
                             ? (uint16_t)
                                   PF_M4_FALCON_SUBMOTION_CATCH_DASH
                             : (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH) ||
                    reference_move->total_frames != grab_ticks)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
            }

            ++action_ticks;
            if (reference_move != NULL)
            {
                /* Catch begins at source counter zero. The imported
                 * total_frames value is its highest zero-based counter, so
                 * canonical one-based ticks 1..N+1 own source frames 0..N. */
                source_submotion = reference_move->subaction_index;
                source_animation_frame_f32 =
                    (int32_t)(action_ticks - UINT16_C(1)) * 1.0f;
                source_animation_rate_f32 = 1.0f;
            }
            if (reference_move != NULL
                    ? (uint32_t)action_ticks >
                          (uint32_t)reference_move->total_frames +
                              UINT32_C(1)
                    : (uint32_t)action_ticks >= grab_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRAB_HOLD)
        {
            if (fighter->reference_frame_data_enabled != UINT8_C(0))
            {
                if (source_submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH)
                {
                    const float catch_wait_frame_f32 =
                        (float)(fighter->grab_startup_ticks +
                                fighter->grab_active_ticks + UINT16_C(1));
                    const float next_frame_f32 =
                        source_animation_frame_f32 + 1.0f;

                    if (next_frame_f32 >=
                        catch_wait_frame_f32)
                    {
                        source_submotion = (uint16_t)
                            PF_M4_FALCON_SUBMOTION_CATCH_WAIT;
                        source_animation_frame_f32 = INT32_C(0);
                    }
                    else
                    {
                        source_animation_frame_f32 = next_frame_f32;
                    }
                    source_animation_rate_f32 = 1.0f;
                }
                else if (source_submotion ==
                         (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_WAIT)
                {
                    if (!falcon_advance_loop_animation_f32(
                            source_submotion,
                            source_animation_frame_f32,
                            1.0f,
                            &source_animation_frame_f32))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    source_animation_rate_f32 = 1.0f;
                }
                else
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
            }
            const uint8_t grab_action =
                light_attack_pressed != 0
                    ? (uint8_t)PF_M4_ACTION_PUMMEL
                    : grab_action_for_input(
                          fighter,
                          input,
                          input_tilt_x_age,
                          input_tilt_y_age,
                          world->previous_secondary_stick_x[player_index],
                          world->previous_secondary_stick_y[player_index],
                          facing);

            if (grab_action != (uint8_t)PF_M4_ACTION_GRAB_HOLD)
            {
                action_state = grab_action;
                action_ticks = UINT16_C(0);
                if (grab_action == (uint8_t)PF_M4_ACTION_PUMMEL)
                {
                    source_submotion =
                        (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_ATTACK;
                }
                else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
                         action_is_throw(grab_action))
                {
                    uint16_t victim_submotion;

                    if (!falcon_reference_throw_motions(
                            grab_action,
                            &source_submotion,
                            &victim_submotion,
                            &source_animation_rate_f32))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    (void)victim_submotion;
                    /* Fighter_ChangeMotionState is followed by the common
                     * animation callback in the same update. The first
                     * observable throw row therefore already owns one shared
                     * weight-scaled animation step. */
                    source_animation_frame_f32 =
                        source_animation_rate_f32;
                }
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
            else if (action_ticks < UINT16_C(600))
            {
                ++action_ticks;
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_PUMMEL)
        {
            ++action_ticks;
            if (action_ticks > fighter->pummel_total_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GRAB_HOLD;
                action_ticks = UINT16_C(0);
                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_WAIT;
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRABBED)
        {
            const uint8_t owner_slot =
                scratch->grab_owner_slot[player_index];
            int escape_locked = 0;
            int capture_wait_entered = 0;

            if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
                (source_submotion == (uint16_t)
                     PF_M4_FALCON_SUBMOTION_CAPTURE_PULLED_LOW ||
                 source_submotion == (uint16_t)
                     PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_LOW))
            {
                const falcon_submotion_data *capture_motion =
                    falcon_reference_submotion(source_submotion);

                if (capture_motion == NULL ||
                    capture_motion->animation_frame_count == UINT16_C(0))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (source_submotion == (uint16_t)
                        PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_LOW)
                {
                    if (!falcon_advance_loop_animation_f32(
                            source_submotion,
                            source_animation_frame_f32,
                            1.0f,
                            &source_animation_frame_f32))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                }
                else
                {
                    const float terminal_frame_f32 =
                        (float)(capture_motion->animation_frame_count -
                                UINT16_C(1));
                    const float next_frame_f32 =
                        source_animation_frame_f32 + 1.0f;

                    source_animation_frame_f32 =
                        next_frame_f32 < terminal_frame_f32
                            ? next_frame_f32
                            : terminal_frame_f32;
                }
                source_animation_rate_f32 = 1.0f;
            }

            if (owner_slot != UINT8_C(0))
            {
                const uint32_t owner_index =
                    (uint32_t)owner_slot - UINT32_C(1);

                if (owner_index >= (uint32_t)world->player_count)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                {
                    const uint8_t owner_action =
                        scratch->action_state[owner_index];
                    const uint8_t owner_effective_action =
                        owner_action == (uint8_t)PF_M4_ACTION_HITLAG
                            ? scratch->hitlag_resume_action[owner_index]
                            : owner_action;

                    if (source_submotion ==
                            (uint16_t)
                                PF_M4_FALCON_SUBMOTION_CAPTURE_DAMAGE_HIGH &&
                        (owner_effective_action ==
                             (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
                         (owner_action ==
                              (uint8_t)PF_M4_ACTION_PUMMEL &&
                          scratch->action_ticks[owner_index] >=
                              fighter->pummel_total_ticks &&
                          scratch->action_ticks[owner_index] ==
                              world->action_ticks[owner_index])))
                    {
                        source_submotion =
                            (uint16_t)
                                PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_HIGH;
                        action_ticks = UINT16_C(0);
                        capture_wait_entered = 1;
                    }

                    escape_locked =
                        action_is_throw(owner_effective_action) ||
                        owner_effective_action ==
                            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
                        owner_effective_action ==
                            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW;
                }
            }
            if (escape_locked == 0)
            {
                const uint32_t mash_pulses = grab_mash_pulses(
                    fighter,
                    raw_input,
                    previous_buttons,
                    dense_shield_pressed,
                    scratch,
                    player_index);
                uint32_t elapsed_ticks =
                    (uint32_t)fighter->grab_escape_tick_decrement;
                elapsed_ticks +=
                    mash_pulses *
                    (uint32_t)fighter->grab_mash_reduction_ticks;
                if ((uint32_t)scratch->grab_escape_ticks[player_index] <=
                    elapsed_ticks)
                {
                    scratch->grab_escape_ticks[player_index] = UINT16_C(0);
                }
                else
                {
                    scratch->grab_escape_ticks[player_index] =
                        (uint16_t)(
                            (uint32_t)scratch
                                ->grab_escape_ticks[player_index] -
                            elapsed_ticks);
                }
            }
            if (capture_wait_entered == 0 &&
                action_ticks < UINT16_C(600))
            {
                ++action_ticks;
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE)
        {
            const falcon_submotion_data *release_motion =
                falcon_reference_submotion(source_submotion);
            uint16_t release_ticks;

            if (release_motion == NULL ||
                (source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT &&
                 source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_CUT))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            release_ticks =
                release_motion->animation_frame_count != UINT16_C(0)
                    ? release_motion->animation_frame_count
                    : UINT16_C(1);
            ++action_ticks;
            if (action_ticks >= release_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
            }
        }
        else
        {
            const struct throw_data *throw_data =
                throw_for_action(fighter, action_state);
            const uint32_t throw_ticks =
                throw_data != NULL
                    ? (uint32_t)throw_data->release_tick +
                          (uint32_t)throw_data->recovery_ticks
                    : UINT32_C(0);

            if (throw_data == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (fighter->reference_frame_data_enabled != UINT8_C(0))
            {
                const falcon_submotion_data *throw_motion;
                uint16_t expected_holder_submotion;
                uint16_t victim_submotion;
                float expected_rate_f32;
                float terminal_frame_f32;
                float next_frame_f32;

                if (!falcon_reference_throw_motions(
                        action_state,
                        &expected_holder_submotion,
                        &victim_submotion,
                        &expected_rate_f32) ||
                    source_submotion != expected_holder_submotion ||
                    source_animation_rate_f32 != expected_rate_f32)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                (void)victim_submotion;
                throw_motion =
                    falcon_reference_submotion(source_submotion);
                if (throw_motion == NULL ||
                    throw_motion->animation_frame_count == UINT16_C(0))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                terminal_frame_f32 =
                    (float)throw_motion->animation_frame_count;
                next_frame_f32 =
                    source_animation_frame_f32 + source_animation_rate_f32;
                if (!isfinite(next_frame_f32) ||
                    next_frame_f32 >= terminal_frame_f32)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                source_animation_frame_f32 = next_frame_f32;
            }
            ++action_ticks;
            if (fighter->reference_frame_data_enabled == UINT8_C(0) &&
                (uint32_t)action_ticks >= throw_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_CHARGE_GROUND)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        if (scratch->charge_ticks[player_index] <
            content->charge.max_charge_ticks)
        {
            ++scratch->charge_ticks[player_index];
        }
        if (action_ticks < UINT16_C(600))
        {
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= content->charge.store_animation_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_SHIELD;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND)
    {
        const uint32_t release_ticks =
            (uint32_t)content->charge.release_startup_ticks +
            (uint32_t)content->charge.release_active_ticks +
            (uint32_t)content->charge.release_recovery_ticks;

        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if ((uint32_t)action_ticks >= release_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
            scratch->charge_ticks[player_index] = UINT16_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND)
    {
        const uint32_t reflector_ticks =
            (uint32_t)content->reflector.startup_ticks +
            (uint32_t)content->reflector.active_ticks +
            (uint32_t)content->reflector.recovery_ticks;

        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if ((uint32_t)action_ticks >= reflector_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND)
    {
        const struct reference_move *move =
            falcon_reference_move(
                PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND);
        float reference_motion_x_f32;

        if (move == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_ticks >= move->total_frames)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            velocity_x = INT32_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
        else
        {
            if (!falcon_reference_motion_x_f32(
                    action_state,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &reference_motion_x_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x = (float)facing * reference_motion_x_f32;
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND)
    {
        const struct reference_move *move =
            falcon_move_for_action(action_state);
        const falcon_special_attributes *attributes =
            falcon_reference_special_attributes();

        if (move == NULL || attributes == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_ticks >= move->total_frames)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND;
            action_ticks = UINT16_C(0);
            if (falcon_kick_ground_end_velocity(
                    UINT16_C(0),
                    facing,
                    scratch->falcon_kick_hit_count[player_index],
                    1,
                    &velocity_x) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
        else
        {
            const float hit_scale_f32 =
                falcon_kick_hit_velocity_scale(
                    attributes,
                    scratch->falcon_kick_hit_count[player_index]);

            if (falcon_kick_root_velocity(
                    action_state,
                    action_ticks,
                    facing,
                    0,
                    &velocity_x,
                    &velocity_y) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x =
                multiply_f32(velocity_x, hit_scale_f32);
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND)
    {
        const struct reference_move *move =
            falcon_move_for_action(action_state);

        if (move == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_ticks >= move->total_frames)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->falcon_kick_hit_count[player_index] = UINT8_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
        else
        {
            if (falcon_kick_ground_end_velocity(
                    action_ticks,
                    facing,
                    scratch->falcon_kick_hit_count[player_index],
                    0,
                    &velocity_x) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        (action_state ==
             (uint8_t)PF_M4_ACTION_FALCON_KICK_LANDING ||
         action_state ==
             (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND))
    {
        const struct reference_move *move =
            falcon_move_for_action(action_state);
        const falcon_common_attributes *common =
            falcon_reference_common_attributes();
        const falcon_common_special_attributes *common_special =
            falcon_reference_common_special_attributes();
        const falcon_special_attributes *attributes =
            falcon_reference_special_attributes();
        const falcon_down_special_timing *timing =
            falcon_reference_down_special_timing();

        if (move == NULL || common == NULL || common_special == NULL ||
            attributes == NULL || timing == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_ticks >= move->total_frames)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->falcon_kick_hit_count[player_index] = UINT8_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND)
        {
            if (falcon_kick_root_velocity(
                    action_state,
                    action_ticks,
                    facing,
                    0,
                    &velocity_x,
                    &velocity_y) != PF_STATUS_OK)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            ++action_ticks;
        }
        else
        {
            const float friction_f32 =
                action_ticks >= timing->landing_traction_begin_frame &&
                    action_ticks <= timing->landing_traction_end_frame
                    ? multiply_f32(
                          common->friction_f32,
                          attributes
                              ->speciallw_air_landing_traction_f32)
                    : falcon_source_ground_friction(
                          common,
                          common_special,
                          velocity_x);

            velocity_x =
                approach(velocity_x, INT32_C(0), friction_f32);
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND)
    {
        const struct reference_move *move =
            falcon_reference_move(
                PF_M4_FALCON_UP_SPECIAL_GROUND);
        const falcon_up_special_timing *timing =
            falcon_reference_up_special_timing();
        const uint16_t displayed_frame =
            (uint16_t)(action_ticks + UINT16_C(1));

        if (move == NULL || timing == NULL ||
            action_ticks >= move->total_frames ||
            falcon_dive_start_velocity(
                fighter,
                input,
                action_state,
                action_ticks,
                &facing,
                &velocity_x,
                &velocity_y) != PF_STATUS_OK)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        ++action_ticks;
        if (displayed_frame > timing->air_control_begin_frame)
        {
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            launched_this_tick = 1;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        (action_state ==
             (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND ||
         action_state ==
             (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND))
    {
        const falcon_move_index move_index =
            action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND
                ? PF_M4_FALCON_SIDE_SPECIAL_START_GROUND
                : PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND;
        const struct reference_move *move =
            falcon_reference_move(move_index);
        float reference_motion_x_f32;

        if (move == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_ticks >= move->total_frames)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            velocity_x = INT32_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
        }
        else
        {
            if (!falcon_reference_motion_x_f32(
                    action_state,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &reference_motion_x_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x = (float)facing * reference_motion_x_f32;
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= content->projectile.fire_recovery_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        (action_state == (uint8_t)PF_M4_ACTION_ITEM_THROW ||
         action_state ==
             (uint8_t)PF_M4_ACTION_ITEM_DASH_THROW))
    {
        const uint16_t recovery_ticks =
            action_state == (uint8_t)PF_M4_ACTION_ITEM_DASH_THROW
                ? content->item.dash_throw_recovery_ticks
                : content->item.throw_recovery_ticks;

        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= recovery_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        (action_state == (uint8_t)PF_M4_ACTION_REBOUND_STOP ||
         action_state == (uint8_t)PF_M4_ACTION_REBOUND))
    {
        const int entered_rebound =
            action_state == (uint8_t)PF_M4_ACTION_REBOUND_STOP;
        const falcon_submotion_data *rebound_motion =
            falcon_reference_submotion(
                (uint16_t)PF_M4_FALCON_SUBMOTION_REBOUND);

        if (rebound_motion == NULL ||
            rebound_motion->animation_frame_count == UINT16_C(0) ||
            scratch->rebound_duration_ticks[player_index] == UINT16_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_state == (uint8_t)PF_M4_ACTION_REBOUND_STOP)
        {
            action_state = (uint8_t)PF_M4_ACTION_REBOUND;
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_REBOUND;
        }
        if (entered_rebound == 0)
        {
            velocity_x = approach(
                velocity_x,
                INT32_C(0),
                fighter->traction_f32);
        }
        ++action_ticks;
        if (action_ticks >=
            scratch->rebound_duration_ticks[player_index])
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
            scratch->rebound_duration_ticks[player_index] = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_TAUNT)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            stationary_ground_friction(fighter, velocity_x));
        ++action_ticks;
        if (action_ticks >= fighter->taunt_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        (grounded != UINT8_C(0) ||
         action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN) &&
        action_locks_ground_control(action_state) &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING)
    {
        int8_t prone_roll_direction = INT8_C(0);
        const prone_option selected_prone_option =
            action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                    action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT
                ? select_prone_option(
                      fighter,
                      input,
                      world->previous_directional_input_flags[player_index],
                      scratch->prone_attack_input_age[player_index],
                      shield_pressed,
                      &prone_roll_direction)
                : PF_M4_PRONE_OPTION_NONE;

        velocity_x = INT32_C(0);
        if (action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
        {
            ++action_ticks;
            if (action_ticks >= fighter->reset_bound_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_FORCED_GETUP;
                action_ticks = UINT16_C(0);
                scratch->hitstun_ticks[player_index] = UINT16_C(0);
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_FORCED_GETUP)
        {
            ++action_ticks;
            if (action_ticks >= fighter->reset_forced_getup_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                scratch->prone_orientation[player_index] =
                    (uint8_t)PF_M4_PRONE_NONE;
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN)
        {
            ++action_ticks;
            if (action_ticks >= fighter->knockdown_ticks)
            {
                if (selected_prone_option == PF_M4_PRONE_OPTION_NONE)
                {
                    action_state = (uint8_t)PF_M4_ACTION_DOWN_WAIT;
                    action_ticks = UINT16_C(0);
                }
                else
                {
                    const pf_status prone_status =
                        enter_prone_option(
                        selected_prone_option,
                        prone_roll_direction,
                        scratch->prone_orientation[player_index],
                        1,
                        facing,
                        &velocity_x,
                        &action_state,
                        &action_ticks,
                        scratch,
                        player_index);
                    if (prone_status != PF_STATUS_OK)
                    {
                        return prone_status;
                    }
                }
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT)
        {
            if (selected_prone_option != PF_M4_PRONE_OPTION_NONE)
            {
                const pf_status prone_status =
                    enter_prone_option(
                    selected_prone_option,
                    prone_roll_direction,
                    scratch->prone_orientation[player_index],
                    0,
                    facing,
                    &velocity_x,
                    &action_state,
                    &action_ticks,
                    scratch,
                    player_index);
                if (prone_status != PF_STATUS_OK)
                {
                    return prone_status;
                }
            }
            else
            {
                ++action_ticks;
                if (action_ticks >= fighter->down_wait_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL;
                    action_ticks = UINT16_C(0);
                }
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL)
        {
            const uint16_t submotion_index =
                scratch->tech_direction[player_index] == facing
                    ? (uint16_t)
                          PF_M4_FALCON_SUBMOTION_TECH_ROLL_FORWARD
                    : (uint16_t)
                          PF_M4_FALCON_SUBMOTION_TECH_ROLL_BACKWARD;
            float translation_x_f32;

            if (falcon_reference_translation_f32(
                    submotion_index,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &translation_x_f32,
                    NULL))
            {
                velocity_x = (float)facing * translation_x_f32;
            }
            else
            {
                velocity_x = INT32_C(0);
            }
            ++action_ticks;
            if (action_ticks >= fighter->tech_roll_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                velocity_x = INT32_C(0);
                scratch->tech_direction[player_index] =
                    INT8_C(0);
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_TECH_IN_PLACE)
        {
            ++action_ticks;
            if (action_ticks >= fighter->tech_in_place_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                scratch->prone_orientation[player_index] =
                    (uint8_t)PF_M4_PRONE_NONE;
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL)
        {
            ++action_ticks;
            if (action_ticks >= fighter->getup_neutral_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                scratch->prone_orientation[player_index] =
                    (uint8_t)PF_M4_PRONE_NONE;
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL)
        {
            const uint16_t submotion_index =
                getup_roll_submotion_for(
                    scratch->prone_roll_motion_orientation[player_index],
                    scratch->tech_direction[player_index],
                    facing);
            float translation_x_f32;

            ++action_ticks;
            if (action_ticks >= fighter->getup_roll_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                velocity_x = INT32_C(0);
                scratch->tech_direction[player_index] =
                    INT8_C(0);
                scratch->prone_orientation[player_index] =
                    (uint8_t)PF_M4_PRONE_NONE;
                scratch->prone_roll_motion_orientation[player_index] =
                    (uint8_t)PF_M4_PRONE_NONE;
            }
            else if (submotion_index == UINT16_MAX ||
                     !falcon_reference_translation_f32(
                         submotion_index,
                         (uint16_t)(action_ticks + UINT16_C(1)),
                         &translation_x_f32,
                         NULL))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            else
            {
                velocity_x = (float)facing * translation_x_f32;
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
            action_state == (uint8_t)PF_M4_ACTION_ROLL_BACKWARD)
        {
            const int forward =
                action_state ==
                (uint8_t)PF_M4_ACTION_ROLL_FORWARD;
            const uint16_t total_ticks =
                forward != 0
                    ? fighter->forward_roll_ticks
                    : fighter->backward_roll_ticks;
            const int8_t direction = dash_direction != INT8_C(0)
                                         ? dash_direction
                                         : (forward != 0 ? facing
                                                         : (int8_t)-facing);
            const int8_t source_facing =
                forward != 0 ? direction : (int8_t)-direction;
            const uint16_t submotion_index =
                forward != 0
                    ? (uint16_t)PF_M4_FALCON_SUBMOTION_ROLL_FORWARD
                    : (uint16_t)PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD;
            float translation_x_f32;

            if (falcon_reference_translation_f32(
                    submotion_index,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &translation_x_f32,
                    NULL))
            {
                velocity_x =
                    (float)source_facing * translation_x_f32;
            }
            else
            {
                velocity_x = INT32_C(0);
            }
            if (forward != 0 && action_ticks == UINT16_C(19))
            {
                facing = (int8_t)-direction;
            }
            ++action_ticks;
            if (action_ticks > total_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                velocity_x = INT32_C(0);
                dash_direction = INT8_C(0);
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_SPOT_DODGE)
        {
            ++action_ticks;
            if (action_ticks > fighter->spot_dodge_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
            }
        }
        else
        {
            ++action_ticks;
            if (action_ticks >= fighter->getup_attack_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] =
                    UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                scratch->prone_orientation[player_index] =
                    (uint8_t)PF_M4_PRONE_NONE;
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_is_ground_attack(action_state))
    {
        uint32_t attack_ticks = UINT32_C(0);

        if (action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_START ||
            action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP ||
            action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_END)
        {
            falcon_move_index move_index;
            const struct reference_move *move;
            float reference_motion_x_f32;

            if (!falcon_reference_move_for_action(
                    action_state,
                    &move_index) ||
                (move = falcon_reference_move(move_index)) == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (action_ticks >= move->total_frames)
            {
                if (action_state ==
                    (uint8_t)PF_M4_ACTION_RAPID_JAB_START)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                }
                else if (action_state ==
                         (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP)
                {
                    /* Attack100Loop restarts with SkipAttackCount: targets
                     * may be hit by the next loop, but the stale queue entry
                     * remains the single Attack100 move. */
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                }
                else
                {
                    action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                    scratch->rapid_jab_input_count[player_index] =
                        UINT8_C(0);
                    scratch->rapid_jab_continue[player_index] = UINT8_C(0);
                }
            }
            else
            {
                const uint16_t next_frame =
                    (uint16_t)(action_ticks + UINT16_C(1));

                if (action_state ==
                        (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP &&
                    falcon_reference_effective_hit_frame(
                        move_index,
                        next_frame) == UINT16_C(4))
                {
                    /* Every source script hitbox-create window is a fresh
                     * collision epoch, including the five windows within one
                     * Attack100Loop animation. */
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                }
                if (falcon_reference_motion_x_f32(
                        action_state,
                        next_frame,
                        &reference_motion_x_f32))
                {
                    velocity_x =
                        (float)facing * reference_motion_x_f32;
                }
                else
                {
                    velocity_x = INT32_C(0);
                }
                action_ticks = next_frame;
            }
        }
        else if (action_is_smash_charge(action_state))
        {
            const uint8_t release_action =
                smash_release_action(action_state);
            uint16_t source_charge_frame = UINT16_C(0);

            if (fighter->reference_frame_data_enabled != UINT8_C(0))
            {
                falcon_move_index move_index;
                const struct reference_move *move;

                if (!falcon_reference_move_for_action(
                        release_action,
                        &move_index) ||
                    (move = falcon_reference_move(move_index)) ==
                        NULL ||
                    move->charge_frame == UINT16_C(0))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                source_charge_frame = move->charge_frame;
            }

            if (source_charge_frame != UINT16_C(0) &&
                action_ticks < source_charge_frame)
            {
                float reference_motion_x_f32;

                if (falcon_reference_ground_physics_for_action(
                        release_action) ==
                        PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION &&
                    falcon_reference_motion_x_f32(
                        release_action,
                        (uint16_t)(action_ticks + UINT16_C(1)),
                        &reference_motion_x_f32))
                {
                    velocity_x =
                        (float)facing * reference_motion_x_f32;
                }
                else
                {
                    velocity_x = approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->traction_f32);
                }
                ++action_ticks;
            }
            else
            {
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
                if (light_attack_held != 0 &&
                    scratch->smash_charge_ticks[player_index] <
                        fighter->smash_charge_max_ticks)
                {
                    ++scratch->smash_charge_ticks[player_index];
                    if (source_charge_frame == UINT16_C(0))
                    {
                        action_ticks =
                            scratch->smash_charge_ticks[player_index];
                    }
                }
            }
            if ((source_charge_frame == UINT16_C(0) ||
                 action_ticks >= source_charge_frame) &&
                (light_attack_held == 0 ||
                 scratch->smash_charge_ticks[player_index] >=
                     fighter->smash_charge_max_ticks))
            {
                const attack_data *attack;

                action_state = release_action;
                action_ticks = source_charge_frame != UINT16_C(0)
                                   ? (uint16_t)(
                                         source_charge_frame - UINT16_C(1))
                                   : UINT16_C(0);
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                attack = directional_ground_data(
                    fighter,
                    action_state);
                if (attack != NULL)
                {
                    attack_ticks =
                        (uint32_t)attack->startup_ticks +
                        (uint32_t)attack->active_ticks +
                        (uint32_t)attack->recovery_ticks;
                }
            }
        }
        else
        {
            const reference_timing timing =
                ground_attack_timing(fighter, action_state);

            attack_ticks =
                (uint32_t)timing.startup_ticks +
                (uint32_t)timing.active_ticks +
                (uint32_t)timing.recovery_ticks;
        }

        if (attack_ticks != UINT32_C(0))
        {
            const int reference_match =
                falcon_ground_reference_matches(
                    fighter,
                    action_state);
            float reference_motion_x_f32;

            if (reference_match != 0 &&
                falcon_reference_ground_physics_for_action(
                    action_state) ==
                    PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION &&
                falcon_reference_motion_x_f32(
                    action_state,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &reference_motion_x_f32))
            {
                velocity_x =
                    (float)facing * reference_motion_x_f32;
            }
            else
            {
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
            }
            ++action_ticks;
            /* Imported move.total_frames is the last displayed animation
             * frame.  Entries whose source counter begins at zero (such as
             * Jab 2) also begin with canonical action_ticks == 0, so the same
             * strict terminal test preserves both counter conventions. */
            if (reference_match != 0
                    ? (uint32_t)action_ticks > attack_ticks
                    : (uint32_t)action_ticks >= attack_ticks)
            {
                if (action_state ==
                    (uint8_t)PF_M4_ACTION_DOWN_ATTACK)
                {
                    const int crouch_held =
                        input->main_stick_y >=
                        (int16_t)fighter->crouch_release_axis_threshold;

                    action_state = crouch_held
                        ? (uint8_t)PF_M4_ACTION_CROUCH
                        : (uint8_t)PF_M4_ACTION_CROUCH_END;
                    action_ticks = crouch_held
                        ? UINT16_C(0)
                        : UINT16_C(1);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    action_ticks = UINT16_C(0);
                }
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                scratch->smash_charge_ticks[player_index] = UINT16_C(0);
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        if (action_ticks + UINT16_C(1) < fighter->jump_squat_ticks)
        {
            velocity_x = approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_f32 ||
                        velocity_x < -fighter->walk_speed_f32
                    ? fighter->turn_acceleration_f32
                    : fighter->traction_f32);
        }
        if (short_hop_latched != UINT8_C(2) &&
            (input->buttons & PF_INPUT_BUTTON_JUMP) == UINT64_C(0) &&
            secondary_jump_up_held == 0 &&
            main_jump_up_held == 0)
        {
            short_hop_latched = UINT8_C(1);
        }
        if (world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
        {
            ++action_ticks;
        }
        if (action_ticks >= fighter->jump_squat_ticks)
        {
            enter_ground_jump(
                fighter,
                world->previous_main_stick_x[player_index],
                short_hop_latched,
                facing,
                &velocity_x,
                &velocity_y,
                &action_ticks,
                &source_submotion,
                &grounded,
                &action_state,
                &support,
                &short_hop_latched,
                &fast_fall,
                &tilt_y_age);
            launched_this_tick = 1;
            ground_jump_entry_this_tick = 1;
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             fighter->reference_frame_data_enabled != UINT8_C(0) &&
             action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
    {
        /* An airborne Damage action can cross the floor below x1E4 without
         * changing motion state. Its ground physics is ordinary friction;
         * Damage_Anim remains authoritative for the terminal transition. */
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_f32);
        status = advance_retained_damage_animation(
            source_submotion,
            grounded,
            &action_state,
            &action_ticks,
            scratch->hitstun_ticks[player_index],
            &source_submotion);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    else if (!ledge_motion_handled &&
             !hitstun_locked &&
             grounded != UINT8_C(0) &&
             action_state == (uint8_t)PF_M4_ACTION_LANDING)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_f32 ||
                    velocity_x < -fighter->walk_speed_f32
                ? fighter->turn_acceleration_f32
                : fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= fighter->landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_is_aerial_landing(action_state))
    {
        uint16_t landing_ticks = aerial_landing_ticks(
            fighter,
            action_state);
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            stationary_ground_friction(fighter, velocity_x));
        ++action_ticks;
        if (action_ticks >= landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             (action_state ==
                  (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
              action_state ==
                  (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT))
    {
        const falcon_special_attributes *attributes =
            falcon_reference_special_attributes();
        const float lag_f32 =
            action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT
                ? (attributes != NULL
                       ? attributes->specials_hit_landing_lag_f32
                       : INT32_C(0))
                : (attributes != NULL
                       ? attributes->specials_miss_landing_lag_f32
                       : INT32_C(0));
        const uint16_t landing_ticks = (uint16_t)lag_f32;

        if (attributes == NULL || lag_f32 <= 0.0f ||
            lag_f32 != truncf(lag_f32))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            stationary_ground_friction(fighter, velocity_x));
        ++action_ticks;
        if (action_ticks >= landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state ==
                 (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING)
    {
        const falcon_special_attributes *attributes =
            falcon_reference_special_attributes();
        const float lag_f32 =
            attributes != NULL
                ? attributes->specialhi_landing_lag_f32
                : INT32_C(0);
        const uint16_t landing_ticks = (uint16_t)lag_f32;

        if (attributes == NULL || lag_f32 <= 0.0f ||
            lag_f32 != truncf(lag_f32))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            stationary_ground_friction(fighter, velocity_x));
        ++action_ticks;
        if (action_ticks >= landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state ==
                 (uint8_t)PF_M4_ACTION_SPECIAL_LANDING)
    {
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_f32 ||
                    velocity_x < -fighter->walk_speed_f32
                ? fighter->turn_acceleration_f32
                : fighter->traction_f32);
        ++action_ticks;
        if (action_ticks >= fighter->special_landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            dash_direction = INT8_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state ==
                 (uint8_t)PF_M4_ACTION_CROUCH_STEP)
    {
        velocity_x = INT32_C(0);
        ++action_ticks;
        if (action_ticks >= fighter->crouch_step_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_CROUCH;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state ==
                 (uint8_t)PF_M4_ACTION_CROUCH_START)
    {
        int enter_platform_pass_value = 0;

        if (source_ground_input != NULL)
        {
            uint8_t *const pending_ticks =
                &scratch->crouch_pass_pending_ticks[player_index];

            /* ftCo_80099F9C latches mv.co.pass.x0 and loads x470.  On later
             * Squat IASA updates the inline tail decrements x4 and enters
             * Pass exactly when it reaches zero on a pass-through line.
             * Encoding x0 as value 1 keeps the expired latch from rearming. */
            if (*pending_ticks > UINT8_C(1))
            {
                --*pending_ticks;
                if (*pending_ticks == UINT8_C(1) &&
                    surface_is_pass_through(content, support) != 0)
                {
                    enter_platform_pass_value = 1;
                }
            }
            else if (*pending_ticks == UINT8_C(0) &&
                     surface_is_pass_through(content, support) != 0 &&
                     input->main_stick_y >=
                         (int16_t)source_ground_input
                             ->platform_drop_axis_threshold &&
                     input_tilt_y_age <
                         source_ground_input
                             ->platform_drop_tilt_window_ticks)
            {
                *pending_ticks = (uint8_t)(
                    source_ground_input->crouch_pass_delay_ticks +
                    UINT16_C(1));
            }
        }
        else if (surface_is_pass_through(content, support) != 0 &&
                 input->main_stick_y >=
                     (int16_t)fighter->crouch_axis_threshold &&
                 action_ticks >= fighter->platform_drop_startup_ticks)
        {
            enter_platform_pass_value = 1;
        }

        if (enter_platform_pass_value != 0)
        {
            enter_platform_pass(
                fighter,
                &position_y,
                &velocity_y,
                &action_ticks,
                &source_submotion,
                &grounded,
                &action_state,
                &support,
                &platform_drop_ticks,
                &fast_fall);
            if (source_ground_input != NULL)
            {
                tilt_y_age = UINT8_C(254);
                scratch->crouch_pass_pending_ticks[player_index] =
                    UINT8_C(0);
            }
            dropped_platform_this_tick = 1;
        }
        else
        {
            velocity_x = approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_f32 ||
                        velocity_x < -fighter->walk_speed_f32
                    ? fighter->turn_acceleration_f32
                    : fighter->traction_f32);
            if (action_ticks >= fighter->crouch_start_ticks)
            {
                action_state =
                    input->main_stick_y <
                            (int16_t)fighter->crouch_release_axis_threshold
                        ? (uint8_t)PF_M4_ACTION_CROUCH_END
                        : (uint8_t)PF_M4_ACTION_CROUCH;
                action_ticks = UINT16_C(1);
            }
            else
            {
                ++action_ticks;
            }
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state == (uint8_t)PF_M4_ACTION_CROUCH)
    {
        const int crouch_platform_drop_requested =
            source_ground_input != NULL &&
            surface_is_pass_through(content, support) != 0 &&
            input->main_stick_y >=
                (int16_t)source_ground_input
                    ->platform_drop_axis_threshold &&
            input_tilt_y_age <
                source_ground_input->platform_drop_tilt_window_ticks;
        const uint16_t crouch_release_axis_threshold =
            ucf084_enabled != 0 &&
                    input_tilt_x_age == UINT8_C(0) &&
                    ucf084_adjusted_radial_qualifies(
                        input->main_stick_x,
                        input->main_stick_y)
                ? PF_M4_UCF084_DBOOC_RELEASE_AXIS_THRESHOLD
                : fighter->crouch_release_axis_threshold;
        const int crouch_dash_requested =
            strong_direction_value != INT8_C(0) &&
            tilt_x_age < fighter->dash_input_window_ticks;

        /* ftCo_SquatWait_IASA checks Pass before Dash before SquatRv. */
        if (crouch_platform_drop_requested != 0)
        {
            enter_platform_pass(
                fighter,
                &position_y,
                &velocity_y,
                &action_ticks,
                &source_submotion,
                &grounded,
                &action_state,
                &support,
                &platform_drop_ticks,
                &fast_fall);
            tilt_y_age = UINT8_C(254);
            dropped_platform_this_tick = 1;
        }
        else if (crouch_dash_requested != 0)
        {
            action_ticks = UINT16_C(1);
            dash_direction = strong_direction_value;
            if (strong_direction_value == facing)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                velocity_x = enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    strong_direction_value);
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
            }
        }
        else if (input->main_stick_y <
            (int16_t)crouch_release_axis_threshold)
        {
            velocity_x = approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_f32 ||
                        velocity_x < -fighter->walk_speed_f32
                    ? fighter->turn_acceleration_f32
                    : fighter->traction_f32);
            action_state = (uint8_t)PF_M4_ACTION_CROUCH_END;
            action_ticks = UINT16_C(1);
        }
        else
        {
            velocity_x = approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_f32 ||
                        velocity_x < -fighter->walk_speed_f32
                    ? fighter->turn_acceleration_f32
                    : fighter->traction_f32);
            if (action_ticks < UINT16_C(600))
            {
                ++action_ticks;
            }
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state ==
                 (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        if (ground_horizontal_direction == facing &&
            horizontal_magnitude >= fighter->walk_axis_threshold)
        {
            action_state = (uint8_t)PF_M4_ACTION_WALK;
            action_ticks = UINT16_C(1);
            dash_direction = INT8_C(0);
            velocity_x = apply_ground_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                fighter->walk_speed_f32,
                1);
        }
        else
        {
            velocity_x = approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_f32 ||
                        velocity_x < -fighter->walk_speed_f32
                    ? fighter->turn_acceleration_f32
                    : fighter->traction_f32);
            if (action_ticks >= fighter->crouch_end_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
            }
            else
            {
                ++action_ticks;
            }
        }
    }
    else if (!ledge_motion_handled &&
             !hitstun_locked &&
             grounded != UINT8_C(0) &&
             action_state !=
                 (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
             action_state !=
                 (uint8_t)PF_M4_ACTION_STANDING_TURN &&
             action_state !=
                 (uint8_t)PF_M4_ACTION_CROUCH_START &&
             action_state !=
                 (uint8_t)PF_M4_ACTION_CROUCH &&
             action_state !=
                 (uint8_t)PF_M4_ACTION_CROUCH_END &&
             !(source_ground_input != NULL &&
               action_state == (uint8_t)PF_M4_ACTION_RUN) &&
             !(source_ground_input != NULL &&
               action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH) &&
             input->main_stick_y >=
                 (int16_t)fighter->crouch_axis_threshold)
    {
        action_state = (uint8_t)PF_M4_ACTION_CROUCH_START;
        action_ticks = UINT16_C(1);
        velocity_x = approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_f32 ||
                    velocity_x < -fighter->walk_speed_f32
                ? fighter->turn_acceleration_f32
                : fighter->traction_f32);
        dash_direction = INT8_C(0);
    }
    else if (!ledge_motion_handled &&
             !hitstun_locked &&
             grounded != UINT8_C(0))
    {
        const int run_turnaround_requested =
            horizontal_direction == -facing &&
            horizontal_magnitude >=
                fighter->run_turnaround_axis_threshold;
        const int run_continues =
            horizontal_direction == facing &&
            horizontal_magnitude >=
                fighter->run_continue_axis_threshold;
        const int fresh_dash_input =
            strong_direction_value != INT8_C(0) &&
            tilt_x_age < fighter->dash_input_window_ticks;

        if (action_state == (uint8_t)PF_M4_ACTION_TEETER)
        {
            const falcon_submotion_data *teeter_motion =
                falcon_reference_submotion(source_submotion);

            if (teeter_motion == NULL ||
                (source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER &&
                 source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER_WAIT) ||
                teeter_motion->animation_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x = INT32_C(0);
            if (fresh_dash_input != 0)
            {
                if (strong_direction_value == facing)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                    action_ticks = UINT16_C(1);
                    dash_direction = strong_direction_value;
                    initial_dash_entered_this_tick = 1;
                    initial_dash_entry_motion_velocity_x = velocity_x;
                    velocity_x = enter_initial_dash_velocity(
                        fighter,
                        velocity_x,
                        strong_direction_value);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_STANDING_TURN;
                    action_ticks = UINT16_C(0);
                    dash_direction = strong_direction_value;
                }
            }
            else if (ground_horizontal_direction == -facing &&
                     horizontal_magnitude >=
                         fighter->teeter_turn_axis_threshold)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                action_ticks = UINT16_C(0);
                dash_direction =
                    (int8_t)(ground_horizontal_direction * INT8_C(2));
            }
            else if (ground_horizontal_direction == facing &&
                     horizontal_magnitude >=
                         fighter->teeter_walk_axis_threshold)
            {
                action_state = (uint8_t)PF_M4_ACTION_WALK;
                action_ticks = UINT16_C(1);
                dash_direction = INT8_C(0);
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->walk_speed_f32,
                    1);
            }
            else if ((uint32_t)action_ticks + UINT32_C(1) >=
                     (uint32_t)teeter_motion->animation_frame_count)
            {
                if (source_submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER)
                {
                    source_submotion =
                        (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER_WAIT;
                }
                action_ticks = UINT16_C(0);
            }
            else
            {
                ++action_ticks;
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_STANDING_TURN)
        {
            const int smash_turn =
                dash_direction == -PF_M4_STANDING_TURN_SMASH_PHASE ||
                dash_direction == PF_M4_STANDING_TURN_SMASH_PHASE;
            const int8_t target_direction =
                dash_direction < INT8_C(0)
                    ? INT8_C(-1)
                    : INT8_C(1);
            const int physical_facing_flip =
                (dash_direction ==
                     (int8_t)(target_direction *
                              PF_M4_STANDING_TURN_BASIC_PHASE) ||
                 dash_direction ==
                     (int8_t)(target_direction *
                              PF_M4_STANDING_TURN_DASH_ARMED_PHASE)) &&
                action_ticks + UINT16_C(1) >=
                    fighter->standing_turn_facing_tick;
            int dash_armed =
                dash_direction ==
                    (int8_t)(target_direction *
                             PF_M4_STANDING_TURN_DASH_ARMED_PHASE);
            const int target_held =
                strong_direction_value == target_direction;
            const int ucf_dashback =
                ucf084_enabled != 0 &&
                smash_turn == 0 &&
                action_ticks == UINT16_C(1) &&
                target_held != 0 &&
                input_tilt_x_age < UINT8_C(2) &&
                ucf_raw_delta_x * ucf_raw_delta_x > 75 * 75;

            if (ucf_dashback != 0)
            {
                facing = target_direction;
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                action_state = (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                action_ticks = UINT16_C(1);
                dash_direction =
                    (int8_t)(target_direction *
                             PF_M4_INITIAL_DASH_TURN_PHASE);
                velocity_x = enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    target_direction);
            }

            /* ftCo_Turn_IASA's fn_800C9C2C retains mv.co.turn.x8 when a
             * fresh dash-threshold input arrives during a basic turn. It is
             * consumed only on the physical facing-flip update. Magnitude 3
             * encodes that one bit in the existing signed phase byte. */
            if (ucf_dashback == 0 &&
                source_ground_input != NULL &&
                dash_direction ==
                    (int8_t)(target_direction *
                             PF_M4_STANDING_TURN_BASIC_PHASE) &&
                fresh_dash_input != 0 &&
                target_held != 0)
            {
                dash_armed = 1;
                dash_direction =
                    (int8_t)(target_direction *
                             PF_M4_STANDING_TURN_DASH_ARMED_PHASE);
            }

            if (ucf_dashback != 0)
            {
                /* The UCF hook has already forced just_turned and the source
                 * Dash callback above. */
            }
            else if (smash_turn != 0)
            {
                facing = target_direction;
            }
            else if (physical_facing_flip != 0)
            {
                facing = target_direction;
            }
            if (ucf_dashback == 0 &&
                (smash_turn != 0 ||
                 (physical_facing_flip != 0 && dash_armed != 0)) &&
                target_held != 0)
            {
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                action_state =
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                action_ticks = UINT16_C(1);
                dash_direction =
                    (int8_t)(target_direction *
                             PF_M4_INITIAL_DASH_TURN_PHASE);
                velocity_x = enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    target_direction);
            }
            else if (ucf_dashback == 0)
            {
                if (smash_turn != 0 || physical_facing_flip != 0)
                {
                    /* just_turned is a one-update source flag. A smash Turn
                     * exposes it on the first callback update; a basic Turn
                     * exposes it on the physical flip only when x8 was
                     * armed. Once that update rejects Dash, neither path may
                     * retry merely because the target direction is held on
                     * a later animation frame. */
                    dash_direction =
                        (int8_t)(target_direction *
                                 PF_M4_STANDING_TURN_TURNED_PHASE);
                }
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
                ++action_ticks;
                if (action_ticks >= fighter->standing_turn_ticks)
                {
                    dash_direction = INT8_C(0);
                    if (ground_horizontal_direction == facing &&
                        horizontal_magnitude >= fighter->walk_axis_threshold)
                    {
                        action_state = (uint8_t)PF_M4_ACTION_WALK;
                        action_ticks = UINT16_C(1);
                        velocity_x = apply_ground_input(
                            fighter,
                            velocity_x,
                            input->main_stick_x,
                            fighter->walk_speed_f32,
                            1);
                    }
                    else
                    {
                        action_state =
                            (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                        action_ticks = UINT16_C(0);
                    }
                }
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_INITIAL_DASH)
        {
            const int8_t initial_dash_direction =
                signed_phase_direction(dash_direction);

            if (fresh_dash_input != 0 &&
                strong_direction_value == -initial_dash_direction &&
                (source_ground_input == NULL ||
                 reference_initial_dash_turn_origin != 0 ||
                 reference_current_anim_frame >
                     initial_dash_early_end_frame))
            {
                dash_direction = strong_direction_value;
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                action_ticks = UINT16_C(1);
                velocity_x = approach(
                    multiply_f32(
                        velocity_x,
                        1.0f / INT32_C(4)),
                    INT32_C(0),
                    fighter->traction_f32);
            }
            else if (source_ground_input != NULL &&
                     fresh_dash_input != 0 &&
                     strong_direction_value == initial_dash_direction &&
                     action_ticks > initial_dash_special_end_frame)
            {
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                action_ticks = UINT16_C(1);
                velocity_x = enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    strong_direction_value);
            }
            else
            {
                const float velocity_before_ground_input = velocity_x;

                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    0);
                ++action_ticks;
                if (run_continues != 0 &&
                    action_ticks >=
                        fighter->dash_run_transition_ticks)
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
                    action_ticks =
                        fighter->run_turnaround_lockout_ticks;
                    dash_direction = INT8_C(0);
                }
                else if (action_ticks >= fighter->initial_dash_ticks)
                {
                    dash_direction = INT8_C(0);
                    action_ticks = UINT16_C(0);
                    action_state =
                        horizontal_magnitude >=
                                fighter->walk_axis_threshold
                            ? (uint8_t)PF_M4_ACTION_WALK
                            : (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    if (action_state ==
                            (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                        (velocity_before_ground_input >
                             fighter->walk_speed_f32 ||
                         velocity_before_ground_input <
                             -fighter->walk_speed_f32))
                    {
                        /*
                         * Dash physics already applied one traction step.
                         * Wait selects its stronger friction from the velocity
                         * entering this frame, then applies the second step.
                         */
                        velocity_x = approach(
                            velocity_x,
                            INT32_C(0),
                            fighter->traction_f32);
                    }
                }
            }
        }
        else if (action_state ==
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND)
        {
            const int8_t target_direction = dash_direction;
            const int target_held =
                horizontal_direction == target_direction &&
                horizontal_magnitude >=
                    fighter->run_continue_axis_threshold;

            if (action_ticks >= fighter->run_turnaround_ticks)
            {
                dash_direction = INT8_C(0);
                action_ticks = UINT16_C(0);
                if (target_held)
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
                    action_ticks = UINT16_C(1);
                    velocity_x = apply_ground_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->run_speed_f32,
                        2);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    velocity_x = approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->traction_f32);
                }
            }
            else
            {
                /*
                 * TurnRun freezes on displayed frame 9 until the old-facing
                 * ground velocity crosses the common 0.01 threshold. The
                 * facing flip occurs one physics tick after the crossing.
                 */
                if (facing != target_direction &&
                    (int64_t)velocity_x * (int64_t)facing <=
                        INT64_C(68))
                {
                    facing = target_direction;
                }
                else if (facing == target_direction ||
                         action_ticks < UINT16_C(10))
                {
                    ++action_ticks;
                }
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_RUN_BRAKE)
        {
            if (run_turnaround_requested)
            {
                /*
                 * RunBrake's animation command enables TurnRun while
                 * preserving the current animation cursor. Internal TurnRun
                 * ticks are one greater than its displayed frame, so advance
                 * the RunBrake cursor once instead of restarting at zero.
                 */
                action_state =
                    (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
                ++action_ticks;
                dash_direction = horizontal_direction;
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    0);
            }
            else
            {
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
                ++action_ticks;
                if (action_ticks >= fighter->run_brake_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    action_ticks = UINT16_C(0);
                    dash_direction = INT8_C(0);
                }
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_RUN)
        {
            if (action_ticks <
                fighter->run_turnaround_lockout_ticks)
            {
                ++action_ticks;
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    2);
            }
            else if (run_turnaround_requested)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
                action_ticks = UINT16_C(1);
                dash_direction = horizontal_direction;
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    0);
            }
            else if (!run_continues)
            {
                action_state = (uint8_t)PF_M4_ACTION_RUN_BRAKE;
                action_ticks = UINT16_C(1);
                dash_direction = INT8_C(0);
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
            }
            else
            {
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    2);
            }
        }
        else
        {
            const int walk_direction_changed =
                action_state == (uint8_t)PF_M4_ACTION_WALK &&
                ground_horizontal_direction != facing;
            const int aged_walk_continues =
                action_state == (uint8_t)PF_M4_ACTION_WALK &&
                action_ticks >= fighter->dash_input_window_ticks &&
                walk_direction_changed == 0;
            const int dash_started =
                fresh_dash_input != 0;

            if (action_is_ground_damage(action_state) &&
                dash_started == 0 &&
                horizontal_magnitude < fighter->walk_axis_threshold)
            {
                /* Damage IASA exposes the Wait table as soon as x0 reaches
                 * zero, but the damage animation remains active when no
                 * option is selected. Its Phys callback still applies the
                 * ordinary grounded friction channel. */
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_f32);
                status = advance_ground_damage_animation(
                    &action_state,
                    &action_ticks,
                    scratch->hitstun_ticks[player_index],
                    &scratch
                         ->ground_knockback_velocity_f32[player_index]);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
            }
            else if (dash_started)
            {
                action_ticks = UINT16_C(1);
                dash_direction = strong_direction_value;
                if (strong_direction_value == facing)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                    initial_dash_entered_this_tick = 1;
                    initial_dash_entry_motion_velocity_x = velocity_x;
                    velocity_x = enter_initial_dash_velocity(
                        fighter,
                        velocity_x,
                        strong_direction_value);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_STANDING_TURN;
                    velocity_x = approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->traction_f32);
                }
            }
            else if (action_state ==
                         (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                     strong_direction_value ==
                         signed_phase_direction(dash_direction) &&
                     action_ticks <
                         fighter->dash_run_transition_ticks)
            {
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_f32,
                    0);
                ++action_ticks;
                if (action_ticks >=
                    fighter->dash_run_transition_ticks)
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
                    action_ticks =
                        fighter->run_turnaround_lockout_ticks;
                    dash_direction = INT8_C(0);
                }
            }
            else if (source_ground_input != NULL &&
                     action_state ==
                         (uint8_t)PF_M4_ACTION_WALK &&
                     ground_horizontal_direction == -facing)
            {
                /* Walk checks Dash before its generic Wait transition.  A
                 * fresh full reversal was consumed above; every remaining
                 * opposite-facing walk input reaches ft_8008A244 and enters
                 * Wait instead of starting a basic Turn. */
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                dash_direction = INT8_C(0);
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    velocity_x > fighter->walk_speed_f32 ||
                            velocity_x < -fighter->walk_speed_f32
                        ? fighter->turn_acceleration_f32
                        : fighter->traction_f32);
            }
            else if ((action_state ==
                          (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
                      action_state ==
                          (uint8_t)PF_M4_ACTION_WALK) &&
                     ground_horizontal_direction == -facing &&
                     horizontal_magnitude >=
                         fighter->teeter_turn_axis_threshold)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                action_ticks = UINT16_C(1);
                dash_direction =
                    (int8_t)(INT8_C(2) * ground_horizontal_direction);
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    velocity_x > fighter->walk_speed_f32 ||
                            velocity_x < -fighter->walk_speed_f32
                        ? fighter->turn_acceleration_f32
                        : fighter->traction_f32);
            }
            else if (horizontal_magnitude >=
                     fighter->walk_axis_threshold)
            {
                int walk;

                facing = ground_horizontal_direction;
                dash_direction = INT8_C(0);
                if (source_ground_input == NULL &&
                    strong_direction_value != INT8_C(0) &&
                    aged_walk_continues == 0)
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
                    action_ticks =
                        fighter->run_turnaround_lockout_ticks;
                    walk = 0;
                }
                else
                {
                    action_state = (uint8_t)PF_M4_ACTION_WALK;
                    if (walk_direction_changed != 0 ||
                        action_ticks == UINT16_C(0))
                    {
                        action_ticks = UINT16_C(1);
                    }
                    else if (action_ticks <
                             fighter->dash_input_window_ticks)
                    {
                        ++action_ticks;
                    }
                    walk = 1;
                }
                velocity_x = apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    walk != 0
                        ? fighter->walk_speed_f32
                        : fighter->run_speed_f32,
                    walk != 0 ? 1 : 2);
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                dash_direction = INT8_C(0);
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    velocity_x > fighter->walk_speed_f32 ||
                            velocity_x < -fighter->walk_speed_f32
                        ? fighter->turn_acceleration_f32
                        : fighter->traction_f32);
            }
        }
    }

    if (!ledge_motion_handled &&
        grounded == UINT8_C(0) &&
        action_is_surface_tech(action_state))
    {
        dash_direction = INT8_C(0);
        ++action_ticks;
        if (action_is_wall_tech(action_state))
        {
            const uint16_t action_duration =
                action_state ==
                        (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP
                    ? fighter->wall_tech_jump_ticks
                    : fighter->wall_tech_ticks;

            if (action_ticks < fighter->wall_tech_stall_ticks)
            {
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
            }
            else if (action_ticks == fighter->wall_tech_stall_ticks)
            {
                if (action_state ==
                    (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP)
                {
                    velocity_x =
                        (int32_t)scratch
                            ->tech_direction[player_index] *
                        fighter->wall_tech_jump_speed_x_f32;
                    velocity_y =
                        -fighter->wall_tech_jump_speed_y_f32;
                }
                else
                {
                    velocity_x =
                        (int32_t)scratch
                            ->tech_direction[player_index] *
                        fighter->wall_tech_speed_f32;
                    velocity_y = INT32_C(0);
                }
            }
            if (action_ticks >= fighter->wall_tech_stall_ticks)
            {
                velocity_x = approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->air_friction_f32);
            }
            if (action_ticks >= action_duration)
            {
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
                scratch->tech_direction[player_index] = INT8_C(0);
            }
        }
        else
        {
            if (action_ticks == fighter->ceiling_tech_control_tick)
            {
                velocity_x = scale_axis_f32(
                    input->main_stick_x,
                    fighter->ceiling_tech_speed_f32);
            }
            velocity_x = apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                fighter->air_speed_f32);
            if (action_ticks >= fighter->ceiling_tech_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
                scratch->hitstun_ticks[player_index] = UINT16_C(0);
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded == UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_KNOCKDOWN)
    {
        dash_direction = INT8_C(0);
        if (hitstun_locked)
        {
            if (action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
            {
                ++action_ticks;
            }
            else if (action_is_surface_bounce(action_state))
            {
                ++action_ticks;
            }
            else
            {
                if (action_state == (uint8_t)PF_M4_ACTION_HITSTUN &&
                    action_ticks < UINT16_MAX)
                {
                    ++action_ticks;
                }
                else if (action_state != (uint8_t)PF_M4_ACTION_HITSTUN)
                {
                    action_ticks = UINT16_C(0);
                }
                action_state = (uint8_t)PF_M4_ACTION_HITSTUN;
            }
        }
        else if (
            action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            const falcon_submotion_data *shield_break_fly =
                falcon_reference_submotion(source_submotion);

            if (source_submotion !=
                    (uint16_t)
                        PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_FLY ||
                shield_break_fly == NULL ||
                shield_break_fly->animation_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x = INT32_C(0);
            if ((uint32_t)action_ticks + UINT32_C(1) <
                (uint32_t)shield_break_fly->animation_frame_count)
            {
                ++action_ticks;
            }
            else
            {
                /* ShieldBreakFall skips model animation and therefore keeps
                 * the terminal ShieldBreakFly pose indefinitely. */
                action_ticks = (uint16_t)(
                    shield_break_fly->animation_frame_count -
                    UINT16_C(1));
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE)
        {
            ++action_ticks;
            if (action_ticks >= fighter->air_dodge_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_FALL_SPECIAL;
                action_ticks = UINT16_C(0);
            }
            else if (
                action_ticks <
                fighter->air_dodge_ordinary_physics_begin_tick)
            {
                velocity_x = multiply_f32(
                    velocity_x,
                    fighter->air_dodge_decay_f32);
                velocity_y = multiply_f32(
                    velocity_y,
                    fighter->air_dodge_decay_f32);
            }
            else
            {
                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_max_horizontal_speed_f32);
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_VECTOR_ASCENT)
        {
            velocity_x = apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                content->recovery.horizontal_speed_f32);
            ++action_ticks;
            if (action_ticks >= content->recovery.ascent_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_FALL_SPECIAL;
                action_ticks = UINT16_C(0);
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
            action_state ==
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS ||
            action_state ==
                (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT ||
            action_state ==
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL)
        {
            const falcon_special_attributes *attributes =
                action_state ==
                        (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL
                    ? falcon_reference_special_attributes()
                    : NULL;
            const float maximum_speed_f32 =
                attributes != NULL
                    ? multiply_f32(
                          fighter->air_max_horizontal_speed_f32,
                          attributes->specialhi_freefall_air_spd_mul_f32)
                    : fighter->fall_special_mobility_f32;

            if (action_uses_fall_special_pose(action_state))
            {
                const falcon_submotion_data *fall_special_motion =
                    falcon_reference_submotion(
                        PF_M4_FALCON_SUBMOTION_FALL_SPECIAL);

                if (fall_special_motion == NULL ||
                    fall_special_motion->animation_frame_count == UINT16_C(0))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                action_ticks =
                    action_ticks + UINT16_C(1) <
                            fall_special_motion->animation_frame_count
                        ? (uint16_t)(action_ticks + UINT16_C(1))
                        : UINT16_C(0);
            }
            else
            {
                action_ticks = UINT16_C(0);
            }
            velocity_x = apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                maximum_speed_f32);
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_WALL_JUMP)
        {
            if (strong_attack_pressed != 0 ||
                light_attack_pressed != 0)
            {
                action_state = select_aerial_attack_action(
                    fighter,
                    input,
                    facing,
                    strong_attack_pressed);
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
            else if (jump_pressed != 0 &&
                     air_jumps_remaining > UINT8_C(0))
            {
                enter_double_jump(
                    fighter,
                    input,
                    &velocity_x,
                    &velocity_y,
                    &air_jumps_remaining,
                    &fast_fall,
                    &tilt_y_age,
                    &action_state,
                    &action_ticks,
                    &source_submotion,
                    facing);
            }
            else
            {
                ++action_ticks;
                if (action_ticks >= fighter->wall_jump_ticks)
                {
                    action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                }
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE)
        {
            const falcon_submotion_data *release_motion =
                falcon_reference_submotion(source_submotion);
            uint16_t release_ticks;

            if (release_motion == NULL ||
                (source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT &&
                 source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_CUT))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (action_ticks == UINT16_C(0))
            {
                if (source_submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT)
                {
                    velocity_x =
                        -(int32_t)facing *
                        fighter->grab_release_air_speed_x_f32;
                    velocity_y = -fighter->grab_release_air_speed_y_f32;
                }
                else
                {
                    velocity_x =
                        -(int32_t)facing *
                        fighter->grab_release_speed_x_f32;
                }
            }
            velocity_x = apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                fighter->air_speed_f32);
            release_ticks =
                release_motion->animation_frame_count != UINT16_C(0)
                    ? release_motion->animation_frame_count
                    : UINT16_C(1);
            ++action_ticks;
            if (action_ticks >= release_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_JUMP)
        {
            const falcon_submotion_data *motion =
                falcon_reference_submotion(source_submotion);

            if (motion == NULL ||
                motion->gameplay_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if ((uint32_t)action_ticks + UINT32_C(1) >=
                (uint32_t)motion->gameplay_frame_count)
            {
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_f32);
            }
            else
            {
                /* The transition tick already consumed CliffJump2's one
                 * deferred physics callback while installing frame one. */
                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_f32);
                ++action_ticks;
            }
        }
        else
        {
            const uint32_t strong_aerial_attack_ticks =
                (uint32_t)fighter->strong_startup_ticks +
                (uint32_t)fighter->strong_active_ticks +
                (uint32_t)fighter->strong_recovery_ticks;
            const int double_jump_cancel_window =
                action_state ==
                    (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
                action_ticks < fighter->double_jump_cancel_ticks;

            if (action_state ==
                (uint8_t)PF_M4_ACTION_REFLECTOR_AIR)
            {
                const uint32_t reflector_ticks =
                    (uint32_t)content->reflector.startup_ticks +
                    (uint32_t)content->reflector.active_ticks +
                    (uint32_t)content->reflector.recovery_ticks;

                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_f32);
                ++action_ticks;
                if ((uint32_t)action_ticks >= reflector_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
            }
            else if (
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR)
            {
                const struct reference_move *move =
                    falcon_move_for_action(action_state);
                const falcon_special_attributes *attributes =
                    falcon_reference_special_attributes();

                if (move == NULL || attributes == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (action_ticks >= move->total_frames)
                {
                    const int ground_origin =
                        action_state ==
                        (uint8_t)
                            PF_M4_ACTION_FALCON_KICK_START_GROUND;

                    action_state =
                        ground_origin != 0
                            ? (uint8_t)
                                  PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND
                            : (uint8_t)
                                  PF_M4_ACTION_FALCON_KICK_END_AIR;
                    action_ticks = UINT16_C(0);
                    if (ground_origin != 0)
                    {
                        velocity_x = INT32_C(0);
                        velocity_y = INT32_C(0);
                        launched_this_tick = 1;
                    }
                    else
                    {
                        const falcon_common_attributes *common =
                            falcon_reference_common_attributes();

                        if (common == NULL)
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                        falcon_source_air_physics(
                            common,
                            &velocity_x,
                            &velocity_y);
                        launched_this_tick = 1;
                    }
                    scratch->attack_hit_mask[player_index] =
                        UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                    /* SpecialLw_ChangeMotion's ground-origin edge conversion
                     * enters the aerial end state with zero self velocity and
                     * does not run ordinary air physics on that conversion
                     * update. The natural aerial route already ran its source
                     * air callback above and continues through the shared
                     * airborne phase below. */
                    launched_this_tick = ground_origin != 0;
                }
                else
                {
                    if (falcon_kick_root_velocity(
                            action_state,
                            action_ticks,
                            facing,
                            1,
                            &velocity_x,
                            &velocity_y) != PF_STATUS_OK)
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    if (action_state ==
                        (uint8_t)
                            PF_M4_ACTION_FALCON_KICK_START_GROUND)
                    {
                        const float hit_scale_f32 =
                            falcon_kick_hit_velocity_scale(
                                attributes,
                                scratch
                                    ->falcon_kick_hit_count[player_index]);

                        velocity_x = multiply_f32(
                            velocity_x,
                            hit_scale_f32);
                        velocity_y = multiply_f32(
                            velocity_y,
                            hit_scale_f32);
                    }
                    ++action_ticks;
                    launched_this_tick = 1;
                    fast_fall = UINT8_C(0);
                }
            }
            else if (
                action_state ==
                    (uint8_t)
                        PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND)
            {
                const struct reference_move *move =
                    falcon_move_for_action(action_state);
                const falcon_common_attributes *common =
                    falcon_reference_common_attributes();
                const falcon_down_special_timing *timing =
                    falcon_reference_down_special_timing();
                const uint16_t command_frame =
                    (uint16_t)(action_ticks + UINT16_C(2));

                if (move == NULL || common == NULL || timing == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (action_ticks >= move->total_frames)
                {
                    action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    falcon_source_air_physics(
                        common,
                        &velocity_x,
                        &velocity_y);
                    launched_this_tick = 1;
                    scratch->falcon_kick_hit_count[player_index] =
                        UINT8_C(0);
                    scratch->attack_hit_mask[player_index] =
                        UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    const int root_motion =
                        action_state ==
                            (uint8_t)
                                PF_M4_ACTION_FALCON_KICK_WALL_REBOUND ||
                        (action_state ==
                             (uint8_t)
                                 PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND &&
                         /* Commands execute before Dolphin exposes the
                          * resulting post-frame pose. */
                         command_frame <
                             timing
                                 ->ground_origin_air_physics_begin_frame);

                    if (root_motion != 0)
                    {
                        if (falcon_kick_root_velocity(
                                action_state,
                                action_ticks,
                                facing,
                                1,
                                &velocity_x,
                                &velocity_y) != PF_STATUS_OK)
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                    }
                    else
                    {
                        falcon_source_air_physics(
                            common,
                            &velocity_x,
                            &velocity_y);
                    }
                    ++action_ticks;
                    launched_this_tick = 1;
                    fast_fall = UINT8_C(0);
                }
            }
            else if (
                action_state ==
                     (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
            {
                const falcon_move_index move_index =
                    action_state ==
                            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND
                        ? PF_M4_FALCON_UP_SPECIAL_GROUND
                        : PF_M4_FALCON_UP_SPECIAL_AIR;
                const struct reference_move *move =
                    falcon_reference_move(move_index);

                if (move == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (launched_this_tick != 0)
                {
                    fast_fall = UINT8_C(0);
                }
                else if (action_ticks >= move->total_frames)
                {
                    const falcon_special_attributes *attributes =
                        falcon_reference_special_attributes();
                    const float fall_maximum_f32 =
                        attributes != NULL
                            ? multiply_f32(
                                  fighter->air_max_horizontal_speed_f32,
                                  attributes
                                      ->specialhi_freefall_air_spd_mul_f32)
                            : INT32_C(0);

                    if (attributes == NULL ||
                        fall_maximum_f32 <= INT32_C(0))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    action_state =
                        (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL;
                    action_ticks = UINT16_C(0);
                    velocity_x = apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fall_maximum_f32);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    if (falcon_dive_start_velocity(
                            fighter,
                            input,
                            action_state,
                            action_ticks,
                            &facing,
                            &velocity_x,
                            &velocity_y) != PF_STATUS_OK)
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    ++action_ticks;
                    launched_this_tick = 1;
                    fast_fall = UINT8_C(0);
                }
            }
            else if (action_state ==
                (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW)
            {
                const struct reference_move *move =
                    falcon_reference_move(
                        PF_M4_FALCON_UP_SPECIAL_THROW);

                if (move == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (launched_this_tick != 0)
                {
                    fast_fall = UINT8_C(0);
                }
                else if (action_ticks >= move->total_frames)
                {
                    action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    recovery_available = UINT8_C(1);
                    velocity_x = apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_f32);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    if (falcon_dive_throw_velocity(
                            fighter,
                            input,
                            action_ticks,
                            facing,
                            &velocity_x,
                            &velocity_y) != PF_STATUS_OK)
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    ++action_ticks;
                    launched_this_tick = 1;
                    fast_fall = UINT8_C(0);
                }
            }
            else if (action_state ==
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR)
            {
                const struct reference_move *move =
                    falcon_reference_move(
                        PF_M4_FALCON_NEUTRAL_SPECIAL_AIR);
                const falcon_special_attributes *attributes =
                    falcon_reference_special_attributes();
                const falcon_neutral_special_timing *timing =
                    falcon_reference_neutral_special_timing();
                const uint16_t displayed_frame =
                    action_ticks + UINT16_C(1);

                if (move == NULL || attributes == NULL || timing == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (displayed_frame == timing->launch_frame)
                {
                    falcon_punch_launch_velocity(
                        attributes,
                        input->main_stick_y,
                        facing,
                        &velocity_x,
                        &velocity_y);
                }
                if (displayed_frame >=
                        timing->velocity_scale_begin_frame &&
                    displayed_frame <=
                        timing->velocity_scale_end_frame)
                {
                    velocity_x = multiply_f32(
                        velocity_x,
                        attributes->specialn_vel_mul_f32);
                    velocity_y = multiply_f32(
                        velocity_y,
                        attributes->specialn_vel_mul_f32);
                    launched_this_tick = 1;
                }
                else if (displayed_frame <
                         timing->ordinary_air_physics_begin_frame)
                {
                    velocity_x = approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->air_friction_f32);
                }
                else
                {
                    velocity_x = apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_f32);
                }
                ++action_ticks;
                if (action_ticks > move->total_frames)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] =
                        UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
            }
            else if (
                action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR)
            {
                const int hit =
                    action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR;
                const falcon_move_index move_index =
                    hit != 0
                        ? PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR
                        : PF_M4_FALCON_SIDE_SPECIAL_START_AIR;
                const struct reference_move *move =
                    falcon_reference_move(move_index);
                const falcon_special_attributes *attributes =
                    falcon_reference_special_attributes();
                const falcon_side_special_timing *timing =
                    falcon_reference_side_special_timing();
                float reference_motion_x_f32;

                if (move == NULL || attributes == NULL || timing == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (action_ticks >= move->total_frames)
                {
                    action_state = hit != 0
                                       ? (uint8_t)
                                             PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT
                                       : (uint8_t)
                                             PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    const uint16_t displayed_frame =
                        action_ticks + UINT16_C(1);

                    if (!falcon_reference_motion_x_f32(
                            action_state,
                            displayed_frame,
                            &reference_motion_x_f32))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    velocity_x =
                        (float)facing * reference_motion_x_f32;
                    if (hit != 0 ||
                        displayed_frame >=
                            timing->air_gravity_begin_frame)
                    {
                        const float gravity_f32 =
                            falcon_source_velocity_to_sim_f32(
                                attributes->specials_grav_f32,
                                INT32_C(11),
                                INT32_C(62));
                        const float terminal_f32 =
                            falcon_source_velocity_to_sim_f32(
                                attributes->specials_terminal_vel_f32,
                                INT32_C(11),
                                INT32_C(62));

                        velocity_y = approach(
                            velocity_y,
                            terminal_f32,
                            gravity_f32);
                    }
                    ++action_ticks;
                    launched_this_tick = 1;
                }
                fast_fall = UINT8_C(0);
            }
            else if (action_state ==
                (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR)
            {
                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_f32);
                ++action_ticks;
                if (action_ticks >=
                    content->projectile.fire_recovery_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                }
            }
            else if (action_is_light_aerial(action_state))
            {
                const uint32_t aerial_attack_ticks =
                    light_aerial_ticks(fighter, action_state);
                const int aerial_iasa_active =
                    falcon_aerial_reference_matches(
                        fighter,
                        action_state) &&
                    falcon_reference_iasa_active(
                        action_state,
                        (uint32_t)action_ticks + UINT32_C(2));

                /* AttackAir IASA does not call AttackAir_CheckInput. For
                 * Falcon with no held item or tether, its only actionable
                 * callback is JumpAerial_CheckInput; a second aerial cannot
                 * replace the current animation before it reaches Fall. */
                if (aerial_iasa_active != 0 &&
                    jump_pressed != 0 &&
                    air_jumps_remaining > UINT8_C(0))
                {
                    enter_double_jump(
                        fighter,
                        input,
                        &velocity_x,
                        &velocity_y,
                        &air_jumps_remaining,
                        &fast_fall,
                        &tilt_y_age,
                        &action_state,
                        &action_ticks,
                        &source_submotion,
                        facing);
                    scratch->tumble[player_index] = UINT8_C(0);
                    scratch->attack_hit_mask[player_index] =
                        UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    if (fighter->reference_frame_data_enabled != UINT8_C(0))
                    {
                        float reference_motion_x_f32 = INT32_C(0);
                        float reference_motion_y_f32 = INT32_C(0);
                        const uint16_t displayed_frame =
                            (uint16_t)(action_ticks + UINT16_C(1));

                        (void)falcon_reference_motion_x_f32(
                            action_state,
                            displayed_frame,
                            &reference_motion_x_f32);
                        (void)falcon_reference_motion_y_f32(
                            action_state,
                            displayed_frame,
                            &reference_motion_y_f32);
                        animation_motion_x_f32 =
                            (float)facing * reference_motion_x_f32;
                        animation_motion_y_f32 = reference_motion_y_f32;
                    }
                    velocity_x = apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_f32);
                    ++action_ticks;
                    if ((uint32_t)action_ticks >= aerial_attack_ticks)
                    {
                        action_state =
                            (uint8_t)PF_M4_ACTION_AIRBORNE;
                        action_ticks = UINT16_C(0);
                        scratch->attack_hit_mask[player_index] =
                            UINT8_C(0);
                        scratch->attack_stale_registered[player_index] =
                            UINT8_C(0);
                    }
                }
            }
            else if (
                action_state ==
                (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK)
            {
                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_f32);
                ++action_ticks;
                if ((uint32_t)action_ticks >=
                    strong_aerial_attack_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    scratch->attack_hit_mask[player_index] =
                        UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
            }
            else if (
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
                action_state == (uint8_t)PF_M4_ACTION_GRABBED)
            {
                /* CaptureCaptain freezes both airborne participants. The
                 * action/capture timeline advances in the shared grab path;
                 * do not route either participant through ordinary fall. */
                velocity_x = INT32_C(0);
                velocity_y = INT32_C(0);
                launched_this_tick = 1;
                fast_fall = UINT8_C(0);
            }
            else if (
                released_ledge_this_tick == 0 &&
                dense_shield_pressed != 0 &&
                input_shield_strength_value >=
                    fighter->digital_trigger_threshold &&
                scratch->tumble[player_index] == UINT8_C(0) &&
                damage_fall_wiggle_this_tick == 0)
            {
                status = enter_air_dodge(
                    fighter,
                    input->main_stick_x,
                    input->main_stick_y,
                    &velocity_x,
                    &velocity_y);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                action_state = (uint8_t)PF_M4_ACTION_AIR_DODGE;
                action_ticks = UINT16_C(0);
                fast_fall = UINT8_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
            }
            else if (released_ledge_this_tick == 0 &&
                     scratch->tumble[player_index] == UINT8_C(0) &&
                     damage_fall_wiggle_this_tick == 0 &&
                     (strong_attack_pressed != 0 ||
                      light_attack_pressed != 0))
            {
                if (double_jump_cancel_window != 0)
                {
                    velocity_y = INT32_C(0);
                }
                action_state = select_aerial_attack_action(
                    fighter,
                    input,
                    facing,
                    strong_attack_pressed);
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] =
                    UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
                velocity_x = apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_f32);
            }
            else
            {
                if (!launched_this_tick)
                {
                    velocity_x = apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_f32);
                }
                if (action_state ==
                    (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP)
                {
                    ++action_ticks;
                    if (action_ticks >=
                        fighter->double_jump_cancel_ticks)
                    {
                        action_state =
                            (uint8_t)PF_M4_ACTION_AIRBORNE;
                    }
                }
                else if (
                    action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
                    (world->action_state[player_index] ==
                         (uint8_t)PF_M4_ACTION_AIRBORNE ||
                     source_submotion ==
                         (uint16_t)
                             PF_M4_FALCON_SUBMOTION_PLATFORM_DROP))
                {
                    if (world->action_state[player_index] ==
                            (uint8_t)PF_M4_ACTION_AIRBORNE &&
                        !advance_falcon_source_submotion(
                            &source_submotion,
                            &action_ticks))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                }
                else if (action_state ==
                             (uint8_t)PF_M4_ACTION_HITSTUN &&
                         fighter->reference_frame_data_enabled !=
                             UINT8_C(0))
                {
                    /* DamageFall's released IASA uses the ordinary Fall
                     * table, but a neutral sample leaves the current damage
                     * animation running until that selected submotion's
                     * final sourced frame. */
                    status = advance_retained_damage_animation(
                        source_submotion,
                        grounded,
                        &action_state,
                        &action_ticks,
                        scratch->hitstun_ticks[player_index],
                        &source_submotion);
                    if (status != PF_STATUS_OK)
                    {
                        return status;
                    }
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                    source_submotion =
                        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
                }
            }

            if ((action_state ==
                     (uint8_t)PF_M4_ACTION_AIRBORNE ||
                 (damage_released_jump_requested != 0 &&
                  action_is_damage(action_state))) &&
                !launched_this_tick &&
                released_ledge_this_tick == 0 &&
                (jump_pressed != 0 ||
                 damage_released_jump_requested != 0) &&
                air_jumps_remaining > UINT8_C(0))
            {
                enter_double_jump(
                    fighter,
                    input,
                    &velocity_x,
                    &velocity_y,
                    &air_jumps_remaining,
                    &fast_fall,
                    &tilt_y_age,
                    &action_state,
                    &action_ticks,
                    &source_submotion,
                    facing);
                scratch->tumble[player_index] = UINT8_C(0);
            }
        }
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        ((world->grounded[player_index] != UINT8_C(0) &&
          grounded == UINT8_C(0)) ||
         (air_jumps_remaining <
              world->air_jumps_remaining[player_index] &&
          (source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD ||
           source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD))))
    {
        /* ftCommon_8007D5D4 always relocks the ECB for ten map updates,
         * including when JumpAerial resets an existing lock.  Melee keeps
         * CollData.desired_ecb.bottom from the preceding map update; derive
         * that same value from canonical source state instead of storing a
         * second copy of the complete desired ECB. */
        ecb_locked_bottom_y_f32 =
            reference_ecb_lock_bottom_from_world_f32(
                fighter,
                world,
                player_index,
                previous_locked_bottom_y_f32);
        ecb_bottom_lock_ticks = PF_M4_COMMON_AIR_ENTRY_ECB_LOCK_TICKS;
        inherited_locked_bottom_y_f32 = ecb_locked_bottom_y_f32;
    }

    if (action_state != (uint8_t)PF_M4_ACTION_SHIELD &&
        action_state != (uint8_t)PF_M4_ACTION_SHIELD_STUN)
    {
        scratch->shield_health_f32[player_index] =
            shield_health_add(
                scratch->shield_health_f32[player_index],
                fighter->shield_regeneration_f32,
                fighter->shield_health_f32);
    }

    const uint8_t shield_recoil_bit =
        (uint8_t)(UINT8_C(1) << player_index);
    float shield_recoil_x =
        (scratch->shield_recoil_mask & shield_recoil_bit) != UINT8_C(0)
            ? scratch->shield_recoil_x_f32[player_index]
            : 0.0f;

    if (shield_recoil_x != INT32_C(0))
    {
        const float recoil_decay_f32 =
            grounded != UINT8_C(0)
                ? multiply_f32(
                      fighter->traction_f32,
                      fighter
                          ->shield_attacker_pushback_ground_friction_scale_f32)
                : fighter->shield_attacker_pushback_air_decay_f32;

        shield_recoil_x = approach(
            shield_recoil_x,
            INT32_C(0),
            recoil_decay_f32);
        if (shield_recoil_x == INT32_C(0))
        {
            scratch->shield_recoil_mask =
                (uint8_t)(
                    scratch->shield_recoil_mask &
                    (uint8_t)~shield_recoil_bit);
        }
    }

    /* Evaluate DownBound's animated ECB contact before selecting the xF0/x8c
     * knockback-decay channel. The action and retained support stay grounded
     * semantics, but source contactless frames use air-vector decay starting
     * on the same displayed frame. */
    if (action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN)
    {
        grounded = down_bound_floor_contact(
            scratch->prone_orientation[player_index],
            action_ticks);
    }

    /* Fighter_procUpdate runs the action physics callback first, then decays
     * x8c_kb_vel, then adds self velocity and knockback to position. The
     * dedicated knockback channel remains distinct from ordinary velocity
     * throughout integration. */
    if (scratch->knockback_velocity_x_f32[player_index] != INT32_C(0) ||
        scratch->knockback_velocity_y_f32[player_index] != INT32_C(0) ||
        scratch->ground_knockback_velocity_f32[player_index] != INT32_C(0))
    {
        if (grounded != UINT8_C(0))
        {
            const float ground_decay_f32 =
                source_character != NULL
                    ? source_character->friction_f32 *
                          fighter->ground_knockback_decay_scale_f32
                    : multiply_f32(
                          fighter->traction_f32,
                          fighter->ground_knockback_decay_scale_f32);

            if (scratch->ground_knockback_velocity_f32[player_index] ==
                INT32_C(0))
            {
                scratch->ground_knockback_velocity_f32[player_index] =
                    scratch->knockback_velocity_x_f32[player_index];
            }
            scratch->ground_knockback_velocity_f32[player_index] =
                approach(
                    scratch->ground_knockback_velocity_f32[player_index],
                    INT32_C(0),
                    ground_decay_f32);
            project_ground_scalar_f32(
                content,
                support,
                scratch->ground_knockback_velocity_f32[player_index],
                &scratch->knockback_velocity_x_f32[player_index],
                &scratch->knockback_velocity_y_f32[player_index]);
        }
        else
        {
            scratch->ground_knockback_velocity_f32[player_index] =
                INT32_C(0);
            status = ssbm_decay_air_knockback_f32(
                fighter->air_knockback_decay_f32,
                &scratch->knockback_velocity_x_f32[player_index],
                &scratch->knockback_velocity_y_f32[player_index]);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
    }

    if (initial_dash_entered_this_tick != 0)
    {
        /* ftCo_Dash_Enter invalidates the horizontal tilt edge before any
         * later Dash callback can consume it. */
        tilt_x_age = UINT8_C(254);
    }
    integrated_self_x_f32 =
        initial_dash_entered_this_tick != 0
            ? initial_dash_entry_motion_velocity_x
            : velocity_x;
    integrated_self_y_f32 = velocity_y;
    integrated_animation_x_f32 = animation_motion_x_f32;
    integrated_animation_y_f32 = animation_motion_y_f32;
    if (grounded != UINT8_C(0) &&
        stage->reference_collision_profile !=
            (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        project_ground_scalar_f32(
            content,
            support,
            integrated_self_x_f32,
            &integrated_self_x_f32,
            &integrated_self_y_f32);
        project_ground_scalar_f32(
            content,
            support,
            animation_motion_x_f32,
            &integrated_animation_x_f32,
            &integrated_animation_y_f32);
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_is_damage(
            effective_action_state(
                action_state,
                scratch->hitlag_resume_action[player_index])))
    {
        const uint8_t previous_effective_action =
            effective_action_state(
                previous_action_state,
                previous_hitlag_resume_action);
        const falcon_submotion_data *damage_motion;

        if (action_is_damage(previous_effective_action))
        {
            source_submotion = previous_source_submotion;
            source_animation_frame_f32 =
                previous_source_animation_frame_f32;
        }
        damage_motion =
            falcon_reference_submotion(source_submotion);
        if (damage_motion == NULL ||
            source_submotion <
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_HIGH_1 ||
            source_submotion >
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL ||
            damage_motion->animation_frame_count == UINT16_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        source_animation_rate_f32 = 1.0f;
        if (action_state != (uint8_t)PF_M4_ACTION_HITLAG)
        {
            const float terminal_frame_f32 =
                (float)(damage_motion->animation_frame_count - UINT16_C(1));
            const float next_frame_f32 =
                source_animation_frame_f32 + source_animation_rate_f32;

            source_animation_frame_f32 =
                next_frame_f32 < terminal_frame_f32
                    ? next_frame_f32
                    : terminal_frame_f32;
        }
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SPECIAL_LANDING)
    {
        const falcon_submotion_data *landing =
            falcon_reference_submotion(
                (uint16_t)
                    PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL);
        const float numerator =
            landing != NULL
                ? (float)landing->animation_frame_count + 0.1f
                : 0.0f;

        if (landing == NULL ||
            landing->animation_frame_count == UINT16_C(0) ||
            fighter->special_landing_ticks == UINT16_C(0) ||
            !isfinite(numerator))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL;
        source_animation_rate_f32 =
            numerator / (float)fighter->special_landing_ticks;
        source_animation_frame_f32 =
            (float)action_ticks * source_animation_rate_f32;
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        const uint8_t previous_effective_action =
            effective_action_state(
                previous_action_state,
                previous_hitlag_resume_action);

        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_AIR_DODGE;
        source_animation_rate_f32 = 1.0f;
        if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_f32 =
                previous_source_animation_frame_f32;
        }
        else if (previous_effective_action ==
                 (uint8_t)PF_M4_ACTION_AIR_DODGE)
        {
            const float next_frame_f32 =
                previous_source_animation_frame_f32 + 1.0f;

            if (!isfinite(next_frame_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            source_animation_frame_f32 = next_frame_f32;
        }
        else
        {
            /* Fighter_ChangeMotionState starts EscapeAir at animation frame
             * zero, then the priority-zero animation proc advances its JObj
             * before this update's map collision. Slippi's displayed frame
             * one therefore owns HSD animation frame one for the sweep. */
            source_animation_frame_f32 = 1.0f;
        }
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             (action_uses_fall_special_pose(
                  effective_action_state(
                      action_state,
                      scratch->hitlag_resume_action[player_index])) ||
              effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index]) ==
                  (uint8_t)PF_M4_ACTION_AIRBORNE))
    {
        status = update_falcon_fall_animation_clock(
            fighter,
            previous_action_state,
            previous_hitlag_resume_action,
            previous_source_submotion,
            previous_source_animation_frame_f32,
            previous_source_animation_rate_f32,
            previous_fall_animation_blend_f32,
            previous_fall_animation_target_switched,
            previous_ground_velocity_f32,
            previous_facing,
            action_state,
            scratch->hitlag_resume_action[player_index],
            &source_submotion,
            &source_animation_frame_f32,
            &source_animation_rate_f32,
            &fall_animation_blend_f32,
            &fall_animation_target_switched);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        falcon_reference_direct_hsd_pose(
            action_state,
            action_ticks,
            grounded,
            &source_submotion,
            &source_animation_frame_f32))
    {
        source_animation_rate_f32 = 1.0f;
    }

    previous_position_x = position_x;
    next_position = position_x + player_nudge_x_f32_value +
                    integrated_self_x_f32 +
                    scratch->knockback_velocity_x_f32[player_index] +
                    shield_recoil_x + integrated_animation_x_f32;
    if (!ledge_motion_handled)
    {
        position_x = next_position;
    }

    if (!ledge_motion_handled)
    {
        const float future_y =
            position_y + integrated_self_y_f32 +
            scratch->knockback_velocity_y_f32[player_index] +
            integrated_animation_y_f32;
        const float swept_center_top =
            future_y < position_y
                ? future_y
                : position_y;
        const float swept_center_bottom =
            future_y > position_y
                ? future_y
                : position_y;
        const float body_top =
            swept_center_top - fighter->half_height_f32;
        const float body_bottom =
            swept_center_bottom + fighter->half_height_f32;
        float wall_swept_top = body_top;
        float wall_swept_bottom = body_bottom;
        float wall_side_x_extent_f32 = fighter->half_width_f32;
        falcon_ecb_pose_f32 wall_pose;
        float wall_side_x_from_origin_f32 = fighter->half_width_f32;
        float wall_side_y_from_origin_f32 = INT32_C(0);
        float wall_impact_self_velocity_y_f32 = velocity_y;
        float exact_wall_future_y = future_y;
        hsd_compact_pose wall_blend_pose;
        float wall_blend_progress_f32 = INT32_C(0);
        const hsd_compact_pose *wall_blend_pose_or_null = NULL;
        int exact_reference_wall_pose = 0;
        int exact_wall_contact = 0;
        int reference_wall_position_only = 0;
        const int moving_right = position_x > previous_position_x;
        const int vertical_overlap =
            body_bottom > stage->solid_top_f32 &&
            body_top < stage->solid_bottom_f32;
        int8_t away_direction = INT8_C(0);
        uint8_t wall_support = UINT8_C(0);

        if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
            (source_submotion ==
                 (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON ||
             falcon_reference_hsd_ecb_pose(
                 source_submotion,
                 source_animation_frame_f32,
                 grounded != UINT8_C(0),
                 PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
                 &wall_pose) != 0))
        {
            status = evaluate_falcon_ground_blend_pose(
                world,
                player_index,
                action_state,
                scratch->hitlag_resume_action[player_index],
                source_submotion,
                source_animation_frame_f32,
                action_ticks,
                &wall_blend_pose,
                &wall_blend_progress_f32);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
            if (wall_blend_progress_f32 > INT32_C(0))
            {
                wall_blend_pose_or_null = &wall_blend_pose;
            }
        }
        exact_reference_wall_pose = reference_ecb_pose_f32(
            fighter,
            effective_action_state(
                action_state,
                scratch->hitlag_resume_action[player_index]),
            action_ticks,
            grounded,
            inherited_locked_bottom_y_f32,
            source_submotion,
            source_animation_frame_f32,
            fall_animation_blend_f32,
            fall_animation_target_switched,
            scratch->prone_orientation[player_index],
            scratch->prone_roll_motion_orientation[player_index],
            scratch->tech_direction[player_index],
            facing,
            total_velocity_f32(
                velocity_x,
                scratch->knockback_velocity_x_f32[player_index]),
            total_velocity_f32(
                velocity_y,
                scratch->knockback_velocity_y_f32[player_index]),
            wall_blend_progress_f32,
            wall_blend_pose_or_null,
            &wall_pose);
        if (exact_reference_wall_pose != 0)
        {
            ecb_world_wall_side_f32(
                &wall_pose,
                fighter->half_height_f32,
                facing,
                moving_right,
                &wall_side_x_from_origin_f32,
                &wall_side_y_from_origin_f32);
            wall_side_x_extent_f32 =
                wall_side_x_from_origin_f32 >= INT32_C(0)
                    ? wall_side_x_from_origin_f32
                    : -wall_side_x_from_origin_f32;
            wall_impact_self_velocity_y_f32 =
                fast_fall != UINT8_C(0)
                    ? fighter->fast_fall_speed_f32
                    : approach(
                          velocity_y,
                          fighter->fall_speed_f32,
                          fighter->gravity_f32);
            exact_wall_future_y =
                position_y + wall_impact_self_velocity_y_f32 +
                scratch->knockback_velocity_y_f32[player_index];
            const float previous_side_y =
                position_y - wall_side_y_from_origin_f32;
            const float future_side_y =
                exact_wall_future_y - wall_side_y_from_origin_f32;

            wall_swept_top =
                previous_side_y < future_side_y
                    ? previous_side_y
                    : future_side_y;
            wall_swept_bottom =
                previous_side_y > future_side_y
                    ? previous_side_y
                    : future_side_y;
            if (wall_swept_top == wall_swept_bottom)
            {
                ++wall_swept_bottom;
            }
        }

        if (stage->reference_collision_profile !=
            (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
        {
            if (exact_reference_wall_pose != 0)
            {
                falcon_ecb_pose_f32 previous_wall_pose;
                float previous_side_x_from_origin_f32 = INT32_C(0);
                float previous_side_y_from_origin_f32 = INT32_C(0);
                float future_position_y_f32 = INT32_C(0);
                float contact_fraction_f32 = UINT32_C(0);

                if (!reference_ecb_pose_f32(
                    fighter,
                    effective_action_state(
                        world->action_state[player_index],
                        world->hitlag_resume_action[player_index]),
                    world->action_ticks[player_index],
                    world->grounded[player_index],
                    previous_locked_bottom_y_f32,
                    world->source_submotion[player_index],
                    world->source_animation_frame_f32[player_index],
                    world->fall_animation_blend_f32[player_index],
                    world->fall_animation_target_switched[player_index],
                    world->prone_orientation[player_index],
                    world->prone_roll_motion_orientation[player_index],
                    world->tech_direction[player_index],
                    world->facing[player_index],
                    total_velocity_f32(
                        world->velocity_x_f32[player_index],
                        world->knockback_velocity_x_f32[player_index]),
                    total_velocity_f32(
                        world->velocity_y_f32[player_index],
                        world->knockback_velocity_y_f32[player_index]),
                    world->ground_blend_progress_f32[player_index],
                    world->ground_blend_progress_f32[player_index] >
                            INT32_C(0)
                        ? &world->ground_blend_pose[player_index]
                        : NULL,
                    &previous_wall_pose))
                {
                    previous_wall_pose = wall_pose;
                }
                ecb_world_wall_side_f32(
                    &previous_wall_pose,
                    fighter->half_height_f32,
                    facing,
                    moving_right,
                    &previous_side_x_from_origin_f32,
                    &previous_side_y_from_origin_f32);
                future_position_y_f32 = exact_wall_future_y;
                exact_wall_contact =
                    ssbm_reference_stage_find_wall_point_contact(
                        stage->reference_collision_profile,
                        previous_position_x +
                            previous_side_x_from_origin_f32,
                        position_y - previous_side_y_from_origin_f32,
                        position_x + wall_side_x_from_origin_f32,
                        future_position_y_f32 -
                            wall_side_y_from_origin_f32,
                        &contact_fraction_f32,
                        &wall_support,
                        &away_direction);
                if (exact_wall_contact != 0)
                {
                    const float root_delta_x_f32 =
                        position_x - previous_position_x;
                    const float root_delta_y_f32 =
                        exact_wall_future_y - position_y;

                    position_x = previous_position_x +
                        root_delta_x_f32 * contact_fraction_f32;
                    exact_wall_contact_position_y_f32 =
                        position_y +
                            root_delta_y_f32 * contact_fraction_f32;
                }
                else if (grounded == UINT8_C(0) &&
                         reference_resolve_wall_ecb_f32(
                             stage->reference_collision_profile,
                             previous_position_x,
                             position_y,
                             &previous_wall_pose,
                             future_position_y_f32,
                             &wall_pose,
                             fighter->half_height_f32,
                             facing,
                             moving_right,
                             &position_x,
                             &wall_support,
                             &away_direction))
                {
                    /* mpColl resolves a changing ECB polygon against the
                     * wall without changing self velocity. The vertical
                     * channel continues through the ordinary physics pass. */
                    reference_wall_position_only = 1;
                }
                else if (grounded == UINT8_C(0) &&
                         position_x == previous_position_x &&
                         reference_resolve_wall_ecb_f32(
                             stage->reference_collision_profile,
                             previous_position_x,
                             position_y,
                             &previous_wall_pose,
                             future_position_y_f32,
                             &wall_pose,
                             fighter->half_height_f32,
                             facing,
                             1,
                             &position_x,
                             &wall_support,
                             &away_direction))
                {
                    /* A motion can expand the opposite ECB side into a wall
                     * while the fighter root has zero horizontal velocity.
                     * mpColl resolves that pose-only overlap in the same map
                     * update, so test the other side before the scalar wall
                     * fallback. */
                    reference_wall_position_only = 1;
                }
            }
            if (exact_wall_contact == 0 &&
                reference_wall_position_only == 0)
            {
                (void)ssbm_reference_stage_find_wall_contact(
                    stage->reference_collision_profile,
                    previous_position_x,
                    position_x,
                    wall_swept_top,
                    wall_swept_bottom,
                    wall_side_x_extent_f32,
                    &position_x,
                    &wall_support,
                    &away_direction);
            }
        }
        else if (vertical_overlap &&
                 previous_position_x + fighter->half_width_f32 <=
                     stage->solid_left_f32 &&
                 position_x + fighter->half_width_f32 >=
                     stage->solid_left_f32)
        {
            position_x =
                stage->solid_left_f32 - fighter->half_width_f32;
            away_direction = INT8_C(-1);
        }
        else if (
            vertical_overlap &&
            previous_position_x - fighter->half_width_f32 >=
                stage->solid_right_f32 &&
            position_x - fighter->half_width_f32 <=
                stage->solid_right_f32)
        {
            position_x =
                stage->solid_right_f32 + fighter->half_width_f32;
            away_direction = INT8_C(1);
        }

        (void)wall_support;

        if (away_direction != INT8_C(0))
        {
            const falcon_down_special_timing *kick_timing =
                action_is_falcon_kick(action_state) != 0
                    ? falcon_reference_down_special_timing()
                    : NULL;
            const int falcon_kick_wall_rebound =
                kick_timing != NULL &&
                ((action_state ==
                      (uint8_t)
                          PF_M4_ACTION_FALCON_KICK_START_GROUND &&
                  action_ticks >=
                      kick_timing->ground_wall_rebound_begin_frame) ||
                 (action_state ==
                      (uint8_t)PF_M4_ACTION_FALCON_KICK_START_AIR &&
                  action_ticks >=
                      kick_timing->air_wall_rebound_begin_frame)) &&
                away_direction == (int8_t)-facing;

            if (falcon_kick_wall_rebound != 0)
            {
                grounded = UINT8_C(0);
                support = (uint8_t)PF_M4_SURFACE_NONE;
                /* ftCommon_8007D5D4 clears Melee's ground channel but
                 * preserves self_vel. Falcon Kick has already copied its
                 * root speed into that channel, so the action-363 entry
                 * post-frame still exposes the incoming horizontal speed. */
                action_ticks = UINT16_C(0);
                action_state =
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND;
                fast_fall = UINT8_C(0);
                /* Melee locks the ECB for ten frames in ftCommon_8007D5D4.
                 * Marking this transition as launched prevents only the
                 * impossible same-tick floor reattachment; the imported
                 * rebound root motion clears the floor on the next tick. */
                launched_this_tick = 1;
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
            else if (grounded == UINT8_C(0) &&
                scratch->tumble[player_index] != UINT8_C(0) &&
                (!action_is_surface_bounce(action_state) ||
                 action_ticks >=
                     fighter->surface_bounce_collision_lockout_ticks) &&
                (scratch->knockback_velocity_x_f32[player_index] >
                     fighter->surface_collision_threshold_x_f32 ||
                 scratch->knockback_velocity_x_f32[player_index] <
                     -fighter->surface_collision_threshold_x_f32))
            {
                const ssbm_stage_collision_line *wall_line =
                    wall_support != UINT8_C(0)
                        ? ssbm_reference_stage_line(
                              stage->reference_collision_profile,
                              wall_support)
                        : NULL;
                const float source_normal_x_f32 =
                    wall_line != NULL
                        ? wall_line->source_normal_x_f32
                        : (int32_t)away_direction * 1.0f;
                const float source_normal_y_f32 =
                    wall_line != NULL
                        ? wall_line->source_normal_y_f32
                        : INT32_C(0);
                const int up_held =
                    input->main_stick_y <=
                    -(int16_t)fighter->crouch_axis_threshold;

                if (exact_wall_contact != 0)
                {
                    velocity_y = wall_impact_self_velocity_y_f32;
                }

                status = enter_wall_impact(
                    fighter,
                    jump_pressed || up_held,
                    away_direction,
                    source_normal_x_f32,
                    source_normal_y_f32,
                    scratch,
                    player_index,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &action_state,
                    &fast_fall,
                    &facing);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                exact_wall_response_this_tick = exact_wall_contact;
            }
            else
            {
                const int wall_jump_requested =
                    grounded == UINT8_C(0) &&
                    fighter->wall_jump_enabled != UINT8_C(0) &&
                    action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
                    strong_direction_value == away_direction &&
                    strong_direction_value != previous_strong_direction;

                if (wall_jump_requested != 0)
                {
                    enter_wall_jump(
                        fighter,
                        away_direction,
                        &velocity_x,
                        &velocity_y,
                        &action_ticks,
                        &action_state,
                        &fast_fall,
                        &facing);
                    launched_this_tick = 1;
                }
                else if (reference_wall_position_only == 0)
                {
                    velocity_x = INT32_C(0);
                }
            }
        }
    }

    if (!ledge_motion_handled &&
        (grounded != UINT8_C(0) ||
         (action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN &&
          support != (uint8_t)PF_M4_SURFACE_NONE)))
    {
        float surface_left;
        float surface_right;
        int retains_surface;

        surface_bounds_f32(
            content,
            support,
            world->tick + UINT64_C(1),
            &surface_left,
            &surface_right);
        retains_surface =
            position_x >= surface_left && position_x <= surface_right;
        if (retains_surface == 0 &&
            stage->reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED &&
            reference_connected_floor_support(
                stage->reference_collision_profile,
                support,
                position_x,
                &support))
        {
            /* mpColl's grounded line traversal follows the endpoint links in
             * the stage catalog before deciding that the fighter left the
             * floor. A single logical floor may be split into several lines
             * (Battlefield's center and wing segments are the common case).
             * Keep the action grounded and transfer support without a Fall
             * row; the successor line's height/normal owns the final snap. */
            surface_bounds_f32(
                content,
                support,
                world->tick + UINT64_C(1),
                &surface_left,
                &surface_right);
            retains_surface = 1;
        }
        if (retains_surface == 0 &&
            action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN &&
            grounded == UINT8_C(0) &&
            action_ticks > UINT16_C(0) &&
            down_bound_floor_contact(
                scratch->prone_orientation[player_index],
                (uint16_t)(action_ticks - UINT16_C(1))) == UINT8_C(0))
        {
            /* The first contactless ECB frame consumes the current root.
             * Later contactless frames retain the floor through the source
             * collision callback's preceding root. */
            retains_surface =
                previous_position_x >= surface_left &&
                previous_position_x <= surface_right;
        }
        if (stage->reference_collision_profile ==
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED &&
            support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
        {
            retains_surface =
                body_overlaps_horizontal_interval(
                    position_x,
                    fighter->half_width_f32,
                    surface_left,
                    surface_right);
        }
        if (action_state ==
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND &&
            previous_position_x >= surface_left &&
            previous_position_x <= surface_right &&
            (position_x < surface_left || position_x > surface_right))
        {
            /* ftCa_SpecialN_Coll uses ft_800827A0, whose mode-2 floor
             * callback snaps the ECB bottom to the crossed floor endpoint
             * and remains grounded. This also prevents one-Q16 root motion
             * from spuriously cascading into SpecialAirN at a ledge. */
            position_x = position_x < surface_left
                             ? surface_left
                             : surface_right;
            retains_surface = 1;
        }
        if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
            action_is_ground_attack(action_state) != 0 &&
            previous_position_x >= surface_left &&
            previous_position_x <= surface_right &&
            (position_x < surface_left || position_x > surface_right))
        {
            /* AttackDash and Falcon's other common ground attacks use
             * ft_80084104 -> mpColl_8004B2DC (ground collision mode 2).
             * That mode retains the prior floor at a terminal endpoint and
             * clamps the root to the endpoint instead of converting to Fall.
             * Dash/Run use ft_800844EC -> mode 0 and remain free to run off. */
            position_x = position_x < surface_left
                             ? surface_left
                             : surface_right;
            retains_surface = 1;
        }
        if (horizontal_magnitude <= fighter->axis_dead_zone &&
            action_can_enter_teeter(action_state) != 0 &&
            position_x < surface_left &&
            facing == INT8_C(-1) &&
            previous_position_x >= surface_left &&
            surface_left - position_x <= fighter->teeter_snap_distance_f32)
        {
            position_x = surface_left;
            velocity_x = INT32_C(0);
            action_state = (uint8_t)PF_M4_ACTION_TEETER;
            action_ticks = UINT16_C(0);
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER;
            dash_direction = INT8_C(0);
        }
        else if (
            horizontal_magnitude <= fighter->axis_dead_zone &&
            action_can_enter_teeter(action_state) != 0 &&
            position_x > surface_right &&
            facing == INT8_C(1) &&
            previous_position_x <= surface_right &&
            position_x - surface_right <= fighter->teeter_snap_distance_f32)
        {
            position_x = surface_right;
            velocity_x = INT32_C(0);
            action_state = (uint8_t)PF_M4_ACTION_TEETER;
            action_ticks = UINT16_C(0);
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_TEETER;
            dash_direction = INT8_C(0);
        }
        else if (retains_surface == 0)
        {
            const int down_bound_fall =
                action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN;
            const int shield_break_fall =
                action_is_shield_break(action_state);
            const int falcon_punch_fall =
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND;
            const int raptor_boost_start_fall =
                action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND;
            const int raptor_boost_hit_fall =
                action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND;
            const int falcon_kick_start_fall =
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND;
            const int falcon_kick_end_fall =
                action_state ==
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND;
            const int falcon_kick_ground_origin_end_fall =
                action_state ==
                    (uint8_t)
                        PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND;
            const int capture_cut_fall =
                action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE &&
                source_submotion ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_CUT;
            const int ordinary_fall =
                down_bound_fall != 0 ||
                (shield_break_fall == 0 &&
                 falcon_punch_fall == 0 &&
                 raptor_boost_start_fall == 0 &&
                 raptor_boost_hit_fall == 0 &&
                 falcon_kick_start_fall == 0 &&
                 falcon_kick_end_fall == 0 &&
                 falcon_kick_ground_origin_end_fall == 0 &&
                 capture_cut_fall == 0);
            const falcon_side_special_timing *raptor_timing =
                (raptor_boost_start_fall != 0 ||
                 raptor_boost_hit_fall != 0)
                    ? falcon_reference_side_special_timing()
                    : NULL;
            const int raptor_boost_active_fall =
                raptor_boost_start_fall != 0 &&
                raptor_timing != NULL &&
                action_ticks >= raptor_timing->ground_search_begin_frame &&
                action_ticks <= raptor_timing->ground_search_end_frame;
            const int raptor_boost_edge_fall =
                raptor_boost_active_fall != 0 ||
                raptor_boost_hit_fall != 0;

            /* mpColl_8004B108's SpecialAttackGround edge conversion keeps
             * Falcon's full root velocity but commits half of the crossing
             * displacement.  The conversion tick itself runs no air gravity. */
            if (falcon_kick_start_fall != 0)
            {
                position_x = previous_position_x +
                    (position_x - previous_position_x) / INT32_C(2);
                launched_this_tick = 1;
            }
            if (raptor_boost_edge_fall != 0)
            {
                velocity_x = clamp_f32(
                    velocity_x,
                    -fighter->air_speed_f32,
                    fighter->air_speed_f32);
                launched_this_tick = 1;
            }

            if (stage->reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
            {
                project_ground_scalar_f32(
                    content,
                    support,
                    velocity_x,
                    &velocity_x,
                    &velocity_y);
                /* The ground callback owns the edge-conversion frame. Air
                 * gravity begins on the next tick, after the tangent velocity
                 * has committed this frame's displacement. */
                launched_this_tick = 1;
            }
            if (ordinary_fall != 0)
            {
                /* ftCo_Fall_Enter clamps self velocity to air_drift_max after
                 * the ground collision callback has committed the edge-exit
                 * displacement. The clamp therefore affects the next air
                 * physics update, not the conversion frame's position. */
                velocity_x = clamp_f32(
                    velocity_x,
                    -fighter->air_speed_f32,
                    fighter->air_speed_f32);
            }

            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state =
                shield_break_fall != 0
                    ? (uint8_t)PF_M4_ACTION_SHIELD_BREAK
                    : falcon_punch_fall != 0
                    ? (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR
                    : raptor_boost_hit_fall != 0
                    ? (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT
                    : raptor_boost_active_fall != 0
                    ? (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS
                    : falcon_kick_start_fall != 0
                    ? (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND
                    : falcon_kick_end_fall != 0
                    ? (uint8_t)
                          PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND
                    : falcon_kick_ground_origin_end_fall != 0
                    ? (uint8_t)
                          PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND
                    : capture_cut_fall != 0
                    ? (uint8_t)PF_M4_ACTION_GRAB_RELEASE
                    : (uint8_t)PF_M4_ACTION_AIRBORNE;
            if (falcon_punch_fall == 0 &&
                falcon_kick_start_fall == 0 &&
                falcon_kick_ground_origin_end_fall == 0 &&
                capture_cut_fall == 0)
            {
                action_ticks = UINT16_C(0);
            }
            if (shield_break_fall == 0 &&
                falcon_punch_fall == 0 &&
                raptor_boost_edge_fall == 0 &&
                falcon_kick_start_fall == 0 &&
                falcon_kick_end_fall == 0 &&
                falcon_kick_ground_origin_end_fall == 0 &&
                capture_cut_fall == 0)
            {
                source_submotion =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            }
            short_hop_latched = UINT8_C(0);
            fast_fall = UINT8_C(0);
            dash_direction = INT8_C(0);
            scratch->shield_stun_ticks[player_index] =
                UINT16_C(0);
            if (down_bound_fall != 0)
            {
                /* DownBound_Coll enters Fall without a keep-state flag; the
                 * source transition clears its retained damage timer. */
                scratch->hitstun_ticks[player_index] = UINT16_C(0);
            }
            scratch->powershield[player_index] = UINT8_C(0);
            scratch->tech_direction[player_index] = INT8_C(0);
            if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
                ordinary_fall != 0)
            {
                /* ftCo_Fall_Enter runs inside the ground collision callback.
                 * Fighter_procMap has already performed this update's ECB
                 * lock decrement, so the newly installed ten-update lock
                 * must commit at ten instead of being decremented below. */
                ecb_locked_bottom_y_f32 =
                    reference_ecb_lock_bottom_from_world_f32(
                        fighter,
                        world,
                        player_index,
                        previous_locked_bottom_y_f32);
                ecb_bottom_lock_ticks =
                    PF_M4_COMMON_AIR_ENTRY_ECB_LOCK_TICKS;
                inherited_locked_bottom_y_f32 =
                    ecb_locked_bottom_y_f32;
                ecb_lock_entered_during_map = 1;
            }
        }
        else
        {
            position_y =
                surface_y_f32(content, support, position_x) -
                fighter->half_height_f32;
            if (action_state !=
                    (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND &&
                action_state !=
                    (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND)
            {
                velocity_y = INT32_C(0);
            }
        }
    }

    if (!ledge_motion_handled &&
        grounded == UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_KNOCKDOWN)
    {
        const float previous_bottom =
            position_y + fighter->half_height_f32;
        int previous_exact_floor_contact_pose = 0;
        const float previous_floor_contact_bottom_extent_f32 =
            floor_contact_bottom_extent_f32(
                fighter,
                effective_action_state(
                    world->action_state[player_index],
                    world->hitlag_resume_action[player_index]),
                world->action_ticks[player_index],
                world->grounded[player_index],
                previous_locked_bottom_y_f32,
                world->source_submotion[player_index],
                world->source_animation_frame_f32[player_index],
                world->fall_animation_blend_f32[player_index],
                world->fall_animation_target_switched[player_index],
                world->prone_orientation[player_index],
                world->prone_roll_motion_orientation[player_index],
                world->tech_direction[player_index],
                world->facing[player_index],
                total_velocity_f32(
                    world->velocity_x_f32[player_index],
                    world->knockback_velocity_x_f32[player_index]),
                total_velocity_f32(
                    world->velocity_y_f32[player_index],
                    world->knockback_velocity_y_f32[player_index]),
                &previous_exact_floor_contact_pose);
        int exact_floor_contact_pose = 0;
        float ceiling_top_extent_f32 = fighter->half_height_f32;
        falcon_ecb_pose_f32 ceiling_pose;
        const float floor_contact_bottom_extent_f32_value =
            floor_contact_bottom_extent_f32(
                fighter,
                effective_action_state(
                    action_state,
                    scratch->hitlag_resume_action[player_index]),
                action_ticks,
                grounded,
                inherited_locked_bottom_y_f32,
                source_submotion,
                source_animation_frame_f32,
                fall_animation_blend_f32,
                fall_animation_target_switched,
                scratch->prone_orientation[player_index],
                scratch->prone_roll_motion_orientation[player_index],
                scratch->tech_direction[player_index],
                facing,
                total_velocity_f32(
                    velocity_x,
                    scratch->knockback_velocity_x_f32[player_index]),
                total_velocity_f32(
                    velocity_y,
                    scratch->knockback_velocity_y_f32[player_index]),
                &exact_floor_contact_pose);
        const pass_through_floor_sweep_policy sweep_policy =
                exact_floor_contact_pose == 0
                    ? PF_M4_PASS_THROUGH_FLOOR_SWEEP_DEFERRED
                    : (action_is_light_aerial(action_state) ||
                               action_state == (uint8_t)
                                                   PF_M4_ACTION_STRONG_AERIAL_ATTACK
                           ? PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT_OR_DEFERRED
                           : PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT);
        const float previous_floor_contact =
            world->position_y_f32[player_index] +
            previous_floor_contact_bottom_extent_f32;
        if (reference_ecb_pose_f32(
                fighter,
                effective_action_state(
                    action_state,
                    scratch->hitlag_resume_action[player_index]),
                action_ticks,
                grounded,
                inherited_locked_bottom_y_f32,
                source_submotion,
                source_animation_frame_f32,
                fall_animation_blend_f32,
                fall_animation_target_switched,
                scratch->prone_orientation[player_index],
                scratch->prone_roll_motion_orientation[player_index],
                scratch->tech_direction[player_index],
                facing,
                total_velocity_f32(
                    velocity_x,
                    scratch->knockback_velocity_x_f32[player_index]),
                total_velocity_f32(
                    velocity_y,
                    scratch->knockback_velocity_y_f32[player_index]),
                INT32_C(0),
                NULL,
                &ceiling_pose) != 0)
        {
            /* HSD derives the ECB around Melee's fighter root. Canonical
             * positions use the body centre, which is half_height above
             * that root on the imported coordinate map. */
            ceiling_top_extent_f32 =
                ceiling_pose.top_y_from_origin_f32 -
                fighter->half_height_f32;
        }
        const float previous_top =
            position_y - ceiling_top_extent_f32;
        const int wall_tech_stalled =
            action_is_wall_tech(action_state) &&
            action_ticks < fighter->wall_tech_stall_ticks;
        float new_bottom;
        float new_floor_contact;
        float new_top;

        if (exact_wall_response_this_tick != 0)
        {
            velocity_y = INT32_C(0);
        }
        else if (wall_tech_stalled)
        {
            velocity_y = INT32_C(0);
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE &&
            action_ticks <
                fighter->air_dodge_ordinary_physics_begin_tick)
        {
            fast_fall = UINT8_C(0);
        }
        else if (launched_this_tick &&
                 !(ground_jump_entry_this_tick != 0 &&
                   (action_is_light_aerial(action_state) ||
                    action_state ==
                        (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK)))
        {
            fast_fall = UINT8_C(0);
        }
        else if (dropped_platform_this_tick)
        {
            fast_fall = UINT8_C(0);
        }
        else if (!hitstun_locked &&
            !dropped_platform_this_tick &&
            action_state !=
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK &&
            action_state !=
                (uint8_t)PF_M4_ACTION_VECTOR_ASCENT &&
            action_allows_fresh_fast_fall(
                action_state,
                action_ticks) != 0 &&
            !action_is_surface_tech(action_state) &&
            input->main_stick_y >=
                (int16_t)fighter->fast_fall_axis_threshold &&
            tilt_y_direction == INT8_C(1) &&
            tilt_y_age < fighter->fast_fall_input_window_ticks &&
            velocity_y > INT32_C(0))
        {
            velocity_y = fighter->fast_fall_speed_f32;
            fast_fall = UINT8_C(1);
        }
        else if (fast_fall != UINT8_C(0))
        {
            velocity_y = fighter->fast_fall_speed_f32;
        }
        else
        {
            velocity_y = approach(
                velocity_y,
                fighter->fall_speed_f32,
                fighter->gravity_f32);
        }

        next_position =
            exact_wall_response_this_tick != 0
                ? exact_wall_contact_position_y_f32
                : position_y +
                      (wall_tech_stalled
                           ? 0.0f
                           : velocity_y +
                                 scratch->knockback_velocity_y_f32[player_index]);
        position_y = next_position;
        new_bottom = position_y + fighter->half_height_f32;
        new_floor_contact =
            position_y + floor_contact_bottom_extent_f32_value;
        new_top = position_y - ceiling_top_extent_f32;

        if (total_velocity_f32(
                velocity_y,
                scratch->knockback_velocity_y_f32[player_index]) >=
                INT32_C(0) &&
            !(launched_this_tick != 0 &&
              action_state ==
                  (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND))
        {
            const float platform_center =
                platform_center_x_f32(
                    stage,
                    world->tick + UINT64_C(1));
            const float platform_left =
                platform_center - stage->platform_half_width_f32;
            const float platform_right =
                platform_center + stage->platform_half_width_f32;
            const float upper_platform_left =
                stage->upper_platform_center_x_f32 -
                stage->upper_platform_half_width_f32;
            const float upper_platform_right =
                stage->upper_platform_center_x_f32 +
                stage->upper_platform_half_width_f32;
            const int down_held =
                input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;
            const int pass_through_allowed =
                !down_held ||
                action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
            float landing_y_f32 = INFINITY;
            uint8_t landing_support =
                (uint8_t)PF_M4_SURFACE_NONE;

            if (stage->reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
            {
                (void)reference_stage_find_floor_landing(
                    content,
                    position_x,
                    previous_floor_contact,
                    new_floor_contact,
                    fast_fall,
                    sweep_policy,
                    pass_through_allowed,
                    platform_drop_ticks,
                    &landing_y_f32,
                    &landing_support);
            }
            else
            {
                if (body_overlaps_horizontal_interval(
                        position_x,
                        fighter->half_width_f32,
                        stage->solid_left_f32,
                        stage->solid_right_f32) &&
                    previous_bottom <= stage->solid_top_f32 &&
                    new_bottom >= stage->solid_top_f32)
                {
                    landing_y_f32 = stage->solid_top_f32;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_SOLID_TOP;
                }
                if (pass_through_allowed != 0 &&
                    platform_drop_ticks == UINT8_C(0) &&
                    position_x >= platform_left &&
                    position_x <= platform_right &&
                    floor_sweep_crosses_surface(
                        previous_bottom,
                        new_bottom,
                        stage->platform_y_f32,
                        1,
                        fast_fall,
                        sweep_policy) &&
                    stage->platform_y_f32 < landing_y_f32)
                {
                    landing_y_f32 = stage->platform_y_f32;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_PLATFORM;
                }
                if (pass_through_allowed != 0 &&
                    platform_drop_ticks == UINT8_C(0) &&
                    position_x >= upper_platform_left &&
                    position_x <= upper_platform_right &&
                    floor_sweep_crosses_surface(
                        previous_bottom,
                        new_bottom,
                        stage->upper_platform_y_f32,
                        1,
                        fast_fall,
                        sweep_policy) &&
                    stage->upper_platform_y_f32 < landing_y_f32)
                {
                    landing_y_f32 = stage->upper_platform_y_f32;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
                }
                if (position_x >= stage->floor_left_f32 &&
                    position_x <= stage->floor_right_f32 &&
                    floor_sweep_crosses_surface(
                        previous_floor_contact,
                        new_floor_contact,
                        stage->floor_y_f32,
                        0,
                        fast_fall,
                        sweep_policy) &&
                    stage->floor_y_f32 < landing_y_f32)
                {
                    landing_y_f32 = stage->floor_y_f32;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_FLOOR;
                }
            }
            if (landing_support != (uint8_t)PF_M4_SURFACE_NONE)
            {
                const int falcon_punch_landing =
                    action_state ==
                        (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR;
                const uint16_t falcon_punch_action_ticks =
                    action_ticks;

                land_from_air(
                    content,
                    landing_y_f32,
                    landing_support,
                    input->main_stick_x,
                    facing,
                    scratch,
                    player_index,
                    &position_y,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &source_submotion,
                    &grounded,
                    &action_state,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &fast_fall,
                    &dash_direction);
                if (grounded != UINT8_C(0))
                {
                    recovery_available = UINT8_C(1);
                }
                if (falcon_punch_landing != 0)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_FALCON_PUNCH_GROUND;
                    action_ticks = falcon_punch_action_ticks;
                }
            }
        }
        else
        {
            float ceiling_y_f32 = INT32_C(0);
            uint8_t ceiling_support = UINT8_C(0);
            const int hit_ceiling =
                exact_wall_response_this_tick != 0
                    ? 0
                    : stage->reference_collision_profile !=
                        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED
                    ? ssbm_reference_stage_find_ceiling_contact(
                          stage->reference_collision_profile,
                          position_x,
                          previous_top,
                          new_top,
                          &ceiling_y_f32,
                          &ceiling_support)
                    : body_overlaps_horizontal_interval(
                          position_x,
                          fighter->half_width_f32,
                          stage->solid_left_f32,
                          stage->solid_right_f32) &&
                          previous_top >= stage->solid_bottom_f32 &&
                          new_top <= stage->solid_bottom_f32;

            (void)ceiling_support;
            if (hit_ceiling != 0)
            {
                if (stage->reference_collision_profile ==
                    (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
                {
                    ceiling_y_f32 = stage->solid_bottom_f32;
                }
                position_y = ceiling_y_f32 + ceiling_top_extent_f32;
                if (scratch->tumble[player_index] != UINT8_C(0))
                {
                    if ((!action_is_surface_bounce(action_state) ||
                         action_ticks >= fighter
                                             ->surface_bounce_collision_lockout_ticks) &&
                        scratch->knockback_velocity_y_f32[player_index] <
                            -fighter->surface_collision_threshold_y_f32)
                    {
                        const ssbm_stage_collision_line *ceiling_line =
                            ceiling_support != UINT8_C(0)
                                ? ssbm_reference_stage_line(
                                      stage->reference_collision_profile,
                                      ceiling_support)
                                : NULL;
                        const float source_normal_x_f32 =
                            ceiling_line != NULL
                                ? ceiling_line->source_normal_x_f32
                                : INT32_C(0);
                        const float source_normal_y_f32 =
                            ceiling_line != NULL
                                ? ceiling_line->source_normal_y_f32
                                : 1.0f;

                        status = enter_ceiling_impact(
                            fighter,
                            source_normal_x_f32,
                            source_normal_y_f32,
                            scratch,
                            player_index,
                            &velocity_x,
                            &velocity_y,
                            &action_ticks,
                            &action_state,
                            &fast_fall);
                        if (status != PF_STATUS_OK)
                        {
                            return status;
                        }
                    }
                    else
                    {
                        velocity_y = INT32_C(0);
                    }
                }
                else
                {
                    velocity_y = INT32_C(0);
                }
            }
        }
    }

    if (action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
    {
        if (scratch->hitstun_ticks[player_index] > UINT16_C(0))
        {
            --scratch->hitstun_ticks[player_index];
        }
        if (action_ticks >= fighter->reset_bound_ticks)
        {
            scratch->hitstun_ticks[player_index] = UINT16_C(0);
            scratch->tumble[player_index] = UINT8_C(0);
            action_state =
                grounded != UINT8_C(0)
                    ? (uint8_t)PF_M4_ACTION_FORCED_GETUP
                    : (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (hitstun_locked &&
        !action_is_surface_tech(action_state))
    {
        if (scratch->hitstun_ticks[player_index] > UINT16_C(0))
        {
            --scratch->hitstun_ticks[player_index];
        }
        if (grounded != UINT8_C(0) &&
            action_is_ground_damage(action_state))
        {
            status = advance_ground_damage_animation(
                &action_state,
                &action_ticks,
                scratch->hitstun_ticks[player_index],
                &scratch
                     ->ground_knockback_velocity_f32[player_index]);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
    }

    {
        const int8_t ledge_probe_direction_value =
            ledge_probe_direction(
                action_state,
                action_ticks,
                facing);

        if (!ledge_motion_handled &&
            !released_ledge_this_tick &&
            grounded == UINT8_C(0) &&
            ledge_probe_direction_value !=
                (int8_t)PF_M4_LEDGE_PROBE_NONE &&
            platform_drop_ticks == UINT8_C(0))
        {
            if (try_grab_ledge(
                    content,
                    world,
                    scratch,
                    player_index,
                    &position_x,
                    &position_y,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &source_submotion,
                    &grounded,
                    &action_state,
                    &support,
                    &air_jumps_remaining,
                    &short_hop_latched,
                    &fast_fall,
                    &scratch->ledge_invulnerability_ticks[player_index],
                    scratch->ledge_regrab_lockout_ticks[player_index],
                    action_state,
                    action_ticks,
                    previous_position_x,
                    ledge_probe_direction_value,
                    &facing,
                    input->main_stick_y,
                    reference_ledge_response,
                    &dash_direction))
            {
                source_submotion =
                    fighter->reference_frame_data_enabled != UINT8_C(0)
                        ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH
                        : (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
                scratch->knockback_velocity_x_f32[player_index] =
                    INT32_C(0);
                scratch->knockback_velocity_y_f32[player_index] =
                    INT32_C(0);
                scratch->ground_knockback_velocity_f32[player_index] =
                    INT32_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
                scratch->tech_direction[player_index] = INT8_C(0);
                recovery_available = UINT8_C(1);
            }
        }
    }

    /*
     * ftCo_800D3158 compares Fighter.cur_pos against the stage blast
     * bounds.  Imported stage Y coordinates use that source-root origin,
     * while the canonical simulation position is the body center.
     */
    const float source_root_y_f32 =
        position_y + fighter->half_height_f32;

    if (position_x < stage->blast_left_f32 ||
        position_x > stage->blast_right_f32 ||
        source_root_y_f32 < stage->blast_top_f32 ||
        source_root_y_f32 > stage->blast_bottom_f32)
    {
        const float ko_damage_f32 =
            scratch->damage_f32[player_index];
        const float ko_velocity_x_f32 = velocity_x;
        const float ko_velocity_y_f32 = velocity_y;
        const uint8_t ko_source_player =
            scratch->last_hit_sequence[player_index] != UINT32_C(0) &&
                    scratch->last_hit_attacker[player_index] <
                        world->player_count
                ? scratch->last_hit_attacker[player_index]
                : PF_SIM_EVENT_NO_PLAYER;
        uint16_t event_flags = UINT16_C(0);

        if (scratch->match_falls[player_index] != UINT16_MAX)
        {
            ++scratch->match_falls[player_index];
        }
        if (ko_source_player != PF_SIM_EVENT_NO_PLAYER &&
            ko_source_player != (uint8_t)player_index &&
            (world->mode != (uint8_t)PF_SIM_MODE_TEAMS ||
             world->team[ko_source_player] !=
                 world->team[player_index]))
        {
            if (scratch->match_kos[ko_source_player] != UINT16_MAX)
            {
                ++scratch->match_kos[ko_source_player];
            }
        }

        /*
         * Melee's Dead* states preserve the fighter's death position and
         * displayed damage until the delayed Rebirth entry.  The stored
         * spawn-platform position is consumed by ftCo_800D4FF4 only when
         * Rebirth begins; resetting and teleporting here makes the entire
         * 60-frame dead interval observably wrong.
         */
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        action_ticks = UINT16_C(0);
        source_submotion =
            fighter->reference_frame_data_enabled != UINT8_C(0)
                ? (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT
                : UINT16_C(0);
        grounded = UINT8_C(0);
        support = (uint8_t)PF_M4_SURFACE_NONE;
        short_hop_latched = UINT8_C(0);
        platform_drop_ticks = UINT8_C(0);
        fast_fall = UINT8_C(0);
        dash_direction = INT8_C(0);
        previous_strong_direction = INT8_C(0);
        directional_input_flags = UINT8_C(0);
        scratch->knockback_velocity_x_f32[player_index] = INT32_C(0);
        scratch->knockback_velocity_y_f32[player_index] = INT32_C(0);
        scratch->ground_knockback_velocity_f32[player_index] = INT32_C(0);
        scratch->hitlag_ticks[player_index] = UINT16_C(0);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->shield_stun_ticks[player_index] = UINT16_C(0);
        recovery_available = UINT8_C(1);
        if (respawn_count != UINT16_MAX)
        {
            ++respawn_count;
        }
        if (world->stock_count != UINT8_C(0) &&
            scratch->stocks_remaining[player_index] > UINT8_C(0))
        {
            --scratch->stocks_remaining[player_index];
        }
        scratch->active[player_index] = UINT8_C(0);
        scratch->respawn_invulnerability_ticks[player_index] =
            UINT16_C(0);
        grounded = UINT8_C(0);
        support = (uint8_t)PF_M4_SURFACE_NONE;
        if (world->stock_count != UINT8_C(0) &&
            scratch->stocks_remaining[player_index] == UINT8_C(0))
        {
            scratch->respawn_ticks[player_index] = UINT16_C(0);
            action_state = (uint8_t)PF_M4_ACTION_ELIMINATED;
        }
        else
        {
            scratch->respawn_ticks[player_index] =
                world->respawn_delay_config_ticks != UINT16_C(0)
                    ? world->respawn_delay_config_ticks
                    : UINT16_C(1);
            action_state = (uint8_t)PF_M4_ACTION_RESPAWN_WAIT;
        }
        if (world->stock_count != UINT8_C(0) &&
            scratch->stocks_remaining[player_index] == UINT8_C(0))
        {
            event_flags |=
                (uint16_t)PF_SIM_EVENT_FLAG_ELIMINATED |
                (uint16_t)PF_SIM_EVENT_FLAG_LAST_STOCK;
        }
        if (world->sudden_death != UINT8_C(0))
        {
            event_flags |=
                (uint16_t)PF_SIM_EVENT_FLAG_SUDDEN_DEATH;
        }
        status = pf_sim_push_event(
            scratch,
            world->tick,
            PF_SIM_EVENT_KO,
            ko_source_player,
            (uint8_t)player_index,
            ko_damage_f32,
            ko_velocity_x_f32,
            ko_velocity_y_f32,
            event_flags,
            (uint16_t)scratch->stocks_remaining[player_index],
            NULL);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    else
    {
        previous_strong_direction = strong_direction_value;
    }

    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
        world->action_state[player_index] !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        if (world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
        {
            source_submotion =
                falcon_jump_submotion_from_x(
                    world->previous_main_stick_x[player_index],
                    facing,
                    0);
            action_ticks = UINT16_C(0);
            directional_input_flags = (uint8_t)(
                directional_input_flags &
                (uint8_t)~PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL);
        }
        else if (world->action_state[player_index] !=
                     (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
                 source_submotion !=
                     (uint16_t)PF_M4_FALCON_SUBMOTION_PLATFORM_DROP)
        {
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            action_ticks = UINT16_C(0);
        }
    }

    if (scratch->smash_charge_ticks[player_index] != UINT16_C(0) &&
        !action_is_smash_charge(action_state) &&
        !action_is_smash_release(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          action_is_smash_release(
              scratch->hitlag_resume_action[player_index])))
    {
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
    }

    if (!action_is_reference_jab_chain(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          action_is_reference_jab_chain(
              scratch->hitlag_resume_action[player_index])))
    {
        scratch->jab_chain_buffered[player_index] = UINT8_C(0);
        scratch->rapid_jab_input_count[player_index] = UINT8_C(0);
        scratch->rapid_jab_continue[player_index] = UINT8_C(0);
    }

    if (action_state != (uint8_t)PF_M4_ACTION_REBOUND_STOP &&
        action_state != (uint8_t)PF_M4_ACTION_REBOUND)
    {
        scratch->rebound_duration_ticks[player_index] = UINT16_C(0);
    }

    if (action_state != (uint8_t)PF_M4_ACTION_DOWN_ATTACK &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          scratch->hitlag_resume_action[player_index] ==
              (uint8_t)PF_M4_ACTION_DOWN_ATTACK))
    {
        scratch->down_tilt_repeat_buffered[player_index] = UINT8_C(0);
    }

    if (!action_is_damage(action_state) &&
        !action_is_surface_bounce(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          (action_is_damage(
               scratch->hitlag_resume_action[player_index]) ||
           action_is_surface_bounce(
               scratch->hitlag_resume_action[player_index]))))
    {
        scratch->damage_jump_buffer_ticks[player_index] = UINT16_C(0);
    }

    if (!action_retains_shield_strength(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        scratch->shield_strength[player_index] = UINT16_C(0);
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SPECIAL_LANDING)
    {
        /* LandingFallSpecial's rate/frame were advanced before collision so
         * that its exact HSD pose owns the sweep and any terminal GuardOn
         * blend. Preserve that clock through the final source-state pass. */
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        /* EscapeAir's source clock was advanced before collision so the
         * just-evaluated HSD ECB owns this update's floor sweep. */
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             (action_uses_fall_special_pose(
                  effective_action_state(
                      action_state,
                      scratch->hitlag_resume_action[player_index])) ||
              (effective_action_state(
                   action_state,
                   scratch->hitlag_resume_action[player_index]) ==
                   (uint8_t)PF_M4_ACTION_AIRBORNE)))
    {
        /* The common Fall animation callback runs before physics so its
         * clock and direction blend were already advanced above. */
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             (effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index]) ==
                  (uint8_t)PF_M4_ACTION_GRAB ||
              effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index]) ==
                  (uint8_t)PF_M4_ACTION_DASH_GRAB ||
              effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index]) ==
                  (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
              effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index]) ==
                  (uint8_t)PF_M4_ACTION_GRABBED ||
              action_is_throw(
                  effective_action_state(
                      action_state,
                      scratch->hitlag_resume_action[player_index]))))
    {
        /* Paired grab/throw actions own their source clocks above; the victim
         * half is synchronized after all players step. */
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             action_uses_ground_animation_clock(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        status = update_falcon_ground_animation_clock(
            fighter,
            rng_state,
            previous_action_state,
            previous_hitlag_resume_action,
            previous_source_submotion,
            previous_source_animation_frame_f32,
            previous_source_animation_rate_f32,
            previous_ground_velocity_f32,
            previous_facing,
            action_state,
            scratch->hitlag_resume_action[player_index],
            &source_submotion,
            &source_animation_frame_f32,
            &source_animation_rate_f32);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             action_is_damage(
                 effective_action_state(
                     action_state,
                     scratch->hitlag_resume_action[player_index])))
    {
        /* Damage's animation callback runs before physics and map collision;
         * its source clock was already advanced above so the same HSD pose
         * owns this update's collision sweep. */
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_SHIELD_STUN)
    {
        const uint8_t previous_effective_action =
            effective_action_state(
                previous_action_state,
                previous_hitlag_resume_action);

        if (previous_effective_action !=
                (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
            previous_source_animation_rate_f32 <= INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        source_animation_frame_f32 = previous_source_animation_frame_f32;
        source_animation_rate_f32 = previous_source_animation_rate_f32;
        if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
        {
            /* The new GuardSetOff motion and rate exist during hitlag, but
             * display/collision bones remain on the receiving guard pose. */
            source_submotion = previous_source_submotion;
        }
        else
        {
            const float next_frame_f32 =
                source_animation_frame_f32 + source_animation_rate_f32;

            if (!isfinite(next_frame_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF;
            source_animation_frame_f32 = next_frame_f32;
        }
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        const uint8_t previous_effective_action =
            effective_action_state(
                previous_action_state,
                previous_hitlag_resume_action);

        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA;
        if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_f32 =
                previous_source_animation_frame_f32;
        }
        else if (previous_effective_action ==
                 (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            if (!falcon_advance_loop_animation_f32(
                    source_submotion,
                    previous_source_animation_frame_f32,
                    (int32_t)1.0f,
                    &source_animation_frame_f32))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        else
        {
            source_animation_frame_f32 = INT32_C(0);
        }
        source_animation_rate_f32 = (int32_t)1.0f;
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             action_state == (uint8_t)PF_M4_ACTION_CROUCH)
    {
        const int previous_crouch =
            previous_action_state == (uint8_t)PF_M4_ACTION_CROUCH ||
            (previous_action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
             previous_hitlag_resume_action ==
                 (uint8_t)PF_M4_ACTION_CROUCH);
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT;
        if (previous_crouch == 0)
        {
            source_animation_frame_f32 = INT32_C(0);
        }
        else if (!falcon_advance_loop_animation_f32(
                     source_submotion,
                     previous_source_animation_frame_f32,
                     (int32_t)1.0f,
                     &source_animation_frame_f32))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        source_animation_rate_f32 = (int32_t)1.0f;
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE;
        if (action_state != (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_f32 =
                (int32_t)(action_ticks - UINT16_C(1)) * 1.0f;
        }
        source_animation_rate_f32 = 1.0f;
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             action_uses_direct_hsd_pose(
                 effective_action_state(
                     action_state,
                     scratch->hitlag_resume_action[player_index])))
    {
        const uint8_t effective_action = effective_action_state(
            action_state,
            scratch->hitlag_resume_action[player_index]);
        float direct_frame_f32 = INT32_C(0);

        if (!falcon_reference_direct_hsd_pose(
                effective_action,
                action_ticks,
                grounded,
                &source_submotion,
                &direct_frame_f32))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_state != (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_f32 = direct_frame_f32;
        }
        source_animation_rate_f32 = (int32_t)1.0f;
    }
    else
    {
        source_animation_frame_f32 = INT32_C(0);
        source_animation_rate_f32 = INT32_C(0);
        fall_animation_blend_f32 = INT32_C(0);
        fall_animation_target_switched = UINT8_C(0);
    }

    if (action_state == (uint8_t)PF_M4_ACTION_TAUNT)
    {
        source_submotion =
            facing > INT8_C(0)
                ? (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT
                : (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_LEFT;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        source_submotion =
            action_ticks < fighter->shield_minimum_hold_ticks
                ? (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON
                : (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_OFF;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN)
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_TURN;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND)
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_TURN_RUN;
    }

    if (!action_retains_source_submotion(
            action_state,
            scratch->hitlag_resume_action[player_index]) ||
        (fighter->reference_frame_data_enabled == UINT8_C(0) &&
         (action_uses_fall_special_pose(
              effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index])) ||
          action_uses_direct_hsd_pose(
              effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index])))))
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
    }

    /* Falcon's EscapeAir, SpecialAirS, grounded SpecialS edge conversion,
     * and SpecialHi routes all enter FallSpecial through ftCo_80096900. Its
     * airborne entry calls ftCommon_UseAllJumps; keeping the old jump count
     * made the compact state disagree even though this state has no usable
     * Falcon jump callback. Apply that common-entry side effect exactly once
     * when one of the represented FallSpecial identities is entered. */
    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_uses_fall_special_pose(action_state) &&
        !action_uses_fall_special_pose(
            world->action_state[player_index]))
    {
        air_jumps_remaining = UINT8_C(0);
    }

    /* xF0 is a ground-tangent scalar. Ground-to-air conversions keep the
     * already projected x8c launch velocity but must not retain xF0 as a
     * second, stale motion channel. */
    if (grounded == UINT8_C(0))
    {
        scratch->ground_knockback_velocity_f32[player_index] = INT32_C(0);
    }

    update_shield_tilt(
        scratch,
        input,
        player_index,
        action_state,
        scratch->hitlag_resume_action[player_index],
        facing);

    if (action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          (scratch->hitlag_resume_action[player_index] ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND ||
           scratch->hitlag_resume_action[player_index] ==
               (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND)))
    {
        scratch->falcon_kick_hit_count[player_index] = UINT8_C(0);
    }

    if (shield_recoil_x != INT32_C(0))
    {
        scratch->shield_recoil_x_f32[player_index] = shield_recoil_x;
    }
    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_HITLAG &&
        action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW)
    {
        if (!falcon_direct_hsd_locked_bottom_f32(
                action_state,
                (int32_t)1.0f,
                UINT8_C(0),
                &ecb_locked_bottom_y_f32) ||
            ecb_locked_bottom_y_f32 ==
                PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        ecb_bottom_lock_ticks = PF_M4_USE_ALL_JUMPS_ECB_LOCK_TICKS;
    }
    else if (action_state != (uint8_t)PF_M4_ACTION_HITLAG &&
             ecb_bottom_lock_ticks != UINT8_C(0) &&
             ecb_lock_entered_during_map == 0)
    {
        --ecb_bottom_lock_ticks;
    }
    if (grounded != UINT8_C(0) || ecb_bottom_lock_ticks == UINT8_C(0))
    {
        ecb_bottom_lock_ticks = UINT8_C(0);
        ecb_locked_bottom_y_f32 = INT32_C(0);
    }
    /* Run/Dash set GuardOn's x24 from common-data x68 after entering the
     * motion. GuardOn checks A before decrementing it on each later IASA
     * update; every other callback owner either cannot consume the field or
     * has changed the move-variable union. */
    if (source_ground_input == NULL ||
        action_state != (uint8_t)PF_M4_ACTION_SHIELD)
    {
        scratch->guard_dash_grab_window_ticks[player_index] = UINT8_C(0);
    }
    else if (guard_dash_grab_window_entered_this_tick == 0 &&
             callback_owner.action_state ==
                 (uint8_t)PF_M4_ACTION_SHIELD &&
             scratch->guard_dash_grab_window_ticks[player_index] !=
                 UINT8_C(0))
    {
        --scratch->guard_dash_grab_window_ticks[player_index];
    }
    if (source_ground_input == NULL ||
        action_state != (uint8_t)PF_M4_ACTION_CROUCH_START)
    {
        scratch->crouch_pass_pending_ticks[player_index] = UINT8_C(0);
    }
    write_scratch(
        scratch,
        player_index,
        input,
        position_x,
        position_y,
        velocity_x,
        velocity_y,
        action_ticks,
        source_submotion,
        source_animation_frame_f32,
        source_animation_rate_f32,
        respawn_count,
        grounded,
        action_state,
        support,
        air_jumps_remaining,
        recovery_available,
        short_hop_latched,
        platform_drop_ticks,
        fast_fall,
        facing,
            dash_direction,
            previous_strong_direction,
            directional_input_flags,
            tilt_x_direction,
            tilt_y_direction,
            tilt_x_age,
            tilt_y_age);
    scratch->fall_animation_blend_f32[player_index] =
        fall_animation_blend_f32;
    scratch->fall_animation_target_switched[player_index] =
        fall_animation_target_switched;
    scratch->ecb_bottom_lock_ticks[player_index] =
        ecb_bottom_lock_ticks;
    scratch->ecb_locked_bottom_y_f32[player_index] =
        ecb_locked_bottom_y_f32;
    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        status = evaluate_falcon_ground_blend_pose(
            world,
            player_index,
            scratch->action_state[player_index],
            scratch->hitlag_resume_action[player_index],
            scratch->source_submotion[player_index],
            scratch->source_animation_frame_f32[player_index],
            scratch->action_ticks[player_index],
            &scratch->ground_blend_pose[player_index],
            &scratch->ground_blend_progress_f32[player_index]);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    return PF_STATUS_OK;
}

pf_status inspect(
    const pf_sim *sim,
    struct inspection *out_inspection)
{
    const stage_data *stage;
    float platform_center;
    uint32_t player_index;

    if (out_inspection == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_inspection, 0, sizeof(*out_inspection));
    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    out_inspection->struct_size =
        (uint32_t)sizeof(*out_inspection);
    out_inspection->schema_version =
        PF_M4_INSPECTION_SCHEMA_VERSION;
    out_inspection->player_count = sim->world.player_count;
    out_inspection->stock_count = sim->world.stock_count;
    out_inspection->tick = sim->world.tick;
    out_inspection->respawn_delay_ticks =
        sim->world.respawn_delay_config_ticks;
    out_inspection->respawn_invulnerability_ticks =
        sim->world.respawn_invulnerability_config_ticks;
    out_inspection->sudden_death = sim->world.sudden_death;
    out_inspection->terminated = sim->world.terminated;
    out_inspection->truncated = sim->world.truncated;
    out_inspection->winner_mask = sim->world.winner_mask;

    stage = &sim->content.stage;
    platform_center =
        platform_center_x_f32(stage, sim->world.tick);
    out_inspection->stage.floor_left_f32 =
        stage->floor_left_f32;
    out_inspection->stage.floor_right_f32 =
        stage->floor_right_f32;
    out_inspection->stage.floor_y_f32 = stage->floor_y_f32;
    out_inspection->stage.platform_left_f32 =
        platform_center - stage->platform_half_width_f32;
    out_inspection->stage.platform_right_f32 =
        platform_center + stage->platform_half_width_f32;
    out_inspection->stage.platform_y_f32 =
        stage->platform_y_f32;
    out_inspection->stage.solid_left_f32 =
        stage->solid_left_f32;
    out_inspection->stage.solid_right_f32 =
        stage->solid_right_f32;
    out_inspection->stage.solid_top_f32 =
        stage->solid_top_f32;
    out_inspection->stage.solid_bottom_f32 =
        stage->solid_bottom_f32;
    out_inspection->stage.left_ledge_x_f32 =
        stage->floor_left_f32;
    out_inspection->stage.right_ledge_x_f32 =
        stage->floor_right_f32;
    out_inspection->stage.ledge_y_f32 = stage->floor_y_f32;
    out_inspection->stage.blast_left_f32 =
        stage->blast_left_f32;
    out_inspection->stage.blast_right_f32 =
        stage->blast_right_f32;
    out_inspection->stage.blast_top_f32 =
        stage->blast_top_f32;
    out_inspection->stage.blast_bottom_f32 =
        stage->blast_bottom_f32;
    out_inspection->stage.revival_platform_start_y_f32 =
        stage->revival_platform_start_y_f32;
    out_inspection->stage.revival_platform_end_y_f32 =
        stage->revival_platform_end_y_f32;
    out_inspection->stage.revival_platform_half_width_f32 =
        stage->revival_platform_half_width_f32;
    out_inspection->stage.revival_platform_descent_ticks =
        stage->revival_platform_descent_ticks;
    out_inspection->stage.revival_platform_hold_ticks =
        stage->revival_platform_hold_ticks;
    out_inspection->stage.upper_platform_left_f32 =
        stage->upper_platform_center_x_f32 -
        stage->upper_platform_half_width_f32;
    out_inspection->stage.upper_platform_right_f32 =
        stage->upper_platform_center_x_f32 +
        stage->upper_platform_half_width_f32;
    out_inspection->stage.upper_platform_y_f32 =
        stage->upper_platform_y_f32;
    out_inspection->item.position_x_f32 =
        sim->world.item_position_x_f32;
    out_inspection->item.position_y_f32 =
        sim->world.item_position_y_f32;
    out_inspection->item.velocity_x_f32 =
        sim->world.item_velocity_x_f32;
    out_inspection->item.velocity_y_f32 =
        sim->world.item_velocity_y_f32;
    out_inspection->item.lifetime_ticks =
        sim->world.item_lifetime_ticks;
    out_inspection->item.respawn_ticks =
        sim->world.item_respawn_ticks;
    out_inspection->item.pickup_lockout_ticks =
        sim->world.item_pickup_lockout_ticks;
    out_inspection->item.enabled = sim->content.item.enabled;
    out_inspection->item.state = sim->world.item_state;
    out_inspection->item.holder =
        sim->world.item_holder_slot != UINT8_C(0)
            ? (uint8_t)(sim->world.item_holder_slot - UINT8_C(1))
            : PF_SIM_EVENT_NO_PLAYER;
    out_inspection->item.source =
        sim->world.item_source_slot != UINT8_C(0)
            ? (uint8_t)(sim->world.item_source_slot - UINT8_C(1))
            : PF_SIM_EVENT_NO_PLAYER;
    out_inspection->item.throw_direction =
        sim->world.item_throw_direction;
    out_inspection->item.hit_mask = sim->world.item_hit_mask;
    out_inspection->item.stale_registered =
        sim->world.item_stale_registered;
    out_inspection->item.hitbox_active =
        sim->world.item_state ==
                (uint8_t)PF_M4_ITEM_STATE_AIRBORNE &&
            sim->world.item_source_slot != UINT8_C(0)
        ? UINT8_C(1)
        : UINT8_C(0);
    out_inspection->projectile.position_x_f32 =
        sim->world.projectile_position_x_f32;
    out_inspection->projectile.position_y_f32 =
        sim->world.projectile_position_y_f32;
    out_inspection->projectile.velocity_x_f32 =
        sim->world.projectile_velocity_x_f32;
    out_inspection->projectile.velocity_y_f32 =
        sim->world.projectile_velocity_y_f32;
    out_inspection->projectile.hitbox_left_f32 =
        sim->world.projectile_position_x_f32 -
        sim->content.projectile.half_width_f32;
    out_inspection->projectile.hitbox_right_f32 =
        sim->world.projectile_position_x_f32 +
        sim->content.projectile.half_width_f32;
    out_inspection->projectile.hitbox_top_f32 =
        sim->world.projectile_position_y_f32 -
        sim->content.projectile.half_height_f32;
    out_inspection->projectile.hitbox_bottom_f32 =
        sim->world.projectile_position_y_f32 +
        sim->content.projectile.half_height_f32;
    out_inspection->projectile.lifetime_ticks =
        sim->world.projectile_lifetime_ticks;
    out_inspection->projectile.enabled =
        sim->content.projectile.enabled;
    out_inspection->projectile.state =
        sim->world.projectile_state;
    out_inspection->projectile.owner =
        sim->world.projectile_owner_slot != UINT8_C(0)
            ? (uint8_t)(
                  sim->world.projectile_owner_slot - UINT8_C(1))
            : PF_SIM_EVENT_NO_PLAYER;
    out_inspection->projectile.hitbox_active =
        sim->world.projectile_state ==
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE
            ? UINT8_C(1)
            : UINT8_C(0);

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        player_inspection *player =
            &out_inspection->players[player_index];
        hsd_local_pose
            ground_loop_pose[PF_M4_HSD_POSE_MAX_JOINTS];
        const hsd_local_pose *ground_loop_pose_or_null = NULL;

        player->position_x_f32 =
            sim->world.position_x_f32[player_index];
        player->position_y_f32 =
            sim->world.position_y_f32[player_index];
        player->self_velocity_x_f32 =
            sim->world.velocity_x_f32[player_index];
        player->self_velocity_y_f32 =
            sim->world.velocity_y_f32[player_index];
        if (sim->world.grounded[player_index] != UINT8_C(0) &&
            sim->content.stage.reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED &&
            !(sim->world.action_state[player_index] ==
                  (uint8_t)PF_M4_ACTION_LANDING &&
              sim->world.action_ticks[player_index] == UINT16_C(0)) &&
            sim->world.action_ticks[player_index] + UINT16_C(1) !=
                falcon_reference_ledge_option_ground_frame(
                    sim->world.source_submotion[player_index]) &&
            !(sim->world.action_ticks[player_index] == UINT16_C(0) &&
              (sim->world.action_state[player_index] ==
                   (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
               action_is_surface_tech(
                   sim->world.action_state[player_index]))))
        {
            project_ground_scalar_f32(
                &sim->content,
                sim->world.support[player_index],
                sim->world.velocity_x_f32[player_index],
                &player->self_velocity_x_f32,
                &player->self_velocity_y_f32);
        }
        player->velocity_x_f32 =
            total_velocity_f32(
                player->self_velocity_x_f32,
                sim->world.knockback_velocity_x_f32[player_index]);
        player->velocity_y_f32 =
            total_velocity_f32(
                player->self_velocity_y_f32,
                sim->world.knockback_velocity_y_f32[player_index]);
        player->knockback_velocity_x_f32 =
            sim->world.knockback_velocity_x_f32[player_index];
        player->knockback_velocity_y_f32 =
            sim->world.knockback_velocity_y_f32[player_index];
        player->ground_knockback_velocity_f32 =
            sim->world.ground_knockback_velocity_f32[player_index];
        player->shield_recoil_x_f32 =
            sim->world.shield_recoil_x_f32[player_index];
        player->source_animation_frame_f32 =
            sim->world.source_animation_frame_f32[player_index];
        player->source_animation_rate_f32 =
            sim->world.source_animation_rate_f32[player_index];
        player->fall_animation_blend_f32 =
            sim->world.fall_animation_blend_f32[player_index];
        player->fall_animation_target_switched =
            sim->world.fall_animation_target_switched[player_index];
        {
            falcon_ecb_pose_f32 ecb_pose;
            const uint8_t effective_action = effective_action_state(
                sim->world.action_state[player_index],
                sim->world.hitlag_resume_action[player_index]);

            if (reference_ecb_pose_f32(
                    &sim->content.fighter,
                    effective_action,
                    sim->world.action_ticks[player_index],
                    sim->world.grounded[player_index],
                    sim->world.ecb_bottom_lock_ticks[player_index] !=
                            UINT8_C(0)
                        ? sim->world.ecb_locked_bottom_y_f32[player_index]
                        : PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
                    sim->world.source_submotion[player_index],
                    sim->world.source_animation_frame_f32[player_index],
                    sim->world.fall_animation_blend_f32[player_index],
                    sim->world.fall_animation_target_switched[player_index],
                    sim->world.prone_orientation[player_index],
                    sim->world.prone_roll_motion_orientation[player_index],
                    sim->world.tech_direction[player_index],
                    sim->world.facing[player_index],
                    player->velocity_x_f32,
                    player->velocity_y_f32,
                    sim->world.ground_blend_progress_f32[player_index],
                    sim->world.ground_blend_progress_f32[player_index] >
                            INT32_C(0)
                        ? &sim->world.ground_blend_pose[player_index]
                        : NULL,
                    &ecb_pose))
            {
                player->ecb_bottom_y_from_origin_f32 =
                    ecb_pose.bottom_y_from_origin_f32;
            }
        }
        player->action_ticks =
            sim->world.action_ticks[player_index];
        player->source_submotion =
            sim->world.source_submotion[player_index];
        player->respawn_count =
            sim->world.respawn_count[player_index];
        player->action_state =
            sim->world.action_state[player_index];
        player->hitlag_resume_action =
            sim->world.hitlag_resume_action[player_index];
        player->facing = sim->world.facing[player_index];
        player->dash_direction =
            sim->world.dash_direction[player_index] < INT8_C(0)
                ? INT8_C(-1)
                : sim->world.dash_direction[player_index] > INT8_C(0)
                    ? INT8_C(1)
                    : INT8_C(0);
        player->previous_strong_direction =
            sim->world.previous_strong_direction[player_index];
        player->tilt_x_age = sim->world.tilt_x_age[player_index];
        player->grounded = sim->world.grounded[player_index];
        player->support = sim->world.support[player_index];
        player->air_jumps_remaining =
            sim->world.air_jumps_remaining[player_index];
        player->fast_fall =
            sim->world.fast_fall[player_index];
        player->short_hop_latched =
            sim->world.short_hop_latched[player_index];
        player->platform_drop_ticks =
            sim->world.platform_drop_ticks[player_index];
        player->active = sim->world.active[player_index];
        player->ledge = ledge_from_state(
            player->action_state,
            sim->world.hitlag_resume_action[player_index],
            player->facing);
        player->last_hit_tick =
            sim->world.last_hit_tick[player_index];
        player->damage_f32 =
            sim->world.damage_f32[player_index];
        player->last_hit_sequence =
            sim->world.last_hit_sequence[player_index];
        player->last_hit_damage_f32 =
            sim->world.last_hit_damage_f32[player_index];
        player->hitlag_ticks =
            sim->world.hitlag_ticks[player_index];
        player->hitstun_ticks =
            sim->world.hitstun_ticks[player_index];
        player->tech_window_ticks =
            sim->world.tech_window_ticks[player_index];
        player->tech_lockout_ticks =
            sim->world.tech_lockout_ticks[player_index];
        player->shield_stun_ticks =
            sim->world.shield_stun_ticks[player_index];
        player->attack_hit_mask =
            sim->world.attack_hit_mask[player_index];
        player->last_hit_valid =
            player->last_hit_sequence != UINT32_C(0)
                ? UINT8_C(1)
                : UINT8_C(0);
        player->last_hit_attacker =
            sim->world.last_hit_attacker[player_index];
        player->shield_held =
            (sim->world.shield_held[player_index] &
             PF_M4_TRIGGER_STATE_HELD_MASK) != UINT8_C(0)
                ? UINT8_C(1)
                : UINT8_C(0);
        player->trigger_input_age =
            sim->world.trigger_input_age[player_index];
        player->l_cancel_eligible =
            player->trigger_input_age <
                    sim->content.fighter.l_cancel_window_ticks
                ? UINT8_C(1)
                : UINT8_C(0);
        player->powershield =
            sim->world.powershield[player_index];
        player->tumble = sim->world.tumble[player_index];
        player->invulnerable =
            action_is_match_entry(player->action_state) ||
                sim->world.respawn_invulnerability_ticks[player_index] !=
                    UINT16_C(0) ||
                sim->world.ledge_invulnerability_ticks[player_index] !=
                    UINT16_C(0) ||
                action_is_recovery_invulnerable(
                    &sim->content.fighter,
                    player->action_state,
                    player->action_ticks,
                    sim->world.prone_orientation[player_index],
                    sim->world
                        .prone_roll_motion_orientation[player_index],
                    sim->world.tech_direction[player_index],
                    player->facing) ||
                (sim->content.fighter.reference_frame_data_enabled !=
                     UINT8_C(0) &&
                 falcon_reference_body_invulnerable(
                     sim->world.source_submotion[player_index],
                     player->action_ticks))
                ? UINT8_C(1)
                : UINT8_C(0);
        player->sdi_pulse_count =
            sim->world.sdi_pulse_count[player_index];
        player->sdi_direction_x =
            sim->world.sdi_direction_x[player_index];
        player->sdi_direction_y =
            sim->world.sdi_direction_y[player_index];
        player->tech_direction =
            sim->world.tech_direction[player_index];
        player->prone_orientation =
            sim->world.prone_orientation[player_index];
        player->shield_health_f32 =
            sim->world.shield_health_f32[player_index];
        player->respawn_ticks =
            sim->world.respawn_ticks[player_index];
        player->respawn_invulnerability_ticks =
            sim->world
                .respawn_invulnerability_ticks[player_index];
        player->ledge_invulnerability_ticks =
            sim->world.ledge_invulnerability_ticks[player_index];
        player->ledge_regrab_lockout_ticks =
            sim->world.ledge_regrab_lockout_ticks[player_index];
        player->grab_escape_ticks =
            sim->world.grab_escape_ticks[player_index];
        player->charge_ticks =
            sim->world.charge_ticks[player_index];
        player->smash_charge_ticks =
            sim->world.smash_charge_ticks[player_index];
        player->shield_strength =
            sim->world.shield_strength[player_index];
        player->shield_angle_turn =
            sim->world.shield_angle_turn[player_index];
        player->shield_magnitude =
            sim->world.shield_magnitude[player_index];
        shield_tilt_axes(
            sim->world.shield_angle_turn[player_index],
            sim->world.shield_magnitude[player_index],
            player->facing,
            &player->shield_tilt_x,
            &player->shield_tilt_y);
        player->shield_active = (uint8_t)shield_box(
            &sim->content.fighter,
            player->position_x_f32,
            player->position_y_f32,
            player->action_state,
            sim->world.hitlag_resume_action[player_index],
            player->shield_health_f32,
            player->shield_strength,
            player->facing,
            sim->world.shield_angle_turn[player_index],
            sim->world.shield_magnitude[player_index],
            &player->shield_left_f32,
            &player->shield_right_f32,
            &player->shield_top_f32,
            &player->shield_bottom_f32);
        player->revival_platform_active =
            player->action_state ==
                    (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM
                ? UINT8_C(1)
                : UINT8_C(0);
        if (player->revival_platform_active != UINT8_C(0))
        {
            player->revival_platform_left_f32 =
                player->position_x_f32 -
                stage->revival_platform_half_width_f32;
            player->revival_platform_right_f32 =
                player->position_x_f32 +
                stage->revival_platform_half_width_f32;
            player->revival_platform_y_f32 =
                player->position_y_f32 +
                sim->content.fighter.half_height_f32;
        }
        player->stale_move_count =
            sim->world.stale_move_count[player_index];
        if (player->stale_move_count == UINT8_C(0))
        {
            player->stale_move_multiplier_f32 =
                (uint32_t)1.0f;
        }
        else
        {
            const uint8_t current_action =
                player->action_state == (uint8_t)PF_M4_ACTION_HITLAG
                    ? sim->world.hitlag_resume_action[player_index]
                    : player->action_state;

            player->stale_move_multiplier_f32 =
                stale_move_multiplier_f32(
                    &sim->content.fighter,
                    sim->world.stale_move_ids[player_index],
                    sim->world.stale_move_count[player_index],
                    stale_move_id_for_action(current_action));
            (void)memcpy(
                player->stale_move_ids,
                sim->world.stale_move_ids[player_index],
                (size_t)player->stale_move_count);
        }
        player->attack_stale_registered =
            sim->world.attack_stale_registered[player_index];
        player->grab_target =
            sim->world.grab_target_slot[player_index] != UINT8_C(0)
                ? (uint8_t)(
                      sim->world.grab_target_slot[player_index] -
                      UINT8_C(1))
                : PF_SIM_EVENT_NO_PLAYER;
        player->grab_owner =
            sim->world.grab_owner_slot[player_index] != UINT8_C(0)
                ? (uint8_t)(
                      sim->world.grab_owner_slot[player_index] -
                      UINT8_C(1))
                : PF_SIM_EVENT_NO_PLAYER;
        player->stocks_remaining =
            sim->world.stocks_remaining[player_index];
        player->recovery_available =
            sim->world.recovery_available[player_index];
        player->hitbox_active = (uint8_t)attack_hitbox(
            &sim->content,
            player->position_x_f32,
            player->position_y_f32,
            player->facing,
            player->action_state,
            player->action_ticks,
            sim->world.source_submotion[player_index],
            &player->hitbox_left_f32,
            &player->hitbox_right_f32,
            &player->hitbox_top_f32,
            &player->hitbox_bottom_f32);
        player->hit_sphere_count = attack_hit_spheres(
            &sim->content,
            player->position_x_f32,
            player->position_y_f32,
            player->facing,
            player->action_state,
            player->action_ticks,
            player->hit_spheres);
        player->grabbox_active = (uint8_t)grabbox(
            &sim->content,
            player->position_x_f32,
            player->position_y_f32,
            player->facing,
            player->action_state,
            player->action_ticks,
            &player->grabbox_left_f32,
            &player->grabbox_right_f32,
            &player->grabbox_top_f32,
            &player->grabbox_bottom_f32);
        if (sim->world.ground_blend_progress_f32[player_index] >
            INT32_C(0))
        {
            const hsd_pose_data *data =
                falcon_reference_hsd_pose_data();
            if (data == NULL ||
                !falcon_resolve_compact_hsd_pose(
                    sim->world.source_submotion[player_index],
                    sim->world.source_animation_frame_f32[player_index],
                    sim->world.ground_blend_progress_f32[player_index],
                    &sim->world.ground_blend_pose[player_index],
                    ground_loop_pose))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            ground_loop_pose_or_null = ground_loop_pose;
        }
        player->hurt_capsule_count =
            action_is_match_entry(player->action_state)
                ? UINT8_C(0)
                : reference_world_hurt_capsules(
                &sim->content.fighter,
                player->position_x_f32,
                player->position_y_f32,
                player->facing,
                sim->world.dash_direction[player_index],
                player->grounded,
                player->action_state,
                player->hitlag_resume_action,
                sim->world.source_submotion[player_index],
                sim->world.source_animation_frame_f32[player_index],
                player->action_ticks,
                player->velocity_x_f32,
                player->velocity_y_f32,
                ground_loop_pose_or_null,
                player->hurt_capsules);
    }
    return PF_STATUS_OK;
}
