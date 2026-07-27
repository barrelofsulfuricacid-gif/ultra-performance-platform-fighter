#include "sim_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define PF_M2_HORIZONTAL_ACCEL_Q16 INT32_C(8192)
#define PF_M2_HORIZONTAL_FRICTION_Q16 INT32_C(4096)
#define PF_M2_MAX_HORIZONTAL_SPEED_Q16 INT32_C(131072)
#define PF_M2_JUMP_SPEED_Q16 INT32_C(196608)
#define PF_M2_GRAVITY_Q16 INT32_C(8192)
#define PF_M2_AXIS_DEAD_ZONE INT16_C(4096)

static int32_t pf_clamp_i64_to_i32_range(
    int64_t value,
    int32_t minimum,
    int32_t maximum)
{
    if (value < (int64_t)minimum)
    {
        return minimum;
    }
    if (value > (int64_t)maximum)
    {
        return maximum;
    }
    return (int32_t)value;
}

static int32_t pf_scale_axis_q16(int16_t axis, int32_t magnitude_q16)
{
    const int64_t denominator =
        axis < INT16_C(0) ? INT64_C(32768) : INT64_C(32767);
    return (int32_t)(((int64_t)axis * (int64_t)magnitude_q16) /
                     denominator);
}

static int32_t pf_approach_zero(int32_t value, int32_t amount)
{
    if (value > amount)
    {
        return value - amount;
    }
    if (value < -amount)
    {
        return value + amount;
    }
    return INT32_C(0);
}

static void pf_write_result(const pf_world_state *world, pf_tick_result *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->completed_tick = world->tick;
    result->fault_flags = world->fault_flags;
    result->terminated = world->terminated;
    result->truncated = world->truncated;
    result->winner_mask = world->winner_mask;
}

static pf_status pf_validate_inputs(
    const pf_world_state *world,
    const pf_input_frame *inputs,
    size_t player_count)
{
    size_t player_index;

    if (inputs == NULL ||
        player_count != (size_t)world->player_count)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    for (player_index = (size_t)0; player_index < player_count; ++player_index)
    {
        const pf_input_frame *input = &inputs[player_index];
        if (input->schema_version != PF_SIM_INPUT_SCHEMA_VERSION)
        {
            return PF_STATUS_UNSUPPORTED_VERSION;
        }
        if (input->reserved != UINT8_C(0) ||
            input->player_slot != (uint8_t)player_index ||
            (input->buttons & ~PF_INPUT_KNOWN_BUTTONS) != UINT64_C(0))
        {
            return PF_STATUS_INVALID_ARGUMENT;
        }
        if (input->tick != world->tick)
        {
            return PF_STATUS_TICK_MISMATCH;
        }
    }

    return PF_STATUS_OK;
}

