#include "sim_internal.h"
#include "sim_falcon_frame_data.h"
#include "sim_ssbm_common_data.h"
#include "sim_ssbm_damage.h"
#include "sim_ssbm_stage_data.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static int pf_m4_checked_i32(int64_t value, int32_t *out_value)
{
    if (value < (int64_t)INT32_MIN || value > (int64_t)INT32_MAX)
    {
        return 0;
    }
    *out_value = (int32_t)value;
    return 1;
}

static int32_t pf_m4_approach(
    int32_t value,
    int32_t target,
    int32_t amount)
{
    if (value < target)
    {
        const int64_t next = (int64_t)value + (int64_t)amount;
        return next > (int64_t)target ? target : (int32_t)next;
    }
    if (value > target)
    {
        const int64_t next = (int64_t)value - (int64_t)amount;
        return next < (int64_t)target ? target : (int32_t)next;
    }
    return value;
}

static int pf_m4_update_mash_stick_direction(
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

static uint32_t pf_m4_grab_mash_pulses(
    const pf_m4_fighter_data *fighter,
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
        pf_m4_update_mash_stick_direction(
            input->main_stick_x,
            fighter->mash_stick_axis_threshold,
            &scratch->mash_stick_x_direction[player_index]) |
        pf_m4_update_mash_stick_direction(
            input->main_stick_y,
            fighter->mash_stick_axis_threshold,
            &scratch->mash_stick_y_direction[player_index]);

    return (uint32_t)digital_pulse + (uint32_t)stick_pulse;
}

static uint16_t pf_m4_falcon_jump_submotion(
    const pf_input_frame *input,
    int8_t facing,
    int aerial)
{
    const int32_t relative_axis =
        (int32_t)input->main_stick_x * (int32_t)facing;
    const int32_t backward_threshold =
        -(int32_t)
            pf_m4_ssbm_common_reference_jump_backward_axis_threshold();
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

static int pf_m4_falcon_direct_hsd_locked_bottom_q16(
    uint8_t action_state,
    int32_t source_animation_frame_q16,
    uint8_t grounded,
    int32_t *out_locked_bottom_y_q16)
{
    pf_m4_falcon_ecb_pose_q16 source_pose;

    if (out_locked_bottom_y_q16 == NULL)
    {
        return 0;
    }
    *out_locked_bottom_y_q16 =
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16;
    if (action_state == (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_AIR &&
        source_animation_frame_q16 <
            INT32_C(5) * (int32_t)PF_Q16_ONE)
    {
        *out_locked_bottom_y_q16 = INT32_C(0);
    }
    else if (action_state ==
                 (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND &&
             grounded == UINT8_C(0) &&
             source_animation_frame_q16 >=
                 INT32_C(14) * (int32_t)PF_Q16_ONE &&
             source_animation_frame_q16 <=
                 INT32_C(17) * (int32_t)PF_Q16_ONE)
    {
        *out_locked_bottom_y_q16 = INT32_C(0);
    }
    else if (action_state ==
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
    {
        if (source_animation_frame_q16 <=
            INT32_C(5) * (int32_t)PF_Q16_ONE)
        {
            *out_locked_bottom_y_q16 = INT32_C(0);
        }
        else if (source_animation_frame_q16 >=
                     INT32_C(14) * (int32_t)PF_Q16_ONE &&
                 source_animation_frame_q16 <=
                     INT32_C(17) * (int32_t)PF_Q16_ONE)
        {
            if (!pf_m4_falcon_reference_hsd_ecb_pose(
                    (uint16_t)
                        PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR,
                    INT32_C(13) * (int32_t)PF_Q16_ONE,
                    0,
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                    &source_pose))
            {
                return 0;
            }
            *out_locked_bottom_y_q16 =
                source_pose.bottom_y_from_origin_q16;
        }
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW &&
             source_animation_frame_q16 >= (int32_t)PF_Q16_ONE)
    {
        if (!pf_m4_falcon_reference_hsd_ecb_pose(
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_THROW,
                INT32_C(0),
                0,
                PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                &source_pose))
        {
            return 0;
        }
        *out_locked_bottom_y_q16 =
            source_pose.bottom_y_from_origin_q16;
    }
    return 1;
}

static int pf_m4_falcon_walk_submotion(uint16_t source_submotion)
{
    return source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_SLOW ||
           source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_MIDDLE ||
           source_submotion ==
               (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_FAST;
}

static uint16_t pf_m4_falcon_walk_submotion_for_velocity(
    const pf_m4_fighter_data *fighter,
    int32_t ground_velocity_q16)
{
    const int32_t speed_q16 =
        ground_velocity_q16 < INT32_C(0)
            ? -ground_velocity_q16
            : ground_velocity_q16;
    const int32_t middle_threshold_q16 = pf_m4_multiply_q16(
        fighter->walk_speed_q16,
        (int32_t)fighter->walk_middle_speed_ratio_q16);
    const int32_t fast_threshold_q16 = pf_m4_multiply_q16(
        fighter->walk_speed_q16,
        (int32_t)fighter->walk_fast_speed_ratio_q16);

    if (speed_q16 >= fast_threshold_q16)
    {
        return (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_FAST;
    }
    if (speed_q16 >= middle_threshold_q16)
    {
        return (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_MIDDLE;
    }
    return (uint16_t)PF_M4_FALCON_SUBMOTION_WALK_SLOW;
}

static int32_t pf_m4_falcon_ground_animation_rate_q16(
    int32_t ground_velocity_q16,
    int8_t facing,
    int32_t animation_scaling_q16)
{
    int64_t numerator;

    if ((int64_t)ground_velocity_q16 * (int64_t)facing <= INT64_C(0))
    {
        return INT32_C(0);
    }
    numerator =
        (int64_t)(ground_velocity_q16 < INT32_C(0)
                      ? -ground_velocity_q16
                      : ground_velocity_q16) *
        (int64_t)PF_Q16_ONE;
    return (int32_t)(
        (numerator + (int64_t)animation_scaling_q16 / INT64_C(2)) /
        (int64_t)animation_scaling_q16);
}

static int32_t pf_m4_falcon_walk_animation_scaling_q16(
    const pf_m4_fighter_data *fighter,
    uint16_t source_submotion)
{
    switch (source_submotion)
    {
        case PF_M4_FALCON_SUBMOTION_WALK_SLOW:
            return fighter->slow_walk_animation_scaling_q16;
        case PF_M4_FALCON_SUBMOTION_WALK_MIDDLE:
            return fighter->middle_walk_animation_scaling_q16;
        case PF_M4_FALCON_SUBMOTION_WALK_FAST:
            return fighter->fast_walk_animation_scaling_q16;
        default:
            return INT32_C(0);
    }
}

static int pf_m4_falcon_advance_loop_animation_q16(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int32_t source_animation_rate_q16,
    int32_t *out_frame_q16)
{
    const pf_m4_falcon_submotion_data *motion =
        pf_m4_falcon_reference_submotion(source_submotion);
    int64_t length_q16;
    int64_t next_frame_q16;

    if (motion == NULL || motion->animation_frame_count == UINT16_C(0) ||
        source_animation_frame_q16 < INT32_C(0) ||
        source_animation_rate_q16 < INT32_C(0))
    {
        return 0;
    }
    length_q16 =
        (int64_t)motion->animation_frame_count * (int64_t)PF_Q16_ONE;
    next_frame_q16 =
        (int64_t)source_animation_frame_q16 +
        (int64_t)source_animation_rate_q16;
    *out_frame_q16 = (int32_t)(next_frame_q16 % length_q16);
    return 1;
}

static int pf_m4_falcon_remap_walk_animation_q16(
    uint16_t old_submotion,
    uint16_t new_submotion,
    int32_t advanced_frame_q16,
    int32_t *out_frame_q16)
{
    const pf_m4_falcon_submotion_data *old_motion =
        pf_m4_falcon_reference_submotion(old_submotion);
    const pf_m4_falcon_submotion_data *new_motion =
        pf_m4_falcon_reference_submotion(new_submotion);
    int64_t old_length_q16;
    int64_t remapped_integer_frame;

    if (old_motion == NULL || new_motion == NULL ||
        old_motion->animation_frame_count == UINT16_C(0) ||
        new_motion->animation_frame_count == UINT16_C(0) ||
        advanced_frame_q16 < INT32_C(0))
    {
        return 0;
    }
    old_length_q16 =
        (int64_t)old_motion->animation_frame_count *
        (int64_t)PF_Q16_ONE;
    remapped_integer_frame =
        (int64_t)new_motion->animation_frame_count *
        ((int64_t)advanced_frame_q16 % old_length_q16) /
        old_length_q16;
    remapped_integer_frame =
        (remapped_integer_frame + INT64_C(1)) %
        (int64_t)new_motion->animation_frame_count;
    *out_frame_q16 =
        (int32_t)(remapped_integer_frame * (int64_t)PF_Q16_ONE);
    return 1;
}

static const pf_m4_hsd_wait_animation *
pf_m4_falcon_select_wait_animation(
    uint64_t *rng_state,
    uint16_t current_submotion)
{
    uint8_t animation_count;
    const pf_m4_hsd_wait_animation *animations =
        pf_m4_falcon_reference_wait_animations(&animation_count);

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
                const pf_m4_hsd_wait_animation *selected =
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

static pf_status pf_m4_update_falcon_ground_animation_clock(
    const pf_m4_fighter_data *fighter,
    uint64_t *rng_state,
    uint8_t previous_action,
    uint8_t previous_resume_action,
    uint16_t previous_submotion,
    int32_t previous_frame_q16,
    int32_t previous_rate_q16,
    int32_t previous_ground_velocity_q16,
    int8_t previous_facing,
    uint8_t action,
    uint8_t resume_action,
    uint16_t *source_submotion,
    int32_t *source_animation_frame_q16,
    int32_t *source_animation_rate_q16)
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
        *source_animation_frame_q16 = INT32_C(0);
        *source_animation_rate_q16 = INT32_C(0);
        return PF_STATUS_OK;
    }
    if (action == (uint8_t)PF_M4_ACTION_HITLAG ||
        previous_action == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        *source_submotion = previous_submotion;
        *source_animation_frame_q16 = previous_frame_q16;
        *source_animation_rate_q16 = previous_rate_q16;
        return PF_STATUS_OK;
    }
    if (effective_action == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        const pf_m4_falcon_submotion_data *wait =
            pf_m4_falcon_reference_submotion(previous_submotion);
        const int32_t terminal_frame_q16 =
            wait != NULL && wait->animation_frame_count != UINT16_C(0)
                ? (int32_t)(wait->animation_frame_count - UINT16_C(1)) *
                      PF_Q16_ONE
                : INT32_C(-1);

        if (terminal_frame_q16 < INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (effective_previous_action !=
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            *source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
            *source_animation_frame_q16 = INT32_C(0);
            *source_animation_rate_q16 = PF_Q16_ONE;
        }
        else if (previous_frame_q16 >= terminal_frame_q16)
        {
            const pf_m4_hsd_wait_animation *selected =
                pf_m4_falcon_select_wait_animation(
                    rng_state, previous_submotion);

            if (selected == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *source_submotion = selected->source_submotion;
            *source_animation_frame_q16 = INT32_C(0);
            *source_animation_rate_q16 = PF_Q16_ONE;
        }
        else
        {
            *source_submotion = previous_submotion;
            *source_animation_frame_q16 =
                previous_frame_q16 + previous_rate_q16;
            *source_animation_rate_q16 = PF_Q16_ONE;
        }
        return PF_STATUS_OK;
    }
    if (effective_action == (uint8_t)PF_M4_ACTION_WALK)
    {
        const uint16_t selected_submotion =
            pf_m4_falcon_walk_submotion_for_velocity(
                fighter,
                previous_ground_velocity_q16);

        if (effective_previous_action != (uint8_t)PF_M4_ACTION_WALK ||
            !pf_m4_falcon_walk_submotion(previous_submotion))
        {
            const pf_m4_falcon_submotion_data *motion =
                pf_m4_falcon_reference_submotion(selected_submotion);

            if (motion == NULL ||
                motion->animation_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *source_submotion = selected_submotion;
            *source_animation_frame_q16 =
                motion->animation_frame_count > UINT16_C(1)
                    ? PF_Q16_ONE
                    : INT32_C(0);
            *source_animation_rate_q16 = PF_Q16_ONE;
            return PF_STATUS_OK;
        }
        if (!pf_m4_falcon_advance_loop_animation_q16(
                previous_submotion,
                previous_frame_q16,
                previous_rate_q16,
                source_animation_frame_q16))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *source_submotion = selected_submotion;
        if (selected_submotion != previous_submotion)
        {
            if (!pf_m4_falcon_remap_walk_animation_q16(
                    previous_submotion,
                    selected_submotion,
                    *source_animation_frame_q16,
                    source_animation_frame_q16))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            *source_animation_rate_q16 = PF_Q16_ONE;
        }
        else
        {
            *source_animation_rate_q16 =
                pf_m4_falcon_ground_animation_rate_q16(
                    previous_ground_velocity_q16,
                    previous_facing,
                    pf_m4_falcon_walk_animation_scaling_q16(
                        fighter,
                        selected_submotion));
        }
        return PF_STATUS_OK;
    }

    *source_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_RUN;
    if (effective_previous_action != (uint8_t)PF_M4_ACTION_RUN ||
        previous_submotion != (uint16_t)PF_M4_FALCON_SUBMOTION_RUN)
    {
        *source_animation_frame_q16 = INT32_C(0);
        *source_animation_rate_q16 = PF_Q16_ONE;
        return PF_STATUS_OK;
    }
    if (!pf_m4_falcon_advance_loop_animation_q16(
            previous_submotion,
            previous_frame_q16,
            previous_rate_q16,
            source_animation_frame_q16))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *source_animation_rate_q16 =
        pf_m4_falcon_ground_animation_rate_q16(
            previous_ground_velocity_q16,
            previous_facing,
            fighter->run_animation_scaling_q16);
    return PF_STATUS_OK;
}

static pf_status pf_m4_update_falcon_fall_animation_clock(
    const pf_m4_fighter_data *fighter,
    uint8_t previous_action,
    uint8_t previous_resume_action,
    uint16_t previous_submotion,
    int32_t previous_frame_q16,
    int32_t previous_rate_q16,
    int32_t previous_blend_q16,
    uint8_t previous_target_switched,
    int32_t previous_velocity_x_q16,
    int8_t previous_facing,
    uint8_t action,
    uint8_t resume_action,
    uint16_t *source_submotion,
    int32_t *source_animation_frame_q16,
    int32_t *source_animation_rate_q16,
    int32_t *fall_animation_blend_q16,
    uint8_t *fall_animation_target_switched)
{
    const pf_m4_ssbm_fall_animation_attributes *common =
        pf_m4_ssbm_common_reference_fall_animation();
    const pf_m4_falcon_submotion_data *neutral =
        pf_m4_falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_FALL_SPECIAL);
    const uint8_t effective_previous_action =
        pf_m4_effective_action_state(previous_action, previous_resume_action);
    const uint8_t effective_action =
        pf_m4_effective_action_state(action, resume_action);
    int32_t air_drift_fraction_q16;
    int32_t magnitude_q16;
    int32_t target_blend_q16 = INT32_C(0);
    int32_t next_blend_q16;
    uint16_t selected_submotion =
        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL;

    if (!pf_m4_action_uses_fall_special_pose(effective_action))
    {
        *fall_animation_blend_q16 = INT32_C(0);
        *fall_animation_target_switched = UINT8_C(0);
        return PF_STATUS_OK;
    }
    if (common == NULL || neutral == NULL ||
        neutral->animation_frame_count == UINT16_C(0) ||
        fighter->air_speed_q16 <= INT32_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (action == (uint8_t)PF_M4_ACTION_HITLAG ||
        previous_action == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        *source_submotion = previous_submotion;
        *source_animation_frame_q16 = previous_frame_q16;
        *source_animation_rate_q16 = previous_rate_q16;
        *fall_animation_blend_q16 = previous_blend_q16;
        *fall_animation_target_switched = previous_target_switched;
        return PF_STATUS_OK;
    }
    if (!pf_m4_action_uses_fall_special_pose(effective_previous_action))
    {
        *source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL;
        *source_animation_frame_q16 = INT32_C(0);
        *source_animation_rate_q16 = PF_Q16_ONE;
        *fall_animation_blend_q16 = INT32_C(0);
        *fall_animation_target_switched = UINT8_C(0);
        return PF_STATUS_OK;
    }

    *source_animation_frame_q16 = previous_frame_q16 + previous_rate_q16;
    while (*source_animation_frame_q16 >=
           (int32_t)neutral->animation_frame_count * PF_Q16_ONE)
    {
        *source_animation_frame_q16 -=
            (int32_t)neutral->animation_frame_count * PF_Q16_ONE;
    }
    *source_animation_rate_q16 = PF_Q16_ONE;

    air_drift_fraction_q16 = (int32_t)(
        ((int64_t)previous_velocity_x_q16 * PF_Q16_ONE) /
        fighter->air_speed_q16);
    if (air_drift_fraction_q16 > PF_Q16_ONE)
    {
        air_drift_fraction_q16 = PF_Q16_ONE;
    }
    else if (air_drift_fraction_q16 < -PF_Q16_ONE)
    {
        air_drift_fraction_q16 = -PF_Q16_ONE;
    }
    magnitude_q16 = air_drift_fraction_q16 < INT32_C(0)
                        ? -air_drift_fraction_q16
                        : air_drift_fraction_q16;
    if (magnitude_q16 > common->direction_threshold_q16)
    {
        target_blend_q16 = (int32_t)(
            ((int64_t)(magnitude_q16 - common->direction_threshold_q16) *
             PF_Q16_ONE) /
            (PF_Q16_ONE - common->direction_threshold_q16));
        selected_submotion =
            (air_drift_fraction_q16 * (int32_t)previous_facing > INT32_C(0))
                ? (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_FORWARD
                : (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_BACKWARD;
    }
    next_blend_q16 = previous_blend_q16 +
                     pf_m4_multiply_q16(
                         target_blend_q16 - previous_blend_q16,
                         common->blend_rate_q16);
    *fall_animation_blend_q16 = next_blend_q16;
    *fall_animation_target_switched =
        next_blend_q16 != INT32_C(0) &&
                selected_submotion != previous_submotion
            ? UINT8_C(1)
            : UINT8_C(0);
    *source_submotion = next_blend_q16 != INT32_C(0)
                            ? selected_submotion
                            : previous_submotion;
    return PF_STATUS_OK;
}

static int32_t pf_m4_ground_blend_weight_q16(
    int32_t old_progress_q16,
    int32_t new_progress_q16)
{
    const int32_t old_remaining_q16 =
        INT32_C(6) * PF_Q16_ONE - old_progress_q16;
    const int32_t new_remaining_q16 =
        INT32_C(6) * PF_Q16_ONE - new_progress_q16;

    return old_remaining_q16 > INT32_C(0)
               ? (int32_t)(
                     ((int64_t)new_remaining_q16 * PF_Q16_ONE +
                      old_remaining_q16 / INT32_C(2)) /
                     old_remaining_q16)
               : INT32_C(0);
}

static int pf_m4_falcon_continue_ground_blend_pose(
    const pf_m4_hsd_pose_data *data,
    const pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS],
    const pf_m4_hsd_compact_pose *previous_compact,
    int32_t previous_progress_q16,
    int32_t frame_delta_q16,
    pf_m4_hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS],
    int32_t *out_progress_q16)
{
    pf_m4_hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS];
    const int32_t progress_q16 = previous_progress_q16 + frame_delta_q16;

    if (progress_q16 >= INT32_C(6) * PF_Q16_ONE)
    {
        (void)memcpy(
            out_pose, target, sizeof(*target) * data->joint_count);
    }
    else if (!pf_m4_hsd_inflate_compact_pose_q16(
                 data, target, previous_compact, current) ||
             !pf_m4_hsd_blend_local_pose_q16(
                 data,
                 target,
                 current,
                 pf_m4_ground_blend_weight_q16(
                     previous_progress_q16, progress_q16),
                 out_pose))
    {
        return 0;
    }
    *out_progress_q16 = progress_q16;
    return 1;
}

static int pf_m4_falcon_ground_blend_source_pose(
    const pf_world_state *world,
    uint32_t player_index,
    const pf_m4_hsd_pose_data *data,
    pf_m4_hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    const uint8_t previous_action = pf_m4_effective_action_state(
        world->action_state[player_index],
        world->hitlag_resume_action[player_index]);
    uint16_t source_submotion;
    int32_t source_frame_q16;

    if (previous_action == (uint8_t)PF_M4_ACTION_WALK ||
        previous_action == (uint8_t)PF_M4_ACTION_RUN)
    {
        pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        int32_t ignored_progress_q16;

        source_submotion = world->source_submotion[player_index];
        if (!pf_m4_falcon_advance_loop_animation_q16(
                source_submotion,
                world->source_animation_frame_q16[player_index],
                world->source_animation_rate_q16[player_index],
                &source_frame_q16) ||
            !pf_m4_hsd_evaluate_local_pose_q16(
                data, source_submotion, source_frame_q16, target))
        {
            return 0;
        }
        if (world->ground_blend_progress_q16[player_index] <= INT32_C(0))
        {
            (void)memcpy(
                out_pose, target, sizeof(*target) * data->joint_count);
            return 1;
        }
        return pf_m4_falcon_continue_ground_blend_pose(
            data,
            target,
            &world->ground_blend_pose[player_index],
            world->ground_blend_progress_q16[player_index],
            world->source_animation_rate_q16[player_index],
            out_pose,
            &ignored_progress_q16);
    }
    else if (previous_action == (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        int32_t ignored_progress_q16;

        if (pf_m4_falcon_reference_wait_animation(
                world->source_submotion[player_index]) == NULL)
        {
            return 0;
        }
        source_submotion = world->source_submotion[player_index];
        source_frame_q16 =
            world->source_animation_frame_q16[player_index] +
            world->source_animation_rate_q16[player_index];
        if (!pf_m4_hsd_evaluate_local_pose_q16(
                data, source_submotion, source_frame_q16, target))
        {
            return 0;
        }
        if (world->ground_blend_progress_q16[player_index] <= INT32_C(0))
        {
            (void)memcpy(
                out_pose, target, sizeof(*target) * data->joint_count);
            return 1;
        }
        return pf_m4_falcon_continue_ground_blend_pose(
            data,
            target,
            &world->ground_blend_pose[player_index],
            world->ground_blend_progress_q16[player_index],
            world->source_animation_rate_q16[player_index],
            out_pose,
            &ignored_progress_q16);
    }
    else if (previous_action == (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        const pf_m4_falcon_submotion_data *dash =
            pf_m4_falcon_reference_submotion(
                PF_M4_FALCON_SUBMOTION_DASH);

        if (dash == NULL || dash->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        source_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_DASH;
        source_frame_q16 = (int32_t)(
            world->action_ticks[player_index] + UINT16_C(1) <
                    dash->animation_frame_count
                ? world->action_ticks[player_index] + UINT16_C(1)
                : dash->animation_frame_count - UINT16_C(1)) *
            PF_Q16_ONE;
    }
    else if ((previous_action == (uint8_t)PF_M4_ACTION_CROUCH_END ||
              pf_m4_action_uses_direct_hsd_pose(previous_action)) &&
             world->source_animation_rate_q16[player_index] > INT32_C(0))
    {
        source_submotion = world->source_submotion[player_index];
        source_frame_q16 =
            world->source_animation_frame_q16[player_index] +
            world->source_animation_rate_q16[player_index];
    }
    else
    {
        return 0;
    }
    return pf_m4_hsd_evaluate_local_pose_q16(
        data, source_submotion, source_frame_q16, out_pose);
}

static pf_status pf_m4_evaluate_falcon_ground_blend_pose(
    const pf_world_state *world,
    uint32_t player_index,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    pf_m4_hsd_compact_pose *out_pose,
    int32_t *out_progress_q16)
{
    const pf_m4_hsd_pose_data *data =
        pf_m4_falcon_reference_hsd_pose_data();
    const uint8_t previous_action = pf_m4_effective_action_state(
        world->action_state[player_index],
        world->hitlag_resume_action[player_index]);
    const uint8_t action = pf_m4_effective_action_state(
        action_state, hitlag_resume_action);
    pf_m4_hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
    pf_m4_hsd_local_pose current[PF_M4_HSD_POSE_MAX_JOINTS];
    pf_m4_hsd_local_pose result[PF_M4_HSD_POSE_MAX_JOINTS];
    int32_t progress_q16 = INT32_C(0);
    int transition_steps = 0;

    if (out_pose == NULL || out_progress_q16 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (data == NULL ||
        !pf_m4_action_uses_ground_animation_clock(
            action_state, hitlag_resume_action))
    {
        (void)memset(out_pose, 0, sizeof(*out_pose));
        *out_progress_q16 = INT32_C(0);
        return PF_STATUS_OK;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
    {
        *out_pose = world->ground_blend_pose[player_index];
        *out_progress_q16 = world->ground_blend_progress_q16[player_index];
        return PF_STATUS_OK;
    }
    if (!pf_m4_hsd_evaluate_local_pose_q16(
            data,
            source_submotion,
            source_animation_frame_q16,
            target))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (source_submotion != world->source_submotion[player_index] ||
        (action == (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
         action == previous_action &&
         source_animation_frame_q16 <
             world->source_animation_frame_q16[player_index]) ||
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
        const pf_m4_hsd_wait_animation *wait_animation =
            pf_m4_falcon_reference_wait_animation(source_submotion);

        if (wait_animation != NULL &&
            wait_animation->blend_frames == UINT8_C(0))
        {
            (void)memset(out_pose, 0, sizeof(*out_pose));
            *out_progress_q16 = INT32_C(0);
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
         pf_m4_action_uses_direct_hsd_pose(previous_action)) &&
        world->source_animation_rate_q16[player_index] == PF_Q16_ONE)
    {
        (void)memset(out_pose, 0, sizeof(*out_pose));
        out_pose->replay.source_submotion =
            world->source_submotion[player_index];
        out_pose->replay.source_frame_q16 =
            world->source_animation_frame_q16[player_index] + PF_Q16_ONE;
        out_pose->replay.target_entry_frame_q16 =
            source_animation_frame_q16;
        out_pose->replay.target_step_q16 = PF_Q16_ONE;
        out_pose->replay.blend_frames_q16 =
            INT32_C(6) * PF_Q16_ONE;
        out_pose->mode = (uint8_t)PF_M4_HSD_COMPACT_POSE_REPLAY;
        *out_progress_q16 = PF_Q16_ONE;
        return PF_STATUS_OK;
    }
    if (transition_steps > 0 &&
        pf_m4_falcon_ground_blend_source_pose(
            world, player_index, data, current))
    {
        int step;

        for (step = 1; step <= transition_steps; ++step)
        {
            pf_m4_hsd_local_pose step_target[PF_M4_HSD_POSE_MAX_JOINTS];
            pf_m4_hsd_local_pose next[PF_M4_HSD_POSE_MAX_JOINTS];
            int32_t step_frame_q16 =
                source_animation_frame_q16 -
                (transition_steps - step) * PF_Q16_ONE;
            const pf_m4_falcon_submotion_data *motion =
                pf_m4_falcon_reference_submotion(
                    source_submotion);

            if (motion == NULL || motion->animation_frame_count == UINT16_C(0))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            while (step_frame_q16 < INT32_C(0))
            {
                step_frame_q16 +=
                    (int32_t)motion->animation_frame_count * PF_Q16_ONE;
            }
            if (!pf_m4_hsd_evaluate_local_pose_q16(
                    data,
                    source_submotion,
                    step_frame_q16,
                    step_target) ||
                !pf_m4_hsd_blend_local_pose_q16(
                    data,
                    step_target,
                    current,
                    (int32_t)(INT32_C(6) - step) * PF_Q16_ONE /
                        (int32_t)(INT32_C(7) - step),
                    next))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            (void)memcpy(current, next, sizeof(*next) * data->joint_count);
        }
        progress_q16 = transition_steps * PF_Q16_ONE;
    }
    else if (world->ground_blend_progress_q16[player_index] > INT32_C(0) &&
             action == previous_action &&
             source_submotion == world->source_submotion[player_index])
    {
        int32_t frame_delta_q16 =
            source_animation_frame_q16 -
            world->source_animation_frame_q16[player_index];
        const pf_m4_falcon_submotion_data *motion =
            pf_m4_falcon_reference_submotion(
                source_submotion);

        if (motion == NULL || motion->animation_frame_count == UINT16_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (frame_delta_q16 < INT32_C(0))
        {
            frame_delta_q16 +=
                (int32_t)motion->animation_frame_count * PF_Q16_ONE;
        }
        progress_q16 = world->ground_blend_progress_q16[player_index] +
                       frame_delta_q16;
        if (progress_q16 < INT32_C(6) * PF_Q16_ONE &&
            world->ground_blend_pose[player_index].mode ==
                (uint8_t)PF_M4_HSD_COMPACT_POSE_REPLAY)
        {
            *out_pose = world->ground_blend_pose[player_index];
            *out_progress_q16 = progress_q16;
            return PF_STATUS_OK;
        }
        else if (progress_q16 < INT32_C(6) * PF_Q16_ONE)
        {
            if (!pf_m4_falcon_continue_ground_blend_pose(
                    data,
                    target,
                    &world->ground_blend_pose[player_index],
                    world->ground_blend_progress_q16[player_index],
                    frame_delta_q16,
                    result,
                    &progress_q16))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
    }
    if (progress_q16 > INT32_C(0) &&
        progress_q16 < INT32_C(6) * PF_Q16_ONE)
    {
        if (transition_steps > 0)
        {
            (void)memcpy(
                result,
                current,
                sizeof(*current) * data->joint_count);
        }
        if (!pf_m4_hsd_pack_compact_pose_q16(
                data,
                result,
                out_pose))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *out_progress_q16 = progress_q16;
    }
    else
    {
        (void)memset(out_pose, 0, sizeof(*out_pose));
        *out_progress_q16 = INT32_C(0);
    }
    return PF_STATUS_OK;
}

static int pf_m4_advance_falcon_source_submotion(
    uint16_t *submotion_index,
    uint16_t *action_ticks)
{
    const pf_m4_falcon_submotion_data *submotion =
        pf_m4_falcon_reference_submotion(*submotion_index);
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

static uint16_t pf_m4_ground_damage_submotion(uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

static pf_status pf_m4_advance_ground_damage_animation(
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t hitstun_ticks,
    int32_t *ground_knockback_velocity_q16)
{
    const uint16_t submotion_index =
        pf_m4_ground_damage_submotion(*action_state);
    const pf_m4_falcon_submotion_data *motion =
        submotion_index != UINT16_MAX
            ? pf_m4_falcon_reference_submotion(submotion_index)
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
        hitstun_ticks == UINT16_C(0))
    {
        *action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        *action_ticks = UINT16_C(0);
        *ground_knockback_velocity_q16 = INT32_C(0);
    }
    return PF_STATUS_OK;
}

static int32_t pf_m4_clamp_i32(int32_t value, int32_t minimum, int32_t maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static uint32_t pf_m4_shield_health_add(
    uint32_t health_q16,
    uint32_t amount_q16,
    uint32_t maximum_q16)
{
    if (health_q16 >= maximum_q16 ||
        amount_q16 >= maximum_q16 - health_q16)
    {
        return maximum_q16;
    }
    return health_q16 + amount_q16;
}

static uint32_t pf_m4_shield_health_subtract(
    uint32_t health_q16,
    uint32_t amount_q16)
{
    return amount_q16 >= health_q16
               ? UINT32_C(0)
               : health_q16 - amount_q16;
}

static uint16_t pf_m4_input_shield_strength(
    const pf_m4_fighter_data *fighter,
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

static uint32_t pf_m4_lerp_u32(
    uint32_t low,
    uint32_t high,
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
    return low +
           (uint32_t)(
               ((uint64_t)(high - low) *
                (uint64_t)(value - low_value)) /
               (uint64_t)(high_value - low_value));
}

static uint32_t pf_m4_shield_hold_depletion_q16(
    const pf_m4_fighter_data *fighter,
    uint16_t shield_strength)
{
    return pf_m4_lerp_u32(
        fighter->light_shield_hold_depletion_q16,
        fighter->shield_hold_depletion_q16,
        shield_strength,
        UINT16_C(0),
        UINT16_MAX);
}

static int pf_m4_action_retains_shield_strength(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
            hitlag_resume_action ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN);
}

static int pf_m4_action_freezes_shield_strength(
    uint8_t action_state,
    uint8_t hitlag_resume_action)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
            hitlag_resume_action ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN);
}

static uint16_t pf_m4_shield_break_stun_ticks(
    const pf_m4_fighter_data *fighter,
    uint32_t damage_q16)
{
    const uint32_t damage_percent =
        damage_q16 / (uint32_t)PF_Q16_ONE;
    const uint32_t maximum_reduction =
        (uint32_t)fighter->shield_break_stun_ticks -
        (uint32_t)fighter->shield_break_minimum_stun_ticks;

    return damage_percent >= maximum_reduction
               ? fighter->shield_break_minimum_stun_ticks
               : (uint16_t)(
                     (uint32_t)fighter->shield_break_stun_ticks -
                     damage_percent);
}

static int32_t pf_m4_scale_axis_q16(
    int16_t axis,
    int32_t magnitude_q16)
{
    const int64_t denominator =
        axis < INT16_C(0) ? INT64_C(32768) : INT64_C(32767);
    const int64_t product =
        (int64_t)axis * (int64_t)magnitude_q16;

    return product < INT64_C(0)
               ? (int32_t)(
                     -((-product + denominator / INT64_C(2)) /
                       denominator))
               : (int32_t)(
                     (product + denominator / INT64_C(2)) /
                     denominator);
}

static uint16_t pf_m4_axis_magnitude(int16_t axis)
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

static const int16_t pf_m4_sine_q15_table[65] = {
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

static int32_t pf_m4_sine_q15(uint16_t angle_turn)
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
    lower = pf_m4_sine_q15_table[index];
    upper = pf_m4_sine_q15_table[
        index < UINT32_C(64) ? index + UINT32_C(1) : index];
    value = lower +
            (int32_t)(((int64_t)(upper - lower) * fraction) /
                      INT64_C(1024));
    return quadrant >= UINT32_C(2) ? -value : value;
}

static int32_t pf_m4_falcon_source_velocity_to_sim_q16(
    int32_t source_velocity_q16,
    int32_t numerator,
    int32_t denominator)
{
    const int64_t product =
        (int64_t)source_velocity_q16 * (int64_t)numerator;

    return product < INT64_C(0)
               ? (int32_t)(
                     -((-product + denominator / INT32_C(2)) /
                       denominator))
               : (int32_t)(
                     (product + denominator / INT32_C(2)) /
                     denominator);
}

static void pf_m4_falcon_punch_launch_velocity(
    const pf_m4_falcon_special_attributes *attributes,
    int16_t stick_y,
    int8_t facing,
    int32_t *out_velocity_x_q16,
    int32_t *out_velocity_y_q16)
{
    const uint32_t stick_magnitude_q16 =
        ((uint32_t)pf_m4_axis_magnitude(stick_y) * UINT32_C(65536) +
         UINT32_C(16384)) /
        UINT32_C(32768);
    const uint32_t bounded_stick_q16 =
        stick_magnitude_q16 >
                (uint32_t)attributes->specialn_stick_range_y_pos_q16
            ? (uint32_t)attributes->specialn_stick_range_y_pos_q16
            : stick_magnitude_q16;
    const uint32_t angle_input_q16 =
        bounded_stick_q16 >
                (uint32_t)attributes->specialn_stick_range_y_neg_q16
            ? bounded_stick_q16 -
                  (uint32_t)attributes->specialn_stick_range_y_neg_q16
            : UINT32_C(0);
    const uint32_t angle_range_q16 =
        (uint32_t)(
            attributes->specialn_stick_range_y_pos_q16 -
            attributes->specialn_stick_range_y_neg_q16);
    const uint32_t angle_degrees_q16 =
        (uint32_t)(
            ((uint64_t)angle_input_q16 *
             (uint64_t)(uint32_t)attributes->specialn_angle_diff_q16) /
            angle_range_q16);
    uint16_t angle_turn = (uint16_t)(
        angle_degrees_q16 / UINT32_C(360));
    const int32_t source_x_q16 =
        pf_m4_falcon_source_velocity_to_sim_q16(
            attributes->specialn_vel_x_q16,
            INT32_C(12),
            INT32_C(115));
    const int32_t source_y_q16 =
        pf_m4_falcon_source_velocity_to_sim_q16(
            attributes->specialn_vel_x_q16,
            INT32_C(11),
            INT32_C(62));

    if (stick_y < INT16_C(0))
    {
        angle_turn = (uint16_t)(UINT16_C(0) - angle_turn);
    }
    *out_velocity_x_q16 =
        (int32_t)facing *
        (int32_t)(
            ((int64_t)source_x_q16 *
             (int64_t)pf_m4_sine_q15(
                 (uint16_t)(angle_turn + UINT16_C(16384)))) /
            INT64_C(32767));
    *out_velocity_y_q16 =
        -(int32_t)(
            ((int64_t)source_y_q16 *
             (int64_t)pf_m4_sine_q15(angle_turn)) /
            INT64_C(32767));
}

void pf_m4_shield_tilt_axes(
    uint16_t angle_turn,
    uint16_t magnitude,
    int8_t facing,
    int16_t *out_x,
    int16_t *out_y)
{
    const int32_t local_x_q15 =
        pf_m4_sine_q15((uint16_t)(angle_turn + UINT16_C(16384)));
    const int32_t local_y_q15 = pf_m4_sine_q15(angle_turn);

    *out_x = (int16_t)(
        ((int64_t)local_x_q15 * (int64_t)magnitude * (int64_t)facing) /
        INT64_C(65535));
    *out_y = (int16_t)(
        -((int64_t)local_y_q15 * (int64_t)magnitude) /
        INT64_C(65535));
}

static uint16_t pf_m4_atan2_turn(int32_t y, int32_t x)
{
    const uint32_t angle = (uint32_t)pf_m4_fixed_atan2_turn(y, x);

    /* GALE01 clamps angles in the final degree to 359 before smoothing. */
    return (uint16_t)(angle > UINT32_C(65354) ? UINT32_C(65354) : angle);
}

static int32_t pf_m4_half_nearest(int32_t value)
{
    return value < INT32_C(0)
               ? -((-value + INT32_C(1)) / INT32_C(2))
               : (value + INT32_C(1)) / INT32_C(2);
}

static uint16_t pf_m4_shield_target_magnitude(const pf_input_frame *input)
{
    const uint32_t x = pf_m4_axis_magnitude(input->main_stick_x);
    const uint32_t y = pf_m4_axis_magnitude(input->main_stick_y);
    uint32_t magnitude = pf_m4_u64_sqrt(
        (uint64_t)x * (uint64_t)x + (uint64_t)y * (uint64_t)y);

    if (magnitude > UINT32_C(32768))
    {
        magnitude = UINT32_C(32768);
    }
    return (uint16_t)(
        (magnitude * UINT32_C(65535) + UINT32_C(16384)) >> 15U);
}

static void pf_m4_update_shield_tilt(
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    int8_t facing)
{
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        const uint16_t target_angle = pf_m4_atan2_turn(
            -(int32_t)input->main_stick_y,
            (int32_t)input->main_stick_x * (int32_t)facing);
        const uint16_t current_angle =
            scratch->shield_angle_turn[player_index];
        int32_t angle_delta =
            (int32_t)target_angle - (int32_t)current_angle;
        const uint16_t target_magnitude =
            pf_m4_shield_target_magnitude(input);

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
                       pf_m4_half_nearest(angle_delta)) &
            UINT32_C(65535));
        scratch->shield_magnitude[player_index] = (uint16_t)(
            ((uint32_t)scratch->shield_magnitude[player_index] +
             (uint32_t)target_magnitude + UINT32_C(1)) /
            UINT32_C(2));
    }
    else if (!pf_m4_action_retains_shield_strength(
                 action_state,
                 hitlag_resume_action))
    {
        scratch->shield_angle_turn[player_index] = UINT16_C(0);
        scratch->shield_magnitude[player_index] = UINT16_C(0);
    }
}

static int8_t pf_m4_axis_direction(int16_t axis, uint16_t dead_zone)
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

static pf_status pf_m4_enter_air_dodge(
    const pf_m4_fighter_data *fighter,
    int16_t stick_x,
    int16_t stick_y,
    int32_t *velocity_x,
    int32_t *velocity_y)
{
    const uint16_t magnitude_x = pf_m4_axis_magnitude(stick_x);
    const uint16_t magnitude_y = pf_m4_axis_magnitude(stick_y);
    uint32_t stick_magnitude;
    int64_t component;

    if (magnitude_x < fighter->air_dodge_dead_zone &&
        magnitude_y < fighter->air_dodge_dead_zone)
    {
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        return PF_STATUS_OK;
    }

    stick_magnitude = pf_m4_u64_sqrt(
        (uint64_t)((int64_t)stick_x * (int64_t)stick_x) +
        (uint64_t)((int64_t)stick_y * (int64_t)stick_y));
    if (stick_magnitude == UINT32_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    component =
        (int64_t)stick_x *
        (int64_t)fighter->air_dodge_speed_x_q16 /
        (int64_t)stick_magnitude;
    if (!pf_m4_checked_i32(component, velocity_x))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    component =
        (int64_t)stick_y *
        (int64_t)fighter->air_dodge_speed_y_q16 /
        (int64_t)stick_magnitude;
    if (!pf_m4_checked_i32(component, velocity_y))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    return PF_STATUS_OK;
}

static int8_t pf_m4_strong_direction(
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

static uint8_t pf_m4_tilt_age(
    int16_t axis,
    uint16_t threshold,
    int8_t previous_direction,
    uint8_t previous_age,
    int8_t *out_direction)
{
    const int8_t direction = pf_m4_axis_direction(axis, threshold);

    *out_direction = direction;
    if (direction == INT8_C(0))
    {
        return UINT8_C(254);
    }
    if (previous_direction == INT8_C(0) ||
        direction != previous_direction)
    {
        return UINT8_C(0);
    }
    if (previous_age < UINT8_C(253))
    {
        return (uint8_t)(previous_age + UINT8_C(1));
    }
    return UINT8_C(253);
}

static int pf_m4_is_moonwalk_lower_sweep(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    int16_t stick_y)
{
    return (action_state ==
                (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
            action_state ==
                (uint8_t)PF_M4_ACTION_MOONWALK_SETUP) &&
           stick_y >=
               (int16_t)fighter->crouch_axis_threshold;
}

static int pf_m4_is_moonwalk_lower_back(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    int8_t facing,
    int16_t stick_x,
    int16_t stick_y)
{
    return pf_m4_is_moonwalk_lower_sweep(
               fighter,
               action_state,
               stick_y) &&
           pf_m4_axis_direction(
               stick_x,
               fighter->axis_dead_zone) == -facing;
}

static int32_t pf_m4_ground_input_acceleration(
    const pf_m4_fighter_data *fighter,
    int16_t stick_x,
    int32_t velocity_x,
    int32_t target_x,
    int movement_mode)
{
    const int walk = movement_mode == 1;
    const int run = movement_mode == 2;
    const int32_t stick_acceleration_q16 =
        walk != 0
            ? fighter->walk_initial_velocity_q16
            : fighter->ground_acceleration_q16;
    const int32_t base_acceleration_q16 =
        walk != 0
            ? fighter->walk_acceleration_q16
            : fighter->dash_run_base_acceleration_q16;
    const int32_t taper_q16 =
        walk != 0
            ? fighter->walk_acceleration_taper_q16
            : fighter->run_acceleration_taper_q16;
    int32_t acceleration =
        pf_m4_scale_axis_q16(stick_x, stick_acceleration_q16) +
        (stick_x < INT16_C(0)
             ? -base_acceleration_q16
             : base_acceleration_q16);

    if (walk == 0 &&
        (stick_x >= INT16_C(32767) ||
         stick_x <= INT16_C(-32767)))
    {
        acceleration =
            stick_x < INT16_C(0)
                ? -fighter->turn_acceleration_q16
                : fighter->turn_acceleration_q16;
    }

    if ((walk != 0 || run != 0) &&
        target_x != INT32_C(0) &&
        (int64_t)velocity_x * (int64_t)target_x > INT64_C(0) &&
        ((target_x > INT32_C(0) && velocity_x < target_x) ||
         (target_x < INT32_C(0) && velocity_x > target_x)))
    {
        const int64_t remaining =
            target_x > velocity_x
                ? (int64_t)target_x - (int64_t)velocity_x
                : (int64_t)velocity_x - (int64_t)target_x;
        const int64_t target_magnitude =
            target_x < INT32_C(0)
                ? -(int64_t)target_x
                : (int64_t)target_x;
        const int64_t factor_q16 =
            (remaining * (int64_t)taper_q16) / target_magnitude;
        int64_t tapered =
            ((int64_t)acceleration * factor_q16) /
            (int64_t)PF_Q16_ONE;

        if (tapered == INT64_C(0))
        {
            tapered = acceleration < INT32_C(0)
                          ? -INT64_C(1)
                          : INT64_C(1);
        }
        acceleration = (int32_t)tapered;
    }
    return acceleration;
}

static int32_t pf_m4_apply_ground_input(
    const pf_m4_fighter_data *fighter,
    int32_t velocity_x,
    int16_t stick_x,
    int32_t maximum_speed_q16,
    int movement_mode)
{
    const int32_t target_x =
        pf_m4_scale_axis_q16(stick_x, maximum_speed_q16);
    int32_t acceleration;
    int64_t next;

    if (target_x == INT32_C(0))
    {
        return pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
    }
    acceleration = pf_m4_ground_input_acceleration(
        fighter,
        stick_x,
        velocity_x,
        target_x,
        movement_mode);
    next = (int64_t)velocity_x + (int64_t)acceleration;

    if ((int64_t)velocity_x * (int64_t)acceleration >= INT64_C(0))
    {
        if (acceleration > INT32_C(0) && next > (int64_t)target_x)
        {
            next =
                (int64_t)velocity_x - (int64_t)fighter->traction_q16;
            if (next < (int64_t)target_x)
            {
                next = target_x;
            }
            if (next >
                (int64_t)fighter->ground_max_horizontal_speed_q16)
            {
                next = fighter->ground_max_horizontal_speed_q16;
            }
        }
        else if (
            acceleration < INT32_C(0) && next < (int64_t)target_x)
        {
            next =
                (int64_t)velocity_x + (int64_t)fighter->traction_q16;
            if (next > (int64_t)target_x)
            {
                next = target_x;
            }
            if (next <
                -(int64_t)fighter->ground_max_horizontal_speed_q16)
            {
                next = -fighter->ground_max_horizontal_speed_q16;
            }
        }
    }
    return (int32_t)next;
}

static int32_t pf_m4_enter_initial_dash_velocity(
    const pf_m4_fighter_data *fighter,
    int32_t velocity_x,
    int8_t direction)
{
    const int32_t impulse =
        (int32_t)direction * fighter->initial_dash_speed_q16;

    if (velocity_x * (int32_t)direction < INT32_C(0))
    {
        return velocity_x + impulse;
    }
    return impulse;
}

static int32_t pf_m4_apply_air_input(
    const pf_m4_fighter_data *fighter,
    int32_t velocity_x,
    int16_t stick_x,
    int32_t maximum_speed_q16)
{
    const int8_t direction =
        stick_x < INT16_C(0)
            ? INT8_C(-1)
            : (stick_x > INT16_C(0) ? INT8_C(1) : INT8_C(0));
    const int32_t target_x =
        pf_m4_scale_axis_q16(stick_x, maximum_speed_q16);
    int32_t acceleration =
        pf_m4_scale_axis_q16(
            stick_x,
            fighter->air_acceleration_q16) +
        (int32_t)direction * fighter->air_base_acceleration_q16;
    int64_t next;

    if (direction == INT8_C(0))
    {
        return pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->air_friction_q16);
    }

    next = (int64_t)velocity_x + (int64_t)acceleration;
    if ((int64_t)velocity_x * (int64_t)acceleration >= INT64_C(0))
    {
        if (acceleration > INT32_C(0) && next > (int64_t)target_x)
        {
            next =
                (int64_t)velocity_x -
                (int64_t)fighter->air_friction_q16;
            if (next < (int64_t)target_x)
            {
                next = target_x;
            }
            if (next >
                (int64_t)fighter->air_max_horizontal_speed_q16)
            {
                next = fighter->air_max_horizontal_speed_q16;
            }
        }
        else if (
            acceleration < INT32_C(0) && next < (int64_t)target_x)
        {
            next =
                (int64_t)velocity_x +
                (int64_t)fighter->air_friction_q16;
            if (next > (int64_t)target_x)
            {
                next = target_x;
            }
            if (next <
                -(int64_t)fighter->air_max_horizontal_speed_q16)
            {
                next = -fighter->air_max_horizontal_speed_q16;
            }
        }
    }
    return (int32_t)next;
}

static int32_t pf_m4_falcon_dive_air_control(
    const pf_m4_fighter_data *fighter,
    const pf_m4_falcon_common_special_attributes *common,
    const pf_m4_falcon_special_attributes *special,
    int32_t axis_q16,
    int32_t internal_x_q16,
    int32_t maximum_q16)
{
    int32_t acceleration_q16 = INT32_C(0);
    int32_t target_q16 = INT32_C(0);
    int64_t candidate;

    if (internal_x_q16 > maximum_q16 ||
        internal_x_q16 < -maximum_q16)
    {
        return pf_m4_approach(
            internal_x_q16,
            INT32_C(0),
            common->air_drift_over_maximum_deceleration_q16);
    }
    if (axis_q16 >= common->air_drift_dead_zone_q16 ||
        axis_q16 <= -common->air_drift_dead_zone_q16)
    {
        acceleration_q16 = pf_m4_multiply_q16(
            axis_q16,
            pf_m4_multiply_q16(
                fighter->air_acceleration_q16,
                special->specialhi_air_friction_mul_q16));
        target_q16 = pf_m4_multiply_q16(axis_q16, maximum_q16);
    }
    if (target_q16 == INT32_C(0))
    {
        return INT32_C(0);
    }
    candidate = (int64_t)internal_x_q16 + (int64_t)acceleration_q16;
    if ((int64_t)internal_x_q16 * (int64_t)acceleration_q16 >=
        INT64_C(0))
    {
        if ((acceleration_q16 > INT32_C(0) &&
             candidate > (int64_t)target_q16) ||
            (acceleration_q16 < INT32_C(0) &&
             candidate < (int64_t)target_q16))
        {
            return target_q16;
        }
    }
    return (int32_t)candidate;
}

static pf_status pf_m4_falcon_dive_start_velocity(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input,
    uint8_t action_state,
    uint16_t action_ticks,
    int8_t *facing,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    const pf_m4_falcon_common_special_attributes *common;
    const pf_m4_falcon_special_attributes *special;
    const pf_m4_falcon_up_special_timing *timing;
    uint16_t displayed_frame;
    int8_t previous_facing;
    int32_t axis_q16;
    int32_t maximum_q16;
    int32_t previous_root_x_q16 = INT32_C(0);
    int32_t root_x_q16;
    int32_t root_y_q16;
    int32_t internal_x_q16;

    if (fighter == NULL || input == NULL || facing == NULL ||
        velocity_x_q16 == NULL || velocity_y_q16 == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    common = pf_m4_falcon_reference_common_special_attributes();
    special = pf_m4_falcon_reference_special_attributes();
    timing = pf_m4_falcon_reference_up_special_timing();
    displayed_frame = (uint16_t)(action_ticks + UINT16_C(1));
    previous_facing = *facing;
    axis_q16 = pf_m4_axis_q16(input->main_stick_x);
    maximum_q16 =
        special != NULL
            ? pf_m4_multiply_q16(
                  fighter->air_speed_q16,
                  special->specialhi_horz_vel_q16)
            : INT32_C(0);
    if (common == NULL || special == NULL || timing == NULL ||
        maximum_q16 <= INT32_C(0) ||
        !pf_m4_falcon_reference_motion_x_q16(
            action_state,
            displayed_frame,
            &root_x_q16) ||
        !pf_m4_falcon_reference_motion_y_q16(
            action_state,
            displayed_frame,
            &root_y_q16) ||
        (action_ticks != UINT16_C(0) &&
         !pf_m4_falcon_reference_motion_x_q16(
             action_state,
             action_ticks,
             &previous_root_x_q16)))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    internal_x_q16 =
        action_ticks == UINT16_C(0)
            ? INT32_C(0)
            : *velocity_x_q16 -
                  (int32_t)previous_facing * previous_root_x_q16;
    /* ftCa_SpecialHiAir_IASA consumes and clears the action-script command
     * variable once. Direction may change on that exact gate frame only;
     * later air-control samples do not repeatedly turn Falcon. */
    if (displayed_frame == timing->air_control_begin_frame &&
        (axis_q16 > special->specialhi_input_var_q16 ||
         axis_q16 < -special->specialhi_input_var_q16))
    {
        *facing = axis_q16 < INT32_C(0) ? INT8_C(-1) : INT8_C(1);
    }
    *velocity_x_q16 =
        (int32_t)*facing * root_x_q16 +
        pf_m4_falcon_dive_air_control(
            fighter,
            common,
            special,
            axis_q16,
            internal_x_q16,
            maximum_q16);
    *velocity_y_q16 = root_y_q16;
    return PF_STATUS_OK;
}

static pf_status pf_m4_falcon_dive_throw_velocity(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input,
    uint16_t action_ticks,
    int8_t facing,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    const pf_m4_falcon_common_special_attributes *common =
        pf_m4_falcon_reference_common_special_attributes();
    const pf_m4_falcon_special_attributes *special =
        pf_m4_falcon_reference_special_attributes();
    const pf_m4_falcon_up_special_timing *timing =
        pf_m4_falcon_reference_up_special_timing();
    const uint16_t displayed_frame =
        (uint16_t)(action_ticks + UINT16_C(1));
    int32_t root_x_q16;
    int32_t root_y_q16;
    int32_t previous_root_x_q16 = INT32_C(0);
    int32_t previous_root_y_q16 = INT32_C(0);
    int32_t internal_x_q16 = INT32_C(0);
    int32_t internal_y_q16 = INT32_C(0);
    int32_t maximum_q16;

    if (fighter == NULL || input == NULL || velocity_x_q16 == NULL ||
        velocity_y_q16 == NULL || common == NULL || special == NULL ||
        timing == NULL ||
        !pf_m4_falcon_reference_motion_x_q16(
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
            displayed_frame,
            &root_x_q16) ||
        !pf_m4_falcon_reference_motion_y_q16(
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
            displayed_frame,
            &root_y_q16) ||
        (action_ticks != UINT16_C(0) &&
         (!pf_m4_falcon_reference_motion_x_q16(
              (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
              action_ticks,
              &previous_root_x_q16) ||
          !pf_m4_falcon_reference_motion_y_q16(
              (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW,
              action_ticks,
              &previous_root_y_q16))))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    maximum_q16 = pf_m4_multiply_q16(
        fighter->air_speed_q16,
        special->specialhi_horz_vel_q16);
    if (maximum_q16 <= INT32_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (displayed_frame >= timing->throw_gravity_begin_frame)
    {
        internal_x_q16 =
            *velocity_x_q16 - (int32_t)facing * previous_root_x_q16;
        internal_y_q16 = *velocity_y_q16 - previous_root_y_q16;
        internal_x_q16 = pf_m4_falcon_dive_air_control(
            fighter,
            common,
            special,
            pf_m4_axis_q16(input->main_stick_x),
            internal_x_q16,
            maximum_q16);
        internal_y_q16 = pf_m4_approach(
            internal_y_q16,
            fighter->fall_speed_q16,
            pf_m4_falcon_source_velocity_to_sim_q16(
                special->specialhi_catch_grav_q16,
                INT32_C(11),
                INT32_C(62)));
    }
    *velocity_x_q16 = (int32_t)facing * root_x_q16 + internal_x_q16;
    *velocity_y_q16 = root_y_q16 + internal_y_q16;
    if (*velocity_y_q16 > fighter->fall_speed_q16)
    {
        *velocity_y_q16 = fighter->fall_speed_q16;
    }
    return PF_STATUS_OK;
}

static const pf_m4_reference_move *pf_m4_falcon_move_for_action(
    uint8_t action_state)
{
    pf_m4_falcon_move_index move_index;

    return pf_m4_falcon_reference_move_for_action(
               action_state,
               &move_index) != 0
               ? pf_m4_falcon_reference_move(move_index)
               : NULL;
}

static pf_status pf_m4_falcon_kick_root_velocity(
    uint8_t action_state,
    uint16_t action_ticks,
    int8_t facing,
    int include_vertical,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    const uint16_t displayed_frame =
        (uint16_t)(action_ticks + UINT16_C(1));
    int32_t root_x_q16;
    int32_t root_y_q16 = INT32_C(0);

    if (velocity_x_q16 == NULL || velocity_y_q16 == NULL ||
        !pf_m4_falcon_reference_motion_x_q16(
            action_state,
            displayed_frame,
            &root_x_q16) ||
        (include_vertical != 0 &&
         !pf_m4_falcon_reference_motion_y_q16(
             action_state,
             displayed_frame,
             &root_y_q16)))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    *velocity_x_q16 = (int32_t)facing * root_x_q16;
    *velocity_y_q16 = root_y_q16;
    return PF_STATUS_OK;
}

static int32_t pf_m4_falcon_kick_hit_velocity_scale(
    const pf_m4_falcon_special_attributes *attributes,
    uint8_t hit_count)
{
    int32_t scale_q16 = PF_Q16_ONE;

    while (hit_count != UINT8_C(0))
    {
        scale_q16 = pf_m4_multiply_q16(
            scale_q16,
            attributes->speciallw_on_hit_spd_modifier_q16);
        --hit_count;
    }
    return scale_q16;
}

static int32_t pf_m4_falcon_source_ground_friction(
    const pf_m4_falcon_common_attributes *common,
    const pf_m4_falcon_common_special_attributes *common_special,
    int32_t velocity_x_q16)
{
    const int32_t speed_q16 =
        velocity_x_q16 < INT32_C(0)
            ? -velocity_x_q16
            : velocity_x_q16;

    return speed_q16 > common->walk_maximum_velocity_q16
               ? pf_m4_multiply_q16(
                     common->friction_q16,
                     common_special
                         ->fast_ground_friction_multiplier_q16)
               : common->friction_q16;
}

static int32_t pf_m4_falcon_kick_parallel_velocity(
    int32_t unscaled_velocity_q16,
    int32_t applied_friction_q16,
    int32_t hit_scale_q16)
{
    return pf_m4_multiply_q16(unscaled_velocity_q16, hit_scale_q16) -
           pf_m4_multiply_q16(
               applied_friction_q16,
               PF_Q16_ONE - hit_scale_q16);
}

static pf_status pf_m4_falcon_kick_ground_end_velocity(
    uint16_t action_ticks,
    int8_t facing,
    uint8_t hit_count,
    int is_entry_frame,
    int32_t *velocity_x_q16)
{
    const pf_m4_reference_move *start_move =
        pf_m4_falcon_move_for_action(
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND);
    const pf_m4_falcon_common_attributes *common =
        pf_m4_falcon_reference_common_attributes();
    const pf_m4_falcon_common_special_attributes *common_special =
        pf_m4_falcon_reference_common_special_attributes();
    const pf_m4_falcon_special_attributes *attributes =
        pf_m4_falcon_reference_special_attributes();
    const pf_m4_falcon_down_special_timing *timing =
        pf_m4_falcon_reference_down_special_timing();
    int32_t root_velocity_q16;
    int32_t ignored_velocity_y_q16;
    int32_t unscaled_velocity_q16;
    int32_t applied_friction_q16;
    int32_t hit_scale_q16;
    uint16_t step;

    if (velocity_x_q16 == NULL || start_move == NULL || common == NULL ||
        common_special == NULL || attributes == NULL || timing == NULL ||
        start_move->total_frames == UINT16_C(0) ||
        pf_m4_falcon_kick_root_velocity(
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND,
            (uint16_t)(start_move->total_frames - UINT16_C(1)),
            1,
            0,
            &root_velocity_q16,
            &ignored_velocity_y_q16) != PF_STATUS_OK)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    unscaled_velocity_q16 = pf_m4_multiply_q16(
        root_velocity_q16,
        timing->ground_end_entry_velocity_scale_q16);
    hit_scale_q16 = pf_m4_falcon_kick_hit_velocity_scale(
        attributes,
        hit_count);
    /* Melee advances gr_vel and self_vel in parallel here. Reconstruct the
     * bounded ground channel from imported root motion and friction instead
     * of serializing a duplicate runtime velocity. */
    if (is_entry_frame != 0)
    {
        applied_friction_q16 = pf_m4_falcon_source_ground_friction(
            common,
            common_special,
            root_velocity_q16);
    }
    else
    {
        applied_friction_q16 = INT32_C(0);
        for (step = UINT16_C(0); step <= action_ticks; ++step)
        {
            const uint16_t displayed_frame =
                (uint16_t)(step + UINT16_C(2));
            const int32_t friction_q16 =
                displayed_frame >= timing->ground_end_traction_begin_frame &&
                        displayed_frame <=
                            timing->ground_end_traction_end_frame
                    ? pf_m4_multiply_q16(
                          common->friction_q16,
                          attributes->speciallw_ground_traction_q16)
                    : pf_m4_falcon_source_ground_friction(
                          common,
                          common_special,
                          unscaled_velocity_q16);
            const int32_t next_velocity_q16 = pf_m4_approach(
                unscaled_velocity_q16,
                INT32_C(0),
                friction_q16);

            applied_friction_q16 =
                unscaled_velocity_q16 - next_velocity_q16;
            unscaled_velocity_q16 = next_velocity_q16;
        }
    }
    *velocity_x_q16 = (int32_t)facing *
        pf_m4_falcon_kick_parallel_velocity(
            unscaled_velocity_q16,
            applied_friction_q16,
            hit_scale_q16);
    return PF_STATUS_OK;
}

static void pf_m4_falcon_source_air_physics(
    const pf_m4_falcon_common_attributes *common,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    *velocity_x_q16 = pf_m4_approach(
        *velocity_x_q16,
        INT32_C(0),
        common->air_friction_q16);
    *velocity_y_q16 = pf_m4_approach(
        *velocity_y_q16,
        common->terminal_velocity_q16,
        common->gravity_q16);
    if (*velocity_y_q16 > common->terminal_velocity_q16)
    {
        *velocity_y_q16 = common->terminal_velocity_q16;
    }
}

static int32_t pf_m4_moonwalk_sweep_velocity(
    const pf_m4_fighter_data *fighter,
    int32_t velocity_x,
    int16_t stick_x)
{
    return pf_m4_apply_ground_input(
        fighter,
        velocity_x,
        stick_x,
        fighter->initial_dash_speed_q16,
        0);
}

static int pf_m4_body_overlaps_horizontal_interval(
    int32_t position_x,
    int32_t half_width,
    int32_t interval_left,
    int32_t interval_right)
{
    return (int64_t)position_x + (int64_t)half_width >
               (int64_t)interval_left &&
           (int64_t)position_x - (int64_t)half_width <
               (int64_t)interval_right;
}

int32_t pf_m4_platform_center_x_q16(
    const pf_m4_stage_data *stage,
    uint64_t tick)
{
    const uint64_t period =
        (uint64_t)stage->platform_motion_period_ticks;
    const uint64_t half_period = period / UINT64_C(2);
    const uint64_t phase = tick % period;
    int64_t offset;

    if (phase <= half_period)
    {
        offset =
            -(int64_t)stage->platform_motion_amplitude_q16 +
            (INT64_C(2) *
             (int64_t)stage->platform_motion_amplitude_q16 *
             (int64_t)phase) /
                (int64_t)half_period;
    }
    else
    {
        const uint64_t descending_phase = phase - half_period;
        offset =
            (int64_t)stage->platform_motion_amplitude_q16 -
            (INT64_C(2) *
             (int64_t)stage->platform_motion_amplitude_q16 *
             (int64_t)descending_phase) /
                (int64_t)half_period;
    }
    return (int32_t)(
        (int64_t)stage->platform_center_x_q16 + offset);
}

static int pf_m4_find_drop_cancel_platform(
    const pf_m4_stage_data *stage,
    const pf_m4_fighter_data *fighter,
    uint64_t tick,
    int32_t position_x_q16,
    int32_t position_y_q16,
    int32_t *out_surface_y_q16,
    uint8_t *out_support)
{
    const int32_t platform_center =
        pf_m4_platform_center_x_q16(stage, tick);
    const int32_t platform_left =
        platform_center - stage->platform_half_width_q16;
    const int32_t platform_right =
        platform_center + stage->platform_half_width_q16;
    const int32_t upper_left =
        stage->upper_platform_center_x_q16 -
        stage->upper_platform_half_width_q16;
    const int32_t upper_right =
        stage->upper_platform_center_x_q16 +
        stage->upper_platform_half_width_q16;
    const int64_t body_bottom =
        (int64_t)position_y_q16 + fighter->half_height_q16;
    int64_t best_distance = INT64_MAX;

    if (position_x_q16 >= platform_left &&
        position_x_q16 <= platform_right)
    {
        const int64_t distance =
            body_bottom - (int64_t)stage->platform_y_q16;

        if (distance >= INT64_C(0) &&
            distance <= fighter->drop_cancel_snap_distance_q16)
        {
            best_distance = distance;
            *out_surface_y_q16 = stage->platform_y_q16;
            *out_support = (uint8_t)PF_M4_SURFACE_PLATFORM;
        }
    }
    if (position_x_q16 >= upper_left &&
        position_x_q16 <= upper_right)
    {
        const int64_t distance =
            body_bottom - (int64_t)stage->upper_platform_y_q16;

        if (distance >= INT64_C(0) &&
            distance <= fighter->drop_cancel_snap_distance_q16 &&
            distance < best_distance)
        {
            best_distance = distance;
            *out_surface_y_q16 = stage->upper_platform_y_q16;
            *out_support = (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
        }
    }
    return best_distance != INT64_MAX;
}

static int32_t pf_m4_surface_y_q16(
    const pf_m4_content *content,
    uint8_t support,
    int32_t position_x_q16)
{
    const pf_m4_ssbm_stage_collision_line *line =
        pf_m4_ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        return pf_m4_ssbm_stage_line_y_q16(line, position_x_q16);
    }
    if (support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        return content->stage.platform_y_q16;
    }
    if (support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
    {
        return content->stage.solid_top_q16;
    }
    if (support == (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM)
    {
        return content->stage.upper_platform_y_q16;
    }
    return content->stage.floor_y_q16;
}

static void pf_m4_surface_ground_projection_q16(
    const pf_m4_content *content,
    uint8_t support,
    int32_t *out_x_q16,
    int32_t *out_y_q16)
{
    const pf_m4_ssbm_stage_collision_line *line =
        pf_m4_ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        *out_x_q16 = line->ground_projection_x_q16;
        *out_y_q16 = line->ground_projection_y_q16;
        return;
    }
    *out_x_q16 = PF_Q16_ONE;
    *out_y_q16 = INT32_C(0);
}

static void pf_m4_project_ground_scalar_q16(
    const pf_m4_content *content,
    uint8_t support,
    int32_t scalar_q16,
    int32_t *out_x_q16,
    int32_t *out_y_q16)
{
    int32_t projection_x_q16;
    int32_t projection_y_q16;

    pf_m4_surface_ground_projection_q16(
        content,
        support,
        &projection_x_q16,
        &projection_y_q16);
    *out_x_q16 = pf_m4_multiply_q16(scalar_q16, projection_x_q16);
    *out_y_q16 = pf_m4_multiply_q16(scalar_q16, projection_y_q16);
}

static uint8_t pf_m4_stage_spawn_support(const pf_m4_stage_data *stage)
{
    if (stage->reference_collision_profile !=
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        return (uint8_t)(stage->reference_spawn_line + UINT16_C(1));
    }
    return (uint8_t)PF_M4_SURFACE_FLOOR;
}

static uint16_t pf_m4_damage_fly_ecb_frame_index(uint16_t action_ticks)
{
    return action_ticks < PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT
               ? action_ticks
               : (uint16_t)(
                     PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT -
                     UINT16_C(1));
}

static uint16_t pf_m4_clamped_pose_index(
    uint16_t action_ticks,
    uint16_t frame_count)
{
    return action_ticks < frame_count
               ? action_ticks
               : (uint16_t)(frame_count - UINT16_C(1));
}

static int pf_m4_reference_ecb_pose_q16(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    int32_t inherited_locked_bottom_y_q16,
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int32_t fall_animation_blend_q16,
    uint8_t fall_animation_target_switched,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing,
    int32_t ground_loop_progress_q16,
    const pf_m4_hsd_compact_pose *ground_loop_compact,
    pf_m4_falcon_ecb_pose_q16 *out_pose)
{
    const pf_m4_falcon_collision_pose *pose =
        pf_m4_falcon_reference_collision_pose();
    const pf_m4_falcon_ecb_pose_q16 *prone_pose;
    const pf_m4_falcon_ecb_pose_q16 *guard_pose;
    const pf_m4_falcon_ecb_pose_q16 *airborne_pose;
    uint16_t frame_index;
    int32_t locked_bottom_y_q16 =
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16;
    int32_t retained_hsd_frame_q16;
    int retained_hsd_pose;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) || pose == NULL ||
        out_pose == NULL)
    {
        return 0;
    }
    if (ground_loop_compact != NULL)
    {
        const pf_m4_hsd_pose_data *data =
            pf_m4_falcon_reference_hsd_pose_data();
        pf_m4_hsd_local_pose blended[PF_M4_HSD_POSE_MAX_JOINTS];

        if (data == NULL ||
            !pf_m4_hsd_resolve_compact_pose_q16(
                data, source_submotion, source_animation_frame_q16,
                ground_loop_progress_q16, ground_loop_compact, blended) ||
            !pf_m4_falcon_reference_hsd_ground_ecb_pose_from_local_pose(
                blended, out_pose))
        {
            return 0;
        }
        return 1;
    }
    retained_hsd_pose = pf_m4_falcon_reference_retained_hsd_pose(
            action_state,
            source_submotion,
            action_ticks,
            source_animation_frame_q16,
            &retained_hsd_frame_q16);
    if (retained_hsd_pose)
    {
        source_animation_frame_q16 = retained_hsd_frame_q16;
    }
    if (!pf_m4_falcon_direct_hsd_locked_bottom_q16(
            action_state,
            source_animation_frame_q16,
            grounded,
            &locked_bottom_y_q16))
    {
        return 0;
    }
    if (locked_bottom_y_q16 ==
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16 &&
        inherited_locked_bottom_y_q16 !=
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16)
    {
        locked_bottom_y_q16 = inherited_locked_bottom_y_q16;
    }
    if (pf_m4_action_uses_fall_special_pose(action_state) &&
        pf_m4_falcon_reference_hsd_fall_special_ecb_pose(
            source_submotion,
            source_animation_frame_q16,
            fall_animation_blend_q16,
            fall_animation_target_switched,
            locked_bottom_y_q16,
            out_pose))
    {
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        frame_index = pf_m4_clamped_pose_index(
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
        frame_index = pf_m4_clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_SHIELD_BREAK_DOWN_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_down_down[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN)
    {
        frame_index = pf_m4_clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_SHIELD_BREAK_STAND_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_stand_down[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA)
    {
        frame_index = pf_m4_clamped_pose_index(
            (uint16_t)(
                source_animation_frame_q16 > INT32_C(0)
                    ? source_animation_frame_q16 /
                          (int32_t)PF_Q16_ONE
                    : INT32_C(0)),
            PF_M4_FALCON_SHIELD_BREAK_STUN_ECB_FRAME_COUNT);
        *out_pose = pose->shield_break_stun[frame_index];
        return 1;
    }
    guard_pose = pf_m4_falcon_reference_guard_ecb_pose(
        action_state, source_submotion, action_ticks);
    if (guard_pose != NULL)
    {
        *out_pose = *guard_pose;
        return 1;
    }
    if (pf_m4_falcon_reference_action_hsd_ecb_pose(
            action_state,
            action_ticks,
            grounded,
            locked_bottom_y_q16,
            out_pose))
    {
        return 1;
    }
    if (pf_m4_falcon_reference_hsd_ecb_pose(
            source_submotion,
            action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                    !pf_m4_falcon_wait_hsd_pose_is_direct(
                        source_submotion,
                        source_animation_frame_q16)
                ? INT32_C(0)
                : source_animation_frame_q16,
            (pf_m4_action_uses_direct_hsd_pose(action_state) ||
             retained_hsd_pose)
                ? grounded != UINT8_C(0)
                : 1,
            locked_bottom_y_q16,
            out_pose))
    {
        return 1;
    }
    airborne_pose =
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE
            ? pf_m4_falcon_reference_airborne_ecb_pose(
                  source_submotion,
                  action_ticks)
            : NULL;
    if (airborne_pose != NULL)
    {
        *out_pose = *airborne_pose;
        return pf_m4_falcon_reference_ecb_apply_bottom_lock_q16(
            inherited_locked_bottom_y_q16,
            out_pose);
    }
    prone_pose = pf_m4_falcon_reference_prone_ecb_pose(
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
        frame_index = pf_m4_damage_fly_ecb_frame_index(action_ticks);
        out_pose->top_x_from_origin_q16 = INT32_C(0);
        out_pose->top_y_from_origin_q16 =
            pose->damage_fly_top_y_from_origin_q16[frame_index];
        out_pose->bottom_x_from_origin_q16 = INT32_C(0);
        out_pose->bottom_y_from_origin_q16 =
            pose->damage_fly_bottom_y_from_origin_q16[frame_index];
        out_pose->right_x_from_origin_q16 =
            pose->damage_fly_side_x_from_origin_q16[frame_index];
        out_pose->right_y_from_origin_q16 =
            pose->damage_fly_side_y_from_origin_q16[frame_index];
        out_pose->left_x_from_origin_q16 =
            -pose->damage_fly_side_x_from_origin_q16[frame_index];
        out_pose->left_y_from_origin_q16 =
            pose->damage_fly_side_y_from_origin_q16[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_CEILING_BOUNCE)
    {
        frame_index = pf_m4_clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT);
        *out_pose = pose->ceiling_bounce[frame_index];
        return 1;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_WALL_BOUNCE)
    {
        frame_index = pf_m4_clamped_pose_index(
            action_ticks,
            PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT);
        *out_pose = pose->wall_bounce[frame_index];
        return 1;
    }
    return 0;
}

static void pf_m4_ecb_world_wall_side_q16(
    const pf_m4_falcon_ecb_pose_q16 *pose,
    int8_t facing,
    int moving_right,
    int32_t *out_x_from_origin_q16,
    int32_t *out_y_from_origin_q16)
{
    if (moving_right != 0)
    {
        if (facing >= INT8_C(0))
        {
            *out_x_from_origin_q16 = pose->right_x_from_origin_q16;
            *out_y_from_origin_q16 = pose->right_y_from_origin_q16;
        }
        else
        {
            *out_x_from_origin_q16 = -pose->left_x_from_origin_q16;
            *out_y_from_origin_q16 = pose->left_y_from_origin_q16;
        }
    }
    else if (facing >= INT8_C(0))
    {
        *out_x_from_origin_q16 = pose->left_x_from_origin_q16;
        *out_y_from_origin_q16 = pose->left_y_from_origin_q16;
    }
    else
    {
        *out_x_from_origin_q16 = -pose->right_x_from_origin_q16;
        *out_y_from_origin_q16 = pose->right_y_from_origin_q16;
    }
}

static int32_t pf_m4_floor_contact_bottom_extent_q16(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    int32_t inherited_locked_bottom_y_q16,
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int32_t fall_animation_blend_q16,
    uint8_t fall_animation_target_switched,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing,
    int *out_exact_reference_pose)
{
    const pf_m4_falcon_collision_pose *pose =
        pf_m4_falcon_reference_collision_pose();
    int32_t bottom_y_from_origin_q16 = INT32_C(0);
    int has_reference_pose = 0;
    int exact_reference_pose = 0;
    pf_m4_falcon_ecb_pose_q16 action_pose;
    const pf_m4_falcon_ecb_pose_q16 *airborne_pose = NULL;

    *out_exact_reference_pose = 0;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) || pose == NULL)
    {
        return fighter->half_height_q16;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        airborne_pose = pf_m4_falcon_reference_airborne_ecb_pose(
            source_submotion,
            action_ticks);
    }
    if (pf_m4_reference_ecb_pose_q16(
            fighter,
            action_state,
            action_ticks,
            grounded,
            inherited_locked_bottom_y_q16,
            source_submotion,
            source_animation_frame_q16,
            fall_animation_blend_q16,
            fall_animation_target_switched,
            prone_orientation,
            prone_roll_motion_orientation,
            tech_direction,
            facing,
            INT32_C(0),
            NULL,
            &action_pose) != 0)
    {
        bottom_y_from_origin_q16 = action_pose.bottom_y_from_origin_q16;
        has_reference_pose = 1;
        exact_reference_pose = 1;
    }
    else if (airborne_pose != NULL)
    {
        bottom_y_from_origin_q16 = airborne_pose->bottom_y_from_origin_q16;
        has_reference_pose = 1;
        exact_reference_pose = 1;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        const uint16_t frame_index =
            action_ticks < PF_M4_FALCON_AIR_DODGE_ECB_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(
                      PF_M4_FALCON_AIR_DODGE_ECB_FRAME_COUNT -
                      UINT16_C(1));

        bottom_y_from_origin_q16 =
            pose->air_dodge_bottom_y_from_origin_q16[frame_index];
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

        bottom_y_from_origin_q16 =
            pose->platform_drop_bottom_y_from_origin_q16[frame_index];
        has_reference_pose = 1;
        exact_reference_pose = 1;
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
             action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW)
    {
        bottom_y_from_origin_q16 = pose->falling_bottom_y_from_origin_q16;
        has_reference_pose = 1;
    }
    if (has_reference_pose != 0 &&
        bottom_y_from_origin_q16 >= INT32_C(0))
    {
        *out_exact_reference_pose = exact_reference_pose;
        return fighter->half_height_q16 - bottom_y_from_origin_q16;
    }
    return fighter->half_height_q16;
}

static uint8_t pf_m4_down_bound_floor_contact(
    uint8_t prone_orientation,
    uint16_t action_ticks)
{
    const pf_m4_falcon_collision_pose *pose =
        pf_m4_falcon_reference_collision_pose();
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

static int pf_m4_surface_is_pass_through(
    const pf_m4_content *content,
    uint8_t support)
{
    const pf_m4_ssbm_stage_collision_line *line =
        pf_m4_ssbm_reference_stage_line(
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

static void pf_m4_surface_bounds_q16(
    const pf_m4_content *content,
    uint8_t support,
    uint64_t tick,
    int32_t *out_left,
    int32_t *out_right)
{
    const pf_m4_ssbm_stage_collision_line *line =
        pf_m4_ssbm_reference_stage_line(
            content->stage.reference_collision_profile,
            support);

    if (line != NULL)
    {
        *out_left = line->start_x_q16 < line->end_x_q16
                        ? line->start_x_q16
                        : line->end_x_q16;
        *out_right = line->start_x_q16 > line->end_x_q16
                         ? line->start_x_q16
                         : line->end_x_q16;
        return;
    }
    if (support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        const int32_t center =
            pf_m4_platform_center_x_q16(&content->stage, tick);
        *out_left = center - content->stage.platform_half_width_q16;
        *out_right = center + content->stage.platform_half_width_q16;
    }
    else if (support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
    {
        *out_left = content->stage.solid_left_q16;
        *out_right = content->stage.solid_right_q16;
    }
    else if (support == (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM)
    {
        *out_left =
            content->stage.upper_platform_center_x_q16 -
            content->stage.upper_platform_half_width_q16;
        *out_right =
            content->stage.upper_platform_center_x_q16 +
            content->stage.upper_platform_half_width_q16;
    }
    else
    {
        *out_left = content->stage.floor_left_q16;
        *out_right = content->stage.floor_right_q16;
    }
}

typedef enum pf_m4_pass_through_floor_sweep_policy
{
    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT = 0,
    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DEFERRED = 1,
    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT_OR_DEFERRED = 2
} pf_m4_pass_through_floor_sweep_policy;

static int pf_m4_floor_sweep_crosses_surface(
    int32_t previous_floor_contact_q16,
    int32_t new_floor_contact_q16,
    int32_t surface_y_q16,
    int is_pass_through,
    uint8_t fast_fall,
    pf_m4_pass_through_floor_sweep_policy pass_through_policy)
{
    const int direct_crossing =
        previous_floor_contact_q16 <= surface_y_q16 &&
        new_floor_contact_q16 >= surface_y_q16;

    if (is_pass_through != 0 && fast_fall == UINT8_C(0) &&
        pass_through_policy != PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT)
    {
        const int64_t previous_overshoot =
            (int64_t)previous_floor_contact_q16 -
            (int64_t)surface_y_q16;
        const int64_t current_displacement =
            (int64_t)new_floor_contact_q16 -
            (int64_t)previous_floor_contact_q16;

        const int immediately_previous_crossing =
            previous_overshoot > INT64_C(0) &&
            current_displacement > INT64_C(0) &&
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

static int pf_m4_reference_stage_find_floor_landing(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t previous_floor_contact_q16,
    int32_t new_floor_contact_q16,
    uint8_t fast_fall,
    pf_m4_pass_through_floor_sweep_policy pass_through_policy,
    int pass_through_allowed,
    uint8_t platform_drop_ticks,
    int32_t *out_surface_y_q16,
    uint8_t *out_support)
{
    const pf_m4_ssbm_stage_collision_profile *profile =
        pf_m4_ssbm_reference_stage_collision(
            content->stage.reference_collision_profile);
    int32_t best_surface_y_q16 = INT32_MAX;
    uint16_t floor_offset;

    if (profile == NULL || out_surface_y_q16 == NULL ||
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
        const pf_m4_ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const uint8_t support = (uint8_t)(line_index + UINT16_C(1));
        const int32_t left =
            line->start_x_q16 < line->end_x_q16
                ? line->start_x_q16
                : line->end_x_q16;
        const int32_t right =
            line->start_x_q16 > line->end_x_q16
                ? line->start_x_q16
                : line->end_x_q16;
        const int is_pass_through =
            pf_m4_surface_is_pass_through(content, support);
        int32_t surface_y_q16;
        int crosses_surface;

        if ((line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0) ||
            line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
            position_x_q16 < left || position_x_q16 > right ||
            (is_pass_through != 0 &&
             (pass_through_allowed == 0 ||
              platform_drop_ticks != UINT8_C(0))))
        {
            continue;
        }
        surface_y_q16 =
            pf_m4_surface_y_q16(content, support, position_x_q16);
        crosses_surface = pf_m4_floor_sweep_crosses_surface(
            previous_floor_contact_q16,
            new_floor_contact_q16,
            surface_y_q16,
            is_pass_through,
            fast_fall,
            pass_through_policy);
        if (crosses_surface != 0 &&
            surface_y_q16 < best_surface_y_q16)
        {
            best_surface_y_q16 = surface_y_q16;
            *out_support = support;
        }
    }
    if (*out_support == (uint8_t)PF_M4_SURFACE_NONE)
    {
        return 0;
    }
    *out_surface_y_q16 = best_surface_y_q16;
    return 1;
}

static int pf_m4_body_sweep_hits_solid(
    const pf_m4_content *content,
    int32_t previous_position_x_q16,
    int32_t previous_position_y_q16,
    int32_t current_position_x_q16,
    int32_t current_position_y_q16)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const int64_t left =
        (int64_t)current_position_x_q16 - fighter->half_width_q16;
    const int64_t right =
        (int64_t)current_position_x_q16 + fighter->half_width_q16;
    const int64_t top =
        (int64_t)current_position_y_q16 - fighter->half_height_q16;
    const int64_t bottom =
        (int64_t)current_position_y_q16 + fighter->half_height_q16;

    if (stage->reference_collision_profile !=
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        const int64_t previous_top =
            (int64_t)previous_position_y_q16 - fighter->half_height_q16;
        const int64_t previous_bottom =
            (int64_t)previous_position_y_q16 + fighter->half_height_q16;
        const int64_t swept_top = previous_top < top ? previous_top : top;
        const int64_t swept_bottom =
            previous_bottom > bottom ? previous_bottom : bottom;
        int32_t contact_position_x_q16;
        int32_t contact_y_q16;
        uint8_t contact_support;
        int8_t away_direction;

        if (pf_m4_ssbm_reference_stage_find_wall_contact(
                stage->reference_collision_profile,
                previous_position_x_q16,
                current_position_x_q16,
                swept_top,
                swept_bottom,
                fighter->half_width_q16,
                &contact_position_x_q16,
                &contact_support,
                &away_direction))
        {
            return 1;
        }
        if (current_position_y_q16 < previous_position_y_q16 &&
            pf_m4_ssbm_reference_stage_find_ceiling_contact(
                stage->reference_collision_profile,
                current_position_x_q16,
                previous_top,
                top,
                &contact_y_q16,
                &contact_support))
        {
            return 1;
        }
        if (current_position_y_q16 > previous_position_y_q16)
        {
            contact_support = (uint8_t)PF_M4_SURFACE_NONE;
            if (previous_bottom < INT32_MIN || previous_bottom > INT32_MAX ||
                bottom < INT32_MIN || bottom > INT32_MAX)
            {
                return 1;
            }
            if (pf_m4_reference_stage_find_floor_landing(
                    content,
                    current_position_x_q16,
                    (int32_t)previous_bottom,
                    (int32_t)bottom,
                    UINT8_C(0),
                    PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT,
                    1,
                    UINT8_C(0),
                    &contact_y_q16,
                    &contact_support))
            {
                return 1;
            }
        }
        return 0;
    }

    return right > (int64_t)stage->solid_left_q16 &&
           left < (int64_t)stage->solid_right_q16 &&
           bottom > (int64_t)stage->solid_top_q16 &&
           top < (int64_t)stage->solid_bottom_q16;
}

static int8_t pf_m4_wall_contact_away_direction(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const int64_t body_top =
        (int64_t)position_y_q16 - fighter->half_height_q16;
    const int64_t body_bottom =
        (int64_t)position_y_q16 + fighter->half_height_q16;
    const int vertical_overlap =
        body_bottom > (int64_t)stage->solid_top_q16 &&
        body_top < (int64_t)stage->solid_bottom_q16;

    if (stage->reference_collision_profile !=
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        int32_t contact_position_x_q16;
        uint8_t contact_support;
        int8_t away_direction;

        if (position_x_q16 > INT32_MIN &&
            pf_m4_ssbm_reference_stage_find_wall_contact(
                stage->reference_collision_profile,
                position_x_q16 - INT32_C(1),
                position_x_q16,
                body_top,
                body_bottom,
                fighter->half_width_q16,
                &contact_position_x_q16,
                &contact_support,
                &away_direction) &&
            contact_position_x_q16 == position_x_q16)
        {
            return away_direction;
        }
        if (position_x_q16 < INT32_MAX &&
            pf_m4_ssbm_reference_stage_find_wall_contact(
                stage->reference_collision_profile,
                position_x_q16 + INT32_C(1),
                position_x_q16,
                body_top,
                body_bottom,
                fighter->half_width_q16,
                &contact_position_x_q16,
                &contact_support,
                &away_direction) &&
            contact_position_x_q16 == position_x_q16)
        {
            return away_direction;
        }
        return INT8_C(0);
    }

    if (!vertical_overlap)
    {
        return INT8_C(0);
    }
    if ((int64_t)position_x_q16 + fighter->half_width_q16 ==
        (int64_t)stage->solid_left_q16)
    {
        return INT8_C(-1);
    }
    if ((int64_t)position_x_q16 - fighter->half_width_q16 ==
        (int64_t)stage->solid_right_q16)
    {
        return INT8_C(1);
    }
    return INT8_C(0);
}

static pf_status pf_m4_apply_hitlag_shift(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int16_t stick_x,
    int16_t stick_y,
    int32_t maximum_distance_x_q16,
    int32_t maximum_distance_y_q16,
    int preserve_ground_support)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const int32_t old_x = scratch->position_x_q16[player_index];
    const int32_t old_y = scratch->position_y_q16[player_index];
    int32_t next_x;
    int32_t next_y;
    int64_t shifted_x;
    int64_t shifted_y;

    shifted_x =
        (int64_t)old_x +
        (int64_t)pf_m4_ssbm_analog_displacement_q16(
            stick_x,
            maximum_distance_x_q16);
    shifted_y =
        (int64_t)old_y +
        (int64_t)pf_m4_ssbm_analog_displacement_q16(
            stick_y,
            maximum_distance_y_q16);
    if (!pf_m4_checked_i32(shifted_x, &next_x) ||
        !pf_m4_checked_i32(shifted_y, &next_y))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (pf_m4_body_sweep_hits_solid(
            content,
            old_x,
            old_y,
            next_x,
            next_y))
    {
        next_x = old_x;
        next_y = old_y;
    }

    if (scratch->grounded[player_index] != UINT8_C(0))
    {
        const uint8_t support = scratch->support[player_index];
        const int32_t surface_y =
            pf_m4_surface_y_q16(content, support, next_x);
        const int32_t standing_y =
            surface_y - fighter->half_height_q16;
        int32_t surface_left;
        int32_t surface_right;

        if (next_y > standing_y)
        {
            next_y = standing_y;
        }
        pf_m4_surface_bounds_q16(
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
    else if (next_y > old_y)
    {
        const int32_t old_bottom =
            old_y + fighter->half_height_q16;
        const int32_t next_bottom =
            next_y + fighter->half_height_q16;
        const int32_t platform_center =
            pf_m4_platform_center_x_q16(stage, world->tick);
        const int32_t platform_left =
            platform_center - stage->platform_half_width_q16;
        const int32_t platform_right =
            platform_center + stage->platform_half_width_q16;
        const int crosses_platform =
            next_x >= platform_left &&
            next_x <= platform_right &&
            old_bottom <= stage->platform_y_q16 &&
            next_bottom >= stage->platform_y_q16;
        const int crosses_floor =
            next_x >= stage->floor_left_q16 &&
            next_x <= stage->floor_right_q16 &&
            old_bottom <= stage->floor_y_q16 &&
            next_bottom >= stage->floor_y_q16;

        if (crosses_platform || crosses_floor)
        {
            next_y = old_y;
        }
    }

    scratch->position_x_q16[player_index] = next_x;
    scratch->position_y_q16[player_index] = next_y;
    return PF_STATUS_OK;
}

static int pf_m4_action_is_throw(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_THROW_FORWARD ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_BACK ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_UP ||
           action_state == (uint8_t)PF_M4_ACTION_THROW_DOWN;
}

static const pf_m4_throw_data *pf_m4_throw_for_action(
    const pf_m4_fighter_data *fighter,
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

static uint8_t pf_m4_grab_action_for_input(
    const pf_m4_fighter_data *fighter,
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

static int pf_m4_action_locks_ground_control(uint8_t action_state)
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
           pf_m4_action_is_throw(action_state) ||
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

static int pf_m4_action_is_reference_special_locked(uint8_t action_state)
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

static int pf_m4_action_is_falcon_kick(uint8_t action_state)
{
    return action_state >=
               (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND &&
           action_state <=
               (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND;
}

static int pf_m4_action_is_shield_break(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN ||
           action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN;
}

static int pf_m4_action_is_wall_tech(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_WALL_TECH ||
           action_state == (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP;
}

static int pf_m4_action_is_surface_tech(uint8_t action_state)
{
    return pf_m4_action_is_wall_tech(action_state) ||
           action_state == (uint8_t)PF_M4_ACTION_CEILING_TECH;
}

static int pf_m4_action_is_surface_bounce(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_WALL_BOUNCE ||
           action_state == (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
}

static int pf_m4_action_is_shield(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
           pf_m4_action_is_shield_break(action_state);
}

static int pf_m4_action_is_ground_attack(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_UP_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
           pf_m4_action_is_reference_angled_normal(action_state) ||
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

static int pf_m4_action_is_forward_ground_attack(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
           pf_m4_action_is_reference_angled_normal(action_state) ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_LOW;
}

static int pf_m4_action_allows_fresh_fast_fall(
    uint8_t action_state,
    uint16_t action_ticks)
{
    const pf_m4_falcon_neutral_special_timing *timing;

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
        !pf_m4_action_is_falcon_kick(action_state))
    {
        return 1;
    }
    if (action_state !=
        (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR)
    {
        return 0;
    }
    timing = pf_m4_falcon_reference_neutral_special_timing();
    return timing != NULL &&
           action_ticks >= timing->ordinary_air_physics_begin_frame;
}

static int pf_m4_action_is_smash_charge(uint8_t action_state)
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

static uint8_t pf_m4_smash_release_action(uint8_t charge_action)
{
    switch ((pf_m4_action_state)charge_action)
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

static uint8_t pf_m4_smash_charge_action_for_release(uint8_t release_action)
{
    switch ((pf_m4_action_state)release_action)
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

static const pf_m4_attack_data *pf_m4_directional_ground_data(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

static pf_m4_reference_timing pf_m4_ground_attack_timing(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    pf_m4_reference_timing timing = {0};
    const pf_m4_attack_data *attack =
        pf_m4_directional_ground_data(fighter, action_state);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        (pf_m4_action_is_reference_jab_extension(action_state) ||
         pf_m4_action_is_reference_angled_normal(action_state)))
    {
        pf_m4_falcon_move_index move_index;

        if (pf_m4_falcon_reference_move_for_action(
                action_state,
                &move_index))
        {
            return pf_m4_falcon_reference_timing(move_index);
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
    switch ((pf_m4_action_state)action_state)
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

static uint32_t pf_m4_ground_attack_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    const pf_m4_attack_data *attack =
        pf_m4_directional_ground_data(fighter, action_state);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        pf_m4_action_is_reference_jab_extension(action_state))
    {
        pf_m4_falcon_move_index move_index;
        const pf_m4_reference_hit_effect *effect;

        if (!pf_m4_falcon_reference_move_for_action(
                action_state,
                &move_index) ||
            (effect = pf_m4_falcon_reference_primary_effect(move_index)) ==
                NULL)
        {
            return UINT32_C(0);
        }
        return (uint32_t)effect->damage * UINT32_C(65536);
    }
    if (attack != NULL)
    {
        return attack->damage_q16;
    }
    switch ((pf_m4_action_state)action_state)
    {
        case PF_M4_ACTION_DASH_ATTACK:
            return fighter->dash_attack_damage_q16;
        case PF_M4_ACTION_JAB_FINAL:
            return fighter->jab_final_damage_q16;
        case PF_M4_ACTION_STRONG_ATTACK:
            return fighter->strong_damage_q16;
        case PF_M4_ACTION_GROUND_ATTACK:
            return fighter->jab_damage_q16;
        default:
            return UINT32_C(0);
    }
}

static const pf_m4_reference_move *pf_m4_falcon_ground_reference_attack(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    const pf_m4_reference_timing authored =
        pf_m4_ground_attack_timing(fighter, action_state);

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        pf_m4_action_is_reference_angled_normal(action_state))
    {
        pf_m4_falcon_move_index move_index;

        return pf_m4_falcon_reference_move_for_action(
                   action_state,
                   &move_index)
                   ? pf_m4_falcon_reference_move(move_index)
                   : NULL;
    }

    return pf_m4_falcon_reference_attack(
        action_state,
        authored,
        pf_m4_ground_attack_damage_q16(fighter, action_state));
}

static int pf_m4_falcon_ground_reference_matches(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    return pf_m4_falcon_ground_reference_attack(
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

static uint8_t pf_m4_falcon_ground_iasa_capabilities(
    pf_m4_reference_iasa_policy policy)
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

static int8_t pf_m4_source_forward_angle_band(
    int16_t stick_x,
    int16_t stick_y,
    int32_t outer_angle_tan_q16,
    int32_t inner_angle_tan_q16)
{
    const int32_t source_y = -(int32_t)stick_y;
    const uint16_t horizontal_magnitude = pf_m4_axis_magnitude(stick_x);
    const int64_t angle_numerator =
        (int64_t)(source_y < INT32_C(0) ? -source_y : source_y) *
        INT64_C(65536);

    if (angle_numerator >
        (int64_t)horizontal_magnitude * outer_angle_tan_q16)
    {
        return source_y > INT32_C(0) ? INT8_C(2) : INT8_C(-2);
    }
    if (angle_numerator >
        (int64_t)horizontal_magnitude * inner_angle_tan_q16)
    {
        return source_y > INT32_C(0) ? INT8_C(1) : INT8_C(-1);
    }
    return INT8_C(0);
}

static uint8_t pf_m4_select_ground_light_attack_action(
    const pf_m4_fighter_data *fighter,
    const pf_m4_ssbm_ground_input_attributes *source_ground_input,
    int8_t facing,
    int16_t stick_x,
    int16_t stick_y)
{
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(stick_x);
    if (source_ground_input != NULL)
    {
        const int32_t source_y = -(int32_t)stick_y;
        const int64_t angle_numerator =
            (int64_t)(source_y < INT32_C(0) ? -source_y : source_y) *
            INT64_C(65536);
        const int64_t direction_boundary =
            (int64_t)horizontal_magnitude *
            source_ground_input->tilt_direction_angle_tan_q16;

        /* Wait_IASA checks S3 before Hi3/Lw3 before Attack11. Match those
         * independent source predicates rather than choosing the dominant
         * Cartesian axis. */
        if (horizontal_magnitude >=
                source_ground_input->forward_tilt_axis_threshold &&
            pf_m4_axis_direction(
                stick_x,
                source_ground_input->forward_tilt_axis_threshold) == facing &&
            angle_numerator < direction_boundary)
        {
            const int8_t angle_band = pf_m4_source_forward_angle_band(
                stick_x,
                stick_y,
                source_ground_input->forward_tilt_outer_angle_tan_q16,
                source_ground_input->forward_tilt_inner_angle_tan_q16);

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

    if (pf_m4_axis_magnitude(stick_y) >= fighter->axis_dead_zone &&
        pf_m4_axis_magnitude(stick_y) > horizontal_magnitude)
    {
        return stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_ATTACK
                   : (uint8_t)PF_M4_ACTION_DOWN_ATTACK;
    }
    if (horizontal_magnitude >= fighter->axis_dead_zone &&
        horizontal_magnitude >= pf_m4_axis_magnitude(stick_y) &&
        pf_m4_axis_direction(stick_x, fighter->axis_dead_zone) == facing)
    {
        return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK;
    }
    return (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
}

static uint8_t pf_m4_select_ground_strong_attack_action(
    const pf_m4_fighter_data *fighter,
    const pf_m4_ssbm_ground_input_attributes *source_ground_input,
    int16_t stick_x,
    int16_t stick_y)
{
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(stick_x);
    const uint16_t vertical_magnitude =
        pf_m4_axis_magnitude(stick_y);

    if (source_ground_input != NULL)
    {
        if (horizontal_magnitude >=
            source_ground_input->c_stick_horizontal_smash_threshold)
        {
            const int8_t angle_band = pf_m4_source_forward_angle_band(
                stick_x,
                stick_y,
                source_ground_input->forward_smash_outer_angle_tan_q16,
                source_ground_input->forward_smash_inner_angle_tan_q16);

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

static uint8_t pf_m4_select_ground_strong_input_action(
    const pf_m4_fighter_data *fighter,
    const pf_m4_ssbm_ground_input_attributes *source_ground_input,
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
    return pf_m4_select_ground_strong_attack_action(
        fighter,
        source_ground_input,
        stick_x,
        stick_y);
}

static int pf_m4_action_is_light_aerial(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_FORWARD_AERIAL ||
           action_state == (uint8_t)PF_M4_ACTION_BACK_AERIAL ||
           action_state == (uint8_t)PF_M4_ACTION_UP_AERIAL ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_AERIAL;
}

static const pf_m4_attack_data *pf_m4_directional_aerial_data(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

static uint32_t pf_m4_light_aerial_ticks(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    const pf_m4_attack_data *attack =
        pf_m4_directional_aerial_data(fighter, action_state);

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

static uint16_t pf_m4_aerial_landing_lag_for_action(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

static int pf_m4_falcon_aerial_reference_matches(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;
    const pf_m4_reference_hit_effect *effect;
    const pf_m4_attack_data *attack =
        pf_m4_directional_aerial_data(fighter, action_state);
    const uint32_t damage_q16 =
        attack != NULL ? attack->damage_q16 : fighter->aerial_damage_q16;

    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = pf_m4_falcon_reference_move(move_index);
    effect = pf_m4_falcon_reference_primary_effect(move_index);
    return move != NULL && effect != NULL &&
           move->landing_lag != UINT16_C(0) &&
           pf_m4_light_aerial_ticks(fighter, action_state) ==
               (uint32_t)move->total_frames &&
           damage_q16 ==
               (uint32_t)effect->damage * UINT32_C(65536) &&
           pf_m4_aerial_landing_lag_for_action(
               fighter,
               action_state) == move->landing_lag;
}

static int pf_m4_light_aerial_landing_lag_active(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_frame)
{
    const int reference_lag_active =
        pf_m4_falcon_aerial_reference_matches(fighter, action_state)
            ? pf_m4_falcon_reference_landing_lag_active(
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

static uint8_t pf_m4_select_light_aerial_action(
    const pf_m4_fighter_data *fighter,
    int16_t stick_x,
    int16_t stick_y,
    int8_t facing)
{
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(stick_x);
    const uint16_t vertical_magnitude =
        pf_m4_axis_magnitude(stick_y);

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const pf_m4_ssbm_ground_input_attributes *ground_input =
            pf_m4_ssbm_common_reference_ground_input();

        if (ground_input != NULL)
        {
            const int32_t source_y = -(int32_t)stick_y;
            const int64_t direction_height =
                (int64_t)(source_y < INT32_C(0) ? -source_y : source_y) *
                (int64_t)PF_Q16_ONE;
            const int64_t direction_width =
                (int64_t)horizontal_magnitude *
                (int64_t)ground_input->aerial_direction_angle_tan_q16;

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

static uint8_t pf_m4_select_aerial_attack_action(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input,
    int8_t facing,
    int strong_attack_pressed)
{
    if (strong_attack_pressed != 0)
    {
        const pf_m4_ssbm_ground_input_attributes *ground_input =
            fighter->reference_frame_data_enabled != UINT8_C(0)
                ? pf_m4_ssbm_common_reference_ground_input()
                : NULL;
        const int secondary_stick_active =
            pf_m4_axis_magnitude(input->secondary_stick_x) >=
                (ground_input != NULL
                     ? ground_input->aerial_neutral_x_threshold
                     : fighter->axis_dead_zone) ||
            pf_m4_axis_magnitude(input->secondary_stick_y) >=
                (ground_input != NULL
                     ? ground_input->aerial_neutral_y_threshold
                     : fighter->axis_dead_zone);
        const uint8_t c_stick_action =
            pf_m4_select_light_aerial_action(
                fighter,
                input->secondary_stick_x,
                input->secondary_stick_y,
                facing);

        /* Melee's C-stick is an alternate directional input for the same
         * aerial scripts. Retain the authored strong-aerial extension for
         * custom content that does not exactly match the Falcon catalog. */
        if (secondary_stick_active != 0 &&
            pf_m4_falcon_aerial_reference_matches(
                fighter,
                c_stick_action))
        {
            return c_stick_action;
        }
        return (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK;
    }
    return pf_m4_select_light_aerial_action(
        fighter,
        input->main_stick_x,
        input->main_stick_y,
        facing);
}

static uint8_t pf_m4_reference_c_stick_attack_action(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input,
    int16_t previous_x,
    int16_t previous_y,
    int grounded)
{
    const pf_m4_ssbm_ground_input_attributes *ground_input;
    const uint16_t current_x =
        pf_m4_axis_magnitude(input->secondary_stick_x);
    const uint16_t previous_x_magnitude =
        pf_m4_axis_magnitude(previous_x);

    if (fighter->reference_frame_data_enabled == UINT8_C(0))
    {
        return UINT8_MAX;
    }
    ground_input = pf_m4_ssbm_common_reference_ground_input();
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
                       (pf_m4_axis_magnitude(previous_y) <
                            ground_input->aerial_neutral_y_threshold &&
                        pf_m4_axis_magnitude(input->secondary_stick_y) >=
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

static int pf_m4_action_can_enter_teeter(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_RUN_BRAKE ||
           action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
           action_state == (uint8_t)PF_M4_ACTION_TAUNT;
}

static int pf_m4_action_is_aerial_landing(uint8_t action_state)
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

static int pf_m4_action_is_grounded_landing(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_LANDING ||
           action_state == (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING ||
           pf_m4_action_is_aerial_landing(action_state);
}

static int pf_m4_action_is_l_cancel_landing(uint8_t action_state)
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

static uint8_t pf_m4_aerial_landing_action(
    uint8_t aerial_action,
    int l_cancelled)
{
    switch ((pf_m4_action_state)aerial_action)
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

static uint16_t pf_m4_aerial_landing_ticks(
    const pf_m4_fighter_data *fighter,
    uint8_t landing_action)
{
    pf_m4_falcon_move_index move_index = PF_M4_FALCON_MOVE_COUNT;
    uint8_t aerial_action = UINT8_MAX;
    uint16_t authored_ticks;
    int l_cancelled = pf_m4_action_is_l_cancel_landing(landing_action);

    switch ((pf_m4_action_state)landing_action)
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
        pf_m4_falcon_aerial_reference_matches(fighter, aerial_action) &&
        pf_m4_falcon_reference_move_for_action(
            aerial_action,
            &move_index))
    {
        const pf_m4_reference_move *move =
            pf_m4_falcon_reference_move(move_index);

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

static int pf_m4_action_is_recovery_invulnerable(
    const pf_m4_fighter_data *fighter,
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
    if (pf_m4_action_is_wall_tech(action_state))
    {
        return action_ticks <
               fighter->wall_tech_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_CEILING_TECH)
    {
        return action_ticks < fighter->ceiling_tech_control_tick;
    }
    if (pf_m4_action_is_surface_bounce(action_state))
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
        const pf_m4_getup_roll_timing *timing =
            pf_m4_getup_roll_timing_for(
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
               pf_m4_getup_attack_invulnerability_ticks_for(
                   fighter,
                   prone_orientation);
}

static uint8_t pf_m4_ledge_from_state(
    uint8_t action_state,
    uint8_t hitlag_resume_action,
    int8_t facing)
{
    if (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
        pf_m4_action_uses_ledge(hitlag_resume_action))
    {
        action_state = hitlag_resume_action;
    }
    if (!pf_m4_action_uses_ledge(action_state))
    {
        return (uint8_t)PF_M4_LEDGE_NONE;
    }
    return facing == INT8_C(1)
               ? (uint8_t)PF_M4_LEDGE_LEFT
               : (uint8_t)PF_M4_LEDGE_RIGHT;
}

static int8_t pf_m4_ledge_inward_direction(uint8_t ledge)
{
    return ledge == (uint8_t)PF_M4_LEDGE_LEFT
               ? INT8_C(1)
               : INT8_C(-1);
}

static int32_t pf_m4_ledge_x_q16(
    const pf_m4_stage_data *stage,
    uint8_t ledge)
{
    return ledge == (uint8_t)PF_M4_LEDGE_LEFT
               ? stage->floor_left_q16
               : stage->floor_right_q16;
}

static uint16_t pf_m4_ledge_transition_ticks(
    const pf_m4_fighter_data *fighter)
{
    return fighter->ledge_transition_ticks;
}

static void pf_m4_ledge_hang_position(
    const pf_m4_fighter_data *fighter,
    const pf_m4_stage_data *stage,
    uint8_t ledge,
    int32_t *out_x,
    int32_t *out_y)
{
    const int8_t inward = pf_m4_ledge_inward_direction(ledge);

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const pf_m4_falcon_ledge_root_positions *roots =
            pf_m4_falcon_reference_ledge_root_positions();

        if (roots != NULL)
        {
            *out_x =
                pf_m4_ledge_x_q16(stage, ledge) +
                (int32_t)inward * roots->wait_frame_one_x_q16;
            *out_y =
                stage->floor_y_q16 + roots->wait_frame_one_y_q16 -
                fighter->half_height_q16;
            return;
        }
    }

    *out_x =
        pf_m4_ledge_x_q16(stage, ledge) -
        (int32_t)inward * fighter->half_width_q16;
    *out_y =
        stage->floor_y_q16 + fighter->half_height_q16 / INT32_C(2);
}

static int pf_m4_reference_ledge_direction_option(
    int16_t stick_x,
    int16_t stick_y,
    int8_t facing,
    const pf_m4_ssbm_ledge_response_attributes *attributes)
{
    const uint32_t magnitude_x = pf_m4_axis_magnitude(stick_x);
    const uint32_t magnitude_y = pf_m4_axis_magnitude(stick_y);
    const int32_t source_y = -(int32_t)stick_y;
    const int64_t angle_left =
        (int64_t)(source_y < INT32_C(0) ? -source_y : source_y) *
        (int64_t)PF_Q16_ONE;
    const int64_t angle_right =
        (int64_t)magnitude_x *
        (int64_t)attributes->direction_angle_tan_q16;
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

static uint16_t pf_m4_reference_ledge_option_submotion(
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

static int pf_m4_enter_reference_ledge_option(
    const pf_m4_fighter_data *fighter,
    const pf_m4_stage_data *stage,
    uint8_t ledge,
    uint8_t next_action,
    int quick,
    int32_t *position_x,
    int32_t *position_y,
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t *source_submotion)
{
    int32_t anchor_x_q16;
    int32_t anchor_y_q16;
    const uint16_t submotion =
        pf_m4_reference_ledge_option_submotion(next_action, quick);

    if (!pf_m4_falcon_reference_ledge_option_anchor_q16(
            submotion,
            &anchor_x_q16,
            &anchor_y_q16))
    {
        return 0;
    }
    *position_x =
        pf_m4_ledge_x_q16(stage, ledge) +
        (int32_t)pf_m4_ledge_inward_direction(ledge) * anchor_x_q16;
    *position_y =
        stage->floor_y_q16 + anchor_y_q16 - fighter->half_height_q16;
    *action_state = next_action;
    *action_ticks = UINT16_C(0);
    *source_submotion = submotion;
    return 1;
}

static int pf_m4_ledge_catch_position(
    const pf_m4_fighter_data *fighter,
    const pf_m4_stage_data *stage,
    uint8_t ledge,
    int32_t *out_x,
    int32_t *out_y)
{
    const pf_m4_falcon_ledge_root_positions *roots =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? pf_m4_falcon_reference_ledge_root_positions()
            : NULL;

    if (roots == NULL)
    {
        return 0;
    }
    *out_x =
        pf_m4_ledge_x_q16(stage, ledge) +
        (int32_t)pf_m4_ledge_inward_direction(ledge) *
            roots->catch_frame_one_x_q16;
    *out_y =
        stage->floor_y_q16 + roots->catch_frame_one_y_q16 -
        fighter->half_height_q16;
    return 1;
}

static int pf_m4_ledge_occupied(
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
            pf_m4_ledge_from_state(
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
        if (pf_m4_ledge_from_state(
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

static int8_t pf_m4_ledge_probe_direction(
    uint8_t action_state,
    uint16_t action_ticks,
    int8_t facing)
{
    if (action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR)
    {
        const pf_m4_falcon_up_special_timing *timing =
            pf_m4_falcon_reference_up_special_timing();

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

static int pf_m4_try_grab_ledge(
    const pf_m4_content *content,
    const pf_world_state *world,
    const pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *position_x,
    int32_t *position_y,
    int32_t *velocity_x,
    int32_t *velocity_y,
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
    int32_t previous_position_x,
    int8_t ledge_probe_direction,
    int8_t *facing,
    int16_t main_stick_y,
    const pf_m4_ssbm_ledge_response_attributes *reference_ledge_response,
    int8_t *dash_direction)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    int64_t horizontal_reach =
        (int64_t)fighter->half_width_q16 +
        (int64_t)fighter->air_speed_q16;
    int32_t catch_top =
        stage->floor_y_q16 - fighter->half_height_q16;
    int32_t catch_bottom =
        stage->floor_y_q16 + fighter->half_height_q16;
    int32_t melee_bottom_extent_q16 = INT32_C(0);
    int32_t left_probe_position_x = *position_x;
    int32_t right_probe_position_x = *position_x;
    int use_melee_ledge_probe = 0;
    uint8_t ledge = (uint8_t)PF_M4_LEDGE_NONE;

    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        const pf_m4_falcon_ledge_attributes *ledge_attributes =
            pf_m4_falcon_reference_ledge_attributes();

        if (ledge_attributes != NULL)
        {
            horizontal_reach =
                (int64_t)ledge_attributes->snap_x_q16 +
                (int64_t)fighter->half_width_q16;
            catch_top =
                stage->floor_y_q16 + ledge_attributes->snap_y_q16 -
                ledge_attributes->snap_height_q16 / INT32_C(2) -
                fighter->half_height_q16;
            catch_bottom =
                stage->floor_y_q16 + ledge_attributes->snap_y_q16 +
                ledge_attributes->snap_height_q16 / INT32_C(2) -
                fighter->half_height_q16;
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
                int32_t source_frame_q16 = INT32_C(0);
                int32_t locked_bottom_y_q16 =
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16;
                pf_m4_falcon_ecb_pose_q16 dive_pose;

                if (pf_m4_falcon_reference_direct_hsd_pose(
                        action_state_before_catch,
                        action_ticks_before_catch,
                        UINT8_C(0),
                        &dive_submotion,
                        &source_frame_q16) &&
                    pf_m4_falcon_direct_hsd_locked_bottom_q16(
                        action_state_before_catch,
                        source_frame_q16,
                        UINT8_C(0),
                        &locked_bottom_y_q16) &&
                    pf_m4_falcon_reference_hsd_ecb_pose(
                        dive_submotion,
                        source_frame_q16,
                        0,
                        locked_bottom_y_q16,
                        &dive_pose))
                {
                    horizontal_reach =
                        (int64_t)ledge_attributes->snap_x_q16 +
                        (int64_t)dive_pose.right_x_from_origin_q16;
                    melee_bottom_extent_q16 =
                        dive_pose.bottom_y_from_origin_q16;
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
         (int64_t)*position_y - (int64_t)*velocity_y >
             (int64_t)catch_bottom) ||
        (use_melee_ledge_probe != 0 &&
         (int64_t)*position_y + (int64_t)fighter->half_height_q16 -
                 (int64_t)melee_bottom_extent_q16 <=
             (int64_t)stage->floor_y_q16))
    {
        return 0;
    }

    if (*position_x < stage->floor_left_q16 &&
        (ledge_probe_direction == (int8_t)PF_M4_LEDGE_PROBE_BOTH ||
         ledge_probe_direction == (int8_t)PF_M4_LEDGE_PROBE_LEFT) &&
        (int64_t)stage->floor_left_q16 -
                (int64_t)left_probe_position_x <=
            horizontal_reach)
    {
        ledge = (uint8_t)PF_M4_LEDGE_LEFT;
    }
    else if (*position_x > stage->floor_right_q16 &&
             (ledge_probe_direction ==
                  (int8_t)PF_M4_LEDGE_PROBE_BOTH ||
              ledge_probe_direction ==
                  (int8_t)PF_M4_LEDGE_PROBE_RIGHT) &&
             (int64_t)right_probe_position_x -
                     (int64_t)stage->floor_right_q16 <=
                 horizontal_reach)
    {
        ledge = (uint8_t)PF_M4_LEDGE_RIGHT;
    }

    if (ledge == (uint8_t)PF_M4_LEDGE_NONE ||
        pf_m4_ledge_occupied(
            world,
            scratch,
            player_index,
            ledge))
    {
        return 0;
    }

    if (!pf_m4_ledge_catch_position(
            fighter,
            stage,
            ledge,
            position_x,
            position_y))
    {
        pf_m4_ledge_hang_position(
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
    *facing = pf_m4_ledge_inward_direction(ledge);
    *ledge_invulnerability_ticks =
        fighter->ledge_invulnerability_ticks;
    *dash_direction = INT8_C(0);
    return 1;
}

void pf_m4_reset_player(
    pf_sim *sim,
    uint32_t player_index,
    int count_respawn)
{
    const pf_m4_fighter_data *fighter = &sim->content.fighter;
    const pf_m4_stage_data *stage = &sim->content.stage;
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
    const pf_m4_ssbm_stage_spawn_point *reference_spawn =
        pf_m4_ssbm_reference_stage_spawn_point(
            stage->reference_collision_profile,
            (uint8_t)player_index);
    const uint8_t spawn_support =
        reference_spawn != NULL
            ? reference_spawn->support
            : pf_m4_stage_spawn_support(stage);
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
                pf_m4_action_is_throw(
                    sim->world.action_state[other_index]))
            {
                sim->world.action_state[other_index] =
                    (uint8_t)PF_M4_ACTION_GRAB_RELEASE;
                sim->world.action_ticks[other_index] = UINT16_C(0);
                sim->world.source_submotion[other_index] =
                    (uint16_t)PF_M4_FALCON_SUBMOTION_CATCH_CUT;
                sim->world.source_animation_frame_q16[other_index] =
                    INT32_C(0);
                sim->world.source_animation_rate_q16[other_index] =
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
                sim->world.source_animation_frame_q16[other_index] =
                    INT32_C(0);
                sim->world.source_animation_rate_q16[other_index] =
                    INT32_C(0);
            }
        }
    }

    sim->world.previous_buttons[player_index] = UINT64_C(0);
    sim->world.position_x_q16[player_index] =
        reference_spawn != NULL
            ? reference_spawn->position_x_q16
            : centered_slot * stage->spawn_spacing_q16 +
                  (stage->reference_collision_profile !=
                           (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED
                       ? stage->reference_spawn_x_q16
                       : INT32_C(0));
    sim->world.position_y_q16[player_index] =
        pf_m4_surface_y_q16(
            &sim->content,
            spawn_support,
            sim->world.position_x_q16[player_index]) -
        fighter->half_height_q16;
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.match_kos[player_index] = UINT16_C(0);
    sim->world.match_falls[player_index] = UINT16_C(0);
    sim->world.shield_recoil_x_q16[player_index] = INT32_C(0);
    sim->world.shield_recoil_mask =
        (uint8_t)(
            sim->world.shield_recoil_mask &
            (uint8_t)~(UINT8_C(1) << player_index));
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.source_submotion[player_index] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
    sim->world.source_animation_frame_q16[player_index] = INT32_C(0);
    sim->world.source_animation_rate_q16[player_index] = INT32_C(0);
    sim->world.fall_animation_blend_q16[player_index] = INT32_C(0);
    sim->world.fall_animation_target_switched[player_index] = UINT8_C(0);
    sim->world.ecb_bottom_lock_ticks[player_index] = UINT8_C(0);
    sim->world.ecb_locked_bottom_y_q16[player_index] = INT32_C(0);
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
    sim->world.tilt_x_age[player_index] = UINT8_C(254);
    sim->world.tilt_y_age[player_index] = UINT8_C(254);
    sim->world.horizontal_input_age[player_index] = UINT8_C(254);
    sim->world.horizontal_input_direction[player_index] = INT8_C(0);
    sim->world.damage_q16[player_index] = UINT32_C(0);
    sim->world.knockback_velocity_x_q16[player_index] = INT32_C(0);
    sim->world.knockback_velocity_y_q16[player_index] = INT32_C(0);
    sim->world.ground_knockback_velocity_q16[player_index] = INT32_C(0);
    sim->world.last_hit_sequence[player_index] = UINT32_C(0);
    sim->world.last_hit_tick[player_index] = UINT64_C(0);
    sim->world.last_hit_damage_q16[player_index] = UINT32_C(0);
    sim->world.hitlag_ticks[player_index] = UINT16_C(0);
    sim->world.hitstun_ticks[player_index] = UINT16_C(0);
    sim->world.tech_window_ticks[player_index] = UINT16_C(0);
    sim->world.tech_lockout_ticks[player_index] = UINT16_C(0);
    sim->world.shield_stun_ticks[player_index] = UINT16_C(0);
    sim->world.shield_health_q16[player_index] =
        fighter->shield_health_q16;
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
    sim->world.powershield[player_index] = UINT8_C(0);
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

static void pf_m4_land(
    const pf_m4_fighter_data *fighter,
    int32_t surface_y_q16,
    uint8_t surface,
    int32_t *position_y,
    int32_t *velocity_y,
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
    *position_y = surface_y_q16 - fighter->half_height_q16;
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(1);
    *action_state = (uint8_t)PF_M4_ACTION_LANDING;
    *support = surface;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
}

static void pf_m4_enter_shield_break_launch(
    const pf_m4_fighter_data *fighter,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *velocity_x,
    int32_t *velocity_y,
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
    *velocity_y = -fighter->shield_break_launch_speed_q16;
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

static void pf_m4_transfer_air_knockback_to_flat_ground(
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    const pf_m4_ssbm_damage_response_attributes *common =
        pf_m4_ssbm_common_reference_damage_response();
    int32_t scalar =
        scratch->knockback_velocity_x_q16[player_index];

    if (common == NULL)
    {
        scalar = INT32_C(0);
    }
    else if (scalar > common->ground_knockback_max_speed_q16)
    {
        scalar = common->ground_knockback_max_speed_q16;
    }
    else if (scalar < -common->ground_knockback_max_speed_q16)
    {
        scalar = -common->ground_knockback_max_speed_q16;
    }
    scratch->ground_knockback_velocity_q16[player_index] = scalar;
    scratch->knockback_velocity_x_q16[player_index] = scalar;
    scratch->knockback_velocity_y_q16[player_index] = INT32_C(0);
}

static void pf_m4_land_from_air(
    const pf_m4_content *content,
    int32_t surface_y_q16,
    uint8_t surface,
    int16_t horizontal_input,
    int8_t facing,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *position_y,
    int32_t *velocity_x,
    int32_t *velocity_y,
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
    const pf_m4_fighter_data *fighter = &content->fighter;
    const int8_t roll_direction =
        pf_m4_axis_direction(
            horizontal_input,
            fighter->tech_roll_axis_threshold);
    const int32_t incoming_velocity_x = pf_m4_total_velocity_q16(
        *velocity_x,
        scratch->knockback_velocity_x_q16[player_index]);

    scratch->prone_orientation[player_index] =
        (uint8_t)PF_M4_PRONE_NONE;

    if (*action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        *position_y = surface_y_q16 - fighter->half_height_q16;
        *velocity_x = INT32_C(0);
        /* ftCommon_8007D7FC transfers the horizontal component to gr_vel but
         * leaves self_vel.y observable on ShieldBreakDown frame 1. The first
         * grounded physics callback clears it on the following frame. */
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state =
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN;
        *source_submotion =
            pf_m4_falcon_reference_shield_break_down_submotion();
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
        *position_y = surface_y_q16 - fighter->half_height_q16;
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
        *position_y = surface_y_q16 - fighter->half_height_q16;
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
        *position_y = surface_y_q16 - fighter->half_height_q16;
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
        *position_y = surface_y_q16 - fighter->half_height_q16;
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

        *position_y = surface_y_q16 - fighter->half_height_q16;
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
             pf_m4_falcon_reference_up_special_timing()
                 ->air_control_begin_frame))
    {
        const int preserve_fall_special_velocity =
            *action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL;

        *position_y = surface_y_q16 - fighter->half_height_q16;
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
        *position_y = surface_y_q16 - fighter->half_height_q16;
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
        *position_y = surface_y_q16 - fighter->half_height_q16;
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

    if (pf_m4_action_is_light_aerial(*action_state) ||
        *action_state ==
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK)
    {
        const uint8_t aerial_action = *action_state;
        const int strong_aerial =
            *action_state ==
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK;
        const int landing_lag_active =
            strong_aerial != 0 ||
            pf_m4_light_aerial_landing_lag_active(
                fighter,
                aerial_action,
                *action_ticks);

        pf_m4_land(
            fighter,
            surface_y_q16,
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
                *action_state = pf_m4_aerial_landing_action(
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
        pf_m4_land(
            fighter,
            surface_y_q16,
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
        pf_m4_transfer_air_knockback_to_flat_ground(
            scratch,
            player_index);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        pf_m4_land(
            fighter,
            surface_y_q16,
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

    *position_y = surface_y_q16 - fighter->half_height_q16;
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(1);
    *support = surface;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
    scratch->tumble[player_index] = UINT8_C(0);
    /* The collision callback exposes the incoming air channels on its entry
     * frame. x8c is projected onto the selected floor immediately; xF0
     * initialization and 0.08 traction decay begin on the following tick. */
    pf_m4_project_ground_scalar_q16(
        content,
        surface,
        scratch->knockback_velocity_x_q16[player_index],
        &scratch->knockback_velocity_x_q16[player_index],
        &scratch->knockback_velocity_y_q16[player_index]);
    scratch->ground_knockback_velocity_q16[player_index] = INT32_C(0);

    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        if (roll_direction == INT8_C(0))
        {
            *velocity_x = INT32_C(0);
            *action_state =
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE;
            scratch->tech_direction[player_index] = INT8_C(0);
        }
        else
        {
            *velocity_x = INT32_C(0);
            *action_state = (uint8_t)PF_M4_ACTION_TECH_ROLL;
            scratch->tech_direction[player_index] = roll_direction;
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
    }
}

static int pf_m4_action_can_start_grab(uint8_t action_state)
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
           pf_m4_action_is_ground_damage(action_state);
}

static int pf_m4_action_can_start_dash_attack(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    return action_state == (uint8_t)PF_M4_ACTION_RUN ||
           (action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
            action_ticks >= fighter->forward_smash_input_window_ticks);
}

static int pf_m4_action_can_start_taunt(uint8_t action_state)
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
           pf_m4_action_is_ground_damage(action_state);
}

static int pf_m4_normal_landing_is_interruptible(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    return action_state != (uint8_t)PF_M4_ACTION_LANDING ||
           (uint32_t)action_ticks + UINT32_C(1) >=
               (uint32_t)fighter->landing_interruptible_tick;
}

static int pf_m4_drop_cancel_hitlag_is_eligible(
    const pf_m4_fighter_data *fighter,
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

static pf_status pf_m4_enter_wall_impact(
    const pf_m4_fighter_data *fighter,
    int wall_tech_jump,
    int8_t away_direction,
    int32_t source_normal_x_q16,
    int32_t source_normal_y_q16,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *velocity_x,
    int32_t *velocity_y,
    uint16_t *action_ticks,
    uint8_t *action_state,
    uint8_t *fast_fall,
    int8_t *facing)
{
    const int32_t total_velocity_x = pf_m4_total_velocity_q16(
        *velocity_x,
        scratch->knockback_velocity_x_q16[player_index]);
    const int32_t total_velocity_y = pf_m4_total_velocity_q16(
        *velocity_y,
        scratch->knockback_velocity_y_q16[player_index]);
    *action_ticks = UINT16_C(0);
    *fast_fall = UINT8_C(0);
    *facing = away_direction;
    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_q16[player_index] = INT32_C(0);
        scratch->knockback_velocity_y_q16[player_index] = INT32_C(0);
        scratch->ground_knockback_velocity_q16[player_index] = INT32_C(0);
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
        int32_t reflected_velocity_x = total_velocity_x;
        int32_t reflected_velocity_y = total_velocity_y;
        const pf_status status = pf_m4_ssbm_mirror_velocity_q16(
            source_normal_x_q16,
            source_normal_y_q16,
            fighter->surface_bounce_multiplier_q16,
            &reflected_velocity_x,
            &reflected_velocity_y);

        if (status != PF_STATUS_OK)
        {
            return status;
        }
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_q16[player_index] =
            reflected_velocity_x;
        scratch->knockback_velocity_y_q16[player_index] =
            reflected_velocity_y;
        *action_state = (uint8_t)PF_M4_ACTION_WALL_BOUNCE;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    return PF_STATUS_OK;
}

static pf_status pf_m4_enter_ceiling_impact(
    const pf_m4_fighter_data *fighter,
    int32_t source_normal_x_q16,
    int32_t source_normal_y_q16,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *velocity_x,
    int32_t *velocity_y,
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
        scratch->knockback_velocity_x_q16[player_index] = INT32_C(0);
        scratch->knockback_velocity_y_q16[player_index] = INT32_C(0);
        scratch->ground_knockback_velocity_q16[player_index] = INT32_C(0);
        *action_state = (uint8_t)PF_M4_ACTION_CEILING_TECH;
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    else
    {
        int32_t total_velocity_x = pf_m4_total_velocity_q16(
            *velocity_x,
            scratch->knockback_velocity_x_q16[player_index]);
        int32_t total_velocity_y = pf_m4_total_velocity_q16(
            *velocity_y,
            scratch->knockback_velocity_y_q16[player_index]);
        const pf_status status = pf_m4_ssbm_mirror_velocity_q16(
            source_normal_x_q16,
            source_normal_y_q16,
            fighter->surface_bounce_multiplier_q16,
            &total_velocity_x,
            &total_velocity_y);

        if (status != PF_STATUS_OK)
        {
            return status;
        }

        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        scratch->knockback_velocity_x_q16[player_index] =
            total_velocity_x;
        scratch->knockback_velocity_y_q16[player_index] =
            total_velocity_y;
        *action_state = (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    return PF_STATUS_OK;
}

static void pf_m4_write_scratch(
    pf_sim_scratch *scratch,
    uint32_t player_index,
    const pf_input_frame *input,
    int32_t position_x,
    int32_t position_y,
    int32_t velocity_x,
    int32_t velocity_y,
    uint16_t action_ticks,
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int32_t source_animation_rate_q16,
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
    scratch->position_x_q16[player_index] = position_x;
    scratch->position_y_q16[player_index] = position_y;
    scratch->velocity_x_q16[player_index] = velocity_x;
    scratch->velocity_y_q16[player_index] = velocity_y;
    scratch->action_ticks[player_index] = action_ticks;
    scratch->source_submotion[player_index] = source_submotion;
    scratch->source_animation_frame_q16[player_index] =
        source_animation_frame_q16;
    scratch->source_animation_rate_q16[player_index] =
        source_animation_rate_q16;
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

static void pf_m4_copy_combat_scratch(
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
    scratch->damage_q16[player_index] =
        world->damage_q16[player_index];
    scratch->knockback_velocity_x_q16[player_index] =
        world->knockback_velocity_x_q16[player_index];
    scratch->knockback_velocity_y_q16[player_index] =
        world->knockback_velocity_y_q16[player_index];
    scratch->ground_knockback_velocity_q16[player_index] =
        world->ground_knockback_velocity_q16[player_index];
    scratch->last_hit_sequence[player_index] =
        world->last_hit_sequence[player_index];
    scratch->last_hit_tick[player_index] =
        world->last_hit_tick[player_index];
    scratch->last_hit_damage_q16[player_index] =
        world->last_hit_damage_q16[player_index];
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
    scratch->shield_health_q16[player_index] =
        world->shield_health_q16[player_index];
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
    scratch->powershield[player_index] =
        world->powershield[player_index];
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

static int32_t pf_m4_revival_platform_y(
    const pf_m4_stage_data *stage,
    uint16_t action_ticks)
{
    const uint16_t descent_ticks =
        stage->revival_platform_descent_ticks;
    const uint16_t elapsed =
        action_ticks < descent_ticks ? action_ticks : descent_ticks;
    const int64_t distance =
        (int64_t)stage->revival_platform_end_y_q16 -
        (int64_t)stage->revival_platform_start_y_q16;

    return stage->revival_platform_start_y_q16 +
           (int32_t)(
               distance * (int64_t)elapsed /
               (int64_t)descent_ticks);
}

static void pf_m4_prepare_spawn(
    const pf_m4_fighter_data *fighter,
    const pf_m4_stage_data *stage,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *position_x,
    int32_t *position_y,
    int32_t *velocity_x,
    int32_t *velocity_y,
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
    const int32_t centered_slot =
        (int32_t)(UINT32_C(2) * player_index + UINT32_C(1)) -
        (int32_t)world->player_count;

    *position_x = centered_slot * stage->spawn_spacing_q16;
    *position_y = stage->revival_platform_start_y_q16 -
                  fighter->half_height_q16;
    *velocity_x = INT32_C(0);
    *velocity_y = INT32_C(0);
    *action_ticks = UINT16_C(0);
    *source_submotion =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT
            : UINT16_C(0);
    *grounded = UINT8_C(1);
    *action_state = (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM;
    *support = (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *platform_drop_ticks = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *facing =
        centered_slot <= INT32_C(0) ? INT8_C(1) : INT8_C(-1);
    *dash_direction = INT8_C(0);
    *previous_strong_direction = INT8_C(0);
    *directional_input_flags = UINT8_C(0);
    scratch->damage_q16[player_index] = UINT32_C(0);
    scratch->knockback_velocity_x_q16[player_index] = INT32_C(0);
    scratch->knockback_velocity_y_q16[player_index] = INT32_C(0);
    scratch->ground_knockback_velocity_q16[player_index] = INT32_C(0);
    scratch->last_hit_sequence[player_index] = UINT32_C(0);
    scratch->last_hit_tick[player_index] = UINT64_C(0);
    scratch->last_hit_damage_q16[player_index] = UINT32_C(0);
    scratch->hitlag_ticks[player_index] = UINT16_C(0);
    scratch->hitstun_ticks[player_index] = UINT16_C(0);
    scratch->tech_window_ticks[player_index] = UINT16_C(0);
    scratch->tech_lockout_ticks[player_index] = UINT16_C(0);
    scratch->shield_stun_ticks[player_index] = UINT16_C(0);
    scratch->shield_recoil_x_q16[player_index] = INT32_C(0);
    scratch->shield_recoil_mask =
        (uint8_t)(
            scratch->shield_recoil_mask &
            (uint8_t)~(UINT8_C(1) << player_index));
    scratch->shield_health_q16[player_index] =
        fighter->shield_health_q16;
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
    scratch->powershield[player_index] = UINT8_C(0);
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

static void pf_m4_enter_wall_jump(
    const pf_m4_fighter_data *fighter,
    int8_t away_direction,
    int32_t *velocity_x,
    int32_t *velocity_y,
    uint16_t *action_ticks,
    uint8_t *action_state,
    uint8_t *fast_fall,
    int8_t *facing)
{
    *velocity_x =
        (int32_t)away_direction * fighter->wall_jump_speed_x_q16;
    *velocity_y = -fighter->wall_jump_speed_y_q16;
    *action_ticks = UINT16_C(0);
    *action_state = (uint8_t)PF_M4_ACTION_WALL_JUMP;
    *fast_fall = UINT8_C(0);
    *facing = away_direction;
}

static void pf_m4_enter_double_jump(
    const pf_m4_fighter_data *fighter,
    const pf_input_frame *input,
    int32_t *velocity_x,
    int32_t *velocity_y,
    uint8_t *air_jumps_remaining,
    uint8_t *fast_fall,
    uint8_t *action_state,
    uint16_t *action_ticks,
    uint16_t *source_submotion,
    int8_t facing)
{
    *velocity_x = pf_m4_scale_axis_q16(
        input->main_stick_x,
        fighter->double_jump_horizontal_speed_q16);
    /* Melee enters JumpAerial during IASA, then executes its ordinary aerial
     * physics callback on that same fighter update. Apply air control to the
     * newly assigned jump velocity here; applying it before entry is lost when
     * the entry callback replaces self_vel.x. */
    *velocity_x = pf_m4_apply_air_input(
        fighter,
        *velocity_x,
        input->main_stick_x,
        fighter->air_speed_q16);
    *velocity_y = -fighter->double_jump_speed_q16;
    --*air_jumps_remaining;
    *fast_fall = UINT8_C(0);
    *action_state = fighter->double_jump_cancel_ticks > UINT16_C(0)
                        ? (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP
                        : (uint8_t)PF_M4_ACTION_AIRBORNE;
    *action_ticks = UINT16_C(0);
    *source_submotion =
        pf_m4_falcon_jump_submotion(input, facing, 1);
}

static int pf_m4_action_can_start_vector_ascent(uint8_t action_state)
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
           pf_m4_action_is_damage(action_state);
}

static int pf_m4_reference_action_allows_special(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    int normal_landing_interruptible,
    uint8_t ground_iasa_capabilities,
    uint16_t initial_dash_special_end_frame)
{
    if (grounded == UINT8_C(0))
    {
        /* Fall/Jump IASA routes SpecialAir first. DamageFall uses that same
         * table once damage lockout has ended; EscapeAir, passive-wall states,
         * and FallSpecial use narrower callback tables. */
        return action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
               action_state ==
                   (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
               pf_m4_action_is_damage(action_state);
    }
    if (pf_m4_action_is_ground_attack(action_state))
    {
        return (ground_iasa_capabilities &
                PF_M4_FALCON_IASA_SPECIAL) != UINT8_C(0);
    }
    /* Squat has the full common IASA table; SquatWait and SquatRv do not.
     * Guard, GuardOff, RunBrake, TurnRun, and every recovery state likewise
     * have no SpecialS/Hi/N/Lw check. */
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           action_state == (uint8_t)PF_M4_ACTION_WALK ||
           (action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
            action_ticks <= initial_dash_special_end_frame) ||
           action_state == (uint8_t)PF_M4_ACTION_RUN ||
           action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN ||
           action_state == (uint8_t)PF_M4_ACTION_CROUCH_START ||
           pf_m4_action_is_ground_damage(action_state) ||
           (action_state == (uint8_t)PF_M4_ACTION_LANDING &&
            normal_landing_interruptible != 0);
}

typedef enum pf_m4_prone_option
{
    PF_M4_PRONE_OPTION_NONE = 0,
    PF_M4_PRONE_OPTION_ATTACK = 1,
    PF_M4_PRONE_OPTION_ROLL = 2,
    PF_M4_PRONE_OPTION_NEUTRAL = 3
} pf_m4_prone_option;

static int pf_m4_axis_is_in_prone_horizontal_wedge(
    int16_t axis_x,
    int16_t axis_y,
    uint16_t threshold,
    int32_t angle_tan_q16)
{
    const uint32_t magnitude_x = pf_m4_axis_magnitude(axis_x);
    const uint32_t magnitude_y = pf_m4_axis_magnitude(axis_y);

    return magnitude_x >= threshold &&
           (uint64_t)magnitude_y * UINT64_C(65536) <
               (uint64_t)magnitude_x * (uint32_t)angle_tan_q16;
}

static pf_m4_prone_option pf_m4_select_prone_option(
    const pf_m4_fighter_data *fighter,
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
        pf_m4_axis_is_in_prone_horizontal_wedge(
            input->secondary_stick_x,
            input->secondary_stick_y,
            fighter->down_horizontal_axis_threshold,
            fighter->down_horizontal_angle_tan_q16);
    const int c_roll_pressed =
        (previous_c_roll_direction == INT8_C(0) && current_c_roll != 0) ||
        (previous_c_roll_direction != INT8_C(0) && current_c_roll == 0);
    const int main_roll_held =
        pf_m4_axis_is_in_prone_horizontal_wedge(
            input->main_stick_x,
            input->main_stick_y,
            fighter->down_horizontal_axis_threshold,
            fighter->down_horizontal_angle_tan_q16);
    const uint32_t main_up_magnitude =
        input->main_stick_y < INT16_C(0)
            ? (uint32_t)(-(int32_t)input->main_stick_y)
            : UINT32_C(0);
    const int main_up_held =
        main_up_magnitude >= fighter->down_up_axis_threshold &&
        (uint64_t)main_up_magnitude * UINT64_C(65536) >=
            (uint64_t)pf_m4_axis_magnitude(input->main_stick_x) *
                (uint32_t)fighter->down_horizontal_angle_tan_q16;

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

static pf_status pf_m4_enter_prone_option(
    pf_m4_prone_option option,
    int8_t roll_direction,
    uint8_t prone_orientation,
    int from_knockdown,
    int8_t facing,
    int32_t *velocity_x,
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
            pf_m4_getup_roll_submotion_for(
                motion_orientation,
                roll_direction,
                facing);
        int32_t translation_x_q16;

        if (submotion_index == UINT16_MAX ||
            !pf_m4_falcon_reference_translation_q16(
                submotion_index,
                UINT16_C(1),
                &translation_x_q16,
                NULL))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        *action_state = (uint8_t)PF_M4_ACTION_GETUP_ROLL;
        *velocity_x = (int32_t)facing * translation_x_q16;
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

pf_status pf_m4_step_player(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    const pf_input_frame *raw_input,
    uint32_t player_index,
    int32_t player_nudge_x_q16,
    uint64_t *rng_state)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const pf_m4_falcon_common_attributes *source_character =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? pf_m4_falcon_reference_common_attributes()
            : NULL;
    const uint8_t previous_action_state =
        world->action_state[player_index];
    const uint8_t previous_hitlag_resume_action =
        world->hitlag_resume_action[player_index];
    const uint16_t previous_source_submotion =
        world->source_submotion[player_index];
    const int32_t previous_source_animation_frame_q16 =
        world->source_animation_frame_q16[player_index];
    const int32_t previous_source_animation_rate_q16 =
        world->source_animation_rate_q16[player_index];
    const int32_t previous_fall_animation_blend_q16 =
        world->fall_animation_blend_q16[player_index];
    const uint8_t previous_fall_animation_target_switched =
        world->fall_animation_target_switched[player_index];
    const int32_t previous_ground_velocity_q16 =
        world->velocity_x_q16[player_index];
    const int8_t previous_facing = world->facing[player_index];

    if (rng_state == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (!pf_m4_ssbm_stage_support_valid(
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
    int8_t input_tilt_x_direction;
    const uint8_t input_tilt_x_age = pf_m4_tilt_age(
        input->main_stick_x,
        fighter->tilt_axis_threshold,
        world->previous_tilt_x_direction[player_index],
        world->tilt_x_age[player_index],
        &input_tilt_x_direction);
    int8_t input_tilt_y_direction;
    const uint8_t input_tilt_y_age = pf_m4_tilt_age(
        input->main_stick_y,
        fighter->tilt_axis_threshold,
        world->previous_tilt_y_direction[player_index],
        world->tilt_y_age[player_index],
        &input_tilt_y_direction);
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
    const uint8_t reference_c_stick_attack_action =
        pf_m4_reference_c_stick_attack_action(
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
        reference_c_stick_attack_action != UINT8_MAX;
    const int special_pressed =
        (input->buttons & PF_INPUT_BUTTON_SPECIAL) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_SPECIAL) == UINT64_C(0);
    const int taunt_pressed =
        (input->buttons & PF_INPUT_BUTTON_TAUNT) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_TAUNT) == UINT64_C(0);
    const uint16_t input_shield_strength =
        pf_m4_input_shield_strength(fighter, input);
    const uint8_t input_trigger_state =
        pf_m4_input_trigger_state(fighter, input);
    const uint8_t previous_trigger_state =
        world->shield_held[player_index];
    const int shield_held =
        (input_trigger_state & PF_M4_TRIGGER_STATE_HELD_MASK) !=
        UINT8_C(0);
    const int dense_shield_pressed =
        (input_trigger_state & PF_M4_TRIGGER_STATE_DENSE_MASK &
         (uint8_t)~previous_trigger_state) != UINT8_C(0);
    const int shield_pressed =
        ((input_trigger_state & PF_M4_TRIGGER_STATE_HELD_MASK &
          (uint8_t)~previous_trigger_state) != UINT8_C(0)) ||
        dense_shield_pressed != 0;
    const int grab_pressed =
        shield_held != 0 && light_attack_pressed != 0;
    const int powershield_release_cancel_ready =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        scratch->powershield[player_index] != UINT8_C(0) &&
        fighter->powershield_cancel_enabled != UINT8_C(0) &&
        world->action_ticks[player_index] >=
            fighter->powershield_cancel_delay_ticks;
    const int grab_blocks_attack =
        grab_pressed != 0 &&
        (pf_m4_action_can_start_grab(
             world->action_state[player_index]) ||
         powershield_release_cancel_ready != 0);
    const int grab_fallback_attack_pressed =
        grab_pressed != 0 && grab_blocks_attack == 0;
    const int boost_grab_pressed =
        world->grounded[player_index] != UINT8_C(0) &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_DASH_ATTACK &&
        world->action_ticks[player_index] >=
            fighter->boost_grab_cancel_begin_tick &&
        world->action_ticks[player_index] <=
            fighter->boost_grab_cancel_end_tick &&
        shield_held != 0 &&
        (light_attack_pressed != 0 ||
         (light_attack_held != 0 && shield_pressed != 0));
    const int jab_combo_window = source_character == NULL &&
        world->grounded[player_index] != UINT8_C(0) &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
        world->action_ticks[player_index] >=
            fighter->jab_combo_input_begin_tick &&
        world->action_ticks[player_index] <=
            fighter->jab_combo_input_end_tick;
    const int jab_cancel_pressed =
        jab_combo_window != 0 && shield_pressed != 0;
    const int jab_final_pressed =
        jab_combo_window != 0 && shield_held == 0 &&
        light_attack_pressed != 0;
    const int was_shielding =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD ||
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_STUN;
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(input->main_stick_x);
    const uint16_t vertical_magnitude =
        pf_m4_axis_magnitude(input->main_stick_y);
    const int8_t horizontal_direction =
        pf_m4_axis_direction(
            input->main_stick_x,
            fighter->axis_dead_zone);
    const int8_t ground_horizontal_direction =
        pf_m4_axis_direction(
            input->main_stick_x,
            fighter->walk_axis_threshold);
    const int8_t strong_direction =
        pf_m4_strong_direction(
            input->main_stick_x,
            fighter->dash_axis_threshold);
    const uint16_t secondary_horizontal_magnitude =
        pf_m4_axis_magnitude(input->secondary_stick_x);
    const uint16_t secondary_vertical_magnitude =
        pf_m4_axis_magnitude(input->secondary_stick_y);
    const pf_m4_ssbm_ground_input_attributes *source_ground_input =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? pf_m4_ssbm_common_reference_ground_input()
            : NULL;
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
    const uint16_t initial_dash_special_end_frame =
        source_ground_input != NULL
            ? source_ground_input->initial_dash_special_end_frame
            : UINT16_C(0);
    /* ftCo_Dash_IASA observes the animation frame being processed now,
     * whereas action_ticks is the last completed frame exposed by the sim.
     * Keep that clock conversion explicit so the imported x44/x4C branch
     * boundaries retain their source meaning. */
    const uint32_t reference_current_anim_frame =
        (uint32_t)world->action_ticks[player_index] + UINT32_C(1);
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
        pf_m4_axis_direction(
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
        pf_m4_strong_direction(
            input->main_stick_x,
            forward_smash_axis_threshold);
    const int forward_smash_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        forward_smash_direction != INT8_C(0) &&
        (((world->action_state[player_index] ==
                   (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           world->action_state[player_index] ==
                   (uint8_t)PF_M4_ACTION_WALK) &&
          input_tilt_x_age < forward_smash_window_ticks) ||
         (world->action_state[player_index] ==
              (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          forward_smash_direction == world->facing[player_index]));
    const int vertical_smash_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        (world->action_state[player_index] ==
             (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
         world->action_state[player_index] ==
             (uint8_t)PF_M4_ACTION_WALK) &&
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
        pf_m4_select_ground_strong_attack_action(
            fighter,
            source_ground_input,
            input->main_stick_x,
            input->main_stick_y);
    const uint8_t ground_smash_charge_action =
        forward_smash_pressed != 0
            ? pf_m4_smash_charge_action_for_release(
                  forward_smash_release_action)
            : (input->main_stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_STRONG_CHARGE
                   : (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE);
    const int ground_strong_attack_pressed =
        grab_blocks_attack == 0 && strong_attack_pressed != 0;
    const uint8_t ground_light_attack_action =
        pf_m4_select_ground_light_attack_action(
            fighter,
            source_ground_input,
            world->facing[player_index],
            input->main_stick_x,
            input->main_stick_y);
    const uint8_t ground_strong_attack_action =
        pf_m4_select_ground_strong_input_action(
            fighter,
            source_ground_input,
            reference_c_stick_attack_action,
            strong_attack_stick_x,
            strong_attack_stick_y);
    const int dash_attack_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        ground_smash_charge_pressed == 0 &&
        pf_m4_action_can_start_dash_attack(
            fighter,
            world->action_state[player_index],
            world->action_ticks[player_index]);
    const int reference_initial_dash_forward_smash =
        source_ground_input != NULL &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        reference_current_anim_frame <=
            initial_dash_early_end_frame &&
        (forward_smash_pressed != 0 ||
         reference_c_stick_attack_action ==
             (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK);
    const int reference_initial_dash_dash_attack =
        source_ground_input != NULL &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        reference_current_anim_frame >
            initial_dash_early_end_frame &&
        reference_current_anim_frame <=
            initial_dash_special_end_frame &&
        grab_blocks_attack == 0 && light_attack_pressed != 0;
    const int reference_initial_dash_attack_allowed =
        source_ground_input == NULL ||
        world->action_state[player_index] !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        reference_initial_dash_forward_smash != 0 ||
        reference_initial_dash_dash_attack != 0;
    const int attack_pressed =
        grab_blocks_attack == 0 &&
        (light_attack_pressed != 0 || strong_attack_pressed != 0);
    const int jump_cancel_attack_pressed =
        world->grounded[player_index] != UINT8_C(0) &&
        world->action_state[player_index] ==
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
    const int main_stick_spot_dodge_pressed =
        shield_held != 0 &&
        input->main_stick_y >= (int16_t)escape_axis_threshold &&
        input_tilt_y_age < escape_tilt_window_ticks;
    const int secondary_stick_spot_dodge_buffered =
        shield_held != 0 &&
        input->secondary_stick_y >= (int16_t)escape_axis_threshold &&
        world->action_state[player_index] ==
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
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD;
    const int roll_pressed =
        main_stick_roll_pressed != 0 ||
        secondary_stick_roll_buffered != 0;
    const int8_t roll_direction =
        main_stick_roll_pressed != 0
            ? pf_m4_axis_direction(
                  input->main_stick_x,
                  escape_axis_threshold)
            : pf_m4_axis_direction(
                  input->secondary_stick_x,
                  escape_axis_threshold);
    const int shield_jump_pressed =
        jump_pressed != 0 ||
        (shield_held != 0 &&
         secondary_jump_up_buffered != 0 &&
         world->action_state[player_index] ==
             (uint8_t)PF_M4_ACTION_SHIELD);
    const int shield_release_spot_dodge_pressed =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        ((input->main_stick_y >= (int16_t)escape_axis_threshold &&
          input_tilt_y_age < escape_tilt_window_ticks) ||
         input->secondary_stick_y >=
             (int16_t)escape_axis_threshold);
    const int shield_release_jump_pressed =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        (jump_pressed != 0 || secondary_jump_up_buffered != 0);
    const int shield_platform_drop_requested =
        shield_held != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        pf_m4_surface_is_pass_through(
            content,
            world->support[player_index]) != 0 &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD &&
        input->main_stick_y >=
            (int16_t)fighter->shield_drop_axis_threshold;
    const pf_m4_reference_move *ground_reference_attack =
        world->grounded[player_index] != UINT8_C(0) &&
        pf_m4_action_is_ground_attack(
            world->action_state[player_index])
            ? pf_m4_falcon_ground_reference_attack(
                  fighter,
                  world->action_state[player_index])
            : NULL;
    const pf_m4_reference_iasa_policy ground_iasa_policy =
        ground_reference_attack != NULL
            ? pf_m4_falcon_reference_iasa_policy_for_action(
                  world->action_state[player_index])
            : PF_M4_REFERENCE_IASA_NONE;
    const int ground_attack_iasa =
        ground_reference_attack != NULL &&
        pf_m4_falcon_reference_iasa_active(
            world->action_state[player_index],
            (uint32_t)world->action_ticks[player_index] + UINT32_C(1));
    const uint8_t ground_iasa_capabilities =
        ground_attack_iasa != 0
            ? pf_m4_falcon_ground_iasa_capabilities(
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
        ((world->action_state[player_index] ==
              (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
          (uint32_t)world->action_ticks[player_index] + UINT32_C(1) >=
              source_character->jab_1_combo_enable_frame) ||
         (world->action_state[player_index] ==
              (uint8_t)PF_M4_ACTION_JAB_FINAL &&
          (uint32_t)world->action_ticks[player_index] + UINT32_C(1) >=
              source_character->jab_2_combo_enable_frame));
    const int reference_rapid_jab_ready =
        source_character != NULL &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_JAB_THIRD &&
        (uint32_t)world->action_ticks[player_index] + UINT32_C(1) >=
            source_character->jab_3_rapid_enable_frame &&
        (uint16_t)world->rapid_jab_input_count[player_index] +
                (uint16_t)(light_attack_pressed != 0 ||
                           light_attack_released != 0) >=
            source_character->rapid_jab_input_count;
    int32_t position_x = world->position_x_q16[player_index];
    int32_t position_y = world->position_y_q16[player_index];
    int32_t velocity_x = world->velocity_x_q16[player_index];
    int32_t velocity_y = world->velocity_y_q16[player_index];
    uint16_t action_ticks = world->action_ticks[player_index];
    uint16_t source_submotion =
        world->source_submotion[player_index];
    int32_t source_animation_frame_q16 =
        world->source_animation_frame_q16[player_index];
    int32_t fall_animation_blend_q16 =
        world->fall_animation_blend_q16[player_index];
    uint8_t fall_animation_target_switched =
        world->fall_animation_target_switched[player_index];
    int32_t source_animation_rate_q16 =
        world->source_animation_rate_q16[player_index];
    uint8_t ecb_bottom_lock_ticks =
        world->ecb_bottom_lock_ticks[player_index];
    int32_t ecb_locked_bottom_y_q16 =
        world->ecb_locked_bottom_y_q16[player_index];
    const int32_t previous_locked_bottom_y_q16 =
        ecb_bottom_lock_ticks != UINT8_C(0)
            ? ecb_locked_bottom_y_q16
            : PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16;
    int32_t inherited_locked_bottom_y_q16 =
        ecb_bottom_lock_ticks > UINT8_C(1)
            ? ecb_locked_bottom_y_q16
            : PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16;

    scratch->ecb_bottom_lock_ticks[player_index] =
        ecb_bottom_lock_ticks;
    scratch->ecb_locked_bottom_y_q16[player_index] =
        ecb_locked_bottom_y_q16;
    uint16_t respawn_count = world->respawn_count[player_index];
    uint8_t grounded = world->grounded[player_index];
    uint8_t action_state = world->action_state[player_index];
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
    uint8_t fast_fall = world->fast_fall[player_index];
    int8_t facing = world->facing[player_index];
    const pf_m4_ssbm_ledge_response_attributes *reference_ledge_response =
        fighter->reference_frame_data_enabled != UINT8_C(0)
            ? pf_m4_ssbm_common_reference_ledge_response()
            : NULL;
    const int ledge_c_attack_held =
        reference_ledge_response != NULL &&
        input->secondary_stick_y <=
            -(int16_t)reference_ledge_response->c_attack_axis_threshold;
    const int ledge_c_roll_inward_held =
        reference_ledge_response != NULL &&
        (int32_t)facing * (int32_t)input->secondary_stick_x >=
            (int32_t)reference_ledge_response->c_roll_axis_threshold;
    int8_t dash_direction =
        world->dash_direction[player_index];
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
            (pf_m4_axis_is_in_prone_horizontal_wedge(
                 input->secondary_stick_x,
                 input->secondary_stick_y,
                 fighter->down_horizontal_axis_threshold,
                 fighter->down_horizontal_angle_tan_q16)
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
    int dropped_platform_this_tick = 0;
    int ledge_motion_handled = 0;
    int released_ledge_this_tick = 0;
    int initial_dash_entered_this_tick = 0;
    int resumed_hitlag_motion_this_tick = 0;
    int revival_drop_this_tick = 0;
    int damage_fall_wiggle_this_tick = 0;
    int damage_released_jump_requested = 0;
    int exact_wall_response_this_tick = 0;
    int32_t exact_wall_contact_position_y_q16 = INT32_C(0);
    int32_t initial_dash_entry_motion_velocity_x = velocity_x;
    int32_t animation_motion_x_q16 = INT32_C(0);
    int32_t integrated_self_x_q16;
    int32_t integrated_self_y_q16;
    int32_t integrated_animation_x_q16;
    int32_t integrated_animation_y_q16;
    int hitstun_locked;
    int32_t previous_position_x;
    int64_t next_position;
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
        animation_motion_x_q16 =
            (world->dash_direction[player_index] < INT8_C(0)
                 ? INT32_C(-1)
                 : INT32_C(1)) *
            INT32_C(2051);
        dash_direction = INT8_C(0);
    }

    pf_m4_copy_combat_scratch(world, scratch, player_index);
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
                pf_m4_prepare_spawn(
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
                    scratch->damage_q16[player_index] =
                        UINT32_C(300) * (uint32_t)PF_Q16_ONE;
                }
                status = pf_sim_push_event(
                    scratch,
                    world->tick,
                    PF_SIM_EVENT_RESPAWN,
                    PF_SIM_EVENT_NO_PLAYER,
                    (uint8_t)player_index,
                    scratch->damage_q16[player_index],
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

        pf_m4_update_shield_tilt(
            scratch,
            input,
            player_index,
            action_state,
            scratch->hitlag_resume_action[player_index],
            facing);
        pf_m4_write_scratch(
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
            action_ticks >= stage->revival_platform_descent_ticks;
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

        position_x =
            ((int32_t)(UINT32_C(2) * player_index + UINT32_C(1)) -
             (int32_t)world->player_count) *
            stage->spawn_spacing_q16;
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        grounded = UINT8_C(1);
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
            position_y = stage->revival_platform_end_y_q16 -
                         fighter->half_height_q16;
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
                pf_m4_revival_platform_y(stage, action_ticks) -
                fighter->half_height_q16;
        }

        if (input_drop == 0 && automatic_drop == 0)
        {
            pf_m4_update_shield_tilt(
                scratch,
                input,
                player_index,
                action_state,
                scratch->hitlag_resume_action[player_index],
                facing);
            pf_m4_write_scratch(
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
        !pf_m4_action_freezes_shield_strength(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        scratch->shield_strength[player_index] =
            input_shield_strength;
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
        !pf_m4_action_is_shield_break(action_state) &&
        scratch->tech_lockout_ticks[player_index] == UINT16_C(0))
    {
        scratch->tech_window_ticks[player_index] =
            fighter->tech_window_ticks;
        scratch->tech_lockout_ticks[player_index] =
            fighter->tech_lockout_ticks;
    }
    scratch->shield_held[player_index] = input_trigger_state;
    if (shield_pressed != 0)
    {
        scratch->trigger_input_age[player_index] = UINT8_C(0);
    }
    else if (scratch->trigger_input_age[player_index] < UINT8_MAX)
    {
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

    if (scratch->hitlag_ticks[player_index] > UINT16_C(0) ||
        (action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
         scratch->hitlag_resume_action[player_index] != UINT8_C(0)))
    {
        const int resolving_zero_hitlag =
            scratch->hitlag_ticks[player_index] == UINT16_C(0);
        const int drop_cancel_eligible =
            resolving_zero_hitlag == 0 &&
            pf_m4_drop_cancel_hitlag_is_eligible(
                fighter,
                action_ticks,
                scratch->hitlag_ticks[player_index],
                scratch->hitlag_resume_action[player_index],
                platform_drop_ticks);

        scratch->position_x_q16[player_index] = position_x;
        scratch->position_y_q16[player_index] = position_y;
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
            int32_t drop_cancel_surface_y_q16 = INT32_C(0);
            uint8_t drop_cancel_support =
                (uint8_t)PF_M4_SURFACE_NONE;

            if (pf_m4_find_drop_cancel_platform(
                    stage,
                    fighter,
                    world->tick + UINT64_C(1),
                    position_x,
                    position_y,
                    &drop_cancel_surface_y_q16,
                    &drop_cancel_support))
            {
                uint8_t landing_action =
                    scratch->hitlag_resume_action[player_index];

                pf_m4_land_from_air(
                    content,
                    drop_cancel_surface_y_q16,
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
                scratch->position_y_q16[player_index] = position_y;
                scratch->grounded[player_index] = grounded;
                scratch->support[player_index] = support;
            }
        }
        if (resolving_zero_hitlag == 0 &&
            (pf_m4_action_is_damage(
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
                    ? pf_m4_axis_magnitude(input->main_stick_x) >=
                          fighter->sdi_stick_threshold
                    : pf_m4_ssbm_stick_meets_radial_threshold(
                          input->main_stick_x,
                          input->main_stick_y,
                          fighter->sdi_stick_threshold);
            const int fresh_sdi_tilt =
                shield_sdi != 0
                    ? tilt_x_age < fighter->sdi_stick_window_ticks
                    : tilt_x_age < fighter->sdi_stick_window_ticks ||
                          tilt_y_age < fighter->sdi_stick_window_ticks;
            const int8_t sdi_x =
                sdi_stick_active != 0
                    ? pf_m4_axis_direction(
                          input->main_stick_x,
                          UINT16_C(0))
                    : INT8_C(0);
            const int8_t sdi_y =
                sdi_stick_active != 0 && shield_sdi == 0
                    ? pf_m4_axis_direction(
                          input->main_stick_y,
                          UINT16_C(0))
                    : INT8_C(0);

            if (sdi_stick_active != 0 && fresh_sdi_tilt != 0)
            {
                status = pf_m4_apply_hitlag_shift(
                    content,
                    world,
                    scratch,
                    player_index,
                    input->main_stick_x,
                    shield_sdi != 0
                        ? INT16_C(0)
                        : input->main_stick_y,
                    shield_sdi != 0
                        ? pf_m4_multiply_q16(
                              fighter->sdi_distance_x_q16,
                              fighter->shield_sdi_scale_q16)
                        : fighter->sdi_distance_x_q16,
                    shield_sdi != 0
                        ? INT32_C(0)
                        : fighter->sdi_distance_y_q16,
                    shield_sdi);
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
            if (pf_m4_action_is_damage(action_state) ||
                action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
            {
                const int c_stick_asdi =
                    pf_m4_ssbm_stick_meets_radial_threshold(
                        input->secondary_stick_x,
                        input->secondary_stick_y,
                        fighter->sdi_stick_threshold);
                const int main_stick_asdi =
                    pf_m4_ssbm_stick_meets_radial_threshold(
                        input->main_stick_x,
                        input->main_stick_y,
                        fighter->sdi_stick_threshold);

                if (c_stick_asdi != 0 || main_stick_asdi != 0)
                {
                    status = pf_m4_apply_hitlag_shift(
                        content,
                        world,
                        scratch,
                        player_index,
                        c_stick_asdi != 0
                            ? input->secondary_stick_x
                            : input->main_stick_x,
                        c_stick_asdi != 0
                            ? input->secondary_stick_y
                            : input->main_stick_y,
                        fighter->asdi_distance_x_q16,
                        fighter->asdi_distance_y_q16,
                        0);
                    if (status != PF_STATUS_OK)
                    {
                        return status;
                    }
                }
                status = pf_m4_ssbm_apply_di_q16(
                    fighter->di_max_angle_radians_q30,
                    input->main_stick_x,
                    input->main_stick_y,
                    &scratch
                         ->knockback_velocity_x_q16[player_index],
                    &scratch
                         ->knockback_velocity_y_q16[player_index]);
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
                    scratch->ground_knockback_velocity_q16[player_index] !=
                        INT32_C(0))
                {
                    scratch->knockback_velocity_x_q16[player_index] =
                        scratch->ground_knockback_velocity_q16[player_index];
                    scratch->knockback_velocity_y_q16[player_index] =
                        INT32_C(0);
                }
                else
                {
                    grounded = UINT8_C(0);
                    support = (uint8_t)PF_M4_SURFACE_NONE;
                    scratch->grounded[player_index] = UINT8_C(0);
                    scratch->support[player_index] =
                        (uint8_t)PF_M4_SURFACE_NONE;
                    scratch->ground_knockback_velocity_q16[player_index] =
                        INT32_C(0);
                }
                fast_fall = UINT8_C(0);
                dash_direction = INT8_C(0);
            }
            else if (
                action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN)
            {
                if (pf_m4_axis_magnitude(input->main_stick_x) >=
                    fighter->sdi_stick_threshold)
                {
                    status = pf_m4_apply_hitlag_shift(
                        content,
                        world,
                        scratch,
                        player_index,
                        input->main_stick_x,
                        INT16_C(0),
                        pf_m4_multiply_q16(
                            fighter->asdi_distance_x_q16,
                            fighter->shield_sdi_scale_q16),
                        INT32_C(0),
                        1);
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
                pf_m4_enter_shield_break_launch(
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
        position_x = scratch->position_x_q16[player_index];
        position_y = scratch->position_y_q16[player_index];
        grounded = scratch->grounded[player_index];
        support = scratch->support[player_index];
        if (!pf_m4_action_retains_shield_strength(
                action_state,
                scratch->hitlag_resume_action[player_index]))
        {
            scratch->shield_strength[player_index] = UINT16_C(0);
        }
        pf_m4_update_shield_tilt(
            scratch,
            input,
            player_index,
            action_state,
            scratch->hitlag_resume_action[player_index],
            facing);
        if (resumed_hitlag_motion_this_tick == 0)
        {
            pf_m4_write_scratch(
                scratch,
                player_index,
                input,
                position_x,
                position_y,
                velocity_x,
                velocity_y,
                action_ticks,
                source_submotion,
                source_animation_frame_q16,
                source_animation_rate_q16,
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
        ((pf_m4_action_is_damage(action_state) ||
          pf_m4_action_is_surface_bounce(action_state)) &&
         scratch->hitstun_ticks[player_index] > UINT16_C(0));
    }

    if (hitstun_locked &&
        action_state == (uint8_t)PF_M4_ACTION_HITSTUN &&
        grounded == UINT8_C(0) &&
        (directional_input_flags &
         PF_M4_DIRECTIONAL_INPUT_METEOR_CANCEL) != UINT8_C(0))
    {
        const pf_m4_ssbm_damage_response_attributes *damage_response =
            pf_m4_ssbm_common_reference_damage_response();
        const int lockout_elapsed =
            damage_response != NULL &&
            (uint32_t)action_ticks + UINT32_C(1) >=
                damage_response->meteor_cancel_lockout_ticks;
        const int falling_from_meteor =
            scratch->knockback_velocity_y_q16[player_index] > INT32_C(0);
        const int up_special_cancel =
            lockout_elapsed != 0 && falling_from_meteor != 0 &&
            special_pressed != 0 &&
            input->main_stick_y <=
                -(int16_t)fighter->dash_axis_threshold;
        const int double_jump_cancel =
            up_special_cancel == 0 && lockout_elapsed != 0 &&
            falling_from_meteor != 0 && jump_pressed != 0 &&
            air_jumps_remaining > UINT8_C(0);

            if (up_special_cancel != 0 || double_jump_cancel != 0)
            {
            scratch->knockback_velocity_x_q16[player_index] =
                INT32_C(0);
            scratch->knockback_velocity_y_q16[player_index] =
                INT32_C(0);
            scratch->ground_knockback_velocity_q16[player_index] =
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
                pf_m4_enter_double_jump(
                    fighter,
                    input,
                    &velocity_x,
                    &velocity_y,
                    &air_jumps_remaining,
                    &fast_fall,
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

    if (pf_m4_action_is_damage(action_state) ||
        pf_m4_action_is_surface_bounce(action_state))
    {
        const pf_m4_ssbm_damage_response_attributes *damage_response =
            pf_m4_ssbm_common_reference_damage_response();

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
            if (action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
            {
                action_state =
                    grounded != UINT8_C(0)
                        ? (uint8_t)PF_M4_ACTION_GROUND_IDLE
                        : (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
                source_submotion =
                    grounded != UINT8_C(0)
                        ? (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT
                        : (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            }
            /* Damage_IASA synthesizes X/Y from the stored x14 timer and
             * then enters the ordinary Wait/Fall IASA table. Preserve that
             * request until those tables reach their jump callback: special,
             * escape, attack, and taunt inputs must retain their source
             * priority and must not consume an air jump first. */
            damage_released_jump_requested = requested_jump;
            hitstun_locked = 0;
        }
    }

    if (!hitstun_locked &&
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
        grounded == UINT8_C(0) &&
        scratch->tumble[player_index] != UINT8_C(0) &&
        special_pressed == 0 && light_attack_pressed == 0 &&
        strong_attack_pressed == 0 &&
        !((jump_pressed != 0 ||
           damage_released_jump_requested != 0) &&
          air_jumps_remaining > UINT8_C(0)))
    {
        const pf_m4_ssbm_damage_response_attributes *damage_response =
            pf_m4_ssbm_common_reference_damage_response();

        if (damage_response != NULL &&
            horizontal_magnitude >=
                damage_response->damage_fall_wiggle_axis_threshold &&
            tilt_x_age <
                damage_response->damage_fall_wiggle_tilt_window_ticks)
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

    if (pf_m4_action_uses_ledge(action_state))
    {
        const uint8_t ledge =
            pf_m4_ledge_from_state(
                action_state,
                scratch->hitlag_resume_action[player_index],
                facing);
        const int8_t inward =
            pf_m4_ledge_inward_direction(ledge);
        const int8_t outward = (int8_t)-inward;
        int32_t hang_x = position_x;
        int32_t hang_y = position_y;

        if (fighter->reference_frame_data_enabled != UINT8_C(0))
        {
            const pf_m4_ssbm_ledge_response_attributes *ledge_response =
                reference_ledge_response;
            const pf_m4_falcon_common_attributes *common =
                pf_m4_falcon_reference_common_attributes();
            const int quick =
                ledge_response != NULL &&
                scratch->damage_q16[player_index] <
                    (uint32_t)ledge_response->damage_threshold_percent *
                        (uint32_t)PF_Q16_ONE;
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
                const pf_m4_falcon_submotion_data *catch_motion =
                    pf_m4_falcon_reference_submotion(
                        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH);
                int32_t translation_x_q16;
                int32_t translation_y_q16;

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
                    pf_m4_ledge_hang_position(
                        fighter,
                        stage,
                        ledge,
                        &position_x,
                        &position_y);
                }
                else if (!pf_m4_falcon_reference_translation_q16(
                             (uint16_t)
                                 PF_M4_FALCON_SUBMOTION_LEDGE_CATCH,
                             (uint16_t)(action_ticks + UINT16_C(2)),
                             &translation_x_q16,
                             &translation_y_q16))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                else
                {
                    position_x += (int32_t)facing * translation_x_q16;
                    position_y += translation_y_q16;
                    ++action_ticks;
                }
                ledge_motion_handled = 1;
            }
            else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG)
            {
                const int main_option =
                    pf_m4_reference_ledge_direction_option(
                        input->main_stick_x,
                        input->main_stick_y,
                        facing,
                        ledge_response);
                const int c_option =
                    pf_m4_reference_ledge_direction_option(
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
                pf_m4_ledge_hang_position(
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
                    if (!pf_m4_enter_reference_ledge_option(
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
                const pf_m4_falcon_submotion_data *motion =
                    pf_m4_falcon_reference_submotion(source_submotion);
                const uint16_t ground_frame =
                    pf_m4_falcon_reference_ledge_option_ground_frame(
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
                const int32_t previous_option_y_q16 = position_y;
                int32_t translation_x_q16;
                int32_t translation_y_q16;
                int32_t jump_x_from_wait_q16 = INT32_C(0);
                int32_t jump_y_from_wait_q16 = INT32_C(0);
                const int uses_hyrule_jump_path =
                    stage->reference_collision_profile ==
                        (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE &&
                    pf_m4_falcon_reference_hyrule_ledge_jump_position_q16(
                        source_submotion,
                        translation_frame,
                        &jump_x_from_wait_q16,
                        &jump_y_from_wait_q16);

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
                            common->ledge_jump_horizontal_velocity_q16;
                        velocity_y =
                            -common->ledge_jump_vertical_velocity_q16;
                        if (stage->reference_collision_profile ==
                                (uint16_t)
                                    PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE &&
                            pf_m4_falcon_reference_hyrule_ledge_jump_position_q16(
                                source_submotion,
                                UINT16_C(1),
                                &jump_x_from_wait_q16,
                                &jump_y_from_wait_q16))
                        {
                            pf_m4_ledge_hang_position(
                                fighter,
                                stage,
                                ledge,
                                &hang_x,
                                &hang_y);
                            position_x =
                                hang_x -
                                (int32_t)inward * jump_x_from_wait_q16;
                            position_y = hang_y + jump_y_from_wait_q16;
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
                        pf_m4_ledge_hang_position(
                            fighter,
                            stage,
                            ledge,
                            &hang_x,
                            &hang_y);
                        position_x =
                            hang_x - (int32_t)inward * jump_x_from_wait_q16;
                        position_y = hang_y + jump_y_from_wait_q16;
                    }
                    else
                    {
                        if (translation_frame > motion->translation_count)
                        {
                            translation_x_q16 = INT32_C(0);
                            translation_y_q16 = INT32_C(0);
                        }
                        else if (!pf_m4_falcon_reference_translation_q16(
                                     source_submotion,
                                     translation_frame,
                                     &translation_x_q16,
                                     &translation_y_q16))
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                        position_x += (int32_t)inward * translation_x_q16;
                        if (grounded == UINT8_C(0))
                        {
                            position_y += translation_y_q16;
                            if (will_ground != 0)
                            {
                                position_x +=
                                    (int32_t)inward * translation_x_q16;
                                support =
                                    pf_m4_ssbm_reference_stage_ledge_support(
                                        stage->reference_collision_profile,
                                        pf_m4_ledge_x_q16(stage, ledge));
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
                                pf_m4_surface_y_q16(
                                    content,
                                    support,
                                    position_x) -
                                fighter->half_height_q16;
                            if (was_grounded != 0)
                            {
                                velocity_x =
                                    (int32_t)inward * translation_x_q16;
                                velocity_y =
                                    position_y - previous_option_y_q16;
                            }
                            else
                            {
                                velocity_x =
                                    (int32_t)inward * translation_x_q16;
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
            pf_m4_ledge_hang_position(
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
            const pf_m4_falcon_submotion_data *catch_motion =
                pf_m4_falcon_reference_submotion(
                    (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH);
            int32_t translation_x_q16;
            int32_t translation_y_q16;

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
                pf_m4_ledge_hang_position(
                    fighter,
                    stage,
                    ledge,
                    &position_x,
                    &position_y);
            }
            else if (!pf_m4_falcon_reference_translation_q16(
                         (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH,
                         (uint16_t)(action_ticks + UINT16_C(2)),
                         &translation_x_q16,
                         &translation_y_q16))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            else
            {
                position_x += (int32_t)facing * translation_x_q16;
                position_y += translation_y_q16;
                ++action_ticks;
            }
            ledge_motion_handled = 1;
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            const uint16_t catch_ticks =
                pf_m4_ledge_transition_ticks(fighter);
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
                    (int32_t)inward * fighter->ledge_jump_speed_x_q16;
                velocity_y = -fighter->ledge_jump_speed_y_q16;
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
                        ? (int32_t)outward *
                              fighter->air_speed_q16
                        : (int32_t)outward *
                              fighter->platform_drop_nudge_q16;
                velocity_y = fighter->gravity_q16;
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
                pf_m4_ledge_transition_ticks(fighter);
            const int32_t target_x =
                pf_m4_ledge_x_q16(stage, ledge) +
                (int32_t)inward *
                    (fighter->half_width_q16 +
                     fighter->platform_drop_nudge_q16);
            const int32_t target_y =
                stage->floor_y_q16 - fighter->half_height_q16;

            ++action_ticks;
            if (action_ticks >= climb_ticks)
            {
                position_x = target_x;
                pf_m4_land(
                    fighter,
                    stage->floor_y_q16,
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
                position_x =
                    hang_x +
                    (int32_t)(
                        ((int64_t)target_x - (int64_t)hang_x) *
                        (int64_t)action_ticks /
                        (int64_t)climb_ticks);
                position_y =
                    hang_y +
                    (int32_t)(
                        ((int64_t)target_y - (int64_t)hang_y) *
                        (int64_t)action_ticks /
                        (int64_t)climb_ticks);
            }
            ledge_motion_handled = 1;
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL)
        {
            const int32_t target_x =
                pf_m4_ledge_x_q16(stage, ledge) +
                (int32_t)inward * fighter->ledge_roll_distance_q16;
            const int32_t target_y =
                stage->floor_y_q16 - fighter->half_height_q16;
            const uint16_t movement_ticks =
                fighter->ledge_roll_movement_ticks;

            ++action_ticks;
            if (action_ticks >= fighter->ledge_roll_ticks)
            {
                position_x = target_x;
                pf_m4_land(
                    fighter,
                    stage->floor_y_q16,
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

                position_x =
                    hang_x +
                    (int32_t)(
                        ((int64_t)target_x - (int64_t)hang_x) *
                        (int64_t)progress_ticks /
                        (int64_t)movement_ticks);
                position_y =
                    hang_y +
                    (int32_t)(
                        ((int64_t)target_y - (int64_t)hang_y) *
                        (int64_t)progress_ticks /
                        (int64_t)movement_ticks);
            }
            ledge_motion_handled = 1;
        }
        else
        {
            const pf_m4_attack_data *attack = &fighter->ledge_attack;
            const uint32_t total_ticks =
                (uint32_t)attack->startup_ticks +
                (uint32_t)attack->active_ticks +
                (uint32_t)attack->recovery_ticks;
            const int32_t target_x =
                pf_m4_ledge_x_q16(stage, ledge) +
                (int32_t)inward *
                    (fighter->half_width_q16 +
                     fighter->platform_drop_nudge_q16);
            const int32_t target_y =
                stage->floor_y_q16 - fighter->half_height_q16;

            ++action_ticks;
            if ((uint32_t)action_ticks >= total_ticks)
            {
                position_x = target_x;
                pf_m4_land(
                    fighter,
                    stage->floor_y_q16,
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

                position_x =
                    hang_x +
                    (int32_t)(
                        ((int64_t)target_x - (int64_t)hang_x) *
                        (int64_t)progress_ticks /
                        (int64_t)movement_ticks);
                position_y =
                    hang_y +
                    (int32_t)(
                        ((int64_t)target_y - (int64_t)hang_y) *
                        (int64_t)progress_ticks /
                        (int64_t)movement_ticks);
            }
            ledge_motion_handled = 1;
        }
        }
    }

    if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        const int32_t previous_platform_x =
            pf_m4_platform_center_x_q16(stage, world->tick);
        const int32_t next_platform_x =
            pf_m4_platform_center_x_q16(
                stage,
                world->tick + UINT64_C(1));
        next_position =
            (int64_t)position_x +
            ((int64_t)next_platform_x -
             (int64_t)previous_platform_x);
        if (!pf_m4_checked_i32(next_position, &position_x))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
    }

    if (!ledge_motion_handled &&
        released_ledge_this_tick == 0 &&
        !hitstun_locked &&
        grounded == UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
        fighter->wall_jump_enabled != UINT8_C(0) &&
        strong_direction != INT8_C(0) &&
        strong_direction != previous_strong_direction &&
        strong_direction == pf_m4_wall_contact_away_direction(
                                content,
                                position_x,
                                position_y))
    {
        pf_m4_enter_wall_jump(
            fighter,
            strong_direction,
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
        pf_m4_action_is_ground_attack(action_state))
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
        !pf_m4_action_is_reference_special_locked(action_state) &&
        action_state != (uint8_t)PF_M4_ACTION_WALL_JUMP &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        !(fighter->reference_frame_data_enabled != UINT8_C(0) &&
          action_state == (uint8_t)PF_M4_ACTION_WALK &&
          grab_pressed != 0) &&
        (fighter->reference_frame_data_enabled == UINT8_C(0) ||
         powershield_release_cancel_ready != 0 ||
         pf_m4_reference_action_allows_special(
             action_state,
             action_ticks,
             grounded,
             pf_m4_normal_landing_is_interruptible(
                 fighter,
                 action_state,
                 action_ticks),
             ground_iasa_capabilities,
             initial_dash_special_end_frame)) &&
        (!pf_m4_action_is_ground_attack(action_state) ||
         ground_reference_attack == NULL ||
         (ground_iasa_capabilities &
          PF_M4_FALCON_IASA_SPECIAL) != UINT8_C(0)) &&
        special_pressed != 0)
    {
        const pf_m4_falcon_common_special_attributes *
            common_special_attributes =
                pf_m4_falcon_reference_common_special_attributes();
        const uint16_t special_vertical_axis_threshold =
            source_ground_input != NULL
                ? source_ground_input->special_vertical_axis_threshold
                : fighter->dash_axis_threshold;
        const int raw_up_special_requested =
            input->main_stick_y <=
            -(int16_t)special_vertical_axis_threshold;
        const int raw_down_special_requested =
            input->main_stick_y >=
            (int16_t)special_vertical_axis_threshold;
        const int32_t special_stick_x_q16 =
            pf_m4_axis_q16(input->main_stick_x);
        const int raw_side_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            common_special_attributes != NULL &&
            (special_stick_x_q16 >=
                 common_special_attributes
                     ->side_special_stick_threshold_q16 ||
             special_stick_x_q16 <=
                 -common_special_attributes
                      ->side_special_stick_threshold_q16);
        /* Ground common IASA checks SpecialS before Hi/N/Lw. SpecialAir
         * instead checks Hi, Lw, S, then N. */
        const int up_special_requested =
            raw_up_special_requested != 0 &&
            (grounded == UINT8_C(0) ||
             raw_side_special_requested == 0);
        const int charge_requested =
            content->charge.enabled != UINT8_C(0) &&
            grounded != UINT8_C(0) &&
            up_special_requested != 0 &&
            light_attack_held != 0;
        const int vector_ascent_requested =
            up_special_requested != 0 && charge_requested == 0;
        const int falcon_down_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            raw_down_special_requested != 0 &&
            (grounded == UINT8_C(0) ||
             raw_side_special_requested == 0);
        const int reflector_requested =
            fighter->reference_frame_data_enabled == UINT8_C(0) &&
            content->reflector.enabled != UINT8_C(0) &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;
        const int falcon_side_special_requested =
            raw_side_special_requested != 0 &&
            (grounded != UINT8_C(0) ||
             (up_special_requested == 0 &&
              falcon_down_special_requested == 0)) &&
            reflector_requested == 0;
        const int falcon_neutral_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            up_special_requested == 0 &&
            falcon_down_special_requested == 0 &&
            reflector_requested == 0 &&
            falcon_side_special_requested == 0;
        const int falcon_punch_blocked =
            falcon_neutral_special_requested != 0 &&
            (world->action_state[player_index] ==
                 (uint8_t)PF_M4_ACTION_CROUCH ||
             world->action_state[player_index] ==
                 (uint8_t)PF_M4_ACTION_CROUCH_END);

        if (vector_ascent_requested != 0)
        {
            if ((fighter->reference_frame_data_enabled != UINT8_C(0) ||
                 (content->recovery.enabled != UINT8_C(0) &&
                  recovery_available != UINT8_C(0))) &&
                pf_m4_action_can_start_vector_ascent(action_state))
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
                    velocity_x = pf_m4_scale_axis_q16(
                        input->main_stick_x,
                        content->recovery.horizontal_speed_q16);
                    velocity_y = -content->recovery.vertical_speed_q16;
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
        else if (falcon_punch_blocked == 0)
        {
            if (falcon_side_special_requested != 0)
            {
                if (special_stick_x_q16 * (int32_t)facing <
                    -common_special_attributes
                         ->side_special_turn_threshold_q16)
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
        if (falcon_punch_blocked == 0 &&
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
        pf_m4_normal_landing_is_interruptible(
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
        !(strong_direction != INT8_C(0) &&
          tilt_x_age < fighter->dash_input_window_ticks))
    {
        action_state = (uint8_t)PF_M4_ACTION_CROUCH;
        action_ticks = UINT16_C(0);
    }
    else if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_LANDING &&
        pf_m4_normal_landing_is_interruptible(
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
          (pf_m4_action_can_start_grab(action_state) ||
           (action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
            powershield_release_cancel_ready != 0) ||
           (pf_m4_action_is_ground_attack(action_state) &&
            (ground_iasa_capabilities &
             PF_M4_FALCON_IASA_GRAB) != UINT8_C(0)))) ||
         boost_grab_pressed != 0) &&
        scratch->grab_target_slot[player_index] == UINT8_C(0) &&
        scratch->grab_owner_slot[player_index] == UINT8_C(0))
    {
        action_state =
            boost_grab_pressed != 0 ||
                    world->action_state[player_index] ==
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
                    world->action_state[player_index] ==
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
         scratch->shield_health_q16[player_index] >
             pf_m4_shield_hold_depletion_q16(
                 fighter,
                 scratch->shield_strength[player_index])) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        if (was_shielding)
        {
            scratch->shield_health_q16[player_index] =
                pf_m4_shield_health_subtract(
                    scratch->shield_health_q16[player_index],
                    pf_m4_shield_hold_depletion_q16(
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
        shield_held != 0 &&
        grab_fallback_attack_pressed == 0 &&
        (source_ground_input == NULL || attack_pressed == 0) &&
        !pf_m4_action_is_shield(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          (source_ground_input == NULL ||
           reference_current_anim_frame <=
               initial_dash_early_end_frame)) &&
        (!pf_m4_action_is_ground_attack(action_state) ||
         (ground_iasa_capabilities &
          PF_M4_FALCON_IASA_GUARD) != UINT8_C(0)) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        pf_m4_normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        !pf_m4_action_is_aerial_landing(action_state) &&
        !pf_m4_action_locks_ground_control(action_state))
    {
        const int reference_escape_allowed =
            source_ground_input != NULL &&
            main_stick_spot_dodge_pressed != 0 &&
            (!pf_m4_action_is_ground_attack(action_state) ||
             (ground_iasa_capabilities &
              PF_M4_FALCON_IASA_ESCAPE) != UINT8_C(0));

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
        }
        action_ticks = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        pf_m4_normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        !pf_m4_action_is_aerial_landing(action_state) &&
        (!pf_m4_action_is_ground_attack(action_state) ||
         ((ground_iasa_capabilities &
           PF_M4_FALCON_IASA_ATTACK) != UINT8_C(0) &&
          (ground_iasa_policy !=
               PF_M4_REFERENCE_IASA_JAB_CHAIN ||
           ground_smash_charge_pressed != 0 ||
           ground_strong_attack_pressed != 0 ||
           ground_light_attack_action !=
               (uint8_t)PF_M4_ACTION_GROUND_ATTACK))) &&
        !pf_m4_action_is_shield(action_state) &&
        !pf_m4_action_locks_ground_control(action_state) &&
        reference_initial_dash_attack_allowed != 0 &&
        attack_pressed)
    {
        uint8_t next_attack_action;

        if (reference_initial_dash_dash_attack != 0 ||
            dash_attack_pressed != 0)
        {
            next_attack_action = (uint8_t)PF_M4_ACTION_DASH_ATTACK;
        }
        else if (reference_initial_dash_forward_smash != 0)
        {
            next_attack_action =
                reference_c_stick_attack_action ==
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
                (int32_t)facing * fighter->dash_attack_speed_q16;
        }
        if ((ground_strong_attack_pressed != 0
                 ? strong_attack_horizontal_direction
                 : horizontal_direction) != INT8_C(0) &&
            pf_m4_action_is_forward_ground_attack(action_state))
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
        pf_m4_normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        !pf_m4_action_is_aerial_landing(action_state) &&
        !pf_m4_action_is_ground_attack(action_state) &&
        !pf_m4_action_is_shield(action_state) &&
        !pf_m4_action_locks_ground_control(action_state) &&
        !(source_ground_input != NULL &&
          action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          action_ticks <= initial_dash_special_end_frame) &&
        taunt_pressed == 0 &&
        (button_jump_pressed != 0 ||
         damage_released_jump_requested != 0 ||
         (((action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
            action_ticks > initial_dash_special_end_frame) ||
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
        !(source_ground_input != NULL &&
          action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          action_ticks <= initial_dash_special_end_frame) &&
        (pf_m4_action_can_start_taunt(action_state) ||
         (pf_m4_action_is_ground_attack(action_state) &&
          (ground_iasa_capabilities &
           PF_M4_FALCON_IASA_TAUNT) != UINT8_C(0))))
    {
        action_state = (uint8_t)PF_M4_ACTION_TAUNT;
        action_ticks = UINT16_C(0);
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
        pf_m4_action_is_grounded_landing(action_state))
    {
        velocity_y = INT32_C(0);
    }

    if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        const uint32_t shield_hold_depletion_q16 =
            pf_m4_shield_hold_depletion_q16(
                fighter,
                scratch->shield_strength[player_index]);
        const uint32_t shield_health_before_depletion =
            scratch->shield_health_q16[player_index];
        const uint32_t depleted_shield_health =
            shield_hold_depletion_q16 >=
                    shield_health_before_depletion
                ? shield_health_before_depletion
                : shield_hold_depletion_q16;
        const int shield_broken_by_depletion =
            was_shielding &&
            shield_hold_depletion_q16 >
                shield_health_before_depletion;

        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_q16 ||
                    velocity_x < -fighter->walk_speed_q16
                ? fighter->turn_acceleration_q16
                : fighter->traction_q16);
        if (was_shielding)
        {
            scratch->shield_health_q16[player_index] =
                pf_m4_shield_health_subtract(
                    scratch->shield_health_q16[player_index],
                    shield_hold_depletion_q16);
        }
        if (shield_broken_by_depletion != 0)
        {
            pf_m4_enter_shield_break_launch(
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
        else if (shield_platform_drop_requested != 0)
        {
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            source_submotion =
                fighter->reference_frame_data_enabled != UINT8_C(0)
                    ? (uint16_t)
                          PF_M4_FALCON_SUBMOTION_PLATFORM_DROP
                    : (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            platform_drop_ticks =
                (uint8_t)fighter->platform_drop_ticks;
            position_y += fighter->platform_drop_nudge_q16;
            velocity_y = fighter->platform_drop_speed_y_q16;
            short_hop_latched = UINT8_C(0);
            fast_fall = UINT8_C(0);
            dash_direction = INT8_C(0);
            dropped_platform_this_tick = 1;
            scratch->powershield[player_index] = UINT8_C(0);
            scratch->shield_stun_ticks[player_index] =
                UINT16_C(0);
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_q16 ||
                    velocity_x < -fighter->walk_speed_q16
                ? fighter->turn_acceleration_q16
                : fighter->traction_q16);
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
                    input_shield_strength;
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_q16 ||
                    velocity_x < -fighter->walk_speed_q16
                ? fighter->turn_acceleration_q16
                : fighter->traction_q16);
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
            action_ticks = UINT16_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            scratch->attack_stale_registered[player_index] =
                UINT8_C(0);
            short_hop_latched = UINT8_C(0);
            dash_direction = INT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
            if ((strong_attack_pressed != 0
                     ? strong_attack_horizontal_direction
                     : horizontal_direction) != INT8_C(0) &&
                pf_m4_action_is_forward_ground_attack(action_state))
            {
                facing = strong_attack_pressed != 0
                             ? strong_attack_horizontal_direction
                             : horizontal_direction;
            }
        }
        else if (shield_release_spot_dodge_pressed != 0)
        {
            action_state = (uint8_t)PF_M4_ACTION_SPOT_DODGE;
            action_ticks = UINT16_C(0);
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
        else
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        ++action_ticks;
        if (action_ticks >= fighter->shield_break_stand_ticks)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN;
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA;
            action_ticks = pf_m4_shield_break_stun_ticks(
                fighter,
                scratch->damage_q16[player_index]);
            scratch->mash_stick_x_direction[player_index] = INT8_C(0);
            scratch->mash_stick_y_direction[player_index] = INT8_C(0);
            scratch->shield_health_q16[player_index] =
                fighter->shield_reset_health_q16;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        const uint32_t mash_pulses = pf_m4_grab_mash_pulses(
            fighter,
            raw_input,
            previous_buttons,
            dense_shield_pressed,
            scratch,
            player_index);
        uint32_t elapsed_ticks =
            (uint32_t)fighter->shield_break_stun_tick_decrement;

        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        scratch->shield_health_q16[player_index] =
            fighter->shield_reset_health_q16;
        elapsed_ticks +=
            mash_pulses *
            (uint32_t)
                fighter->shield_break_mash_reduction_ticks;
        if ((uint32_t)action_ticks <= elapsed_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->shield_health_q16[player_index] =
                fighter->shield_reset_health_q16;
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
        const pf_m4_reference_move *catch_move =
            pf_m4_falcon_reference_move(
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
            const pf_m4_falcon_up_special_timing *timing =
                pf_m4_falcon_reference_up_special_timing();

            if (timing == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            if (grounded != UINT8_C(0))
            {
                position_x +=
                    (int32_t)facing *
                    timing->grounded_throw_reposition_x_q16;
                position_y +=
                    timing->grounded_throw_reposition_y_q16;
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
           pf_m4_action_is_throw(action_state))) ||
         action_state == (uint8_t)PF_M4_ACTION_GRABBED))
    {
        if (action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE &&
            action_ticks == UINT16_C(0))
        {
            velocity_x =
                -(int32_t)facing * fighter->grab_release_speed_x_q16;
        }
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        velocity_y = INT32_C(0);
        if (action_state == (uint8_t)PF_M4_ACTION_GRAB ||
            action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB)
        {
            const int dash_grab =
                action_state == (uint8_t)PF_M4_ACTION_DASH_GRAB;
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

            ++action_ticks;
            if ((uint32_t)action_ticks >= grab_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRAB_HOLD)
        {
            const uint8_t grab_action =
                light_attack_pressed != 0
                    ? (uint8_t)PF_M4_ACTION_PUMMEL
                    : pf_m4_grab_action_for_input(
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
                        pf_m4_action_is_throw(owner_effective_action) ||
                        owner_effective_action ==
                            (uint8_t)PF_M4_ACTION_FALCON_DIVE_CATCH ||
                        owner_effective_action ==
                            (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW;
                }
            }
            if (escape_locked == 0)
            {
                const uint32_t mash_pulses = pf_m4_grab_mash_pulses(
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
            const pf_m4_falcon_submotion_data *release_motion =
                pf_m4_falcon_reference_submotion(source_submotion);
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
            const pf_m4_throw_data *throw_data =
                pf_m4_throw_for_action(fighter, action_state);
            const uint32_t throw_ticks =
                throw_data != NULL
                    ? (uint32_t)throw_data->release_tick +
                          (uint32_t)throw_data->recovery_ticks
                    : UINT32_C(0);

            if (throw_data == NULL)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            ++action_ticks;
            if ((uint32_t)action_ticks >= throw_ticks)
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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

        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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

        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        const pf_m4_reference_move *move =
            pf_m4_falcon_reference_move(
                PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND);
        int32_t reference_motion_x_q16;

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
            if (!pf_m4_falcon_reference_motion_x_q16(
                    action_state,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &reference_motion_x_q16))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x = (int32_t)facing * reference_motion_x_q16;
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_START_GROUND)
    {
        const pf_m4_reference_move *move =
            pf_m4_falcon_move_for_action(action_state);
        const pf_m4_falcon_special_attributes *attributes =
            pf_m4_falcon_reference_special_attributes();

        if (move == NULL || attributes == NULL)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_ticks >= move->total_frames)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND;
            action_ticks = UINT16_C(0);
            if (pf_m4_falcon_kick_ground_end_velocity(
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
            const int32_t hit_scale_q16 =
                pf_m4_falcon_kick_hit_velocity_scale(
                    attributes,
                    scratch->falcon_kick_hit_count[player_index]);

            if (pf_m4_falcon_kick_root_velocity(
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
                pf_m4_multiply_q16(velocity_x, hit_scale_q16);
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_KICK_END_GROUND)
    {
        const pf_m4_reference_move *move =
            pf_m4_falcon_move_for_action(action_state);

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
            if (pf_m4_falcon_kick_ground_end_velocity(
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
        const pf_m4_reference_move *move =
            pf_m4_falcon_move_for_action(action_state);
        const pf_m4_falcon_common_attributes *common =
            pf_m4_falcon_reference_common_attributes();
        const pf_m4_falcon_common_special_attributes *common_special =
            pf_m4_falcon_reference_common_special_attributes();
        const pf_m4_falcon_special_attributes *attributes =
            pf_m4_falcon_reference_special_attributes();
        const pf_m4_falcon_down_special_timing *timing =
            pf_m4_falcon_reference_down_special_timing();

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
            if (pf_m4_falcon_kick_root_velocity(
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
            const int32_t friction_q16 =
                action_ticks >= timing->landing_traction_begin_frame &&
                    action_ticks <= timing->landing_traction_end_frame
                    ? pf_m4_multiply_q16(
                          common->friction_q16,
                          attributes
                              ->speciallw_air_landing_traction_q16)
                    : pf_m4_falcon_source_ground_friction(
                          common,
                          common_special,
                          velocity_x);

            velocity_x =
                pf_m4_approach(velocity_x, INT32_C(0), friction_q16);
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND)
    {
        const pf_m4_reference_move *move =
            pf_m4_falcon_reference_move(
                PF_M4_FALCON_UP_SPECIAL_GROUND);
        const pf_m4_falcon_up_special_timing *timing =
            pf_m4_falcon_reference_up_special_timing();
        const uint16_t displayed_frame =
            (uint16_t)(action_ticks + UINT16_C(1));

        if (move == NULL || timing == NULL ||
            action_ticks >= move->total_frames ||
            pf_m4_falcon_dive_start_velocity(
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
        const pf_m4_falcon_move_index move_index =
            action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND
                ? PF_M4_FALCON_SIDE_SPECIAL_START_GROUND
                : PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND;
        const pf_m4_reference_move *move =
            pf_m4_falcon_reference_move(move_index);
        int32_t reference_motion_x_q16;

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
            if (!pf_m4_falcon_reference_motion_x_q16(
                    action_state,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &reference_motion_x_q16))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            velocity_x = (int32_t)facing * reference_motion_x_q16;
            ++action_ticks;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND)
    {
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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

        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        const pf_m4_falcon_submotion_data *rebound_motion =
            pf_m4_falcon_reference_submotion(
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
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                fighter->traction_q16);
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        pf_m4_action_locks_ground_control(action_state) &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING)
    {
        int8_t prone_roll_direction = INT8_C(0);
        const pf_m4_prone_option prone_option =
            action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                    action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT
                ? pf_m4_select_prone_option(
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
                if (prone_option == PF_M4_PRONE_OPTION_NONE)
                {
                    action_state = (uint8_t)PF_M4_ACTION_DOWN_WAIT;
                    action_ticks = UINT16_C(0);
                }
                else
                {
                    const pf_status prone_status =
                        pf_m4_enter_prone_option(
                        prone_option,
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
            if (prone_option != PF_M4_PRONE_OPTION_NONE)
            {
                const pf_status prone_status =
                    pf_m4_enter_prone_option(
                    prone_option,
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
            int32_t translation_x_q16;

            if (pf_m4_falcon_reference_translation_q16(
                    submotion_index,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &translation_x_q16,
                    NULL))
            {
                velocity_x = (int32_t)facing * translation_x_q16;
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
                pf_m4_getup_roll_submotion_for(
                    scratch->prone_roll_motion_orientation[player_index],
                    scratch->tech_direction[player_index],
                    facing);
            int32_t translation_x_q16;

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
                     !pf_m4_falcon_reference_translation_q16(
                         submotion_index,
                         (uint16_t)(action_ticks + UINT16_C(1)),
                         &translation_x_q16,
                         NULL))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            else
            {
                velocity_x = (int32_t)facing * translation_x_q16;
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
            int32_t translation_x_q16;

            if (pf_m4_falcon_reference_translation_q16(
                    submotion_index,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &translation_x_q16,
                    NULL))
            {
                velocity_x =
                    (int32_t)source_facing * translation_x_q16;
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
        pf_m4_action_is_ground_attack(action_state))
    {
        uint32_t attack_ticks = UINT32_C(0);

        if (action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_START ||
            action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP ||
            action_state == (uint8_t)PF_M4_ACTION_RAPID_JAB_END)
        {
            pf_m4_falcon_move_index move_index;
            const pf_m4_reference_move *move;
            int32_t reference_motion_x_q16;

            if (!pf_m4_falcon_reference_move_for_action(
                    action_state,
                    &move_index) ||
                (move = pf_m4_falcon_reference_move(move_index)) == NULL)
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
                    pf_m4_falcon_reference_effective_hit_frame(
                        move_index,
                        next_frame) == UINT16_C(4))
                {
                    /* Every source script hitbox-create window is a fresh
                     * collision epoch, including the five windows within one
                     * Attack100Loop animation. */
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                }
                if (pf_m4_falcon_reference_motion_x_q16(
                        action_state,
                        next_frame,
                        &reference_motion_x_q16))
                {
                    velocity_x =
                        (int32_t)facing * reference_motion_x_q16;
                }
                else
                {
                    velocity_x = INT32_C(0);
                }
                action_ticks = next_frame;
            }
        }
        else if (pf_m4_action_is_smash_charge(action_state))
        {
            const uint8_t release_action =
                pf_m4_smash_release_action(action_state);
            uint16_t source_charge_frame = UINT16_C(0);

            if (fighter->reference_frame_data_enabled != UINT8_C(0))
            {
                pf_m4_falcon_move_index move_index;
                const pf_m4_reference_move *move;

                if (!pf_m4_falcon_reference_move_for_action(
                        release_action,
                        &move_index) ||
                    (move = pf_m4_falcon_reference_move(move_index)) ==
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
                int32_t reference_motion_x_q16;

                if (pf_m4_falcon_reference_ground_physics_for_action(
                        release_action) ==
                        PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION &&
                    pf_m4_falcon_reference_motion_x_q16(
                        release_action,
                        (uint16_t)(action_ticks + UINT16_C(1)),
                        &reference_motion_x_q16))
                {
                    velocity_x =
                        (int32_t)facing * reference_motion_x_q16;
                }
                else
                {
                    velocity_x = pf_m4_approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->traction_q16);
                }
                ++action_ticks;
            }
            else
            {
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
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
                const pf_m4_attack_data *attack;

                action_state = release_action;
                action_ticks = source_charge_frame != UINT16_C(0)
                                   ? (uint16_t)(
                                         source_charge_frame - UINT16_C(1))
                                   : UINT16_C(0);
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                attack = pf_m4_directional_ground_data(
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
            const pf_m4_reference_timing timing =
                pf_m4_ground_attack_timing(fighter, action_state);

            attack_ticks =
                (uint32_t)timing.startup_ticks +
                (uint32_t)timing.active_ticks +
                (uint32_t)timing.recovery_ticks;
        }

        if (attack_ticks != UINT32_C(0))
        {
            const int reference_match =
                pf_m4_falcon_ground_reference_matches(
                    fighter,
                    action_state);
            int32_t reference_motion_x_q16;

            if (reference_match != 0 &&
                pf_m4_falcon_reference_ground_physics_for_action(
                    action_state) ==
                    PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION &&
                pf_m4_falcon_reference_motion_x_q16(
                    action_state,
                    (uint16_t)(action_ticks + UINT16_C(1)),
                    &reference_motion_x_q16))
            {
                velocity_x =
                    (int32_t)facing * reference_motion_x_q16;
            }
            else
            {
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
            }
            ++action_ticks;
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
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
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
            const int32_t carried_velocity_x = pf_m4_multiply_q16(
                velocity_x,
                fighter->jump_horizontal_momentum_multiplier_q16);
            const int32_t input_velocity_x = pf_m4_scale_axis_q16(
                input->main_stick_x,
                fighter->jump_horizontal_input_speed_q16);
            const int64_t requested_velocity_x =
                (int64_t)carried_velocity_x +
                (int64_t)input_velocity_x;

            if (requested_velocity_x <
                -(int64_t)fighter->jump_horizontal_max_speed_q16)
            {
                velocity_x =
                    -fighter->jump_horizontal_max_speed_q16;
            }
            else if (requested_velocity_x >
                     (int64_t)fighter->jump_horizontal_max_speed_q16)
            {
                velocity_x =
                    fighter->jump_horizontal_max_speed_q16;
            }
            else
            {
                velocity_x = (int32_t)requested_velocity_x;
            }
            velocity_y =
                -(short_hop_latched == UINT8_C(1)
                      ? fighter->short_hop_speed_q16
                      : fighter->full_hop_speed_q16);
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            short_hop_latched = UINT8_C(0);
            fast_fall = UINT8_C(0);
            launched_this_tick = 1;
        }
    }
    else if (!ledge_motion_handled &&
             !hitstun_locked &&
             grounded != UINT8_C(0) &&
             action_state == (uint8_t)PF_M4_ACTION_LANDING)
    {
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_q16 ||
                    velocity_x < -fighter->walk_speed_q16
                ? fighter->turn_acceleration_q16
                : fighter->traction_q16);
        ++action_ticks;
        if (action_ticks >= fighter->landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             pf_m4_action_is_aerial_landing(action_state))
    {
        uint16_t landing_ticks = pf_m4_aerial_landing_ticks(
            fighter,
            action_state);
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        const pf_m4_falcon_special_attributes *attributes =
            pf_m4_falcon_reference_special_attributes();
        const int32_t lag_q16 =
            action_state ==
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT
                ? (attributes != NULL
                       ? attributes->specials_hit_landing_lag_q16
                       : INT32_C(0))
                : (attributes != NULL
                       ? attributes->specials_miss_landing_lag_q16
                       : INT32_C(0));
        const uint16_t landing_ticks =
            (uint16_t)(lag_q16 / (int32_t)PF_Q16_ONE);

        if (attributes == NULL || lag_q16 <= INT32_C(0) ||
            lag_q16 % (int32_t)PF_Q16_ONE != INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        const pf_m4_falcon_special_attributes *attributes =
            pf_m4_falcon_reference_special_attributes();
        const int32_t lag_q16 =
            attributes != NULL
                ? attributes->specialhi_landing_lag_q16
                : INT32_C(0);
        const uint16_t landing_ticks =
            (uint16_t)(lag_q16 / (int32_t)PF_Q16_ONE);

        if (attributes == NULL || lag_q16 <= INT32_C(0) ||
            lag_q16 % (int32_t)PF_Q16_ONE != INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
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
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            velocity_x > fighter->walk_speed_q16 ||
                    velocity_x < -fighter->walk_speed_q16
                ? fighter->turn_acceleration_q16
                : fighter->traction_q16);
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
        if (pf_m4_surface_is_pass_through(content, support) != 0 &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold &&
            action_ticks >= fighter->platform_drop_startup_ticks)
        {
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            source_submotion =
                fighter->reference_frame_data_enabled != UINT8_C(0)
                    ? (uint16_t)
                          PF_M4_FALCON_SUBMOTION_PLATFORM_DROP
                    : (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
            platform_drop_ticks =
                (uint8_t)fighter->platform_drop_ticks;
            position_y += fighter->platform_drop_nudge_q16;
            velocity_y = fighter->platform_drop_speed_y_q16;
            fast_fall = UINT8_C(0);
            dropped_platform_this_tick = 1;
        }
        else
        {
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
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
        const int crouch_dash_requested =
            strong_direction != INT8_C(0) &&
            tilt_x_age < fighter->dash_input_window_ticks;

        if (crouch_dash_requested != 0)
        {
            action_ticks = UINT16_C(1);
            dash_direction = strong_direction;
            if (strong_direction == facing)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                velocity_x = pf_m4_enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    strong_direction);
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
            }
        }
        else if (input->main_stick_y <
            (int16_t)fighter->crouch_release_axis_threshold)
        {
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
            action_state = (uint8_t)PF_M4_ACTION_CROUCH_END;
            action_ticks = UINT16_C(1);
        }
        else
        {
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
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
            velocity_x = pf_m4_apply_ground_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                fighter->walk_speed_q16,
                1);
        }
        else
        {
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
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
               action_state == (uint8_t)PF_M4_ACTION_INITIAL_DASH) &&
             input->main_stick_y >=
                 (int16_t)fighter->crouch_axis_threshold &&
             !(source_ground_input == NULL &&
               pf_m4_is_moonwalk_lower_sweep(
                   fighter,
                   action_state,
                   input->main_stick_y)))
    {
        if (action_state == (uint8_t)PF_M4_ACTION_RUN)
        {
            action_state = (uint8_t)PF_M4_ACTION_CROUCH_START;
            action_ticks = UINT16_C(1);
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
            dash_direction = INT8_C(0);
        }
        else
        {
            action_state = (uint8_t)PF_M4_ACTION_CROUCH_START;
            action_ticks = UINT16_C(1);
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                velocity_x > fighter->walk_speed_q16 ||
                        velocity_x < -fighter->walk_speed_q16
                    ? fighter->turn_acceleration_q16
                    : fighter->traction_q16);
            dash_direction = INT8_C(0);
        }
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
            strong_direction != INT8_C(0) &&
            tilt_x_age < fighter->dash_input_window_ticks;
        const int authored_moonwalk_enabled =
            source_ground_input == NULL;
        const int moonwalk_lower_sweep =
            authored_moonwalk_enabled != 0 &&
            pf_m4_is_moonwalk_lower_sweep(
                fighter,
                action_state,
                input->main_stick_y);
        const int moonwalk_lower_back =
            authored_moonwalk_enabled != 0 &&
            pf_m4_is_moonwalk_lower_back(
                fighter,
                action_state,
                facing,
                input->main_stick_x,
                input->main_stick_y);
        const int moonwalk_reduced_back =
            authored_moonwalk_enabled != 0 &&
            horizontal_direction == -facing &&
            horizontal_magnitude > fighter->axis_dead_zone &&
            strong_direction == INT8_C(0);
        const int moonwalk_setup_back =
            moonwalk_reduced_back != 0 ||
            moonwalk_lower_back != 0;
        const int moonwalk_full_back =
            authored_moonwalk_enabled != 0 &&
            strong_direction == -facing &&
            moonwalk_lower_back == 0;

        if (action_state == (uint8_t)PF_M4_ACTION_TEETER)
        {
            const pf_m4_falcon_submotion_data *teeter_motion =
                pf_m4_falcon_reference_submotion(source_submotion);

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
                if (strong_direction == facing)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                    action_ticks = UINT16_C(1);
                    dash_direction = strong_direction;
                    initial_dash_entered_this_tick = 1;
                    initial_dash_entry_motion_velocity_x = velocity_x;
                    velocity_x = pf_m4_enter_initial_dash_velocity(
                        fighter,
                        velocity_x,
                        strong_direction);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_STANDING_TURN;
                    action_ticks = UINT16_C(0);
                    dash_direction = strong_direction;
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
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->walk_speed_q16,
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
                dash_direction >= INT8_C(-1) &&
                dash_direction <= INT8_C(1);
            const int8_t target_direction =
                dash_direction < INT8_C(0)
                    ? INT8_C(-1)
                    : INT8_C(1);
            const int target_held =
                strong_direction == target_direction;

            if (smash_turn != 0)
            {
                facing = target_direction;
            }
            else if (action_ticks + UINT16_C(1) >=
                     fighter->standing_turn_facing_tick)
            {
                facing = target_direction;
            }
            if (smash_turn != 0 && target_held != 0)
            {
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                action_state =
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                action_ticks = UINT16_C(1);
                velocity_x = pf_m4_enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    target_direction);
            }
            else
            {
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
                ++action_ticks;
                if (action_ticks >= fighter->standing_turn_ticks)
                {
                    dash_direction = INT8_C(0);
                    if (ground_horizontal_direction == facing &&
                        horizontal_magnitude >= fighter->walk_axis_threshold)
                    {
                        action_state = (uint8_t)PF_M4_ACTION_WALK;
                        action_ticks = UINT16_C(1);
                        velocity_x = pf_m4_apply_ground_input(
                            fighter,
                            velocity_x,
                            input->main_stick_x,
                            fighter->walk_speed_q16,
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
            if (moonwalk_lower_sweep != 0 ||
                moonwalk_reduced_back != 0)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_MOONWALK_SETUP;
                if (moonwalk_lower_sweep != 0 &&
                    moonwalk_lower_back == 0)
                {
                    action_ticks = fighter->moonwalk_setup_ticks;
                    velocity_x = pf_m4_moonwalk_sweep_velocity(
                        fighter,
                        velocity_x,
                        input->main_stick_x);
                }
                else
                {
                    action_ticks = UINT16_C(1);
                    velocity_x = pf_m4_approach(
                        velocity_x,
                        -(int32_t)facing *
                            fighter->initial_dash_speed_q16,
                        fighter->turn_acceleration_q16);
                }
            }
            else if (fresh_dash_input != 0 &&
                     strong_direction == -dash_direction &&
                     (source_ground_input == NULL ||
                      reference_current_anim_frame >
                          initial_dash_early_end_frame))
            {
                dash_direction = strong_direction;
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                action_ticks = UINT16_C(1);
                velocity_x = pf_m4_approach(
                    pf_m4_multiply_q16(
                        velocity_x,
                        PF_Q16_ONE / INT32_C(4)),
                    INT32_C(0),
                    fighter->traction_q16);
            }
            else if (source_ground_input != NULL &&
                     fresh_dash_input != 0 &&
                     strong_direction == dash_direction &&
                     action_ticks > initial_dash_special_end_frame)
            {
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                action_ticks = UINT16_C(1);
                velocity_x = pf_m4_enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    strong_direction);
            }
            else
            {
                const int32_t velocity_before_ground_input = velocity_x;

                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
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
                             fighter->walk_speed_q16 ||
                         velocity_before_ground_input <
                             -fighter->walk_speed_q16))
                    {
                        /*
                         * Dash physics already applied one traction step.
                         * Wait selects its stronger friction from the velocity
                         * entering this frame, then applies the second step.
                         */
                        velocity_x = pf_m4_approach(
                            velocity_x,
                            INT32_C(0),
                            fighter->traction_q16);
                    }
                }
            }
        }
        else if (action_state ==
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP)
        {
            if (moonwalk_setup_back)
            {
                if (action_ticks < fighter->moonwalk_setup_ticks)
                {
                    ++action_ticks;
                }
                velocity_x = pf_m4_approach(
                    velocity_x,
                    -(int32_t)facing *
                        fighter->initial_dash_speed_q16,
                    fighter->turn_acceleration_q16);
            }
            else if (moonwalk_lower_sweep)
            {
                action_ticks = fighter->moonwalk_setup_ticks;
                velocity_x = pf_m4_moonwalk_sweep_velocity(
                    fighter,
                    velocity_x,
                    input->main_stick_x);
            }
            else if (moonwalk_full_back)
            {
                if (action_ticks >= fighter->moonwalk_setup_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_MOONWALK;
                    action_ticks = UINT16_C(1);
                    velocity_x =
                        -(int32_t)facing *
                        fighter->initial_dash_speed_q16;
                }
                else
                {
                    facing = (int8_t)-facing;
                    dash_direction = facing;
                    action_state =
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                    action_ticks = UINT16_C(1);
                    velocity_x =
                        (int32_t)facing *
                        fighter->initial_dash_speed_q16;
                }
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                dash_direction = INT8_C(0);
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_MOONWALK)
        {
            if (horizontal_direction == -facing &&
                horizontal_magnitude > fighter->axis_dead_zone)
            {
                velocity_x =
                    -(int32_t)facing *
                    fighter->initial_dash_speed_q16;
                ++action_ticks;
                if (action_ticks >= fighter->initial_dash_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    action_ticks = UINT16_C(0);
                    dash_direction = INT8_C(0);
                }
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                dash_direction = INT8_C(0);
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
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
                    velocity_x = pf_m4_apply_ground_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->run_speed_q16,
                        2);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    velocity_x = pf_m4_approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->traction_q16);
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
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
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
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
                    0);
            }
            else
            {
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
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
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
                    2);
            }
            else if (run_turnaround_requested)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
                action_ticks = UINT16_C(1);
                dash_direction = horizontal_direction;
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
                    0);
            }
            else if (!run_continues)
            {
                action_state = (uint8_t)PF_M4_ACTION_RUN_BRAKE;
                action_ticks = UINT16_C(1);
                dash_direction = INT8_C(0);
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
            }
            else
            {
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
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
            const int moonwalk_setup_started = 0;
            const int dash_started =
                fresh_dash_input != 0;

            if (pf_m4_action_is_ground_damage(action_state) &&
                dash_started == 0 &&
                horizontal_magnitude < fighter->walk_axis_threshold)
            {
                /* Damage IASA exposes the Wait table as soon as x0 reaches
                 * zero, but the damage animation remains active when no
                 * option is selected. Its Phys callback still applies the
                 * ordinary grounded friction channel. */
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->traction_q16);
                status = pf_m4_advance_ground_damage_animation(
                    &action_state,
                    &action_ticks,
                    scratch->hitstun_ticks[player_index],
                    &scratch
                         ->ground_knockback_velocity_q16[player_index]);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
            }
            else if (moonwalk_setup_started)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_MOONWALK_SETUP;
                if (moonwalk_lower_sweep != 0 &&
                    moonwalk_lower_back == 0)
                {
                    action_ticks = fighter->moonwalk_setup_ticks;
                    velocity_x = pf_m4_moonwalk_sweep_velocity(
                        fighter,
                        velocity_x,
                        input->main_stick_x);
                }
                else
                {
                    action_ticks = UINT16_C(1);
                    velocity_x = pf_m4_approach(
                        velocity_x,
                        -(int32_t)facing *
                            fighter->initial_dash_speed_q16,
                        fighter->turn_acceleration_q16);
                }
            }
            else if (dash_started)
            {
                action_ticks = UINT16_C(1);
                dash_direction = strong_direction;
                if (strong_direction == facing)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                    initial_dash_entered_this_tick = 1;
                    initial_dash_entry_motion_velocity_x = velocity_x;
                    velocity_x = pf_m4_enter_initial_dash_velocity(
                        fighter,
                        velocity_x,
                        strong_direction);
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_STANDING_TURN;
                    velocity_x = pf_m4_approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->traction_q16);
                }
            }
            else if (action_state ==
                         (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                     strong_direction == dash_direction &&
                     action_ticks <
                         fighter->dash_run_transition_ticks)
            {
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->run_speed_q16,
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
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    velocity_x > fighter->walk_speed_q16 ||
                            velocity_x < -fighter->walk_speed_q16
                        ? fighter->turn_acceleration_q16
                        : fighter->traction_q16);
            }
            else if (horizontal_magnitude >=
                     fighter->walk_axis_threshold)
            {
                int walk;

                facing = ground_horizontal_direction;
                dash_direction = INT8_C(0);
                if (source_ground_input == NULL &&
                    strong_direction != INT8_C(0) &&
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
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    walk != 0
                        ? fighter->walk_speed_q16
                        : fighter->run_speed_q16,
                    walk != 0 ? 1 : 2);
            }
            else
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                dash_direction = INT8_C(0);
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    velocity_x > fighter->walk_speed_q16 ||
                            velocity_x < -fighter->walk_speed_q16
                        ? fighter->turn_acceleration_q16
                        : fighter->traction_q16);
            }
        }
    }

    if (!ledge_motion_handled &&
        grounded == UINT8_C(0) &&
        pf_m4_action_is_surface_tech(action_state))
    {
        dash_direction = INT8_C(0);
        ++action_ticks;
        if (pf_m4_action_is_wall_tech(action_state))
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
                        fighter->wall_tech_jump_speed_x_q16;
                    velocity_y =
                        -fighter->wall_tech_jump_speed_y_q16;
                }
                else
                {
                    velocity_x =
                        (int32_t)scratch
                            ->tech_direction[player_index] *
                        fighter->wall_tech_speed_q16;
                    velocity_y = INT32_C(0);
                }
            }
            if (action_ticks >= fighter->wall_tech_stall_ticks)
            {
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    fighter->air_friction_q16);
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
                velocity_x = pf_m4_scale_axis_q16(
                    input->main_stick_x,
                    fighter->ceiling_tech_speed_q16);
            }
            velocity_x = pf_m4_apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                fighter->air_speed_q16);
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
            else if (pf_m4_action_is_surface_bounce(action_state))
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
            const pf_m4_falcon_submotion_data *shield_break_fly =
                pf_m4_falcon_reference_submotion(source_submotion);

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
                velocity_x = pf_m4_multiply_q16(
                    velocity_x,
                    fighter->air_dodge_decay_q16);
                velocity_y = pf_m4_multiply_q16(
                    velocity_y,
                    fighter->air_dodge_decay_q16);
            }
            else
            {
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_max_horizontal_speed_q16);
            }
        }
        else if (action_state ==
                 (uint8_t)PF_M4_ACTION_VECTOR_ASCENT)
        {
            velocity_x = pf_m4_apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                content->recovery.horizontal_speed_q16);
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
            const pf_m4_falcon_special_attributes *attributes =
                action_state ==
                        (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL
                    ? pf_m4_falcon_reference_special_attributes()
                    : NULL;
            const int32_t maximum_speed_q16 =
                attributes != NULL
                    ? pf_m4_multiply_q16(
                          fighter->air_max_horizontal_speed_q16,
                          attributes->specialhi_freefall_air_spd_mul_q16)
                    : fighter->fall_special_mobility_q16;

            if (pf_m4_action_uses_fall_special_pose(action_state))
            {
                const pf_m4_falcon_submotion_data *fall_special_motion =
                    pf_m4_falcon_reference_submotion(
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
            velocity_x = pf_m4_apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                maximum_speed_q16);
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_WALL_JUMP)
        {
            if (strong_attack_pressed != 0 ||
                light_attack_pressed != 0)
            {
                action_state = pf_m4_select_aerial_attack_action(
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
                pf_m4_enter_double_jump(
                    fighter,
                    input,
                    &velocity_x,
                    &velocity_y,
                    &air_jumps_remaining,
                    &fast_fall,
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
            const pf_m4_falcon_submotion_data *release_motion =
                pf_m4_falcon_reference_submotion(source_submotion);
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
                        fighter->grab_release_air_speed_x_q16;
                    velocity_y = -fighter->grab_release_air_speed_y_q16;
                }
                else
                {
                    velocity_x =
                        -(int32_t)facing *
                        fighter->grab_release_speed_x_q16;
                }
            }
            velocity_x = pf_m4_apply_air_input(
                fighter,
                velocity_x,
                input->main_stick_x,
                fighter->air_speed_q16);
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
            const pf_m4_falcon_submotion_data *motion =
                pf_m4_falcon_reference_submotion(source_submotion);

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
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
            }
            else
            {
                /* The transition tick already consumed CliffJump2's one
                 * deferred physics callback while installing frame one. */
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
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

                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
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
                const pf_m4_reference_move *move =
                    pf_m4_falcon_move_for_action(action_state);
                const pf_m4_falcon_special_attributes *attributes =
                    pf_m4_falcon_reference_special_attributes();

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
                        const pf_m4_falcon_common_attributes *common =
                            pf_m4_falcon_reference_common_attributes();

                        if (common == NULL)
                        {
                            return PF_STATUS_DETERMINISTIC_FAULT;
                        }
                        pf_m4_falcon_source_air_physics(
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
                    if (pf_m4_falcon_kick_root_velocity(
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
                        const int32_t hit_scale_q16 =
                            pf_m4_falcon_kick_hit_velocity_scale(
                                attributes,
                                scratch
                                    ->falcon_kick_hit_count[player_index]);

                        velocity_x = pf_m4_multiply_q16(
                            velocity_x,
                            hit_scale_q16);
                        velocity_y = pf_m4_multiply_q16(
                            velocity_y,
                            hit_scale_q16);
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
                const pf_m4_reference_move *move =
                    pf_m4_falcon_move_for_action(action_state);
                const pf_m4_falcon_common_attributes *common =
                    pf_m4_falcon_reference_common_attributes();
                const pf_m4_falcon_down_special_timing *timing =
                    pf_m4_falcon_reference_down_special_timing();
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
                    pf_m4_falcon_source_air_physics(
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
                        if (pf_m4_falcon_kick_root_velocity(
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
                        pf_m4_falcon_source_air_physics(
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
                const pf_m4_falcon_move_index move_index =
                    action_state ==
                            (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND
                        ? PF_M4_FALCON_UP_SPECIAL_GROUND
                        : PF_M4_FALCON_UP_SPECIAL_AIR;
                const pf_m4_reference_move *move =
                    pf_m4_falcon_reference_move(move_index);

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
                    const pf_m4_falcon_special_attributes *attributes =
                        pf_m4_falcon_reference_special_attributes();
                    const int32_t fall_maximum_q16 =
                        attributes != NULL
                            ? pf_m4_multiply_q16(
                                  fighter->air_max_horizontal_speed_q16,
                                  attributes
                                      ->specialhi_freefall_air_spd_mul_q16)
                            : INT32_C(0);

                    if (attributes == NULL ||
                        fall_maximum_q16 <= INT32_C(0))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    action_state =
                        (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL;
                    action_ticks = UINT16_C(0);
                    velocity_x = pf_m4_apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fall_maximum_q16);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    if (pf_m4_falcon_dive_start_velocity(
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
                const pf_m4_reference_move *move =
                    pf_m4_falcon_reference_move(
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
                    velocity_x = pf_m4_apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_q16);
                    scratch->attack_hit_mask[player_index] = UINT8_C(0);
                    scratch->attack_stale_registered[player_index] =
                        UINT8_C(0);
                }
                else
                {
                    if (pf_m4_falcon_dive_throw_velocity(
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
                const pf_m4_reference_move *move =
                    pf_m4_falcon_reference_move(
                        PF_M4_FALCON_NEUTRAL_SPECIAL_AIR);
                const pf_m4_falcon_special_attributes *attributes =
                    pf_m4_falcon_reference_special_attributes();
                const pf_m4_falcon_neutral_special_timing *timing =
                    pf_m4_falcon_reference_neutral_special_timing();
                const uint16_t displayed_frame =
                    action_ticks + UINT16_C(1);

                if (move == NULL || attributes == NULL || timing == NULL)
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                if (displayed_frame == timing->launch_frame)
                {
                    pf_m4_falcon_punch_launch_velocity(
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
                    velocity_x = pf_m4_multiply_q16(
                        velocity_x,
                        attributes->specialn_vel_mul_q16);
                    velocity_y = pf_m4_multiply_q16(
                        velocity_y,
                        attributes->specialn_vel_mul_q16);
                    launched_this_tick = 1;
                }
                else if (displayed_frame <
                         timing->ordinary_air_physics_begin_frame)
                {
                    velocity_x = pf_m4_approach(
                        velocity_x,
                        INT32_C(0),
                        fighter->air_friction_q16);
                }
                else
                {
                    velocity_x = pf_m4_apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_q16);
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
                const pf_m4_falcon_move_index move_index =
                    hit != 0
                        ? PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR
                        : PF_M4_FALCON_SIDE_SPECIAL_START_AIR;
                const pf_m4_reference_move *move =
                    pf_m4_falcon_reference_move(move_index);
                const pf_m4_falcon_special_attributes *attributes =
                    pf_m4_falcon_reference_special_attributes();
                const pf_m4_falcon_side_special_timing *timing =
                    pf_m4_falcon_reference_side_special_timing();
                int32_t reference_motion_x_q16;

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

                    if (!pf_m4_falcon_reference_motion_x_q16(
                            action_state,
                            displayed_frame,
                            &reference_motion_x_q16))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                    velocity_x =
                        (int32_t)facing * reference_motion_x_q16;
                    if (hit != 0 ||
                        displayed_frame >=
                            timing->air_gravity_begin_frame)
                    {
                        const int32_t gravity_q16 =
                            pf_m4_falcon_source_velocity_to_sim_q16(
                                attributes->specials_grav_q16,
                                INT32_C(11),
                                INT32_C(62));
                        const int32_t terminal_q16 =
                            pf_m4_falcon_source_velocity_to_sim_q16(
                                attributes->specials_terminal_vel_q16,
                                INT32_C(11),
                                INT32_C(62));

                        velocity_y = pf_m4_approach(
                            velocity_y,
                            terminal_q16,
                            gravity_q16);
                    }
                    ++action_ticks;
                    launched_this_tick = 1;
                }
                fast_fall = UINT8_C(0);
            }
            else if (action_state ==
                (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR)
            {
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
                ++action_ticks;
                if (action_ticks >=
                    content->projectile.fire_recovery_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                }
            }
            else if (pf_m4_action_is_light_aerial(action_state))
            {
                const uint32_t aerial_attack_ticks =
                    pf_m4_light_aerial_ticks(fighter, action_state);
                const int aerial_iasa_active =
                    pf_m4_falcon_aerial_reference_matches(
                        fighter,
                        action_state) &&
                    pf_m4_falcon_reference_iasa_active(
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
                    pf_m4_enter_double_jump(
                        fighter,
                        input,
                        &velocity_x,
                        &velocity_y,
                        &air_jumps_remaining,
                        &fast_fall,
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
                    velocity_x = pf_m4_apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_q16);
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
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
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
                input_shield_strength >=
                    fighter->digital_trigger_threshold &&
                (scratch->tumble[player_index] == UINT8_C(0) ||
                 pf_m4_action_is_damage(action_state)) &&
                damage_fall_wiggle_this_tick == 0)
            {
                status = pf_m4_enter_air_dodge(
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
                     (strong_attack_pressed != 0 ||
                      light_attack_pressed != 0))
            {
                if (double_jump_cancel_window != 0)
                {
                    velocity_y = INT32_C(0);
                }
                action_state = pf_m4_select_aerial_attack_action(
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
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
            }
            else
            {
                if (!launched_this_tick)
                {
                    velocity_x = pf_m4_apply_air_input(
                        fighter,
                        velocity_x,
                        input->main_stick_x,
                        fighter->air_speed_q16);
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
                        !pf_m4_advance_falcon_source_submotion(
                            &source_submotion,
                            &action_ticks))
                    {
                        return PF_STATUS_DETERMINISTIC_FAULT;
                    }
                }
                else if (action_state ==
                             (uint8_t)PF_M4_ACTION_HITSTUN &&
                         (uint32_t)action_ticks + UINT32_C(1) <
                             PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT)
                {
                    /* DamageFall's released IASA uses the ordinary Fall
                     * table, but a neutral sample leaves the current damage
                     * animation running until its final sourced frame. */
                    ++action_ticks;
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
                  pf_m4_action_is_damage(action_state))) &&
                !launched_this_tick &&
                released_ledge_this_tick == 0 &&
                (jump_pressed != 0 ||
                 damage_released_jump_requested != 0) &&
                air_jumps_remaining > UINT8_C(0))
            {
                pf_m4_enter_double_jump(
                    fighter,
                    input,
                    &velocity_x,
                    &velocity_y,
                    &air_jumps_remaining,
                    &fast_fall,
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
        int previous_exact_pose = 0;

        /* ftCommon_8007D5D4 always relocks the ECB for ten map updates,
         * including when JumpAerial resets an existing lock.  Melee keeps
         * CollData.desired_ecb.bottom from the preceding map update; derive
         * that same value from canonical source state instead of storing a
         * second copy of the complete desired ECB. */
        ecb_locked_bottom_y_q16 =
            previous_locked_bottom_y_q16 !=
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16
                ? previous_locked_bottom_y_q16
                : pf_m4_floor_contact_bottom_extent_q16(
                      fighter,
                      pf_m4_effective_action_state(
                          world->action_state[player_index],
                          world->hitlag_resume_action[player_index]),
                      world->action_ticks[player_index],
                      world->grounded[player_index],
                      PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                      world->source_submotion[player_index],
                      world->source_animation_frame_q16[player_index],
                      world->fall_animation_blend_q16[player_index],
                      world->fall_animation_target_switched[player_index],
                      world->prone_orientation[player_index],
                      world->prone_roll_motion_orientation[player_index],
                      world->tech_direction[player_index],
                      world->facing[player_index],
                      &previous_exact_pose);
        (void)previous_exact_pose;
        ecb_bottom_lock_ticks = PF_M4_COMMON_AIR_ENTRY_ECB_LOCK_TICKS;
        inherited_locked_bottom_y_q16 = ecb_locked_bottom_y_q16;
    }

    if (action_state != (uint8_t)PF_M4_ACTION_SHIELD &&
        action_state != (uint8_t)PF_M4_ACTION_SHIELD_STUN)
    {
        scratch->shield_health_q16[player_index] =
            pf_m4_shield_health_add(
                scratch->shield_health_q16[player_index],
                fighter->shield_regeneration_q16,
                fighter->shield_health_q16);
    }

    const uint8_t shield_recoil_bit =
        (uint8_t)(UINT8_C(1) << player_index);
    int32_t shield_recoil_x =
        (scratch->shield_recoil_mask & shield_recoil_bit) != UINT8_C(0)
            ? scratch->shield_recoil_x_q16[player_index]
            : INT32_C(0);

    if (shield_recoil_x != INT32_C(0))
    {
        const int32_t recoil_decay_q16 =
            grounded != UINT8_C(0)
                ? pf_m4_multiply_q16(
                      fighter->traction_q16,
                      fighter
                          ->shield_attacker_pushback_ground_friction_scale_q16)
                : fighter->shield_attacker_pushback_air_decay_q16;

        shield_recoil_x = pf_m4_approach(
            shield_recoil_x,
            INT32_C(0),
            recoil_decay_q16);
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
        grounded = pf_m4_down_bound_floor_contact(
            scratch->prone_orientation[player_index],
            action_ticks);
    }

    /* Fighter_procUpdate runs the action physics callback first, then decays
     * x8c_kb_vel, then adds self velocity and knockback to position. The
     * dedicated knockback channel remains distinct from ordinary velocity
     * throughout integration. */
    if (scratch->knockback_velocity_x_q16[player_index] != INT32_C(0) ||
        scratch->knockback_velocity_y_q16[player_index] != INT32_C(0) ||
        scratch->ground_knockback_velocity_q16[player_index] != INT32_C(0))
    {
        if (grounded != UINT8_C(0))
        {
            const int32_t ground_decay_q16 = pf_m4_multiply_q16(
                fighter->traction_q16,
                fighter->ground_knockback_decay_scale_q16);

            if (scratch->ground_knockback_velocity_q16[player_index] ==
                INT32_C(0))
            {
                scratch->ground_knockback_velocity_q16[player_index] =
                    scratch->knockback_velocity_x_q16[player_index];
            }
            scratch->ground_knockback_velocity_q16[player_index] =
                pf_m4_approach(
                    scratch->ground_knockback_velocity_q16[player_index],
                    INT32_C(0),
                    ground_decay_q16);
            pf_m4_project_ground_scalar_q16(
                content,
                support,
                scratch->ground_knockback_velocity_q16[player_index],
                &scratch->knockback_velocity_x_q16[player_index],
                &scratch->knockback_velocity_y_q16[player_index]);
        }
        else
        {
            scratch->ground_knockback_velocity_q16[player_index] =
                INT32_C(0);
            status = pf_m4_ssbm_decay_air_knockback_q16(
                fighter->air_knockback_decay_q16,
                &scratch->knockback_velocity_x_q16[player_index],
                &scratch->knockback_velocity_y_q16[player_index]);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
    }

    integrated_self_x_q16 =
        initial_dash_entered_this_tick != 0
            ? initial_dash_entry_motion_velocity_x
            : velocity_x;
    integrated_self_y_q16 = velocity_y;
    integrated_animation_x_q16 = animation_motion_x_q16;
    integrated_animation_y_q16 = INT32_C(0);
    if (grounded != UINT8_C(0) &&
        stage->reference_collision_profile !=
            (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        pf_m4_project_ground_scalar_q16(
            content,
            support,
            integrated_self_x_q16,
            &integrated_self_x_q16,
            &integrated_self_y_q16);
        pf_m4_project_ground_scalar_q16(
            content,
            support,
            animation_motion_x_q16,
            &integrated_animation_x_q16,
            &integrated_animation_y_q16);
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        pf_m4_action_uses_fall_special_pose(
            pf_m4_effective_action_state(
                action_state,
                scratch->hitlag_resume_action[player_index])))
    {
        status = pf_m4_update_falcon_fall_animation_clock(
            fighter,
            previous_action_state,
            previous_hitlag_resume_action,
            previous_source_submotion,
            previous_source_animation_frame_q16,
            previous_source_animation_rate_q16,
            previous_fall_animation_blend_q16,
            previous_fall_animation_target_switched,
            previous_ground_velocity_q16,
            previous_facing,
            action_state,
            scratch->hitlag_resume_action[player_index],
            &source_submotion,
            &source_animation_frame_q16,
            &source_animation_rate_q16,
            &fall_animation_blend_q16,
            &fall_animation_target_switched);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        pf_m4_falcon_reference_direct_hsd_pose(
            action_state,
            action_ticks,
            grounded,
            &source_submotion,
            &source_animation_frame_q16))
    {
        source_animation_rate_q16 = (int32_t)PF_Q16_ONE;
    }

    previous_position_x = position_x;
    next_position =
        (int64_t)position_x +
        (int64_t)player_nudge_x_q16 +
        (int64_t)integrated_self_x_q16 +
        (int64_t)scratch->knockback_velocity_x_q16[player_index] +
        (int64_t)shield_recoil_x +
        (int64_t)integrated_animation_x_q16;
    if (!ledge_motion_handled &&
        !pf_m4_checked_i32(next_position, &position_x))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    if (!ledge_motion_handled)
    {
        const int64_t future_y =
            (int64_t)position_y + (int64_t)integrated_self_y_q16 +
            (int64_t)scratch->knockback_velocity_y_q16[player_index] +
            (int64_t)integrated_animation_y_q16;
        const int64_t swept_center_top =
            future_y < (int64_t)position_y
                ? future_y
                : (int64_t)position_y;
        const int64_t swept_center_bottom =
            future_y > (int64_t)position_y
                ? future_y
                : (int64_t)position_y;
        const int64_t body_top =
            swept_center_top - fighter->half_height_q16;
        const int64_t body_bottom =
            swept_center_bottom + fighter->half_height_q16;
        int64_t wall_swept_top = body_top;
        int64_t wall_swept_bottom = body_bottom;
        int32_t wall_side_x_extent_q16 = fighter->half_width_q16;
        pf_m4_falcon_ecb_pose_q16 wall_pose;
        int32_t wall_side_x_from_origin_q16 = fighter->half_width_q16;
        int32_t wall_side_y_from_origin_q16 = INT32_C(0);
        int32_t wall_impact_self_velocity_y_q16 = velocity_y;
        int64_t exact_wall_future_y = future_y;
        pf_m4_hsd_compact_pose wall_blend_pose;
        int32_t wall_blend_progress_q16 = INT32_C(0);
        const pf_m4_hsd_compact_pose *wall_blend_pose_or_null = NULL;
        int exact_reference_wall_pose = 0;
        int exact_wall_contact = 0;
        const int moving_right = position_x > previous_position_x;
        const int vertical_overlap =
            body_bottom > (int64_t)stage->solid_top_q16 &&
            body_top < (int64_t)stage->solid_bottom_q16;
        int8_t away_direction = INT8_C(0);
        uint8_t wall_support = UINT8_C(0);

        if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
            pf_m4_falcon_reference_hsd_ecb_pose(
                source_submotion,
                source_animation_frame_q16,
                grounded != UINT8_C(0),
                PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                &wall_pose) != 0)
        {
            status = pf_m4_evaluate_falcon_ground_blend_pose(
                world,
                player_index,
                action_state,
                scratch->hitlag_resume_action[player_index],
                source_submotion,
                source_animation_frame_q16,
                &wall_blend_pose,
                &wall_blend_progress_q16);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
            if (wall_blend_progress_q16 > INT32_C(0))
            {
                wall_blend_pose_or_null = &wall_blend_pose;
            }
        }
        exact_reference_wall_pose = pf_m4_reference_ecb_pose_q16(
            fighter,
            pf_m4_effective_action_state(
                action_state,
                scratch->hitlag_resume_action[player_index]),
            action_ticks,
            grounded,
            inherited_locked_bottom_y_q16,
            source_submotion,
            source_animation_frame_q16,
            fall_animation_blend_q16,
            fall_animation_target_switched,
            scratch->prone_orientation[player_index],
            scratch->prone_roll_motion_orientation[player_index],
            scratch->tech_direction[player_index],
            facing,
            wall_blend_progress_q16,
            wall_blend_pose_or_null,
            &wall_pose);
        if (exact_reference_wall_pose != 0)
        {
            pf_m4_ecb_world_wall_side_q16(
                &wall_pose,
                facing,
                moving_right,
                &wall_side_x_from_origin_q16,
                &wall_side_y_from_origin_q16);
            wall_side_x_extent_q16 =
                wall_side_x_from_origin_q16 >= INT32_C(0)
                    ? wall_side_x_from_origin_q16
                    : -wall_side_x_from_origin_q16;
            wall_impact_self_velocity_y_q16 =
                fast_fall != UINT8_C(0)
                    ? fighter->fast_fall_speed_q16
                    : pf_m4_approach(
                          velocity_y,
                          fighter->fall_speed_q16,
                          fighter->gravity_q16);
            exact_wall_future_y =
                (int64_t)position_y +
                wall_impact_self_velocity_y_q16 +
                scratch->knockback_velocity_y_q16[player_index];
            const int64_t previous_side_y =
                (int64_t)position_y -
                (int64_t)wall_side_y_from_origin_q16;
            const int64_t future_side_y =
                exact_wall_future_y -
                (int64_t)wall_side_y_from_origin_q16;

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
            if (exact_reference_wall_pose != 0 &&
                position_x != previous_position_x)
            {
                pf_m4_falcon_ecb_pose_q16 previous_wall_pose;
                int32_t previous_side_x_from_origin_q16 = INT32_C(0);
                int32_t previous_side_y_from_origin_q16 = INT32_C(0);
                int32_t future_position_y_q16 = INT32_C(0);
                uint32_t contact_fraction_q16 = UINT32_C(0);

                if (!pf_m4_reference_ecb_pose_q16(
                    fighter,
                    pf_m4_effective_action_state(
                        world->action_state[player_index],
                        world->hitlag_resume_action[player_index]),
                    world->action_ticks[player_index],
                    world->grounded[player_index],
                    previous_locked_bottom_y_q16,
                    world->source_submotion[player_index],
                    world->source_animation_frame_q16[player_index],
                    world->fall_animation_blend_q16[player_index],
                    world->fall_animation_target_switched[player_index],
                    world->prone_orientation[player_index],
                    world->prone_roll_motion_orientation[player_index],
                    world->tech_direction[player_index],
                    world->facing[player_index],
                    world->ground_blend_progress_q16[player_index],
                    world->ground_blend_progress_q16[player_index] >
                            INT32_C(0)
                        ? &world->ground_blend_pose[player_index]
                        : NULL,
                    &previous_wall_pose))
                {
                    previous_wall_pose = wall_pose;
                }
                pf_m4_ecb_world_wall_side_q16(
                    &previous_wall_pose,
                    facing,
                    moving_right,
                    &previous_side_x_from_origin_q16,
                    &previous_side_y_from_origin_q16);
                if (!pf_m4_checked_i32(
                        exact_wall_future_y,
                        &future_position_y_q16))
                {
                    return PF_STATUS_DETERMINISTIC_FAULT;
                }
                exact_wall_contact =
                    pf_m4_ssbm_reference_stage_find_wall_point_contact(
                        stage->reference_collision_profile,
                        previous_position_x +
                            previous_side_x_from_origin_q16,
                        position_y - previous_side_y_from_origin_q16,
                        position_x + wall_side_x_from_origin_q16,
                        future_position_y_q16 -
                            wall_side_y_from_origin_q16,
                        &contact_fraction_q16,
                        &wall_support,
                        &away_direction);
                if (exact_wall_contact != 0)
                {
                    const int64_t root_delta_x_q16 =
                        (int64_t)position_x - previous_position_x;
                    const int64_t root_delta_y_q16 =
                        exact_wall_future_y - position_y;

                    position_x = previous_position_x + (int32_t)(
                        root_delta_x_q16 * contact_fraction_q16 /
                        INT64_C(65536));
                    exact_wall_contact_position_y_q16 =
                        position_y + (int32_t)(
                            root_delta_y_q16 * contact_fraction_q16 /
                            INT64_C(65536));
                }
            }
            if (exact_wall_contact == 0)
            {
                (void)pf_m4_ssbm_reference_stage_find_wall_contact(
                    stage->reference_collision_profile,
                    previous_position_x,
                    position_x,
                    wall_swept_top,
                    wall_swept_bottom,
                    wall_side_x_extent_q16,
                    &position_x,
                    &wall_support,
                    &away_direction);
            }
        }
        else if (vertical_overlap &&
                 (int64_t)previous_position_x +
                         fighter->half_width_q16 <=
                     (int64_t)stage->solid_left_q16 &&
                 (int64_t)position_x + fighter->half_width_q16 >=
                     (int64_t)stage->solid_left_q16)
        {
            position_x =
                stage->solid_left_q16 - fighter->half_width_q16;
            away_direction = INT8_C(-1);
        }
        else if (
            vertical_overlap &&
            (int64_t)previous_position_x -
                    fighter->half_width_q16 >=
                (int64_t)stage->solid_right_q16 &&
            (int64_t)position_x - fighter->half_width_q16 <=
                (int64_t)stage->solid_right_q16)
        {
            position_x =
                stage->solid_right_q16 + fighter->half_width_q16;
            away_direction = INT8_C(1);
        }

        (void)wall_support;

        if (away_direction != INT8_C(0))
        {
            const pf_m4_falcon_down_special_timing *kick_timing =
                pf_m4_action_is_falcon_kick(action_state) != 0
                    ? pf_m4_falcon_reference_down_special_timing()
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
                (!pf_m4_action_is_surface_bounce(action_state) ||
                 action_ticks >=
                     fighter->surface_bounce_collision_lockout_ticks) &&
                (scratch->knockback_velocity_x_q16[player_index] >
                     fighter->surface_collision_threshold_x_q16 ||
                 scratch->knockback_velocity_x_q16[player_index] <
                     -fighter->surface_collision_threshold_x_q16))
            {
                const pf_m4_ssbm_stage_collision_line *wall_line =
                    wall_support != UINT8_C(0)
                        ? pf_m4_ssbm_reference_stage_line(
                              stage->reference_collision_profile,
                              wall_support)
                        : NULL;
                const int32_t source_normal_x_q16 =
                    wall_line != NULL
                        ? wall_line->source_normal_x_q16
                        : (int32_t)away_direction * PF_Q16_ONE;
                const int32_t source_normal_y_q16 =
                    wall_line != NULL
                        ? wall_line->source_normal_y_q16
                        : INT32_C(0);
                const int up_held =
                    input->main_stick_y <=
                    -(int16_t)fighter->crouch_axis_threshold;

                if (exact_wall_contact != 0)
                {
                    velocity_y = wall_impact_self_velocity_y_q16;
                }

                status = pf_m4_enter_wall_impact(
                    fighter,
                    jump_pressed || up_held,
                    away_direction,
                    source_normal_x_q16,
                    source_normal_y_q16,
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
                    strong_direction == away_direction &&
                    strong_direction != previous_strong_direction;

                if (wall_jump_requested != 0)
                {
                    pf_m4_enter_wall_jump(
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
                else
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
        int32_t surface_left;
        int32_t surface_right;
        int retains_surface;

        pf_m4_surface_bounds_q16(
            content,
            support,
            world->tick + UINT64_C(1),
            &surface_left,
            &surface_right);
        retains_surface =
            position_x >= surface_left && position_x <= surface_right;
        if (retains_surface == 0 &&
            action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN &&
            grounded == UINT8_C(0) &&
            action_ticks > UINT16_C(0) &&
            pf_m4_down_bound_floor_contact(
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
        if (support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
        {
            retains_surface =
                pf_m4_body_overlaps_horizontal_interval(
                    position_x,
                    fighter->half_width_q16,
                    surface_left,
                    surface_right);
        }
        if (horizontal_magnitude <= fighter->axis_dead_zone &&
            pf_m4_action_can_enter_teeter(action_state) != 0 &&
            position_x < surface_left &&
            facing == INT8_C(-1) &&
            previous_position_x >= surface_left &&
            (int64_t)surface_left - (int64_t)position_x <=
                (int64_t)fighter->teeter_snap_distance_q16)
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
            pf_m4_action_can_enter_teeter(action_state) != 0 &&
            position_x > surface_right &&
            facing == INT8_C(1) &&
            previous_position_x <= surface_right &&
            (int64_t)position_x - (int64_t)surface_right <=
                (int64_t)fighter->teeter_snap_distance_q16)
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
                pf_m4_action_is_shield_break(action_state);
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
            const pf_m4_falcon_side_special_timing *raptor_timing =
                (raptor_boost_start_fall != 0 ||
                 raptor_boost_hit_fall != 0)
                    ? pf_m4_falcon_reference_side_special_timing()
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
                velocity_x = pf_m4_clamp_i32(
                    velocity_x,
                    -fighter->air_speed_q16,
                    fighter->air_speed_q16);
                launched_this_tick = 1;
            }

            if (stage->reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
            {
                pf_m4_project_ground_scalar_q16(
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
        }
        else
        {
            position_y =
                pf_m4_surface_y_q16(content, support, position_x) -
                fighter->half_height_q16;
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
        const int32_t previous_bottom =
            position_y + fighter->half_height_q16;
        int previous_exact_floor_contact_pose = 0;
        const int32_t previous_floor_contact_bottom_extent_q16 =
            pf_m4_floor_contact_bottom_extent_q16(
                fighter,
                pf_m4_effective_action_state(
                    world->action_state[player_index],
                    world->hitlag_resume_action[player_index]),
                world->action_ticks[player_index],
                world->grounded[player_index],
                previous_locked_bottom_y_q16,
                world->source_submotion[player_index],
                world->source_animation_frame_q16[player_index],
                world->fall_animation_blend_q16[player_index],
                world->fall_animation_target_switched[player_index],
                world->prone_orientation[player_index],
                world->prone_roll_motion_orientation[player_index],
                world->tech_direction[player_index],
                world->facing[player_index],
                &previous_exact_floor_contact_pose);
        int exact_floor_contact_pose = 0;
        int32_t ceiling_top_extent_q16 = fighter->half_height_q16;
        pf_m4_falcon_ecb_pose_q16 ceiling_pose;
        const int32_t floor_contact_bottom_extent_q16 =
            pf_m4_floor_contact_bottom_extent_q16(
                fighter,
                pf_m4_effective_action_state(
                    action_state,
                    scratch->hitlag_resume_action[player_index]),
                action_ticks,
                grounded,
                inherited_locked_bottom_y_q16,
                source_submotion,
                source_animation_frame_q16,
                fall_animation_blend_q16,
                fall_animation_target_switched,
                scratch->prone_orientation[player_index],
                scratch->prone_roll_motion_orientation[player_index],
                scratch->tech_direction[player_index],
                facing,
                &exact_floor_contact_pose);
        const pf_m4_pass_through_floor_sweep_policy
            pass_through_floor_sweep_policy =
                exact_floor_contact_pose == 0
                    ? PF_M4_PASS_THROUGH_FLOOR_SWEEP_DEFERRED
                    : (pf_m4_action_is_light_aerial(action_state) ||
                               action_state == (uint8_t)
                                                   PF_M4_ACTION_STRONG_AERIAL_ATTACK
                           ? PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT_OR_DEFERRED
                           : PF_M4_PASS_THROUGH_FLOOR_SWEEP_DIRECT);
        const int32_t previous_floor_contact =
            world->position_y_q16[player_index] +
            previous_floor_contact_bottom_extent_q16;
        if (pf_m4_reference_ecb_pose_q16(
                fighter,
                pf_m4_effective_action_state(
                    action_state,
                    scratch->hitlag_resume_action[player_index]),
                action_ticks,
                grounded,
                inherited_locked_bottom_y_q16,
                source_submotion,
                source_animation_frame_q16,
                fall_animation_blend_q16,
                fall_animation_target_switched,
                scratch->prone_orientation[player_index],
                scratch->prone_roll_motion_orientation[player_index],
                scratch->tech_direction[player_index],
                facing,
                INT32_C(0),
                NULL,
                &ceiling_pose) != 0)
        {
            ceiling_top_extent_q16 = ceiling_pose.top_y_from_origin_q16;
        }
        const int32_t previous_top =
            position_y - ceiling_top_extent_q16;
        const int wall_tech_stalled =
            pf_m4_action_is_wall_tech(action_state) &&
            action_ticks < fighter->wall_tech_stall_ticks;
        int32_t new_bottom;
        int32_t new_floor_contact;
        int32_t new_top;

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
        else if (launched_this_tick)
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
            pf_m4_action_allows_fresh_fast_fall(
                action_state,
                action_ticks) != 0 &&
            !pf_m4_action_is_surface_tech(action_state) &&
            input->main_stick_y >=
                (int16_t)fighter->fast_fall_axis_threshold &&
            tilt_y_direction == INT8_C(1) &&
            tilt_y_age < fighter->fast_fall_input_window_ticks &&
            velocity_y > INT32_C(0))
        {
            velocity_y = fighter->fast_fall_speed_q16;
            fast_fall = UINT8_C(1);
        }
        else if (fast_fall != UINT8_C(0))
        {
            velocity_y = fighter->fast_fall_speed_q16;
        }
        else
        {
            velocity_y = pf_m4_approach(
                velocity_y,
                fighter->fall_speed_q16,
                fighter->gravity_q16);
        }

        next_position =
            exact_wall_response_this_tick != 0
                ? (int64_t)exact_wall_contact_position_y_q16
                : (int64_t)position_y +
                      (wall_tech_stalled
                           ? INT64_C(0)
                           : (int64_t)velocity_y +
                                 (int64_t)scratch
                                     ->knockback_velocity_y_q16[player_index]);
        if (!pf_m4_checked_i32(next_position, &position_y))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        new_bottom = position_y + fighter->half_height_q16;
        new_floor_contact =
            position_y + floor_contact_bottom_extent_q16;
        new_top = position_y - ceiling_top_extent_q16;

        if (pf_m4_total_velocity_q16(
                velocity_y,
                scratch->knockback_velocity_y_q16[player_index]) >=
                INT32_C(0) &&
            !(launched_this_tick != 0 &&
              action_state ==
                  (uint8_t)PF_M4_ACTION_FALCON_KICK_WALL_REBOUND))
        {
            const int32_t platform_center =
                pf_m4_platform_center_x_q16(
                    stage,
                    world->tick + UINT64_C(1));
            const int32_t platform_left =
                platform_center - stage->platform_half_width_q16;
            const int32_t platform_right =
                platform_center + stage->platform_half_width_q16;
            const int32_t upper_platform_left =
                stage->upper_platform_center_x_q16 -
                stage->upper_platform_half_width_q16;
            const int32_t upper_platform_right =
                stage->upper_platform_center_x_q16 +
                stage->upper_platform_half_width_q16;
            const int down_held =
                input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;
            const int pass_through_allowed =
                !down_held ||
                action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE ||
                action_state ==
                    (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
            int32_t landing_y_q16 = INT32_MAX;
            uint8_t landing_support =
                (uint8_t)PF_M4_SURFACE_NONE;

            if (stage->reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
            {
                (void)pf_m4_reference_stage_find_floor_landing(
                    content,
                    position_x,
                    previous_floor_contact,
                    new_floor_contact,
                    fast_fall,
                    pass_through_floor_sweep_policy,
                    pass_through_allowed,
                    platform_drop_ticks,
                    &landing_y_q16,
                    &landing_support);
            }
            else
            {
                if (pf_m4_body_overlaps_horizontal_interval(
                        position_x,
                        fighter->half_width_q16,
                        stage->solid_left_q16,
                        stage->solid_right_q16) &&
                    previous_bottom <= stage->solid_top_q16 &&
                    new_bottom >= stage->solid_top_q16)
                {
                    landing_y_q16 = stage->solid_top_q16;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_SOLID_TOP;
                }
                if (pass_through_allowed != 0 &&
                    platform_drop_ticks == UINT8_C(0) &&
                    position_x >= platform_left &&
                    position_x <= platform_right &&
                    pf_m4_floor_sweep_crosses_surface(
                        previous_bottom,
                        new_bottom,
                        stage->platform_y_q16,
                        1,
                        fast_fall,
                        pass_through_floor_sweep_policy) &&
                    stage->platform_y_q16 < landing_y_q16)
                {
                    landing_y_q16 = stage->platform_y_q16;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_PLATFORM;
                }
                if (pass_through_allowed != 0 &&
                    platform_drop_ticks == UINT8_C(0) &&
                    position_x >= upper_platform_left &&
                    position_x <= upper_platform_right &&
                    pf_m4_floor_sweep_crosses_surface(
                        previous_bottom,
                        new_bottom,
                        stage->upper_platform_y_q16,
                        1,
                        fast_fall,
                        pass_through_floor_sweep_policy) &&
                    stage->upper_platform_y_q16 < landing_y_q16)
                {
                    landing_y_q16 = stage->upper_platform_y_q16;
                    landing_support =
                        (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
                }
                if (position_x >= stage->floor_left_q16 &&
                    position_x <= stage->floor_right_q16 &&
                    pf_m4_floor_sweep_crosses_surface(
                        previous_floor_contact,
                        new_floor_contact,
                        stage->floor_y_q16,
                        0,
                        fast_fall,
                        pass_through_floor_sweep_policy) &&
                    stage->floor_y_q16 < landing_y_q16)
                {
                    landing_y_q16 = stage->floor_y_q16;
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

                pf_m4_land_from_air(
                    content,
                    landing_y_q16,
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
            int32_t ceiling_y_q16 = INT32_C(0);
            uint8_t ceiling_support = UINT8_C(0);
            const int hit_ceiling =
                exact_wall_response_this_tick != 0
                    ? 0
                    : stage->reference_collision_profile !=
                        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED
                    ? pf_m4_ssbm_reference_stage_find_ceiling_contact(
                          stage->reference_collision_profile,
                          position_x,
                          previous_top,
                          new_top,
                          &ceiling_y_q16,
                          &ceiling_support)
                    : pf_m4_body_overlaps_horizontal_interval(
                          position_x,
                          fighter->half_width_q16,
                          stage->solid_left_q16,
                          stage->solid_right_q16) &&
                          previous_top >= stage->solid_bottom_q16 &&
                          new_top <= stage->solid_bottom_q16;

            (void)ceiling_support;
            if (hit_ceiling != 0)
            {
                if (stage->reference_collision_profile ==
                    (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
                {
                    ceiling_y_q16 = stage->solid_bottom_q16;
                }
                position_y = ceiling_y_q16 + ceiling_top_extent_q16;
                if (scratch->tumble[player_index] != UINT8_C(0))
                {
                    if ((!pf_m4_action_is_surface_bounce(action_state) ||
                         action_ticks >= fighter
                                             ->surface_bounce_collision_lockout_ticks) &&
                        scratch->knockback_velocity_y_q16[player_index] <
                            -fighter->surface_collision_threshold_y_q16)
                    {
                        const pf_m4_ssbm_stage_collision_line *ceiling_line =
                            ceiling_support != UINT8_C(0)
                                ? pf_m4_ssbm_reference_stage_line(
                                      stage->reference_collision_profile,
                                      ceiling_support)
                                : NULL;
                        const int32_t source_normal_x_q16 =
                            ceiling_line != NULL
                                ? ceiling_line->source_normal_x_q16
                                : INT32_C(0);
                        const int32_t source_normal_y_q16 =
                            ceiling_line != NULL
                                ? ceiling_line->source_normal_y_q16
                                : PF_Q16_ONE;

                        status = pf_m4_enter_ceiling_impact(
                            fighter,
                            source_normal_x_q16,
                            source_normal_y_q16,
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
        !pf_m4_action_is_surface_tech(action_state))
    {
        if (scratch->hitstun_ticks[player_index] > UINT16_C(0))
        {
            --scratch->hitstun_ticks[player_index];
        }
        if (grounded != UINT8_C(0) &&
            pf_m4_action_is_ground_damage(action_state))
        {
            status = pf_m4_advance_ground_damage_animation(
                &action_state,
                &action_ticks,
                scratch->hitstun_ticks[player_index],
                &scratch
                     ->ground_knockback_velocity_q16[player_index]);
            if (status != PF_STATUS_OK)
            {
                return status;
            }
        }
        else if (scratch->hitstun_ticks[player_index] == UINT16_C(0) &&
            action_state == (uint8_t)PF_M4_ACTION_HITSTUN &&
            (grounded != UINT8_C(0) ||
             action_ticks >=
                 PF_M4_FALCON_DAMAGE_FLY_ECB_FRAME_COUNT))
        {
            action_state =
                grounded != UINT8_C(0)
                    ? (uint8_t)PF_M4_ACTION_LANDING
                    : (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
        }
    }

    {
        const int8_t ledge_probe_direction =
            pf_m4_ledge_probe_direction(
                action_state,
                action_ticks,
                facing);

        if (!ledge_motion_handled &&
            !released_ledge_this_tick &&
            grounded == UINT8_C(0) &&
            ledge_probe_direction !=
                (int8_t)PF_M4_LEDGE_PROBE_NONE &&
            platform_drop_ticks == UINT8_C(0))
        {
            if (pf_m4_try_grab_ledge(
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
                    ledge_probe_direction,
                    &facing,
                    input->main_stick_y,
                    reference_ledge_response,
                    &dash_direction))
            {
                source_submotion =
                    fighter->reference_frame_data_enabled != UINT8_C(0)
                        ? (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CATCH
                        : (uint16_t)PF_M4_FALCON_SUBMOTION_WAIT;
                scratch->knockback_velocity_x_q16[player_index] =
                    INT32_C(0);
                scratch->knockback_velocity_y_q16[player_index] =
                    INT32_C(0);
                scratch->ground_knockback_velocity_q16[player_index] =
                    INT32_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
                scratch->tech_direction[player_index] = INT8_C(0);
                recovery_available = UINT8_C(1);
            }
        }
    }

    if (position_x < stage->blast_left_q16 ||
        position_x > stage->blast_right_q16 ||
        position_y < stage->blast_top_q16 ||
        position_y > stage->blast_bottom_q16)
    {
        const uint32_t ko_damage_q16 =
            scratch->damage_q16[player_index];
        const int32_t ko_velocity_x_q16 = velocity_x;
        const int32_t ko_velocity_y_q16 = velocity_y;
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

        pf_m4_prepare_spawn(
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
            ko_damage_q16,
            ko_velocity_x_q16,
            ko_velocity_y_q16,
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
        previous_strong_direction = strong_direction;
    }

    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
        world->action_state[player_index] !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        if (world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
        {
            source_submotion =
                pf_m4_falcon_jump_submotion(input, facing, 0);
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
        !pf_m4_action_is_smash_charge(action_state) &&
        !pf_m4_action_is_smash_release(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          pf_m4_action_is_smash_release(
              scratch->hitlag_resume_action[player_index])))
    {
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
    }

    if (!pf_m4_action_is_reference_jab_chain(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          pf_m4_action_is_reference_jab_chain(
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

    if (!pf_m4_action_is_damage(action_state) &&
        !pf_m4_action_is_surface_bounce(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          (pf_m4_action_is_damage(
               scratch->hitlag_resume_action[player_index]) ||
           pf_m4_action_is_surface_bounce(
               scratch->hitlag_resume_action[player_index]))))
    {
        scratch->damage_jump_buffer_ticks[player_index] = UINT16_C(0);
    }

    if (!pf_m4_action_retains_shield_strength(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        scratch->shield_strength[player_index] = UINT16_C(0);
    }

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        pf_m4_action_uses_fall_special_pose(
            pf_m4_effective_action_state(
                action_state,
                scratch->hitlag_resume_action[player_index])))
    {
        /* The common Fall animation callback runs before physics so its
         * clock and direction blend were already advanced above. */
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_action_uses_ground_animation_clock(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        status = pf_m4_update_falcon_ground_animation_clock(
            fighter,
            rng_state,
            previous_action_state,
            previous_hitlag_resume_action,
            previous_source_submotion,
            previous_source_animation_frame_q16,
            previous_source_animation_rate_q16,
            previous_ground_velocity_q16,
            previous_facing,
            action_state,
            scratch->hitlag_resume_action[player_index],
            &source_submotion,
            &source_animation_frame_q16,
            &source_animation_rate_q16);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_SHIELD_STUN)
    {
        const uint8_t previous_effective_action =
            pf_m4_effective_action_state(
                previous_action_state,
                previous_hitlag_resume_action);

        if (previous_effective_action !=
                (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
            previous_source_animation_rate_q16 <= INT32_C(0))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        source_animation_frame_q16 = previous_source_animation_frame_q16;
        source_animation_rate_q16 = previous_source_animation_rate_q16;
        if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
        {
            /* The new GuardSetOff motion and rate exist during hitlag, but
             * display/collision bones remain on the receiving guard pose. */
            source_submotion = previous_source_submotion;
        }
        else
        {
            const int64_t next_frame_q16 =
                (int64_t)source_animation_frame_q16 +
                (int64_t)source_animation_rate_q16;

            if (next_frame_q16 > (int64_t)INT32_MAX)
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            source_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF;
            source_animation_frame_q16 = (int32_t)next_frame_q16;
        }
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        const uint8_t previous_effective_action =
            pf_m4_effective_action_state(
                previous_action_state,
                previous_hitlag_resume_action);

        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA;
        if (action_state == (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_q16 =
                previous_source_animation_frame_q16;
        }
        else if (previous_effective_action ==
                 (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            if (!pf_m4_falcon_advance_loop_animation_q16(
                    source_submotion,
                    previous_source_animation_frame_q16,
                    (int32_t)PF_Q16_ONE,
                    &source_animation_frame_q16))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
        }
        else
        {
            source_animation_frame_q16 = INT32_C(0);
        }
        source_animation_rate_q16 = (int32_t)PF_Q16_ONE;
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
            source_animation_frame_q16 = INT32_C(0);
        }
        else if (!pf_m4_falcon_advance_loop_animation_q16(
                     source_submotion,
                     previous_source_animation_frame_q16,
                     (int32_t)PF_Q16_ONE,
                     &source_animation_frame_q16))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        source_animation_rate_q16 = (int32_t)PF_Q16_ONE;
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_effective_action_state(
                 action_state,
                 scratch->hitlag_resume_action[player_index]) ==
                 (uint8_t)PF_M4_ACTION_CROUCH_END)
    {
        source_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE;
        if (action_state != (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_q16 =
                (int32_t)(action_ticks - UINT16_C(1)) * PF_Q16_ONE;
        }
        source_animation_rate_q16 = PF_Q16_ONE;
    }
    else if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
             pf_m4_action_uses_direct_hsd_pose(
                 pf_m4_effective_action_state(
                     action_state,
                     scratch->hitlag_resume_action[player_index])))
    {
        const uint8_t effective_action = pf_m4_effective_action_state(
            action_state,
            scratch->hitlag_resume_action[player_index]);
        int32_t direct_frame_q16 = INT32_C(0);

        if (!pf_m4_falcon_reference_direct_hsd_pose(
                effective_action,
                action_ticks,
                grounded,
                &source_submotion,
                &direct_frame_q16))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        if (action_state != (uint8_t)PF_M4_ACTION_HITLAG)
        {
            source_animation_frame_q16 = direct_frame_q16;
        }
        source_animation_rate_q16 = (int32_t)PF_Q16_ONE;
    }
    else
    {
        source_animation_frame_q16 = INT32_C(0);
        source_animation_rate_q16 = INT32_C(0);
        fall_animation_blend_q16 = INT32_C(0);
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

    if (!pf_m4_action_retains_source_submotion(
            action_state,
            scratch->hitlag_resume_action[player_index]) ||
        (fighter->reference_frame_data_enabled == UINT8_C(0) &&
         (pf_m4_action_uses_fall_special_pose(
              pf_m4_effective_action_state(
                  action_state,
                  scratch->hitlag_resume_action[player_index])) ||
          pf_m4_action_uses_direct_hsd_pose(
              pf_m4_effective_action_state(
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
        pf_m4_action_uses_fall_special_pose(action_state) &&
        !pf_m4_action_uses_fall_special_pose(
            world->action_state[player_index]))
    {
        air_jumps_remaining = UINT8_C(0);
    }

    /* xF0 is a ground-tangent scalar. Ground-to-air conversions keep the
     * already projected x8c launch velocity but must not retain xF0 as a
     * second, stale motion channel. */
    if (grounded == UINT8_C(0))
    {
        scratch->ground_knockback_velocity_q16[player_index] = INT32_C(0);
    }

    pf_m4_update_shield_tilt(
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
        scratch->shield_recoil_x_q16[player_index] = shield_recoil_x;
    }
    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_HITLAG &&
        action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW)
    {
        if (!pf_m4_falcon_direct_hsd_locked_bottom_q16(
                action_state,
                (int32_t)PF_Q16_ONE,
                UINT8_C(0),
                &ecb_locked_bottom_y_q16) ||
            ecb_locked_bottom_y_q16 ==
                PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16)
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        ecb_bottom_lock_ticks = PF_M4_USE_ALL_JUMPS_ECB_LOCK_TICKS;
    }
    else if (action_state != (uint8_t)PF_M4_ACTION_HITLAG &&
             ecb_bottom_lock_ticks != UINT8_C(0))
    {
        --ecb_bottom_lock_ticks;
    }
    if (grounded != UINT8_C(0) || ecb_bottom_lock_ticks == UINT8_C(0))
    {
        ecb_bottom_lock_ticks = UINT8_C(0);
        ecb_locked_bottom_y_q16 = INT32_C(0);
    }
    pf_m4_write_scratch(
        scratch,
        player_index,
        input,
        position_x,
        position_y,
        velocity_x,
        velocity_y,
        action_ticks,
        source_submotion,
        source_animation_frame_q16,
        source_animation_rate_q16,
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
    scratch->fall_animation_blend_q16[player_index] =
        fall_animation_blend_q16;
    scratch->fall_animation_target_switched[player_index] =
        fall_animation_target_switched;
    scratch->ecb_bottom_lock_ticks[player_index] =
        ecb_bottom_lock_ticks;
    scratch->ecb_locked_bottom_y_q16[player_index] =
        ecb_locked_bottom_y_q16;
    if (fighter->reference_frame_data_enabled != UINT8_C(0))
    {
        status = pf_m4_evaluate_falcon_ground_blend_pose(
            world,
            player_index,
            scratch->action_state[player_index],
            scratch->hitlag_resume_action[player_index],
            scratch->source_submotion[player_index],
            scratch->source_animation_frame_q16[player_index],
            &scratch->ground_blend_pose[player_index],
            &scratch->ground_blend_progress_q16[player_index]);
        if (status != PF_STATUS_OK)
        {
            return status;
        }
    }
    return PF_STATUS_OK;
}

pf_status pf_m4_inspect(
    const pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    const pf_m4_stage_data *stage;
    int32_t platform_center;
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
        pf_m4_platform_center_x_q16(stage, sim->world.tick);
    out_inspection->stage.floor_left_q16 =
        stage->floor_left_q16;
    out_inspection->stage.floor_right_q16 =
        stage->floor_right_q16;
    out_inspection->stage.floor_y_q16 = stage->floor_y_q16;
    out_inspection->stage.platform_left_q16 =
        platform_center - stage->platform_half_width_q16;
    out_inspection->stage.platform_right_q16 =
        platform_center + stage->platform_half_width_q16;
    out_inspection->stage.platform_y_q16 =
        stage->platform_y_q16;
    out_inspection->stage.solid_left_q16 =
        stage->solid_left_q16;
    out_inspection->stage.solid_right_q16 =
        stage->solid_right_q16;
    out_inspection->stage.solid_top_q16 =
        stage->solid_top_q16;
    out_inspection->stage.solid_bottom_q16 =
        stage->solid_bottom_q16;
    out_inspection->stage.left_ledge_x_q16 =
        stage->floor_left_q16;
    out_inspection->stage.right_ledge_x_q16 =
        stage->floor_right_q16;
    out_inspection->stage.ledge_y_q16 = stage->floor_y_q16;
    out_inspection->stage.blast_left_q16 =
        stage->blast_left_q16;
    out_inspection->stage.blast_right_q16 =
        stage->blast_right_q16;
    out_inspection->stage.blast_top_q16 =
        stage->blast_top_q16;
    out_inspection->stage.blast_bottom_q16 =
        stage->blast_bottom_q16;
    out_inspection->stage.revival_platform_start_y_q16 =
        stage->revival_platform_start_y_q16;
    out_inspection->stage.revival_platform_end_y_q16 =
        stage->revival_platform_end_y_q16;
    out_inspection->stage.revival_platform_half_width_q16 =
        stage->revival_platform_half_width_q16;
    out_inspection->stage.revival_platform_descent_ticks =
        stage->revival_platform_descent_ticks;
    out_inspection->stage.revival_platform_hold_ticks =
        stage->revival_platform_hold_ticks;
    out_inspection->stage.upper_platform_left_q16 =
        stage->upper_platform_center_x_q16 -
        stage->upper_platform_half_width_q16;
    out_inspection->stage.upper_platform_right_q16 =
        stage->upper_platform_center_x_q16 +
        stage->upper_platform_half_width_q16;
    out_inspection->stage.upper_platform_y_q16 =
        stage->upper_platform_y_q16;
    out_inspection->item.position_x_q16 =
        sim->world.item_position_x_q16;
    out_inspection->item.position_y_q16 =
        sim->world.item_position_y_q16;
    out_inspection->item.velocity_x_q16 =
        sim->world.item_velocity_x_q16;
    out_inspection->item.velocity_y_q16 =
        sim->world.item_velocity_y_q16;
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
    out_inspection->projectile.position_x_q16 =
        sim->world.projectile_position_x_q16;
    out_inspection->projectile.position_y_q16 =
        sim->world.projectile_position_y_q16;
    out_inspection->projectile.velocity_x_q16 =
        sim->world.projectile_velocity_x_q16;
    out_inspection->projectile.velocity_y_q16 =
        sim->world.projectile_velocity_y_q16;
    out_inspection->projectile.hitbox_left_q16 =
        sim->world.projectile_position_x_q16 -
        sim->content.projectile.half_width_q16;
    out_inspection->projectile.hitbox_right_q16 =
        sim->world.projectile_position_x_q16 +
        sim->content.projectile.half_width_q16;
    out_inspection->projectile.hitbox_top_q16 =
        sim->world.projectile_position_y_q16 -
        sim->content.projectile.half_height_q16;
    out_inspection->projectile.hitbox_bottom_q16 =
        sim->world.projectile_position_y_q16 +
        sim->content.projectile.half_height_q16;
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
        pf_m4_player_inspection *player =
            &out_inspection->players[player_index];
        pf_m4_hsd_local_pose
            ground_loop_pose[PF_M4_HSD_POSE_MAX_JOINTS];
        const pf_m4_hsd_local_pose *ground_loop_pose_or_null = NULL;

        player->position_x_q16 =
            sim->world.position_x_q16[player_index];
        player->position_y_q16 =
            sim->world.position_y_q16[player_index];
        player->self_velocity_x_q16 =
            sim->world.velocity_x_q16[player_index];
        player->self_velocity_y_q16 =
            sim->world.velocity_y_q16[player_index];
        if (sim->world.grounded[player_index] != UINT8_C(0) &&
            sim->content.stage.reference_collision_profile !=
                (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED &&
            !(sim->world.action_state[player_index] ==
                  (uint8_t)PF_M4_ACTION_LANDING &&
              sim->world.action_ticks[player_index] == UINT16_C(0)) &&
            sim->world.action_ticks[player_index] + UINT16_C(1) !=
                pf_m4_falcon_reference_ledge_option_ground_frame(
                    sim->world.source_submotion[player_index]) &&
            !(sim->world.action_ticks[player_index] == UINT16_C(0) &&
              (sim->world.action_state[player_index] ==
                   (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
               pf_m4_action_is_surface_tech(
                   sim->world.action_state[player_index]))))
        {
            pf_m4_project_ground_scalar_q16(
                &sim->content,
                sim->world.support[player_index],
                sim->world.velocity_x_q16[player_index],
                &player->self_velocity_x_q16,
                &player->self_velocity_y_q16);
        }
        player->velocity_x_q16 =
            pf_m4_total_velocity_q16(
                player->self_velocity_x_q16,
                sim->world.knockback_velocity_x_q16[player_index]);
        player->velocity_y_q16 =
            pf_m4_total_velocity_q16(
                player->self_velocity_y_q16,
                sim->world.knockback_velocity_y_q16[player_index]);
        player->knockback_velocity_x_q16 =
            sim->world.knockback_velocity_x_q16[player_index];
        player->knockback_velocity_y_q16 =
            sim->world.knockback_velocity_y_q16[player_index];
        player->ground_knockback_velocity_q16 =
            sim->world.ground_knockback_velocity_q16[player_index];
        player->shield_recoil_x_q16 =
            sim->world.shield_recoil_x_q16[player_index];
        player->source_animation_frame_q16 =
            sim->world.source_animation_frame_q16[player_index];
        player->source_animation_rate_q16 =
            sim->world.source_animation_rate_q16[player_index];
        player->fall_animation_blend_q16 =
            sim->world.fall_animation_blend_q16[player_index];
        player->fall_animation_target_switched =
            sim->world.fall_animation_target_switched[player_index];
        {
            pf_m4_falcon_ecb_pose_q16 ecb_pose;
            const uint8_t effective_action = pf_m4_effective_action_state(
                sim->world.action_state[player_index],
                sim->world.hitlag_resume_action[player_index]);

            if (pf_m4_reference_ecb_pose_q16(
                    &sim->content.fighter,
                    effective_action,
                    sim->world.action_ticks[player_index],
                    sim->world.grounded[player_index],
                    sim->world.ecb_bottom_lock_ticks[player_index] !=
                            UINT8_C(0)
                        ? sim->world.ecb_locked_bottom_y_q16[player_index]
                        : PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                    sim->world.source_submotion[player_index],
                    sim->world.source_animation_frame_q16[player_index],
                    sim->world.fall_animation_blend_q16[player_index],
                    sim->world.fall_animation_target_switched[player_index],
                    sim->world.prone_orientation[player_index],
                    sim->world.prone_roll_motion_orientation[player_index],
                    sim->world.tech_direction[player_index],
                    sim->world.facing[player_index],
                    sim->world.ground_blend_progress_q16[player_index],
                    sim->world.ground_blend_progress_q16[player_index] >
                            INT32_C(0)
                        ? &sim->world.ground_blend_pose[player_index]
                        : NULL,
                    &ecb_pose))
            {
                player->ecb_bottom_y_from_origin_q16 =
                    ecb_pose.bottom_y_from_origin_q16;
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
        player->ledge = pf_m4_ledge_from_state(
            player->action_state,
            sim->world.hitlag_resume_action[player_index],
            player->facing);
        player->last_hit_tick =
            sim->world.last_hit_tick[player_index];
        player->damage_q16 =
            sim->world.damage_q16[player_index];
        player->last_hit_sequence =
            sim->world.last_hit_sequence[player_index];
        player->last_hit_damage_q16 =
            sim->world.last_hit_damage_q16[player_index];
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
            sim->world.respawn_invulnerability_ticks[player_index] !=
                    UINT16_C(0) ||
                sim->world.ledge_invulnerability_ticks[player_index] !=
                    UINT16_C(0) ||
                pf_m4_action_is_recovery_invulnerable(
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
                 pf_m4_falcon_reference_body_invulnerable(
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
        player->shield_health_q16 =
            sim->world.shield_health_q16[player_index];
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
        pf_m4_shield_tilt_axes(
            sim->world.shield_angle_turn[player_index],
            sim->world.shield_magnitude[player_index],
            player->facing,
            &player->shield_tilt_x,
            &player->shield_tilt_y);
        player->shield_active = (uint8_t)pf_m4_shield_box(
            &sim->content.fighter,
            player->position_x_q16,
            player->position_y_q16,
            player->action_state,
            sim->world.hitlag_resume_action[player_index],
            player->shield_health_q16,
            player->shield_strength,
            player->facing,
            sim->world.shield_angle_turn[player_index],
            sim->world.shield_magnitude[player_index],
            &player->shield_left_q16,
            &player->shield_right_q16,
            &player->shield_top_q16,
            &player->shield_bottom_q16);
        player->revival_platform_active =
            player->action_state ==
                    (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM
                ? UINT8_C(1)
                : UINT8_C(0);
        if (player->revival_platform_active != UINT8_C(0))
        {
            player->revival_platform_left_q16 =
                player->position_x_q16 -
                stage->revival_platform_half_width_q16;
            player->revival_platform_right_q16 =
                player->position_x_q16 +
                stage->revival_platform_half_width_q16;
            player->revival_platform_y_q16 =
                player->position_y_q16 +
                sim->content.fighter.half_height_q16;
        }
        player->stale_move_count =
            sim->world.stale_move_count[player_index];
        if (player->stale_move_count == UINT8_C(0))
        {
            player->stale_move_multiplier_q16 =
                (uint32_t)PF_Q16_ONE;
        }
        else
        {
            const uint8_t current_action =
                player->action_state == (uint8_t)PF_M4_ACTION_HITLAG
                    ? sim->world.hitlag_resume_action[player_index]
                    : player->action_state;

            player->stale_move_multiplier_q16 =
                pf_m4_stale_move_multiplier_q16(
                    &sim->content.fighter,
                    sim->world.stale_move_ids[player_index],
                    sim->world.stale_move_count[player_index],
                    pf_m4_stale_move_id_for_action(current_action));
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
        player->hitbox_active = (uint8_t)pf_m4_attack_hitbox(
            &sim->content,
            player->position_x_q16,
            player->position_y_q16,
            player->facing,
            player->action_state,
            player->action_ticks,
            sim->world.source_submotion[player_index],
            &player->hitbox_left_q16,
            &player->hitbox_right_q16,
            &player->hitbox_top_q16,
            &player->hitbox_bottom_q16);
        player->hit_sphere_count = pf_m4_attack_hit_spheres(
            &sim->content,
            player->position_x_q16,
            player->position_y_q16,
            player->facing,
            player->action_state,
            player->action_ticks,
            player->hit_spheres);
        player->grabbox_active = (uint8_t)pf_m4_grabbox(
            &sim->content,
            player->position_x_q16,
            player->position_y_q16,
            player->facing,
            player->action_state,
            player->action_ticks,
            &player->grabbox_left_q16,
            &player->grabbox_right_q16,
            &player->grabbox_top_q16,
            &player->grabbox_bottom_q16);
        if (sim->world.ground_blend_progress_q16[player_index] >
            INT32_C(0))
        {
            const pf_m4_hsd_pose_data *data =
                pf_m4_falcon_reference_hsd_pose_data();
            if (data == NULL ||
                !pf_m4_hsd_resolve_compact_pose_q16(
                    data,
                    sim->world.source_submotion[player_index],
                    sim->world.source_animation_frame_q16[player_index],
                    sim->world.ground_blend_progress_q16[player_index],
                    &sim->world.ground_blend_pose[player_index],
                    ground_loop_pose))
            {
                return PF_STATUS_DETERMINISTIC_FAULT;
            }
            ground_loop_pose_or_null = ground_loop_pose;
        }
        player->hurt_capsule_count =
            pf_m4_reference_world_hurt_capsules(
                &sim->content.fighter,
                player->position_x_q16,
                player->position_y_q16,
                player->facing,
                sim->world.dash_direction[player_index],
                player->grounded,
                player->action_state,
                player->hitlag_resume_action,
                sim->world.source_submotion[player_index],
                sim->world.source_animation_frame_q16[player_index],
                player->action_ticks,
                ground_loop_pose_or_null,
                player->hurt_capsules);
    }
    return PF_STATUS_OK;
}
