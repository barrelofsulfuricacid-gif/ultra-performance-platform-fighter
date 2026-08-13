#include "playtest.h"

#include "pf/m4.h"
#include "pf/sim.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#if !defined(__EMSCRIPTEN__)
#include <stdio.h>
#endif

#define PF_WEB_M4_MEMORY_BYTES 4096U
#define PF_WEB_M4_MEMORY_ALIGNMENT 64U
#define PF_WEB_M4_DUEL_PLAYER_COUNT UINT8_C(2)
#define PF_WEB_M4_TEAM_PLAYER_COUNT UINT8_C(4)
#define PF_WEB_M4_MAX_SETUP_STOCKS UINT8_C(4)
#define PF_WEB_M4_WALK_AXIS INT16_C(13500)
#define PF_WEB_M4_DASH_AXIS INT16_C(32767)
#define PF_WEB_M4_MAX_TICKS UINT64_C(1728000)
#define PF_WEB_M4_RESET_SEED UINT64_C(0x4d34504c41595445)
#define PF_WEB_M4_CAMPING_MINIMUM_SEPARATION_F32 INT32_C(693712)
#define PF_WEB_M4_VIEW_PLAYER_STRIDE 53
#define PF_WEB_M4_VIEW_PLAYER0 25
#define PF_WEB_M4_VIEW_EVENT_STRIDE 10
#define PF_WEB_M4_VIEW_EVENT0 237
#define PF_WEB_M4_VIEW_ITEM0 397
#define PF_WEB_M4_VIEW_PROJECTILE0 415
#define PF_WEB_M4_VIEW_RECOVERY0 427
#define PF_WEB_M4_VIEW_REVIVAL0 431
#define PF_WEB_M4_VIEW_REVIVAL_STRIDE 4
#define PF_WEB_M4_VIEW_STALE_MOVE0 447
#define PF_WEB_M4_VIEW_STALE_MOVE_STRIDE 12
#define PF_WEB_M4_VIEW_ITEM_STALE_REGISTERED 495
#define PF_WEB_M4_VIEW_UPPER_PLATFORM0 496
#define PF_WEB_M4_VIEW_PRONE_ORIENTATION0 499
#define PF_WEB_M4_VIEW_HIT_SPHERE0 503
#define PF_WEB_M4_VIEW_HIT_SPHERE_PLAYER_STRIDE 25
#define PF_WEB_M4_VIEW_HIT_SPHERE_STRIDE 6
#define PF_WEB_M4_VIEW_COUNT 603

