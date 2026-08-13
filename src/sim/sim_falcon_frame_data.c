#include "sim_falcon_frame_data.h"
#include "sim_hsd_pose.h"
#include "sim_ssbm_common_data.h"

#include "pf/m4.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "../../generated/data/falcon_ntsc102_frame_data.inc"
#include "../../generated/data/falcon_ntsc102_hit_geometry.inc"
#include "../../generated/data/ssbm_falcon_ledge_hurt.inc"
#include "../../generated/data/ssbm_falcon_airborne_hurt.inc"
#include "../../generated/data/ssbm_falcon_turn_hurt.inc"
#include "../../generated/data/ssbm_falcon_crouch_taunt_hurt.inc"
#include "../../generated/data/ssbm_falcon_guard_hurt.inc"
#include "../../generated/data/ssbm_falcon_ground_loop_hsd.inc"

_Static_assert(
    (size_t)PF_M4_FALCON_LEDGE_HURT_COUNT ==
        (size_t)PF_M4_FALCON_LEDGE_HURT_TRACK_COUNT,
    "Falcon enum ledge hurt-pose manifest and runtime binding disagree");

_Static_assert(
    sizeof(falcon_moves) / sizeof(falcon_moves[0]) ==
        (size_t)PF_M4_FALCON_MOVE_COUNT,
    "Falcon move table must cover every indexed move");
_Static_assert(
    sizeof(falcon_hurt_moves) /
            sizeof(falcon_hurt_moves[0]) ==
        (size_t)PF_M4_FALCON_MOVE_COUNT,
    "Falcon hurt-pose table must cover every indexed move");
_Static_assert(
    sizeof(falcon_common_attribute_bits) /
            sizeof(falcon_common_attribute_bits[0]) ==
        (size_t)PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT,
    "Falcon common-attribute table must be complete");
_Static_assert(
    sizeof(falcon_submotions) / sizeof(falcon_submotions[0]) ==
        (size_t)PF_M4_FALCON_SUBMOTION_COUNT,
    "Falcon submotion table must cover every source slot");
_Static_assert(
    sizeof(falcon_script_events) /
            sizeof(falcon_script_events[0]) ==
        (size_t)PF_M4_FALCON_SCRIPT_EVENT_COUNT,
    "Falcon action-script event table must be complete");
_Static_assert(
    sizeof(falcon_body_collision_timings) /
            sizeof(falcon_body_collision_timings[0]) ==
        (size_t)PF_M4_FALCON_SUBMOTION_COUNT,
    "Falcon body-collision timing table must cover every source slot");
_Static_assert(
    sizeof(falcon_translation_x_f32) /
            sizeof(falcon_translation_x_f32[0]) ==
        (size_t)PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT,
    "Falcon X translation table must be complete");
_Static_assert(
    sizeof(falcon_translation_y_f32) /
            sizeof(falcon_translation_y_f32[0]) ==
        (size_t)PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT,
    "Falcon Y translation table must be complete");
_Static_assert(
    sizeof(falcon_script_bytes) /
            sizeof(falcon_script_bytes[0]) ==
        (size_t)PF_M4_FALCON_SCRIPT_BYTE_COUNT,
    "Falcon action-script byte table must be complete");
_Static_assert(
    sizeof(falcon_special_attributes) == (size_t)0x8c,
    "Falcon special-attribute view must cover the source block exactly");
_Static_assert(
    PF_M4_MELEE_STALE_MOVE_SLOT_COUNT == PF_SIM_STALE_MOVE_QUEUE_CAPACITY,
    "Imported Melee stale-move table must cover the runtime queue");
_Static_assert(
    sizeof(falcon_collision_pose_data.crouch_wait) /
            sizeof(falcon_collision_pose_data.crouch_wait[0]) ==
        (size_t)PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT,
    "Falcon CrouchWait ECB cycle must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.airborne) /
            sizeof(falcon_collision_pose_data.airborne[0]) ==
        (size_t)PF_M4_FALCON_AIRBORNE_ECB_FRAME_COUNT,
    "Falcon airborne ECB table must be complete");
_Static_assert(
    PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT +
            PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT +
            PF_M4_FALCON_JUMP_AERIAL_FORWARD_ECB_FRAME_COUNT +
            PF_M4_FALCON_JUMP_AERIAL_BACKWARD_ECB_FRAME_COUNT +
            PF_M4_FALCON_FALL_ECB_FRAME_COUNT +
            PF_M4_FALCON_FALL_AERIAL_ECB_FRAME_COUNT ==
        PF_M4_FALCON_AIRBORNE_ECB_FRAME_COUNT,
    "Falcon airborne ECB track spans must cover the packed table");
