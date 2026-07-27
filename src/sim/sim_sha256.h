#ifndef PF_SIM_SHA256_H
#define PF_SIM_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct pf_sha256
{
    uint32_t state[8];
    uint64_t byte_count;
    uint8_t block[64];
    size_t block_size;
} pf_sha256;

void pf_sha256_init(pf_sha256 *context);
void pf_sha256_update(
    pf_sha256 *context,
    const uint8_t *bytes,
    size_t byte_count);
void pf_sha256_finish(pf_sha256 *context, uint8_t digest[32]);

#endif
