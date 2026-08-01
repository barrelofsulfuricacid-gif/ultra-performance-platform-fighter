#include "m4_playtest.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_VIEW_COUNT 392
#define TEST_PLAYER0_BASE 25
#define TEST_PLAYER_STRIDE 44
#define TEST_PLAYER1_BASE (TEST_PLAYER0_BASE + TEST_PLAYER_STRIDE)
#define TEST_PLAYER2_BASE (TEST_PLAYER1_BASE + TEST_PLAYER_STRIDE)
#define TEST_PLAYER3_BASE (TEST_PLAYER2_BASE + TEST_PLAYER_STRIDE)
#define TEST_ACTION_GRAB 49
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
#define TEST_PLAYER_GRABBOX_ACTIVE 35
#define TEST_PLAYER_GRAB_ESCAPE_TICKS 40
#define TEST_PLAYER_GRAB_TARGET 41
#define TEST_PLAYER_GRAB_OWNER 42
#define TEST_EVENT_COUNT 201
#define TEST_EVENT0 202
#define TEST_EVENT_SEQUENCE 0
#define TEST_EVENT_TICK 1
#define TEST_EVENT_TYPE 2
#define TEST_EVENT_SOURCE 3
#define TEST_EVENT_TARGET 4
#define TEST_EVENT_VALUE 5
#define TEST_EVENT_DETAIL 9
#define TEST_ITEM_BASE 362
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
#define TEST_PROJECTILE_BASE 380
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

static int test_install_count;
static int test_render_count;
static int test_walk_axis;
static int test_dash_axis;
static int test_input_probe;
static int test_air_facing_probe;
static int test_instant_double_jump_probe;
static int test_double_jump_cancel_probe;
static int test_double_jump_cancel_counter_probe;
static int test_bat_drop_probe;
static int test_glide_toss_probe;
static int test_jump_cancel_throw_probe;
static int test_jump_cancel_probe;
static int test_edge_hop_probe;
static int test_edge_dash_probe;
static int test_fox_trot_probe;
static int test_moonwalk_probe;
static int test_teeter_cancel_probe;
static int test_stage_humping_probe;
static int test_taunt_cancel_probe;
static int test_scar_jump_probe;
static int test_team_wobble_probe;
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
static int test_juggling_probe;
static int test_ladder_probe;
static int test_kill_confirm_probe;
static int test_zero_to_death_probe;
static int test_ledge_cancel_probe;
static int test_planking_probe;
static int test_jump_cancelled_grab_probe;
static int test_boost_grab_probe;
static int test_jab_cancel_probe;
static int test_jab_reset_probe;
static int test_chain_grab_probe;
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
static int test_short_hop_laser_probe;
static int test_camping_probe;
static int test_shine_spike_probe;
static int test_charge_storage_probe;
static int test_aerial_landing_lag_ticks;
static int test_strong_aerial_landing_lag_ticks;
static int32_t test_view[TEST_VIEW_COUNT];

