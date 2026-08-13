#include "playtest.h"

#include "pf/m4.h"
#include "pf/sim.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_VIEW_COUNT 603
#define TEST_PLAYER0_BASE 25
#define TEST_PLAYER_STRIDE 53
#define TEST_PLAYER1_BASE (TEST_PLAYER0_BASE + TEST_PLAYER_STRIDE)
#define TEST_PLAYER2_BASE (TEST_PLAYER1_BASE + TEST_PLAYER_STRIDE)
#define TEST_PLAYER3_BASE (TEST_PLAYER2_BASE + TEST_PLAYER_STRIDE)
#define TEST_VIEW_SCHEMA 0
#define TEST_VIEW_TICK 1
#define TEST_SOLID_LEFT 14
#define TEST_SOLID_RIGHT 15
#define TEST_SOLID_TOP 16
#define TEST_SOLID_BOTTOM 17
#define TEST_STOCK_COUNT 18
#define TEST_PLAYER_STOCKS 32
#define TEST_PLAYER_HITBOX_ACTIVE 14
#define TEST_PLAYER_HITBOX_LEFT 15
#define TEST_PLAYER_HITBOX_RIGHT 16
#define TEST_PLAYER_HITBOX_TOP 17
#define TEST_PLAYER_HITBOX_BOTTOM 18
#define TEST_PLAYER_SHIELD_STRENGTH 45
#define TEST_PLAYER_SHIELD_ACTIVE 46
#define TEST_PLAYER_SHIELD_LEFT 47
#define TEST_PLAYER_SHIELD_RIGHT 48
#define TEST_PLAYER_SHIELD_TOP 49
#define TEST_PLAYER_SHIELD_BOTTOM 50
#define TEST_PLAYER_SHIELD_TILT_X 51
#define TEST_PLAYER_SHIELD_TILT_Y 52
#define TEST_EVENT_COUNT 236
#define TEST_ITEM_BASE 397
#define TEST_ITEM_ENABLED 0
#define TEST_PROJECTILE_BASE 415
#define TEST_PROJECTILE_ENABLED 0
#define TEST_UPPER_PLATFORM_LEFT 496
#define TEST_UPPER_PLATFORM_RIGHT 497
#define TEST_UPPER_PLATFORM_Y 498
#define TEST_HIT_SPHERE0 503
#define TEST_HIT_SPHERE_PLAYER_STRIDE 25
#define TEST_HIT_SPHERE_STRIDE 6

static int test_install_count;
static int test_render_count;
static int test_observe_count;
static int test_walk_axis;
static int test_dash_axis;
static int test_aerial_landing_lag_ticks;
static int test_strong_aerial_landing_lag_ticks;
static float test_view[TEST_VIEW_COUNT];
static pf_input_frame test_inputs[PF_SIM_MAX_PLAYERS];
static size_t test_input_count;

void pf_web_playtest_install(
    int walk_axis,
    int dash_axis,
    int aerial_landing_lag_ticks,
    int strong_aerial_landing_lag_ticks);
void pf_web_playtest_render(
    const float *view,
    int view_count);
void pf_web_playtest_observe_inputs(
    const pf_input_frame *inputs,
    size_t input_count);
int pf_web_playtest_test_render_dynamic_player(
    const player_inspection *player,
    int player_index);

