#include "m4_playtest.h"

#include "pf/m4.h"
#include "pf/sim.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

#define PF_WEB_M4_MEMORY_BYTES 4096U
#define PF_WEB_M4_MEMORY_ALIGNMENT 64U
#define PF_WEB_M4_PLAYER_COUNT UINT8_C(2)
#define PF_WEB_M4_WALK_AXIS INT16_C(13500)
#define PF_WEB_M4_DASH_AXIS INT16_C(32767)
#define PF_WEB_M4_MAX_TICKS UINT64_C(1728000)
#define PF_WEB_M4_RESET_SEED UINT64_C(0x4d34504c41595445)
#define PF_WEB_M4_VIEW_PLAYER_STRIDE 32
#define PF_WEB_M4_VIEW_PLAYER0 18
#define PF_WEB_M4_VIEW_COUNT 82

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
    PF_WEB_M4_VIEW_PLAYER_L_CANCEL_ELIGIBLE = 31
};

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
    int combat_probe_passed,
    int reaction_probe_passed,
    int shield_probe_passed,
    int tumble_probe_passed,
    int floor_recovery_probe_passed,
    int surface_tech_probe_passed,
    int air_dodge_probe_passed,
    int ground_dodge_probe_passed,
    int aerial_l_cancel_probe_passed,
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks);

extern void pf_web_m4_playtest_render(
    const int32_t *view,
    int view_count);

static pf_web_m4_storage pf_web_m4_sim_storage;
static pf_m4_content pf_web_m4_content;
static pf_sim *pf_web_m4_sim;
static int32_t pf_web_m4_view[PF_WEB_M4_VIEW_COUNT];

static void pf_web_m4_make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick,
    int16_t player0_x,
    int16_t player0_y,
    uint64_t player0_buttons,
    uint16_t player0_trigger,
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons,
    uint16_t player1_trigger)
{
    uint32_t player_index;

    (void)memset(
        inputs,
        0,
        sizeof(*inputs) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)PF_WEB_M4_PLAYER_COUNT;
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }
    inputs[0].main_stick_x = player0_x;
    inputs[0].main_stick_y = player0_y;
    inputs[0].buttons = player0_buttons;
    inputs[0].left_trigger = player0_trigger;
    inputs[1].main_stick_x = player1_x;
    inputs[1].main_stick_y = player1_y;
    inputs[1].buttons = player1_buttons;
    inputs[1].left_trigger = player1_trigger;
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
        before.tick,
        player0_x,
        player0_y,
        player0_buttons,
        player0_trigger,
        player1_x,
        player1_y,
        player1_buttons,
        player1_trigger);
    return pf_sim_tick(
               pf_web_m4_sim,
               inputs,
               (size_t)PF_WEB_M4_PLAYER_COUNT,
               &result) == PF_STATUS_OK &&
           pf_m4_inspect(pf_web_m4_sim, out_inspection) ==
               PF_STATUS_OK;
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
    return pf_web_m4_sim != NULL &&
           pf_sim_reset(
               pf_web_m4_sim,
               PF_WEB_M4_RESET_SEED) == PF_STATUS_OK;
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
        inspection.players[0].facing != facing)
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
    return inspection.players[0].invulnerable == UINT8_C(1) &&
           inspection.players[0].facing == facing;
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
        inspection.players[0].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[0].facing != INT8_C(1))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
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
    return inspection.players[0].velocity_x_q16 > INT32_C(0);
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
           inspection.players[1].last_hit_attacker == UINT8_C(0);
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
    return 1;
}

static int pf_web_m4_run_shield_probe(void)
{
    pf_m4_inspection inspection;
    uint32_t tick;

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
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    return inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
           inspection.players[1].powershield == UINT8_C(0);
}

static int pf_web_m4_render(void)
{
    pf_m4_inspection inspection;
    uint32_t player_index;

    if (pf_m4_inspect(pf_web_m4_sim, &inspection) != PF_STATUS_OK ||
        inspection.tick > (uint64_t)INT32_MAX)
    {
        return 0;
    }

    (void)memset(pf_web_m4_view, 0, sizeof(pf_web_m4_view));
    pf_web_m4_view[PF_WEB_M4_VIEW_SCHEMA] = INT32_C(11);
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

    for (player_index = UINT32_C(0);
         player_index < (uint32_t)PF_WEB_M4_PLAYER_COUNT;
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
    }

    pf_web_m4_playtest_render(
        pf_web_m4_view,
        PF_WEB_M4_VIEW_COUNT);
    return 1;
}

