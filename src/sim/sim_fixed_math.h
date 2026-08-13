#ifndef PF_SIM_FIXED_MATH_H
#define PF_SIM_FIXED_MATH_H

#include <stdint.h>

extern const uint16_t pf_m4_atan_turn_table[65];

static inline uint32_t pf_m4_u64_sqrt(uint64_t value)
{
    uint64_t result = UINT64_C(0);
    uint64_t bit = UINT64_C(1) << 62U;

    while (bit > value)
    {
        bit >>= 2U;
    }
    while (bit != UINT64_C(0))
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1U) + bit;
        }
        else
        {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result > (uint64_t)UINT32_MAX
               ? UINT32_MAX
               : (uint32_t)result;
}

static inline uint16_t pf_m4_fixed_atan_octant_turn(
    uint32_t numerator,
    uint32_t denominator)
{
    const uint64_t position =
        ((uint64_t)numerator << 22U) / (uint64_t)denominator;
    const uint32_t index = (uint32_t)(position >> 16U);
    const uint32_t fraction = (uint32_t)position & UINT32_C(65535);
    const uint32_t lower = pf_m4_atan_turn_table[index];
    const uint32_t upper = pf_m4_atan_turn_table[
        index < UINT32_C(64) ? index + UINT32_C(1) : index];

    return (uint16_t)(
        lower +
        (((upper - lower) * fraction + UINT32_C(32768)) >> 16U));
}

static inline uint16_t pf_m4_fixed_atan2_turn(int32_t y, int32_t x)
{
    const uint32_t absolute_x =
        x < INT32_C(0) ? (uint32_t)(-(int64_t)x) : (uint32_t)x;
    const uint32_t absolute_y =
        y < INT32_C(0) ? (uint32_t)(-(int64_t)y) : (uint32_t)y;
    uint16_t octant;
    uint32_t angle;

    if ((absolute_x | absolute_y) == UINT32_C(0))
    {
        return UINT16_C(0);
    }
    octant = absolute_x >= absolute_y
                  ? pf_m4_fixed_atan_octant_turn(absolute_y, absolute_x)
                  : (uint16_t)(
                        UINT16_C(16384) -
                        pf_m4_fixed_atan_octant_turn(absolute_x, absolute_y));
    if (x >= INT32_C(0))
    {
        angle = y >= INT32_C(0)
                    ? (uint32_t)octant
                    : UINT32_C(65536) - (uint32_t)octant;
    }
    else
    {
        angle = y >= INT32_C(0)
                    ? UINT32_C(32768) - (uint32_t)octant
                    : UINT32_C(32768) + (uint32_t)octant;
    }
    return (uint16_t)angle;
}

#endif