enum pf_web_view_field
{
    PF_WEB_M4_VIEW_SCHEMA = 0,
    PF_WEB_M4_VIEW_TICK = 1,
    PF_WEB_M4_VIEW_FLOOR_LEFT = 2,
    PF_WEB_M4_VIEW_FLOOR_RIGHT = 3,
    PF_WEB_M4_VIEW_FLOOR_Y = 4,
    PF_WEB_M4_VIEW_PLATFORM_LEFT = 5,
    PF_WEB_M4_VIEW_PLATFORM_RIGHT = 6,
    PF_WEB_M4_VIEW_PLATFORM_Y = 7,
    PF_WEB_M4_VIEW_BLAST_LEFT = 8,
    PF_WEB_M4_VIEW_BLAST_RIGHT = 9,
    PF_WEB_M4_VIEW_BLAST_TOP = 10,
    PF_WEB_M4_VIEW_BLAST_BOTTOM = 11,
    PF_WEB_M4_VIEW_FIGHTER_HALF_WIDTH = 12,
    PF_WEB_M4_VIEW_FIGHTER_HALF_HEIGHT = 13,
    PF_WEB_M4_VIEW_SOLID_LEFT = 14,
    PF_WEB_M4_VIEW_SOLID_RIGHT = 15,
    PF_WEB_M4_VIEW_SOLID_TOP = 16,
    PF_WEB_M4_VIEW_SOLID_BOTTOM = 17,
    PF_WEB_M4_VIEW_STOCK_COUNT = 18,
    PF_WEB_M4_VIEW_RESPAWN_DELAY = 19,
    PF_WEB_M4_VIEW_RESPAWN_INVULNERABILITY = 20,
    PF_WEB_M4_VIEW_SUDDEN_DEATH = 21,
    PF_WEB_M4_VIEW_TERMINATED = 22,
    PF_WEB_M4_VIEW_TRUNCATED = 23,
    PF_WEB_M4_VIEW_WINNER_MASK = 24,
    PF_WEB_M4_VIEW_EVENT_COUNT = 236,
    PF_WEB_M4_VIEW_PLAYER_X = 0,
    PF_WEB_M4_VIEW_PLAYER_Y = 1,
    PF_WEB_M4_VIEW_PLAYER_VX = 2,
    PF_WEB_M4_VIEW_PLAYER_VY = 3,
    PF_WEB_M4_VIEW_PLAYER_ACTION = 4,
    PF_WEB_M4_VIEW_PLAYER_FACING = 5,
    PF_WEB_M4_VIEW_PLAYER_GROUNDED = 6,
    PF_WEB_M4_VIEW_PLAYER_SUPPORT = 7,
    PF_WEB_M4_VIEW_PLAYER_AIR_JUMPS = 8,
    PF_WEB_M4_VIEW_PLAYER_FAST_FALL = 9,
    PF_WEB_M4_VIEW_PLAYER_RESPAWNS = 10,
    PF_WEB_M4_VIEW_PLAYER_DAMAGE = 11,
    PF_WEB_M4_VIEW_PLAYER_HITLAG = 12,
    PF_WEB_M4_VIEW_PLAYER_HITSTUN = 13,
    PF_WEB_M4_VIEW_PLAYER_HITBOX_ACTIVE = 14,
    PF_WEB_M4_VIEW_PLAYER_HITBOX_LEFT = 15,
    PF_WEB_M4_VIEW_PLAYER_HITBOX_RIGHT = 16,
    PF_WEB_M4_VIEW_PLAYER_HITBOX_TOP = 17,
    PF_WEB_M4_VIEW_PLAYER_HITBOX_BOTTOM = 18,
    PF_WEB_M4_VIEW_PLAYER_LAST_HIT_SEQUENCE = 19,
    PF_WEB_M4_VIEW_PLAYER_TECH_WINDOW = 20,
    PF_WEB_M4_VIEW_PLAYER_TECH_LOCKOUT = 21,
    PF_WEB_M4_VIEW_PLAYER_TUMBLE = 22,
    PF_WEB_M4_VIEW_PLAYER_SDI_PULSE_COUNT = 23,
    PF_WEB_M4_VIEW_PLAYER_TECH_DIRECTION = 24,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_HEALTH = 25,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_STUN = 26,
    PF_WEB_M4_VIEW_PLAYER_POWERSHIELD = 27,
    PF_WEB_M4_VIEW_PLAYER_INVULNERABLE = 28,
    PF_WEB_M4_VIEW_PLAYER_ACTION_TICKS = 29,
    PF_WEB_M4_VIEW_PLAYER_TRIGGER_INPUT_AGE = 30,
    PF_WEB_M4_VIEW_PLAYER_L_CANCEL_ELIGIBLE = 31,
    PF_WEB_M4_VIEW_PLAYER_STOCKS = 32,
    PF_WEB_M4_VIEW_PLAYER_RESPAWN_TICKS = 33,
    PF_WEB_M4_VIEW_PLAYER_RESPAWN_INVULNERABILITY = 34,
    PF_WEB_M4_VIEW_PLAYER_GRABBOX_ACTIVE = 35,
    PF_WEB_M4_VIEW_PLAYER_GRABBOX_LEFT = 36,
    PF_WEB_M4_VIEW_PLAYER_GRABBOX_RIGHT = 37,
    PF_WEB_M4_VIEW_PLAYER_GRABBOX_TOP = 38,
    PF_WEB_M4_VIEW_PLAYER_GRABBOX_BOTTOM = 39,
    PF_WEB_M4_VIEW_PLAYER_GRAB_ESCAPE_TICKS = 40,
    PF_WEB_M4_VIEW_PLAYER_GRAB_TARGET = 41,
    PF_WEB_M4_VIEW_PLAYER_GRAB_OWNER = 42,
    PF_WEB_M4_VIEW_PLAYER_CHARGE_TICKS = 43,
    PF_WEB_M4_VIEW_PLAYER_SMASH_CHARGE_TICKS = 44,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_STRENGTH = 45,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_ACTIVE = 46,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_LEFT = 47,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_RIGHT = 48,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_TOP = 49,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_BOTTOM = 50,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_TILT_X = 51,
    PF_WEB_M4_VIEW_PLAYER_SHIELD_TILT_Y = 52,
    PF_WEB_M4_VIEW_EVENT_SEQUENCE = 0,
    PF_WEB_M4_VIEW_EVENT_TICK = 1,
    PF_WEB_M4_VIEW_EVENT_TYPE = 2,
    PF_WEB_M4_VIEW_EVENT_SOURCE = 3,
    PF_WEB_M4_VIEW_EVENT_TARGET = 4,
    PF_WEB_M4_VIEW_EVENT_VALUE = 5,
    PF_WEB_M4_VIEW_EVENT_VELOCITY_X = 6,
    PF_WEB_M4_VIEW_EVENT_VELOCITY_Y = 7,
    PF_WEB_M4_VIEW_EVENT_FLAGS = 8,
    PF_WEB_M4_VIEW_EVENT_DETAIL = 9,
    PF_WEB_M4_VIEW_ITEM_ENABLED = 0,
    PF_WEB_M4_VIEW_ITEM_STATE = 1,
    PF_WEB_M4_VIEW_ITEM_HOLDER = 2,
    PF_WEB_M4_VIEW_ITEM_SOURCE = 3,
    PF_WEB_M4_VIEW_ITEM_THROW_DIRECTION = 4,
    PF_WEB_M4_VIEW_ITEM_HITBOX_ACTIVE = 5,
    PF_WEB_M4_VIEW_ITEM_X = 6,
    PF_WEB_M4_VIEW_ITEM_Y = 7,
    PF_WEB_M4_VIEW_ITEM_VX = 8,
    PF_WEB_M4_VIEW_ITEM_VY = 9,
    PF_WEB_M4_VIEW_ITEM_LIFETIME = 10,
    PF_WEB_M4_VIEW_ITEM_RESPAWN = 11,
    PF_WEB_M4_VIEW_ITEM_PICKUP_LOCKOUT = 12,
    PF_WEB_M4_VIEW_ITEM_HIT_MASK = 13,
    PF_WEB_M4_VIEW_ITEM_HALF_WIDTH = 14,
    PF_WEB_M4_VIEW_ITEM_HALF_HEIGHT = 15,
    PF_WEB_M4_VIEW_ITEM_HITBOX_HALF_WIDTH = 16,
    PF_WEB_M4_VIEW_ITEM_HITBOX_HALF_HEIGHT = 17,
    PF_WEB_M4_VIEW_PROJECTILE_ENABLED = 0,
    PF_WEB_M4_VIEW_PROJECTILE_STATE = 1,
    PF_WEB_M4_VIEW_PROJECTILE_OWNER = 2,
    PF_WEB_M4_VIEW_PROJECTILE_HITBOX_ACTIVE = 3,
    PF_WEB_M4_VIEW_PROJECTILE_X = 4,
    PF_WEB_M4_VIEW_PROJECTILE_Y = 5,
    PF_WEB_M4_VIEW_PROJECTILE_VX = 6,
    PF_WEB_M4_VIEW_PROJECTILE_VY = 7,
    PF_WEB_M4_VIEW_PROJECTILE_LIFETIME = 8,
    PF_WEB_M4_VIEW_PROJECTILE_HALF_WIDTH = 9,
    PF_WEB_M4_VIEW_PROJECTILE_HALF_HEIGHT = 10,
    PF_WEB_M4_VIEW_PROJECTILE_REFLECT_WINDOW = 11,
    PF_WEB_M4_VIEW_REVIVAL_ACTIVE = 0,
    PF_WEB_M4_VIEW_REVIVAL_LEFT = 1,
    PF_WEB_M4_VIEW_REVIVAL_RIGHT = 2,
    PF_WEB_M4_VIEW_REVIVAL_Y = 3,
    PF_WEB_M4_VIEW_STALE_MOVE_COUNT = 0,
    PF_WEB_M4_VIEW_STALE_MOVE_MULTIPLIER = 1,
    PF_WEB_M4_VIEW_STALE_MOVE_REGISTERED = 2,
    PF_WEB_M4_VIEW_STALE_MOVE_IDS = 3,
    PF_WEB_M4_VIEW_UPPER_PLATFORM_LEFT = 0,
    PF_WEB_M4_VIEW_UPPER_PLATFORM_RIGHT = 1,
    PF_WEB_M4_VIEW_UPPER_PLATFORM_Y = 2
};


