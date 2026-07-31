#include "m4_playtest.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_VIEW_COUNT 256
#define TEST_PLAYER0_BASE 25
#define TEST_SOLID_LEFT 14
#define TEST_SOLID_RIGHT 15
#define TEST_SOLID_TOP 16
#define TEST_SOLID_BOTTOM 17
#define TEST_PLAYER_ACTION 4
#define TEST_PLAYER_VX 2
#define TEST_PLAYER_FACING 5
#define TEST_PLAYER_GROUNDED 6
#define TEST_PLAYER_HITBOX_ACTIVE 14
#define TEST_PLAYER_TECH_WINDOW 20
#define TEST_PLAYER_TECH_LOCKOUT 21
#define TEST_PLAYER_SHIELD_HEALTH 25
#define TEST_PLAYER_SHIELD_STUN 26
#define TEST_PLAYER_POWERSHIELD 27
#define TEST_PLAYER_ACTION_TICKS 29
#define TEST_PLAYER_TRIGGER_INPUT_AGE 30
#define TEST_PLAYER_L_CANCEL_ELIGIBLE 31
#define TEST_PLAYER_STOCKS 32
#define TEST_STOCK_COUNT 18
#define TEST_RESPAWN_DELAY 19
#define TEST_RESPAWN_INVULNERABILITY 20
#define TEST_EVENT_COUNT 95
#define TEST_EVENT0 96
#define TEST_EVENT_SEQUENCE 0
#define TEST_EVENT_TICK 1
#define TEST_EVENT_TYPE 2
#define TEST_EVENT_SOURCE 3
#define TEST_EVENT_TARGET 4
#define TEST_EVENT_VALUE 5

static int test_install_count;
static int test_render_count;
static int test_walk_axis;
static int test_dash_axis;
static int test_input_probe;
static int test_air_facing_probe;
static int test_instant_double_jump_probe;
static int test_edge_hop_probe;
static int test_edge_dash_probe;
static int test_fox_trot_probe;
static int test_pivot_probe;
static int test_dash_cancel_probe;
static int test_dashing_shield_probe;
static int test_shield_platform_drop_probe;
static int test_small_step_forward_smash_probe;
static int test_drop_cancel_probe;
static int test_v_cancel_probe;
static int test_approach_probe;
static int test_spacing_probe;
static int test_sharking_probe;
static int test_cross_up_probe;
static int test_mindgame_probe;
static int test_combat_probe;
static int test_reaction_probe;
static int test_shield_probe;
static int test_shield_break_probe;
static int test_tumble_probe;
static int test_floor_recovery_probe;
static int test_tech_chase_probe;
static int test_surface_tech_probe;
static int test_air_dodge_probe;
static int test_ground_dodge_probe;
static int test_aerial_l_cancel_probe;
static int test_match_probe;
static int test_aerial_landing_lag_ticks;
static int test_strong_aerial_landing_lag_ticks;
static int32_t test_view[TEST_VIEW_COUNT];

void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int input_probe_passed,
    int air_facing_probe_passed,
    int instant_double_jump_probe_passed,
    int edge_hop_probe_passed,
    int edge_dash_probe_passed,
    int fox_trot_probe_passed,
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
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks);

void pf_web_m4_playtest_render(
    const int32_t *view,
    int view_count);

