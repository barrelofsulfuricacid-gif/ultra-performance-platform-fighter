#include "pf/m4.h"
#include "pf/replay.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_SAVE_CAPACITY 1024U
#define TEST_DETERMINISTIC_TICKS UINT64_C(20000)
#define TEST_PSC_REPLAY_TICKS UINT64_C(20)
#define TEST_PSC_REPLAY_INPUT_COUNT 40U
#define TEST_PSC_REPLAY_HASH_COUNT 21U
#define TEST_PSC_REPLAY_CAPACITY 8192U
#define TEST_ALC_REPLAY_TICKS UINT64_C(96)
#define TEST_ALC_REPLAY_INPUT_COUNT 192U
#define TEST_ALC_REPLAY_HASH_COUNT 97U
#define TEST_ALC_REPLAY_CAPACITY 16384U
#define TEST_MAX_HITSTUN_TICKS UINT16_C(600)

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static pf_tick_result test_last_result;

static int fail(const char *operation)
{
    (void)fprintf(
        stderr,
        "m4-combat=fail operation=%s\n",
        operation);
    return 0;
}

static int expect_status(
    pf_status actual,
    pf_status expected,
    const char *operation)
{
    if (actual != expected)
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static int make_combat_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "combat-content-view");
}

static int make_grab_content(
    int32_t spawn_spacing_q16,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "grab-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 = spawn_spacing_q16;
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "grab-content-view");
}

static int make_team_wobble_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "team-wobble-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(2) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "team-wobble-content-view");
}

static int make_boost_grab_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "boost-grab-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(17) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "boost-grab-content-view");
}

static int make_jab_cancel_content(
    int32_t spawn_spacing_q16,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "jab-cancel-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 = spawn_spacing_q16;
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->fighter.jab_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.jab_base_knockback_y_q16 = INT32_C(1);
    out_content->fighter.jab_knockback_growth_q16 = INT32_C(1);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "jab-cancel-content-view");
}

static int make_jab_reset_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "jab-reset-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->fighter.strong_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.strong_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(2);
    out_content->fighter.strong_knockback_growth_q16 = INT32_C(1);
    out_content->fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(13);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "jab-reset-content-view");
}

static int make_grab_damage_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_grab_content(
            (INT32_C(4) * PF_Q16_ONE) / INT32_C(5),
            out_content,
            out_view))
    {
        return 0;
    }

    out_content->fighter.jab_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.jab_base_knockback_y_q16 = INT32_C(1);
    out_content->fighter.jab_knockback_growth_q16 = INT32_C(1);
    out_content->fighter.grab_escape_damage_ticks_q16 = PF_Q16_ONE;
    out_content->fighter.grab_escape_max_ticks = UINT16_C(33);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "grab-damage-content-view");
}

static int make_chain_grab_escape_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_grab_content(
            (INT32_C(4) * PF_Q16_ONE) / INT32_C(5),
            out_content,
            out_view))
    {
        return 0;
    }

    out_content->fighter.jab_damage_q16 =
        UINT32_C(45) * UINT32_C(65536);
    out_content->fighter.jab_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.jab_base_knockback_y_q16 = INT32_C(1);
    out_content->fighter.jab_knockback_growth_q16 = INT32_C(1);
    out_content->fighter.jab_startup_ticks = UINT16_C(1);
    out_content->fighter.jab_active_ticks = UINT16_C(1);
    out_content->fighter.jab_recovery_ticks = UINT16_C(1);
    out_content->fighter.jab_hitlag_ticks = UINT16_C(1);
    out_content->fighter.jab_combo_input_begin_tick = UINT16_C(2);
    out_content->fighter.jab_combo_input_end_tick = UINT16_C(2);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "chain-grab-escape-content-view");
}

static int make_small_step_forward_smash_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "small-step-forward-smash-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(2);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "small-step-forward-smash-content-view");
}

static int make_drop_cancel_content(
    int whiff_fixture,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "drop-cancel-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        whiff_fixture != 0
            ? INT32_C(4) * PF_Q16_ONE
            : PF_Q16_ONE / INT32_C(2);
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "drop-cancel-content-view");
}

static int make_v_cancel_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_combat_content(out_content, out_view))
    {
        return 0;
    }

    out_content->fighter.air_dodge_invulnerability_begin_tick =
        UINT16_C(6);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "v-cancel-content-view");
}

static int make_double_jump_cancel_counter_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "double-jump-cancel-counter-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->fighter.aerial_startup_ticks = UINT16_C(1);
    out_content->fighter.platform_drop_ticks = UINT16_C(7);
    out_content->fighter.aerial_landing_lag_begin_tick =
        UINT16_C(1);
    out_content->fighter.aerial_hitbox_half_width_q16 = PF_Q16_ONE;
    out_content->fighter.aerial_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->fighter.strong_hitbox_half_width_q16 = PF_Q16_ONE;
    out_content->fighter.strong_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "double-jump-cancel-counter-content-view");
}

static int make_spacing_content(
    int32_t spawn_spacing_q16,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "spacing-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 = spawn_spacing_q16;
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "spacing-content-view");
}

static int make_cross_up_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "cross-up-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "cross-up-content-view");
}

static int make_juggling_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "juggling-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(10);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "juggling-content-view");
}

static int make_ladder_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "ladder-default-content"))
    {
        return 0;
    }

    out_content->fighter.aerial_hitbox_offset_x_q16 = INT32_C(0);
    out_content->fighter.aerial_hitbox_offset_y_q16 =
        -PF_Q16_ONE / INT32_C(4);
    out_content->fighter.aerial_hitbox_half_width_q16 = PF_Q16_ONE;
    out_content->fighter.aerial_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->fighter.aerial_damage_q16 =
        UINT32_C(4) * UINT32_C(65536);
    out_content->fighter.aerial_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.aerial_base_knockback_y_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.aerial_knockback_growth_q16 = INT32_C(1);
    out_content->fighter.aerial_startup_ticks = UINT16_C(1);
    out_content->fighter.aerial_active_ticks = UINT16_C(2);
    out_content->fighter.aerial_recovery_ticks = UINT16_C(2);
    out_content->fighter.aerial_hitlag_ticks = UINT16_C(3);
    out_content->fighter.platform_drop_ticks = UINT16_C(4);
    out_content->fighter.aerial_landing_lag_begin_tick = UINT16_C(1);
    out_content->fighter.aerial_landing_lag_end_tick = UINT16_C(4);
    out_content->fighter.strong_hitbox_offset_x_q16 = INT32_C(0);
    out_content->fighter.strong_hitbox_offset_y_q16 =
        -PF_Q16_ONE / INT32_C(4);
    out_content->fighter.strong_hitbox_half_width_q16 = PF_Q16_ONE;
    out_content->fighter.strong_hitbox_half_height_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->fighter.strong_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.strong_base_knockback_y_q16 =
        (INT32_C(15) * PF_Q16_ONE) / INT32_C(16);
    out_content->fighter.strong_knockback_growth_q16 = INT32_C(1);
    out_content->fighter.strong_startup_ticks = UINT16_C(2);
    out_content->fighter.strong_active_ticks = UINT16_C(2);
    out_content->fighter.strong_recovery_ticks = UINT16_C(6);
    out_content->fighter.strong_hitlag_ticks = UINT16_C(4);
    out_content->fighter.hitstun_velocity_per_tick_q16 =
        PF_Q16_ONE / INT32_C(200);
    out_content->fighter.full_hop_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.double_jump_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.gravity_q16 = PF_Q16_ONE / INT32_C(100);
    out_content->fighter.tumble_hitstun_threshold_ticks = UINT16_C(600);
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 = INT32_C(20) * PF_Q16_ONE;
    out_content->stage.solid_right_q16 = INT32_C(30) * PF_Q16_ONE;
    out_content->stage.blast_top_q16 = INT32_C(4) * PF_Q16_ONE;
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "ladder-content-view");
}

static int make_kill_confirm_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "kill-confirm-default-content"))
    {
        return 0;
    }

    out_content->fighter.jab_base_knockback_x_q16 = INT32_C(1);
    out_content->fighter.jab_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(5);
    out_content->fighter.jab_knockback_growth_q16 = INT32_C(1);
    out_content->fighter.strong_base_knockback_x_q16 =
        PF_Q16_ONE / INT32_C(20);
    out_content->fighter.hitstun_velocity_per_tick_q16 =
        PF_Q16_ONE / INT32_C(200);
    out_content->fighter.jab_recovery_ticks = UINT16_C(3);
    out_content->fighter.jab_combo_input_end_tick = UINT16_C(6);
    out_content->fighter.tumble_hitstun_threshold_ticks = UINT16_C(600);
    out_content->stage.floor_left_q16 =
        -INT32_C(60) * PF_Q16_ONE;
    out_content->stage.floor_right_q16 =
        INT32_C(60) * PF_Q16_ONE;
    out_content->stage.platform_center_x_q16 =
        -INT32_C(30) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 =
        -INT32_C(55) * PF_Q16_ONE;
    out_content->stage.solid_right_q16 =
        -INT32_C(45) * PF_Q16_ONE;
    out_content->stage.blast_left_q16 =
        -INT32_C(64) * PF_Q16_ONE;
    out_content->stage.blast_right_q16 =
        INT32_C(64) * PF_Q16_ONE;
    out_content->stage.blast_top_q16 = INT32_C(8) * PF_Q16_ONE;
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "kill-confirm-content-view");
}

static int make_reaction_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_combat_content(out_content, out_view))
    {
        return 0;
    }

    out_content->fighter.jab_base_knockback_x_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(10);
    out_content->fighter.jab_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.jab_knockback_growth_q16 =
        PF_Q16_ONE / INT32_C(4096);
    out_content->fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(20);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "reaction-content-view");
}

static int make_tech_invulnerability_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_combat_content(out_content, out_view))
    {
        return 0;
    }

    out_content->fighter.jab_base_knockback_x_q16 =
        PF_Q16_ONE / INT32_C(100);
    out_content->fighter.jab_base_knockback_y_q16 =
        (INT32_C(17) * PF_Q16_ONE) / INT32_C(20);
    out_content->fighter.jab_knockback_growth_q16 =
        PF_Q16_ONE / INT32_C(4096);
    out_content->fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(20);
    out_content->stage.platform_center_x_q16 =
        INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 =
        INT32_C(0);
    out_content->stage.solid_left_q16 =
        INT32_C(24) * PF_Q16_ONE;
    out_content->stage.solid_right_q16 =
        INT32_C(30) * PF_Q16_ONE;
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(2) * PF_Q16_ONE) / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "tech-invulnerability-content-view");
}

static int make_floor_attack_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!make_tech_invulnerability_content(
            out_content,
            out_view))
    {
        return 0;
    }

    out_content->fighter.ground_acceleration_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.turn_acceleration_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.traction_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.walk_speed_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(20);
    out_content->fighter.run_speed_q16 =
        PF_Q16_ONE / INT32_C(5);
    out_content->fighter.initial_dash_speed_q16 =
        PF_Q16_ONE / INT32_C(5);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "floor-attack-content-view");
}

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    int reset,
    pf_sim **out_sim)
{
    pf_sim_config config;

    if (!expect_status(
            pf_sim_default_config(&config, player_count, mode),
            PF_STATUS_OK,
            "default-config"))
    {
        return 0;
    }
    config.max_ticks = UINT64_C(100000);
    config.stock_count = UINT8_C(0);
    if (!expect_status(
            pf_sim_init(
                storage->state,
                sizeof(storage->state),
                storage->scratch,
                sizeof(storage->scratch),
                content,
                &config,
                out_sim),
            PF_STATUS_OK,
            "init"))
    {
        return 0;
    }
    return reset == 0 ||
           expect_status(
               pf_sim_reset(*out_sim, UINT64_C(0x4d34434f4d424154)),
               PF_STATUS_OK,
               "reset");
}

static void make_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint8_t player_count,
    uint64_t tick)
{
    uint32_t player_index;

    (void)memset(
        inputs,
        0,
        sizeof(*inputs) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)player_count;
         ++player_index)
    {
        inputs[player_index].tick = tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }
}

static int step_players_with_triggers(
    pf_sim *sim,
    uint8_t player_count,
    const int16_t axes_x[PF_SIM_MAX_PLAYERS],
    const int16_t axes_y[PF_SIM_MAX_PLAYERS],
    const uint64_t buttons[PF_SIM_MAX_PLAYERS],
    const uint16_t left_triggers[PF_SIM_MAX_PLAYERS],
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_m4_inspection before;
    uint32_t player_index;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, player_count, before.tick);
    for (player_index = UINT32_C(0);
         player_index < (uint32_t)player_count;
         ++player_index)
    {
        inputs[player_index].main_stick_x = axes_x[player_index];
        inputs[player_index].main_stick_y = axes_y[player_index];
        inputs[player_index].buttons = buttons[player_index];
        inputs[player_index].left_trigger =
            left_triggers[player_index];
    }
    return expect_status(
               pf_sim_tick(
                   sim,
                   inputs,
                   (size_t)player_count,
                   &test_last_result),
               PF_STATUS_OK,
               "tick") &&
           expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "inspect-after-step");
}

static int step_players(
    pf_sim *sim,
    uint8_t player_count,
    const int16_t axes_x[PF_SIM_MAX_PLAYERS],
    const int16_t axes_y[PF_SIM_MAX_PLAYERS],
    const uint64_t buttons[PF_SIM_MAX_PLAYERS],
    pf_m4_inspection *out_inspection)
{
    const uint16_t left_triggers[PF_SIM_MAX_PLAYERS] = {
        UINT16_C(0), UINT16_C(0), UINT16_C(0), UINT16_C(0)};

    return step_players_with_triggers(
        sim,
        player_count,
        axes_x,
        axes_y,
        buttons,
        left_triggers,
        out_inspection);
}

static int step_duel(
    pf_sim *sim,
    int16_t player0_x,
    uint64_t player0_buttons,
    int16_t player1_x,
    uint64_t player1_buttons,
    pf_m4_inspection *out_inspection)
{
    const int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        player0_x, player1_x, INT16_C(0), INT16_C(0)};
    const int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    const uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        player0_buttons,
        player1_buttons,
        UINT64_C(0),
        UINT64_C(0)};

    return step_players(
        sim,
        UINT8_C(2),
        axes_x,
        axes_y,
        buttons,
        out_inspection);
}

static int step_reaction_duel(
    pf_sim *sim,
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
    const int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        player0_x, player1_x, INT16_C(0), INT16_C(0)};
    const int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        player0_y, player1_y, INT16_C(0), INT16_C(0)};
    const uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        player0_buttons,
        player1_buttons,
        UINT64_C(0),
        UINT64_C(0)};
    const uint16_t left_triggers[PF_SIM_MAX_PLAYERS] = {
        player0_trigger,
        player1_trigger,
        UINT16_C(0),
        UINT16_C(0)};

    return step_players_with_triggers(
        sim,
        UINT8_C(2),
        axes_x,
        axes_y,
        buttons,
        left_triggers,
        out_inspection);
}

static int hash_equal(
    const pf_state_hash *left,
    const pf_state_hash *right)
{
    return left->algorithm == right->algorithm &&
           left->algorithm_version == right->algorithm_version &&
           left->digest_size == right->digest_size &&
           left->reserved == right->reserved &&
           memcmp(left->bytes, right->bytes, sizeof(left->bytes)) == 0;
}

static const pf_sim_event *find_last_tick_event(
    pf_sim_event_type type)
{
    uint32_t event_index;

    for (event_index = UINT32_C(0);
         event_index < (uint32_t)test_last_result.event_count;
         ++event_index)
    {
        if (test_last_result.events[event_index].type == (uint16_t)type)
        {
            return &test_last_result.events[event_index];
        }
    }
    return NULL;
}

typedef struct test_v_cancel_result
{
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint16_t hitstun_ticks;
    uint16_t tech_lockout_ticks;
    uint8_t tumble;
    uint8_t trigger_input_age;
} test_v_cancel_result;

static int run_v_cancel_air_case(
    const pf_m4_content *content,
    const pf_content_view *view,
    uint32_t trigger_lead_ticks,
    int target_attacks,
    int preexisting_lockout,
    test_v_cancel_result *out_result)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;
    int hit_found = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }
    if (preexisting_lockout != 0 &&
        !step_reaction_duel(
            sim,
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
    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
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
        if (!step_reaction_duel(
                sim,
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
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            target_attacks != 0
                ? PF_INPUT_BUTTON_STRONG_ATTACK
                : UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(12) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        uint16_t target_trigger = UINT16_C(0);

        if (trigger_lead_ticks != UINT32_MAX &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
            (uint32_t)inspection.players[0].action_ticks +
                    trigger_lead_ticks + UINT32_C(1) ==
                (uint32_t)content->fighter.aerial_startup_ticks)
        {
            target_trigger = UINT16_MAX;
        }
        if (!step_reaction_duel(
                sim,
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
         tick < (uint32_t)test_last_result.event_count;
         ++tick)
    {
        const pf_sim_event *event = &test_last_result.events[tick];

        if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            out_result->velocity_x_q16 = event->velocity_x_q16;
            out_result->velocity_y_q16 = event->velocity_y_q16;
            hit_found = 1;
            break;
        }
    }
    if (hit_found == 0 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return 0;
    }
    out_result->hitstun_ticks =
        inspection.players[1].hitstun_ticks;
    out_result->tech_lockout_ticks =
        inspection.players[1].tech_lockout_ticks;
    out_result->tumble = inspection.players[1].tumble;
    out_result->trigger_input_age =
        inspection.players[1].trigger_input_age;
    return 1;
}

static int run_v_cancel_ground_case(
    const pf_content_view *view,
    int trigger_on_hit,
    test_v_cancel_result *out_result)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t event_index;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            UINT64_C(0),
            trigger_on_hit != 0 ? UINT16_MAX : UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (event_index = UINT32_C(0);
         event_index < (uint32_t)test_last_result.event_count;
         ++event_index)
    {
        const pf_sim_event *event =
            &test_last_result.events[event_index];

        if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            out_result->velocity_x_q16 = event->velocity_x_q16;
            out_result->velocity_y_q16 = event->velocity_y_q16;
            out_result->hitstun_ticks =
                inspection.players[1].hitstun_ticks;
            out_result->tech_lockout_ticks =
                inspection.players[1].tech_lockout_ticks;
            out_result->tumble = inspection.players[1].tumble;
            out_result->trigger_input_age =
                inspection.players[1].trigger_input_age;
            return 1;
        }
    }
    return 0;
}

static int run_v_cancel_jump_case(
    const pf_content_view *view,
    int trigger_during_jump_squat,
    test_v_cancel_result *out_result)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t event_index;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            trigger_during_jump_squat != 0
                ? UINT16_MAX
                : UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
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
    for (event_index = UINT32_C(0);
         event_index < (uint32_t)test_last_result.event_count;
         ++event_index)
    {
        const pf_sim_event *event =
            &test_last_result.events[event_index];

        if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            out_result->velocity_x_q16 = event->velocity_x_q16;
            out_result->velocity_y_q16 = event->velocity_y_q16;
            out_result->hitstun_ticks =
                inspection.players[1].hitstun_ticks;
            out_result->tech_lockout_ticks =
                inspection.players[1].tech_lockout_ticks;
            out_result->tumble = inspection.players[1].tumble;
            out_result->trigger_input_age =
                inspection.players[1].trigger_input_age;
            return inspection.players[1].grounded == UINT8_C(0) &&
                   inspection.players[1].action_state ==
                       (uint8_t)PF_M4_ACTION_HITLAG;
        }
    }
    return 0;
}

static int run_v_cancel_fall_special_case(
    const pf_content_view *view,
    int trigger_on_hit,
    test_v_cancel_result *out_result)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         inspection.players[1].grounded != UINT8_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
    if (inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
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
            (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(60) &&
         inspection.players[1].action_state !=
             (uint8_t)PF_M4_ACTION_FALL_SPECIAL;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        inspection.players[1].tech_lockout_ticks != UINT16_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            trigger_on_hit != 0 ? UINT16_MAX : UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)test_last_result.event_count;
         ++tick)
    {
        const pf_sim_event *event = &test_last_result.events[tick];

        if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            out_result->velocity_x_q16 = event->velocity_x_q16;
            out_result->velocity_y_q16 = event->velocity_y_q16;
            out_result->hitstun_ticks =
                inspection.players[1].hitstun_ticks;
            out_result->tech_lockout_ticks =
                inspection.players[1].tech_lockout_ticks;
            out_result->tumble = inspection.players[1].tumble;
            out_result->trigger_input_age =
                inspection.players[1].trigger_input_age;
            return 1;
        }
    }
    return 0;
}

static int prepare_drop_cancel_platform(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
        if (!step_reaction_duel(
                sim,
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
    return out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[1].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[1].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE;
}

static int run_one_way_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t frozen_x[2];
    int32_t frozen_y[2];
    uint32_t freeze_tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return fail("startup-and-active-schedule");
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.jab_hitlag_ticks ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.jab_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].last_hit_valid != UINT8_C(1) ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[1].last_hit_sequence != UINT32_C(1) ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].last_hit_tick + UINT64_C(1) !=
            inspection.tick ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        test_last_result.events[0].sequence !=
            inspection.players[1].last_hit_sequence ||
        test_last_result.events[0].tick !=
            inspection.players[1].last_hit_tick ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 !=
            content->fighter.jab_damage_q16 ||
        test_last_result.events[0].velocity_x_q16 <= INT32_C(0) ||
        test_last_result.events[0].velocity_y_q16 >= INT32_C(0) ||
        test_last_result.events[0].flags != UINT16_C(0) ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK ||
        (inspection.players[0].attack_hit_mask & UINT8_C(2)) ==
            UINT8_C(0))
    {
        return fail("damage-hitlag-and-event");
    }

    frozen_x[0] = inspection.players[0].position_x_q16;
    frozen_x[1] = inspection.players[1].position_x_q16;
    frozen_y[0] = inspection.players[0].position_y_q16;
    frozen_y[1] = inspection.players[1].position_y_q16;
    for (freeze_tick = UINT32_C(0);
         freeze_tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++freeze_tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
                &inspection) ||
            inspection.players[0].position_x_q16 != frozen_x[0] ||
            inspection.players[1].position_x_q16 != frozen_x[1] ||
            inspection.players[0].position_y_q16 != frozen_y[0] ||
            inspection.players[1].position_y_q16 != frozen_y[1])
        {
            return fail("hitlag-freeze");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[1].velocity_y_q16 >= INT32_C(0) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        inspection.players[1].position_x_q16 <= frozen_x[1] ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16)
    {
        return fail("knockback-hitstun-and-single-hit");
    }

    return 1;
}

typedef struct test_weight_reaction
{
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint32_t damage_q16;
    uint16_t hitstun_ticks;
    uint16_t hitlag_ticks;
} test_weight_reaction;

static int run_weight_reaction_case(
    const pf_content_view *view,
    test_weight_reaction *out_reaction)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *event;

        if (!step_duel(
                sim,
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
        event = find_last_tick_event(PF_SIM_EVENT_HIT);
        if (event != NULL)
        {
            out_reaction->velocity_x_q16 = event->velocity_x_q16;
            out_reaction->velocity_y_q16 = event->velocity_y_q16;
            out_reaction->damage_q16 = event->value_q16;
            out_reaction->hitstun_ticks =
                inspection.players[1].hitstun_ticks;
            out_reaction->hitlag_ticks =
                inspection.players[1].hitlag_ticks;
            return 1;
        }
    }
    return fail("weight-reaction-hit-missing");
}

static uint16_t expected_weight_hitstun_ticks(
    const pf_m4_fighter_data *fighter,
    int32_t velocity_x_q16,
    int32_t velocity_y_q16)
{
    const int64_t horizontal =
        velocity_x_q16 < INT32_C(0)
            ? -(int64_t)velocity_x_q16
            : (int64_t)velocity_x_q16;
    const int64_t vertical =
        velocity_y_q16 < INT32_C(0)
            ? -(int64_t)velocity_y_q16
            : (int64_t)velocity_y_q16;
    const int64_t divisor =
        (int64_t)fighter->hitstun_velocity_per_tick_q16;
    int64_t ticks =
        (horizontal + vertical + divisor - INT64_C(1)) / divisor;

    if (ticks < INT64_C(1))
    {
        ticks = INT64_C(1);
    }
    if (ticks > (int64_t)TEST_MAX_HITSTUN_TICKS)
    {
        ticks = (int64_t)TEST_MAX_HITSTUN_TICKS;
    }
    return (uint16_t)ticks;
}

static int run_weight_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    pf_m4_content minimum = *content;
    pf_m4_content heavy = *content;
    pf_m4_content below_minimum = *content;
    pf_m4_content above_maximum = *content;
    pf_content_view heavy_view;
    test_weight_reaction ordinary_reaction;
    test_weight_reaction heavy_reaction;

    minimum.fighter.weight_q16 = PF_Q16_ONE / INT32_C(2);
    heavy.fighter.weight_q16 = INT32_C(2) * PF_Q16_ONE;
    below_minimum.fighter.weight_q16 =
        PF_Q16_ONE / INT32_C(2) - INT32_C(1);
    above_maximum.fighter.weight_q16 =
        INT32_C(2) * PF_Q16_ONE + INT32_C(1);

    if (content->fighter.weight_q16 != PF_Q16_ONE ||
        !expect_status(
            pf_m4_validate_content(&minimum),
            PF_STATUS_OK,
            "accept-minimum-weight") ||
        !expect_status(
            pf_m4_validate_content(&heavy),
            PF_STATUS_OK,
            "accept-maximum-weight") ||
        !expect_status(
            pf_m4_validate_content(&below_minimum),
            PF_STATUS_INVALID_CONFIG,
            "reject-below-minimum-weight") ||
        !expect_status(
            pf_m4_validate_content(&above_maximum),
            PF_STATUS_INVALID_CONFIG,
            "reject-above-maximum-weight") ||
        !expect_status(
            pf_m4_make_content_view(&heavy, &heavy_view),
            PF_STATUS_OK,
            "heavy-weight-content-view") ||
        memcmp(
            view->content_hash.bytes,
            heavy_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        !run_weight_reaction_case(view, &ordinary_reaction) ||
        !run_weight_reaction_case(&heavy_view, &heavy_reaction))
    {
        return fail("weight-data-and-content-hash");
    }

    if (heavy_reaction.velocity_x_q16 !=
            ordinary_reaction.velocity_x_q16 / INT32_C(2) ||
        heavy_reaction.velocity_y_q16 !=
            ordinary_reaction.velocity_y_q16 / INT32_C(2) ||
        ordinary_reaction.hitstun_ticks !=
            expected_weight_hitstun_ticks(
                &content->fighter,
                ordinary_reaction.velocity_x_q16,
                ordinary_reaction.velocity_y_q16) ||
        heavy_reaction.hitstun_ticks !=
            expected_weight_hitstun_ticks(
                &heavy.fighter,
                heavy_reaction.velocity_x_q16,
                heavy_reaction.velocity_y_q16) ||
        heavy_reaction.hitstun_ticks >=
            ordinary_reaction.hitstun_ticks ||
        ordinary_reaction.damage_q16 != content->fighter.jab_damage_q16 ||
        heavy_reaction.damage_q16 != ordinary_reaction.damage_q16 ||
        ordinary_reaction.hitlag_ticks !=
            content->fighter.jab_hitlag_ticks ||
        heavy_reaction.hitlag_ticks != ordinary_reaction.hitlag_ticks)
    {
        return fail("weight-scales-launch-and-hitstun-only");
    }
    return 1;
}

static int run_v_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_v_cancel_result ordinary;
    test_v_cancel_result age_zero;
    test_v_cancel_result age_one;
    test_v_cancel_result age_two;
    test_v_cancel_result age_three;
    test_v_cancel_result attacking;
    test_v_cancel_result locked_out;
    test_v_cancel_result grounded;
    test_v_cancel_result grounded_trigger;
    test_v_cancel_result jump_ordinary;
    test_v_cancel_result jump_cancelled;
    test_v_cancel_result fall_special_ordinary;
    test_v_cancel_result fall_special_cancelled;
    int32_t expected_x;
    int32_t expected_y;

    if (content->fighter.v_cancel_velocity_scale_q16 !=
            (INT32_C(95) * PF_Q16_ONE) / INT32_C(100) ||
        content->fighter.v_cancel_window_ticks != UINT16_C(3) ||
        content->fighter.tech_lockout_ticks != UINT16_C(40))
    {
        return fail("v-cancel-default-data");
    }
    if (!run_v_cancel_air_case(
            content,
            view,
            UINT32_MAX,
            0,
            0,
            &ordinary) ||
        !run_v_cancel_air_case(
            content,
            view,
            UINT32_C(0),
            0,
            0,
            &age_zero) ||
        !run_v_cancel_air_case(
            content,
            view,
            UINT32_C(1),
            0,
            0,
            &age_one) ||
        !run_v_cancel_air_case(
            content,
            view,
            UINT32_C(2),
            0,
            0,
            &age_two) ||
        !run_v_cancel_air_case(
            content,
            view,
            UINT32_C(3),
            0,
            0,
            &age_three) ||
        !run_v_cancel_air_case(
            content,
            view,
            UINT32_C(0),
            1,
            0,
            &attacking) ||
        !run_v_cancel_air_case(
            content,
            view,
            UINT32_C(0),
            0,
            1,
            &locked_out) ||
        !run_v_cancel_ground_case(view, 0, &grounded) ||
        !run_v_cancel_ground_case(view, 1, &grounded_trigger) ||
        !run_v_cancel_jump_case(view, 0, &jump_ordinary) ||
        !run_v_cancel_jump_case(view, 1, &jump_cancelled) ||
        !run_v_cancel_fall_special_case(
            view,
            0,
            &fall_special_ordinary) ||
        !run_v_cancel_fall_special_case(
            view,
            1,
            &fall_special_cancelled))
    {
        return fail("v-cancel-route-setup");
    }
    expected_x = (int32_t)(
        ((int64_t)ordinary.velocity_x_q16 *
         (int64_t)content->fighter.v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_y = (int32_t)(
        ((int64_t)ordinary.velocity_y_q16 *
         (int64_t)content->fighter.v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    if (ordinary.velocity_x_q16 <= INT32_C(0) ||
        ordinary.velocity_y_q16 >= INT32_C(0) ||
        age_zero.velocity_x_q16 != expected_x ||
        age_zero.velocity_y_q16 != expected_y ||
        age_one.velocity_x_q16 != expected_x ||
        age_one.velocity_y_q16 != expected_y ||
        age_two.velocity_x_q16 != expected_x ||
        age_two.velocity_y_q16 != expected_y ||
        age_zero.trigger_input_age != UINT8_C(0) ||
        age_one.trigger_input_age != UINT8_C(1) ||
        age_two.trigger_input_age != UINT8_C(2) ||
        age_zero.tech_lockout_ticks != UINT16_C(40) ||
        age_one.tech_lockout_ticks != UINT16_C(39) ||
        age_two.tech_lockout_ticks != UINT16_C(38))
    {
        return fail("v-cancel-three-tick-scaled-launch");
    }
    if (age_zero.hitstun_ticks != ordinary.hitstun_ticks ||
        age_one.hitstun_ticks != ordinary.hitstun_ticks ||
        age_two.hitstun_ticks != ordinary.hitstun_ticks ||
        age_zero.tumble != ordinary.tumble ||
        age_one.tumble != ordinary.tumble ||
        age_two.tumble != ordinary.tumble)
    {
        return fail("v-cancel-preserves-hitstun-and-tumble");
    }
    if (age_three.trigger_input_age != UINT8_C(3) ||
        age_three.velocity_x_q16 != ordinary.velocity_x_q16 ||
        age_three.velocity_y_q16 != ordinary.velocity_y_q16 ||
        attacking.trigger_input_age != UINT8_C(0) ||
        attacking.velocity_x_q16 != ordinary.velocity_x_q16 ||
        attacking.velocity_y_q16 != ordinary.velocity_y_q16 ||
        locked_out.trigger_input_age != UINT8_C(0) ||
        locked_out.tech_lockout_ticks >= UINT16_C(40) ||
        locked_out.velocity_x_q16 != ordinary.velocity_x_q16 ||
        locked_out.velocity_y_q16 != ordinary.velocity_y_q16 ||
        grounded_trigger.trigger_input_age != UINT8_C(0) ||
        grounded_trigger.velocity_x_q16 != grounded.velocity_x_q16 ||
        grounded_trigger.velocity_y_q16 != grounded.velocity_y_q16 ||
        jump_cancelled.trigger_input_age != UINT8_C(2) ||
        jump_cancelled.tech_lockout_ticks != UINT16_C(38) ||
        jump_cancelled.velocity_x_q16 !=
            (int32_t)(
                ((int64_t)jump_ordinary.velocity_x_q16 *
                 (int64_t)content->fighter
                     .v_cancel_velocity_scale_q16) /
                (int64_t)PF_Q16_ONE) ||
        jump_cancelled.velocity_y_q16 !=
            (int32_t)(
                ((int64_t)jump_ordinary.velocity_y_q16 *
                 (int64_t)content->fighter
                     .v_cancel_velocity_scale_q16) /
                (int64_t)PF_Q16_ONE) ||
        fall_special_cancelled.trigger_input_age != UINT8_C(0) ||
        fall_special_cancelled.tech_lockout_ticks != UINT16_C(40) ||
        fall_special_cancelled.velocity_x_q16 !=
            (int32_t)(
                ((int64_t)fall_special_ordinary.velocity_x_q16 *
                 (int64_t)content->fighter
                     .v_cancel_velocity_scale_q16) /
                (int64_t)PF_Q16_ONE) ||
        fall_special_cancelled.velocity_y_q16 !=
            (int32_t)(
                ((int64_t)fall_special_ordinary.velocity_y_q16 *
                 (int64_t)content->fighter
                     .v_cancel_velocity_scale_q16) /
                (int64_t)PF_Q16_ONE) ||
        jump_cancelled.hitstun_ticks != jump_ordinary.hitstun_ticks ||
        fall_special_cancelled.hitstun_ticks !=
            fall_special_ordinary.hitstun_ticks)
    {
        return fail("v-cancel-exclusions-and-boundaries");
    }
    return 1;
}

static int prepare_v_cancel_snapshot(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
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
        if (!step_reaction_duel(
                sim,
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
    return out_inspection->players[0].grounded == UINT8_C(0) &&
           out_inspection->players[1].grounded == UINT8_C(0) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           out_inspection->players[0].action_ticks + UINT16_C(1) ==
               content->fighter.aerial_startup_ticks &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_AIR_DODGE &&
           out_inspection->players[1].trigger_input_age == UINT8_C(1);
}

static int run_v_cancel_snapshot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_tick_result source_result;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !prepare_v_cancel_snapshot(
            source,
            content,
            &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-v-cancel-save-size") ||
        save_size != (size_t)694)
    {
        return fail("v-cancel-snapshot-setup");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-v-cancel") ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-v-cancel") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-v-cancel-source") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-v-cancel-loaded") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("v-cancel-snapshot-round-trip");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection))
        {
            return fail("v-cancel-source-continuation");
        }
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events[0]) *
                    (size_t)source_result.event_count) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-v-cancel-source-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-v-cancel-loaded-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("v-cancel-snapshot-continuation");
        }
        if (tick == UINT32_C(0) &&
            (source_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_HITLAG ||
             source_inspection.players[1].trigger_input_age !=
                 UINT8_C(2) ||
             source_result.event_count != UINT8_C(1) ||
             source_result.events[0].type !=
                 (uint16_t)PF_SIM_EVENT_HIT))
        {
            return fail("v-cancel-snapshot-hit");
        }
    }
    return 1;
}

