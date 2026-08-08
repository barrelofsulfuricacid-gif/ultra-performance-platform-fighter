#include "sim_internal.h"
#include "sim_falcon_frame_data.h"

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

/* atan(i / 64) in unsigned-turn units. The compact octant table keeps guard
 * steering deterministic across native and Wasm builds without a libm call. */
static const uint16_t pf_m4_atan_turn_table[65] = {
    UINT16_C(0), UINT16_C(163), UINT16_C(326), UINT16_C(489),
    UINT16_C(651), UINT16_C(813), UINT16_C(975), UINT16_C(1136),
    UINT16_C(1297), UINT16_C(1457), UINT16_C(1617), UINT16_C(1775),
    UINT16_C(1933), UINT16_C(2090), UINT16_C(2246), UINT16_C(2401),
    UINT16_C(2555), UINT16_C(2708), UINT16_C(2860), UINT16_C(3010),
    UINT16_C(3159), UINT16_C(3307), UINT16_C(3453), UINT16_C(3599),
    UINT16_C(3742), UINT16_C(3884), UINT16_C(4025), UINT16_C(4164),
    UINT16_C(4302), UINT16_C(4438), UINT16_C(4572), UINT16_C(4705),
    UINT16_C(4836), UINT16_C(4966), UINT16_C(5094), UINT16_C(5220),
    UINT16_C(5344), UINT16_C(5467), UINT16_C(5589), UINT16_C(5708),
    UINT16_C(5826), UINT16_C(5943), UINT16_C(6058), UINT16_C(6171),
    UINT16_C(6282), UINT16_C(6392), UINT16_C(6500), UINT16_C(6607),
    UINT16_C(6712), UINT16_C(6815), UINT16_C(6917), UINT16_C(7018),
    UINT16_C(7117), UINT16_C(7214), UINT16_C(7310), UINT16_C(7405),
    UINT16_C(7498), UINT16_C(7589), UINT16_C(7679), UINT16_C(7768),
    UINT16_C(7856), UINT16_C(7942), UINT16_C(8026), UINT16_C(8110),
    UINT16_C(8192)};

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

static uint16_t pf_m4_atan_octant_turn(uint32_t numerator, uint32_t denominator)
{
    const uint64_t position =
        ((uint64_t)numerator << 22U) / (uint64_t)denominator;
    const uint32_t index = (uint32_t)(position >> 16U);
    const uint32_t fraction = (uint32_t)position & UINT32_C(65535);
    const uint32_t lower = pf_m4_atan_turn_table[index];
    const uint32_t upper = pf_m4_atan_turn_table[
        index < UINT32_C(64) ? index + UINT32_C(1) : index];

    return (uint16_t)(
        lower +
        (((upper - lower) * fraction + UINT32_C(32768)) >> 16U));
}

static uint16_t pf_m4_atan2_turn(int32_t y, int32_t x)
{
    const uint32_t absolute_x =
        x < INT32_C(0) ? (uint32_t)(-x) : (uint32_t)x;
    const uint32_t absolute_y =
        y < INT32_C(0) ? (uint32_t)(-y) : (uint32_t)y;
    uint16_t octant;
    uint32_t angle;

    if ((absolute_x | absolute_y) == UINT32_C(0))
    {
        return UINT16_C(0);
    }
    octant = absolute_x >= absolute_y
                  ? pf_m4_atan_octant_turn(absolute_y, absolute_x)
                  : (uint16_t)(
                        UINT16_C(16384) -
                        pf_m4_atan_octant_turn(absolute_x, absolute_y));
    if (x >= INT32_C(0))
    {
        angle = y >= INT32_C(0)
                    ? (uint32_t)octant
                    : UINT32_C(65536) - (uint32_t)octant;
    }
    else
    {
        angle = y >= INT32_C(0)
                    ? UINT32_C(32768) - (uint32_t)octant
                    : UINT32_C(32768) + (uint32_t)octant;
    }
    /* GALE01 clamps angles in the final degree to 359 before smoothing. */
    return (uint16_t)(angle > UINT32_C(65354) ? UINT32_C(65354) : angle);
}

static int32_t pf_m4_half_nearest(int32_t value)
{
    return value < INT32_C(0)
               ? -((-value + INT32_C(1)) / INT32_C(2))
               : (value + INT32_C(1)) / INT32_C(2);
}

static uint32_t pf_m4_u64_sqrt(uint64_t value);

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
    if (support == (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM)
    {
        return content->stage.upper_platform_y_q16;
    }
    return content->stage.floor_y_q16;
}