typedef struct pf_web_storage
{
    alignas(PF_WEB_M4_MEMORY_ALIGNMENT)
        uint8_t state[PF_WEB_M4_MEMORY_BYTES];
    alignas(PF_WEB_M4_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_WEB_M4_MEMORY_BYTES];
} pf_web_storage;

extern void pf_web_playtest_install(
    int walk_axis,
    int dash_axis,
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks);
extern void pf_web_playtest_render(
    const float *view,
    int view_count);
#if defined(PF_WEB_M4_TEST)
extern void pf_web_playtest_observe_inputs(
    const pf_input_frame *inputs,
    size_t input_count);
int pf_web_playtest_test_render_dynamic_player(
    const player_inspection *player,
    int player_index);
#endif

static pf_web_storage pf_web_sim_storage;
static struct content pf_web_content;
static pf_sim *pf_web_sim;
static pf_tick_result pf_web_last_result;
static float pf_web_view[PF_WEB_M4_VIEW_COUNT];
static uint8_t pf_web_player_count = PF_WEB_M4_DUEL_PLAYER_COUNT;
static uint8_t pf_web_team_lab_active;
static uint8_t pf_web_stock_count = PF_SIM_DEFAULT_STOCK_COUNT;

static int pf_web_initialize_content(
    uint8_t player_count,
    pf_sim_mode mode)
{
    pf_content_view content_view;
    pf_memory_requirements requirements;
    pf_sim_config config;

    if (make_content_view(
            &pf_web_content,
            &content_view) != PF_STATUS_OK ||
        pf_sim_default_config(
            &config,
            player_count,
            mode) != PF_STATUS_OK)
    {
        return 0;
    }
    config.max_ticks = PF_WEB_M4_MAX_TICKS;
    config.stock_count = pf_web_stock_count;
    if (pf_sim_query_memory(&config, &requirements) != PF_STATUS_OK ||
        requirements.state_bytes >
            sizeof(pf_web_sim_storage.state) ||
        requirements.scratch_bytes >
            sizeof(pf_web_sim_storage.scratch) ||
        requirements.state_alignment >
            PF_WEB_M4_MEMORY_ALIGNMENT ||
        requirements.scratch_alignment >
            PF_WEB_M4_MEMORY_ALIGNMENT ||
        pf_sim_init(
            pf_web_sim_storage.state,
            sizeof(pf_web_sim_storage.state),
            pf_web_sim_storage.scratch,
            sizeof(pf_web_sim_storage.scratch),
            &content_view,
            &config,
            &pf_web_sim) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_player_count = player_count;
    pf_web_team_lab_active =
        mode == PF_SIM_MODE_TEAMS ? UINT8_C(1) : UINT8_C(0);
    return 1;
}

static int pf_web_initialize_current_content(void)
{
    return pf_web_initialize_content(
        PF_WEB_M4_DUEL_PLAYER_COUNT,
        PF_SIM_MODE_DUEL);
}

static void pf_web_make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    const struct inspection *before,
    uint64_t tick,
    int16_t player0_x,
    int16_t player0_y,
    int16_t player0_secondary_x,
    int16_t player0_secondary_y,
    uint64_t player0_buttons,
    uint16_t player0_left_trigger,
    uint16_t player0_right_trigger,
    int16_t player1_x,
    int16_t player1_y,
    int16_t player1_secondary_x,
    int16_t player1_secondary_y,
    uint64_t player1_buttons,
    uint16_t player1_left_trigger,
    uint16_t player1_right_trigger)
{
    uint32_t player_index;

    (void)memset(
        inputs,
        0,
        sizeof(*inputs) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)pf_web_player_count;
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }
    inputs[0].main_stick_x = player0_x;
    inputs[0].main_stick_y = player0_y;
    inputs[0].secondary_stick_x = player0_secondary_x;
    inputs[0].secondary_stick_y = player0_secondary_y;
    inputs[0].buttons = player0_buttons;
    inputs[0].left_trigger = player0_left_trigger;
    inputs[0].right_trigger = player0_right_trigger;
    if (pf_web_team_lab_active != UINT8_C(0))
    {
        inputs[2].main_stick_x = player1_x;
        inputs[2].main_stick_y = player1_y;
        inputs[2].secondary_stick_x = player1_secondary_x;
        inputs[2].secondary_stick_y = player1_secondary_y;
        inputs[2].buttons = player1_buttons;
        inputs[2].left_trigger = player1_left_trigger;
        inputs[2].right_trigger = player1_right_trigger;
        if (before != NULL &&
            before->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GRABBED &&
            (tick & UINT64_C(1)) == UINT64_C(0))
        {
            inputs[1].buttons = PF_INPUT_BUTTON_JUMP;
        }
    }
    else
    {
        inputs[1].main_stick_x = player1_x;
        inputs[1].main_stick_y = player1_y;
        inputs[1].secondary_stick_x = player1_secondary_x;
        inputs[1].secondary_stick_y = player1_secondary_y;
        inputs[1].buttons = player1_buttons;
        inputs[1].left_trigger = player1_left_trigger;
        inputs[1].right_trigger = player1_right_trigger;
    }
}

