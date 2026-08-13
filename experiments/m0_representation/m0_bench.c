#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define NUM_ENVS 1024u
#define FIGHTERS_PER_ENV 4u
#define NUM_ACTORS (NUM_ENVS * FIGHTERS_PER_ENV)
#define LAYOUT_ENTITIES 32768u
#define DISPATCH_ENTITIES 32768u
#define MAX_ATTACKERS 16u
#define MAX_TARGETS 64u
#define SPARSE_TARGETS 16u
#define DENSE_TARGETS 64u
#define SNAPSHOT_BYTES 65536u
#define SNAPSHOT_CHUNK_BYTES 64u
#define SNAPSHOT_CHUNKS (SNAPSHOT_BYTES / SNAPSHOT_CHUNK_BYTES)
#define DIRTY_CHUNKS 8u

typedef uint64_t (*BenchFn)(uint64_t iterations);

typedef struct {
    const char *family;
    const char *candidate;
    const char *unit;
    BenchFn fn;
    uint64_t work_items_per_iteration;
    size_t state_bytes;
} BenchCase;

static volatile uint64_t g_sink;
static uint64_t g_rng = UINT64_C(0x6a09e667f3bcc909);

static uint64_t xorshift64(void)
{
    uint64_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    g_rng = x;
    return x;
}

static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

static uint64_t hash_bytes(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_mix(uint64_t hash, uint64_t value)
{
    hash ^= value + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6) +
            (hash >> 2);
    return hash;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} MotionF32;

typedef struct {
    float x_f32;
    float y_f32;
    float vx_f32;
    float vy_f32;
} MotionQ16;

typedef struct {
    uint8_t x;
    uint8_t y;
    int8_t vx;
    int8_t vy;
} MotionCell256;

typedef struct {
    int32_t x_subcell;
    int32_t y_subcell;
    float vx_subcell;
    float vy_subcell;
    float remainder_x;
    float remainder_y;
} MotionHybrid;

static MotionF32 g_motion_f32[NUM_ACTORS];
static MotionQ16 g_motion_f32[NUM_ACTORS];
static MotionCell256 g_motion_cell[NUM_ACTORS];
static MotionHybrid g_motion_hybrid[NUM_ACTORS];
static int8_t g_input_x[NUM_ACTORS];
static uint8_t g_fast_fall[NUM_ACTORS];

static void init_motion(void)
{
    for (size_t i = 0; i < NUM_ACTORS; ++i) {
        const uint32_t x = 24u + (uint32_t)(xorshift64() % 208u);
        const uint32_t y = 32u + (uint32_t)(xorshift64() % 192u);
        const int32_t vx = (int32_t)(xorshift64() % 7u) - 3;
        const int32_t vy = (int32_t)(xorshift64() % 7u) - 3;

        g_input_x[i] = (int8_t)((int32_t)(xorshift64() % 3u) - 1);
        g_fast_fall[i] = (uint8_t)(xorshift64() & 1u);

        g_motion_f32[i] =
            (MotionF32){(float)x, (float)y, (float)vx * 0.25f,
                        (float)vy * 0.25f};
        g_motion_f32[i] =
            (MotionQ16){(int32_t)x << 16, (int32_t)y << 16,
                        vx * (1 << 14), vy * (1 << 14)};
        g_motion_cell[i] =
            (MotionCell256){(uint8_t)x, (uint8_t)y, (int8_t)vx, (int8_t)vy};
        g_motion_hybrid[i] =
            (MotionHybrid){(int32_t)x * 256, (int32_t)y * 256,
                           (float)vx * 64.0f, (float)vy * 64.0f, 0.0f, 0.0f};
    }
}

static uint64_t bench_motion_f32(uint64_t iterations)
{
    const float accel = 0.0625f;
    const float traction = 0.96875f;
    const float gravity = 0.046875f;
    const float fast_gravity = 0.09375f;
    uint64_t checksum = 0;

    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < NUM_ACTORS; ++i) {
            MotionF32 *m = &g_motion_f32[i];
            m->vx = (m->vx + (float)g_input_x[i] * accel) * traction;
            if (fabsf(m->vx) < 0.00001f) {
                m->vx = 0.0f;
            }
            m->vy -= g_fast_fall[i] ? fast_gravity : gravity;
            m->x += m->vx;
            m->y += m->vy;

            if (m->x < 0.0f) {
                m->x = -m->x;
                m->vx = -m->vx * 0.5f;
            } else if (m->x > 255.0f) {
                m->x = 510.0f - m->x;
                m->vx = -m->vx * 0.5f;
            }
            if (m->y < 0.0f) {
                m->y = 0.0f;
                m->vy = -m->vy * 0.25f;
            } else if (m->y > 255.0f) {
                m->y = 255.0f;
                m->vy = -fabsf(m->vy) * 0.25f;
            }
        }
    }

    for (size_t i = 0; i < NUM_ACTORS; i += 97u) {
        checksum = hash_mix(checksum, float_bits(g_motion_f32[i].x));
        checksum = hash_mix(checksum, float_bits(g_motion_f32[i].y));
    }
    g_sink ^= checksum;
    return checksum;
}

static float bench_motion_f32(uint64_t iterations)
{
    const int32_t accel = 4096;
    const int32_t gravity = 3072;
    const int32_t fast_gravity = 6144;
    const int32_t max_position = 255 << 16;
    uint64_t checksum = 0;

    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < NUM_ACTORS; ++i) {
            MotionQ16 *m = &g_motion_f32[i];
            m->vx_f32 += (int32_t)g_input_x[i] * accel;
            m->vx_f32 -= m->vx_f32 >> 5;
            m->vy_f32 -= g_fast_fall[i] ? fast_gravity : gravity;
            m->x_f32 += m->vx_f32;
            m->y_f32 += m->vy_f32;

            if (m->x_f32 < 0) {
                m->x_f32 = -m->x_f32;
                m->vx_f32 = -(m->vx_f32 >> 1);
            } else if (m->x_f32 > max_position) {
                m->x_f32 = max_position - (m->x_f32 - max_position);
                m->vx_f32 = -(m->vx_f32 >> 1);
            }
            if (m->y_f32 < 0) {
                m->y_f32 = 0;
                m->vy_f32 = -(m->vy_f32 >> 2);
            } else if (m->y_f32 > max_position) {
                m->y_f32 = max_position;
                m->vy_f32 = -abs(m->vy_f32 >> 2);
            }
        }
    }

    for (size_t i = 0; i < NUM_ACTORS; i += 97u) {
        checksum = hash_mix(checksum, (uint32_t)g_motion_f32[i].x_f32);
        checksum = hash_mix(checksum, (uint32_t)g_motion_f32[i].y_f32);
    }
    g_sink ^= checksum;
    return checksum;
}

static int8_t clamp_i8(int value, int low, int high)
{
    if (value < low) {
        value = low;
    } else if (value > high) {
        value = high;
    }
    return (int8_t)value;
}