typedef struct test_crouch_cancel_result
{
    pf_sim_event event;
    uint32_t damage_q16;
    uint16_t hitlag_ticks;
    uint16_t hitstun_ticks;
    uint8_t tumble;
} test_crouch_cancel_result;

static int start_crouch_cancel_hit(
    pf_sim *sim,
    int target_crouches,
    int target_holds_crouch,
    pf_m4_inspection *out_inspection,
    test_crouch_cancel_result *out_result)
{
    const int16_t setup_target_y =
        target_crouches != 0 ? INT16_MAX : INT16_C(0);
    const int16_t reaction_target_y =
        target_holds_crouch != 0 ? INT16_MAX : INT16_C(0);
    uint32_t tick;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            setup_target_y,
            UINT64_C(0),
            UINT16_C(0),
            out_inspection) ||
        (target_crouches != 0 &&
         out_inspection->players[1].action_state !=
             (uint8_t)PF_M4_ACTION_CROUCH) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            reaction_target_y,
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            out_result->event = *event;
            out_result->damage_q16 =
                out_inspection->players[1].damage_q16;
            out_result->hitlag_ticks =
                out_inspection->players[1].hitlag_ticks;
            out_result->hitstun_ticks =
                out_inspection->players[1].hitstun_ticks;
            out_result->tumble =
                out_inspection->players[1].tumble;
            return out_inspection->players[1].action_state ==
                   (uint8_t)PF_M4_ACTION_HITLAG;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                reaction_target_y,
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int run_crouch_cancel_case(
    const pf_content_view *view,
    int target_crouches,
    int target_holds_crouch,
    test_crouch_cancel_result *out_result)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    return initialize_sim(
               &storage,
               view,
               UINT8_C(2),
               PF_SIM_MODE_DUEL,
               1,
               &sim) &&
           start_crouch_cancel_hit(
               sim,
               target_crouches,
               target_holds_crouch,
               &inspection,
               out_result);
}

static int run_crouch_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_content invalid_velocity = *content;
    pf_m4_content invalid_hitstun = *content;
    pf_m4_content invalid_damage = *content;
    pf_m4_content exact_content = *content;
    pf_m4_content below_content = *content;
    pf_m4_content hash_content = *content;
    pf_content_view exact_view;
    pf_content_view below_view;
    pf_content_view hash_view;
    test_crouch_cancel_result ordinary;
    test_crouch_cancel_result crouched;
    test_crouch_cancel_result released;
    test_crouch_cancel_result exact;
    test_crouch_cancel_result below;
    test_crouch_cancel_result snapshot;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_tick_result source_result;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    int32_t expected_x;
    int32_t expected_y;
    uint32_t expected_hitstun;
    uint32_t tick;

    if (content->fighter.crouch_cancel_max_damage_q16 !=
            UINT32_C(40) * UINT32_C(65536) ||
        content->fighter.crouch_cancel_velocity_scale_q16 !=
            (INT32_C(2) * PF_Q16_ONE) / INT32_C(3) ||
        content->fighter.crouch_cancel_hitstun_scale_q16 !=
            (INT32_C(2) * PF_Q16_ONE) / INT32_C(3))
    {
        return fail("crouch-cancel-default-data");
    }
    invalid_velocity.fighter.crouch_cancel_velocity_scale_q16 =
        PF_Q16_ONE;
    invalid_hitstun.fighter.crouch_cancel_hitstun_scale_q16 =
        INT32_C(0);
    invalid_damage.fighter.crouch_cancel_max_damage_q16 =
        UINT32_C(0);
    hash_content.fighter.crouch_cancel_max_damage_q16 +=
        UINT32_C(1);
    exact_content.fighter.jab_damage_q16 =
        content->fighter.crouch_cancel_max_damage_q16;
    below_content.fighter.jab_damage_q16 =
        content->fighter.crouch_cancel_max_damage_q16;
    below_content.fighter.crouch_cancel_max_damage_q16 =
        content->fighter.crouch_cancel_max_damage_q16 - UINT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_velocity),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-crouch-cancel-velocity-scale") ||
        !expect_status(
            pf_m4_validate_content(&invalid_hitstun),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-crouch-cancel-hitstun-scale") ||
        !expect_status(
            pf_m4_validate_content(&invalid_damage),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-crouch-cancel-damage") ||
        !expect_status(
            pf_m4_make_content_view(&hash_content, &hash_view),
            PF_STATUS_OK,
            "crouch-cancel-hash-content-view") ||
        memcmp(
            view->content_hash.bytes,
            hash_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        !expect_status(
            pf_m4_make_content_view(&exact_content, &exact_view),
            PF_STATUS_OK,
            "crouch-cancel-exact-content-view") ||
        !expect_status(
            pf_m4_make_content_view(&below_content, &below_view),
            PF_STATUS_OK,
            "crouch-cancel-below-content-view") ||
        !run_crouch_cancel_case(view, 0, 0, &ordinary) ||
        !run_crouch_cancel_case(view, 1, 1, &crouched) ||
        !run_crouch_cancel_case(view, 1, 0, &released) ||
        !run_crouch_cancel_case(&exact_view, 1, 1, &exact) ||
        !run_crouch_cancel_case(&below_view, 1, 1, &below))
    {
        return fail("crouch-cancel-case-setup");
    }

    expected_x = (int32_t)(
        ((int64_t)ordinary.event.velocity_x_q16 *
         (int64_t)content->fighter
             .crouch_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_y = (int32_t)(
        ((int64_t)ordinary.event.velocity_y_q16 *
         (int64_t)content->fighter
             .crouch_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_hitstun =
        ((uint32_t)ordinary.hitstun_ticks *
         (uint32_t)content->fighter
             .crouch_cancel_hitstun_scale_q16) /
        (uint32_t)PF_Q16_ONE;
    if (ordinary.hitstun_ticks != UINT16_C(0) &&
        expected_hitstun == UINT32_C(0))
    {
        expected_hitstun = UINT32_C(1);
    }
    if ((ordinary.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) != UINT16_C(0) ||
        (crouched.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) == UINT16_C(0) ||
        crouched.event.value_q16 != ordinary.event.value_q16 ||
        crouched.damage_q16 != ordinary.damage_q16 ||
        crouched.hitlag_ticks != ordinary.hitlag_ticks ||
        crouched.event.velocity_x_q16 != expected_x ||
        crouched.event.velocity_y_q16 != expected_y ||
        crouched.hitstun_ticks != (uint16_t)expected_hitstun ||
        crouched.hitstun_ticks >= ordinary.hitstun_ticks ||
        crouched.tumble !=
            (uint8_t)(
                crouched.hitstun_ticks >=
                content->fighter.tumble_hitstun_threshold_ticks) ||
        (((crouched.event.flags &
           (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE) != UINT16_C(0)) !=
         (crouched.tumble != UINT8_C(0))))
    {
        return fail("crouch-cancel-scaled-reaction");
    }
    if ((released.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) != UINT16_C(0) ||
        released.event.velocity_x_q16 != ordinary.event.velocity_x_q16 ||
        released.event.velocity_y_q16 != ordinary.event.velocity_y_q16 ||
        released.hitstun_ticks != ordinary.hitstun_ticks)
    {
        return fail("crouch-cancel-released-down");
    }
    if ((exact.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) == UINT16_C(0) ||
        exact.damage_q16 !=
            content->fighter.crouch_cancel_max_damage_q16 ||
        (below.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) != UINT16_C(0) ||
        below.damage_q16 != exact.damage_q16 ||
        exact.hitlag_ticks != below.hitlag_ticks ||
        exact.event.velocity_x_q16 !=
            (int32_t)(
                ((int64_t)below.event.velocity_x_q16 *
                 (int64_t)content->fighter
                     .crouch_cancel_velocity_scale_q16) /
                (int64_t)PF_Q16_ONE) ||
        exact.event.velocity_y_q16 !=
            (int32_t)(
                ((int64_t)below.event.velocity_y_q16 *
                 (int64_t)content->fighter
                     .crouch_cancel_velocity_scale_q16) /
                (int64_t)PF_Q16_ONE) ||
        exact.hitstun_ticks !=
            (uint16_t)(
                ((uint32_t)below.hitstun_ticks *
                 (uint32_t)content->fighter
                     .crouch_cancel_hitstun_scale_q16) /
                (uint32_t)PF_Q16_ONE))
    {
        return fail("crouch-cancel-threshold-boundary");
    }

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !start_crouch_cancel_hit(
            source,
            1,
            1,
            &source_inspection,
            &snapshot) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-crouch-cancel-save-size") ||
        save_size != (size_t)694)
    {
        return fail("crouch-cancel-snapshot-setup");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-crouch-cancel") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-crouch-cancel") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-crouch-cancel-source") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-crouch-cancel-loaded") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("crouch-cancel-snapshot-round-trip");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection))
        {
            return fail("crouch-cancel-source-continuation");
        }
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events[0]) *
                    (size_t)source_result.event_count) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-crouch-cancel-source-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-crouch-cancel-loaded-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("crouch-cancel-snapshot-continuation");
        }
    }
    return 1;
}

static int launch_double_jump_counter_pair(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
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
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
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

static int start_double_jump_counter_weak_hit(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    const pf_sim_event *event;

    if (!launch_double_jump_counter_pair(sim, out_inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        out_inspection->players[0].action_ticks != UINT16_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    return event != NULL &&
           event->source_player == UINT8_C(0) &&
           event->target_player == UINT8_C(1);
}

static int start_double_jump_counter_late_hit(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    const pf_sim_event *event;
    uint32_t tick;

    if (!launch_double_jump_counter_pair(sim, out_inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter
                    .double_jump_cancel_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            out_inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    return event != NULL &&
           event->source_player == UINT8_C(0) &&
           event->target_player == UINT8_C(1);
}

static int start_double_jump_counter_strong_hit(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    const pf_sim_event *event;

    if (!launch_double_jump_counter_pair(sim, out_inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return 0;
    }
    while (out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK &&
           out_inspection->players[0].action_ticks <
               content->fighter.strong_startup_ticks &&
           out_inspection->players[1].damage_q16 == UINT32_C(0))
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[1].damage_q16 != UINT32_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            out_inspection))
    {
        return 0;
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    return event != NULL &&
           event->source_player == UINT8_C(0) &&
           event->target_player == UINT8_C(1);
}

static int run_double_jump_cancel_counter_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage exact_storage;
    test_sim_storage below_storage;
    test_sim_storage disabled_storage;
    test_sim_storage late_storage;
    test_sim_storage strong_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *exact = NULL;
    pf_sim *below = NULL;
    pf_sim *disabled = NULL;
    pf_sim *late = NULL;
    pf_sim *strong = NULL;
    pf_m4_content invalid_content = *content;
    pf_m4_content invalid_coupled_content = *content;
    pf_m4_content exact_content = *content;
    pf_m4_content below_content = *content;
    pf_m4_content disabled_content = *content;
    pf_content_view exact_view;
    pf_content_view below_view;
    pf_content_view disabled_view;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection exact_inspection;
    pf_m4_inspection below_inspection;
    pf_m4_inspection disabled_inspection;
    pf_m4_inspection late_inspection;
    pf_m4_inspection strong_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_tick_result source_result;
    pf_sim_event exact_event;
    pf_sim_event below_event;
    pf_sim_event disabled_event;
    pf_sim_event late_event;
    pf_sim_event strong_event;
    const pf_sim_event *event;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    int32_t frozen_position_x;
    int32_t frozen_position_y;
    int32_t frozen_velocity_x;
    int32_t frozen_velocity_y;
    uint32_t tick;

    if (content->fighter.double_jump_armor_max_hitstun_ticks !=
            UINT16_C(20))
    {
        return fail("double-jump-cancel-counter-default-data");
    }
    invalid_content.fighter.double_jump_armor_max_hitstun_ticks =
        UINT16_MAX;
    invalid_coupled_content.fighter.double_jump_cancel_ticks =
        UINT16_C(0);
    exact_content.fighter.double_jump_armor_max_hitstun_ticks =
        UINT16_C(16);
    below_content.fighter.double_jump_armor_max_hitstun_ticks =
        UINT16_C(15);
    disabled_content.fighter.double_jump_armor_max_hitstun_ticks =
        UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-double-jump-armor") ||
        !expect_status(
            pf_m4_validate_content(&invalid_coupled_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-armor-without-delayed-jump") ||
        !expect_status(
            pf_m4_make_content_view(&exact_content, &exact_view),
            PF_STATUS_OK,
            "exact-double-jump-armor-content") ||
        !expect_status(
            pf_m4_make_content_view(&below_content, &below_view),
            PF_STATUS_OK,
            "below-double-jump-armor-content") ||
        !expect_status(
            pf_m4_make_content_view(
                &disabled_content,
                &disabled_view),
            PF_STATUS_OK,
            "disabled-double-jump-armor-content") ||
        memcmp(
            view->content_hash.bytes,
            exact_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        memcmp(
            exact_view.content_hash.bytes,
            below_view.content_hash.bytes,
            sizeof(exact_view.content_hash.bytes)) == 0 ||
        memcmp(
            below_view.content_hash.bytes,
            disabled_view.content_hash.bytes,
            sizeof(below_view.content_hash.bytes)) == 0)
    {
        return fail("double-jump-cancel-counter-content-contract");
    }

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &exact_storage,
            &exact_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &exact) ||
        !initialize_sim(
            &below_storage,
            &below_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &below) ||
        !initialize_sim(
            &disabled_storage,
            &disabled_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &disabled) ||
        !initialize_sim(
            &late_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &late) ||
        !initialize_sim(
            &strong_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &strong))
    {
        return fail("double-jump-cancel-counter-initialize");
    }

    if (!start_double_jump_counter_weak_hit(
            source,
            &source_inspection))
    {
        return fail("double-jump-cancel-counter-weak-contact");
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (event == NULL)
    {
        return fail("double-jump-cancel-counter-event-missing");
    }
    frozen_position_x = source_inspection.players[1].position_x_q16;
    frozen_position_y = source_inspection.players[1].position_y_q16;
    frozen_velocity_x = source_inspection.players[1].velocity_x_q16;
    frozen_velocity_y = source_inspection.players[1].velocity_y_q16;
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        source_inspection.players[1].action_ticks != UINT16_C(0) ||
        source_inspection.players[1].hitlag_ticks !=
            content->fighter.aerial_hitlag_ticks ||
        source_inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        source_inspection.players[1].tumble != UINT8_C(0) ||
        source_inspection.players[1].damage_q16 !=
            content->fighter.aerial_damage_q16 ||
        source_inspection.players[1].air_jumps_remaining !=
            UINT8_C(0) ||
        frozen_velocity_y !=
            -content->fighter.double_jump_speed_q16 +
                content->fighter.gravity_q16 ||
        event->value_q16 != content->fighter.aerial_damage_q16 ||
        event->velocity_x_q16 != INT32_C(0) ||
        event->velocity_y_q16 != INT32_C(0) ||
        event->flags != UINT16_C(0) ||
        event->detail !=
            (uint16_t)PF_M4_ACTION_AERIAL_ATTACK)
    {
        return fail("double-jump-cancel-counter-armored-hit");
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "double-jump-cancel-counter-save-size") ||
        save_size != (size_t)694)
    {
        return fail("double-jump-cancel-counter-save-size-contract");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "double-jump-cancel-counter-save"))
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "double-jump-cancel-counter-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "double-jump-cancel-counter-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "double-jump-cancel-counter-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("double-jump-cancel-counter-save-load");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.aerial_hitlag_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return fail("double-jump-cancel-counter-source-hitlag");
        }
        source_result = test_last_result;
        if (!step_duel(
                loaded,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events[0]) *
                    (size_t)source_result.event_count) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "double-jump-cancel-counter-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "double-jump-cancel-counter-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash) ||
            source_inspection.players[1].position_x_q16 !=
                frozen_position_x ||
            source_inspection.players[1].position_y_q16 !=
                frozen_position_y ||
            source_inspection.players[1].velocity_x_q16 !=
                frozen_velocity_x ||
            source_inspection.players[1].velocity_y_q16 !=
                frozen_velocity_y)
        {
            return fail("double-jump-cancel-counter-hitlag-future");
        }
    }
    if (source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        source_inspection.players[1].action_ticks != UINT16_C(0) ||
        source_inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        source_inspection.players[1].velocity_y_q16 !=
            frozen_velocity_y)
    {
        return fail("double-jump-cancel-counter-resume");
    }

    if (!step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &source_inspection))
    {
        return fail("double-jump-cancel-counter-source-punish");
    }
    source_result = test_last_result;
    if (!step_duel(
            loaded,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &loaded_inspection) ||
        source_result.event_count != test_last_result.event_count ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "double-jump-cancel-counter-source-punish-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "double-jump-cancel-counter-loaded-punish-hash") ||
        !hash_equal(&source_hash, &loaded_hash) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        source_inspection.players[1].action_ticks != UINT16_C(0) ||
        source_inspection.players[1].velocity_y_q16 !=
            content->fighter.gravity_q16)
    {
        return fail("double-jump-cancel-counter-punish-entry");
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(4) &&
         source_inspection.players[0].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return fail("double-jump-cancel-counter-source-contact");
        }
        source_result = test_last_result;
        if (!step_duel(
                loaded,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events[0]) *
                    (size_t)source_result.event_count) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "double-jump-cancel-counter-source-contact-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "double-jump-cancel-counter-loaded-contact-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("double-jump-cancel-counter-contact-future");
        }
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (source_inspection.players[0].damage_q16 !=
            content->fighter.aerial_damage_q16 ||
        event == NULL ||
        event->source_player != UINT8_C(1) ||
        event->target_player != UINT8_C(0))
    {
        return fail("double-jump-cancel-counter-punish-hit");
    }

    if (!start_double_jump_counter_weak_hit(
            exact,
            &exact_inspection))
    {
        return fail("double-jump-cancel-counter-exact-contact");
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (event == NULL)
    {
        return 0;
    }
    exact_event = *event;
    if (!start_double_jump_counter_weak_hit(
            below,
            &below_inspection))
    {
        return fail("double-jump-cancel-counter-below-contact");
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (event == NULL)
    {
        return 0;
    }
    below_event = *event;
    if (!start_double_jump_counter_weak_hit(
            disabled,
            &disabled_inspection))
    {
        return fail("double-jump-cancel-counter-disabled-contact");
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (event == NULL)
    {
        return 0;
    }
    disabled_event = *event;
    if (exact_inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        exact_event.velocity_x_q16 != INT32_C(0) ||
        exact_event.velocity_y_q16 != INT32_C(0) ||
        below_inspection.players[1].hitstun_ticks != UINT16_C(16) ||
        below_event.velocity_x_q16 == INT32_C(0) ||
        below_event.velocity_y_q16 == INT32_C(0) ||
        disabled_inspection.players[1].hitstun_ticks !=
            UINT16_C(16) ||
        disabled_event.velocity_x_q16 == INT32_C(0) ||
        disabled_event.velocity_y_q16 == INT32_C(0))
    {
        return fail("double-jump-cancel-counter-threshold-boundary");
    }

    if (!start_double_jump_counter_late_hit(
            late,
            content,
            &late_inspection))
    {
        return fail("double-jump-cancel-counter-late-contact");
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (event == NULL)
    {
        return 0;
    }
    late_event = *event;
    if (late_inspection.players[1].hitstun_ticks != UINT16_C(16) ||
        late_event.velocity_x_q16 == INT32_C(0) ||
        late_event.velocity_y_q16 == INT32_C(0))
    {
        return fail("double-jump-cancel-counter-late-negative");
    }

    if (!start_double_jump_counter_strong_hit(
            strong,
            content,
            &strong_inspection))
    {
        return fail("double-jump-cancel-counter-strong-contact");
    }
    event = find_last_tick_event(PF_SIM_EVENT_HIT);
    if (event == NULL)
    {
        return 0;
    }
    strong_event = *event;
    if (strong_inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        strong_inspection.players[1].hitstun_ticks != UINT16_C(34) ||
        strong_inspection.players[1].tumble != UINT8_C(1) ||
        strong_event.velocity_x_q16 == INT32_C(0) ||
        strong_event.velocity_y_q16 == INT32_C(0) ||
        (strong_event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE) == UINT16_C(0))
    {
        return fail("double-jump-cancel-counter-strong-negative");
    }

    return 1;
}

static int run_aerial_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t frozen_x[2];
    int32_t frozen_y[2];
    uint32_t tick;
    uint32_t hit_sequence;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection))
    {
        return fail("aerial-hit-jump-start");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("aerial-hit-jump-squat");
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
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
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("aerial-hit-attack-start");
    }

    for (tick = UINT32_C(0);
         tick <
                 (uint32_t)content->fighter.aerial_startup_ticks +
                     (uint32_t)content->fighter.aerial_active_ticks +
                     UINT32_C(2) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("aerial-hit-active-schedule");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].action_ticks !=
            content->fighter.aerial_startup_ticks ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.aerial_hitlag_ticks ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.aerial_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.aerial_damage_q16 ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.aerial_damage_q16 ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[1].last_hit_valid != UINT8_C(1) ||
        (inspection.players[0].attack_hit_mask & UINT8_C(2)) ==
            UINT8_C(0))
    {
        return fail("aerial-hit-damage-hitlag-and-event");
    }

    hit_sequence = inspection.players[1].last_hit_sequence;
    frozen_x[0] = inspection.players[0].position_x_q16;
    frozen_x[1] = inspection.players[1].position_x_q16;
    frozen_y[0] = inspection.players[0].position_y_q16;
    frozen_y[1] = inspection.players[1].position_y_q16;
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.aerial_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].position_x_q16 != frozen_x[0] ||
            inspection.players[1].position_x_q16 != frozen_x[1] ||
            inspection.players[0].position_y_q16 != frozen_y[0] ||
            inspection.players[1].position_y_q16 != frozen_y[1])
        {
            return fail("aerial-hitlag-freeze");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[1].velocity_y_q16 >= INT32_C(0))
    {
        return fail("aerial-hitlag-resume");
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.aerial_active_ticks +
                 UINT32_C(2);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[1].damage_q16 !=
                content->fighter.aerial_damage_q16 ||
            inspection.players[1].last_hit_sequence !=
                hit_sequence)
        {
            return fail("aerial-single-hit-per-target");
        }
    }
    return 1;
}

static int run_strong_aerial_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection))
    {
        return fail("strong-aerial-hit-jump-start");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[1].grounded != UINT8_C(0));
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("strong-aerial-hit-jump-squat");
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("strong-aerial-hit-attack-start");
    }

    for (tick = UINT32_C(0);
         tick <
                 (uint32_t)content->fighter.strong_startup_ticks +
                     (uint32_t)content->fighter.strong_active_ticks +
                     UINT32_C(2) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("strong-aerial-hit-active-schedule");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[1].last_hit_valid != UINT8_C(1) ||
        (inspection.players[0].attack_hit_mask & UINT8_C(2)) ==
            UINT8_C(0))
    {
        return fail("strong-aerial-hit-damage-hitlag-and-event");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.strong_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("strong-aerial-hitlag-step");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_AERIAL_ATTACK ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN)
    {
        return fail("strong-aerial-hitlag-resume");
    }
    return 1;
}

static int run_default_strong_tumble_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;
    int saw_tumble_hitstun = 0;
    int saw_knockdown = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("strong-attack-startup");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.strong_startup_ticks +
                    (uint32_t)content->fighter.strong_active_ticks +
                    UINT32_C(2) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("strong-attack-active-schedule");
        }
    }

    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].hitlag_ticks !=
            content->fighter.strong_hitlag_ticks ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].hitstun_ticks <
            content->fighter.tumble_hitstun_threshold_ticks ||
        inspection.players[1].tumble != UINT8_C(1) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        (test_last_result.events[0].flags &
         (uint16_t)PF_SIM_EVENT_FLAG_TUMBLE) == UINT16_C(0) ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_STRONG_ATTACK)
    {
        return fail("default-strong-attack-enters-tumble");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.strong_hitlag_ticks;
         ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("strong-hitlag-resume");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].tumble != UINT8_C(1))
    {
        return fail("strong-tumble-launch");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        if (inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_HITSTUN &&
            inspection.players[1].tumble != UINT8_C(0))
        {
            saw_tumble_hitstun = 1;
        }
        if (inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_KNOCKDOWN)
        {
            saw_knockdown = 1;
            break;
        }
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("strong-tumble-landing");
        }
    }
    return (saw_tumble_hitstun != 0 && saw_knockdown != 0) ||
           fail("default-strong-tumble-route");
}