static int pf_web_tick_with_dual_triggers(
    int16_t player0_x,
    int16_t player0_y,
    int16_t player0_secondary_x,
    int16_t player0_secondary_y,
    uint64_t player0_buttons,
    uint16_t player0_left_trigger,
    uint16_t player0_right_trigger,
    int16_t player1_x,
    int16_t player1_y,
    int16_t player1_secondary_x,
    int16_t player1_secondary_y,
    uint64_t player1_buttons,
    uint16_t player1_left_trigger,
    uint16_t player1_right_trigger,
    struct inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    struct inspection before;

    if (pf_web_sim == NULL ||
        inspect(pf_web_sim, &before) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_make_inputs(
        inputs,
        &before,
        before.tick,
        player0_x,
        player0_y,
        player0_secondary_x,
        player0_secondary_y,
        player0_buttons,
        player0_left_trigger,
        player0_right_trigger,
        player1_x,
        player1_y,
        player1_secondary_x,
        player1_secondary_y,
        player1_buttons,
        player1_left_trigger,
        player1_right_trigger);
#if defined(PF_WEB_M4_TEST)
    pf_web_playtest_observe_inputs(
        inputs,
        (size_t)pf_web_player_count);
#endif
    if (pf_sim_tick(
            pf_web_sim,
            inputs,
            (size_t)pf_web_player_count,
            &result) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_last_result = result;
    return inspect(pf_web_sim, out_inspection) ==
           PF_STATUS_OK;
}

static int pf_web_reset_internal(void)
{
    if (pf_web_sim == NULL ||
        pf_sim_reset(
            pf_web_sim,
            PF_WEB_M4_RESET_SEED) != PF_STATUS_OK)
    {
        return 0;
    }
    (void)memset(
        &pf_web_last_result,
        0,
        sizeof(pf_web_last_result));
    return 1;
}

static int pf_web_initialize_team_wobble_lab(void)
{
    if (default_content(&pf_web_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_content.stage.spawn_spacing_f32 =
        (INT32_C(2) * 1.0f) / INT32_C(5);
    pf_web_content.stage.platform_center_x_f32 =
        -INT32_C(20) * 1.0f;
    pf_web_content.stage.platform_motion_amplitude_f32 = INT32_C(0);
    pf_web_content.item.enabled = UINT8_C(0);
    return pf_web_initialize_content(
               PF_WEB_M4_TEAM_PLAYER_COUNT,
               PF_SIM_MODE_TEAMS) &&
           pf_web_reset_internal();
}

static int pf_web_initialize_live_item_lab(void)
{
    if (default_content(&pf_web_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_content.item.enabled = UINT8_C(1);
    pf_web_content.item.lifetime_ticks = UINT16_C(3600);
    pf_web_content.projectile.enabled = UINT8_C(1);
    pf_web_content.reflector.enabled = UINT8_C(1);
    pf_web_content.charge.enabled = UINT8_C(1);
    pf_web_content.recovery.enabled = UINT8_C(1);
    return pf_web_initialize_current_content() &&
           pf_web_reset_internal();
}

static void pf_web_pack_dynamic_player_view(
    const player_inspection *player,
    int base,
    int hit_sphere_base)
{
    uint8_t hit_sphere_index;

    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_ACTIVE] =
        (float)player->hitbox_active;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_LEFT] =
        player->hitbox_left_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_RIGHT] =
        player->hitbox_right_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_TOP] =
        player->hitbox_top_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_BOTTOM] =
        player->hitbox_bottom_f32;
    pf_web_view[hit_sphere_base] =
        (float)player->hit_sphere_count;
    for (hit_sphere_index = UINT8_C(0);
         hit_sphere_index < player->hit_sphere_count;
         ++hit_sphere_index)
    {
        const hit_sphere_inspection *sphere =
            &player->hit_spheres[hit_sphere_index];
        const int sphere_base =
            hit_sphere_base + INT32_C(1) +
            (int)hit_sphere_index * PF_WEB_M4_VIEW_HIT_SPHERE_STRIDE;

        pf_web_view[sphere_base] = sphere->center_x_f32;
        pf_web_view[sphere_base + 1] = sphere->center_y_f32;
        pf_web_view[sphere_base + 2] = sphere->radius_f32;
        pf_web_view[sphere_base + 3] =
            (float)sphere->effect_index;
        pf_web_view[sphere_base + 4] =
            (float)sphere->hitbox_id;
        pf_web_view[sphere_base + 5] =
            (float)sphere->group_id;
    }
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_STRENGTH] =
        (float)player->shield_strength;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_ACTIVE] =
        (float)player->shield_active;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_LEFT] =
        player->shield_left_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_RIGHT] =
        player->shield_right_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_TOP] =
        player->shield_top_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_BOTTOM] =
        player->shield_bottom_f32;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_TILT_X] =
        (float)player->shield_tilt_x;
    pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_TILT_Y] =
        (float)player->shield_tilt_y;
}