void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int input_probe_passed,
    int air_facing_probe_passed,
    int instant_double_jump_probe_passed,
    int edge_hop_probe_passed,
    int edge_dash_probe_passed,
    int fox_trot_probe_passed,
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
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks)
{
    ++test_install_count;
    test_walk_axis = walk_axis;
    test_dash_axis = dash_axis;
    test_input_probe = input_probe_passed;
    test_air_facing_probe = air_facing_probe_passed;
    test_instant_double_jump_probe =
        instant_double_jump_probe_passed;
    test_edge_hop_probe = edge_hop_probe_passed;
    test_edge_dash_probe = edge_dash_probe_passed;
    test_fox_trot_probe = fox_trot_probe_passed;
    test_pivot_probe = pivot_probe_passed;
    test_dash_cancel_probe = dash_cancel_probe_passed;
    test_dashing_shield_probe = dashing_shield_probe_passed;
    test_shield_platform_drop_probe =
        shield_platform_drop_probe_passed;
    test_small_step_forward_smash_probe =
        small_step_forward_smash_probe_passed;
    test_drop_cancel_probe = drop_cancel_probe_passed;
    test_v_cancel_probe = v_cancel_probe_passed;
    test_approach_probe = approach_probe_passed;
    test_spacing_probe = spacing_probe_passed;
    test_sharking_probe = sharking_probe_passed;
    test_cross_up_probe = cross_up_probe_passed;
    test_mindgame_probe = mindgame_probe_passed;
    test_combat_probe = combat_probe_passed;
    test_reaction_probe = reaction_probe_passed;
    test_shield_probe = shield_probe_passed;
    test_shield_break_probe = shield_break_probe_passed;
    test_tumble_probe = tumble_probe_passed;
    test_floor_recovery_probe = floor_recovery_probe_passed;
    test_tech_chase_probe = tech_chase_probe_passed;
    test_surface_tech_probe = surface_tech_probe_passed;
    test_air_dodge_probe = air_dodge_probe_passed;
    test_ground_dodge_probe = ground_dodge_probe_passed;
    test_aerial_l_cancel_probe = aerial_l_cancel_probe_passed;
    test_match_probe = match_probe_passed;
    test_aerial_landing_lag_ticks = aerial_landing_lag_ticks;
    test_strong_aerial_landing_lag_ticks =
        strong_aerial_landing_lag_ticks;
}

void pf_web_m4_playtest_render(
    const int32_t *view,
    int view_count)
{
    ++test_render_count;
    if (view == NULL || view_count != TEST_VIEW_COUNT)
    {
        (void)memset(test_view, 0xff, sizeof(test_view));
        return;
    }
    (void)memcpy(test_view, view, sizeof(test_view));
}

static int fail(const char *operation)
{
    (void)fprintf(
        stderr,
        "m4-browser-adapter=fail operation=%s\n",
        operation);
    return 1;
}

static int test_step(
    int player0_x,
    int player0_y,
    int player0_jump,
    int player0_attack,
    int player0_shield,
    int player1_x,
    int player1_y,
    int player1_jump,
    int player1_attack,
    int player1_shield)
{
    return pf_web_m4_playtest_step(
        player0_x,
        player0_y,
        player0_jump,
        player0_attack,
        0,
        player0_shield,
        player1_x,
        player1_y,
        player1_jump,
        player1_attack,
        0,
        player1_shield);
}

static int test_player0_strong_step(void)
{
    return pf_web_m4_playtest_step(
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0);
}

#define pf_web_m4_playtest_step test_step

static int test_player0_reach_run(void)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(10); ++tick)
    {
        if (!pf_web_m4_playtest_step(
                test_dash_axis,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0))
        {
            return 0;
        }
    }
    return test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] == 3;
}

