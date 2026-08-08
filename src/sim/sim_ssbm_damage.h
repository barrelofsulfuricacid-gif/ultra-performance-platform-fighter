#ifndef PF_SIM_SSBM_DAMAGE_H
#define PF_SIM_SSBM_DAMAGE_H

#include "pf/sim.h"

#include <stdint.h>

pf_status pf_m4_ssbm_apply_di_q16(
    int32_t max_angle_radians_q30,
    int16_t stick_x,
    int16_t stick_y,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16);

int pf_m4_ssbm_stick_meets_radial_threshold(
    int16_t stick_x,
    int16_t stick_y,
    uint16_t threshold);

int32_t pf_m4_ssbm_analog_displacement_q16(
    int16_t stick_axis,
    int32_t maximum_distance_q16);

pf_status pf_m4_ssbm_decay_air_knockback_q16(
    int32_t decay_q16,
    int32_t *velocity_x_q16,
    int32_t *velocity_y_q16);

#endif
