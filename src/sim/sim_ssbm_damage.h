#ifndef PF_SIM_SSBM_DAMAGE_H
#define PF_SIM_SSBM_DAMAGE_H

#include "pf/sim.h"

#include <stdint.h>

typedef enum ssbm_damage_motion_kind
{
    PF_M4_SSBM_DAMAGE_MOTION_ORDINARY = 0,
    PF_M4_SSBM_DAMAGE_MOTION_FLY_TOP = 1,
    PF_M4_SSBM_DAMAGE_MOTION_FLY_ROLL = 2
} ssbm_damage_motion_kind;

typedef enum ssbm_damage_floor_response
{
    PF_M4_SSBM_DAMAGE_FLOOR_KEEP_ACTION = 0,
    PF_M4_SSBM_DAMAGE_FLOOR_LANDING = 1,
    PF_M4_SSBM_DAMAGE_FLOOR_DOWN_BOUND = 2
} ssbm_damage_floor_response;

pf_status ssbm_select_damage_motion(
    uint8_t launch_grounded,
    uint8_t damage_level,
    uint32_t resulting_damage_q16,
    int32_t launch_velocity_x_q16,
    int32_t launch_velocity_y_q16,
    uint64_t *rng_state,
    ssbm_damage_motion_kind *out_motion);

ssbm_damage_floor_response
ssbm_select_damage_floor_response_q16(
    int32_t knockback_velocity_x_q16,
    int32_t knockback_velocity_y_q16,
    uint8_t force_down_bound);

pf_status ssbm_resolve_ground_damage_launch_q16(
    int32_t source_normal_x_q16,
    int32_t source_normal_y_q16,
    int32_t ground_projection_x_q16,
    int32_t ground_projection_y_q16,
    uint8_t damage_level,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16,
    int32_t *ground_scalar_q16,
    uint8_t *launch_grounded);

pf_status ssbm_apply_di_q16(
    int32_t max_angle_radians_q30,
    int16_t stick_x,
    int16_t stick_y,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16);

int ssbm_stick_meets_radial_threshold(
    int16_t stick_x,
    int16_t stick_y,
    uint16_t threshold);

int32_t ssbm_analog_displacement_q16(
    int16_t stick_axis,
    int32_t maximum_distance_q16);

pf_status ssbm_decay_air_knockback_q16(
    int32_t decay_q16,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16);

pf_status ssbm_mirror_velocity_q16(
    int32_t source_normal_x_q16,
    int32_t source_normal_y_q16,
    int32_t multiplier_q16,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16);

#endif