void pf_web_playtest_install(
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

void pf_web_playtest_render(
    const float *view,
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

void pf_web_playtest_observe_inputs(
    const pf_input_frame *inputs,
    size_t input_count)
{
    ++test_observe_count;
    test_input_count = input_count;
    (void)memset(test_inputs, 0, sizeof(test_inputs));
    if (inputs == NULL || input_count > (size_t)PF_SIM_MAX_PLAYERS)
    {
        test_input_count = 0;
        return;
    }
    (void)memcpy(
        test_inputs,
        inputs,
        input_count * sizeof(*inputs));
}

static int fail(const char *operation)
{
    (void)fprintf(
        stderr,
        "m4-web-bridge=fail operation=%s\n",
        operation);
    return 1;
}

static int test_startup_view_contract(void)
{
    return pf_web_playtest_start() &&
           test_install_count == 1 &&
           test_render_count == 1 &&
           test_walk_axis == 13500 &&
           test_dash_axis == 32767 &&
           test_aerial_landing_lag_ticks == 15 &&
           test_strong_aerial_landing_lag_ticks == 30 &&
           test_view[TEST_VIEW_SCHEMA] == 48 &&
           test_view[TEST_VIEW_TICK] == 0 &&
           test_view[TEST_STOCK_COUNT] == 4 &&
           test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] == 4 &&
           test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] == 4 &&
           test_view[TEST_PLAYER2_BASE + TEST_PLAYER_STOCKS] == 0 &&
           test_view[TEST_PLAYER3_BASE + TEST_PLAYER_STOCKS] == 0 &&
           test_view[TEST_EVENT_COUNT] == 0 &&
           test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] == 1 &&
           test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_ENABLED] == 1 &&
           test_view[TEST_UPPER_PLATFORM_LEFT] == 16.0f &&
           test_view[TEST_UPPER_PLATFORM_RIGHT] == 24.0f &&
           test_view[TEST_UPPER_PLATFORM_Y] == 13.0f &&
           test_view[TEST_SOLID_LEFT] == 14.0f &&
           test_view[TEST_SOLID_RIGHT] == 27.0f &&
           test_view[TEST_SOLID_TOP] == 16.0f &&
           test_view[TEST_SOLID_BOTTOM] == 29.0f;
}

static int test_basic_step(void)
{
    return pf_web_playtest_step(
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0);
}

static int test_special_step(void)
{
    return pf_web_playtest_step_special(
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0);
}

static int test_input_equals(
    size_t index,
    uint64_t tick,
    uint8_t player_slot,
    int16_t main_x,
    int16_t main_y,
    int16_t secondary_x,
    int16_t secondary_y,
    uint64_t buttons,
    uint16_t left_trigger,
    uint16_t right_trigger)
{
    const pf_input_frame *input;

    if (index >= test_input_count)
    {
        return 0;
    }
    input = &test_inputs[index];
    return input->tick == tick &&
           input->schema_version == PF_SIM_INPUT_SCHEMA_VERSION &&
           input->player_slot == player_slot &&
           input->main_stick_x == main_x &&
           input->main_stick_y == main_y &&
           input->secondary_stick_x == secondary_x &&
           input->secondary_stick_y == secondary_y &&
           input->buttons == buttons &&
           input->left_trigger == left_trigger &&
           input->right_trigger == right_trigger;
}

static int test_dual_trigger_input_contract(void)
{
    const uint64_t tick = (uint64_t)test_view[TEST_VIEW_TICK];
    const int observe_count = test_observe_count;

    return pf_web_playtest_step_dual_trigger_special(
               1234, -2345, 3456, -4567,
               1, 0, 1, 11111, 22222,
               -5678, 6789, -7890, 8901,
               0, 1, 0, 33333, 44444,
               1, 1, 1, 0) &&
           test_observe_count == observe_count + 1 &&
           test_input_count == 2 &&
           test_input_equals(
               0,
               tick,
               UINT8_C(0),
               INT16_C(1234),
               -INT16_C(2345),
               INT16_C(3456),
               -INT16_C(4567),
               PF_INPUT_BUTTON_JUMP |
                   PF_INPUT_BUTTON_STRONG_ATTACK |
                   PF_INPUT_BUTTON_SPECIAL |
                   PF_INPUT_BUTTON_TAUNT,
               UINT16_C(11111),
               UINT16_C(22222)) &&
           test_input_equals(
               1,
               tick,
               UINT8_C(1),
               -INT16_C(5678),
               INT16_C(6789),
               -INT16_C(7890),
               INT16_C(8901),
               PF_INPUT_BUTTON_ATTACK | PF_INPUT_BUTTON_SPECIAL,
               UINT16_C(33333),
               UINT16_C(44444));
}