static int run_small_step_forward_smash_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage standing_storage;
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage negative_storage;
    pf_m4_content invalid_content = *content;
    pf_sim *standing = NULL;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *negative = NULL;
    pf_m4_inspection standing_inspection;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection negative_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t standing_x;
    uint32_t attack_ticks;
    uint32_t tick;

    if (content->fighter.forward_smash_input_window_ticks !=
        UINT16_C(3))
    {
        return fail("small-step-forward-smash-default-window");
    }
    invalid_content.fighter.forward_smash_input_window_ticks =
        UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-forward-smash-window"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.forward_smash_input_window_ticks =
        (uint16_t)(
            invalid_content.fighter.initial_dash_ticks +
            UINT16_C(1));
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-forward-smash-window-after-initial-dash") ||
        !initialize_sim(
            &standing_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &standing) ||
        !initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &negative_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &negative) ||
        !expect_status(
            pf_m4_inspect(standing, &standing_inspection),
            PF_STATUS_OK,
            "small-step-standing-inspect"))
    {
        return 0;
    }

    standing_x = standing_inspection.players[0].position_x_q16;
    if (!step_duel(
            standing,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &standing_inspection) ||
        standing_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        standing_inspection.players[0].action_ticks != UINT16_C(1) ||
        standing_inspection.players[0].facing != INT8_C(1) ||
        standing_inspection.players[0].position_x_q16 != standing_x)
    {
        return fail("small-step-standing-forward-smash-entry");
    }
    attack_ticks =
        (uint32_t)content->fighter.strong_startup_ticks +
        (uint32_t)content->fighter.strong_active_ticks +
        (uint32_t)content->fighter.strong_recovery_ticks;
    for (tick = UINT32_C(1); tick <= attack_ticks; ++tick)
    {
        if (!step_duel(
                standing,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &standing_inspection))
        {
            return fail("small-step-standing-forward-smash-step");
        }
    }
    if (standing_inspection.players[1].damage_q16 != UINT32_C(0) ||
        standing_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("small-step-standing-forward-smash-range");
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter
                 .forward_smash_input_window_ticks;
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection))
        {
            return fail("small-step-forward-smash-dash");
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        source_inspection.players[0].action_ticks !=
            content->fighter.forward_smash_input_window_ticks ||
        source_inspection.players[0].dash_direction != INT8_C(1) ||
        source_inspection.players[0].position_x_q16 <= standing_x ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "small-step-forward-smash-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("small-step-forward-smash-window-boundary");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "small-step-forward-smash-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "small-step-forward-smash-load") ||
        !step_duel(
            source,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            loaded,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &loaded_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].position_x_q16 <= standing_x ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "small-step-forward-smash-source-entry-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "small-step-forward-smash-loaded-entry-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("small-step-forward-smash-entry");
    }
    for (tick = UINT32_C(0);
         tick <
                 (uint32_t)content->fighter.strong_startup_ticks +
                     (uint32_t)content->fighter.strong_active_ticks +
                     UINT32_C(2) &&
         source_inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "small-step-forward-smash-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "small-step-forward-smash-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("small-step-forward-smash-continuation");
        }
    }
    if (source_inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        loaded_inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        source_inspection.players[1].last_hit_damage_q16 !=
            content->fighter.strong_damage_q16 ||
        source_inspection.players[0].position_x_q16 <= standing_x)
    {
        return fail("small-step-forward-smash-extended-range-hit");
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter
                     .forward_smash_input_window_ticks +
                 UINT32_C(1);
         ++tick)
    {
        if (!step_duel(
                negative,
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &negative_inspection))
        {
            return fail("small-step-forward-smash-late-setup");
        }
    }
    if (!step_duel(
            negative,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return fail("small-step-forward-smash-late-negative");
    }
    if (!expect_status(
            pf_sim_reset(negative, UINT64_C(0x5f5f5f48)),
            PF_STATUS_OK,
            "small-step-forward-smash-pivot-reset") ||
        !step_duel(
            negative,
            INT16_MAX,
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        negative_inspection.players[0].action_ticks != UINT16_C(1) ||
        !step_duel(
            negative,
            (int16_t)-INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        negative_inspection.players[0].facing != INT8_C(-1))
    {
        return fail("small-step-forward-smash-pivot-window");
    }
    if (!expect_status(
            pf_sim_reset(negative, UINT64_C(0x5f5f5f49)),
            PF_STATUS_OK,
            "small-step-forward-smash-late-pivot-reset") ||
        !step_duel(
            negative,
            INT16_MAX,
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        !step_duel(
            negative,
            INT16_MAX,
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        !step_duel(
            negative,
            (int16_t)-INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return fail("small-step-forward-smash-late-pivot-negative");
    }
    if (!expect_status(
            pf_sim_reset(negative, UINT64_C(0x5f5f5f47)),
            PF_STATUS_OK,
            "small-step-forward-smash-direction-reset"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter
                 .forward_smash_input_window_ticks;
         ++tick)
    {
        if (!step_duel(
                negative,
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &negative_inspection))
        {
            return fail("small-step-forward-smash-direction-setup");
        }
    }
    if (!step_duel(
            negative,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return fail("small-step-forward-smash-direction-negative");
    }
    return 1;
}

static int run_drop_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view,
    const pf_content_view *whiff_view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage late_storage;
    test_sim_storage whiff_storage;
    pf_m4_content invalid_content = *content;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *late = NULL;
    pf_sim *whiff = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection late_inspection;
    pf_m4_inspection whiff_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t landing_ticks = UINT32_C(0);
    uint32_t tick;
    int saw_hit = 0;
    int saw_platform_snap = 0;
    int saw_landing = 0;
    int saw_late_hit = 0;
    int saw_late_platform_snap = 0;
    int saw_whiff_hitlag = 0;
    int saw_whiff_platform_snap = 0;

    if (content->fighter.platform_drop_ticks != UINT16_C(9) ||
        content->fighter.drop_cancel_snap_distance_q16 !=
            (INT32_C(5) * PF_Q16_ONE) / INT32_C(8))
    {
        return fail("drop-cancel-default-data");
    }
    invalid_content.fighter.drop_cancel_snap_distance_q16 =
        invalid_content.fighter.platform_drop_nudge_q16;
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-short-drop-cancel-snap"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.drop_cancel_snap_distance_q16 =
        invalid_content.fighter.half_height_q16 + INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-drop-cancel-snap"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.platform_drop_ticks =
        (uint16_t)(
            invalid_content.fighter.aerial_startup_ticks +
            UINT16_C(1));
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-short-drop-cancel-timer"))
    {
        return 0;
    }
    invalid_content = *content;
    invalid_content.fighter.platform_drop_ticks =
        (uint16_t)(
            invalid_content.fighter.aerial_startup_ticks +
            invalid_content.fighter.aerial_hitlag_ticks +
            UINT16_C(2));
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-long-drop-cancel-timer") ||
        !initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &late_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &late) ||
        !initialize_sim(
            &whiff_storage,
            whiff_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &whiff) ||
        !prepare_drop_cancel_platform(source, &source_inspection))
    {
        return fail("drop-cancel-setup");
    }

    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].grounded != UINT8_C(0) ||
        source_inspection.players[0].platform_drop_ticks != UINT8_C(9) ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].platform_drop_ticks != UINT8_C(8) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "drop-cancel-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("drop-cancel-first-airborne-frame");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "drop-cancel-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "drop-cancel-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "drop-cancel-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "drop-cancel-loaded-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("drop-cancel-save-load-continuation");
        }
        if (source_inspection.players[1].damage_q16 ==
            content->fighter.aerial_damage_q16)
        {
            saw_hit = 1;
        }
        if (source_inspection.players[0].grounded != UINT8_C(0) &&
            source_inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            if (source_inspection.players[0].position_y_q16 !=
                source_inspection.stage.platform_y_q16 -
                    content->fighter.half_height_q16)
            {
                return fail("drop-cancel-platform-snap-position");
            }
            saw_platform_snap = 1;
        }
        if (saw_platform_snap != 0 &&
            source_inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AERIAL_LANDING)
        {
            saw_landing = 1;
            break;
        }
    }
    if (saw_hit == 0 ||
        saw_platform_snap == 0 ||
        saw_landing == 0 ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].platform_drop_ticks != UINT8_C(0) ||
        loaded_inspection.players[1].damage_q16 !=
            content->fighter.aerial_damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=drop-cancel-hitlag-platform-return"
            " hit=%d snap=%d landing=%d action=%u action_ticks=%u"
            " drop_ticks=%u grounded=%u support=%u damage=%" PRIu32
            " y=%" PRId32 " platform_y=%" PRId32 "\n",
            saw_hit,
            saw_platform_snap,
            saw_landing,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)
                source_inspection.players[0].platform_drop_ticks,
            (unsigned int)source_inspection.players[0].grounded,
            (unsigned int)source_inspection.players[0].support,
            source_inspection.players[1].damage_q16,
            source_inspection.players[0].position_y_q16,
            source_inspection.stage.platform_y_q16);
        return 0;
    }

    while (source_inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_LANDING &&
           landing_ticks < UINT32_C(30))
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "drop-cancel-landing-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "drop-cancel-landing-loaded-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("drop-cancel-landing-continuation");
        }
        ++landing_ticks;
    }
    if (landing_ticks !=
            (uint32_t)content->fighter.aerial_landing_lag_ticks ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        source_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_PLATFORM)
    {
        return fail("drop-cancel-exact-landing-lag");
    }

    if (!prepare_drop_cancel_platform(late, &late_inspection) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &late_inspection) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &late_inspection) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &late_inspection) ||
        late_inspection.players[0].platform_drop_ticks != UINT8_C(7))
    {
        return fail("drop-cancel-late-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_reaction_duel(
                late,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &late_inspection))
        {
            return fail("drop-cancel-late-step");
        }
        if (late_inspection.players[1].damage_q16 ==
            content->fighter.aerial_damage_q16)
        {
            saw_late_hit = 1;
        }
        if (late_inspection.players[0].grounded != UINT8_C(0) &&
            late_inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            saw_late_platform_snap = 1;
        }
        if (saw_late_hit != 0 &&
            late_inspection.players[0].hitlag_ticks == UINT16_C(0) &&
            late_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG)
        {
            break;
        }
    }
    if (saw_late_hit == 0 ||
        saw_late_platform_snap != 0 ||
        late_inspection.players[0].grounded != UINT8_C(0) ||
        late_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_NONE)
    {
        return fail("drop-cancel-one-tick-late-negative");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         late_inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                late,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &late_inspection))
        {
            return fail("drop-cancel-late-floor-step");
        }
    }
    if (late_inspection.players[0].support !=
        (uint8_t)PF_M4_SURFACE_FLOOR)
    {
        return fail("drop-cancel-late-falls-through");
    }

    if (!prepare_drop_cancel_platform(whiff, &whiff_inspection) ||
        !step_reaction_duel(
            whiff,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &whiff_inspection) ||
        !step_reaction_duel(
            whiff,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &whiff_inspection))
    {
        return fail("drop-cancel-whiff-setup");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         whiff_inspection.players[0].grounded == UINT8_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                whiff,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &whiff_inspection))
        {
            return fail("drop-cancel-whiff-step");
        }
        if (whiff_inspection.players[0].hitlag_ticks > UINT16_C(0))
        {
            saw_whiff_hitlag = 1;
        }
        if (whiff_inspection.players[0].grounded != UINT8_C(0) &&
            whiff_inspection.players[0].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            saw_whiff_platform_snap = 1;
        }
    }
    if (whiff_inspection.players[1].damage_q16 != UINT32_C(0) ||
        saw_whiff_hitlag != 0 ||
        saw_whiff_platform_snap != 0 ||
        whiff_inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR)
    {
        return fail("drop-cancel-whiff-negative");
    }
    return 1;
}

static int prepare_sharking_target(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
        if (out_inspection->players[1].grounded != UINT8_C(0) &&
            out_inspection->players[1].support ==
                (uint8_t)PF_M4_SURFACE_PLATFORM)
        {
            break;
        }
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(20) &&
         out_inspection->players[1].action_state !=
             (uint8_t)PF_M4_ACTION_GROUND_IDLE;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
    return out_inspection->players[0].grounded != UINT8_C(0) &&
           out_inspection->players[0].support ==
               (uint8_t)PF_M4_SURFACE_FLOOR &&
           out_inspection->players[1].grounded != UINT8_C(0) &&
           out_inspection->players[1].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE;
}

static int start_sharking_aerial(
    pf_sim *sim,
    int early_attack,
    uint16_t target_trigger,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
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
            if (!step_reaction_duel(
                    sim,
                    INT16_C(0),
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
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               target_trigger,
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           out_inspection->players[0].position_y_q16 >
               out_inspection->stage.platform_y_q16 &&
           out_inspection->players[1].support ==
               (uint8_t)PF_M4_SURFACE_PLATFORM;
}

static int run_sharking_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage early_storage;
    test_sim_storage shield_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *early = NULL;
    pf_sim *shield = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection early_inspection;
    pf_m4_inspection shield_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;
    int saw_platform_shark_hit = 0;
    int saw_early_hitbox = 0;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !prepare_sharking_target(source, &source_inspection) ||
        !start_sharking_aerial(
            source,
            0,
            UINT16_C(0),
            &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "sharking-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("sharking-hit-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "sharking-save-mid-aerial") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "sharking-load-mid-aerial") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "sharking-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "sharking-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("sharking-mid-aerial-round-trip");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "sharking-source-continuation-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "sharking-loaded-continuation-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("sharking-deterministic-continuation");
        }
        if (source_inspection.players[1].damage_q16 ==
            content->fighter.aerial_damage_q16)
        {
            if (source_inspection.players[1].last_hit_attacker !=
                    UINT8_C(0))
            {
                return fail("sharking-hit-origin");
            }
            saw_platform_shark_hit = 1;
        }
    }
    if (saw_platform_shark_hit == 0)
    {
        return fail("sharking-aerial-did-not-hit");
    }

    if (!initialize_sim(
            &early_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &early) ||
        !prepare_sharking_target(early, &early_inspection) ||
        !start_sharking_aerial(
            early,
            1,
            UINT16_C(0),
            &early_inspection))
    {
        return fail("sharking-early-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
    {
        if (!step_reaction_duel(
                early,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &early_inspection))
        {
            return fail("sharking-early-step");
        }
        if (early_inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_early_hitbox = 1;
        }
    }
    if (saw_early_hitbox == 0 ||
        early_inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("sharking-early-whiff");
    }

    if (!initialize_sim(
            &shield_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &shield) ||
        !prepare_sharking_target(shield, &shield_inspection) ||
        !step_reaction_duel(
            shield,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &shield_inspection) ||
        !start_sharking_aerial(
            shield,
            0,
            UINT16_MAX,
            &shield_inspection))
    {
        return fail("sharking-shield-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!step_reaction_duel(
                shield,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &shield_inspection))
        {
            return fail("sharking-shield-step");
        }
        if (shield_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_HITLAG)
        {
            break;
        }
    }
    return (tick < UINT32_C(20) &&
            shield_inspection.players[1].damage_q16 == UINT32_C(0) &&
            shield_inspection.players[1].shield_health_q16 <
                content->fighter.shield_health_q16 &&
            shield_inspection.players[1].powershield == UINT8_C(0) &&
            test_last_result.event_count == UINT8_C(1) &&
            test_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK) ||
           fail("sharking-shield-pressure");
}

static int cross_up_steering_axis(
    const pf_m4_inspection *inspection)
{
    return inspection->players[0].position_x_q16 <=
                   inspection->players[1].position_x_q16 -
                       PF_Q16_ONE / INT32_C(2)
               ? INT16_C(32767)
               : INT16_C(-32767);
}

static int prepare_cross_up_aerial(
    pf_sim *sim,
    int back_aerial,
    int early_attack,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (back_aerial != 0)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(-13500),
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
        for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
        {
            if (!step_reaction_duel(
                    sim,
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
            if (out_inspection->players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                out_inspection->players[0].velocity_x_q16 == INT32_C(0))
            {
                break;
            }
        }
        if (tick == UINT32_C(20) ||
            out_inspection->players[0].facing != INT8_C(-1))
        {
            return 0;
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
        if (!step_reaction_duel(
                sim,
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
            (back_aerial != 0 ? INT8_C(-1) : INT8_C(1)) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD)
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
            if (!step_reaction_duel(
                    sim,
                    back_aerial != 0
                        ? (int16_t)cross_up_steering_axis(out_inspection)
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
    return step_reaction_duel(
               sim,
               back_aerial != 0
                   ? (int16_t)cross_up_steering_axis(out_inspection)
                   : INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           (back_aerial == 0 || early_attack != 0 ||
            out_inspection->players[0].position_x_q16 >
                out_inspection->players[1].position_x_q16);
}

static int run_cross_up_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage early_storage;
    test_sim_storage front_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *early = NULL;
    pf_sim *front = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection early_inspection;
    pf_m4_inspection front_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;
    int saw_cross_block = 0;
    int saw_cross_hitbox = 0;
    int saw_early_hitbox = 0;
    int saw_early_block = 0;
    int saw_front_block = 0;
    int saw_front_hitbox = 0;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !prepare_cross_up_aerial(
            source,
            1,
            0,
            &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "cross-up-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("cross-up-setup");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "cross-up-save-mid-aerial") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "cross-up-load-mid-aerial") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "cross-up-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "cross-up-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("cross-up-mid-aerial-round-trip");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
    {
        const int16_t axis =
            source_inspection.players[0].grounded == UINT8_C(0)
                ? (int16_t)cross_up_steering_axis(&source_inspection)
                : INT16_C(0);

        if (!step_reaction_duel(
                source,
                axis,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                axis,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "cross-up-source-continuation-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "cross-up-loaded-continuation-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("cross-up-deterministic-continuation");
        }
        if (source_inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_cross_hitbox = 1;
        }
        if (test_last_result.event_count == UINT8_C(1) &&
            test_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK)
        {
            saw_cross_block = 1;
        }
    }
    if (saw_cross_hitbox == 0 || saw_cross_block == 0 ||
        source_inspection.players[0].position_x_q16 <=
            source_inspection.players[1].position_x_q16 ||
        source_inspection.players[0].facing != INT8_C(-1) ||
        source_inspection.players[1].facing != INT8_C(-1) ||
        source_inspection.players[1].damage_q16 != UINT32_C(0) ||
        source_inspection.players[1].shield_health_q16 >=
            content->fighter.shield_health_q16)
    {
        return fail("cross-up-behind-shield");
    }

    if (!initialize_sim(
            &early_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &early) ||
        !prepare_cross_up_aerial(
            early,
            1,
            1,
            &early_inspection))
    {
        return fail("cross-up-early-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
    {
        if (!step_reaction_duel(
                early,
                early_inspection.players[0].grounded == UINT8_C(0)
                    ? INT16_C(32767)
                    : INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &early_inspection))
        {
            return fail("cross-up-early-step");
        }
        if (early_inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_early_hitbox = 1;
        }
        if (test_last_result.event_count != UINT8_C(0))
        {
            saw_early_block = 1;
        }
    }
    if (saw_early_hitbox == 0 || saw_early_block != 0 ||
        early_inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("cross-up-early-whiff");
    }

    if (!initialize_sim(
            &front_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &front) ||
        !prepare_cross_up_aerial(
            front,
            0,
            0,
            &front_inspection))
    {
        return fail("cross-up-front-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
    {
        if (!step_reaction_duel(
                front,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &front_inspection))
        {
            return fail("cross-up-front-step");
        }
        if (front_inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_front_hitbox = 1;
        }
        if (test_last_result.event_count == UINT8_C(1) &&
            test_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK)
        {
            saw_front_block = 1;
        }
    }
    if (saw_front_hitbox == 0 || saw_front_block == 0 ||
        front_inspection.players[0].position_x_q16 >=
            front_inspection.players[1].position_x_q16 ||
        front_inspection.players[0].facing != INT8_C(1) ||
        front_inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("cross-up-front-control");
    }
    return 1;
}

static int16_t juggling_chase_axis(
    const pf_m4_inspection *inspection)
{
    const int32_t delta =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;

    if (delta >
        (INT32_C(5) * PF_Q16_ONE) / INT32_C(4))
    {
        return INT16_C(13500);
    }
    if (delta <
        -(INT32_C(5) * PF_Q16_ONE) / INT32_C(4))
    {
        return INT16_C(-13500);
    }
    return INT16_C(0);
}

static int run_juggling_route(
    const pf_m4_content *content,
    const pf_content_view *view,
    int escape_route)
{
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t launcher_sequence = UINT32_C(0);
    uint32_t tick;
    uint32_t jump_hold_ticks = UINT32_C(0);
    int launched_airborne = 0;
    int jump_started = 0;
    int aerial_started = 0;
    int saw_followup_hitbox = 0;
    int air_dodge_started = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        (escape_route == 0 &&
         !initialize_sim(
             &loaded_storage,
             view,
             UINT8_C(2),
             PF_SIM_MODE_DUEL,
             0,
             &loaded)) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("juggling-launcher-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("juggling-launcher-step");
        }
        if (inspection.players[1].damage_q16 ==
            content->fighter.strong_damage_q16)
        {
            launcher_sequence =
                inspection.players[1].last_hit_sequence;
            break;
        }
    }
    if (tick == UINT32_C(24) || launcher_sequence == UINT32_C(0))
    {
        return fail("juggling-launcher-hit");
    }
    if (escape_route == 0)
    {
        destination.bytes = save_bytes;
        destination.capacity = sizeof(save_bytes);
        destination.size = (size_t)0;
        if (!expect_status(
                pf_sim_query_save_size(sim, &save_size),
                PF_STATUS_OK,
                "juggling-query-save-size") ||
            save_size != (size_t)694 ||
            !expect_status(
                pf_sim_save(sim, &destination),
                PF_STATUS_OK,
                "juggling-save-mid-launch") ||
            destination.size != save_size)
        {
            return fail("juggling-save-setup");
        }
        save.bytes = save_bytes;
        save.size = save_size;
        if (!expect_status(
                pf_sim_load(loaded, save),
                PF_STATUS_OK,
                "juggling-load-mid-launch") ||
            !expect_status(
                pf_sim_hash(sim, &source_hash),
                PF_STATUS_OK,
                "juggling-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "juggling-loaded-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("juggling-mid-launch-round-trip");
        }
    }

    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        int16_t attacker_x = juggling_chase_axis(&inspection);
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
            break;
        }

        if (escape_route != 0)
        {
            target_x = INT16_C(32767);
            target_y = INT16_C(-32767);
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

        if (!step_reaction_duel(
                sim,
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
            return fail("juggling-followup-step");
        }
        if (escape_route == 0 &&
            (!step_reaction_duel(
                 loaded,
                 attacker_x,
                 INT16_C(0),
                 attacker_buttons,
                 UINT16_C(0),
                 target_x,
                 target_y,
                 UINT64_C(0),
                 target_trigger,
                 &loaded_inspection) ||
             !expect_status(
                 pf_sim_hash(sim, &source_hash),
                 PF_STATUS_OK,
                 "juggling-source-continuation-hash") ||
             !expect_status(
                 pf_sim_hash(loaded, &loaded_hash),
                 PF_STATUS_OK,
                 "juggling-loaded-continuation-hash") ||
             !hash_equal(&source_hash, &loaded_hash)))
        {
            return fail("juggling-deterministic-continuation");
        }
        if (aerial_started != 0 &&
            inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_followup_hitbox = 1;
        }
        if (inspection.players[1].last_hit_sequence !=
                launcher_sequence &&
            inspection.players[1].damage_q16 ==
                content->fighter.strong_damage_q16 +
                    content->fighter.aerial_damage_q16)
        {
            return (escape_route == 0 && launched_airborne != 0 &&
                    inspection.players[1].grounded == UINT8_C(0)) ||
                   fail("juggling-escape-was-hit");
        }
    }
    if (escape_route != 0 && launched_airborne != 0 &&
        air_dodge_started != 0 && saw_followup_hitbox != 0 &&
        inspection.players[1].damage_q16 ==
            content->fighter.strong_damage_q16 &&
        inspection.players[1].last_hit_sequence == launcher_sequence)
    {
        return 1;
    }
    return fail(
        escape_route != 0
            ? "juggling-air-dodge-escape"
            : "juggling-airborne-followup");
}

static int run_juggling_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    return run_juggling_route(content, view, 0) &&
           run_juggling_route(content, view, 1);
}

static int wait_for_kill_confirm_neutral(
    pf_sim *sim,
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
        if (!step_reaction_duel(
                sim,
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
    if (tick == UINT32_C(160))
    {
        return fail("kill-confirm-neutral-timeout");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(320); ++tick)
    {
        const int32_t gap =
            out_inspection->players[1].position_x_q16 -
            out_inspection->players[0].position_x_q16;
        const int16_t attacker_axis =
            gap > (INT32_C(3) * PF_Q16_ONE) / INT32_C(2)
                ? INT16_C(13500)
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
        if (!step_reaction_duel(
                sim,
                attacker_axis,
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
    return fail("kill-confirm-spacing-timeout");
}

static int run_kill_confirm_route(
    const pf_m4_content *content,
    const pf_content_view *view,
    uint32_t buildup_jabs,
    int16_t target_di_x,
    int16_t target_di_y,
    int expect_ko,
    int verify_save_load)
{
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_tick_result source_result;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t setup_sequence;
    uint32_t tick;
    uint32_t jab_index;
    int strong_started = 0;
    int finisher_hit = 0;
    int defender_escaped = 0;
    int saw_strong_hitbox = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        (verify_save_load != 0 &&
         !initialize_sim(
             &loaded_storage,
             view,
             UINT8_C(2),
             PF_SIM_MODE_DUEL,
             0,
             &loaded)) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "kill-confirm-initial-inspect"))
    {
        return fail("kill-confirm-initialize");
    }

    for (jab_index = UINT32_C(0);
         jab_index < buildup_jabs;
         ++jab_index)
    {
        const uint32_t previous_sequence =
            inspection.players[1].last_hit_sequence;
        const uint32_t expected_damage =
            (jab_index + UINT32_C(1)) *
            content->fighter.jab_damage_q16;

        if (!wait_for_kill_confirm_neutral(sim, &inspection) ||
            !step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("kill-confirm-buildup-start");
        }
        for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
        {
            if (inspection.players[1].last_hit_sequence !=
                previous_sequence)
            {
                break;
            }
            if (!step_reaction_duel(
                    sim,
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
                return fail("kill-confirm-buildup-step");
            }
        }
        if (tick == UINT32_C(32) ||
            inspection.players[1].damage_q16 != expected_damage)
        {
            return fail("kill-confirm-buildup-hit");
        }
    }

    if (!wait_for_kill_confirm_neutral(sim, &inspection))
    {
        return 0;
    }
    setup_sequence = inspection.players[1].last_hit_sequence;
    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("kill-confirm-setup-start");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (inspection.players[1].last_hit_sequence != setup_sequence)
        {
            setup_sequence =
                inspection.players[1].last_hit_sequence;
            break;
        }
        if (!step_reaction_duel(
                sim,
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
            return fail("kill-confirm-setup-step");
        }
    }
    if (tick == UINT32_C(32) || setup_sequence == UINT32_C(0) ||
        inspection.players[1].damage_q16 !=
            (buildup_jabs + UINT32_C(1)) *
                content->fighter.jab_damage_q16)
    {
        return fail("kill-confirm-setup-hit");
    }

    if (verify_save_load != 0)
    {
        destination.bytes = save_bytes;
        destination.capacity = sizeof(save_bytes);
        destination.size = (size_t)0;
        if (!expect_status(
                pf_sim_query_save_size(sim, &save_size),
                PF_STATUS_OK,
                "kill-confirm-query-save-size") ||
            save_size != (size_t)694 ||
            !expect_status(
                pf_sim_save(sim, &destination),
                PF_STATUS_OK,
                "kill-confirm-save-after-setup") ||
            destination.size != save_size)
        {
            return fail("kill-confirm-save-setup");
        }
        save.bytes = save_bytes;
        save.size = save_size;
        if (!expect_status(
                pf_sim_load(loaded, save),
                PF_STATUS_OK,
                "kill-confirm-load-after-setup") ||
            !expect_status(
                pf_sim_hash(sim, &source_hash),
                PF_STATUS_OK,
                "kill-confirm-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "kill-confirm-loaded-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("kill-confirm-setup-round-trip");
        }
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
                return fail("kill-confirm-defender-action-window");
            }
            defender_escaped = 1;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                target_di_x,
                target_di_y,
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("kill-confirm-finisher-step");
        }
        source_result = test_last_result;
        if (verify_save_load != 0 &&
            (!step_reaction_duel(
                 loaded,
                 INT16_C(0),
                 INT16_C(0),
                 attacker_buttons,
                 UINT16_C(0),
                 target_di_x,
                 target_di_y,
                 UINT64_C(0),
                 UINT16_C(0),
                 &loaded_inspection) ||
             !expect_status(
                 pf_sim_hash(sim, &source_hash),
                 PF_STATUS_OK,
                 "kill-confirm-source-continuation-hash") ||
             !expect_status(
                 pf_sim_hash(loaded, &loaded_hash),
                 PF_STATUS_OK,
                 "kill-confirm-loaded-continuation-hash") ||
             !hash_equal(&source_hash, &loaded_hash)))
        {
            return fail("kill-confirm-deterministic-continuation");
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
                (buildup_jabs + UINT32_C(1)) *
                    content->fighter.jab_damage_q16 &&
            inspection.players[1].respawn_count == UINT16_C(0))
        {
            return 1;
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            if (expect_ko == 0 || finisher_hit == 0 ||
                source_result.event_count != UINT8_C(1) ||
                source_result.events[0].type !=
                    (uint16_t)PF_SIM_EVENT_KO ||
                source_result.events[0].source_player != UINT8_C(0) ||
                source_result.events[0].target_player != UINT8_C(1) ||
                source_result.events[0].value_q16 !=
                    (buildup_jabs + UINT32_C(1)) *
                            content->fighter.jab_damage_q16 +
                        content->fighter.strong_damage_q16)
            {
                return fail("kill-confirm-ko-result");
            }
            return 1;
        }
        if (expect_ko == 0 && finisher_hit != 0 &&
            inspection.players[1].grounded != UINT8_C(0) &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            return 1;
        }
    }
    return fail(
        expect_ko != 0
            ? "kill-confirm-ko-timeout"
            : "kill-confirm-survival-timeout");
}

static int run_kill_confirm_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    return run_kill_confirm_route(
               content,
               view,
               UINT32_C(20),
               INT16_C(0),
               INT16_C(0),
               1,
               1) &&
           run_kill_confirm_route(
               content,
               view,
               UINT32_C(0),
               INT16_C(0),
               INT16_C(0),
               0,
               0) &&
           run_kill_confirm_route(
               content,
               view,
               UINT32_C(20),
               INT16_C(32767),
               INT16_C(0),
               0,
               0);
}

static int run_zero_to_death_route(
    const pf_m4_content *content,
    const pf_content_view *view,
    int16_t target_di_x,
    int expect_ko,
    int verify_save_load)
{
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_tick_result source_result;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t last_sequence = UINT32_C(0);
    uint32_t hit_count = UINT32_C(0);
    uint32_t tick;
    int chain_started = 0;
    int chain_broken = 0;
    int saved = 0;
    int strong_started = 0;
    int saw_post_escape_hitbox = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        (verify_save_load != 0 &&
         !initialize_sim(
             &loaded_storage,
             view,
             UINT8_C(2),
             PF_SIM_MODE_DUEL,
             0,
             &loaded)) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "zero-to-death-initial-inspect") ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[1].respawn_count != UINT16_C(0))
    {
        return fail("zero-to-death-initialize-at-zero");
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
                return fail("zero-to-death-interrupted");
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

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                defender_axis,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("zero-to-death-step");
        }
        source_result = test_last_result;

        if (inspection.players[1].last_hit_sequence != last_sequence &&
            inspection.players[1].last_hit_sequence != UINT32_C(0))
        {
            last_sequence =
                inspection.players[1].last_hit_sequence;
            ++hit_count;
            chain_started = 1;
        }

        if (verify_save_load != 0 && saved == 0 &&
            hit_count == UINT32_C(11))
        {
            destination.bytes = save_bytes;
            destination.capacity = sizeof(save_bytes);
            destination.size = (size_t)0;
            if (!expect_status(
                    pf_sim_query_save_size(sim, &save_size),
                    PF_STATUS_OK,
                    "zero-to-death-query-save-size") ||
                save_size != (size_t)694 ||
                !expect_status(
                    pf_sim_save(sim, &destination),
                    PF_STATUS_OK,
                    "zero-to-death-save-mid-chain") ||
                destination.size != save_size)
            {
                return fail("zero-to-death-save");
            }
            save.bytes = save_bytes;
            save.size = save_size;
            if (!expect_status(
                    pf_sim_load(loaded, save),
                    PF_STATUS_OK,
                    "zero-to-death-load-mid-chain") ||
                !expect_status(
                    pf_m4_inspect(loaded, &loaded_inspection),
                    PF_STATUS_OK,
                    "zero-to-death-loaded-inspect") ||
                !expect_status(
                    pf_sim_hash(sim, &source_hash),
                    PF_STATUS_OK,
                    "zero-to-death-source-hash") ||
                !expect_status(
                    pf_sim_hash(loaded, &loaded_hash),
                    PF_STATUS_OK,
                    "zero-to-death-loaded-hash") ||
                !hash_equal(&source_hash, &loaded_hash))
            {
                return fail("zero-to-death-mid-chain-round-trip");
            }
            saved = 1;
        }
        else if (verify_save_load != 0 && saved != 0)
        {
            if (!step_reaction_duel(
                    loaded,
                    INT16_C(0),
                    INT16_C(0),
                    attacker_buttons,
                    UINT16_C(0),
                    defender_axis,
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    &loaded_inspection) ||
                !expect_status(
                    pf_sim_hash(sim, &source_hash),
                    PF_STATUS_OK,
                    "zero-to-death-source-continuation-hash") ||
                !expect_status(
                    pf_sim_hash(loaded, &loaded_hash),
                    PF_STATUS_OK,
                    "zero-to-death-loaded-continuation-hash") ||
                !hash_equal(&source_hash, &loaded_hash))
            {
                return fail("zero-to-death-deterministic-continuation");
            }
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
            return (hit_count > UINT32_C(0) &&
                    hit_count < UINT32_C(21) &&
                    strong_started == 0 &&
                    inspection.players[1].damage_q16 ==
                        hit_count *
                            content->fighter.jab_damage_q16) ||
                   fail("zero-to-death-di-route-still-connected");
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            if (expect_ko == 0 || strong_started == 0 ||
                hit_count != UINT32_C(22) || saved == 0 ||
                inspection.players[1].damage_q16 != UINT32_C(0) ||
                source_result.event_count != UINT8_C(1) ||
                source_result.events[0].type !=
                    (uint16_t)PF_SIM_EVENT_KO ||
                source_result.events[0].source_player != UINT8_C(0) ||
                source_result.events[0].target_player != UINT8_C(1) ||
                source_result.events[0].value_q16 !=
                    UINT32_C(21) *
                            content->fighter.jab_damage_q16 +
                        content->fighter.strong_damage_q16)
            {
                return fail("zero-to-death-ko-result");
            }
            return 1;
        }
    }
    return fail(
        expect_ko != 0
            ? "zero-to-death-ko-timeout"
            : "zero-to-death-di-escape-timeout");
}

static int run_zero_to_death_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    return run_zero_to_death_route(
               content,
               view,
               INT16_C(0),
               1,
               1) &&
           run_zero_to_death_route(
               content,
               view,
               INT16_C(32767),
               0,
               0);
}

static int run_ladder_route(
    const pf_m4_content *content,
    const pf_content_view *view,
    int16_t target_di_x,
    int expect_ko,
    int verify_save_load)
{
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    pf_tick_result source_result;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t last_sequence = UINT32_C(0);
    uint32_t hit_count = UINT32_C(0);
    uint32_t light_attack_starts = UINT32_C(0);
    uint32_t tick;
    int32_t first_hit_y_q16 = INT32_MAX;
    int chain_started = 0;
    int chain_broken = 0;
    int double_jump_used = 0;
    int saved = 0;
    int strong_started = 0;
    int saw_escape_followup_hitbox = 0;
    int saw_above_platform = 0;
    int saw_vertical_carry = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        (verify_save_load != 0 &&
         !initialize_sim(
             &loaded_storage,
             view,
             UINT8_C(2),
             PF_SIM_MODE_DUEL,
             0,
             &loaded)) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "ladder-initial-inspect"))
    {
        return fail("ladder-initialize");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("ladder-full-hop-step");
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
        return fail("ladder-full-hop-start");
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
                return fail("ladder-interrupted");
            }
            chain_broken = 1;
        }
        if (expect_ko != 0 && chain_started != 0 &&
            inspection.players[0].grounded != UINT8_C(0))
        {
            return fail("ladder-attacker-landed");
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

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                defender_axis,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("ladder-step");
        }
        source_result = test_last_result;

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
                content->stage.platform_y_q16)
            {
                saw_above_platform = 1;
            }
        }

        if (verify_save_load != 0 && saved == 0 &&
            hit_count == UINT32_C(2))
        {
            destination.bytes = save_bytes;
            destination.capacity = sizeof(save_bytes);
            destination.size = (size_t)0;
            if (!expect_status(
                    pf_sim_query_save_size(sim, &save_size),
                    PF_STATUS_OK,
                    "ladder-query-save-size") ||
                save_size != (size_t)694 ||
                !expect_status(
                    pf_sim_save(sim, &destination),
                    PF_STATUS_OK,
                    "ladder-save-mid-route") ||
                destination.size != save_size)
            {
                return fail("ladder-save");
            }
            save.bytes = save_bytes;
            save.size = save_size;
            if (!expect_status(
                    pf_sim_load(loaded, save),
                    PF_STATUS_OK,
                    "ladder-load-mid-route") ||
                !expect_status(
                    pf_m4_inspect(loaded, &loaded_inspection),
                    PF_STATUS_OK,
                    "ladder-loaded-inspect") ||
                !expect_status(
                    pf_sim_hash(sim, &source_hash),
                    PF_STATUS_OK,
                    "ladder-source-hash") ||
                !expect_status(
                    pf_sim_hash(loaded, &loaded_hash),
                    PF_STATUS_OK,
                    "ladder-loaded-hash") ||
                !hash_equal(&source_hash, &loaded_hash))
            {
                return fail("ladder-mid-route-round-trip");
            }
            saved = 1;
        }
        else if (verify_save_load != 0 && saved != 0)
        {
            if (!step_reaction_duel(
                    loaded,
                    INT16_C(0),
                    INT16_C(0),
                    attacker_buttons,
                    UINT16_C(0),
                    defender_axis,
                    INT16_C(0),
                    UINT64_C(0),
                    UINT16_C(0),
                    &loaded_inspection) ||
                !expect_status(
                    pf_sim_hash(sim, &source_hash),
                    PF_STATUS_OK,
                    "ladder-source-continuation-hash") ||
                !expect_status(
                    pf_sim_hash(loaded, &loaded_hash),
                    PF_STATUS_OK,
                    "ladder-loaded-continuation-hash") ||
                !hash_equal(&source_hash, &loaded_hash))
            {
                return fail("ladder-deterministic-continuation");
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
            return (hit_count > UINT32_C(0) &&
                    hit_count < UINT32_C(3) &&
                    strong_started == 0 &&
                    inspection.players[1].damage_q16 ==
                        hit_count *
                            content->fighter.aerial_damage_q16) ||
                   fail("ladder-di-route-still-connected");
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            if (expect_ko == 0 || strong_started == 0 ||
                hit_count != UINT32_C(4) ||
                double_jump_used == 0 || saved == 0 ||
                saw_above_platform == 0 ||
                saw_vertical_carry == 0 ||
                inspection.players[1].damage_q16 != UINT32_C(0) ||
                source_result.event_count != UINT8_C(1) ||
                source_result.events[0].type !=
                    (uint16_t)PF_SIM_EVENT_KO ||
                source_result.events[0].source_player != UINT8_C(0) ||
                source_result.events[0].target_player != UINT8_C(1) ||
                source_result.events[0].value_q16 !=
                    UINT32_C(3) *
                            content->fighter.aerial_damage_q16 +
                        content->fighter.strong_damage_q16)
            {
                return fail("ladder-ko-result");
            }
            return 1;
        }
    }
    return fail(
        expect_ko != 0
            ? "ladder-ko-timeout"
            : "ladder-di-escape-timeout");
}

static int run_ladder_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    return run_ladder_route(
               content,
               view,
               INT16_C(0),
               1,
               1) &&
           run_ladder_route(
               content,
               view,
               INT16_C(32767),
               0,
               0);
}

static int make_surface_tech_content(
    int ceiling_fixture,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "surface-tech-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 =
        ceiling_fixture != 0
            ? INT32_C(0)
            : (INT32_C(23) * PF_Q16_ONE) / INT32_C(10);
    out_content->stage.solid_right_q16 =
        INT32_C(6) * PF_Q16_ONE;
    out_content->stage.solid_top_q16 =
        INT32_C(14) * PF_Q16_ONE;
    out_content->stage.solid_bottom_q16 =
        INT32_C(29) * PF_Q16_ONE;
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "surface-tech-content-view");
}

static int drive_strong_to_surface(
    pf_sim *sim,
    int arm_tech,
    int16_t target_x,
    int16_t target_y,
    uint64_t target_buttons,
    uint8_t expected_action,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("surface-tech-strong-start");
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(16) &&
         inspection.players[1].damage_q16 == UINT32_C(0);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("surface-tech-strong-active");
        }
    }
    if (inspection.players[1].damage_q16 == UINT32_C(0) ||
        inspection.players[1].tumble != UINT8_C(1))
    {
        return fail("surface-tech-strong-did-not-tumble");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(90); ++tick)
    {
        const uint16_t trigger =
            arm_tech != 0 ? UINT16_MAX : UINT16_C(0);

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                target_y,
                target_buttons,
                trigger,
                &inspection))
        {
            return fail("surface-tech-impact-step");
        }
        if (inspection.players[1].action_state == expected_action)
        {
            *out_inspection = inspection;
            return 1;
        }
    }
    return fail("surface-tech-impact-not-observed");
}

