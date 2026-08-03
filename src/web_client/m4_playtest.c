#include "m4_playtest.h"

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
#define PF_WEB_M4_CAMPING_MINIMUM_SEPARATION_Q16 INT32_C(693712)
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
#define PF_WEB_M4_VIEW_COUNT 503

enum pf_web_m4_view_field
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

static uint32_t pf_web_m4_expected_repeated_move_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t damage_q16,
    uint32_t hit_count)
{
    uint32_t total_q16 = UINT32_C(0);
    uint32_t hit;

    for (hit = UINT32_C(0); hit < hit_count; ++hit)
    {
        uint32_t reduction_q16 = UINT32_C(0);
        uint32_t slot;
        const uint32_t occupied_slots =
            hit < (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY
                ? hit
                : (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;

        for (slot = UINT32_C(0); slot < occupied_slots; ++slot)
        {
            reduction_q16 +=
                (uint32_t)fighter
                    ->stale_move_slot_reduction_q16[slot];
        }
        total_q16 += (uint32_t)(
            (uint64_t)damage_q16 *
            ((uint64_t)(uint32_t)PF_Q16_ONE -
             (uint64_t)reduction_q16) /
            (uint64_t)(uint32_t)PF_Q16_ONE);
    }
    return total_q16;
}

static uint32_t pf_web_m4_expected_stale_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t damage_q16,
    const uint8_t stale_move_ids[PF_SIM_STALE_MOVE_QUEUE_CAPACITY],
    uint8_t stale_move_count,
    uint8_t move_id)
{
    uint32_t reduction_q16 = UINT32_C(0);
    uint32_t slot;

    for (slot = UINT32_C(0);
         slot < (uint32_t)stale_move_count;
         ++slot)
    {
        if (stale_move_ids[slot] == move_id)
        {
            reduction_q16 +=
                (uint32_t)fighter
                    ->stale_move_slot_reduction_q16[slot];
        }
    }
    return (uint32_t)(
        (uint64_t)damage_q16 *
        ((uint64_t)(uint32_t)PF_Q16_ONE -
         (uint64_t)reduction_q16) /
        (uint64_t)(uint32_t)PF_Q16_ONE);
}

typedef struct pf_web_m4_storage
{
    alignas(PF_WEB_M4_MEMORY_ALIGNMENT)
        uint8_t state[PF_WEB_M4_MEMORY_BYTES];
    alignas(PF_WEB_M4_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_WEB_M4_MEMORY_BYTES];
} pf_web_m4_storage;

extern void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int input_probe_passed,
    int air_facing_probe_passed,
    int instant_double_jump_probe_passed,
    int double_jump_cancel_probe_passed,
    int double_jump_cancel_counter_probe_passed,
    int bat_drop_probe_passed,
    int glide_toss_probe_passed,
    int jump_cancel_throw_probe_passed,
    int jump_cancel_probe_passed,
    int edge_hop_probe_passed,
    int edge_dash_probe_passed,
    int fox_trot_probe_passed,
    int moonwalk_probe_passed,
    int teeter_cancel_probe_passed,
    int stage_humping_probe_passed,
    int taunt_cancel_probe_passed,
    int scar_jump_probe_passed,
    int team_wobble_probe_passed,
    int pivot_probe_passed,
    int dash_cancel_probe_passed,
    int dashing_shield_probe_passed,
    int shield_platform_drop_probe_passed,
    int small_step_forward_smash_probe_passed,
    int drop_cancel_probe_passed,
    int v_cancel_probe_passed,
    int approach_probe_passed,
    int spacing_probe_passed,
    int sharking_probe_passed,
    int cross_up_probe_passed,
    int mindgame_probe_passed,
    int juggling_probe_passed,
    int ladder_probe_passed,
    int kill_confirm_probe_passed,
    int zero_to_death_probe_passed,
    int ledge_cancel_probe_passed,
    int planking_probe_passed,
    int jump_cancelled_grab_probe_passed,
    int boost_grab_probe_passed,
    int jab_cancel_probe_passed,
    int jab_reset_probe_passed,
    int chain_grab_probe_passed,
    int combat_probe_passed,
    int reaction_probe_passed,
    int shield_probe_passed,
    int shield_break_probe_passed,
    int tumble_probe_passed,
    int floor_recovery_probe_passed,
    int tech_chase_probe_passed,
    int surface_tech_probe_passed,
    int air_dodge_probe_passed,
    int ground_dodge_probe_passed,
    int aerial_l_cancel_probe_passed,
    int match_probe_passed,
    int short_hop_laser_probe_passed,
    int camping_probe_passed,
    int shine_spike_probe_passed,
    int charge_storage_probe_passed,
    int vector_ascent_probe_passed,
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks);

extern void pf_web_m4_playtest_render(
    const int32_t *view,
    int view_count);

static pf_web_m4_storage pf_web_m4_sim_storage;
static pf_m4_content pf_web_m4_content;
static pf_sim *pf_web_m4_sim;
static pf_tick_result pf_web_m4_last_result;
static int32_t pf_web_m4_view[PF_WEB_M4_VIEW_COUNT];
static uint8_t pf_web_m4_player_count = PF_WEB_M4_DUEL_PLAYER_COUNT;
static uint8_t pf_web_m4_team_lab_active;
static uint8_t pf_web_m4_stock_count = PF_SIM_DEFAULT_STOCK_COUNT;

static const pf_sim_event *pf_web_m4_find_event(
    pf_sim_event_type event_type);

static int pf_web_m4_initialize_content(
    uint8_t player_count,
    pf_sim_mode mode)
{
    pf_content_view content_view;
    pf_memory_requirements requirements;
    pf_sim_config config;

    if (pf_m4_make_content_view(
            &pf_web_m4_content,
            &content_view) != PF_STATUS_OK ||
        pf_sim_default_config(
            &config,
            player_count,
            mode) != PF_STATUS_OK)
    {
        return 0;
    }
    config.max_ticks = PF_WEB_M4_MAX_TICKS;
    config.stock_count = pf_web_m4_stock_count;
    if (pf_sim_query_memory(&config, &requirements) != PF_STATUS_OK ||
        requirements.state_bytes >
            sizeof(pf_web_m4_sim_storage.state) ||
        requirements.scratch_bytes >
            sizeof(pf_web_m4_sim_storage.scratch) ||
        requirements.state_alignment >
            PF_WEB_M4_MEMORY_ALIGNMENT ||
        requirements.scratch_alignment >
            PF_WEB_M4_MEMORY_ALIGNMENT ||
        pf_sim_init(
            pf_web_m4_sim_storage.state,
            sizeof(pf_web_m4_sim_storage.state),
            pf_web_m4_sim_storage.scratch,
            sizeof(pf_web_m4_sim_storage.scratch),
            &content_view,
            &config,
            &pf_web_m4_sim) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_player_count = player_count;
    pf_web_m4_team_lab_active =
        mode == PF_SIM_MODE_TEAMS ? UINT8_C(1) : UINT8_C(0);
    return 1;
}

static int pf_web_m4_initialize_current_content(void)
{
    return pf_web_m4_initialize_content(
        PF_WEB_M4_DUEL_PLAYER_COUNT,
        PF_SIM_MODE_DUEL);
}

static void pf_web_m4_make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    const pf_m4_inspection *before,
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
         player_index < (uint32_t)pf_web_m4_player_count;
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
    if (pf_web_m4_team_lab_active != UINT8_C(0))
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

static int pf_web_m4_tick_with_dual_triggers(
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
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    pf_m4_inspection before;

    if (pf_web_m4_sim == NULL ||
        pf_m4_inspect(pf_web_m4_sim, &before) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_make_inputs(
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
    if (pf_sim_tick(
            pf_web_m4_sim,
            inputs,
            (size_t)pf_web_m4_player_count,
            &result) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_last_result = result;
    return pf_m4_inspect(pf_web_m4_sim, out_inspection) ==
           PF_STATUS_OK;
}

static int pf_web_m4_tick_with_triggers(
    int16_t player0_x,
    int16_t player0_y,
    uint64_t player0_buttons,
    uint16_t player0_trigger,
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons,
    uint16_t player1_trigger,
    pf_m4_inspection *out_inspection)
{
    return pf_web_m4_tick_with_dual_triggers(
        player0_x,
        player0_y,
        INT16_C(0),
        INT16_C(0),
        player0_buttons,
        player0_trigger,
        UINT16_C(0),
        player1_x,
        player1_y,
        INT16_C(0),
        INT16_C(0),
        player1_buttons,
        player1_trigger,
        UINT16_C(0),
        out_inspection);
}

static int pf_web_m4_tick(
    int16_t player0_x,
    int16_t player0_y,
    uint64_t player0_buttons,
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons,
    pf_m4_inspection *out_inspection)
{
    return pf_web_m4_tick_with_triggers(
        player0_x,
        player0_y,
        player0_buttons,
        UINT16_C(0),
        player1_x,
        player1_y,
        player1_buttons,
        UINT16_C(0),
        out_inspection);
}

static int pf_web_m4_reset_internal(void)
{
    if (pf_web_m4_sim == NULL ||
        pf_sim_reset(
            pf_web_m4_sim,
            PF_WEB_M4_RESET_SEED) != PF_STATUS_OK)
    {
        return 0;
    }
    (void)memset(
        &pf_web_m4_last_result,
        0,
        sizeof(pf_web_m4_last_result));
    return 1;
}

static int pf_web_m4_capture_hop_apex(
    uint32_t held_ticks,
    int32_t *out_apex)
{
    pf_m4_inspection inspection;
    int32_t apex = INT32_MAX;
    uint32_t tick;
    int became_airborne = 0;

    if (out_apex == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const uint64_t buttons =
            tick < held_ticks ? PF_INPUT_BUTTON_JUMP : UINT64_C(0);

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                buttons,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].position_y_q16 < apex)
        {
            apex = inspection.players[0].position_y_q16;
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            became_airborne = 1;
        }
        else if (became_airborne != 0)
        {
            *out_apex = apex;
            return 1;
        }
    }
    return 0;
}

static int pf_web_m4_run_input_probe(void)
{
    pf_m4_inspection inspection;
    int32_t short_early_apex;
    int32_t short_late_apex;
    int32_t full_release_apex;
    int32_t full_hold_apex;
    int16_t ramp_low;
    int16_t ramp_middle;
    int16_t ramp_high;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_WALK_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK)
    {
        return 0;
    }
    ramp_low = (int16_t)(
        pf_web_m4_content.fighter.axis_dead_zone + UINT16_C(1));
    ramp_middle = (int16_t)(
        ((uint32_t)pf_web_m4_content.fighter.axis_dead_zone +
         (uint32_t)pf_web_m4_content.fighter.dash_axis_threshold) /
        UINT32_C(2));
    ramp_high = (int16_t)(
        pf_web_m4_content.fighter.dash_axis_threshold - UINT16_C(1));
    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            ramp_low,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.dash_input_window_ticks ||
        !pf_web_m4_tick(
            ramp_middle,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.dash_input_window_ticks ||
        !pf_web_m4_tick(
            ramp_high,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.dash_input_window_ticks ||
        inspection.players[0].dash_direction != INT8_C(0) ||
        inspection.players[0].velocity_x_q16 >
            pf_web_m4_content.fighter.walk_speed_q16)
    {
        return 0;
    }
    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            (int16_t)(
                ((uint32_t)pf_web_m4_content.fighter.axis_dead_zone +
                 (uint32_t)
                     pf_web_m4_content.fighter.dash_axis_threshold) /
                    UINT32_C(2) +
                UINT32_C(1)),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(1))
    {
        return 0;
    }
    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].dash_direction != INT8_C(-1))
    {
        return 0;
    }
    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN_TURNAROUND)
    {
        return 0;
    }
    if (!pf_web_m4_capture_hop_apex(
            UINT32_C(1),
            &short_early_apex) ||
        !pf_web_m4_capture_hop_apex(
            UINT32_C(2),
            &short_late_apex) ||
        !pf_web_m4_capture_hop_apex(
            UINT32_C(3),
            &full_release_apex) ||
        !pf_web_m4_capture_hop_apex(
            UINT32_C(12),
            &full_hold_apex))
    {
        return 0;
    }
    return short_early_apex == short_late_apex &&
           full_release_apex == full_hold_apex &&
           full_release_apex < short_early_apex;
}

static int pf_web_m4_run_fox_trot_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t burst;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (burst = UINT32_C(0); burst < UINT32_C(4); ++burst)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
            inspection.players[0].action_ticks != UINT16_C(1) ||
            inspection.players[0].dash_direction != INT8_C(1) ||
            inspection.players[0].facing != INT8_C(1) ||
            !pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
            inspection.players[0].dash_direction != INT8_C(0))
        {
            return 0;
        }
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
        (uint8_t)PF_M4_ACTION_RUN)
    {
        return 0;
    }

    return pf_web_m4_reset_internal() &&
           pf_web_m4_tick(
               PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           pf_web_m4_tick(
               PF_WEB_M4_WALK_AXIS,
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_WALK;
}

static int pf_web_m4_run_pivot_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].dash_direction != INT8_C(-1) ||
        inspection.players[0].facing != INT8_C(-1) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].facing != INT8_C(-1) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].facing != INT8_C(-1) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks != UINT16_C(2))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_RUN &&
           pf_web_m4_tick(
               -PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
}

static int pf_web_m4_run_dash_cancel_probe(void)
{
    pf_m4_inspection inspection;
    int32_t run_velocity;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CROUCH ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_x_q16 >= run_velocity ||
        !pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DOWN_ATTACK ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (!pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_x_q16 >= run_velocity)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return pf_web_m4_tick(
               -PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_RUN_TURNAROUND &&
           pf_web_m4_tick(
               INT16_C(0),
               PF_WEB_M4_DASH_AXIS,
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_RUN_TURNAROUND;
}

static int pf_web_m4_run_dashing_shield_probe(void)
{
    pf_m4_inspection inspection;
    int32_t idle_x;
    int32_t run_start_x;
    int32_t tap_release_x;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    run_start_x = inspection.players[0].position_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].position_x_q16 <= run_start_x)
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <
             (uint32_t)pf_web_m4_content.fighter
                 .shield_minimum_hold_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        return 0;
    }
    tap_release_x = inspection.players[0].position_x_q16;
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.shield_release_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].position_x_q16 <= tap_release_x)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter
                 .shield_minimum_hold_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.shield_minimum_hold_ticks ||
        inspection.players[0].position_x_q16 != tap_release_x)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(
            pf_web_m4_sim,
            &inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    idle_x = inspection.players[0].position_x_q16;
    return pf_web_m4_tick_with_triggers(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD &&
           inspection.players[0].velocity_x_q16 == INT32_C(0) &&
           inspection.players[0].position_x_q16 == idle_x;
}

static int pf_web_m4_run_small_step_forward_smash_probe(void)
{
    pf_m4_inspection inspection;
    int32_t standing_x;
    uint32_t tick;

    if (pf_web_m4_content.fighter
            .forward_smash_input_window_ticks != UINT16_C(3) ||
        !pf_web_m4_reset_internal() ||
        pf_m4_inspect(
            pf_web_m4_sim,
            &inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    standing_x = inspection.players[0].position_x_q16;
    if (!pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
        inspection.players[0].position_x_q16 != standing_x)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter
                 .forward_smash_input_window_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter
                .forward_smash_input_window_ticks ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
        inspection.players[0].position_x_q16 <= standing_x)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter
                     .forward_smash_input_window_ticks +
                 UINT32_C(1);
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_ATTACK)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter
                 .forward_smash_input_window_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
}

static int pf_web_m4_prepare_drop_cancel_platform(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(10); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(7); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(46); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[1].grounded != UINT8_C(0) &&
            out_inspection->players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM &&
            out_inspection->players[1].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            break;
        }
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(20) &&
         (out_inspection->players[0].action_state !=
              (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
          out_inspection->players[1].action_state !=
              (uint8_t)PF_M4_ACTION_GROUND_IDLE);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(34); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_WALK_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_WALK_AXIS,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(20) &&
         (out_inspection->players[0].velocity_x_q16 != INT32_C(0) ||
          out_inspection->players[1].velocity_x_q16 != INT32_C(0) ||
          out_inspection->players[0].action_state !=
              (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
          out_inspection->players[1].action_state !=
              (uint8_t)PF_M4_ACTION_GROUND_IDLE);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[1].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[1].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
           out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
           out_inspection->players[1].velocity_x_q16 == INT32_C(0);
}

static int pf_web_m4_run_shield_platform_drop_probe(void)
{
    pf_m4_inspection inspection;

    if (pf_web_m4_content.fighter.shield_drop_axis_threshold !=
            UINT16_C(12288) ||
        PF_WEB_M4_WALK_AXIS <
            (int16_t)pf_web_m4_content.fighter
                .shield_drop_axis_threshold ||
        PF_WEB_M4_WALK_AXIS >=
            (int16_t)pf_web_m4_content.fighter
                .crouch_axis_threshold ||
        !pf_web_m4_prepare_drop_cancel_platform(&inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            PF_WEB_M4_WALK_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            PF_WEB_M4_WALK_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        inspection.players[0].platform_drop_ticks !=
            (uint8_t)pf_web_m4_content.fighter
                .platform_drop_ticks ||
        inspection.players[0].fast_fall != UINT8_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_prepare_drop_cancel_platform(&inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            (int16_t)(
                pf_web_m4_content.fighter
                    .shield_drop_axis_threshold -
                UINT16_C(1)),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        return 0;
    }

    return pf_web_m4_prepare_drop_cancel_platform(&inspection) &&
           pf_web_m4_tick_with_triggers(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               &inspection) &&
           pf_web_m4_tick_with_triggers(
               INT16_C(0),
               (int16_t)pf_web_m4_content.fighter
                   .crouch_axis_threshold,
               UINT64_C(0),
               UINT16_MAX,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SPOT_DODGE &&
           inspection.players[0].grounded != UINT8_C(0) &&
           inspection.players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           inspection.players[0].platform_drop_ticks == UINT8_C(0);
}

static int pf_web_m4_run_drop_cancel_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;
    int saw_hit = 0;
    int saw_snap = 0;
    int saw_landing = 0;
    int saw_late_hit = 0;
    int saw_late_snap = 0;

    if (pf_web_m4_content.fighter.platform_drop_ticks !=
            UINT16_C(9) ||
        !pf_web_m4_prepare_drop_cancel_platform(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].fast_fall != UINT8_C(0) ||
        inspection.players[0].platform_drop_ticks != UINT8_C(9) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].platform_drop_ticks != UINT8_C(8))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].damage_q16 ==
            pf_web_m4_content.fighter.aerial_damage_q16)
        {
            saw_hit = 1;
        }
        if (inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            saw_snap = 1;
        }
        if (saw_snap != 0 &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AERIAL_LANDING)
        {
            saw_landing = 1;
            break;
        }
    }
    if (saw_hit == 0 || saw_snap == 0 || saw_landing == 0)
    {
        return 0;
    }

    if (!pf_web_m4_prepare_drop_cancel_platform(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].damage_q16 ==
            pf_web_m4_content.fighter.aerial_damage_q16)
        {
            saw_late_hit = 1;
        }
        if (inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            saw_late_snap = 1;
        }
        if (saw_late_hit != 0 &&
            inspection.players[0].hitlag_ticks == UINT16_C(0) &&
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG)
        {
            break;
        }
    }
    return saw_late_hit != 0 &&
           saw_late_snap == 0 &&
           inspection.players[0].grounded == UINT8_C(0) &&
           inspection.players[0].support ==
               (uint8_t)PF_M4_SURFACE_NONE;
}

static int pf_web_m4_capture_v_cancel_launch(
    int trigger_on_hit,
    int target_attacks,
    int preexisting_lockout,
    int32_t *out_velocity_x_q16,
    int32_t *out_velocity_y_q16,
    uint16_t *out_hitstun_ticks,
    uint16_t *out_tech_lockout_ticks,
    uint8_t *out_trigger_input_age);

static int16_t pf_web_m4_sharking_axis(
    const pf_m4_inspection *inspection)
{
    const int16_t magnitude =
        inspection->players[0].grounded != UINT8_C(0)
            ? PF_WEB_M4_WALK_AXIS
            : PF_WEB_M4_DASH_AXIS;

    return inspection->players[1].position_x_q16 >=
                   inspection->players[0].position_x_q16
               ? magnitude
               : (int16_t)-magnitude;
}