void pf_web_m4_playtest_install(
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
    test_double_jump_cancel_probe =
        double_jump_cancel_probe_passed;
    test_double_jump_cancel_counter_probe =
        double_jump_cancel_counter_probe_passed;
    test_bat_drop_probe = bat_drop_probe_passed;
    test_glide_toss_probe = glide_toss_probe_passed;
    test_jump_cancel_throw_probe = jump_cancel_throw_probe_passed;
    test_jump_cancel_probe = jump_cancel_probe_passed;
    test_edge_hop_probe = edge_hop_probe_passed;
    test_edge_dash_probe = edge_dash_probe_passed;
    test_fox_trot_probe = fox_trot_probe_passed;
    test_moonwalk_probe = moonwalk_probe_passed;
    test_teeter_cancel_probe = teeter_cancel_probe_passed;
    test_stage_humping_probe = stage_humping_probe_passed;
    test_taunt_cancel_probe = taunt_cancel_probe_passed;
    test_scar_jump_probe = scar_jump_probe_passed;
    test_team_wobble_probe = team_wobble_probe_passed;
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
    test_juggling_probe = juggling_probe_passed;
    test_ladder_probe = ladder_probe_passed;
    test_kill_confirm_probe = kill_confirm_probe_passed;
    test_zero_to_death_probe = zero_to_death_probe_passed;
    test_ledge_cancel_probe = ledge_cancel_probe_passed;
    test_planking_probe = planking_probe_passed;
    test_jump_cancelled_grab_probe =
        jump_cancelled_grab_probe_passed;
    test_boost_grab_probe = boost_grab_probe_passed;
    test_jab_cancel_probe = jab_cancel_probe_passed;
    test_jab_reset_probe = jab_reset_probe_passed;
    test_chain_grab_probe = chain_grab_probe_passed;
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
    test_short_hop_laser_probe = short_hop_laser_probe_passed;
    test_camping_probe = camping_probe_passed;
    test_shine_spike_probe = shine_spike_probe_passed;
    test_charge_storage_probe = charge_storage_probe_passed;
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
        test_double_jump_cancel_probe != 1 ||
        test_double_jump_cancel_counter_probe != 1 ||
        test_bat_drop_probe != 1 ||
        test_glide_toss_probe != 1 ||
        test_jump_cancel_throw_probe != 1 ||
        test_jump_cancel_probe != 1 ||
        test_edge_hop_probe != 1 ||
        test_edge_dash_probe != 1 ||
        test_fox_trot_probe != 1 ||
        test_moonwalk_probe != 1 ||
        test_teeter_cancel_probe != 1 ||
        test_stage_humping_probe != 1 ||
        test_taunt_cancel_probe != 1 ||
        test_scar_jump_probe != 1 ||
        test_team_wobble_probe != 1 ||
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
        test_juggling_probe != 1 ||
        test_ladder_probe != 1 ||
        test_kill_confirm_probe != 1 ||
        test_zero_to_death_probe != 1 ||
        test_ledge_cancel_probe != 1 ||
        test_planking_probe != 1 ||
        test_jump_cancelled_grab_probe != 1 ||
        test_boost_grab_probe != 1 ||
        test_jab_cancel_probe != 1 ||
        test_jab_reset_probe != 1 ||
        test_chain_grab_probe != 1 ||
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
        test_short_hop_laser_probe != 1 ||
        test_camping_probe != 1 ||
        test_shine_spike_probe != 1 ||
        test_charge_storage_probe != 1 ||
        test_aerial_landing_lag_ticks != 12 ||
        test_strong_aerial_landing_lag_ticks != 30 ||
        test_view[0] != 32 ||
        test_view[1] != 0 ||
        test_view[TEST_STOCK_COUNT] != 4 ||
        test_view[TEST_RESPAWN_DELAY] != 60 ||
        test_view[TEST_RESPAWN_INVULNERABILITY] != 120 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_STOCKS] != 4 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRABBOX_ACTIVE] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_ESCAPE_TICKS] != 0 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET] != 255 ||
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_OWNER] != 255 ||
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
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_VX] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_VY] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_LIFETIME] != 0 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_HALF_WIDTH] !=
            65536 / 5 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_HALF_HEIGHT] !=
            65536 / 5 ||
        test_view[
            TEST_PROJECTILE_BASE + TEST_PROJECTILE_REFLECT_WINDOW] != 2 ||
        test_view[TEST_SOLID_LEFT] != 14 * 65536 ||
        test_view[TEST_SOLID_RIGHT] != 27 * 65536 ||
        test_view[TEST_SOLID_TOP] != 16 * 65536 ||
        test_view[TEST_SOLID_BOTTOM] != 29 * 65536)
    {
        (void)fprintf(
            stderr,
            "m4-browser-adapter=debug installs=%d renders=%d walk=%d "
            "dash=%d input_probe=%d air_facing_probe=%d "
            "instant_double_jump_probe=%d "
            "double_jump_cancel_probe=%d "
            "double_jump_cancel_counter_probe=%d bat_drop_probe=%d "
            "glide_toss_probe=%d jump_cancel_throw_probe=%d "
            "jump_cancel_probe=%d edge_hop_probe=%d "
            "edge_dash_probe=%d fox_trot_probe=%d moonwalk_probe=%d "
            "teeter_cancel_probe=%d "
            "stage_humping_probe=%d "
            "taunt_cancel_probe=%d "
            "scar_jump_probe=%d "
            "team_wobble_probe=%d "
            "pivot_probe=%d "
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
            "juggling_probe=%d "
            "ladder_probe=%d "
            "kill_confirm_probe=%d "
            "zero_to_death_probe=%d "
            "ledge_cancel_probe=%d "
            "planking_probe=%d "
            "jump_cancelled_grab_probe=%d "
            "boost_grab_probe=%d "
            "jab_cancel_probe=%d "
            "jab_reset_probe=%d "
            "chain_grab_probe=%d "
            "combat_probe=%d "
            "reaction_probe=%d shield_probe=%d shield_break_probe=%d "
            "tumble_probe=%d "
            "floor_recovery_probe=%d tech_chase_probe=%d "
            "surface_tech_probe=%d "
            "air_dodge_probe=%d ground_dodge_probe=%d "
            "aerial_l_cancel_probe=%d match_probe=%d "
            "short_hop_laser_probe=%d camping_probe=%d "
            "shine_spike_probe=%d "
            "charge_storage_probe=%d "
            "aerial_lag=%d strong_aerial_lag=%d "
            "schema=%d tick=%d\n",
            test_install_count,
            test_render_count,
            test_walk_axis,
            test_dash_axis,
            test_input_probe,
            test_air_facing_probe,
            test_instant_double_jump_probe,
            test_double_jump_cancel_probe,
            test_double_jump_cancel_counter_probe,
            test_bat_drop_probe,
            test_glide_toss_probe,
            test_jump_cancel_throw_probe,
            test_jump_cancel_probe,
            test_edge_hop_probe,
            test_edge_dash_probe,
            test_fox_trot_probe,
            test_moonwalk_probe,
            test_teeter_cancel_probe,
            test_stage_humping_probe,
            test_taunt_cancel_probe,
            test_scar_jump_probe,
            test_team_wobble_probe,
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
            test_juggling_probe,
            test_ladder_probe,
            test_kill_confirm_probe,
            test_zero_to_death_probe,
            test_ledge_cancel_probe,
            test_planking_probe,
            test_jump_cancelled_grab_probe,
            test_boost_grab_probe,
            test_jab_cancel_probe,
            test_jab_reset_probe,
            test_chain_grab_probe,
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
            test_short_hop_laser_probe,
            test_camping_probe,
            test_shine_spike_probe,
            test_charge_storage_probe,
            test_aerial_landing_lag_ticks,
            test_strong_aerial_landing_lag_ticks,
            (int)test_view[0],
            (int)test_view[1]);
        return fail("start-and-input-probe");
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 64 ||
        test_view[TEST_EVENT_COUNT] != 1 ||
        test_view[TEST_EVENT0 + TEST_EVENT_TYPE] != 19 ||
        test_view[TEST_EVENT0 + TEST_EVENT_SOURCE] != 0 ||
        test_view[TEST_EVENT0 + TEST_EVENT_DETAIL] != 64 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_STATE] != 2 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_OWNER] != 0 ||
        test_view[
            TEST_PROJECTILE_BASE + TEST_PROJECTILE_HITBOX_ACTIVE] != 1 ||
        test_view[TEST_PROJECTILE_BASE + TEST_PROJECTILE_VX] <= 0 ||
        !pf_web_m4_playtest_reset())
    {
        return fail("live-projectile-special-route");
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 66 ||
        test_view[TEST_EVENT_COUNT] != 0 ||
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
        test_view[TEST_PLAYER0_BASE + TEST_PLAYER_HITBOX_ACTIVE] != 1 ||
        !pf_web_m4_playtest_reset())
    {
        return fail("live-reflector-down-special-route");
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
        int grab_seen = 0;
        int throw_seen = 0;

        if (!pf_web_m4_playtest_reset())
        {
            return fail("browser-grab-reset");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(23); ++tick)
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
                return fail("browser-grab-approach");
            }
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
            if (test_view[TEST_EVENT_COUNT] == 1 &&
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
                0, test_dash_axis, 0, 1, 0, 0, 0, 0, 0, 0) ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 56 ||
            test_view[TEST_EVENT_COUNT] != 0)
        {
            return fail("browser-down-throw-input-view");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
        {
            if (!pf_web_m4_playtest_step(
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
            {
                return fail("browser-down-throw-release-step");
            }
            if (test_view[TEST_EVENT_COUNT] == 1 &&
                test_view[TEST_EVENT0 + TEST_EVENT_TYPE] == 13)
            {
                throw_seen = 1;
                break;
            }
        }
        if (throw_seen == 0 ||
            test_view[TEST_EVENT0 + TEST_EVENT_SOURCE] != 0 ||
            test_view[TEST_EVENT0 + TEST_EVENT_TARGET] != 1 ||
            test_view[TEST_EVENT0 + TEST_EVENT_VALUE] != 6 * 65536 ||
            test_view[TEST_EVENT0 + TEST_EVENT_DETAIL] != 56 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_ACTION] != 13 ||
            test_view[TEST_PLAYER1_BASE + TEST_PLAYER_ACTION] != 13 ||
            test_view[TEST_PLAYER0_BASE + TEST_PLAYER_GRAB_TARGET] != 255 ||
            test_view[TEST_PLAYER1_BASE + TEST_PLAYER_GRAB_OWNER] != 255 ||
            !pf_web_m4_playtest_reset())
        {
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
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ||
        !pf_web_m4_playtest_step(
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
                    -test_dash_axis,
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
            test_view[TEST_EVENT_COUNT] != 1 ||
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
        "m4-browser-adapter=pass walk_axis=%d dash_axis=%d "
        "input_probe=%d air_facing_probe=%d "
        "instant_double_jump_probe=%d "
        "double_jump_cancel_probe=%d "
        "double_jump_cancel_counter_probe=%d bat_drop_probe=%d "
        "glide_toss_probe=%d jump_cancel_throw_probe=%d "
        "jump_cancel_probe=%d "
        "edge_hop_probe=%d "
        "edge_dash_probe=%d fox_trot_probe=%d moonwalk_probe=%d "
        "teeter_cancel_probe=%d "
        "stage_humping_probe=%d "
        "taunt_cancel_probe=%d "
        "scar_jump_probe=%d "
        "team_wobble_probe=%d "
        "pivot_probe=%d "
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
        "juggling_probe=%d "
        "ladder_probe=%d "
        "kill_confirm_probe=%d "
        "zero_to_death_probe=%d "
        "ledge_cancel_probe=%d "
        "planking_probe=%d "
        "jump_cancelled_grab_probe=%d "
        "boost_grab_probe=%d "
        "jab_cancel_probe=%d "
        "jab_reset_probe=%d "
        "chain_grab_probe=%d "
        "combat_probe=%d reaction_probe=%d "
        "shield_probe=%d shield_break_probe=%d "
        "powershield_cancel_probe=%d tumble_probe=%d "
        "floor_recovery_probe=%d tech_chase_probe=%d "
        "surface_tech_probe=%d "
        "air_dodge_probe=%d ground_dodge_probe=%d "
        "aerial_l_cancel_probe=%d match_probe=%d "
        "short_hop_laser_probe=%d camping_probe=%d "
        "shine_spike_probe=%d "
        "charge_storage_probe=%d "
        "event_journal_probe=%d renders=%d\n",
        test_walk_axis,
        test_dash_axis,
        test_input_probe,
        test_air_facing_probe,
        test_instant_double_jump_probe,
        test_double_jump_cancel_probe,
        test_double_jump_cancel_counter_probe,
        test_bat_drop_probe,
        test_glide_toss_probe,
        test_jump_cancel_throw_probe,
        test_jump_cancel_probe,
        test_edge_hop_probe,
        test_edge_dash_probe,
        test_fox_trot_probe,
        test_moonwalk_probe,
        test_teeter_cancel_probe,
        test_stage_humping_probe,
        test_taunt_cancel_probe,
        test_scar_jump_probe,
        test_team_wobble_probe,
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
        test_juggling_probe,
        test_ladder_probe,
        test_kill_confirm_probe,
        test_zero_to_death_probe,
        test_ledge_cancel_probe,
        test_planking_probe,
        test_jump_cancelled_grab_probe,
        test_boost_grab_probe,
        test_jab_cancel_probe,
        test_jab_reset_probe,
        test_chain_grab_probe,
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
        test_short_hop_laser_probe,
        test_camping_probe,
        test_shine_spike_probe,
        test_charge_storage_probe,
        test_combat_probe,
        test_render_count);
    return 0;
}