static int run_surface_tech_test(
    const pf_m4_content *wall_content,
    const pf_content_view *wall_view,
    const pf_m4_content *ceiling_content,
    const pf_content_view *ceiling_view)
{
    test_sim_storage wall_storage;
    test_sim_storage wall_jump_storage;
    test_sim_storage wall_bounce_storage;
    test_sim_storage ceiling_storage;
    test_sim_storage ceiling_bounce_storage;
    pf_sim *wall = NULL;
    pf_sim *wall_jump = NULL;
    pf_sim *wall_bounce = NULL;
    pf_sim *ceiling = NULL;
    pf_sim *ceiling_bounce = NULL;
    pf_m4_inspection inspection;
    int32_t bounce_x;
    int32_t bounce_y;
    uint32_t tick;

    if (!initialize_sim(
            &wall_storage,
            wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &wall) ||
        !initialize_sim(
            &wall_jump_storage,
            wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &wall_jump) ||
        !initialize_sim(
            &wall_bounce_storage,
            wall_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &wall_bounce) ||
        !initialize_sim(
            &ceiling_storage,
            ceiling_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &ceiling) ||
        !initialize_sim(
            &ceiling_bounce_storage,
            ceiling_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &ceiling_bounce))
    {
        return fail("surface-tech-init");
    }

    if (!drive_strong_to_surface(
            wall,
            1,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_WALL_TECH,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        inspection.players[1].tech_window_ticks != UINT16_C(0) ||
        inspection.players[1].facing != INT8_C(-1) ||
        inspection.players[1].velocity_x_q16 != INT32_C(0) ||
        inspection.players[1].velocity_y_q16 != INT32_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("wall-tech-entry");
    }
    for (tick = UINT32_C(1);
         tick <= (uint32_t)wall_content->fighter.wall_tech_stall_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                wall,
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
            return fail("wall-tech-stall-step");
        }
        if (tick < (uint32_t)wall_content->fighter.wall_tech_stall_ticks &&
            (inspection.players[1].velocity_x_q16 != INT32_C(0) ||
             inspection.players[1].velocity_y_q16 != INT32_C(0)))
        {
            return fail("wall-tech-exact-stall");
        }
    }
    if (inspection.players[1].velocity_x_q16 >= INT32_C(0))
    {
        return fail("wall-tech-away-release");
    }

    if (!drive_strong_to_surface(
            wall_jump,
            1,
            INT16_C(0),
            INT16_C(-32767),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP,
            &inspection))
    {
        return fail("wall-tech-jump-entry");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)wall_content->fighter.wall_tech_stall_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                wall_jump,
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
            return fail("wall-tech-jump-stall-step");
        }
    }
    if (inspection.players[1].velocity_x_q16 >= INT32_C(0) ||
        inspection.players[1].velocity_y_q16 >= INT32_C(0))
    {
        return fail("wall-tech-jump-release");
    }

    if (!drive_strong_to_surface(
            wall_bounce,
            0,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_WALL_BOUNCE,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(1) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        inspection.players[1].velocity_x_q16 >= INT32_C(0))
    {
        return fail("wall-bounce-preserves-reaction");
    }
    bounce_x = inspection.players[1].velocity_x_q16;
    bounce_y = inspection.players[1].velocity_y_q16;
    if (bounce_x == INT32_C(0) || bounce_y == INT32_C(0))
    {
        return fail("wall-bounce-reflects-motion");
    }

    if (!drive_strong_to_surface(
            ceiling,
            1,
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_CEILING_TECH,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 != INT32_C(0) ||
        inspection.players[1].velocity_x_q16 !=
            ceiling_content->fighter.ceiling_tech_speed_q16 ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("ceiling-tech-entry");
    }

    if (!drive_strong_to_surface(
            ceiling_bounce,
            0,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_CEILING_BOUNCE,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(1) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 <= INT32_C(0))
    {
        return fail("ceiling-bounce-preserves-reaction");
    }
    return 1;
}

static int run_whiff_and_trade_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_duel(
            sim,
            INT16_C(-13500),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].facing != INT8_C(-1) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("facing-away-whiff");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "trade-reset") ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection))
    {
        return fail("trade-schedule");
    }

    if (inspection.players[0].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].last_hit_attacker != UINT8_C(1) ||
        inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        inspection.players[0].last_hit_sequence == UINT32_C(0) ||
        inspection.players[1].last_hit_sequence == UINT32_C(0) ||
        inspection.players[0].last_hit_sequence ==
            inspection.players[1].last_hit_sequence)
    {
        return fail("simultaneous-trade");
    }
    return 1;
}

static int start_normal_shield_block(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection);
}

static int start_powershield_block(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_MAX,
               out_inspection);
}

static int advance_block_to_release(
    const pf_m4_content *content,
    pf_sim *sim,
    int powershield,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if ((powershield != 0
             ? !start_powershield_block(sim, out_inspection)
             : !start_normal_shield_block(sim, out_inspection)))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
    for (tick = UINT32_C(0); tick < UINT32_C(600); ++tick)
    {
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
        {
            return 1;
        }
        if (!step_reaction_duel(
                sim,
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
    return 0;
}

static int run_powershield_cancel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage early_storage;
    test_sim_storage cancel_storage;
    test_sim_storage strong_cancel_storage;
    test_sim_storage normal_storage;
    pf_sim *early = NULL;
    pf_sim *cancel = NULL;
    pf_sim *strong_cancel = NULL;
    pf_sim *normal = NULL;
    pf_m4_inspection inspection;

    if (!initialize_sim(
            &early_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &early) ||
        !initialize_sim(
            &cancel_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &cancel) ||
        !initialize_sim(
            &normal_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &normal) ||
        !initialize_sim(
            &strong_cancel_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &strong_cancel) ||
        !advance_block_to_release(
            content,
            early,
            1,
            &inspection) ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(1))
    {
        return fail("powershield-cancel-opportunity");
    }
    if (!step_reaction_duel(
            early,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        inspection.players[1].action_ticks !=
            content->fighter.powershield_cancel_delay_ticks ||
        inspection.players[1].powershield != UINT8_C(1))
    {
        return fail("powershield-cancel-frame-one-rejected");
    }

    if (!advance_block_to_release(
            content,
            cancel,
            1,
            &inspection) ||
        !step_reaction_duel(
            cancel,
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
            content->fighter.powershield_cancel_delay_ticks ||
        !step_reaction_duel(
            cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-frame-two-attack");
    }

    if (!advance_block_to_release(
            content,
            strong_cancel,
            1,
            &inspection) ||
        !step_reaction_duel(
            strong_cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            strong_cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-frame-two-strong-attack");
    }

    if (!advance_block_to_release(
            content,
            normal,
            0,
            &inspection) ||
        inspection.players[1].powershield != UINT8_C(0) ||
        !step_reaction_duel(
            normal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            normal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        inspection.players[1].action_ticks != UINT16_C(2))
    {
        return fail("normal-shield-release-not-canceled");
    }
    return 1;
}

static int run_powershield_cancel_replay_test(
    const pf_content_view *view)
{
    test_sim_storage initial_storage;
    test_sim_storage source_storage;
    test_sim_storage playback_storage;
    pf_sim *initial = NULL;
    pf_sim *source = NULL;
    pf_sim *playback = NULL;
    pf_input_frame inputs[TEST_PSC_REPLAY_INPUT_COUNT];
    pf_state_hash hashes[TEST_PSC_REPLAY_HASH_COUNT];
    pf_input_frame tick_inputs[PF_SIM_MAX_PLAYERS];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    pf_tick_result result;
    pf_mut_bytes destination;
    pf_bytes replay;
    pf_m4_inspection inspection;
    uint8_t replay_bytes[TEST_PSC_REPLAY_CAPACITY];
    size_t replay_size = (size_t)0;
    uint64_t tick;
    int saw_powershield = 0;
    int saw_cancel_release = 0;
    int saw_cancel_attack = 0;

    if (!initialize_sim(
            &initial_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &initial) ||
        !initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &source) ||
        !initialize_sim(
            &playback_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &playback) ||
        !expect_status(
            pf_sim_clone(source, initial),
            PF_STATUS_OK,
            "powershield-cancel-replay-clone") ||
        !expect_status(
            pf_sim_hash(initial, &hashes[0]),
            PF_STATUS_OK,
            "powershield-cancel-replay-initial-hash"))
    {
        return fail("powershield-cancel-replay-init");
    }

    for (tick = UINT64_C(0);
         tick < TEST_PSC_REPLAY_TICKS;
         ++tick)
    {
        pf_input_frame *stored =
            &inputs[(size_t)tick * (size_t)UINT8_C(2)];

        make_inputs(tick_inputs, UINT8_C(2), tick);
        if (tick == UINT64_C(0))
        {
            tick_inputs[0].buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (tick == UINT64_C(2))
        {
            tick_inputs[1].left_trigger = UINT16_MAX;
        }
        if (tick == UINT64_C(12))
        {
            tick_inputs[1].buttons = PF_INPUT_BUTTON_ATTACK;
        }
        (void)memcpy(
            stored,
            tick_inputs,
            sizeof(*stored) * (size_t)UINT8_C(2));
        if (!expect_status(
                pf_sim_tick(
                    source,
                    tick_inputs,
                    (size_t)UINT8_C(2),
                    &result),
                PF_STATUS_OK,
                "powershield-cancel-replay-tick") ||
            !expect_status(
                pf_sim_hash(
                    source,
                    &hashes[(size_t)tick + (size_t)1]),
                PF_STATUS_OK,
                "powershield-cancel-replay-hash") ||
            !expect_status(
                pf_m4_inspect(source, &inspection),
                PF_STATUS_OK,
                "powershield-cancel-replay-inspect"))
        {
            return fail("powershield-cancel-replay-record");
        }
        if (inspection.players[1].powershield != UINT8_C(0))
        {
            saw_powershield = 1;
        }
        if (inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_RELEASE &&
            inspection.players[1].powershield != UINT8_C(0))
        {
            saw_cancel_release = 1;
        }
        if (tick == UINT64_C(12) &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
        {
            saw_cancel_attack = 1;
        }
    }

    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = inputs;
    replay_source.input_frame_count =
        (size_t)TEST_PSC_REPLAY_INPUT_COUNT;
    replay_source.state_hashes = hashes;
    replay_source.state_hash_count =
        (size_t)TEST_PSC_REPLAY_HASH_COUNT;
    replay_source.tick_count = TEST_PSC_REPLAY_TICKS;
    replay_source.final_result = result;
    destination.bytes = replay_bytes;
    destination.capacity = sizeof(replay_bytes);
    replay.bytes = replay_bytes;

    if (saw_powershield == 0 ||
        saw_cancel_release == 0 ||
        saw_cancel_attack == 0 ||
        !expect_status(
            pf_replay_query_size(&replay_source, &replay_size),
            PF_STATUS_OK,
            "powershield-cancel-replay-query") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("powershield-cancel-replay-trace");
    }
    destination.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_OK,
            "powershield-cancel-replay-encode"))
    {
        return fail("powershield-cancel-replay-encode");
    }
    replay.size = destination.size;
    if (!expect_status(
            pf_replay_verify(
                playback,
                replay,
                &verification),
            PF_STATUS_OK,
            "powershield-cancel-replay-verify") ||
        verification.verified_ticks !=
            TEST_PSC_REPLAY_TICKS ||
        !expect_status(
            pf_m4_inspect(playback, &inspection),
            PF_STATUS_OK,
            "powershield-cancel-replay-final-inspect") ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-replay-result");
    }
    return 1;
}

static int run_aerial_l_cancel_replay_test(void)
{
    test_sim_storage initial_storage;
    test_sim_storage source_storage;
    test_sim_storage playback_storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim *initial = NULL;
    pf_sim *source = NULL;
    pf_sim *playback = NULL;
    pf_input_frame inputs[TEST_ALC_REPLAY_INPUT_COUNT];
    pf_state_hash hashes[TEST_ALC_REPLAY_HASH_COUNT];
    pf_input_frame tick_inputs[PF_SIM_MAX_PLAYERS];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    pf_tick_result result;
    pf_mut_bytes destination;
    pf_bytes replay;
    pf_m4_inspection inspection;
    uint8_t replay_bytes[TEST_ALC_REPLAY_CAPACITY];
    size_t replay_size = (size_t)0;
    uint64_t tick;
    int attack_started = 0;
    int trigger_pressed = 0;
    int saw_aerial = 0;
    int saw_fast_fall = 0;
    int saw_l_cancel_eligible = 0;
    int saw_l_cancel_landing = 0;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "aerial-replay-default-content") ||
        !expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "aerial-replay-content-view") ||
        !initialize_sim(
            &initial_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &initial) ||
        !initialize_sim(
            &source_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &source) ||
        !initialize_sim(
            &playback_storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &playback) ||
        !expect_status(
            pf_sim_clone(source, initial),
            PF_STATUS_OK,
            "aerial-replay-clone") ||
        !expect_status(
            pf_sim_hash(initial, &hashes[0]),
            PF_STATUS_OK,
            "aerial-replay-initial-hash") ||
        !expect_status(
            pf_m4_inspect(source, &inspection),
            PF_STATUS_OK,
            "aerial-replay-initial-inspect"))
    {
        return fail("aerial-replay-init");
    }

    for (tick = UINT64_C(0);
         tick < TEST_ALC_REPLAY_TICKS;
         ++tick)
    {
        pf_input_frame *stored =
            &inputs[(size_t)tick * (size_t)UINT8_C(2)];

        make_inputs(tick_inputs, UINT8_C(2), tick);
        if (tick == UINT64_C(0))
        {
            tick_inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
        }
        else if (
            attack_started == 0 &&
            inspection.players[0].grounded == UINT8_C(0) &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE)
        {
            tick_inputs[0].buttons = PF_INPUT_BUTTON_ATTACK;
            attack_started = 1;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
        {
            tick_inputs[0].main_stick_y = INT16_MAX;
            if (trigger_pressed == 0 &&
                inspection.players[0].velocity_y_q16 >= INT32_C(0))
            {
                tick_inputs[0].left_trigger = UINT16_MAX;
                trigger_pressed = 1;
            }
        }

        (void)memcpy(
            stored,
            tick_inputs,
            sizeof(*stored) * (size_t)UINT8_C(2));
        if (!expect_status(
                pf_sim_tick(
                    source,
                    tick_inputs,
                    (size_t)UINT8_C(2),
                    &result),
                PF_STATUS_OK,
                "aerial-replay-tick") ||
            !expect_status(
                pf_sim_hash(
                    source,
                    &hashes[(size_t)tick + (size_t)1]),
                PF_STATUS_OK,
                "aerial-replay-hash") ||
            !expect_status(
                pf_m4_inspect(source, &inspection),
                PF_STATUS_OK,
                "aerial-replay-inspect"))
        {
            return fail("aerial-replay-record");
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
        {
            saw_aerial = 1;
        }
        if (inspection.players[0].fast_fall != UINT8_C(0))
        {
            saw_fast_fall = 1;
        }
        if (inspection.players[0].l_cancel_eligible != UINT8_C(0))
        {
            saw_l_cancel_eligible = 1;
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_L_CANCEL_LANDING)
        {
            saw_l_cancel_landing = 1;
        }
    }

    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = inputs;
    replay_source.input_frame_count =
        (size_t)TEST_ALC_REPLAY_INPUT_COUNT;
    replay_source.state_hashes = hashes;
    replay_source.state_hash_count =
        (size_t)TEST_ALC_REPLAY_HASH_COUNT;
    replay_source.tick_count = TEST_ALC_REPLAY_TICKS;
    replay_source.final_result = result;
    destination.bytes = replay_bytes;
    destination.capacity = sizeof(replay_bytes);
    replay.bytes = replay_bytes;

    if (saw_aerial == 0 ||
        saw_fast_fall == 0 ||
        saw_l_cancel_eligible == 0 ||
        saw_l_cancel_landing == 0 ||
        !expect_status(
            pf_replay_query_size(&replay_source, &replay_size),
            PF_STATUS_OK,
            "aerial-replay-query") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("aerial-replay-trace");
    }
    destination.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &destination),
            PF_STATUS_OK,
            "aerial-replay-encode"))
    {
        return fail("aerial-replay-encode");
    }
    replay.size = destination.size;
    if (!expect_status(
            pf_replay_verify(
                playback,
                replay,
                &verification),
            PF_STATUS_OK,
            "aerial-replay-verify") ||
        verification.verified_ticks != TEST_ALC_REPLAY_TICKS ||
        !expect_status(
            pf_m4_inspect(playback, &inspection),
            PF_STATUS_OK,
            "aerial-replay-final-inspect") ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].grounded != UINT8_C(1))
    {
        return fail("aerial-replay-result");
    }
    return 1;
}

static int run_shield_state_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t run_velocity;
    uint32_t shield_health;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return fail("shield-state-init");
    }
    if (!step_reaction_duel(
            sim,
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 ||
        !expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "shield-state-reset"))
    {
        return fail("initial-dash-cannot-shield");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(11); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(32767),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("shield-stop-run-setup");
        }
    }
    run_velocity = inspection.players[0].velocity_x_q16;
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        run_velocity <= INT32_C(0) ||
        !step_reaction_duel(
            sim,
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
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_x_q16 >= run_velocity ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 -
                content->fighter.shield_hold_depletion_q16)
    {
        return fail("shield-stop-and-entry");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("shield-minimum-hold-release");
        }
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
        {
            break;
        }
    }
    if (inspection.players[0].action_state !=
        (uint8_t)PF_M4_ACTION_SHIELD_RELEASE)
    {
        return fail("shield-release-entry");
    }
    shield_health = inspection.players[0].shield_health_q16;
    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].shield_health_q16 <= shield_health ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        return fail("shield-regeneration-and-jump-cancel");
    }
    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "long-shield-reset"))
    {
        return fail("long-shield-reset");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("long-shield-hold");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].action_ticks !=
            content->fighter.shield_minimum_hold_ticks)
    {
        return fail("shield-action-timer-saturates");
    }
    return 1;
}

static int run_dashing_shield_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage tap_storage;
    test_sim_storage held_storage;
    test_sim_storage loaded_storage;
    test_sim_storage idle_storage;
    pf_sim *tap = NULL;
    pf_sim *held = NULL;
    pf_sim *loaded = NULL;
    pf_sim *idle = NULL;
    pf_m4_inspection tap_inspection;
    pf_m4_inspection held_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection idle_inspection;
    pf_state_hash tap_hash;
    pf_state_hash held_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    int32_t run_start_x;
    uint32_t tick;

    if (!initialize_sim(
            &tap_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &tap) ||
        !initialize_sim(
            &held_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &held) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &idle_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &idle))
    {
        return fail("dashing-shield-init");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.initial_dash_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                tap,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &tap_inspection) ||
            !step_reaction_duel(
                held,
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &held_inspection))
        {
            return fail("dashing-shield-run-setup");
        }
    }
    run_start_x = tap_inspection.players[0].position_x_q16;
    if (tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        held_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_RUN ||
        tap_inspection.players[0].velocity_x_q16 !=
            held_inspection.players[0].velocity_x_q16 ||
        tap_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        !step_reaction_duel(
            tap,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &tap_inspection) ||
        !step_reaction_duel(
            held,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &held_inspection) ||
        tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        tap_inspection.players[0].action_ticks != UINT16_C(1) ||
        tap_inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        tap_inspection.players[0].position_x_q16 <= run_start_x ||
        !expect_status(
            pf_sim_hash(tap, &tap_hash),
            PF_STATUS_OK,
            "dashing-shield-tap-hash") ||
        !expect_status(
            pf_sim_hash(held, &held_hash),
            PF_STATUS_OK,
            "dashing-shield-held-hash") ||
        !hash_equal(&tap_hash, &held_hash) ||
        !expect_status(
            pf_sim_query_save_size(tap, &save_size),
            PF_STATUS_OK,
            "dashing-shield-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("dashing-shield-entry");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(tap, &destination),
            PF_STATUS_OK,
            "dashing-shield-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "dashing-shield-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(1);
         tick < (uint32_t)content->fighter.shield_minimum_hold_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                tap,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &tap_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !step_reaction_duel(
                held,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &held_inspection) ||
            !expect_status(
                pf_sim_hash(tap, &tap_hash),
                PF_STATUS_OK,
                "dashing-shield-release-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "dashing-shield-loaded-release-hash") ||
            !hash_equal(&tap_hash, &loaded_hash))
        {
            return fail("dashing-shield-minimum-hold");
        }
    }
    if (tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_RELEASE ||
        tap_inspection.players[0].action_ticks != UINT16_C(0) ||
        held_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        held_inspection.players[0].action_ticks !=
            content->fighter.shield_minimum_hold_ticks ||
        tap_inspection.players[0].position_x_q16 !=
            held_inspection.players[0].position_x_q16 ||
        tap_inspection.players[0].position_x_q16 <= run_start_x ||
        tap_inspection.players[0].velocity_x_q16 !=
            held_inspection.players[0].velocity_x_q16)
    {
        return fail("dashing-shield-tap-versus-held");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.shield_release_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                tap,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &tap_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !step_reaction_duel(
                held,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &held_inspection) ||
            !expect_status(
                pf_sim_hash(tap, &tap_hash),
                PF_STATUS_OK,
                "dashing-shield-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "dashing-shield-loaded-future-hash") ||
            !hash_equal(&tap_hash, &loaded_hash))
        {
            return fail("dashing-shield-release-duration");
        }
    }
    if (tap_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        held_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        tap_inspection.players[0].position_x_q16 !=
            held_inspection.players[0].position_x_q16 ||
        tap_inspection.players[0].position_x_q16 <= run_start_x ||
        tap_inspection.players[0].shield_health_q16 <=
            held_inspection.players[0].shield_health_q16)
    {
        return fail("dashing-shield-recovery-versus-held");
    }

    if (!expect_status(
            pf_m4_inspect(idle, &idle_inspection),
            PF_STATUS_OK,
            "dashing-shield-idle-inspect"))
    {
        return 0;
    }
    run_start_x = idle_inspection.players[0].position_x_q16;
    if (!step_reaction_duel(
            idle,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &idle_inspection) ||
        idle_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        idle_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        idle_inspection.players[0].position_x_q16 != run_start_x)
    {
        return fail("dashing-shield-idle-negative");
    }
    return 1;
}

static int spacing_distance_matches(
    const pf_m4_content *content,
    const pf_m4_inspection *inspection,
    int expect_jab_range,
    int expect_strong_range)
{
    const int32_t distance =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;
    const int32_t jab_reach =
        content->fighter.jab_hitbox_offset_x_q16 +
        content->fighter.jab_hitbox_half_width_q16 +
        content->fighter.half_width_q16;
    const int32_t strong_reach =
        content->fighter.strong_hitbox_offset_x_q16 +
        content->fighter.strong_hitbox_half_width_q16 +
        content->fighter.half_width_q16;

    return (distance <= jab_reach) == expect_jab_range &&
           (distance <= strong_reach) == expect_strong_range;
}

static int start_spacing_whiff_counter(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[1].hitbox_active != UINT8_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(8) ||
        out_inspection->players[0].damage_q16 != UINT32_C(0) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return 0;
    }
    return step_duel(
               sim,
               INT16_C(0),
               PF_INPUT_BUTTON_STRONG_ATTACK,
               INT16_C(0),
               UINT64_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_STRONG_ATTACK &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
           out_inspection->players[1].hitbox_active != UINT8_C(0);
}

static int run_spacing_counter_snapshot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;
    int saw_counter_hit = 0;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !expect_status(
            pf_m4_inspect(source, &source_inspection),
            PF_STATUS_OK,
            "spacing-initial-inspect") ||
        !spacing_distance_matches(
            content,
            &source_inspection,
            0,
            1) ||
        !start_spacing_whiff_counter(source, &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "spacing-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("spacing-safe-tip-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "spacing-save-mid-counter") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "spacing-load-mid-counter") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "spacing-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "spacing-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("spacing-mid-counter-round-trip");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "spacing-source-continuation-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "spacing-loaded-continuation-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("spacing-deterministic-continuation");
        }
        if (source_inspection.players[1].damage_q16 ==
            content->fighter.strong_damage_q16)
        {
            if (source_inspection.players[0].damage_q16 != UINT32_C(0) ||
                source_inspection.players[1].last_hit_attacker !=
                    UINT8_C(0) ||
                source_inspection.players[1].last_hit_valid == UINT8_C(0))
            {
                return fail("spacing-safe-tip-counter-result");
            }
            saw_counter_hit = 1;
        }
    }
    return saw_counter_hit != 0 || fail("spacing-counter-did-not-hit");
}

static int run_spacing_close_negative_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "spacing-close-inspect") ||
        !spacing_distance_matches(content, &inspection, 1, 1) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection))
    {
        return fail("spacing-close-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                tick == UINT32_C(3)
                    ? PF_INPUT_BUTTON_STRONG_ATTACK
                    : UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("spacing-close-step");
        }
        if (inspection.players[0].damage_q16 != UINT32_C(0))
        {
            break;
        }
    }
    return (inspection.players[0].damage_q16 ==
                content->fighter.jab_damage_q16 &&
            inspection.players[0].last_hit_attacker == UINT8_C(1) &&
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_STRONG_ATTACK &&
            inspection.players[1].damage_q16 == UINT32_C(0)) ||
           fail("spacing-close-jab-punishes");
}

static int run_spacing_far_negative_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;
    int saw_strong_hitbox = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "spacing-far-inspect") ||
        !spacing_distance_matches(content, &inspection, 0, 0) ||
        !start_spacing_whiff_counter(sim, &inspection))
    {
        return fail("spacing-far-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("spacing-far-step");
        }
        if (inspection.players[0].hitbox_active != UINT8_C(0))
        {
            saw_strong_hitbox = 1;
        }
    }
    return (saw_strong_hitbox != 0 &&
            inspection.players[0].damage_q16 == UINT32_C(0) &&
            inspection.players[1].damage_q16 == UINT32_C(0)) ||
           fail("spacing-far-counter-whiffs");
}

static int run_spacing_shield_control_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "spacing-shield-inspect") ||
        !spacing_distance_matches(content, &inspection, 0, 1))
    {
        return fail("spacing-shield-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(6); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("spacing-shield-start");
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("spacing-shield-step");
        }
        if (inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_HITLAG)
        {
            break;
        }
    }
    return (tick < UINT32_C(12) &&
            inspection.players[1].damage_q16 == UINT32_C(0) &&
            inspection.players[1].shield_health_q16 <
                content->fighter.shield_health_q16 &&
            inspection.players[1].powershield == UINT8_C(0) &&
            test_last_result.event_count == UINT8_C(1) &&
            test_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK) ||
           fail("spacing-tip-shield-block");
}

