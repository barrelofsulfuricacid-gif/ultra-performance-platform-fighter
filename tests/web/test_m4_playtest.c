#include "m4_playtest.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_VIEW_COUNT 72
#define TEST_PLAYER0_BASE 14
#define TEST_PLAYER_ACTION 4
#define TEST_PLAYER_FACING 5
#define TEST_PLAYER_HITBOX_ACTIVE 14
#define TEST_PLAYER_TECH_WINDOW 20
#define TEST_PLAYER_TECH_LOCKOUT 21
#define TEST_PLAYER_SHIELD_HEALTH 25
#define TEST_PLAYER_SHIELD_STUN 26
#define TEST_PLAYER_POWERSHIELD 27

static int test_install_count;
static int test_render_count;
static int test_walk_axis;
static int test_dash_axis;
static int test_input_probe;
static int test_air_facing_probe;
static int test_combat_probe;
static int test_reaction_probe;
static int test_shield_probe;
static int test_tumble_probe;
static int test_floor_recovery_probe;
static int32_t test_view[TEST_VIEW_COUNT];

void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int input_probe_passed,
    int air_facing_probe_passed,
    int combat_probe_passed,
    int reaction_probe_passed,
    int shield_probe_passed,
    int tumble_probe_passed,
    int floor_recovery_probe_passed);

void pf_web_m4_playtest_render(
    const int32_t *view,
    int view_count);

void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int input_probe_passed,
    int air_facing_probe_passed,
    int combat_probe_passed,
    int reaction_probe_passed,
    int shield_probe_passed,
    int tumble_probe_passed,
    int floor_recovery_probe_passed)
{
    ++test_install_count;
    test_walk_axis = walk_axis;
    test_dash_axis = dash_axis;
    test_input_probe = input_probe_passed;
    test_air_facing_probe = air_facing_probe_passed;
    test_combat_probe = combat_probe_passed;
    test_reaction_probe = reaction_probe_passed;
    test_shield_probe = shield_probe_passed;
    test_tumble_probe = tumble_probe_passed;
    test_floor_recovery_probe = floor_recovery_probe_passed;
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

int main(void)
{
    if (!pf_web_m4_playtest_start() ||
        test_install_count != 1 ||
        test_render_count != 1 ||
        test_walk_axis != 13500 ||
        test_dash_axis != 32767 ||
        test_input_probe != 1 ||
        test_air_facing_probe != 1 ||
        test_combat_probe != 1 ||
        test_reaction_probe != 1 ||
        test_shield_probe != 1 ||
        test_tumble_probe != 1 ||
        test_floor_recovery_probe != 1 ||
        test_view[0] != 6 ||
        test_view[1] != 0)
    {
        (void)fprintf(
            stderr,
            "m4-browser-adapter=debug installs=%d renders=%d walk=%d "
            "dash=%d input_probe=%d air_facing_probe=%d combat_probe=%d "
            "reaction_probe=%d shield_probe=%d tumble_probe=%d "
            "floor_recovery_probe=%d schema=%d tick=%d\n",
            test_install_count,
            test_render_count,
            test_walk_axis,
            test_dash_axis,
            test_input_probe,
            test_air_facing_probe,
            test_combat_probe,
            test_reaction_probe,
            test_shield_probe,
            test_tumble_probe,
            test_floor_recovery_probe,
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
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 5 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 6)
    {
        return fail("keyboard-short-hop-selection");
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
        "input_probe=%d air_facing_probe=%d combat_probe=%d reaction_probe=%d "
        "shield_probe=%d powershield_cancel_probe=%d tumble_probe=%d "
        "floor_recovery_probe=%d renders=%d\n",
        test_walk_axis,
        test_dash_axis,
        test_input_probe,
        test_air_facing_probe,
        test_combat_probe,
        test_reaction_probe,
        test_shield_probe,
        test_shield_probe,
        test_tumble_probe,
        test_floor_recovery_probe,
        test_render_count);
    return 0;
}