int main(void)
{
    if (!pf_web_m4_playtest_start() ||
        test_install_count != 1 ||
        test_render_count != 1 ||
        test_walk_axis != 13500 ||
        test_dash_axis != 32767 ||
        test_input_probe != 1 ||
        test_air_facing_probe != 1 ||
        test_instant_double_jump_probe != 1 ||
        test_edge_hop_probe != 1 ||
        test_edge_dash_probe != 1 ||
        test_fox_trot_probe != 1 ||
        test_pivot_probe != 1 ||
        test_dash_cancel_probe != 1 ||
        test_dashing_shield_probe != 1 ||
        test_shield_platform_drop_probe != 1 ||
        test_small_step_forward_smash_probe != 1 ||
        test_drop_cancel_probe != 1 ||
        test_v_cancel_probe != 1 ||
        test_approach_probe != 1 ||
        test_spacing_probe != 1 ||
        test_sharking_probe != 1 ||
        test_cross_up_probe != 1 ||
        test_mindgame_probe != 1 ||
        test_combat_probe != 1 ||
        test_reaction_probe != 1 ||
        test_shield_probe != 1 ||
        test_shield_break_probe != 1 ||
        test_tumble_probe != 1 ||
        test_floor_recovery_probe != 1 ||
        test_tech_chase_probe != 1 ||
        test_surface_tech_probe != 1 ||
        test_air_dodge_probe != 1 ||
        test_ground_dodge_probe != 1 ||
        test_aerial_l_cancel_probe != 1 ||
        test_match_probe != 1 ||
        test_aerial_landing_lag_ticks != 12 ||
        test_strong_aerial_landing_lag_ticks != 30 ||
        test_view[0] != 14 ||
        test_view[1] != 0 ||
        test_view[TEST_STOCK_COUNT] != 4 ||
        test_view[TEST_RESPAWN_DELAY] != 60 ||
        test_view[TEST_RESPAWN_INVULNERABILITY] != 120 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_EVENT_COUNT] != 0 ||
        test_view[TEST_SOLID_LEFT] != 14 * 65536 ||
        test_view[TEST_SOLID_RIGHT] != 27 * 65536 ||
        test_view[TEST_SOLID_TOP] != 16 * 65536 ||
        test_view[TEST_SOLID_BOTTOM] != 29 * 65536)
    {
        (void)fprintf(
            stderr,
            "m4-browser-adapter=debug installs=%d renders=%d walk=%d "
            "dash=%d input_probe=%d air_facing_probe=%d "
            "instant_double_jump_probe=%d edge_hop_probe=%d "
            "edge_dash_probe=%d fox_trot_probe=%d pivot_probe=%d "
            "dash_cancel_probe=%d dashing_shield_probe=%d "
            "shield_platform_drop_probe=%d "
            "small_step_forward_smash_probe=%d "
            "drop_cancel_probe=%d "
            "v_cancel_probe=%d "
            "approach_probe=%d "
            "spacing_probe=%d "
            "sharking_probe=%d "
            "cross_up_probe=%d "
            "mindgame_probe=%d "
            "combat_probe=%d "
            "reaction_probe=%d shield_probe=%d shield_break_probe=%d "
            "tumble_probe=%d "
            "floor_recovery_probe=%d tech_chase_probe=%d "
            "surface_tech_probe=%d "
            "air_dodge_probe=%d ground_dodge_probe=%d "
            "aerial_l_cancel_probe=%d match_probe=%d "
            "aerial_lag=%d strong_aerial_lag=%d "
            "schema=%d tick=%d\n",
            test_install_count,
            test_render_count,
            test_walk_axis,
            test_dash_axis,
            test_input_probe,
            test_air_facing_probe,
            test_instant_double_jump_probe,
            test_edge_hop_probe,
            test_edge_dash_probe,
            test_fox_trot_probe,
            test_pivot_probe,
            test_dash_cancel_probe,
            test_dashing_shield_probe,
            test_shield_platform_drop_probe,
            test_small_step_forward_smash_probe,
            test_drop_cancel_probe,
            test_v_cancel_probe,
            test_approach_probe,
            test_spacing_probe,
            test_sharking_probe,
            test_cross_up_probe,
            test_mindgame_probe,
            test_combat_probe,
            test_reaction_probe,
            test_shield_probe,
            test_shield_break_probe,
            test_tumble_probe,
            test_floor_recovery_probe,
            test_tech_chase_probe,
            test_surface_tech_probe,
            test_air_dodge_probe,
            test_ground_dodge_probe,
            test_aerial_l_cancel_probe,
            test_match_probe,
            test_aerial_landing_lag_ticks,
            test_strong_aerial_landing_lag_ticks,
            (int)test_view[0],
            (int)test_view[1]);
        return fail("start-and-input-probe");
    }

    if (!pf_web_m4_playtest_step(
            test_walk_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 1)
    {
        return fail("keyboard-walk-magnitude");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 2 ||
        !pf_web_m4_playtest_step(
            -test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 2 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != -1)
    {
        return fail("keyboard-dash-dance");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 2 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 2 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS] != 1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != 1)
    {
        return fail("keyboard-fox-trot");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        !pf_web_m4_playtest_step(
            -test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 2 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != -1 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 12 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != -1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] >= 0)
    {
        return fail("keyboard-pivot-attack");
    }

    if (!pf_web_m4_playtest_reset() ||
        !test_player0_reach_run() ||
        !pf_web_m4_playtest_step(
            0,
            test_dash_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 4 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0 ||
        !pf_web_m4_playtest_step(
            0, test_dash_axis, 0, 1, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 12 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0)
    {
        return fail("keyboard-dash-cancel-crouch-attack");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            test_dash_axis,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 38 ||
        !pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0,
            test_dash_axis,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 40)
    {
        return fail("keyboard-ground-roll-and-spot-dodge");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 5 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 6 ||
        !pf_web_m4_playtest_step(
            test_dash_axis,
            test_dash_axis,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 34 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GROUNDED] != 1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS] != 0)
    {
        return fail("keyboard-short-hop-air-dodge-wavedash");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 12 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[
            TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE] != 1)
    {
        return fail("keyboard-attack-and-hitbox-view");
    }

    if (!pf_web_m4_playtest_reset())
    {
        return fail("event-journal-reset");
    }
    {
        int approach_tick;

        for (approach_tick = 0; approach_tick < 27; ++approach_tick)
        {
            if (!pf_web_m4_playtest_step(
                    test_dash_axis,
                    0,
                    0,
                    0,
                    0,
                    -test_dash_axis,
                    0,
                    0,
                    0,
                    0))
            {
                return fail("event-journal-approach");
            }
        }
    }
    if (!pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_EVENT_COUNT] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_SEQUENCE] <= 0 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TICK] != test_view[1] - 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_SOURCE] != 0 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TARGET] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_VALUE] <= 0)
    {
        return fail("event-journal-hit-view");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 6 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 35 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 1, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 35 ||
        test_view[
            TEST_PLAYER0_BASE + TEST_PLAYER_TRIGGER_INPUT_AGE] != 0 ||
        test_view[
            TEST_PLAYER0_BASE + TEST_PLAYER_L_CANCEL_ELIGIBLE] != 1)
    {
        return fail("keyboard-aerial-and-l-cancel-trigger");
    }

    if (!pf_web_m4_playtest_reset() ||
        !test_player0_strong_step() ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 22 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[
            TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE] != 1)
    {
        return fail("keyboard-strong-attack-and-hitbox-view");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 1, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_TECH_WINDOW] != 20 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_TECH_LOCKOUT] != 40 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 18 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_HEALTH] >=
            60 * 65536 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STUN] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_POWERSHIELD] != 0 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 1, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_TECH_WINDOW] != 19 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_TECH_LOCKOUT] != 39)
    {
        return fail("keyboard-tech-trigger-edge");
    }

    (void)printf(
        "m4-browser-adapter=pass walk_axis=%d dash_axis=%d "
        "input_probe=%d air_facing_probe=%d "
        "instant_double_jump_probe=%d edge_hop_probe=%d "
        "edge_dash_probe=%d fox_trot_probe=%d pivot_probe=%d "
        "dash_cancel_probe=%d dashing_shield_probe=%d "
        "shield_platform_drop_probe=%d "
        "small_step_forward_smash_probe=%d "
        "drop_cancel_probe=%d "
        "v_cancel_probe=%d "
        "approach_probe=%d "
        "spacing_probe=%d "
        "sharking_probe=%d "
        "cross_up_probe=%d "
        "mindgame_probe=%d "
        "combat_probe=%d reaction_probe=%d "
        "shield_probe=%d shield_break_probe=%d "
        "powershield_cancel_probe=%d tumble_probe=%d "
        "floor_recovery_probe=%d tech_chase_probe=%d "
        "surface_tech_probe=%d "
        "air_dodge_probe=%d ground_dodge_probe=%d "
        "aerial_l_cancel_probe=%d match_probe=%d "
        "event_journal_probe=%d renders=%d\n",
        test_walk_axis,
        test_dash_axis,
        test_input_probe,
        test_air_facing_probe,
        test_instant_double_jump_probe,
        test_edge_hop_probe,
        test_edge_dash_probe,
        test_fox_trot_probe,
        test_pivot_probe,
        test_dash_cancel_probe,
        test_dashing_shield_probe,
        test_shield_platform_drop_probe,
        test_small_step_forward_smash_probe,
        test_drop_cancel_probe,
        test_v_cancel_probe,
        test_approach_probe,
        test_spacing_probe,
        test_sharking_probe,
        test_cross_up_probe,
        test_mindgame_probe,
        test_combat_probe,
        test_reaction_probe,
        test_shield_probe,
        test_shield_break_probe,
        test_shield_probe,
        test_tumble_probe,
        test_floor_recovery_probe,
        test_tech_chase_probe,
        test_surface_tech_probe,
        test_air_dodge_probe,
        test_ground_dodge_probe,
        test_aerial_l_cancel_probe,
        test_match_probe,
        test_combat_probe,
        test_render_count);
    return 0;
}