static uint64_t bench_motion_cell256(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < NUM_ACTORS; ++i) {
            MotionCell256 *m = &g_motion_cell[i];
            int vx = (int)m->vx + (int)g_input_x[i];
            int vy = (int)m->vy - (g_fast_fall[i] ? 2 : 1);
            vx -= vx / 8;
            vx = clamp_i8(vx, -12, 12);
            vy = clamp_i8(vy, -16, 16);

            int x = (int)m->x + vx;
            int y = (int)m->y + vy;
            if (x < 0) {
                x = -x;
                vx = -vx / 2;
            } else if (x > 255) {
                x = 510 - x;
                vx = -vx / 2;
            }
            if (y < 0) {
                y = 0;
                vy = -vy / 4;
            } else if (y > 255) {
                y = 255;
                vy = -abs(vy) / 4;
            }
            m->x = (uint8_t)x;
            m->y = (uint8_t)y;
            m->vx = (int8_t)vx;
            m->vy = (int8_t)vy;
        }
    }

    for (size_t i = 0; i < NUM_ACTORS; i += 97u) {
        checksum = hash_mix(checksum, g_motion_cell[i].x);
        checksum = hash_mix(checksum, g_motion_cell[i].y);
    }
    g_sink ^= checksum;
    return checksum;
}

static int32_t round_float_to_i32(float value)
{
    return value >= 0.0f ? (int32_t)(value + 0.5f)
                         : (int32_t)(value - 0.5f);
}

static uint64_t bench_motion_hybrid(uint64_t iterations)
{
    const float accel = 16.0f;
    const float gravity = 12.0f;
    const float fast_gravity = 24.0f;
    const int32_t max_position = 255 * 256;
    uint64_t checksum = 0;

    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < NUM_ACTORS; ++i) {
            MotionHybrid *m = &g_motion_hybrid[i];
            m->vx_subcell =
                (m->vx_subcell + (float)g_input_x[i] * accel) * 0.96875f;
            if (fabsf(m->vx_subcell) < 0.001f) {
                m->vx_subcell = 0.0f;
            }
            m->vy_subcell -= g_fast_fall[i] ? fast_gravity : gravity;

            const float step_x = m->vx_subcell + m->remainder_x;
            const float step_y = m->vy_subcell + m->remainder_y;
            const int32_t dx = round_float_to_i32(step_x);
            const int32_t dy = round_float_to_i32(step_y);
            m->remainder_x = step_x - (float)dx;
            m->remainder_y = step_y - (float)dy;
            m->x_subcell += dx;
            m->y_subcell += dy;

            if (m->x_subcell < 0) {
                m->x_subcell = -m->x_subcell;
                m->vx_subcell = -m->vx_subcell * 0.5f;
            } else if (m->x_subcell > max_position) {
                m->x_subcell =
                    max_position - (m->x_subcell - max_position);
                m->vx_subcell = -m->vx_subcell * 0.5f;
            }
            if (m->y_subcell < 0) {
                m->y_subcell = 0;
                m->vy_subcell = -m->vy_subcell * 0.25f;
            } else if (m->y_subcell > max_position) {
                m->y_subcell = max_position;
                m->vy_subcell = -fabsf(m->vy_subcell) * 0.25f;
            }
        }
    }

    for (size_t i = 0; i < NUM_ACTORS; i += 97u) {
        checksum = hash_mix(checksum, (uint32_t)g_motion_hybrid[i].x_subcell);
        checksum = hash_mix(checksum, (uint32_t)g_motion_hybrid[i].y_subcell);
    }
    g_sink ^= checksum;
    return checksum;
}

typedef struct {
    uint8_t x;
    uint8_t y;
    int8_t vx;
    int8_t vy;
} WorldU8;

typedef struct {
    uint16_t x;
    uint16_t y;
    int16_t vx;
    int16_t vy;
} WorldU16;

static WorldU8 g_world_u8[NUM_ACTORS];
static WorldU16 g_world_u16[NUM_ACTORS];

static void init_world_resolution(void)
{
    for (size_t i = 0; i < NUM_ACTORS; ++i) {
        const uint8_t x = (uint8_t)(16u + xorshift64() % 224u);
        const uint8_t y = (uint8_t)(16u + xorshift64() % 224u);
        const int8_t vx = (int8_t)((int32_t)(xorshift64() % 7u) - 3);
        const int8_t vy = (int8_t)((int32_t)(xorshift64() % 7u) - 3);
        g_world_u8[i] = (WorldU8){x, y, vx, vy};
        g_world_u16[i] =
            (WorldU16){(uint16_t)x * 16u, (uint16_t)y * 16u,
                       (int16_t)vx * 16, (int16_t)vy * 16};
    }
}

static uint64_t bench_world_u8(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < NUM_ACTORS; ++i) {
            WorldU8 *w = &g_world_u8[i];
            int x = (int)w->x + (int)w->vx;
            int y = (int)w->y + (int)w->vy;
            if (x < 0 || x > 255) {
                w->vx = (int8_t)-w->vx;
                x = x < 0 ? -x : 510 - x;
            }
            if (y < 0 || y > 255) {
                w->vy = (int8_t)-w->vy;
                y = y < 0 ? -y : 510 - y;
            }
            w->x = (uint8_t)x;
            w->y = (uint8_t)y;
        }
    }
    checksum = hash_bytes(g_world_u8, sizeof(g_world_u8));
    g_sink ^= checksum;
    return checksum;
}

static uint64_t bench_world_u16(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < NUM_ACTORS; ++i) {
            WorldU16 *w = &g_world_u16[i];
            int32_t x = (int32_t)w->x + (int32_t)w->vx;
            int32_t y = (int32_t)w->y + (int32_t)w->vy;
            if (x < 0 || x > 4095) {
                w->vx = (int16_t)-w->vx;
                x = x < 0 ? -x : 8190 - x;
            }
            if (y < 0 || y > 4095) {
                w->vy = (int16_t)-w->vy;
                y = y < 0 ? -y : 8190 - y;
            }
            w->x = (uint16_t)x;
            w->y = (uint16_t)y;
        }
    }
    checksum = hash_bytes(g_world_u16, sizeof(g_world_u16));
    g_sink ^= checksum;
    return checksum;
}

typedef struct {
    uint8_t min_x;
    uint8_t min_y;
    uint8_t max_x;
    uint8_t max_y;
} Box;

static Box g_attackers[MAX_ATTACKERS];
static Box g_targets[MAX_TARGETS];

