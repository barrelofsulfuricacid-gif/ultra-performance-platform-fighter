#include "sim_falcon_frame_data.h"
#include "sim_hsd_pose.h"

#include "pf/m4.h"

#include <stddef.h>
#include <stdint.h>

#include "../../generated/data/m4_falcon_ntsc102_frame_data.inc"
#include "../../generated/data/m4_falcon_ntsc102_hit_geometry.inc"
#include "../../generated/data/m4_ssbm_falcon_ledge_hurt.inc"
#include "../../generated/data/m4_ssbm_falcon_airborne_hurt.inc"
#include "../../generated/data/m4_ssbm_falcon_turn_hurt.inc"
#include "../../generated/data/m4_ssbm_falcon_crouch_taunt_hurt.inc"
#include "../../generated/data/m4_ssbm_falcon_guard_hurt.inc"
#include "../../generated/data/m4_ssbm_falcon_ground_loop_hsd.inc"

_Static_assert(
    (size_t)PF_M4_FALCON_LEDGE_HURT_COUNT ==
        (size_t)PF_M4_FALCON_LEDGE_HURT_TRACK_COUNT,
    "Falcon ledge hurt-pose manifest and runtime binding disagree");

_Static_assert(
    sizeof(pf_m4_falcon_moves) / sizeof(pf_m4_falcon_moves[0]) ==
        (size_t)PF_M4_FALCON_MOVE_COUNT,
    "Falcon move table must cover every indexed move");
_Static_assert(
    sizeof(pf_m4_falcon_hurt_moves) /
            sizeof(pf_m4_falcon_hurt_moves[0]) ==
        (size_t)PF_M4_FALCON_MOVE_COUNT,
    "Falcon hurt-pose table must cover every indexed move");
_Static_assert(
    sizeof(pf_m4_falcon_common_attribute_bits) /
            sizeof(pf_m4_falcon_common_attribute_bits[0]) ==
        (size_t)PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT,
    "Falcon common-attribute table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_submotions) / sizeof(pf_m4_falcon_submotions[0]) ==
        (size_t)PF_M4_FALCON_SUBMOTION_COUNT,
    "Falcon submotion table must cover every source slot");
_Static_assert(
    sizeof(pf_m4_falcon_script_events) /
            sizeof(pf_m4_falcon_script_events[0]) ==
        (size_t)PF_M4_FALCON_SCRIPT_EVENT_COUNT,
    "Falcon action-script event table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_body_collision_timings) /
            sizeof(pf_m4_falcon_body_collision_timings[0]) ==
        (size_t)PF_M4_FALCON_SUBMOTION_COUNT,
    "Falcon body-collision timing table must cover every source slot");
_Static_assert(
    sizeof(pf_m4_falcon_translation_x_q16) /
            sizeof(pf_m4_falcon_translation_x_q16[0]) ==
        (size_t)PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT,
    "Falcon X translation table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_translation_y_q16) /
            sizeof(pf_m4_falcon_translation_y_q16[0]) ==
        (size_t)PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT,
    "Falcon Y translation table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_script_bytes) /
            sizeof(pf_m4_falcon_script_bytes[0]) ==
        (size_t)PF_M4_FALCON_SCRIPT_BYTE_COUNT,
    "Falcon action-script byte table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_special_attributes) == (size_t)0x8c,
    "Falcon special-attribute view must cover the source block exactly");
_Static_assert(
    PF_M4_MELEE_STALE_MOVE_SLOT_COUNT == PF_SIM_STALE_MOVE_QUEUE_CAPACITY,
    "Imported Melee stale-move table must cover the runtime queue");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.crouch_wait) /
            sizeof(pf_m4_falcon_collision_pose_data.crouch_wait[0]) ==
        (size_t)PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT,
    "Falcon CrouchWait ECB cycle must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.airborne) /
            sizeof(pf_m4_falcon_collision_pose_data.airborne[0]) ==
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
    sizeof(pf_m4_falcon_collision_pose_data
               .aerial_attack_bottom_y_from_origin_q16) /
            sizeof(pf_m4_falcon_collision_pose_data
                       .aerial_attack_bottom_y_from_origin_q16[0]) ==
        (size_t)PF_M4_FALCON_AERIAL_ATTACK_ECB_FRAME_COUNT,
    "Falcon aerial-attack ECB table must be complete");