_Static_assert(
    sizeof(falcon_collision_pose_data.shield_break_fly) /
            sizeof(falcon_collision_pose_data.shield_break_fly[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_FLY_ECB_FRAME_COUNT,
    "Falcon ShieldBreakFly ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.shield_break_down_down) /
            sizeof(falcon_collision_pose_data
                       .shield_break_down_down[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_DOWN_ECB_FRAME_COUNT,
    "Falcon ShieldBreakDownD ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.shield_break_stand_down) /
            sizeof(falcon_collision_pose_data
                       .shield_break_stand_down[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_STAND_ECB_FRAME_COUNT,
    "Falcon ShieldBreakStandD ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.shield_break_stun) /
            sizeof(falcon_collision_pose_data.shield_break_stun[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_STUN_ECB_FRAME_COUNT,
    "Falcon Furafura ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.guard_on) /
            sizeof(falcon_collision_pose_data.guard_on[0]) ==
        (size_t)PF_M4_FALCON_GUARD_ON_FRAME_COUNT,
    "Falcon GuardOn ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.guard) /
            sizeof(falcon_collision_pose_data.guard[0]) ==
        (size_t)PF_M4_FALCON_GUARD_FRAME_COUNT,
    "Falcon Guard ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.guard_off) /
            sizeof(falcon_collision_pose_data.guard_off[0]) ==
        (size_t)PF_M4_FALCON_GUARD_OFF_FRAME_COUNT,
    "Falcon GuardOff ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.ceiling_bounce) /
            sizeof(falcon_collision_pose_data.ceiling_bounce[0]) ==
        (size_t)PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT,
    "Falcon ceiling-bounce ECB table must be complete");
_Static_assert(
    sizeof(falcon_collision_pose_data.wall_bounce) /
            sizeof(falcon_collision_pose_data.wall_bounce[0]) ==
        (size_t)PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT,
    "Falcon wall-bounce ECB table must be complete");

const uint8_t *falcon_reference_source_sha256(void)
{
    return falcon_source_sha256;
}

const uint8_t *falcon_reference_complete_source_sha256(void)
{
    return falcon_complete_source_sha256;
}

const uint8_t *falcon_reference_submotion_catalog_sha256(void)
{
    return falcon_submotion_catalog_sha256;
}

const uint8_t *falcon_reference_action_script_sha256(void)
{
    return falcon_action_script_sha256;
}

const uint8_t *falcon_reference_animation_tracks_sha256(void)
{
    return falcon_animation_tracks_sha256;
}

const falcon_animation_decode_summary *
falcon_reference_animation_decode_summary(void)
{
    return &falcon_animation_decode_summary_data;
}

const falcon_submotion_data *falcon_reference_submotion(
    uint16_t submotion_index)
{
    if (submotion_index >= PF_M4_FALCON_SUBMOTION_COUNT)
    {
        return NULL;
    }
    return &falcon_submotions[submotion_index];
}

int falcon_reference_damage_submotion(
    uint8_t source_grounded,
    uint8_t damage_level,
    uint8_t hurtbox_height,
    uint16_t *out_submotion_index)
{
    static const uint16_t damage_submotions[2][4][3] = {
        {
            { UINT16_C(171), UINT16_C(168), UINT16_C(165) },
            { UINT16_C(172), UINT16_C(169), UINT16_C(166) },
            { UINT16_C(173), UINT16_C(170), UINT16_C(167) },
            { UINT16_C(179), UINT16_C(178), UINT16_C(177) },
        },
        {
            { UINT16_C(174), UINT16_C(174), UINT16_C(174) },
            { UINT16_C(175), UINT16_C(175), UINT16_C(175) },
            { UINT16_C(176), UINT16_C(176), UINT16_C(176) },
            { UINT16_C(179), UINT16_C(178), UINT16_C(177) },
        },
    };

    if (out_submotion_index == NULL || source_grounded > UINT8_C(1) ||
        damage_level > UINT8_C(3) || hurtbox_height > UINT8_C(2))
    {
        return 0;
    }
    *out_submotion_index =
        damage_submotions[source_grounded == UINT8_C(0) ? 1 : 0]
                         [damage_level][hurtbox_height];
    return 1;
}

const falcon_script_event *falcon_reference_submotion_event(
    uint16_t submotion_index,
    uint16_t event_index,
    const uint8_t **out_bytes)
{
    const falcon_submotion_data *submotion;
    const falcon_script_event *event;

    if (out_bytes != NULL)
    {
        *out_bytes = NULL;
    }
    submotion = falcon_reference_submotion(submotion_index);
    if (submotion == NULL || event_index >= submotion->event_count)
    {
        return NULL;
    }
    event = &falcon_script_events[
        (uint32_t)submotion->event_offset + (uint32_t)event_index];
    if (out_bytes != NULL)
    {
        *out_bytes = &falcon_script_bytes[event->byte_offset];
    }
    return event;
}

const falcon_body_collision_timing *
falcon_reference_body_collision_timing(uint16_t submotion_index)
{
    if (submotion_index >= PF_M4_FALCON_SUBMOTION_COUNT)
    {
        return NULL;
    }
    return &falcon_body_collision_timings[submotion_index];
}

const uint32_t *falcon_reference_common_attribute_bits(
    uint16_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT;
    }
    return falcon_common_attribute_bits;
}

const falcon_common_attributes *
falcon_reference_common_attributes(void)
{
    return &falcon_common_attribute_data;
}

const falcon_ledge_attributes *
falcon_reference_ledge_attributes(void)
{
    return &falcon_ledge_attribute_data;
}

const falcon_ledge_root_positions *
falcon_reference_ledge_root_positions(void)
{
    return &falcon_ledge_root_position_data;
}

const falcon_ledge_attack_reference *
falcon_reference_ledge_attack(uint16_t submotion_index)
{
    if (submotion_index <
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW ||
        submotion_index >
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK)
    {
        return NULL;
    }
    return &falcon_ledge_attack_references[
        submotion_index -
        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW];
}

int falcon_reference_ledge_option_anchor_f32(
    uint16_t submotion_index,
    float *out_x_f32,
    float *out_y_f32)
{
    uint16_t option_index;

    if (submotion_index < PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST ||
        submotion_index >=
            PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST +
                PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT ||
        submotion_index ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_2 ||
        submotion_index ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2)
    {
        return 0;
    }
    option_index = (uint16_t)(
        submotion_index - PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST);
    if (out_x_f32 != NULL)
    {
        *out_x_f32 =
            falcon_ledge_root_position_data
                .option_frame_one_x_f32[option_index];
    }
    if (out_y_f32 != NULL)
    {
        *out_y_f32 =
            falcon_ledge_root_position_data
                .option_frame_one_y_f32[option_index];
    }
    return 1;
}

uint16_t falcon_reference_ledge_option_ground_frame(
    uint16_t submotion_index)
{
    if (submotion_index < PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST ||
        submotion_index >=
            PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST +
                PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT)
    {
        return UINT16_C(0);
    }
    return falcon_ledge_root_position_data.option_ground_frame[
        submotion_index - PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST];
}

int falcon_reference_hyrule_ledge_jump_position_f32(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    float *out_x_from_wait_f32,
    float *out_y_from_wait_f32)
{
    const float (*path_f32)[2];
    uint16_t frame_count;

    if (submotion_index ==
        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_1)
    {
        path_f32 = falcon_hyrule_ledge_jump1_quick_from_wait_f32;
        frame_count = PF_M4_FALCON_LEDGE_JUMP1_QUICK_FRAME_COUNT;
    }
    else if (submotion_index ==
             (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_1)
    {
        path_f32 = falcon_hyrule_ledge_jump1_slow_from_wait_f32;
        frame_count = PF_M4_FALCON_LEDGE_JUMP1_SLOW_FRAME_COUNT;
    }
    else if (displayed_frame == UINT16_C(1) &&
             (submotion_index ==
                  (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2 ||
              submotion_index ==
                  (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_2))
    {
        const uint16_t phase_two_index =
            submotion_index ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2
                ? UINT16_C(0)
                : UINT16_C(1);

        if (out_x_from_wait_f32 != NULL)
        {
            *out_x_from_wait_f32 =
                falcon_hyrule_ledge_jump2_frame_one_from_wait_f32
                    [phase_two_index][0];
        }
        if (out_y_from_wait_f32 != NULL)
        {
            *out_y_from_wait_f32 =
                falcon_hyrule_ledge_jump2_frame_one_from_wait_f32
                    [phase_two_index][1];
        }
        return 1;
    }
    else
    {
        return 0;
    }
    if (displayed_frame == UINT16_C(0) || displayed_frame > frame_count)
    {
        return 0;
    }
    if (out_x_from_wait_f32 != NULL)
    {
        *out_x_from_wait_f32 = path_f32[displayed_frame - UINT16_C(1)][0];
    }
    if (out_y_from_wait_f32 != NULL)
    {
        *out_y_from_wait_f32 = path_f32[displayed_frame - UINT16_C(1)][1];
    }
    return 1;
}

int falcon_reference_body_invulnerable(
    uint16_t submotion_index,
    uint16_t action_ticks)
{
    const falcon_body_collision_timing *timing =
        falcon_reference_body_collision_timing(submotion_index);
    const uint32_t displayed_frame =
        (uint32_t)action_ticks + UINT32_C(1);

    return timing != NULL &&
           timing->state_two_frame != UINT16_MAX &&
           displayed_frame >= (uint32_t)timing->state_two_frame &&
           (timing->state_zero_frame == UINT16_MAX ||
            displayed_frame < (uint32_t)timing->state_zero_frame);
}

const falcon_special_attributes *
falcon_reference_special_attributes(void)
{
    return &falcon_special_attribute_data;
}

const falcon_common_special_attributes *
falcon_reference_common_special_attributes(void)
{
    return &falcon_common_special_attribute_data;
}

const falcon_air_dodge_attributes *
falcon_reference_air_dodge_attributes(void)
{
    return &falcon_air_dodge_attribute_data;
}

const melee_stale_move_data *
falcon_reference_stale_move_data(void)
{
    return &melee_stale_move_data_source;
}

const falcon_smash_charge_attributes *
falcon_reference_smash_charge_attributes(void)
{
    return &falcon_smash_charge_attributes_source;
}

const falcon_neutral_special_timing *
falcon_reference_neutral_special_timing(void)
{
    return &falcon_neutral_special_timing_data;
}

const falcon_side_special_timing *
falcon_reference_side_special_timing(void)
{
    return &falcon_side_special_timing_data;
}

const falcon_up_special_timing *
falcon_reference_up_special_timing(void)
{
    return &falcon_up_special_timing_data;
}

const falcon_down_special_timing *
falcon_reference_down_special_timing(void)
{
    return &falcon_down_special_timing_data;
}

const falcon_collision_pose *
falcon_reference_collision_pose(void)
{
    return &falcon_collision_pose_data;
}

const falcon_ecb_pose_f32 *
falcon_reference_guard_ecb_pose(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks)
{
    if ((action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
         action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN) &&
        source_submotion !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF)
    {
        if (source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON)
        {
            const uint16_t frame =
                action_ticks < PF_M4_FALCON_GUARD_ON_FRAME_COUNT
                    ? action_ticks
                    : (uint16_t)(PF_M4_FALCON_GUARD_ON_FRAME_COUNT -
                                 UINT16_C(1));
            return &falcon_collision_pose_data.guard_on[frame];
        }
        if (source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD)
        {
            return &falcon_collision_pose_data.guard[0];
        }
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_OFF)
    {
        const uint16_t frame =
            action_ticks < PF_M4_FALCON_GUARD_OFF_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(PF_M4_FALCON_GUARD_OFF_FRAME_COUNT -
                             UINT16_C(1));
        return &falcon_collision_pose_data.guard_off[frame];
    }
    return NULL;
}

static int falcon_prone_pose_orientation_index(
    uint8_t prone_orientation,
    uint16_t *out_index)
{
    if (prone_orientation == (uint8_t)PF_M4_PRONE_BACK)
    {
        *out_index = UINT16_C(0);
        return 1;
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_STOMACH)
    {
        *out_index = UINT16_C(1);
        return 1;
    }
    return 0;
}

const falcon_ecb_pose_f32 *
falcon_reference_prone_ecb_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t prone_orientation,
    uint8_t prone_roll_motion_orientation,
    int8_t tech_direction,
    int8_t facing)
{
    uint16_t orientation_index;
    uint16_t frame_index;

    if (action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN &&
        falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index = action_ticks < PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT
                          ? action_ticks
                          : (uint16_t)(
                                PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT -
                                UINT16_C(1));
        return &falcon_collision_pose_data
                    .down_bound[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT &&
        falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index = (uint16_t)(
            (action_ticks + UINT16_C(1)) %
            PF_M4_FALCON_DOWN_WAIT_ECB_FRAME_COUNT);
        return &falcon_collision_pose_data
                    .down_wait[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL &&
        falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index =
            action_ticks < PF_M4_FALCON_GETUP_NEUTRAL_ECB_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(
                      PF_M4_FALCON_GETUP_NEUTRAL_ECB_FRAME_COUNT -
                      UINT16_C(1));
        return &falcon_collision_pose_data
                    .getup_neutral[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
        falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index = action_ticks < PF_M4_FALCON_GETUP_ATTACK_ECB_FRAME_COUNT
                          ? action_ticks
                          : (uint16_t)(
                                PF_M4_FALCON_GETUP_ATTACK_ECB_FRAME_COUNT -
                                UINT16_C(1));
        return &falcon_collision_pose_data
                    .getup_attack[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL &&
        (tech_direction == INT8_C(-1) || tech_direction == INT8_C(1)) &&
        (facing == INT8_C(-1) || facing == INT8_C(1)) &&
        falcon_prone_pose_orientation_index(
            prone_roll_motion_orientation,
            &orientation_index))
    {
        const uint16_t direction_index =
            tech_direction == facing ? UINT16_C(0) : UINT16_C(1);

        frame_index = action_ticks < PF_M4_FALCON_GETUP_ROLL_ECB_FRAME_COUNT
                          ? action_ticks
                          : (uint16_t)(
                                PF_M4_FALCON_GETUP_ROLL_ECB_FRAME_COUNT -
                                UINT16_C(1));
        return &falcon_collision_pose_data
                    .getup_roll[orientation_index][direction_index][frame_index];
    }
    return NULL;
}

static float falcon_ecb_source_scale_f32(
    float value_f32,
    int32_t numerator,
    int32_t denominator)
{
    return value_f32 * (float)numerator / (float)denominator;
}

static float falcon_ecb_sim_to_source_abs_f32(
    float value_f32,
    uint32_t numerator,
    uint32_t denominator)
{
    return value_f32 * (float)denominator / (float)numerator;
}

int falcon_reference_collision_sweep_step_count_f32(
    float position_delta_x_f32,
    float position_delta_y_f32,
    const falcon_ecb_pose_f32 *current_ecb,
    const falcon_ecb_pose_f32 *desired_ecb,
    uint16_t *out_step_count)
{
    const float source_threshold_f32 = 6.0f;
    float maximum_source_delta_f32;
    float source_delta_f32;
    float step_count;

    if (current_ecb == NULL || desired_ecb == NULL ||
        out_step_count == NULL)
    {
        return 0;
    }
    maximum_source_delta_f32 =
        falcon_ecb_sim_to_source_abs_f32(
            fabsf(position_delta_x_f32),
            UINT32_C(12),
            UINT32_C(115));
    source_delta_f32 = falcon_ecb_sim_to_source_abs_f32(
        fabsf(position_delta_y_f32),
        UINT32_C(11),
        UINT32_C(62));
    if (source_delta_f32 > maximum_source_delta_f32)
    {
        maximum_source_delta_f32 = source_delta_f32;
    }
#define PF_M4_FALCON_ACCUMULATE_SWEEP_X(field)                              \
    do                                                                       \
    {                                                                        \
        source_delta_f32 = falcon_ecb_sim_to_source_abs_f32(          \
            fabsf(desired_ecb->field - current_ecb->field),                  \
            UINT32_C(12),                                                     \
            UINT32_C(115));                                                   \
        if (source_delta_f32 > maximum_source_delta_f32)                     \
        {                                                                    \
            maximum_source_delta_f32 = source_delta_f32;                     \
        }                                                                    \
    } while (0)
#define PF_M4_FALCON_ACCUMULATE_SWEEP_Y(field)                              \
    do                                                                       \
    {                                                                        \
        source_delta_f32 = falcon_ecb_sim_to_source_abs_f32(          \
            fabsf(desired_ecb->field - current_ecb->field),                  \
            UINT32_C(11),                                                     \
            UINT32_C(62));                                                    \
        if (source_delta_f32 > maximum_source_delta_f32)                     \
        {                                                                    \
            maximum_source_delta_f32 = source_delta_f32;                     \
        }                                                                    \
    } while (0)
    PF_M4_FALCON_ACCUMULATE_SWEEP_X(left_x_from_origin_f32);
    PF_M4_FALCON_ACCUMULATE_SWEEP_X(right_x_from_origin_f32);
    PF_M4_FALCON_ACCUMULATE_SWEEP_Y(top_y_from_origin_f32);
    PF_M4_FALCON_ACCUMULATE_SWEEP_Y(right_y_from_origin_f32);
#undef PF_M4_FALCON_ACCUMULATE_SWEEP_Y
#undef PF_M4_FALCON_ACCUMULATE_SWEEP_X
    step_count = maximum_source_delta_f32 > source_threshold_f32
                     ? ceilf(maximum_source_delta_f32 / source_threshold_f32)
                     : 1.0f;
    if (step_count > (float)UINT16_MAX)
    {
        return 0;
    }
    *out_step_count = (uint16_t)step_count;
    return 1;
}

enum
{
    PF_M4_FALCON_HSD_ECB_JOINT_COUNT =
        sizeof(falcon_dynamic_hsd_ecb_joint_indices) /
        sizeof(falcon_dynamic_hsd_ecb_joint_indices[0]),
    PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT =
        PF_M4_FALCON_HSD_ECB_JOINT_COUNT + 1
};

static inline int falcon_hsd_ecb_evaluation_joints(
    uint8_t joint_indices[PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT])
{
    uint8_t joint_index;

    if (joint_indices == NULL ||
        falcon_dynamic_hsd_data.copy_target_joint_count !=
            UINT8_C(1))
    {
        return 0;
    }
    for (joint_index = UINT8_C(0);
         joint_index < PF_M4_FALCON_HSD_ECB_JOINT_COUNT;
         ++joint_index)
    {
        joint_indices[joint_index] =
            falcon_dynamic_hsd_ecb_joint_indices[joint_index];
    }
    joint_indices[PF_M4_FALCON_HSD_ECB_JOINT_COUNT] =
        falcon_dynamic_hsd_data.copy_target_joint_indices[0];
    return 1;
}

int falcon_reference_ecb_apply_bottom_lock_f32(
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *pose)
{
    const float one_x_f32 = falcon_ecb_source_scale_f32(
        1.0f, INT32_C(12), INT32_C(115));
    const float one_y_f32 = falcon_ecb_source_scale_f32(
        1.0f, INT32_C(11), INT32_C(62));
    const float epsilon_y_f32 = falcon_ecb_source_scale_f32(
        0.00100708008f, INT32_C(11), INT32_C(62));

    if (pose == NULL)
    {
        return 0;
    }
    if (locked_bottom_y_f32 != PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32)
    {
        pose->bottom_y_from_origin_f32 = locked_bottom_y_f32;
    }
    if (fabsf(
            pose->top_y_from_origin_f32 -
            pose->bottom_y_from_origin_f32) < one_y_f32)
    {
        pose->top_y_from_origin_f32 += one_y_f32;
        pose->right_y_from_origin_f32 =
            (pose->top_y_from_origin_f32 +
             pose->bottom_y_from_origin_f32) * 0.5f;
        pose->left_y_from_origin_f32 = pose->right_y_from_origin_f32;
    }
    if (pose->top_y_from_origin_f32 < one_y_f32)
    {
        pose->top_y_from_origin_f32 = one_y_f32;
    }
    if (pose->left_x_from_origin_f32 > -one_x_f32)
    {
        pose->left_x_from_origin_f32 = -one_x_f32;
    }
    if (pose->right_x_from_origin_f32 < one_x_f32)
    {
        pose->right_x_from_origin_f32 = one_x_f32;
    }
    if (pose->top_y_from_origin_f32 < pose->bottom_y_from_origin_f32)
    {
        pose->top_y_from_origin_f32 =
            pose->bottom_y_from_origin_f32 + one_y_f32;
    }
    if (pose->right_y_from_origin_f32 > pose->top_y_from_origin_f32 ||
        pose->right_y_from_origin_f32 < pose->bottom_y_from_origin_f32)
    {
        pose->right_y_from_origin_f32 =
            (pose->top_y_from_origin_f32 +
             pose->bottom_y_from_origin_f32) * 0.5f;
        pose->left_y_from_origin_f32 = pose->right_y_from_origin_f32;
    }
    if (pose->top_y_from_origin_f32 - pose->right_y_from_origin_f32 <
            epsilon_y_f32 ||
        pose->right_y_from_origin_f32 - pose->bottom_y_from_origin_f32 <
            epsilon_y_f32)
    {
        pose->right_y_from_origin_f32 =
            (pose->top_y_from_origin_f32 +
             pose->bottom_y_from_origin_f32) * 0.5f;
    }
    if (pose->top_y_from_origin_f32 - pose->left_y_from_origin_f32 <
            epsilon_y_f32 ||
        pose->left_y_from_origin_f32 - pose->bottom_y_from_origin_f32 <
            epsilon_y_f32)
    {
        pose->left_y_from_origin_f32 =
            (pose->top_y_from_origin_f32 +
             pose->bottom_y_from_origin_f32) * 0.5f;
    }
    return 1;
}

static int falcon_hsd_ecb_from_origins(
    const float *origins_f32,
    uint8_t origin_count,
    const float reference_origin_f32[3],
    int grounded,
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *out_pose)
{
    float left_f32;
    float right_f32;
    float bottom_f32;
    float top_f32;
    float side_y_f32;
    uint8_t index;

    if (origins_f32 == NULL || origin_count == UINT8_C(0) ||
        reference_origin_f32 == NULL || out_pose == NULL)
    {
        return 0;
    }
    left_f32 = right_f32 =
        origins_f32[0] - reference_origin_f32[0];
    bottom_f32 = top_f32 =
        origins_f32[1] - reference_origin_f32[1];
    for (index = UINT8_C(1); index < origin_count; ++index)
    {
        const size_t offset = (size_t)index * (size_t)3;

        const float x_f32 =
            origins_f32[offset] - reference_origin_f32[0];
        const float y_f32 =
            origins_f32[offset + (size_t)1] - reference_origin_f32[1];

        if (x_f32 < left_f32)
        {
            left_f32 = x_f32;
        }
        if (x_f32 > right_f32)
        {
            right_f32 = x_f32;
        }
        if (y_f32 < bottom_f32)
        {
            bottom_f32 = y_f32;
        }
        if (y_f32 > top_f32)
        {
            top_f32 = y_f32;
        }
    }
    if (right_f32 - left_f32 < 10.0f)
    {
        right_f32 = (right_f32 - left_f32) * 0.5f;
        left_f32 = -right_f32;
    }
    if (top_f32 - bottom_f32 < 10.0f)
    {
        const float half_height_f32 =
            (top_f32 - bottom_f32) * 0.5f;
        const float middle_f32 =
            (top_f32 + bottom_f32) * 0.5f;

        top_f32 = middle_f32 + half_height_f32;
        bottom_f32 = middle_f32 - half_height_f32;
    }
    if (right_f32 < 2.0f)
    {
        right_f32 = 2.0f;
    }
    if (left_f32 > -2.0f)
    {
        left_f32 = -2.0f;
    }
    if (grounded != 0)
    {
        bottom_f32 = 0.0f;
    }
    else if (bottom_f32 < 0.0f)
    {
        bottom_f32 = 0.0f;
    }
    side_y_f32 = (bottom_f32 + top_f32) * 0.5f;
    out_pose->top_x_from_origin_f32 = 0.0f;
    out_pose->top_y_from_origin_f32 = falcon_ecb_source_scale_f32(
        top_f32, INT32_C(11), INT32_C(62));
    out_pose->bottom_x_from_origin_f32 = 0.0f;
    out_pose->bottom_y_from_origin_f32 =
        falcon_ecb_source_scale_f32(
            bottom_f32, INT32_C(11), INT32_C(62));
    out_pose->right_x_from_origin_f32 = falcon_ecb_source_scale_f32(
        right_f32, INT32_C(12), INT32_C(115));
    out_pose->right_y_from_origin_f32 = falcon_ecb_source_scale_f32(
        side_y_f32, INT32_C(11), INT32_C(62));
    out_pose->left_x_from_origin_f32 = falcon_ecb_source_scale_f32(
        left_f32, INT32_C(12), INT32_C(115));
    out_pose->left_y_from_origin_f32 = out_pose->right_y_from_origin_f32;
    return falcon_reference_ecb_apply_bottom_lock_f32(
        locked_bottom_y_f32,
        out_pose);
}

int
falcon_reference_hsd_ecb_pose(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    int grounded,
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *out_pose)
{
    const falcon_submotion_data *submotion;
    static const float zero_origin_f32[3] = {
        0.0f, 0.0f, 0.0f
    };
    float origins_f32[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t joint_indices[PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT];
    if (out_pose == NULL || source_animation_frame_f32 < 0.0f)
    {
        return 0;
    }
    if (source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT)
    {
        const uint16_t frame_index = (uint16_t)(
            (uint32_t)source_animation_frame_f32 %
            PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT);

        *out_pose = falcon_collision_pose_data.crouch_wait[frame_index];
        return 1;
    }
    submotion = falcon_reference_submotion(source_submotion);
    if (submotion == NULL)
    {
        return 0;
    }
    if (!falcon_hsd_ecb_evaluation_joints(joint_indices))
    {
        return 0;
    }
    if (!hsd_evaluate_joint_origins_source_f32(
            &falcon_dynamic_hsd_data,
            source_submotion,
            source_animation_frame_f32,
            joint_indices,
            (uint8_t)PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT,
            origins_f32))
    {
        return 0;
    }
    return falcon_hsd_ecb_from_origins(
        &origins_f32[0][0],
        (uint8_t)PF_M4_FALCON_HSD_ECB_JOINT_COUNT,
        (submotion->animation_flags &
         PF_M4_HSD_FIGHTER_ANIMATION_TRANSLATION_FLAG) != UINT32_C(0)
            ? origins_f32[PF_M4_FALCON_HSD_ECB_JOINT_COUNT]
            : zero_origin_f32,
        grounded,
        locked_bottom_y_f32,
        out_pose);
}

int falcon_reference_action_hsd_source(
    uint8_t action_state,
    uint16_t action_ticks,
    uint16_t *out_submotion,
    float *out_frame_f32)
{
    falcon_move_index move_index;
    const struct reference_move *move;
    int8_t frame_offset;
    int32_t source_frame;

    if (out_submotion == NULL || out_frame_f32 == NULL ||
        !falcon_reference_move_for_action(action_state, &move_index))
    {
        return 0;
    }
    move = falcon_reference_move(move_index);
    if (move == NULL ||
        !falcon_dynamic_hsd_action_frame_offset(
            move->subaction_index, &frame_offset))
    {
        return 0;
    }
    source_frame = (int32_t)action_ticks + INT32_C(1) + frame_offset;
    if (source_frame < INT32_C(0))
    {
        return 0;
    }
    *out_submotion = move->subaction_index;
    *out_frame_f32 = (float)source_frame;
    return 1;
}

int falcon_reference_action_hsd_ecb_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *out_pose)
{
    uint16_t source_submotion;
    float source_frame_f32;

    if (out_pose == NULL ||
        !falcon_reference_action_hsd_source(
            action_state,
            action_ticks,
            &source_submotion,
            &source_frame_f32))
    {
        return 0;
    }
    return falcon_reference_hsd_ecb_pose(
        source_submotion,
        source_frame_f32,
        grounded != UINT8_C(0),
        locked_bottom_y_f32,
        out_pose);
}

const hsd_pose_data *
falcon_reference_hsd_pose_data(void)
{
    return &falcon_dynamic_hsd_data;
}

const hsd_local_pose *
falcon_reference_guard_target_hsd_pose(void)
{
    return falcon_dynamic_hsd_guard_target_pose;
}

int falcon_resolve_compact_hsd_pose(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    float progress_f32,
    const hsd_compact_pose *compact,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    if (compact != NULL &&
        compact->mode == (uint8_t)PF_M4_HSD_COMPACT_POSE_PACKED &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON)
    {
        return hsd_inflate_compact_pose_f32(
            &falcon_dynamic_hsd_data,
            falcon_dynamic_hsd_guard_target_pose,
            compact,
            out_pose);
    }
    return hsd_resolve_compact_pose_f32(
        &falcon_dynamic_hsd_data,
        source_submotion,
        source_animation_frame_f32,
        progress_f32,
        compact,
        out_pose);
}

const hsd_wait_animation *
falcon_reference_wait_animations(uint8_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = (uint8_t)(
            sizeof(falcon_dynamic_hsd_wait_animations) /
            sizeof(falcon_dynamic_hsd_wait_animations[0]));
    }
    return falcon_dynamic_hsd_wait_animations;
}

const hsd_wait_animation *
falcon_reference_wait_animation(uint16_t source_submotion)
{
    uint8_t count;
    const hsd_wait_animation *animations =
        falcon_reference_wait_animations(&count);
    uint8_t index;

    for (index = UINT8_C(0); index < count; ++index)
    {
        if (animations[index].source_submotion == source_submotion)
        {
            return &animations[index];
        }
    }
    return NULL;
}

int falcon_reference_direct_hsd_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    uint16_t *out_submotion,
    float *out_frame_f32)
{
    uint16_t submotion;
    float frame_f32 = (int32_t)action_ticks * 1.0f;

    if (out_submotion == NULL || out_frame_f32 == NULL)
    {
        return 0;
    }
    switch (action_state)
    {
    case PF_M4_ACTION_RAPTOR_BOOST_START_GROUND:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_GROUND;
        break;
    case PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_GROUND;
        break;
    case PF_M4_ACTION_RAPTOR_BOOST_START_AIR:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_AIR;
        break;
    case PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_AIR;
        break;
    case PF_M4_ACTION_FALCON_DIVE_START_GROUND:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND;
        break;
    case PF_M4_ACTION_FALCON_DIVE_START_AIR:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR;
        break;
    case PF_M4_ACTION_FALCON_DIVE_CATCH:
        if (grounded != UINT8_C(0))
        {
            submotion = (uint16_t)
                PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND;
            frame_f32 = INT32_C(13) * 1.0f;
        }
        else if (action_ticks == UINT16_C(0))
        {
            submotion = (uint16_t)
                PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR;
            frame_f32 = INT32_C(13) * 1.0f;
        }
        else
        {
            submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_CATCH;
        }
        break;
    case PF_M4_ACTION_FALCON_DIVE_THROW:
        submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_THROW;
        break;
    default:
        return 0;
    }
    *out_submotion = submotion;
    *out_frame_f32 = frame_f32;
    return 1;
}

static int falcon_reference_hsd_ecb_pose_from_local_pose(
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    int grounded,
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *out_pose)
{
    float origins_f32[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t joint_indices[PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT];

    if (pose == NULL || out_pose == NULL ||
        !falcon_hsd_ecb_evaluation_joints(joint_indices))
    {
        return 0;
    }
    if (
        !hsd_evaluate_joint_origins_from_local_pose_f32(
            &falcon_dynamic_hsd_data,
            pose,
            joint_indices,
            (uint8_t)PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT,
            origins_f32))
    {
        return 0;
    }
    return falcon_hsd_ecb_from_origins(
        &origins_f32[0][0],
        (uint8_t)PF_M4_FALCON_HSD_ECB_JOINT_COUNT,
        origins_f32[PF_M4_FALCON_HSD_ECB_JOINT_COUNT],
        grounded,
        locked_bottom_y_f32,
        out_pose);
}

int falcon_reference_hsd_ground_ecb_pose_from_local_pose(
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    falcon_ecb_pose_f32 *out_pose)
{
    return falcon_reference_hsd_ecb_pose_from_local_pose(
        pose,
        1,
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
        out_pose);
}

int falcon_reference_hsd_fall_ecb_pose(
    uint16_t directional_submotion,
    float source_animation_frame_f32,
    float directional_blend_f32,
    uint8_t directional_target_switched,
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *out_pose)
{
    hsd_local_pose neutral[PF_M4_HSD_POSE_MAX_JOINTS];
    hsd_local_pose directional[PF_M4_HSD_POSE_MAX_JOINTS];
    hsd_local_pose transition[PF_M4_HSD_POSE_MAX_JOINTS];
    hsd_local_pose blended[PF_M4_HSD_POSE_MAX_JOINTS];
    const hsd_local_pose *pose = neutral;
    uint16_t neutral_submotion;

    if (directional_submotion >=
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL &&
        directional_submotion <=
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_BACKWARD)
    {
        neutral_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    }
    else if (directional_submotion >=
                 (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL &&
             directional_submotion <=
                 (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL_BACKWARD)
    {
        neutral_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_AERIAL;
    }
    else if (directional_submotion >=
                 (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL &&
             directional_submotion <=
                 (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_BACKWARD)
    {
        neutral_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL;
    }
    else
    {
        return 0;
    }

    if (out_pose == NULL || source_animation_frame_f32 < INT32_C(0) ||
        directional_blend_f32 < INT32_C(0) ||
        directional_blend_f32 > 1.0f ||
        directional_target_switched > UINT8_C(1) ||
        !hsd_evaluate_local_pose_f32(
            &falcon_dynamic_hsd_data,
            neutral_submotion,
            source_animation_frame_f32,
            neutral))
    {
        return 0;
    }
    if (directional_blend_f32 != INT32_C(0))
    {
        const falcon_submotion_data *directional_motion =
            falcon_reference_submotion(directional_submotion);
        float directional_frame_f32 =
            source_animation_frame_f32 + 1.0f;

        if (directional_motion == NULL ||
            directional_motion->animation_frame_count == UINT16_C(0))
        {
            return 0;
        }
        /* ftCo_Fall_Anim_Inner installs and blends a newly selected target
         * skeleton once, then ftCo_800CC988 advances that skeleton and
         * applies the same blend again. Stable targets take only the latter
         * path. Preserve both passes so transition-frame ECBs match the HSD
         * skeleton rather than approximating them with a larger scalar. */
        if (directional_target_switched != UINT8_C(0) &&
            (!hsd_evaluate_local_pose_f32(
                 &falcon_dynamic_hsd_data,
                 directional_submotion,
                 source_animation_frame_f32,
                 directional) ||
             !hsd_blend_local_pose_f32(
                 &falcon_dynamic_hsd_data,
                 directional,
                 neutral,
                 1.0f - directional_blend_f32,
                 transition)))
        {
            return 0;
        }
        while (directional_frame_f32 >=
            (int32_t)directional_motion->animation_frame_count * 1.0f)
        {
            directional_frame_f32 -=
                (int32_t)directional_motion->animation_frame_count *
                1.0f;
        }
        if (!hsd_evaluate_local_pose_f32(
                &falcon_dynamic_hsd_data,
                directional_submotion,
                directional_frame_f32,
                directional) ||
            !hsd_blend_local_pose_f32(
                &falcon_dynamic_hsd_data,
                directional,
                directional_target_switched != UINT8_C(0)
                    ? transition
                    : neutral,
                1.0f - directional_blend_f32,
                blended))
        {
            return 0;
        }
        pose = blended;
    }
    return falcon_reference_hsd_ecb_pose_from_local_pose(
        pose,
        0,
        locked_bottom_y_f32,
        out_pose);
}

const falcon_ecb_pose_f32 *
falcon_reference_airborne_ecb_pose(
    uint16_t source_submotion,
    uint16_t action_ticks)
{
    uint16_t offset;
    uint16_t frame_count;

    switch (source_submotion)
    {
    case PF_M4_FALCON_SUBMOTION_JUMP_FORWARD:
        offset = UINT16_C(0);
        frame_count = PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_SUBMOTION_JUMP_BACKWARD:
        offset = PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT;
        frame_count = PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD:
        offset = (uint16_t)(PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_JUMP_AERIAL_FORWARD_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD:
        offset = (uint16_t)(PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_AERIAL_FORWARD_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_JUMP_AERIAL_BACKWARD_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_SUBMOTION_FALL:
        offset = (uint16_t)(PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_AERIAL_FORWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_AERIAL_BACKWARD_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_FALL_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_SUBMOTION_FALL_AERIAL:
        offset = (uint16_t)(PF_M4_FALCON_JUMP_FORWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_BACKWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_AERIAL_FORWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_JUMP_AERIAL_BACKWARD_ECB_FRAME_COUNT +
                            PF_M4_FALCON_FALL_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_FALL_AERIAL_ECB_FRAME_COUNT;
        break;
    default:
        return NULL;
    }
    if (action_ticks >= frame_count)
    {
        action_ticks = (uint16_t)(frame_count - UINT16_C(1));
    }
    return &falcon_collision_pose_data.airborne[offset + action_ticks];
}

const reference_search_sphere *
falcon_reference_side_special_search_spheres(
    int airborne,
    uint8_t *out_count)
{
    const uint16_t offset = airborne != 0
                                ? falcon_side_special_air_search_offset
                                : falcon_side_special_ground_search_offset;
    const uint8_t count = airborne != 0
                              ? falcon_side_special_air_search_count
                              : falcon_side_special_ground_search_count;

    if (out_count != NULL)
    {
        *out_count = count;
    }
    return count == UINT8_C(0)
               ? NULL
               : &falcon_side_special_search_spheres[offset];
}

const uint8_t *falcon_reference_geometry_sha256(void)
{
    return falcon_geometry_sha256;
}

void falcon_reference_capture_offset_f32(
    float *out_x_f32,
    float *out_y_f32)
{
    if (out_x_f32 != NULL)
    {
        *out_x_f32 = falcon_capture_offset_x_f32;
    }
    if (out_y_f32 != NULL)
    {
        *out_y_f32 = falcon_capture_offset_y_f32;
    }
}

int falcon_reference_capture_constraint_f32(
    uint16_t holder_submotion,
    float holder_frame_f32,
    int8_t holder_facing,
    uint16_t victim_submotion,
    float victim_frame_f32,
    int8_t victim_facing,
    float *out_x_f32,
    float *out_y_f32)
{
    float holder_origins[PF_M4_HSD_POSE_MAX_JOINTS][3];
    float victim_origins[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t joint_indices[3];
    float relative_x;
    float relative_y;
    const int victim_is_thrown =
        victim_submotion >=
            (uint16_t)PF_M4_FALCON_SUBMOTION_THROWN_FORWARD &&
        victim_submotion <=
            (uint16_t)PF_M4_FALCON_SUBMOTION_THROWN_DOWN;

    if (out_x_f32 == NULL || out_y_f32 == NULL ||
        (holder_facing != INT8_C(-1) && holder_facing != INT8_C(1)) ||
        (victim_facing != INT8_C(-1) && victim_facing != INT8_C(1)) ||
        falcon_dynamic_hsd_data.copy_target_joint_count != UINT8_C(1))
    {
        return 0;
    }
    joint_indices[0] =
        falcon_dynamic_hsd_capture_constraint_joint_indices[0];
    joint_indices[1] =
        falcon_dynamic_hsd_capture_constraint_joint_indices[1];
    joint_indices[2] =
        falcon_dynamic_hsd_data.copy_target_joint_indices[0];
    if (
        !hsd_evaluate_joint_origins_source_f32(
            &falcon_dynamic_hsd_data,
            holder_submotion,
            holder_frame_f32,
            joint_indices,
            UINT8_C(3),
            holder_origins) ||
        !hsd_evaluate_joint_origins_source_f32(
            &falcon_dynamic_hsd_data,
            victim_submotion,
            victim_frame_f32,
            joint_indices,
            UINT8_C(3),
            victim_origins))
    {
        return 0;
    }

    /* CapturePulledLw and the four Thrown motions constrain the victim XRotN
     * (source joint 2) to the
     * holder x11 attachment / TransN2 (source joint 61). Fighter roots already
     * own the animated TransN reference, so subtract that common source joint
     * from each evaluated point before reflecting the local X difference once.
     * The paired action callback owns the facing relation: captures face apart,
     * while Thrown motions copy the thrower's facing. */
    if (victim_is_thrown != 0)
    {
        /* ftCo_800DE508 reads the constrained XRotN world origin directly,
         * then adds Fighter::x1A70.z along facing and x1A70.y vertically.
         * x1A70 is the unscaled base-model root-minus-XRotN offset, so it is
         * imported separately rather than inferred from the animated victim. */
        relative_x =
            (float)holder_facing *
                (holder_origins[1][0] - holder_origins[2][0]) +
            (float)victim_facing *
                falcon_dynamic_hsd_capture_root_offset_source_f32[2];
        relative_y =
            (holder_origins[1][1] - holder_origins[2][1]) +
            falcon_dynamic_hsd_capture_root_offset_source_f32[1];
    }
    else
    {
        if (victim_facing != (int8_t)-holder_facing)
        {
            return 0;
        }
        relative_x = (float)holder_facing *
                     ((holder_origins[1][0] - holder_origins[2][0]) -
                      (victim_origins[0][0] - victim_origins[2][0]));
        relative_y = 0.0f;
    }
    *out_x_f32 = falcon_ecb_source_scale_f32(
        relative_x,
        falcon_dynamic_hsd_data.source_to_sim_numerator,
        falcon_dynamic_hsd_data.source_to_sim_denominator);
    *out_y_f32 = -falcon_ecb_source_scale_f32(
        relative_y,
        falcon_dynamic_hsd_capture_world_y_source_to_sim_numerator,
        falcon_dynamic_hsd_capture_world_y_source_to_sim_denominator);
    return 1;
}

int falcon_reference_throw_motions(
    uint8_t action_state,
    uint16_t *out_holder_submotion,
    uint16_t *out_victim_submotion,
    float *out_animation_rate_f32)
{
    const falcon_common_attributes *attributes =
        falcon_reference_common_attributes();
    uint16_t holder_submotion;
    uint16_t victim_submotion;
    uint32_t throw_index;

    if (out_holder_submotion == NULL || out_victim_submotion == NULL ||
        out_animation_rate_f32 == NULL || attributes == NULL)
    {
        return 0;
    }
    switch (action_state)
    {
    case (uint8_t)PF_M4_ACTION_THROW_FORWARD:
        holder_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_THROW_FORWARD;
        victim_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_THROWN_FORWARD;
        throw_index = UINT32_C(0);
        break;
    case (uint8_t)PF_M4_ACTION_THROW_BACK:
        holder_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_THROW_BACK;
        victim_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_THROWN_BACK;
        throw_index = UINT32_C(1);
        break;
    case (uint8_t)PF_M4_ACTION_THROW_UP:
        holder_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_THROW_UP;
        victim_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_THROWN_UP;
        throw_index = UINT32_C(2);
        break;
    case (uint8_t)PF_M4_ACTION_THROW_DOWN:
        holder_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_THROW_DOWN;
        victim_submotion = (uint16_t)PF_M4_FALCON_SUBMOTION_THROWN_DOWN;
        throw_index = UINT32_C(3);
        break;
    default:
        return 0;
    }
    *out_holder_submotion = holder_submotion;
    *out_victim_submotion = victim_submotion;
    *out_animation_rate_f32 = ssbm_throw_animation_rate_f32(
        attributes->weight,
        (attributes->weight_independent_throws_mask &
         (uint8_t)(UINT8_C(1) << throw_index)) != UINT8_C(0));
    return *out_animation_rate_f32 > INT32_C(0);
}

const struct reference_move *falcon_reference_move(
    falcon_move_index move_index)
{
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return NULL;
    }
    return &falcon_moves[move_index];
}

const reference_hit_phase *falcon_reference_phase(
    falcon_move_index move_index,
    uint16_t phase_index)
{
    const struct reference_move *move =
        falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        phase_index >= (uint16_t)move->phase_count)
    {
        return NULL;
    }
    return &falcon_hit_phases[move->phase_offset + phase_index];
}

const reference_hit_effect *falcon_reference_effect(
    falcon_move_index move_index,
    uint16_t effect_index)
{
    const struct reference_move *move =
        falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        effect_index >= (uint16_t)move->effect_count)
    {
        return NULL;
    }
    return &falcon_hit_effects[move->effect_offset + effect_index];
}

const reference_hit_effect *falcon_reference_primary_effect(
    falcon_move_index move_index)
{
    const reference_hit_phase *phase =
        falcon_reference_phase(move_index, UINT16_C(0));
    uint16_t effect_index = UINT16_C(0);
    uint16_t mask;

    if (phase == NULL || phase->effect_mask == UINT16_C(0))
    {
        return NULL;
    }
    mask = phase->effect_mask;
    while ((mask & UINT16_C(1)) == UINT16_C(0))
    {
        mask >>= 1U;
        ++effect_index;
    }
    return falcon_reference_effect(move_index, effect_index);
}

const reference_hit_phase *falcon_reference_phase_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame)
{
    const struct reference_move *move =
        falcon_reference_move(move_index);
    uint16_t phase_index;

    if (move == NULL || move->present == UINT8_C(0))
    {
        return NULL;
    }
    action_frame = falcon_reference_effective_hit_frame(
        move_index,
        action_frame);
    for (phase_index = UINT16_C(0);
         phase_index < (uint16_t)move->phase_count;
         ++phase_index)
    {
        const reference_hit_phase *phase =
            falcon_reference_phase(move_index, phase_index);

        if (phase != NULL && action_frame >= phase->first_frame &&
            action_frame <= phase->last_frame)
        {
            return phase;
        }
    }
    return NULL;
}

uint16_t falcon_reference_effective_hit_frame(
    falcon_move_index move_index,
    uint16_t action_frame)
{
    if (move_index != PF_M4_FALCON_RAPID_JABS_LOOP ||
        action_frame < UINT16_C(4))
    {
        return action_frame;
    }
    if (action_frame >= UINT16_C(35))
    {
        return (uint16_t)(UINT16_C(4) +
                          (action_frame - UINT16_C(35)));
    }
    return (uint16_t)(UINT16_C(4) +
                      (action_frame - UINT16_C(4)) % UINT16_C(8));
}

const reference_hit_effect *falcon_reference_effect_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame)
{
    const reference_hit_phase *phase =
        falcon_reference_phase_at_frame(move_index, action_frame);
    uint16_t effect_index = UINT16_C(0);
    uint16_t mask;

    if (phase == NULL || phase->effect_mask == UINT16_C(0))
    {
        return NULL;
    }
    mask = phase->effect_mask;
    while ((mask & UINT16_C(1)) == UINT16_C(0))
    {
        mask >>= 1U;
        ++effect_index;
    }
    return falcon_reference_effect(move_index, effect_index);
}

const reference_hit_sphere *
falcon_reference_hit_spheres_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_sphere_count)
{
    const reference_geometry_move *geometry;
    const reference_hit_frame *frame;
    uint16_t relative_frame;

    if (out_sphere_count != NULL)
    {
        *out_sphere_count = UINT8_C(0);
    }
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return NULL;
    }
    action_frame = falcon_reference_effective_hit_frame(
        move_index,
        action_frame);
    geometry = &falcon_geometry_moves[move_index];
    if (geometry->frame_count == UINT8_C(0) ||
        action_frame < (uint16_t)geometry->first_frame)
    {
        return NULL;
    }
    relative_frame =
        action_frame - (uint16_t)geometry->first_frame;
    if (relative_frame >= (uint16_t)geometry->frame_count)
    {
        return NULL;
    }
    frame = &falcon_hit_frames[
        geometry->frame_offset + relative_frame];
    if (frame->sphere_count == UINT8_C(0))
    {
        return NULL;
    }
    if (out_sphere_count != NULL)
    {
        *out_sphere_count = frame->sphere_count;
    }
    return &falcon_hit_spheres[frame->sphere_offset];
}

int falcon_reference_has_hit_geometry(
    falcon_move_index move_index)
{
    return (uint32_t)move_index < (uint32_t)PF_M4_FALCON_MOVE_COUNT &&
           falcon_geometry_moves[move_index].frame_count != UINT8_C(0);
}

const reference_hurt_capsule *
falcon_reference_standing_hurt_capsules(uint8_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = (uint8_t)(
            sizeof(falcon_standing_hurt_capsules) /
            sizeof(falcon_standing_hurt_capsules[0]));
    }
    return falcon_standing_hurt_capsules;
}

static const reference_hurt_capsule *
falcon_reference_hurt_track_at_frame(
    const reference_hurt_move *move,
    const reference_hurt_frame *frames,
    const reference_hurt_capsule *capsules,
    uint16_t action_frame,
    uint8_t *out_count)
{
    const reference_hurt_frame *frame;
    uint16_t relative_frame;

    if (out_count != NULL)
    {
        *out_count = UINT8_C(0);
    }
    if (move->frame_count == UINT8_C(0) ||
        action_frame < (uint16_t)move->first_frame)
    {
        return NULL;
    }
    relative_frame = action_frame - (uint16_t)move->first_frame;
    if (relative_frame >= (uint16_t)move->frame_count)
    {
        return NULL;
    }
    frame = &frames[move->frame_offset + relative_frame];
    if (frame->capsule_count == UINT8_C(0))
    {
        return NULL;
    }
    if (out_count != NULL)
    {
        *out_count = frame->capsule_count;
    }
    return &capsules[frame->capsule_offset];
}

const reference_hurt_capsule *
falcon_reference_hurt_capsules_at_frame(
    falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_count)
{
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return NULL;
    }
    return falcon_reference_hurt_track_at_frame(
        &falcon_hurt_moves[move_index],
        falcon_hurt_frames,
        falcon_hurt_capsules,
        action_frame,
        out_count);
}

static uint8_t falcon_ledge_hurt_track_for_action(
    uint8_t action_state,
    uint16_t source_submotion)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_LEDGE_CLIMB:
            if (source_submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_QUICK)
            {
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_CLIMB_QUICK;
            }
            if (source_submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_CLIMB_SLOW)
            {
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_CLIMB_SLOW;
            }
            break;
        case PF_M4_ACTION_LEDGE_ROLL:
            if (source_submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_QUICK)
            {
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_ROLL_QUICK;
            }
            if (source_submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ROLL_SLOW)
            {
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_ROLL_SLOW;
            }
            break;
        case PF_M4_ACTION_LEDGE_ATTACK:
            if (source_submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK)
            {
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_ATTACK_QUICK;
            }
            if (source_submotion ==
                (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW)
            {
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_ATTACK_SLOW;
            }
            break;
        case PF_M4_ACTION_LEDGE_JUMP:
            switch ((falcon_submotion_index)source_submotion)
            {
                case PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_1:
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_JUMP_QUICK_1;
                case PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_2:
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_JUMP_QUICK_2;
                case PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_1:
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_JUMP_SLOW_1;
                case PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_2:
                return (uint8_t)PF_M4_FALCON_LEDGE_HURT_JUMP_SLOW_2;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return UINT8_MAX;
}

const reference_hurt_capsule *
falcon_reference_common_hurt_capsules_at_frame(
    uint8_t action_state,
    uint16_t action_frame,
    uint8_t *out_count)
{
    return
        falcon_reference_common_hurt_capsules_for_submotion_at_frame(
            action_state,
            UINT16_C(0),
            action_frame,
            out_count);
}

static int falcon_copy_dynamic_ground_hurt_capsules(
    const hsd_evaluated_capsule
        evaluated[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t count,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    uint8_t capsule_index;

    if (evaluated == NULL || out_capsules == NULL || out_count == NULL)
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return 0;
    }
    for (capsule_index = UINT8_C(0);
         capsule_index < count;
         ++capsule_index)
    {
        const hsd_evaluated_capsule *source =
            &evaluated[capsule_index];
        reference_hurt_capsule *destination =
            &out_capsules[capsule_index];

        destination->endpoint_a_x_f32 = source->endpoint_a_f32[0];
        destination->endpoint_a_y_f32 = source->endpoint_a_f32[1];
        destination->endpoint_a_z_f32 = source->endpoint_a_f32[2];
        destination->endpoint_b_x_f32 = source->endpoint_b_f32[0];
        destination->endpoint_b_y_f32 = source->endpoint_b_f32[1];
        destination->endpoint_b_z_f32 = source->endpoint_b_f32[2];
        destination->radius_f32 = source->radius_f32;
        destination->hurtbox_id = source->hurtbox_id;
        destination->height = source->height;
        destination->grabbable = source->grabbable;
        destination->reserved = UINT8_C(0);
    }
    *out_count = count;
    return 1;
}

int falcon_reference_hsd_hurt_capsules(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    hsd_evaluated_capsule evaluated[PF_M4_HSD_POSE_MAX_CAPSULES];
    uint8_t count;

    if (!hsd_evaluate_hurt_pose(
            &falcon_dynamic_hsd_data,
            source_submotion,
            source_animation_frame_f32,
            evaluated,
            &count))
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return 0;
    }
    return falcon_copy_dynamic_ground_hurt_capsules(
        evaluated, count, out_capsules, out_count);
}

int falcon_reference_hsd_hurt_capsules_from_local_pose(
    const hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    hsd_evaluated_capsule evaluated[PF_M4_HSD_POSE_MAX_CAPSULES];
    uint8_t count;

    if (!hsd_evaluate_hurt_pose_from_local_pose(
            &falcon_dynamic_hsd_data,
            pose,
            evaluated,
            &count))
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return 0;
    }
    return falcon_copy_dynamic_ground_hurt_capsules(
        evaluated, count, out_capsules, out_count);
}

static int falcon_reference_damage_hsd_local_pose(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    int8_t facing,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    hsd_local_pose out_pose[PF_M4_HSD_POSE_MAX_JOINTS])
{
    enum
    {
        PF_M4_FALCON_HSD_X_ROT_N_JOINT = 2
    };

    if (out_pose == NULL || (facing != INT8_C(-1) && facing != INT8_C(1)) ||
        !hsd_evaluate_local_pose_f32(
            &falcon_dynamic_hsd_data,
            source_submotion,
            source_animation_frame_f32,
            out_pose))
    {
        return 0;
    }
    if (source_submotion ==
        (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL)
    {
        const float source_x = total_velocity_x_f32 * (115.0f / 12.0f);
        const float source_y = -total_velocity_y_f32 * (62.0f / 11.0f);
        const float two_pi = 6.28318530717958647692f;
        /* DamageFlyRoll's callbacks overwrite FtPart_XRotN with
         * facing * atan2(total_x, total_y) in Melee source space.  The
         * imported model's complete part map keeps XRotN at joint two. */
        out_pose[PF_M4_FALCON_HSD_X_ROT_N_JOINT]
            .rotation_f32[0] =
                atan2f(source_x, source_y) / two_pi * (float)facing;
    }
    return 1;
}

int falcon_reference_damage_hsd_hurt_capsules(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    int8_t facing,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];

    return falcon_reference_damage_hsd_local_pose(
               source_submotion,
               source_animation_frame_f32,
               facing,
               total_velocity_x_f32,
               total_velocity_y_f32,
               pose) &&
           falcon_reference_hsd_hurt_capsules_from_local_pose(
               pose, out_capsules, out_count);
}

int falcon_reference_damage_hsd_ecb_pose(
    uint16_t source_submotion,
    float source_animation_frame_f32,
    int8_t facing,
    float total_velocity_x_f32,
    float total_velocity_y_f32,
    int grounded,
    float locked_bottom_y_f32,
    falcon_ecb_pose_f32 *out_pose)
{
    hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS];

    return falcon_reference_damage_hsd_local_pose(
               source_submotion,
               source_animation_frame_f32,
               facing,
               total_velocity_x_f32,
               total_velocity_y_f32,
               pose) &&
           falcon_reference_hsd_ecb_pose_from_local_pose(
               pose, grounded, locked_bottom_y_f32, out_pose);
}

int falcon_reference_retained_hsd_pose(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks,
    float source_animation_frame_f32,
    float *out_frame_f32)
{
    uint16_t expected_submotion;

    if (out_frame_f32 == NULL)
    {
        return 0;
    }
    switch ((enum action_state)action_state)
    {
    case PF_M4_ACTION_AIR_DODGE:
        expected_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_AIR_DODGE;
        break;
    case PF_M4_ACTION_HITSTUN:
    case PF_M4_ACTION_DAMAGE_LOW_1:
    case PF_M4_ACTION_DAMAGE_LOW_2:
    case PF_M4_ACTION_DAMAGE_LOW_3:
        expected_submotion = source_submotion;
        if (source_submotion <
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_HIGH_1 ||
            source_submotion >
                (uint16_t)PF_M4_FALCON_SUBMOTION_DAMAGE_FLY_ROLL)
        {
            return 0;
        }
        break;
    case PF_M4_ACTION_SHIELD_STUN:
        expected_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_SET_OFF;
        break;
    case PF_M4_ACTION_SHIELD_BREAK:
        expected_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_FLY;
        break;
    case PF_M4_ACTION_SHIELD_BREAK_DOWN:
        expected_submotion =
            falcon_reference_shield_break_down_submotion();
        break;
    case PF_M4_ACTION_SHIELD_BREAK_STAND:
        expected_submotion =
            falcon_reference_shield_break_down_submotion() ==
                    (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN
                ? (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN
                : (uint16_t)PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_UP;
        break;
    case PF_M4_ACTION_SHIELD_BREAK_STUN:
        expected_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FURAFURA;
        break;
    default:
        return 0;
    }
    if (source_submotion != expected_submotion)
    {
        return 0;
    }
    *out_frame_f32 =
        action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE ||
            action_state == (uint8_t)PF_M4_ACTION_HITSTUN ||
            action_state == (uint8_t)PF_M4_ACTION_DAMAGE_LOW_1 ||
            action_state == (uint8_t)PF_M4_ACTION_DAMAGE_LOW_2 ||
            action_state == (uint8_t)PF_M4_ACTION_DAMAGE_LOW_3 ||
            action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
            action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN
            ? source_animation_frame_f32
            : (int32_t)action_ticks * 1.0f;
    return 1;
}

int falcon_reference_retained_hsd_hurt_capsules(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks,
    float source_animation_frame_f32,
    reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    float evaluated_frame_f32;

    if (!falcon_reference_retained_hsd_pose(
            action_state,
            source_submotion,
            action_ticks,
            source_animation_frame_f32,
            &evaluated_frame_f32))
    {
        return 0;
    }
    return falcon_reference_hsd_hurt_capsules(
        source_submotion,
        evaluated_frame_f32,
        out_capsules,
        out_count);
}

uint16_t falcon_reference_shield_break_down_submotion(void)
{
    return falcon_dynamic_hsd_pose_branch_shield_break_down_up !=
                   UINT8_C(0)
               ? (uint16_t)
                     PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_UP
               : (uint16_t)
                     PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN;
}

const reference_hurt_capsule *
falcon_reference_common_hurt_capsules_for_submotion_at_frame(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_frame,
    uint8_t *out_count)
{
    falcon_common_hurt_index track_index;
    const uint8_t ledge_track_index =
        falcon_ledge_hurt_track_for_action(
            action_state,
            source_submotion);

    if (ledge_track_index != UINT8_MAX)
    {
        return falcon_reference_hurt_track_at_frame(
            &falcon_ledge_hurt_moves[ledge_track_index],
            falcon_ledge_hurt_frames,
            falcon_ledge_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GRABBED)
    {
        falcon_capture_hurt_index capture_track;

        if (source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_WAIT_HIGH)
        {
            capture_track = PF_M4_FALCON_CAPTURE_HURT_WAIT_HIGH;
        }
        else if (source_submotion ==
                 (uint16_t)PF_M4_FALCON_SUBMOTION_CAPTURE_DAMAGE_HIGH)
        {
            capture_track = PF_M4_FALCON_CAPTURE_HURT_DAMAGE_HIGH;
        }
        else
        {
            capture_track = PF_M4_FALCON_CAPTURE_HURT_COUNT;
        }
        if (capture_track != PF_M4_FALCON_CAPTURE_HURT_COUNT)
        {
            const reference_hurt_move *track =
                &falcon_capture_hurt_moves[capture_track];

            if (capture_track == PF_M4_FALCON_CAPTURE_HURT_WAIT_HIGH &&
                track->frame_count != UINT8_C(0))
            {
                action_frame =
                    (uint16_t)(action_frame % track->frame_count);
            }
            return falcon_reference_hurt_track_at_frame(
                track,
                falcon_hurt_frames,
                falcon_hurt_capsules,
                action_frame,
                out_count);
        }
    }
    if ((action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
         action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN) &&
        (source_submotion ==
             (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON ||
         source_submotion ==
             (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD))
    {
        uint8_t guard_track;

        if (source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_ON)
        {
            guard_track = PF_M4_FALCON_GUARD_HURT_GUARD_ON;
        }
        else if (source_submotion ==
                 (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD)
        {
            guard_track = PF_M4_FALCON_GUARD_HURT_GUARD;
        }
        else
        {
            guard_track = PF_M4_FALCON_GUARD_HURT_COUNT;
        }
        if (guard_track != PF_M4_FALCON_GUARD_HURT_COUNT)
        {
            return falcon_reference_hurt_track_at_frame(
                &falcon_guard_hurt_moves[guard_track],
                falcon_guard_hurt_frames,
                falcon_guard_hurt_capsules,
                action_frame,
                out_count);
        }
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_OFF)
    {
        return falcon_reference_hurt_track_at_frame(
            &falcon_guard_hurt_moves[
                PF_M4_FALCON_GUARD_HURT_GUARD_OFF],
            falcon_guard_hurt_frames,
            falcon_guard_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        uint8_t airborne_track;

        switch ((falcon_submotion_index)source_submotion)
        {
        case PF_M4_FALCON_SUBMOTION_JUMP_FORWARD:
            airborne_track = PF_M4_FALCON_AIRBORNE_HURT_JUMP_FORWARD;
            break;
        case PF_M4_FALCON_SUBMOTION_JUMP_BACKWARD:
            airborne_track = PF_M4_FALCON_AIRBORNE_HURT_JUMP_BACKWARD;
            break;
        case PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD:
            airborne_track =
                PF_M4_FALCON_AIRBORNE_HURT_JUMP_AERIAL_FORWARD;
            break;
        case PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD:
            airborne_track =
                PF_M4_FALCON_AIRBORNE_HURT_JUMP_AERIAL_BACKWARD;
            break;
        case PF_M4_FALCON_SUBMOTION_FALL:
            airborne_track = PF_M4_FALCON_AIRBORNE_HURT_FALL;
            break;
        case PF_M4_FALCON_SUBMOTION_FALL_AERIAL:
            airborne_track = PF_M4_FALCON_AIRBORNE_HURT_FALL_AERIAL;
            break;
        default:
            airborne_track = UINT8_MAX;
            break;
        }
        if (airborne_track != UINT8_MAX)
        {
            return falcon_reference_hurt_track_at_frame(
                &falcon_airborne_hurt_moves[airborne_track],
                falcon_airborne_hurt_frames,
                falcon_airborne_hurt_capsules,
                action_frame,
                out_count);
        }
    }
    if (action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN &&
        source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TURN)
    {
        return falcon_reference_hurt_track_at_frame(
            &falcon_turn_hurt_moves[
                PF_M4_FALCON_TURN_HURT_STANDING_TURN],
            falcon_turn_hurt_frames,
            falcon_turn_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TURN_RUN)
    {
        return falcon_reference_hurt_track_at_frame(
            &falcon_turn_hurt_moves[
                PF_M4_FALCON_TURN_HURT_RUN_TURNAROUND],
            falcon_turn_hurt_frames,
            falcon_turn_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_CROUCH &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT)
    {
        const reference_hurt_move *track =
            &falcon_crouch_taunt_hurt_moves[
                PF_M4_FALCON_CROUCH_TAUNT_HURT_CROUCH_WAIT];

        if (action_frame >= track->first_frame &&
            track->frame_count != UINT8_C(0))
        {
            action_frame =
                (uint16_t)(
                    track->first_frame +
                    (action_frame - track->first_frame) %
                        track->frame_count);
        }
        return falcon_reference_hurt_track_at_frame(
            track,
            falcon_crouch_taunt_hurt_frames,
            falcon_crouch_taunt_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_TAUNT)
    {
        uint8_t taunt_track_index;

        if (source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT)
        {
            taunt_track_index =
                PF_M4_FALCON_CROUCH_TAUNT_HURT_TAUNT_RIGHT;
        }
        else if (source_submotion ==
                 (uint16_t)PF_M4_FALCON_SUBMOTION_APPEAL_LEFT)
        {
            taunt_track_index =
                PF_M4_FALCON_CROUCH_TAUNT_HURT_TAUNT_LEFT;
        }
        else
        {
            taunt_track_index = UINT8_MAX;
        }
        if (taunt_track_index != UINT8_MAX)
        {
            return falcon_reference_hurt_track_at_frame(
                &falcon_crouch_taunt_hurt_moves[
                    taunt_track_index],
                falcon_crouch_taunt_hurt_frames,
                falcon_crouch_taunt_hurt_capsules,
                action_frame,
                out_count);
        }
    }

    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_INITIAL_DASH:
            track_index = PF_M4_FALCON_COMMON_HURT_INITIAL_DASH;
            break;
        case PF_M4_ACTION_RUN_BRAKE:
            track_index = PF_M4_FALCON_COMMON_HURT_RUN_BRAKE;
            break;
        case PF_M4_ACTION_CROUCH_START:
            track_index = PF_M4_FALCON_COMMON_HURT_CROUCH_START;
            break;
        case PF_M4_ACTION_CROUCH_END:
            track_index = PF_M4_FALCON_COMMON_HURT_CROUCH_END;
            break;
        case PF_M4_ACTION_JUMP_SQUAT:
            track_index = PF_M4_FALCON_COMMON_HURT_KNEE_BEND;
            break;
        case PF_M4_ACTION_SPOT_DODGE:
            track_index = PF_M4_FALCON_COMMON_HURT_SPOT_DODGE;
            break;
        case PF_M4_ACTION_ROLL_FORWARD:
            track_index = PF_M4_FALCON_COMMON_HURT_ROLL_FORWARD;
            break;
        case PF_M4_ACTION_ROLL_BACKWARD:
            track_index = PF_M4_FALCON_COMMON_HURT_ROLL_BACKWARD;
            break;
        case PF_M4_ACTION_AIR_DODGE:
            track_index = PF_M4_FALCON_COMMON_HURT_AIR_DODGE;
            break;
        case PF_M4_ACTION_FALL_SPECIAL:
            track_index = PF_M4_FALCON_COMMON_HURT_FALL_SPECIAL;
            break;
        case PF_M4_ACTION_SPECIAL_LANDING:
            track_index =
                PF_M4_FALCON_COMMON_HURT_LANDING_FALL_SPECIAL;
            break;
        case PF_M4_ACTION_LANDING:
            track_index = PF_M4_FALCON_COMMON_HURT_LANDING;
            break;
        default:
            if (out_count != NULL)
            {
                *out_count = UINT8_C(0);
            }
            return NULL;
    }
    return falcon_reference_hurt_track_at_frame(
        &falcon_common_hurt_moves[track_index],
        falcon_hurt_frames,
        falcon_hurt_capsules,
        action_frame,
        out_count);
}

int falcon_reference_move_for_action(
    uint8_t action_state,
    falcon_move_index *out_move_index)
{
    falcon_move_index move_index;

    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
            move_index = PF_M4_FALCON_JAB1;
            break;
        case PF_M4_ACTION_JAB_FINAL:
            move_index = PF_M4_FALCON_JAB2;
            break;
        case PF_M4_ACTION_JAB_THIRD:
            move_index = PF_M4_FALCON_JAB3;
            break;
        case PF_M4_ACTION_RAPID_JAB_START:
            move_index = PF_M4_FALCON_RAPID_JABS_START;
            break;
        case PF_M4_ACTION_RAPID_JAB_LOOP:
            move_index = PF_M4_FALCON_RAPID_JABS_LOOP;
            break;
        case PF_M4_ACTION_RAPID_JAB_END:
            move_index = PF_M4_FALCON_RAPID_JABS_END;
            break;
        case PF_M4_ACTION_DASH_ATTACK:
            move_index = PF_M4_FALCON_DASH_ATTACK;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK:
            move_index = PF_M4_FALCON_FORWARD_TILT;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK_HIGH:
            move_index = PF_M4_FALCON_FORWARD_TILT_HIGH;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH:
            move_index = PF_M4_FALCON_FORWARD_TILT_MID_HIGH;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK_MID_LOW:
            move_index = PF_M4_FALCON_FORWARD_TILT_MID_LOW;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK_LOW:
            move_index = PF_M4_FALCON_FORWARD_TILT_LOW;
            break;
        case PF_M4_ACTION_UP_ATTACK:
            move_index = PF_M4_FALCON_UP_TILT;
            break;
        case PF_M4_ACTION_DOWN_ATTACK:
            move_index = PF_M4_FALCON_DOWN_TILT;
            break;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
            move_index = PF_M4_FALCON_FORWARD_SMASH;
            break;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
            move_index = PF_M4_FALCON_FORWARD_SMASH_HIGH;
            break;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            move_index = PF_M4_FALCON_FORWARD_SMASH_LOW;
            break;
        case PF_M4_ACTION_UP_STRONG_ATTACK:
            move_index = PF_M4_FALCON_UP_SMASH;
            break;
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
            move_index = PF_M4_FALCON_DOWN_SMASH;
            break;
        case PF_M4_ACTION_AERIAL_ATTACK:
            move_index = PF_M4_FALCON_NEUTRAL_AERIAL;
            break;
        case PF_M4_ACTION_FORWARD_AERIAL:
            move_index = PF_M4_FALCON_FORWARD_AERIAL;
            break;
        case PF_M4_ACTION_BACK_AERIAL:
            move_index = PF_M4_FALCON_BACK_AERIAL;
            break;
        case PF_M4_ACTION_UP_AERIAL:
            move_index = PF_M4_FALCON_UP_AERIAL;
            break;
        case PF_M4_ACTION_DOWN_AERIAL:
            move_index = PF_M4_FALCON_DOWN_AERIAL;
            break;
        case PF_M4_ACTION_GRAB:
            move_index = PF_M4_FALCON_GRAB;
            break;
        case PF_M4_ACTION_DASH_GRAB:
            move_index = PF_M4_FALCON_DASH_GRAB;
            break;
        case PF_M4_ACTION_PUMMEL:
            move_index = PF_M4_FALCON_PUMMEL;
            break;
        case PF_M4_ACTION_THROW_FORWARD:
            move_index = PF_M4_FALCON_FORWARD_THROW;
            break;
        case PF_M4_ACTION_THROW_BACK:
            move_index = PF_M4_FALCON_BACK_THROW;
            break;
        case PF_M4_ACTION_THROW_UP:
            move_index = PF_M4_FALCON_UP_THROW;
            break;
        case PF_M4_ACTION_THROW_DOWN:
            move_index = PF_M4_FALCON_DOWN_THROW;
            break;
        case PF_M4_ACTION_FALCON_PUNCH_GROUND:
            move_index = PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND;
            break;
        case PF_M4_ACTION_FALCON_PUNCH_AIR:
            move_index = PF_M4_FALCON_NEUTRAL_SPECIAL_AIR;
            break;
        case PF_M4_ACTION_RAPTOR_BOOST_START_GROUND:
            move_index = PF_M4_FALCON_SIDE_SPECIAL_START_GROUND;
            break;
        case PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND:
            move_index = PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND;
            break;
        case PF_M4_ACTION_RAPTOR_BOOST_START_AIR:
            move_index = PF_M4_FALCON_SIDE_SPECIAL_START_AIR;
            break;
        case PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR:
            move_index = PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR;
            break;
        case PF_M4_ACTION_FALCON_DIVE_START_GROUND:
            move_index = PF_M4_FALCON_UP_SPECIAL_GROUND;
            break;
        case PF_M4_ACTION_FALCON_DIVE_START_AIR:
            move_index = PF_M4_FALCON_UP_SPECIAL_AIR;
            break;
        case PF_M4_ACTION_FALCON_DIVE_CATCH:
            move_index = PF_M4_FALCON_UP_SPECIAL_CATCH;
            break;
        case PF_M4_ACTION_FALCON_DIVE_THROW:
            move_index = PF_M4_FALCON_UP_SPECIAL_THROW;
            break;
        case PF_M4_ACTION_FALCON_KICK_START_GROUND:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_GROUND;
            break;
        case PF_M4_ACTION_FALCON_KICK_END_GROUND:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_END_GROUND;
            break;
        case PF_M4_ACTION_FALCON_KICK_START_AIR:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_AIR;
            break;
        case PF_M4_ACTION_FALCON_KICK_LANDING:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_LANDING_HIT;
            break;
        case PF_M4_ACTION_FALCON_KICK_END_AIR_FROM_GROUND:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_END_AIR_FROM_GROUND;
            break;
        case PF_M4_ACTION_FALCON_KICK_END_AIR:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_END_AIR;
            break;
        case PF_M4_ACTION_FALCON_KICK_WALL_REBOUND:
            move_index = PF_M4_FALCON_DOWN_SPECIAL_WALL_REBOUND;
            break;
        default:
            return 0;
    }
    if (out_move_index != NULL)
    {
        *out_move_index = move_index;
    }
    return 1;
}

const reference_throw *falcon_reference_throw(
    falcon_move_index move_index)
{
    const struct reference_move *move =
        falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        move->throw_index == UINT16_MAX)
    {
        return NULL;
    }
    return &falcon_throws[move->throw_index];
}

reference_timing falcon_reference_timing(
    falcon_move_index move_index)
{
    const struct reference_move *move =
        falcon_reference_move(move_index);
    reference_timing timing = {0};
    const reference_hit_phase *first;
    const reference_hit_phase *last;

    if (move == NULL || move->present == UINT8_C(0) ||
        move->phase_count == UINT8_C(0))
    {
        return timing;
    }
    first = falcon_reference_phase(move_index, UINT16_C(0));
    last = falcon_reference_phase(
        move_index,
        (uint16_t)(move->phase_count - UINT8_C(1)));
    if (first == NULL || last == NULL)
    {
        return timing;
    }
    timing.startup_ticks = first->first_frame - UINT16_C(1);
    timing.active_ticks = (uint16_t)(
        (uint32_t)last->last_frame -
        (uint32_t)first->first_frame + UINT32_C(1));
    timing.recovery_ticks = move->total_frames - last->last_frame;
    return timing;
}

const struct reference_move *falcon_reference_attack(
    uint8_t action_state,
    reference_timing authored_timing,
    float authored_damage_f32)
{
    falcon_move_index move_index;
    const struct reference_move *move;
    const reference_hit_effect *effect;
    reference_timing reference;

    if (!falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return NULL;
    }
    move = falcon_reference_move(move_index);
    effect = falcon_reference_primary_effect(move_index);
    reference = falcon_reference_timing(move_index);
    if (move == NULL || effect == NULL ||
        authored_timing.startup_ticks != reference.startup_ticks ||
        authored_timing.active_ticks != reference.active_ticks ||
        authored_timing.recovery_ticks != reference.recovery_ticks ||
        authored_damage_f32 != (float)effect->damage)
    {
        return NULL;
    }
    return move;
}

int falcon_reference_attack_matches(
    uint8_t action_state,
    reference_timing authored_timing,
    float authored_damage_f32)
{
    return falcon_reference_attack(
               action_state,
               authored_timing,
               authored_damage_f32) != NULL;
}

reference_iasa_policy falcon_reference_iasa_policy_for_action(
    uint8_t action_state)
{
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
        case PF_M4_ACTION_JAB_FINAL:
            return PF_M4_REFERENCE_IASA_JAB_CHAIN;
        case PF_M4_ACTION_JAB_THIRD:
            return PF_M4_REFERENCE_IASA_WAIT;
        case PF_M4_ACTION_DASH_ATTACK:
        case PF_M4_ACTION_FORWARD_ATTACK:
        case PF_M4_ACTION_FORWARD_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH:
        case PF_M4_ACTION_FORWARD_ATTACK_MID_LOW:
        case PF_M4_ACTION_FORWARD_ATTACK_LOW:
        case PF_M4_ACTION_UP_ATTACK:
        case PF_M4_ACTION_UP_STRONG_ATTACK:
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
            return PF_M4_REFERENCE_IASA_WAIT;
        case PF_M4_ACTION_DOWN_ATTACK:
            return PF_M4_REFERENCE_IASA_DOWN_TILT;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            return PF_M4_REFERENCE_IASA_FORWARD_SMASH;
        default:
            return PF_M4_REFERENCE_IASA_NONE;
    }
}

reference_ground_physics
falcon_reference_ground_physics_for_action(uint8_t action_state)
{
    /*
     * These are the common-action callbacks which call ft_80084FA8 or
     * ft_80085030. The other grounded normals use ft_80084F3C and therefore
     * apply ground friction without consuming animation translation.
     */
    switch ((enum action_state)action_state)
    {
        case PF_M4_ACTION_GROUND_ATTACK:
        case PF_M4_ACTION_JAB_FINAL:
        case PF_M4_ACTION_JAB_THIRD:
        case PF_M4_ACTION_RAPID_JAB_START:
        case PF_M4_ACTION_RAPID_JAB_LOOP:
        case PF_M4_ACTION_RAPID_JAB_END:
        case PF_M4_ACTION_DASH_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW:
            return PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION;
        default:
            return PF_M4_REFERENCE_GROUND_PHYSICS_FRICTION;
    }
}

int falcon_reference_iasa_active(
    uint8_t action_state,
    uint32_t displayed_frame)
{
    falcon_move_index move_index;
    const struct reference_move *move;

    if (!falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = falcon_reference_move(move_index);
    return move != NULL && move->iasa_frame != UINT16_C(0) &&
           displayed_frame >= (uint32_t)move->iasa_frame;
}

int falcon_reference_special_iasa_active(
    uint8_t action_state,
    uint16_t action_ticks)
{
    const reference_iasa_policy policy =
        falcon_reference_iasa_policy_for_action(action_state);

    return (policy == PF_M4_REFERENCE_IASA_WAIT ||
            policy == PF_M4_REFERENCE_IASA_FORWARD_SMASH) &&
           falcon_reference_iasa_active(
               action_state,
               (uint32_t)action_ticks + UINT32_C(1));
}

int falcon_reference_translation_f32(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    float *out_translation_x_f32,
    float *out_translation_y_f32)
{
    const falcon_submotion_data *submotion;
    uint32_t sample_index;

    if (displayed_frame == UINT16_C(0))
    {
        return 0;
    }
    submotion = falcon_reference_submotion(submotion_index);
    if (submotion == NULL || displayed_frame > submotion->translation_count)
    {
        return 0;
    }
    sample_index = (uint32_t)submotion->translation_offset +
                   (uint32_t)displayed_frame - UINT32_C(1);
    if (sample_index >= (uint32_t)PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT)
    {
        return 0;
    }
    if (out_translation_x_f32 != NULL)
    {
        *out_translation_x_f32 =
            falcon_translation_x_f32[sample_index];
    }
    if (out_translation_y_f32 != NULL)
    {
        *out_translation_y_f32 =
            falcon_translation_y_f32[sample_index];
    }
    return 1;
}

int falcon_reference_motion_x_f32(
    uint8_t action_state,
    uint16_t action_frame,
    float *out_motion_x_f32)
{
    falcon_move_index move_index;
    const struct reference_move *move;

    if (action_frame == UINT16_C(0) ||
        !falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = falcon_reference_move(move_index);
    if (move == NULL)
    {
        return 0;
    }
    return falcon_reference_translation_f32(
        move->subaction_index,
        action_frame,
        out_motion_x_f32,
        NULL);
}

int falcon_reference_motion_y_f32(
    uint8_t action_state,
    uint16_t action_frame,
    float *out_motion_y_f32)
{
    falcon_move_index move_index;
    const struct reference_move *move;

    if (action_frame == UINT16_C(0) ||
        !falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = falcon_reference_move(move_index);
    if (move == NULL)
    {
        return 0;
    }
    return falcon_reference_translation_f32(
        move->subaction_index,
        action_frame,
        NULL,
        out_motion_y_f32);
}

int falcon_reference_landing_lag_active(
    uint8_t action_state,
    uint16_t action_frame)
{
    falcon_move_index move_index;
    const struct reference_move *move;

    if (!falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return -1;
    }
    move = falcon_reference_move(move_index);
    if (move == NULL || move->landing_lag == UINT16_C(0) ||
        move->autocancel_before == UINT16_C(0) ||
        move->autocancel_after == UINT16_C(0))
    {
        return -1;
    }
    return action_frame >= move->autocancel_before &&
           action_frame <= move->autocancel_after;
}
