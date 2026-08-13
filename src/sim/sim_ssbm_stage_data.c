#include "sim_ssbm_stage_data.h"

#include <float.h>
#include <stddef.h>
#include <stdint.h>

#include "pf/m4.h"

#include "../../generated/data/ssbm_ntsc102_hyrule_collision.inc"
#include "../../generated/data/ssbm_ntsc102_battlefield_collision.inc"

_Static_assert(
    sizeof(ssbm_hyrule_collision_lines) /
            sizeof(ssbm_hyrule_collision_lines[0]) ==
        (size_t)91,
    "Hyrule Temple collision catalog must remain complete");
_Static_assert(
    sizeof(ssbm_battlefield_collision_lines) /
            sizeof(ssbm_battlefield_collision_lines[0]) ==
        (size_t)23,
    "Battlefield collision catalog must remain complete");

const ssbm_stage_collision_profile *ssbm_reference_stage_collision(
    uint16_t profile_id)
{
    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE)
    {
        return &ssbm_hyrule_collision_profile;
    }
    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_BATTLEFIELD)
    {
        return &ssbm_battlefield_collision_profile;
    }
    return NULL;
}

const ssbm_stage_collision_line *ssbm_reference_stage_line(
    uint16_t profile_id,
    uint8_t support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const uint16_t line_index = (uint16_t)support - UINT16_C(1);

    return profile == NULL || support == UINT8_C(0) ||
                   line_index >= profile->line_count
               ? NULL
               : &profile->lines[line_index];
}

const ssbm_stage_spawn_point *ssbm_reference_stage_spawn_point(
    uint16_t profile_id,
    uint8_t player_index)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);

    return profile == NULL || profile->spawn_points == NULL ||
                   player_index >= profile->spawn_point_count
               ? NULL
               : &profile->spawn_points[player_index];
}

float ssbm_revival_platform_x_f32(
    uint16_t profile_id,
    uint8_t player_count,
    uint8_t player_index,
    float authored_spawn_spacing_f32)
{
    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_BATTLEFIELD &&
        player_count == UINT8_C(2) && player_index < UINT8_C(2))
    {
        static const float battlefield_two_player_x_f32[2] = {
            12.8f * (12.0f / 115.0f),
            -40.0f * (12.0f / 115.0f)};

        return battlefield_two_player_x_f32[player_index];
    }
    return ((float)(UINT32_C(2) * (uint32_t)player_index + UINT32_C(1)) -
            (float)player_count) * authored_spawn_spacing_f32;
}

