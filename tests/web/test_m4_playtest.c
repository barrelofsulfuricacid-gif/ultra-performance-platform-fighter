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
#define TEST_ACTION_GRAB 49
#define TEST_ACTION_AIR_DODGE 32
#define TEST_SOLID_LEFT 14
#define TEST_SOLID_RIGHT 15
#define TEST_SOLID_TOP 16
#define TEST_SOLID_BOTTOM 17
#define TEST_PLAYER_ACTION 4
#define TEST_PLAYER_VX 2
#define TEST_PLAYER_VY 3
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
#define TEST_PLAYER_GRABBOX_ACTIVE 35
#define TEST_PLAYER_GRAB_ESCAPE_TICKS 40
#define TEST_PLAYER_GRAB_TARGET 41
#define TEST_PLAYER_GRAB_OWNER 42
#define TEST_PLAYER_SMASH_CHARGE_TICKS 44
#define TEST_PLAYER_SHIELD_STRENGTH 45
#define TEST_PLAYER_SHIELD_ACTIVE 46
#define TEST_PLAYER_SHIELD_LEFT 47
#define TEST_PLAYER_SHIELD_RIGHT 48
#define TEST_PLAYER_SHIELD_TOP 49
#define TEST_PLAYER_SHIELD_BOTTOM 50
#define TEST_PLAYER_SHIELD_TILT_X 51
#define TEST_PLAYER_SHIELD_TILT_Y 52
#define TEST_EVENT_COUNT 236
#define TEST_EVENT0 237
#define TEST_EVENT_SEQUENCE 0
#define TEST_EVENT_TICK 1
#define TEST_EVENT_TYPE 2
#define TEST_EVENT_SOURCE 3
#define TEST_EVENT_TARGET 4
#define TEST_EVENT_VALUE 5
#define TEST_EVENT_DETAIL 9
#define TEST_UPPER_PLATFORM_LEFT 496
#define TEST_UPPER_PLATFORM_RIGHT 497
#define TEST_UPPER_PLATFORM_Y 498
#define TEST_PRONE_ORIENTATION0 499
#define TEST_ITEM_BASE 397
#define TEST_ITEM_ENABLED 0
#define TEST_ITEM_STATE 1
#define TEST_ITEM_HOLDER 2
#define TEST_ITEM_SOURCE 3
#define TEST_ITEM_THROW_DIRECTION 4
#define TEST_ITEM_HITBOX_ACTIVE 5
#define TEST_ITEM_X 6
#define TEST_ITEM_Y 7
#define TEST_ITEM_VX 8
#define TEST_ITEM_VY 9
#define TEST_ITEM_LIFETIME 10
#define TEST_ITEM_RESPAWN 11
#define TEST_ITEM_PICKUP_LOCKOUT 12
#define TEST_ITEM_HIT_MASK 13
#define TEST_ITEM_HALF_WIDTH 14
#define TEST_ITEM_HALF_HEIGHT 15
#define TEST_ITEM_HITBOX_HALF_WIDTH 16
#define TEST_ITEM_HITBOX_HALF_HEIGHT 17
#define TEST_PROJECTILE_BASE 415
#define TEST_PROJECTILE_ENABLED 0
#define TEST_PROJECTILE_STATE 1
#define TEST_PROJECTILE_OWNER 2
#define TEST_PROJECTILE_HITBOX_ACTIVE 3
#define TEST_PROJECTILE_X 4
#define TEST_PROJECTILE_Y 5
#define TEST_PROJECTILE_VX 6
#define TEST_PROJECTILE_VY 7
#define TEST_PROJECTILE_LIFETIME 8
#define TEST_PROJECTILE_HALF_WIDTH 9
#define TEST_PROJECTILE_HALF_HEIGHT 10
#define TEST_PROJECTILE_REFLECT_WINDOW 11
#define TEST_RECOVERY_BASE 427
#define TEST_REVIVAL_BASE 431
#define TEST_REVIVAL_STRIDE 4
#define TEST_REVIVAL_ACTIVE 0
#define TEST_REVIVAL_LEFT 1
#define TEST_REVIVAL_RIGHT 2
#define TEST_REVIVAL_Y 3
#define TEST_STALE_MOVE_BASE 447
#define TEST_STALE_MOVE_STRIDE 12
#define TEST_STALE_MOVE_COUNT 0
#define TEST_STALE_MOVE_MULTIPLIER 1
#define TEST_STALE_MOVE_REGISTERED 2
#define TEST_STALE_MOVE_IDS 3
#define TEST_ITEM_STALE_REGISTERED 495
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

    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
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