int pf_web_m4_playtest_start(void)
{
    pf_content_view content_view;
    pf_memory_requirements requirements;
    pf_sim_config config;
    int input_probe_passed;
    int air_facing_probe_passed;
    int combat_probe_passed;
    int reaction_probe_passed;
    int shield_probe_passed;
    int tumble_probe_passed;
    int floor_recovery_probe_passed;
    int surface_tech_probe_passed;
    int air_dodge_probe_passed;
    int ground_dodge_probe_passed;
    int aerial_l_cancel_probe_passed;

    if (pf_m4_default_content(&pf_web_m4_content) != PF_STATUS_OK ||
        pf_m4_make_content_view(
            &pf_web_m4_content,
            &content_view) != PF_STATUS_OK ||
        pf_sim_default_config(
            &config,
            PF_WEB_M4_PLAYER_COUNT,
            PF_SIM_MODE_DUEL) != PF_STATUS_OK)
    {
        return 0;
    }
    config.max_ticks = PF_WEB_M4_MAX_TICKS;
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

    input_probe_passed = pf_web_m4_run_input_probe();
    air_facing_probe_passed = pf_web_m4_run_air_facing_probe();
    combat_probe_passed = pf_web_m4_run_combat_probe();
    reaction_probe_passed = pf_web_m4_run_reaction_probe();
    shield_probe_passed = pf_web_m4_run_shield_probe();
    tumble_probe_passed = pf_web_m4_run_tumble_probe();
    floor_recovery_probe_passed =
        pf_web_m4_run_floor_recovery_probe();
    surface_tech_probe_passed =
        pf_web_m4_run_surface_tech_probe();
    air_dodge_probe_passed =
        pf_web_m4_run_air_dodge_probe();
    ground_dodge_probe_passed =
        pf_web_m4_run_ground_dodge_probe();
    aerial_l_cancel_probe_passed =
        pf_web_m4_run_aerial_l_cancel_probe();
    if (input_probe_passed == 0 ||
        air_facing_probe_passed == 0 ||
        combat_probe_passed == 0 ||
        reaction_probe_passed == 0 ||
        shield_probe_passed == 0 ||
        tumble_probe_passed == 0 ||
        floor_recovery_probe_passed == 0 ||
        surface_tech_probe_passed == 0 ||
        air_dodge_probe_passed == 0 ||
        ground_dodge_probe_passed == 0 ||
        aerial_l_cancel_probe_passed == 0 ||
        !pf_web_m4_reset_internal())
    {
        return 0;
    }
    pf_web_m4_playtest_install(
        (int)PF_WEB_M4_WALK_AXIS,
        (int)PF_WEB_M4_DASH_AXIS,
        input_probe_passed,
        air_facing_probe_passed,
        combat_probe_passed,
        reaction_probe_passed,
        shield_probe_passed,
        tumble_probe_passed,
        floor_recovery_probe_passed,
        surface_tech_probe_passed,
        air_dodge_probe_passed,
        ground_dodge_probe_passed,
        aerial_l_cancel_probe_passed,
        (int)pf_web_m4_content.fighter.aerial_landing_lag_ticks,
        (int)pf_web_m4_content.fighter
            .strong_aerial_landing_lag_ticks);
    return pf_web_m4_render();
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
    pf_m4_inspection inspection;
    uint64_t player0_buttons = UINT64_C(0);
    uint64_t player1_buttons = UINT64_C(0);

    if (player0_x < INT16_MIN || player0_x > INT16_MAX ||
        player0_y < INT16_MIN || player0_y > INT16_MAX ||
        player1_x < INT16_MIN || player1_x > INT16_MAX ||
        player1_y < INT16_MIN || player1_y > INT16_MAX ||
        (player0_jump != 0 && player0_jump != 1) ||
        (player0_attack != 0 && player0_attack != 1) ||
        (player0_strong_attack != 0 &&
         player0_strong_attack != 1) ||
        (player0_shield != 0 && player0_shield != 1) ||
        (player1_jump != 0 && player1_jump != 1) ||
        (player1_attack != 0 && player1_attack != 1) ||
        (player1_strong_attack != 0 &&
         player1_strong_attack != 1) ||
        (player1_shield != 0 && player1_shield != 1))
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
    if (!pf_web_m4_tick_with_triggers(
            (int16_t)player0_x,
            (int16_t)player0_y,
            player0_buttons,
            player0_shield != 0 ? UINT16_MAX : UINT16_C(0),
            (int16_t)player1_x,
            (int16_t)player1_y,
            player1_buttons,
            player1_shield != 0 ? UINT16_MAX : UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    return pf_web_m4_render();
}

int pf_web_m4_playtest_reset(void)
{
    return pf_web_m4_reset_internal() && pf_web_m4_render();
}