static int walk_to_approach_distance(
    pf_sim *sim,
    const pf_m4_content *content,
    int close_distance,
    pf_m4_inspection *out_inspection)
{
    const int32_t jab_reach =
        content->fighter.jab_hitbox_offset_x_q16 +
        content->fighter.jab_hitbox_half_width_q16 +
        content->fighter.half_width_q16;
    const int32_t strong_reach =
        content->fighter.strong_hitbox_offset_x_q16 +
        content->fighter.strong_hitbox_half_width_q16 +
        content->fighter.half_width_q16;
    const int32_t target_distance =
        close_distance != 0
            ? jab_reach - PF_Q16_ONE / INT32_C(5)
            : jab_reach +
                  (strong_reach - jab_reach) / INT32_C(2);
    const int32_t brake_distance =
        target_distance + content->fighter.walk_speed_q16;
    uint32_t tick;
    int saw_walk = 0;

    if (!expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "approach-initial-inspect"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(400); ++tick)
    {
        const int16_t walk_input =
            out_inspection->players[1].position_x_q16 -
                    out_inspection->players[0].position_x_q16 >
                brake_distance
                ? INT16_C(13500)
                : INT16_C(0);

        if (!step_duel(
                sim,
                walk_input,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_WALK)
        {
            saw_walk = 1;
        }
        if (walk_input == INT16_C(0) &&
            out_inspection->players[0].velocity_x_q16 == INT32_C(0))
        {
            break;
        }
    }
    return tick < UINT32_C(400) && saw_walk != 0 &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
           spacing_distance_matches(
               content,
               out_inspection,
               close_distance != 0,
               1);
}

static int run_approach_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t start_x;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "approach-start-inspect"))
    {
        return fail("approach-init");
    }
    start_x = inspection.players[0].position_x_q16;
    if (!walk_to_approach_distance(
            sim,
            content,
            0,
            &inspection) ||
        inspection.players[0].position_x_q16 <= start_x ||
        inspection.players[0].facing != INT8_C(1) ||
        !start_spacing_whiff_counter(sim, &inspection))
    {
        return fail("approach-safe-entry");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(14); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("approach-safe-step");
        }
        if (inspection.players[1].damage_q16 != UINT32_C(0))
        {
            break;
        }
    }
    if (tick == UINT32_C(14) ||
        inspection.players[0].damage_q16 != UINT32_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 ||
        inspection.players[1].last_hit_attacker != UINT8_C(0))
    {
        return fail("approach-safe-conversion");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "approach-close-reset") ||
        !walk_to_approach_distance(
            sim,
            content,
            1,
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection))
    {
        return fail("approach-close-entry");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("approach-close-step");
        }
        if (inspection.players[0].damage_q16 != UINT32_C(0))
        {
            break;
        }
    }
    return (tick < UINT32_C(8) &&
            inspection.players[0].damage_q16 ==
                content->fighter.jab_damage_q16 &&
            inspection.players[0].last_hit_attacker == UINT8_C(1) &&
            inspection.players[1].damage_q16 == UINT32_C(0)) ||
           fail("approach-close-intercepted");
}

static int run_shield_block_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage normal_storage;
    test_sim_storage power_storage;
    pf_sim *normal = NULL;
    pf_sim *power = NULL;
    pf_m4_inspection normal_inspection;
    pf_m4_inspection power_inspection;
    const uint32_t shield_damage =
        (uint32_t)(((uint64_t)content->fighter.jab_damage_q16 *
                    (uint64_t)content->fighter
                        .shield_damage_multiplier_q16) >>
                   16U);
    const uint32_t normal_expected_health =
        content->fighter.shield_health_q16 -
        UINT32_C(8) *
            content->fighter.shield_hold_depletion_q16 -
        shield_damage;
    const uint32_t power_expected_health =
        content->fighter.shield_health_q16 -
        content->fighter.shield_hold_depletion_q16;
    int32_t normal_pushback;
    uint16_t normal_shield_stun;
    uint32_t tick;

    if (!initialize_sim(
            &normal_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &normal) ||
        !initialize_sim(
            &power_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &power) ||
        !start_normal_shield_block(normal, &normal_inspection))
    {
        return fail("normal-shield-block-setup");
    }
    if (normal_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        normal_inspection.players[1].damage_q16 != UINT32_C(0) ||
        normal_inspection.players[1].last_hit_valid != UINT8_C(0) ||
        normal_inspection.players[1].powershield != UINT8_C(0) ||
        normal_inspection.players[1].shield_stun_ticks ==
            UINT16_C(0) ||
        normal_inspection.players[1].shield_health_q16 !=
            normal_expected_health ||
        normal_inspection.players[0].velocity_x_q16 >= INT32_C(0) ||
        normal_inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 != shield_damage ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return fail("normal-shield-damage-stun-pushback");
    }
    normal_pushback =
        normal_inspection.players[1].velocity_x_q16;
    normal_shield_stun =
        normal_inspection.players[1].shield_stun_ticks;

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                normal,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &normal_inspection))
        {
            return fail("shield-hitlag-step");
        }
    }
    if (normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        normal_inspection.players[1].hitlag_ticks != UINT16_C(0))
    {
        return fail("shield-hitlag-resume");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_reaction_duel(
                normal,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &normal_inspection))
        {
            return fail("shield-stun-step");
        }
        if (normal_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD)
        {
            break;
        }
    }
    if (normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        normal_inspection.players[1].shield_stun_ticks !=
            UINT16_C(0))
    {
        return fail("shield-stun-duration");
    }

    if (!start_powershield_block(power, &power_inspection) ||
        power_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        power_inspection.players[1].damage_q16 != UINT32_C(0) ||
        power_inspection.players[1].powershield != UINT8_C(1) ||
        power_inspection.players[1].shield_health_q16 !=
            power_expected_health ||
        power_inspection.players[1].shield_stun_ticks !=
            normal_shield_stun ||
        power_inspection.players[1].velocity_x_q16 <=
            normal_pushback ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_POWERSHIELD ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 != UINT32_C(0))
    {
        return fail("powershield-window-and-zero-damage");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                power,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &power_inspection))
        {
            return fail("powershield-hitlag-step");
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_reaction_duel(
                power,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &power_inspection))
        {
            return fail("powershield-stun-step");
        }
        if (power_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD)
        {
            break;
        }
    }
    if (power_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        power_inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-indicator-clears");
    }
    return 1;
}

static int make_shield_break_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "shield-break-default-content"))
    {
        return 0;
    }
    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.shield_health_q16 =
        UINT32_C(4) * UINT32_C(65536);
    out_content->fighter.shield_reset_health_q16 =
        UINT32_C(2) * UINT32_C(65536);
    out_content->fighter.shield_hold_depletion_q16 =
        UINT32_C(655);
    out_content->fighter.shield_attacker_pushback_damage_q16 =
        INT32_C(1);
    out_content->fighter.shield_attacker_pushback_base_q16 =
        INT32_C(1);
    out_content->fighter.gravity_q16 =
        PF_Q16_ONE / INT32_C(10);
    out_content->fighter.fall_speed_q16 =
        (INT32_C(2) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.shield_break_launch_speed_q16 =
        PF_Q16_ONE / INT32_C(2);
    out_content->fighter.shield_break_stun_ticks = UINT16_C(20);
    out_content->fighter.shield_break_minimum_stun_ticks =
        UINT16_C(8);
    out_content->fighter.shield_break_down_ticks = UINT16_C(12);
    out_content->fighter.shield_break_stand_ticks = UINT16_C(12);
    out_content->fighter.shield_break_mash_reduction_ticks =
        UINT16_C(3);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "shield-break-content-view");
}

static int advance_shield_break_to_stun(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection,
    int test_early_invulnerability)
{
    int tested_early_invulnerability = 0;
    int saw_down = 0;
    int saw_stand = 0;
    uint32_t tick;

    if (!start_normal_shield_block(sim, out_inspection) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        out_inspection->players[1].shield_health_q16 !=
            UINT32_C(0) ||
        out_inspection->players[1].powershield != UINT8_C(0) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BREAK ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1))
    {
        return fail("shield-break-hit-setup");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("shield-break-hitlag");
        }
    }
    if (out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK ||
        out_inspection->players[1].grounded != UINT8_C(0) ||
        out_inspection->players[1].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        out_inspection->players[1].velocity_x_q16 != INT32_C(0) ||
        out_inspection->players[1].velocity_y_q16 !=
            -content->fighter.shield_break_launch_speed_q16 ||
        out_inspection->players[1].invulnerable != UINT8_C(1))
    {
        return fail("shield-break-launch");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(256); ++tick)
    {
        uint32_t attack_tick;

        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            break;
        }
        if (test_early_invulnerability != 0 &&
            tested_early_invulnerability == 0 &&
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            for (attack_tick = UINT32_C(0);
                 attack_tick < UINT32_C(3);
                 ++attack_tick)
            {
                if (!step_reaction_duel(
                        sim,
                        INT16_C(0),
                        INT16_C(0),
                        attack_tick == UINT32_C(0)
                            ? PF_INPUT_BUTTON_ATTACK
                            : UINT64_C(0),
                        UINT16_C(0),
                        INT16_C(0),
                        INT16_C(0),
                        UINT64_C(0),
                        UINT16_C(0),
                        out_inspection) ||
                    out_inspection->players[1].damage_q16 !=
                        UINT32_C(0) ||
                    out_inspection->players[1].shield_health_q16 !=
                        UINT32_C(0) ||
                    out_inspection->players[1].action_state ==
                        (uint8_t)PF_M4_ACTION_HITLAG ||
                    out_inspection->players[1].invulnerable !=
                        UINT8_C(1))
                {
                    return fail(
                        "shield-break-early-invulnerability");
                }
            }
            tested_early_invulnerability = 1;
            continue;
        }
        if (!step_reaction_duel(
                sim,
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
            return fail("shield-break-sequence-step");
        }
        if (out_inspection->players[1].shield_health_q16 !=
            UINT32_C(0))
        {
            return fail("shield-break-sequence-health");
        }
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK)
        {
            if (out_inspection->players[1].grounded != UINT8_C(0) ||
                out_inspection->players[1].velocity_x_q16 !=
                    INT32_C(0) ||
                out_inspection->players[1].invulnerable != UINT8_C(1))
            {
                return fail("shield-break-flight");
            }
        }
        else if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_DOWN)
        {
            saw_down = 1;
            if (out_inspection->players[1].grounded != UINT8_C(1) ||
                out_inspection->players[1].invulnerable != UINT8_C(1))
            {
                return fail("shield-break-down");
            }
        }
        else if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STAND)
        {
            saw_stand = 1;
            if (saw_down == 0 ||
                out_inspection->players[1].grounded != UINT8_C(1) ||
                out_inspection->players[1].invulnerable != UINT8_C(1))
            {
                return fail("shield-break-stand");
            }
        }
        else if (out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN)
        {
            return fail("shield-break-phase-order");
        }
    }

    if (tick == UINT32_C(256) ||
        saw_down == 0 ||
        saw_stand == 0 ||
        (test_early_invulnerability != 0 &&
         tested_early_invulnerability == 0) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN ||
        out_inspection->players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks ||
        out_inspection->players[1].grounded != UINT8_C(1) ||
        out_inspection->players[1].invulnerable != UINT8_C(0))
    {
        return fail("shield-break-stun-entry");
    }
    return 1;
}

static int run_shield_break_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage punish_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *punish = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection punish_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !advance_shield_break_to_stun(
            content,
            source,
            &source_inspection,
            1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-shield-break-save-size") ||
        save_size != (size_t)694)
    {
        return fail("shield-break-snapshot-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-shield-break-stun") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-shield-break-stun") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-shield-break") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-shield-break") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("shield-break-snapshot-round-trip");
    }

    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &source_inspection) ||
        !step_reaction_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &loaded_inspection) ||
        source_inspection.players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks -
                UINT16_C(1) -
                content->fighter
                    .shield_break_mash_reduction_ticks ||
        source_inspection.players[1].tech_window_ticks !=
            UINT16_C(0) ||
        source_inspection.players[1].tech_lockout_ticks !=
            UINT16_C(0) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-shield-break-mash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-shield-break-mash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("shield-break-fresh-mash");
    }
    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &source_inspection) ||
        !step_reaction_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &loaded_inspection) ||
        source_inspection.players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks -
                UINT16_C(2) -
                content->fighter
                    .shield_break_mash_reduction_ticks)
    {
        return fail("shield-break-held-input-no-remash");
    }
    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        !step_reaction_duel(
            loaded,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &loaded_inspection) ||
        source_inspection.players[1].action_ticks !=
            content->fighter.shield_break_stun_ticks -
                UINT16_C(3) -
                UINT16_C(2) *
                    content->fighter
                        .shield_break_mash_reduction_ticks ||
        source_inspection.players[1].tech_window_ticks !=
            UINT16_C(0) ||
        source_inspection.players[1].tech_lockout_ticks !=
            UINT16_C(0))
    {
        return fail("shield-break-trigger-mash-no-tech-buffer");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        const uint64_t mash =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_JUMP
                : UINT64_C(0);

        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                mash,
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                mash,
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-shield-break-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-shield-break-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail(
                "shield-break-deterministic-continuation");
        }
        if (source_inspection.players[1].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (tick == UINT32_C(32) ||
        source_inspection.players[1].shield_health_q16 !=
            content->fighter.shield_reset_health_q16 ||
        loaded_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("shield-break-mash-recovery");
    }

    if (!initialize_sim(
            &punish_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &punish) ||
        !advance_shield_break_to_stun(
            content,
            punish,
            &punish_inspection,
            0) ||
        punish_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("shield-break-punish-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                punish,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &punish_inspection))
        {
            return fail("shield-break-punish-step");
        }
    }
    if (punish_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        punish_inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        punish_inspection.players[1].shield_health_q16 !=
            content->fighter.shield_reset_health_q16 ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT)
    {
        return fail("shield-break-vulnerable-stun-punish");
    }
    return 1;
}

static int start_reaction_hit(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               PF_INPUT_BUTTON_ATTACK,
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           step_reaction_duel(
               sim,
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               INT16_C(0),
               INT16_C(0),
               UINT64_C(0),
               UINT16_C(0),
               out_inspection) &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_HITLAG;
}

static int run_di_and_sdi_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage sdi_storage;
    test_sim_storage neutral_storage;
    test_sim_storage di_storage;
    pf_sim *sdi_sim = NULL;
    pf_sim *neutral_sim = NULL;
    pf_sim *di_sim = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection neutral_inspection;
    pf_m4_inspection di_inspection;
    int32_t hit_x;
    int32_t hit_y;
    int32_t first_pulse_x;
    uint32_t freeze_tick;
    int64_t neutral_speed_squared;
    int64_t di_speed_squared;
    int64_t speed_difference;

    if (!initialize_sim(
            &sdi_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sdi_sim) ||
        !initialize_sim(
            &neutral_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &neutral_sim) ||
        !initialize_sim(
            &di_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &di_sim) ||
        !start_reaction_hit(sdi_sim, &inspection))
    {
        return fail("reaction-init");
    }

    hit_x = inspection.players[1].position_x_q16;
    hit_y = inspection.players[1].position_y_q16;
    if (!step_reaction_duel(
            sdi_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
        inspection.players[1].position_x_q16 <= hit_x ||
        inspection.players[1].position_y_q16 != hit_y)
    {
        return fail("sdi-first-component");
    }
    first_pulse_x = inspection.players[1].position_x_q16;

    if (!step_reaction_duel(
            sdi_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
        inspection.players[1].position_x_q16 != first_pulse_x ||
        !step_reaction_duel(
            sdi_sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(-32767),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].sdi_pulse_count != UINT8_C(2) ||
        inspection.players[1].position_x_q16 <= first_pulse_x ||
        inspection.players[1].position_y_q16 >= hit_y)
    {
        return fail("sdi-hold-and-quarter-circle");
    }

    if (!start_reaction_hit(neutral_sim, &neutral_inspection) ||
        !start_reaction_hit(di_sim, &di_inspection))
    {
        return fail("di-hit-setup");
    }
    for (freeze_tick = UINT32_C(0);
         freeze_tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++freeze_tick)
    {
        const int16_t di_y =
            freeze_tick + UINT32_C(1) ==
                    (uint32_t)content->fighter.jab_hitlag_ticks
                ? INT16_C(-32767)
                : INT16_C(0);

        if (!step_reaction_duel(
                neutral_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &neutral_inspection) ||
            !step_reaction_duel(
                di_sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                di_y,
                UINT64_C(0),
                UINT16_C(0),
                &di_inspection))
        {
            return fail("di-freeze-step");
        }
    }

    neutral_speed_squared =
        (int64_t)neutral_inspection.players[1].velocity_x_q16 *
            (int64_t)neutral_inspection.players[1].velocity_x_q16 +
        (int64_t)neutral_inspection.players[1].velocity_y_q16 *
            (int64_t)neutral_inspection.players[1].velocity_y_q16;
    di_speed_squared =
        (int64_t)di_inspection.players[1].velocity_x_q16 *
            (int64_t)di_inspection.players[1].velocity_x_q16 +
        (int64_t)di_inspection.players[1].velocity_y_q16 *
            (int64_t)di_inspection.players[1].velocity_y_q16;
    speed_difference =
        di_speed_squared >= neutral_speed_squared
            ? di_speed_squared - neutral_speed_squared
            : neutral_speed_squared - di_speed_squared;
    if (neutral_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        di_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        neutral_inspection.players[1].tumble != UINT8_C(1) ||
        di_inspection.players[1].velocity_y_q16 >=
            neutral_inspection.players[1].velocity_y_q16 ||
        di_inspection.players[1].velocity_x_q16 >=
            neutral_inspection.players[1].velocity_x_q16 ||
        speed_difference > neutral_speed_squared / INT64_C(100))
    {
        return fail("trajectory-di-angle-and-magnitude");
    }
    return 1;
}

static int run_until_reaction_landing(
    pf_sim *sim,
    int tech_mode,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    int trigger_sent = 0;

    if (!start_reaction_hit(sim, out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const pf_m4_player_inspection *target =
            &out_inspection->players[1];
        const int should_trigger =
            tech_mode != 0 &&
            trigger_sent == 0 &&
            target->action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            target->velocity_y_q16 > INT32_C(0) &&
            target->position_y_q16 +
                    INT32_C(4) * PF_Q16_ONE >=
                INT32_C(32) * PF_Q16_ONE;
        const int16_t target_x =
            tech_mode > 1 &&
                    (should_trigger || trigger_sent != 0)
                ? INT16_C(32767)
                : INT16_C(0);

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                INT16_C(0),
                UINT64_C(0),
                should_trigger
                    ? UINT16_MAX
                    : UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
        if (should_trigger)
        {
            trigger_sent = 1;
        }
        if (out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_TECH_ROLL)
        {
            return tech_mode == 0 || trigger_sent != 0;
        }
    }
    return 0;
}

static int advance_missed_tech_to_down_wait(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint16_t knockdown_tick;

    if (!run_until_reaction_landing(
            sim,
            0,
            out_inspection) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
        out_inspection->players[1].action_ticks != UINT16_C(0) ||
        out_inspection->players[1].invulnerable != UINT8_C(0))
    {
        return 0;
    }

    for (knockdown_tick = UINT16_C(1);
         knockdown_tick <= content->fighter.knockdown_ticks;
         ++knockdown_tick)
    {
        if (!step_reaction_duel(
                sim,
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
        if (knockdown_tick < content->fighter.knockdown_ticks)
        {
            if (out_inspection->players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
                out_inspection->players[1].action_ticks !=
                    knockdown_tick ||
                out_inspection->players[1].invulnerable !=
                    UINT8_C(0))
            {
                return 0;
            }
        }
    }
    return out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_WAIT &&
           out_inspection->players[1].action_ticks == UINT16_C(0) &&
           out_inspection->players[1].invulnerable == UINT8_C(0);
}

static int advance_strong_missed_tech_to_down_wait(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    uint16_t knockdown_tick;
    int hit_seen = 0;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1) &&
            event->detail ==
                (uint16_t)PF_M4_ACTION_STRONG_ATTACK)
        {
            hit_seen = 1;
        }
        if (out_inspection->players[1].action_state ==
            (uint8_t)PF_M4_ACTION_KNOCKDOWN)
        {
            break;
        }
        if (!step_reaction_duel(
                sim,
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
    if (tick == UINT32_C(240) || hit_seen == 0 ||
        out_inspection->players[1].action_ticks != UINT16_C(0) ||
        out_inspection->players[1].tumble != UINT8_C(0))
    {
        return 0;
    }
    for (knockdown_tick = UINT16_C(1);
         knockdown_tick <= content->fighter.knockdown_ticks;
         ++knockdown_tick)
    {
        if (!step_reaction_duel(
                sim,
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
    return out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_DOWN_WAIT &&
           out_inspection->players[1].action_ticks == UINT16_C(0) &&
           out_inspection->players[1].grounded == UINT8_C(1) &&
           out_inspection->players[1].invulnerable == UINT8_C(0);
}

static int run_knockdown_and_tech_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage missed_storage;
    test_sim_storage in_place_storage;
    test_sim_storage roll_storage;
    pf_sim *missed = NULL;
    pf_sim *in_place = NULL;
    pf_sim *roll = NULL;
    pf_m4_inspection missed_inspection;
    pf_m4_inspection in_place_inspection;
    pf_m4_inspection roll_inspection;

    if (!initialize_sim(
            &missed_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &missed) ||
        !initialize_sim(
            &in_place_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &in_place) ||
        !initialize_sim(
            &roll_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &roll) ||
        !run_until_reaction_landing(
            missed,
            0,
            &missed_inspection) ||
        !run_until_reaction_landing(
            in_place,
            1,
            &in_place_inspection) ||
        !run_until_reaction_landing(
            roll,
            2,
            &roll_inspection))
    {
        return fail("tech-landing-setup");
    }

    if (missed_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_KNOCKDOWN ||
        missed_inspection.players[1].grounded != UINT8_C(1) ||
        in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        in_place_inspection.players[1].tech_direction != INT8_C(0) ||
        in_place_inspection.players[1].tech_window_ticks !=
            UINT16_C(0) ||
        in_place_inspection.players[1].tech_lockout_ticks ==
            UINT16_C(0) ||
        in_place_inspection.players[1].invulnerable != UINT8_C(1) ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(1) ||
        roll_inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        roll_inspection.players[1].invulnerable != UINT8_C(1) ||
        missed_inspection.players[1].invulnerable != UINT8_C(0) ||
        roll_inspection.players[1].tumble != UINT8_C(0))
    {
        return fail("missed-tech-in-place-and-roll");
    }
    while (in_place_inspection.players[1].action_ticks <
           content->fighter.tech_invulnerability_ticks)
    {
        if (!step_reaction_duel(
                in_place,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &in_place_inspection))
        {
            return fail("tech-invulnerability-duration-step");
        }
        if (in_place_inspection.players[1].action_ticks <
                content->fighter.tech_invulnerability_ticks &&
            in_place_inspection.players[1].invulnerable !=
                UINT8_C(1))
        {
            return fail("tech-invulnerability-ended-early");
        }
    }
    if (in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        in_place_inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("tech-invulnerability-exact-end");
    }
    return 1;
}

static int16_t tech_chase_axis(
    const pf_m4_inspection *inspection)
{
    const int32_t delta =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;

    if (delta >
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return INT16_MAX;
    }
    if (delta > PF_Q16_ONE / INT32_C(2))
    {
        return INT16_C(13500);
    }
    if (delta <
        -(INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return -INT16_MAX;
    }
    if (delta < -PF_Q16_ONE / INT32_C(2))
    {
        return -INT16_C(13500);
    }
    return INT16_C(0);
}

static int tech_chase_jab_in_range(
    const pf_m4_content *content,
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
        (int64_t)content->fighter.jab_hitbox_offset_x_q16 +
        (int64_t)content->fighter.jab_hitbox_half_width_q16 +
        (int64_t)content->fighter.half_width_q16;

    return delta != INT64_C(0) &&
           attacker->facing == direction &&
           distance <= reach;
}

static int run_until_tech_chase_landing(
    pf_sim *sim,
    int tech_mode,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;
    int trigger_sent = 0;

    if (!start_reaction_hit(sim, out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(240); ++tick)
    {
        const pf_m4_player_inspection *target =
            &out_inspection->players[1];
        const int should_trigger =
            trigger_sent == 0 &&
            target->action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG &&
            target->velocity_y_q16 > INT32_C(0) &&
            target->position_y_q16 +
                    INT32_C(4) * PF_Q16_ONE >=
                INT32_C(32) * PF_Q16_ONE;
        const int16_t target_x =
            tech_mode > 1 &&
                    (should_trigger || trigger_sent != 0)
                ? INT16_MAX
                : INT16_C(0);

        if (!step_reaction_duel(
                sim,
                tech_chase_axis(out_inspection),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_x,
                INT16_C(0),
                UINT64_C(0),
                should_trigger
                    ? UINT16_MAX
                    : UINT16_C(0),
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

static int run_tech_chase_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage in_place_storage;
    test_sim_storage roll_storage;
    test_sim_storage loaded_storage;
    test_sim_storage miss_storage;
    pf_sim *in_place = NULL;
    pf_sim *roll = NULL;
    pf_sim *loaded = NULL;
    pf_sim *miss = NULL;
    pf_m4_inspection in_place_inspection;
    pf_m4_inspection roll_inspection;
    pf_m4_inspection loaded_inspection;
    pf_m4_inspection miss_inspection;
    pf_state_hash roll_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t initial_damage;
    uint32_t tick;
    uint16_t attack_action_tick = UINT16_MAX;
    int attack_sent = 0;
    int saw_hit = 0;

    if (!initialize_sim(
            &in_place_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &in_place) ||
        !initialize_sim(
            &roll_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &roll) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !initialize_sim(
            &miss_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &miss) ||
        !run_until_tech_chase_landing(
            in_place,
            1,
            &in_place_inspection) ||
        !run_until_tech_chase_landing(
            roll,
            2,
            &roll_inspection) ||
        !run_until_tech_chase_landing(
            miss,
            2,
            &miss_inspection))
    {
        return fail("tech-chase-init");
    }
    if (in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        miss_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(1) ||
        in_place_inspection.players[1].damage_q16 !=
            roll_inspection.players[1].damage_q16 ||
        roll_inspection.players[1].damage_q16 !=
            miss_inspection.players[1].damage_q16)
    {
        return fail("tech-chase-outcome-setup");
    }

    initial_damage = in_place_inspection.players[1].damage_q16;
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        int16_t chaser_x = tech_chase_axis(&in_place_inspection);
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            in_place_inspection.players[1].action_ticks >=
                content->fighter.tech_invulnerability_ticks &&
            tech_chase_jab_in_range(
                content,
                &in_place_inspection))
        {
            attack_sent = 1;
            attack_action_tick =
                in_place_inspection.players[1].action_ticks;
            chaser_x = INT16_C(0);
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                in_place,
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &in_place_inspection))
        {
            return fail("tech-chase-in-place-step");
        }
        if (in_place_inspection.players[1].damage_q16 >
            initial_damage)
        {
            saw_hit = 1;
            break;
        }
    }
    if (attack_sent == 0 || saw_hit == 0 ||
        attack_action_tick + content->fighter.jab_startup_ticks >=
            content->fighter.tech_in_place_ticks ||
        in_place_inspection.players[1].damage_q16 !=
            initial_damage + content->fighter.jab_damage_q16 ||
        in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        in_place_inspection.players[1].last_hit_attacker !=
            UINT8_C(0) ||
        test_last_result.event_count != UINT8_C(1) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT)
    {
        return fail("tech-chase-in-place-punish");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                roll,
                tech_chase_axis(&roll_inspection),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &roll_inspection))
        {
            return fail("tech-chase-roll-presnapshot");
        }
    }
    if (roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_ROLL ||
        roll_inspection.players[1].invulnerable != UINT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(roll, &save_size),
            PF_STATUS_OK,
            "tech-chase-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("tech-chase-roll-snapshot-boundary");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(roll, &destination),
            PF_STATUS_OK,
            "tech-chase-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "tech-chase-load") ||
        !expect_status(
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "tech-chase-loaded-inspect"))
    {
        return 0;
    }

    initial_damage = roll_inspection.players[1].damage_q16;
    attack_action_tick = UINT16_MAX;
    attack_sent = 0;
    saw_hit = 0;
    for (tick = UINT32_C(0); tick < UINT32_C(100); ++tick)
    {
        int16_t chaser_x = tech_chase_axis(&roll_inspection);
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            roll_inspection.players[1].action_ticks >=
                content->fighter.tech_invulnerability_ticks &&
            tech_chase_jab_in_range(content, &roll_inspection))
        {
            attack_sent = 1;
            attack_action_tick =
                roll_inspection.players[1].action_ticks;
            chaser_x = INT16_C(0);
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                roll,
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &roll_inspection) ||
            !step_reaction_duel(
                loaded,
                chaser_x,
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(roll, &roll_hash),
                PF_STATUS_OK,
                "tech-chase-roll-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "tech-chase-loaded-hash") ||
            !hash_equal(&roll_hash, &loaded_hash))
        {
            return fail("tech-chase-roll-continuation");
        }
        if (roll_inspection.players[1].damage_q16 >
            initial_damage)
        {
            saw_hit = 1;
            break;
        }
    }
    if (attack_sent == 0 || saw_hit == 0 ||
        attack_action_tick + content->fighter.jab_startup_ticks >=
            content->fighter.tech_roll_ticks ||
        roll_inspection.players[1].damage_q16 !=
            initial_damage + content->fighter.jab_damage_q16 ||
        loaded_inspection.players[1].damage_q16 !=
            roll_inspection.players[1].damage_q16 ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return fail("tech-chase-roll-punish");
    }

    initial_damage = miss_inspection.players[1].damage_q16;
    attack_sent = 0;
    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        uint64_t chaser_buttons = UINT64_C(0);

        if (attack_sent == 0 &&
            miss_inspection.players[1].action_ticks >=
                content->fighter.tech_invulnerability_ticks)
        {
            if (tech_chase_jab_in_range(content, &miss_inspection))
            {
                return fail("tech-chase-static-negative-spacing");
            }
            attack_sent = 1;
            chaser_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                miss,
                INT16_C(0),
                INT16_C(0),
                chaser_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &miss_inspection))
        {
            return fail("tech-chase-static-negative-step");
        }
    }
    if (attack_sent == 0 ||
        miss_inspection.players[1].damage_q16 != initial_damage ||
        miss_inspection.players[1].last_hit_sequence != UINT32_C(1))
    {
        return fail("tech-chase-static-negative-result");
    }
    return 1;
}

static int run_floor_getup_option_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage up_storage;
    test_sim_storage shield_storage;
    test_sim_storage roll_storage;
    test_sim_storage auto_storage;
    test_sim_storage attack_storage;
    pf_sim *up = NULL;
    pf_sim *shield = NULL;
    pf_sim *roll = NULL;
    pf_sim *automatic = NULL;
    pf_sim *attack = NULL;
    pf_m4_inspection up_inspection;
    pf_m4_inspection shield_inspection;
    pf_m4_inspection roll_inspection;
    pf_m4_inspection auto_inspection;
    pf_m4_inspection attack_inspection;
    uint16_t tick;

    if (!initialize_sim(
            &up_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &up) ||
        !initialize_sim(
            &shield_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &shield) ||
        !initialize_sim(
            &roll_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &roll) ||
        !initialize_sim(
            &auto_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &automatic) ||
        !initialize_sim(
            &attack_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &attack) ||
        !advance_missed_tech_to_down_wait(
            content,
            up,
            &up_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            shield,
            &shield_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            roll,
            &roll_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            automatic,
            &auto_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            attack,
            &attack_inspection))
    {
        return fail("floor-getup-down-wait-setup");
    }

    if (!step_reaction_duel(
            up,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(-32767),
            UINT64_C(0),
            UINT16_C(0),
            &up_inspection) ||
        up_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        up_inspection.players[1].action_ticks != UINT16_C(0) ||
        up_inspection.players[1].invulnerable != UINT8_C(1) ||
        !step_reaction_duel(
            shield,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &shield_inspection) ||
        shield_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        shield_inspection.players[1].tech_window_ticks !=
            content->fighter.tech_window_ticks ||
        shield_inspection.players[1].tech_lockout_ticks !=
            content->fighter.tech_lockout_ticks ||
        !step_reaction_duel(
            roll,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &roll_inspection) ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        roll_inspection.players[1].tech_direction != INT8_C(-1) ||
        roll_inspection.players[1].velocity_x_q16 >= INT32_C(0) ||
        roll_inspection.players[1].invulnerable != UINT8_C(1) ||
        !step_reaction_duel(
            attack,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &attack_inspection) ||
        attack_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
        attack_inspection.players[1].action_ticks != UINT16_C(0) ||
        attack_inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("floor-getup-input-routing");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_neutral_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                up,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &up_inspection))
        {
            return fail("neutral-getup-duration-step");
        }
        if (tick < content->fighter.getup_neutral_ticks &&
            (up_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
             up_inspection.players[1].invulnerable !=
                 (tick <
                          content->fighter
                              .getup_neutral_invulnerability_ticks
                      ? UINT8_C(1)
                      : UINT8_C(0))))
        {
            return fail("neutral-getup-duration-or-invulnerability");
        }
    }
    if (up_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        up_inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("neutral-getup-exact-end");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_roll_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                roll,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &roll_inspection))
        {
            return fail("getup-roll-duration-step");
        }
        if (tick < content->fighter.getup_roll_ticks &&
            (roll_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
             roll_inspection.players[1].invulnerable !=
                 (tick <
                          content->fighter
                              .getup_roll_invulnerability_ticks
                      ? UINT8_C(1)
                      : UINT8_C(0))))
        {
            return fail("getup-roll-duration-or-invulnerability");
        }
    }
    if (roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        roll_inspection.players[1].tech_direction != INT8_C(0))
    {
        return fail("getup-roll-exact-end");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.down_wait_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                automatic,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &auto_inspection))
        {
            return fail("automatic-neutral-getup-step");
        }
        if (tick < content->fighter.down_wait_ticks &&
            (auto_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_DOWN_WAIT ||
             auto_inspection.players[1].action_ticks != tick))
        {
            return fail("down-wait-persistence");
        }
    }
    if (auto_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_NEUTRAL ||
        auto_inspection.players[1].action_ticks != UINT16_C(0) ||
        auto_inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("automatic-neutral-getup");
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_attack_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                attack,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &attack_inspection))
        {
            return fail("getup-attack-duration-step");
        }
        if (tick < content->fighter.getup_attack_ticks &&
            (attack_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GETUP_ATTACK ||
             attack_inspection.players[1].invulnerable !=
                 (tick <
                          content->fighter
                              .getup_attack_invulnerability_ticks
                      ? UINT8_C(1)
                      : UINT8_C(0))))
        {
            return fail("getup-attack-duration-or-invulnerability");
        }
    }
    if (attack_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        attack_inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("getup-attack-exact-end");
    }
    return 1;
}

