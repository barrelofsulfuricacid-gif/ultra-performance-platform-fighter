#include "m4_playtest.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_VIEW_COUNT 603
#define TEST_PLAYER0_BASE 25
#define TEST_PLAYER_STRIDE 53
#define TEST_PLAYER1_BASE (TEST_PLAYER0_BASE + TEST_PLAYER_STRIDE)
#define TEST_PLAYER2_BASE (TEST_PLAYER1_BASE + TEST_PLAYER_STRIDE)
#define TEST_PLAYER3_BASE (TEST_PLAYER2_BASE + TEST_PLAYER_STRIDE)
#define TEST_ACTION_WALK 1
#define TEST_ACTION_JAB 12
#define TEST_ACTION_SHIELD 18
#define TEST_ACTION_AIR_DODGE 32
#define TEST_ACTION_ROLL_FORWARD 38
#define TEST_ACTION_GRAB 49
#define TEST_ACTION_TAUNT 75
#define TEST_ACTION_FALCON_PUNCH_GROUND 107
#define TEST_SOLID_LEFT 14
#define TEST_SOLID_RIGHT 15
#define TEST_SOLID_TOP 16
#define TEST_SOLID_BOTTOM 17
#define TEST_STOCK_COUNT 18
#define TEST_RESPAWN_DELAY 19
#define TEST_RESPAWN_INVULNERABILITY 20
#define TEST_PLAYER_ACTION 4
#define TEST_PLAYER_GROUNDED 6
#define TEST_PLAYER_HITBOX_ACTIVE 14
#define TEST_PLAYER_STOCKS 32
#define TEST_PLAYER_GRAB_TARGET 41
#define TEST_PLAYER_GRAB_OWNER 42
#define TEST_PLAYER_SHIELD_STRENGTH 45
#define TEST_PLAYER_SHIELD_ACTIVE 46
#define TEST_PLAYER_SHIELD_LEFT 47
#define TEST_PLAYER_SHIELD_RIGHT 48
#define TEST_PLAYER_SHIELD_TOP 49
#define TEST_PLAYER_SHIELD_BOTTOM 50
#define TEST_EVENT_COUNT 236
#define TEST_EVENT0 237
#define TEST_EVENT_TYPE 2
#define TEST_ITEM_BASE 397
#define TEST_ITEM_ENABLED 0
#define TEST_ITEM_STATE 1
#define TEST_ITEM_HOLDER 2
#define TEST_ITEM_SOURCE 3
#define TEST_ITEM_X 6
#define TEST_ITEM_Y 7
#define TEST_ITEM_LIFETIME 10
#define TEST_ITEM_HALF_WIDTH 14
#define TEST_ITEM_HALF_HEIGHT 15
#define TEST_PROJECTILE_BASE 415
#define TEST_PROJECTILE_ENABLED 0
#define TEST_PROJECTILE_STATE 1
#define TEST_PROJECTILE_OWNER 2
#define TEST_PROJECTILE_REFLECT_WINDOW 11
#define TEST_RECOVERY_BASE 427
#define TEST_REVIVAL_BASE 431
#define TEST_REVIVAL_STRIDE 4
#define TEST_REVIVAL_ACTIVE 0
#define TEST_STALE_MOVE_BASE 447
#define TEST_STALE_MOVE_STRIDE 12
#define TEST_STALE_MOVE_MULTIPLIER 1
#define TEST_ITEM_STALE_REGISTERED 495
#define TEST_UPPER_PLATFORM_LEFT 496
#define TEST_UPPER_PLATFORM_RIGHT 497
#define TEST_UPPER_PLATFORM_Y 498
#define TEST_PRONE_ORIENTATION0 499
#define TEST_HIT_SPHERE0 503
#define TEST_HIT_SPHERE_PLAYER_STRIDE 25
#define TEST_HIT_SPHERE_STRIDE 6

static int test_install_count;
static int test_render_count;
static int test_walk_axis;
static int test_dash_axis;
static int test_aerial_landing_lag_ticks;
static int test_strong_aerial_landing_lag_ticks;
static int32_t test_view[TEST_VIEW_COUNT];

