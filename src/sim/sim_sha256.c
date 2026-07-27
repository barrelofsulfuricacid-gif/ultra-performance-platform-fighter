#include "sim_sha256.h"

#include <stddef.h>
#include <stdint.h>

static const uint32_t pf_sha256_round_constants[64] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491),
    UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b), UINT32_C(0x59f111f1),
    UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98), UINT32_C(0x12835b01),
    UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe),
    UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786),
    UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa),
    UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d),
    UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147),
    UINT32_C(0x06ca6351), UINT32_C(0x14292967),
    UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138),
    UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb),
    UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b),
    UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819), UINT32_C(0xd6990624),
    UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08),
    UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a),
    UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee), UINT32_C(0x78a5636f),
    UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb),
    UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2)};

static uint32_t pf_rotate_right(uint32_t value, uint32_t count)
{
    return (value >> count) | (value << (UINT32_C(32) - count));
}

static uint32_t pf_read_big_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
           ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static void pf_sha256_transform(pf_sha256 *context)
{
    uint32_t schedule[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t round;

    for (round = UINT32_C(0); round < UINT32_C(16); ++round)
    {
        schedule[round] =
            pf_read_big_u32(&context->block[(size_t)round * (size_t)4]);
    }
    for (; round < UINT32_C(64); ++round)
    {
        const uint32_t left = schedule[round - UINT32_C(15)];
        const uint32_t right = schedule[round - UINT32_C(2)];
        const uint32_t sigma0 =
            pf_rotate_right(left, UINT32_C(7)) ^
            pf_rotate_right(left, UINT32_C(18)) ^
            (left >> 3U);
        const uint32_t sigma1 =
            pf_rotate_right(right, UINT32_C(17)) ^
            pf_rotate_right(right, UINT32_C(19)) ^
            (right >> 10U);
        schedule[round] =
            schedule[round - UINT32_C(16)] + sigma0 +
            schedule[round - UINT32_C(7)] + sigma1;
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];

    for (round = UINT32_C(0); round < UINT32_C(64); ++round)
    {
        const uint32_t sum1 =
            pf_rotate_right(e, UINT32_C(6)) ^
            pf_rotate_right(e, UINT32_C(11)) ^
            pf_rotate_right(e, UINT32_C(25));
        const uint32_t choose = (e & f) ^ ((~e) & g);
        const uint32_t temporary1 =
            h + sum1 + choose + pf_sha256_round_constants[round] +
            schedule[round];
        const uint32_t sum0 =
            pf_rotate_right(a, UINT32_C(2)) ^
            pf_rotate_right(a, UINT32_C(13)) ^
            pf_rotate_right(a, UINT32_C(22));
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

void pf_sha256_init(pf_sha256 *context)
{
    context->state[0] = UINT32_C(0x6a09e667);
    context->state[1] = UINT32_C(0xbb67ae85);
    context->state[2] = UINT32_C(0x3c6ef372);
    context->state[3] = UINT32_C(0xa54ff53a);
    context->state[4] = UINT32_C(0x510e527f);
    context->state[5] = UINT32_C(0x9b05688c);
    context->state[6] = UINT32_C(0x1f83d9ab);
    context->state[7] = UINT32_C(0x5be0cd19);
    context->byte_count = UINT64_C(0);
    context->block_size = (size_t)0;
}

void pf_sha256_update(
    pf_sha256 *context,
    const uint8_t *bytes,
    size_t byte_count)
{
    size_t byte_index;

    for (byte_index = (size_t)0; byte_index < byte_count; ++byte_index)
    {
        context->block[context->block_size] = bytes[byte_index];
        ++context->block_size;
        ++context->byte_count;
        if (context->block_size == sizeof(context->block))
        {
            pf_sha256_transform(context);
            context->block_size = (size_t)0;
        }
    }
}

void pf_sha256_finish(pf_sha256 *context, uint8_t digest[32])
{
    const uint64_t bit_count = context->byte_count * UINT64_C(8);
    uint32_t state_index;
    uint32_t byte_index;

    context->block[context->block_size] = UINT8_C(0x80);
    ++context->block_size;

    if (context->block_size > (size_t)56)
    {
        while (context->block_size < sizeof(context->block))
        {
            context->block[context->block_size] = UINT8_C(0);
            ++context->block_size;
        }
        pf_sha256_transform(context);
        context->block_size = (size_t)0;
    }

    while (context->block_size < (size_t)56)
    {
        context->block[context->block_size] = UINT8_C(0);
        ++context->block_size;
    }
    for (byte_index = UINT32_C(0); byte_index < UINT32_C(8); ++byte_index)
    {
        context->block[(size_t)56 + (size_t)byte_index] =
            (uint8_t)(bit_count >>
                      (UINT32_C(56) - UINT32_C(8) * byte_index));
    }
    pf_sha256_transform(context);

    for (state_index = UINT32_C(0);
         state_index < UINT32_C(8);
         ++state_index)
    {
        const uint32_t value = context->state[state_index];
        digest[(size_t)state_index * (size_t)4] =
            (uint8_t)(value >> 24U);
        digest[(size_t)state_index * (size_t)4 + (size_t)1] =
            (uint8_t)(value >> 16U);
        digest[(size_t)state_index * (size_t)4 + (size_t)2] =
            (uint8_t)(value >> 8U);
        digest[(size_t)state_index * (size_t)4 + (size_t)3] =
            (uint8_t)value;
    }
}