static void init_boxes(void)
{
    for (size_t i = 0; i < MAX_ATTACKERS; ++i) {
        const uint8_t x = (uint8_t)(12u + xorshift64() % 210u);
        const uint8_t y = (uint8_t)(12u + xorshift64() % 210u);
        const uint8_t w = (uint8_t)(3u + xorshift64() % 13u);
        const uint8_t h = (uint8_t)(3u + xorshift64() % 13u);
        g_attackers[i] =
            (Box){x, y, (uint8_t)(x + w), (uint8_t)(y + h)};
    }
    for (size_t i = 0; i < MAX_TARGETS; ++i) {
        const uint8_t x = (uint8_t)(12u + xorshift64() % 210u);
        const uint8_t y = (uint8_t)(12u + xorshift64() % 210u);
        const uint8_t w = (uint8_t)(3u + xorshift64() % 13u);
        const uint8_t h = (uint8_t)(3u + xorshift64() % 13u);
        g_targets[i] = (Box){x, y, (uint8_t)(x + w), (uint8_t)(y + h)};
    }
}

static Box shifted_box(Box box, uint8_t shift)
{
    box.min_x = (uint8_t)(box.min_x + shift);
    box.max_x = (uint8_t)(box.max_x + shift);
    return box;
}

static bool boxes_intersect(Box a, Box b)
{
    return a.min_x <= b.max_x && a.max_x >= b.min_x &&
           a.min_y <= b.max_y && a.max_y >= b.min_y;
}

static uint8_t attacker_shift(size_t index, uint64_t tick)
{
    return (uint8_t)((tick + index * 5u) & 7u);
}

static uint8_t target_shift(size_t index, uint64_t tick)
{
    return (uint8_t)((tick * 3u + index * 3u) & 7u);
}

static uint64_t broadphase_naive(uint64_t iterations, size_t target_count)
{
    uint64_t hits = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t a = 0; a < MAX_ATTACKERS; ++a) {
            const Box attacker =
                shifted_box(g_attackers[a], attacker_shift(a, tick));
            bool any = false;
            for (size_t b = 0; b < target_count; ++b) {
                const Box target =
                    shifted_box(g_targets[b], target_shift(b, tick));
                any |= boxes_intersect(attacker, target);
            }
            hits += any ? 1u : 0u;
        }
    }
    g_sink ^= hits;
    return hits;
}

static void sort_target_indices(uint8_t *indices, size_t target_count,
                                uint64_t tick)
{
    for (size_t i = 0; i < target_count; ++i) {
        indices[i] = (uint8_t)i;
    }
    for (size_t i = 1; i < target_count; ++i) {
        const uint8_t value = indices[i];
        const uint16_t key =
            (uint16_t)g_targets[value].min_x + target_shift(value, tick);
        size_t j = i;
        while (j > 0) {
            const uint8_t prior = indices[j - 1u];
            const uint16_t prior_key =
                (uint16_t)g_targets[prior].min_x +
                target_shift(prior, tick);
            if (prior_key <= key) {
                break;
            }
            indices[j] = prior;
            --j;
        }
        indices[j] = value;
    }
}

static uint64_t broadphase_sweep(uint64_t iterations, size_t target_count)
{
    uint64_t hits = 0;
    uint8_t indices[MAX_TARGETS];
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        sort_target_indices(indices, target_count, tick);
        for (size_t a = 0; a < MAX_ATTACKERS; ++a) {
            const Box attacker =
                shifted_box(g_attackers[a], attacker_shift(a, tick));
            bool any = false;
            for (size_t j = 0; j < target_count; ++j) {
                const size_t b = indices[j];
                const Box target =
                    shifted_box(g_targets[b], target_shift(b, tick));
                if (target.min_x > attacker.max_x) {
                    break;
                }
                if (target.max_x < attacker.min_x) {
                    continue;
                }
                any |= attacker.min_y <= target.max_y &&
                       attacker.max_y >= target.min_y;
            }
            hits += any ? 1u : 0u;
        }
    }
    g_sink ^= hits;
    return hits;
}

static uint64_t broadphase_grid(uint64_t iterations, size_t target_count)
{
    uint64_t hits = 0;
    uint64_t cells[16u * 16u];

    for (uint64_t tick = 0; tick < iterations; ++tick) {
        memset(cells, 0, sizeof(cells));
        for (size_t b = 0; b < target_count; ++b) {
            const Box target =
                shifted_box(g_targets[b], target_shift(b, tick));
            const uint8_t min_x = (uint8_t)(target.min_x >> 4);
            const uint8_t max_x = (uint8_t)(target.max_x >> 4);
            const uint8_t min_y = (uint8_t)(target.min_y >> 4);
            const uint8_t max_y = (uint8_t)(target.max_y >> 4);
            const uint64_t target_bit = UINT64_C(1) << b;
            for (uint8_t y = min_y; y <= max_y; ++y) {
                for (uint8_t x = min_x; x <= max_x; ++x) {
                    cells[(size_t)y * 16u + x] |= target_bit;
                }
            }
        }

        for (size_t a = 0; a < MAX_ATTACKERS; ++a) {
            const Box attacker =
                shifted_box(g_attackers[a], attacker_shift(a, tick));
            const uint8_t min_x = (uint8_t)(attacker.min_x >> 4);
            const uint8_t max_x = (uint8_t)(attacker.max_x >> 4);
            const uint8_t min_y = (uint8_t)(attacker.min_y >> 4);
            const uint8_t max_y = (uint8_t)(attacker.max_y >> 4);
            uint64_t candidates = 0;
            for (uint8_t y = min_y; y <= max_y; ++y) {
                for (uint8_t x = min_x; x <= max_x; ++x) {
                    candidates |= cells[(size_t)y * 16u + x];
                }
            }

            bool any = false;
            while (candidates != 0u) {
                const unsigned b = (unsigned)__builtin_ctzll(candidates);
                candidates &= candidates - 1u;
                const Box target =
                    shifted_box(g_targets[b], target_shift(b, tick));
                any |= boxes_intersect(attacker, target);
            }
            hits += any ? 1u : 0u;
        }
    }
    g_sink ^= hits;
    return hits;
}

static void bitboard_set_rect(uint64_t *bits, Box box)
{
    for (uint16_t y = box.min_y; y <= box.max_y; ++y) {
        const uint16_t first_word = (uint16_t)(box.min_x >> 6);
        const uint16_t last_word = (uint16_t)(box.max_x >> 6);
        const uint8_t first_bit = (uint8_t)(box.min_x & 63u);
        const uint8_t last_bit = (uint8_t)(box.max_x & 63u);
        const size_t row = (size_t)y * 4u;

        if (first_word == last_word) {
            const uint64_t low = UINT64_MAX << first_bit;
            const uint64_t high =
                last_bit == 63u ? UINT64_MAX
                                : (UINT64_C(1) << (last_bit + 1u)) - 1u;
            bits[row + first_word] |= low & high;
        } else {
            bits[row + first_word] |= UINT64_MAX << first_bit;
            for (uint16_t word = (uint16_t)(first_word + 1u);
                 word < last_word; ++word) {
                bits[row + word] = UINT64_MAX;
            }
            bits[row + last_word] |=
                last_bit == 63u ? UINT64_MAX
                                : (UINT64_C(1) << (last_bit + 1u)) - 1u;
        }
    }
}

