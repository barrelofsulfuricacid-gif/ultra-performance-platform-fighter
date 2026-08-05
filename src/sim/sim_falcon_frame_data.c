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

const pf_m4_reference_hurt_capsule *
pf_m4_falcon_reference_hurt_capsules_at_frame(
    pf_m4_falcon_move_index move_index,
    uint16_t action_frame,
    uint8_t *out_count)
{
    const pf_m4_reference_hurt_move *move;
    const pf_m4_reference_hurt_frame *frame;
    uint16_t relative_frame;

    if (out_count != NULL)
    {
        *out_count = UINT8_C(0);
    }
    if ((uint32_t)move_index >= (uint32_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return NULL;
    }
    move = &pf_m4_falcon_hurt_moves[move_index];
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

int pf_m4_falcon_reference_special_iasa_active(
    uint8_t action_state,
    uint16_t action_ticks)
{
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_move *move;
    const pf_m4_reference_iasa_policy policy =
        pf_m4_falcon_reference_iasa_policy_for_action(action_state);

    if (policy != PF_M4_REFERENCE_IASA_WAIT &&
        policy != PF_M4_REFERENCE_IASA_FORWARD_SMASH)
    {
        return 0;
    }
    if (!pf_m4_falcon_reference_move_for_action(
            action_state,
            &move_index))
    {
        return 0;
    }
    move = pf_m4_falcon_reference_move(move_index);
    return move != NULL && move->iasa_frame != UINT16_C(0) &&
           (uint32_t)action_ticks + UINT32_C(1) >=
               (uint32_t)move->iasa_frame;
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
    if (move == NULL || action_frame > move->motion_count)
    {
        return 0;
    }
    if (out_motion_x_q16 != NULL)
    {
        *out_motion_x_q16 =
            pf_m4_falcon_motion_x_q16[
                move->motion_offset + action_frame - UINT16_C(1)];
    }
    return 1;
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
    if (move == NULL || action_frame > move->motion_count)
    {
        return 0;
    }
    if (out_motion_y_q16 != NULL)
    {
        *out_motion_y_q16 =
            pf_m4_falcon_motion_y_q16[
                move->motion_offset + action_frame - UINT16_C(1)];
    }
    return 1;
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
