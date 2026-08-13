#include "sim_ssbm_stage_data.h"

#include <stddef.h>
#include <stdint.h>

#include "pf/m4.h"
#include "sim_collision.h"

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

const ssbm_stage_collision_profile *
ssbm_reference_stage_collision(uint16_t profile_id)
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

const ssbm_stage_collision_line *
ssbm_reference_stage_line(uint16_t profile_id, uint8_t support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const uint16_t line_index = (uint16_t)support - UINT16_C(1);

    if (profile == NULL || support == UINT8_C(0) ||
        line_index >= profile->line_count)
    {
        return NULL;
    }
    return &profile->lines[line_index];
}

const ssbm_stage_spawn_point *
ssbm_reference_stage_spawn_point(
    uint16_t profile_id,
    uint8_t player_index)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);

    if (profile == NULL || profile->spawn_points == NULL ||
        player_index >= profile->spawn_point_count)
    {
        return NULL;
    }
    return &profile->spawn_points[player_index];
}

int32_t ssbm_revival_platform_x_q16(
    uint16_t profile_id,
    uint8_t player_count,
    uint8_t player_index,
    int32_t authored_spawn_spacing_q16)
{
    /*
     * fn_8016719C stores the two-player Battlefield spawn-platform positions
     * and ftCo_800D4FF4 reuses them for every stock.  These are source-world
     * X values 12.8 and -40 transformed by the imported 12/115 stage scale.
     * Keep the table here so movement and snapshot validation share one
     * allocation-free source of truth.  FD and Stadium join this catalog when
     * their collision profiles are imported.
     */
    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_BATTLEFIELD &&
        player_count == UINT8_C(2) && player_index < UINT8_C(2))
    {
        static const int32_t battlefield_two_player_x_q16[2] = {
            INT32_C(87533),
            -INT32_C(273542)};

        return battlefield_two_player_x_q16[player_index];
    }

    return (
        (int32_t)(UINT32_C(2) * (uint32_t)player_index + UINT32_C(1)) -
        (int32_t)player_count) *
        authored_spawn_spacing_q16;
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
    out_line->start_x_q16 = source->start_x_q16;
    out_line->start_y_q16 = source->start_y_q16;
    out_line->end_x_q16 = source->end_x_q16;
    out_line->end_y_q16 = source->end_y_q16;
    out_line->source_normal_x_q16 = source->source_normal_x_q16;
    out_line->source_normal_y_q16 = source->source_normal_y_q16;
    out_line->support = (uint16_t)(line_index + UINT16_C(1));
    out_line->kind = source->kind;
    out_line->reserved = UINT8_C(0);
    return PF_STATUS_OK;
}

int32_t ssbm_stage_line_y_q16(
    const ssbm_stage_collision_line *line,
    int32_t position_x_q16)
{
    int64_t dx;
    int64_t dy;

    if (line == NULL)
    {
        return INT32_C(0);
    }
    dx = (int64_t)line->end_x_q16 - (int64_t)line->start_x_q16;
    dy = (int64_t)line->end_y_q16 - (int64_t)line->start_y_q16;
    if (dx == INT64_C(0))
    {
        return line->start_y_q16;
    }
    return (int32_t)(
        (int64_t)line->start_y_q16 +
        ((int64_t)position_x_q16 - (int64_t)line->start_x_q16) * dy /
            dx);
}