static bool bitboard_intersects_rect(const uint64_t *bits, Box box)
{
    for (uint16_t y = box.min_y; y <= box.max_y; ++y) {
        const uint16_t first_word = (uint16_t)(box.min_x >> 6);
        const uint16_t last_word = (uint16_t)(box.max_x >> 6);
        const uint8_t first_bit = (uint8_t)(box.min_x & 63u);
        const uint8_t last_bit = (uint8_t)(box.max_x & 63u);
        const size_t row = (size_t)y * 4u;

        if (first_word == last_word) {
            const uint64_t low = UINT64_MAX << first_bit;
            const uint64_t high =
                last_bit == 63u ? UINT64_MAX
                                : (UINT64_C(1) << (last_bit + 1u)) - 1u;
            if ((bits[row + first_word] & low & high) != 0u) {
                return true;
            }
        } else {
            if ((bits[row + first_word] & (UINT64_MAX << first_bit)) != 0u) {
                return true;
            }
            for (uint16_t word = (uint16_t)(first_word + 1u);
                 word < last_word; ++word) {
                if (bits[row + word] != 0u) {
                    return true;
                }
            }
            const uint64_t mask =
                last_bit == 63u ? UINT64_MAX
                                : (UINT64_C(1) << (last_bit + 1u)) - 1u;
            if ((bits[row + last_word] & mask) != 0u) {
                return true;
            }
        }
    }
    return false;
}

static uint64_t broadphase_bitboard(uint64_t iterations, size_t target_count)
{
    uint64_t hits = 0;
    uint64_t bits[256u * 4u];
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        memset(bits, 0, sizeof(bits));
        for (size_t b = 0; b < target_count; ++b) {
            const Box target =
                shifted_box(g_targets[b], target_shift(b, tick));
            bitboard_set_rect(bits, target);
        }
        for (size_t a = 0; a < MAX_ATTACKERS; ++a) {
            const Box attacker =
                shifted_box(g_attackers[a], attacker_shift(a, tick));
            hits += bitboard_intersects_rect(bits, attacker) ? 1u : 0u;
        }
    }
    g_sink ^= hits;
    return hits;
}

static uint64_t bench_broadphase_naive_sparse(uint64_t iterations)
{
    return broadphase_naive(iterations, SPARSE_TARGETS);
}

static uint64_t bench_broadphase_sweep_sparse(uint64_t iterations)
{
    return broadphase_sweep(iterations, SPARSE_TARGETS);
}

static uint64_t bench_broadphase_grid_sparse(uint64_t iterations)
{
    return broadphase_grid(iterations, SPARSE_TARGETS);
}

static uint64_t bench_broadphase_bitboard_sparse(uint64_t iterations)
{
    return broadphase_bitboard(iterations, SPARSE_TARGETS);
}

static uint64_t bench_broadphase_naive_dense(uint64_t iterations)
{
    return broadphase_naive(iterations, DENSE_TARGETS);
}

static uint64_t bench_broadphase_sweep_dense(uint64_t iterations)
{
    return broadphase_sweep(iterations, DENSE_TARGETS);
}

static uint64_t bench_broadphase_grid_dense(uint64_t iterations)
{
    return broadphase_grid(iterations, DENSE_TARGETS);
}

static uint64_t bench_broadphase_bitboard_dense(uint64_t iterations)
{
    return broadphase_bitboard(iterations, DENSE_TARGETS);
}

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint16_t damage;
    uint8_t state;
    uint8_t flags;
    uint32_t animation_frame;
    uint64_t cold_debug[6];
} EntityAoS;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint16_t damage;
    uint8_t state;
    uint8_t flags;
    uint32_t animation_frame;
} EntityHot;

typedef struct {
    float x[LAYOUT_ENTITIES];
    float y[LAYOUT_ENTITIES];
    float vx[LAYOUT_ENTITIES];
    float vy[LAYOUT_ENTITIES];
    uint16_t damage[LAYOUT_ENTITIES];
    uint8_t state[LAYOUT_ENTITIES];
    uint8_t flags[LAYOUT_ENTITIES];
    uint32_t animation_frame[LAYOUT_ENTITIES];
    uint64_t cold_debug[6][LAYOUT_ENTITIES];
} EntitySoA;

static EntityAoS g_aos[LAYOUT_ENTITIES];
static EntitySoA g_soa;
static EntityHot g_hot[LAYOUT_ENTITIES];
static uint64_t g_cold[LAYOUT_ENTITIES][6];

static void init_layouts(void)
{
    for (size_t i = 0; i < LAYOUT_ENTITIES; ++i) {
        const float x = (float)(xorshift64() & 1023u) * 0.25f;
        const float y = (float)(xorshift64() & 1023u) * 0.25f;
        const float vx = (float)((int32_t)(xorshift64() % 17u) - 8) * 0.03125f;
        const float vy = (float)((int32_t)(xorshift64() % 17u) - 8) * 0.03125f;
        const uint16_t damage = (uint16_t)(xorshift64() % 300u);
        const uint8_t state = (uint8_t)(xorshift64() & 7u);
        const uint8_t flags = (uint8_t)(xorshift64() & 3u);
        const uint32_t animation = (uint32_t)(xorshift64() % 120u);

        g_aos[i].x = x;
        g_aos[i].y = y;
        g_aos[i].vx = vx;
        g_aos[i].vy = vy;
        g_aos[i].damage = damage;
        g_aos[i].state = state;
        g_aos[i].flags = flags;
        g_aos[i].animation_frame = animation;

        g_soa.x[i] = x;
        g_soa.y[i] = y;
        g_soa.vx[i] = vx;
        g_soa.vy[i] = vy;
        g_soa.damage[i] = damage;
        g_soa.state[i] = state;
        g_soa.flags[i] = flags;
        g_soa.animation_frame[i] = animation;

        g_hot[i] = (EntityHot){x, y, vx, vy, damage, state, flags, animation};
        for (size_t c = 0; c < 6u; ++c) {
            const uint64_t cold_value = xorshift64();
            g_aos[i].cold_debug[c] = cold_value;
            g_soa.cold_debug[c][i] = cold_value;
            g_cold[i][c] = cold_value;
        }
    }
}

static uint64_t bench_layout_aos(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < LAYOUT_ENTITIES; ++i) {
            EntityAoS *e = &g_aos[i];
            e->vx += (float)((int)e->state - 3) * 0.0005f;
            e->vy -= 0.00075f;
            e->x += e->vx;
            e->y += e->vy;
            e->animation_frame += 1u;
            e->damage = (uint16_t)(e->damage + (e->flags & 1u));
            e->state = (uint8_t)((e->state + (e->animation_frame == 120u)) &
                                 7u);
            if (e->animation_frame == 120u) {
                e->animation_frame = 0u;
            }
        }
    }
    for (size_t i = 0; i < LAYOUT_ENTITIES; i += 509u) {
        checksum = hash_mix(checksum, float_bits(g_aos[i].x));
        checksum = hash_mix(checksum, g_aos[i].damage);
    }
    g_sink ^= checksum;
    return checksum;
}

