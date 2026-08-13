#ifndef PF_SIM_SSBM_STAGE_DATA_H
#define PF_SIM_SSBM_STAGE_DATA_H

#include <stdint.h>

typedef enum ssbm_stage_surface_kind
{
    PF_M4_SSBM_STAGE_SURFACE_UNCLASSIFIED = 0,
    PF_M4_SSBM_STAGE_SURFACE_FLOOR = 1,
    PF_M4_SSBM_STAGE_SURFACE_CEILING = 2,
    PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL = 3,
    PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL = 4,
    PF_M4_SSBM_STAGE_SURFACE_DYNAMIC = 5
} ssbm_stage_surface_kind;

typedef struct ssbm_stage_collision_line
{
    float start_x_f32;
    float start_y_f32;
    float end_x_f32;
    float end_y_f32;
    float ground_projection_x_f32;
    float ground_projection_y_f32;
    float source_normal_x_f32;
    float source_normal_y_f32;
    int16_t previous_line;
    int16_t next_line;
    int16_t previous_alt_group_line;
    int16_t next_alt_group_line;
    uint32_t runtime_flags;
    uint16_t source_hi_flags;
    uint16_t property_material_flags;
    uint8_t kind;
    uint8_t source_index;
} ssbm_stage_collision_line;

typedef struct ssbm_stage_spawn_point
{
    float position_x_f32;
    float position_y_f32;
    uint8_t support;
    uint8_t source_index;
} ssbm_stage_spawn_point;

typedef struct ssbm_stage_collision_profile
{
    const ssbm_stage_collision_line *lines;
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
    const ssbm_stage_spawn_point *spawn_points;
    uint8_t spawn_point_count;
    float camera_left_f32;
    float camera_right_f32;
    float camera_top_f32;
    float camera_bottom_f32;
    float blast_left_f32;
    float blast_right_f32;
    float blast_top_f32;
    float blast_bottom_f32;
    const uint8_t *source_sha256;
} ssbm_stage_collision_profile;

const ssbm_stage_collision_profile *
ssbm_reference_stage_collision(uint16_t profile_id);

const ssbm_stage_collision_line *
ssbm_reference_stage_line(uint16_t profile_id, uint8_t support);

const ssbm_stage_spawn_point *
ssbm_reference_stage_spawn_point(
    uint16_t profile_id,
    uint8_t player_index);

float ssbm_revival_platform_x_f32(
    uint16_t profile_id,
    uint8_t player_count,
    uint8_t player_index,
    float authored_spawn_spacing_f32);

float ssbm_stage_line_y_f32(
    const ssbm_stage_collision_line *line,
    float position_x_f32);

float ssbm_stage_line_x_f32(
    const ssbm_stage_collision_line *line,
    float position_y_f32);

int ssbm_reference_stage_find_ceiling_contact(
    uint16_t profile_id,
    float position_x_f32,
    float previous_top_f32,
    float current_top_f32,
    float *out_ceiling_y_f32,
    uint8_t *out_support);

int ssbm_reference_stage_find_wall_contact(
    uint16_t profile_id,
    float previous_position_x_f32,
    float current_position_x_f32,
    float swept_body_top_f32,
    float swept_body_bottom_f32,
    float half_width_f32,
    float *out_position_x_f32,
    uint8_t *out_support,
    int8_t *out_away_direction);

int ssbm_reference_stage_find_wall_point_contact(
    uint16_t profile_id,
    float previous_point_x_f32,
    float previous_point_y_f32,
    float current_point_x_f32,
    float current_point_y_f32,
    float *out_fraction_f32,
    uint8_t *out_support,
    int8_t *out_away_direction);

int ssbm_reference_stage_find_floor_point_contact(
    uint16_t profile_id,
    float previous_point_x_f32,
    float previous_point_y_f32,
    float current_point_x_f32,
    float current_point_y_f32,
    float *out_fraction_f32,
    float *out_floor_y_f32,
    uint8_t *out_support);

int ssbm_stage_support_valid(
    uint16_t profile_id,
    uint8_t support,
    uint8_t grounded);

uint8_t ssbm_reference_stage_ledge_support(
    uint16_t profile_id,
    float ledge_x_f32);

#endif
