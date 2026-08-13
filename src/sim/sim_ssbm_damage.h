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
    float resulting_damage_f32,
    float launch_velocity_x_f32,
    float launch_velocity_y_f32,
    uint64_t *rng_state,
    ssbm_damage_motion_kind *out_motion);

ssbm_damage_floor_response
ssbm_select_damage_floor_response_f32(
    float knockback_velocity_x_f32,
    float knockback_velocity_y_f32,
    uint8_t force_down_bound);

pf_status ssbm_resolve_ground_damage_launch_f32(
    float source_normal_x_f32,
    float source_normal_y_f32,
    float ground_projection_x_f32,
    float ground_projection_y_f32,
    uint8_t damage_level,
    float *velocity_x_f32,
    float *velocity_y_f32,
    float *ground_scalar_f32,
    uint8_t *launch_grounded);

pf_status ssbm_apply_di_f32(
    int32_t max_angle_radians_q30,
    int16_t stick_x,
    int16_t stick_y,
    float *velocity_x_f32,
    float *velocity_y_f32);

int ssbm_stick_meets_radial_threshold(
    int16_t stick_x,
    int16_t stick_y,
    uint16_t threshold);

float ssbm_analog_displacement_f32(
    int16_t stick_axis,
    float maximum_distance_f32);

pf_status ssbm_decay_air_knockback_f32(
    float decay_f32,
    float *velocity_x_f32,
    float *velocity_y_f32);

pf_status ssbm_mirror_velocity_f32(
    float source_normal_x_f32,
    float source_normal_y_f32,
    float multiplier_f32,
    float *velocity_x_f32,
    float *velocity_y_f32);

#endif