static int test_player0_reach_reversible_dash(void)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
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
    return test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] == 2;
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

int main(void)
{
    if (!pf_web_m4_playtest_start() ||
        test_install_count != 1 ||
        test_render_count != 1 ||
        test_walk_axis != 13500 ||
        test_dash_axis != 32767 ||
        test_aerial_landing_lag_ticks != 15 ||
        test_strong_aerial_landing_lag_ticks != 30 ||
        test_view[0] != 47 ||
        test_view[1] != 0 ||
        test_view[TEST_STOCK_COUNT] != 4 ||
        test_view[TEST_RESPAWN_DELAY] != 60 ||
        test_view[TEST_RESPAWN_INVULNERABILITY] != 120 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PRONE_ORIENTATION0] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRABBOX_ACTIVE] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_ESCAPE_TICKS] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET] != 255 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_OWNER] != 255 ||
        test_view[
            TEST_PLAYER0_BASE + TEST_PLAYER_SMASH_CHARGE_TICKS] != 0 ||
        test_view[
            TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STRENGTH] != 0 ||
        test_view[TEST_EVENT_COUNT] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] != 1 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_STATE] != 1 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HOLDER] != 255 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_SOURCE] != 255 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_THROW_DIRECTION] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HITBOX_ACTIVE] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_X] != -7 * 65536 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_Y] !=
            32 * 65536 - 32768 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_VX] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_VY] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_LIFETIME] != 3600 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_RESPAWN] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_PICKUP_LOCKOUT] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HIT_MASK] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HALF_WIDTH] != 8192 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HALF_HEIGHT] != 32768 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HITBOX_HALF_WIDTH] !=
            7 * 65536 / 20 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_HITBOX_HALF_HEIGHT] !=
            11 * 65536 / 20 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_ENABLED] != 1 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_STATE] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_OWNER] != 255 ||
        test_view[
            TEST_PROJECTILE_BASE + TEST_PROJECTILE_HITBOX_ACTIVE] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_X] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_Y] != 0 ||
        test_view[TEST_UPPER_PLATFORM_LEFT] != 16 * 65536 ||
        test_view[TEST_UPPER_PLATFORM_RIGHT] != 24 * 65536 ||
        test_view[TEST_UPPER_PLATFORM_Y] != 13 * 65536 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_VX] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_VY] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_LIFETIME] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_HALF_WIDTH] !=
            65536 / 5 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_HALF_HEIGHT] !=
            65536 / 5 ||
        test_view[
            TEST_PROJECTILE_BASE + TEST_PROJECTILE_REFLECT_WINDOW] != 2 ||
        test_view[TEST_RECOVERY_BASE] != 1 ||
        test_view[TEST_RECOVERY_BASE + 1] != 1 ||
        test_view[TEST_REVIVAL_BASE + TEST_REVIVAL_ACTIVE] != 0 ||
        test_view[TEST_REVIVAL_BASE + TEST_REVIVAL_LEFT] != 0 ||
        test_view[TEST_REVIVAL_BASE + TEST_REVIVAL_RIGHT] != 0 ||
        test_view[TEST_REVIVAL_BASE + TEST_REVIVAL_Y] != 0 ||
        test_view[
            TEST_REVIVAL_BASE + TEST_REVIVAL_STRIDE +
            TEST_REVIVAL_ACTIVE] != 0 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_COUNT] != 0 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_MULTIPLIER] != 65536 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_REGISTERED] != 0 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_IDS] != 0 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_STRIDE +
            TEST_STALE_MOVE_MULTIPLIER] != 65536 ||
        test_view[TEST_ITEM_STALE_REGISTERED] != 0 ||
        test_view[TEST_HIT_SPHERE0] != 0 ||
        test_view[
            TEST_HIT_SPHERE0 + TEST_HIT_SPHERE_PLAYER_STRIDE] != 0 ||
        test_view[TEST_SOLID_LEFT] != 14 * 65536 ||
        test_view[TEST_SOLID_RIGHT] != 27 * 65536 ||
        test_view[TEST_SOLID_TOP] != 16 * 65536 ||
        test_view[TEST_SOLID_BOTTOM] != 29 * 65536)
    {
        (void)fprintf(
            stderr,
            "m4-browser-adapter=debug installs=%d renders=%d walk=%d "
            "dash=%d aerial_lag=%d strong_aerial_lag=%d schema=%d tick=%d\n",
            test_install_count,
            test_render_count,
            test_walk_axis,
            test_dash_axis,
            test_aerial_landing_lag_ticks,
            test_strong_aerial_landing_lag_ticks,
            (int)test_view[0],
            (int)test_view[1]);
        return fail("start-and-render");
    }

    if (!pf_web_m4_playtest_refresh() || test_render_count != 2 ||
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
        test_view[1] != 0 ||
        !pf_web_m4_playtest_configure_duel(4) ||
        test_view[TEST_STOCK_COUNT] != 4 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[1] != 0)
    {
        return fail("local-duel-configuration");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
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
            1,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 107 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS] != 1 ||
        test_view[TEST_EVENT_COUNT] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 24 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_STATE] != 0 ||
        test_view[
            TEST_PROJECTILE_BASE + TEST_PROJECTILE_HITBOX_ACTIVE] != 0 ||
        !pf_web_m4_playtest_reset())
    {
        (void)fprintf(
            stderr,
            "m4-browser-adapter=debug operation=live-falcon-punch-route "
            "action=%d action_ticks=%d events=%d projectile_state=%d "
            "projectile_hitbox=%d\n",
            (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
            (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS],
            (int)test_view[TEST_EVENT_COUNT],
            (int)test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_STATE],
            (int)test_view[
                TEST_PROJECTILE_BASE + TEST_PROJECTILE_HITBOX_ACTIVE]);
        return fail("live-falcon-punch-route");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
            0,
            32767,
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
            1,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 123 ||
        test_view[TEST_EVENT_COUNT] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 24 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_STATE] != 0 ||
        !pf_web_m4_playtest_step_special(
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
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 123 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE] != 0 ||
        !pf_web_m4_playtest_reset())
    {
        return fail("live-falcon-kick-down-special-route");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
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
            0,
            0,
            1,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 75 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS] != 1 ||
        !pf_web_m4_playtest_reset())
    {
        return fail("live-taunt-route");
    }

    {
        uint32_t tick;

        if (!pf_web_m4_playtest_reset() ||
            test_view[TEST_RECOVERY_BASE] != 1 ||
            !pf_web_m4_playtest_step_special(
                0,
                0,
                1,
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
                0))
        {
            return fail("live-vector-ascent-jump");
        }
        for (tick = UINT32_C(0);
             tick < UINT32_C(8) &&
             test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GROUNDED] != 0;
             ++tick)
        {
            if (!pf_web_m4_playtest_step_special(
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
                    0,
                    0,
                    0,
                    0))
            {
                return fail("live-vector-ascent-airborne");
            }
        }
        if (tick == UINT32_C(8) ||
            !pf_web_m4_playtest_step_special(
                32767,
                -32768,
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
                1,
                0,
                0,
                0) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 118 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS] != 1 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VY] != 0 ||
            test_view[TEST_RECOVERY_BASE] != 1 ||
            !pf_web_m4_playtest_reset() ||
            test_view[TEST_RECOVERY_BASE] != 1)
        {
            return fail("live-falcon-dive-route");
        }
    }

    {
        uint32_t tick;
        int grab_seen = 0;
        int throw_seen = 0;

        if (!pf_web_m4_playtest_reset())
        {
            return fail("browser-grab-reset");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
        {
            if (!pf_web_m4_playtest_step(
                    test_walk_axis,
                    0,
                    0,
                    0,
                    0,
                    -test_walk_axis,
                    0,
                    0,
                    0,
                    0))
            {
                return fail("browser-grab-approach");
            }
            if (test_view[TEST_PLAYER1_BASE] >
                    test_view[TEST_PLAYER0_BASE] &&
                test_view[TEST_PLAYER1_BASE] -
                        test_view[TEST_PLAYER0_BASE] <=
                    65536)
            {
                break;
            }
        }
        if (tick == UINT32_C(240))
        {
            (void)fprintf(
                stderr,
                "m4-browser-adapter=debug operation=browser-grab-range"
                " p0=%d p1=%d a0=%d a1=%d\n",
                (int)test_view[TEST_PLAYER0_BASE],
                (int)test_view[TEST_PLAYER1_BASE],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION]);
            return fail("browser-grab-approach-range");
        }
        if (!pf_web_m4_playtest_step(
                0, 0, 1, 0, 0, 0, 0, 0, 0, 1) ||
            !pf_web_m4_playtest_step(
                0, 0, 0, 1, 1, 0, 0, 0, 0, 1))
        {
            return fail("browser-grab-entry");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
        {
            if (test_view[TEST_EVENT_COUNT] >= 1 &&
                test_view[TEST_EVENT0 + TEST_EVENT_TYPE] == 11)
            {
                grab_seen = 1;
                break;
            }
            if (!pf_web_m4_playtest_step(
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 1))
            {
                return fail("browser-grab-active-step");
            }
        }
        if (grab_seen == 0 ||
            test_view[TEST_EVENT0 + TEST_EVENT_SOURCE] != 0 ||
            test_view[TEST_EVENT0 + TEST_EVENT_TARGET] != 1 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 50 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET] != 1 ||
            test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION] != 51 ||
            test_view[TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_OWNER] != 0 ||
            test_view[
                TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_ESCAPE_TICKS] <= 0)
        {
            (void)fprintf(
                stderr,
                "m4-browser-adapter=debug operation=browser-grab-view "
                "seen=%d events=%d type=%d source=%d target=%d "
                "p0_x=%d p1_x=%d p0_facing=%d p0_action=%d p0_target=%d p1_action=%d p1_owner=%d "
                "escape=%d\n",
                grab_seen,
                (int)test_view[TEST_EVENT_COUNT],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_TYPE],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_SOURCE],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_TARGET],
                (int)test_view[TEST_PLAYER0_BASE],
                (int)test_view[TEST_PLAYER1_BASE],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_OWNER],
                (int)test_view[
                    TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_ESCAPE_TICKS]);
            return fail("browser-grab-view-and-event");
        }
        if (!pf_web_m4_playtest_step(
                0, test_dash_axis, 0, 0, 0, 0, 0, 0, 0, 0) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 56 ||
            test_view[TEST_EVENT_COUNT] != 1 ||
            test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 24)
        {
            (void)fprintf(
                stderr,
                "m4-browser-adapter=debug operation=down-throw-input "
                "action=%d events=%d event0=%d\n",
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_EVENT_COUNT],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_TYPE]);
            return fail("browser-down-throw-input-view");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
        {
            if (!pf_web_m4_playtest_step(
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
            {
                return fail("browser-down-throw-release-step");
            }
            if (test_view[TEST_EVENT_COUNT] >= 1 &&
                test_view[TEST_EVENT0 + TEST_EVENT_TYPE] == 13)
            {
                throw_seen = 1;
                break;
            }
        }
        if (throw_seen == 0 ||
            test_view[TEST_EVENT0 + TEST_EVENT_SOURCE] != 0 ||
            test_view[TEST_EVENT0 + TEST_EVENT_TARGET] != 1 ||
            test_view[TEST_EVENT0 + TEST_EVENT_VALUE] != 7 * 65536 ||
            test_view[TEST_EVENT0 + TEST_EVENT_DETAIL] != 56 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 56 ||
            test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION] != 14 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET] != 255 ||
            test_view[TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_OWNER] != 255 ||
            !pf_web_m4_playtest_reset())
        {
            (void)fprintf(
                stderr,
                "m4-browser-adapter=debug operation=down-throw-release "
                "seen=%d events=%d type=%d source=%d target=%d value=%d "
                "detail=%d p0_action=%d p1_action=%d targets=(%d,%d)\n",
                throw_seen,
                (int)test_view[TEST_EVENT_COUNT],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_TYPE],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_SOURCE],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_TARGET],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_VALUE],
                (int)test_view[TEST_EVENT0 + TEST_EVENT_DETAIL],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_OWNER]);
            return fail("browser-down-throw-view-and-event");
        }
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
        !test_player0_reach_reversible_dash() ||
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 103 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != 1 ||
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
        !test_player0_reach_reversible_dash() ||
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 103 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != 1 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 12 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING] != -1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0)
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 104 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0 ||
        !pf_web_m4_playtest_step(
            0, test_dash_axis, 0, 1, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 80 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_VX] <= 0)
    {
        return fail("keyboard-dash-cancel-crouch-attack");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 18 ||
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
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            0,
            0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 18 ||
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
        !test_dual_trigger_step(0, 65535, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 18 ||
        !test_dual_trigger_step(1, 65535, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 5)
    {
        return fail("dual-trigger-held-left-setup");
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
                return fail("dual-trigger-held-left-takeoff");
            }
        }
        if (test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GROUNDED] != 0 ||
            !test_dual_trigger_step(0, 65535, 32767) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] ==
                TEST_ACTION_AIR_DODGE ||
            !test_dual_trigger_step(0, 65535, 0) ||
            !test_dual_trigger_step(0, 65535, 65535) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] !=
                TEST_ACTION_AIR_DODGE ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION_TICKS] != 0)
        {
            return fail("dual-trigger-held-left-fresh-right-air-dodge");
        }
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
            TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE] != 1 ||
        test_view[TEST_HIT_SPHERE0] != 3 ||
        test_view[TEST_HIT_SPHERE0 + 1] !=
            test_view[TEST_PLAYER0_BASE] + 72548 ||
        test_view[TEST_HIT_SPHERE0 + 2] !=
            test_view[TEST_PLAYER0_BASE + 1] +
                INT32_C(52428) - INT32_C(73843) ||
        test_view[TEST_HIT_SPHERE0 + 3] != 24040 ||
        test_view[TEST_HIT_SPHERE0 + 4] != 0 ||
        test_view[TEST_HIT_SPHERE0 + 5] != 0 ||
        test_view[TEST_HIT_SPHERE0 + 6] != 0 ||
        test_view[TEST_HIT_SPHERE0 + 1 + TEST_HIT_SPHERE_STRIDE + 4] !=
            1)
    {
        return fail("keyboard-attack-and-hitbox-view");
    }

    if (!pf_web_m4_playtest_reset())
    {
        return fail("event-journal-reset");
    }
    {
        int approach_tick;

        for (approach_tick = 0; approach_tick < 400; ++approach_tick)
        {
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
                    0))
            {
                return fail("event-journal-approach");
            }
            if (test_view[TEST_PLAYER1_BASE] >
                    test_view[TEST_PLAYER0_BASE] &&
                test_view[TEST_PLAYER1_BASE] -
                        test_view[TEST_PLAYER0_BASE] <=
                    100000)
            {
                break;
            }
        }
        if (approach_tick == 400)
        {
            (void)fprintf(
                stderr,
                "m4-browser-adapter=debug operation=event-journal-approach "
                "p0=%d p1=%d a0=%d a1=%d f0=%d f1=%d\n",
                (int)test_view[TEST_PLAYER0_BASE],
                (int)test_view[TEST_PLAYER1_BASE],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION],
                (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING],
                (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_FACING]);
            return fail("event-journal-approach-range");
        }
    }
    {
        int settle_tick;

        for (settle_tick = 0; settle_tick < 60; ++settle_tick)
        {
            if (test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] == 0 &&
                test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION] == 0)
            {
                break;
            }
            if (!pf_web_m4_playtest_step(
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
            {
                return fail("event-journal-settle");
            }
        }
        if (settle_tick == 60)
        {
            return fail("event-journal-settle-timeout");
        }
    }
    if (!pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        test_view[TEST_EVENT_COUNT] != 2 ||
        test_view[TEST_EVENT0 + TEST_EVENT_SEQUENCE] <= 0 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TICK] != test_view[1] - 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_SOURCE] != 0 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TARGET] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_VALUE] <= 0 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_COUNT] != 1 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_MULTIPLIER] != 59638 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_REGISTERED] != 1 ||
        test_view[
            TEST_STALE_MOVE_BASE + TEST_STALE_MOVE_IDS] != 12)
    {
        (void)fprintf(
            stderr,
            "m4-browser-adapter=debug operation=event-journal-hit-view "
            "events=%d type=%d source=%d target=%d value=%d "
            "p0=%d p1=%d a0=%d a1=%d f0=%d f1=%d h0=%d h1=%d "
            "spheres=%d s0=(%d,%d,%d)\n",
            (int)test_view[TEST_EVENT_COUNT],
            (int)test_view[TEST_EVENT0 + TEST_EVENT_TYPE],
            (int)test_view[TEST_EVENT0 + TEST_EVENT_SOURCE],
            (int)test_view[TEST_EVENT0 + TEST_EVENT_TARGET],
            (int)test_view[TEST_EVENT0 + TEST_EVENT_VALUE],
            (int)test_view[TEST_PLAYER0_BASE],
            (int)test_view[TEST_PLAYER1_BASE],
            (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION],
            (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION],
            (int)test_view[TEST_PLAYER0_BASE + TEST_PLAYER_FACING],
            (int)test_view[TEST_PLAYER1_BASE + TEST_PLAYER_FACING],
            (int)test_view[
                TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE],
            (int)test_view[
                TEST_PLAYER1_BASE + TEST_PLAYER_HITBOX_ACTIVE],
            (int)test_view[TEST_HIT_SPHERE0],
            (int)test_view[TEST_HIT_SPHERE0 + 1],
            (int)test_view[TEST_HIT_SPHERE0 + 2],
            (int)test_view[TEST_HIT_SPHERE0 + 3]);
        return fail("event-journal-hit-view");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step(
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_HEALTH] !=
            60 * 65536 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STUN] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_POWERSHIELD] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STRENGTH] !=
            65535 ||
        !pf_web_m4_playtest_step(
            0, 0, 0, 0, 1, 0, 0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_TECH_WINDOW] != 19 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_TECH_LOCKOUT] != 39)
    {
        return fail("keyboard-tech-trigger-edge");
    }

    if (!pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
            0, 0, 0, 0, 0, 19660,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STRENGTH] != 0 ||
        !pf_web_m4_playtest_reset() ||
        !pf_web_m4_playtest_step_special(
            0, 0, 0, 0, 0, 19661,
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 0) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 18 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_STRENGTH] !=
            1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_HEALTH] !=
            60 * 65536 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_POWERSHIELD] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_ACTIVE] != 1 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_TILT_X] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_TILT_Y] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_LEFT] >=
            test_view[TEST_PLAYER0_BASE] - test_view[12] ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_RIGHT] <=
            test_view[TEST_PLAYER0_BASE] + test_view[12] ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_TOP] >=
            test_view[TEST_PLAYER0_BASE + 1] - test_view[13] ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_BOTTOM] <=
            test_view[TEST_PLAYER0_BASE + 1] + test_view[13])
    {
        return fail("analog-light-shield-adapter");
    }

    {
        const int32_t untilted_center_sum =
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_LEFT] +
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_RIGHT];

        if (!pf_web_m4_playtest_step_special(
                10000, 0, 0, 0, 0, 19661,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 18 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_ACTIVE] != 1 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_TILT_X] <= 0 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_TILT_Y] != 0 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_LEFT] +
                    test_view[TEST_PLAYER0_BASE + TEST_PLAYER_SHIELD_RIGHT] <=
                untilted_center_sum)
        {
            return fail("analog-light-shield-tilt-view");
        }
    }

    {
        uint32_t tick;

        if (!pf_web_m4_playtest_reset())
        {
            return fail("live-item-reset");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
        {
            const int32_t delta =
                test_view[TEST_PLAYER0_BASE] -
                test_view[TEST_ITEM_BASE + TEST_ITEM_X];

            if (delta >= -65536 && delta <= 65536)
            {
                break;
            }
            if (!pf_web_m4_playtest_step(
                    -test_walk_axis,
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
                return fail("live-item-approach");
            }
        }
        if (tick == UINT32_C(40) ||
            !pf_web_m4_playtest_step(
                0, 0, 0, 1, 1, 0, 0, 0, 0, 0) ||
            test_view[TEST_EVENT_COUNT] != 1 ||
            test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 14 ||
            test_view[TEST_ITEM_BASE + TEST_ITEM_STATE] != 2 ||
            test_view[TEST_ITEM_BASE + TEST_ITEM_HOLDER] != 0 ||
            !pf_web_m4_playtest_step(
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
            !pf_web_m4_playtest_step(
                0, 0, 0, 1, 0, 0, 0, 0, 0, 0) ||
            test_view[TEST_EVENT_COUNT] != 2 ||
            test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 16 ||
            test_view[TEST_ITEM_BASE + TEST_ITEM_STATE] != 3 ||
            test_view[TEST_ITEM_BASE + TEST_ITEM_SOURCE] != 0 ||
            test_view[TEST_ITEM_BASE + TEST_ITEM_HOLDER] != 255)
        {
            return fail("live-item-pickup-and-throw");
        }
    }

    if (pf_web_m4_playtest_set_team_lab(2) != 0 ||
        !pf_web_m4_playtest_set_team_lab(1) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER2_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER3_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER2_BASE] <= test_view[TEST_PLAYER1_BASE] ||
        test_view[TEST_PLAYER3_BASE] <= test_view[TEST_PLAYER2_BASE] ||
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
        return fail("team-lab-four-player-input-mapping");
    }

    (void)printf(
        "m4-browser-adapter=pass walk_axis=%d dash_axis=%d renders=%d\n",
        test_walk_axis,
        test_dash_axis,
        test_render_count);
    return 0;
}