pf_status pf_sim_tick_impl(
    pf_sim *sim,
    const pf_input_frame *inputs,
    size_t player_count,
    pf_tick_result *out_result)
{
    pf_world_state *world;
    pf_sim_scratch *scratch;
    pf_status status;
    uint64_t forfeit_mask = UINT64_C(0);
    uint32_t player_index;

    if (out_result == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_result, 0, sizeof(*out_result));

    if (!pf_sim_is_valid(sim) || sim->has_reset == UINT8_C(0))
    {
        return PF_STATUS_INVALID_STATE;
    }

    world = &sim->world;
    scratch = sim->scratch;

    if (world->fault_flags != UINT32_C(0))
    {
        pf_write_result(world, out_result);
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (world->terminated != UINT8_C(0) ||
        world->truncated != UINT8_C(0))
    {
        pf_write_result(world, out_result);
        return PF_STATUS_EPISODE_DONE;
    }

    status = pf_validate_inputs(world, inputs, player_count);
    if (status != PF_STATUS_OK)
    {
        pf_write_result(world, out_result);
        return status;
    }

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        const pf_input_frame *input = &inputs[player_index];
        int32_t velocity_x = world->velocity_x_q16[player_index];
        int32_t velocity_y = world->velocity_y_q16[player_index];
        uint8_t grounded = world->grounded[player_index];
        int64_t position_x;
        int64_t position_y;

        if ((input->buttons & PF_INPUT_BUTTON_FORFEIT) != UINT64_C(0))
        {
            forfeit_mask |= UINT64_C(1) << player_index;
        }

        if (input->main_stick_x > PF_M2_AXIS_DEAD_ZONE ||
            input->main_stick_x < -PF_M2_AXIS_DEAD_ZONE)
        {
            const int32_t acceleration =
                pf_scale_axis_q16(
                    input->main_stick_x,
                    PF_M2_HORIZONTAL_ACCEL_Q16);
            velocity_x = pf_clamp_i64_to_i32_range(
                (int64_t)velocity_x + (int64_t)acceleration,
                -PF_M2_MAX_HORIZONTAL_SPEED_Q16,
                PF_M2_MAX_HORIZONTAL_SPEED_Q16);
        }
        else
        {
            velocity_x = pf_approach_zero(
                velocity_x,
                PF_M2_HORIZONTAL_FRICTION_Q16);
        }

        if (grounded != UINT8_C(0) &&
            (input->buttons & PF_INPUT_BUTTON_JUMP) != UINT64_C(0) &&
            (world->previous_buttons[player_index] &
             PF_INPUT_BUTTON_JUMP) == UINT64_C(0))
        {
            velocity_y = PF_M2_JUMP_SPEED_Q16;
            grounded = UINT8_C(0);
        }

        if (grounded == UINT8_C(0))
        {
            velocity_y = pf_clamp_i64_to_i32_range(
                (int64_t)velocity_y - (int64_t)PF_M2_GRAVITY_Q16,
                -PF_M2_JUMP_SPEED_Q16,
                PF_M2_JUMP_SPEED_Q16);
        }

        position_x =
            (int64_t)world->position_x_q16[player_index] +
            (int64_t)velocity_x;
        if (position_x <= -(int64_t)world->arena_half_width_q16)
        {
            position_x = -(int64_t)world->arena_half_width_q16;
            velocity_x = INT32_C(0);
        }
        else if (position_x >=
                 (int64_t)world->arena_half_width_q16)
        {
            position_x = (int64_t)world->arena_half_width_q16;
            velocity_x = INT32_C(0);
        }

        position_y =
            (int64_t)world->position_y_q16[player_index] +
            (int64_t)velocity_y;
        if (position_y <= INT64_C(0))
        {
            position_y = INT64_C(0);
            velocity_y = INT32_C(0);
            grounded = UINT8_C(1);
        }
        else if (position_y >= (int64_t)world->arena_ceiling_q16)
        {
            position_y = (int64_t)world->arena_ceiling_q16;
            velocity_y = INT32_C(0);
        }

        scratch->position_x_q16[player_index] = (int32_t)position_x;
        scratch->position_y_q16[player_index] = (int32_t)position_y;
        scratch->velocity_x_q16[player_index] = velocity_x;
        scratch->velocity_y_q16[player_index] = velocity_y;
        scratch->previous_buttons[player_index] = input->buttons;
        scratch->grounded[player_index] = grounded;
    }

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)world->player_count;
         ++player_index)
    {
        world->position_x_q16[player_index] =
            scratch->position_x_q16[player_index];
        world->position_y_q16[player_index] =
            scratch->position_y_q16[player_index];
        world->velocity_x_q16[player_index] =
            scratch->velocity_x_q16[player_index];
        world->velocity_y_q16[player_index] =
            scratch->velocity_y_q16[player_index];
        world->previous_buttons[player_index] =
            scratch->previous_buttons[player_index];
        world->grounded[player_index] =
            scratch->grounded[player_index];
    }

    ++world->tick;

    if (forfeit_mask != UINT64_C(0))
    {
        const uint64_t active_mask =
            (UINT64_C(1) << world->player_count) - UINT64_C(1);
        world->terminated = UINT8_C(1);
        world->winner_mask =
            (uint8_t)(active_mask & ~forfeit_mask);
    }
    if (world->tick >= world->max_ticks)
    {
        world->truncated = UINT8_C(1);
    }

    pf_write_result(world, out_result);
    return PF_STATUS_OK;
}
