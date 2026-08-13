#include "sim_ssbm_common_data.h"

#include <stddef.h>
#include <stdint.h>

#include "../../generated/data/ssbm_ntsc102_common_data.inc"

_Static_assert(
    sizeof(ssbm_common_raw_words) /
            sizeof(ssbm_common_raw_words[0]) ==
        (size_t)PF_M4_SSBM_COMMON_RAW_WORD_COUNT,
    "SSBM common-data raw-word table must be complete");

const uint8_t *ssbm_common_reference_source_sha256(void)
{
    return ssbm_common_source_sha256;
}

const uint32_t *ssbm_common_reference_raw_words(
    uint16_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = PF_M4_SSBM_COMMON_RAW_WORD_COUNT;
    }
    return ssbm_common_raw_words;
}

uint16_t ssbm_common_reference_jump_backward_axis_threshold(void)
{
    return ssbm_jump_backward_axis_threshold;
}

const ssbm_damage_response_attributes *
ssbm_common_reference_damage_response(void)
{
    return &ssbm_damage_response_attribute_data;
}

const ssbm_surface_response_attributes *
ssbm_common_reference_surface_response(void)
{
    return &ssbm_surface_response_attribute_data;
}

const ssbm_ledge_response_attributes *
ssbm_common_reference_ledge_response(void)
{
    return &ssbm_ledge_response_attribute_data;
}

const ssbm_mash_attributes *
ssbm_common_reference_mash(void)
{
    return &ssbm_mash_attribute_data;
}

const ssbm_ground_input_attributes *
ssbm_common_reference_ground_input(void)
{
    return &ssbm_ground_input_attribute_data;
}

int32_t ssbm_throw_animation_rate_q16(
    uint16_t fighter_weight,
    int weight_independent)
{
    const uint64_t denominator_q30 =
        (uint64_t)fighter_weight *
        (uint64_t)(uint32_t)
            ssbm_ground_input_attribute_data
                .throw_animation_weight_scale_q30;
    uint64_t rate_q16;

    if (weight_independent != 0)
    {
        return INT32_C(65536);
    }
    if (fighter_weight == UINT16_C(0) || denominator_q30 == UINT64_C(0))
    {
        return INT32_C(0);
    }
    rate_q16 =
        (UINT64_C(1) << 46U) + denominator_q30 / UINT64_C(2);
    rate_q16 /= denominator_q30;
    return rate_q16 <= (uint64_t)INT32_MAX
               ? (int32_t)rate_q16
               : INT32_C(0);
}

uint16_t ssbm_throw_animation_ticks(
    uint16_t source_frames,
    uint16_t fighter_weight,
    int weight_independent)
{
    const uint64_t denominator_q30 =
        (uint64_t)fighter_weight *
        (uint64_t)(uint32_t)
            ssbm_ground_input_attribute_data
                .throw_animation_weight_scale_q30;
    uint64_t ticks;

    if (weight_independent != 0)
    {
        return source_frames;
    }
    if (source_frames == UINT16_C(0) || fighter_weight == UINT16_C(0) ||
        denominator_q30 == UINT64_C(0))
    {
        return UINT16_C(0);
    }
    ticks = (uint64_t)source_frames * denominator_q30 +
            ((UINT64_C(1) << 30U) - UINT64_C(1));
    ticks >>= 30U;
    return ticks <= (uint64_t)UINT16_MAX
               ? (uint16_t)ticks
               : UINT16_C(0);
}

const ssbm_rebirth_attributes *
ssbm_common_reference_rebirth(void)
{
    return &ssbm_rebirth_attribute_data;
}

const ssbm_match_entry_attributes *
ssbm_common_reference_match_entry(void)
{
    return &ssbm_match_entry_attribute_data;
}

const ssbm_clank_attributes *
ssbm_common_reference_clank(void)
{
    return &ssbm_clank_attribute_data;
}

const ssbm_fall_animation_attributes *
ssbm_common_reference_fall_animation(void)
{
    return &ssbm_fall_animation_attribute_data;
}