static int run_getup_attack_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage front_storage;
    test_sim_storage back_storage;
    pf_sim *front = NULL;
    pf_sim *back = NULL;
    pf_m4_inspection front_inspection;
    pf_m4_inspection back_inspection;
    uint16_t tick;
    int front_hit = 0;
    int back_hit = 0;

    if (!initialize_sim(
            &front_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &front) ||
        !initialize_sim(
            &back_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &back) ||
        !advance_missed_tech_to_down_wait(
            content,
            front,
            &front_inspection) ||
        !advance_missed_tech_to_down_wait(
            content,
            back,
            &back_inspection) ||
        !step_reaction_duel(
            front,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &front_inspection))
    {
        return fail("getup-attack-hit-setup");
    }

    for (tick = UINT16_C(1);
         tick <=
             content->fighter.getup_attack_front_active_end_tick;
         ++tick)
    {
        if (!step_reaction_duel(
                front,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &front_inspection))
        {
            return fail("getup-attack-front-step");
        }
        if (front_inspection.players[0].damage_q16 != UINT32_C(0))
        {
            front_hit = 1;
            break;
        }
    }
    if (front_hit == 0 ||
        front_inspection.players[0].damage_q16 !=
            content->fighter.getup_attack_damage_q16 ||
        front_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        front_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        (uint32_t)front_inspection.players[1].action_ticks +
                UINT32_C(1) <
            (uint32_t)content->fighter
                .getup_attack_front_active_begin_tick ||
        (uint32_t)front_inspection.players[1].action_ticks +
                UINT32_C(1) >
            (uint32_t)content->fighter
                .getup_attack_front_active_end_tick)
    {
        return fail("getup-attack-front-hit");
    }

    for (tick = UINT16_C(0); tick < UINT16_C(8); ++tick)
    {
        if (!step_reaction_duel(
                back,
                INT16_C(32767),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &back_inspection))
        {
            return fail("getup-attack-back-position");
        }
    }
    for (tick = UINT16_C(0); tick < UINT16_C(2); ++tick)
    {
        if (!step_reaction_duel(
                back,
                INT16_C(0),
                INT16_C(32767),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &back_inspection))
        {
            return fail("getup-attack-back-stop");
        }
    }
    if (back_inspection.players[0].position_x_q16 <=
            back_inspection.players[1].position_x_q16 +
                content->fighter.half_width_q16 ||
        !step_reaction_duel(
            back,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &back_inspection))
    {
        return fail("getup-attack-back-side-setup");
    }

    for (tick = UINT16_C(1);
         tick <=
             content->fighter.getup_attack_back_active_end_tick;
         ++tick)
    {
        if (!step_reaction_duel(
                back,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &back_inspection))
        {
            return fail("getup-attack-back-step");
        }
        if (back_inspection.players[0].damage_q16 != UINT32_C(0))
        {
            if (tick <
                content->fighter
                    .getup_attack_back_active_begin_tick -
                    UINT16_C(1))
            {
                return fail("getup-attack-back-hit-too-early");
            }
            back_hit = 1;
            break;
        }
    }
    if (back_hit == 0 ||
        back_inspection.players[0].damage_q16 !=
            content->fighter.getup_attack_damage_q16 ||
        back_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        back_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        (uint32_t)back_inspection.players[1].action_ticks +
                UINT32_C(1) <
            (uint32_t)content->fighter
                .getup_attack_back_active_begin_tick ||
        (uint32_t)back_inspection.players[1].action_ticks +
                UINT32_C(1) >
            (uint32_t)content->fighter
                .getup_attack_back_active_end_tick)
    {
        return fail("getup-attack-back-hit");
    }
    return 1;
}

static int run_floor_recovery_snapshot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint16_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !advance_missed_tech_to_down_wait(
            content,
            source,
            &source_inspection) ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        source_inspection.players[1].tech_direction != INT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-floor-recovery-save-size") ||
        save_size != (size_t)694)
    {
        return fail("floor-recovery-snapshot-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-floor-recovery") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-floor-recovery") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-floor-recovery") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-floor-recovery") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("floor-recovery-snapshot-round-trip");
    }

    for (tick = UINT16_C(0);
         tick <
             content->fighter.getup_roll_ticks + UINT16_C(5);
         ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-floor-recovery-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-floor-recovery-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("floor-recovery-deterministic-continuation");
        }
    }
    return source_inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                   loaded_inspection.players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE
               ? 1
               : fail("floor-recovery-snapshot-exact-end");
}

static int run_tech_invulnerability_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    const uint32_t initial_damage =
        content->fighter.jab_damage_q16;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !run_until_reaction_landing(sim, 1, &inspection) ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].damage_q16 != initial_damage ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug tech-invulnerability "
            "p0_action=%u p0_hitbox=%u p0_x=%" PRId32
            " p1_action=%u p1_ticks=%u p1_invulnerable=%u "
            "p1_damage=%" PRIu32 " p1_x=%" PRId32
            " p1_y=%" PRId32 " p1_vy=%" PRId32
            " tech_window=%u tech_lockout=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].hitbox_active,
            inspection.players[0].position_x_q16,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16,
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            inspection.players[1].velocity_y_q16,
            (unsigned int)inspection.players[1].tech_window_ticks,
            (unsigned int)inspection.players[1].tech_lockout_ticks);
        return fail("tech-invulnerability-rejects-hit");
    }

    while (inspection.players[1].action_ticks <
               content->fighter.tech_invulnerability_ticks ||
           inspection.players[0].action_state !=
               (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return fail("tech-vulnerability-window-setup");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_TECH_IN_PLACE ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[1].damage_q16 !=
            initial_damage + content->fighter.jab_damage_q16 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return fail("tech-vulnerability-restores-hit");
    }
    return 1;
}

static int run_air_dodge_invulnerability_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_JUMP,
                UINT16_C(0),
                &inspection))
        {
            return fail("air-dodge-invulnerability-jump");
        }
        if (inspection.players[1].grounded == UINT8_C(0))
        {
            break;
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !step_reaction_duel(
            sim,
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
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[1].action_ticks !=
            (uint16_t)(
                content->fighter
                    .air_dodge_invulnerability_begin_tick +
                UINT16_C(1)) ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug air-dodge-invulnerability"
            " p0_action=%u p0_ticks=%u p0_hitbox=%u"
            " p0_y=%" PRId32 " p1_action=%u p1_ticks=%u"
            " p1_invulnerable=%u p1_damage=%" PRIu32
            " p1_y=%" PRId32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active,
            inspection.players[0].position_y_q16,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16,
            inspection.players[1].position_y_q16);
        return fail("air-dodge-invulnerability-rejects-hit");
    }

    while (inspection.players[1].action_ticks <
               (uint16_t)(
                   content->fighter
                       .air_dodge_invulnerability_end_tick -
                   UINT16_C(3)) ||
           inspection.players[0].action_state !=
               (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("air-dodge-vulnerability-window-setup");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE ||
        inspection.players[1].action_ticks !=
            (uint16_t)(
                content->fighter
                    .air_dodge_invulnerability_end_tick -
                UINT16_C(3)) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug air-dodge-expired"
            " p0_action=%u p0_ticks=%u p0_hitbox=%u"
            " p1_action=%u p1_ticks=%u p1_invulnerable=%u"
            " p1_damage=%" PRIu32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16);
        return fail("air-dodge-expired-window-accepts-hit");
    }
    return 1;
}

static int run_ground_dodge_invulnerability_hit_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        inspection.players[1].action_ticks != UINT16_C(1) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16)
    {
        return fail("spot-dodge-startup-accepts-hit");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5d07d0d6e)),
            PF_STATUS_OK,
            "spot-dodge-invulnerable-reset") ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        inspection.players[1].action_ticks !=
            content->fighter.spot_dodge_invulnerability_begin_tick ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("spot-dodge-invulnerability-rejects-hit");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5d07d0d6f)),
            PF_STATUS_OK,
            "spot-dodge-expired-reset") ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    while (inspection.players[1].action_ticks <
           (uint16_t)(
               content->fighter
                   .spot_dodge_invulnerability_end_tick -
               UINT16_C(3)))
    {
        if (!step_reaction_duel(
                sim,
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
            return fail("spot-dodge-expired-window-setup");
        }
    }
    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].invulnerable != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug spot-dodge-expired"
            " action=%u ticks=%u invulnerable=%u damage=%" PRIu32 "\n",
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16);
        return fail("spot-dodge-expired-window-accepts-hit");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x5d07d0d70)),
            PF_STATUS_OK,
            "roll-invulnerable-reset") ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_MIN,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].hitbox_active != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.players[1].action_ticks !=
            content->fighter.roll_invulnerability_begin_tick ||
        inspection.players[1].invulnerable != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("roll-invulnerability-rejects-hit");
    }
    return 1;
}

static int run_hitlag_snapshot_test(const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !step_duel(
            source,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        !step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-combat-save-size") ||
        save_size != (size_t)694)
    {
        return fail("mid-hitlag-save-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-mid-hitlag") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-mid-hitlag") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-mid-hitlag") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-mid-hitlag") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("mid-hitlag-round-trip");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        const uint64_t player0_buttons =
            tick == UINT32_C(20) ? PF_INPUT_BUTTON_ATTACK : UINT64_C(0);
        const uint64_t player1_buttons =
            tick == UINT32_C(38) ? PF_INPUT_BUTTON_ATTACK : UINT64_C(0);

        if (!step_duel(
                source,
                INT16_C(0),
                player0_buttons,
                INT16_C(0),
                player1_buttons,
                &source_inspection) ||
            !step_duel(
                loaded,
                INT16_C(0),
                player0_buttons,
                INT16_C(0),
                player1_buttons,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("mid-hitlag-deterministic-continuation");
        }
    }
    return 1;
}

static int run_shield_hitlag_snapshot_test(
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !start_normal_shield_block(source, &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        source_inspection.players[1].shield_stun_ticks ==
            UINT16_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-shield-save-size") ||
        save_size != (size_t)694)
    {
        return fail("mid-shield-hitlag-save-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "save-mid-shield-hitlag") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "load-mid-shield-hitlag") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "hash-source-mid-shield-hitlag") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "hash-loaded-mid-shield-hitlag") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("mid-shield-hitlag-round-trip");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &source_inspection) ||
            !step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &loaded_inspection) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "hash-source-shield-continuation") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "hash-loaded-shield-continuation") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail(
                "mid-shield-hitlag-deterministic-continuation");
        }
    }
    return 1;
}

static void make_trace_inputs(
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS],
    uint64_t tick)
{
    uint32_t player_index;

    make_inputs(inputs, UINT8_C(4), tick);
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const uint64_t phase =
            (tick / UINT64_C(29) + (uint64_t)player_index) %
            UINT64_C(4);
        const uint64_t attack_period =
            UINT64_C(23) + UINT64_C(2) * (uint64_t)player_index;
        const uint64_t jump_period =
            UINT64_C(71) + UINT64_C(3) * (uint64_t)player_index;

        if (phase == UINT64_C(0))
        {
            inputs[player_index].main_stick_x = INT16_C(32767);
        }
        else if (phase == UINT64_C(2))
        {
            inputs[player_index].main_stick_x = INT16_C(-32767);
        }
        if (tick % attack_period == (uint64_t)player_index)
        {
            inputs[player_index].buttons |= PF_INPUT_BUTTON_ATTACK;
        }
        if (tick % jump_period ==
            UINT64_C(5) + (uint64_t)player_index)
        {
            inputs[player_index].buttons |= PF_INPUT_BUTTON_JUMP;
        }
        if (tick % UINT64_C(97) ==
            UINT64_C(11) + (uint64_t)player_index)
        {
            inputs[player_index].main_stick_y = INT16_C(32767);
        }
        if ((tick + UINT64_C(5) * (uint64_t)player_index) %
                UINT64_C(127) <
            UINT64_C(12))
        {
            inputs[player_index].left_trigger = UINT16_MAX;
        }
    }
}

static int run_deterministic_trace(const pf_content_view *view)
{
    test_sim_storage left_storage;
    test_sim_storage right_storage;
    pf_sim *left = NULL;
    pf_sim *right = NULL;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result left_result;
    pf_tick_result right_result;
    pf_state_hash left_hash;
    pf_state_hash right_hash;
    pf_m4_inspection inspection;
    uint64_t tick;
    int saw_hit = 0;
    int saw_shield = 0;

    if (!initialize_sim(
            &left_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &left) ||
        !initialize_sim(
            &right_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &right))
    {
        return fail("trace-init");
    }

    for (tick = UINT64_C(0);
         tick < TEST_DETERMINISTIC_TICKS;
         ++tick)
    {
        make_trace_inputs(inputs, tick);
        if (!expect_status(
                pf_sim_tick(
                    left,
                    inputs,
                    (size_t)PF_SIM_MAX_PLAYERS,
                    &left_result),
                PF_STATUS_OK,
                "trace-left-tick") ||
            !expect_status(
                pf_sim_tick(
                    right,
                    inputs,
                    (size_t)PF_SIM_MAX_PLAYERS,
                    &right_result),
                PF_STATUS_OK,
                "trace-right-tick") ||
            !expect_status(
                pf_sim_hash(left, &left_hash),
                PF_STATUS_OK,
                "trace-left-hash") ||
            !expect_status(
                pf_sim_hash(right, &right_hash),
                PF_STATUS_OK,
                "trace-right-hash") ||
            !hash_equal(&left_hash, &right_hash))
        {
            return fail("deterministic-combat-trace");
        }

        if ((tick % UINT64_C(113)) == UINT64_C(0) &&
            expect_status(
                pf_m4_inspect(left, &inspection),
                PF_STATUS_OK,
                "trace-inspect"))
        {
            uint32_t player_index;

            for (player_index = UINT32_C(0);
                 player_index < PF_SIM_MAX_PLAYERS;
                 ++player_index)
            {
                if (inspection.players[player_index].last_hit_valid !=
                    UINT8_C(0))
                {
                    saw_hit = 1;
                }
                if (inspection.players[player_index].action_state ==
                        (uint8_t)PF_M4_ACTION_SHIELD ||
                    inspection.players[player_index]
                            .shield_health_q16 <
                        UINT32_C(60) * UINT32_C(65536))
                {
                    saw_shield = 1;
                }
            }
        }
    }
    return (saw_hit != 0 && saw_shield != 0) ||
           fail("trace-did-not-exercise-combat-and-shield");
}

static int begin_close_grab(
    pf_sim *sim,
    int target_shields,
    pf_m4_inspection *out_inspection,
    pf_sim_event *out_event)
{
    uint32_t tick;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            target_shields != 0 ? UINT16_MAX : UINT16_C(0),
            out_inspection))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(10); ++tick)
    {
        const pf_sim_event *event = find_last_tick_event(PF_SIM_EVENT_GRAB);

        if (event != NULL)
        {
            *out_event = *event;
            return 1;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                target_shields != 0 ? UINT16_MAX : UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return 0;
}

static int advance_to_settled_run(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.initial_dash_ticks +
                 UINT32_C(16);
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_MAX,
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
                content->fighter.run_speed_q16)
        {
            return 1;
        }
    }
    return fail("boost-grab-reach-settled-run");
}