static int pf_web_m4_prepare_sharking_target(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!pf_web_m4_prepare_drop_cancel_platform(out_inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[0].support ==
                (uint8_t)PF_M4_SURFACE_FLOOR &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(240) ||
        out_inspection->players[1].grounded == UINT8_C(0) ||
        out_inspection->players[1].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const int32_t delta_x =
            out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16;
        const int32_t distance_x =
            delta_x < INT32_C(0) ? -delta_x : delta_x;
        const int16_t walk_axis =
            distance_x > PF_Q16_ONE
                ? pf_web_m4_sharking_axis(out_inspection)
                : INT16_C(0);

        if (walk_axis == INT16_C(0) &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
        if (!pf_web_m4_tick(
                walk_axis,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(240))
    {
        return 0;
    }
    {
        const int32_t delta_x =
            out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16;
        const int32_t distance_x =
            delta_x < INT32_C(0) ? -delta_x : delta_x;

        return distance_x <= PF_Q16_ONE &&
               out_inspection->players[0].support ==
                   (uint8_t)PF_M4_SURFACE_FLOOR &&
               out_inspection->players[1].support ==
                   (uint8_t)PF_M4_SURFACE_PLATFORM;
    }
}

static int pf_web_m4_start_sharking_aerial(
    int early_attack,
    uint16_t target_trigger,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                pf_web_m4_sharking_axis(out_inspection),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                target_trigger,
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[0].grounded != UINT8_C(0) ||
        out_inspection->players[1].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        return 0;
    }
    if (early_attack == 0)
    {
        for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
        {
            if (out_inspection->players[0].position_y_q16 -
                    out_inspection->players[1].position_y_q16 <=
                INT32_C(4) * PF_Q16_ONE)
            {
                break;
            }
            if (!pf_web_m4_tick_with_triggers(
                    pf_web_m4_sharking_axis(out_inspection),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    target_trigger,
                    out_inspection))
            {
                return 0;
            }
        }
        if (tick == UINT32_C(40))
        {
            return 0;
        }
    }
    return pf_web_m4_tick_with_triggers(
               pf_web_m4_sharking_axis(out_inspection),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               target_trigger,
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_BACK_AERIAL &&
           out_inspection->players[0].position_y_q16 >
               out_inspection->stage.platform_y_q16;
}

static int pf_web_m4_run_sharking_route(int route)
{
    pf_m4_inspection inspection;
    const uint16_t target_trigger =
        route == 2
            ? pf_web_m4_content.fighter.light_shield_trigger_threshold
            : UINT16_C(0);
    uint32_t tick;
    int saw_hitbox = 0;

    if (!pf_web_m4_prepare_sharking_target(&inspection) ||
        (route == 2 &&
         !pf_web_m4_tick_with_triggers(
             INT16_C(0),
             INT16_C(0),
             UINT64_C(0),
             UINT16_C(0),
             INT16_C(0),
             INT16_C(0),
             UINT64_C(0),
             target_trigger,
             &inspection)) ||
        !pf_web_m4_start_sharking_aerial(
            route == 1,
            target_trigger,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(56); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                pf_web_m4_sharking_axis(&inspection),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                target_trigger,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_hitbox = 1;
        }
        if (inspection.players[1].damage_q16 != UINT32_C(0) ||
            (route == 2 &&
             inspection.players[1].action_state ==
                 (uint8_t)PF_M4_ACTION_HITLAG))
        {
            break;
        }
    }
    if (route == 0)
    {
        return inspection.players[1].damage_q16 ==
                   pf_web_m4_content.fighter.back_aerial.damage_q16 &&
               inspection.players[1].last_hit_attacker == UINT8_C(0);
    }
    if (route == 1)
    {
        return saw_hitbox != 0 &&
               inspection.players[1].damage_q16 == UINT32_C(0);
    }
    return tick < UINT32_C(56) &&
           inspection.players[1].damage_q16 == UINT32_C(0) &&
           inspection.players[1].shield_health_q16 <
               pf_web_m4_content.fighter.shield_health_q16 &&
           inspection.players[1].powershield == UINT8_C(0) &&
           pf_web_m4_last_result.event_count == UINT8_C(2) &&
           pf_web_m4_last_result.events[0].type ==
               (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK;
}

static int pf_web_m4_run_sharking_probe(void)
{
    return pf_web_m4_run_sharking_route(0) &&
           pf_web_m4_run_sharking_route(1) &&
           pf_web_m4_run_sharking_route(2);
}

static int16_t pf_web_m4_cross_up_steering_axis(
    const pf_m4_inspection *inspection)
{
    return inspection->players[0].position_x_q16 <=
                   inspection->players[1].position_x_q16 -
                       PF_Q16_ONE / INT32_C(2)
               ? PF_WEB_M4_DASH_AXIS
               : -PF_WEB_M4_DASH_AXIS;
}

static int pf_web_m4_prepare_cross_up_spacing(
    int back_aerial,
    pf_m4_inspection *out_inspection)
{
    const int32_t target_distance =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(2);
    const int32_t brake_distance =
        target_distance +
        pf_web_m4_content.fighter.walk_speed_q16 / INT32_C(2);
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, out_inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(100); ++tick)
    {
        const int16_t target_axis =
            out_inspection->players[1].position_x_q16 <
                    INT32_C(11) * PF_Q16_ONE
                ? PF_WEB_M4_WALK_AXIS
                : INT16_C(0);

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                target_axis,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (target_axis == INT16_C(0) &&
            out_inspection->players[1].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(100))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(400); ++tick)
    {
        const int16_t attacker_axis =
            out_inspection->players[1].position_x_q16 -
                    out_inspection->players[0].position_x_q16 >
                brake_distance
                ? PF_WEB_M4_WALK_AXIS
                : INT16_C(0);

        if (!pf_web_m4_tick(
                attacker_axis,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (attacker_axis == INT16_C(0) &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(400))
    {
        return 0;
    }
    if (out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16 >
        target_distance)
    {
        return 0;
    }
    if (out_inspection->players[0].action_state !=
        (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return 0;
    }
    if (out_inspection->players[1].action_state !=
        (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return 0;
    }
    if (!pf_web_m4_tick(
            back_aerial != 0 ? -PF_WEB_M4_WALK_AXIS : INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            -PF_WEB_M4_WALK_AXIS,
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
            out_inspection->players[1].velocity_x_q16 == INT32_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    return tick < UINT32_C(20) &&
           out_inspection->players[0].facing ==
               (back_aerial != 0 ? INT8_C(-1) : INT8_C(1)) &&
           out_inspection->players[1].facing == INT8_C(-1) &&
           out_inspection->players[1].position_x_q16 <
               INT32_C(13) * PF_Q16_ONE;
}

static int pf_web_m4_start_cross_up_aerial(
    int back_aerial,
    int early_attack,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!pf_web_m4_prepare_cross_up_spacing(
            back_aerial,
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_JUMP
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[0].grounded != UINT8_C(0) ||
        out_inspection->players[0].facing !=
            (back_aerial != 0 ? INT8_C(-1) : INT8_C(1)))
    {
        return 0;
    }
    if (early_attack == 0)
    {
        for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
        {
            const int32_t vertical_gap =
                out_inspection->players[1].position_y_q16 -
                out_inspection->players[0].position_y_q16;
            const int reached_attack_position =
                out_inspection->players[0].velocity_y_q16 > INT32_C(0) &&
                vertical_gap > INT32_C(0) &&
                vertical_gap <= INT32_C(2) * PF_Q16_ONE &&
                (back_aerial == 0 ||
                 out_inspection->players[0].position_x_q16 >
                     out_inspection->players[1].position_x_q16);

            if (reached_attack_position != 0)
            {
                break;
            }
            if (!pf_web_m4_tick_with_triggers(
                    back_aerial != 0
                        ? pf_web_m4_cross_up_steering_axis(out_inspection)
                        : INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_MAX,
                    out_inspection))
            {
                return 0;
            }
        }
        if (tick == UINT32_C(64))
        {
            return 0;
        }
    }
    return pf_web_m4_tick_with_triggers(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK;
}

static int pf_web_m4_run_cross_up_route(int route)
{
    pf_m4_inspection inspection;
    const int back_aerial = route != 2;
    uint32_t tick;
    int saw_hitbox = 0;
    int saw_block = 0;

    if (!pf_web_m4_start_cross_up_aerial(
            back_aerial,
            route == 1,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
    {
        int16_t axis = INT16_C(0);

        if (inspection.players[0].grounded == UINT8_C(0))
        {
            axis = route == 1
                ? PF_WEB_M4_DASH_AXIS
                : (back_aerial != 0
                       ? pf_web_m4_cross_up_steering_axis(&inspection)
                       : INT16_C(0));
        }
        if (!pf_web_m4_tick_with_triggers(
                axis,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_hitbox = 1;
        }
        if (pf_web_m4_last_result.event_count == UINT8_C(2) &&
            pf_web_m4_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK)
        {
            saw_block = 1;
        }
    }
    if (route == 0)
    {
        return saw_hitbox != 0 && saw_block != 0 &&
               inspection.players[0].position_x_q16 >
                   inspection.players[1].position_x_q16 &&
               inspection.players[0].facing == INT8_C(-1) &&
               inspection.players[1].facing == INT8_C(-1) &&
               inspection.players[1].damage_q16 == UINT32_C(0);
    }
    if (route == 1)
    {
        return saw_hitbox != 0 && saw_block == 0 &&
               inspection.players[1].damage_q16 == UINT32_C(0);
    }
    return saw_hitbox != 0 && saw_block != 0 &&
           inspection.players[0].position_x_q16 <
               inspection.players[1].position_x_q16 &&
           inspection.players[0].facing == INT8_C(1) &&
           inspection.players[1].facing == INT8_C(-1) &&
           inspection.players[1].damage_q16 == UINT32_C(0);
}

static int pf_web_m4_run_cross_up_probe(void)
{
    return pf_web_m4_run_cross_up_route(0) &&
           pf_web_m4_run_cross_up_route(1) &&
           pf_web_m4_run_cross_up_route(2);
}

static int16_t pf_web_m4_juggling_chase_axis(
    const pf_m4_inspection *inspection)
{
    const int32_t delta =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;
    const int32_t stop_distance =
        (INT32_C(5) * PF_Q16_ONE) / INT32_C(4);

    if (delta > stop_distance)
    {
        return PF_WEB_M4_WALK_AXIS;
    }
    if (delta < -stop_distance)
    {
        return -PF_WEB_M4_WALK_AXIS;
    }
    return INT16_C(0);
}

static int pf_web_m4_prepare_juggling_spacing(
    int escape_route,
    pf_m4_inspection *out_inspection)
{
    const int32_t target_x = PF_Q16_ONE;
    const int32_t target_distance =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(5);
    const int32_t brake_distance =
        target_distance - INT32_C(4) *
            pf_web_m4_content.fighter.walk_speed_q16;
    const uint64_t platform_clear_phase =
        escape_route != 0 ? UINT64_C(98) : UINT64_C(31);
    uint32_t tick;

    if (out_inspection == NULL || !pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, out_inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(400); ++tick)
    {
        const int16_t target_axis =
            out_inspection->players[1].position_x_q16 > target_x
                ? -PF_WEB_M4_WALK_AXIS
                : INT16_C(0);

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                target_axis,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (target_axis == INT16_C(0) &&
            out_inspection->players[1].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(400))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(400); ++tick)
    {
        const int16_t attacker_axis =
            out_inspection->players[0].position_x_q16 <
                    out_inspection->players[1].position_x_q16 -
                        brake_distance
                ? PF_WEB_M4_WALK_AXIS
                : INT16_C(0);

        if (!pf_web_m4_tick(
                attacker_axis,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (attacker_axis == INT16_C(0) &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(400))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(20))
    {
        return 0;
    }
    if (out_inspection->players[0].facing != INT8_C(1))
    {
        return 0;
    }
    if (out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16 <=
        PF_Q16_ONE)
    {
        return 0;
    }
    if (out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16 >
        pf_web_m4_content.fighter.strong_hitbox_offset_x_q16 +
            pf_web_m4_content.fighter.strong_hitbox_half_width_q16 +
            pf_web_m4_content.fighter.half_width_q16)
    {
        return 0;
    }
    if (out_inspection->players[1].position_x_q16 <=
            INT32_C(0) ||
        out_inspection->players[1].position_x_q16 >=
            INT32_C(2) * PF_Q16_ONE)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        /* Keep the default moving platform out of the descent lane. */
        if ((out_inspection->tick % UINT64_C(120)) ==
            platform_clear_phase)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(120))
    {
        return 0;
    }
    if (out_inspection->players[0].grounded == UINT8_C(0) ||
        out_inspection->players[1].grounded == UINT8_C(0))
    {
        return 0;
    }
    return 1;
}

static int pf_web_m4_initialize_team_wobble_lab(void)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(2) * INT32_C(65536)) / INT32_C(5);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * INT32_C(65536);
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.item.enabled = UINT8_C(0);
    return pf_web_m4_initialize_content(
               PF_WEB_M4_TEAM_PLAYER_COUNT,
               PF_SIM_MODE_TEAMS) &&
           pf_web_m4_reset_internal();
}

static int pf_web_m4_run_juggling_route(int escape_route)
{
    pf_m4_inspection inspection;
    uint32_t launcher_sequence = UINT32_C(0);
    uint32_t tick;
    uint32_t jump_hold_ticks = UINT32_C(0);
    int launched_airborne = 0;
    int jump_started = 0;
    int aerial_started = 0;
    int saw_followup_hitbox = 0;
    int air_dodge_started = 0;

    if (!pf_web_m4_prepare_juggling_spacing(
            escape_route,
            &inspection))
    {
        return 0;
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].damage_q16 ==
            pf_web_m4_content.fighter.strong_damage_q16)
        {
            launcher_sequence =
                inspection.players[1].last_hit_sequence;
            break;
        }
    }
    if (tick == UINT32_C(24) || launcher_sequence == UINT32_C(0))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        const int16_t attacker_x =
            pf_web_m4_juggling_chase_axis(&inspection);
        int16_t target_x = INT16_C(0);
        int16_t target_y = INT16_C(0);
        uint64_t attacker_buttons = UINT64_C(0);
        uint16_t target_trigger = UINT16_C(0);
        int32_t horizontal_gap =
            inspection.players[1].position_x_q16 -
            inspection.players[0].position_x_q16;

        if (horizontal_gap < INT32_C(0))
        {
            horizontal_gap = -horizontal_gap;
        }
        if (inspection.players[1].grounded == UINT8_C(0))
        {
            launched_airborne = 1;
        }
        else if (launched_airborne != 0 &&
                 inspection.players[1].last_hit_sequence ==
                     launcher_sequence)
        {
            if (escape_route == 0 || saw_followup_hitbox != 0)
            {
                break;
            }
        }
        if (escape_route != 0)
        {
            target_x = PF_WEB_M4_DASH_AXIS;
            target_y = -PF_WEB_M4_DASH_AXIS;
            if (launched_airborne != 0 &&
                inspection.players[1].hitstun_ticks == UINT16_C(0) &&
                air_dodge_started == 0)
            {
                target_trigger = UINT16_MAX;
                air_dodge_started = 1;
            }
        }
        if (jump_started == 0 && launched_airborne != 0 &&
            inspection.players[1].velocity_y_q16 > INT32_C(0) &&
            inspection.players[1].position_y_q16 >=
                INT32_C(20) * PF_Q16_ONE &&
            horizontal_gap <= INT32_C(4) * PF_Q16_ONE &&
            inspection.players[0].grounded != UINT8_C(0) &&
            ((inspection.players[1].position_x_q16 -
                      inspection.players[0].position_x_q16 >=
                  PF_Q16_ONE / INT32_C(2) &&
              inspection.players[0].facing == INT8_C(1)) ||
             (inspection.players[1].position_x_q16 -
                      inspection.players[0].position_x_q16 <=
                  -PF_Q16_ONE / INT32_C(2) &&
              inspection.players[0].facing == INT8_C(-1))))
        {
            jump_started = 1;
            jump_hold_ticks = UINT32_C(3);
        }
        if (jump_hold_ticks > UINT32_C(0))
        {
            attacker_buttons |= PF_INPUT_BUTTON_JUMP;
            --jump_hold_ticks;
        }
        if (jump_started != 0 && aerial_started == 0 &&
            inspection.players[0].grounded == UINT8_C(0) &&
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_JUMP_SQUAT &&
            inspection.players[0].position_y_q16 -
                    inspection.players[1].position_y_q16 >
                INT32_C(0) &&
            inspection.players[0].position_y_q16 -
                    inspection.players[1].position_y_q16 <=
                INT32_C(6) * PF_Q16_ONE)
        {
            attacker_buttons |= PF_INPUT_BUTTON_ATTACK;
            aerial_started = 1;
        }
        if (!pf_web_m4_tick_with_triggers(
                attacker_x,
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                target_x,
                target_y,
                UINT64_C(0),
                target_trigger,
                &inspection))
        {
            return 0;
        }
        if (aerial_started != 0 &&
            inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_followup_hitbox = 1;
        }
        if (inspection.players[1].last_hit_sequence !=
                launcher_sequence &&
            inspection.players[1].damage_q16 ==
                pf_web_m4_content.fighter.strong_damage_q16 +
                    pf_web_m4_content.fighter.aerial_damage_q16)
        {
            return escape_route == 0 && launched_airborne != 0 &&
                   inspection.players[1].grounded == UINT8_C(0);
        }
    }
    if (escape_route != 0 && launched_airborne != 0 &&
        air_dodge_started != 0 && saw_followup_hitbox != 0 &&
        inspection.players[1].damage_q16 ==
            pf_web_m4_content.fighter.strong_damage_q16 &&
        inspection.players[1].last_hit_sequence == launcher_sequence)
    {
        return 1;
    }
    return 0;
}

static int pf_web_m4_run_juggling_probe(void)
{
    return pf_web_m4_run_juggling_route(0) &&
           pf_web_m4_run_juggling_route(1);
}

static int pf_web_m4_initialize_ladder_fixture(void)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }

    pf_web_m4_content.fighter.aerial_hitbox_offset_x_q16 = INT32_C(0);
    pf_web_m4_content.fighter.aerial_hitbox_offset_y_q16 =
        -PF_Q16_ONE / INT32_C(4);
    pf_web_m4_content.fighter.aerial_hitbox_half_width_q16 = PF_Q16_ONE;
    pf_web_m4_content.fighter.aerial_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    pf_web_m4_content.fighter.aerial_damage_q16 =
        UINT32_C(4) * UINT32_C(65536);
    pf_web_m4_content.fighter.aerial_base_knockback_x_q16 = INT32_C(1);
    pf_web_m4_content.fighter.aerial_base_knockback_y_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.fighter.aerial_knockback_growth_q16 = INT32_C(1);
    pf_web_m4_content.fighter.aerial_startup_ticks = UINT16_C(1);
    pf_web_m4_content.fighter.aerial_active_ticks = UINT16_C(2);
    pf_web_m4_content.fighter.aerial_recovery_ticks = UINT16_C(2);
    pf_web_m4_content.fighter.aerial_hitlag_ticks = UINT16_C(3);
    pf_web_m4_content.fighter.platform_drop_ticks = UINT16_C(4);
    pf_web_m4_content.fighter.aerial_landing_lag_begin_tick = UINT16_C(1);
    pf_web_m4_content.fighter.aerial_landing_lag_end_tick = UINT16_C(4);
    pf_web_m4_content.fighter.strong_hitbox_offset_x_q16 = INT32_C(0);
    pf_web_m4_content.fighter.strong_hitbox_offset_y_q16 =
        -PF_Q16_ONE / INT32_C(4);
    pf_web_m4_content.fighter.strong_hitbox_half_width_q16 = PF_Q16_ONE;
    pf_web_m4_content.fighter.strong_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    pf_web_m4_content.fighter.strong_base_knockback_x_q16 = INT32_C(1);
    pf_web_m4_content.fighter.strong_base_knockback_y_q16 =
        (INT32_C(15) * PF_Q16_ONE) / INT32_C(16);
    pf_web_m4_content.fighter.strong_knockback_growth_q16 = INT32_C(1);
    pf_web_m4_content.fighter.strong_startup_ticks = UINT16_C(2);
    pf_web_m4_content.fighter.strong_active_ticks = UINT16_C(2);
    pf_web_m4_content.fighter.strong_recovery_ticks = UINT16_C(6);
    pf_web_m4_content.fighter.strong_hitlag_ticks = UINT16_C(4);
    pf_web_m4_content.fighter.hitstun_velocity_per_tick_q16 =
        PF_Q16_ONE / INT32_C(200);
    pf_web_m4_content.fighter.full_hop_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.fighter.double_jump_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.fighter.gravity_q16 =
        PF_Q16_ONE / INT32_C(100);
    pf_web_m4_content.fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(600);
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.stage.solid_left_q16 =
        INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.solid_right_q16 =
        INT32_C(30) * PF_Q16_ONE;
    pf_web_m4_content.stage.blast_top_q16 = INT32_C(4) * PF_Q16_ONE;
    pf_web_m4_content.stage.revival_platform_start_y_q16 =
        INT32_C(5) * PF_Q16_ONE;
    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_run_ladder_route(
    int16_t target_di_x,
    int expect_ko)
{
    pf_m4_inspection inspection;
    uint32_t last_sequence = UINT32_C(0);
    uint32_t hit_count = UINT32_C(0);
    uint32_t light_attack_starts = UINT32_C(0);
    uint32_t tick;
    int32_t first_hit_y_q16 = INT32_MAX;
    int chain_started = 0;
    int chain_broken = 0;
    int double_jump_used = 0;
    int strong_started = 0;
    int saw_escape_followup_hitbox = 0;
    int saw_above_platform = 0;
    int saw_vertical_carry = 0;

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(8) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(480); ++tick)
    {
        uint64_t attacker_buttons = UINT64_C(0);
        int16_t defender_axis;

        if (chain_started != 0 &&
            inspection.players[1].respawn_count == UINT16_C(0) &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITSTUN)
        {
            if (expect_ko != 0)
            {
                return 0;
            }
            chain_broken = 1;
        }
        if (expect_ko != 0 && chain_started != 0 &&
            inspection.players[0].grounded != UINT8_C(0))
        {
            return 0;
        }
        defender_axis =
            chain_started != 0 && chain_broken == 0
                ? target_di_x
                : INT16_C(0);
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AIRBORNE)
        {
            if (hit_count == UINT32_C(2) &&
                double_jump_used == 0)
            {
                attacker_buttons = PF_INPUT_BUTTON_JUMP;
                double_jump_used = 1;
            }
            else if (hit_count < UINT32_C(3))
            {
                attacker_buttons = PF_INPUT_BUTTON_ATTACK;
                ++light_attack_starts;
            }
            else if (strong_started == 0)
            {
                attacker_buttons = PF_INPUT_BUTTON_STRONG_ATTACK;
                strong_started = 1;
            }
        }

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                defender_axis,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }

        if (inspection.players[1].last_hit_sequence != last_sequence &&
            inspection.players[1].last_hit_sequence != UINT32_C(0))
        {
            last_sequence =
                inspection.players[1].last_hit_sequence;
            ++hit_count;
            chain_started = 1;
            if (hit_count == UINT32_C(1))
            {
                first_hit_y_q16 =
                    inspection.players[1].position_y_q16;
            }
            else if (
                inspection.players[1].position_y_q16 <=
                first_hit_y_q16 - INT32_C(2) * PF_Q16_ONE)
            {
                saw_vertical_carry = 1;
            }
            if (inspection.players[1].position_y_q16 <
                pf_web_m4_content.stage.platform_y_q16)
            {
                saw_above_platform = 1;
            }
        }

        if (expect_ko == 0 &&
            light_attack_starts > hit_count &&
            inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_escape_followup_hitbox = 1;
        }
        if (expect_ko == 0 && chain_broken != 0 &&
            saw_escape_followup_hitbox != 0 &&
            inspection.players[1].respawn_count == UINT16_C(0))
        {
            return hit_count > UINT32_C(0) &&
                   hit_count < UINT32_C(3) &&
                   strong_started == 0 &&
                   inspection.players[1].damage_q16 ==
                       pf_web_m4_expected_repeated_move_damage_q16(
                           &pf_web_m4_content.fighter,
                           pf_web_m4_content.fighter
                               .aerial_damage_q16,
                           hit_count);
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            return expect_ko != 0 && strong_started != 0 &&
                   hit_count == UINT32_C(4) &&
                   double_jump_used != 0 &&
                   saw_above_platform != 0 &&
                   saw_vertical_carry != 0 &&
                   inspection.players[1].damage_q16 == UINT32_C(0) &&
                   pf_web_m4_last_result.event_count == UINT8_C(2) &&
                   pf_web_m4_last_result.events[0].type ==
                       (uint16_t)PF_SIM_EVENT_KO &&
                   pf_web_m4_last_result.events[0].source_player ==
                       UINT8_C(0) &&
                   pf_web_m4_last_result.events[0].target_player ==
                       UINT8_C(1) &&
                   pf_web_m4_last_result.events[0].value_q16 ==
                       pf_web_m4_expected_repeated_move_damage_q16(
                           &pf_web_m4_content.fighter,
                           pf_web_m4_content.fighter
                               .aerial_damage_q16,
                           UINT32_C(3)) +
                           pf_web_m4_content.fighter
                               .strong_damage_q16;
        }
    }
    return 0;
}

static int pf_web_m4_run_ladder_probe(void)
{
    int passed = 0;

    if (pf_web_m4_initialize_ladder_fixture())
    {
        passed =
            pf_web_m4_run_ladder_route(
                INT16_C(0),
                1) &&
            pf_web_m4_run_ladder_route(
                PF_WEB_M4_DASH_AXIS,
                0);
    }
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content())
    {
        return 0;
    }
    return passed;
}

static int pf_web_m4_initialize_kill_confirm_fixture(void)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }

    pf_web_m4_content.fighter.jab_base_knockback_x_q16 = INT32_C(1);
    pf_web_m4_content.fighter.jab_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(5);
    pf_web_m4_content.fighter.jab_knockback_growth_q16 = INT32_C(1);
    pf_web_m4_content.fighter.strong_base_knockback_x_q16 =
        PF_Q16_ONE / INT32_C(20);
    pf_web_m4_content.fighter.strong_base_knockback_y_q16 =
        (INT32_C(37) * PF_Q16_ONE) / INT32_C(40);
    pf_web_m4_content.fighter.hitstun_velocity_per_tick_q16 =
        PF_Q16_ONE / INT32_C(200);
    pf_web_m4_content.fighter.jab_recovery_ticks = UINT16_C(3);
    pf_web_m4_content.fighter.jab_combo_input_end_tick = UINT16_C(6);
    pf_web_m4_content.fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(600);
    pf_web_m4_content.stage.floor_left_q16 =
        -INT32_C(60) * PF_Q16_ONE;
    pf_web_m4_content.stage.floor_right_q16 =
        INT32_C(60) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(30) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.stage.solid_left_q16 =
        -INT32_C(55) * PF_Q16_ONE;
    pf_web_m4_content.stage.solid_right_q16 =
        -INT32_C(45) * PF_Q16_ONE;
    pf_web_m4_content.stage.blast_left_q16 =
        -INT32_C(64) * PF_Q16_ONE;
    pf_web_m4_content.stage.blast_right_q16 =
        INT32_C(64) * PF_Q16_ONE;
    pf_web_m4_content.stage.blast_top_q16 =
        INT32_C(8) * PF_Q16_ONE;
    pf_web_m4_content.stage.revival_platform_start_y_q16 =
        INT32_C(9) * PF_Q16_ONE;
    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_wait_for_kill_confirm_neutral(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(160); ++tick)
    {
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[1].grounded != UINT8_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(160))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(320); ++tick)
    {
        const int32_t gap =
            out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16;
        const int16_t attacker_axis =
            gap > (INT32_C(3) * PF_Q16_ONE) / INT32_C(2)
                ? PF_WEB_M4_WALK_AXIS
                : INT16_C(0);

        if (attacker_axis == INT16_C(0) &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            gap > PF_Q16_ONE / INT32_C(2) &&
            gap < (INT32_C(9) * PF_Q16_ONE) / INT32_C(5))
        {
            return 1;
        }
        if (!pf_web_m4_tick(
                attacker_axis,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_kill_confirm_route(
    uint32_t buildup_jabs,
    int16_t target_di_x,
    int16_t target_di_y,
    int expect_ko)
{
    pf_m4_inspection inspection;
    uint32_t setup_sequence;
    uint32_t tick;
    uint32_t jab_index;
    int strong_started = 0;
    int finisher_hit = 0;
    int defender_escaped = 0;
    int saw_strong_hitbox = 0;

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK)
    {
        return 0;
    }

    for (jab_index = UINT32_C(0);
         jab_index < buildup_jabs;
         ++jab_index)
    {
        const uint32_t previous_sequence =
            inspection.players[1].last_hit_sequence;
        const uint32_t expected_damage =
            pf_web_m4_expected_repeated_move_damage_q16(
                &pf_web_m4_content.fighter,
                pf_web_m4_content.fighter.jab_damage_q16,
                jab_index + UINT32_C(1));

        if (!pf_web_m4_wait_for_kill_confirm_neutral(&inspection) ||
            !pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
        {
            if (inspection.players[1].last_hit_sequence !=
                previous_sequence)
            {
                break;
            }
            if (!pf_web_m4_tick(
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection))
            {
                return 0;
            }
        }
        if (tick == UINT32_C(32) ||
            inspection.players[1].damage_q16 != expected_damage)
        {
            return 0;
        }
    }

    if (!pf_web_m4_wait_for_kill_confirm_neutral(&inspection))
    {
        return 0;
    }
    setup_sequence = inspection.players[1].last_hit_sequence;
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (inspection.players[1].last_hit_sequence != setup_sequence)
        {
            setup_sequence =
                inspection.players[1].last_hit_sequence;
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(32) || setup_sequence == UINT32_C(0) ||
        inspection.players[1].damage_q16 !=
            pf_web_m4_expected_repeated_move_damage_q16(
                &pf_web_m4_content.fighter,
                pf_web_m4_content.fighter.jab_damage_q16,
                buildup_jabs + UINT32_C(1)))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        uint64_t attacker_buttons = UINT64_C(0);

        if (strong_started == 0 &&
            inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            attacker_buttons = PF_INPUT_BUTTON_STRONG_ATTACK;
            strong_started = 1;
        }
        if (finisher_hit == 0 &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITSTUN)
        {
            if (target_di_x == INT16_C(0) &&
                target_di_y == INT16_C(0))
            {
                return 0;
            }
            defender_escaped = 1;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                target_di_x,
                target_di_y,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].last_hit_sequence !=
            setup_sequence)
        {
            finisher_hit = 1;
        }
        if (strong_started != 0 &&
            inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_strong_hitbox = 1;
        }
        if (expect_ko == 0 && target_di_x != INT16_C(0) &&
            defender_escaped != 0 &&
            saw_strong_hitbox != 0 && finisher_hit == 0 &&
            inspection.players[0].hitbox_active == UINT8_C(0) &&
            inspection.players[1].damage_q16 ==
                pf_web_m4_expected_repeated_move_damage_q16(
                    &pf_web_m4_content.fighter,
                    pf_web_m4_content.fighter.jab_damage_q16,
                    buildup_jabs + UINT32_C(1)) &&
            inspection.players[1].respawn_count == UINT16_C(0))
        {
            return 1;
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            return expect_ko != 0 && finisher_hit != 0 &&
                   pf_web_m4_last_result.event_count == UINT8_C(2) &&
                   pf_web_m4_last_result.events[0].type ==
                       (uint16_t)PF_SIM_EVENT_KO &&
                   pf_web_m4_last_result.events[0].source_player ==
                       UINT8_C(0) &&
                   pf_web_m4_last_result.events[0].target_player ==
                       UINT8_C(1) &&
                   pf_web_m4_last_result.events[0].value_q16 ==
                       pf_web_m4_expected_repeated_move_damage_q16(
                           &pf_web_m4_content.fighter,
                           pf_web_m4_content.fighter.jab_damage_q16,
                           buildup_jabs + UINT32_C(1)) +
                           pf_web_m4_content.fighter.strong_damage_q16;
        }
        if (expect_ko == 0 && finisher_hit != 0 &&
            inspection.players[1].grounded != UINT8_C(0) &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            return 1;
        }
    }
    return 0;
}

static int pf_web_m4_run_kill_confirm_probe(void)
{
    int passed = 0;

    if (pf_web_m4_initialize_kill_confirm_fixture())
    {
        passed =
            pf_web_m4_run_kill_confirm_route(
                UINT32_C(20),
                INT16_C(0),
                INT16_C(0),
                1) &&
            pf_web_m4_run_kill_confirm_route(
                UINT32_C(0),
                INT16_C(0),
                INT16_C(0),
                0) &&
            pf_web_m4_run_kill_confirm_route(
                UINT32_C(20),
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                0);
    }
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content())
    {
        return 0;
    }
    return passed;
}

static int pf_web_m4_run_zero_to_death_route(
    int16_t target_di_x,
    int expect_ko)
{
    pf_m4_inspection inspection;
    uint32_t last_sequence = UINT32_C(0);
    uint32_t hit_count = UINT32_C(0);
    uint32_t tick;
    int chain_started = 0;
    int chain_broken = 0;
    int strong_started = 0;
    int saw_post_escape_hitbox = 0;

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[1].respawn_count != UINT16_C(0))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(1200); ++tick)
    {
        uint64_t attacker_buttons = UINT64_C(0);
        int16_t defender_axis;

        if (chain_started != 0 &&
            inspection.players[1].respawn_count == UINT16_C(0) &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITSTUN)
        {
            if (expect_ko != 0)
            {
                return 0;
            }
            chain_broken = 1;
        }
        defender_axis =
            chain_started != 0 && chain_broken == 0
                ? target_di_x
                : INT16_C(0);
        if (inspection.players[0].grounded != UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            if (hit_count < UINT32_C(21))
            {
                attacker_buttons = PF_INPUT_BUTTON_ATTACK;
            }
            else if (strong_started == 0)
            {
                attacker_buttons = PF_INPUT_BUTTON_STRONG_ATTACK;
                strong_started = 1;
            }
        }

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                defender_axis,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }

        if (inspection.players[1].last_hit_sequence != last_sequence &&
            inspection.players[1].last_hit_sequence != UINT32_C(0))
        {
            last_sequence =
                inspection.players[1].last_hit_sequence;
            ++hit_count;
            chain_started = 1;
        }

        if (expect_ko == 0 && chain_broken != 0 &&
            inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_post_escape_hitbox = 1;
        }
        if (expect_ko == 0 && chain_broken != 0 &&
            saw_post_escape_hitbox != 0 &&
            inspection.players[0].hitbox_active == UINT8_C(0) &&
            inspection.players[1].respawn_count == UINT16_C(0))
        {
            return hit_count > UINT32_C(0) &&
                   hit_count < UINT32_C(21) &&
                   strong_started == 0 &&
                   inspection.players[1].damage_q16 ==
                       pf_web_m4_expected_repeated_move_damage_q16(
                           &pf_web_m4_content.fighter,
                           pf_web_m4_content.fighter.jab_damage_q16,
                           hit_count);
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            return expect_ko != 0 && strong_started != 0 &&
                   hit_count == UINT32_C(22) &&
                   inspection.players[1].damage_q16 == UINT32_C(0) &&
                   pf_web_m4_last_result.event_count == UINT8_C(2) &&
                   pf_web_m4_last_result.events[0].type ==
                       (uint16_t)PF_SIM_EVENT_KO &&
                   pf_web_m4_last_result.events[0].source_player ==
                       UINT8_C(0) &&
                   pf_web_m4_last_result.events[0].target_player ==
                       UINT8_C(1) &&
                   pf_web_m4_last_result.events[0].value_q16 ==
                       pf_web_m4_expected_repeated_move_damage_q16(
                           &pf_web_m4_content.fighter,
                           pf_web_m4_content.fighter.jab_damage_q16,
                           UINT32_C(21)) +
                           pf_web_m4_content.fighter
                               .strong_damage_q16;
        }
    }
    return 0;
}

