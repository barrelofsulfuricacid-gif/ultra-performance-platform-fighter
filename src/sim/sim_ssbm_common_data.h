#ifndef PF_SIM_SSBM_COMMON_DATA_H
#define PF_SIM_SSBM_COMMON_DATA_H

#include <stdint.h>

#define PF_M4_SSBM_COMMON_RAW_WORD_COUNT UINT16_C(518)

typedef struct pf_m4_ssbm_damage_response_attributes
{
    int32_t hitstun_per_knockback_q16;
    int32_t launch_speed_x_per_knockback_q16;
    int32_t launch_speed_y_per_knockback_q16;
    int32_t sakurai_air_angle_degrees_q16;
    int32_t sakurai_max_ground_angle_degrees_q16;
    int32_t sakurai_low_knockback_q16;
    int32_t sakurai_high_knockback_q16;
    int32_t damage_level_1_threshold_q16;
    int32_t damage_level_2_threshold_q16;
    int32_t grounded_damage_max_level_q16;
    int32_t ground_knockback_max_speed_q16;
    int32_t di_max_angle_radians_q30;
    int32_t ground_knockback_decay_scale_q16;
    int32_t air_knockback_decay_q16;
    int32_t sdi_distance_x_q16;
    int32_t sdi_distance_y_q16;
    int32_t asdi_distance_x_q16;
    int32_t asdi_distance_y_q16;
    int32_t shield_sdi_scale_q16;
    uint16_t stick_tilt_threshold;
    uint16_t sdi_stick_threshold;
    uint16_t sdi_stick_window_ticks;
    uint16_t reserved;
} pf_m4_ssbm_damage_response_attributes;

typedef struct pf_m4_ssbm_surface_response_attributes
{
    int32_t collision_threshold_x_q16;
    int32_t collision_threshold_y_q16;
    int32_t bounce_multiplier_q16;
    uint16_t wall_tech_stall_ticks;
    uint16_t wall_tech_invulnerability_ticks;
    uint16_t bounce_invulnerability_ticks;
    uint16_t bounce_collision_lockout_ticks;
} pf_m4_ssbm_surface_response_attributes;

const uint8_t *pf_m4_ssbm_common_reference_source_sha256(void);

const uint32_t *pf_m4_ssbm_common_reference_raw_words(
    uint16_t *out_count);

const pf_m4_ssbm_damage_response_attributes *
pf_m4_ssbm_common_reference_damage_response(void);

const pf_m4_ssbm_surface_response_attributes *
pf_m4_ssbm_common_reference_surface_response(void);

#endif
