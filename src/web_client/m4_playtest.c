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
#define PF_WEB_M4_VIEW_PLAYER_STRIDE 11
#define PF_WEB_M4_VIEW_PLAYER0 14
#define PF_WEB_M4_VIEW_COUNT 36

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
    PF_WEB_M4_VIEW_PLAYER_RESPAWNS = 10
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
    int input_probe_passed);

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
    int16_t player1_x,
    int16_t player1_y,
    uint64_t player1_buttons)
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
    inputs[1].main_stick_x = player1_x;
    inputs[1].main_stick_y = player1_y;
    inputs[1].buttons = player1_buttons;
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
        player1_x,
        player1_y,
        player1_buttons);
    return pf_sim_tick(
               pf_web_m4_sim,
               inputs,
               (size_t)PF_WEB_M4_PLAYER_COUNT,
               &result) == PF_STATUS_OK &&
           pf_m4_inspect(pf_web_m4_sim, out_inspection) ==
               PF_STATUS_OK;
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
    pf_web_m4_view[PF_WEB_M4_VIEW_SCHEMA] = INT32_C(1);
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
    if (input_probe_passed == 0 ||
        !pf_web_m4_reset_internal())
    {
        return 0;
    }
    pf_web_m4_playtest_install(
        (int)PF_WEB_M4_WALK_AXIS,
        (int)PF_WEB_M4_DASH_AXIS,
        input_probe_passed);
    return pf_web_m4_render();
}

int pf_web_m4_playtest_step(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player1_x,
    int player1_y,
    int player1_jump)
{
    pf_m4_inspection inspection;

    if (player0_x < INT16_MIN || player0_x > INT16_MAX ||
        player0_y < INT16_MIN || player0_y > INT16_MAX ||
        player1_x < INT16_MIN || player1_x > INT16_MAX ||
        player1_y < INT16_MIN || player1_y > INT16_MAX ||
        (player0_jump != 0 && player0_jump != 1) ||
        (player1_jump != 0 && player1_jump != 1) ||
        !pf_web_m4_tick(
            (int16_t)player0_x,
            (int16_t)player0_y,
            player0_jump != 0
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0),
            (int16_t)player1_x,
            (int16_t)player1_y,
            player1_jump != 0
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0),
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