static int pf_web_m4_run_zero_to_death_probe(void)
{
    int passed = 0;

    if (pf_web_m4_initialize_kill_confirm_fixture())
    {
        passed =
            pf_web_m4_run_zero_to_death_route(
                INT16_C(0),
                1) &&
            pf_web_m4_run_zero_to_death_route(
                PF_WEB_M4_DASH_AXIS,
                0);
    }
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content())
    {
        return 0;
    }
    return passed;
}

static int pf_web_m4_initialize_ledge_cancel_fixture(
    int center_control)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }

    pf_web_m4_content.stage.platform_center_x_q16 =
        center_control != 0
            ? -INT32_C(2) * PF_Q16_ONE
            : -(INT32_C(5) * PF_Q16_ONE) / INT32_C(2);
    pf_web_m4_content.stage.platform_half_width_q16 =
        center_control != 0
            ? INT32_C(3) * PF_Q16_ONE
            : PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.stage.spawn_spacing_q16 =
        INT32_C(2) * PF_Q16_ONE;
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_reach_platform_special_landing(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded == UINT8_C(0))
        {
            break;
        }
    }
    if (out_inspection->players[0].action_state !=
        (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const int32_t bottom =
            out_inspection->players[0].position_y_q16 +
            pf_web_m4_content.fighter.half_height_q16;
        const int32_t maximum_diagonal_drop =
            (pf_web_m4_content.fighter.air_dodge_speed_q16 *
             INT32_C(3)) /
            INT32_C(4);

        if (out_inspection->players[0].velocity_y_q16 >= INT32_C(0) &&
            bottom >=
                pf_web_m4_content.stage.platform_y_q16 -
                    maximum_diagonal_drop &&
            bottom <= pf_web_m4_content.stage.platform_y_q16)
        {
            break;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) ||
            !pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return tick != UINT32_C(120) &&
           out_inspection->players[0].grounded == UINT8_C(0) &&
           pf_web_m4_tick_with_triggers(
               PF_WEB_M4_DASH_AXIS,
               PF_WEB_M4_DASH_AXIS,
               UINT64_C(0),
               UINT16_MAX,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
           out_inspection->players[0].action_ticks == UINT16_C(0) &&
           out_inspection->players[0].grounded == UINT8_C(1) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[0].velocity_x_q16 > INT32_C(0);
}

static int pf_web_m4_run_ledge_cancel_probe(void)
{
    pf_m4_inspection inspection;
    int32_t landing_x;
    uint32_t tick;
    int passed = 0;

    if (pf_web_m4_initialize_ledge_cancel_fixture(0) &&
        pf_web_m4_reach_platform_special_landing(&inspection))
    {
        const int32_t platform_right =
            pf_web_m4_content.stage.platform_center_x_q16 +
            pf_web_m4_content.stage.platform_half_width_q16;

        landing_x = inspection.players[0].position_x_q16;
        passed = landing_x < platform_right &&
            pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE &&
            inspection.players[0].action_ticks == UINT16_C(0) &&
            inspection.players[0].grounded == UINT8_C(0) &&
            inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_NONE &&
            inspection.players[0].position_x_q16 > platform_right &&
            inspection.players[0].position_x_q16 > landing_x;
    }

    if (passed != 0 &&
        pf_web_m4_initialize_ledge_cancel_fixture(1) &&
        pf_web_m4_reach_platform_special_landing(&inspection))
    {
        for (tick = UINT32_C(1);
             tick <
                 (uint32_t)pf_web_m4_content.fighter
                     .special_landing_ticks;
             ++tick)
        {
            if (!pf_web_m4_tick_with_triggers(
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    &inspection) ||
                inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
                inspection.players[0].action_ticks != (uint16_t)tick ||
                inspection.players[0].grounded != UINT8_C(1) ||
                inspection.players[0].support !=
                    (uint8_t)PF_M4_SURFACE_PLATFORM)
            {
                passed = 0;
                break;
            }
        }
        if (passed != 0)
        {
            passed = pf_web_m4_tick_with_triggers(
                         INT16_C(0),
                         INT16_C(0),
                         UINT64_C(0),
                         UINT16_C(0),
                         INT16_C(0),
                         INT16_C(0),
                         UINT64_C(0),
                         UINT16_C(0),
                         &inspection) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                inspection.players[0].action_ticks == UINT16_C(0) &&
                inspection.players[0].grounded == UINT8_C(1) &&
                inspection.players[0].support ==
                    (uint8_t)PF_M4_SURFACE_PLATFORM;
        }
    }
    else
    {
        passed = 0;
    }

    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content())
    {
        return 0;
    }
    return passed;
}

static int pf_web_m4_capture_v_cancel_launch(
    int trigger_on_hit,
    int target_attacks,
    int preexisting_lockout,
    int32_t *out_velocity_x_q16,
    int32_t *out_velocity_y_q16,
    uint16_t *out_hitstun_ticks,
    uint16_t *out_tech_lockout_ticks,
    uint8_t *out_trigger_input_age)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(20) &&
         (inspection.players[0].velocity_x_q16 != INT32_C(0) ||
          inspection.players[1].velocity_x_q16 != INT32_C(0) ||
          inspection.players[0].action_state !=
              (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
          inspection.players[1].action_state !=
              (uint8_t)PF_M4_ACTION_GROUND_IDLE);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (preexisting_lockout != 0 &&
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            target_attacks != 0
                ? PF_INPUT_BUTTON_STRONG_ATTACK
                : UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(12) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        const uint16_t target_trigger =
            trigger_on_hit != 0 &&
                    inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
                    inspection.players[0].action_ticks + UINT16_C(1) ==
                        pf_web_m4_content.fighter.aerial_startup_ticks
                ? UINT16_MAX
                : UINT16_C(0);

        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                target_trigger,
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_last_result.event_count;
         ++tick)
    {
        const pf_sim_event *event =
            &pf_web_m4_last_result.events[tick];

        if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            *out_velocity_x_q16 = event->velocity_x_q16;
            *out_velocity_y_q16 = event->velocity_y_q16;
            *out_hitstun_ticks =
                inspection.players[1].hitstun_ticks;
            *out_tech_lockout_ticks =
                inspection.players[1].tech_lockout_ticks;
            *out_trigger_input_age =
                inspection.players[1].trigger_input_age;
            return inspection.players[1].action_state ==
                   (uint8_t)PF_M4_ACTION_HITLAG;
        }
    }
    return 0;
}