static uint64_t bench_layout_soa(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < LAYOUT_ENTITIES; ++i) {
            g_soa.vx[i] += (float)((int)g_soa.state[i] - 3) * 0.0005f;
            g_soa.vy[i] -= 0.00075f;
            g_soa.x[i] += g_soa.vx[i];
            g_soa.y[i] += g_soa.vy[i];
            g_soa.animation_frame[i] += 1u;
            g_soa.damage[i] =
                (uint16_t)(g_soa.damage[i] + (g_soa.flags[i] & 1u));
            g_soa.state[i] =
                (uint8_t)((g_soa.state[i] +
                           (g_soa.animation_frame[i] == 120u)) &
                          7u);
            if (g_soa.animation_frame[i] == 120u) {
                g_soa.animation_frame[i] = 0u;
            }
        }
    }
    for (size_t i = 0; i < LAYOUT_ENTITIES; i += 509u) {
        checksum = hash_mix(checksum, float_bits(g_soa.x[i]));
        checksum = hash_mix(checksum, g_soa.damage[i]);
    }
    g_sink ^= checksum;
    return checksum;
}

static uint64_t bench_layout_hot_cold(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < LAYOUT_ENTITIES; ++i) {
            EntityHot *e = &g_hot[i];
            e->vx += (float)((int)e->state - 3) * 0.0005f;
            e->vy -= 0.00075f;
            e->x += e->vx;
            e->y += e->vy;
            e->animation_frame += 1u;
            e->damage = (uint16_t)(e->damage + (e->flags & 1u));
            e->state = (uint8_t)((e->state + (e->animation_frame == 120u)) &
                                 7u);
            if (e->animation_frame == 120u) {
                e->animation_frame = 0u;
            }
        }
    }
    for (size_t i = 0; i < LAYOUT_ENTITIES; i += 509u) {
        checksum = hash_mix(checksum, float_bits(g_hot[i].x));
        checksum = hash_mix(checksum, g_hot[i].damage);
    }
    g_sink ^= checksum;
    return checksum;
}

static uint8_t g_dispatch_state[DISPATCH_ENTITIES];
static float g_dispatch_x_switch[DISPATCH_ENTITIES];
static float g_dispatch_v_switch[DISPATCH_ENTITIES];
static float g_dispatch_x_table[DISPATCH_ENTITIES];
static float g_dispatch_v_table[DISPATCH_ENTITIES];
static float g_dispatch_x_function[DISPATCH_ENTITIES];
static float g_dispatch_v_function[DISPATCH_ENTITIES];

static const float g_dispatch_scale[8] = {0.0f,  0.5f, 1.0f,  1.5f,
                                          -0.5f, 2.0f, -1.0f, 0.25f};
static const float g_dispatch_bias[8] = {0.01f,  -0.02f, 0.03f,  0.0f,
                                         -0.01f, 0.04f, -0.03f, 0.02f};

static void init_dispatch(void)
{
    for (size_t i = 0; i < DISPATCH_ENTITIES; ++i) {
        g_dispatch_state[i] = (uint8_t)(xorshift64() & 7u);
        const float x = (float)(xorshift64() & 1023u) * 0.125f;
        const float v =
            (float)((int32_t)(xorshift64() % 31u) - 15) * 0.015625f;
        g_dispatch_x_switch[i] = x;
        g_dispatch_v_switch[i] = v;
        g_dispatch_x_table[i] = x;
        g_dispatch_v_table[i] = v;
        g_dispatch_x_function[i] = x;
        g_dispatch_v_function[i] = v;
    }
}

static uint64_t bench_dispatch_switch(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < DISPATCH_ENTITIES; ++i) {
            float scale;
            float bias;
            switch (g_dispatch_state[i]) {
            case 0:
                scale = 0.0f;
                bias = 0.01f;
                break;
            case 1:
                scale = 0.5f;
                bias = -0.02f;
                break;
            case 2:
                scale = 1.0f;
                bias = 0.03f;
                break;
            case 3:
                scale = 1.5f;
                bias = 0.0f;
                break;
            case 4:
                scale = -0.5f;
                bias = -0.01f;
                break;
            case 5:
                scale = 2.0f;
                bias = 0.04f;
                break;
            case 6:
                scale = -1.0f;
                bias = -0.03f;
                break;
            default:
                scale = 0.25f;
                bias = 0.02f;
                break;
            }
            g_dispatch_x_switch[i] +=
                g_dispatch_v_switch[i] * scale + bias;
        }
    }
    for (size_t i = 0; i < DISPATCH_ENTITIES; i += 509u) {
        checksum =
            hash_mix(checksum, float_bits(g_dispatch_x_switch[i]));
    }
    g_sink ^= checksum;
    return checksum;
}

static uint64_t bench_dispatch_data_table(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < DISPATCH_ENTITIES; ++i) {
            const uint8_t state = g_dispatch_state[i];
            g_dispatch_x_table[i] +=
                g_dispatch_v_table[i] * g_dispatch_scale[state] +
                g_dispatch_bias[state];
        }
    }
    for (size_t i = 0; i < DISPATCH_ENTITIES; i += 509u) {
        checksum = hash_mix(checksum, float_bits(g_dispatch_x_table[i]));
    }
    g_sink ^= checksum;
    return checksum;
}

typedef float (*DispatchFn)(float x, float velocity);

#define DEFINE_DISPATCH_FN(index)                                            \
    static float dispatch_fn_##index(float x, float velocity)                \
    {                                                                         \
        return x + (velocity * g_dispatch_scale[index] +                     \
                    g_dispatch_bias[index]);                                  \
    }

DEFINE_DISPATCH_FN(0)
DEFINE_DISPATCH_FN(1)
DEFINE_DISPATCH_FN(2)
DEFINE_DISPATCH_FN(3)
DEFINE_DISPATCH_FN(4)
DEFINE_DISPATCH_FN(5)
DEFINE_DISPATCH_FN(6)
DEFINE_DISPATCH_FN(7)

static const DispatchFn g_dispatch_functions[8] = {
    dispatch_fn_0, dispatch_fn_1, dispatch_fn_2, dispatch_fn_3,
    dispatch_fn_4, dispatch_fn_5, dispatch_fn_6, dispatch_fn_7};

static uint64_t bench_dispatch_function_table(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t i = 0; i < DISPATCH_ENTITIES; ++i) {
            g_dispatch_x_function[i] =
                g_dispatch_functions[g_dispatch_state[i]](
                    g_dispatch_x_function[i], g_dispatch_v_function[i]);
        }
    }
    for (size_t i = 0; i < DISPATCH_ENTITIES; i += 509u) {
        checksum =
            hash_mix(checksum, float_bits(g_dispatch_x_function[i]));
    }
    g_sink ^= checksum;
    return checksum;
}