static int advance_jab_to_action_tick(
    pf_sim *sim,
    const pf_m4_content *content,
    uint16_t target_action_tick,
    pf_m4_inspection *out_inspection,
    int *out_hit_seen)
{
    uint32_t tick;
    int hit_seen = 0;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

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
        if (!step_reaction_duel(
                sim,
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
    (void)content;
    return fail("jab-cancel-reach-action-tick");
}

static int hit_down_wait_target(
    pf_sim *sim,
    uint64_t attack_button,
    pf_m4_inspection *out_inspection,
    pf_sim_event *out_event)
{
    uint32_t tick;

    if (out_event == NULL ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            attack_button,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            *out_event = *event;
            return 1;
        }
        if (!step_reaction_duel(
                sim,
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
    return 0;
}

static int advance_hitlag_to_action(
    pf_sim *sim,
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
        if (!step_reaction_duel(
                sim,
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
    return 0;
}

static int run_jab_reset_test(
    const pf_m4_content *content,
    const pf_content_view *view,
    const pf_m4_content *exact_content,
    const pf_content_view *exact_view,
    const pf_content_view *over_damage_view,
    const pf_content_view *over_hitstun_view)
{
    test_sim_storage positive_storage;
    test_sim_storage exact_storage;
    test_sim_storage over_damage_storage;
    test_sim_storage over_hitstun_storage;
    test_sim_storage getup_storage;
    test_sim_storage sdi_storage;
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *positive = NULL;
    pf_sim *exact = NULL;
    pf_sim *over_damage = NULL;
    pf_sim *over_hitstun = NULL;
    pf_sim *getup = NULL;
    pf_sim *sdi = NULL;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_event hit_event;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;
    int airborne_seen = 0;
    int grounded_seen = 0;
    int strong_sent = 0;
    int punish_seen = 0;

    if (content->fighter.reset_max_damage_q16 !=
            UINT32_C(7) * UINT32_C(65536) ||
        content->fighter.reset_max_hitstun_ticks != UINT16_C(12) ||
        content->fighter.reset_bound_ticks != UINT16_C(12) ||
        content->fighter.reset_forced_getup_ticks != UINT16_C(30) ||
        content->fighter.reset_bound_speed_q16 !=
            PF_Q16_ONE / INT32_C(10))
    {
        return fail("jab-reset-authored-defaults");
    }

    if (!initialize_sim(
            &positive_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &positive) ||
        !advance_strong_missed_tech_to_down_wait(
            content,
            positive,
            &inspection) ||
        !hit_down_wait_target(
            positive,
            PF_INPUT_BUTTON_ATTACK,
            &inspection,
            &hit_event) ||
        hit_event.detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK ||
        hit_event.value_q16 != content->fighter.jab_damage_q16 ||
        hit_event.velocity_y_q16 !=
            -content->fighter.reset_bound_speed_q16 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 +
                content->fighter.jab_damage_q16 ||
        inspection.players[1].hitstun_ticks !=
            content->fighter.reset_max_hitstun_ticks ||
        inspection.players[1].tumble != UINT8_C(0) ||
        !advance_hitlag_to_action(
            positive,
            (uint8_t)PF_M4_ACTION_RESET_BOUND,
            &inspection) ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].grounded != UINT8_C(0) ||
        inspection.players[1].velocity_y_q16 !=
            -content->fighter.reset_bound_speed_q16 ||
        inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("jab-reset-positive-entry");
    }
    for (tick = UINT32_C(1);
         tick <= (uint32_t)content->fighter.reset_bound_ticks;
         ++tick)
    {
        const uint64_t target_buttons =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : PF_INPUT_BUTTON_JUMP;

        if (!step_reaction_duel(
                positive,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                target_buttons,
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        airborne_seen |=
            inspection.players[1].grounded == UINT8_C(0);
        grounded_seen |=
            inspection.players[1].grounded != UINT8_C(0);
        if (tick < (uint32_t)content->fighter.reset_bound_ticks &&
            (inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_RESET_BOUND ||
             inspection.players[1].action_ticks != (uint16_t)tick ||
             inspection.players[1].invulnerable != UINT8_C(0)))
        {
            return fail("jab-reset-bound-duration-or-input-lock");
        }
    }
    if (airborne_seen == 0 || grounded_seen == 0 ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_FORCED_GETUP ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].grounded != UINT8_C(1) ||
        inspection.players[1].invulnerable != UINT8_C(0))
    {
        return fail("jab-reset-exact-bound-to-forced-getup");
    }
    for (tick = UINT32_C(1);
         tick <=
             (uint32_t)content->fighter.reset_forced_getup_ticks;
         ++tick)
    {
        const uint64_t target_buttons =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : PF_INPUT_BUTTON_JUMP;

        if (!step_reaction_duel(
                positive,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                target_buttons,
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (tick <
                (uint32_t)content->fighter
                    .reset_forced_getup_ticks &&
            (inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_FORCED_GETUP ||
             inspection.players[1].action_ticks != (uint16_t)tick ||
             inspection.players[1].invulnerable != UINT8_C(0)))
        {
            return fail("jab-reset-forced-getup-duration-or-input-lock");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[1].action_ticks != UINT16_C(0))
    {
        return fail("jab-reset-forced-getup-exact-end");
    }

    if (!initialize_sim(
            &exact_storage,
            exact_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &exact) ||
        !advance_strong_missed_tech_to_down_wait(
            exact_content,
            exact,
            &inspection) ||
        !hit_down_wait_target(
            exact,
            PF_INPUT_BUTTON_ATTACK,
            &inspection,
            &hit_event) ||
        hit_event.value_q16 !=
            exact_content->fighter.reset_max_damage_q16 ||
        inspection.players[1].hitstun_ticks !=
            exact_content->fighter.reset_max_hitstun_ticks ||
        !advance_hitlag_to_action(
            exact,
            (uint8_t)PF_M4_ACTION_RESET_BOUND,
            &inspection))
    {
        return fail("jab-reset-inclusive-damage-hitstun-boundaries");
    }

    if (!initialize_sim(
            &over_damage_storage,
            over_damage_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &over_damage) ||
        !advance_strong_missed_tech_to_down_wait(
            content,
            over_damage,
            &inspection) ||
        !hit_down_wait_target(
            over_damage,
            PF_INPUT_BUTTON_ATTACK,
            &inspection,
            &hit_event) ||
        hit_event.value_q16 <= content->fighter.reset_max_damage_q16 ||
        !advance_hitlag_to_action(
            over_damage,
            (uint8_t)PF_M4_ACTION_HITSTUN,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(0))
    {
        return fail("jab-reset-over-damage-rejected");
    }

    if (!initialize_sim(
            &over_hitstun_storage,
            over_hitstun_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &over_hitstun) ||
        !advance_strong_missed_tech_to_down_wait(
            content,
            over_hitstun,
            &inspection) ||
        !hit_down_wait_target(
            over_hitstun,
            PF_INPUT_BUTTON_ATTACK,
            &inspection,
            &hit_event) ||
        inspection.players[1].hitstun_ticks <=
            content->fighter.reset_max_hitstun_ticks ||
        !advance_hitlag_to_action(
            over_hitstun,
            (uint8_t)PF_M4_ACTION_HITSTUN,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(1))
    {
        return fail("jab-reset-over-hitstun-rejected");
    }

    if (!initialize_sim(
            &getup_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &getup) ||
        !advance_strong_missed_tech_to_down_wait(
            content,
            getup,
            &inspection) ||
        !step_reaction_duel(
            getup,
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
        return fail("jab-reset-getup-avoidance-entry");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_reaction_duel(
                getup,
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
                content->fighter.strong_damage_q16 ||
            find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            return fail("jab-reset-getup-invulnerability-negative");
        }
    }

    if (!initialize_sim(
            &sdi_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sdi) ||
        !advance_strong_missed_tech_to_down_wait(
            content,
            sdi,
            &inspection) ||
        !hit_down_wait_target(
            sdi,
            PF_INPUT_BUTTON_ATTACK,
            &inspection,
            &hit_event))
    {
        return fail("jab-reset-sdi-escape-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
    {
        const int16_t target_y =
            tick == UINT32_C(1) ? INT16_C(0) : INT16_MIN;

        if (!step_reaction_duel(
                sdi,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                target_y,
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (tick == UINT32_C(0))
        {
            if (!initialize_sim(
                    &loaded_storage,
                    view,
                    UINT8_C(2),
                    PF_SIM_MODE_DUEL,
                    1,
                    &loaded) ||
                !expect_status(
                    pf_sim_query_save_size(sdi, &save_size),
                    PF_STATUS_OK,
                    "jab-reset-hitlag-query-save-size") ||
                save_size != (size_t)694)
            {
                return fail("jab-reset-hitlag-snapshot-setup");
            }
            destination.bytes = save_bytes;
            destination.capacity = sizeof(save_bytes);
            destination.size = (size_t)0;
            if (!expect_status(
                    pf_sim_save(sdi, &destination),
                    PF_STATUS_OK,
                    "jab-reset-hitlag-save") ||
                destination.size != save_size)
            {
                return 0;
            }
            source_bytes.bytes = save_bytes;
            source_bytes.size = destination.size;
            if (!expect_status(
                    pf_sim_load(loaded, source_bytes),
                    PF_STATUS_OK,
                    "jab-reset-hitlag-load") ||
                !expect_status(
                    pf_m4_inspect(loaded, &loaded_inspection),
                    PF_STATUS_OK,
                    "jab-reset-hitlag-inspect") ||
                !expect_status(
                    pf_sim_hash(sdi, &source_hash),
                    PF_STATUS_OK,
                    "jab-reset-hitlag-source-hash") ||
                !expect_status(
                    pf_sim_hash(loaded, &loaded_hash),
                    PF_STATUS_OK,
                    "jab-reset-hitlag-loaded-hash") ||
                !hash_equal(&source_hash, &loaded_hash) ||
                loaded_inspection.players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_HITLAG ||
                loaded_inspection.players[1].sdi_pulse_count !=
                    UINT8_C(1))
            {
                return fail("jab-reset-hitlag-save-load");
            }
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_RESET_BOUND ||
        inspection.players[1].sdi_pulse_count != UINT8_C(2))
    {
        return fail("jab-reset-sdi-pulses");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.reset_bound_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sdi,
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
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        inspection.players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sdi,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK)
    {
        return fail("jab-reset-sdi-airborne-control-escape");
    }

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &loaded) ||
        !advance_strong_missed_tech_to_down_wait(
            content,
            source,
            &inspection) ||
        !hit_down_wait_target(
            source,
            PF_INPUT_BUTTON_ATTACK,
            &inspection,
            &hit_event) ||
        !advance_hitlag_to_action(
            source,
            (uint8_t)PF_M4_ACTION_RESET_BOUND,
            &inspection))
    {
        return fail("jab-reset-snapshot-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        if (!step_reaction_duel(
                source,
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
            (uint8_t)PF_M4_ACTION_RESET_BOUND ||
        inspection.players[1].action_ticks != UINT16_C(3) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "jab-reset-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("jab-reset-snapshot-boundary");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "jab-reset-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "jab-reset-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
    {
        uint64_t attacker_buttons = UINT64_C(0);
        const uint64_t target_buttons =
            (tick & UINT32_C(1)) != UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0);

        if (strong_sent == 0 &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_FORCED_GETUP)
        {
            attacker_buttons = PF_INPUT_BUTTON_STRONG_ATTACK;
            strong_sent = 1;
        }
        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                target_buttons,
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        source_result = test_last_result;
        {
            const pf_sim_event *event =
                find_last_tick_event(PF_SIM_EVENT_HIT);

            if (event != NULL &&
                event->detail ==
                    (uint16_t)PF_M4_ACTION_STRONG_ATTACK &&
                event->target_player == UINT8_C(1))
            {
                punish_seen = 1;
            }
        }
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                target_buttons,
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "jab-reset-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "jab-reset-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash) ||
            inspection.players[1].action_state !=
                loaded_inspection.players[1].action_state ||
            inspection.players[1].action_ticks !=
                loaded_inspection.players[1].action_ticks ||
            inspection.players[1].damage_q16 !=
                loaded_inspection.players[1].damage_q16)
        {
            return fail("jab-reset-save-load-continuation");
        }
    }
    if (strong_sent == 0 || punish_seen == 0 ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16 +
                content->fighter.jab_damage_q16 +
                content->fighter.strong_damage_q16)
    {
        return fail("jab-reset-vulnerable-forced-getup-punish");
    }

    return 1;
}

static int run_jab_cancel_test(
    const pf_m4_content *close_content,
    const pf_content_view *close_view,
    const pf_content_view *far_view)
{
    test_sim_storage hit_storage;
    test_sim_storage whiff_storage;
    test_sim_storage early_storage;
    test_sim_storage late_storage;
    test_sim_storage final_storage;
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *hit = NULL;
    pf_sim *whiff = NULL;
    pf_sim *early = NULL;
    pf_sim *late = NULL;
    pf_sim *final = NULL;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t tick;
    int first_hit_seen = 0;
    int final_hit_seen = 0;

    if (close_content->fighter.jab_combo_input_begin_tick !=
            UINT16_C(4) ||
        close_content->fighter.jab_combo_input_end_tick !=
            UINT16_C(7) ||
        close_content->fighter.jab_final_startup_ticks !=
            UINT16_C(2) ||
        close_content->fighter.jab_final_active_ticks !=
            UINT16_C(2) ||
        close_content->fighter.jab_final_recovery_ticks !=
            UINT16_C(10) ||
        close_content->fighter.jab_final_hitlag_ticks !=
            UINT16_C(4) ||
        close_content->fighter.jab_final_damage_q16 !=
            UINT32_C(7) * UINT32_C(65536))
    {
        return fail("jab-cancel-authored-defaults");
    }

    if (!initialize_sim(
            &hit_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &hit) ||
        !advance_jab_to_action_tick(
            hit,
            close_content,
            close_content->fighter.jab_combo_input_begin_tick,
            &inspection,
            &first_hit_seen) ||
        first_hit_seen == 0 ||
        inspection.players[1].damage_q16 !=
            close_content->fighter.jab_damage_q16 ||
        !step_reaction_duel(
            hit,
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
        inspection.players[1].damage_q16 !=
            close_content->fighter.jab_damage_q16)
    {
        return fail("jab-cancel-on-hit");
    }

    if (!initialize_sim(
            &whiff_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &whiff) ||
        !advance_jab_to_action_tick(
            whiff,
            close_content,
            close_content->fighter.jab_combo_input_end_tick,
            &inspection,
            &first_hit_seen) ||
        first_hit_seen != 0 ||
        !step_reaction_duel(
            whiff,
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
        return fail("jab-cancel-on-whiff-and-end-boundary");
    }

    if (!initialize_sim(
            &early_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &early) ||
        !advance_jab_to_action_tick(
            early,
            close_content,
            close_content->fighter.jab_combo_input_begin_tick -
                UINT16_C(1),
            &inspection,
            &first_hit_seen) ||
        !step_reaction_duel(
            early,
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
            close_content->fighter.jab_combo_input_begin_tick ||
        !step_reaction_duel(
            early,
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
        return fail("jab-cancel-early-held-shield-rejected");
    }

    if (!initialize_sim(
            &late_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &late) ||
        !advance_jab_to_action_tick(
            late,
            close_content,
            close_content->fighter.jab_combo_input_end_tick +
                UINT16_C(1),
            &inspection,
            &first_hit_seen) ||
        !step_reaction_duel(
            late,
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
            close_content->fighter.jab_combo_input_end_tick +
                UINT16_C(2))
    {
        return fail("jab-cancel-first-late-frame-rejected");
    }

    if (!initialize_sim(
            &final_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &final) ||
        !advance_jab_to_action_tick(
            final,
            close_content,
            close_content->fighter.jab_combo_input_begin_tick,
            &inspection,
            &first_hit_seen) ||
        first_hit_seen == 0 ||
        !step_reaction_duel(
            final,
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
            (uint8_t)PF_M4_ACTION_JAB_FINAL ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        return fail("jab-combo-final-entry");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1) &&
            event->detail == (uint16_t)PF_M4_ACTION_JAB_FINAL &&
            event->value_q16 ==
                close_content->fighter.jab_final_damage_q16)
        {
            final_hit_seen = 1;
            break;
        }
        if (!step_reaction_duel(
                final,
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
    if (final_hit_seen == 0 ||
        inspection.players[1].damage_q16 !=
            close_content->fighter.jab_damage_q16 +
                close_content->fighter.jab_final_damage_q16)
    {
        return fail("jab-combo-final-hit-and-identity");
    }

    if (!initialize_sim(
            &source_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &loaded) ||
        !advance_jab_to_action_tick(
            source,
            close_content,
            close_content->fighter.jab_combo_input_begin_tick,
            &inspection,
            &first_hit_seen) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "jab-cancel-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("jab-cancel-snapshot-boundary");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "jab-cancel-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "jab-cancel-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const uint16_t trigger =
            tick == UINT32_C(0) ? UINT16_MAX : UINT16_C(0);

        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
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
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "jab-cancel-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "jab-cancel-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash) ||
            inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            inspection.players[0].action_ticks !=
                loaded_inspection.players[0].action_ticks)
        {
            return fail("jab-cancel-save-load-continuation");
        }
        if (tick == UINT32_C(0) &&
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_SHIELD)
        {
            return fail("jab-cancel-loaded-shield-entry");
        }
    }

    return 1;
}

static int run_boost_grab_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage ordinary_storage;
    test_sim_storage window_storage;
    test_sim_storage reverse_chord_storage;
    test_sim_storage boost_storage;
    test_sim_storage late_storage;
    test_sim_storage hit_storage;
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *ordinary = NULL;
    pf_sim *window = NULL;
    pf_sim *reverse_chord = NULL;
    pf_sim *boost = NULL;
    pf_sim *late = NULL;
    pf_sim *hit = NULL;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_event grab_event = {0};
    pf_sim_event hit_event = {0};
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    int32_t ordinary_velocity;
    int32_t ordinary_active_position = INT32_MIN;
    int32_t boost_active_position = INT32_MIN;
    uint16_t hit_entry_tick = UINT16_C(0);
    uint32_t tick;
    int ordinary_capture_seen = 0;
    int boost_capture_seen = 0;
    int dash_hit_seen = 0;
    int snapshot_capture_seen = 0;

    if (content->fighter.dash_attack_speed_q16 !=
            (INT32_C(7) * PF_Q16_ONE) / INT32_C(20) ||
        content->fighter.dash_attack_startup_ticks != UINT16_C(4) ||
        content->fighter.dash_attack_active_ticks != UINT16_C(3) ||
        content->fighter.dash_attack_recovery_ticks != UINT16_C(12) ||
        content->fighter.dash_attack_hitlag_ticks != UINT16_C(5) ||
        content->fighter.boost_grab_cancel_begin_tick != UINT16_C(1) ||
        content->fighter.boost_grab_cancel_end_tick != UINT16_C(3))
    {
        return fail("boost-grab-authored-defaults");
    }

    for (tick =
             (uint32_t)content->fighter
                 .boost_grab_cancel_begin_tick;
         tick <=
             (uint32_t)content->fighter
                 .boost_grab_cancel_end_tick;
         ++tick)
    {
        if (!initialize_sim(
                &window_storage,
                view,
                UINT8_C(2),
                PF_SIM_MODE_DUEL,
                1,
                &window) ||
            !advance_to_settled_run(window, content, &inspection) ||
            !step_reaction_duel(
                window,
                INT16_MAX,
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        while (inspection.players[0].action_ticks <
               (uint16_t)tick)
        {
            if (!step_reaction_duel(
                    window,
                    INT16_MAX,
                    INT16_C(0),
                    PF_INPUT_BUTTON_ATTACK,
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
                (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
            inspection.players[0].action_ticks != (uint16_t)tick ||
            !step_reaction_duel(
                window,
                INT16_MAX,
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
            inspection.players[0].action_ticks != UINT16_C(1) ||
            inspection.players[0].velocity_x_q16 !=
                content->fighter.dash_attack_speed_q16 -
                    (int32_t)(tick + UINT32_C(1)) *
                        content->fighter.traction_q16)
        {
            return fail("boost-grab-every-legal-cancel-frame");
        }
    }

    if (!initialize_sim(
            &reverse_chord_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &reverse_chord) ||
        !advance_to_settled_run(
            reverse_chord,
            content,
            &inspection) ||
        !step_reaction_duel(
            reverse_chord,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            reverse_chord,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(2) ||
        !step_reaction_duel(
            reverse_chord,
            INT16_MAX,
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
            content->fighter.dash_attack_speed_q16 -
                INT32_C(3) * content->fighter.traction_q16)
    {
        return fail("boost-grab-fresh-light-while-shield-held");
    }

    if (!initialize_sim(
            &ordinary_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &ordinary) ||
        !advance_to_settled_run(ordinary, content, &inspection) ||
        !step_reaction_duel(
            ordinary,
            INT16_MAX,
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
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_q16 !=
            content->fighter.run_speed_q16 -
                content->fighter.traction_q16)
    {
        return fail("boost-grab-same-frame-is-ordinary-dash-grab");
    }
    ordinary_velocity = inspection.players[0].velocity_x_q16;
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.grab_startup_ticks +
                 (uint32_t)content->fighter.grab_active_ticks +
                 (uint32_t)content->fighter.grab_recovery_ticks;
         ++tick)
    {
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GRAB &&
            inspection.players[0].action_ticks ==
                content->fighter.grab_startup_ticks + UINT16_C(1))
        {
            ordinary_active_position =
                inspection.players[0].position_x_q16;
        }
        if (find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL)
        {
            ordinary_capture_seen = 1;
            break;
        }
        if (!step_reaction_duel(
                ordinary,
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
    if (ordinary_capture_seen != 0 ||
        ordinary_active_position == INT32_MIN ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("boost-grab-ordinary-range-whiff");
    }

    if (!initialize_sim(
            &boost_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &boost) ||
        !advance_to_settled_run(boost, content, &inspection) ||
        !step_reaction_duel(
            boost,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_q16 !=
            content->fighter.dash_attack_speed_q16 -
                content->fighter.traction_q16 ||
        inspection.players[0].hitbox_active != UINT8_C(0) ||
        !step_reaction_duel(
            boost,
            INT16_MAX,
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
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].velocity_x_q16 !=
            content->fighter.dash_attack_speed_q16 -
                INT32_C(2) * content->fighter.traction_q16 ||
        inspection.players[0].velocity_x_q16 <= ordinary_velocity)
    {
        return fail("boost-grab-legal-window-momentum-transfer");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_GRAB);

        if (event != NULL)
        {
            grab_event = *event;
            boost_active_position =
                inspection.players[0].position_x_q16;
            boost_capture_seen = 1;
            break;
        }
        if (!step_reaction_duel(
                boost,
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
    if (boost_capture_seen == 0 ||
        boost_active_position <= ordinary_active_position ||
        grab_event.source_player != UINT8_C(0) ||
        grab_event.target_player != UINT8_C(1) ||
        grab_event.detail != (uint16_t)PF_M4_ACTION_GRAB ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED)
    {
        return fail("boost-grab-expanded-range-capture");
    }

    if (!initialize_sim(
            &late_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &late) ||
        !advance_to_settled_run(late, content, &inspection) ||
        !step_reaction_duel(
            late,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    while (inspection.players[0].action_ticks <=
           content->fighter.boost_grab_cancel_end_tick)
    {
        if (!step_reaction_duel(
                late,
                INT16_MAX,
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_ticks !=
            content->fighter.boost_grab_cancel_end_tick +
                UINT16_C(1) ||
        !step_reaction_duel(
            late,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("boost-grab-late-cancel-rejected");
    }

    if (!initialize_sim(
            &hit_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &hit) ||
        !advance_to_settled_run(hit, content, &inspection) ||
        !step_reaction_duel(
            hit,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

        if (event != NULL)
        {
            hit_event = *event;
            dash_hit_seen = 1;
            break;
        }
        hit_entry_tick = inspection.players[0].action_ticks;
        if (hit_entry_tick <
                content->fighter.dash_attack_startup_ticks &&
            inspection.players[1].damage_q16 != UINT32_C(0))
        {
            return fail("dash-attack-startup-is-inactive");
        }
        if (!step_reaction_duel(
                hit,
                INT16_MAX,
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
    if (dash_hit_seen == 0 ||
        hit_entry_tick != content->fighter.dash_attack_startup_ticks ||
        hit_event.source_player != UINT8_C(0) ||
        hit_event.target_player != UINT8_C(1) ||
        hit_event.value_q16 != content->fighter.dash_attack_damage_q16 ||
        hit_event.detail != (uint16_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[1].damage_q16 !=
            content->fighter.dash_attack_damage_q16)
    {
        return fail("dash-attack-first-active-frame-hit");
    }

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &loaded) ||
        !advance_to_settled_run(source, content, &inspection) ||
        !step_reaction_duel(
            source,
            INT16_MAX,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "boost-grab-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("boost-grab-snapshot-boundary");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "boost-grab-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "boost-grab-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        const uint64_t buttons =
            tick == UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0);
        const uint16_t trigger =
            tick == UINT32_C(0) ? UINT16_MAX : UINT16_C(0);

        if (!step_reaction_duel(
                source,
                tick == UINT32_C(0) ? INT16_MAX : INT16_C(0),
                INT16_C(0),
                buttons,
                trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                tick == UINT32_C(0) ? INT16_MAX : INT16_C(0),
                INT16_C(0),
                buttons,
                trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "boost-grab-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "boost-grab-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash) ||
            inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            inspection.players[0].position_x_q16 !=
                loaded_inspection.players[0].position_x_q16 ||
            inspection.players[0].velocity_x_q16 !=
                loaded_inspection.players[0].velocity_x_q16 ||
            inspection.players[0].grab_target !=
                loaded_inspection.players[0].grab_target)
        {
            return fail("boost-grab-save-load-continuation");
        }
        if (tick == UINT32_C(0) &&
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_GRAB)
        {
            return fail("boost-grab-loaded-cancel-entry");
        }
        if (find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL)
        {
            snapshot_capture_seen = 1;
        }
    }
    if (snapshot_capture_seen == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED)
    {
        return fail("boost-grab-snapshot-capture");
    }

    return 1;
}

static int run_jump_cancelled_grab_test(
    const pf_m4_content *close_content,
    const pf_content_view *close_view,
    const pf_content_view *far_view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage mash_storage;
    test_sim_storage dodge_storage;
    test_sim_storage attack_storage;
    test_sim_storage direct_dash_storage;
    test_sim_storage jump_cancel_storage;
    test_sim_storage late_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *mash = NULL;
    pf_sim *dodge = NULL;
    pf_sim *attack = NULL;
    pf_sim *direct_dash = NULL;
    pf_sim *jump_cancel = NULL;
    pf_sim *late = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_event grab_event;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t future_tick;
    uint32_t active_grabbox_ticks = UINT32_C(0);
    uint32_t mash_escape_tick = UINT32_MAX;
    int escape_seen = 0;
    int jump_cancel_capture_seen = 0;

    if (!initialize_sim(
            &source_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !begin_close_grab(source, 1, &inspection, &grab_event) ||
        grab_event.type != (uint16_t)PF_SIM_EVENT_GRAB ||
        grab_event.source_player != UINT8_C(0) ||
        grab_event.target_player != UINT8_C(1) ||
        grab_event.value_q16 != UINT32_C(0) ||
        grab_event.velocity_x_q16 != INT32_C(0) ||
        grab_event.velocity_y_q16 != INT32_C(0) ||
        grab_event.flags != UINT16_C(0) ||
        grab_event.detail != (uint16_t)PF_M4_ACTION_GRAB ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[0].grab_owner != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[1].grab_target != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[1].grab_owner != UINT8_C(0) ||
        inspection.players[1].grab_escape_ticks !=
            close_content->fighter.grab_escape_base_ticks ||
        inspection.players[1].position_x_q16 !=
            inspection.players[0].position_x_q16 +
                (int32_t)inspection.players[0].facing *
                    close_content->fighter.grabbed_offset_x_q16 ||
        inspection.players[1].position_y_q16 !=
            inspection.players[0].position_y_q16 +
                close_content->fighter.grabbed_offset_y_q16 ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "grab-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("grab-shield-capture");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "grab-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "grab-load"))
    {
        return 0;
    }

    for (future_tick = UINT32_C(0);
         future_tick <
             (uint32_t)close_content->fighter.grab_escape_base_ticks;
         ++future_tick)
    {
        if (!step_reaction_duel(
                source,
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
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "grab-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "grab-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash) ||
            inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            inspection.players[1].grab_escape_ticks !=
                loaded_inspection.players[1].grab_escape_ticks)
        {
            return fail("grab-save-load-continuation");
        }
        if (future_tick + UINT32_C(1) <
            (uint32_t)close_content->fighter.grab_escape_base_ticks)
        {
            if (find_last_tick_event(PF_SIM_EVENT_GRAB_ESCAPE) != NULL ||
                inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
                inspection.players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_GRABBED)
            {
                return fail("grab-early-escape");
            }
        }
        else
        {
            const pf_sim_event *escape_event =
                find_last_tick_event(PF_SIM_EVENT_GRAB_ESCAPE);

            if (escape_event == NULL ||
                escape_event->source_player != UINT8_C(1) ||
                escape_event->target_player != UINT8_C(0) ||
                escape_event->value_q16 != UINT32_C(0) ||
                inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
                inspection.players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_GRAB_RELEASE ||
                inspection.players[0].grab_target !=
                    PF_SIM_EVENT_NO_PLAYER ||
                inspection.players[1].grab_owner !=
                    PF_SIM_EVENT_NO_PLAYER ||
                inspection.players[1].grab_escape_ticks != UINT16_C(0))
            {
                return fail("grab-exact-escape");
            }
            escape_seen = 1;
        }
    }
    if (escape_seen == 0)
    {
        return fail("grab-escape-event-missing");
    }
    for (future_tick = UINT32_C(0);
         future_tick < (uint32_t)close_content->fighter.grab_release_ticks;
         ++future_tick)
    {
        if (!step_reaction_duel(
                source,
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
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("grab-release-recovery");
    }

    if (!initialize_sim(
            &mash_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &mash) ||
        !begin_close_grab(mash, 0, &inspection, &grab_event) ||
        !step_reaction_duel(
            mash,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].grab_escape_ticks !=
            (uint16_t)(
                close_content->fighter.grab_escape_base_ticks -
                UINT16_C(1) -
                close_content->fighter.grab_mash_reduction_ticks))
    {
        return fail("grab-mash-reduction");
    }
    for (future_tick = UINT32_C(1);
         future_tick <
             (uint32_t)close_content->fighter.grab_escape_base_ticks;
         ++future_tick)
    {
        const uint64_t mash_button =
            (future_tick & UINT32_C(1)) == UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0);

        if (!step_reaction_duel(
                mash,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                mash_button,
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (find_last_tick_event(PF_SIM_EVENT_GRAB_ESCAPE) != NULL)
        {
            mash_escape_tick = future_tick;
            break;
        }
    }
    if (mash_escape_tick >=
        (uint32_t)close_content->fighter.grab_escape_base_ticks -
            UINT32_C(1))
    {
        return fail("grab-mash-escape-boundary");
    }

    if (!initialize_sim(
            &dodge_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &dodge) ||
        !step_reaction_duel(
            dodge,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (future_tick = UINT32_C(0); future_tick < UINT32_C(18);
         ++future_tick)
    {
        active_grabbox_ticks +=
            (uint32_t)inspection.players[0].grabbox_active;
        if (find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL ||
            !step_reaction_duel(
                dodge,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_MAX,
                UINT64_C(0),
                UINT16_MAX,
                &inspection))
        {
            return fail("grab-spot-dodge-negative-step");
        }
    }
    if (active_grabbox_ticks !=
            (uint32_t)close_content->fighter.grab_active_ticks ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[1].grab_owner != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("grab-spot-dodge-negative");
    }

    if (!initialize_sim(
            &attack_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &attack) ||
        !step_reaction_duel(
            attack,
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
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
    {
        return fail("grab-requires-shield-combination");
    }

    if (!initialize_sim(
            &direct_dash_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &direct_dash) ||
        !step_reaction_duel(
            direct_dash,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            direct_dash,
            INT16_MAX,
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
        inspection.players[0].action_ticks != UINT16_C(2))
    {
        return fail("direct-dash-grab-rejected");
    }

    if (!initialize_sim(
            &jump_cancel_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &jump_cancel) ||
        !step_reaction_duel(
            jump_cancel,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            jump_cancel,
            INT16_MAX,
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
        !step_reaction_duel(
            jump_cancel,
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
        return fail("jump-cancel-grab-entry");
    }
    for (future_tick = UINT32_C(0); future_tick < UINT32_C(12);
         ++future_tick)
    {
        if (find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL)
        {
            jump_cancel_capture_seen = 1;
            break;
        }
        if (!step_reaction_duel(
                jump_cancel,
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
    if (jump_cancel_capture_seen == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED)
    {
        return fail("jump-cancel-grab-capture");
    }

    if (!initialize_sim(
            &late_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &late) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            late,
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
        !step_reaction_duel(
            late,
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
        return fail("jump-cancel-grab-late-negative");
    }

    return 1;
}

static int run_jump_cancelling_test(
    const pf_m4_content *content,
    const pf_content_view *close_view,
    const pf_content_view *far_view)
{
    test_sim_storage light_storage;
    test_sim_storage strong_storage;
    test_sim_storage neutral_storage;
    test_sim_storage shallow_storage;
    test_sim_storage late_storage;
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *light = NULL;
    pf_sim *strong = NULL;
    pf_sim *neutral = NULL;
    pf_sim *shallow = NULL;
    pf_sim *late = NULL;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t future_tick;
    int hit_seen = 0;
    const int16_t shallow_up =
        (int16_t)(
            -((int32_t)content->fighter.dash_axis_threshold -
              INT32_C(1)));

    if (!initialize_sim(
            &light_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &light) ||
        !step_reaction_duel(
            light,
            INT16_MAX,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            light,
            INT16_MAX,
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
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        !step_reaction_duel(
            light,
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
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_y_q16 != INT32_C(0))
    {
        return fail("jump-cancel-light-attack");
    }

    if (!initialize_sim(
            &strong_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &strong) ||
        !step_reaction_duel(
            strong,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            strong,
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
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(1) ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        return fail("jump-cancel-strong-attack");
    }

    if (!initialize_sim(
            &neutral_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &neutral) ||
        !step_reaction_duel(
            neutral,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            neutral,
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
        inspection.players[0].action_ticks != UINT16_C(2) ||
        inspection.players[0].grounded == UINT8_C(0))
    {
        return fail("jump-cancel-neutral-attack-negative");
    }

    if (!initialize_sim(
            &shallow_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &shallow) ||
        !step_reaction_duel(
            shallow,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            shallow,
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
        return fail("jump-cancel-up-threshold-negative");
    }

    if (!initialize_sim(
            &late_storage,
            far_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &late) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            late,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            late,
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
        !step_reaction_duel(
            late,
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
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
        inspection.players[0].grounded != UINT8_C(0))
    {
        return fail("jump-cancel-first-airborne-negative");
    }

    if (!initialize_sim(
            &source_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            close_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            source,
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
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "jump-cancel-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("jump-cancel-save-setup");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "jump-cancel-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "jump-cancel-load"))
    {
        return 0;
    }
    for (future_tick = UINT32_C(0);
         future_tick <
             (uint32_t)content->fighter.strong_startup_ticks +
                 (uint32_t)content->fighter.strong_active_ticks +
                 (uint32_t)content->fighter.strong_recovery_ticks +
                 (uint32_t)content->fighter.strong_hitlag_ticks +
                 UINT32_C(4);
         ++future_tick)
    {
        uint32_t event_index;

        if (!step_reaction_duel(
                source,
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
        source_result = test_last_result;
        for (event_index = UINT32_C(0);
             event_index < (uint32_t)source_result.event_count;
             ++event_index)
        {
            if (source_result.events[event_index].type ==
                    (uint16_t)PF_SIM_EVENT_HIT &&
                source_result.events[event_index].detail ==
                    (uint16_t)PF_M4_ACTION_STRONG_ATTACK)
            {
                hit_seen = 1;
            }
        }
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "jump-cancel-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "jump-cancel-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash) ||
            inspection.players[0].action_state !=
                loaded_inspection.players[0].action_state ||
            inspection.players[0].position_x_q16 !=
                loaded_inspection.players[0].position_x_q16 ||
            inspection.players[1].damage_q16 !=
                loaded_inspection.players[1].damage_q16)
        {
            return fail("jump-cancel-save-load-continuation");
        }
    }
    if (hit_seen == 0 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[1].damage_q16 !=
            content->fighter.strong_damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug jump_cancel_hit=%d action=%u damage=%" PRIu32
            " expected=%" PRIu32 "\n",
            hit_seen,
            (unsigned int)inspection.players[0].action_state,
            inspection.players[1].damage_q16,
            content->fighter.strong_damage_q16);
        return fail("jump-cancel-production-hit");
    }
    return 1;
}

static int run_grab_damage_escape_test(
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_sim_event grab_event;
    uint32_t tick;
    int hit_seen = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        if (find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            hit_seen = 1;
            break;
        }
        if (!step_reaction_duel(
                sim,
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
    if (hit_seen == 0 ||
        inspection.players[1].damage_q16 !=
            UINT32_C(6) * UINT32_C(65536))
    {
        return fail("grab-damage-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(64); ++tick)
    {
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_reaction_duel(
                sim,
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
    if (tick == UINT32_C(64) ||
        !begin_close_grab(sim, 0, &inspection, &grab_event) ||
        grab_event.value_q16 !=
            UINT32_C(6) * UINT32_C(65536) ||
        inspection.players[1].grab_escape_ticks != UINT16_C(33))
    {
        return fail("grab-damage-scaled-capped-escape");
    }
    return 1;
}

static int step_team_handoff(
    pf_sim *sim,
    int16_t player0_y,
    uint64_t player0_buttons,
    uint16_t player0_trigger,
    uint64_t victim_buttons,
    int16_t player2_y,
    uint64_t player2_buttons,
    uint16_t player2_trigger,
    pf_m4_inspection *out_inspection)
{
    const int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0)};
    uint16_t triggers[PF_SIM_MAX_PLAYERS] = {
        UINT16_C(0), UINT16_C(0), UINT16_C(0), UINT16_C(0)};

    axes_y[0] = player0_y;
    axes_y[2] = player2_y;
    buttons[0] = player0_buttons;
    buttons[1] = victim_buttons;
    buttons[2] = player2_buttons;
    triggers[0] = player0_trigger;
    triggers[2] = player2_trigger;
    return step_players_with_triggers(
        sim,
        UINT8_C(4),
        axes_x,
        axes_y,
        buttons,
        triggers,
        out_inspection);
}

static int run_team_handoff_route(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage route_storage;
    test_sim_storage missed_storage;
    pf_sim *route = NULL;
    pf_sim *missed = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;
    uint32_t throw_events = UINT32_C(0);
    uint32_t handoff_events = UINT32_C(0);
    int initial_capture = 0;

    if (!initialize_sim(
            &route_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &route) ||
        !step_team_handoff(
            route,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *grab_event =
            find_last_tick_event(PF_SIM_EVENT_GRAB);

        if (grab_event != NULL)
        {
            initial_capture =
                grab_event->source_player == UINT8_C(0) &&
                grab_event->target_player == UINT8_C(1);
            break;
        }
        if (!step_team_handoff(
                route,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (initial_capture == 0 ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(0))
    {
        return fail("team-handoff-initial-capture");
    }

    if (!step_team_handoff(
            route,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const pf_sim_event *throw_event =
            find_last_tick_event(PF_SIM_EVENT_THROW);
        const pf_sim_event *grab_event =
            find_last_tick_event(PF_SIM_EVENT_GRAB);

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
        if (!step_team_handoff(
                route,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                (tick & UINT32_C(1)) == UINT32_C(0)
                    ? UINT64_C(0)
                    : PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (throw_events != UINT32_C(1) ||
        handoff_events != UINT32_C(1) ||
        inspection.players[2].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(2))
    {
        return fail("team-handoff-player2-capture");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_team_handoff(
                route,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                (tick & UINT32_C(1)) == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(32) ||
        !step_team_handoff(
            route,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            PF_INPUT_BUTTON_JUMP,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection))
    {
        return fail("team-handoff-player0-ready");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        const pf_sim_event *throw_event =
            find_last_tick_event(PF_SIM_EVENT_THROW);
        const pf_sim_event *grab_event =
            find_last_tick_event(PF_SIM_EVENT_GRAB);

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
        if (!step_team_handoff(
                route,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                (tick & UINT32_C(1)) == UINT32_C(0)
                    ? UINT64_C(0)
                    : PF_INPUT_BUTTON_ATTACK,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (throw_events != UINT32_C(2) ||
        handoff_events != UINT32_C(2) ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(0) ||
        inspection.players[1].damage_q16 !=
            UINT32_C(2) * content->fighter.down_throw.damage_q16)
    {
        return fail("team-handoff-player0-recapture");
    }

    if (!initialize_sim(
            &missed_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &missed) ||
        !step_team_handoff(
            missed,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            UINT64_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (inspection.players[0].grab_target == UINT8_C(1) &&
            inspection.players[2].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_team_handoff(
                missed,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (tick == UINT32_C(20) ||
        !step_team_handoff(
            missed,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("team-handoff-missed-setup");
    }
    initial_capture = 0;
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        const pf_sim_event *grab_event =
            find_last_tick_event(PF_SIM_EVENT_GRAB);

        if (grab_event != NULL &&
            grab_event->source_player == UINT8_C(2) &&
            grab_event->target_player == UINT8_C(1))
        {
            initial_capture = 1;
        }
        if (!step_team_handoff(
                missed,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                (tick & UINT32_C(1)) == UINT32_C(0)
                    ? UINT64_C(0)
                    : PF_INPUT_BUTTON_JUMP,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (initial_capture != 0 ||
        inspection.players[1].grab_owner != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("team-handoff-early-grab-negative");
    }
    return 1;
}

static int run_grab_team_resolution_test(
    const pf_content_view *view)
{
    test_sim_storage friendly_storage;
    test_sim_storage priority_storage;
    pf_sim *friendly = NULL;
    pf_sim *priority = NULL;
    pf_m4_inspection inspection;
    int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_MAX, INT16_C(0), INT16_C(0)};
    uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        PF_INPUT_BUTTON_ATTACK,
        UINT64_C(0),
        UINT64_C(0),
        UINT64_C(0)};
    uint16_t triggers[PF_SIM_MAX_PLAYERS] = {
        UINT16_MAX, UINT16_MAX, UINT16_C(0), UINT16_C(0)};
    uint32_t tick;
    int capture_seen = 0;

    if (!initialize_sim(
            &friendly_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &friendly) ||
        !step_players_with_triggers(
            friendly,
            UINT8_C(4),
            axes_x,
            axes_y,
            buttons,
            triggers,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL)
        {
            capture_seen = 1;
        }
        (void)memset(axes_y, 0, sizeof(axes_y));
        (void)memset(buttons, 0, sizeof(buttons));
        (void)memset(triggers, 0, sizeof(triggers));
        if (!step_players_with_triggers(
                friendly,
                UINT8_C(4),
                axes_x,
                axes_y,
                buttons,
                triggers,
                &inspection))
        {
            return 0;
        }
    }
    if (capture_seen != 0 ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[1].grab_owner != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[2].grab_owner != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[3].grab_owner != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("grab-friendly-fire-and-invulnerability");
    }

    (void)memset(axes_y, 0, sizeof(axes_y));
    (void)memset(buttons, 0, sizeof(buttons));
    (void)memset(triggers, 0, sizeof(triggers));
    buttons[0] = PF_INPUT_BUTTON_ATTACK;
    buttons[2] = PF_INPUT_BUTTON_ATTACK;
    triggers[0] = UINT16_MAX;
    triggers[2] = UINT16_MAX;
    if (!initialize_sim(
            &priority_storage,
            view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &priority) ||
        !step_players_with_triggers(
            priority,
            UINT8_C(4),
            axes_x,
            axes_y,
            buttons,
            triggers,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_GRAB);

        if (event != NULL)
        {
            if (event->source_player != UINT8_C(0) ||
                event->target_player != UINT8_C(1))
            {
                return fail("grab-controller-port-priority-event");
            }
            capture_seen = 1;
            break;
        }
        (void)memset(buttons, 0, sizeof(buttons));
        (void)memset(triggers, 0, sizeof(triggers));
        if (!step_players_with_triggers(
                priority,
                UINT8_C(4),
                axes_x,
                axes_y,
                buttons,
                triggers,
                &inspection))
        {
            return 0;
        }
    }
    if (capture_seen == 0 ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].grab_owner != UINT8_C(0) ||
        inspection.players[2].grab_target != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("grab-controller-port-priority-state");
    }
    return 1;
}

static int32_t expected_throw_velocity(
    int32_t base_q16,
    int32_t growth_q16,
    uint32_t damage_q16)
{
    return (int32_t)(
        (int64_t)base_q16 +
        (((int64_t)growth_q16 * (int64_t)damage_q16) >> 16U));
}

static int run_directional_throw_case(
    const pf_content_view *view,
    const pf_m4_throw_data *throw_data,
    int16_t stick_x,
    int16_t stick_y,
    pf_m4_action_state expected_action)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_sim_event grab_event;
    const int32_t expected_velocity_x =
        expected_throw_velocity(
            throw_data->base_velocity_x_q16,
            throw_data->velocity_growth_x_q16,
            throw_data->damage_q16);
    const int32_t expected_velocity_y =
        expected_throw_velocity(
            throw_data->base_velocity_y_q16,
            throw_data->velocity_growth_y_q16,
            throw_data->damage_q16);
    uint32_t tick;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !begin_close_grab(sim, 0, &inspection, &grab_event) ||
        !step_reaction_duel(
            sim,
            stick_x,
            stick_y,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state != (uint8_t)expected_action ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].grab_target != UINT8_C(1) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED ||
        inspection.players[1].grab_owner != UINT8_C(0) ||
        find_last_tick_event(PF_SIM_EVENT_THROW) != NULL)
    {
        return fail("directional-throw-input");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)throw_data->release_tick;
         ++tick)
    {
        const pf_sim_event *throw_event;

        if (!step_reaction_duel(
                sim,
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
        throw_event = find_last_tick_event(PF_SIM_EVENT_THROW);
        if (tick + UINT32_C(1) <
            (uint32_t)throw_data->release_tick)
        {
            if (throw_event != NULL ||
                inspection.players[0].action_state !=
                    (uint8_t)expected_action ||
                inspection.players[0].action_ticks !=
                    (uint16_t)(tick + UINT32_C(1)) ||
                inspection.players[0].grab_target != UINT8_C(1) ||
                inspection.players[1].grab_owner != UINT8_C(0))
            {
                return fail("directional-throw-startup");
            }
        }
        else if (throw_event == NULL ||
                 throw_event->source_player != UINT8_C(0) ||
                 throw_event->target_player != UINT8_C(1) ||
                 throw_event->value_q16 != throw_data->damage_q16 ||
                 throw_event->velocity_x_q16 != expected_velocity_x ||
                 throw_event->velocity_y_q16 != expected_velocity_y ||
                 throw_event->flags != UINT16_C(0) ||
                 throw_event->detail != (uint16_t)expected_action ||
                 inspection.players[0].action_state !=
                     (uint8_t)PF_M4_ACTION_HITLAG ||
                 inspection.players[1].action_state !=
                     (uint8_t)PF_M4_ACTION_HITLAG ||
                 inspection.players[0].hitlag_ticks !=
                     throw_data->hitlag_ticks ||
                 inspection.players[1].hitlag_ticks !=
                     throw_data->hitlag_ticks ||
                 inspection.players[0].grab_target !=
                     PF_SIM_EVENT_NO_PLAYER ||
                 inspection.players[1].grab_owner !=
                     PF_SIM_EVENT_NO_PLAYER ||
                 inspection.players[1].damage_q16 !=
                     throw_data->damage_q16 ||
                 inspection.players[1].last_hit_valid != UINT8_C(1) ||
                 inspection.players[1].last_hit_attacker != UINT8_C(0))
        {
            return fail("directional-throw-release");
        }
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)throw_data->hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
        if (tick + UINT32_C(1) <
            (uint32_t)throw_data->hitlag_ticks)
        {
            if (inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_HITLAG ||
                inspection.players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_HITLAG)
            {
                return fail("directional-throw-hitlag");
            }
        }
    }
    if (inspection.players[0].action_state != (uint8_t)expected_action ||
        inspection.players[0].action_ticks != throw_data->release_tick ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].velocity_x_q16 != expected_velocity_x ||
        inspection.players[1].velocity_y_q16 != expected_velocity_y)
    {
        return fail("directional-throw-hitstun-entry");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)throw_data->recovery_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
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
        if (tick + UINT32_C(1) <
            (uint32_t)throw_data->recovery_ticks)
        {
            if (inspection.players[0].action_state !=
                (uint8_t)expected_action)
            {
                return fail("directional-throw-early-recovery");
            }
        }
        else if (inspection.players[0].action_state !=
                 (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            return fail("directional-throw-exact-recovery");
        }
    }
    return 1;
}

static int run_pummel_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_m4_content invalid_content = *content;
    pf_m4_content changed_content = *content;
    pf_content_view changed_view;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_event grab_event;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t future_tick;
    uint32_t pummel_events = UINT32_C(0);

    if (content->fighter.pummel_damage_q16 !=
            UINT32_C(3) * UINT32_C(65536) ||
        content->fighter.pummel_hit_tick != UINT16_C(2) ||
        content->fighter.pummel_total_ticks != UINT16_C(10))
    {
        return fail("pummel-default-data");
    }
    invalid_content.fighter.pummel_hit_tick =
        invalid_content.fighter.pummel_total_ticks;
    changed_content.fighter.pummel_damage_q16 += UINT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-pummel-window") ||
        !expect_status(
            pf_m4_make_content_view(&changed_content, &changed_view),
            PF_STATUS_OK,
            "changed-pummel-content-view") ||
        memcmp(
            view->content_hash.bytes,
            changed_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0)
    {
        return fail("pummel-content-contract");
    }

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !begin_close_grab(
            source,
            0,
            &source_inspection,
            &grab_event) ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].grab_target != UINT8_C(1) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED ||
        source_inspection.players[1].grab_owner != UINT8_C(0) ||
        find_last_tick_event(PF_SIM_EVENT_PUMMEL) != NULL ||
        find_last_tick_event(PF_SIM_EVENT_THROW) != NULL ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[1].damage_q16 != UINT32_C(0) ||
        find_last_tick_event(PF_SIM_EVENT_PUMMEL) != NULL ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "pummel-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("pummel-entry");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "pummel-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "pummel-load") ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "pummel-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "pummel-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("pummel-save-load");
    }

    for (future_tick = UINT32_C(0);
         future_tick < (uint32_t)content->fighter.pummel_total_ticks;
         ++future_tick)
    {
        const pf_sim_event *pummel_event;

        if (!step_reaction_duel(
                source,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection))
        {
            return 0;
        }
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events[0]) *
                    (size_t)source_result.event_count) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "pummel-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "pummel-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("pummel-snapshot-continuation");
        }
        pummel_event = find_last_tick_event(PF_SIM_EVENT_PUMMEL);
        if (pummel_event != NULL)
        {
            ++pummel_events;
            if (pummel_event->source_player != UINT8_C(0) ||
                pummel_event->target_player != UINT8_C(1) ||
                pummel_event->value_q16 !=
                    content->fighter.pummel_damage_q16 ||
                pummel_event->velocity_x_q16 != INT32_C(0) ||
                pummel_event->velocity_y_q16 != INT32_C(0) ||
                pummel_event->flags != UINT16_C(0) ||
                pummel_event->detail !=
                    (uint16_t)PF_M4_ACTION_PUMMEL ||
                source_inspection.players[0].action_state !=
                    (uint8_t)PF_M4_ACTION_PUMMEL ||
                source_inspection.players[0].action_ticks !=
                    content->fighter.pummel_hit_tick)
            {
                return fail("pummel-event-contract");
            }
        }
    }

    if (pummel_events != UINT32_C(1) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].grab_target != UINT8_C(1) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GRABBED ||
        source_inspection.players[1].grab_owner != UINT8_C(0) ||
        source_inspection.players[1].damage_q16 !=
            content->fighter.pummel_damage_q16 ||
        source_inspection.players[1].last_hit_valid != UINT8_C(1) ||
        source_inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        source_inspection.players[1].last_hit_damage_q16 !=
            content->fighter.pummel_damage_q16)
    {
        return fail("pummel-return-and-held-input");
    }

    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        !step_reaction_duel(
            source,
            (int16_t)(
                content->fighter.dash_axis_threshold - UINT16_C(1)),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].grab_target != UINT8_C(1) ||
        source_inspection.players[1].grab_owner != UINT8_C(0) ||
        find_last_tick_event(PF_SIM_EVENT_PUMMEL) != NULL ||
        find_last_tick_event(PF_SIM_EVENT_THROW) != NULL)
    {
        return fail("pummel-fresh-strong-reduced-input");
    }
    return 1;
}

static int run_directional_throw_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    if (!run_directional_throw_case(
            view,
            &content->fighter.forward_throw,
            INT16_C(32767),
            INT16_C(0),
            PF_M4_ACTION_THROW_FORWARD) ||
        !run_directional_throw_case(
            view,
            &content->fighter.back_throw,
            INT16_C(-32767),
            INT16_C(0),
            PF_M4_ACTION_THROW_BACK) ||
        !run_directional_throw_case(
            view,
            &content->fighter.up_throw,
            INT16_C(0),
            INT16_C(-32767),
            PF_M4_ACTION_THROW_UP) ||
        !run_directional_throw_case(
            view,
            &content->fighter.down_throw,
            INT16_C(0),
            INT16_C(32767),
            PF_M4_ACTION_THROW_DOWN) ||
        !run_directional_throw_case(
            view,
            &content->fighter.forward_throw,
            INT16_C(32767),
            INT16_C(-32767),
            PF_M4_ACTION_THROW_FORWARD) ||
        !run_directional_throw_case(
            view,
            &content->fighter.up_throw,
            INT16_C(30000),
            INT16_C(-32767),
            PF_M4_ACTION_THROW_UP))
    {
        return 0;
    }
    return 1;
}

static int perform_down_throw(
    pf_sim *sim,
    const pf_m4_throw_data *down_throw,
    int16_t target_di_x,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(32767),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            target_di_x,
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_THROW_DOWN)
    {
        return fail("chain-grab-down-throw-input");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)down_throw->release_tick;
         ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_di_x,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return find_last_tick_event(PF_SIM_EVENT_THROW) != NULL ||
           fail("chain-grab-throw-event");
}

static int wait_for_thrower_idle(
    pf_sim *sim,
    int16_t target_di_x,
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
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                target_di_x,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return fail("chain-grab-thrower-recovery");
}

static int run_chain_grab_route(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_sim_event grab_event;
    uint32_t regrab;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !begin_close_grab(sim, 0, &inspection, &grab_event))
    {
        return fail("chain-grab-initial-capture");
    }

    for (regrab = UINT32_C(0); regrab < UINT32_C(2); ++regrab)
    {
        if (!perform_down_throw(
                sim,
                &content->fighter.down_throw,
                INT16_C(0),
                &inspection) ||
            !wait_for_thrower_idle(sim, INT16_C(0), &inspection) ||
            !begin_close_grab(sim, 0, &inspection, &grab_event) ||
            grab_event.type != (uint16_t)PF_SIM_EVENT_GRAB ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_GRAB_HOLD ||
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_GRABBED)
        {
            return fail("chain-grab-legal-regrab");
        }
    }
    if (!perform_down_throw(
            sim,
            &content->fighter.down_throw,
            INT16_C(0),
            &inspection) ||
        inspection.players[1].damage_q16 !=
            UINT32_C(3) * content->fighter.down_throw.damage_q16)
    {
        return fail("chain-grab-three-throw-conversion");
    }
    return 1;
}

static int run_chain_grab_snapshot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_event grab_event;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes source_bytes;
    size_t save_size = (size_t)0;
    uint32_t future_tick;
    uint32_t throws_started = UINT32_C(1);
    uint32_t throw_events = UINT32_C(0);
    uint32_t regrab_events = UINT32_C(0);

    if (!initialize_sim(
            &source_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !begin_close_grab(source, 0, &source_inspection, &grab_event) ||
        !perform_down_throw(
            source,
            &content->fighter.down_throw,
            INT16_C(0),
            &source_inspection) ||
        !wait_for_thrower_idle(
            source,
            INT16_C(0),
            &source_inspection) ||
        !begin_close_grab(source, 0, &source_inspection, &grab_event) ||
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(32767),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_THROW_DOWN ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].grab_target != UINT8_C(1) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "chain-grab-query-save-size") ||
        save_size != (size_t)694)
    {
        return fail("chain-grab-snapshot-setup");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "chain-grab-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    source_bytes.bytes = save_bytes;
    source_bytes.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, source_bytes),
            PF_STATUS_OK,
            "chain-grab-load"))
    {
        return 0;
    }

    for (future_tick = UINT32_C(0);
         future_tick < UINT32_C(160);
         ++future_tick)
    {
        int16_t player0_y = INT16_C(0);
        uint64_t player0_buttons = UINT64_C(0);
        uint16_t player0_trigger = UINT16_C(0);
        uint32_t event_index;

        if (source_inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GRAB_HOLD &&
            throws_started < UINT32_C(2))
        {
            player0_y = INT16_C(32767);
            player0_buttons = PF_INPUT_BUTTON_ATTACK;
            ++throws_started;
        }
        else if (source_inspection.players[0].action_state ==
                     (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
                 throws_started < UINT32_C(2))
        {
            player0_buttons = PF_INPUT_BUTTON_ATTACK;
            player0_trigger = UINT16_MAX;
        }

        if (!step_reaction_duel(
                source,
                INT16_C(0),
                player0_y,
                player0_buttons,
                player0_trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection))
        {
            return 0;
        }
        source_result = test_last_result;
        if (!step_reaction_duel(
                loaded,
                INT16_C(0),
                player0_y,
                player0_buttons,
                player0_trigger,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &loaded_inspection) ||
            source_result.event_count != test_last_result.event_count ||
            memcmp(
                source_result.events,
                test_last_result.events,
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "chain-grab-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "chain-grab-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("chain-grab-snapshot-continuation");
        }

        for (event_index = UINT32_C(0);
             event_index < (uint32_t)source_result.event_count;
             ++event_index)
        {
            if (source_result.events[event_index].type ==
                (uint16_t)PF_SIM_EVENT_THROW)
            {
                ++throw_events;
            }
            else if (source_result.events[event_index].type ==
                     (uint16_t)PF_SIM_EVENT_GRAB)
            {
                ++regrab_events;
            }
        }
        if (throw_events == UINT32_C(2) &&
            regrab_events >= UINT32_C(1) &&
            source_inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (future_tick == UINT32_C(160) ||
        throw_events != UINT32_C(2) ||
        regrab_events < UINT32_C(1) ||
        source_inspection.players[1].damage_q16 !=
            UINT32_C(3) * content->fighter.down_throw.damage_q16)
    {
        return fail("chain-grab-snapshot-future-route");
    }
    return 1;
}

static int run_chain_grab_di_escape_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_sim_event grab_event;
    uint32_t hit_count = UINT32_C(0);
    uint32_t tick;
    int grab_started = 0;
    int grab_event_seen = 0;

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "chain-grab-di-initial-inspection"))
    {
        return fail("chain-grab-di-setup");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(80) && hit_count < UINT32_C(2);
         ++tick)
    {
        uint64_t attacker_buttons = UINT64_C(0);

        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            attacker_buttons = PF_INPUT_BUTTON_ATTACK;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            ++hit_count;
        }
    }
    if (hit_count != UINT32_C(2) ||
        inspection.players[1].damage_q16 !=
            UINT32_C(90) * UINT32_C(65536))
    {
        return fail("chain-grab-di-percent-setup");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
    {
        if (inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_reaction_duel(
                sim,
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
    if (!begin_close_grab(sim, 0, &inspection, &grab_event) ||
        !perform_down_throw(
            sim,
            &content->fighter.down_throw,
            INT16_C(32767),
            &inspection) ||
        !wait_for_thrower_idle(
            sim,
            INT16_C(32767),
            &inspection) ||
        inspection.players[1].damage_q16 !=
            UINT32_C(96) * UINT32_C(65536))
    {
        return fail("chain-grab-di-throw");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
    {
        uint64_t attacker_buttons = UINT64_C(0);
        uint16_t attacker_trigger = UINT16_C(0);

        if (grab_started == 0 &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            attacker_buttons = PF_INPUT_BUTTON_ATTACK;
            attacker_trigger = UINT16_MAX;
            grab_started = 1;
        }
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
                attacker_trigger,
                INT16_C(32767),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return 0;
        }
        if (find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL)
        {
            grab_event_seen = 1;
        }
    }
    if (grab_started == 0 ||
        grab_event_seen != 0 ||
        inspection.players[0].grab_target != PF_SIM_EVENT_NO_PLAYER ||
        inspection.players[1].grab_owner != PF_SIM_EVENT_NO_PLAYER)
    {
        return fail("chain-grab-di-escape-negative");
    }
    return 1;
}

int main(void)
{
    pf_m4_content content;
    pf_m4_content invalid_content;
    pf_m4_content invalid_strong_content;
    pf_m4_content invalid_tech_content;
    pf_m4_content invalid_getup_content;
    pf_m4_content invalid_shield_content;
    pf_m4_content invalid_shield_break_content;
    pf_m4_content invalid_cancel_content;
    pf_m4_content invalid_surface_content;
    pf_m4_content invalid_v_cancel_scale_content;
    pf_m4_content invalid_v_cancel_window_content;
    pf_m4_content invalid_grabbox_content;
    pf_m4_content invalid_grab_escape_content;
    pf_m4_content invalid_throw_content;
    pf_m4_content invalid_dash_attack_content;
    pf_m4_content invalid_boost_grab_window_content;
    pf_m4_content invalid_jab_cancel_window_content;
    pf_m4_content invalid_jab_final_content;
    pf_m4_content invalid_jab_reset_content;
    pf_m4_content reaction_content;
    pf_m4_content tech_invulnerability_content;
    pf_m4_content floor_attack_content;
    pf_m4_content shield_break_content;
    pf_m4_content small_step_forward_smash_content;
    pf_m4_content drop_cancel_content;
    pf_m4_content drop_cancel_whiff_content;
    pf_m4_content v_cancel_content;
    pf_m4_content double_jump_cancel_counter_content;
    pf_m4_content approach_content;
    pf_m4_content spacing_safe_content;
    pf_m4_content spacing_close_content;
    pf_m4_content spacing_far_content;
    pf_m4_content cross_up_content;
    pf_m4_content juggling_content;
    pf_m4_content ladder_content;
    pf_m4_content kill_confirm_content;
    pf_m4_content wall_tech_content;
    pf_m4_content ceiling_tech_content;
    pf_m4_content grab_close_content;
    pf_m4_content grab_far_content;
    pf_m4_content grab_damage_content;
    pf_m4_content team_wobble_content;
    pf_m4_content chain_grab_escape_content;
    pf_m4_content boost_grab_content;
    pf_m4_content jab_cancel_close_content;
    pf_m4_content jab_cancel_far_content;
    pf_m4_content jab_reset_content;
    pf_m4_content jab_reset_exact_content;
    pf_m4_content jab_reset_over_damage_content;
    pf_m4_content jab_reset_over_hitstun_content;
    pf_content_view view;
    pf_content_view reaction_view;
    pf_content_view tech_invulnerability_view;
    pf_content_view floor_attack_view;
    pf_content_view shield_break_view;
    pf_content_view small_step_forward_smash_view;
    pf_content_view drop_cancel_view;
    pf_content_view drop_cancel_whiff_view;
    pf_content_view v_cancel_view;
    pf_content_view double_jump_cancel_counter_view;
    pf_content_view approach_view;
    pf_content_view spacing_safe_view;
    pf_content_view spacing_close_view;
    pf_content_view spacing_far_view;
    pf_content_view cross_up_view;
    pf_content_view juggling_view;
    pf_content_view ladder_view;
    pf_content_view kill_confirm_view;
    pf_content_view wall_tech_view;
    pf_content_view ceiling_tech_view;
    pf_content_view grab_close_view;
    pf_content_view grab_far_view;
    pf_content_view grab_damage_view;
    pf_content_view team_wobble_view;
    pf_content_view chain_grab_escape_view;
    pf_content_view boost_grab_view;
    pf_content_view jab_cancel_close_view;
    pf_content_view jab_cancel_far_view;
    pf_content_view jab_reset_view;
    pf_content_view jab_reset_exact_view;
    pf_content_view jab_reset_over_damage_view;
    pf_content_view jab_reset_over_hitstun_view;

    if (!make_combat_content(&content, &view) ||
        !make_reaction_content(
            &reaction_content,
            &reaction_view) ||
        !make_tech_invulnerability_content(
            &tech_invulnerability_content,
            &tech_invulnerability_view) ||
        !make_floor_attack_content(
            &floor_attack_content,
            &floor_attack_view) ||
        !make_shield_break_content(
            &shield_break_content,
            &shield_break_view) ||
        !make_small_step_forward_smash_content(
            &small_step_forward_smash_content,
            &small_step_forward_smash_view) ||
        !make_drop_cancel_content(
            0,
            &drop_cancel_content,
            &drop_cancel_view) ||
        !make_drop_cancel_content(
            1,
            &drop_cancel_whiff_content,
            &drop_cancel_whiff_view) ||
        !make_v_cancel_content(
            &v_cancel_content,
            &v_cancel_view) ||
        !make_double_jump_cancel_counter_content(
            &double_jump_cancel_counter_content,
            &double_jump_cancel_counter_view) ||
        !make_spacing_content(
            INT32_C(8) * PF_Q16_ONE,
            &approach_content,
            &approach_view) ||
        !make_spacing_content(
            (INT32_C(39) * PF_Q16_ONE) / INT32_C(40),
            &spacing_safe_content,
            &spacing_safe_view) ||
        !make_spacing_content(
            (INT32_C(17) * PF_Q16_ONE) / INT32_C(20),
            &spacing_close_content,
            &spacing_close_view) ||
        !make_spacing_content(
            (INT32_C(9) * PF_Q16_ONE) / INT32_C(8),
            &spacing_far_content,
            &spacing_far_view) ||
        !make_cross_up_content(
            &cross_up_content,
            &cross_up_view) ||
        !make_juggling_content(
            &juggling_content,
            &juggling_view) ||
        !make_ladder_content(
            &ladder_content,
            &ladder_view) ||
        !make_kill_confirm_content(
            &kill_confirm_content,
            &kill_confirm_view) ||
        !make_surface_tech_content(
            0,
            &wall_tech_content,
            &wall_tech_view) ||
        !make_surface_tech_content(
            1,
            &ceiling_tech_content,
            &ceiling_tech_view) ||
        !make_grab_content(
            (INT32_C(4) * PF_Q16_ONE) / INT32_C(5),
            &grab_close_content,
            &grab_close_view) ||
        !make_grab_content(
            (INT32_C(5) * PF_Q16_ONE) / INT32_C(4),
            &grab_far_content,
            &grab_far_view) ||
        !make_grab_damage_content(
            &grab_damage_content,
            &grab_damage_view) ||
        !make_team_wobble_content(
            &team_wobble_content,
            &team_wobble_view) ||
        !make_chain_grab_escape_content(
            &chain_grab_escape_content,
            &chain_grab_escape_view) ||
        !make_boost_grab_content(
            &boost_grab_content,
            &boost_grab_view) ||
        !make_jab_cancel_content(
            (INT32_C(4) * PF_Q16_ONE) / INT32_C(5),
            &jab_cancel_close_content,
            &jab_cancel_close_view) ||
        !make_jab_cancel_content(
            INT32_C(4) * PF_Q16_ONE,
            &jab_cancel_far_content,
            &jab_cancel_far_view) ||
        !make_jab_reset_content(
            &jab_reset_content,
            &jab_reset_view))
    {
        return 1;
    }
    jab_reset_exact_content = jab_reset_content;
    jab_reset_exact_content.fighter.jab_damage_q16 =
        jab_reset_exact_content.fighter.reset_max_damage_q16;
    jab_reset_over_damage_content = jab_reset_content;
    jab_reset_over_damage_content.fighter.jab_damage_q16 =
        jab_reset_over_damage_content.fighter.reset_max_damage_q16 +
        UINT32_C(1);
    jab_reset_over_hitstun_content = jab_reset_content;
    jab_reset_over_hitstun_content.fighter
        .jab_base_knockback_y_q16 +=
        jab_reset_over_hitstun_content.fighter
            .hitstun_velocity_per_tick_q16;
    if (!expect_status(
            pf_m4_make_content_view(
                &jab_reset_exact_content,
                &jab_reset_exact_view),
            PF_STATUS_OK,
            "jab-reset-exact-content-view") ||
        !expect_status(
            pf_m4_make_content_view(
                &jab_reset_over_damage_content,
                &jab_reset_over_damage_view),
            PF_STATUS_OK,
            "jab-reset-over-damage-content-view") ||
        !expect_status(
            pf_m4_make_content_view(
                &jab_reset_over_hitstun_content,
                &jab_reset_over_hitstun_view),
            PF_STATUS_OK,
            "jab-reset-over-hitstun-content-view"))
    {
        return 1;
    }
    invalid_content = content;
    invalid_content.fighter.jab_knockback_growth_q16 =
        INT32_C(4) * PF_Q16_ONE;
    invalid_strong_content = content;
    invalid_strong_content.fighter.strong_knockback_growth_q16 =
        INT32_C(4) * PF_Q16_ONE;
    invalid_tech_content = content;
    invalid_tech_content.fighter.tech_invulnerability_ticks =
        (uint16_t)(
            invalid_tech_content.fighter.tech_in_place_ticks +
            UINT16_C(1));
    invalid_getup_content = content;
    invalid_getup_content.fighter
        .getup_attack_front_active_end_tick =
        invalid_getup_content.fighter
            .getup_attack_back_active_begin_tick;
    invalid_shield_content = content;
    invalid_shield_content.fighter.shield_release_ticks =
        UINT16_C(0);
    invalid_shield_break_content = content;
    invalid_shield_break_content.fighter
        .shield_break_launch_speed_q16 =
        invalid_shield_break_content.fighter.gravity_q16;
    invalid_cancel_content = content;
    invalid_cancel_content.fighter.powershield_cancel_delay_ticks =
        invalid_cancel_content.fighter.shield_release_ticks;
    invalid_surface_content = content;
    invalid_surface_content.stage.solid_bottom_q16 =
        invalid_surface_content.stage.solid_top_q16;
    invalid_v_cancel_scale_content = content;
    invalid_v_cancel_scale_content.fighter
        .v_cancel_velocity_scale_q16 = PF_Q16_ONE;
    invalid_v_cancel_window_content = content;
    invalid_v_cancel_window_content.fighter.v_cancel_window_ticks =
        (uint16_t)(
            invalid_v_cancel_window_content.fighter
                .tech_lockout_ticks +
            UINT16_C(1));
    invalid_grabbox_content = content;
    invalid_grabbox_content.fighter.grabbox_half_width_q16 = INT32_C(0);
    invalid_grab_escape_content = content;
    invalid_grab_escape_content.fighter.grab_escape_max_ticks =
        (uint16_t)(
            invalid_grab_escape_content.fighter.grab_escape_base_ticks -
            UINT16_C(1));
    invalid_throw_content = content;
    invalid_throw_content.fighter.down_throw.release_tick = UINT16_C(0);
    invalid_dash_attack_content = content;
    invalid_dash_attack_content.fighter.dash_attack_speed_q16 =
        invalid_dash_attack_content.fighter.initial_dash_speed_q16;
    invalid_boost_grab_window_content = content;
    invalid_boost_grab_window_content.fighter
        .boost_grab_cancel_end_tick =
        invalid_boost_grab_window_content.fighter
            .dash_attack_startup_ticks;
    invalid_jab_cancel_window_content = content;
    invalid_jab_cancel_window_content.fighter
        .jab_combo_input_begin_tick =
        (uint16_t)(
            (uint32_t)invalid_jab_cancel_window_content.fighter
                .jab_startup_ticks +
            (uint32_t)invalid_jab_cancel_window_content.fighter
                .jab_active_ticks -
            UINT32_C(1));
    invalid_jab_final_content = content;
    invalid_jab_final_content.fighter.jab_final_damage_q16 = UINT32_C(0);
    invalid_jab_reset_content = content;
    invalid_jab_reset_content.fighter.reset_max_damage_q16 =
        UINT32_C(7) * UINT32_C(65536) + UINT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-knockback") ||
        !expect_status(
            pf_m4_validate_content(&invalid_strong_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-strong-knockback") ||
        !expect_status(
            pf_m4_validate_content(&invalid_tech_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-tech-invulnerability") ||
        !expect_status(
            pf_m4_validate_content(&invalid_getup_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overlapping-getup-attack-windows") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_break_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-break-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_cancel_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-powershield-cancel-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_surface_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-solid-geometry") ||
        !expect_status(
            pf_m4_validate_content(&invalid_v_cancel_scale_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-v-cancel-scale") ||
        !expect_status(
            pf_m4_validate_content(&invalid_v_cancel_window_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-v-cancel-window") ||
        !expect_status(
            pf_m4_validate_content(&invalid_grabbox_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-grabbox") ||
        !expect_status(
            pf_m4_validate_content(&invalid_grab_escape_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-grab-escape-window") ||
        !expect_status(
            pf_m4_validate_content(&invalid_throw_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-throw-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_dash_attack_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-dash-attack-speed") ||
        !expect_status(
            pf_m4_validate_content(
                &invalid_boost_grab_window_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-boost-grab-window") ||
        !expect_status(
            pf_m4_validate_content(
                &invalid_jab_cancel_window_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-jab-cancel-window") ||
        !expect_status(
            pf_m4_validate_content(&invalid_jab_final_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-jab-final-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_jab_reset_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-jab-reset-data") ||
        !run_one_way_hit_test(&content, &view) ||
        !run_weight_test(&content, &view) ||
        !run_v_cancel_test(&v_cancel_content, &v_cancel_view) ||
        !run_v_cancel_snapshot_test(
            &v_cancel_content,
            &v_cancel_view) ||
        !run_crouch_cancel_test(&content, &view) ||
        !run_double_jump_cancel_counter_test(
            &double_jump_cancel_counter_content,
            &double_jump_cancel_counter_view) ||
        !run_aerial_hit_test(&content, &view) ||
        !run_strong_aerial_hit_test(&content, &view) ||
        !run_default_strong_tumble_test(&content, &view) ||
        !run_small_step_forward_smash_test(
            &small_step_forward_smash_content,
            &small_step_forward_smash_view) ||
        !run_drop_cancel_test(
            &drop_cancel_content,
            &drop_cancel_view,
            &drop_cancel_whiff_view) ||
        !run_sharking_test(
            &drop_cancel_content,
            &drop_cancel_view) ||
        !run_cross_up_test(
            &cross_up_content,
            &cross_up_view) ||
        !run_juggling_test(
            &juggling_content,
            &juggling_view) ||
        !run_ladder_test(
            &ladder_content,
            &ladder_view) ||
        !run_kill_confirm_test(
            &kill_confirm_content,
            &kill_confirm_view) ||
        !run_zero_to_death_test(
            &kill_confirm_content,
            &kill_confirm_view) ||
        !run_surface_tech_test(
            &wall_tech_content,
            &wall_tech_view,
            &ceiling_tech_content,
            &ceiling_tech_view) ||
        !run_whiff_and_trade_test(&content, &view) ||
        !run_approach_test(
            &approach_content,
            &approach_view) ||
        !run_spacing_counter_snapshot_test(
            &spacing_safe_content,
            &spacing_safe_view) ||
        !run_spacing_close_negative_test(
            &spacing_close_content,
            &spacing_close_view) ||
        !run_spacing_far_negative_test(
            &spacing_far_content,
            &spacing_far_view) ||
        !run_spacing_shield_control_test(
            &spacing_safe_content,
            &spacing_safe_view) ||
        !run_shield_state_test(&content, &view) ||
        !run_dashing_shield_test(&content, &view) ||
        !run_shield_block_test(&content, &view) ||
        !run_powershield_cancel_test(&content, &view) ||
        !run_powershield_cancel_replay_test(&view) ||
        !run_aerial_l_cancel_replay_test() ||
        !run_shield_break_test(
            &shield_break_content,
            &shield_break_view) ||
        !run_di_and_sdi_test(
            &reaction_content,
            &reaction_view) ||
        !run_knockdown_and_tech_test(
            &reaction_content,
            &reaction_view) ||
        !run_tech_chase_test(
            &tech_invulnerability_content,
            &tech_invulnerability_view) ||
        !run_floor_getup_option_test(
            &reaction_content,
            &reaction_view) ||
        !run_getup_attack_hit_test(
            &floor_attack_content,
            &floor_attack_view) ||
        !run_floor_recovery_snapshot_test(
            &reaction_content,
            &reaction_view) ||
        !run_tech_invulnerability_hit_test(
            &tech_invulnerability_content,
            &tech_invulnerability_view) ||
        !run_air_dodge_invulnerability_hit_test(
            &content,
            &view) ||
        !run_ground_dodge_invulnerability_hit_test(
            &content,
            &view) ||
        !run_boost_grab_test(
            &boost_grab_content,
            &boost_grab_view) ||
        !run_jab_cancel_test(
            &jab_cancel_close_content,
            &jab_cancel_close_view,
            &jab_cancel_far_view) ||
        !run_jab_reset_test(
            &jab_reset_content,
            &jab_reset_view,
            &jab_reset_exact_content,
            &jab_reset_exact_view,
            &jab_reset_over_damage_view,
            &jab_reset_over_hitstun_view) ||
        !run_jump_cancelled_grab_test(
            &grab_close_content,
            &grab_close_view,
            &grab_far_view) ||
        !run_jump_cancelling_test(
            &grab_close_content,
            &grab_close_view,
            &grab_far_view) ||
        !run_pummel_test(
            &grab_close_content,
            &grab_close_view) ||
        !run_directional_throw_test(
            &grab_close_content,
            &grab_close_view) ||
        !run_chain_grab_route(
            &grab_close_content,
            &grab_close_view) ||
        !run_chain_grab_snapshot_test(
            &grab_close_content,
            &grab_close_view) ||
        !run_chain_grab_di_escape_test(
            &chain_grab_escape_content,
            &chain_grab_escape_view) ||
        !run_grab_damage_escape_test(&grab_damage_view) ||
        !run_grab_team_resolution_test(&team_wobble_view) ||
        !run_team_handoff_route(
            &team_wobble_content,
            &team_wobble_view) ||
        !run_hitlag_snapshot_test(&view) ||
        !run_shield_hitlag_snapshot_test(&view) ||
        !run_deterministic_trace(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-combat=pass content_schema=%u deterministic_ticks=%" PRIu64
        " combat_invariants=666 journal_invariants=50 weight=1 crouch_cancel=1 double_jump_cancel_counter=1 approach=1 spacing=1 sharking=1 cross_up=1 mindgame=1 juggling=1 ladder=1 kill_confirm=1 zero_to_death=1 jab_reset=1 jab_cancel=1 boost_grab=1 jump_cancelled_grab=1 jump_cancel=1 pummel=1 directional_throws=1 chain_grab=1 team_wobble=1\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        TEST_DETERMINISTIC_TICKS);
    return 0;
}
