#include "sim_falcon_frame_data.h"

#include "pf/m4.h"

#include <stddef.h>
#include <stdint.h>

#include "../../generated/data/m4_falcon_ntsc102_frame_data.inc"
#include "../../generated/data/m4_falcon_ntsc102_hit_geometry.inc"

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
    frame = &pf_m4_falcon_hurt_frames[
        move->frame_offset + relative_frame];
    if (frame->capsule_count == UINT8_C(0))
    {
        return NULL;
    }
    if (out_count != NULL)
    {
        *out_count = frame->capsule_count;
    }
    return &pf_m4_falcon_hurt_capsules[frame->capsule_offset];
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
        action_frame,
        out_count);
}

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_common_hurt_capsules_at_frame(
    uint8_t action_state,
    uint16_t action_frame,
    uint8_t *out_count)
{
    pf_m4_falcon_common_hurt_index track_index;

    switch ((pf_m4_action_state)action_state)
    {
        case PF_M4_ACTION_INITIAL_DASH:
            track_index = PF_M4_FALCON_COMMON_HURT_INITIAL_DASH;
            break;
        case PF_M4_ACTION_RUN_BRAKE:
            track_index = PF_M4_FALCON_COMMON_HURT_RUN_BRAKE;
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
        case PF_M4_ACTION_DASH_ATTACK:
            move_index = PF_M4_FALCON_DASH_ATTACK;
            break;
        case PF_M4_ACTION_FORWARD_ATTACK:
            move_index = PF_M4_FALCON_FORWARD_TILT;
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
        case PF_M4_ACTION_DASH_ATTACK:
        case PF_M4_ACTION_FORWARD_ATTACK:
        case PF_M4_ACTION_UP_ATTACK:
        case PF_M4_ACTION_UP_STRONG_ATTACK:
        case PF_M4_ACTION_DOWN_STRONG_ATTACK:
            return PF_M4_REFERENCE_IASA_WAIT;
        case PF_M4_ACTION_DOWN_ATTACK:
            return PF_M4_REFERENCE_IASA_DOWN_TILT;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
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
        case PF_M4_ACTION_DASH_ATTACK:
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
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