static int test_team_lab_input_contract(void)
{
    const int observe_count = test_observe_count;

    return pf_web_playtest_step_dual_trigger_special(
               0, 0, 0, 0,
               0, 0, 0, 0, 0,
               9012, -10123, 11234, -12345,
               1, 0, 1, 5555, 6666,
               0, 1, 0, 1) &&
           test_observe_count == observe_count + 1 &&
           test_input_count == 4 &&
           test_input_equals(
               1,
               UINT64_C(0),
               UINT8_C(1),
               INT16_C(0),
               INT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               UINT16_C(0)) &&
           test_input_equals(
               2,
               UINT64_C(0),
               UINT8_C(2),
               INT16_C(9012),
               -INT16_C(10123),
               INT16_C(11234),
               -INT16_C(12345),
               PF_INPUT_BUTTON_JUMP |
                   PF_INPUT_BUTTON_STRONG_ATTACK |
                   PF_INPUT_BUTTON_SPECIAL |
                   PF_INPUT_BUTTON_TAUNT,
               UINT16_C(5555),
               UINT16_C(6666)) &&
           test_input_equals(
               3,
               UINT64_C(0),
               UINT8_C(3),
               INT16_C(0),
               INT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               UINT16_C(0));
}

static int test_dynamic_view_contract(void)
{
    player_inspection player;
    const int base = TEST_PLAYER3_BASE;
    const int hit_sphere_base =
        TEST_HIT_SPHERE0 + 3 * TEST_HIT_SPHERE_PLAYER_STRIDE;
    const int render_count = test_render_count;

    (void)memset(&player, 0, sizeof(player));
    player.hitbox_active = UINT8_C(1);
    player.hitbox_left_f32 = INT32_C(101);
    player.hitbox_right_f32 = INT32_C(202);
    player.hitbox_top_f32 = -INT32_C(303);
    player.hitbox_bottom_f32 = INT32_C(404);
    player.hit_sphere_count = UINT8_C(2);
    player.hit_spheres[0].center_x_f32 = INT32_C(1001);
    player.hit_spheres[0].center_y_f32 = INT32_C(1002);
    player.hit_spheres[0].radius_f32 = INT32_C(1003);
    player.hit_spheres[0].effect_index = UINT8_C(4);
    player.hit_spheres[0].hitbox_id = UINT8_C(5);
    player.hit_spheres[0].group_id = UINT8_C(6);
    player.hit_spheres[1].center_x_f32 = INT32_C(2001);
    player.hit_spheres[1].center_y_f32 = INT32_C(2002);
    player.hit_spheres[1].radius_f32 = INT32_C(2003);
    player.hit_spheres[1].effect_index = UINT8_C(7);
    player.hit_spheres[1].hitbox_id = UINT8_C(8);
    player.hit_spheres[1].group_id = UINT8_C(9);
    player.shield_strength = UINT16_C(43210);
    player.shield_active = UINT8_C(1);
    player.shield_left_f32 = -INT32_C(501);
    player.shield_right_f32 = INT32_C(502);
    player.shield_top_f32 = -INT32_C(503);
    player.shield_bottom_f32 = INT32_C(504);
    player.shield_tilt_x = -INT16_C(505);
    player.shield_tilt_y = INT16_C(506);

    return pf_web_playtest_test_render_dynamic_player(&player, 3) &&
           test_render_count == render_count + 1 &&
           test_view[base + TEST_PLAYER_HITBOX_ACTIVE] == 1 &&
           test_view[base + TEST_PLAYER_HITBOX_LEFT] == 101 &&
           test_view[base + TEST_PLAYER_HITBOX_RIGHT] == 202 &&
           test_view[base + TEST_PLAYER_HITBOX_TOP] == -303 &&
           test_view[base + TEST_PLAYER_HITBOX_BOTTOM] == 404 &&
           test_view[hit_sphere_base] == 2 &&
           test_view[hit_sphere_base + 1] == 1001 &&
           test_view[hit_sphere_base + 2] == 1002 &&
           test_view[hit_sphere_base + 3] == 1003 &&
           test_view[hit_sphere_base + 4] == 4 &&
           test_view[hit_sphere_base + 5] == 5 &&
           test_view[hit_sphere_base + 6] == 6 &&
           test_view[
               hit_sphere_base + 1 + TEST_HIT_SPHERE_STRIDE] == 2001 &&
           test_view[
               hit_sphere_base + 2 + TEST_HIT_SPHERE_STRIDE] == 2002 &&
           test_view[
               hit_sphere_base + 3 + TEST_HIT_SPHERE_STRIDE] == 2003 &&
           test_view[
               hit_sphere_base + 4 + TEST_HIT_SPHERE_STRIDE] == 7 &&
           test_view[
               hit_sphere_base + 5 + TEST_HIT_SPHERE_STRIDE] == 8 &&
           test_view[
               hit_sphere_base + 6 + TEST_HIT_SPHERE_STRIDE] == 9 &&
           test_view[base + TEST_PLAYER_SHIELD_STRENGTH] == 43210 &&
           test_view[base + TEST_PLAYER_SHIELD_ACTIVE] == 1 &&
           test_view[base + TEST_PLAYER_SHIELD_LEFT] == -501 &&
           test_view[base + TEST_PLAYER_SHIELD_RIGHT] == 502 &&
           test_view[base + TEST_PLAYER_SHIELD_TOP] == -503 &&
           test_view[base + TEST_PLAYER_SHIELD_BOTTOM] == 504 &&
           test_view[base + TEST_PLAYER_SHIELD_TILT_X] == -505 &&
           test_view[base + TEST_PLAYER_SHIELD_TILT_Y] == 506;
}