static uint8_t g_snapshot_state[SNAPSHOT_BYTES];
static uint8_t g_snapshot_full[SNAPSHOT_BYTES];
static uint8_t g_snapshot_baseline[SNAPSHOT_BYTES];
static uint8_t g_snapshot_dirty[DIRTY_CHUNKS][SNAPSHOT_CHUNK_BYTES];
static uint16_t g_snapshot_dirty_indices[DIRTY_CHUNKS];
static uint8_t g_snapshot_scan[SNAPSHOT_BYTES];
static uint16_t g_snapshot_scan_indices[SNAPSHOT_CHUNKS];

static void init_snapshot(void)
{
    for (size_t i = 0; i < SNAPSHOT_BYTES; ++i) {
        g_snapshot_state[i] = (uint8_t)xorshift64();
    }
    memcpy(g_snapshot_baseline, g_snapshot_state, SNAPSHOT_BYTES);
}

static uint16_t dirty_chunk_index(size_t dirty_index, uint64_t tick)
{
    return (uint16_t)((tick * 131u + dirty_index * 97u) %
                      SNAPSHOT_CHUNKS);
}

static void mutate_snapshot_chunks(uint64_t tick)
{
    for (size_t d = 0; d < DIRTY_CHUNKS; ++d) {
        const uint16_t chunk = dirty_chunk_index(d, tick);
        const size_t offset = (size_t)chunk * SNAPSHOT_CHUNK_BYTES;
        for (size_t j = 0; j < SNAPSHOT_CHUNK_BYTES; ++j) {
            g_snapshot_state[offset + j] ^=
                (uint8_t)(tick + d * 17u + j * 3u + 1u);
        }
    }
}

static uint64_t sample_mutated_snapshot(uint64_t tick)
{
    uint64_t checksum = 0;
    for (size_t d = 0; d < DIRTY_CHUNKS; ++d) {
        const uint16_t chunk = dirty_chunk_index(d, tick);
        const size_t offset = (size_t)chunk * SNAPSHOT_CHUNK_BYTES;
        checksum = hash_mix(checksum, g_snapshot_state[offset]);
        checksum =
            hash_mix(checksum,
                     g_snapshot_state[offset + SNAPSHOT_CHUNK_BYTES - 1u]);
    }
    return checksum;
}

static uint64_t bench_snapshot_full(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        memcpy(g_snapshot_full, g_snapshot_state, SNAPSHOT_BYTES);
        mutate_snapshot_chunks(tick);
        checksum ^= sample_mutated_snapshot(tick);
        memcpy(g_snapshot_state, g_snapshot_full, SNAPSHOT_BYTES);
    }
    g_sink ^= checksum;
    return checksum;
}

static uint64_t bench_snapshot_tracked_dirty(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        for (size_t d = 0; d < DIRTY_CHUNKS; ++d) {
            const uint16_t chunk = dirty_chunk_index(d, tick);
            g_snapshot_dirty_indices[d] = chunk;
            memcpy(g_snapshot_dirty[d],
                   g_snapshot_state +
                       (size_t)chunk * SNAPSHOT_CHUNK_BYTES,
                   SNAPSHOT_CHUNK_BYTES);
        }
        mutate_snapshot_chunks(tick);
        checksum ^= sample_mutated_snapshot(tick);
        for (size_t d = 0; d < DIRTY_CHUNKS; ++d) {
            memcpy(g_snapshot_state +
                       (size_t)g_snapshot_dirty_indices[d] *
                           SNAPSHOT_CHUNK_BYTES,
                   g_snapshot_dirty[d], SNAPSHOT_CHUNK_BYTES);
        }
    }
    g_sink ^= checksum;
    return checksum;
}

static uint64_t bench_snapshot_scan_delta(uint64_t iterations)
{
    uint64_t checksum = 0;
    for (uint64_t tick = 0; tick < iterations; ++tick) {
        mutate_snapshot_chunks(tick);
        checksum ^= sample_mutated_snapshot(tick);

        size_t changed = 0;
        for (size_t chunk = 0; chunk < SNAPSHOT_CHUNKS; ++chunk) {
            const size_t offset = chunk * SNAPSHOT_CHUNK_BYTES;
            if (memcmp(g_snapshot_state + offset,
                       g_snapshot_baseline + offset,
                       SNAPSHOT_CHUNK_BYTES) != 0) {
                g_snapshot_scan_indices[changed] = (uint16_t)chunk;
                memcpy(g_snapshot_scan +
                           changed * SNAPSHOT_CHUNK_BYTES,
                       g_snapshot_baseline + offset,
                       SNAPSHOT_CHUNK_BYTES);
                ++changed;
            }
        }
        for (size_t index = 0; index < changed; ++index) {
            const size_t offset =
                (size_t)g_snapshot_scan_indices[index] *
                SNAPSHOT_CHUNK_BYTES;
            memcpy(g_snapshot_state + offset,
                   g_snapshot_scan + index * SNAPSHOT_CHUNK_BYTES,
                   SNAPSHOT_CHUNK_BYTES);
        }
    }
    g_sink ^= checksum;
    return checksum;
}

static void reset_all_state(void)
{
    g_rng = UINT64_C(0x6a09e667f3bcc909);
    init_motion();
    init_world_resolution();
    init_boxes();
    init_layouts();
    init_dispatch();
    init_snapshot();
}

static uint64_t logical_layout_hash_aos(void)
{
    uint64_t hash = 0;
    for (size_t i = 0; i < LAYOUT_ENTITIES; i += 257u) {
        hash = hash_mix(hash, float_bits(g_aos[i].x));
        hash = hash_mix(hash, float_bits(g_aos[i].y));
        hash = hash_mix(hash, g_aos[i].damage);
        hash = hash_mix(hash, g_aos[i].state);
        hash = hash_mix(hash, g_aos[i].animation_frame);
    }
    return hash;
}

static uint64_t logical_layout_hash_soa(void)
{
    uint64_t hash = 0;
    for (size_t i = 0; i < LAYOUT_ENTITIES; i += 257u) {
        hash = hash_mix(hash, float_bits(g_soa.x[i]));
        hash = hash_mix(hash, float_bits(g_soa.y[i]));
        hash = hash_mix(hash, g_soa.damage[i]);
        hash = hash_mix(hash, g_soa.state[i]);
        hash = hash_mix(hash, g_soa.animation_frame[i]);
    }
    return hash;
}

static uint64_t logical_layout_hash_hot(void)
{
    uint64_t hash = 0;
    for (size_t i = 0; i < LAYOUT_ENTITIES; i += 257u) {
        hash = hash_mix(hash, float_bits(g_hot[i].x));
        hash = hash_mix(hash, float_bits(g_hot[i].y));
        hash = hash_mix(hash, g_hot[i].damage);
        hash = hash_mix(hash, g_hot[i].state);
        hash = hash_mix(hash, g_hot[i].animation_frame);
    }
    return hash;
}

