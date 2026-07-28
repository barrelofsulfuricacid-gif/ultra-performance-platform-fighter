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
    return support == (uint8_t)PF_M4_SURFACE_PLATFORM
               ? content->stage.platform_y_q16
               : content->stage.floor_y_q16;
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
    else
    {
        *out_left = content->stage.floor_left_q16;
        *out_right = content->stage.floor_right_q16;
    }
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
    int64_t next_position;

    if (platform_drop_ticks > UINT8_C(0))
    {
        --platform_drop_ticks;
    }

    if (grounded != UINT8_C(0) &&
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

    if (grounded != UINT8_C(0) &&
        action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
        action_state != (uint8_t)PF_M4_ACTION_LANDING &&
        jump_pressed)
    {
        action_state = (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
        action_ticks = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
    }

    if (grounded != UINT8_C(0) &&
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
    else if (grounded != UINT8_C(0) &&
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
    else if (grounded != UINT8_C(0) &&
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
    else if (grounded != UINT8_C(0))
    {
        const int dash_started =
            strong_direction != INT8_C(0) &&
            (previous_strong_direction == INT8_C(0) ||
             (action_state ==
                  (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
              strong_direction == -dash_direction));

        if (dash_started)
        {
            action_state = (uint8_t)PF_M4_ACTION_INITIAL_DASH;
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
                action_ticks = UINT16_C(0);
            }
        }
        else if (horizontal_magnitude > fighter->axis_dead_zone)
        {
            int32_t target;
            int32_t acceleration;

            facing = horizontal_direction;
            dash_direction = INT8_C(0);
            action_ticks = UINT16_C(0);
            if (strong_direction != INT8_C(0))
            {
                action_state = (uint8_t)PF_M4_ACTION_RUN;
                target =
                    (int32_t)horizontal_direction *
                    fighter->run_speed_q16;
            }
            else
            {
                action_state = (uint8_t)PF_M4_ACTION_WALK;
                target = pf_m4_scale_axis_q16(
                    input->main_stick_x,
                    fighter->walk_speed_q16);
            }
            acceleration =
                pf_m4_signs_differ(velocity_x, target)
                    ? fighter->turn_acceleration_q16
                    : fighter->ground_acceleration_q16;
            velocity_x =
                pf_m4_approach(velocity_x, target, acceleration);
        }
        else
        {
            action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
            action_ticks = UINT16_C(0);
            dash_direction = INT8_C(0);
            velocity_x = pf_m4_approach(
                velocity_x,
                INT32_C(0),
                fighter->traction_q16);
        }
    }

    if (grounded == UINT8_C(0))
    {
        const int32_t air_target = pf_m4_scale_axis_q16(
            input->main_stick_x,
            fighter->air_speed_q16);

        action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
        action_ticks = UINT16_C(0);
        dash_direction = INT8_C(0);
        if (horizontal_direction != INT8_C(0))
        {
            facing = horizontal_direction;
        }
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
        }
    }

    next_position = (int64_t)position_x + (int64_t)velocity_x;
    if (!pf_m4_checked_i32(next_position, &position_x))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    if (grounded != UINT8_C(0))
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
        }
        else
        {
            position_y =
                pf_m4_surface_y_q16(content, support) -
                fighter->half_height_q16;
            velocity_y = INT32_C(0);
        }
    }

    if (grounded == UINT8_C(0))
    {
        const int32_t previous_bottom =
            position_y + fighter->half_height_q16;
        int32_t new_bottom;

        if (input->main_stick_y >=
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

        next_position = (int64_t)position_y + (int64_t)velocity_y;
        if (!pf_m4_checked_i32(next_position, &position_y))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        new_bottom = position_y + fighter->half_height_q16;

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

            if (!down_held &&
                platform_drop_ticks == UINT8_C(0) &&
                position_x >= platform_left &&
                position_x <= platform_right &&
                previous_bottom <= stage->platform_y_q16 &&
                new_bottom >= stage->platform_y_q16)
            {
                pf_m4_land(
                    fighter,
                    stage->platform_y_q16,
                    (uint8_t)PF_M4_SURFACE_PLATFORM,
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
            else if (position_x >= stage->floor_left_q16 &&
                     position_x <= stage->floor_right_q16 &&
                     previous_bottom <= stage->floor_y_q16 &&
                     new_bottom >= stage->floor_y_q16)
            {
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
    }
    return PF_STATUS_OK;
}
