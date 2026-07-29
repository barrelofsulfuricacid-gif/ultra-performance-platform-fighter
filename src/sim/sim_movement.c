#include "sim_internal.h"

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

static int32_t pf_m4_multiply_q16(
    int32_t value_q16,
    int32_t multiplier_q16)
{
    return (int32_t)(
        ((int64_t)value_q16 * (int64_t)multiplier_q16) /
        (int64_t)PF_Q16_ONE);
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

static int32_t pf_m4_scale_axis_q16(
    int16_t axis,
    int32_t magnitude_q16)
{
    const int64_t denominator =
        axis < INT16_C(0) ? INT64_C(32768) : INT64_C(32767);
    return (int32_t)(((int64_t)axis * (int64_t)magnitude_q16) /
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

static uint32_t pf_m4_u64_sqrt(uint64_t value)
{
    uint64_t result = UINT64_C(0);
    uint64_t bit = UINT64_C(1) << 62U;

    while (bit > value)
    {
        bit >>= 2U;
    }
    while (bit != UINT64_C(0))
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1U) + bit;
        }
        else
        {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result > (uint64_t)UINT32_MAX
               ? UINT32_MAX
               : (uint32_t)result;
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

static int8_t pf_m4_sdi_direction(int16_t axis, uint16_t threshold)
{
    if (pf_m4_axis_magnitude(axis) < threshold)
    {
        return INT8_C(0);
    }
    return axis < INT16_C(0) ? INT8_C(-1) : INT8_C(1);
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

static int pf_m4_signs_differ(int32_t left, int32_t right)
{
    return (left < INT32_C(0) && right > INT32_C(0)) ||
           (left > INT32_C(0) && right < INT32_C(0));
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

static int32_t pf_m4_surface_y_q16(
    const pf_m4_content *content,
    uint8_t support)
{
    if (support == (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        return content->stage.platform_y_q16;
    }
    if (support == (uint8_t)PF_M4_SURFACE_SOLID_TOP)
    {
        return content->stage.solid_top_q16;
    }
    return content->stage.floor_y_q16;
}

static void pf_m4_surface_bounds_q16(
    const pf_m4_content *content,
    uint8_t support,
    uint64_t tick,
    int32_t *out_left,
    int32_t *out_right)
{
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
    else
    {
        *out_left = content->stage.floor_left_q16;
        *out_right = content->stage.floor_right_q16;
    }
}

static int pf_m4_body_overlaps_solid(
    const pf_m4_content *content,
    int32_t position_x_q16,
    int32_t position_y_q16)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const int64_t left =
        (int64_t)position_x_q16 - fighter->half_width_q16;
    const int64_t right =
        (int64_t)position_x_q16 + fighter->half_width_q16;
    const int64_t top =
        (int64_t)position_y_q16 - fighter->half_height_q16;
    const int64_t bottom =
        (int64_t)position_y_q16 + fighter->half_height_q16;

    return right > (int64_t)stage->solid_left_q16 &&
           left < (int64_t)stage->solid_right_q16 &&
           bottom > (int64_t)stage->solid_top_q16 &&
           top < (int64_t)stage->solid_bottom_q16;
}

static pf_status pf_m4_apply_hitlag_shift(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int16_t stick_x,
    int16_t stick_y,
    int32_t distance_q16)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const int64_t stick_x_64 = (int64_t)stick_x;
    const int64_t stick_y_64 = (int64_t)stick_y;
    const uint64_t stick_length_squared =
        (uint64_t)(stick_x_64 * stick_x_64) +
        (uint64_t)(stick_y_64 * stick_y_64);
    const uint32_t stick_length =
        pf_m4_u64_sqrt(stick_length_squared);
    const int64_t denominator =
        stick_length > UINT32_C(32767)
            ? (int64_t)stick_length
            : INT64_C(32767);
    const int32_t old_x = scratch->position_x_q16[player_index];
    const int32_t old_y = scratch->position_y_q16[player_index];
    int32_t next_x;
    int32_t next_y;
    int64_t shifted_x;
    int64_t shifted_y;

    if (stick_length == UINT32_C(0))
    {
        return PF_STATUS_OK;
    }

    shifted_x =
        (int64_t)old_x +
        (stick_x_64 * (int64_t)distance_q16) / denominator;
    shifted_y =
        (int64_t)old_y +
        (stick_y_64 * (int64_t)distance_q16) / denominator;
    if (!pf_m4_checked_i32(shifted_x, &next_x) ||
        !pf_m4_checked_i32(shifted_y, &next_y))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (pf_m4_body_overlaps_solid(content, next_x, next_y))
    {
        next_x = old_x;
        next_y = old_y;
    }

    if (scratch->grounded[player_index] != UINT8_C(0))
    {
        const uint8_t support = scratch->support[player_index];
        const int32_t surface_y =
            pf_m4_surface_y_q16(content, support);
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
        if (next_y < standing_y ||
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

static pf_status pf_m4_apply_directional_influence(
    const pf_m4_fighter_data *fighter,
    int16_t stick_x,
    int16_t stick_y,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16)
{
    const int64_t velocity_x = (int64_t)*velocity_x_q16;
    const int64_t velocity_y_math =
        -(int64_t)*velocity_y_q16;
    const int64_t stick_x_64 = (int64_t)stick_x;
    const int64_t stick_y_math = -(int64_t)stick_y;
    const uint64_t speed_squared =
        (uint64_t)(velocity_x * velocity_x) +
        (uint64_t)(velocity_y_math * velocity_y_math);
    const uint32_t speed = pf_m4_u64_sqrt(speed_squared);
    const int64_t denominator =
        (int64_t)speed * INT64_C(32767);
    int64_t cross;
    int64_t turn_fraction_q16;
    int64_t tangent_q16;
    int64_t candidate_x;
    int64_t candidate_y;
    uint64_t candidate_speed_squared;
    uint32_t candidate_speed;
    int64_t influenced_x;
    int64_t influenced_y;

    if (speed == UINT32_C(0) ||
        (pf_m4_axis_magnitude(stick_x) <= fighter->axis_dead_zone &&
         pf_m4_axis_magnitude(stick_y) <= fighter->axis_dead_zone))
    {
        return PF_STATUS_OK;
    }

    cross =
        velocity_x * stick_y_math -
        velocity_y_math * stick_x_64;
    if (cross > denominator)
    {
        cross = denominator;
    }
    else if (cross < -denominator)
    {
        cross = -denominator;
    }
    turn_fraction_q16 =
        (cross * (int64_t)PF_Q16_ONE) / denominator;
    tangent_q16 =
        ((int64_t)fighter->di_max_tangent_q16 *
         turn_fraction_q16) /
        (int64_t)PF_Q16_ONE;

    candidate_x =
        velocity_x -
        (velocity_y_math * tangent_q16) /
            (int64_t)PF_Q16_ONE;
    candidate_y =
        velocity_y_math +
        (velocity_x * tangent_q16) /
            (int64_t)PF_Q16_ONE;
    candidate_speed_squared =
        (uint64_t)(candidate_x * candidate_x) +
        (uint64_t)(candidate_y * candidate_y);
    candidate_speed = pf_m4_u64_sqrt(candidate_speed_squared);
    if (candidate_speed == UINT32_C(0))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    influenced_x =
        candidate_x * (int64_t)speed /
        (int64_t)candidate_speed;
    influenced_y =
        candidate_y * (int64_t)speed /
        (int64_t)candidate_speed;
    if (!pf_m4_checked_i32(influenced_x, velocity_x_q16) ||
        !pf_m4_checked_i32(-influenced_y, velocity_y_q16))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    return PF_STATUS_OK;
}

static int pf_m4_action_uses_ledge(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB;
}

static int pf_m4_action_locks_ground_control(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
           action_state == (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
           action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL ||
           action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
           action_state == (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
           action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
           action_state == (uint8_t)PF_M4_ACTION_GETUP_ATTACK;
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
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
}

static int pf_m4_action_is_ground_attack(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_STRONG_ATTACK;
}

static int pf_m4_action_is_recovery_invulnerable(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    if (action_state ==
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        action_state == (uint8_t)PF_M4_ACTION_TECH_ROLL)
    {
        return action_ticks <
               fighter->tech_invulnerability_ticks;
    }
    if (pf_m4_action_is_surface_tech(action_state))
    {
        return action_ticks <
               fighter->tech_invulnerability_ticks;
    }
    if (action_state ==
        (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL)
    {
        return action_ticks <
               fighter->getup_neutral_invulnerability_ticks;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL)
    {
        return action_ticks <
               fighter->getup_roll_invulnerability_ticks;
    }
    return action_state ==
               (uint8_t)PF_M4_ACTION_GETUP_ATTACK &&
           action_ticks <
               fighter->getup_attack_invulnerability_ticks;
}

static uint8_t pf_m4_ledge_from_state(
    uint8_t action_state,
    int8_t facing)
{
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
    const uint32_t total =
        (uint32_t)fighter->landing_ticks +
        (uint32_t)fighter->jump_squat_ticks;

    return total > UINT32_C(120)
               ? UINT16_C(120)
               : (uint16_t)total;
}

static void pf_m4_ledge_hang_position(
    const pf_m4_fighter_data *fighter,
    const pf_m4_stage_data *stage,
    uint8_t ledge,
    int32_t *out_x,
    int32_t *out_y)
{
    const int8_t inward = pf_m4_ledge_inward_direction(ledge);

    *out_x =
        pf_m4_ledge_x_q16(stage, ledge) -
        (int32_t)inward * fighter->half_width_q16;
    *out_y =
        stage->floor_y_q16 + fighter->half_height_q16 / INT32_C(2);
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
                scratch->facing[other_index]) == ledge)
        {
            return 1;
        }
    }
    return 0;
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
    uint8_t *grounded,
    uint8_t *action_state,
    uint8_t *support,
    uint8_t *air_jumps_remaining,
    uint8_t *short_hop_latched,
    uint8_t *fast_fall,
    int8_t facing,
    int8_t *dash_direction)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const int64_t horizontal_reach =
        (int64_t)fighter->half_width_q16 +
        (int64_t)fighter->air_speed_q16;
    const int32_t catch_top =
        stage->floor_y_q16 - fighter->half_height_q16;
    const int32_t catch_bottom =
        stage->floor_y_q16 + fighter->half_height_q16;
    uint8_t ledge = (uint8_t)PF_M4_LEDGE_NONE;

    if (*velocity_y < INT32_C(0) ||
        *position_y < catch_top ||
        *position_y > catch_bottom)
    {
        return 0;
    }

    if (*position_x < stage->floor_left_q16 &&
        facing == INT8_C(1) &&
        (int64_t)stage->floor_left_q16 -
                (int64_t)*position_x <=
            horizontal_reach)
    {
        ledge = (uint8_t)PF_M4_LEDGE_LEFT;
    }
    else if (*position_x > stage->floor_right_q16 &&
             facing == INT8_C(-1) &&
             (int64_t)*position_x -
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

    pf_m4_ledge_hang_position(
        fighter,
        stage,
        ledge,
        position_x,
        position_y);
    *velocity_x = INT32_C(0);
    *velocity_y = INT32_C(0);
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(0);
    *action_state = (uint8_t)PF_M4_ACTION_LEDGE_HANG;
    *support = (uint8_t)PF_M4_SURFACE_NONE;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
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

    sim->world.previous_buttons[player_index] = UINT64_C(0);
    sim->world.position_x_q16[player_index] =
        centered_slot * stage->spawn_spacing_q16;
    sim->world.position_y_q16[player_index] =
        stage->floor_y_q16 - fighter->half_height_q16;
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.respawn_count[player_index] = respawn_count;
    sim->world.grounded[player_index] = UINT8_C(1);
    sim->world.action_state[player_index] =
        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
    sim->world.support[player_index] =
        (uint8_t)PF_M4_SURFACE_FLOOR;
    sim->world.air_jumps_remaining[player_index] =
        fighter->air_jump_count;
    sim->world.short_hop_latched[player_index] = UINT8_C(0);
    sim->world.platform_drop_ticks[player_index] = UINT8_C(0);
    sim->world.fast_fall[player_index] = UINT8_C(0);
    sim->world.facing[player_index] =
        centered_slot <= INT32_C(0) ? INT8_C(1) : INT8_C(-1);
    sim->world.dash_direction[player_index] = INT8_C(0);
    sim->world.previous_strong_direction[player_index] = INT8_C(0);
    sim->world.damage_q16[player_index] = UINT32_C(0);
    sim->world.pending_velocity_x_q16[player_index] = INT32_C(0);
    sim->world.pending_velocity_y_q16[player_index] = INT32_C(0);
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
    sim->world.last_hit_attacker[player_index] = UINT8_C(0);
    sim->world.shield_held[player_index] = UINT8_C(0);
    sim->world.powershield[player_index] = UINT8_C(0);
    sim->world.tumble[player_index] = UINT8_C(0);
    sim->world.sdi_pulse_count[player_index] = UINT8_C(0);
    sim->world.sdi_direction_x[player_index] = INT8_C(0);
    sim->world.sdi_direction_y[player_index] = INT8_C(0);
    sim->world.tech_direction[player_index] = INT8_C(0);
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
    *position_y = surface_y_q16 - fighter->half_height_q16;
    *velocity_y = INT32_C(0);
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(1);
    *action_state = (uint8_t)PF_M4_ACTION_LANDING;
    *support = surface;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
}

static void pf_m4_land_from_air(
    const pf_m4_fighter_data *fighter,
    int32_t surface_y_q16,
    uint8_t surface,
    int16_t horizontal_input,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *position_y,
    int32_t *velocity_x,
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
    const int8_t roll_direction =
        pf_m4_axis_direction(
            horizontal_input,
            fighter->axis_dead_zone);

    if (scratch->tumble[player_index] == UINT8_C(0))
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
        scratch->tech_direction[player_index] = INT8_C(0);
        return;
    }

    *position_y = surface_y_q16 - fighter->half_height_q16;
    *velocity_y = INT32_C(0);
    *action_ticks = UINT16_C(0);
    *grounded = UINT8_C(1);
    *support = surface;
    *air_jumps_remaining = fighter->air_jump_count;
    *short_hop_latched = UINT8_C(0);
    *fast_fall = UINT8_C(0);
    *dash_direction = INT8_C(0);
    scratch->hitstun_ticks[player_index] = UINT16_C(0);
    scratch->tumble[player_index] = UINT8_C(0);

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
            *velocity_x =
                (int32_t)roll_direction *
                fighter->tech_roll_speed_q16;
            *action_state = (uint8_t)PF_M4_ACTION_TECH_ROLL;
            scratch->tech_direction[player_index] = roll_direction;
        }
    }
    else
    {
        *velocity_x = INT32_C(0);
        *action_state = (uint8_t)PF_M4_ACTION_KNOCKDOWN;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
}

static void pf_m4_enter_wall_impact(
    const pf_m4_fighter_data *fighter,
    int wall_tech_jump,
    int8_t away_direction,
    pf_sim_scratch *scratch,
    uint32_t player_index,
    int32_t *velocity_x,
    int32_t *velocity_y,
    uint16_t *action_ticks,
    uint8_t *action_state,
    uint8_t *fast_fall,
    int8_t *facing)
{
    int32_t horizontal_magnitude =
        *velocity_x < INT32_C(0) ? -*velocity_x : *velocity_x;

    *action_ticks = UINT16_C(0);
    *fast_fall = UINT8_C(0);
    *facing = away_direction;
    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
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
        *velocity_x =
            (int32_t)away_direction *
            pf_m4_multiply_q16(
                horizontal_magnitude,
                fighter->surface_bounce_multiplier_q16);
        *velocity_y = pf_m4_multiply_q16(
            *velocity_y,
            fighter->surface_bounce_multiplier_q16);
        *action_state = (uint8_t)PF_M4_ACTION_WALL_BOUNCE;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
}

static void pf_m4_enter_ceiling_impact(
    const pf_m4_fighter_data *fighter,
    int16_t horizontal_input,
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
        *velocity_x = pf_m4_scale_axis_q16(
            horizontal_input,
            fighter->ceiling_tech_speed_q16);
        *velocity_y = INT32_C(0);
        *action_state = (uint8_t)PF_M4_ACTION_CEILING_TECH;
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    else
    {
        *velocity_x = pf_m4_multiply_q16(
            *velocity_x,
            fighter->surface_bounce_multiplier_q16);
        *velocity_y = -pf_m4_multiply_q16(
            *velocity_y,
            fighter->surface_bounce_multiplier_q16);
        *action_state = (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
        scratch->tech_direction[player_index] = INT8_C(0);
    }
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
    uint16_t respawn_count,
    uint8_t grounded,
    uint8_t action_state,
    uint8_t support,
    uint8_t air_jumps_remaining,
    uint8_t short_hop_latched,
    uint8_t platform_drop_ticks,
    uint8_t fast_fall,
    int8_t facing,
    int8_t dash_direction,
    int8_t previous_strong_direction)
{
    scratch->previous_buttons[player_index] = input->buttons;
    scratch->position_x_q16[player_index] = position_x;
    scratch->position_y_q16[player_index] = position_y;
    scratch->velocity_x_q16[player_index] = velocity_x;
    scratch->velocity_y_q16[player_index] = velocity_y;
    scratch->action_ticks[player_index] = action_ticks;
    scratch->respawn_count[player_index] = respawn_count;
    scratch->grounded[player_index] = grounded;
    scratch->action_state[player_index] = action_state;
    scratch->support[player_index] = support;
    scratch->air_jumps_remaining[player_index] =
        air_jumps_remaining;
    scratch->short_hop_latched[player_index] = short_hop_latched;
    scratch->platform_drop_ticks[player_index] =
        platform_drop_ticks;
    scratch->fast_fall[player_index] = fast_fall;
    scratch->facing[player_index] = facing;
    scratch->dash_direction[player_index] = dash_direction;
    scratch->previous_strong_direction[player_index] =
        previous_strong_direction;
}

static void pf_m4_copy_combat_scratch(
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    uint32_t player_index)
{
    scratch->damage_q16[player_index] =
        world->damage_q16[player_index];
    scratch->pending_velocity_x_q16[player_index] =
        world->pending_velocity_x_q16[player_index];
    scratch->pending_velocity_y_q16[player_index] =
        world->pending_velocity_y_q16[player_index];
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
    scratch->last_hit_attacker[player_index] =
        world->last_hit_attacker[player_index];
    scratch->shield_held[player_index] =
        world->shield_held[player_index];
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
}

pf_status pf_m4_step_player(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const uint64_t previous_buttons =
        world->previous_buttons[player_index];
    const int jump_pressed =
        (input->buttons & PF_INPUT_BUTTON_JUMP) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_JUMP) == UINT64_C(0);
    const int light_attack_pressed =
        (input->buttons & PF_INPUT_BUTTON_ATTACK) != UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_ATTACK) == UINT64_C(0);
    const int strong_attack_pressed =
        (input->buttons & PF_INPUT_BUTTON_STRONG_ATTACK) !=
            UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_STRONG_ATTACK) ==
            UINT64_C(0);
    const int attack_pressed =
        light_attack_pressed || strong_attack_pressed;
    const int shield_held =
        input->left_trigger >= fighter->digital_trigger_threshold ||
        input->right_trigger >= fighter->digital_trigger_threshold;
    const int shield_pressed =
        shield_held != 0 &&
        world->shield_held[player_index] == UINT8_C(0);
    const int was_shielding =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD ||
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_STUN;
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(input->main_stick_x);
    const int8_t horizontal_direction =
        pf_m4_axis_direction(
            input->main_stick_x,
            fighter->axis_dead_zone);
    const int8_t strong_direction =
        pf_m4_strong_direction(
            input->main_stick_x,
            fighter->dash_axis_threshold);
    int32_t position_x = world->position_x_q16[player_index];
    int32_t position_y = world->position_y_q16[player_index];
    int32_t velocity_x = world->velocity_x_q16[player_index];
    int32_t velocity_y = world->velocity_y_q16[player_index];
    uint16_t action_ticks = world->action_ticks[player_index];
    uint16_t respawn_count = world->respawn_count[player_index];
    uint8_t grounded = world->grounded[player_index];
    uint8_t action_state = world->action_state[player_index];
    uint8_t support = world->support[player_index];
    uint8_t air_jumps_remaining =
        world->air_jumps_remaining[player_index];
    uint8_t short_hop_latched =
        world->short_hop_latched[player_index];
    uint8_t platform_drop_ticks =
        world->platform_drop_ticks[player_index];
    uint8_t fast_fall = world->fast_fall[player_index];
    int8_t facing = world->facing[player_index];
    int8_t dash_direction =
        world->dash_direction[player_index];
    int8_t previous_strong_direction =
        world->previous_strong_direction[player_index];
    int launched_this_tick = 0;
    int ledge_motion_handled = 0;
    int released_ledge_this_tick = 0;
    int shield_reset_this_tick = 0;
    int hitstun_locked;
    int32_t previous_position_x;
    int64_t next_position;
    pf_status status;

    pf_m4_copy_combat_scratch(world, scratch, player_index);
    if (scratch->tech_window_ticks[player_index] > UINT16_C(0))
    {
        --scratch->tech_window_ticks[player_index];
    }
    if (scratch->tech_lockout_ticks[player_index] > UINT16_C(0))
    {
        --scratch->tech_lockout_ticks[player_index];
    }
    if (shield_pressed != 0 &&
        scratch->tech_lockout_ticks[player_index] == UINT16_C(0))
    {
        scratch->tech_window_ticks[player_index] =
            fighter->tech_window_ticks;
        scratch->tech_lockout_ticks[player_index] =
            fighter->tech_lockout_ticks;
    }
    scratch->shield_held[player_index] =
        shield_held != 0 ? UINT8_C(1) : UINT8_C(0);

    if (scratch->hitlag_ticks[player_index] > UINT16_C(0))
    {
        scratch->position_x_q16[player_index] = position_x;
        scratch->position_y_q16[player_index] = position_y;
        scratch->grounded[player_index] = grounded;
        scratch->support[player_index] = support;
        if (scratch->hitlag_resume_action[player_index] ==
            (uint8_t)PF_M4_ACTION_HITSTUN)
        {
            const int8_t sdi_x =
                pf_m4_sdi_direction(
                    input->main_stick_x,
                    fighter->sdi_axis_threshold);
            const int8_t sdi_y =
                pf_m4_sdi_direction(
                    input->main_stick_y,
                    fighter->sdi_axis_threshold);
            const int new_sdi_component =
                (sdi_x != INT8_C(0) &&
                 sdi_x !=
                     scratch->sdi_direction_x[player_index]) ||
                (sdi_y != INT8_C(0) &&
                 sdi_y !=
                     scratch->sdi_direction_y[player_index]);

            if (new_sdi_component)
            {
                status = pf_m4_apply_hitlag_shift(
                    content,
                    world,
                    scratch,
                    player_index,
                    input->main_stick_x,
                    input->main_stick_y,
                    fighter->sdi_distance_q16);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                if (scratch->sdi_pulse_count[player_index] !=
                    UINT8_MAX)
                {
                    ++scratch->sdi_pulse_count[player_index];
                }
            }
            scratch->sdi_direction_x[player_index] = sdi_x;
            scratch->sdi_direction_y[player_index] = sdi_y;
        }

        --scratch->hitlag_ticks[player_index];
        action_state = (uint8_t)PF_M4_ACTION_HITLAG;
        if (scratch->hitlag_ticks[player_index] == UINT16_C(0))
        {
            action_state =
                scratch->hitlag_resume_action[player_index];
            scratch->hitlag_resume_action[player_index] = UINT8_C(0);
            if (action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
            {
                status = pf_m4_apply_hitlag_shift(
                    content,
                    world,
                    scratch,
                    player_index,
                    input->main_stick_x,
                    input->main_stick_y,
                    fighter->asdi_distance_q16);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                status = pf_m4_apply_directional_influence(
                    fighter,
                    input->main_stick_x,
                    input->main_stick_y,
                    &scratch
                         ->pending_velocity_x_q16[player_index],
                    &scratch
                         ->pending_velocity_y_q16[player_index]);
                if (status != PF_STATUS_OK)
                {
                    return status;
                }
                velocity_x =
                    scratch->pending_velocity_x_q16[player_index];
                velocity_y =
                    scratch->pending_velocity_y_q16[player_index];
                scratch->pending_velocity_x_q16[player_index] =
                    INT32_C(0);
                scratch->pending_velocity_y_q16[player_index] =
                    INT32_C(0);
                grounded = UINT8_C(0);
                support = (uint8_t)PF_M4_SURFACE_NONE;
                scratch->grounded[player_index] = UINT8_C(0);
                scratch->support[player_index] =
                    (uint8_t)PF_M4_SURFACE_NONE;
                fast_fall = UINT8_C(0);
                dash_direction = INT8_C(0);
            }
            scratch->sdi_direction_x[player_index] = INT8_C(0);
            scratch->sdi_direction_y[player_index] = INT8_C(0);
        }
        position_x = scratch->position_x_q16[player_index];
        position_y = scratch->position_y_q16[player_index];
        grounded = scratch->grounded[player_index];
        support = scratch->support[player_index];
        pf_m4_write_scratch(
            scratch,
            player_index,
            input,
            position_x,
            position_y,
            velocity_x,
            velocity_y,
            action_ticks,
            respawn_count,
            grounded,
            action_state,
            support,
            air_jumps_remaining,
            short_hop_latched,
            platform_drop_ticks,
            fast_fall,
            facing,
            dash_direction,
            previous_strong_direction);
        return PF_STATUS_OK;
    }

    hitstun_locked =
        (action_state == (uint8_t)PF_M4_ACTION_HITSTUN ||
         pf_m4_action_is_surface_bounce(action_state)) &&
        scratch->hitstun_ticks[player_index] > UINT16_C(0);

    if (platform_drop_ticks > UINT8_C(0))
    {
        --platform_drop_ticks;
    }

    if (pf_m4_action_uses_ledge(action_state))
    {
        const uint8_t ledge =
            pf_m4_ledge_from_state(action_state, facing);
        const int8_t inward =
            pf_m4_ledge_inward_direction(ledge);
        const int8_t outward = (int8_t)-inward;
        int32_t hang_x;
        int32_t hang_y;

        pf_m4_ledge_hang_position(
            fighter,
            stage,
            ledge,
            &hang_x,
            &hang_y);
        position_x = hang_x;
        position_y = hang_y;
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        grounded = UINT8_C(0);
        support = (uint8_t)PF_M4_SURFACE_NONE;
        short_hop_latched = UINT8_C(0);
        fast_fall = UINT8_C(0);
        dash_direction = INT8_C(0);

        if (action_state == (uint8_t)PF_M4_ACTION_LEDGE_HANG)
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

            if (action_ticks >= catch_ticks && jump_pressed)
            {
                velocity_x =
                    (int32_t)inward * fighter->air_speed_q16;
                velocity_y = -fighter->double_jump_speed_q16;
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
        }
        else
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
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        shield_held != 0 &&
        !pf_m4_action_is_shield(action_state) &&
        action_state != (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        !pf_m4_action_is_ground_attack(action_state) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        action_state != (uint8_t)PF_M4_ACTION_LANDING &&
        !pf_m4_action_locks_ground_control(action_state))
    {
        action_state = (uint8_t)PF_M4_ACTION_SHIELD;
        action_ticks = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        action_state != (uint8_t)PF_M4_ACTION_LANDING &&
        !pf_m4_action_is_ground_attack(action_state) &&
        !pf_m4_action_is_shield(action_state) &&
        !pf_m4_action_locks_ground_control(action_state) &&
        attack_pressed)
    {
        action_state =
            strong_attack_pressed
                ? (uint8_t)PF_M4_ACTION_STRONG_ATTACK
                : (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
        action_ticks = UINT16_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        action_state != (uint8_t)PF_M4_ACTION_LANDING &&
        !pf_m4_action_is_ground_attack(action_state) &&
        !pf_m4_action_is_shield(action_state) &&
        !pf_m4_action_locks_ground_control(action_state) &&
        jump_pressed)
    {
        action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
        action_ticks = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
    }

    if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_SHIELD)
    {
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        scratch->shield_health_q16[player_index] =
            pf_m4_shield_health_subtract(
                scratch->shield_health_q16[player_index],
                fighter->shield_hold_depletion_q16);
        if (scratch->shield_health_q16[player_index] ==
            UINT32_C(0))
        {
            action_state = (uint8_t)PF_M4_ACTION_SHIELD_BREAK;
            action_ticks = UINT16_C(0);
            velocity_x = INT32_C(0);
            scratch->shield_stun_ticks[player_index] =
                UINT16_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
        }
        else if (was_shielding && jump_pressed)
        {
            action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
            action_ticks = UINT16_C(0);
            short_hop_latched = UINT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
        }
        else
        {
            if (action_ticks <
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
            fighter->traction_q16);
        if (scratch->shield_stun_ticks[player_index] >
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
            fighter->traction_q16);
        if (scratch->powershield[player_index] != UINT8_C(0) &&
            fighter->powershield_cancel_enabled != UINT8_C(0) &&
            action_ticks >=
                fighter->powershield_cancel_delay_ticks &&
            attack_pressed)
        {
            action_state =
                strong_attack_pressed
                    ? (uint8_t)PF_M4_ACTION_STRONG_ATTACK
                    : (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
            action_ticks = UINT16_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
            short_hop_latched = UINT8_C(0);
            dash_direction = INT8_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
        }
        else if (jump_pressed)
        {
            action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
            action_ticks = UINT16_C(0);
            short_hop_latched = UINT8_C(0);
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
        action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        velocity_x = INT32_C(0);
        ++action_ticks;
        if (action_ticks >= fighter->shield_break_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->shield_health_q16[player_index] =
                fighter->shield_reset_health_q16;
            shield_reset_this_tick = 1;
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        pf_m4_action_locks_ground_control(action_state))
    {
        velocity_x = INT32_C(0);
        if (action_state == (uint8_t)PF_M4_ACTION_KNOCKDOWN)
        {
            ++action_ticks;
            if (action_ticks >= fighter->knockdown_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_DOWN_WAIT;
                action_ticks = UINT16_C(0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_DOWN_WAIT)
        {
            const int up_held =
                input->main_stick_y <=
                -(int16_t)fighter->crouch_axis_threshold;

            if (attack_pressed)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GETUP_ATTACK;
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] =
                    UINT8_C(0);
                scratch->tech_direction[player_index] =
                    INT8_C(0);
            }
            else if (horizontal_direction != INT8_C(0))
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GETUP_ROLL;
                action_ticks = UINT16_C(0);
                scratch->tech_direction[player_index] =
                    horizontal_direction;
                velocity_x =
                    (int32_t)horizontal_direction *
                    fighter->getup_roll_speed_q16;
            }
            else if (up_held || shield_pressed)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL;
                action_ticks = UINT16_C(0);
                scratch->tech_direction[player_index] =
                    INT8_C(0);
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
            velocity_x =
                (int32_t)scratch->tech_direction[player_index] *
                fighter->tech_roll_speed_q16;
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
            }
        }
        else if (
            action_state == (uint8_t)PF_M4_ACTION_GETUP_ROLL)
        {
            velocity_x =
                (int32_t)scratch->tech_direction[player_index] *
                fighter->getup_roll_speed_q16;
            ++action_ticks;
            if (action_ticks >= fighter->getup_roll_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                velocity_x = INT32_C(0);
                scratch->tech_direction[player_index] =
                    INT8_C(0);
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
            }
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        pf_m4_action_is_ground_attack(action_state))
    {
        const uint32_t attack_ticks =
            action_state == (uint8_t)PF_M4_ACTION_STRONG_ATTACK
                ? (uint32_t)fighter->strong_startup_ticks +
                      (uint32_t)fighter->strong_active_ticks +
                      (uint32_t)fighter->strong_recovery_ticks
                : (uint32_t)fighter->jab_startup_ticks +
                      (uint32_t)fighter->jab_active_ticks +
                      (uint32_t)fighter->jab_recovery_ticks;

        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        ++action_ticks;
        if ((uint32_t)action_ticks >= attack_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            scratch->attack_hit_mask[player_index] = UINT8_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        if ((input->buttons & PF_INPUT_BUTTON_JUMP) == UINT64_C(0))
        {
            short_hop_latched = UINT8_C(1);
        }
        ++action_ticks;
        if (action_ticks >= fighter->jump_squat_ticks)
        {
            velocity_y =
                -(short_hop_latched != UINT8_C(0)
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
             grounded != UINT8_C(0) &&
             action_state == (uint8_t)PF_M4_ACTION_LANDING)
    {
        velocity_x = pf_m4_approach(
            velocity_x,
            INT32_C(0),
            fighter->traction_q16);
        ++action_ticks;
        if (action_ticks >= fighter->landing_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
             grounded != UINT8_C(0) &&
             action_state !=
                 (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
             input->main_stick_y >=
                 (int16_t)fighter->crouch_axis_threshold)
    {
        if (support == (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            platform_drop_ticks =
                (uint8_t)fighter->platform_drop_ticks;
            position_y += fighter->platform_drop_nudge_q16;
            velocity_y = fighter->gravity_q16;
            fast_fall = UINT8_C(0);
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_RUN)
        {
            action_state = (uint8_t)PF_M4_ACTION_RUN_BRAKE;
            action_ticks = UINT16_C(1);
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                fighter->traction_q16);
            dash_direction = INT8_C(0);
        }
        else
        {
            action_state = (uint8_t)PF_M4_ACTION_CROUCH;
            action_ticks = UINT16_C(0);
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                fighter->traction_q16);
            dash_direction = INT8_C(0);
        }
    }
    else if (!ledge_motion_handled &&
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

        if (action_state ==
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND)
        {
            const int8_t target_direction = dash_direction;
            const int target_held =
                horizontal_direction == target_direction &&
                horizontal_magnitude >=
                    fighter->run_continue_axis_threshold;

            facing = target_direction;
            velocity_x = pf_m4_approach(
                velocity_x,
                (int32_t)target_direction *
                    fighter->run_speed_q16,
                fighter->turn_acceleration_q16);
            ++action_ticks;
            if (action_ticks >= fighter->run_turnaround_ticks)
            {
                dash_direction = INT8_C(0);
                action_ticks = UINT16_C(0);
                if (target_held)
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
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
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_RUN_BRAKE)
        {
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                fighter->traction_q16);
            ++action_ticks;
            if (run_turnaround_requested)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
                action_ticks = UINT16_C(1);
                dash_direction = horizontal_direction;
                facing = horizontal_direction;
                velocity_x = pf_m4_approach(
                    velocity_x,
                    (int32_t)horizontal_direction *
                        fighter->run_speed_q16,
                    fighter->turn_acceleration_q16);
            }
            else if (run_continues)
            {
                action_state = (uint8_t)PF_M4_ACTION_RUN;
                action_ticks =
                    fighter->run_turnaround_lockout_ticks;
                velocity_x = pf_m4_approach(
                    velocity_x,
                    (int32_t)facing * fighter->run_speed_q16,
                    fighter->ground_acceleration_q16);
            }
            else if (action_ticks >= fighter->run_brake_ticks)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                action_ticks = UINT16_C(0);
                dash_direction = INT8_C(0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_RUN)
        {
            if (action_ticks <
                fighter->run_turnaround_lockout_ticks)
            {
                ++action_ticks;
                velocity_x = pf_m4_approach(
                    velocity_x,
                    (int32_t)facing * fighter->run_speed_q16,
                    fighter->ground_acceleration_q16);
            }
            else if (run_turnaround_requested)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
                action_ticks = UINT16_C(1);
                dash_direction = horizontal_direction;
                facing = horizontal_direction;
                velocity_x = pf_m4_approach(
                    velocity_x,
                    (int32_t)horizontal_direction *
                        fighter->run_speed_q16,
                    fighter->turn_acceleration_q16);
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
                velocity_x = pf_m4_approach(
                    velocity_x,
                    (int32_t)facing * fighter->run_speed_q16,
                    fighter->ground_acceleration_q16);
            }
        }
        else
        {
            const int dash_started =
                strong_direction != INT8_C(0) &&
                (previous_strong_direction == INT8_C(0) ||
                 (action_state ==
                      (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                  strong_direction == -dash_direction));

            if (dash_started)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                action_ticks = UINT16_C(1);
                dash_direction = strong_direction;
                facing = strong_direction;
                velocity_x =
                    (int32_t)strong_direction *
                    fighter->initial_dash_speed_q16;
            }
            else if (action_state ==
                         (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                     strong_direction == dash_direction &&
                     action_ticks < fighter->initial_dash_ticks)
            {
                velocity_x =
                    (int32_t)dash_direction *
                    fighter->initial_dash_speed_q16;
                ++action_ticks;
                if (action_ticks >= fighter->initial_dash_ticks)
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
                    action_ticks =
                        fighter->run_turnaround_lockout_ticks;
                    dash_direction = INT8_C(0);
                }
            }
            else if (horizontal_magnitude >
                     fighter->axis_dead_zone)
            {
                int32_t target;
                int32_t acceleration;

                facing = horizontal_direction;
                dash_direction = INT8_C(0);
                if (strong_direction != INT8_C(0))
                {
                    action_state = (uint8_t)PF_M4_ACTION_RUN;
                    action_ticks =
                        fighter->run_turnaround_lockout_ticks;
                    target =
                        (int32_t)horizontal_direction *
                        fighter->run_speed_q16;
                }
                else
                {
                    action_state = (uint8_t)PF_M4_ACTION_WALK;
                    action_ticks = UINT16_C(0);
                    target = pf_m4_scale_axis_q16(
                        input->main_stick_x,
                        fighter->walk_speed_q16);
                }
                acceleration =
                    pf_m4_signs_differ(velocity_x, target)
                        ? fighter->turn_acceleration_q16
                        : fighter->ground_acceleration_q16;
                velocity_x = pf_m4_approach(
                    velocity_x,
                    target,
                    acceleration);
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
    }

    if (!ledge_motion_handled &&
        grounded == UINT8_C(0) &&
        pf_m4_action_is_surface_tech(action_state))
    {
        dash_direction = INT8_C(0);
        ++action_ticks;
        if (pf_m4_action_is_wall_tech(action_state))
        {
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
            if (action_ticks >= fighter->wall_tech_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                action_ticks = UINT16_C(0);
                scratch->tech_direction[player_index] = INT8_C(0);
            }
        }
        else if (action_ticks >= fighter->ceiling_tech_ticks)
        {
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded == UINT8_C(0))
    {
        dash_direction = INT8_C(0);
        if (hitstun_locked)
        {
            action_state = (uint8_t)PF_M4_ACTION_HITSTUN;
        }
        else
        {
            const int32_t air_target = pf_m4_scale_axis_q16(
                input->main_stick_x,
                fighter->air_speed_q16);

            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            velocity_x = pf_m4_approach(
                velocity_x,
                air_target,
                fighter->air_acceleration_q16);

            if (!launched_this_tick &&
                jump_pressed &&
                air_jumps_remaining > UINT8_C(0))
            {
                velocity_y = -fighter->double_jump_speed_q16;
                --air_jumps_remaining;
                fast_fall = UINT8_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
            }
        }
    }

    if (action_state != (uint8_t)PF_M4_ACTION_SHIELD &&
        action_state != (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
        action_state != (uint8_t)PF_M4_ACTION_SHIELD_BREAK &&
        shield_reset_this_tick == 0)
    {
        scratch->shield_health_q16[player_index] =
            pf_m4_shield_health_add(
                scratch->shield_health_q16[player_index],
                fighter->shield_regeneration_q16,
                fighter->shield_health_q16);
    }

    previous_position_x = position_x;
    next_position = (int64_t)position_x + (int64_t)velocity_x;
    if (!ledge_motion_handled &&
        !pf_m4_checked_i32(next_position, &position_x))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    if (!ledge_motion_handled)
    {
        const int64_t body_top =
            (int64_t)position_y - fighter->half_height_q16;
        const int64_t body_bottom =
            (int64_t)position_y + fighter->half_height_q16;
        const int vertical_overlap =
            body_bottom > (int64_t)stage->solid_top_q16 &&
            body_top < (int64_t)stage->solid_bottom_q16;
        int8_t away_direction = INT8_C(0);

        if (vertical_overlap &&
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

        if (away_direction != INT8_C(0))
        {
            if (grounded == UINT8_C(0) &&
                scratch->tumble[player_index] != UINT8_C(0))
            {
                const int up_held =
                    input->main_stick_y <=
                    -(int16_t)fighter->crouch_axis_threshold;

                pf_m4_enter_wall_impact(
                    fighter,
                    jump_pressed || up_held,
                    away_direction,
                    scratch,
                    player_index,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &action_state,
                    &fast_fall,
                    &facing);
            }
            else
            {
                velocity_x = INT32_C(0);
            }
        }
    }

    if (!ledge_motion_handled &&
        grounded != UINT8_C(0))
    {
        int32_t surface_left;
        int32_t surface_right;

        pf_m4_surface_bounds_q16(
            content,
            support,
            world->tick + UINT64_C(1),
            &surface_left,
            &surface_right);
        if (position_x < surface_left || position_x > surface_right)
        {
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
            short_hop_latched = UINT8_C(0);
            fast_fall = UINT8_C(0);
            dash_direction = INT8_C(0);
            scratch->shield_stun_ticks[player_index] =
                UINT16_C(0);
            scratch->powershield[player_index] = UINT8_C(0);
            scratch->tech_direction[player_index] = INT8_C(0);
        }
        else
        {
            position_y =
                pf_m4_surface_y_q16(content, support) -
                fighter->half_height_q16;
            velocity_y = INT32_C(0);
        }
    }

    if (!ledge_motion_handled &&
        grounded == UINT8_C(0))
    {
        const int32_t previous_bottom =
            position_y + fighter->half_height_q16;
        const int32_t previous_top =
            position_y - fighter->half_height_q16;
        const int wall_tech_stalled =
            pf_m4_action_is_wall_tech(action_state) &&
            action_ticks < fighter->wall_tech_stall_ticks;
        int32_t new_bottom;
        int32_t new_top;

        if (wall_tech_stalled)
        {
            velocity_y = INT32_C(0);
        }
        else if (!hitstun_locked &&
            !pf_m4_action_is_surface_tech(action_state) &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold &&
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
            (int64_t)position_y +
            (wall_tech_stalled ? INT64_C(0) : (int64_t)velocity_y);
        if (!pf_m4_checked_i32(next_position, &position_y))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        new_bottom = position_y + fighter->half_height_q16;
        new_top = position_y - fighter->half_height_q16;

        if (velocity_y >= INT32_C(0))
        {
            const int32_t platform_center =
                pf_m4_platform_center_x_q16(
                    stage,
                    world->tick + UINT64_C(1));
            const int32_t platform_left =
                platform_center - stage->platform_half_width_q16;
            const int32_t platform_right =
                platform_center + stage->platform_half_width_q16;
            const int down_held =
                input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;

            if (position_x >= stage->solid_left_q16 &&
                position_x <= stage->solid_right_q16 &&
                previous_bottom <= stage->solid_top_q16 &&
                new_bottom >= stage->solid_top_q16)
            {
                pf_m4_land_from_air(
                    fighter,
                    stage->solid_top_q16,
                    (uint8_t)PF_M4_SURFACE_SOLID_TOP,
                    input->main_stick_x,
                    scratch,
                    player_index,
                    &position_y,
                    &velocity_x,
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
            else if (!down_held &&
                platform_drop_ticks == UINT8_C(0) &&
                position_x >= platform_left &&
                position_x <= platform_right &&
                previous_bottom <= stage->platform_y_q16 &&
                new_bottom >= stage->platform_y_q16)
            {
                pf_m4_land_from_air(
                    fighter,
                    stage->platform_y_q16,
                    (uint8_t)PF_M4_SURFACE_PLATFORM,
                    input->main_stick_x,
                    scratch,
                    player_index,
                    &position_y,
                    &velocity_x,
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
            else if (position_x >= stage->floor_left_q16 &&
                     position_x <= stage->floor_right_q16 &&
                     previous_bottom <= stage->floor_y_q16 &&
                     new_bottom >= stage->floor_y_q16)
            {
                pf_m4_land_from_air(
                    fighter,
                    stage->floor_y_q16,
                    (uint8_t)PF_M4_SURFACE_FLOOR,
                    input->main_stick_x,
                    scratch,
                    player_index,
                    &position_y,
                    &velocity_x,
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
        }
        else if (
            position_x >= stage->solid_left_q16 &&
            position_x <= stage->solid_right_q16 &&
            previous_top >= stage->solid_bottom_q16 &&
            new_top <= stage->solid_bottom_q16)
        {
            position_y =
                stage->solid_bottom_q16 + fighter->half_height_q16;
            if (scratch->tumble[player_index] != UINT8_C(0))
            {
                pf_m4_enter_ceiling_impact(
                    fighter,
                    input->main_stick_x,
                    scratch,
                    player_index,
                    &velocity_x,
                    &velocity_y,
                    &action_ticks,
                    &action_state,
                    &fast_fall);
            }
            else
            {
                velocity_y = INT32_C(0);
            }
        }
    }

    if (hitstun_locked)
    {
        if (scratch->hitstun_ticks[player_index] > UINT16_C(0))
        {
            --scratch->hitstun_ticks[player_index];
        }
        if (grounded != UINT8_C(0))
        {
            scratch->hitstun_ticks[player_index] = UINT16_C(0);
        }
        if (scratch->hitstun_ticks[player_index] == UINT16_C(0) &&
            action_state == (uint8_t)PF_M4_ACTION_HITSTUN)
        {
            action_state =
                grounded != UINT8_C(0)
                    ? (uint8_t)PF_M4_ACTION_LANDING
                    : (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
        }
    }

    if (!ledge_motion_handled &&
        !released_ledge_this_tick &&
        grounded == UINT8_C(0) &&
        action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
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
                &grounded,
                &action_state,
                &support,
                &air_jumps_remaining,
                &short_hop_latched,
                &fast_fall,
                facing,
                &dash_direction))
        {
            scratch->tumble[player_index] = UINT8_C(0);
            scratch->tech_direction[player_index] = INT8_C(0);
        }
    }

    if (position_x < stage->blast_left_q16 ||
        position_x > stage->blast_right_q16 ||
        position_y < stage->blast_top_q16 ||
        position_y > stage->blast_bottom_q16)
    {
        const int32_t centered_slot =
            (int32_t)(UINT32_C(2) * player_index + UINT32_C(1)) -
            (int32_t)world->player_count;

        position_x = centered_slot * stage->spawn_spacing_q16;
        position_y =
            stage->floor_y_q16 - fighter->half_height_q16;
        velocity_x = INT32_C(0);
        velocity_y = INT32_C(0);
        action_ticks = UINT16_C(0);
        if (respawn_count != UINT16_MAX)
        {
            ++respawn_count;
        }
        grounded = UINT8_C(1);
        action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        support = (uint8_t)PF_M4_SURFACE_FLOOR;
        air_jumps_remaining = fighter->air_jump_count;
        short_hop_latched = UINT8_C(0);
        platform_drop_ticks = UINT8_C(0);
        fast_fall = UINT8_C(0);
        facing =
            centered_slot <= INT32_C(0) ? INT8_C(1) : INT8_C(-1);
        dash_direction = INT8_C(0);
        previous_strong_direction = INT8_C(0);
        scratch->damage_q16[player_index] = UINT32_C(0);
        scratch->pending_velocity_x_q16[player_index] = INT32_C(0);
        scratch->pending_velocity_y_q16[player_index] = INT32_C(0);
        scratch->hitlag_ticks[player_index] = UINT16_C(0);
        scratch->hitstun_ticks[player_index] = UINT16_C(0);
        scratch->tech_window_ticks[player_index] = UINT16_C(0);
        scratch->tech_lockout_ticks[player_index] = UINT16_C(0);
        scratch->shield_stun_ticks[player_index] = UINT16_C(0);
        scratch->shield_health_q16[player_index] =
            fighter->shield_health_q16;
        scratch->hitlag_resume_action[player_index] = UINT8_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->shield_held[player_index] = UINT8_C(0);
        scratch->powershield[player_index] = UINT8_C(0);
        scratch->tumble[player_index] = UINT8_C(0);
        scratch->sdi_direction_x[player_index] = INT8_C(0);
        scratch->sdi_direction_y[player_index] = INT8_C(0);
        scratch->tech_direction[player_index] = INT8_C(0);
    }
    else
    {
        previous_strong_direction = strong_direction;
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
        respawn_count,
        grounded,
        action_state,
        support,
        air_jumps_remaining,
        short_hop_latched,
        platform_drop_ticks,
        fast_fall,
        facing,
        dash_direction,
        previous_strong_direction);
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
    out_inspection->tick = sim->world.tick;

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

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        pf_m4_player_inspection *player =
            &out_inspection->players[player_index];

        player->position_x_q16 =
            sim->world.position_x_q16[player_index];
        player->position_y_q16 =
            sim->world.position_y_q16[player_index];
        player->velocity_x_q16 =
            sim->world.velocity_x_q16[player_index];
        player->velocity_y_q16 =
            sim->world.velocity_y_q16[player_index];
        player->action_ticks =
            sim->world.action_ticks[player_index];
        player->respawn_count =
            sim->world.respawn_count[player_index];
        player->action_state =
            sim->world.action_state[player_index];
        player->facing = sim->world.facing[player_index];
        player->dash_direction =
            sim->world.dash_direction[player_index];
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
            sim->world.shield_held[player_index];
        player->powershield =
            sim->world.powershield[player_index];
        player->tumble = sim->world.tumble[player_index];
        player->invulnerable =
            pf_m4_action_is_recovery_invulnerable(
                &sim->content.fighter,
                player->action_state,
                player->action_ticks)
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
        player->shield_health_q16 =
            sim->world.shield_health_q16[player_index];
        player->hitbox_active = (uint8_t)pf_m4_attack_hitbox(
            &sim->content,
            player->position_x_q16,
            player->position_y_q16,
            player->facing,
            player->action_state,
            player->action_ticks,
            &player->hitbox_left_q16,
            &player->hitbox_right_q16,
            &player->hitbox_top_q16,
            &player->hitbox_bottom_q16);
    }
    return PF_STATUS_OK;
}
