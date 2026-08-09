#include "sim_ssbm_stage_data.h"

#include <stddef.h>
#include <stdint.h>

#include "pf/m4.h"

#include "../../generated/data/m4_ssbm_ntsc102_hyrule_collision.inc"

_Static_assert(
    sizeof(pf_m4_ssbm_hyrule_collision_lines) /
            sizeof(pf_m4_ssbm_hyrule_collision_lines[0]) ==
        (size_t)91,
    "Hyrule Temple collision catalog must remain complete");

const pf_m4_ssbm_stage_collision_profile *
pf_m4_ssbm_reference_stage_collision(uint16_t profile_id)
{
    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE)
    {
        return &pf_m4_ssbm_hyrule_collision_profile;
    }
    return NULL;
}

const pf_m4_ssbm_stage_collision_line *
pf_m4_ssbm_reference_stage_line(uint16_t profile_id, uint8_t support)
{
    const pf_m4_ssbm_stage_collision_profile *profile =
        pf_m4_ssbm_reference_stage_collision(profile_id);
    const uint16_t line_index = (uint16_t)support - UINT16_C(1);

    if (profile == NULL || support == UINT8_C(0) ||
        line_index >= profile->line_count)
    {
        return NULL;
    }
    return &profile->lines[line_index];
}

int32_t pf_m4_ssbm_stage_line_y_q16(
    const pf_m4_ssbm_stage_collision_line *line,
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

int pf_m4_ssbm_stage_support_valid(
    uint16_t profile_id,
    uint8_t support,
    uint8_t grounded)
{
    const pf_m4_ssbm_stage_collision_line *line;

    if (profile_id == (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        return support <= (uint8_t)PF_M4_SURFACE_UPPER_PLATFORM;
    }
    if (support == (uint8_t)PF_M4_SURFACE_NONE)
    {
        return grounded == UINT8_C(0);
    }
    line = pf_m4_ssbm_reference_stage_line(profile_id, support);
    return line != NULL &&
           (line->runtime_flags & UINT32_C(0x00010000)) != UINT32_C(0) &&
           (line->runtime_flags & UINT32_C(0x00040000)) == UINT32_C(0) &&
           (grounded == UINT8_C(0) ||
            line->kind == (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR);
}