static bool motion_bounds_valid(void)
{
    for (size_t i = 0; i < NUM_ACTORS; ++i) {
        if (!isfinite(g_motion_f32[i].x) ||
            !isfinite(g_motion_f32[i].y) || g_motion_f32[i].x < 0.0f ||
            g_motion_f32[i].x > 255.0f || g_motion_f32[i].y < 0.0f ||
            g_motion_f32[i].y > 255.0f) {
            return false;
        }
        if (g_motion_f32[i].x_f32 < 0 ||
            g_motion_f32[i].x_f32 > (255 << 16) ||
            g_motion_f32[i].y_f32 < 0 ||
            g_motion_f32[i].y_f32 > (255 << 16)) {
            return false;
        }
        if (g_motion_hybrid[i].x_subcell < 0 ||
            g_motion_hybrid[i].x_subcell > 255 * 256 ||
            g_motion_hybrid[i].y_subcell < 0 ||
            g_motion_hybrid[i].y_subcell > 255 * 256) {
            return false;
        }
    }
    return true;
}

static bool self_test(void)
{
    reset_all_state();
    (void)bench_motion_f32(64u);
    (void)bench_motion_f32(64u);
    (void)bench_motion_cell256(64u);
    (void)bench_motion_hybrid(64u);
    if (!motion_bounds_valid()) {
        fprintf(stderr, "motion bounds self-test failed\n");
        return false;
    }

    const uint64_t naive_sparse = broadphase_naive(32u, SPARSE_TARGETS);
    const uint64_t sweep_sparse = broadphase_sweep(32u, SPARSE_TARGETS);
    const uint64_t grid_sparse = broadphase_grid(32u, SPARSE_TARGETS);
    const uint64_t bits_sparse =
        broadphase_bitboard(32u, SPARSE_TARGETS);
    if (naive_sparse != sweep_sparse || naive_sparse != grid_sparse ||
        naive_sparse != bits_sparse) {
        fprintf(stderr,
                "sparse broadphase mismatch: naive=%" PRIu64
                " sweep=%" PRIu64 " grid=%" PRIu64
                " bitboard=%" PRIu64 "\n",
                naive_sparse, sweep_sparse, grid_sparse, bits_sparse);
        return false;
    }

    const uint64_t naive_dense = broadphase_naive(32u, DENSE_TARGETS);
    const uint64_t sweep_dense = broadphase_sweep(32u, DENSE_TARGETS);
    const uint64_t grid_dense = broadphase_grid(32u, DENSE_TARGETS);
    const uint64_t bits_dense =
        broadphase_bitboard(32u, DENSE_TARGETS);
    if (naive_dense != sweep_dense || naive_dense != grid_dense ||
        naive_dense != bits_dense) {
        fprintf(stderr,
                "dense broadphase mismatch: naive=%" PRIu64
                " sweep=%" PRIu64 " grid=%" PRIu64
                " bitboard=%" PRIu64 "\n",
                naive_dense, sweep_dense, grid_dense, bits_dense);
        return false;
    }

    reset_all_state();
    (void)bench_layout_aos(3u);
    const uint64_t aos_hash = logical_layout_hash_aos();
    (void)bench_layout_soa(3u);
    const uint64_t soa_hash = logical_layout_hash_soa();
    (void)bench_layout_hot_cold(3u);
    const uint64_t hot_hash = logical_layout_hash_hot();
    if (aos_hash != soa_hash || aos_hash != hot_hash) {
        fprintf(stderr,
                "layout mismatch: aos=%" PRIu64 " soa=%" PRIu64
                " hot=%" PRIu64 "\n",
                aos_hash, soa_hash, hot_hash);
        return false;
    }

    reset_all_state();
    (void)bench_dispatch_switch(4u);
    (void)bench_dispatch_data_table(4u);
    (void)bench_dispatch_function_table(4u);
    for (size_t i = 0; i < DISPATCH_ENTITIES; ++i) {
        if (float_bits(g_dispatch_x_switch[i]) !=
                float_bits(g_dispatch_x_table[i]) ||
            float_bits(g_dispatch_x_switch[i]) !=
                float_bits(g_dispatch_x_function[i])) {
            fprintf(stderr, "dispatch mismatch at %zu\n", i);
            return false;
        }
    }

    reset_all_state();
    const uint64_t baseline_hash =
        hash_bytes(g_snapshot_state, SNAPSHOT_BYTES);
    const uint64_t full_checksum = bench_snapshot_full(32u);
    if (hash_bytes(g_snapshot_state, SNAPSHOT_BYTES) != baseline_hash) {
        fprintf(stderr, "full snapshot restore failed\n");
        return false;
    }
    const uint64_t dirty_checksum = bench_snapshot_tracked_dirty(32u);
    if (hash_bytes(g_snapshot_state, SNAPSHOT_BYTES) != baseline_hash) {
        fprintf(stderr, "tracked-dirty snapshot restore failed\n");
        return false;
    }
    const uint64_t scan_checksum = bench_snapshot_scan_delta(32u);
    if (hash_bytes(g_snapshot_state, SNAPSHOT_BYTES) != baseline_hash) {
        fprintf(stderr, "scan-delta snapshot restore failed\n");
        return false;
    }
    if (full_checksum != dirty_checksum || full_checksum != scan_checksum) {
        fprintf(stderr,
                "snapshot workload mismatch: full=%" PRIu64
                " dirty=%" PRIu64 " scan=%" PRIu64 "\n",
                full_checksum, dirty_checksum, scan_checksum);
        return false;
    }

    reset_all_state();
    return true;
}