static int pf_web_m4_run_v_cancel_probe(void)
{
    int32_t ordinary_x;
    int32_t ordinary_y;
    int32_t cancelled_x;
    int32_t cancelled_y;
    int32_t attacking_x;
    int32_t attacking_y;
    int32_t locked_x;
    int32_t locked_y;
    int32_t expected_x;
    int32_t expected_y;
    uint16_t ordinary_hitstun;
    uint16_t cancelled_hitstun;
    uint16_t ignored_hitstun;
    uint16_t ordinary_lockout;
    uint16_t cancelled_lockout;
    uint16_t attacking_lockout;
    uint16_t locked_lockout;
    uint8_t ordinary_age;
    uint8_t cancelled_age;
    uint8_t attacking_age;
    uint8_t locked_age;

    if (pf_web_m4_content.fighter.v_cancel_velocity_scale_q16 !=
            (INT32_C(95) * PF_Q16_ONE) / INT32_C(100) ||
        pf_web_m4_content.fighter.v_cancel_window_ticks !=
            UINT16_C(3) ||
        !pf_web_m4_capture_v_cancel_launch(
            0,
            0,
            0,
            &ordinary_x,
            &ordinary_y,
            &ordinary_hitstun,
            &ordinary_lockout,
            &ordinary_age) ||
        !pf_web_m4_capture_v_cancel_launch(
            1,
            0,
            0,
            &cancelled_x,
            &cancelled_y,
            &cancelled_hitstun,
            &cancelled_lockout,
            &cancelled_age) ||
        !pf_web_m4_capture_v_cancel_launch(
            1,
            1,
            0,
            &attacking_x,
            &attacking_y,
            &ignored_hitstun,
            &attacking_lockout,
            &attacking_age) ||
        !pf_web_m4_capture_v_cancel_launch(
            1,
            0,
            1,
            &locked_x,
            &locked_y,
            &ignored_hitstun,
            &locked_lockout,
            &locked_age))
    {
        return 0;
    }
    expected_x = (int32_t)(
        ((int64_t)ordinary_x *
         (int64_t)pf_web_m4_content.fighter
             .v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_y = (int32_t)(
        ((int64_t)ordinary_y *
         (int64_t)pf_web_m4_content.fighter
             .v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    return ordinary_x > INT32_C(0) &&
           ordinary_y < INT32_C(0) &&
           ordinary_age == UINT8_MAX &&
           ordinary_lockout == UINT16_C(0) &&
           cancelled_x == expected_x &&
           cancelled_y == expected_y &&
           cancelled_hitstun == ordinary_hitstun &&
           cancelled_age == UINT8_C(0) &&
           cancelled_lockout == UINT16_C(40) &&
           attacking_x == ordinary_x &&
           attacking_y == ordinary_y &&
           attacking_age == UINT8_C(0) &&
           attacking_lockout == UINT16_C(40) &&
           locked_x == ordinary_x &&
           locked_y == ordinary_y &&
           locked_age == UINT8_C(0) &&
           locked_lockout < UINT16_C(40);
}

static int pf_web_m4_prepare_spacing_distance(
    int distance_mode,
    pf_m4_inspection *out_inspection)
{
    const int32_t jab_reach =
        pf_web_m4_content.fighter.jab_hitbox_offset_x_q16 +
        pf_web_m4_content.fighter.jab_hitbox_half_width_q16 +
        pf_web_m4_content.fighter.half_width_q16;
    const int32_t strong_reach =
        pf_web_m4_content.fighter.strong_hitbox_offset_x_q16 +
        pf_web_m4_content.fighter.strong_hitbox_half_width_q16 +
        pf_web_m4_content.fighter.half_width_q16;
    int32_t target_distance;
    int32_t brake_distance;
    int32_t distance;
    uint32_t tick;

    if (out_inspection == NULL || !pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, out_inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    if (distance_mode == 0)
    {
        target_distance = jab_reach +
            (strong_reach - jab_reach) / INT32_C(2);
    }
    else if (distance_mode == 1)
    {
        target_distance = jab_reach - PF_Q16_ONE / INT32_C(5);
    }
    else
    {
        target_distance =
            strong_reach +
            (INT32_C(3) * PF_Q16_ONE) / INT32_C(10);
    }
    brake_distance =
        target_distance + pf_web_m4_content.fighter.walk_speed_q16;

    for (tick = UINT32_C(0); tick < UINT32_C(400); ++tick)
    {
        const int16_t walk_input =
            out_inspection->players[1].position_x_q16 -
                    out_inspection->players[0].position_x_q16 >
                brake_distance
                ? PF_WEB_M4_WALK_AXIS
                : INT16_C(0);

        if (!pf_web_m4_tick(
                walk_input,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (walk_input == INT16_C(0) &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(400) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return 0;
    }
    distance =
        out_inspection->players[1].position_x_q16 -
        out_inspection->players[0].position_x_q16;
    if (distance_mode == 0)
    {
        return distance > jab_reach && distance <= strong_reach;
    }
    if (distance_mode == 1)
    {
        return distance <= jab_reach;
    }
    return distance > strong_reach;
}

static int pf_web_m4_run_spacing_exchange(int distance_mode)
{
    pf_m4_inspection inspection;
    uint32_t tick;
    int saw_strong_hitbox = 0;

    if (!pf_web_m4_prepare_spacing_distance(
            distance_mode,
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].hitbox_active != UINT8_C(0) ||
            inspection.players[0].damage_q16 != UINT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(8))
    {
        return 0;
    }
    if (distance_mode == 1)
    {
        return inspection.players[0].damage_q16 ==
                   pf_web_m4_content.fighter.jab_damage_q16 &&
               inspection.players[0].last_hit_attacker == UINT8_C(1) &&
               inspection.players[1].damage_q16 == UINT32_C(0);
    }
    if (inspection.players[0].damage_q16 != UINT32_C(0) ||
        inspection.players[1].hitbox_active == UINT8_C(0) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(14); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_strong_hitbox = 1;
        }
        if (inspection.players[1].damage_q16 != UINT32_C(0))
        {
            break;
        }
    }
    if (distance_mode == 0)
    {
        return inspection.players[0].damage_q16 == UINT32_C(0) &&
               inspection.players[1].damage_q16 ==
                   pf_web_m4_content.fighter.strong_damage_q16 &&
               inspection.players[1].last_hit_attacker == UINT8_C(0);
    }
    return saw_strong_hitbox != 0 &&
           inspection.players[0].damage_q16 == UINT32_C(0) &&
           inspection.players[1].damage_q16 == UINT32_C(0);
}

static int pf_web_m4_run_spacing_shield_control(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_prepare_spacing_distance(0, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(6); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(5)
                    ? PF_INPUT_BUTTON_STRONG_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(14); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_HITLAG)
        {
            break;
        }
    }
    return tick < UINT32_C(14) &&
           inspection.players[1].damage_q16 == UINT32_C(0) &&
           inspection.players[1].shield_health_q16 <
               pf_web_m4_content.fighter.shield_health_q16 &&
           inspection.players[1].powershield == UINT8_C(0) &&
           pf_web_m4_last_result.event_count == UINT8_C(2) &&
           pf_web_m4_last_result.events[0].type ==
               (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK;
}

static int pf_web_m4_run_approach_probe(void)
{
    return pf_web_m4_run_spacing_exchange(0);
}

static int pf_web_m4_run_spacing_probe(int safe_exchange_passed)
{
    return safe_exchange_passed != 0 &&
           pf_web_m4_run_spacing_exchange(1) &&
           pf_web_m4_run_spacing_exchange(2) &&
           pf_web_m4_run_spacing_shield_control();
}

static int pf_web_m4_run_ground_dodge_probe(void)
{
    pf_m4_inspection inspection;
    int8_t facing;

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(
            pf_web_m4_sim,
            &inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    facing = inspection.players[0].facing;
    if (!pf_web_m4_tick_with_triggers(
            facing == INT8_C(1)
                ? PF_WEB_M4_DASH_AXIS
                : -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.players[0].facing != (int8_t)-facing)
    {
        return 0;
    }
    while (inspection.players[0].action_ticks <
           pf_web_m4_content.fighter
               .roll_invulnerability_begin_tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].invulnerable != UINT8_C(1) ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            facing == INT8_C(1)
                ? -PF_WEB_M4_DASH_AXIS
                : PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_BACKWARD ||
        inspection.players[0].facing != facing ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE)
    {
        return 0;
    }
    while (inspection.players[0].action_ticks <
           pf_web_m4_content.fighter
               .spot_dodge_invulnerability_begin_tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].invulnerable != UINT8_C(1) ||
        inspection.players[0].facing != facing ||
        !pf_web_m4_reset_internal() ||
        pf_m4_inspect(
            pf_web_m4_sim,
            &inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    facing = inspection.players[0].facing;
    if (!pf_web_m4_tick_with_dual_triggers(
            INT16_C(0),
            INT16_C(0),
            facing == INT8_C(1)
                ? PF_WEB_M4_DASH_AXIS
                : -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !pf_web_m4_tick_with_dual_triggers(
            INT16_C(0),
            INT16_C(0),
            facing == INT8_C(1)
                ? PF_WEB_M4_DASH_AXIS
                : -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.players[0].facing != (int8_t)-facing ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_dual_triggers(
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !pf_web_m4_tick_with_dual_triggers(
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_dual_triggers(
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            -PF_WEB_M4_DASH_AXIS,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        !pf_web_m4_tick_with_dual_triggers(
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            -PF_WEB_M4_DASH_AXIS,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_MAX,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[0].action_state ==
        (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
}

static int pf_web_m4_run_air_facing_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].facing != INT8_C(1) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(1) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].grounded != UINT8_C(0) ||
            inspection.players[0].facing != INT8_C(1))
        {
            return 0;
        }
    }
    if (inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           inspection.players[0].grounded == UINT8_C(0) &&
           inspection.players[0].facing == INT8_C(1) &&
           inspection.players[0].velocity_x_q16 >=
               -pf_web_m4_content.fighter.air_acceleration_q16 &&
           inspection.players[0].velocity_x_q16 <=
               pf_web_m4_content.fighter.air_acceleration_q16;
}

static int pf_web_m4_run_instant_double_jump_probe(void)
{
    pf_m4_inspection inspection;
    int32_t launch_y;
    const int32_t expected_velocity_y =
        -pf_web_m4_content.fighter.double_jump_speed_q16 +
        pf_web_m4_content.fighter.gravity_q16;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(1))
    {
        return 0;
    }

    launch_y = inspection.players[0].position_y_q16;
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        inspection.players[0].velocity_y_q16 !=
            expected_velocity_y ||
        inspection.players[0].position_y_q16 !=
            launch_y + expected_velocity_y)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return inspection.players[0].grounded == UINT8_C(0) &&
           inspection.players[0].air_jumps_remaining == UINT8_C(1);
}

static int pf_web_m4_enter_double_jump_cancel_window(
    pf_m4_inspection *out_inspection)
{
    const int32_t expected_velocity_y =
        -pf_web_m4_content.fighter.double_jump_speed_q16 +
        pf_web_m4_content.fighter.gravity_q16;

    return out_inspection != NULL &&
           pf_web_m4_reset_internal() &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_JUMP,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           out_inspection->players[0].air_jumps_remaining ==
               UINT8_C(1) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_JUMP,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].grounded == UINT8_C(0) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP &&
           out_inspection->players[0].action_ticks == UINT16_C(0) &&
           out_inspection->players[0].air_jumps_remaining ==
               UINT8_C(0) &&
           out_inspection->players[0].velocity_y_q16 ==
               expected_velocity_y;
}

static int pf_web_m4_run_double_jump_cancel_probe(void)
{
    pf_m4_inspection inspection;
    int32_t before_cancel_position_y;
    int32_t before_late_velocity_y;
    int32_t before_simultaneous_velocity_y;
    uint32_t cancel_landing_tick = UINT32_C(2);
    uint32_t late_landing_tick;
    uint32_t tick;

    if (pf_web_m4_content.fighter.double_jump_cancel_ticks !=
            UINT16_C(6) ||
        !pf_web_m4_enter_double_jump_cancel_window(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        return 0;
    }

    before_cancel_position_y = inspection.players[0].position_y_q16;
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].velocity_y_q16 !=
            pf_web_m4_content.fighter.gravity_q16 ||
        inspection.players[0].position_y_q16 !=
            before_cancel_position_y +
                pf_web_m4_content.fighter.gravity_q16)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        ++cancel_landing_tick;
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        !pf_web_m4_enter_double_jump_cancel_window(&inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter
                    .double_jump_cancel_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    before_late_velocity_y = inspection.players[0].velocity_y_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        before_late_velocity_y >= INT32_C(0) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].velocity_y_q16 !=
            before_late_velocity_y +
                pf_web_m4_content.fighter.gravity_q16 ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0))
    {
        return 0;
    }
    late_landing_tick =
        (uint32_t)pf_web_m4_content.fighter
            .double_jump_cancel_ticks +
        UINT32_C(1);
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        ++late_landing_tick;
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        late_landing_tick <= cancel_landing_tick ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        return 0;
    }
    before_simultaneous_velocity_y =
        inspection.players[0].velocity_y_q16;
    return pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           inspection.players[0].air_jumps_remaining == UINT8_C(1) &&
           inspection.players[0].velocity_y_q16 ==
               before_simultaneous_velocity_y +
                   pf_web_m4_content.fighter.gravity_q16;
}

static int pf_web_m4_launch_double_jump_counter_pair(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (out_inspection->players[0].grounded != UINT8_C(0) ||
          out_inspection->players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].grounded == UINT8_C(0) &&
           out_inspection->players[1].grounded == UINT8_C(0);
}

static int pf_web_m4_run_double_jump_cancel_counter_route(void)
{
    pf_m4_inspection inspection;
    const pf_m4_fighter_data *fighter = &pf_web_m4_content.fighter;
    const pf_sim_event *event;
    int32_t frozen_position_x;
    int32_t frozen_position_y;
    int32_t frozen_velocity_x;
    int32_t frozen_velocity_y;
    uint32_t tick;

    if (fighter->double_jump_armor_max_hitstun_ticks !=
            UINT16_C(20) ||
        !pf_web_m4_launch_double_jump_counter_pair(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }

    event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
    frozen_position_x = inspection.players[1].position_x_q16;
    frozen_position_y = inspection.players[1].position_y_q16;
    frozen_velocity_x = inspection.players[1].velocity_x_q16;
    frozen_velocity_y = inspection.players[1].velocity_y_q16;
    if (event == NULL ||
        event->source_player != UINT8_C(0) ||
        event->target_player != UINT8_C(1) ||
        event->value_q16 != fighter->aerial_damage_q16 ||
        event->velocity_x_q16 != INT32_C(0) ||
        event->velocity_y_q16 != INT32_C(0) ||
        event->flags != UINT16_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].hitlag_ticks !=
            fighter->aerial_hitlag_ticks ||
        inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            fighter->aerial_damage_q16 ||
        inspection.players[1].air_jumps_remaining != UINT8_C(0) ||
        frozen_velocity_y !=
            -fighter->double_jump_speed_q16 + fighter->gravity_q16)
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)fighter->aerial_hitlag_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[1].position_x_q16 !=
                frozen_position_x ||
            inspection.players[1].position_y_q16 !=
                frozen_position_y ||
            inspection.players[1].velocity_x_q16 !=
                frozen_velocity_x ||
            inspection.players[1].velocity_y_q16 !=
                frozen_velocity_y)
        {
            return 0;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 != frozen_velocity_y ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 != fighter->gravity_q16)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(4) &&
         inspection.players[0].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
    if (event == NULL ||
        event->source_player != UINT8_C(1) ||
        event->target_player != UINT8_C(0) ||
        inspection.players[0].damage_q16 !=
            fighter->aerial_damage_q16)
    {
        return 0;
    }

    if (!pf_web_m4_launch_double_jump_counter_pair(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)fighter->double_jump_cancel_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
    if (event == NULL ||
        inspection.players[1].hitstun_ticks != UINT16_C(16) ||
        event->velocity_x_q16 == INT32_C(0) ||
        event->velocity_y_q16 == INT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_launch_double_jump_counter_pair(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
           inspection.players[0].action_ticks <
               fighter->strong_startup_ticks &&
           inspection.players[1].damage_q16 == UINT32_C(0))
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].damage_q16 != UINT32_C(0) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
    return event != NULL &&
           event->source_player == UINT8_C(0) &&
           event->target_player == UINT8_C(1) &&
           event->velocity_x_q16 != INT32_C(0) &&
           event->velocity_y_q16 != INT32_C(0) &&
           (event->flags &
            (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE) != UINT16_C(0) &&
           inspection.players[1].damage_q16 ==
               fighter->strong_damage_q16 &&
           inspection.players[1].hitstun_ticks == UINT16_C(34) &&
           inspection.players[1].tumble == UINT8_C(1);
}

static int pf_web_m4_run_double_jump_cancel_counter_probe(void)
{
    int passed;
    int restored;

    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.fighter.aerial_startup_ticks = UINT16_C(1);
    pf_web_m4_content.fighter.platform_drop_ticks = UINT16_C(7);
    pf_web_m4_content.fighter.aerial_landing_lag_begin_tick =
        UINT16_C(1);
    pf_web_m4_content.fighter.aerial_hitbox_half_width_q16 = PF_Q16_ONE;
    pf_web_m4_content.fighter.aerial_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    pf_web_m4_content.fighter.strong_hitbox_half_width_q16 = PF_Q16_ONE;
    pf_web_m4_content.fighter.strong_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    passed =
        pf_web_m4_initialize_current_content() &&
        pf_web_m4_run_double_jump_cancel_counter_route();
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_grab_player0_right_ledge(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded != UINT8_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_RUN &&
            out_inspection->players[0].position_x_q16 >=
                out_inspection->stage.right_ledge_x_q16 -
                    PF_Q16_ONE / INT32_C(2))
        {
            break;
        }
    }
    if (tick == UINT32_C(240))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!pf_web_m4_tick(
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            return out_inspection->players[0].invulnerable ==
                UINT8_C(1);
        }
    }
    return 0;
}

static int pf_web_m4_initialize_planking_fixture(void)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_content.fighter.double_jump_speed_q16 =
        INT32_C(31) * PF_Q16_ONE / INT32_C(100);
    pf_web_m4_content.fighter.jab_hitbox_half_width_q16 =
        INT32_C(64) * PF_Q16_ONE;
    pf_web_m4_content.fighter.jab_hitbox_half_height_q16 =
        INT32_C(64) * PF_Q16_ONE;
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_run_planking_route(int mistimed)
{
    pf_m4_inspection inspection;
    uint32_t catch_ticks;
    uint32_t cycle;
    uint32_t tick;
    const uint32_t cycle_count =
        mistimed != 0 ? UINT32_C(1) : UINT32_C(3);

    if (!pf_web_m4_initialize_planking_fixture() ||
        !pf_web_m4_grab_player0_right_ledge(&inspection))
    {
        return 0;
    }
    catch_ticks =
        (uint32_t)pf_web_m4_content.fighter.landing_ticks +
        (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;

    for (cycle = UINT32_C(0); cycle < cycle_count; ++cycle)
    {
        for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
        {
            if (!pf_web_m4_tick(
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection))
            {
                return 0;
            }
        }
        if (inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
            inspection.players[0].action_ticks !=
                (uint16_t)catch_ticks ||
            !pf_web_m4_tick(
                INT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_AIRBORNE ||
            inspection.players[0].platform_drop_ticks != UINT8_C(0) ||
            inspection.players[0].ledge_regrab_lockout_ticks !=
                pf_web_m4_content.fighter.ledge_regrab_lockout_ticks)
        {
            return 0;
        }

        for (tick = UINT32_C(1);
             tick <= (uint32_t)pf_web_m4_content.fighter
                         .ledge_regrab_lockout_ticks;
             ++tick)
        {
            const int16_t player0_y =
                mistimed != 0 && tick >= UINT32_C(28)
                    ? PF_WEB_M4_DASH_AXIS
                    : INT16_C(0);
            const uint64_t player0_buttons =
                tick == UINT32_C(1)
                    ? PF_INPUT_BUTTON_JUMP
                    : UINT64_C(0);
            const uint64_t player1_buttons =
                tick == UINT32_C(26)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0);

            if (!pf_web_m4_tick(
                    INT16_C(0),
                    player0_y,
                    player0_buttons,
                    INT16_C(0),
                    INT16_C(0),
                    player1_buttons,
                    &inspection))
            {
                return 0;
            }
            if (tick < (uint32_t)pf_web_m4_content.fighter
                           .ledge_regrab_lockout_ticks &&
                (inspection.players[0].action_state ==
                     (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
                 inspection.players[0].ledge_regrab_lockout_ticks !=
                     (uint16_t)(
                         (uint32_t)pf_web_m4_content.fighter
                             .ledge_regrab_lockout_ticks -
                         tick)))
            {
                return 0;
            }
            if (mistimed == 0 &&
                tick + UINT32_C(1) ==
                    (uint32_t)pf_web_m4_content.fighter
                        .ledge_regrab_lockout_ticks &&
                (inspection.players[0].velocity_y_q16 < INT32_C(0) ||
                 inspection.players[0].position_y_q16 <
                     pf_web_m4_content.stage.floor_y_q16 -
                         pf_web_m4_content.fighter.half_height_q16 ||
                 inspection.players[0].position_y_q16 >
                     pf_web_m4_content.stage.floor_y_q16 +
                         pf_web_m4_content.fighter.half_height_q16 ||
                 inspection.players[0].position_x_q16 <=
                     pf_web_m4_content.stage.floor_right_q16 ||
                 (int64_t)inspection.players[0].position_x_q16 -
                         (int64_t)pf_web_m4_content.stage
                             .floor_right_q16 >
                     (int64_t)pf_web_m4_content.fighter
                             .half_width_q16 +
                         (int64_t)pf_web_m4_content.fighter
                             .air_speed_q16 ||
                 inspection.players[0].facing != INT8_C(-1)))
            {
                return 0;
            }
        }

        if (mistimed == 0)
        {
            if (inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
                inspection.players[0].ledge !=
                    (uint8_t)PF_M4_LEDGE_RIGHT ||
                inspection.players[0].ledge_regrab_lockout_ticks !=
                    UINT16_C(0) ||
                inspection.players[0].ledge_invulnerability_ticks !=
                    pf_web_m4_content.fighter
                        .ledge_invulnerability_ticks ||
                inspection.players[0].damage_q16 != UINT32_C(0))
            {
                return 0;
            }
        }
    }

    return mistimed == 0 ||
           (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_HITLAG &&
            inspection.players[0].ledge ==
                (uint8_t)PF_M4_LEDGE_NONE &&
            inspection.players[0].ledge_regrab_lockout_ticks ==
                UINT16_C(0) &&
            inspection.players[0].ledge_invulnerability_ticks ==
                UINT16_C(0) &&
            inspection.players[0].damage_q16 ==
                pf_web_m4_content.fighter.jab_damage_q16);
}

static int pf_web_m4_run_planking_probe(void)
{
    const int positive_passed = pf_web_m4_run_planking_route(0);
    const int negative_passed =
        positive_passed != 0
            ? pf_web_m4_run_planking_route(1)
            : 0;
    const int restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();

    return positive_passed != 0 &&
           negative_passed != 0 &&
           restored != 0;
}

static int pf_web_m4_run_jump_cancel_route(void)
{
    pf_m4_inspection inspection;
    const int16_t shallow_up =
        (int16_t)(
            -((int32_t)pf_web_m4_content.fighter
                  .dash_axis_threshold -
              INT32_C(1)));

    pf_web_m4_content.stage.spawn_spacing_q16 =
        INT32_C(4) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    if (!pf_web_m4_initialize_current_content() ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        inspection.players[0].action_ticks != UINT16_C(2))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            shallow_up,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        inspection.players[0].action_ticks != UINT16_C(2))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_MIN,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_UP_AERIAL ||
        inspection.players[0].grounded != UINT8_C(0))
    {
        return 0;
    }
    return 1;
}

static int pf_web_m4_run_jump_cancel_probe(void)
{
    const int route_passed = pf_web_m4_run_jump_cancel_route();
    const int restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();

    return route_passed != 0 && restored != 0;
}

static int pf_web_m4_run_jump_cancelled_grab_route(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;
    int capture_seen = 0;

    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(5) * PF_Q16_ONE) / INT32_C(4);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    if (!pf_web_m4_initialize_current_content() ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        pf_web_m4_last_result.event_count != UINT8_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (pf_web_m4_last_result.event_count == UINT8_C(2) &&
            pf_web_m4_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_GRAB &&
            pf_web_m4_last_result.events[0].source_player == UINT8_C(0) &&
            pf_web_m4_last_result.events[0].target_player == UINT8_C(1))
        {
            capture_seen = 1;
            break;
        }
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (capture_seen == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GRAB ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER)
    {
        return 0;
    }
    return 1;
}

static int pf_web_m4_run_jump_cancelled_grab_probe(void)
{
    const int route_passed =
        pf_web_m4_run_jump_cancelled_grab_route();
    const int restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();

    return route_passed != 0 && restored != 0;
}

static const pf_sim_event *pf_web_m4_find_event(
    pf_sim_event_type event_type)
{
    uint32_t event_index;

    for (event_index = UINT32_C(0);
         event_index < (uint32_t)pf_web_m4_last_result.event_count;
         ++event_index)
    {
        if (pf_web_m4_last_result.events[event_index].type ==
            (uint16_t)event_type)
        {
            return &pf_web_m4_last_result.events[event_index];
        }
    }
    return NULL;
}

static int pf_web_m4_prepare_jab_cancel_content(
    int32_t spawn_spacing_q16)
{
    pf_web_m4_content.stage.spawn_spacing_q16 = spawn_spacing_q16;
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.fighter.jab_base_knockback_x_q16 = INT32_C(1);
    pf_web_m4_content.fighter.jab_base_knockback_y_q16 = INT32_C(1);
    pf_web_m4_content.fighter.jab_knockback_growth_q16 = INT32_C(1);
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_advance_jab_to_action_tick(
    uint16_t target_action_tick,
    pf_m4_inspection *out_inspection,
    int *out_hit_seen)
{
    uint32_t tick;
    int hit_seen = 0;

    if (out_inspection == NULL ||
        out_hit_seen == NULL ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1) &&
            event->detail ==
                (uint16_t)PF_M4_ACTION_GROUND_ATTACK)
        {
            hit_seen = 1;
        }
        if (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
            out_inspection->players[0].action_ticks ==
                target_action_tick)
        {
            *out_hit_seen = hit_seen;
            return 1;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_jab_cancel_route(void)
{
    pf_m4_inspection inspection;
    const int32_t close_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    const int32_t far_spacing_q16 = INT32_C(4) * PF_Q16_ONE;
    uint32_t tick;
    int first_hit_seen = 0;

    if (pf_web_m4_content.fighter.jab_combo_input_begin_tick !=
            UINT16_C(4) ||
        pf_web_m4_content.fighter.jab_combo_input_end_tick !=
            UINT16_C(7) ||
        pf_web_m4_content.fighter.jab_final_startup_ticks !=
            UINT16_C(2) ||
        pf_web_m4_content.fighter.jab_final_active_ticks !=
            UINT16_C(2) ||
        pf_web_m4_content.fighter.jab_final_recovery_ticks !=
            UINT16_C(10) ||
        pf_web_m4_content.fighter.jab_final_hitlag_ticks !=
            UINT16_C(4) ||
        pf_web_m4_content.fighter.jab_final_damage_q16 !=
            UINT32_C(7) * UINT32_C(65536))
    {
        return 0;
    }

    if (!pf_web_m4_prepare_jab_cancel_content(close_spacing_q16) ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_jab_to_action_tick(
            pf_web_m4_content.fighter.jab_combo_input_begin_tick,
            &inspection,
            &first_hit_seen) ||
        first_hit_seen == 0 ||
        inspection.players[1].damage_q16 !=
            pf_web_m4_content.fighter.jab_damage_q16 ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        return 0;
    }

    if (!pf_web_m4_prepare_jab_cancel_content(far_spacing_q16) ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_jab_to_action_tick(
            pf_web_m4_content.fighter.jab_combo_input_end_tick,
            &inspection,
            &first_hit_seen) ||
        first_hit_seen != 0 ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_jab_to_action_tick(
            pf_web_m4_content.fighter.jab_combo_input_begin_tick -
                UINT16_C(1),
            &inspection,
            &first_hit_seen) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.jab_combo_input_begin_tick ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_jab_to_action_tick(
            pf_web_m4_content.fighter.jab_combo_input_end_tick +
                UINT16_C(1),
            &inspection,
            &first_hit_seen) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.jab_combo_input_end_tick +
                UINT16_C(2))
    {
        return 0;
    }

    if (!pf_web_m4_prepare_jab_cancel_content(close_spacing_q16) ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_jab_to_action_tick(
            pf_web_m4_content.fighter.jab_combo_input_begin_tick,
            &inspection,
            &first_hit_seen) ||
        first_hit_seen == 0 ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JAB_FINAL ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1) &&
            event->detail == (uint16_t)PF_M4_ACTION_JAB_FINAL &&
            event->value_q16 ==
                pf_web_m4_content.fighter.jab_final_damage_q16)
        {
            return inspection.players[1].damage_q16 ==
                   pf_web_m4_content.fighter.jab_damage_q16 +
                       pf_web_m4_content.fighter.jab_final_damage_q16;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_jab_cancel_probe(void)
{
    const int route_passed = pf_web_m4_run_jab_cancel_route();
    const int restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();

    return route_passed != 0 && restored != 0;
}

static int pf_web_m4_prepare_jab_reset_content(void)
{
    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.fighter.strong_base_knockback_x_q16 = INT32_C(1);
    pf_web_m4_content.fighter.strong_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(2);
    pf_web_m4_content.fighter.strong_knockback_growth_q16 = INT32_C(1);
    pf_web_m4_content.fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(13);
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_reach_jab_reset_down_wait(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    int hit_seen = 0;

    if (out_inspection == NULL ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1) &&
            event->detail ==
                (uint16_t)PF_M4_ACTION_STRONG_ATTACK)
        {
            hit_seen = 1;
        }
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_DOWN_WAIT)
        {
            return hit_seen != 0 &&
                   out_inspection->players[1].action_ticks ==
                       UINT16_C(0) &&
                   out_inspection->players[1].grounded == UINT8_C(1) &&
                   out_inspection->players[1].invulnerable == UINT8_C(0);
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_hit_down_wait_with_jab(
    pf_m4_inspection *out_inspection,
    pf_sim_event *out_event)
{
    uint32_t tick;

    if (out_inspection == NULL || out_event == NULL ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            *out_event = *event;
            return 1;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_advance_target_to_action(
    uint8_t expected_action,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        if (out_inspection->players[1].action_state == expected_action)
        {
            return 1;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_jab_reset_route(void)
{
    pf_m4_inspection inspection;
    pf_sim_event hit_event;
    const uint32_t jab_damage =
        pf_web_m4_content.fighter.jab_damage_q16;
    uint32_t tick;
    int airborne_seen = 0;
    int grounded_seen = 0;

    if (pf_web_m4_content.fighter.reset_max_damage_q16 !=
            UINT32_C(7) * UINT32_C(65536) ||
        pf_web_m4_content.fighter.reset_max_hitstun_ticks !=
            UINT16_C(12) ||
        pf_web_m4_content.fighter.reset_bound_ticks != UINT16_C(12) ||
        pf_web_m4_content.fighter.reset_forced_getup_ticks !=
            UINT16_C(30) ||
        pf_web_m4_content.fighter.reset_bound_speed_q16 !=
            PF_Q16_ONE / INT32_C(10) ||
        !pf_web_m4_prepare_jab_reset_content() ||
        !pf_web_m4_reach_jab_reset_down_wait(&inspection) ||
        !pf_web_m4_hit_down_wait_with_jab(&inspection, &hit_event) ||
        hit_event.detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK ||
        hit_event.value_q16 != jab_damage ||
        hit_event.velocity_x_q16 != INT32_C(0) ||
        hit_event.velocity_y_q16 !=
            -pf_web_m4_content.fighter.reset_bound_speed_q16 ||
        inspection.players[1].hitstun_ticks !=
            pf_web_m4_content.fighter.reset_max_hitstun_ticks ||
        inspection.players[1].tumble != UINT8_C(0) ||
        !pf_web_m4_advance_target_to_action(
            (uint8_t)PF_M4_ACTION_RESET_BOUND,
            &inspection) ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].grounded != UINT8_C(0))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <=
             (uint32_t)pf_web_m4_content.fighter.reset_bound_ticks;
         ++tick)
    {
        const uint64_t target_buttons =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : PF_INPUT_BUTTON_JUMP;

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                target_buttons,
                &inspection))
        {
            return 0;
        }
        airborne_seen |=
            inspection.players[1].grounded == UINT8_C(0);
        grounded_seen |=
            inspection.players[1].grounded != UINT8_C(0);
        if (tick <
                (uint32_t)pf_web_m4_content.fighter
                    .reset_bound_ticks &&
            (inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_RESET_BOUND ||
             inspection.players[1].action_ticks != (uint16_t)tick))
        {
            return 0;
        }
    }
    if (airborne_seen == 0 || grounded_seen == 0 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_FORCED_GETUP ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(0))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <= (uint32_t)pf_web_m4_content.fighter
                     .reset_forced_getup_ticks;
         ++tick)
    {
        const uint64_t target_buttons =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : PF_INPUT_BUTTON_JUMP;

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                target_buttons,
                &inspection))
        {
            return 0;
        }
        if (tick < (uint32_t)pf_web_m4_content.fighter
                       .reset_forced_getup_ticks &&
            (inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_FORCED_GETUP ||
             inspection.players[1].action_ticks != (uint16_t)tick ||
             inspection.players[1].invulnerable != UINT8_C(0)))
        {
            return 0;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !pf_web_m4_reach_jab_reset_down_wait(&inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection) ||
            inspection.players[1].damage_q16 !=
                pf_web_m4_content.fighter.strong_damage_q16 ||
            pf_web_m4_find_event(PF_SIM_EVENT_HIT) != NULL)
        {
            return 0;
        }
    }

    pf_web_m4_content.fighter.jab_damage_q16 =
        pf_web_m4_content.fighter.reset_max_damage_q16 + UINT32_C(1);
    if (!pf_web_m4_initialize_current_content() ||
        !pf_web_m4_reach_jab_reset_down_wait(&inspection) ||
        !pf_web_m4_hit_down_wait_with_jab(&inspection, &hit_event) ||
        !pf_web_m4_advance_target_to_action(
            (uint8_t)PF_M4_ACTION_HITSTUN,
            &inspection))
    {
        return 0;
    }

    pf_web_m4_content.fighter.jab_damage_q16 = jab_damage;
    if (!pf_web_m4_initialize_current_content() ||
        !pf_web_m4_reach_jab_reset_down_wait(&inspection) ||
        !pf_web_m4_hit_down_wait_with_jab(&inspection, &hit_event))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        const int16_t target_y =
            tick == UINT32_C(1) ? INT16_C(0) : INT16_MIN;

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                target_y,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_RESET_BOUND ||
        inspection.players[1].sdi_pulse_count != UINT8_C(2))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.reset_bound_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           inspection.players[1].grounded == UINT8_C(0);
}

static int pf_web_m4_run_jab_reset_probe(void)
{
    const int route_passed = pf_web_m4_run_jab_reset_route();
    const int restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();

    return route_passed != 0 && restored != 0;
}

static int pf_web_m4_advance_to_settled_run(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.fighter.initial_dash_ticks +
                 UINT32_C(16);
         ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_RUN &&
            out_inspection->players[0].velocity_x_q16 ==
                pf_web_m4_content.fighter.run_speed_q16)
        {
            return 1;
        }
    }
    return 0;
}

static int pf_web_m4_run_boost_grab_route(void)
{
    pf_m4_inspection inspection;
    const pf_m4_fighter_data *fighter = &pf_web_m4_content.fighter;
    int32_t ordinary_velocity;
    int32_t ordinary_active_position = INT32_MIN;
    int32_t boost_active_position = INT32_MIN;
    uint32_t tick;
    int capture_seen = 0;
    int dash_hit_seen = 0;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_to_settled_run(&inspection) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB ||
        inspection.players[0].velocity_x_q16 !=
            fighter->run_speed_q16 - fighter->traction_q16)
    {
        return 0;
    }
    ordinary_velocity = inspection.players[0].velocity_x_q16;
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)fighter->grab_startup_ticks +
                 (uint32_t)fighter->grab_active_ticks +
                 (uint32_t)fighter->grab_recovery_ticks;
         ++tick)
    {
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GRAB &&
            inspection.players[0].action_ticks ==
                fighter->grab_startup_ticks + UINT16_C(1))
        {
            ordinary_active_position =
                inspection.players[0].position_x_q16;
        }
        if (pf_web_m4_find_event(PF_SIM_EVENT_GRAB) != NULL ||
            !pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (ordinary_active_position == INT32_MIN ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_to_settled_run(&inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_q16 !=
            fighter->dash_attack_speed_q16 -
                fighter->traction_q16 ||
        inspection.players[0].hitbox_active != UINT8_C(0) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB ||
        inspection.players[0].velocity_x_q16 !=
            fighter->dash_attack_speed_q16 -
                INT32_C(2) * fighter->traction_q16 ||
        inspection.players[0].velocity_x_q16 <= ordinary_velocity)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_GRAB);

        if (event != NULL)
        {
            if (event->source_player != UINT8_C(0) ||
                event->target_player != UINT8_C(1))
            {
                return 0;
            }
            boost_active_position =
                inspection.players[0].position_x_q16;
            capture_seen = 1;
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (capture_seen == 0 ||
        boost_active_position <= ordinary_active_position ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_to_settled_run(&inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    while (inspection.players[0].action_ticks <=
           fighter->boost_grab_cancel_end_tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_advance_to_settled_run(&inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_HIT);

        if (event != NULL)
        {
            if (event->source_player != UINT8_C(0) ||
                event->target_player != UINT8_C(1) ||
                event->value_q16 != fighter->dash_attack_damage_q16 ||
                event->detail != (uint16_t)PF_M4_ACTION_DASH_ATTACK)
            {
                return 0;
            }
            dash_hit_seen = 1;
            break;
        }
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return dash_hit_seen != 0 &&
           inspection.players[1].damage_q16 ==
               fighter->dash_attack_damage_q16;
}

static int pf_web_m4_run_boost_grab_probe(void)
{
    int passed;
    int restored;

    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(17) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    passed =
        pf_web_m4_initialize_current_content() &&
        pf_web_m4_run_boost_grab_route();
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_begin_close_grab(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *grab_event =
            pf_web_m4_find_event(PF_SIM_EVENT_GRAB);

        if (grab_event != NULL)
        {
            return grab_event->source_player == UINT8_C(0) &&
                   grab_event->target_player == UINT8_C(1) &&
                   out_inspection->players[0].action_state ==
                       (uint8_t)PF_M4_ACTION_GRAB_HOLD &&
                   out_inspection->players[1].action_state ==
                       (uint8_t)PF_M4_ACTION_GRABBED;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_perform_throw(
    int16_t stick_x,
    int16_t stick_y,
    pf_m4_action_state action,
    const pf_m4_throw_data *throw_data,
    pf_m4_inspection *out_inspection)
{
    const uint32_t expected_damage_q16 =
        pf_web_m4_expected_stale_damage_q16(
            &pf_web_m4_content.fighter,
            throw_data->damage_q16,
            out_inspection->players[0].stale_move_ids,
            out_inspection->players[0].stale_move_count,
            (uint8_t)action);
    uint32_t tick;

    if (!pf_web_m4_tick(
            stick_x,
            stick_y,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state != (uint8_t)action ||
        pf_web_m4_find_event(PF_SIM_EVENT_THROW) != NULL)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)throw_data->release_tick;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    {
        const pf_sim_event *throw_event =
            pf_web_m4_find_event(PF_SIM_EVENT_THROW);

        return throw_event != NULL &&
               throw_event->source_player == UINT8_C(0) &&
               throw_event->target_player == UINT8_C(1) &&
               throw_event->value_q16 == expected_damage_q16 &&
               throw_event->detail == (uint16_t)action &&
               out_inspection->players[0].grab_target ==
                   PF_SIM_EVENT_NO_PLAYER &&
               out_inspection->players[1].grab_owner ==
                   PF_SIM_EVENT_NO_PLAYER;
    }
}

static int pf_web_m4_wait_for_thrower_idle(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(60); ++tick)
    {
        if (out_inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            return 1;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_directional_throw_case(
    int16_t stick_x,
    int16_t stick_y,
    pf_m4_action_state action,
    const pf_m4_throw_data *throw_data)
{
    pf_m4_inspection inspection;

    return pf_web_m4_reset_internal() &&
           pf_web_m4_begin_close_grab(&inspection) &&
           pf_web_m4_perform_throw(
               stick_x,
               stick_y,
               action,
               throw_data,
               &inspection);
}

static int pf_web_m4_run_pummel_case(void)
{
    pf_m4_inspection inspection;
    const pf_sim_event *pummel_event;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_begin_close_grab(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(0) ||
        pf_web_m4_find_event(PF_SIM_EVENT_PUMMEL) != NULL)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter.pummel_hit_tick;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    pummel_event = pf_web_m4_find_event(PF_SIM_EVENT_PUMMEL);
    if (pummel_event == NULL ||
        pummel_event->source_player != UINT8_C(0) ||
        pummel_event->target_player != UINT8_C(1) ||
        pummel_event->value_q16 !=
            pf_web_m4_content.fighter.pummel_damage_q16 ||
        pummel_event->velocity_x_q16 != INT32_C(0) ||
        pummel_event->velocity_y_q16 != INT32_C(0) ||
        pummel_event->flags != UINT16_C(0) ||
        pummel_event->detail != (uint16_t)PF_M4_ACTION_PUMMEL ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.pummel_hit_tick ||
        inspection.players[1].damage_q16 !=
            pf_web_m4_content.fighter.pummel_damage_q16)
    {
        return 0;
    }
    for (tick =
             (uint32_t)pf_web_m4_content.fighter.pummel_hit_tick;
         tick <
             (uint32_t)pf_web_m4_content.fighter.pummel_total_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GRAB_HOLD &&
           inspection.players[0].action_ticks == UINT16_C(1) &&
           inspection.players[0].grab_target == UINT8_C(1) &&
           inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GRABBED &&
           inspection.players[1].grab_owner == UINT8_C(0) &&
           inspection.players[1].damage_q16 ==
               pf_web_m4_content.fighter.pummel_damage_q16 &&
           pf_web_m4_find_event(PF_SIM_EVENT_PUMMEL) == NULL;
}

static int pf_web_m4_run_chain_grab_route(void)
{
    pf_m4_inspection inspection;
    uint32_t regrab;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_begin_close_grab(&inspection))
    {
        return 0;
    }
    for (regrab = UINT32_C(0); regrab < UINT32_C(2); ++regrab)
    {
        if (!pf_web_m4_perform_throw(
                INT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                PF_M4_ACTION_THROW_DOWN,
                &pf_web_m4_content.fighter.down_throw,
                &inspection) ||
            !pf_web_m4_wait_for_thrower_idle(&inspection) ||
            !pf_web_m4_begin_close_grab(&inspection))
        {
            return 0;
        }
    }
    return pf_web_m4_perform_throw(
               INT16_C(0),
               PF_WEB_M4_DASH_AXIS,
               PF_M4_ACTION_THROW_DOWN,
               &pf_web_m4_content.fighter.down_throw,
               &inspection) &&
           inspection.players[1].damage_q16 ==
               pf_web_m4_expected_repeated_move_damage_q16(
                   &pf_web_m4_content.fighter,
                   pf_web_m4_content.fighter.down_throw.damage_q16,
                   UINT32_C(3));
}

static int pf_web_m4_run_chain_grab_probe(void)
{
    int passed;
    int restored;

    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    passed =
        pf_web_m4_initialize_current_content() &&
        pf_web_m4_run_directional_throw_case(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_M4_ACTION_THROW_FORWARD,
            &pf_web_m4_content.fighter.forward_throw) &&
        pf_web_m4_run_directional_throw_case(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_M4_ACTION_THROW_BACK,
            &pf_web_m4_content.fighter.back_throw) &&
        pf_web_m4_run_directional_throw_case(
            INT16_C(0),
            -PF_WEB_M4_DASH_AXIS,
            PF_M4_ACTION_THROW_UP,
            &pf_web_m4_content.fighter.up_throw) &&
        pf_web_m4_run_directional_throw_case(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            PF_M4_ACTION_THROW_DOWN,
            &pf_web_m4_content.fighter.down_throw) &&
        pf_web_m4_run_pummel_case() &&
        pf_web_m4_run_chain_grab_route();

    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_run_edge_hop_probe(void)
{
    pf_m4_inspection inspection;
    const uint32_t catch_ticks =
        (uint32_t)pf_web_m4_content.fighter.landing_ticks +
        (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;
    int32_t exhausted_jump_velocity_y;
    uint32_t tick;

    if (!pf_web_m4_grab_player0_right_ledge(&inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].air_jumps_remaining != UINT8_C(1) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(-1) ||
        inspection.players[0].invulnerable != UINT8_C(1) ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    exhausted_jump_velocity_y =
        inspection.players[0].velocity_y_q16;
    if (!pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].air_jumps_remaining != UINT8_C(0) ||
        inspection.players[0].velocity_y_q16 <=
            exhausted_jump_velocity_y ||
        !pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_FORWARD_AERIAL &&
           inspection.players[0].invulnerable == UINT8_C(1);
}

static int pf_web_m4_reach_scar_jump_wall(
    pf_m4_inspection *out_inspection)
{
    const uint32_t catch_ticks =
        (uint32_t)pf_web_m4_content.fighter.landing_ticks +
        (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;
    const int32_t contact_x =
        pf_web_m4_content.stage.solid_right_q16 +
        pf_web_m4_content.fighter.half_width_q16;
    uint32_t tick;

    if (!pf_web_m4_grab_player0_right_ledge(out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
    {
        if (!pf_web_m4_tick(
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].position_x_q16 == contact_x &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            return out_inspection->players[0].action_state ==
                       (uint8_t)PF_M4_ACTION_AIRBORNE &&
                   out_inspection->players[0].air_jumps_remaining ==
                       pf_web_m4_content.fighter.air_jump_count;
        }
    }
    return 0;
}

static int pf_web_m4_run_scar_jump_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_reach_scar_jump_wall(&inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALL_JUMP ||
        inspection.players[0].air_jumps_remaining !=
            pf_web_m4_content.fighter.air_jump_count ||
        inspection.players[0].invulnerable != UINT8_C(1) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].air_jumps_remaining !=
            pf_web_m4_content.fighter.air_jump_count ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_grab_player0_right_ledge(&inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter.landing_ticks +
                    (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_WALL_JUMP)
        {
            return 0;
        }
    }
    return inspection.players[0].air_jumps_remaining ==
           pf_web_m4_content.fighter.air_jump_count;
}

static int pf_web_m4_run_team_wobble_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;
    uint32_t throw_events = UINT32_C(0);
    uint32_t handoff_events = UINT32_C(0);
    int route_passed = 0;
    int negative_passed = 0;

    if (!pf_web_m4_initialize_team_wobble_lab() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        goto cleanup;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *grab_event =
            pf_web_m4_find_event(PF_SIM_EVENT_GRAB);

        if (grab_event != NULL &&
            grab_event->source_player == UINT8_C(0) &&
            grab_event->target_player == UINT8_C(1))
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            goto cleanup;
        }
    }
    if (tick == UINT32_C(12) ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(0) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection))
    {
        goto cleanup;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const pf_sim_event *throw_event =
            pf_web_m4_find_event(PF_SIM_EVENT_THROW);
        const pf_sim_event *grab_event =
            pf_web_m4_find_event(PF_SIM_EVENT_GRAB);

        if (throw_event != NULL &&
            throw_event->source_player == UINT8_C(0) &&
            throw_event->target_player == UINT8_C(1))
        {
            ++throw_events;
        }
        if (grab_event != NULL &&
            grab_event->source_player == UINT8_C(2) &&
            grab_event->target_player == UINT8_C(1))
        {
            ++handoff_events;
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            goto cleanup;
        }
    }
    if (throw_events != UINT32_C(1) ||
        handoff_events != UINT32_C(1) ||
        inspection.players[2].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(2))
    {
        goto cleanup;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            goto cleanup;
        }
    }
    if (tick == UINT32_C(32) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection))
    {
        goto cleanup;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const pf_sim_event *throw_event =
            pf_web_m4_find_event(PF_SIM_EVENT_THROW);
        const pf_sim_event *grab_event =
            pf_web_m4_find_event(PF_SIM_EVENT_GRAB);

        if (throw_event != NULL &&
            throw_event->source_player == UINT8_C(2) &&
            throw_event->target_player == UINT8_C(1))
        {
            ++throw_events;
        }
        if (grab_event != NULL &&
            grab_event->source_player == UINT8_C(0) &&
            grab_event->target_player == UINT8_C(1))
        {
            ++handoff_events;
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            goto cleanup;
        }
    }
    route_passed =
        throw_events == UINT32_C(2) &&
        handoff_events == UINT32_C(2) &&
        inspection.players[0].grab_target == UINT8_C(1) &&
        inspection.players[1].grab_owner == UINT8_C(0) &&
        inspection.players[1].damage_q16 ==
            UINT32_C(2) *
                pf_web_m4_content.fighter.down_throw.damage_q16;

    if (!pf_web_m4_initialize_team_wobble_lab() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection))
    {
        goto cleanup;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (inspection.players[0].grab_target == UINT8_C(1) &&
            inspection.players[2].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            goto cleanup;
        }
    }
    if (tick == UINT32_C(20) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        goto cleanup;
    }
    negative_passed = 1;
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        const pf_sim_event *grab_event =
            pf_web_m4_find_event(PF_SIM_EVENT_GRAB);

        if (grab_event != NULL &&
            grab_event->source_player == UINT8_C(2) &&
            grab_event->target_player == UINT8_C(1))
        {
            negative_passed = 0;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            negative_passed = 0;
            break;
        }
    }
    negative_passed =
        negative_passed != 0 &&
        inspection.players[1].grab_owner == PF_SIM_EVENT_NO_PLAYER;

cleanup:
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content())
    {
        return 0;
    }
    return route_passed != 0 && negative_passed != 0;
}

static int pf_web_m4_run_edge_dash_probe(void)
{
    pf_m4_inspection inspection;
    const uint32_t catch_ticks =
        (uint32_t)pf_web_m4_content.fighter.landing_ticks +
        (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;
    uint32_t tick;

    if (!pf_web_m4_grab_player0_right_ledge(&inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[0].position_y_q16 +
                 pf_web_m4_content.fighter.half_height_q16 >
             pf_web_m4_content.stage.floor_y_q16;
         ++tick)
    {
        if (!pf_web_m4_tick(
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        !pf_web_m4_tick_with_triggers(
            -PF_WEB_M4_DASH_AXIS,
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        (inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_AIR_DODGE &&
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_SPECIAL_LANDING) ||
        inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_AIR_DODGE;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick <
             (uint32_t)pf_web_m4_content.fighter.special_landing_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
            inspection.players[0].action_ticks != (uint16_t)tick ||
            inspection.players[0].invulnerable != UINT8_C(1))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].invulnerable != UINT8_C(1) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].invulnerable != UINT8_C(1))
    {
        return 0;
    }

    if (!pf_web_m4_grab_player0_right_ledge(&inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         inspection.players[0].invulnerable != UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(120) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[0].position_y_q16 +
                 pf_web_m4_content.fighter.half_height_q16 >
             pf_web_m4_content.stage.floor_y_q16;
         ++tick)
    {
        if (!pf_web_m4_tick(
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(16) ||
        !pf_web_m4_tick_with_triggers(
            -PF_WEB_M4_DASH_AXIS,
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_AIR_DODGE;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return tick != UINT32_C(16) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
           inspection.players[0].invulnerable == UINT8_C(0);
}

static int pf_web_m4_start_short_hop_aerial(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         out_inspection->players[0].grounded != UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           out_inspection->players[0].grounded == UINT8_C(0) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           out_inspection->players[0].action_ticks == UINT16_C(0);
}

static int pf_web_m4_start_full_hop_aerial(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           out_inspection->players[0].grounded == UINT8_C(0) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           out_inspection->players[0].action_ticks == UINT16_C(0);
}

static int pf_web_m4_start_short_hop_strong_aerial(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (out_inspection == NULL ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         out_inspection->players[0].grounded != UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           out_inspection->players[0].grounded == UINT8_C(0) &&
           pf_web_m4_tick(
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_STRONG_ATTACK,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
           out_inspection->players[0].action_ticks == UINT16_C(0);
}

static int pf_web_m4_run_aerial_l_cancel_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t landing_ticks;
    uint32_t tick;
    int trigger_pressed = 0;

    if (!pf_web_m4_start_full_hop_aerial(&inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        !pf_web_m4_start_short_hop_aerial(&inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        return 0;
    }
    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_LANDING &&
           landing_ticks < UINT32_C(40))
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)pf_web_m4_content.fighter
                .aerial_landing_lag_ticks ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !pf_web_m4_start_short_hop_aerial(&inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        uint16_t trigger = UINT16_C(0);

        if (trigger_pressed == 0 &&
            inspection.players[0].velocity_y_q16 >= INT32_C(0))
        {
            trigger = UINT16_MAX;
            trigger_pressed = 1;
        }
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                UINT64_C(0),
                trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (trigger_pressed == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].trigger_input_age >=
            pf_web_m4_content.fighter.l_cancel_window_ticks)
    {
        return 0;
    }
    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING &&
           landing_ticks < UINT32_C(40))
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)(
                pf_web_m4_content.fighter
                    .aerial_landing_lag_ticks /
                pf_web_m4_content.fighter.l_cancel_divisor) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !pf_web_m4_start_short_hop_aerial(&inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].trigger_input_age != UINT8_C(0) ||
        inspection.players[0].l_cancel_eligible != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(1); tick < UINT32_C(7); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].trigger_input_age !=
                (uint8_t)tick ||
            inspection.players[0].l_cancel_eligible != UINT8_C(1))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].trigger_input_age != UINT8_C(7) ||
        inspection.players[0].l_cancel_eligible != UINT8_C(0) ||
        !pf_web_m4_start_short_hop_strong_aerial(&inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        return 0;
    }
    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_LANDING &&
           landing_ticks < UINT32_C(60))
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)pf_web_m4_content.fighter
                .strong_aerial_landing_lag_ticks ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !pf_web_m4_start_short_hop_strong_aerial(&inspection))
    {
        return 0;
    }

    trigger_pressed = 0;
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        uint16_t trigger = UINT16_C(0);

        if (trigger_pressed == 0 &&
            inspection.players[0].velocity_y_q16 >= INT32_C(0))
        {
            trigger = UINT16_MAX;
            trigger_pressed = 1;
        }
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                UINT64_C(0),
                trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (trigger_pressed == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].trigger_input_age >=
            pf_web_m4_content.fighter.l_cancel_window_ticks)
    {
        return 0;
    }
    landing_ticks = UINT32_C(0);
    while (inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_L_CANCEL_LANDING &&
           landing_ticks < UINT32_C(60))
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        ++landing_ticks;
    }
    return landing_ticks ==
               (uint32_t)(
                   pf_web_m4_content.fighter
                       .strong_aerial_landing_lag_ticks /
                   pf_web_m4_content.fighter.l_cancel_divisor) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE;
}

static int pf_web_m4_run_air_dodge_probe(void)
{
    pf_m4_inspection inspection;
    int32_t landing_x;
    int8_t takeoff_facing;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            break;
        }
    }
    takeoff_facing = inspection.players[0].facing;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint16_t)(
                pf_web_m4_content.fighter.digital_trigger_threshold -
                UINT16_C(1)),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            -PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].facing != takeoff_facing)
    {
        return 0;
    }
    while (inspection.players[0].action_ticks <
           pf_web_m4_content.fighter
               .air_dodge_invulnerability_begin_tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                PF_WEB_M4_DASH_AXIS,
                -PF_WEB_M4_DASH_AXIS,
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].invulnerable != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <= (uint32_t)pf_web_m4_content.fighter.air_dodge_ticks;
         ++tick)
    {
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL)
        {
            break;
        }
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        inspection.players[0].invulnerable != UINT8_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        const uint64_t buttons =
            tick == UINT32_C(0)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            break;
        }
    }
    takeoff_facing = inspection.players[0].facing;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].facing != takeoff_facing)
    {
        return 0;
    }
    landing_x = inspection.players[0].position_x_q16;
    return pf_web_m4_tick_with_triggers(
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SPECIAL_LANDING &&
           inspection.players[0].position_x_q16 > landing_x;
}