static int pf_m4_action_uses_fall_special_pose(uint8_t action_state)
{
    return action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_MISS ||
           action_state ==
               (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_FALL_HIT ||
           action_state == (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL;
}

static int32_t pf_m4_floor_contact_bottom_extent_q16(
    const pf_m4_fighter_data *fighter,
    uint8_t action_state,
    uint16_t action_ticks)
{
    const pf_m4_falcon_collision_pose *pose =
        pf_m4_falcon_reference_collision_pose();
    int32_t bottom_y_from_origin_q16 = INT32_C(0);
    int has_reference_pose = 0;

    if (fighter->reference_frame_data_enabled == UINT8_C(0) || pose == NULL)
    {
        return fighter->half_height_q16;
    }
    if (action_state == (uint8_t)PF_M4_ACTION_AIR_DODGE)
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
    }
    else if (action_state == (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_HIT_AIR)
    {
        const uint16_t frame_index =
            action_ticks <
                    PF_M4_FALCON_RAPTOR_BOOST_HIT_AIR_ECB_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(
                      PF_M4_FALCON_RAPTOR_BOOST_HIT_AIR_ECB_FRAME_COUNT -
                      UINT16_C(1));

        bottom_y_from_origin_q16 =
            pose->raptor_boost_hit_air_bottom_y_from_origin_q16[frame_index];
        has_reference_pose = 1;
    }
    else if (pf_m4_action_uses_fall_special_pose(action_state))
    {
        const uint16_t frame_index =
            action_ticks < PF_M4_FALCON_FALL_SPECIAL_ECB_FRAME_COUNT
                ? action_ticks
                : (uint16_t)(
                      PF_M4_FALCON_FALL_SPECIAL_ECB_FRAME_COUNT -
                      UINT16_C(1));
        bottom_y_from_origin_q16 =
            pose->fall_special_bottom_y_from_origin_q16[frame_index];
        has_reference_pose = 1;
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
        return fighter->half_height_q16 - bottom_y_from_origin_q16;
    }
    return fighter->half_height_q16;
}

static int pf_m4_surface_is_pass_through(uint8_t support)
{
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
    int32_t distance_q16,
    int preserve_ground_support)
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
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_CLIMB ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
           action_state == (uint8_t)PF_M4_ACTION_LEDGE_ATTACK;
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
    int8_t facing)
{
    const int use_secondary_stick =
        (input->buttons & PF_INPUT_BUTTON_STRONG_ATTACK) != UINT64_C(0) &&
        (pf_m4_axis_magnitude(input->secondary_stick_x) >=
             fighter->axis_dead_zone ||
         pf_m4_axis_magnitude(input->secondary_stick_y) >=
             fighter->axis_dead_zone);
    const int16_t stick_x =
        use_secondary_stick != 0
            ? input->secondary_stick_x
            : input->main_stick_x;
    const int16_t stick_y =
        use_secondary_stick != 0
            ? input->secondary_stick_y
            : input->main_stick_y;
    const uint16_t horizontal =
        pf_m4_axis_magnitude(stick_x);
    const uint16_t vertical =
        pf_m4_axis_magnitude(stick_y);

    if (horizontal < fighter->dash_axis_threshold &&
        vertical < fighter->dash_axis_threshold)
    {
        return (uint8_t)PF_M4_ACTION_PUMMEL;
    }
    if (vertical > horizontal)
    {
        return stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_THROW_UP
                   : (uint8_t)PF_M4_ACTION_THROW_DOWN;
    }
    return (stick_x < INT16_C(0) ? INT8_C(-1) : INT8_C(1)) ==
                   facing
               ? (uint8_t)PF_M4_ACTION_THROW_FORWARD
               : (uint8_t)PF_M4_ACTION_THROW_BACK;
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
           action_state == (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
           action_state == (uint8_t)PF_M4_ACTION_JAB_FINAL;
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
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE;
}

static int pf_m4_action_is_smash_release(uint8_t action_state)
{
    return action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
           action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
}

static uint8_t pf_m4_smash_release_action(uint8_t charge_action)
{
    switch ((pf_m4_action_state)charge_action)
    {
        case PF_M4_ACTION_FORWARD_STRONG_CHARGE:
            return (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK;
        case PF_M4_ACTION_UP_STRONG_CHARGE:
            return (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK;
        case PF_M4_ACTION_DOWN_STRONG_CHARGE:
            return (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK;
        default:
            return (uint8_t)PF_M4_ACTION_GROUND_IDLE;
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
            return &fighter->forward_attack;
        case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
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

static int pf_m4_falcon_ground_iasa_active(
    const pf_m4_reference_move *move,
    uint16_t action_ticks)
{

    return move != NULL && move->iasa_frame != UINT16_C(0) &&
           (uint32_t)action_ticks + UINT32_C(1) >=
               (uint32_t)move->iasa_frame;
}

static uint8_t pf_m4_select_ground_light_attack_action(
    const pf_m4_fighter_data *fighter,
    int8_t facing,
    int16_t stick_x,
    int16_t stick_y)
{
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(stick_x);
    const uint16_t vertical_magnitude =
        pf_m4_axis_magnitude(stick_y);

    if (vertical_magnitude >= fighter->axis_dead_zone &&
        vertical_magnitude > horizontal_magnitude)
    {
        return stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_ATTACK
                   : (uint8_t)PF_M4_ACTION_DOWN_ATTACK;
    }
    if (horizontal_magnitude >= fighter->axis_dead_zone &&
        horizontal_magnitude >= vertical_magnitude &&
        pf_m4_axis_direction(stick_x, fighter->axis_dead_zone) == facing)
    {
        return (uint8_t)PF_M4_ACTION_FORWARD_ATTACK;
    }
    return (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
}

static uint8_t pf_m4_select_ground_strong_attack_action(
    const pf_m4_fighter_data *fighter,
    int16_t stick_x,
    int16_t stick_y)
{
    const uint16_t horizontal_magnitude =
        pf_m4_axis_magnitude(stick_x);
    const uint16_t vertical_magnitude =
        pf_m4_axis_magnitude(stick_y);

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
        const pf_m4_getup_roll_timing *timing =
            pf_m4_getup_roll_timing_for(
                fighter,
                prone_orientation,
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
               fighter->getup_attack_invulnerability_ticks;
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
    uint16_t *ledge_invulnerability_ticks,
    uint16_t ledge_regrab_lockout_ticks,
    uint8_t action_state_before_catch,
    uint16_t action_ticks_before_catch,
    int32_t previous_position_x,
    int8_t facing,
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

    if (fighter->reference_frame_data_enabled != UINT8_C(0) &&
        (action_state_before_catch ==
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
         action_state_before_catch ==
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR))
    {
        const pf_m4_falcon_ledge_attributes *ledge_attributes =
            pf_m4_falcon_reference_ledge_attributes();
        const pf_m4_falcon_collision_pose *pose =
            pf_m4_falcon_reference_collision_pose();

        if (ledge_attributes != NULL && pose != NULL)
        {
            const uint16_t frame_index =
                action_ticks_before_catch > UINT16_C(0) &&
                        action_ticks_before_catch <=
                            PF_M4_FALCON_DIVE_ECB_FRAME_COUNT
                    ? (uint16_t)(action_ticks_before_catch - UINT16_C(1))
                    : (uint16_t)(
                          PF_M4_FALCON_DIVE_ECB_FRAME_COUNT - UINT16_C(1));

            horizontal_reach =
                (int64_t)ledge_attributes->snap_x_q16 +
                (int64_t)pose
                    ->falcon_dive_right_x_from_origin_q16[frame_index];
            melee_bottom_extent_q16 =
                pose->falcon_dive_bottom_y_from_origin_q16[frame_index];
            catch_top =
                stage->floor_y_q16 + ledge_attributes->snap_y_q16 -
                ledge_attributes->snap_height_q16 / INT32_C(2);
            catch_bottom =
                stage->floor_y_q16 + ledge_attributes->snap_y_q16 +
                ledge_attributes->snap_height_q16 / INT32_C(2);
            left_probe_position_x =
                previous_position_x > *position_x
                    ? previous_position_x
                    : *position_x;
            right_probe_position_x =
                previous_position_x < *position_x
                    ? previous_position_x
                    : *position_x;
            use_melee_ledge_probe = 1;
        }
    }

    if (ledge_regrab_lockout_ticks != UINT16_C(0) ||
        *velocity_y < INT32_C(0) ||
        (use_melee_ledge_probe != 0 && *velocity_y == INT32_C(0)) ||
        *position_y < catch_top ||
        (use_melee_ledge_probe == 0 && *position_y > catch_bottom) ||
        (use_melee_ledge_probe != 0 &&
         (int64_t)*position_y - (int64_t)*velocity_y >
             (int64_t)catch_bottom) ||
        (use_melee_ledge_probe != 0 &&
         (int64_t)*position_y - (int64_t)melee_bottom_extent_q16 <=
             (int64_t)stage->floor_y_q16))
    {
        return 0;
    }

    if (*position_x < stage->floor_left_q16 &&
        facing == INT8_C(1) &&
        (int64_t)stage->floor_left_q16 -
                (int64_t)left_probe_position_x <=
            horizontal_reach)
    {
        ledge = (uint8_t)PF_M4_LEDGE_LEFT;
    }
    else if (*position_x > stage->floor_right_q16 &&
             facing == INT8_C(-1) &&
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
            }
        }
    }

    sim->world.previous_buttons[player_index] = UINT64_C(0);
    sim->world.position_x_q16[player_index] =
        centered_slot * stage->spawn_spacing_q16;
    sim->world.position_y_q16[player_index] =
        stage->floor_y_q16 - fighter->half_height_q16;
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.shield_recoil_x_q16[player_index] = INT32_C(0);
    sim->world.shield_recoil_mask =
        (uint8_t)(
            sim->world.shield_recoil_mask &
            (uint8_t)~(UINT8_C(1) << player_index));
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.respawn_count[player_index] = respawn_count;
    sim->world.respawn_ticks[player_index] = UINT16_C(0);
    sim->world.respawn_invulnerability_ticks[player_index] =
        UINT16_C(0);
    sim->world.ledge_invulnerability_ticks[player_index] =
        UINT16_C(0);
    sim->world.ledge_regrab_lockout_ticks[player_index] =
        UINT16_C(0);
    sim->world.grab_escape_ticks[player_index] = UINT16_C(0);
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
        (uint8_t)PF_M4_SURFACE_FLOOR;
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
    sim->world.previous_dodge_down[player_index] = UINT8_C(0);
    sim->world.previous_tilt_x_direction[player_index] = INT8_C(0);
    sim->world.previous_tilt_y_direction[player_index] = INT8_C(0);
    sim->world.tilt_x_age[player_index] = UINT8_C(254);
    sim->world.tilt_y_age[player_index] = UINT8_C(254);
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
    sim->world.attack_stale_registered[player_index] = UINT8_C(0);
    sim->world.falcon_kick_hit_count[player_index] = UINT8_C(0);
    sim->world.stale_move_count[player_index] = UINT8_C(0);
    (void)memset(
        sim->world.stale_move_ids[player_index],
        0,
        sizeof(sim->world.stale_move_ids[player_index]));
    sim->world.last_hit_attacker[player_index] = UINT8_C(0);
    sim->world.shield_held[player_index] = UINT8_C(0);
    sim->world.trigger_input_age[player_index] = UINT8_MAX;
    sim->world.powershield[player_index] = UINT8_C(0);
    sim->world.tumble[player_index] = UINT8_C(0);
    sim->world.sdi_pulse_count[player_index] = UINT8_C(0);
    sim->world.sdi_direction_x[player_index] = INT8_C(0);
    sim->world.sdi_direction_y[player_index] = INT8_C(0);
    sim->world.tech_direction[player_index] = INT8_C(0);
    sim->world.prone_orientation[player_index] =
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

static void pf_m4_land_from_air(
    const pf_m4_fighter_data *fighter,
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
    const int32_t incoming_velocity_x = *velocity_x;

    scratch->prone_orientation[player_index] =
        (uint8_t)PF_M4_PRONE_NONE;

    if (*action_state == (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
    {
        *position_y = surface_y_q16 - fighter->half_height_q16;
        *velocity_x = INT32_C(0);
        *velocity_y = INT32_C(0);
        *action_ticks = UINT16_C(0);
        *grounded = UINT8_C(1);
        *action_state =
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN;
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
        *dash_direction =
            incoming_velocity_x < INT32_C(0)
                ? INT8_C(-1)
                : (incoming_velocity_x > INT32_C(0)
                       ? INT8_C(1)
                       : INT8_C(0));
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
           action_state == (uint8_t)PF_M4_ACTION_TEETER ||
           action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
           action_state == (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
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
           action_state == (uint8_t)PF_M4_ACTION_RUN_TURNAROUND ||
           action_state == (uint8_t)PF_M4_ACTION_TEETER;
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
    uint8_t recovery_available,
    uint8_t short_hop_latched,
    uint8_t platform_drop_ticks,
    uint8_t fast_fall,
    int8_t facing,
    int8_t dash_direction,
    int8_t previous_strong_direction,
    uint8_t previous_dodge_down,
    int8_t tilt_x_direction,
    int8_t tilt_y_direction,
    uint8_t tilt_x_age,
    uint8_t tilt_y_age)
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
    scratch->previous_dodge_down[player_index] =
        previous_dodge_down;
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
    scratch->attack_stale_registered[player_index] =
        world->attack_stale_registered[player_index];
    scratch->falcon_kick_hit_count[player_index] =
        world->falcon_kick_hit_count[player_index];
    scratch->last_hit_attacker[player_index] =
        world->last_hit_attacker[player_index];
    scratch->shield_held[player_index] =
        world->shield_held[player_index];
    scratch->trigger_input_age[player_index] =
        world->trigger_input_age[player_index];
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
    uint8_t *previous_dodge_down)
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
    *previous_dodge_down = UINT8_C(0);
    scratch->damage_q16[player_index] = UINT32_C(0);
    scratch->pending_velocity_x_q16[player_index] = INT32_C(0);
    scratch->pending_velocity_y_q16[player_index] = INT32_C(0);
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
    scratch->last_hit_attacker[player_index] = UINT8_C(0);
    scratch->shield_held[player_index] = UINT8_C(0);
    scratch->trigger_input_age[player_index] = UINT8_MAX;
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
           action_state == (uint8_t)PF_M4_ACTION_SHIELD_RELEASE;
}

pf_status pf_m4_step_player(
    const pf_m4_content *content,
    const pf_world_state *world,
    pf_sim_scratch *scratch,
    const pf_input_frame *input,
    uint32_t player_index,
    int32_t player_nudge_x_q16)
{
    const pf_m4_fighter_data *fighter = &content->fighter;
    const pf_m4_stage_data *stage = &content->stage;
    const uint64_t previous_buttons =
        world->previous_buttons[player_index];
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
    const int strong_attack_pressed =
        (input->buttons & PF_INPUT_BUTTON_STRONG_ATTACK) !=
            UINT64_C(0) &&
        (previous_buttons & PF_INPUT_BUTTON_STRONG_ATTACK) ==
            UINT64_C(0);
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
    const int grab_blocks_attack =
        grab_pressed != 0 &&
        pf_m4_action_can_start_grab(
            world->action_state[player_index]);
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
    const int jab_combo_window =
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
    const int8_t strong_direction =
        pf_m4_strong_direction(
            input->main_stick_x,
            fighter->dash_axis_threshold);
    const uint16_t secondary_horizontal_magnitude =
        pf_m4_axis_magnitude(input->secondary_stick_x);
    const uint16_t secondary_vertical_magnitude =
        pf_m4_axis_magnitude(input->secondary_stick_y);
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
    const int8_t secondary_strong_direction =
        pf_m4_strong_direction(
            input->secondary_stick_x,
            fighter->dash_axis_threshold);
    const int forward_smash_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        strong_direction != INT8_C(0) &&
        horizontal_magnitude >= vertical_magnitude &&
        (((world->action_state[player_index] ==
                   (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
           world->action_state[player_index] ==
                   (uint8_t)PF_M4_ACTION_WALK) &&
           world->previous_strong_direction[player_index] ==
               INT8_C(0)) ||
         (world->action_state[player_index] ==
              (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          ((strong_direction ==
                world->dash_direction[player_index] &&
            input_tilt_x_age <=
                fighter->forward_smash_input_window_ticks) ||
           (strong_direction ==
                -world->dash_direction[player_index] &&
           world->action_ticks[player_index] == UINT16_C(1)))));
    const int vertical_smash_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        (world->action_state[player_index] ==
             (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
         world->action_state[player_index] ==
             (uint8_t)PF_M4_ACTION_WALK) &&
        vertical_magnitude >= fighter->dash_axis_threshold &&
        vertical_magnitude > horizontal_magnitude;
    const int ground_smash_charge_pressed =
        forward_smash_pressed != 0 || vertical_smash_pressed != 0;
    const uint8_t ground_smash_charge_action =
        forward_smash_pressed != 0
            ? (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE
            : (input->main_stick_y < INT16_C(0)
                   ? (uint8_t)PF_M4_ACTION_UP_STRONG_CHARGE
                   : (uint8_t)PF_M4_ACTION_DOWN_STRONG_CHARGE);
    const int ground_strong_attack_pressed =
        grab_blocks_attack == 0 && strong_attack_pressed != 0;
    const uint8_t ground_light_attack_action =
        pf_m4_select_ground_light_attack_action(
            fighter,
            world->facing[player_index],
            input->main_stick_x,
            input->main_stick_y);
    const uint8_t ground_strong_attack_action =
        pf_m4_select_ground_strong_attack_action(
            fighter,
            strong_attack_stick_x,
            strong_attack_stick_y);
    const int dash_attack_pressed =
        grab_blocks_attack == 0 && light_attack_pressed != 0 &&
        ground_smash_charge_pressed == 0 &&
        pf_m4_action_can_start_dash_attack(
            fighter,
            world->action_state[player_index],
            world->action_ticks[player_index]);
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
    const int throw_pressed =
        light_attack_pressed != 0 || strong_attack_pressed != 0;
    const int dodge_down_held =
        input->main_stick_y >=
        (int16_t)fighter->crouch_axis_threshold;
    const int secondary_dodge_down_held =
        input->secondary_stick_y >=
        (int16_t)fighter->crouch_axis_threshold;
    const int secondary_jump_up_buffered =
        input->secondary_stick_y <=
        -(int16_t)fighter->dash_axis_threshold;
    const int secondary_jump_up_held =
        input->secondary_stick_y <=
        -(int16_t)fighter->crouch_axis_threshold;
    const int main_stick_spot_dodge_pressed =
        shield_held != 0 &&
        dodge_down_held != 0 &&
        world->previous_dodge_down[player_index] == UINT8_C(0);
    const int secondary_stick_spot_dodge_buffered =
        shield_held != 0 &&
        secondary_dodge_down_held != 0 &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD;
    const int spot_dodge_pressed =
        main_stick_spot_dodge_pressed != 0 ||
        secondary_stick_spot_dodge_buffered != 0;
    const int main_stick_roll_pressed =
        shield_held != 0 &&
        strong_direction != INT8_C(0) &&
        world->previous_strong_direction[player_index] == INT8_C(0);
    const int secondary_stick_roll_buffered =
        shield_held != 0 &&
        secondary_strong_direction != INT8_C(0) &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD;
    const int roll_pressed =
        main_stick_roll_pressed != 0 ||
        secondary_stick_roll_buffered != 0;
    const int8_t roll_direction =
        main_stick_roll_pressed != 0
            ? strong_direction
            : secondary_strong_direction;
    const int shield_jump_pressed =
        jump_pressed != 0 ||
        (shield_held != 0 &&
         secondary_jump_up_buffered != 0 &&
         world->action_state[player_index] ==
             (uint8_t)PF_M4_ACTION_SHIELD);
    const int shield_release_spot_dodge_pressed =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        ((dodge_down_held != 0 &&
          world->previous_dodge_down[player_index] == UINT8_C(0)) ||
         secondary_dodge_down_held != 0);
    const int shield_release_jump_pressed =
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
        (jump_pressed != 0 || secondary_jump_up_buffered != 0);
    const int shield_platform_drop_requested =
        shield_held != 0 &&
        world->grounded[player_index] != UINT8_C(0) &&
        pf_m4_surface_is_pass_through(
            world->support[player_index]) != 0 &&
        world->action_state[player_index] ==
            (uint8_t)PF_M4_ACTION_SHIELD &&
        input->main_stick_y >=
            (int16_t)fighter->shield_drop_axis_threshold &&
        input->main_stick_y <
            (int16_t)fighter->crouch_axis_threshold;
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
        pf_m4_falcon_ground_iasa_active(
            ground_reference_attack,
            world->action_ticks[player_index]);
    const uint8_t ground_iasa_capabilities =
        ground_attack_iasa != 0
            ? pf_m4_falcon_ground_iasa_capabilities(
                  ground_iasa_policy)
            : UINT8_C(0);
    const int ground_common_iasa_input =
        jump_pressed != 0 ||
        horizontal_magnitude > fighter->axis_dead_zone ||
        input->main_stick_y >=
            (int16_t)fighter->crouch_axis_threshold;
    const int ground_common_iasa_unclaimed =
        attack_pressed == 0 && grab_pressed == 0 &&
        shield_held == 0 && special_pressed == 0 &&
        taunt_pressed == 0;
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
    uint8_t recovery_available =
        world->recovery_available[player_index];
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
    uint8_t previous_dodge_down =
        dodge_down_held != 0 ? UINT8_C(1) : UINT8_C(0);
    int8_t tilt_x_direction = input_tilt_x_direction;
    int8_t tilt_y_direction = input_tilt_y_direction;
    uint8_t tilt_x_age = input_tilt_x_age;
    uint8_t tilt_y_age = input_tilt_y_age;
    int launched_this_tick = 0;
    int dropped_platform_this_tick = 0;
    int ledge_motion_handled = 0;
    int released_ledge_this_tick = 0;
    int shield_reset_this_tick = 0;
    int initial_dash_entered_this_tick = 0;
    int resumed_hitlag_motion_this_tick = 0;
    int32_t initial_dash_entry_motion_velocity_x = velocity_x;
    int32_t animation_motion_x_q16 = INT32_C(0);
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
                    &previous_dodge_down);
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
            previous_dodge_down,
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
        const int input_drop =
            descent_complete != 0 &&
            (horizontal_magnitude > fighter->axis_dead_zone ||
             vertical_magnitude > fighter->axis_dead_zone ||
             (input->buttons &
              (PF_INPUT_KNOWN_BUTTONS & ~PF_INPUT_BUTTON_FORFEIT)) !=
                 UINT64_C(0) ||
             shield_held != 0);
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
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            position_y = stage->revival_platform_end_y_q16 -
                         fighter->half_height_q16;
            scratch->respawn_invulnerability_ticks[player_index] =
                world->respawn_invulnerability_config_ticks;
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
            previous_dodge_down,
            tilt_x_direction,
            tilt_y_direction,
            tilt_x_age,
            tilt_y_age);
        return PF_STATUS_OK;
    }
    if (shield_held != 0 &&
        !pf_m4_action_freezes_shield_strength(
            action_state,
            scratch->hitlag_resume_action[player_index]))
    {
        scratch->shield_strength[player_index] =
            input_shield_strength;
    }
    if (scratch->respawn_invulnerability_ticks[player_index] >
        UINT16_C(0))
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
                    fighter,
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
            (scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_HITSTUN ||
            scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_RESET_BOUND ||
            scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN))
        {
            const int shield_sdi =
                scratch->hitlag_resume_action[player_index] ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN;
            const int8_t sdi_x =
                pf_m4_sdi_direction(
                    input->main_stick_x,
                    fighter->sdi_axis_threshold);
            const int8_t sdi_y =
                shield_sdi != 0
                    ? INT8_C(0)
                    : pf_m4_sdi_direction(
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
                    shield_sdi != 0
                        ? INT16_C(0)
                        : input->main_stick_y,
                    shield_sdi != 0
                        ? pf_m4_multiply_q16(
                              fighter->sdi_distance_q16,
                              fighter->shield_sdi_scale_q16)
                        : fighter->sdi_distance_q16,
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
            if (action_state == (uint8_t)PF_M4_ACTION_HITSTUN ||
                action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
            {
                status = pf_m4_apply_hitlag_shift(
                    content,
                    world,
                    scratch,
                    player_index,
                    input->main_stick_x,
                    input->main_stick_y,
                    fighter->asdi_distance_q16,
                    0);
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
            else if (
                action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_STUN)
            {
                status = pf_m4_apply_hitlag_shift(
                    content,
                    world,
                    scratch,
                    player_index,
                    input->main_stick_x,
                    INT16_C(0),
                    pf_m4_multiply_q16(
                        fighter->asdi_distance_q16,
                        fighter->shield_sdi_scale_q16),
                    1);
                if (status != PF_STATUS_OK)
                {
                    return status;
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
                previous_dodge_down,
                tilt_x_direction,
                tilt_y_direction,
                tilt_x_age,
                tilt_y_age);
            return PF_STATUS_OK;
        }
    }

    hitstun_locked =
        action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND ||
        ((action_state == (uint8_t)PF_M4_ACTION_HITSTUN ||
          pf_m4_action_is_surface_bounce(action_state)) &&
         scratch->hitstun_ticks[player_index] > UINT16_C(0));

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
        (ground_iasa_capabilities &
         PF_M4_FALCON_IASA_COMMON_MOVEMENT) != UINT8_C(0) &&
        ground_common_iasa_input != 0 &&
        ground_common_iasa_unclaimed != 0 &&
        pf_m4_action_is_ground_attack(action_state))
    {
        if (jump_pressed == 0 &&
            input->main_stick_y <
                (int16_t)fighter->crouch_axis_threshold &&
            horizontal_magnitude > fighter->axis_dead_zone &&
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
        !hitstun_locked &&
        !pf_m4_action_is_reference_special_locked(action_state) &&
        action_state != (uint8_t)PF_M4_ACTION_WALL_JUMP &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
        (!pf_m4_action_is_ground_attack(action_state) ||
         ground_reference_attack == NULL ||
         (ground_iasa_capabilities &
          PF_M4_FALCON_IASA_SPECIAL) != UINT8_C(0)) &&
        special_pressed != 0)
    {
        const int up_special_requested =
            input->main_stick_y <=
                -(int16_t)fighter->dash_axis_threshold;
        const int charge_requested =
            content->charge.enabled != UINT8_C(0) &&
            grounded != UINT8_C(0) &&
            up_special_requested != 0 &&
            light_attack_held != 0;
        const int vector_ascent_requested =
            up_special_requested != 0 && charge_requested == 0;
        const int falcon_down_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;
        const int reflector_requested =
            fighter->reference_frame_data_enabled == UINT8_C(0) &&
            content->reflector.enabled != UINT8_C(0) &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold;
        const pf_m4_falcon_common_special_attributes *
            common_special_attributes =
                pf_m4_falcon_reference_common_special_attributes();
        const int32_t special_stick_x_q16 =
            pf_m4_axis_q16(input->main_stick_x);
        const int falcon_side_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            common_special_attributes != NULL &&
            up_special_requested == 0 &&
            falcon_down_special_requested == 0 &&
            reflector_requested == 0 &&
            (special_stick_x_q16 >=
                 common_special_attributes
                     ->side_special_stick_threshold_q16 ||
             special_stick_x_q16 <=
                 -common_special_attributes
                      ->side_special_stick_threshold_q16);
        const int falcon_neutral_special_requested =
            fighter->reference_frame_data_enabled != UINT8_C(0) &&
            up_special_requested == 0 &&
            falcon_down_special_requested == 0 &&
            reflector_requested == 0 &&
            falcon_side_special_requested == 0 &&
            horizontal_magnitude <= fighter->axis_dead_zone &&
            vertical_magnitude <= fighter->axis_dead_zone;
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
        (horizontal_magnitude > fighter->axis_dead_zone ||
         jump_pressed != 0 || attack_pressed != 0 ||
         grab_pressed != 0 || shield_held != 0 ||
         special_pressed != 0 || taunt_pressed != 0))
    {
        action_state = (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        action_ticks = UINT16_C(0);
    }

    if (!ledge_motion_handled &&
        !hitstun_locked &&
        grounded != UINT8_C(0) &&
        ((grab_pressed != 0 &&
          spot_dodge_pressed == 0 &&
          roll_pressed == 0 &&
          (pf_m4_action_can_start_grab(action_state) ||
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
        (action_state == (uint8_t)PF_M4_ACTION_SHIELD ||
         (!pf_m4_action_is_shield(action_state) &&
          action_state != (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
          (!pf_m4_action_is_ground_attack(action_state) ||
           (ground_iasa_capabilities &
            PF_M4_FALCON_IASA_ESCAPE) != UINT8_C(0)) &&
          action_state != (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
          pf_m4_normal_landing_is_interruptible(
              fighter,
              action_state,
              action_ticks) &&
          action_state !=
              (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
          action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
          !pf_m4_action_is_aerial_landing(action_state) &&
          !pf_m4_action_locks_ground_control(action_state))))
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
        !pf_m4_action_is_shield(action_state) &&
        action_state != (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
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
        !pf_m4_action_is_aerial_landing(action_state) &&
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
        pf_m4_normal_landing_is_interruptible(
            fighter,
            action_state,
            action_ticks) &&
        action_state != (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
        action_state != (uint8_t)PF_M4_ACTION_RUN_BRAKE &&
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
        attack_pressed)
    {
        action_state =
            dash_attack_pressed != 0
                ? (uint8_t)PF_M4_ACTION_DASH_ATTACK
                : (ground_smash_charge_pressed != 0
                       ? ground_smash_charge_action
                       : (ground_strong_attack_pressed != 0
                              ? ground_strong_attack_action
                              : ground_light_attack_action));
        action_ticks = UINT16_C(0);
        scratch->attack_hit_mask[player_index] = UINT8_C(0);
        scratch->attack_stale_registered[player_index] = UINT8_C(0);
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
        short_hop_latched = UINT8_C(0);
        dash_direction = INT8_C(0);
        if (dash_attack_pressed != 0)
        {
            velocity_x =
                (int32_t)facing * fighter->dash_attack_speed_q16;
        }
        if ((ground_strong_attack_pressed != 0
                 ? strong_attack_horizontal_direction
                 : horizontal_direction) != INT8_C(0) &&
            (action_state ==
                 (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
             action_state ==
                 (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
             action_state ==
                 (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE))
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
        jump_pressed)
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
        if (scratch->shield_health_q16[player_index] ==
            UINT32_C(0))
        {
            pf_m4_enter_shield_break_launch(
                fighter,
                scratch,
                player_index,
                &velocity_x,
                &velocity_y,
                &action_ticks,
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
                (action_state ==
                     (uint8_t)PF_M4_ACTION_FORWARD_ATTACK ||
                 action_state ==
                     (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK))
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
        velocity_x = INT32_C(0);
        ++action_ticks;
        if (action_ticks >= fighter->shield_break_down_ticks)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND;
            action_ticks = UINT16_C(0);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
    {
        velocity_x = INT32_C(0);
        ++action_ticks;
        if (action_ticks >= fighter->shield_break_stand_ticks)
        {
            action_state =
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN;
            action_ticks = pf_m4_shield_break_stun_ticks(
                fighter,
                scratch->damage_q16[player_index]);
        }
    }
    else if (!ledge_motion_handled &&
        grounded != UINT8_C(0) &&
        action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
    {
        uint32_t mash_pulses = UINT32_C(0);
        uint32_t elapsed_ticks = UINT32_C(1);

        velocity_x = INT32_C(0);
        if (jump_pressed)
        {
            ++mash_pulses;
        }
        if (light_attack_pressed)
        {
            ++mash_pulses;
        }
        if (strong_attack_pressed)
        {
            ++mash_pulses;
        }
        if (shield_pressed)
        {
            ++mash_pulses;
        }
        if (strong_direction != INT8_C(0) &&
            world->previous_strong_direction[player_index] ==
                INT8_C(0))
        {
            ++mash_pulses;
        }
        if (dodge_down_held != 0 &&
            world->previous_dodge_down[player_index] ==
                UINT8_C(0))
        {
            ++mash_pulses;
        }
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
            shield_reset_this_tick = 1;
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
                throw_pressed != 0
                    ? pf_m4_grab_action_for_input(
                          fighter,
                          input,
                          facing)
                    : (uint8_t)PF_M4_ACTION_GRAB_HOLD;

            if (grab_action != (uint8_t)PF_M4_ACTION_GRAB_HOLD)
            {
                action_state = grab_action;
                action_ticks = UINT16_C(0);
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
            if (action_ticks >= fighter->pummel_total_ticks)
            {
                action_state = (uint8_t)PF_M4_ACTION_GRAB_HOLD;
                action_ticks = UINT16_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRABBED)
        {
            uint32_t elapsed_ticks = UINT32_C(1);
            uint32_t mash_pulses = UINT32_C(0);

            if (jump_pressed)
            {
                ++mash_pulses;
            }
            if (light_attack_pressed)
            {
                ++mash_pulses;
            }
            if (strong_attack_pressed)
            {
                ++mash_pulses;
            }
            if (shield_pressed)
            {
                ++mash_pulses;
            }
            if (strong_direction != INT8_C(0) &&
                world->previous_strong_direction[player_index] ==
                    INT8_C(0))
            {
                ++mash_pulses;
            }
            if (dodge_down_held != 0 &&
                world->previous_dodge_down[player_index] ==
                    UINT8_C(0))
            {
                ++mash_pulses;
            }
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
            if (action_ticks < UINT16_C(600))
            {
                ++action_ticks;
            }
        }
        else if (action_state == (uint8_t)PF_M4_ACTION_GRAB_RELEASE)
        {
            ++action_ticks;
            if (action_ticks >= fighter->grab_release_ticks)
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
        grounded != UINT8_C(0) &&
        pf_m4_action_locks_ground_control(action_state) &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_MISS &&
        action_state !=
            (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_LANDING_HIT &&
        action_state !=
            (uint8_t)PF_M4_ACTION_FALCON_DIVE_LANDING)
    {
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
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                scratch->tech_direction[player_index] =
                    INT8_C(0);
            }
            else if (horizontal_direction != INT8_C(0))
            {
                const pf_m4_getup_roll_timing *timing;

                action_state =
                    (uint8_t)PF_M4_ACTION_GETUP_ROLL;
                action_ticks = UINT16_C(0);
                scratch->tech_direction[player_index] =
                    horizontal_direction;
                timing = pf_m4_getup_roll_timing_for(
                    fighter,
                    scratch->prone_orientation[player_index],
                    horizontal_direction,
                    facing);
                velocity_x = timing != NULL &&
                                     timing->movement_begin_tick ==
                                         UINT16_C(1)
                                 ? (int32_t)horizontal_direction *
                                       fighter->getup_roll_speed_q16
                                 : INT32_C(0);
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
            const pf_m4_getup_roll_timing *timing =
                pf_m4_getup_roll_timing_for(
                    fighter,
                    scratch->prone_orientation[player_index],
                    scratch->tech_direction[player_index],
                    facing);

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
            }
            else if (timing != NULL &&
                     (uint16_t)(action_ticks + UINT16_C(1)) >=
                         timing->movement_begin_tick)
            {
                velocity_x =
                    (int32_t)scratch->tech_direction[player_index] *
                    fighter->getup_roll_speed_q16;
            }
            else
            {
                velocity_x = INT32_C(0);
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

        if (pf_m4_action_is_smash_charge(action_state))
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
                action_ticks =
                    scratch->smash_charge_ticks[player_index];
            }
            if (light_attack_held == 0 ||
                scratch->smash_charge_ticks[player_index] >=
                    fighter->smash_charge_max_ticks)
            {
                const pf_m4_attack_data *attack;

                action_state = pf_m4_smash_release_action(action_state);
                action_ticks = UINT16_C(0);
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
        const int32_t root_motion_q16 = INT32_C(2051);

        animation_motion_x_q16 =
            (int32_t)dash_direction *
            (action_ticks < UINT16_C(5)
                 ? -root_motion_q16
                 : root_motion_q16);
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
            dash_direction = (int8_t)(dash_direction * INT8_C(2));
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
        if (pf_m4_surface_is_pass_through(support) != 0 &&
            input->main_stick_y >=
                (int16_t)fighter->crouch_axis_threshold &&
            action_ticks >= fighter->platform_drop_startup_ticks)
        {
            grounded = UINT8_C(0);
            support = (uint8_t)PF_M4_SURFACE_NONE;
            action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
            action_ticks = UINT16_C(0);
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
        if (horizontal_direction == facing &&
            horizontal_magnitude > fighter->axis_dead_zone)
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
             input->main_stick_y >=
                 (int16_t)fighter->crouch_axis_threshold &&
             !pf_m4_is_moonwalk_lower_sweep(
                 fighter,
                 action_state,
                 input->main_stick_y))
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
        const int moonwalk_lower_sweep =
            pf_m4_is_moonwalk_lower_sweep(
                fighter,
                action_state,
                input->main_stick_y);
        const int moonwalk_lower_back =
            pf_m4_is_moonwalk_lower_back(
                fighter,
                action_state,
                facing,
                input->main_stick_x,
                input->main_stick_y);
        const int moonwalk_reduced_back =
            horizontal_direction == -facing &&
            horizontal_magnitude > fighter->axis_dead_zone &&
            strong_direction == INT8_C(0);
        const int moonwalk_setup_back =
            moonwalk_reduced_back != 0 ||
            moonwalk_lower_back != 0;
        const int moonwalk_full_back =
            strong_direction == -facing &&
            moonwalk_lower_back == 0;

        if (action_state == (uint8_t)PF_M4_ACTION_TEETER)
        {
            if (horizontal_magnitude <= fighter->axis_dead_zone)
            {
                velocity_x = INT32_C(0);
                ++action_ticks;
                if (action_ticks >= fighter->teeter_ticks)
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                    action_ticks = UINT16_C(0);
                }
            }
            else if (strong_direction != INT8_C(0))
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_INITIAL_DASH;
                action_ticks = UINT16_C(1);
                dash_direction = strong_direction;
                facing = strong_direction;
                initial_dash_entered_this_tick = 1;
                initial_dash_entry_motion_velocity_x = velocity_x;
                velocity_x = pf_m4_enter_initial_dash_velocity(
                    fighter,
                    velocity_x,
                    strong_direction);
            }
            else
            {
                action_state = (uint8_t)PF_M4_ACTION_WALK;
                action_ticks = UINT16_C(1);
                dash_direction = INT8_C(0);
                facing = horizontal_direction;
                velocity_x = pf_m4_apply_ground_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->walk_speed_q16,
                    1);
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
                    if (horizontal_direction == facing &&
                        horizontal_magnitude > fighter->axis_dead_zone)
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
                strong_direction == -dash_direction)
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
                        horizontal_magnitude >
                                fighter->axis_dead_zone
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
                horizontal_direction != facing;
            const int aged_walk_continues =
                action_state == (uint8_t)PF_M4_ACTION_WALK &&
                action_ticks >= fighter->dash_input_window_ticks &&
                walk_direction_changed == 0;
            const int moonwalk_setup_started = 0;
            const int dash_started =
                fresh_dash_input != 0;

            if (moonwalk_setup_started)
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
                     horizontal_direction == -facing &&
                     horizontal_magnitude > fighter->axis_dead_zone)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_STANDING_TURN;
                action_ticks = UINT16_C(1);
                dash_direction =
                    (int8_t)(INT8_C(2) * horizontal_direction);
                velocity_x = pf_m4_approach(
                    velocity_x,
                    INT32_C(0),
                    velocity_x > fighter->walk_speed_q16 ||
                            velocity_x < -fighter->walk_speed_q16
                        ? fighter->turn_acceleration_q16
                        : fighter->traction_q16);
            }
            else if (horizontal_magnitude >
                     fighter->axis_dead_zone)
            {
                int walk;

                facing = horizontal_direction;
                dash_direction = INT8_C(0);
                if (strong_direction != INT8_C(0) &&
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
            if (action_state == (uint8_t)PF_M4_ACTION_RESET_BOUND)
            {
                ++action_ticks;
            }
            else
            {
                action_state = (uint8_t)PF_M4_ACTION_HITSTUN;
            }
        }
        else if (
            action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            velocity_x = INT32_C(0);
            action_ticks = UINT16_C(0);
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
                action_ticks =
                    action_ticks + UINT16_C(1) <
                            PF_M4_FALCON_FALL_SPECIAL_ECB_FRAME_COUNT
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
            if (strong_attack_pressed != 0)
            {
                action_state =
                    (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK;
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
            else if (light_attack_pressed != 0)
            {
                action_state = pf_m4_select_light_aerial_action(
                    fighter,
                    input->main_stick_x,
                    input->main_stick_y,
                    facing);
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] = UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
            }
            else if (jump_pressed != 0 &&
                     air_jumps_remaining > UINT8_C(0))
            {
                velocity_x = pf_m4_scale_axis_q16(
                    input->main_stick_x,
                    fighter->double_jump_horizontal_speed_q16);
                velocity_y = -fighter->double_jump_speed_q16;
                --air_jumps_remaining;
                fast_fall = UINT8_C(0);
                if (fighter->double_jump_cancel_ticks > UINT16_C(0))
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP;
                    action_ticks = UINT16_C(0);
                }
                else
                {
                    action_state = (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                }
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
                    launched_this_tick = 0;
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
            else if (strong_attack_pressed &&
                     scratch->tumble[player_index] == UINT8_C(0))
            {
                if (double_jump_cancel_window != 0)
                {
                    velocity_y = INT32_C(0);
                }
                action_state =
                    (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK;
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] =
                    UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
            }
            else if (light_attack_pressed &&
                     scratch->tumble[player_index] == UINT8_C(0))
            {
                if (double_jump_cancel_window != 0)
                {
                    velocity_y = INT32_C(0);
                }
                action_state = pf_m4_select_light_aerial_action(
                    fighter,
                    input->main_stick_x,
                    input->main_stick_y,
                    facing);
                action_ticks = UINT16_C(0);
                scratch->attack_hit_mask[player_index] =
                    UINT8_C(0);
                scratch->attack_stale_registered[player_index] =
                    UINT8_C(0);
                velocity_x = pf_m4_apply_air_input(
                    fighter,
                    velocity_x,
                    input->main_stick_x,
                    fighter->air_speed_q16);
            }
            else if (
                dense_shield_pressed != 0 &&
                input_shield_strength >=
                    fighter->digital_trigger_threshold &&
                scratch->tumble[player_index] == UINT8_C(0))
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
                        action_ticks = UINT16_C(0);
                    }
                }
                else
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_AIRBORNE;
                    action_ticks = UINT16_C(0);
                }
            }

            if (action_state ==
                    (uint8_t)PF_M4_ACTION_AIRBORNE &&
                !launched_this_tick &&
                jump_pressed &&
                air_jumps_remaining > UINT8_C(0))
            {
                velocity_x = pf_m4_scale_axis_q16(
                    input->main_stick_x,
                    fighter->double_jump_horizontal_speed_q16);
                velocity_y = -fighter->double_jump_speed_q16;
                --air_jumps_remaining;
                fast_fall = UINT8_C(0);
                scratch->tumble[player_index] = UINT8_C(0);
                if (fighter->double_jump_cancel_ticks > UINT16_C(0))
                {
                    action_state =
                        (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP;
                    action_ticks = UINT16_C(0);
                }
            }
        }
    }

    if (action_state != (uint8_t)PF_M4_ACTION_SHIELD &&
        action_state != (uint8_t)PF_M4_ACTION_SHIELD_STUN &&
        !pf_m4_action_is_shield_break(action_state) &&
        shield_reset_this_tick == 0)
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

    previous_position_x = position_x;
    next_position =
        (int64_t)position_x +
        (int64_t)player_nudge_x_q16 +
        (int64_t)(initial_dash_entered_this_tick != 0
                      ? initial_dash_entry_motion_velocity_x
                      : velocity_x) +
        (int64_t)shield_recoil_x +
        (int64_t)animation_motion_x_q16;
    if (!ledge_motion_handled &&
        !pf_m4_checked_i32(next_position, &position_x))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    if (!ledge_motion_handled)
    {
        const int64_t future_y =
            (int64_t)position_y + (int64_t)velocity_y;
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
        grounded != UINT8_C(0))
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
            dash_direction = INT8_C(0);
        }
        else if (retains_surface == 0)
        {
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
                    : (uint8_t)PF_M4_ACTION_AIRBORNE;
            if (falcon_punch_fall == 0 &&
                falcon_kick_start_fall == 0 &&
                falcon_kick_ground_origin_end_fall == 0)
            {
                action_ticks = UINT16_C(0);
            }
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
        grounded == UINT8_C(0))
    {
        const int32_t previous_bottom =
            position_y + fighter->half_height_q16;
        const int32_t floor_contact_bottom_extent_q16 =
            pf_m4_floor_contact_bottom_extent_q16(
                fighter,
                action_state,
                action_ticks);
        const int32_t previous_floor_contact =
            position_y + floor_contact_bottom_extent_q16;
        const int32_t previous_top =
            position_y - fighter->half_height_q16;
        const int wall_tech_stalled =
            pf_m4_action_is_wall_tech(action_state) &&
            action_ticks < fighter->wall_tech_stall_ticks;
        int32_t new_bottom;
        int32_t new_floor_contact;
        int32_t new_top;

        if (wall_tech_stalled)
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
            (int64_t)position_y +
            (wall_tech_stalled ? INT64_C(0) : (int64_t)velocity_y);
        if (!pf_m4_checked_i32(next_position, &position_y))
        {
            return PF_STATUS_DETERMINISTIC_FAULT;
        }
        new_bottom = position_y + fighter->half_height_q16;
        new_floor_contact =
            position_y + floor_contact_bottom_extent_q16;
        new_top = position_y - fighter->half_height_q16;

        if (velocity_y >= INT32_C(0) &&
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
                ((action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
                  (previous_bottom > stage->platform_y_q16 ||
                   (fast_fall != UINT8_C(0) &&
                    previous_bottom <= stage->platform_y_q16 &&
                    new_bottom >= stage->platform_y_q16))) ||
                 (action_state != (uint8_t)PF_M4_ACTION_AIRBORNE &&
                  previous_bottom <= stage->platform_y_q16 &&
                  new_bottom >= stage->platform_y_q16)) &&
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
                ((action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
                  (previous_bottom > stage->upper_platform_y_q16 ||
                   (fast_fall != UINT8_C(0) &&
                    previous_bottom <= stage->upper_platform_y_q16 &&
                    new_bottom >= stage->upper_platform_y_q16))) ||
                 (action_state != (uint8_t)PF_M4_ACTION_AIRBORNE &&
                  previous_bottom <= stage->upper_platform_y_q16 &&
                  new_bottom >= stage->upper_platform_y_q16)) &&
                stage->upper_platform_y_q16 < landing_y_q16)
            {
                landing_y_q16 = stage->upper_platform_y_q16;
                landing_support =
                    (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
            }
            if (position_x >= stage->floor_left_q16 &&
                position_x <= stage->floor_right_q16 &&
                ((action_state == (uint8_t)PF_M4_ACTION_AIRBORNE &&
                  (previous_floor_contact > stage->floor_y_q16 ||
                   (previous_floor_contact <= stage->floor_y_q16 &&
                    new_floor_contact >= stage->floor_y_q16))) ||
                 (action_state != (uint8_t)PF_M4_ACTION_AIRBORNE &&
                  previous_floor_contact <= stage->floor_y_q16 &&
                  new_floor_contact >= stage->floor_y_q16)) &&
                stage->floor_y_q16 < landing_y_q16)
            {
                landing_y_q16 = stage->floor_y_q16;
                landing_support = (uint8_t)PF_M4_SURFACE_FLOOR;
            }
            if (landing_support != (uint8_t)PF_M4_SURFACE_NONE)
            {
                const int falcon_punch_landing =
                    action_state ==
                        (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR;
                const uint16_t falcon_punch_action_ticks =
                    action_ticks;

                pf_m4_land_from_air(
                    fighter,
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
        else if (
            pf_m4_body_overlaps_horizontal_interval(
                position_x,
                fighter->half_width_q16,
                stage->solid_left_q16,
                stage->solid_right_q16) &&
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
    else if (hitstun_locked)
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
        (action_state == (uint8_t)PF_M4_ACTION_AIRBORNE ||
         action_state ==
             (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
         action_state == (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
         action_state == (uint8_t)PF_M4_ACTION_VECTOR_ASCENT ||
         action_state ==
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_FALL ||
         action_state ==
             (uint8_t)PF_M4_ACTION_FALCON_DIVE_THROW ||
         ((action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_GROUND ||
           action_state ==
               (uint8_t)PF_M4_ACTION_FALCON_DIVE_START_AIR) &&
          action_ticks >=
              pf_m4_falcon_reference_up_special_timing()
                  ->air_control_begin_frame)) &&
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
                &scratch->ledge_invulnerability_ticks[player_index],
                scratch->ledge_regrab_lockout_ticks[player_index],
                action_state,
                action_ticks,
                previous_position_x,
                facing,
                &dash_direction))
        {
            scratch->tumble[player_index] = UINT8_C(0);
            scratch->tech_direction[player_index] = INT8_C(0);
            recovery_available = UINT8_C(1);
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
            &previous_dodge_down);
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

    if (scratch->smash_charge_ticks[player_index] != UINT16_C(0) &&
        !pf_m4_action_is_smash_charge(action_state) &&
        !pf_m4_action_is_smash_release(action_state) &&
        !(action_state == (uint8_t)PF_M4_ACTION_HITLAG &&
          pf_m4_action_is_smash_release(
              scratch->hitlag_resume_action[player_index])))
    {
        scratch->smash_charge_ticks[player_index] = UINT16_C(0);
    }

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
        recovery_available,
        short_hop_latched,
        platform_drop_ticks,
        fast_fall,
        facing,
            dash_direction,
            previous_strong_direction,
            previous_dodge_down,
            tilt_x_direction,
            tilt_y_direction,
            tilt_x_age,
            tilt_y_age);
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

        player->position_x_q16 =
            sim->world.position_x_q16[player_index];
        player->position_y_q16 =
            sim->world.position_y_q16[player_index];
        player->velocity_x_q16 =
            sim->world.velocity_x_q16[player_index];
        player->velocity_y_q16 =
            sim->world.velocity_y_q16[player_index];
        player->shield_recoil_x_q16 =
            sim->world.shield_recoil_x_q16[player_index];
        player->action_ticks =
            sim->world.action_ticks[player_index];
        player->respawn_count =
            sim->world.respawn_count[player_index];
        player->action_state =
            sim->world.action_state[player_index];
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
                    sim->world.tech_direction[player_index],
                    player->facing)
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
    }
    return PF_STATUS_OK;
}