static const BenchCase g_cases[] = {
    {"numeric_motion", "float32", "fighter_ticks", bench_motion_f32,
     NUM_ACTORS, sizeof(g_motion_f32)},
    {"numeric_motion", "fixed_f32_16", "fighter_ticks", bench_motion_f32,
     NUM_ACTORS, sizeof(g_motion_f32)},
    {"numeric_motion", "cell_256_int8", "fighter_ticks",
     bench_motion_cell256, NUM_ACTORS, sizeof(g_motion_cell)},
    {"numeric_motion", "hybrid_int_position_float_velocity",
     "fighter_ticks", bench_motion_hybrid, NUM_ACTORS,
     sizeof(g_motion_hybrid)},
    {"world_resolution", "world_256_u8", "fighter_ticks", bench_world_u8,
     NUM_ACTORS, sizeof(g_world_u8)},
    {"world_resolution", "world_4096_u16", "fighter_ticks",
     bench_world_u16, NUM_ACTORS, sizeof(g_world_u16)},
    {"broadphase_sparse", "naive", "attacker_queries",
     bench_broadphase_naive_sparse, MAX_ATTACKERS, 0u},
    {"broadphase_sparse", "sweep_rebuild", "attacker_queries",
     bench_broadphase_sweep_sparse, MAX_ATTACKERS, 0u},
    {"broadphase_sparse", "grid_16x16", "attacker_queries",
     bench_broadphase_grid_sparse, MAX_ATTACKERS,
     sizeof(uint64_t) * 16u * 16u},
    {"broadphase_sparse", "bitboard_256x256", "attacker_queries",
     bench_broadphase_bitboard_sparse, MAX_ATTACKERS,
     sizeof(uint64_t) * 256u * 4u},
    {"broadphase_dense", "naive", "attacker_queries",
     bench_broadphase_naive_dense, MAX_ATTACKERS, 0u},
    {"broadphase_dense", "sweep_rebuild", "attacker_queries",
     bench_broadphase_sweep_dense, MAX_ATTACKERS, 0u},
    {"broadphase_dense", "grid_16x16", "attacker_queries",
     bench_broadphase_grid_dense, MAX_ATTACKERS,
     sizeof(uint64_t) * 16u * 16u},
    {"broadphase_dense", "bitboard_256x256", "attacker_queries",
     bench_broadphase_bitboard_dense, MAX_ATTACKERS,
     sizeof(uint64_t) * 256u * 4u},
    {"layout_update", "aos_with_cold", "entity_updates", bench_layout_aos,
     LAYOUT_ENTITIES, sizeof(g_aos)},
    {"layout_update", "soa", "entity_updates", bench_layout_soa,
     LAYOUT_ENTITIES, sizeof(g_soa)},
    {"layout_update", "hot_cold_split", "entity_updates",
     bench_layout_hot_cold, LAYOUT_ENTITIES,
     sizeof(g_hot) + sizeof(g_cold)},
    {"state_dispatch", "switch", "entity_dispatches",
     bench_dispatch_switch, DISPATCH_ENTITIES,
     sizeof(g_dispatch_state) + sizeof(g_dispatch_x_switch) +
         sizeof(g_dispatch_v_switch)},
    {"state_dispatch", "data_table", "entity_dispatches",
     bench_dispatch_data_table, DISPATCH_ENTITIES,
     sizeof(g_dispatch_state) + sizeof(g_dispatch_x_table) +
         sizeof(g_dispatch_v_table)},
    {"state_dispatch", "function_table", "entity_dispatches",
     bench_dispatch_function_table, DISPATCH_ENTITIES,
     sizeof(g_dispatch_state) + sizeof(g_dispatch_x_function) +
         sizeof(g_dispatch_v_function)},
    {"snapshot_64k", "full_copy_restore", "snapshots",
     bench_snapshot_full, 1u, SNAPSHOT_BYTES},
    {"snapshot_64k", "tracked_dirty_8x64", "snapshots",
     bench_snapshot_tracked_dirty, 1u,
     DIRTY_CHUNKS * (SNAPSHOT_CHUNK_BYTES + sizeof(uint16_t))},
    {"snapshot_64k", "scan_delta_64byte_chunks", "snapshots",
     bench_snapshot_scan_delta, 1u, SNAPSHOT_BYTES}};

static uint64_t calibrate_iterations(BenchFn fn, uint64_t target_ns)
{
    uint64_t iterations = 1u;
    for (unsigned attempt = 0; attempt < 12u; ++attempt) {
        const uint64_t start = monotonic_ns();
        const uint64_t checksum = fn(iterations);
        const uint64_t elapsed = monotonic_ns() - start;
        g_sink ^= checksum;
        if (elapsed >= target_ns / 2u) {
            const double scale =
                (double)target_ns / (double)(elapsed == 0u ? 1u : elapsed);
            uint64_t adjusted =
                (uint64_t)((double)iterations * scale);
            if (adjusted < 1u) {
                adjusted = 1u;
            }
            return adjusted;
        }
        if (elapsed == 0u) {
            iterations *= 16u;
        } else {
            uint64_t scale = target_ns / elapsed;
            if (scale < 2u) {
                scale = 2u;
            } else if (scale > 16u) {
                scale = 16u;
            }
            if (iterations > UINT64_MAX / scale) {
                return iterations;
            }
            iterations *= scale;
        }
    }
    return iterations;
}

static void shuffle_indices(size_t *indices, size_t count)
{
    for (size_t i = count; i > 1u; --i) {
        const size_t j = (size_t)(xorshift64() % i);
        const size_t temporary = indices[i - 1u];
        indices[i - 1u] = indices[j];
        indices[j] = temporary;
    }
}

static void run_benchmarks(unsigned rounds, uint64_t target_ns)
{
    uint64_t calibrated[ARRAY_COUNT(g_cases)];
    size_t order[ARRAY_COUNT(g_cases)];

    for (size_t i = 0; i < ARRAY_COUNT(g_cases); ++i) {
        reset_all_state();
        calibrated[i] = calibrate_iterations(g_cases[i].fn, target_ns);
        order[i] = i;
    }

    puts("family,candidate,round,iterations,work_items,elapsed_ns,"
         "items_per_second,checksum,state_bytes,unit");
    for (unsigned round = 0; round < rounds; ++round) {
        shuffle_indices(order, ARRAY_COUNT(order));
        for (size_t position = 0; position < ARRAY_COUNT(order);
             ++position) {
            const size_t index = order[position];
            const BenchCase *bench = &g_cases[index];
            const uint64_t iterations = calibrated[index];
            reset_all_state();
            const uint64_t start = monotonic_ns();
            const uint64_t checksum = bench->fn(iterations);
            const uint64_t elapsed = monotonic_ns() - start;
            const uint64_t work_items =
                iterations * bench->work_items_per_iteration;
            const double items_per_second =
                elapsed == 0u
                    ? 0.0
                    : (double)work_items * 1000000000.0 /
                          (double)elapsed;
            printf("%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64
                   ",%.3f,%" PRIu64 ",%zu,%s\n",
                   bench->family, bench->candidate, round, iterations,
                   work_items, elapsed, items_per_second, checksum,
                   bench->state_bytes, bench->unit);
        }
    }
}

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s [--mode smoke|commit|milestone]\n", program);
}

int main(int argc, char **argv)
{
    const char *mode = "commit";
    if (argc == 3 && strcmp(argv[1], "--mode") == 0) {
        mode = argv[2];
    } else if (argc != 1) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    unsigned rounds;
    uint64_t target_ns;
    if (strcmp(mode, "smoke") == 0) {
        rounds = 1u;
        target_ns = UINT64_C(1000000);
    } else if (strcmp(mode, "commit") == 0) {
        rounds = 5u;
        target_ns = UINT64_C(20000000);
    } else if (strcmp(mode, "milestone") == 0) {
        rounds = 15u;
        target_ns = UINT64_C(100000000);
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!self_test()) {
        return EXIT_FAILURE;
    }
    fprintf(stderr, "self-test=pass cases=%zu mode=%s sink=%" PRIu64 "\n",
            ARRAY_COUNT(g_cases), mode, g_sink);
    run_benchmarks(rounds, target_ns);
    fprintf(stderr, "benchmark=complete sink=%" PRIu64 "\n", g_sink);
    return EXIT_SUCCESS;
}