static int pf_web_m4_run_directional_attack_hit_case(
    const pf_m4_attack_data *attack,
    int16_t input_x,
    int16_t input_y,
    uint64_t attack_button,
    pf_m4_action_state expected_action)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    if (!pf_web_m4_tick(
            input_x,
            input_y,
            attack_button,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    if (inspection.players[0].action_state !=
        (uint8_t)expected_action)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)attack->startup_ticks +
                    (uint32_t)attack->active_ticks;
         ++tick)
    {
        const pf_sim_event *event;

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
        if (event == NULL)
        {
            continue;
        }
        {
            const int passed =
               event->source_player == UINT8_C(0) &&
               event->target_player == UINT8_C(1) &&
               event->detail == (uint16_t)expected_action &&
               event->value_q16 == attack->damage_q16 &&
               event->velocity_x_q16 > INT32_C(0) &&
               (expected_action == PF_M4_ACTION_DOWN_ATTACK
                    ? event->velocity_y_q16 > INT32_C(0)
                    : event->velocity_y_q16 < INT32_C(0)) &&
               inspection.players[1].damage_q16 ==
                   attack->damage_q16 &&
               inspection.players[1].hitlag_ticks ==
                   attack->hitlag_ticks;
            if (!passed)
            {
                return 0;
            }
            return passed;
        }
    }
    return 0;
}

static int pf_web_m4_run_directional_ground_attack_probe(void)
{
    const pf_m4_content saved_content = pf_web_m4_content;
    pf_m4_inspection inspection;
    int initialized;
    int up_passed;
    int down_passed;
    int forward_passed;
    int forward_strong_passed;
    int up_strong_passed;
    int down_strong_passed;
    int neutral_passed;
    int diagonal_passed;
    int diagonal_release_passed;
    int neutral_strong_passed;
    int passed;
    int restored;

    pf_web_m4_content.stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    initialized = pf_web_m4_initialize_current_content();
    up_passed = initialized &&
        pf_web_m4_run_directional_attack_hit_case(
            &pf_web_m4_content.fighter.up_attack,
            INT16_C(0),
            (int16_t)-(
                (int32_t)pf_web_m4_content.fighter
                    .dash_axis_threshold -
                INT32_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_UP_ATTACK);
    down_passed = up_passed &&
        pf_web_m4_run_directional_attack_hit_case(
            &pf_web_m4_content.fighter.down_attack,
            INT16_C(0),
            (int16_t)(
                pf_web_m4_content.fighter.dash_axis_threshold -
                UINT16_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_DOWN_ATTACK);
    forward_passed = down_passed &&
        pf_web_m4_run_directional_attack_hit_case(
            &pf_web_m4_content.fighter.forward_attack,
            (int16_t)(
                pf_web_m4_content.fighter.axis_dead_zone + UINT16_C(1)),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_FORWARD_ATTACK);
    forward_strong_passed = forward_passed &&
        pf_web_m4_run_directional_attack_hit_case(
            &pf_web_m4_content.fighter.forward_strong_attack,
            INT16_C(32767),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_FORWARD_STRONG_ATTACK);
    up_strong_passed = forward_strong_passed &&
        pf_web_m4_run_directional_attack_hit_case(
            &pf_web_m4_content.fighter.up_strong_attack,
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_UP_STRONG_ATTACK);
    down_strong_passed = up_strong_passed &&
        pf_web_m4_run_directional_attack_hit_case(
            &pf_web_m4_content.fighter.down_strong_attack,
            INT16_C(0),
            INT16_C(32767),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_DOWN_STRONG_ATTACK);
    neutral_passed = down_strong_passed &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_C(0),
            (int16_t)(
                pf_web_m4_content.fighter.axis_dead_zone -
                UINT16_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK;
    diagonal_passed = neutral_passed &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_C(32767),
            INT16_C(-32767),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE &&
        inspection.players[0].smash_charge_ticks == UINT16_C(1);
    diagonal_release_passed = diagonal_passed &&
        pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK &&
        inspection.players[0].action_ticks == UINT16_C(1) &&
        inspection.players[0].smash_charge_ticks == UINT16_C(1);
    neutral_strong_passed = diagonal_release_passed &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK;
    passed = neutral_strong_passed;

    pf_web_m4_content = saved_content;
    restored = pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_run_directional_aerial_action_case(
    int16_t input_x,
    int16_t input_y,
    uint64_t attack_button,
    pf_m4_action_state expected_action)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return tick < UINT32_C(8) &&
           pf_web_m4_tick(
               input_x,
               input_y,
               attack_button,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               &inspection) &&
           inspection.players[0].action_state ==
               (uint8_t)expected_action &&
           inspection.players[0].action_ticks == UINT16_C(0) &&
           inspection.players[0].grounded == UINT8_C(0);
}

static int pf_web_m4_run_directional_aerial_probe(void)
{
    const int16_t reduced_vertical = (int16_t)(
        pf_web_m4_content.fighter.dash_axis_threshold - UINT16_C(1));

    return pf_web_m4_run_directional_aerial_action_case(
               PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               PF_M4_ACTION_FORWARD_AERIAL) &&
           pf_web_m4_run_directional_aerial_action_case(
               -PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               PF_M4_ACTION_BACK_AERIAL) &&
           pf_web_m4_run_directional_aerial_action_case(
               INT16_C(0),
               -PF_WEB_M4_DASH_AXIS,
               PF_INPUT_BUTTON_ATTACK,
               PF_M4_ACTION_UP_AERIAL) &&
           pf_web_m4_run_directional_aerial_action_case(
               INT16_C(0),
               PF_WEB_M4_DASH_AXIS,
               PF_INPUT_BUTTON_ATTACK,
               PF_M4_ACTION_DOWN_AERIAL) &&
           pf_web_m4_run_directional_aerial_action_case(
               INT16_C(0),
               reduced_vertical,
               PF_INPUT_BUTTON_ATTACK,
               PF_M4_ACTION_AERIAL_ATTACK) &&
           pf_web_m4_run_directional_aerial_action_case(
               PF_WEB_M4_DASH_AXIS,
               -PF_WEB_M4_DASH_AXIS,
               PF_INPUT_BUTTON_STRONG_ATTACK,
               PF_M4_ACTION_STRONG_AERIAL_ATTACK);
}

static int pf_web_m4_wait_for_ledge_actionable(
    pf_m4_inspection *out_inspection)
{
    const uint32_t catch_ticks =
        (uint32_t)pf_web_m4_content.fighter.landing_ticks +
        (uint32_t)pf_web_m4_content.fighter.jump_squat_ticks;
    uint32_t tick;

    for (tick = UINT32_C(0); tick < catch_ticks; ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_LEDGE_HANG &&
           out_inspection->players[0].action_ticks ==
               (uint16_t)catch_ticks;
}

static int pf_web_m4_run_ledge_option_probe(void)
{
    pf_m4_inspection inspection;
    int32_t roll_target_x;
    uint32_t tick;

    if (!pf_web_m4_grab_player0_right_ledge(&inspection) ||
        !pf_web_m4_wait_for_ledge_actionable(&inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            pf_web_m4_content.fighter.digital_trigger_threshold,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ROLL ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT)
    {
        return 0;
    }
    roll_target_x =
        inspection.stage.right_ledge_x_q16 -
        pf_web_m4_content.fighter.ledge_roll_distance_q16;
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter.ledge_roll_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].position_x_q16 != roll_target_x)
    {
        return 0;
    }

    if (!pf_web_m4_grab_player0_right_ledge(&inspection) ||
        !pf_web_m4_wait_for_ledge_actionable(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <= (uint32_t)pf_web_m4_content.fighter
                         .ledge_attack.startup_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
        inspection.players[0].hitbox_active == UINT8_C(0) ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.ledge_attack.startup_ticks +
                UINT16_C(1))
    {
        return 0;
    }

    if (!pf_web_m4_grab_player0_right_ledge(&inspection) ||
        !pf_web_m4_wait_for_ledge_actionable(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_LEDGE_ATTACK &&
           inspection.players[0].action_ticks == UINT16_C(0);
}

static int pf_web_m4_run_combat_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[1].damage_q16 ==
               pf_web_m4_content.fighter.jab_damage_q16 &&
           inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_HITLAG &&
           inspection.players[1].last_hit_attacker == UINT8_C(0) &&
           pf_web_m4_last_result.event_count == UINT8_C(2) &&
           pf_web_m4_last_result.events[0].type ==
               (uint16_t)PF_SIM_EVENT_HIT &&
           pf_web_m4_last_result.events[0].source_player ==
               UINT8_C(0) &&
           pf_web_m4_last_result.events[0].target_player ==
               UINT8_C(1) &&
            pf_web_m4_last_result.events[0].sequence ==
                inspection.players[1].last_hit_sequence &&
            pf_web_m4_run_directional_ground_attack_probe() &&
            pf_web_m4_run_directional_aerial_probe() &&
            pf_web_m4_run_ledge_option_probe();
}

static int pf_web_m4_run_tumble_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter
                        .strong_startup_ticks +
                    (uint32_t)pf_web_m4_content.fighter
                        .strong_active_ticks +
                    UINT32_C(2);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].damage_q16 != UINT32_C(0))
        {
            break;
        }
    }
    return inspection.players[1].damage_q16 ==
               pf_web_m4_content.fighter.strong_damage_q16 &&
           inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_HITLAG &&
           inspection.players[1].hitlag_ticks ==
               pf_web_m4_content.fighter.strong_hitlag_ticks &&
           inspection.players[1].hitstun_ticks >=
               pf_web_m4_content.fighter
                   .tumble_hitstun_threshold_ticks &&
           inspection.players[1].tumble == UINT8_C(1) &&
           inspection.players[1].last_hit_damage_q16 ==
               pf_web_m4_content.fighter.strong_damage_q16 &&
           inspection.players[1].last_hit_attacker == UINT8_C(0);
}

static int pf_web_m4_run_surface_tech_probe(void)
{
    pf_m4_inspection inspection;
    int armed_tech = 0;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(34); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (inspection.players[1].damage_q16 != UINT32_C(0))
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].damage_q16 !=
            pf_web_m4_content.fighter.strong_damage_q16 ||
        inspection.players[1].tumble != UINT8_C(1))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const pf_m4_player_inspection *target =
            &inspection.players[1];
        const int64_t wall_contact_x =
            (int64_t)inspection.stage.solid_left_q16 -
            (int64_t)pf_web_m4_content.fighter.half_width_q16;
        const int64_t distance_to_wall =
            wall_contact_x - (int64_t)target->position_x_q16;
        const int64_t maximum_tech_travel =
            (int64_t)target->velocity_x_q16 *
            ((int64_t)pf_web_m4_content.fighter.tech_window_ticks -
             INT64_C(2));
        uint16_t trigger = UINT16_C(0);
        int16_t target_y = INT16_C(0);

        if (armed_tech == 0 &&
            target->tumble != UINT8_C(0) &&
            target->velocity_x_q16 > INT32_C(0) &&
            distance_to_wall > INT64_C(0) &&
            distance_to_wall <= maximum_tech_travel)
        {
            armed_tech = 1;
        }
        if (armed_tech != 0)
        {
            trigger = UINT16_MAX;
            target_y = -PF_WEB_M4_DASH_AXIS;
        }

        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                target_y,
                UINT64_C(0),
                trigger,
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP)
        {
            return armed_tech != 0 &&
                   inspection.players[1].tumble == UINT8_C(0) &&
                   inspection.players[1].hitstun_ticks ==
                       UINT16_C(0) &&
                   inspection.players[1].tech_window_ticks ==
                       UINT16_C(0) &&
                   inspection.players[1].facing == INT8_C(-1) &&
                   inspection.players[1].tech_direction ==
                       INT8_C(-1) &&
                   inspection.players[1].invulnerable ==
                       UINT8_C(1);
        }
    }
    return 0;
}

