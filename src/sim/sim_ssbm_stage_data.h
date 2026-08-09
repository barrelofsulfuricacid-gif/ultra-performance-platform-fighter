#ifndef PF_SIM_SSBM_STAGE_DATA_H
#define PF_SIM_SSBM_STAGE_DATA_H

#include <stdint.h>

typedef enum pf_m4_ssbm_stage_surface_kind
{
    PF_M4_SSBM_STAGE_SURFACE_UNCLASSIFIED = 0,
    PF_M4_SSBM_STAGE_SURFACE_FLOOR = 1,
    PF_M4_SSBM_STAGE_SURFACE_CEILING = 2,
    PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL = 3,
    PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL = 4,
    PF_M4_SSBM_STAGE_SURFACE_DYNAMIC = 5
} pf_m4_ssbm_stage_surface_kind;

typedef struct pf_m4_ssbm_stage_collision_line
{
    int32_t start_x_q16;
    int32_t start_y_q16;
    int32_t end_x_q16;
    int32_t end_y_q16;
    int32_t ground_projection_x_q16;
    int32_t ground_projection_y_q16;
    int16_t previous_line;
    int16_t next_line;
    int16_t previous_alt_group_line;
    int16_t next_alt_group_line;
    uint32_t runtime_flags;
    uint16_t source_hi_flags;
    uint16_t property_material_flags;
    uint8_t kind;
    uint8_t source_index;
} pf_m4_ssbm_stage_collision_line;

typedef struct pf_m4_ssbm_stage_collision_profile
{
    const pf_m4_ssbm_stage_collision_line *lines;
    uint16_t line_count;
    uint16_t floor_start;
    uint16_t floor_count;
    uint16_t ceiling_start;
    uint16_t ceiling_count;
    uint16_t right_wall_start;
    uint16_t right_wall_count;
    uint16_t left_wall_start;
    uint16_t left_wall_count;
    uint16_t dynamic_start;
    uint16_t dynamic_count;
    const uint8_t *source_sha256;
} pf_m4_ssbm_stage_collision_profile;

const pf_m4_ssbm_stage_collision_profile *
pf_m4_ssbm_reference_stage_collision(uint16_t profile_id);

const pf_m4_ssbm_stage_collision_line *
pf_m4_ssbm_reference_stage_line(uint16_t profile_id, uint8_t support);

int32_t pf_m4_ssbm_stage_line_y_q16(
    const pf_m4_ssbm_stage_collision_line *line,
    int32_t position_x_q16);

int pf_m4_ssbm_stage_support_valid(
    uint16_t profile_id,
    uint8_t support,
    uint8_t grounded);

#endif