int ssbm_reference_stage_find_ceiling_contact(
    uint16_t profile_id,
    int32_t position_x_q16,
    int64_t previous_top_q16,
    int64_t current_top_q16,
    int32_t *out_ceiling_y_q16,
    uint8_t *out_support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    int32_t nearest_y_q16 = INT32_MIN;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t offset;

    if (profile == NULL || out_ceiling_y_q16 == NULL ||
        out_support == NULL || current_top_q16 > previous_top_q16)
    {
        return 0;
    }
    for (offset = UINT16_C(0); offset < profile->ceiling_count; ++offset)
    {
        const uint16_t line_index =
            (uint16_t)(profile->ceiling_start + offset);
        const ssbm_stage_collision_line *line;
        int32_t left_q16;
        int32_t right_q16;
        int32_t ceiling_y_q16;

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
        left_q16 = line->start_x_q16 < line->end_x_q16
                       ? line->start_x_q16
                       : line->end_x_q16;
        right_q16 = line->start_x_q16 > line->end_x_q16
                        ? line->start_x_q16
                        : line->end_x_q16;
        if (position_x_q16 < left_q16 || position_x_q16 > right_q16)
        {
            continue;
        }
        ceiling_y_q16 =
            ssbm_stage_line_y_q16(line, position_x_q16);
        if (previous_top_q16 >= (int64_t)ceiling_y_q16 &&
            current_top_q16 <= (int64_t)ceiling_y_q16 &&
            (nearest_support == UINT8_C(0) ||
             ceiling_y_q16 > nearest_y_q16))
        {
            nearest_y_q16 = ceiling_y_q16;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_ceiling_y_q16 = nearest_y_q16;
    *out_support = nearest_support;
    return 1;
}

static int32_t ssbm_stage_line_x_q16(
    const ssbm_stage_collision_line *line,
    int32_t position_y_q16)
{
    const int64_t dx =
        (int64_t)line->end_x_q16 - (int64_t)line->start_x_q16;
    const int64_t dy =
        (int64_t)line->end_y_q16 - (int64_t)line->start_y_q16;

    if (dy == INT64_C(0))
    {
        return line->start_x_q16;
    }
    return (int32_t)(
        (int64_t)line->start_x_q16 +
        ((int64_t)position_y_q16 - (int64_t)line->start_y_q16) * dx /
            dy);
}

int ssbm_reference_stage_find_wall_contact(
    uint16_t profile_id,
    int32_t previous_position_x_q16,
    int32_t current_position_x_q16,
    int64_t swept_body_top_q16,
    int64_t swept_body_bottom_q16,
    int32_t half_width_q16,
    int32_t *out_position_x_q16,
    uint8_t *out_support,
    int8_t *out_away_direction)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const int moving_right =
        current_position_x_q16 > previous_position_x_q16;
    const int moving_left =
        current_position_x_q16 < previous_position_x_q16;
    int64_t nearest_position_x_q16 =
        moving_right != 0 ? INT64_MAX : INT64_MIN;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t range_start;
    uint16_t range_count;
    uint16_t offset;

    if (profile == NULL || out_position_x_q16 == NULL ||
        out_support == NULL || out_away_direction == NULL ||
        (moving_right == 0 && moving_left == 0) ||
        swept_body_top_q16 >= swept_body_bottom_q16)
    {
        return 0;
    }
    if (moving_right != 0)
    {
        range_start = profile->left_wall_start;
        range_count = profile->left_wall_count;
    }
    else
    {
        range_start = profile->right_wall_start;
        range_count = profile->right_wall_count;
    }
    for (offset = UINT16_C(0); offset < range_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(range_start + offset);
        const ssbm_stage_collision_line *line;
        const uint8_t expected_kind =
            moving_right != 0
                ? (uint8_t)PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL
                : (uint8_t)PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL;
        int32_t line_top_q16;
        int32_t line_bottom_q16;
        int64_t overlap_top_q16;
        int64_t overlap_bottom_q16;
        int32_t x_at_top_q16;
        int32_t x_at_bottom_q16;
        int32_t wall_x_q16;
        int64_t candidate_position_x_q16;
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
        line_top_q16 = line->start_y_q16 < line->end_y_q16
                           ? line->start_y_q16
                           : line->end_y_q16;
        line_bottom_q16 = line->start_y_q16 > line->end_y_q16
                              ? line->start_y_q16
                              : line->end_y_q16;
        overlap_top_q16 =
            swept_body_top_q16 > (int64_t)line_top_q16
                ? swept_body_top_q16
                : (int64_t)line_top_q16;
        overlap_bottom_q16 =
            swept_body_bottom_q16 < (int64_t)line_bottom_q16
                ? swept_body_bottom_q16
                : (int64_t)line_bottom_q16;
        if (overlap_top_q16 >= overlap_bottom_q16)
        {
            continue;
        }
        x_at_top_q16 = ssbm_stage_line_x_q16(
            line,
            (int32_t)overlap_top_q16);
        x_at_bottom_q16 = ssbm_stage_line_x_q16(
            line,
            (int32_t)overlap_bottom_q16);
        wall_x_q16 =
            moving_right != 0
                ? (x_at_top_q16 < x_at_bottom_q16 ? x_at_top_q16
                                                   : x_at_bottom_q16)
                : (x_at_top_q16 > x_at_bottom_q16 ? x_at_top_q16
                                                   : x_at_bottom_q16);
        candidate_position_x_q16 =
            moving_right != 0
                ? (int64_t)wall_x_q16 - (int64_t)half_width_q16
                : (int64_t)wall_x_q16 + (int64_t)half_width_q16;
        crossed =
            moving_right != 0
                ? (int64_t)previous_position_x_q16 <=
                          candidate_position_x_q16 &&
                      (int64_t)current_position_x_q16 >=
                          candidate_position_x_q16
                : (int64_t)previous_position_x_q16 >=
                          candidate_position_x_q16 &&
                      (int64_t)current_position_x_q16 <=
                          candidate_position_x_q16;
        if (crossed != 0 &&
            (nearest_support == UINT8_C(0) ||
             (moving_right != 0 &&
              candidate_position_x_q16 < nearest_position_x_q16) ||
             (moving_left != 0 &&
              candidate_position_x_q16 > nearest_position_x_q16)))
        {
            nearest_position_x_q16 = candidate_position_x_q16;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0) ||
        nearest_position_x_q16 < INT32_MIN ||
        nearest_position_x_q16 > INT32_MAX)
    {
        return 0;
    }
    *out_position_x_q16 = (int32_t)nearest_position_x_q16;
    *out_support = nearest_support;
    *out_away_direction = moving_right != 0 ? INT8_C(-1) : INT8_C(1);
    return 1;
}

static int64_t ssbm_cross_q16(
    int64_t ax_q16,
    int64_t ay_q16,
    int64_t bx_q16,
    int64_t by_q16)
{
    return ax_q16 * by_q16 - ay_q16 * bx_q16;
}

int ssbm_reference_stage_find_floor_point_contact(
    uint16_t profile_id,
    int32_t previous_point_x_q16,
    int32_t previous_point_y_q16,
    int32_t current_point_x_q16,
    int32_t current_point_y_q16,
    uint32_t *out_fraction_q16,
    int32_t *out_floor_y_q16,
    uint8_t *out_support)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const int64_t sweep_x_q16 =
        (int64_t)current_point_x_q16 - previous_point_x_q16;
    const int64_t sweep_y_q16 =
        (int64_t)current_point_y_q16 - previous_point_y_q16;
    uint32_t nearest_fraction_q16 = UINT32_MAX;
    int32_t nearest_floor_y_q16 = INT32_C(0);
    uint8_t nearest_support = UINT8_C(0);
    uint16_t offset;

    if (profile == NULL || out_fraction_q16 == NULL ||
        out_floor_y_q16 == NULL || out_support == NULL ||
        sweep_y_q16 <= INT64_C(0))
    {
        return 0;
    }
    for (offset = UINT16_C(0); offset < profile->floor_count; ++offset)
    {
        const uint16_t line_index =
            (uint16_t)(profile->floor_start + offset);
        const ssbm_stage_collision_line *line;
        int64_t line_x_q16;
        int64_t line_y_q16;
        int64_t relative_x_q16;
        int64_t relative_y_q16;
        int64_t denominator;
        int64_t sweep_numerator;
        int64_t line_numerator;
        uint32_t fraction_q16;
        int64_t contact_y_q16;

        if (line_index >= profile->line_count || line_index >= UINT8_MAX)
        {
            return 0;
        }
        line = &profile->lines[line_index];
        if (line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
            (line->runtime_flags & UINT32_C(0x00010000)) == UINT32_C(0) ||
            (line->runtime_flags & UINT32_C(0x00040000)) != UINT32_C(0))
        {
            continue;
        }

        line_x_q16 = (int64_t)line->end_x_q16 - line->start_x_q16;
        line_y_q16 = (int64_t)line->end_y_q16 - line->start_y_q16;
        relative_x_q16 =
            (int64_t)line->start_x_q16 - previous_point_x_q16;
        relative_y_q16 =
            (int64_t)line->start_y_q16 - previous_point_y_q16;
        denominator = ssbm_cross_q16(
            sweep_x_q16,
            sweep_y_q16,
            line_x_q16,
            line_y_q16);
        sweep_numerator = ssbm_cross_q16(
            relative_x_q16,
            relative_y_q16,
            line_x_q16,
            line_y_q16);
        line_numerator = ssbm_cross_q16(
            relative_x_q16,
            relative_y_q16,
            sweep_x_q16,
            sweep_y_q16);
        if (denominator < INT64_C(0))
        {
            denominator = -denominator;
            sweep_numerator = -sweep_numerator;
            line_numerator = -line_numerator;
        }
        if (denominator == INT64_C(0) ||
            sweep_numerator < INT64_C(0) ||
            sweep_numerator > denominator ||
            line_numerator < INT64_C(0) ||
            line_numerator > denominator)
        {
            continue;
        }
        fraction_q16 = (uint32_t)collision_ratio_q16(
            sweep_numerator,
            denominator);
        contact_y_q16 =
            (int64_t)previous_point_y_q16 +
            (sweep_y_q16 * (int64_t)fraction_q16) / INT64_C(65536);
        if (contact_y_q16 < (int64_t)INT32_MIN ||
            contact_y_q16 > (int64_t)INT32_MAX)
        {
            return 0;
        }
        if (nearest_support == UINT8_C(0) ||
            fraction_q16 < nearest_fraction_q16)
        {
            nearest_fraction_q16 = fraction_q16;
            nearest_floor_y_q16 = (int32_t)contact_y_q16;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }
    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_fraction_q16 = nearest_fraction_q16;
    *out_floor_y_q16 = nearest_floor_y_q16;
    *out_support = nearest_support;
    return 1;
}

int ssbm_reference_stage_find_wall_point_contact(
    uint16_t profile_id,
    int32_t previous_point_x_q16,
    int32_t previous_point_y_q16,
    int32_t current_point_x_q16,
    int32_t current_point_y_q16,
    uint32_t *out_fraction_q16,
    uint8_t *out_support,
    int8_t *out_away_direction)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    const int64_t sweep_x_q16 =
        (int64_t)current_point_x_q16 - previous_point_x_q16;
    const int64_t sweep_y_q16 =
        (int64_t)current_point_y_q16 - previous_point_y_q16;
    const int moving_right = sweep_x_q16 > INT64_C(0);
    const int moving_left = sweep_x_q16 < INT64_C(0);
    uint32_t nearest_fraction_q16 = UINT32_MAX;
    uint8_t nearest_support = UINT8_C(0);
    uint16_t range_start;
    uint16_t range_count;
    uint16_t offset;

    if (profile == NULL || out_fraction_q16 == NULL ||
        out_support == NULL || out_away_direction == NULL ||
        (moving_right == 0 && moving_left == 0))
    {
        return 0;
    }
    if (moving_right != 0)
    {
        range_start = profile->left_wall_start;
        range_count = profile->left_wall_count;
    }
    else
    {
        range_start = profile->right_wall_start;
        range_count = profile->right_wall_count;
    }

    for (offset = UINT16_C(0); offset < range_count; ++offset)
    {
        const uint16_t line_index = (uint16_t)(range_start + offset);
        const ssbm_stage_collision_line *line;
        const uint8_t expected_kind =
            moving_right != 0
                ? (uint8_t)PF_M4_SSBM_STAGE_SURFACE_LEFT_WALL
                : (uint8_t)PF_M4_SSBM_STAGE_SURFACE_RIGHT_WALL;
        int64_t line_x_q16;
        int64_t line_y_q16;
        int64_t relative_x_q16;
        int64_t relative_y_q16;
        int64_t denominator;
        int64_t sweep_numerator;
        int64_t line_numerator;
        uint32_t fraction_q16;

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

        line_x_q16 = (int64_t)line->end_x_q16 - line->start_x_q16;
        line_y_q16 = (int64_t)line->end_y_q16 - line->start_y_q16;
        relative_x_q16 =
            (int64_t)line->start_x_q16 - previous_point_x_q16;
        relative_y_q16 =
            (int64_t)line->start_y_q16 - previous_point_y_q16;
        denominator = ssbm_cross_q16(
            sweep_x_q16,
            sweep_y_q16,
            line_x_q16,
            line_y_q16);
        sweep_numerator = ssbm_cross_q16(
            relative_x_q16,
            relative_y_q16,
            line_x_q16,
            line_y_q16);
        line_numerator = ssbm_cross_q16(
            relative_x_q16,
            relative_y_q16,
            sweep_x_q16,
            sweep_y_q16);
        if (denominator < INT64_C(0))
        {
            denominator = -denominator;
            sweep_numerator = -sweep_numerator;
            line_numerator = -line_numerator;
        }
        if (denominator == INT64_C(0) ||
            sweep_numerator < INT64_C(0) ||
            sweep_numerator > denominator ||
            line_numerator < INT64_C(0) ||
            line_numerator > denominator)
        {
            continue;
        }
        fraction_q16 = (uint32_t)collision_ratio_q16(
            sweep_numerator,
            denominator);
        if (nearest_support == UINT8_C(0) ||
            fraction_q16 < nearest_fraction_q16)
        {
            nearest_fraction_q16 = fraction_q16;
            nearest_support = (uint8_t)(line_index + UINT16_C(1));
        }
    }

    if (nearest_support == UINT8_C(0))
    {
        return 0;
    }
    *out_fraction_q16 = nearest_fraction_q16;
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
    int32_t ledge_x_q16)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(profile_id);
    uint16_t line_index;

    if (profile == NULL)
    {
        return (uint8_t)PF_M4_SURFACE_FLOOR;
    }
    for (line_index = UINT16_C(0);
         line_index < profile->line_count;
         ++line_index)
    {
        const ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const int endpoint_matches =
            line->start_x_q16 == ledge_x_q16 ||
            line->end_x_q16 == ledge_x_q16;

        if (endpoint_matches != 0 &&
            line->kind == (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR &&
            (line->property_material_flags & UINT16_C(0x0200)) !=
                UINT16_C(0) &&
            line_index < UINT8_MAX)
        {
            return (uint8_t)(line_index + UINT16_C(1));
        }
    }
    return (uint8_t)PF_M4_SURFACE_NONE;
}