int main(void)
{
    int render_count;

    if (!test_startup_view_contract())
    {
        return fail("startup-view-contract");
    }

    render_count = test_render_count;
    if (!pf_web_playtest_refresh() ||
        test_render_count != render_count + 1 ||
        test_view[TEST_VIEW_TICK] != 0)
    {
        return fail("pause-safe-refresh");
    }

    if (pf_web_playtest_configure_duel(0) != 0 ||
        pf_web_playtest_configure_duel(5) != 0 ||
        !pf_web_playtest_configure_duel(2) ||
        test_view[TEST_STOCK_COUNT] != 2 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 2 ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] != 2 ||
        !pf_web_playtest_configure_duel(4))
    {
        return fail("duel-configuration-contract");
    }

    render_count = test_render_count;
    if (!test_basic_step() ||
        test_render_count != render_count + 1 ||
        test_view[TEST_VIEW_TICK] != 1 ||
        !test_special_step() ||
        test_render_count != render_count + 2 ||
        test_view[TEST_VIEW_TICK] != 2 ||
        !test_dual_trigger_input_contract() ||
        test_render_count != render_count + 3 ||
        test_view[TEST_VIEW_TICK] != 3)
    {
        return fail("step-endpoint-contract");
    }

    if (!test_dynamic_view_contract())
    {
        return fail("dynamic-view-packing-contract");
    }

    if (pf_web_playtest_set_team_lab(2) != 0 ||
        !pf_web_playtest_set_team_lab(1) ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER1_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER2_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER3_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] != 0 ||
        !test_team_lab_input_contract() ||
        !pf_web_playtest_set_team_lab(0) ||
        test_view[TEST_PLAYER2_BASE + TEST_PLAYER_STOCKS] != 0 ||
        test_view[TEST_PLAYER3_BASE + TEST_PLAYER_STOCKS] != 0 ||
        test_view[TEST_ITEM_BASE + TEST_ITEM_ENABLED] != 1)
    {
        return fail("team-lab-configuration-contract");
    }

    (void)printf(
        "m4-web-bridge=pass view_schema=48 endpoints=3 renders=%d\n",
        test_render_count);
    return 0;
}