void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks);
void pf_web_m4_playtest_render(
    const int32_t *view,
    int view_count);

void pf_web_m4_playtest_install(
    int walk_axis,
    int dash_axis,
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks)
{
    ++test_install_count;
    test_walk_axis = walk_axis;
    test_dash_axis = dash_axis;
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

static int test_dual_trigger_step(
    int player0_jump,
    int player0_left_shield,
    int player0_right_shield)
{
    return pf_web_m4_playtest_step_dual_trigger_special(
        0,
        0,
        0,
        0,
        player0_jump,
        0,
        0,
        player0_left_shield,
        player0_right_shield,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0);
}

static int test_startup_view_contract(void)
{
    return pf_web_m4_playtest_start() &&
           test_install_count == 1 &&
           test_render_count == 1 &&
           test_walk_axis == 13500 &&
           test_dash_axis == 32767 &&
           test_aerial_landing_lag_ticks == 15 &&
           test_strong_aerial_landing_lag_ticks == 30 &&
           test_view[0] == 47 &&
           test_view[1] == 0 &&
           test_view[TEST_STOCK_COUNT] == 4 &&
           test_view[TEST_RESPAWN_DELAY] == 60 &&
           test_view[TEST_RESPAWN_INVULNERABILITY] == 120 &&
           test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] == 4 &&
           test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET] == 255 &&
           test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_OWNER] == 255 &&
           test_view[TEST_PRONE_ORIENTATION0] == 0 &&
           test_view[TEST_EVENT_COUNT] == 0 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] == 1 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_STATE] == 1 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_HOLDER] == 255 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_SOURCE] == 255 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_X] == -7 * 65536 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_Y] ==
               32 * 65536 - 32768 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_LIFETIME] == 3600 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_HALF_WIDTH] == 8192 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_HALF_HEIGHT] == 32768 &&
           test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_ENABLED] == 1 &&
           test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_STATE] == 0 &&
           test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_OWNER] == 255 &&
           test_view[
               TEST_PROJECTILE_BASE + TEST_PROJECTILE_REFLECT_WINDOW] == 2 &&
           test_view[TEST_RECOVERY_BASE] == 1 &&
           test_view[TEST_RECOVERY_BASE + 1] == 1 &&
           test_view[TEST_REVIVAL_BASE + TEST_REVIVAL_ACTIVE] == 0 &&
           test_view[
               TEST_REVIVAL_BASE + TEST_REVIVAL_STRIDE +
               TEST_REVIVAL_ACTIVE] == 0 &&
           test_view[
               TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_MULTIPLIER] == 65536 &&
           test_view[
               TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_STRIDE +
               TEST_STALE_MOVE_MULTIPLIER] == 65536 &&
           test_view[TEST_ITEM_STALE_REGISTERED] == 0 &&
           test_view[TEST_HIT_SPHERE0] == 0 &&
           test_view[
               TEST_HIT_SPHERE0 + TEST_HIT_SPHERE_PLAYER_STRIDE] == 0 &&
           test_view[TEST_UPPER_PLATFORM_LEFT] == 16 * 65536 &&
           test_view[TEST_UPPER_PLATFORM_RIGHT] == 24 * 65536 &&
           test_view[TEST_UPPER_PLATFORM_Y] == 13 * 65536 &&
           test_view[TEST_SOLID_LEFT] == 14 * 65536 &&
           test_view[TEST_SOLID_RIGHT] == 27 * 65536 &&
           test_view[TEST_SOLID_TOP] == 16 * 65536 &&
           test_view[TEST_SOLID_BOTTOM] == 29 * 65536;
}