static int pf_web_m4_reach_down_wait(
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    uint16_t knockdown_steps = UINT16_C(0);

    if (out_inspection == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(600); ++tick)
    {
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_KNOCKDOWN)
        {
            if (out_inspection->players[1].invulnerable !=
                UINT8_C(0))
            {
                return 0;
            }
            ++knockdown_steps;
        }
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_DOWN_WAIT)
        {
            return out_inspection->players[1].action_ticks ==
                       UINT16_C(0) &&
                   out_inspection->players[1].prone_orientation ==
                       (uint8_t)PF_M4_PRONE_BACK &&
                   knockdown_steps ==
                       pf_web_m4_content.fighter.knockdown_ticks;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_floor_recovery_probe(void)
{
    pf_m4_inspection inspection;
    const pf_m4_getup_roll_timing *back_backward =
        &pf_web_m4_content.fighter.getup_roll_back_backward;
    uint32_t tick;

    if (!pf_web_m4_reach_down_wait(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick + UINT32_C(1) <
             (uint32_t)pf_web_m4_content.fighter
                 .getup_attack_front_active_begin_tick;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].hitbox_active != UINT8_C(1))
    {
        return 0;
    }
    while ((uint32_t)inspection.players[1].action_ticks +
               UINT32_C(1) <=
           (uint32_t)pf_web_m4_content.fighter
               .getup_attack_front_active_end_tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].hitbox_active != UINT8_C(0))
    {
        return 0;
    }
    while ((uint32_t)inspection.players[1].action_ticks +
               UINT32_C(1) <
           (uint32_t)pf_web_m4_content.fighter
               .getup_attack_back_active_begin_tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].hitbox_active != UINT8_C(1) ||
        !pf_web_m4_reach_down_wait(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        inspection.players[1].tech_direction != INT8_C(1) ||
        inspection.players[1].prone_orientation !=
            (uint8_t)PF_M4_PRONE_BACK ||
        inspection.players[1].velocity_x_q16 != INT32_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(0))
    {
        return 0;
    }
    while ((uint32_t)inspection.players[1].action_ticks + UINT32_C(1) <
           (uint32_t)back_backward->movement_begin_tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        !pf_web_m4_reach_down_wait(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(-32767),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL &&
           inspection.players[1].invulnerable == UINT8_C(1);
}

static int16_t pf_web_m4_tech_chase_axis(
    const pf_m4_inspection *inspection)
{
    const int32_t delta =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;
    const int aged_walk =
        inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_WALK &&
        inspection->players[0].action_ticks >=
            pf_web_m4_content.fighter.dash_input_window_ticks;
    const int target_is_tech_rolling =
        inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_TECH_ROLL;

    if (target_is_tech_rolling != 0)
    {
        if (aged_walk != 0)
        {
            return INT16_C(0);
        }
        if (delta > INT32_C(0))
        {
            return PF_WEB_M4_DASH_AXIS;
        }
        if (delta < INT32_C(0))
        {
            return -PF_WEB_M4_DASH_AXIS;
        }
    }

    if (delta >
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return aged_walk != 0
                   ? INT16_C(0)
                   : PF_WEB_M4_DASH_AXIS;
    }
    if (delta > PF_Q16_ONE / INT32_C(2))
    {
        return PF_WEB_M4_WALK_AXIS;
    }
    if (delta <
        -(INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return aged_walk != 0
                   ? INT16_C(0)
                   : -PF_WEB_M4_DASH_AXIS;
    }
    if (delta < -PF_Q16_ONE / INT32_C(2))
    {
        return -PF_WEB_M4_WALK_AXIS;
    }
    return INT16_C(0);
}

static int pf_web_m4_tech_chase_jab_in_range(
    const pf_m4_inspection *inspection)
{
    const pf_m4_player_inspection *attacker =
        &inspection->players[0];
    const pf_m4_player_inspection *target =
        &inspection->players[1];
    const int64_t delta =
        (int64_t)target->position_x_q16 -
        (int64_t)attacker->position_x_q16;
    const int8_t direction =
        delta > INT64_C(0) ? INT8_C(1) : INT8_C(-1);
    const int64_t distance =
        delta >= INT64_C(0) ? delta : -delta;
    const int64_t reach =
        (int64_t)pf_web_m4_content.fighter
            .jab_hitbox_offset_x_q16 +
        (int64_t)pf_web_m4_content.fighter
            .jab_hitbox_half_width_q16 +
        (int64_t)pf_web_m4_content.fighter.half_width_q16;

    return delta != INT64_C(0) &&
           attacker->facing == direction &&
           distance <= reach;
}

static int pf_web_m4_reach_tech_chase_landing(
    int tech_mode,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    int trigger_sent = 0;

    if (out_inspection == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (out_inspection->players[1].damage_q16 != UINT32_C(0))
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[1].damage_q16 !=
            pf_web_m4_content.fighter.strong_damage_q16 ||
        out_inspection->players[1].tumble != UINT8_C(1))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const pf_m4_player_inspection *target =
            &out_inspection->players[1];
        int32_t landing_surface_y_q16 =
            out_inspection->stage.floor_y_q16;

        if ((int64_t)target->position_x_q16 +
                    (int64_t)pf_web_m4_content.fighter
                        .half_width_q16 >=
                (int64_t)out_inspection->stage.solid_left_q16 &&
            (int64_t)target->position_x_q16 -
                    (int64_t)pf_web_m4_content.fighter
                        .half_width_q16 <=
                (int64_t)out_inspection->stage.solid_right_q16 &&
            target->position_y_q16 <
                out_inspection->stage.solid_top_q16)
        {
            landing_surface_y_q16 =
                out_inspection->stage.solid_top_q16;
        }
        else if (
            (int64_t)target->position_x_q16 +
                    (int64_t)pf_web_m4_content.fighter
                        .half_width_q16 >=
                (int64_t)out_inspection->stage.platform_left_q16 &&
            (int64_t)target->position_x_q16 -
                    (int64_t)pf_web_m4_content.fighter
                        .half_width_q16 <=
                (int64_t)out_inspection->stage.platform_right_q16 &&
            target->position_y_q16 <
                out_inspection->stage.platform_y_q16)
        {
            landing_surface_y_q16 =
                out_inspection->stage.platform_y_q16;
        }
        const int should_trigger =
            trigger_sent == 0 &&
            target->action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            target->velocity_y_q16 > INT32_C(0) &&
            target->position_y_q16 +
                    INT32_C(4) * PF_Q16_ONE >=
                landing_surface_y_q16;
        const int16_t target_x =
            target->action_state == (uint8_t)PF_M4_ACTION_HITLAG
                ? -PF_WEB_M4_DASH_AXIS
                : (tech_mode > 1 &&
                           (should_trigger || trigger_sent != 0)
                       ? PF_WEB_M4_DASH_AXIS
                       : INT16_C(0));
        const int16_t target_y =
            target->action_state == (uint8_t)PF_M4_ACTION_HITLAG
                ? -PF_WEB_M4_DASH_AXIS
                : INT16_C(0);

        if (!pf_web_m4_tick_with_triggers(
                pf_web_m4_tech_chase_axis(out_inspection),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                target_y,
                UINT64_C(0),
                should_trigger ? UINT16_MAX : UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (should_trigger)
        {
            trigger_sent = 1;
        }
        if (out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_ROLL)
        {
            return trigger_sent != 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_tech_chase_route(
    int tech_mode,
    int react)
{
    pf_m4_inspection inspection;
    uint32_t initial_damage;
    uint32_t initial_hit_sequence;
    uint32_t tick;
    uint16_t attack_action_tick = UINT16_MAX;
    int attack_sent = 0;

    if (!pf_web_m4_reach_tech_chase_landing(
            tech_mode,
            &inspection))
    {
        return 0;
    }
    if ((tech_mode == 1 &&
         inspection.players[1].action_state !=
             (uint8_t)PF_M4_ACTION_TECH_IN_PLACE) ||
        (tech_mode == 2 &&
         (inspection.players[1].action_state !=
              (uint8_t)PF_M4_ACTION_TECH_ROLL ||
          inspection.players[1].tech_direction != INT8_C(1))))
    {
        return 0;
    }

    initial_damage = inspection.players[1].damage_q16;
    initial_hit_sequence =
        inspection.players[1].last_hit_sequence;
    for (tick = UINT32_C(0); tick < UINT32_C(100); ++tick)
    {
        int16_t chaser_x =
            react != 0
                ? pf_web_m4_tech_chase_axis(&inspection)
                : INT16_C(0);
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            inspection.players[1].action_ticks >=
                pf_web_m4_content.fighter
                    .tech_invulnerability_ticks &&
            (react == 0 ||
             pf_web_m4_tech_chase_jab_in_range(&inspection)))
        {
            if (react == 0 &&
                pf_web_m4_tech_chase_jab_in_range(&inspection))
            {
                return 0;
            }
            attack_sent = 1;
            attack_action_tick =
                inspection.players[1].action_ticks;
            chaser_x = INT16_C(0);
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!pf_web_m4_tick(
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[1].damage_q16 > initial_damage)
        {
            const uint16_t total_ticks =
                tech_mode == 1
                    ? pf_web_m4_content.fighter
                          .tech_in_place_ticks
                    : pf_web_m4_content.fighter.tech_roll_ticks;

            return react != 0 && attack_sent != 0 &&
                   attack_action_tick +
                           pf_web_m4_content.fighter
                               .jab_startup_ticks <
                       total_ticks &&
                   inspection.players[1].damage_q16 ==
                       initial_damage +
                           pf_web_m4_content.fighter
                               .jab_damage_q16 &&
                   inspection.players[1].action_state ==
                       (uint8_t)PF_M4_ACTION_HITLAG &&
                   inspection.players[1].last_hit_attacker ==
                       UINT8_C(0);
        }
    }
    return react == 0 && attack_sent != 0 &&
           inspection.players[1].damage_q16 == initial_damage &&
           inspection.players[1].last_hit_sequence ==
               initial_hit_sequence;
}

static int pf_web_m4_run_tech_chase_probe(void)
{
    return pf_web_m4_run_tech_chase_route(1, 1) &&
           pf_web_m4_run_tech_chase_route(2, 1) &&
           pf_web_m4_run_tech_chase_route(2, 0);
}

typedef struct pf_web_m4_crouch_cancel_result
{
    pf_sim_event event;
    uint32_t damage_q16;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
} pf_web_m4_crouch_cancel_result;

static int pf_web_m4_run_crouch_cancel_route(
    int target_crouches,
    pf_web_m4_crouch_cancel_result *out_result)
{
    pf_m4_inspection inspection;
    const int16_t target_y =
        target_crouches != 0 ? INT16_MAX : INT16_C(0);
    uint32_t tick;

    if (out_result == NULL || !pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            target_y,
            UINT64_C(0),
            &inspection) ||
        (target_crouches != 0 &&
         inspection.players[1].action_state !=
             (uint8_t)PF_M4_ACTION_CROUCH) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            target_y,
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        const pf_sim_event *event =
            pf_web_m4_find_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            out_result->event = *event;
            out_result->damage_q16 =
                inspection.players[1].damage_q16;
            out_result->hitlag_ticks =
                inspection.players[1].hitlag_ticks;
            out_result->hitstun_ticks =
                inspection.players[1].hitstun_ticks;
            return inspection.players[1].action_state ==
                   (uint8_t)PF_M4_ACTION_HITLAG;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                target_y,
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_crouch_cancel_probe(void)
{
    pf_web_m4_crouch_cancel_result ordinary;
    pf_web_m4_crouch_cancel_result crouched;
    int32_t expected_x;
    int32_t expected_y;
    uint32_t expected_hitstun;

    if (!pf_web_m4_run_crouch_cancel_route(0, &ordinary) ||
        !pf_web_m4_run_crouch_cancel_route(1, &crouched))
    {
        return 0;
    }
    expected_x = (int32_t)(
        ((int64_t)ordinary.event.velocity_x_q16 *
         (int64_t)pf_web_m4_content.fighter
             .crouch_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_y = (int32_t)(
        ((int64_t)ordinary.event.velocity_y_q16 *
         (int64_t)pf_web_m4_content.fighter
             .crouch_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_hitstun =
        ((uint32_t)ordinary.hitstun_ticks *
         (uint32_t)pf_web_m4_content.fighter
             .crouch_cancel_hitstun_scale_q16) /
        (uint32_t)PF_Q16_ONE;
    if (ordinary.hitstun_ticks != UINT16_C(0) &&
        expected_hitstun == UINT32_C(0))
    {
        expected_hitstun = UINT32_C(1);
    }
    return (ordinary.event.flags &
            (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) == UINT16_C(0) &&
           (crouched.event.flags &
            (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) != UINT16_C(0) &&
           crouched.event.value_q16 == ordinary.event.value_q16 &&
           crouched.damage_q16 == ordinary.damage_q16 &&
           crouched.hitlag_ticks == ordinary.hitlag_ticks &&
           crouched.event.velocity_x_q16 == expected_x &&
           crouched.event.velocity_y_q16 == expected_y &&
           crouched.hitstun_ticks == (uint16_t)expected_hitstun &&
           crouched.hitstun_ticks < ordinary.hitstun_ticks;
}

static int pf_web_m4_run_weight_probe(void)
{
    pf_web_m4_crouch_cancel_result ordinary;
    pf_web_m4_crouch_cancel_result heavy;
    int route_passed = 0;
    int restored;

    if (pf_web_m4_content.fighter.weight_q16 == PF_Q16_ONE &&
        pf_web_m4_run_crouch_cancel_route(0, &ordinary))
    {
        pf_web_m4_content.fighter.weight_q16 =
            INT32_C(2) * PF_Q16_ONE;
        if (pf_web_m4_initialize_current_content() &&
            pf_web_m4_run_crouch_cancel_route(0, &heavy))
        {
            route_passed =
                (ordinary.event.flags &
                 (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) ==
                    UINT16_C(0) &&
                (heavy.event.flags &
                 (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) ==
                    UINT16_C(0) &&
                heavy.event.velocity_x_q16 ==
                    ordinary.event.velocity_x_q16 / INT32_C(2) &&
                heavy.event.velocity_y_q16 ==
                    ordinary.event.velocity_y_q16 / INT32_C(2) &&
                heavy.hitstun_ticks < ordinary.hitstun_ticks &&
                heavy.damage_q16 == ordinary.damage_q16 &&
                heavy.hitlag_ticks == ordinary.hitlag_ticks;
        }
    }

    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return route_passed != 0 && restored != 0;
}

static int pf_web_m4_run_reaction_probe(void)
{
    pf_m4_inspection inspection;
    int32_t target_x_before_sdi;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return 0;
    }
    target_x_before_sdi = inspection.players[1].position_x_q16;
    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
        inspection.players[1].position_x_q16 <= target_x_before_sdi)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].tech_window_ticks !=
            pf_web_m4_content.fighter.tech_window_ticks ||
        inspection.players[0].tech_lockout_ticks !=
            pf_web_m4_content.fighter.tech_lockout_ticks ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].tech_window_ticks + UINT16_C(1) !=
            pf_web_m4_content.fighter.tech_window_ticks ||
        inspection.players[0].tech_lockout_ticks + UINT16_C(1) !=
            pf_web_m4_content.fighter.tech_lockout_ticks)
    {
        return 0;
    }
    return pf_web_m4_run_crouch_cancel_probe() &&
           pf_web_m4_run_weight_probe();
}

static int pf_web_m4_run_shield_probe(void)
{
    pf_m4_inspection inspection;
    const int32_t shield_sdi_distance_q16 =
        (int32_t)(
            ((int64_t)pf_web_m4_content.fighter.sdi_distance_q16 *
             (int64_t)pf_web_m4_content.fighter
                 .shield_sdi_scale_q16) /
            (int64_t)PF_Q16_ONE);
    const int32_t shield_asdi_distance_q16 =
        (int32_t)(
            ((int64_t)pf_web_m4_content.fighter.asdi_distance_q16 *
             (int64_t)pf_web_m4_content.fighter
                 .shield_sdi_scale_q16) /
            (int64_t)PF_Q16_ONE);
    int32_t shield_sdi_start_x;
    int32_t shield_sdi_start_y;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint16_t)(
                pf_web_m4_content.fighter
                    .light_shield_trigger_threshold -
                UINT16_C(1)),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].shield_strength != UINT16_C(0) ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            pf_web_m4_content.fighter
                .light_shield_trigger_threshold,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].shield_strength !=
            pf_web_m4_content.fighter
                .light_shield_trigger_threshold ||
        inspection.players[0].shield_health_q16 !=
            pf_web_m4_content.fighter.shield_health_q16 -
                pf_web_m4_content.fighter
                    .light_shield_hold_depletion_q16 ||
        inspection.players[0].powershield != UINT8_C(0))
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(22); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return 0;
        }
    }
    if (!pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[1].shield_stun_ticks == UINT16_C(0) ||
        inspection.players[1].shield_health_q16 >=
            pf_web_m4_content.fighter.shield_health_q16 ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return 0;
    }
    shield_sdi_start_x = inspection.players[1].position_x_q16;
    shield_sdi_start_y = inspection.players[1].position_y_q16;
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                PF_WEB_M4_DASH_AXIS,
                tick >= UINT32_C(2) ? INT16_MIN : INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &inspection) ||
            inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
            inspection.players[1].position_y_q16 != shield_sdi_start_y)
        {
            return 0;
        }
        if (tick + UINT32_C(1) <
                (uint32_t)pf_web_m4_content.fighter
                    .jab_hitlag_ticks &&
            inspection.players[1].position_x_q16 !=
                shield_sdi_start_x + shield_sdi_distance_q16)
        {
            return 0;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        inspection.players[1].position_x_q16 !=
            shield_sdi_start_x + shield_sdi_distance_q16 +
                shield_asdi_distance_q16)
    {
        return 0;
    }

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(27); ++tick)
    {
        if (!pf_web_m4_tick(
                PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                -PF_WEB_M4_DASH_AXIS,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].powershield != UINT8_C(1) ||
        inspection.players[1].shield_health_q16 !=
            pf_web_m4_content.fighter.shield_health_q16 -
                pf_web_m4_content.fighter
                    .shield_hold_depletion_q16)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)pf_web_m4_content.fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(600); ++tick)
    {
        if (inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
        {
            break;
        }
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(1) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_ticks !=
            pf_web_m4_content.fighter
                .powershield_cancel_delay_ticks ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_UP_ATTACK &&
           inspection.players[1].powershield == UINT8_C(0);
}

static int pf_web_m4_run_shield_break_probe(void)
{
    pf_m4_inspection inspection;
    int saw_down = 0;
    int saw_stand = 0;
    uint32_t tick;

    if (!pf_web_m4_reset_internal())
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        if (!pf_web_m4_tick_with_triggers(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            break;
        }
    }
    if (tick == UINT32_C(300) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].shield_health_q16 != UINT32_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(1) ||
        pf_web_m4_last_result.event_count != UINT8_C(2) ||
        pf_web_m4_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BREAK ||
        pf_web_m4_last_result.events[0].source_player !=
            PF_SIM_EVENT_NO_PLAYER ||
        pf_web_m4_last_result.events[0].target_player != UINT8_C(0))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN)
        {
            saw_down = 1;
        }
        else if (
            inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
        {
            if (saw_down == 0)
            {
                return 0;
            }
            saw_stand = 1;
        }
    }
    if (tick == UINT32_C(300) ||
        saw_down == 0 ||
        saw_stand == 0 ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].invulnerable != UINT8_C(0) ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.shield_break_stun_ticks)
    {
        return 0;
    }

    if (!pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.shield_break_stun_ticks -
                UINT16_C(1) -
                pf_web_m4_content.fighter
                    .shield_break_mash_reduction_ticks ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_ticks !=
            pf_web_m4_content.fighter.shield_break_stun_ticks -
                UINT16_C(2) -
                pf_web_m4_content.fighter
                    .shield_break_mash_reduction_ticks)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(200); ++tick)
    {
        const uint64_t buttons =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_JUMP
                : PF_INPUT_BUTTON_ATTACK;

        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                buttons,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    return tick < UINT32_C(200) &&
           inspection.players[0].shield_health_q16 ==
               pf_web_m4_content.fighter.shield_reset_health_q16;
}

static int pf_web_m4_run_match_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK ||
        inspection.stock_count != PF_SIM_DEFAULT_STOCK_COUNT ||
        inspection.respawn_delay_ticks !=
            PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS ||
        inspection.respawn_invulnerability_ticks !=
            PF_SIM_DEFAULT_RESPAWN_INVULNERABILITY_TICKS)
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(600); ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_MIN,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        if (inspection.players[0].active == UINT8_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(600) ||
        inspection.players[0].stocks_remaining !=
            PF_SIM_DEFAULT_STOCK_COUNT - UINT8_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RESPAWN_WAIT ||
        inspection.players[0].respawn_ticks !=
            PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS ||
        inspection.terminated != UINT8_C(0))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].active != UINT8_C(1) ||
        inspection.players[0].respawn_ticks != UINT16_C(0) ||
        inspection.players[0].respawn_invulnerability_ticks !=
            UINT16_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_REVIVAL_PLATFORM ||
        inspection.players[0].invulnerable != UINT8_C(1) ||
        inspection.players[0].revival_platform_active != UINT8_C(1) ||
        inspection.players[0].revival_platform_y_q16 !=
            pf_web_m4_content.stage.revival_platform_start_y_q16 ||
        pf_web_m4_find_event(PF_SIM_EVENT_RESPAWN) == NULL)
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)pf_web_m4_content.stage
                 .revival_platform_descent_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_REVIVAL_PLATFORM ||
            inspection.players[0].action_ticks !=
                (uint16_t)(tick + UINT32_C(1)) ||
            inspection.players[0].velocity_x_q16 != INT32_C(0) ||
            inspection.players[0].velocity_y_q16 != INT32_C(0) ||
            inspection.players[0].revival_platform_active != UINT8_C(1))
        {
            return 0;
        }
    }
    if (inspection.players[0].revival_platform_y_q16 !=
            pf_web_m4_content.stage.revival_platform_end_y_q16 ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    if (!(inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AIRBORNE &&
           inspection.players[0].grounded == UINT8_C(0) &&
           inspection.players[0].support ==
               (uint8_t)PF_M4_SURFACE_NONE &&
           inspection.players[0].revival_platform_active == UINT8_C(0) &&
           inspection.players[0].respawn_invulnerability_ticks ==
               PF_SIM_DEFAULT_RESPAWN_INVULNERABILITY_TICKS &&
           inspection.players[0].invulnerable == UINT8_C(1) &&
           pf_web_m4_find_event(PF_SIM_EVENT_REVIVAL_DROP) != NULL &&
           pf_web_m4_find_event(PF_SIM_EVENT_REVIVAL_DROP)->detail ==
               UINT16_C(0)))
    {
        return 0;
    }
    return 1;
}

static int pf_web_m4_initialize_item_fixture(void)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_content.stage.spawn_spacing_q16 = PF_Q16_ONE;
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.item.enabled = UINT8_C(1);
    pf_web_m4_content.item.spawn_x_q16 =
        -PF_Q16_ONE / INT32_C(2);
    pf_web_m4_content.item.spawn_y_q16 =
        pf_web_m4_content.stage.floor_y_q16 -
        pf_web_m4_content.item.half_height_q16;
    return pf_web_m4_initialize_current_content();
}

static int pf_web_m4_initialize_live_item_lab(void)
{
    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_content.item.enabled = UINT8_C(1);
    pf_web_m4_content.item.lifetime_ticks = UINT16_C(3600);
    pf_web_m4_content.projectile.enabled = UINT8_C(1);
    pf_web_m4_content.reflector.enabled = UINT8_C(1);
    pf_web_m4_content.charge.enabled = UINT8_C(1);
    pf_web_m4_content.recovery.enabled = UINT8_C(1);
    return pf_web_m4_initialize_current_content() &&
           pf_web_m4_reset_internal();
}

static int pf_web_m4_pickup_item(pf_m4_inspection *out_inspection)
{
    const pf_sim_event *event;

    if (out_inspection == NULL || !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    event = pf_web_m4_find_event(PF_SIM_EVENT_ITEM_PICKUP);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        out_inspection->item.state !=
            (uint8_t)PF_M4_ITEM_STATE_HELD ||
        out_inspection->item.holder != UINT8_C(0))
    {
        return 0;
    }
    return pf_web_m4_tick(
        INT16_C(0),
        INT16_C(0),
        UINT64_C(0),
        INT16_C(0),
        INT16_C(0),
        UINT64_C(0),
        out_inspection);
}