pf_status reference_stage_geometry_line_count(
    enum reference_stage stage,
    uint16_t *out_line_count)
{
    const ssbm_stage_collision_profile *profile;

    if (out_line_count == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    profile = ssbm_reference_stage_collision((uint16_t)stage);
    if (profile == NULL)
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    *out_line_count = profile->line_count;
    return PF_STATUS_OK;
}

pf_status reference_stage_geometry_line(
    enum reference_stage stage,
    uint16_t line_index,
    reference_stage_line *out_line)
{
    const ssbm_stage_collision_profile *profile;
    const ssbm_stage_collision_line *source;

    if (out_line == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    profile = ssbm_reference_stage_collision((uint16_t)stage);
    if (profile == NULL || line_index >= profile->line_count)
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    source = &profile->lines[line_index];
    out_line->start_x_f32 = source->start_x_f32;
    out_line->start_y_f32 = source->start_y_f32;
    out_line->end_x_f32 = source->end_x_f32;
    out_line->end_y_f32 = source->end_y_f32;
    out_line->source_normal_x_f32 = source->source_normal_x_f32;
    out_line->source_normal_y_f32 = source->source_normal_y_f32;
    out_line->support = (uint16_t)(line_index + UINT16_C(1));
    out_line->kind = source->kind;
    out_line->reserved = UINT8_C(0);
    return PF_STATUS_OK;
}

float ssbm_stage_line_y_f32(
    const ssbm_stage_collision_line *line,
    float position_x_f32)
{
    float dx;

    if (line == NULL)
    {
        return 0.0f;
    }
    dx = line->end_x_f32 - line->start_x_f32;
    return dx == 0.0f
               ? line->start_y_f32
               : line->start_y_f32 +
                     (position_x_f32 - line->start_x_f32) *
                         (line->end_y_f32 - line->start_y_f32) / dx;
}

float ssbm_stage_line_x_f32(
    const ssbm_stage_collision_line *line,
    float position_y_f32)
{
    const float dy = line->end_y_f32 - line->start_y_f32;

    return dy == 0.0f
               ? line->start_x_f32
               : line->start_x_f32 +
                     (position_y_f32 - line->start_y_f32) *
                         (line->end_x_f32 - line->start_x_f32) / dy;
}

int ssbm_reference_stage_find_ceiling_contact(
    uint16_t profile_id,
    float position_x_f32,
    float previous_top_f32,
    float current_top_f32,
    float *out_ceiling_y_f32,
    uint8_t *out_support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    float nearest_y_f32 = -FLT_MAX;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t offset;

    if (profile == NULL || out_ceiling_y_f32 == NULL ||
        out_support == NULL || current_top_f32 > previous_top_f32)
    {
        return 0;
    }
    for (offset = UINT16_C(0); offset < profile->ceiling_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(profile->ceiling_start + offset);
        const ssbm_stage_collision_line *line;
        float left;
        float right;
        float ceiling;

        if (line_index >= profile->line_count || line_index >= UINT8_MAX)
        {
            return 0;
        }
        line = &profile->lines[line_index];
        if (line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_CEILING ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0))
        {
            continue;
        }
        left = line->start_x_f32 < line->end_x_f32
                   ? line->start_x_f32
                   : line->end_x_f32;
        right = line->start_x_f32 > line->end_x_f32
                    ? line->start_x_f32
                    : line->end_x_f32;
        if (position_x_f32 < left || position_x_f32 > right)
        {
            continue;
        }
        ceiling = ssbm_stage_line_y_f32(line, position_x_f32);
        if (previous_top_f32 >= ceiling && current_top_f32 <= ceiling &&
            (nearest_support == UINT8_C(0) || ceiling > nearest_y_f32))
        {
            nearest_y_f32 = ceiling;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_ceiling_y_f32 = nearest_y_f32;
    *out_support = nearest_support;
    return 1;
}

int ssbm_reference_stage_find_wall_contact(
    uint16_t profile_id,
    float previous_position_x_f32,
    float current_position_x_f32,
    float swept_body_top_f32,
    float swept_body_bottom_f32,
    float half_width_f32,
    float *out_position_x_f32,
    uint8_t *out_support,
    int8_t *out_away_direction)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const int moving_right = current_position_x_f32 > previous_position_x_f32;
    const int moving_left = current_position_x_f32 < previous_position_x_f32;
    float nearest = moving_right != 0 ? FLT_MAX : -FLT_MAX;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t range_start;
    uint16_t range_count;
    uint16_t offset;

    if (profile == NULL || out_position_x_f32 == NULL || out_support == NULL ||
        out_away_direction == NULL || (moving_right == 0 && moving_left == 0) ||
        swept_body_top_f32 >= swept_body_bottom_f32)
    {
        return 0;
    }
    range_start = moving_right != 0 ? profile->left_wall_start
                                    : profile->right_wall_start;
    range_count = moving_right != 0 ? profile->left_wall_count
                                    : profile->right_wall_count;
    for (offset = UINT16_C(0); offset < range_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(range_start + offset);
        const ssbm_stage_collision_line *line;
        const uint8_t expected_kind =
            moving_right != 0 ? (uint8_t)PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL
                              : (uint8_t)PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL;
        float line_top;
        float line_bottom;
        float overlap_top;
        float overlap_bottom;
        float x_top;
        float x_bottom;
        float wall_x;
        float candidate;
        int crossed;

        if (line_index >= profile->line_count || line_index >= UINT8_MAX)
        {
            return 0;
        }
        line = &profile->lines[line_index];
        if (line->kind != expected_kind ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0))
        {
            continue;
        }
        line_top = line->start_y_f32 < line->end_y_f32
                       ? line->start_y_f32
                       : line->end_y_f32;
        line_bottom = line->start_y_f32 > line->end_y_f32
                          ? line->start_y_f32
                          : line->end_y_f32;
        overlap_top = swept_body_top_f32 > line_top
                          ? swept_body_top_f32
                          : line_top;
        overlap_bottom = swept_body_bottom_f32 < line_bottom
                             ? swept_body_bottom_f32
                             : line_bottom;
        if (overlap_top >= overlap_bottom)
        {
            continue;
        }
        x_top = ssbm_stage_line_x_f32(line, overlap_top);
        x_bottom = ssbm_stage_line_x_f32(line, overlap_bottom);
        wall_x = moving_right != 0 ? (x_top < x_bottom ? x_top : x_bottom)
                                   : (x_top > x_bottom ? x_top : x_bottom);
        candidate = moving_right != 0 ? wall_x - half_width_f32
                                      : wall_x + half_width_f32;
        crossed = moving_right != 0
                      ? previous_position_x_f32 <= candidate &&
                            current_position_x_f32 >= candidate
                      : previous_position_x_f32 >= candidate &&
                            current_position_x_f32 <= candidate;
        if (crossed != 0 &&
            (nearest_support == UINT8_C(0) ||
             (moving_right != 0 && candidate < nearest) ||
             (moving_left != 0 && candidate > nearest)))
        {
            nearest = candidate;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_position_x_f32 = nearest;
    *out_support = nearest_support;
    *out_away_direction = moving_right != 0 ? INT8_C(-1) : INT8_C(1);
    return 1;
}

static float ssbm_cross(float ax, float ay, float bx, float by)
{
    return ax * by - ay * bx;
}

static int ssbm_segment_contact(
    float previous_x,
    float previous_y,
    float current_x,
    float current_y,
    const ssbm_stage_collision_line *line,
    float *out_fraction)
{
    const float sweep_x = current_x - previous_x;
    const float sweep_y = current_y - previous_y;
    const float line_x = line->end_x_f32 - line->start_x_f32;
    const float line_y = line->end_y_f32 - line->start_y_f32;
    const float relative_x = line->start_x_f32 - previous_x;
    const float relative_y = line->start_y_f32 - previous_y;
    const float denominator = ssbm_cross(sweep_x, sweep_y, line_x, line_y);
    float sweep_fraction;
    float line_fraction;

    if (denominator == 0.0f)
    {
        return 0;
    }
    sweep_fraction = ssbm_cross(relative_x, relative_y, line_x, line_y) /
                     denominator;
    line_fraction = ssbm_cross(relative_x, relative_y, sweep_x, sweep_y) /
                    denominator;
    if (sweep_fraction < 0.0f || sweep_fraction > 1.0f ||
        line_fraction < 0.0f || line_fraction > 1.0f)
    {
        return 0;
    }
    *out_fraction = sweep_fraction;
    return 1;
}

int ssbm_reference_stage_find_floor_point_contact(
    uint16_t profile_id,
    float previous_point_x_f32,
    float previous_point_y_f32,
    float current_point_x_f32,
    float current_point_y_f32,
    float *out_fraction_f32,
    float *out_floor_y_f32,
    uint8_t *out_support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    float nearest_fraction = FLT_MAX;
    float nearest_floor_y = 0.0f;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t offset;

    if (profile == NULL || out_fraction_f32 == NULL ||
        out_floor_y_f32 == NULL || out_support == NULL ||
        current_point_y_f32 <= previous_point_y_f32)
    {
        return 0;
    }
    for (offset = UINT16_C(0); offset < profile->floor_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(profile->floor_start + offset);
        const ssbm_stage_collision_line *line;
        float fraction;

        if (line_index >= profile->line_count || line_index >= UINT8_MAX)
        {
            return 0;
        }
        line = &profile->lines[line_index];
        if (line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0) ||
            !ssbm_segment_contact(
                previous_point_x_f32,
                previous_point_y_f32,
                current_point_x_f32,
                current_point_y_f32,
                line,
                &fraction))
        {
            continue;
        }
        if (nearest_support == UINT8_C(0) || fraction < nearest_fraction)
        {
            nearest_fraction = fraction;
            nearest_floor_y = previous_point_y_f32 +
                              (current_point_y_f32 - previous_point_y_f32) *
                                  fraction;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_fraction_f32 = nearest_fraction;
    *out_floor_y_f32 = nearest_floor_y;
    *out_support = nearest_support;
    return 1;
}

int ssbm_reference_stage_find_wall_point_contact(
    uint16_t profile_id,
    float previous_point_x_f32,
    float previous_point_y_f32,
    float current_point_x_f32,
    float current_point_y_f32,
    float *out_fraction_f32,
    uint8_t *out_support,
    int8_t *out_away_direction)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const int moving_right = current_point_x_f32 > previous_point_x_f32;
    const int moving_left = current_point_x_f32 < previous_point_x_f32;
    float nearest_fraction = FLT_MAX;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t range_start;
    uint16_t range_count;
    uint16_t offset;

    if (profile == NULL || out_fraction_f32 == NULL || out_support == NULL ||
        out_away_direction == NULL || (moving_right == 0 && moving_left == 0))
    {
        return 0;
    }
    range_start = moving_right != 0 ? profile->left_wall_start
                                    : profile->right_wall_start;
    range_count = moving_right != 0 ? profile->left_wall_count
                                    : profile->right_wall_count;
    for (offset = UINT16_C(0); offset < range_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(range_start + offset);
        const ssbm_stage_collision_line *line;
        const uint8_t expected_kind =
            moving_right != 0 ? (uint8_t)PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL
                              : (uint8_t)PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL;
        float fraction;

        if (line_index >= profile->line_count || line_index >= UINT8_MAX)
        {
            return 0;
        }
        line = &profile->lines[line_index];
        if (line->kind != expected_kind ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0) ||
            !ssbm_segment_contact(
                previous_point_x_f32,
                previous_point_y_f32,
                current_point_x_f32,
                current_point_y_f32,
                line,
                &fraction))
        {
            continue;
        }
        if (nearest_support == UINT8_C(0) || fraction < nearest_fraction)
        {
            nearest_fraction = fraction;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_fraction_f32 = nearest_fraction;
    *out_support = nearest_support;
    *out_away_direction = moving_right != 0 ? INT8_C(-1) : INT8_C(1);
    return 1;
}

int ssbm_stage_support_valid(
    uint16_t profile_id,
    uint8_t support,
    uint8_t grounded)
{
    const ssbm_stage_collision_line *line;

    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        return support <= (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
    }
    if (support == (uint8_t)PF_M4_SURFACE_NONE)
    {
        return grounded == UINT8_C(0);
    }
    line = ssbm_reference_stage_line(profile_id, support);
    return line != NULL &&
           (line->runtime_flags & UINT32_C(0x00010000)) != UINT32_C(0) &&
           (line->runtime_flags & UINT32_C(0x00040000)) == UINT32_C(0) &&
           (grounded == UINT8_C(0) ||
            line->kind == (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR);
}

uint8_t ssbm_reference_stage_ledge_support(
    uint16_t profile_id,
    float ledge_x_f32)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    uint16_t line_index;

    if (profile == NULL)
    {
        return (uint8_t)PF_M4_SURFACE_FLOOR;
    }
    for (line_index = UINT16_C(0); line_index < profile->line_count; ++line_index)
    {
        const ssbm_stage_collision_line *line = &profile->lines[line_index];
        const int endpoint_matches = line->start_x_f32 == ledge_x_f32 ||
                                     line->end_x_f32 == ledge_x_f32;

        if (endpoint_matches != 0 &&
            line->kind == (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR &&
            (line->property_material_flags & UINT16_C(0x0200)) != UINT16_C(0) &&
            line_index < UINT8_MAX)
        {
            return (uint8_t)(line_index + UINT16_C(1));
        }
    }
    return (uint8_t)PF_M4_SURFACE_NONE;
}
