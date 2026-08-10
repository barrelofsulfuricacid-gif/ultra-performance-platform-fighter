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
    int32_t source_normal_x_q16;
    int32_t source_normal_y_q16;
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

typedef struct pf_m4_ssbm_stage_spawn_point
{
    int32_t position_x_q16;
    int32_t position_y_q16;
    uint8_t support;
    uint8_t source_index;
} pf_m4_ssbm_stage_spawn_point;

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
    uint16_t source_grkind;
    const pf_m4_ssbm_stage_spawn_point *spawn_points;
    uint8_t spawn_point_count;
    int32_t camera_left_q16;
    int32_t camera_right_q16;
    int32_t camera_top_q16;
    int32_t camera_bottom_q16;
    int32_t blast_left_q16;
    int32_t blast_right_q16;
    int32_t blast_top_q16;
    int32_t blast_bottom_q16;
    const uint8_t *source_sha256;
} pf_m4_ssbm_stage_collision_profile;

const pf_m4_ssbm_stage_collision_profile *
pf_m4_ssbm_reference_stage_collision(uint16_t profile_id);

const pf_m4_ssbm_stage_collision_line *
pf_m4_ssbm_reference_stage_line(uint16_t profile_id, uint8_t support);

const pf_m4_ssbm_stage_spawn_point *
pf_m4_ssbm_reference_stage_spawn_point(
    uint16_t profile_id,
    uint8_t player_index);

int32_t pf_m4_ssbm_stage_line_y_q16(
    const pf_m4_ssbm_stage_collision_line *line,
    int32_t position_x_q16);

int pf_m4_ssbm_reference_stage_find_ceiling_contact(
    uint16_t profile_id,
    int32_t position_x_q16,
    int64_t previous_top_q16,
    int64_t current_top_q16,
    int32_t *out_ceiling_y_q16,
    uint8_t *out_support);

int pf_m4_ssbm_reference_stage_find_wall_contact(
    uint16_t profile_id,
    int32_t previous_position_x_q16,
    int32_t current_position_x_q16,
    int64_t swept_body_top_q16,
    int64_t swept_body_bottom_q16,
    int32_t half_width_q16,
    int32_t *out_position_x_q16,
    uint8_t *out_support,
    int8_t *out_away_direction);

int pf_m4_ssbm_reference_stage_find_wall_point_contact(
    uint16_t profile_id,
    int32_t previous_point_x_q16,
    int32_t previous_point_y_q16,
    int32_t current_point_x_q16,
    int32_t current_point_y_q16,
    uint32_t *out_fraction_q16,
    uint8_t *out_support,
    int8_t *out_away_direction);

int pf_m4_ssbm_stage_support_valid(
    uint16_t profile_id,
    uint8_t support,
    uint8_t grounded);

uint8_t pf_m4_ssbm_reference_stage_ledge_support(
    uint16_t profile_id,
    int32_t ledge_x_q16);

#endif