_Static_assert(
    PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_FORWARD_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_BACK_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_UP_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_DOWN_AERIAL_ECB_FRAME_COUNT ==
        PF_M4_FALCON_AERIAL_ATTACK_ECB_FRAME_COUNT,
    "Falcon aerial-attack ECB spans must cover the packed table");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.shield_break_fly) /
            sizeof(pf_m4_falcon_collision_pose_data.shield_break_fly[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_FLY_ECB_FRAME_COUNT,
    "Falcon ShieldBreakFly ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.shield_break_down_down) /
            sizeof(pf_m4_falcon_collision_pose_data
                       .shield_break_down_down[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_DOWN_ECB_FRAME_COUNT,
    "Falcon ShieldBreakDownD ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.shield_break_stand_down) /
            sizeof(pf_m4_falcon_collision_pose_data
                       .shield_break_stand_down[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_STAND_ECB_FRAME_COUNT,
    "Falcon ShieldBreakStandD ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.shield_break_stun) /
            sizeof(pf_m4_falcon_collision_pose_data.shield_break_stun[0]) ==
        (size_t)PF_M4_FALCON_SHIELD_BREAK_STUN_ECB_FRAME_COUNT,
    "Falcon Furafura ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.guard_on) /
            sizeof(pf_m4_falcon_collision_pose_data.guard_on[0]) ==
        (size_t)PF_M4_FALCON_GUARD_ON_FRAME_COUNT,
    "Falcon GuardOn ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.guard) /
            sizeof(pf_m4_falcon_collision_pose_data.guard[0]) ==
        (size_t)PF_M4_FALCON_GUARD_FRAME_COUNT,
    "Falcon Guard ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.guard_off) /
            sizeof(pf_m4_falcon_collision_pose_data.guard_off[0]) ==
        (size_t)PF_M4_FALCON_GUARD_OFF_FRAME_COUNT,
    "Falcon GuardOff ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.ceiling_bounce) /
            sizeof(pf_m4_falcon_collision_pose_data.ceiling_bounce[0]) ==
        (size_t)PF_M4_FALCON_CEILING_BOUNCE_ECB_FRAME_COUNT,
    "Falcon ceiling-bounce ECB table must be complete");
_Static_assert(
    sizeof(pf_m4_falcon_collision_pose_data.wall_bounce) /
            sizeof(pf_m4_falcon_collision_pose_data.wall_bounce[0]) ==
        (size_t)PF_M4_FALCON_WALL_BOUNCE_ECB_FRAME_COUNT,
    "Falcon wall-bounce ECB table must be complete");

const uint8_t *pf_m4_falcon_reference_source_sha256(void)
{
    return pf_m4_falcon_source_sha256;
}

const uint8_t *pf_m4_falcon_reference_complete_source_sha256(void)
{
    return pf_m4_falcon_complete_source_sha256;
}

const uint8_t *pf_m4_falcon_reference_submotion_catalog_sha256(void)
{
    return pf_m4_falcon_submotion_catalog_sha256;
}

const uint8_t *pf_m4_falcon_reference_action_script_sha256(void)
{
    return pf_m4_falcon_action_script_sha256;
}

const uint8_t *pf_m4_falcon_reference_animation_tracks_sha256(void)
{
    return pf_m4_falcon_animation_tracks_sha256;
}

const pf_m4_falcon_animation_decode_summary *
pf_m4_falcon_reference_animation_decode_summary(void)
{
    return &pf_m4_falcon_animation_decode_summary_data;
}

const pf_m4_falcon_submotion_data *pf_m4_falcon_reference_submotion(
    uint16_t submotion_index)
{
    if (submotion_index >= PF_M4_FALCON_SUBMOTION_COUNT)
    {
        return NULL;
    }
    return &pf_m4_falcon_submotions[submotion_index];
}

const pf_m4_falcon_script_event *pf_m4_falcon_reference_submotion_event(
    uint16_t submotion_index,
    uint16_t event_index,
    const uint8_t **out_bytes)
{
    const pf_m4_falcon_submotion_data *submotion;
    const pf_m4_falcon_script_event *event;

    if (out_bytes != NULL)
    {
        *out_bytes = NULL;
    }
    submotion = pf_m4_falcon_reference_submotion(submotion_index);
    if (submotion == NULL || event_index >= submotion->event_count)
    {
        return NULL;
    }
    event = &pf_m4_falcon_script_events[
        (uint32_t)submotion->event_offset + (uint32_t)event_index];
    if (out_bytes != NULL)
    {
        *out_bytes = &pf_m4_falcon_script_bytes[event->byte_offset];
    }
    return event;
}

const pf_m4_falcon_body_collision_timing *
pf_m4_falcon_reference_body_collision_timing(uint16_t submotion_index)
{
    if (submotion_index >= PF_M4_FALCON_SUBMOTION_COUNT)
    {
        return NULL;
    }
    return &pf_m4_falcon_body_collision_timings[submotion_index];
}

const uint32_t *pf_m4_falcon_reference_common_attribute_bits(
    uint16_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT;
    }
    return pf_m4_falcon_common_attribute_bits;
}

const pf_m4_falcon_common_attributes *
pf_m4_falcon_reference_common_attributes(void)
{
    return &pf_m4_falcon_common_attribute_data;
}

const pf_m4_falcon_ledge_attributes *
pf_m4_falcon_reference_ledge_attributes(void)
{
    return &pf_m4_falcon_ledge_attribute_data;
}

const pf_m4_falcon_ledge_root_positions *
pf_m4_falcon_reference_ledge_root_positions(void)
{
    return &pf_m4_falcon_ledge_root_position_data;
}

const pf_m4_falcon_ledge_attack_reference *
pf_m4_falcon_reference_ledge_attack(uint16_t submotion_index)
{
    if (submotion_index <
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW ||
        submotion_index >
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK)
    {
        return NULL;
    }
    return &pf_m4_falcon_ledge_attack_references[
        submotion_index -
        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW];
}

int pf_m4_falcon_reference_ledge_option_anchor_q16(
    uint16_t submotion_index,
    int32_t *out_x_q16,
    int32_t *out_y_q16)
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
    if (out_x_q16 != NULL)
    {
        *out_x_q16 =
            pf_m4_falcon_ledge_root_position_data
                .option_frame_one_x_q16[option_index];
    }
    if (out_y_q16 != NULL)
    {
        *out_y_q16 =
            pf_m4_falcon_ledge_root_position_data
                .option_frame_one_y_q16[option_index];
    }
    return 1;
}

uint16_t pf_m4_falcon_reference_ledge_option_ground_frame(
    uint16_t submotion_index)
{
    if (submotion_index < PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST ||
        submotion_index >=
            PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST +
                PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_COUNT)
    {
        return UINT16_C(0);
    }
    return pf_m4_falcon_ledge_root_position_data.option_ground_frame[
        submotion_index - PF_M4_FALCON_LEDGE_OPTION_SUBMOTION_FIRST];
}

int pf_m4_falcon_reference_hyrule_ledge_jump_position_q16(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    int32_t *out_x_from_wait_q16,
    int32_t *out_y_from_wait_q16)
{
    const int32_t (*path_q16)[2];
    uint16_t frame_count;

    if (submotion_index ==
        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_QUICK_1)
    {
        path_q16 = pf_m4_falcon_hyrule_ledge_jump1_quick_from_wait_q16;
        frame_count = PF_M4_FALCON_LEDGE_JUMP1_QUICK_FRAME_COUNT;
    }
    else if (submotion_index ==
             (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_JUMP_SLOW_1)
    {
        path_q16 = pf_m4_falcon_hyrule_ledge_jump1_slow_from_wait_q16;
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

        if (out_x_from_wait_q16 != NULL)
        {
            *out_x_from_wait_q16 =
                pf_m4_falcon_hyrule_ledge_jump2_frame_one_from_wait_q16
                    [phase_two_index][0];
        }
        if (out_y_from_wait_q16 != NULL)
        {
            *out_y_from_wait_q16 =
                pf_m4_falcon_hyrule_ledge_jump2_frame_one_from_wait_q16
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
    if (out_x_from_wait_q16 != NULL)
    {
        *out_x_from_wait_q16 = path_q16[displayed_frame - UINT16_C(1)][0];
    }
    if (out_y_from_wait_q16 != NULL)
    {
        *out_y_from_wait_q16 = path_q16[displayed_frame - UINT16_C(1)][1];
    }
    return 1;
}

int pf_m4_falcon_reference_body_invulnerable(
    uint16_t submotion_index,
    uint16_t action_ticks)
{
    const pf_m4_falcon_body_collision_timing *timing =
        pf_m4_falcon_reference_body_collision_timing(submotion_index);
    const uint32_t displayed_frame =
        (uint32_t)action_ticks + UINT32_C(1);

    return timing != NULL &&
           timing->state_two_frame != UINT16_MAX &&
           displayed_frame >= (uint32_t)timing->state_two_frame &&
           (timing->state_zero_frame == UINT16_MAX ||
            displayed_frame < (uint32_t)timing->state_zero_frame);
}

const pf_m4_falcon_special_attributes *
pf_m4_falcon_reference_special_attributes(void)
{
    return &pf_m4_falcon_special_attribute_data;
}

const pf_m4_falcon_common_special_attributes *
pf_m4_falcon_reference_common_special_attributes(void)
{
    return &pf_m4_falcon_common_special_attribute_data;
}

const pf_m4_falcon_air_dodge_attributes *
pf_m4_falcon_reference_air_dodge_attributes(void)
{
    return &pf_m4_falcon_air_dodge_attribute_data;
}

const pf_m4_melee_stale_move_data *
pf_m4_falcon_reference_stale_move_data(void)
{
    return &pf_m4_melee_stale_move_data_source;
}

const pf_m4_falcon_smash_charge_attributes *
pf_m4_falcon_reference_smash_charge_attributes(void)
{
    return &pf_m4_falcon_smash_charge_attributes_source;
}

const pf_m4_falcon_neutral_special_timing *
pf_m4_falcon_reference_neutral_special_timing(void)
{
    return &pf_m4_falcon_neutral_special_timing_data;
}

const pf_m4_falcon_side_special_timing *
pf_m4_falcon_reference_side_special_timing(void)
{
    return &pf_m4_falcon_side_special_timing_data;
}

const pf_m4_falcon_up_special_timing *
pf_m4_falcon_reference_up_special_timing(void)
{
    return &pf_m4_falcon_up_special_timing_data;
}

const pf_m4_falcon_down_special_timing *
pf_m4_falcon_reference_down_special_timing(void)
{
    return &pf_m4_falcon_down_special_timing_data;
}

const pf_m4_falcon_collision_pose *
pf_m4_falcon_reference_collision_pose(void)
{
    return &pf_m4_falcon_collision_pose_data;
}

const pf_m4_falcon_ecb_pose_q16 *
pf_m4_falcon_reference_guard_ecb_pose(
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
            return &pf_m4_falcon_collision_pose_data.guard_on[frame];
        }
        if (source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD)
        {
            return &pf_m4_falcon_collision_pose_data.guard[0];
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
        return &pf_m4_falcon_collision_pose_data.guard_off[frame];
    }
    return NULL;
}

static int pf_m4_falcon_prone_pose_orientation_index(
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

const pf_m4_falcon_ecb_pose_q16 *
pf_m4_falcon_reference_prone_ecb_pose(
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
        pf_m4_falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index = action_ticks < PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT
                          ? action_ticks
                          : (uint16_t)(
                                PF_M4_FALCON_DOWN_BOUND_ECB_FRAME_COUNT -
                                UINT16_C(1));
        return &pf_m4_falcon_collision_pose_data
                    .down_bound[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT &&
        pf_m4_falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index = (uint16_t)(
            (action_ticks + UINT16_C(1)) %
            PF_M4_FALCON_DOWN_WAIT_ECB_FRAME_COUNT);
        return &pf_m4_falcon_collision_pose_data
                    .down_wait[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL &&
        pf_m4_falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index =
            action_ticks < PF_M4_FALCON_GETUP_NEUTRAL_ECB_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(
                      PF_M4_FALCON_GETUP_NEUTRAL_ECB_FRAME_COUNT -
                      UINT16_C(1));
        return &pf_m4_falcon_collision_pose_data
                    .getup_neutral[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
        pf_m4_falcon_prone_pose_orientation_index(
            prone_orientation,
            &orientation_index))
    {
        frame_index = action_ticks < PF_M4_FALCON_GETUP_ATTACK_ECB_FRAME_COUNT
                          ? action_ticks
                          : (uint16_t)(
                                PF_M4_FALCON_GETUP_ATTACK_ECB_FRAME_COUNT -
                                UINT16_C(1));
        return &pf_m4_falcon_collision_pose_data
                    .getup_attack[orientation_index][frame_index];
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL &&
        (tech_direction == INT8_C(-1) || tech_direction == INT8_C(1)) &&
        (facing == INT8_C(-1) || facing == INT8_C(1)) &&
        pf_m4_falcon_prone_pose_orientation_index(
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
        return &pf_m4_falcon_collision_pose_data
                    .getup_roll[orientation_index][direction_index][frame_index];
    }
    return NULL;
}

static int32_t pf_m4_falcon_ecb_source_scale_q16(
    int32_t value_q16,
    int32_t numerator,
    int32_t denominator)
{
    const int64_t scaled = (int64_t)value_q16 * (int64_t)numerator;

    return (int32_t)(scaled < INT64_C(0)
                         ? -((-scaled + denominator / INT32_C(2)) /
                             denominator)
                         : (scaled + denominator / INT32_C(2)) /
                               denominator);
}

enum
{
    PF_M4_FALCON_HSD_ECB_JOINT_COUNT =
        sizeof(pf_m4_falcon_dynamic_hsd_ecb_joint_indices) /
        sizeof(pf_m4_falcon_dynamic_hsd_ecb_joint_indices[0]),
    PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT =
        PF_M4_FALCON_HSD_ECB_JOINT_COUNT + 1
};

static inline int pf_m4_falcon_hsd_ecb_evaluation_joints(
    uint8_t joint_indices[PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT])
{
    uint8_t joint_index;

    if (joint_indices == NULL ||
        pf_m4_falcon_dynamic_hsd_data.copy_target_joint_count !=
            UINT8_C(1))
    {
        return 0;
    }
    for (joint_index = UINT8_C(0);
         joint_index < PF_M4_FALCON_HSD_ECB_JOINT_COUNT;
         ++joint_index)
    {
        joint_indices[joint_index] =
            pf_m4_falcon_dynamic_hsd_ecb_joint_indices[joint_index];
    }
    joint_indices[PF_M4_FALCON_HSD_ECB_JOINT_COUNT] =
        pf_m4_falcon_dynamic_hsd_data.copy_target_joint_indices[0];
    return 1;
}

int pf_m4_falcon_reference_ecb_apply_bottom_lock_q16(
    int32_t locked_bottom_y_q16,
    pf_m4_falcon_ecb_pose_q16 *pose)
{
    const int32_t one_x_q16 = pf_m4_falcon_ecb_source_scale_q16(
        INT32_C(65536), INT32_C(12), INT32_C(115));
    const int32_t one_y_q16 = pf_m4_falcon_ecb_source_scale_q16(
        INT32_C(65536), INT32_C(11), INT32_C(62));
    const int32_t epsilon_y_q16 = pf_m4_falcon_ecb_source_scale_q16(
        INT32_C(66), INT32_C(11), INT32_C(62));

    if (pose == NULL)
    {
        return 0;
    }
    if (locked_bottom_y_q16 != PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16)
    {
        pose->bottom_y_from_origin_q16 = locked_bottom_y_q16;
    }
    if ((int64_t)pose->top_y_from_origin_q16 -
            (int64_t)pose->bottom_y_from_origin_q16 <
            (int64_t)one_y_q16 &&
        (int64_t)pose->bottom_y_from_origin_q16 -
            (int64_t)pose->top_y_from_origin_q16 <
            (int64_t)one_y_q16)
    {
        pose->top_y_from_origin_q16 += one_y_q16;
        pose->right_y_from_origin_q16 =
            (pose->top_y_from_origin_q16 +
             pose->bottom_y_from_origin_q16) /
            INT32_C(2);
        pose->left_y_from_origin_q16 = pose->right_y_from_origin_q16;
    }
    if (pose->top_y_from_origin_q16 < one_y_q16)
    {
        pose->top_y_from_origin_q16 = one_y_q16;
    }
    if (pose->left_x_from_origin_q16 > -one_x_q16)
    {
        pose->left_x_from_origin_q16 = -one_x_q16;
    }
    if (pose->right_x_from_origin_q16 < one_x_q16)
    {
        pose->right_x_from_origin_q16 = one_x_q16;
    }
    if (pose->top_y_from_origin_q16 < pose->bottom_y_from_origin_q16)
    {
        pose->top_y_from_origin_q16 =
            pose->bottom_y_from_origin_q16 + one_y_q16;
    }
    if (pose->right_y_from_origin_q16 > pose->top_y_from_origin_q16 ||
        pose->right_y_from_origin_q16 < pose->bottom_y_from_origin_q16)
    {
        pose->right_y_from_origin_q16 =
            (pose->top_y_from_origin_q16 +
             pose->bottom_y_from_origin_q16) /
            INT32_C(2);
        pose->left_y_from_origin_q16 = pose->right_y_from_origin_q16;
    }
    if (pose->top_y_from_origin_q16 - pose->right_y_from_origin_q16 <
            epsilon_y_q16 ||
        pose->right_y_from_origin_q16 - pose->bottom_y_from_origin_q16 <
            epsilon_y_q16)
    {
        pose->right_y_from_origin_q16 =
            (pose->top_y_from_origin_q16 +
             pose->bottom_y_from_origin_q16) /
            INT32_C(2);
    }
    if (pose->top_y_from_origin_q16 - pose->left_y_from_origin_q16 <
            epsilon_y_q16 ||
        pose->left_y_from_origin_q16 - pose->bottom_y_from_origin_q16 <
            epsilon_y_q16)
    {
        pose->left_y_from_origin_q16 =
            (pose->top_y_from_origin_q16 +
             pose->bottom_y_from_origin_q16) /
            INT32_C(2);
    }
    return 1;
}

static int pf_m4_falcon_hsd_ecb_from_origins(
    const int32_t *origins_q16,
    uint8_t origin_count,
    const int32_t reference_origin_q16[3],
    int grounded,
    int32_t locked_bottom_y_q16,
    pf_m4_falcon_ecb_pose_q16 *out_pose)
{
    int32_t left_q16;
    int32_t right_q16;
    int32_t bottom_q16;
    int32_t top_q16;
    int32_t side_y_q16;
    uint8_t index;

    if (origins_q16 == NULL || origin_count == UINT8_C(0) ||
        reference_origin_q16 == NULL || out_pose == NULL)
    {
        return 0;
    }
    left_q16 = right_q16 =
        origins_q16[0] - reference_origin_q16[0];
    bottom_q16 = top_q16 =
        origins_q16[1] - reference_origin_q16[1];
    for (index = UINT8_C(1); index < origin_count; ++index)
    {
        const size_t offset = (size_t)index * (size_t)3;

        const int32_t x_q16 =
            origins_q16[offset] - reference_origin_q16[0];
        const int32_t y_q16 =
            origins_q16[offset + (size_t)1] - reference_origin_q16[1];

        if (x_q16 < left_q16)
        {
            left_q16 = x_q16;
        }
        if (x_q16 > right_q16)
        {
            right_q16 = x_q16;
        }
        if (y_q16 < bottom_q16)
        {
            bottom_q16 = y_q16;
        }
        if (y_q16 > top_q16)
        {
            top_q16 = y_q16;
        }
    }
    if (right_q16 - left_q16 < INT32_C(10) * INT32_C(65536))
    {
        right_q16 = (right_q16 - left_q16) / INT32_C(2);
        left_q16 = -right_q16;
    }
    if (top_q16 - bottom_q16 < INT32_C(10) * INT32_C(65536))
    {
        const int32_t half_height_q16 =
            (top_q16 - bottom_q16) / INT32_C(2);
        const int32_t middle_q16 =
            (top_q16 + bottom_q16) / INT32_C(2);

        top_q16 = middle_q16 + half_height_q16;
        bottom_q16 = middle_q16 - half_height_q16;
    }
    if (right_q16 < INT32_C(2) * INT32_C(65536))
    {
        right_q16 = INT32_C(2) * INT32_C(65536);
    }
    if (left_q16 > -INT32_C(2) * INT32_C(65536))
    {
        left_q16 = -INT32_C(2) * INT32_C(65536);
    }
    if (grounded != 0)
    {
        bottom_q16 = INT32_C(0);
    }
    else if (bottom_q16 < INT32_C(0))
    {
        bottom_q16 = INT32_C(0);
    }
    side_y_q16 = (bottom_q16 + top_q16) / INT32_C(2);
    out_pose->top_x_from_origin_q16 = INT32_C(0);
    out_pose->top_y_from_origin_q16 = pf_m4_falcon_ecb_source_scale_q16(
        top_q16, INT32_C(11), INT32_C(62));
    out_pose->bottom_x_from_origin_q16 = INT32_C(0);
    out_pose->bottom_y_from_origin_q16 =
        pf_m4_falcon_ecb_source_scale_q16(
            bottom_q16, INT32_C(11), INT32_C(62));
    out_pose->right_x_from_origin_q16 = pf_m4_falcon_ecb_source_scale_q16(
        right_q16, INT32_C(12), INT32_C(115));
    out_pose->right_y_from_origin_q16 = pf_m4_falcon_ecb_source_scale_q16(
        side_y_q16, INT32_C(11), INT32_C(62));
    out_pose->left_x_from_origin_q16 = pf_m4_falcon_ecb_source_scale_q16(
        left_q16, INT32_C(12), INT32_C(115));
    out_pose->left_y_from_origin_q16 = out_pose->right_y_from_origin_q16;
    return pf_m4_falcon_reference_ecb_apply_bottom_lock_q16(
        locked_bottom_y_q16,
        out_pose);
}

int
pf_m4_falcon_reference_hsd_ecb_pose(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    int grounded,
    int32_t locked_bottom_y_q16,
    pf_m4_falcon_ecb_pose_q16 *out_pose)
{
    int32_t origins_q16[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t joint_indices[PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT];
    if (out_pose == NULL || source_animation_frame_q16 < INT32_C(0))
    {
        return 0;
    }
    if (source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT)
    {
        const uint16_t frame_index = (uint16_t)(
            ((uint32_t)source_animation_frame_q16 >> UINT32_C(16)) %
            PF_M4_FALCON_CROUCH_WAIT_ECB_FRAME_COUNT);

        *out_pose = pf_m4_falcon_collision_pose_data.crouch_wait[frame_index];
        return 1;
    }
    if (!pf_m4_falcon_hsd_ecb_evaluation_joints(joint_indices))
    {
        return 0;
    }
    if (!pf_m4_hsd_evaluate_joint_origins_source_q16(
            &pf_m4_falcon_dynamic_hsd_data,
            source_submotion,
            source_animation_frame_q16,
            joint_indices,
            (uint8_t)PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT,
            origins_q16))
    {
        return 0;
    }
    return pf_m4_falcon_hsd_ecb_from_origins(
        &origins_q16[0][0],
        (uint8_t)PF_M4_FALCON_HSD_ECB_JOINT_COUNT,
        origins_q16[PF_M4_FALCON_HSD_ECB_JOINT_COUNT],
        grounded,
        locked_bottom_y_q16,
        out_pose);
}

const pf_m4_hsd_pose_data *
pf_m4_falcon_reference_hsd_pose_data(void)
{
    return &pf_m4_falcon_dynamic_hsd_data;
}

const pf_m4_hsd_wait_animation *
pf_m4_falcon_reference_wait_animations(uint8_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = (uint8_t)(
            sizeof(pf_m4_falcon_dynamic_hsd_wait_animations) /
            sizeof(pf_m4_falcon_dynamic_hsd_wait_animations[0]));
    }
    return pf_m4_falcon_dynamic_hsd_wait_animations;
}

const pf_m4_hsd_wait_animation *
pf_m4_falcon_reference_wait_animation(uint16_t source_submotion)
{
    uint8_t count;
    const pf_m4_hsd_wait_animation *animations =
        pf_m4_falcon_reference_wait_animations(&count);
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

int pf_m4_falcon_reference_direct_hsd_pose(
    uint8_t action_state,
    uint16_t action_ticks,
    uint8_t grounded,
    uint16_t *out_submotion,
    int32_t *out_frame_q16)
{
    if (out_submotion == NULL || out_frame_q16 == NULL)
    {
        return 0;
    }
    *out_frame_q16 = (int32_t)action_ticks * PF_Q16_ONE;
    switch (action_state)
    {
    case PF_M4_ACTION_RAPTOR_BOOST_START_GROUND:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_GROUND;
        return 1;
    case PF_M4_ACTION_RAPTOR_BOOST_HIT_GROUND:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_GROUND;
        return 1;
    case PF_M4_ACTION_RAPTOR_BOOST_START_AIR:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_AIR;
        return 1;
    case PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_AIR;
        return 1;
    case PF_M4_ACTION_FALCON_DIVE_START_GROUND:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND;
        return 1;
    case PF_M4_ACTION_FALCON_DIVE_START_AIR:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR;
        return 1;
    case PF_M4_ACTION_FALCON_DIVE_CATCH:
        if (grounded != UINT8_C(0))
        {
            *out_submotion = (uint16_t)
                PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND;
            *out_frame_q16 = INT32_C(13) * PF_Q16_ONE;
        }
        else if (action_ticks == UINT16_C(0))
        {
            *out_submotion = (uint16_t)
                PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR;
            *out_frame_q16 = INT32_C(13) * PF_Q16_ONE;
        }
        else
        {
            *out_submotion =
                (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_CATCH;
        }
        return 1;
    case PF_M4_ACTION_FALCON_DIVE_THROW:
        *out_submotion =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALCON_DIVE_THROW;
        return 1;
    default:
        return 0;
    }
}

static int pf_m4_falcon_reference_hsd_ecb_pose_from_local_pose(
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    int grounded,
    int32_t locked_bottom_y_q16,
    pf_m4_falcon_ecb_pose_q16 *out_pose)
{
    int32_t origins_q16[PF_M4_HSD_POSE_MAX_JOINTS][3];
    uint8_t joint_indices[PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT];

    if (pose == NULL || out_pose == NULL ||
        !pf_m4_falcon_hsd_ecb_evaluation_joints(joint_indices))
    {
        return 0;
    }
    if (
        !pf_m4_hsd_evaluate_joint_origins_from_local_pose_q16(
            &pf_m4_falcon_dynamic_hsd_data,
            pose,
            joint_indices,
            (uint8_t)PF_M4_FALCON_HSD_ECB_EVALUATION_JOINT_COUNT,
            origins_q16))
    {
        return 0;
    }
    return pf_m4_falcon_hsd_ecb_from_origins(
        &origins_q16[0][0],
        (uint8_t)PF_M4_FALCON_HSD_ECB_JOINT_COUNT,
        origins_q16[PF_M4_FALCON_HSD_ECB_JOINT_COUNT],
        grounded,
        locked_bottom_y_q16,
        out_pose);
}

int pf_m4_falcon_reference_hsd_ground_ecb_pose_from_local_pose(
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    pf_m4_falcon_ecb_pose_q16 *out_pose)
{
    return pf_m4_falcon_reference_hsd_ecb_pose_from_local_pose(
        pose,
        1,
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
        out_pose);
}

int pf_m4_falcon_reference_hsd_fall_special_ecb_pose(
    uint16_t directional_submotion,
    int32_t source_animation_frame_q16,
    int32_t directional_blend_q16,
    uint8_t directional_target_switched,
    int32_t locked_bottom_y_q16,
    pf_m4_falcon_ecb_pose_q16 *out_pose)
{
    pf_m4_hsd_local_pose neutral[PF_M4_HSD_POSE_MAX_JOINTS];
    pf_m4_hsd_local_pose directional[PF_M4_HSD_POSE_MAX_JOINTS];
    pf_m4_hsd_local_pose transition[PF_M4_HSD_POSE_MAX_JOINTS];
    pf_m4_hsd_local_pose blended[PF_M4_HSD_POSE_MAX_JOINTS];
    const pf_m4_hsd_local_pose *pose = neutral;

    if (out_pose == NULL || source_animation_frame_q16 < INT32_C(0) ||
        directional_blend_q16 < INT32_C(0) ||
        directional_blend_q16 > PF_Q16_ONE ||
        directional_target_switched > UINT8_C(1) ||
        (directional_submotion !=
             (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL &&
         directional_submotion !=
             (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_FORWARD &&
         directional_submotion !=
             (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_BACKWARD) ||
        !pf_m4_hsd_evaluate_local_pose_q16(
            &pf_m4_falcon_dynamic_hsd_data,
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL,
            source_animation_frame_q16,
            neutral))
    {
        return 0;
    }
    if (directional_blend_q16 != INT32_C(0))
    {
        const pf_m4_falcon_submotion_data *directional_motion =
            pf_m4_falcon_reference_submotion(directional_submotion);
        int32_t directional_frame_q16 =
            source_animation_frame_q16 + PF_Q16_ONE;

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
            (!pf_m4_hsd_evaluate_local_pose_q16(
                 &pf_m4_falcon_dynamic_hsd_data,
                 directional_submotion,
                 source_animation_frame_q16,
                 directional) ||
             !pf_m4_hsd_blend_local_pose_q16(
                 &pf_m4_falcon_dynamic_hsd_data,
                 directional,
                 neutral,
                 PF_Q16_ONE - directional_blend_q16,
                 transition)))
        {
            return 0;
        }
        while (directional_frame_q16 >=
            (int32_t)directional_motion->animation_frame_count * PF_Q16_ONE)
        {
            directional_frame_q16 -=
                (int32_t)directional_motion->animation_frame_count *
                PF_Q16_ONE;
        }
        if (!pf_m4_hsd_evaluate_local_pose_q16(
                &pf_m4_falcon_dynamic_hsd_data,
                directional_submotion,
                directional_frame_q16,
                directional) ||
            !pf_m4_hsd_blend_local_pose_q16(
                &pf_m4_falcon_dynamic_hsd_data,
                directional,
                directional_target_switched != UINT8_C(0)
                    ? transition
                    : neutral,
                PF_Q16_ONE - directional_blend_q16,
                blended))
        {
            return 0;
        }
        pose = blended;
    }
    return pf_m4_falcon_reference_hsd_ecb_pose_from_local_pose(
        pose,
        0,
        locked_bottom_y_q16,
        out_pose);
}

const pf_m4_falcon_ecb_pose_q16 *
pf_m4_falcon_reference_airborne_ecb_pose(
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
    return &pf_m4_falcon_collision_pose_data.airborne[offset + action_ticks];
}

int pf_m4_falcon_reference_aerial_attack_bottom_q16(
    uint8_t action_state,
    uint16_t action_ticks,
    int32_t *out_bottom_y_from_origin_q16)
{
    pf_m4_falcon_move_index move_index;
    uint16_t offset;
    uint16_t frame_count;

    if (out_bottom_y_from_origin_q16 == NULL ||
        !pf_m4_falcon_reference_move_for_action(action_state, &move_index))
    {
        return 0;
    }
    switch (move_index)
    {
    case PF_M4_FALCON_NEUTRAL_AERIAL:
        offset = UINT16_C(0);
        frame_count = PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_FORWARD_AERIAL:
        offset = PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT;
        frame_count = PF_M4_FALCON_FORWARD_AERIAL_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_BACK_AERIAL:
        offset = (uint16_t)(
            PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_FORWARD_AERIAL_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_BACK_AERIAL_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_UP_AERIAL:
        offset = (uint16_t)(
            PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_FORWARD_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_BACK_AERIAL_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_UP_AERIAL_ECB_FRAME_COUNT;
        break;
    case PF_M4_FALCON_DOWN_AERIAL:
        offset = (uint16_t)(
            PF_M4_FALCON_NEUTRAL_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_FORWARD_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_BACK_AERIAL_ECB_FRAME_COUNT +
            PF_M4_FALCON_UP_AERIAL_ECB_FRAME_COUNT);
        frame_count = PF_M4_FALCON_DOWN_AERIAL_ECB_FRAME_COUNT;
        break;
    default:
        return 0;
    }
    if (action_ticks >= frame_count)
    {
        action_ticks = (uint16_t)(frame_count - UINT16_C(1));
    }
    *out_bottom_y_from_origin_q16 =
        pf_m4_falcon_collision_pose_data
            .aerial_attack_bottom_y_from_origin_q16[offset + action_ticks];
    return 1;
}

const pf_m4_reference_search_sphere *
pf_m4_falcon_reference_side_special_search_spheres(
    int airborne,
    uint8_t *out_count)
{
    const uint16_t offset = airborne != 0
                                ? pf_m4_falcon_side_special_air_search_offset
                                : pf_m4_falcon_side_special_ground_search_offset;
    const uint8_t count = airborne != 0
                              ? pf_m4_falcon_side_special_air_search_count
                              : pf_m4_falcon_side_special_ground_search_count;

    if (out_count != NULL)
    {
        *out_count = count;
    }
    return count == UINT8_C(0)
               ? NULL
               : &pf_m4_falcon_side_special_search_spheres[offset];
}

const uint8_t *pf_m4_falcon_reference_geometry_sha256(void)
{
    return pf_m4_falcon_geometry_sha256;
}

void pf_m4_falcon_reference_capture_offset_q16(
    int32_t *out_x_q16,
    int32_t *out_y_q16)
{
    if (out_x_q16 != NULL)
    {
        *out_x_q16 = pf_m4_falcon_capture_offset_x_q16;
    }
    if (out_y_q16 != NULL)
    {
        *out_y_q16 = pf_m4_falcon_capture_offset_y_q16;
    }
}

const pf_m4_reference_move *pf_m4_falcon_reference_move(
    pf_m4_falcon_move_index move_index)
{
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return NULL;
    }
    return &pf_m4_falcon_moves[move_index];
}

const pf_m4_reference_hit_phase *pf_m4_falcon_reference_phase(
    pf_m4_falcon_move_index move_index,
    uint16_t phase_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        phase_index >= (uint16_t)move->phase_count)
    {
        return NULL;
    }
    return &pf_m4_falcon_hit_phases[move->phase_offset + phase_index];
}

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_effect(
    pf_m4_falcon_move_index move_index,
    uint16_t effect_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        effect_index >= (uint16_t)move->effect_count)
    {
        return NULL;
    }
    return &pf_m4_falcon_hit_effects[move->effect_offset + effect_index];
}

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_primary_effect(
    pf_m4_falcon_move_index move_index)
{
    const pf_m4_reference_hit_phase *phase =
        pf_m4_falcon_reference_phase(move_index, UINT16_C(0));
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
    return pf_m4_falcon_reference_effect(move_index, effect_index);
}

const pf_m4_reference_hit_phase *pf_m4_falcon_reference_phase_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);
    uint16_t phase_index;

    if (move == NULL || move->present == UINT8_C(0))
    {
        return NULL;
    }
    action_frame = pf_m4_falcon_reference_effective_hit_frame(
        move_index,
        action_frame);
    for (phase_index = UINT16_C(0);
         phase_index < (uint16_t)move->phase_count;
         ++phase_index)
    {
        const pf_m4_reference_hit_phase *phase =
            pf_m4_falcon_reference_phase(move_index, phase_index);

        if (phase != NULL && action_frame >= phase->first_frame &&
            action_frame <= phase->last_frame)
        {
            return phase;
        }
    }
    return NULL;
}

uint16_t pf_m4_falcon_reference_effective_hit_frame(
    pf_m4_falcon_move_index move_index,
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

const pf_m4_reference_hit_effect *pf_m4_falcon_reference_effect_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame)
{
    const pf_m4_reference_hit_phase *phase =
        pf_m4_falcon_reference_phase_at_frame(move_index, action_frame);
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
    return pf_m4_falcon_reference_effect(move_index, effect_index);
}

const pf_m4_reference_hit_sphere *
pf_m4_falcon_reference_hit_spheres_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_sphere_count)
{
    const pf_m4_reference_geometry_move *geometry;
    const pf_m4_reference_hit_frame *frame;
    uint16_t relative_frame;

    if (out_sphere_count != NULL)
    {
        *out_sphere_count = UINT8_C(0);
    }
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return NULL;
    }
    action_frame = pf_m4_falcon_reference_effective_hit_frame(
        move_index,
        action_frame);
    geometry = &pf_m4_falcon_geometry_moves[move_index];
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
    frame = &pf_m4_falcon_hit_frames[
        geometry->frame_offset + relative_frame];
    if (frame->sphere_count == UINT8_C(0))
    {
        return NULL;
    }
    if (out_sphere_count != NULL)
    {
        *out_sphere_count = frame->sphere_count;
    }
    return &pf_m4_falcon_hit_spheres[frame->sphere_offset];
}

int pf_m4_falcon_reference_has_hit_geometry(
    pf_m4_falcon_move_index move_index)
{
    return (uint32_t)move_index < (uint32_t)PF_M4_FALCON_MOVE_COUNT &&
           pf_m4_falcon_geometry_moves[move_index].frame_count != UINT8_C(0);
}

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_standing_hurt_capsules(uint8_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = (uint8_t)(
            sizeof(pf_m4_falcon_standing_hurt_capsules) /
            sizeof(pf_m4_falcon_standing_hurt_capsules[0]));
    }
    return pf_m4_falcon_standing_hurt_capsules;
}

static const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_hurt_track_at_frame(
    const pf_m4_reference_hurt_move *move,
    const pf_m4_reference_hurt_frame *frames,
    const pf_m4_reference_hurt_capsule *capsules,
    uint16_t action_frame,
    uint8_t *out_count)
{
    const pf_m4_reference_hurt_frame *frame;
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

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_hurt_capsules_at_frame(
    pf_m4_falcon_move_index move_index,
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
    return pf_m4_falcon_reference_hurt_track_at_frame(
        &pf_m4_falcon_hurt_moves[move_index],
        pf_m4_falcon_hurt_frames,
        pf_m4_falcon_hurt_capsules,
        action_frame,
        out_count);
}

static uint8_t pf_m4_falcon_ledge_hurt_track_for_action(
    uint8_t action_state,
    uint16_t source_submotion)
{
    switch ((pf_m4_action_state)action_state)
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
            switch ((pf_m4_falcon_submotion_index)source_submotion)
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

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_common_hurt_capsules_at_frame(
    uint8_t action_state,
    uint16_t action_frame,
    uint8_t *out_count)
{
    return
        pf_m4_falcon_reference_common_hurt_capsules_for_submotion_at_frame(
            action_state,
            UINT16_C(0),
            action_frame,
            out_count);
}

static int pf_m4_falcon_copy_dynamic_ground_hurt_capsules(
    const pf_m4_hsd_evaluated_capsule
        evaluated[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t count,
    pf_m4_reference_hurt_capsule
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
        const pf_m4_hsd_evaluated_capsule *source =
            &evaluated[capsule_index];
        pf_m4_reference_hurt_capsule *destination =
            &out_capsules[capsule_index];

        destination->endpoint_a_x_q16 = source->endpoint_a_q16[0];
        destination->endpoint_a_y_q16 = source->endpoint_a_q16[1];
        destination->endpoint_a_z_q16 = source->endpoint_a_q16[2];
        destination->endpoint_b_x_q16 = source->endpoint_b_q16[0];
        destination->endpoint_b_y_q16 = source->endpoint_b_q16[1];
        destination->endpoint_b_z_q16 = source->endpoint_b_q16[2];
        destination->radius_q16 = source->radius_q16;
        destination->hurtbox_id = source->hurtbox_id;
        destination->height = source->height;
        destination->grabbable = source->grabbable;
        destination->reserved = UINT8_C(0);
    }
    *out_count = count;
    return 1;
}

int pf_m4_falcon_reference_hsd_hurt_capsules(
    uint16_t source_submotion,
    int32_t source_animation_frame_q16,
    pf_m4_reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    pf_m4_hsd_evaluated_capsule evaluated[PF_M4_HSD_POSE_MAX_CAPSULES];
    uint8_t count;

    if (!pf_m4_hsd_evaluate_hurt_pose(
            &pf_m4_falcon_dynamic_hsd_data,
            source_submotion,
            source_animation_frame_q16,
            evaluated,
            &count))
    {
        if (out_count != NULL)
        {
            *out_count = UINT8_C(0);
        }
        return 0;
    }
    return pf_m4_falcon_copy_dynamic_ground_hurt_capsules(
        evaluated, count, out_capsules, out_count);
}

int pf_m4_falcon_reference_hsd_hurt_capsules_from_local_pose(
    const pf_m4_hsd_local_pose pose[PF_M4_HSD_POSE_MAX_JOINTS],
    pf_m4_reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    pf_m4_hsd_evaluated_capsule evaluated[PF_M4_HSD_POSE_MAX_CAPSULES];
    uint8_t count;

    if (!pf_m4_hsd_evaluate_hurt_pose_from_local_pose(
            &pf_m4_falcon_dynamic_hsd_data,
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
    return pf_m4_falcon_copy_dynamic_ground_hurt_capsules(
        evaluated, count, out_capsules, out_count);
}

int pf_m4_falcon_reference_retained_hsd_pose(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks,
    int32_t source_animation_frame_q16,
    int32_t *out_frame_q16)
{
    uint16_t expected_submotion;

    if (out_frame_q16 == NULL)
    {
        return 0;
    }
    switch ((pf_m4_action_state)action_state)
    {
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
            pf_m4_falcon_reference_shield_break_down_submotion();
        break;
    case PF_M4_ACTION_SHIELD_BREAK_STAND:
        expected_submotion =
            pf_m4_falcon_reference_shield_break_down_submotion() ==
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
    *out_frame_q16 =
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
            action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN
            ? source_animation_frame_q16
            : (int32_t)action_ticks * PF_Q16_ONE;
    return 1;
}

int pf_m4_falcon_reference_retained_hsd_hurt_capsules(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_ticks,
    int32_t source_animation_frame_q16,
    pf_m4_reference_hurt_capsule
        out_capsules[PF_M4_HSD_POSE_MAX_CAPSULES],
    uint8_t *out_count)
{
    int32_t evaluated_frame_q16;

    if (!pf_m4_falcon_reference_retained_hsd_pose(
            action_state,
            source_submotion,
            action_ticks,
            source_animation_frame_q16,
            &evaluated_frame_q16))
    {
        return 0;
    }
    return pf_m4_falcon_reference_hsd_hurt_capsules(
        source_submotion,
        evaluated_frame_q16,
        out_capsules,
        out_count);
}

uint16_t pf_m4_falcon_reference_shield_break_down_submotion(void)
{
    _Static_assert(
        pf_m4_falcon_dynamic_hsd_pose_branch_shield_break_down_up_component_q16 <
            INT32_C(0),
        "Falcon terminal ShieldBreakFly HipN branch must remain DownD");
    return pf_m4_falcon_dynamic_hsd_pose_branch_shield_break_down_up !=
                   UINT8_C(0)
               ? (uint16_t)
                     PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_UP
               : (uint16_t)
                     PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN;
}

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_common_hurt_capsules_for_submotion_at_frame(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_frame,
    uint8_t *out_count)
{
    pf_m4_falcon_common_hurt_index track_index;
    const uint8_t ledge_track_index =
        pf_m4_falcon_ledge_hurt_track_for_action(
            action_state,
            source_submotion);

    if (ledge_track_index != UINT8_MAX)
    {
        return pf_m4_falcon_reference_hurt_track_at_frame(
            &pf_m4_falcon_ledge_hurt_moves[ledge_track_index],
            pf_m4_falcon_ledge_hurt_frames,
            pf_m4_falcon_ledge_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GRABBED)
    {
        pf_m4_falcon_capture_hurt_index capture_track;

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
            const pf_m4_reference_hurt_move *track =
                &pf_m4_falcon_capture_hurt_moves[capture_track];

            if (capture_track == PF_M4_FALCON_CAPTURE_HURT_WAIT_HIGH &&
                track->frame_count != UINT8_C(0))
            {
                action_frame =
                    (uint16_t)(action_frame % track->frame_count);
            }
            return pf_m4_falcon_reference_hurt_track_at_frame(
                track,
                pf_m4_falcon_hurt_frames,
                pf_m4_falcon_hurt_capsules,
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
            return pf_m4_falcon_reference_hurt_track_at_frame(
                &pf_m4_falcon_guard_hurt_moves[guard_track],
                pf_m4_falcon_guard_hurt_frames,
                pf_m4_falcon_guard_hurt_capsules,
                action_frame,
                out_count);
        }
    }
    if (action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_GUARD_OFF)
    {
        return pf_m4_falcon_reference_hurt_track_at_frame(
            &pf_m4_falcon_guard_hurt_moves[
                PF_M4_FALCON_GUARD_HURT_GUARD_OFF],
            pf_m4_falcon_guard_hurt_frames,
            pf_m4_falcon_guard_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        uint8_t airborne_track;

        switch ((pf_m4_falcon_submotion_index)source_submotion)
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
            return pf_m4_falcon_reference_hurt_track_at_frame(
                &pf_m4_falcon_airborne_hurt_moves[airborne_track],
                pf_m4_falcon_airborne_hurt_frames,
                pf_m4_falcon_airborne_hurt_capsules,
                action_frame,
                out_count);
        }
    }
    if (action_state == (uint8_t)PF_M4_ACTION_STANDING_TURN &&
        source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TURN)
    {
        return pf_m4_falcon_reference_hurt_track_at_frame(
            &pf_m4_falcon_turn_hurt_moves[
                PF_M4_FALCON_TURN_HURT_STANDING_TURN],
            pf_m4_falcon_turn_hurt_frames,
            pf_m4_falcon_turn_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
        source_submotion == (uint16_t)PF_M4_FALCON_SUBMOTION_TURN_RUN)
    {
        return pf_m4_falcon_reference_hurt_track_at_frame(
            &pf_m4_falcon_turn_hurt_moves[
                PF_M4_FALCON_TURN_HURT_RUN_TURNAROUND],
            pf_m4_falcon_turn_hurt_frames,
            pf_m4_falcon_turn_hurt_capsules,
            action_frame,
            out_count);
    }
    if (action_state == (uint8_t)PF_M4_ACTION_CROUCH &&
        source_submotion ==
            (uint16_t)PF_M4_FALCON_SUBMOTION_SQUAT_WAIT)
    {
        const pf_m4_reference_hurt_move *track =
            &pf_m4_falcon_crouch_taunt_hurt_moves[
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
        return pf_m4_falcon_reference_hurt_track_at_frame(
            track,
            pf_m4_falcon_crouch_taunt_hurt_frames,
            pf_m4_falcon_crouch_taunt_hurt_capsules,
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
            return pf_m4_falcon_reference_hurt_track_at_frame(
                &pf_m4_falcon_crouch_taunt_hurt_moves[
                    taunt_track_index],
                pf_m4_falcon_crouch_taunt_hurt_frames,
                pf_m4_falcon_crouch_taunt_hurt_capsules,
                action_frame,
                out_count);
        }
    }

    switch ((pf_m4_action_state)action_state)
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
    return pf_m4_falcon_reference_hurt_track_at_frame(
        &pf_m4_falcon_common_hurt_moves[track_index],
        pf_m4_falcon_hurt_frames,
        pf_m4_falcon_hurt_capsules,
        action_frame,
        out_count);
}

int pf_m4_falcon_reference_move_for_action(
    uint8_t action_state,
    pf_m4_falcon_move_index *out_move_index)
{
    pf_m4_falcon_move_index move_index;

    switch ((pf_m4_action_state)action_state)
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

const pf_m4_reference_throw *pf_m4_falcon_reference_throw(
    pf_m4_falcon_move_index move_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);

    if (move == NULL || move->present == UINT8_C(0) ||
        move->throw_index == UINT16_MAX)
    {
        return NULL;
    }
    return &pf_m4_falcon_throws[move->throw_index];
}

pf_m4_reference_timing pf_m4_falcon_reference_timing(
    pf_m4_falcon_move_index move_index)
{
    const pf_m4_reference_move *move =
        pf_m4_falcon_reference_move(move_index);
    pf_m4_reference_timing timing = {0};
    const pf_m4_reference_hit_phase *first;
    const pf_m4_reference_hit_phase *last;

    if (move == NULL || move->present == UINT8_C(0) ||
        move->phase_count == UINT8_C(0))
    {
        return timing;
    }
    first = pf_m4_falcon_reference_phase(move_index, UINT16_C(0));
    last = pf_m4_falcon_reference_phase(
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

const pf_m4_reference_move *pf_m4_falcon_reference_attack(
    uint8_t action_state,
    pf_m4_reference_timing authored_timing,
    uint32_t authored_damage_q16)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;
    const pf_m4_reference_hit_effect *effect;
    pf_m4_reference_timing reference;

    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return NULL;
    }
    move = pf_m4_falcon_reference_move(move_index);
    effect = pf_m4_falcon_reference_primary_effect(move_index);
    reference = pf_m4_falcon_reference_timing(move_index);
    if (move == NULL || effect == NULL ||
        authored_timing.startup_ticks != reference.startup_ticks ||
        authored_timing.active_ticks != reference.active_ticks ||
        authored_timing.recovery_ticks != reference.recovery_ticks ||
        authored_damage_q16 !=
            (uint32_t)effect->damage * UINT32_C(65536))
    {
        return NULL;
    }
    return move;
}

int pf_m4_falcon_reference_attack_matches(
    uint8_t action_state,
    pf_m4_reference_timing authored_timing,
    uint32_t authored_damage_q16)
{
    return pf_m4_falcon_reference_attack(
               action_state,
               authored_timing,
               authored_damage_q16) != NULL;
}

pf_m4_reference_iasa_policy pf_m4_falcon_reference_iasa_policy_for_action(
    uint8_t action_state)
{
    switch ((pf_m4_action_state)action_state)
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

pf_m4_reference_ground_physics
pf_m4_falcon_reference_ground_physics_for_action(uint8_t action_state)
{
    /*
     * These are the common-action callbacks which call ft_80084FA8 or
     * ft_80085030. The other grounded normals use ft_80084F3C and therefore
     * apply ground friction without consuming animation translation.
     */
    switch ((pf_m4_action_state)action_state)
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

int pf_m4_falcon_reference_iasa_active(
    uint8_t action_state,
    uint32_t displayed_frame)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;

    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = pf_m4_falcon_reference_move(move_index);
    return move != NULL && move->iasa_frame != UINT16_C(0) &&
           displayed_frame >= (uint32_t)move->iasa_frame;
}

int pf_m4_falcon_reference_special_iasa_active(
    uint8_t action_state,
    uint16_t action_ticks)
{
    const pf_m4_reference_iasa_policy policy =
        pf_m4_falcon_reference_iasa_policy_for_action(action_state);

    return (policy == PF_M4_REFERENCE_IASA_WAIT ||
            policy == PF_M4_REFERENCE_IASA_FORWARD_SMASH) &&
           pf_m4_falcon_reference_iasa_active(
               action_state,
               (uint32_t)action_ticks + UINT32_C(1));
}

int pf_m4_falcon_reference_translation_q16(
    uint16_t submotion_index,
    uint16_t displayed_frame,
    int32_t *out_translation_x_q16,
    int32_t *out_translation_y_q16)
{
    const pf_m4_falcon_submotion_data *submotion;
    uint32_t sample_index;

    if (displayed_frame == UINT16_C(0))
    {
        return 0;
    }
    submotion = pf_m4_falcon_reference_submotion(submotion_index);
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
    if (out_translation_x_q16 != NULL)
    {
        *out_translation_x_q16 =
            pf_m4_falcon_translation_x_q16[sample_index];
    }
    if (out_translation_y_q16 != NULL)
    {
        *out_translation_y_q16 =
            pf_m4_falcon_translation_y_q16[sample_index];
    }
    return 1;
}

int pf_m4_falcon_reference_motion_x_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_x_q16)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;

    if (action_frame == UINT16_C(0) ||
        !pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = pf_m4_falcon_reference_move(move_index);
    if (move == NULL)
    {
        return 0;
    }
    return pf_m4_falcon_reference_translation_q16(
        move->subaction_index,
        action_frame,
        out_motion_x_q16,
        NULL);
}

int pf_m4_falcon_reference_motion_y_q16(
    uint8_t action_state,
    uint16_t action_frame,
    int32_t *out_motion_y_q16)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;

    if (action_frame == UINT16_C(0) ||
        !pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = pf_m4_falcon_reference_move(move_index);
    if (move == NULL)
    {
        return 0;
    }
    return pf_m4_falcon_reference_translation_q16(
        move->subaction_index,
        action_frame,
        NULL,
        out_motion_y_q16);
}

int pf_m4_falcon_reference_landing_lag_active(
    uint8_t action_state,
    uint16_t action_frame)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;

    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return -1;
    }
    move = pf_m4_falcon_reference_move(move_index);
    if (move == NULL || move->landing_lag == UINT16_C(0) ||
        move->autocancel_before == UINT16_C(0) ||
        move->autocancel_after == UINT16_C(0))
    {
        return -1;
    }
    return action_frame >= move->autocancel_before &&
           action_frame <= move->autocancel_after;
}