static int pf_web_m4_advance_roll_to_tick(
    uint16_t target_tick,
    pf_m4_inspection *out_inspection)
{
    uint32_t guard;

    if (!pf_web_m4_tick_with_triggers(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(40); ++guard)
    {
        if (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_ROLL_FORWARD &&
            out_inspection->players[0].action_ticks == target_tick)
        {
            return 1;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int pf_web_m4_run_glide_toss_item_probe(void)
{
    pf_m4_inspection inspection;
    const pf_sim_event *event;

    if (!pf_web_m4_pickup_item(&inspection) ||
        !pf_web_m4_advance_roll_to_tick(
            pf_web_m4_content.item.glide_toss_end_tick,
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    event = pf_web_m4_find_event(PF_SIM_EVENT_ITEM_THROW);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ITEM_THROW ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.item.velocity_x_q16 <=
            pf_web_m4_content.item.forward_throw.velocity_x_q16)
    {
        return 0;
    }

    if (!pf_web_m4_pickup_item(&inspection) ||
        !pf_web_m4_advance_roll_to_tick(
            (uint16_t)(
                pf_web_m4_content.item.glide_toss_end_tick +
                UINT16_C(1)),
            &inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_ROLL_FORWARD &&
           inspection.item.state ==
               (uint8_t)PF_M4_ITEM_STATE_HELD &&
           inspection.item.holder == UINT8_C(0) &&
           pf_web_m4_find_event(PF_SIM_EVENT_ITEM_THROW) == NULL;
}

static int pf_web_m4_start_item_jump_from_dash(
    pf_m4_inspection *out_inspection)
{
    return pf_web_m4_tick(
               PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               UINT64_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           pf_web_m4_tick(
               PF_WEB_M4_DASH_AXIS,
               INT16_C(0),
               PF_INPUT_BUTTON_JUMP,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
}

static int pf_web_m4_run_jump_cancel_throw_item_probe(void)
{
    pf_m4_inspection inspection;
    const pf_sim_event *event;
    uint32_t guard;

    if (!pf_web_m4_pickup_item(&inspection) ||
        !pf_web_m4_start_item_jump_from_dash(&inspection) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    event = pf_web_m4_find_event(PF_SIM_EVENT_ITEM_THROW);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ITEM_THROW ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].velocity_x_q16 <=
            pf_web_m4_content.item.dash_throw_speed_q16 ||
        inspection.item.velocity_x_q16 <=
            pf_web_m4_content.item.forward_throw.velocity_x_q16)
    {
        return 0;
    }

    if (!pf_web_m4_pickup_item(&inspection) ||
        !pf_web_m4_start_item_jump_from_dash(&inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(8); ++guard)
    {
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        !pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[0].grounded == UINT8_C(0) &&
           inspection.players[0].action_state !=
               (uint8_t)PF_M4_ACTION_ITEM_THROW &&
           inspection.item.state ==
               (uint8_t)PF_M4_ITEM_STATE_AIRBORNE &&
           pf_web_m4_find_event(PF_SIM_EVENT_ITEM_THROW) != NULL;
}

static int pf_web_m4_run_bat_drop_route(int align_with_target)
{
    pf_m4_inspection inspection;
    const pf_sim_event *event;
    uint32_t guard;

    if (!pf_web_m4_pickup_item(&inspection) ||
        !pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(80); ++guard)
    {
        const int64_t delta =
            (int64_t)inspection.item.position_x_q16 -
            (int64_t)inspection.players[1].position_x_q16;

        if (inspection.players[0].grounded == UINT8_C(0) &&
            (align_with_target == 0 ||
             (delta >= -(int64_t)(PF_Q16_ONE / INT32_C(2)) &&
              delta <= (int64_t)(PF_Q16_ONE / INT32_C(2)))))
        {
            break;
        }
        if (!pf_web_m4_tick(
                align_with_target != 0
                    ? PF_WEB_M4_DASH_AXIS
                    : INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (guard == UINT32_C(80) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        !pf_web_m4_tick_with_triggers(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        pf_web_m4_find_event(PF_SIM_EVENT_ITEM_DROP) == NULL)
    {
        return 0;
    }

    for (guard = UINT32_C(0); guard < UINT32_C(180); ++guard)
    {
        event = pf_web_m4_find_event(PF_SIM_EVENT_ITEM_HIT);
        if (event != NULL)
        {
            return align_with_target != 0 &&
                   event->source_player == UINT8_C(0) &&
                   event->target_player == UINT8_C(1) &&
                   event->value_q16 ==
                       pf_web_m4_content.item.damage_q16 &&
                   inspection.players[1].damage_q16 ==
                       pf_web_m4_content.item.damage_q16 &&
                   inspection.item.velocity_y_q16 < INT32_C(0);
        }
        if (inspection.item.state !=
            (uint8_t)PF_M4_ITEM_STATE_AIRBORNE)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    return align_with_target == 0 &&
           guard < UINT32_C(180) &&
           inspection.players[1].damage_q16 == UINT32_C(0);
}

static int pf_web_m4_run_item_probes(
    int *out_bat_drop,
    int *out_glide_toss,
    int *out_jump_cancel_throw)
{
    int restored;

    if (out_bat_drop == NULL || out_glide_toss == NULL ||
        out_jump_cancel_throw == NULL ||
        !pf_web_m4_initialize_item_fixture())
    {
        return 0;
    }
    *out_bat_drop =
        pf_web_m4_run_bat_drop_route(1) &&
        pf_web_m4_run_bat_drop_route(0);
    *out_glide_toss = pf_web_m4_run_glide_toss_item_probe();
    *out_jump_cancel_throw =
        pf_web_m4_run_jump_cancel_throw_item_probe();
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return restored;
}

static int pf_web_m4_run_short_hop_laser_probe(void)
{
    pf_m4_inspection inspection;
    const pf_sim_event *event = NULL;
    uint32_t guard;
    int fired = 0;
    int passed = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK)
    {
        return 0;
    }
    pf_web_m4_content.stage.spawn_spacing_q16 =
        INT32_C(2) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    pf_web_m4_content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    pf_web_m4_content.projectile.enabled = UINT8_C(1);

    if (pf_web_m4_initialize_current_content() &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        for (guard = UINT32_C(0);
             guard < UINT32_C(12) &&
             inspection.players[0].grounded != UINT8_C(0);
             ++guard)
        {
            if (!pf_web_m4_tick(
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection))
            {
                break;
            }
        }
        if (guard < UINT32_C(12) &&
            inspection.players[0].grounded == UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE &&
            pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_SPECIAL,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            event = pf_web_m4_find_event(PF_SIM_EVENT_PROJECTILE_FIRE);
            fired =
                event != NULL && event->source_player == UINT8_C(0) &&
                event->detail ==
                    (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR &&
                inspection.projectile.state ==
                    (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE;
        }
        if (fired != 0)
        {
            for (guard = UINT32_C(0);
                 guard < UINT32_C(180) &&
                 inspection.players[0].grounded == UINT8_C(0);
                 ++guard)
            {
                if (!pf_web_m4_tick(
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection))
                {
                    break;
                }
            }
            passed =
                guard < UINT32_C(180) &&
                inspection.players[0].grounded != UINT8_C(0) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_LANDING;
        }
    }

    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

typedef struct pf_web_m4_camping_trace
{
    uint32_t projectile_fires;
    uint32_t projectile_hits;
    uint32_t approach_hits;
    uint32_t camper_damage_q16;
    uint32_t approacher_damage_q16;
    int32_t minimum_separation_q16;
    uint64_t completed_ticks;
} pf_web_m4_camping_trace;

static int pf_web_m4_run_camping_trace(
    int fire_projectiles,
    pf_web_m4_camping_trace *out_trace)
{
    pf_m4_inspection inspection;
    uint32_t tick;
    int special_held_previous_tick = 0;

    if (out_trace == NULL || !pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    (void)memset(out_trace, 0, sizeof(*out_trace));
    out_trace->minimum_separation_q16 = INT32_MAX;
    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        const int32_t separation_q16 =
            inspection.players[1].position_x_q16 -
            inspection.players[0].position_x_q16;
        const int approach_attack_requested =
            separation_q16 <= INT32_C(2) * PF_Q16_ONE &&
            (tick & UINT32_C(1)) == UINT32_C(0);
        const int special_requested =
            fire_projectiles != 0 &&
            special_held_previous_tick == 0 &&
            inspection.projectile.state ==
                (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        const pf_sim_event *event;

        if (separation_q16 < out_trace->minimum_separation_q16)
        {
            out_trace->minimum_separation_q16 = separation_q16;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                special_requested != 0
                    ? PF_INPUT_BUTTON_SPECIAL
                    : UINT64_C(0),
                INT16_MIN,
                INT16_C(0),
                approach_attack_requested != 0
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                &inspection) ||
            pf_web_m4_last_result.terminated != UINT8_C(0) ||
            pf_web_m4_last_result.truncated != UINT8_C(0))
        {
            return 0;
        }
        special_held_previous_tick = special_requested;
        event = pf_web_m4_find_event(PF_SIM_EVENT_PROJECTILE_FIRE);
        if (event != NULL && event->source_player == UINT8_C(0))
        {
            ++out_trace->projectile_fires;
        }
        event = pf_web_m4_find_event(PF_SIM_EVENT_PROJECTILE_HIT);
        if (event != NULL && event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            ++out_trace->projectile_hits;
        }
        event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
        if (event != NULL && event->source_player == UINT8_C(1) &&
            event->target_player == UINT8_C(0))
        {
            ++out_trace->approach_hits;
        }
    }
    out_trace->camper_damage_q16 = inspection.players[0].damage_q16;
    out_trace->approacher_damage_q16 =
        inspection.players[1].damage_q16;
    out_trace->completed_ticks = inspection.tick;
    return 1;
}

static int pf_web_m4_run_camping_probe(void)
{
    pf_web_m4_camping_trace camping;
    pf_web_m4_camping_trace no_fire;
    int passed = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK)
    {
        pf_web_m4_content.projectile.enabled = UINT8_C(1);
        pf_web_m4_content.stage.spawn_spacing_q16 =
            INT32_C(8) * PF_Q16_ONE;
        pf_web_m4_content.stage.platform_motion_amplitude_q16 =
            INT32_C(0);
        if (pf_web_m4_initialize_current_content() &&
            pf_web_m4_run_camping_trace(1, &camping) &&
            pf_web_m4_run_camping_trace(0, &no_fire))
        {
            passed =
                camping.completed_ticks == UINT64_C(180) &&
                camping.projectile_fires == UINT32_C(7) &&
                camping.projectile_hits == UINT32_C(6) &&
                camping.approach_hits == UINT32_C(0) &&
                camping.camper_damage_q16 == UINT32_C(0) &&
                camping.approacher_damage_q16 >=
                    UINT32_C(18) * UINT32_C(65536) &&
                camping.minimum_separation_q16 ==
                    PF_WEB_M4_CAMPING_MINIMUM_SEPARATION_Q16 &&
                no_fire.completed_ticks == UINT64_C(180) &&
                no_fire.projectile_fires == UINT32_C(0) &&
                no_fire.projectile_hits == UINT32_C(0) &&
                no_fire.approach_hits == UINT32_C(3) &&
                no_fire.camper_damage_q16 != UINT32_C(0) &&
                no_fire.minimum_separation_q16 <
                    camping.minimum_separation_q16;
        }
    }
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_run_shine_spike_case(
    int attack_enabled,
    int *out_reflector_hit,
    int *out_recovered)
{
    pf_m4_inspection inspection;
    uint32_t target_offstage_ticks = UINT32_C(0);
    int target_left_stage = 0;
    int reflector_used = 0;
    int ko_seen = 0;
    uint32_t tick;

    if (out_reflector_hit == NULL || out_recovered == NULL ||
        !pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    *out_reflector_hit = 0;
    *out_recovered = 0;
    for (tick = UINT32_C(0); tick < UINT32_C(260); ++tick)
    {
        int16_t player0_x = INT16_MAX;
        int16_t player0_y = INT16_C(0);
        uint64_t player0_buttons = UINT64_C(0);
        int16_t player1_x = INT16_MAX;
        uint64_t player1_buttons = UINT64_C(0);
        const pf_sim_event *event;

        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            return attack_enabled != 0 &&
                   *out_reflector_hit != 0 && ko_seen != 0;
        }
        if (target_left_stage != 0 &&
            inspection.players[1].grounded != UINT8_C(0))
        {
            *out_recovered = 1;
            return attack_enabled == 0;
        }
        if (inspection.players[1].grounded == UINT8_C(0))
        {
            target_left_stage = 1;
            ++target_offstage_ticks;
            player1_x = INT16_MIN;
            if (target_offstage_ticks == UINT32_C(9))
            {
                player1_buttons = PF_INPUT_BUTTON_JUMP;
            }
        }
        if (attack_enabled == 0)
        {
            player0_x = INT16_C(0);
        }
        else if (reflector_used == 0 &&
                 inspection.players[0].grounded == UINT8_C(0) &&
                 target_left_stage != 0)
        {
            player0_y = INT16_MAX;
            player0_buttons = PF_INPUT_BUTTON_SPECIAL;
            reflector_used = 1;
        }
        if (!pf_web_m4_tick(
                player0_x,
                player0_y,
                player0_buttons,
                player1_x,
                INT16_C(0),
                player1_buttons,
                &inspection))
        {
            return 0;
        }
        event = pf_web_m4_find_event(PF_SIM_EVENT_HIT);
        if (event != NULL &&
            event->detail == (uint16_t)PF_M4_ACTION_REFLECTOR_AIR &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1) &&
            event->velocity_y_q16 > INT32_C(0))
        {
            *out_reflector_hit = 1;
        }
        event = pf_web_m4_find_event(PF_SIM_EVENT_KO);
        if (event != NULL && event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            ko_seen = 1;
        }
    }
    return 0;
}

static int pf_web_m4_run_shine_spike_probe(void)
{
    int reflector_hit = 0;
    int recovered = 0;
    int control_hit = 0;
    int control_recovered = 0;
    int passed = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK)
    {
        pf_web_m4_content.reflector.enabled = UINT8_C(1);
        pf_web_m4_content.projectile.enabled = UINT8_C(1);
        pf_web_m4_content.stage.floor_left_q16 =
            -INT32_C(8) * PF_Q16_ONE;
        pf_web_m4_content.stage.floor_right_q16 =
            INT32_C(8) * PF_Q16_ONE;
        pf_web_m4_content.stage.platform_center_x_q16 =
            -INT32_C(4) * PF_Q16_ONE;
        pf_web_m4_content.stage.platform_half_width_q16 = PF_Q16_ONE;
        pf_web_m4_content.stage.platform_motion_amplitude_q16 =
            INT32_C(0);
        pf_web_m4_content.stage.upper_platform_center_x_q16 =
            INT32_C(0);
        pf_web_m4_content.stage.upper_platform_half_width_q16 =
            PF_Q16_ONE;
        pf_web_m4_content.stage.solid_left_q16 =
            INT32_C(2) * PF_Q16_ONE;
        pf_web_m4_content.stage.solid_right_q16 =
            INT32_C(6) * PF_Q16_ONE;
        pf_web_m4_content.stage.blast_left_q16 =
            -INT32_C(12) * PF_Q16_ONE;
        pf_web_m4_content.stage.blast_right_q16 =
            INT32_C(12) * PF_Q16_ONE;
        pf_web_m4_content.stage.blast_bottom_q16 =
            INT32_C(40) * PF_Q16_ONE;
        pf_web_m4_content.stage.spawn_spacing_q16 = PF_Q16_ONE;
        if (pf_web_m4_initialize_current_content() &&
            pf_web_m4_run_shine_spike_case(
                1,
                &reflector_hit,
                &recovered) &&
            pf_web_m4_run_shine_spike_case(
                0,
                &control_hit,
                &control_recovered))
        {
            passed = reflector_hit != 0 && recovered == 0 &&
                     control_hit == 0 && control_recovered != 0;
        }
    }
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_run_charge_storage_probe(void)
{
    pf_m4_inspection inspection;
    uint16_t stored_charge;
    uint32_t tick;
    int passed = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK)
    {
        pf_web_m4_content.charge.enabled = UINT8_C(1);
        if (pf_web_m4_initialize_current_content() &&
            pf_web_m4_reset_internal() &&
            pf_web_m4_tick(
                INT16_C(0),
                INT16_MIN,
                PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_CHARGE_GROUND &&
            inspection.players[0].charge_ticks == UINT16_C(1))
        {
            for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
            {
                if (!pf_web_m4_tick(
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection))
                {
                    break;
                }
            }
            stored_charge = inspection.players[0].charge_ticks;
            if (tick == UINT32_C(5) && stored_charge == UINT16_C(6) &&
                pf_web_m4_tick_with_triggers(
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_MAX,
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    &inspection) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND &&
                pf_web_m4_tick(
                    INT16_C(0),
                    INT16_C(0),
                    PF_INPUT_BUTTON_ATTACK,
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
                inspection.players[0].charge_ticks == stored_charge)
            {
                for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
                {
                    if (!pf_web_m4_tick(
                            INT16_C(0),
                            INT16_C(0),
                            UINT64_C(0),
                            INT16_C(0),
                            INT16_C(0),
                            UINT64_C(0),
                            &inspection))
                    {
                        break;
                    }
                }
                if (tick == UINT32_C(20) &&
                    inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                    pf_web_m4_tick(
                        INT16_C(0),
                        INT16_MIN,
                        PF_INPUT_BUTTON_SPECIAL |
                            PF_INPUT_BUTTON_ATTACK,
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection) &&
                    inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_CHARGE_GROUND &&
                    inspection.players[0].charge_ticks ==
                        (uint16_t)(stored_charge + UINT16_C(1)) &&
                    pf_web_m4_tick(
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection) &&
                    pf_web_m4_tick(
                        INT16_C(0),
                        INT16_C(0),
                        PF_INPUT_BUTTON_ATTACK,
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection) &&
                    inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND)
                {
                    passed = 1;
                }
            }
        }
    }
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_run_vector_ascent_probe(void)
{
    pf_m4_inspection inspection;
    int32_t landed_x;
    uint32_t guard;
    int grounded_passed = 0;
    int aerial_passed = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK)
    {
        pf_web_m4_content.recovery.enabled = UINT8_C(1);
        if (pf_web_m4_initialize_current_content() &&
            pf_web_m4_reset_internal() &&
            pf_web_m4_tick(
                INT16_C(0),
                INT16_MIN,
                PF_INPUT_BUTTON_SPECIAL,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) &&
            inspection.players[0].grounded == UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_VECTOR_ASCENT &&
            inspection.players[0].recovery_available == UINT8_C(0))
        {
            for (guard = UINT32_C(0);
                 guard < UINT32_C(240) &&
                 inspection.players[0].grounded == UINT8_C(0);
                 ++guard)
            {
                if (!pf_web_m4_tick(
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection))
                {
                    break;
                }
            }
            if (guard < UINT32_C(240) &&
                inspection.players[0].recovery_available == UINT8_C(1))
            {
                for (guard = UINT32_C(0);
                     guard < UINT32_C(32) &&
                     inspection.players[0].action_state !=
                         (uint8_t)PF_M4_ACTION_GROUND_IDLE;
                     ++guard)
                {
                    if (!pf_web_m4_tick(
                            INT16_C(0),
                            INT16_C(0),
                            UINT64_C(0),
                            INT16_C(0),
                            INT16_C(0),
                            UINT64_C(0),
                            &inspection))
                    {
                        break;
                    }
                }
                landed_x = inspection.players[0].position_x_q16;
                if (guard < UINT32_C(32) &&
                    pf_web_m4_tick(
                        INT16_MAX,
                        INT16_C(0),
                        UINT64_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection) &&
                    inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
                    inspection.players[0].position_x_q16 > landed_x)
                {
                    grounded_passed = 1;
                }
            }
        }
        if (pf_web_m4_reset_internal() &&
            pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            for (guard = UINT32_C(0);
                 guard < UINT32_C(8) &&
                 inspection.players[0].grounded != UINT8_C(0);
                 ++guard)
            {
                if (!pf_web_m4_tick(
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        &inspection))
                {
                    break;
                }
            }
            if (guard < UINT32_C(8) &&
                inspection.players[0].grounded == UINT8_C(0) &&
                inspection.players[0].recovery_available == UINT8_C(1) &&
                pf_web_m4_tick(
                    INT16_MAX,
                    INT16_MIN,
                    PF_INPUT_BUTTON_SPECIAL,
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_VECTOR_ASCENT &&
                inspection.players[0].action_ticks == UINT16_C(1) &&
                inspection.players[0].recovery_available == UINT8_C(0) &&
                inspection.players[0].velocity_x_q16 > INT32_C(0) &&
                inspection.players[0].velocity_y_q16 < INT32_C(0))
            {
                aerial_passed = 1;
            }
        }
    }
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return grounded_passed != 0 && aerial_passed != 0 &&
           restored != 0;
}

static int pf_web_m4_run_moonwalk_probe(void)
{
    pf_m4_inspection inspection;
    int passed = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content() &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        pf_web_m4_tick(
            PF_WEB_M4_DASH_AXIS,
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP &&
        inspection.players[0].action_ticks ==
            pf_web_m4_content.fighter.moonwalk_setup_ticks &&
        inspection.players[0].facing == INT8_C(1) &&
        pf_web_m4_tick(
            INT16_C(0),
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP &&
        inspection.players[0].action_ticks ==
            pf_web_m4_content.fighter.moonwalk_setup_ticks &&
        pf_web_m4_tick(
            -PF_WEB_M4_DASH_AXIS,
            PF_WEB_M4_DASH_AXIS,
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP &&
        inspection.players[0].action_ticks ==
            pf_web_m4_content.fighter.moonwalk_setup_ticks &&
        pf_web_m4_tick(
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_MOONWALK &&
        inspection.players[0].facing == INT8_C(1) &&
        inspection.players[0].velocity_x_q16 < INT32_C(0) &&
        pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
        inspection.players[0].facing == INT8_C(1) &&
        inspection.players[0].velocity_x_q16 < INT32_C(0) &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        pf_web_m4_tick(
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        inspection.players[0].facing == INT8_C(-1) &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_tick(
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        pf_web_m4_tick(
            -PF_WEB_M4_WALK_AXIS,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_MOONWALK_SETUP &&
        inspection.players[0].action_ticks == UINT16_C(1) &&
        pf_web_m4_tick(
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        inspection.players[0].facing == INT8_C(-1))
    {
        passed = 1;
    }
    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return passed != 0 && restored != 0;
}

static int pf_web_m4_enter_right_teeter(
    uint64_t edge_buttons,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (pf_m4_inspect(pf_web_m4_sim, out_inspection) != PF_STATUS_OK)
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const int32_t distance_q16 =
            pf_web_m4_content.stage.floor_right_q16 -
            out_inspection->players[0].position_x_q16;

        if (distance_q16 <= INT32_C(3) * PF_Q16_ONE)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0))
        {
            return 0;
        }
    }

    for (tick = UINT32_C(0); tick < UINT32_C(60); ++tick)
    {
        if (out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                edge_buttons,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0) ||
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_TEETER)
        {
            return 0;
        }
    }

    for (tick = UINT32_C(0); tick < UINT32_C(300); ++tick)
    {
        const int32_t velocity_q16 =
            out_inspection->players[0].velocity_x_q16;
        const int32_t distance_q16 =
            pf_web_m4_content.stage.floor_right_q16 -
            out_inspection->players[0].position_x_q16;
        const int32_t release_velocity_q16 =
            velocity_q16 > pf_web_m4_content.fighter.traction_q16
                ? velocity_q16 -
                      pf_web_m4_content.fighter.traction_q16
                : INT32_C(0);
        int16_t selected_axis = INT16_C(0);
        int32_t selected_velocity_q16 = INT32_C(0);
        uint32_t axis;

        if (release_velocity_q16 > distance_q16)
        {
            if (!pf_web_m4_tick(
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    out_inspection))
            {
                return 0;
            }
            break;
        }

        for (axis =
                 (uint32_t)pf_web_m4_content.fighter.axis_dead_zone +
                     UINT32_C(1);
             axis <
                 (uint32_t)
                     pf_web_m4_content.fighter.dash_axis_threshold;
             ++axis)
        {
            const int32_t target_q16 =
                (int32_t)(
                    (int64_t)(int32_t)axis *
                    (int64_t)
                        pf_web_m4_content.fighter.walk_speed_q16 /
                    INT64_C(32767));
            int32_t next_velocity_q16 = velocity_q16;
            const int32_t acceleration_q16 =
                pf_web_m4_content.fighter.ground_acceleration_q16;
            int32_t next_release_velocity_q16;

            if (next_velocity_q16 < target_q16)
            {
                next_velocity_q16 += acceleration_q16;
                if (next_velocity_q16 > target_q16)
                {
                    next_velocity_q16 = target_q16;
                }
            }
            else if (next_velocity_q16 > target_q16)
            {
                next_velocity_q16 -= acceleration_q16;
                if (next_velocity_q16 < target_q16)
                {
                    next_velocity_q16 = target_q16;
                }
            }
            next_release_velocity_q16 =
                next_velocity_q16 >
                        pf_web_m4_content.fighter.traction_q16
                    ? next_velocity_q16 -
                          pf_web_m4_content.fighter.traction_q16
                    : INT32_C(0);

            if (next_velocity_q16 < distance_q16 &&
                distance_q16 - next_velocity_q16 <
                    next_release_velocity_q16)
            {
                selected_axis = (int16_t)axis;
                selected_velocity_q16 = next_velocity_q16;
                break;
            }
            if (next_velocity_q16 < distance_q16 &&
                next_velocity_q16 > selected_velocity_q16)
            {
                selected_axis = (int16_t)axis;
                selected_velocity_q16 = next_velocity_q16;
            }
        }
        if (selected_axis == INT16_C(0) ||
            !pf_web_m4_tick(
                selected_axis,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection) ||
            out_inspection->players[0].grounded == UINT8_C(0))
        {
            return 0;
        }
    }

    return out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_TEETER &&
           out_inspection->players[0].action_ticks == UINT16_C(0) &&
           out_inspection->players[0].position_x_q16 ==
               pf_web_m4_content.stage.floor_right_q16 &&
           out_inspection->players[0].velocity_x_q16 == INT32_C(0) &&
           out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_FLOOR &&
           out_inspection->players[0].facing == INT8_C(1);
}

static int pf_web_m4_run_teeter_cancel_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;
    int attack_cancel = 0;
    int reverse_dash_cancel = 0;
    int held_ran_off = 0;
    int held_saw_teeter = 0;
    int early_release = 1;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content() &&
        pf_web_m4_reset_internal() &&
        pf_web_m4_enter_right_teeter(UINT64_C(0), &inspection) &&
        pf_web_m4_tick(
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
        inspection.players[0].grounded != UINT8_C(0) &&
        inspection.players[0].position_x_q16 ==
            pf_web_m4_content.stage.floor_right_q16)
    {
        attack_cancel = 1;
    }

    if (pf_web_m4_reset_internal() &&
        pf_web_m4_enter_right_teeter(UINT64_C(0), &inspection) &&
        pf_web_m4_tick(
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
        inspection.players[0].facing == INT8_C(-1) &&
        inspection.players[0].dash_direction == INT8_C(-1) &&
        inspection.players[0].grounded != UINT8_C(0) &&
        inspection.players[0].position_x_q16 <
            pf_web_m4_content.stage.floor_right_q16)
    {
        reverse_dash_cancel = 1;
    }

    if (!pf_web_m4_reset_internal())
    {
        early_release = 0;
    }
    for (tick = UINT32_C(0);
         early_release != 0 && tick < UINT32_C(300);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            early_release = 0;
            break;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_TEETER)
        {
            held_saw_teeter = 1;
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            held_ran_off = 1;
            break;
        }
    }

    if (!pf_web_m4_reset_internal())
    {
        early_release = 0;
    }
    for (tick = UINT32_C(0);
         early_release != 0 && tick < UINT32_C(20);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            early_release = 0;
        }
    }
    for (tick = UINT32_C(0);
         early_release != 0 && tick < UINT32_C(60);
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_TEETER)
        {
            early_release = 0;
            break;
        }
        if (inspection.players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    early_release =
        early_release != 0 &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
        inspection.players[0].position_x_q16 <
            pf_web_m4_content.stage.floor_right_q16 -
                pf_web_m4_content.fighter.teeter_snap_distance_q16;

    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return attack_cancel != 0 && reverse_dash_cancel != 0 &&
           held_ran_off != 0 && held_saw_teeter == 0 &&
           early_release != 0 && restored != 0;
}

static int pf_web_m4_run_taunt_cancel_probe(void)
{
    pf_m4_inspection inspection;
    int32_t dash_position_q16 = INT32_C(0);
    uint32_t tick;
    int full_duration = 1;
    int edge_cancel = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content() ||
        !pf_web_m4_reset_internal() ||
        !pf_web_m4_tick(
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        full_duration = 0;
    }
    else
    {
        dash_position_q16 = inspection.players[0].position_x_q16;
    }
    if (full_duration != 0 &&
        (!pf_web_m4_tick(
             INT16_C(0),
             INT16_C(0),
             PF_INPUT_BUTTON_TAUNT,
             INT16_C(0),
             INT16_C(0),
             UINT64_C(0),
             &inspection) ||
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_TAUNT ||
         inspection.players[0].action_ticks != UINT16_C(1) ||
         inspection.players[0].position_x_q16 <= dash_position_q16 ||
         inspection.players[0].velocity_x_q16 !=
             pf_web_m4_content.fighter.initial_dash_speed_q16 -
                 pf_web_m4_content.fighter.traction_q16))
    {
        full_duration = 0;
    }
    for (tick = UINT32_C(1);
         full_duration != 0 &&
         tick < (uint32_t)pf_web_m4_content.fighter.taunt_ticks;
         ++tick)
    {
        if (!pf_web_m4_tick(
                INT16_MIN,
                INT16_C(0),
                PF_INPUT_BUTTON_TAUNT,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            (tick + UINT32_C(1) <
                 (uint32_t)pf_web_m4_content.fighter.taunt_ticks &&
             (inspection.players[0].action_state !=
                  (uint8_t)PF_M4_ACTION_TAUNT ||
              inspection.players[0].action_ticks !=
                  (uint16_t)(tick + UINT32_C(1)))))
        {
            full_duration = 0;
        }
    }
    if (full_duration != 0 &&
        (inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
         inspection.players[0].action_ticks != UINT16_C(0) ||
         !pf_web_m4_tick(
             INT16_C(0),
             INT16_C(0),
             PF_INPUT_BUTTON_TAUNT,
             INT16_C(0),
             INT16_C(0),
             UINT64_C(0),
             &inspection) ||
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE))
    {
        full_duration = 0;
    }

    if (pf_web_m4_reset_internal() &&
        pf_web_m4_enter_right_teeter(
            PF_INPUT_BUTTON_TAUNT,
            &inspection) &&
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_TEETER &&
        inspection.players[0].action_ticks == UINT16_C(0) &&
        inspection.players[0].position_x_q16 ==
            pf_web_m4_content.stage.floor_right_q16)
    {
        edge_cancel = 1;
    }

    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return full_duration != 0 && edge_cancel != 0 && restored != 0;
}

static int pf_web_m4_run_stage_humping_probe(void)
{
    pf_m4_inspection inspection;
    int32_t start_position_q16;
    int32_t held_position_q16;
    uint32_t repetition;
    int repeated_steps = 1;
    int held_negative = 0;
    int neutral_down_negative = 0;
    int horizontal_only_negative = 0;
    int restored;

    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content() ||
        !pf_web_m4_reset_internal() ||
        pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK)
    {
        repeated_steps = 0;
        start_position_q16 = INT32_C(0);
    }
    else
    {
        start_position_q16 = inspection.players[0].position_x_q16;
    }
    for (repetition = UINT32_C(0);
         repeated_steps != 0 && repetition < UINT32_C(8);
         ++repetition)
    {
        if (!pf_web_m4_tick(
                INT16_MAX,
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
            inspection.players[0].position_x_q16 !=
                start_position_q16 +
                    (int32_t)(repetition + UINT32_C(1)) *
                        pf_web_m4_content.fighter
                            .crouch_step_speed_q16 ||
            !pf_web_m4_tick(
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_CROUCH ||
            inspection.players[0].velocity_x_q16 != INT32_C(0))
        {
            repeated_steps = 0;
        }
    }
    if (repeated_steps != 0 &&
        (!pf_web_m4_tick(
             INT16_MIN,
             INT16_MAX,
             UINT64_C(0),
             INT16_C(0),
             INT16_C(0),
             UINT64_C(0),
             &inspection) ||
         inspection.players[0].action_state !=
             (uint8_t)PF_M4_ACTION_CROUCH_STEP ||
         inspection.players[0].facing != INT8_C(-1) ||
         inspection.players[0].position_x_q16 !=
             start_position_q16 +
                 INT32_C(7) *
                     pf_web_m4_content.fighter
                         .crouch_step_speed_q16))
    {
        repeated_steps = 0;
    }

    if (pf_web_m4_reset_internal() &&
        pf_m4_inspect(pf_web_m4_sim, &inspection) == PF_STATUS_OK)
    {
        start_position_q16 = inspection.players[0].position_x_q16;
        if (pf_web_m4_tick(
                INT16_MAX,
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            held_position_q16 =
                inspection.players[0].position_x_q16;
            if (held_position_q16 ==
                    start_position_q16 +
                        pf_web_m4_content.fighter
                            .crouch_step_speed_q16 &&
                pf_web_m4_tick(
                    INT16_MAX,
                    INT16_MAX,
                    UINT64_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection) &&
                pf_web_m4_tick(
                    INT16_MAX,
                    INT16_MAX,
                    UINT64_C(0),
                    INT16_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_CROUCH &&
                inspection.players[0].position_x_q16 ==
                    held_position_q16)
            {
                held_negative = 1;
            }
        }
    }

    if (pf_web_m4_reset_internal() &&
        pf_m4_inspect(pf_web_m4_sim, &inspection) == PF_STATUS_OK)
    {
        start_position_q16 = inspection.players[0].position_x_q16;
        if (pf_web_m4_tick(
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_CROUCH &&
            inspection.players[0].position_x_q16 ==
                start_position_q16)
        {
            neutral_down_negative = 1;
        }
    }

    if (pf_web_m4_reset_internal() &&
        pf_m4_inspect(pf_web_m4_sim, &inspection) == PF_STATUS_OK)
    {
        start_position_q16 = inspection.players[0].position_x_q16;
        if (pf_web_m4_tick(
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_INITIAL_DASH &&
            inspection.players[0].position_x_q16 >
                start_position_q16)
        {
            horizontal_only_negative = 1;
        }
    }

    restored =
        pf_m4_default_content(&pf_web_m4_content) == PF_STATUS_OK &&
        pf_web_m4_initialize_current_content();
    return repeated_steps != 0 && held_negative != 0 &&
           neutral_down_negative != 0 &&
           horizontal_only_negative != 0 && restored != 0;
}

static int pf_web_m4_render(void)
{
    pf_m4_inspection inspection;
    uint32_t event_index;
    uint32_t player_index;

    if (pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK ||
        inspection.tick > (uint64_t)INT32_MAX)
    {
        return 0;
    }

    (void)memset(pf_web_m4_view, 0, sizeof(pf_web_m4_view));
    pf_web_m4_view[PF_WEB_M4_VIEW_SCHEMA] = INT32_C(47);
    pf_web_m4_view[PF_WEB_M4_VIEW_TICK] =
        (int32_t)inspection.tick;
    pf_web_m4_view[PF_WEB_M4_VIEW_FLOOR_LEFT] =
        inspection.stage.floor_left_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_FLOOR_RIGHT] =
        inspection.stage.floor_right_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_FLOOR_Y] =
        inspection.stage.floor_y_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_PLATFORM_LEFT] =
        inspection.stage.platform_left_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_PLATFORM_RIGHT] =
        inspection.stage.platform_right_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_PLATFORM_Y] =
        inspection.stage.platform_y_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_BLAST_LEFT] =
        inspection.stage.blast_left_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_BLAST_RIGHT] =
        inspection.stage.blast_right_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_BLAST_TOP] =
        inspection.stage.blast_top_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_BLAST_BOTTOM] =
        inspection.stage.blast_bottom_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_FIGHTER_HALF_WIDTH] =
        pf_web_m4_content.fighter.half_width_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_FIGHTER_HALF_HEIGHT] =
        pf_web_m4_content.fighter.half_height_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_SOLID_LEFT] =
        inspection.stage.solid_left_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_SOLID_RIGHT] =
        inspection.stage.solid_right_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_SOLID_TOP] =
        inspection.stage.solid_top_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_SOLID_BOTTOM] =
        inspection.stage.solid_bottom_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_STOCK_COUNT] =
        (int32_t)inspection.stock_count;
    pf_web_m4_view[PF_WEB_M4_VIEW_RESPAWN_DELAY] =
        (int32_t)inspection.respawn_delay_ticks;
    pf_web_m4_view[PF_WEB_M4_VIEW_RESPAWN_INVULNERABILITY] =
        (int32_t)inspection.respawn_invulnerability_ticks;
    pf_web_m4_view[PF_WEB_M4_VIEW_SUDDEN_DEATH] =
        (int32_t)inspection.sudden_death;
    pf_web_m4_view[PF_WEB_M4_VIEW_TERMINATED] =
        (int32_t)inspection.terminated;
    pf_web_m4_view[PF_WEB_M4_VIEW_TRUNCATED] =
        (int32_t)inspection.truncated;
    pf_web_m4_view[PF_WEB_M4_VIEW_WINNER_MASK] =
        (int32_t)inspection.winner_mask;

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)pf_web_m4_player_count;
         ++player_index)
    {
        const pf_m4_player_inspection *player =
            &inspection.players[player_index];
        const int base =
            PF_WEB_M4_VIEW_PLAYER0 +
            (int)player_index * PF_WEB_M4_VIEW_PLAYER_STRIDE;

        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_X] =
            player->position_x_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_Y] =
            player->position_y_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_VX] =
            player->velocity_x_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_VY] =
            player->velocity_y_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_ACTION] =
            (int32_t)player->action_state;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_FACING] =
            (int32_t)player->facing;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GROUNDED] =
            (int32_t)player->grounded;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_SUPPORT] =
            (int32_t)player->support;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_AIR_JUMPS] =
            (int32_t)player->air_jumps_remaining;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_FAST_FALL] =
            (int32_t)player->fast_fall;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_RESPAWNS] =
            (int32_t)player->respawn_count;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_DAMAGE] =
            (int32_t)player->damage_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITLAG] =
            (int32_t)player->hitlag_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITSTUN] =
            (int32_t)player->hitstun_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_ACTIVE] =
            (int32_t)player->hitbox_active;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_LEFT] =
            player->hitbox_left_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_RIGHT] =
            player->hitbox_right_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_TOP] =
            player->hitbox_top_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_HITBOX_BOTTOM] =
            player->hitbox_bottom_q16;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_LAST_HIT_SEQUENCE] =
            (int32_t)player->last_hit_sequence;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_TECH_WINDOW] =
            (int32_t)player->tech_window_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_TECH_LOCKOUT] =
            (int32_t)player->tech_lockout_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_TUMBLE] =
            (int32_t)player->tumble;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SDI_PULSE_COUNT] =
            (int32_t)player->sdi_pulse_count;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_TECH_DIRECTION] =
            (int32_t)player->tech_direction;
        pf_web_m4_view[
            PF_WEB_M4_VIEW_PRONE_ORIENTATION0 + (int)player_index] =
            (int32_t)player->prone_orientation;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_HEALTH] =
            (int32_t)player->shield_health_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_SHIELD_STUN] =
            (int32_t)player->shield_stun_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_POWERSHIELD] =
            (int32_t)player->powershield;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_INVULNERABLE] =
            (int32_t)player->invulnerable;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_ACTION_TICKS] =
            (int32_t)player->action_ticks;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_TRIGGER_INPUT_AGE] =
            (int32_t)player->trigger_input_age;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_L_CANCEL_ELIGIBLE] =
            (int32_t)player->l_cancel_eligible;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_STOCKS] =
            (int32_t)player->stocks_remaining;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_RESPAWN_TICKS] =
            (int32_t)player->respawn_ticks;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_RESPAWN_INVULNERABILITY] =
            (int32_t)player->respawn_invulnerability_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_ACTIVE] =
            (int32_t)player->grabbox_active;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_LEFT] =
            player->grabbox_left_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_RIGHT] =
            player->grabbox_right_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_TOP] =
            player->grabbox_top_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRABBOX_BOTTOM] =
            player->grabbox_bottom_q16;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_GRAB_ESCAPE_TICKS] =
            (int32_t)player->grab_escape_ticks;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRAB_TARGET] =
            (int32_t)player->grab_target;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_GRAB_OWNER] =
            (int32_t)player->grab_owner;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_PLAYER_CHARGE_TICKS] =
            (int32_t)player->charge_ticks;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SMASH_CHARGE_TICKS] =
            (int32_t)player->smash_charge_ticks;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_STRENGTH] =
            (int32_t)player->shield_strength;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_ACTIVE] =
            (int32_t)player->shield_active;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_LEFT] =
            player->shield_left_q16;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_RIGHT] =
            player->shield_right_q16;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_TOP] =
            player->shield_top_q16;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_BOTTOM] =
            player->shield_bottom_q16;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_TILT_X] =
            (int32_t)player->shield_tilt_x;
        pf_web_m4_view[
            base + PF_WEB_M4_VIEW_PLAYER_SHIELD_TILT_Y] =
            (int32_t)player->shield_tilt_y;
    }
    pf_web_m4_view[PF_WEB_M4_VIEW_EVENT_COUNT] =
        (int32_t)pf_web_m4_last_result.event_count;
    for (event_index = UINT32_C(0);
         event_index <
         (uint32_t)pf_web_m4_last_result.event_count;
         ++event_index)
    {
        const pf_sim_event *event =
            &pf_web_m4_last_result.events[event_index];
        const int base =
            PF_WEB_M4_VIEW_EVENT0 +
            (int)event_index * PF_WEB_M4_VIEW_EVENT_STRIDE;

        if (event->tick > (uint64_t)INT32_MAX ||
            event->sequence > (uint32_t)INT32_MAX ||
            event->value_q16 > (uint32_t)INT32_MAX)
        {
            return 0;
        }
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_SEQUENCE] =
            (int32_t)event->sequence;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_TICK] =
            (int32_t)event->tick;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_TYPE] =
            (int32_t)event->type;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_SOURCE] =
            (int32_t)event->source_player;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_TARGET] =
            (int32_t)event->target_player;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_VALUE] =
            (int32_t)event->value_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_VELOCITY_X] =
            event->velocity_x_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_VELOCITY_Y] =
            event->velocity_y_q16;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_FLAGS] =
            (int32_t)event->flags;
        pf_web_m4_view[base + PF_WEB_M4_VIEW_EVENT_DETAIL] =
            (int32_t)event->detail;
    }

    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_ENABLED] =
        (int32_t)inspection.item.enabled;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_STATE] =
        (int32_t)inspection.item.state;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HOLDER] =
        (int32_t)inspection.item.holder;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_SOURCE] =
        (int32_t)inspection.item.source;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_THROW_DIRECTION] =
        (int32_t)inspection.item.throw_direction;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HITBOX_ACTIVE] =
        (int32_t)inspection.item.hitbox_active;
    pf_web_m4_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_X] =
        inspection.item.position_x_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_Y] =
        inspection.item.position_y_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_VX] =
        inspection.item.velocity_x_q16;
    pf_web_m4_view[PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_VY] =
        inspection.item.velocity_y_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_LIFETIME] =
        (int32_t)inspection.item.lifetime_ticks;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_RESPAWN] =
        (int32_t)inspection.item.respawn_ticks;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_PICKUP_LOCKOUT] =
        (int32_t)inspection.item.pickup_lockout_ticks;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HIT_MASK] =
        (int32_t)inspection.item.hit_mask;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HALF_WIDTH] =
        pf_web_m4_content.item.half_width_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HALF_HEIGHT] =
        pf_web_m4_content.item.half_height_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HITBOX_HALF_WIDTH] =
        pf_web_m4_content.item.hitbox_half_width_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_ITEM0 + PF_WEB_M4_VIEW_ITEM_HITBOX_HALF_HEIGHT] =
        pf_web_m4_content.item.hitbox_half_height_q16;

    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_ENABLED] =
        (int32_t)inspection.projectile.enabled;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_STATE] =
        (int32_t)inspection.projectile.state;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_OWNER] =
        (int32_t)inspection.projectile.owner;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_HITBOX_ACTIVE] =
        (int32_t)inspection.projectile.hitbox_active;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_X] =
        inspection.projectile.position_x_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_Y] =
        inspection.projectile.position_y_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_VX] =
        inspection.projectile.velocity_x_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 + PF_WEB_M4_VIEW_PROJECTILE_VY] =
        inspection.projectile.velocity_y_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_LIFETIME] =
        (int32_t)inspection.projectile.lifetime_ticks;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_HALF_WIDTH] =
        pf_web_m4_content.projectile.half_width_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_HALF_HEIGHT] =
        pf_web_m4_content.projectile.half_height_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_PROJECTILE0 +
        PF_WEB_M4_VIEW_PROJECTILE_REFLECT_WINDOW] =
        (int32_t)pf_web_m4_content.projectile
            .powershield_reflect_window_ticks;
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)pf_web_m4_player_count;
         ++player_index)
    {
        const pf_m4_player_inspection *player =
            &inspection.players[player_index];
        const int revival_base =
            PF_WEB_M4_VIEW_REVIVAL0 +
            (int)player_index * PF_WEB_M4_VIEW_REVIVAL_STRIDE;
        const int stale_move_base =
            PF_WEB_M4_VIEW_STALE_MOVE0 +
            (int)player_index * PF_WEB_M4_VIEW_STALE_MOVE_STRIDE;
        uint32_t stale_slot;

        pf_web_m4_view[PF_WEB_M4_VIEW_RECOVERY0 + (int)player_index] =
            (int32_t)player->recovery_available;
        pf_web_m4_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_ACTIVE] =
            (int32_t)player->revival_platform_active;
        pf_web_m4_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_LEFT] =
            player->revival_platform_left_q16;
        pf_web_m4_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_RIGHT] =
            player->revival_platform_right_q16;
        pf_web_m4_view[
            revival_base + PF_WEB_M4_VIEW_REVIVAL_Y] =
            player->revival_platform_y_q16;
        pf_web_m4_view[
            stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_COUNT] =
            (int32_t)player->stale_move_count;
        pf_web_m4_view[
            stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_MULTIPLIER] =
            (int32_t)player->stale_move_multiplier_q16;
        pf_web_m4_view[
            stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_REGISTERED] =
            (int32_t)player->attack_stale_registered;
        for (stale_slot = UINT32_C(0);
             stale_slot <
                 (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
             ++stale_slot)
        {
            pf_web_m4_view[
                stale_move_base + PF_WEB_M4_VIEW_STALE_MOVE_IDS +
                (int)stale_slot] =
                (int32_t)player->stale_move_ids[stale_slot];
        }
    }
    pf_web_m4_view[PF_WEB_M4_VIEW_ITEM_STALE_REGISTERED] =
        (int32_t)inspection.item.stale_registered;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_UPPER_PLATFORM0 +
        PF_WEB_M4_VIEW_UPPER_PLATFORM_LEFT] =
        inspection.stage.upper_platform_left_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_UPPER_PLATFORM0 +
        PF_WEB_M4_VIEW_UPPER_PLATFORM_RIGHT] =
        inspection.stage.upper_platform_right_q16;
    pf_web_m4_view[
        PF_WEB_M4_VIEW_UPPER_PLATFORM0 +
        PF_WEB_M4_VIEW_UPPER_PLATFORM_Y] =
        inspection.stage.upper_platform_y_q16;

    pf_web_m4_playtest_render(
        pf_web_m4_view,
        PF_WEB_M4_VIEW_COUNT);
    return 1;
}

