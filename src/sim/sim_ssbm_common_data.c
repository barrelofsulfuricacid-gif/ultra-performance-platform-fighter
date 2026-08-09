#include "sim_ssbm_common_data.h"

#include <stddef.h>
#include <stdint.h>

#include "../../generated/data/m4_ssbm_ntsc102_common_data.inc"

_Static_assert(
    sizeof(pf_m4_ssbm_common_raw_words) /
            sizeof(pf_m4_ssbm_common_raw_words[0]) ==
        (size_t)PF_M4_SSBM_COMMON_RAW_WORD_COUNT,
    "SSBM common-data raw-word table must be complete");

const uint8_t *pf_m4_ssbm_common_reference_source_sha256(void)
{
    return pf_m4_ssbm_common_source_sha256;
}

const uint32_t *pf_m4_ssbm_common_reference_raw_words(
    uint16_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = PF_M4_SSBM_COMMON_RAW_WORD_COUNT;
    }
    return pf_m4_ssbm_common_raw_words;
}

const pf_m4_ssbm_damage_response_attributes *
pf_m4_ssbm_common_reference_damage_response(void)
{
    return &pf_m4_ssbm_damage_response_attribute_data;
}

const pf_m4_ssbm_surface_response_attributes *
pf_m4_ssbm_common_reference_surface_response(void)
{
    return &pf_m4_ssbm_surface_response_attribute_data;
}