#if defined(PF_WEB_M4_TEST)
int pf_web_playtest_test_render_dynamic_player(
    const player_inspection *player,
    int player_index)
{
    int base;
    int hit_sphere_base;

    if (player == NULL || player_index < 0 ||
        player_index >= (int)PF_SIM_MAX_PLAYERS ||
        player->hit_sphere_count >
            (uint8_t)PF_M4_INSPECTION_HIT_SPHERE_CAPACITY)
    {
        return 0;
    }
    base =
        PF_WEB_M4_VIEW_PLAYER0 +
        player_index * PF_WEB_M4_VIEW_PLAYER_STRIDE;
    hit_sphere_base =
        PF_WEB_M4_VIEW_HIT_SPHERE0 +
        player_index * PF_WEB_M4_VIEW_HIT_SPHERE_PLAYER_STRIDE;
    (void)memset(pf_web_view, 0, sizeof(pf_web_view));
    pf_web_pack_dynamic_player_view(
        player,
        base,
        hit_sphere_base);
    pf_web_playtest_render(
        pf_web_view,
        PF_WEB_M4_VIEW_COUNT);
    return 1;
}
#endif

static int pf_web_render(void)
{
    struct inspection inspection;
    uint32_t event_index;
    uint32_t player_index;

    if (inspect(pf_web_sim, &inspection) != PF_STATUS_OK ||
        inspection.tick > (uint64_t)INT32_MAX)
    {
        return 0;
    }

    (void)memset(pf_web_view, 0, sizeof(pf_web_view));
    pf_web_view[PF_WEB_M4_VIEW_SCHEMA] = 49.0f;
    pf_web_view[PF_WEB_M4_VIEW_TICK] =
        (float)inspection.tick;
    pf_web_view[PF_WEB_M4_VIEW_FLOOR_LEFT] =
        inspection.stage.floor_left_f32;
    pf_web_view[PF_WEB_M4_VIEW_FLOOR_RIGHT] =
        inspection.stage.floor_right_f32;
    pf_web_view[PF_WEB_M4_VIEW_FLOOR_Y] =
        inspection.stage.floor_y_f32;
    pf_web_view[PF_WEB_M4_VIEW_PLATFORM_LEFT] =
        inspection.stage.platform_left_f32;
    pf_web_view[PF_WEB_M4_VIEW_PLATFORM_RIGHT] =
        inspection.stage.platform_right_f32;
    pf_web_view[PF_WEB_M4_VIEW_PLATFORM_Y] =
        inspection.stage.platform_y_f32;
    pf_web_view[PF_WEB_M4_VIEW_BLAST_LEFT] =
        inspection.stage.blast_left_f32;
    pf_web_view[PF_WEB_M4_VIEW_BLAST_RIGHT] =
        inspection.stage.blast_right_f32;
    pf_web_view[PF_WEB_M4_VIEW_BLAST_TOP] =
        inspection.stage.blast_top_f32;
    pf_web_view[PF_WEB_M4_VIEW_BLAST_BOTTOM] =
        inspection.stage.blast_bottom_f32;
    pf_web_view[PF_WEB_M4_VIEW_FIGHTER_HALF_WIDTH] =
        pf_web_content.fighter.half_width_f32;
    pf_web_view[PF_WEB_M4_VIEW_FIGHTER_HALF_HEIGHT] =
        pf_web_content.fighter.half_height_f32;
    pf_web_view[PF_WEB_M4_VIEW_SOLID_LEFT] =
        inspection.stage.solid_left_f32;
    pf_web_view[PF_WEB_M4_VIEW_SOLID_RIGHT] =
        inspection.stage.solid_right_f32;
    pf_web_view[PF_WEB_M4_VIEW_SOLID_TOP] =
        inspection.stage.solid_top_f32;
    pf_web_view[PF_WEB_M4_VIEW_SOLID_BOTTOM] =
        inspection.stage.solid_bottom_f32;
    pf_web_view[PF_WEB_M4_VIEW_STOCK_COUNT] =
        (float)inspection.stock_count;
    pf_web_view[PF_WEB_M4_VIEW_RESPAWN_DELAY] =
        (float)inspection.respawn_delay_ticks;
    pf_web_view[PF_WEB_M4_VIEW_RESPAWN_INVULNERABILITY] =
        (float)inspection.respawn_invulnerability_ticks;
    pf_web_view[PF_WEB_M4_VIEW_SUDDEN_DEATH] =
        (float)inspection.sudden_death;
    pf_web_view[PF_WEB_M4_VIEW_TERMINATED] =
        (float)inspection.terminated;
    pf_web_view[PF_WEB_M4_VIEW_TRUNCATED] =
        (float)inspection.truncated;
    pf_web_view[PF_WEB_M4_VIEW_WINNER_MASK] =
        (float)inspection.winner_mask;

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)pf_web_player_count;
         ++player_index)
    {
        const player_inspection *player =
            &inspection.players[player_index];
        const int base =
            PF_WEB_M4_VIEW_PLAYER0 +
            (int)player_index * PF_WEB_M4_VIEW_PLAYER_STRIDE;
        const int hit_sphere_base =
            PF_WEB_M4_VIEW_HIT_SPHERE0 +
            (int)player_index * PF_WEB_M4_VIEW_HIT_SPHERE_PLAYER_STRIDE;

        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_X] =
            player->position_x_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_Y] =
            player->position_y_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_VX] =
            player->velocity_x_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_VY] =
            player->velocity_y_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_ACTION] =
            (float)player->action_state;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_FACING] =
            (float)player->facing;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GROUNDED] =
            (float)player->grounded;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SUPPORT] =
            (float)player->support;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_AIR_JUMPS] =
            (float)player->air_jumps_remaining;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_FAST_FALL] =
            (float)player->fast_fall;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_RESPAWNS] =
            (float)player->respawn_count;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_DAMAGE] =
            (float)player->damage_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITLAG] =
            (float)player->hitlag_ticks;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_HITSTUN] =
            (float)player->hitstun_ticks;
        pf_web_pack_dynamic_player_view(
            player,
            base,
            hit_sphere_base);
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_LAST_HIT_SEQUENCE] =
            (float)player->last_hit_sequence;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_TECH_WINDOW] =
            (float)player->tech_window_ticks;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_TECH_LOCKOUT] =
            (float)player->tech_lockout_ticks;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_TUMBLE] =
            (float)player->tumble;
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_SDI_PULSE_COUNT] =
            (float)player->sdi_pulse_count;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_TECH_DIRECTION] =
            (float)player->tech_direction;
        pf_web_view[
            PF_WEB_M4_VIEW_PRONE_ORIENTATION0 + (int)player_index] =
            (float)player->prone_orientation;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_HEALTH] =
            (float)player->shield_health_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_STUN] =
            (float)player->shield_stun_ticks;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_POWERSHIELD] =
            (float)player->powershield;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_INVULNERABLE] =
            (float)player->invulnerable;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_ACTION_TICKS] =
            (float)player->action_ticks;
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_TRIGGER_INPUT_AGE] =
            (float)player->trigger_input_age;
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_L_CANCEL_ELIGIBLE] =
            (float)player->l_cancel_eligible;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_STOCKS] =
            (float)player->stocks_remaining;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_RESPAWN_TICKS] =
            (float)player->respawn_ticks;
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_RESPAWN_INVULNERABILITY] =
            (float)player->respawn_invulnerability_ticks;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_ACTIVE] =
            (float)player->grabbox_active;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_LEFT] =
            player->grabbox_left_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_RIGHT] =
            player->grabbox_right_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_TOP] =
            player->grabbox_top_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_BOTTOM] =
            player->grabbox_bottom_f32;
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_GRAB_ESCAPE_TICKS] =
            (float)player->grab_escape_ticks;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRAB_TARGET] =
            (float)player->grab_target;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_GRAB_OWNER] =
            (float)player->grab_owner;
        pf_web_view[base + PF_WEB_M4_VIEW_PLAYER_CHARGE_TICKS] =
            (float)player->charge_ticks;
        pf_web_view[
            base + PF_WEB_M4_VIEW_PLAYER_SMASH_CHARGE_TICKS] =
            (float)player->smash_charge_ticks;
    }
    pf_web_view[PF_WEB_M4_VIEW_EVENT_COUNT] =
        (float)pf_web_last_result.event_count;
    for (event_index = UINT32_C(0);
         event_index <
         (uint32_t)pf_web_last_result.event_count;
         ++event_index)
    {
        const pf_sim_event *event =
            &pf_web_last_result.events[event_index];
        const int base =
            PF_WEB_M4_VIEW_EVENT0 +
            (int)event_index * PF_WEB_M4_VIEW_EVENT_STRIDE;

        if (event->tick > (uint64_t)INT32_MAX ||
            event->sequence > (uint32_t)INT32_MAX ||
            event->value_f32 > (uint32_t)INT32_MAX)
        {
            return 0;
        }
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_SEQUENCE] =
            (float)event->sequence;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_TICK] =
            (float)event->tick;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_TYPE] =
            (float)event->type;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_SOURCE] =
            (float)event->source_player;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_TARGET] =
            (float)event->target_player;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_VALUE] =
            (float)event->value_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_VELOCITY_X] =
            event->velocity_x_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_VELOCITY_Y] =
            event->velocity_y_f32;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_FLAGS] =
            (float)event->flags;
        pf_web_view[base + PF_WEB_M4_VIEW_EVENT_DETAIL] =
            (float)event->detail;
    }

    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_ENABLED] =
        (float)inspection.item.enabled;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_STATE] =
        (float)inspection.item.state;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HOLDER] =
        (float)inspection.item.holder;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_SOURCE] =
        (float)inspection.item.source;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_THROW_DIRECTION] =
        (float)inspection.item.throw_direction;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HITBOX_ACTIVE] =
        (float)inspection.item.hitbox_active;
    pf_web_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_X] =
        inspection.item.position_x_f32;
    pf_web_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_Y] =
        inspection.item.position_y_f32;
    pf_web_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_VX] =
        inspection.item.velocity_x_f32;
    pf_web_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_VY] =
        inspection.item.velocity_y_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_LIFETIME] =
        (float)inspection.item.lifetime_ticks;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_RESPAWN] =
        (float)inspection.item.respawn_ticks;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_PICKUP_LOCKOUT] =
        (float)inspection.item.pickup_lockout_ticks;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HIT_MASK] =
        (float)inspection.item.hit_mask;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HALF_WIDTH] =
        pf_web_content.item.half_width_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HALF_HEIGHT] =
        pf_web_content.item.half_height_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HITBOX_HALF_WIDTH] =
        pf_web_content.item.hitbox_half_width_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HITBOX_HALF_HEIGHT] =
        pf_web_content.item.hitbox_half_height_f32;

    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_ENABLED] =
        (float)inspection.projectile.enabled;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_STATE] =
        (float)inspection.projectile.state;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_OWNER] =
        (float)inspection.projectile.owner;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_HITBOX_ACTIVE] =
        (float)inspection.projectile.hitbox_active;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_X] =
        inspection.projectile.position_x_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_Y] =
        inspection.projectile.position_y_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_VX] =
        inspection.projectile.velocity_x_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_VY] =
        inspection.projectile.velocity_y_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_LIFETIME] =
        (float)inspection.projectile.lifetime_ticks;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_HALF_WIDTH] =
        pf_web_content.projectile.half_width_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_HALF_HEIGHT] =
        pf_web_content.projectile.half_height_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_REFLECT_WINDOW] =
        (float)pf_web_content.projectile
            .powershield_reflect_window_ticks;
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)pf_web_player_count;
         ++player_index)
    {
        const player_inspection *player =
            &inspection.players[player_index];
        const int revival_base =
            PF_WEB_M4_VIEW_REVIVAL0 +
            (int)player_index * PF_WEB_M4_VIEW_REVIVAL_STRIDE;
        const int stale_move_base =
            PF_WEB_M4_VIEW_STALE_MOVE0 +
            (int)player_index * PF_WEB_M4_VIEW_STALE_MOVE_STRIDE;
        uint32_t stale_slot;

        pf_web_view[PF_WEB_M4_VIEW_RECOVERY0 + (int)player_index] =
            (float)player->recovery_available;
        pf_web_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_ACTIVE] =
            (float)player->revival_platform_active;
        pf_web_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_LEFT] =
            player->revival_platform_left_f32;
        pf_web_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_RIGHT] =
            player->revival_platform_right_f32;
        pf_web_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_Y] =
            player->revival_platform_y_f32;
        pf_web_view[
            stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_COUNT] =
            (float)player->stale_move_count;
        pf_web_view[
            stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_MULTIPLIER] =
            (float)player->stale_move_multiplier_f32;
        pf_web_view[
            stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_REGISTERED] =
            (float)player->attack_stale_registered;
        for (stale_slot = UINT32_C(0);
             stale_slot <
                 (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
             ++stale_slot)
        {
            pf_web_view[
                stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_IDS +
                (int)stale_slot] =
                (float)player->stale_move_ids[stale_slot];
        }
    }
    pf_web_view[PF_WEB_M4_VIEW_ITEM_STALE_REGISTERED] =
        (float)inspection.item.stale_registered;
    pf_web_view[
        PF_WEB_M4_VIEW_UPPER_PLATFORM0 +
        PF_WEB_M4_VIEW_UPPER_PLATFORM_LEFT] =
        inspection.stage.upper_platform_left_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_UPPER_PLATFORM0 +
        PF_WEB_M4_VIEW_UPPER_PLATFORM_RIGHT] =
        inspection.stage.upper_platform_right_f32;
    pf_web_view[
        PF_WEB_M4_VIEW_UPPER_PLATFORM0 +
        PF_WEB_M4_VIEW_UPPER_PLATFORM_Y] =
        inspection.stage.upper_platform_y_f32;

    pf_web_playtest_render(
        pf_web_view,
        PF_WEB_M4_VIEW_COUNT);
    return 1;
}

