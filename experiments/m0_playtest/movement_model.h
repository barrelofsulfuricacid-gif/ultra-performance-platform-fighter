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

typedef struct M0FloatMotion
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
    uint8_t short_hop_cut;
    uint8_t platform_drop_ticks;
} M0FloatMotion;

typedef struct M0FixedMotion
{
    int32_t x;
    int32_t y;
    int32_t velocity_x;
    int32_t velocity_y;
    uint32_t tick;
    uint32_t respawns;
    uint16_t jump_squat;
    uint8_t grounded;
    uint8_t on_platform;
    uint8_t air_jumps;
    uint8_t short_hop_cut;
    uint8_t platform_drop_ticks;
} M0FixedMotion;

typedef struct M0MovementPair
{
    M0FloatMotion float32;
    M0FixedMotion q16_16;
} M0MovementPair;

typedef struct M0MovementView
{
    double x;
    double y;
    double velocity_x;
    double velocity_y;
    uint32_t tick;
    uint32_t respawns;
    uint16_t jump_squat;
    uint8_t grounded;
    uint8_t on_platform;
    uint8_t air_jumps;
} M0MovementView;

typedef struct M0StageGeometry
{
    double floor_left;
    double floor_right;
    double floor_y;
    double platform_left;
    double platform_right;
    double platform_y;
    double fighter_half_width;
    double fighter_half_height;
    double blast_left;
    double blast_right;
    double blast_bottom;
} M0StageGeometry;

void m0_float_reset(M0FloatMotion *state);
void m0_fixed_reset(M0FixedMotion *state);
void m0_pair_reset(M0MovementPair *pair);

void m0_float_step(M0FloatMotion *state, M0MovementInput input);
void m0_fixed_step(M0FixedMotion *state, M0MovementInput input);
void m0_pair_step(M0MovementPair *pair, M0MovementInput input);

M0MovementView m0_float_view(const M0FloatMotion *state);
M0MovementView m0_fixed_view(const M0FixedMotion *state);
const M0StageGeometry *m0_stage_geometry(void);

uint64_t m0_float_hash(const M0FloatMotion *state);
uint64_t m0_fixed_hash(const M0FixedMotion *state);
int16_t m0_axis_clamp(int value);

#endif
