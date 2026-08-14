#ifndef M0_MOVEMENT_MODEL_H
#define M0_MOVEMENT_MODEL_H

#include <stdint.h>

enum
{
    M0_AXIS_MIN = -32767,
    M0_AXIS_MAX = 32767
};

typedef struct M0MovementInput
{
    int16_t move_x;
    uint8_t jump_pressed;
    uint8_t jump_held;
    uint8_t down_held;
} M0MovementInput;

typedef struct M0Motion
{
    float x;
    float y;
    float velocity_x;
    float velocity_y;
    uint32_t tick;
    uint32_t respawns;
    uint16_t jump_squat;
    uint8_t grounded;
    uint8_t on_platform;
    uint8_t air_jumps;
    uint8_t short_hop_latched;
    uint8_t platform_drop_ticks;
    uint8_t dash_ticks;
    int8_t dash_direction;
    int8_t previous_strong_direction;
} M0Motion;

typedef struct M0MovementPair
{
    M0Motion primary;
    M0Motion repeat;
} M0MovementPair;

typedef struct M0MovementView
{
    float x;
    float y;
    float velocity_x;
    float velocity_y;
    uint32_t tick;
    uint32_t respawns;
    uint16_t jump_squat;
    uint8_t grounded;
    uint8_t on_platform;
    uint8_t air_jumps;
    uint8_t dash_ticks;
} M0MovementView;

typedef struct M0StageGeometry
{
    float floor_left;
    float floor_right;
    float floor_y;
    float platform_left;
    float platform_right;
    float platform_y;
    float fighter_half_width;
    float fighter_half_height;
    float blast_left;
    float blast_right;
    float blast_bottom;
} M0StageGeometry;

void m0_reset(M0Motion *state);
void m0_pair_reset(M0MovementPair *pair);

void m0_step(M0Motion *state, M0MovementInput input);
void m0_pair_step(M0MovementPair *pair, M0MovementInput input);

M0MovementView m0_view(const M0Motion *state);
const M0StageGeometry *m0_stage_geometry(void);

uint64_t m0_hash(const M0Motion *state);
int16_t m0_axis_clamp(int value);

#endif