int pf_web_playtest_step_dual_trigger_special(
    int player0_x,
    int player0_y,
    int player0_secondary_x,
    int player0_secondary_y,
    int player0_jump,
    int player0_attack,
    int player0_strong_attack,
    int player0_left_shield,
    int player0_right_shield,
    int player1_x,
    int player1_y,
    int player1_secondary_x,
    int player1_secondary_y,
    int player1_jump,
    int player1_attack,
    int player1_strong_attack,
    int player1_left_shield,
    int player1_right_shield,
    int player0_special,
    int player1_special,
    int player0_taunt,
    int player1_taunt)
{
    struct inspection inspection;
    uint64_t player0_buttons = UINT64_C(0);
    uint64_t player1_buttons = UINT64_C(0);

    if (pf_web_sim != NULL &&
        inspect(pf_web_sim, &inspection) == PF_STATUS_OK &&
        (inspection.terminated != UINT8_C(0) ||
         inspection.truncated != UINT8_C(0)))
    {
        return pf_web_render();
    }

    if (player0_x < INT16_MIN || player0_x > INT16_MAX ||
        player0_y < INT16_MIN || player0_y > INT16_MAX ||
        player0_secondary_x < INT16_MIN ||
        player0_secondary_x > INT16_MAX ||
        player0_secondary_y < INT16_MIN ||
        player0_secondary_y > INT16_MAX ||
        player1_x < INT16_MIN || player1_x > INT16_MAX ||
        player1_y < INT16_MIN || player1_y > INT16_MAX ||
        player1_secondary_x < INT16_MIN ||
        player1_secondary_x > INT16_MAX ||
        player1_secondary_y < INT16_MIN ||
        player1_secondary_y > INT16_MAX ||
        (player0_jump != 0 && player0_jump != 1) ||
        (player0_attack != 0 && player0_attack != 1) ||
        (player0_strong_attack != 0 &&
         player0_strong_attack != 1) ||
        player0_left_shield < 0 ||
        player0_left_shield > (int)UINT16_MAX ||
        player0_right_shield < 0 ||
        player0_right_shield > (int)UINT16_MAX ||
        (player0_special != 0 && player0_special != 1) ||
        (player0_taunt != 0 && player0_taunt != 1) ||
        (player1_jump != 0 && player1_jump != 1) ||
        (player1_attack != 0 && player1_attack != 1) ||
        (player1_strong_attack != 0 &&
         player1_strong_attack != 1) ||
        player1_left_shield < 0 ||
        player1_left_shield > (int)UINT16_MAX ||
        player1_right_shield < 0 ||
        player1_right_shield > (int)UINT16_MAX ||
        (player1_special != 0 && player1_special != 1) ||
        (player1_taunt != 0 && player1_taunt != 1))
    {
        return 0;
    }
    if (player0_jump != 0)
    {
        player0_buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (player0_attack != 0)
    {
        player0_buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (player0_strong_attack != 0)
    {
        player0_buttons |= PF_INPUT_BUTTON_STRONG_ATTACK;
    }
    if (player0_special != 0)
    {
        player0_buttons |= PF_INPUT_BUTTON_SPECIAL;
    }
    if (player0_taunt != 0)
    {
        player0_buttons |= PF_INPUT_BUTTON_TAUNT;
    }
    if (player1_jump != 0)
    {
        player1_buttons |= PF_INPUT_BUTTON_JUMP;
    }
    if (player1_attack != 0)
    {
        player1_buttons |= PF_INPUT_BUTTON_ATTACK;
    }
    if (player1_strong_attack != 0)
    {
        player1_buttons |= PF_INPUT_BUTTON_STRONG_ATTACK;
    }
    if (player1_special != 0)
    {
        player1_buttons |= PF_INPUT_BUTTON_SPECIAL;
    }
    if (player1_taunt != 0)
    {
        player1_buttons |= PF_INPUT_BUTTON_TAUNT;
    }
    if (!pf_web_tick_with_dual_triggers(
            (int16_t)player0_x,
            (int16_t)player0_y,
            (int16_t)player0_secondary_x,
            (int16_t)player0_secondary_y,
            player0_buttons,
            player0_left_shield == 1
                ? UINT16_MAX
                : (uint16_t)player0_left_shield,
            player0_right_shield == 1
                ? UINT16_MAX
                : (uint16_t)player0_right_shield,
            (int16_t)player1_x,
            (int16_t)player1_y,
            (int16_t)player1_secondary_x,
            (int16_t)player1_secondary_y,
            player1_buttons,
            player1_left_shield == 1
                ? UINT16_MAX
                : (uint16_t)player1_left_shield,
            player1_right_shield == 1
                ? UINT16_MAX
                : (uint16_t)player1_right_shield,
            &inspection))
    {
        return 0;
    }
    return pf_web_render();
}

int pf_web_playtest_start(void)
{
    if (default_content(&pf_web_content) != PF_STATUS_OK ||
        !pf_web_initialize_live_item_lab())
    {
        return 0;
    }

    pf_web_playtest_install(
        (int)PF_WEB_M4_WALK_AXIS,
        (int)PF_WEB_M4_DASH_AXIS,
        (int)pf_web_content.fighter.aerial_landing_lag_ticks,
        (int)pf_web_content.fighter
            .strong_aerial_landing_lag_ticks);
    return pf_web_render();
}

int pf_web_playtest_step_special(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player0_attack,
    int player0_strong_attack,
    int player0_shield,
    int player1_x,
    int player1_y,
    int player1_jump,
    int player1_attack,
    int player1_strong_attack,
    int player1_shield,
    int player0_special,
    int player1_special,
    int player0_taunt,
    int player1_taunt)
{
    return pf_web_playtest_step_dual_trigger_special(
        player0_x,
        player0_y,
        0,
        0,
        player0_jump,
        player0_attack,
        player0_strong_attack,
        player0_shield,
        0,
        player1_x,
        player1_y,
        0,
        0,
        player1_jump,
        player1_attack,
        player1_strong_attack,
        player1_shield,
        0,
        player0_special,
        player1_special,
        player0_taunt,
        player1_taunt);
}

int pf_web_playtest_step(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player0_attack,
    int player0_strong_attack,
    int player0_shield,
    int player1_x,
    int player1_y,
    int player1_jump,
    int player1_attack,
    int player1_strong_attack,
    int player1_shield)
{
    return pf_web_playtest_step_special(
        player0_x,
        player0_y,
        player0_jump,
        player0_attack,
        player0_strong_attack,
        player0_shield,
        player1_x,
        player1_y,
        player1_jump,
        player1_attack,
        player1_strong_attack,
        player1_shield,
        0,
        0,
        0,
        0);
}

int pf_web_playtest_reset(void)
{
    return pf_web_reset_internal() && pf_web_render();
}

int pf_web_playtest_refresh(void)
{
    return pf_web_render();
}

int pf_web_playtest_configure_duel(int stock_count)
{
    const uint8_t previous_stock_count = pf_web_stock_count;

    if (stock_count < 1 ||
        stock_count > (int)PF_WEB_M4_MAX_SETUP_STOCKS)
    {
        return 0;
    }
    pf_web_stock_count = (uint8_t)stock_count;
    if (!pf_web_initialize_live_item_lab())
    {
        pf_web_stock_count = previous_stock_count;
        (void)pf_web_initialize_live_item_lab();
        return 0;
    }
    return pf_web_render();
}

int pf_web_playtest_set_team_lab(int enabled)
{
    if (enabled != 0 && enabled != 1)
    {
        return 0;
    }
    if (enabled != 0)
    {
        return pf_web_initialize_team_wobble_lab() &&
               pf_web_render();
    }
    return pf_web_initialize_live_item_lab() &&
           pf_web_render();
}