int pf_web_m4_playtest_start(void)
{
    int input_probe_passed;
    int air_facing_probe_passed;
    int instant_double_jump_probe_passed;
    int double_jump_cancel_probe_passed;
    int double_jump_cancel_counter_probe_passed;
    int bat_drop_probe_passed = 0;
    int glide_toss_probe_passed = 0;
    int jump_cancel_throw_probe_passed = 0;
    int jump_cancel_probe_passed;
    int item_probe_harness_passed;
    int edge_hop_probe_passed;
    int edge_dash_probe_passed;
    int fox_trot_probe_passed;
    int moonwalk_probe_passed;
    int teeter_cancel_probe_passed;
    int stage_humping_probe_passed;
    int taunt_cancel_probe_passed;
    int scar_jump_probe_passed;
    int team_wobble_probe_passed;
    int pivot_probe_passed;
    int dash_cancel_probe_passed;
    int dashing_shield_probe_passed;
    int shield_platform_drop_probe_passed;
    int small_step_forward_smash_probe_passed;
    int drop_cancel_probe_passed;
    int v_cancel_probe_passed;
    int approach_probe_passed;
    int spacing_probe_passed;
    int sharking_probe_passed;
    int cross_up_probe_passed;
    int mindgame_probe_passed;
    int juggling_probe_passed;
    int ladder_probe_passed;
    int kill_confirm_probe_passed;
    int zero_to_death_probe_passed;
    int ledge_cancel_probe_passed;
    int planking_probe_passed;
    int jump_cancelled_grab_probe_passed;
    int boost_grab_probe_passed;
    int jab_cancel_probe_passed;
    int jab_reset_probe_passed;
    int chain_grab_probe_passed;
    int combat_probe_passed;
    int reaction_probe_passed;
    int shield_probe_passed;
    int shield_break_probe_passed;
    int tumble_probe_passed;
    int floor_recovery_probe_passed;
    int tech_chase_probe_passed;
    int surface_tech_probe_passed;
    int air_dodge_probe_passed;
    int ground_dodge_probe_passed;
    int aerial_l_cancel_probe_passed;
    int match_probe_passed;
    int short_hop_laser_probe_passed;
    int camping_probe_passed;
    int shine_spike_probe_passed;
    int charge_storage_probe_passed;
    int vector_ascent_probe_passed;

    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        !pf_web_m4_initialize_current_content())
    {
        return 0;
    }

    input_probe_passed = pf_web_m4_run_input_probe();
    air_facing_probe_passed = pf_web_m4_run_air_facing_probe();
    instant_double_jump_probe_passed =
        pf_web_m4_run_instant_double_jump_probe();
    double_jump_cancel_probe_passed =
        pf_web_m4_run_double_jump_cancel_probe();
    double_jump_cancel_counter_probe_passed =
        pf_web_m4_run_double_jump_cancel_counter_probe();
    edge_hop_probe_passed = pf_web_m4_run_edge_hop_probe();
    edge_dash_probe_passed = pf_web_m4_run_edge_dash_probe();
    fox_trot_probe_passed = pf_web_m4_run_fox_trot_probe();
    moonwalk_probe_passed = pf_web_m4_run_moonwalk_probe();
    teeter_cancel_probe_passed =
        pf_web_m4_run_teeter_cancel_probe();
    stage_humping_probe_passed =
        pf_web_m4_run_stage_humping_probe();
    taunt_cancel_probe_passed =
        pf_web_m4_run_taunt_cancel_probe();
    scar_jump_probe_passed = pf_web_m4_run_scar_jump_probe();
    team_wobble_probe_passed = pf_web_m4_run_team_wobble_probe();
    pivot_probe_passed = pf_web_m4_run_pivot_probe();
    dash_cancel_probe_passed = pf_web_m4_run_dash_cancel_probe();
    dashing_shield_probe_passed =
        pf_web_m4_run_dashing_shield_probe();
    shield_platform_drop_probe_passed =
        pf_web_m4_run_shield_platform_drop_probe();
    small_step_forward_smash_probe_passed =
        pf_web_m4_run_small_step_forward_smash_probe();
    drop_cancel_probe_passed =
        pf_web_m4_run_drop_cancel_probe();
    v_cancel_probe_passed = pf_web_m4_run_v_cancel_probe();
    approach_probe_passed = pf_web_m4_run_approach_probe();
    spacing_probe_passed =
        pf_web_m4_run_spacing_probe(approach_probe_passed);
    sharking_probe_passed = pf_web_m4_run_sharking_probe();
    cross_up_probe_passed = pf_web_m4_run_cross_up_probe();
    mindgame_probe_passed =
        approach_probe_passed && spacing_probe_passed &&
        cross_up_probe_passed;
    juggling_probe_passed = pf_web_m4_run_juggling_probe();
    ladder_probe_passed = pf_web_m4_run_ladder_probe();
    kill_confirm_probe_passed =
        pf_web_m4_run_kill_confirm_probe();
    zero_to_death_probe_passed =
        pf_web_m4_run_zero_to_death_probe();
    ledge_cancel_probe_passed =
        pf_web_m4_run_ledge_cancel_probe();
    planking_probe_passed = pf_web_m4_run_planking_probe();
    jump_cancel_probe_passed = pf_web_m4_run_jump_cancel_probe();
    jump_cancelled_grab_probe_passed =
        pf_web_m4_run_jump_cancelled_grab_probe();
    boost_grab_probe_passed = pf_web_m4_run_boost_grab_probe();
    jab_cancel_probe_passed = pf_web_m4_run_jab_cancel_probe();
    jab_reset_probe_passed = pf_web_m4_run_jab_reset_probe();
    chain_grab_probe_passed = pf_web_m4_run_chain_grab_probe();
    combat_probe_passed = pf_web_m4_run_combat_probe();
    reaction_probe_passed = pf_web_m4_run_reaction_probe();
    shield_probe_passed = pf_web_m4_run_shield_probe();
    shield_break_probe_passed =
        pf_web_m4_run_shield_break_probe();
    tumble_probe_passed = pf_web_m4_run_tumble_probe();
    floor_recovery_probe_passed =
        pf_web_m4_run_floor_recovery_probe();
    tech_chase_probe_passed =
        pf_web_m4_run_tech_chase_probe();
    surface_tech_probe_passed =
        pf_web_m4_run_surface_tech_probe();
    air_dodge_probe_passed =
        pf_web_m4_run_air_dodge_probe();
    ground_dodge_probe_passed =
        pf_web_m4_run_ground_dodge_probe();
    aerial_l_cancel_probe_passed =
        pf_web_m4_run_aerial_l_cancel_probe();
    match_probe_passed = pf_web_m4_run_match_probe();
    item_probe_harness_passed = pf_web_m4_run_item_probes(
        &bat_drop_probe_passed,
        &glide_toss_probe_passed,
        &jump_cancel_throw_probe_passed);
    short_hop_laser_probe_passed =
        pf_web_m4_run_short_hop_laser_probe();
    camping_probe_passed = pf_web_m4_run_camping_probe();
    shine_spike_probe_passed = pf_web_m4_run_shine_spike_probe();
    charge_storage_probe_passed =
        pf_web_m4_run_charge_storage_probe();
    vector_ascent_probe_passed =
        pf_web_m4_run_vector_ascent_probe();
    if (input_probe_passed == 0 ||
        air_facing_probe_passed == 0 ||
        instant_double_jump_probe_passed == 0 ||
        double_jump_cancel_probe_passed == 0 ||
        double_jump_cancel_counter_probe_passed == 0 ||
        bat_drop_probe_passed == 0 ||
        glide_toss_probe_passed == 0 ||
        jump_cancel_throw_probe_passed == 0 ||
        jump_cancel_probe_passed == 0 ||
        item_probe_harness_passed == 0 ||
        edge_hop_probe_passed == 0 ||
        edge_dash_probe_passed == 0 ||
        fox_trot_probe_passed == 0 ||
        moonwalk_probe_passed == 0 ||
        teeter_cancel_probe_passed == 0 ||
        stage_humping_probe_passed == 0 ||
        taunt_cancel_probe_passed == 0 ||
        scar_jump_probe_passed == 0 ||
        team_wobble_probe_passed == 0 ||
        pivot_probe_passed == 0 ||
        dash_cancel_probe_passed == 0 ||
        dashing_shield_probe_passed == 0 ||
        shield_platform_drop_probe_passed == 0 ||
        small_step_forward_smash_probe_passed == 0 ||
        drop_cancel_probe_passed == 0 ||
        v_cancel_probe_passed == 0 ||
        approach_probe_passed == 0 ||
        spacing_probe_passed == 0 ||
        sharking_probe_passed == 0 ||
        cross_up_probe_passed == 0 ||
        mindgame_probe_passed == 0 ||
        juggling_probe_passed == 0 ||
        ladder_probe_passed == 0 ||
        kill_confirm_probe_passed == 0 ||
        zero_to_death_probe_passed == 0 ||
        ledge_cancel_probe_passed == 0 ||
        planking_probe_passed == 0 ||
        jump_cancelled_grab_probe_passed == 0 ||
        boost_grab_probe_passed == 0 ||
        jab_cancel_probe_passed == 0 ||
        jab_reset_probe_passed == 0 ||
        chain_grab_probe_passed == 0 ||
        combat_probe_passed == 0 ||
        reaction_probe_passed == 0 ||
        shield_probe_passed == 0 ||
        shield_break_probe_passed == 0 ||
        tumble_probe_passed == 0 ||
        floor_recovery_probe_passed == 0 ||
        tech_chase_probe_passed == 0 ||
        surface_tech_probe_passed == 0 ||
        air_dodge_probe_passed == 0 ||
        ground_dodge_probe_passed == 0 ||
        aerial_l_cancel_probe_passed == 0 ||
        match_probe_passed == 0 ||
        short_hop_laser_probe_passed == 0 ||
        camping_probe_passed == 0 ||
        shine_spike_probe_passed == 0 ||
        charge_storage_probe_passed == 0 ||
        vector_ascent_probe_passed == 0 ||
        !pf_web_m4_initialize_live_item_lab())
    {
#if !defined(__EMSCRIPTEN__)
#define PF_WEB_M4_REPORT_FAILED_PROBE(probe)                             \
    do                                                                  \
    {                                                                   \
        if ((probe) == 0)                                               \
        {                                                               \
            (void)fprintf(                                              \
                stderr,                                                 \
                "m4-browser-probe=fail name=%s\n",                     \
                #probe);                                                \
        }                                                               \
    } while (0)
        PF_WEB_M4_REPORT_FAILED_PROBE(input_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(air_facing_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(instant_double_jump_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(double_jump_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(
            double_jump_cancel_counter_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(bat_drop_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(glide_toss_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(jump_cancel_throw_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(jump_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(item_probe_harness_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(edge_hop_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(edge_dash_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(fox_trot_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(moonwalk_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(teeter_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(stage_humping_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(taunt_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(scar_jump_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(team_wobble_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(pivot_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(dash_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(dashing_shield_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(
            shield_platform_drop_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(
            small_step_forward_smash_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(drop_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(v_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(approach_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(spacing_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(sharking_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(cross_up_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(mindgame_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(juggling_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(ladder_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(kill_confirm_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(zero_to_death_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(ledge_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(planking_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(jump_cancelled_grab_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(boost_grab_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(jab_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(jab_reset_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(chain_grab_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(combat_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(reaction_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(shield_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(shield_break_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(tumble_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(floor_recovery_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(tech_chase_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(surface_tech_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(air_dodge_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(ground_dodge_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(aerial_l_cancel_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(match_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(short_hop_laser_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(camping_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(shine_spike_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(charge_storage_probe_passed);
        PF_WEB_M4_REPORT_FAILED_PROBE(vector_ascent_probe_passed);
#undef PF_WEB_M4_REPORT_FAILED_PROBE
#endif
        return 0;
    }
    pf_web_m4_playtest_install(
        (int)PF_WEB_M4_WALK_AXIS,
        (int)PF_WEB_M4_DASH_AXIS,
        input_probe_passed,
        air_facing_probe_passed,
        instant_double_jump_probe_passed,
        double_jump_cancel_probe_passed,
        double_jump_cancel_counter_probe_passed,
        bat_drop_probe_passed,
        glide_toss_probe_passed,
        jump_cancel_throw_probe_passed,
        jump_cancel_probe_passed,
        edge_hop_probe_passed,
        edge_dash_probe_passed,
        fox_trot_probe_passed,
        moonwalk_probe_passed,
        teeter_cancel_probe_passed,
        stage_humping_probe_passed,
        taunt_cancel_probe_passed,
        scar_jump_probe_passed,
        team_wobble_probe_passed,
        pivot_probe_passed,
        dash_cancel_probe_passed,
        dashing_shield_probe_passed,
        shield_platform_drop_probe_passed,
        small_step_forward_smash_probe_passed,
        drop_cancel_probe_passed,
        v_cancel_probe_passed,
        approach_probe_passed,
        spacing_probe_passed,
        sharking_probe_passed,
        cross_up_probe_passed,
        mindgame_probe_passed,
        juggling_probe_passed,
        ladder_probe_passed,
        kill_confirm_probe_passed,
        zero_to_death_probe_passed,
        ledge_cancel_probe_passed,
        planking_probe_passed,
        jump_cancelled_grab_probe_passed,
        boost_grab_probe_passed,
        jab_cancel_probe_passed,
        jab_reset_probe_passed,
        chain_grab_probe_passed,
        combat_probe_passed,
        reaction_probe_passed,
        shield_probe_passed,
        shield_break_probe_passed,
        tumble_probe_passed,
        floor_recovery_probe_passed,
        tech_chase_probe_passed,
        surface_tech_probe_passed,
        air_dodge_probe_passed,
        ground_dodge_probe_passed,
        aerial_l_cancel_probe_passed,
        match_probe_passed,
        short_hop_laser_probe_passed,
        camping_probe_passed,
        shine_spike_probe_passed,
        charge_storage_probe_passed,
        vector_ascent_probe_passed,
        (int)pf_web_m4_content.fighter.aerial_landing_lag_ticks,
        (int)pf_web_m4_content.fighter
            .strong_aerial_landing_lag_ticks);
    return pf_web_m4_render();
}

int pf_web_m4_playtest_step_dual_trigger_special(
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
    pf_m4_inspection inspection;
    uint64_t player0_buttons = UINT64_C(0);
    uint64_t player1_buttons = UINT64_C(0);

    if (pf_web_m4_sim != NULL &&
        pf_m4_inspect(pf_web_m4_sim, &inspection) == PF_STATUS_OK &&
        (inspection.terminated != UINT8_C(0) ||
         inspection.truncated != UINT8_C(0)))
    {
        return pf_web_m4_render();
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
    if (!pf_web_m4_tick_with_dual_triggers(
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
    return pf_web_m4_render();
}

int pf_web_m4_playtest_step_special(
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
    return pf_web_m4_playtest_step_dual_trigger_special(
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

int pf_web_m4_playtest_step(
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
    return pf_web_m4_playtest_step_special(
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

int pf_web_m4_playtest_reset(void)
{
    return pf_web_m4_reset_internal() && pf_web_m4_render();
}

int pf_web_m4_playtest_refresh(void)
{
    return pf_web_m4_render();
}

int pf_web_m4_playtest_configure_duel(int stock_count)
{
    const uint8_t previous_stock_count = pf_web_m4_stock_count;

    if (stock_count < 1 ||
        stock_count > (int)PF_WEB_M4_MAX_SETUP_STOCKS)
    {
        return 0;
    }
    pf_web_m4_stock_count = (uint8_t)stock_count;
    if (!pf_web_m4_initialize_live_item_lab())
    {
        pf_web_m4_stock_count = previous_stock_count;
        (void)pf_web_m4_initialize_live_item_lab();
        return 0;
    }
    return pf_web_m4_render();
}

int pf_web_m4_playtest_set_team_lab(int enabled)
{
    if (enabled != 0 && enabled != 1)
    {
        return 0;
    }
    if (enabled != 0)
    {
        return pf_web_m4_initialize_team_wobble_lab() &&
               pf_web_m4_render();
    }
    return pf_web_m4_initialize_live_item_lab() &&
           pf_web_m4_render();
}