int main(void)
{
    if (!test_startup_view_contract())
    {
        return fail("startup-view-contract");
    }

    if (!pf_web_m4_playtest_refresh() ||
        test_render_count != 2 ||
        test_view[1] != 0)
    {
        return fail("pause-safe-refresh");
    }

    if (pf_web_m4_playtest_configure_duel(0) != 0 ||
        pf_web_m4_playtest_configure_duel(5) != 0 ||
        !pf_web_m4_playtest_configure_duel(2) ||
        test_view[TEST_STOCK_COUNT] != 2 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 2 ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] != 2 ||
        !pf_web_m4_playtest_configure_duel(4))
    {
        return fail("duel-configuration-contract");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            test_walk_axis,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0) ||
        test_view[1] != 1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != TEST_ACTION_WALK)
    {
        return fail("basic-input-and-tick-contract");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            1, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] !=
            TEST_ACTION_FALCON_PUNCH_GROUND ||
        test_view[TEST_EVENT_COUNT] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 24 ||
        !pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
            0, 0, 1, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != TEST_ACTION_TAUNT)
    {
        return fail("special-and-taunt-input-contract");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
            0, 0, 0, 0, 0, 19661,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] !=
            TEST_ACTION_SHIELD ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STRENGTH] != 1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_ACTIVE] != 1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_LEFT] >=
            test_view[TEST_PLAYER0_BASE] ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_RIGHT] <=
            test_view[TEST_PLAYER0_BASE] ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_TOP] >=
            test_view[TEST_PLAYER0_BASE + 1] ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_BOTTOM] <=
            test_view[TEST_PLAYER0_BASE + 1])
    {
        return fail("analog-shield-view-contract");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_dual_trigger_special(
            0, 0, 0, 0, 0, 0, 0, 65535, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] !=
            TEST_ACTION_SHIELD ||
        !pf_web_m4_playtest_step_dual_trigger_special(
            0, 0, test_dash_axis, 0, 0, 0, 0, 65535, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] !=
            TEST_ACTION_ROLL_FORWARD)
    {
        return fail("secondary-stick-input-contract");
    }

    if (!pf_web_m4_playtest_reset() ||
        !test_dual_trigger_step(0, 65535, 0) ||
        !test_dual_trigger_step(1, 65535, 0))
    {
        return fail("dual-trigger-setup");
    }
    {
        uint32_t tick;

        for (tick = UINT32_C(0);
             tick < UINT32_C(8) &&
             test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GROUNDED] != 0;
             ++tick)
        {
            if (!test_dual_trigger_step(0, 65535, 0))
            {
                return fail("dual-trigger-takeoff");
            }
        }
        if (test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GROUNDED] != 0 ||
            !test_dual_trigger_step(0, 65535, 32767) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] ==
                TEST_ACTION_AIR_DODGE ||
            !test_dual_trigger_step(0, 65535, 0) ||
            !test_dual_trigger_step(0, 65535, 65535) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] !=
                TEST_ACTION_AIR_DODGE)
        {
            return fail("independent-trigger-edge-contract");
        }
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != TEST_ACTION_JAB ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE] != 1 ||
        test_view[TEST_HIT_SPHERE0] != 3 ||
        test_view[
            TEST_HIT_SPHERE0 + 1 + TEST_HIT_SPHERE_STRIDE + 4] != 1)
    {
        return fail("hit-geometry-view-contract");
    }

    if (pf_web_m4_playtest_set_team_lab(2) != 0 ||
        !pf_web_m4_playtest_set_team_lab(1) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER2_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER3_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] != 0 ||
        !pf_web_m4_playtest_step_special(
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 1,
            0, 0, 0, 0) ||
        test_view[TEST_PLAYER2_BASE + TEST_PLAYER_ACTION] !=
            TEST_ACTION_GRAB ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION] ==
            TEST_ACTION_GRAB ||
        !pf_web_m4_playtest_set_team_lab(0) ||
        test_view[TEST_PLAYER2_BASE + TEST_PLAYER_STOCKS] != 0 ||
        test_view[TEST_PLAYER3_BASE + TEST_PLAYER_STOCKS] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] != 1)
    {
        return fail("team-lab-input-contract");
    }

    (void)printf(
        "m4-browser-adapter=pass walk_axis=%d dash_axis=%d renders=%d\n",
        test_walk_axis,
        test_dash_axis,
        test_render_count);
    return 0;
}
