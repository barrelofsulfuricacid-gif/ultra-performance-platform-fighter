#include "pf/m4.h"
#include "pf/replay.h"
#include "pf/rl.h"
#include "pf/sim.h"
#include "../../src/sim/sim_falcon_frame_data.h"
#include "../../src/sim/sim_internal.h"
#include "../../src/sim/sim_collision.h"
#include "../../src/sim/sim_melee.h"
#include "../../src/sim/sim_ssbm_common_data.h"
#include "../../src/sim/sim_ssbm_damage.h"
#include "../../src/sim/sim_ssbm_stage_data.h"
#include "ssbm_stored_oracle.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#include "../../generated/data/m4_ssbm_falcon_common_hurt_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_turn_hurt_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_dive_grab_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_punch_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_damage_response_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_ground_knockback_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_surface_response_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_battlefield_surface_response_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_battlefield_bounce_recontact_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_floor_response_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_prone_response_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_player_push_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_slope_ledge_response_oracle.inc"
#include "../../generated/data/m4_ssbm_falcon_ledge_options_oracle.inc"

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

static int make_player_push_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "player-push-default-content"))
    {
        return 0;
    }

    /* Each fighter starts 3.6 Melee units from center. This is just outside
       Falcon's strict 3.5 + 3.5 source-radius overlap boundary. */
    out_content->stage.spawn_spacing_q16 = (int32_t)(
        (INT64_C(216) * PF_Q16_ONE + INT64_C(287)) / INT64_C(575));
    out_content->item.enabled = UINT8_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "player-push-content-view");
}

static int make_shield_poke_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "shield-poke-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        (INT32_C(4) * PF_Q16_ONE) / INT32_C(5);
    out_content->fighter.jab_hitbox_offset_x_q16 = PF_Q16_ONE;
    out_content->fighter.jab_hitbox_offset_y_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(20);
    out_content->fighter.jab_hitbox_half_height_q16 =
        PF_Q16_ONE / INT32_C(20);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
    out_content->item.enabled = UINT8_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "shield-poke-content-view");
}

static int make_ledge_attack_content(
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "ledge-attack-default-content"))
    {
        return 0;
    }

    out_content->stage.spawn_spacing_q16 =
        INT32_C(2) * PF_Q16_ONE;
    out_content->stage.solid_left_q16 =
        -INT32_C(27) * PF_Q16_ONE;
    out_content->stage.solid_right_q16 =
        -INT32_C(14) * PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->fighter.ledge_invulnerability_ticks = UINT16_C(1);
    out_content->item.enabled = UINT8_C(0);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "ledge-attack-content-view");
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
    out_content->fighter.jab_melee_knockback.enabled = UINT8_C(0);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
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
    out_content->fighter.jab_melee_knockback.enabled = UINT8_C(0);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
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
    out_content->fighter.jab_melee_knockback.enabled = UINT8_C(0);
    out_content->fighter.jab_startup_ticks = UINT16_C(1);
    out_content->fighter.jab_active_ticks = UINT16_C(1);
    out_content->fighter.jab_recovery_ticks = UINT16_C(1);
    out_content->fighter.jab_hitlag_ticks = UINT16_C(1);
    out_content->fighter.jab_combo_input_begin_tick = UINT16_C(2);
    out_content->fighter.jab_combo_input_end_tick = UINT16_C(2);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
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
    /* These fixtures test authored spacing, not Falcon's generated poses. */
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
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
    out_content->stage.revival_platform_start_y_q16 =
        INT32_C(5) * PF_Q16_ONE;
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
    out_content->fighter.jab_melee_knockback.enabled = UINT8_C(0);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
    out_content->fighter.strong_base_knockback_x_q16 =
        PF_Q16_ONE / INT32_C(20);
    out_content->fighter.strong_base_knockback_y_q16 =
        (INT32_C(37) * PF_Q16_ONE) / INT32_C(40);
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
    out_content->stage.revival_platform_start_y_q16 =
        INT32_C(9) * PF_Q16_ONE;
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
        PF_Q16_ONE / INT32_C(5);
    out_content->fighter.jab_base_knockback_y_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(10);
    out_content->fighter.jab_knockback_growth_q16 =
        PF_Q16_ONE / INT32_C(4096);
    out_content->fighter.jab_melee_knockback.enabled = UINT8_C(0);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
    out_content->fighter.tumble_hitstun_threshold_ticks =
        UINT16_C(20);
    /* Keep reaction-state unit routes away from an unrelated ledge now that
     * source-authentic grounded knockback survives floor impact. */
    out_content->stage.floor_left_q16 =
        -INT32_C(48) * PF_Q16_ONE;
    out_content->stage.floor_right_q16 =
        INT32_C(48) * PF_Q16_ONE;
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
    out_content->fighter.jab_melee_knockback.enabled = UINT8_C(0);
    out_content->fighter.reference_frame_data_enabled = UINT8_C(0);
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

static int initialize_sim_with_arena_extent(
    test_sim_storage *storage,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    int reset,
    int32_t arena_half_width_q16,
    int32_t arena_ceiling_q16,
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
    if (arena_half_width_q16 != INT32_C(0))
    {
        config.arena_half_width_q16 = arena_half_width_q16;
    }
    if (arena_ceiling_q16 != INT32_C(0))
    {
        config.arena_ceiling_q16 = arena_ceiling_q16;
    }
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

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    uint8_t player_count,
    pf_sim_mode mode,
    int reset,
    pf_sim **out_sim)
{
    return initialize_sim_with_arena_extent(
        storage,
        content,
        player_count,
        mode,
        reset,
        INT32_C(0),
        INT32_C(0),
        out_sim);
}

static int clone_sim_through_canonical_save(
    pf_sim *source_sim,
    const pf_content_view *content,
    test_sim_storage *destination_storage,
    pf_sim **out_destination_sim)
{
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination = {
        save_bytes,
        sizeof(save_bytes),
        (size_t)0};
    pf_bytes source;
    pf_state_hash source_hash;
    pf_state_hash destination_hash;
    size_t save_size = (size_t)0;

    if (source_sim == NULL || content == NULL ||
        destination_storage == NULL || out_destination_sim == NULL ||
        !expect_status(
            pf_sim_query_save_size(source_sim, &save_size),
            PF_STATUS_OK,
            "clone-query-save-size") ||
        save_size > sizeof(save_bytes) ||
        !expect_status(
            pf_sim_save(source_sim, &destination),
            PF_STATUS_OK,
            "clone-save") ||
        destination.size != save_size ||
        !initialize_sim(
            destination_storage,
            content,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            out_destination_sim))
    {
        return 0;
    }
    source.bytes = save_bytes;
    source.size = destination.size;
    return expect_status(
               pf_sim_load(*out_destination_sim, source),
               PF_STATUS_OK,
               "clone-load") &&
           expect_status(
               pf_sim_hash(source_sim, &source_hash),
               PF_STATUS_OK,
               "clone-source-hash") &&
           expect_status(
               pf_sim_hash(*out_destination_sim, &destination_hash),
               PF_STATUS_OK,
               "clone-destination-hash") &&
           memcmp(
               source_hash.bytes,
               destination_hash.bytes,
               sizeof(source_hash.bytes)) == 0;
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
            UINT64_C(0),
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
        const uint64_t target_buttons =
            target_attacks != 0 &&
                    inspection.players[0].action_ticks == UINT16_C(1)
                ? PF_INPUT_BUTTON_STRONG_ATTACK
                : UINT64_C(0);

        if (trigger_lead_ticks != UINT32_MAX &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
            (uint32_t)inspection.players[0].action_ticks +
                    trigger_lead_ticks + UINT32_C(1) ==
                (uint32_t)content->fighter.aerial_startup_ticks +
                    UINT32_C(1))
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
                target_buttons,
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
        return fail(
            hit_found == 0
                ? "v-cancel-air-hit-event-missing"
                : "v-cancel-air-target-not-in-hitlag");
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

static uint32_t expected_stale_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t damage_q16,
    uint16_t matching_slots)
{
    uint32_t reduction_q16 = UINT32_C(0);
    uint32_t slot;

    for (slot = UINT32_C(0);
         slot < (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++slot)
    {
        if ((matching_slots & (uint16_t)(UINT16_C(1) << slot)) !=
            UINT16_C(0))
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

static uint32_t expected_repeated_move_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t damage_q16,
    uint32_t hit_count)
{
    uint32_t total_q16 = UINT32_C(0);
    uint32_t hit;

    for (hit = UINT32_C(0); hit < hit_count; ++hit)
    {
        const uint32_t occupied_slots =
            hit < (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY
                ? hit
                : (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
        const uint16_t matching_slots =
            (uint16_t)(
                occupied_slots == UINT32_C(0)
                    ? UINT32_C(0)
                    : (UINT32_C(1) << occupied_slots) -
                          UINT32_C(1));

        total_q16 += expected_stale_damage_q16(
            fighter,
            damage_q16,
            matching_slots);
    }
    return total_q16;
}

static int32_t expected_shield_pressure_lerp_q16(
    int32_t light_q16,
    int32_t dense_q16,
    uint16_t shield_strength)
{
    return light_q16 -
           (int32_t)(
               ((int64_t)(light_q16 - dense_q16) *
                (int64_t)shield_strength) /
               (int64_t)UINT16_MAX);
}

static uint32_t expected_shield_hit_damage_q16(
    uint32_t attack_damage_q16)
{
    uint32_t integer_damage = attack_damage_q16 >> 16U;

    if (attack_damage_q16 != UINT32_C(0) &&
        integer_damage == UINT32_C(0))
    {
        integer_damage = UINT32_C(1);
    }
    return integer_damage << 16U;
}

static uint32_t expected_shield_damage_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    uint16_t shield_strength)
{
    const uint32_t shield_hit_damage_q16 =
        expected_shield_hit_damage_q16(attack_damage_q16);
    const int32_t multiplier_q16 =
        expected_shield_pressure_lerp_q16(
            (int32_t)fighter->light_shield_damage_multiplier_q16,
            (int32_t)fighter->dense_shield_damage_multiplier_q16,
            shield_strength);

    return (uint32_t)(
        ((uint64_t)shield_hit_damage_q16 *
         (uint64_t)(uint32_t)multiplier_q16) >>
        16U);
}

static int64_t expected_shield_stun_duration_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    uint16_t shield_strength)
{
    const uint32_t shield_hit_damage_q16 =
        expected_shield_hit_damage_q16(attack_damage_q16);
    const int32_t multiplier_q16 =
        expected_shield_pressure_lerp_q16(
            fighter->light_shield_stun_damage_multiplier_q16,
            fighter->dense_shield_stun_damage_multiplier_q16,
            shield_strength);

    return (((int64_t)shield_hit_damage_q16 *
             (int64_t)multiplier_q16) >>
            16U) +
           (int64_t)fighter->shield_stun_base_q16;
}

static uint16_t expected_shield_stun_ticks(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    uint16_t shield_strength)
{
    int64_t ticks =
        (expected_shield_stun_duration_q16(
             fighter,
             attack_damage_q16,
             shield_strength) *
         INT64_C(200) / INT64_C(201)) >>
        16U;

    if (ticks < INT64_C(1))
    {
        ticks = INT64_C(1);
    }
    return (uint16_t)ticks;
}

static int32_t expected_shield_defender_pushback_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    uint16_t shield_strength,
    int powershield)
{
    int64_t pushback_q16 =
        (expected_shield_stun_duration_q16(
             fighter,
             attack_damage_q16,
             shield_strength) *
         (int64_t)fighter->shield_defender_pushback_stun_scale_q16) >>
        16U;

    if (powershield == 0)
    {
        pushback_q16 =
            (pushback_q16 *
             (int64_t)fighter
                 ->shield_defender_pushback_normal_scale_q16) >>
            16U;
    }
    if (pushback_q16 >
        (int64_t)fighter->shield_defender_pushback_max_q16)
    {
        pushback_q16 =
            (int64_t)fighter->shield_defender_pushback_max_q16;
    }
    return (int32_t)pushback_q16;
}

static int32_t expected_shield_attacker_pushback_q16(
    const pf_m4_fighter_data *fighter,
    uint32_t attack_damage_q16,
    uint16_t shield_strength)
{
    const uint32_t shield_hit_damage_q16 =
        expected_shield_hit_damage_q16(attack_damage_q16);
    const uint64_t pressure_damage_q16 =
        ((uint64_t)shield_hit_damage_q16 *
         (uint64_t)shield_strength) /
        (uint64_t)UINT16_MAX;

    return (int32_t)(
        ((pressure_damage_q16 *
          (uint64_t)(uint32_t)fighter
              ->shield_attacker_pushback_damage_q16) >>
         16U) +
        (uint64_t)(uint32_t)fighter
            ->shield_attacker_pushback_base_q16);
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

static int prepare_v_cancel_fall_special_target(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

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
    for (tick = UINT32_C(0);
         tick < UINT32_C(8) &&
         out_inspection->players[1].grounded != UINT8_C(0);
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
    if (out_inspection->players[1].grounded != UINT8_C(0) ||
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
            out_inspection) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIR_DODGE)
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(60) &&
         out_inspection->players[1].action_state !=
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
                out_inspection))
        {
            return 0;
        }
    }
    if (out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL ||
        out_inspection->players[1].tech_lockout_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=prepare-v-cancel-fall-special"
            " action=%u tick=%u grounded=%u tech_lockout=%u\n",
            (unsigned int)out_inspection->players[1].action_state,
            (unsigned int)out_inspection->players[1].action_ticks,
            (unsigned int)out_inspection->players[1].grounded,
            (unsigned int)out_inspection->players[1].tech_lockout_ticks);
        return 0;
    }
    return 1;
}

static int make_v_cancel_fall_special_setup(
    const pf_m4_content *content,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    *out_content = *content;
    /* This setup isolates FallSpecial V-cancel behavior. Production
     * EscapeAir duration, gravity, and landing are verified independently. */
    out_content->fighter.air_dodge_ticks = UINT16_C(41);
    out_content->fighter.air_dodge_invulnerability_begin_tick = UINT16_C(0);
    out_content->fighter.air_dodge_invulnerability_end_tick = UINT16_C(1);
    out_content->fighter.air_dodge_ordinary_physics_begin_tick = UINT16_C(1);
    out_content->fighter.gravity_q16 = INT32_C(1);
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "v-cancel-fall-special-setup-content");
}

static int run_v_cancel_fall_special_case(
    const pf_m4_content *content,
    uint32_t trigger_lead_ticks,
    test_v_cancel_result *out_result)
{
    test_sim_storage storage;
    pf_m4_content setup_content;
    pf_content_view setup_view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (!make_v_cancel_fall_special_setup(
            content,
            &setup_content,
            &setup_view) ||
        !initialize_sim(
            &storage,
            &setup_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_v_cancel_fall_special_target(sim, &inspection))
    {
        return 0;
    }
    if (trigger_lead_ticks == UINT32_C(3) &&
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
    for (tick = UINT32_C(0); tick < UINT32_C(3); ++tick)
    {
        const uint64_t attacker_buttons =
            tick == UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0);
        const uint16_t target_trigger =
            trigger_lead_ticks <= UINT32_C(2) &&
                    tick == UINT32_C(2) - trigger_lead_ticks
                ? UINT16_MAX
                : UINT16_C(0);

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                attacker_buttons,
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
    (void)fprintf(
        stderr,
        "m4-combat=fail operation=v-cancel-fall-special-hit"
        " target_action=%u target_x=%" PRId32 " target_y=%" PRId32 "\n",
        (unsigned int)inspection.players[1].action_state,
        inspection.players[1].position_x_q16,
        inspection.players[1].position_y_q16);
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
        inspection.players[1].last_hit_sequence != UINT32_C(2) ||
        inspection.players[1].last_hit_damage_q16 !=
            content->fighter.jab_damage_q16 ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].last_hit_tick + UINT64_C(1) !=
            inspection.tick ||
        test_last_result.event_count != UINT8_C(2) ||
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
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
        test_last_result.events[1].source_player !=
            PF_SIM_EVENT_NO_PLAYER ||
        test_last_result.events[1].target_player !=
            PF_SIM_EVENT_NO_PLAYER ||
        test_last_result.events[1].value_q16 != UINT32_C(0x00000d0d) ||
        test_last_result.events[1].velocity_x_q16 != INT32_C(0x0000000c) ||
        test_last_result.events[1].velocity_y_q16 != INT32_C(0) ||
        test_last_result.events[1].flags != UINT16_C(0) ||
        test_last_result.events[1].detail != UINT16_C(3) ||
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
         freeze_tick + UINT32_C(1) <
             (uint32_t)content->fighter.jab_hitlag_ticks;
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
    if (!step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP | PF_INPUT_BUTTON_ATTACK,
            &inspection))
    {
        return fail("hitlag-resume-tick");
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

static int32_t test_abs_i32(int32_t value)
{
    return value < INT32_C(0) ? -value : value;
}

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
    heavy.fighter.knockback_weight = UINT16_C(200);
    below_minimum.fighter.weight_q16 =
        PF_Q16_ONE / INT32_C(2) - INT32_C(1);
    above_maximum.fighter.weight_q16 =
        INT32_C(2) * PF_Q16_ONE + INT32_C(1);

    if (content->fighter.weight_q16 != PF_Q16_ONE ||
        content->fighter.jab_damage_q16 !=
            UINT32_C(2) * UINT32_C(65536) ||
        content->fighter.jab_startup_ticks != UINT16_C(2) ||
        content->fighter.jab_active_ticks != UINT16_C(3) ||
        content->fighter.jab_recovery_ticks != UINT16_C(16) ||
        content->fighter.jab_melee_knockback.enabled != UINT8_C(1) ||
        content->fighter.jab_melee_knockback.angle_degrees !=
            UINT16_C(80) ||
        content->fighter.jab_melee_knockback.growth != UINT16_C(100) ||
        content->fighter.jab_melee_knockback.weight_set != UINT16_C(20) ||
        content->fighter.jab_melee_knockback.base != UINT16_C(0) ||
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

    if (ordinary_reaction.velocity_x_q16 != INT32_C(1179) ||
        ordinary_reaction.velocity_y_q16 != -INT32_C(11369) ||
        ordinary_reaction.hitstun_ticks != UINT16_C(13) ||
        ordinary_reaction.hitlag_ticks != UINT16_C(3) ||
        test_abs_i32(heavy_reaction.velocity_x_q16) >=
            test_abs_i32(ordinary_reaction.velocity_x_q16) ||
        test_abs_i32(heavy_reaction.velocity_y_q16) >=
            test_abs_i32(ordinary_reaction.velocity_y_q16) ||
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

typedef struct test_directional_attack_reaction
{
    int32_t velocity_x_q16;
    int32_t velocity_y_q16;
    uint32_t damage_q16;
    uint16_t hitstun_ticks;
    uint16_t hitlag_ticks;
} test_directional_attack_reaction;

static int run_directional_attack_hit_case(
    const pf_content_view *view,
    const pf_m4_attack_data *attack,
    int16_t input_x,
    int16_t input_y,
    uint64_t attack_button,
    pf_m4_action_state expected_action,
    test_directional_attack_reaction *out_reaction)
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
            input_x,
            input_y,
            attack_button,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)expected_action ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        return fail("directional-attack-input-route");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)attack->startup_ticks +
                    (uint32_t)attack->active_ticks;
         ++tick)
    {
        const pf_sim_event *event;

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
        event = find_last_tick_event(PF_SIM_EVENT_HIT);
        if (event == NULL)
        {
            continue;
        }
        if (event->source_player != UINT8_C(0) ||
            event->target_player != UINT8_C(1) ||
            event->detail != (uint16_t)expected_action ||
            event->value_q16 != attack->damage_q16 ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG ||
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG ||
            inspection.players[1].damage_q16 != attack->damage_q16 ||
            inspection.players[1].hitlag_ticks !=
                pf_m4_melee_hitlag_ticks(
                    attack->damage_q16,
                    UINT8_C(0),
                    UINT32_C(65536)))
        {
            return fail("directional-attack-hit-event");
        }
        out_reaction->velocity_x_q16 = event->velocity_x_q16;
        out_reaction->velocity_y_q16 = event->velocity_y_q16;
        out_reaction->damage_q16 = event->value_q16;
        out_reaction->hitstun_ticks =
            inspection.players[1].hitstun_ticks;
        out_reaction->hitlag_ticks =
            inspection.players[1].hitlag_ticks;
        return 1;
    }
    switch (expected_action)
    {
    case PF_M4_ACTION_UP_ATTACK:
        return fail("up-tilt-hit-missing");
    case PF_M4_ACTION_DOWN_ATTACK:
        return fail("down-tilt-hit-missing");
    case PF_M4_ACTION_FORWARD_ATTACK:
        return fail("forward-tilt-hit-missing");
    case PF_M4_ACTION_FORWARD_STRONG_ATTACK:
        return fail("forward-smash-hit-missing");
    case PF_M4_ACTION_UP_STRONG_ATTACK:
        return fail("up-smash-hit-missing");
    case PF_M4_ACTION_DOWN_STRONG_ATTACK:
        return fail("down-smash-hit-missing");
    default:
        return fail("directional-attack-hit-missing");
    }
}

static int directional_attack_reaction_matches_effect(
    const pf_m4_fighter_data *fighter,
    const pf_m4_attack_data *attack,
    const test_directional_attack_reaction *reaction,
    const pf_m4_reference_hit_effect *effect,
    uint8_t target_grounded)
{
    pf_m4_melee_knockback_data knockback = {0};
    pf_m4_melee_knockback_result result;
    const pf_m4_ssbm_damage_response_attributes *damage_response =
        pf_m4_ssbm_common_reference_damage_response();
    uint32_t target_hitlag_multiplier_q16 = UINT32_C(65536);

    if (effect == NULL || damage_response == NULL)
    {
        return 0;
    }
    if (effect->element == (uint8_t)PF_M4_REFERENCE_HIT_ELECTRIC)
    {
        target_hitlag_multiplier_q16 =
            damage_response->electric_hitlag_scale_q16;
    }
    knockback.angle_degrees = effect->angle_degrees;
    knockback.growth = effect->growth;
    knockback.weight_set = effect->weight_set;
    knockback.base = effect->base;
    knockback.enabled = UINT8_C(1);
    result = pf_m4_melee_knockback_for_state(
        &knockback,
        fighter->knockback_weight,
        attack->damage_q16,
        attack->damage_q16,
        target_grounded,
        UINT8_C(0),
        UINT8_C(0),
        target_hitlag_multiplier_q16);

    if (reaction->velocity_x_q16 != result.velocity_x_q16 ||
        reaction->velocity_y_q16 != -result.velocity_y_q16 ||
        reaction->damage_q16 != attack->damage_q16 ||
        reaction->hitstun_ticks != result.hitstun_ticks ||
        reaction->hitlag_ticks != result.hitlag_ticks)
    {
        (void)fprintf(
            stderr,
            "m4-combat=detail operation=reference-hit-response"
            " angle=%u actual_v=(%d,%d) expected_v=(%d,%d)"
            " actual_timers=(%u,%u) expected_timers=(%u,%u)\n",
            (unsigned int)effect->angle_degrees,
            reaction->velocity_x_q16,
            reaction->velocity_y_q16,
            result.velocity_x_q16,
            -result.velocity_y_q16,
            (unsigned int)reaction->hitlag_ticks,
            (unsigned int)reaction->hitstun_ticks,
            (unsigned int)result.hitlag_ticks,
            (unsigned int)result.hitstun_ticks);
        return 0;
    }
    return 1;
}

static int directional_attack_reaction_matches(
    const pf_m4_fighter_data *fighter,
    const pf_m4_attack_data *attack,
    const test_directional_attack_reaction *reaction,
    pf_m4_falcon_move_index move_index)
{
    return directional_attack_reaction_matches_effect(
        fighter,
        attack,
        reaction,
        pf_m4_falcon_reference_primary_effect(move_index),
        UINT8_C(1));
}

static int run_directional_attack_snapshot_test(
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
    int hit_found = 0;

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
        !step_reaction_duel(
            source,
            INT16_C(0),
            (int16_t)-(
                (int32_t)content->fighter.dash_axis_threshold -
                INT32_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection))
    {
        return fail("directional-attack-snapshot-setup");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.up_attack.startup_ticks +
                    (uint32_t)content->fighter.up_attack.active_ticks;
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
                &source_inspection))
        {
            return 0;
        }
        if (find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            hit_found = 1;
            break;
        }
    }
    if (hit_found == 0 ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "directional-attack-query-save-size") ||
        save_size != (size_t)915)
    {
        return fail("directional-attack-hitlag-snapshot-boundary");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "directional-attack-save") ||
        destination.size != save_size)
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "directional-attack-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.up_attack.hitlag_ticks +
                    UINT32_C(4);
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
                "directional-attack-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "directional-attack-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("directional-attack-snapshot-continuation");
        }
    }
    return 1;
}

static int run_smash_charge_snapshot_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_m4_inspection source_inspection;
    pf_m4_inspection loaded_inspection;
    pf_tick_result source_result;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    const pf_m4_reference_move *forward_smash_move =
        pf_m4_falcon_reference_move(PF_M4_FALCON_FORWARD_SMASH);
    const uint16_t snapshot_charge_ticks = UINT16_C(10);
    const uint32_t maximum_damage_q16 =
        content->fighter.forward_strong_attack.damage_q16 +
        (uint32_t)(
            (uint64_t)content->fighter.forward_strong_attack.damage_q16 *
            (uint64_t)content->fighter.smash_charge_damage_bonus_q16 >>
            16U);
    uint32_t tick;
    int hit_seen = 0;
    uint8_t trade_source_mask = UINT8_C(0);

    if (forward_smash_move == NULL ||
        forward_smash_move->charge_frame == UINT16_C(0))
    {
        return fail("smash-charge-missing-source-frame");
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
        !step_reaction_duel(
            source,
            INT16_C(32767),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
        source_inspection.players[0].action_ticks != UINT16_C(1) ||
        source_inspection.players[0].smash_charge_ticks != UINT16_C(0))
    {
        return fail("smash-charge-entry");
    }

    for (tick = UINT32_C(1);
         tick < (uint32_t)forward_smash_move->charge_frame;
         ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(32767),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
            source_inspection.players[0].action_ticks !=
                (uint16_t)(tick + UINT32_C(1)) ||
            source_inspection.players[0].smash_charge_ticks !=
                UINT16_C(0))
        {
            return fail("smash-charge-precharge");
        }
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)snapshot_charge_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(32767),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &source_inspection) ||
            source_inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
            source_inspection.players[0].action_ticks !=
                forward_smash_move->charge_frame ||
            source_inspection.players[0].smash_charge_ticks !=
                (uint16_t)(tick + UINT32_C(1)))
        {
            return fail("smash-charge-hold");
        }
    }

    if (!expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "smash-charge-query-save-size") ||
        save_size != (size_t)915)
    {
        return fail("smash-charge-save-size");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "smash-charge-save") ||
        destination.size != save_size ||
        memcmp(save_bytes, "PFSAVE58", (size_t)8) != 0)
    {
        return fail("smash-charge-save-format");
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "smash-charge-load"))
    {
        return 0;
    }

    while (source_inspection.players[0].action_state ==
           (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(32767),
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
                INT16_C(32767),
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
                sizeof(source_result.events)) != 0 ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "smash-charge-source-hold-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "smash-charge-loaded-hold-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("smash-charge-loaded-hold");
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
        source_inspection.players[0].action_ticks !=
            forward_smash_move->charge_frame ||
        source_inspection.players[0].smash_charge_ticks !=
            content->fighter.smash_charge_max_ticks ||
        loaded_inspection.players[0].action_state !=
            source_inspection.players[0].action_state)
    {
        return fail("smash-charge-maximum-release");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(96); ++tick)
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
                &source_inspection))
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
                    (uint16_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK &&
                source_result.events[event_index].value_q16 ==
                    maximum_damage_q16)
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
                "smash-charge-source-release-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "smash-charge-loaded-release-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("smash-charge-loaded-release");
        }
        if (source_inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            loaded_inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
    }
    if (hit_seen == 0 ||
        source_inspection.players[1].damage_q16 != maximum_damage_q16 ||
        loaded_inspection.players[1].damage_q16 != maximum_damage_q16 ||
        source_inspection.players[0].smash_charge_ticks != UINT16_C(0) ||
        loaded_inspection.players[0].smash_charge_ticks != UINT16_C(0))
    {
        return fail("smash-charge-maximum-damage-and-clear");
    }

    if (!expect_status(
            pf_sim_reset(source, UINT64_C(0x534d415348545244)),
            PF_STATUS_OK,
            "smash-charge-trade-reset") ||
        !step_reaction_duel(
            source,
            INT16_C(32767),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &source_inspection) ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE)
    {
        return fail("smash-charge-trade-entry");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)forward_smash_move->charge_frame +
                    (uint32_t)content->fighter.smash_charge_max_ticks +
                    UINT32_C(2) &&
         source_inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE &&
         source_inspection.players[1].action_state ==
             (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE;
         ++tick)
    {
        if (!step_reaction_duel(
                source,
                INT16_C(32767),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                INT16_C(-32767),
                INT16_C(0),
                PF_INPUT_BUTTON_ATTACK,
                UINT16_C(0),
                &source_inspection))
        {
            return fail("smash-charge-trade-hold");
        }
    }
    if (source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK ||
        source_inspection.players[0].action_ticks !=
            forward_smash_move->charge_frame ||
        source_inspection.players[1].action_ticks !=
            forward_smash_move->charge_frame ||
        source_inspection.players[0].smash_charge_ticks !=
            content->fighter.smash_charge_max_ticks ||
        source_inspection.players[1].smash_charge_ticks !=
            content->fighter.smash_charge_max_ticks)
    {
        return fail("smash-charge-trade-release");
    }
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.forward_strong_attack.startup_ticks +
                 (uint32_t)content->fighter.forward_strong_attack.active_ticks +
                 UINT32_C(2);
         ++tick)
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
                &source_inspection))
        {
            return fail("smash-charge-trade-schedule");
        }
        for (event_index = UINT32_C(0);
             event_index < (uint32_t)test_last_result.event_count;
             ++event_index)
        {
            const pf_sim_event *event =
                &test_last_result.events[event_index];

            if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
                event->detail ==
                    (uint16_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK &&
                event->value_q16 == maximum_damage_q16 &&
                event->source_player < UINT8_C(2))
            {
                trade_source_mask |=
                    (uint8_t)(UINT32_C(1) << event->source_player);
            }
        }
        if (trade_source_mask == UINT8_C(3))
        {
            break;
        }
    }
    if (trade_source_mask != UINT8_C(0) ||
        source_inspection.players[0].damage_q16 != UINT32_C(0) ||
        source_inspection.players[1].damage_q16 != UINT32_C(0) ||
        source_inspection.players[0].smash_charge_ticks != UINT16_C(0) ||
        source_inspection.players[1].smash_charge_ticks != UINT16_C(0))
    {
        return fail("smash-charge-slot-independent-clank");
    }
    return 1;
}

static int run_directional_ground_attack_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    const pf_m4_ssbm_ground_input_attributes *ground_input =
        pf_m4_ssbm_common_reference_ground_input();
    pf_m4_content changed = *content;
    pf_m4_content close = *content;
    pf_m4_content invalid_extent = *content;
    pf_m4_content invalid_timing = *content;
    pf_m4_content invalid_charge = *content;
    pf_content_view changed_view;
    pf_content_view close_view;
    test_directional_attack_reaction up_reaction;
    test_directional_attack_reaction down_reaction;
    test_directional_attack_reaction forward_reaction;
    test_directional_attack_reaction forward_strong_reaction;
    test_directional_attack_reaction up_strong_reaction;
    test_directional_attack_reaction down_strong_reaction;
    test_sim_storage control_storage;
    pf_sim *control = NULL;
    pf_m4_inspection inspection;
    uint32_t tick;

    if (ground_input == NULL)
    {
        return fail("directional-attack-missing-source-ground-input");
    }

    changed.fighter.up_attack.damage_q16 += UINT32_C(1);
    close.stage.spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    close.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    close.stage.platform_motion_amplitude_q16 = INT32_C(0);
    invalid_extent.fighter.down_attack.hitbox_half_height_q16 =
        INT32_C(0);
    invalid_timing.fighter.up_attack.startup_ticks = UINT16_C(0);
    invalid_charge.fighter.smash_charge_max_ticks = UINT16_C(0);
    if (content->fighter.up_attack.damage_q16 !=
            UINT32_C(13) * UINT32_C(65536) ||
        content->fighter.up_attack.startup_ticks != UINT16_C(16) ||
        content->fighter.up_attack.active_ticks != UINT16_C(5) ||
        content->fighter.up_attack.recovery_ticks != UINT16_C(18) ||
        content->fighter.down_attack.damage_q16 !=
            UINT32_C(12) * UINT32_C(65536) ||
        content->fighter.down_attack.startup_ticks != UINT16_C(9) ||
        content->fighter.down_attack.active_ticks != UINT16_C(6) ||
        content->fighter.down_attack.recovery_ticks != UINT16_C(20) ||
        content->fighter.forward_attack.damage_q16 !=
            UINT32_C(11) * UINT32_C(65536) ||
        content->fighter.forward_attack.startup_ticks != UINT16_C(8) ||
        content->fighter.forward_attack.active_ticks != UINT16_C(3) ||
        content->fighter.forward_attack.recovery_ticks != UINT16_C(18) ||
        content->fighter.forward_strong_attack.damage_q16 !=
            UINT32_C(20) * UINT32_C(65536) ||
        content->fighter.forward_strong_attack.startup_ticks !=
            UINT16_C(17) ||
        content->fighter.forward_strong_attack.active_ticks !=
            UINT16_C(4) ||
        content->fighter.forward_strong_attack.recovery_ticks !=
            UINT16_C(43) ||
        content->fighter.up_strong_attack.damage_q16 !=
            UINT32_C(8) * UINT32_C(65536) ||
        content->fighter.up_strong_attack.startup_ticks != UINT16_C(20) ||
        content->fighter.up_strong_attack.active_ticks != UINT16_C(8) ||
        content->fighter.up_strong_attack.recovery_ticks != UINT16_C(26) ||
        content->fighter.down_strong_attack.damage_q16 !=
            UINT32_C(18) * UINT32_C(65536) ||
        content->fighter.down_strong_attack.startup_ticks != UINT16_C(18) ||
        content->fighter.down_strong_attack.active_ticks != UINT16_C(14) ||
        content->fighter.down_strong_attack.recovery_ticks != UINT16_C(17) ||
        content->fighter.smash_charge_damage_bonus_q16 !=
            UINT32_C(24064) ||
        content->fighter.smash_charge_max_ticks != UINT16_C(60) ||
        !expect_status(
            pf_m4_make_content_view(&changed, &changed_view),
            PF_STATUS_OK,
            "directional-attack-changed-content-view") ||
        !expect_status(
            pf_m4_make_content_view(&close, &close_view),
            PF_STATUS_OK,
            "directional-attack-close-content-view") ||
        memcmp(
            view->content_hash.bytes,
            changed_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        !expect_status(
            pf_m4_validate_content(&invalid_extent),
            PF_STATUS_INVALID_CONFIG,
            "reject-directional-attack-invalid-extent") ||
        !expect_status(
            pf_m4_validate_content(&invalid_timing),
            PF_STATUS_INVALID_CONFIG,
            "reject-directional-attack-invalid-timing") ||
        !expect_status(
            pf_m4_validate_content(&invalid_charge),
            PF_STATUS_INVALID_CONFIG,
            "reject-smash-charge-invalid-maximum"))
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic directional"
            " up=%u/%u/%u/%u down=%u/%u/%u/%u"
            " forward=%u/%u/%u/%u fsmash=%u/%u/%u/%u"
            " usmash=%u/%u/%u/%u dsmash=%u/%u/%u/%u"
            " charge_bonus=%u charge_max=%u hash_equal=%d\n",
            content->fighter.up_attack.damage_q16 / UINT32_C(65536),
            (unsigned int)content->fighter.up_attack.startup_ticks,
            (unsigned int)content->fighter.up_attack.active_ticks,
            (unsigned int)content->fighter.up_attack.recovery_ticks,
            content->fighter.down_attack.damage_q16 / UINT32_C(65536),
            (unsigned int)content->fighter.down_attack.startup_ticks,
            (unsigned int)content->fighter.down_attack.active_ticks,
            (unsigned int)content->fighter.down_attack.recovery_ticks,
            content->fighter.forward_attack.damage_q16 / UINT32_C(65536),
            (unsigned int)content->fighter.forward_attack.startup_ticks,
            (unsigned int)content->fighter.forward_attack.active_ticks,
            (unsigned int)content->fighter.forward_attack.recovery_ticks,
            content->fighter.forward_strong_attack.damage_q16 / UINT32_C(65536),
            (unsigned int)content->fighter.forward_strong_attack.startup_ticks,
            (unsigned int)content->fighter.forward_strong_attack.active_ticks,
            (unsigned int)content->fighter.forward_strong_attack.recovery_ticks,
            content->fighter.up_strong_attack.damage_q16 / UINT32_C(65536),
            (unsigned int)content->fighter.up_strong_attack.startup_ticks,
            (unsigned int)content->fighter.up_strong_attack.active_ticks,
            (unsigned int)content->fighter.up_strong_attack.recovery_ticks,
            content->fighter.down_strong_attack.damage_q16 / UINT32_C(65536),
            (unsigned int)content->fighter.down_strong_attack.startup_ticks,
            (unsigned int)content->fighter.down_strong_attack.active_ticks,
            (unsigned int)content->fighter.down_strong_attack.recovery_ticks,
            content->fighter.smash_charge_damage_bonus_q16,
            (unsigned int)content->fighter.smash_charge_max_ticks,
            memcmp(
                view->content_hash.bytes,
                changed_view.content_hash.bytes,
                sizeof(view->content_hash.bytes)) == 0);
        return fail("directional-attack-data-and-hash");
    }

    if (!run_directional_attack_hit_case(
            &close_view,
            &content->fighter.up_attack,
            INT16_C(0),
            (int16_t)-(
                (int32_t)ground_input->vertical_smash_axis_threshold -
                INT32_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_UP_ATTACK,
            &up_reaction) ||
        !run_directional_attack_hit_case(
            &close_view,
            &content->fighter.down_attack,
            INT16_C(0),
            (int16_t)(
                ground_input->vertical_smash_axis_threshold - UINT16_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_DOWN_ATTACK,
            &down_reaction) ||
        !run_directional_attack_hit_case(
            &close_view,
            &content->fighter.forward_attack,
            (int16_t)(content->fighter.axis_dead_zone + UINT16_C(1)),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_FORWARD_ATTACK,
            &forward_reaction) ||
        !run_directional_attack_hit_case(
            &close_view,
            &content->fighter.forward_strong_attack,
            INT16_C(32767),
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_FORWARD_STRONG_ATTACK,
            &forward_strong_reaction) ||
        !run_directional_attack_hit_case(
            &close_view,
            &content->fighter.up_strong_attack,
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_UP_STRONG_ATTACK,
            &up_strong_reaction) ||
        !run_directional_attack_hit_case(
            &close_view,
            &content->fighter.down_strong_attack,
            INT16_C(0),
            INT16_C(32767),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_DOWN_STRONG_ATTACK,
            &down_strong_reaction))
    {
        return 0;
    }

    if (!directional_attack_reaction_matches(
            &content->fighter,
            &content->fighter.up_attack,
            &up_reaction,
            PF_M4_FALCON_UP_TILT) ||
        !directional_attack_reaction_matches(
            &content->fighter,
            &content->fighter.down_attack,
            &down_reaction,
            PF_M4_FALCON_DOWN_TILT) ||
        !directional_attack_reaction_matches(
            &content->fighter,
            &content->fighter.forward_attack,
            &forward_reaction,
            PF_M4_FALCON_FORWARD_TILT) ||
        !directional_attack_reaction_matches(
            &content->fighter,
            &content->fighter.forward_strong_attack,
            &forward_strong_reaction,
            PF_M4_FALCON_FORWARD_SMASH) ||
        !directional_attack_reaction_matches(
            &content->fighter,
            &content->fighter.up_strong_attack,
            &up_strong_reaction,
            PF_M4_FALCON_UP_SMASH) ||
        !directional_attack_reaction_matches(
            &content->fighter,
            &content->fighter.down_strong_attack,
            &down_strong_reaction,
            PF_M4_FALCON_DOWN_SMASH))
    {
        return fail("directional-attack-exact-launch");
    }

    if (!initialize_sim(
            &control_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &control))
    {
        return 0;
    }
    if (!step_reaction_duel(
            control,
            INT16_C(0),
            (int16_t)(
                ground_input->vertical_tilt_axis_threshold - UINT16_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        !expect_status(
            pf_sim_reset(control, UINT64_C(0x4449524154544143)),
            PF_STATUS_OK,
            "directional-attack-diagonal-reset") ||
        !step_reaction_duel(
            control,
            INT16_C(32767),
            INT16_C(-32767),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH ||
        inspection.players[0].smash_charge_ticks != UINT16_C(0))
    {
        return fail("directional-attack-input-arbitration");
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(120) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE_HIGH;
         ++tick)
    {
        if (!step_reaction_duel(
                control,
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
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH ||
        inspection.players[0].action_ticks != UINT16_C(10) ||
        inspection.players[0].smash_charge_ticks != UINT16_C(0))
    {
        return fail("directional-attack-angled-smash-release");
    }

    if (!expect_status(
            pf_sim_reset(control, UINT64_C(0x4449525354524f4e)),
            PF_STATUS_OK,
            "directional-attack-strong-reset") ||
        !step_reaction_duel(
            control,
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
        !expect_status(
            pf_sim_reset(control, UINT64_C(0x4449524e45555452)),
            PF_STATUS_OK,
            "directional-attack-neutral-strong-reset") ||
        !step_reaction_duel(
            control,
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
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK)
    {
        return fail("directional-attack-input-arbitration");
    }

    return run_smash_charge_snapshot_test(&close, &close_view) &&
           run_directional_attack_snapshot_test(&close, &close_view);
}

static int start_directional_aerial_case(
    pf_sim *sim,
    const pf_m4_content *content,
    int turn_left,
    int16_t input_x,
    int16_t input_y,
    uint64_t attack_button,
    pf_m4_action_state expected_action,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (turn_left != 0)
    {
        const int16_t turn_axis = (int16_t)-(
            (int32_t)content->fighter.axis_dead_zone + INT32_C(1));

        if (!step_reaction_duel(
                sim,
                turn_axis,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                out_inspection))
        {
            return fail("directional-aerial-turn-setup");
        }
        for (tick = UINT32_C(1);
             tick <
                 (uint32_t)content->fighter.standing_turn_facing_tick;
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
                return fail("directional-aerial-turn-timing");
            }
        }
        if (out_inspection->players[0].facing != INT8_C(-1))
        {
            return fail("directional-aerial-turn-facing");
        }
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
            out_inspection))
    {
        return fail("directional-aerial-jump-start");
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
            return fail("directional-aerial-jump-squat");
        }
    }
    if (out_inspection->players[0].grounded != UINT8_C(0) ||
        out_inspection->players[1].grounded != UINT8_C(0) ||
        !step_reaction_duel(
            sim,
            input_x,
            input_y,
            attack_button,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection) ||
        out_inspection->players[0].action_state !=
            (uint8_t)expected_action ||
        out_inspection->players[0].action_ticks != UINT16_C(0))
    {
        return fail("directional-aerial-input-route");
    }
    return 1;
}

static int run_directional_aerial_route_case(
    const pf_m4_content *content,
    const pf_content_view *view,
    int turn_left,
    int16_t input_x,
    int16_t input_y,
    uint64_t attack_button,
    pf_m4_action_state expected_action)
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
           start_directional_aerial_case(
               sim,
               content,
               turn_left,
               input_x,
               input_y,
               attack_button,
               expected_action,
               &inspection);
}

static int run_directional_aerial_landing_case(
    const pf_m4_content *content,
    const pf_content_view *view,
    int16_t input_x,
    int16_t input_y,
    pf_m4_action_state expected_attack,
    pf_m4_action_state expected_landing,
    uint16_t expected_lag)
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
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("directional-aerial-landing-jump");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         (inspection.players[0].grounded != UINT8_C(0) ||
          inspection.players[0].velocity_y_q16 <
              -INT32_C(2) * content->fighter.gravity_q16);
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
            return fail("directional-aerial-landing-apex");
        }
    }
    if (tick == UINT32_C(80) ||
        !step_reaction_duel(
            sim,
            input_x,
            input_y,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)expected_attack)
    {
        return fail("directional-aerial-landing-route");
    }
    if (input_y <= INT16_C(0) &&
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_MAX,
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("directional-aerial-landing-fast-fall");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(80) &&
         inspection.players[0].grounded == UINT8_C(0);
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
            return fail("directional-aerial-landing-descent");
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)expected_landing ||
        inspection.players[0].action_ticks != UINT16_C(0))
    {
        return fail("directional-aerial-landing-state");
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(240) &&
         inspection.players[0].action_state ==
             (uint8_t)expected_landing;
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
            return fail("directional-aerial-landing-lag");
        }
    }
    if (tick != (uint32_t)expected_lag ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=directional-aerial-landing-duration"
            " expected=%u actual=%" PRIu32 " action=%u\n",
            (unsigned int)expected_lag,
            tick,
            (unsigned int)inspection.players[0].action_state);
        return 0;
    }
    return 1;
}

static int run_directional_aerial_hit_case(
    const pf_m4_content *content,
    const pf_content_view *view,
    const pf_m4_attack_data *attack,
    int turn_left,
    int16_t input_x,
    int16_t input_y,
    pf_m4_action_state expected_action,
    test_directional_attack_reaction *out_reaction)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_hit_effect *primary_effect;
    const pf_m4_ssbm_damage_response_attributes *damage_response =
        pf_m4_ssbm_common_reference_damage_response();
    uint32_t target_hitlag_multiplier_q16 = UINT32_C(65536);
    uint32_t tick;

    if (damage_response == NULL ||
        !pf_m4_falcon_reference_move_for_action(
            (uint8_t)expected_action,
            &move_index) ||
        (primary_effect =
             pf_m4_falcon_reference_primary_effect(move_index)) == NULL)
    {
        return fail("directional-aerial-missing-source-effect");
    }
    if (primary_effect->element ==
        (uint8_t)PF_M4_REFERENCE_HIT_ELECTRIC)
    {
        target_hitlag_multiplier_q16 =
            damage_response->electric_hitlag_scale_q16;
    }

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !start_directional_aerial_case(
            sim,
            content,
            turn_left,
            input_x,
            input_y,
            PF_INPUT_BUTTON_ATTACK,
            expected_action,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)attack->startup_ticks +
                    (uint32_t)attack->active_ticks + UINT32_C(2);
         ++tick)
    {
        const pf_sim_event *event;

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
        event = find_last_tick_event(PF_SIM_EVENT_HIT);
        if (event == NULL)
        {
            continue;
        }
        if (event->source_player != UINT8_C(0) ||
            event->target_player != UINT8_C(1) ||
            event->detail != (uint16_t)expected_action ||
            event->value_q16 != attack->damage_q16 ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG ||
            inspection.players[1].action_state !=
                (uint8_t)PF_M4_ACTION_HITLAG ||
            inspection.players[1].damage_q16 != attack->damage_q16 ||
            inspection.players[1].hitlag_ticks !=
                pf_m4_melee_hitlag_ticks(
                    attack->damage_q16,
                    UINT8_C(0),
                    target_hitlag_multiplier_q16))
        {
            (void)fprintf(
                stderr,
                "m4-combat=diagnostic aerial-hit expected=%u"
                " event=%u/%u/%u/%u attacker=%u/%u"
                " target_damage=%u target_hitlag=%u expected_hitlag=%u\n",
                (unsigned int)expected_action,
                (unsigned int)event->source_player,
                (unsigned int)event->target_player,
                (unsigned int)event->detail,
                event->value_q16,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[1].action_state,
                inspection.players[1].damage_q16,
                (unsigned int)inspection.players[1].hitlag_ticks,
                (unsigned int)pf_m4_melee_hitlag_ticks(
                    attack->damage_q16,
                    UINT8_C(0),
                    target_hitlag_multiplier_q16));
            return fail("directional-aerial-hit-event");
        }
        out_reaction->velocity_x_q16 = event->velocity_x_q16;
        out_reaction->velocity_y_q16 = event->velocity_y_q16;
        out_reaction->damage_q16 = event->value_q16;
        out_reaction->hitstun_ticks =
            inspection.players[1].hitstun_ticks;
        out_reaction->hitlag_ticks =
            inspection.players[1].hitlag_ticks;
        return 1;
    }
    (void)fprintf(
        stderr,
        "m4-combat=fail operation=directional-aerial-hit-missing"
        " expected_action=%u attacker=(%d,%d) target=(%d,%d)\n",
        (unsigned int)expected_action,
        inspection.players[0].position_x_q16,
        inspection.players[0].position_y_q16,
        inspection.players[1].position_x_q16,
        inspection.players[1].position_y_q16);
    return 0;
}

static int run_directional_aerial_snapshot_test(
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
    int hit_found = 0;

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
        !start_directional_aerial_case(
            source,
            content,
            0,
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_UP_AERIAL,
            &source_inspection))
    {
        return fail("directional-aerial-snapshot-setup");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.up_aerial.startup_ticks +
                    (uint32_t)content->fighter.up_aerial.active_ticks +
                    UINT32_C(2);
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
                &source_inspection))
        {
            return 0;
        }
        if (find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            hit_found = 1;
            break;
        }
    }
    if (hit_found == 0 ||
        source_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "directional-aerial-query-save-size") ||
        save_size != (size_t)915)
    {
        return fail("directional-aerial-hitlag-snapshot-boundary");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "directional-aerial-save") ||
        destination.size != save_size ||
        memcmp(save_bytes, "PFSAVE58", (size_t)8) != 0)
    {
        return fail("directional-aerial-save-format");
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "directional-aerial-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.up_aerial.hitlag_ticks +
                    UINT32_C(4);
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
                "directional-aerial-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "directional-aerial-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("directional-aerial-snapshot-continuation");
        }
    }
    return 1;
}

static int run_directional_aerial_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    const pf_m4_ssbm_ground_input_attributes *ground_input =
        pf_m4_ssbm_common_reference_ground_input();
    pf_m4_content changed = *content;
    pf_m4_content close = *content;
    pf_m4_content invalid_extent = *content;
    pf_m4_content invalid_timing = *content;
    pf_content_view changed_view;
    pf_content_view close_view;
    test_directional_attack_reaction forward_reaction;
    test_directional_attack_reaction back_reaction;
    test_directional_attack_reaction up_reaction;
    test_directional_attack_reaction down_reaction;
    const pf_m4_attack_data *attacks[4] = {
        &content->fighter.forward_aerial,
        &content->fighter.back_aerial,
        &content->fighter.up_aerial,
        &content->fighter.down_aerial};
    const test_directional_attack_reaction *reactions[4] = {
        &forward_reaction,
        &back_reaction,
        &up_reaction,
        &down_reaction};
    const pf_m4_falcon_move_index moves[4] = {
        PF_M4_FALCON_FORWARD_AERIAL,
        PF_M4_FALCON_BACK_AERIAL,
        PF_M4_FALCON_UP_AERIAL,
        PF_M4_FALCON_DOWN_AERIAL};
    uint32_t attack_index;

    if (ground_input == NULL)
    {
        return fail("directional-aerial-missing-source-ground-input");
    }

    changed.fighter.forward_aerial.damage_q16 += UINT32_C(1);
    close.stage.spawn_spacing_q16 =
        PF_Q16_ONE / INT32_C(2);
    close.stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    close.stage.platform_motion_amplitude_q16 = INT32_C(0);
    invalid_extent.fighter.down_aerial.hitbox_half_height_q16 =
        INT32_C(0);
    invalid_timing.fighter.back_aerial.startup_ticks = UINT16_C(0);
    if (content->fighter.forward_aerial.damage_q16 !=
            UINT32_C(18) * UINT32_C(65536) ||
        content->fighter.forward_aerial.startup_ticks != UINT16_C(13) ||
        content->fighter.forward_aerial.active_ticks != UINT16_C(17) ||
        content->fighter.forward_aerial.recovery_ticks != UINT16_C(9) ||
        content->fighter.back_aerial.damage_q16 !=
            UINT32_C(14) * UINT32_C(65536) ||
        content->fighter.back_aerial.startup_ticks != UINT16_C(9) ||
        content->fighter.back_aerial.active_ticks != UINT16_C(8) ||
        content->fighter.back_aerial.recovery_ticks != UINT16_C(18) ||
        content->fighter.up_aerial.damage_q16 !=
            UINT32_C(13) * UINT32_C(65536) ||
        content->fighter.up_aerial.startup_ticks != UINT16_C(5) ||
        content->fighter.up_aerial.active_ticks != UINT16_C(9) ||
        content->fighter.up_aerial.recovery_ticks != UINT16_C(19) ||
        content->fighter.down_aerial.damage_q16 !=
            UINT32_C(16) * UINT32_C(65536) ||
        content->fighter.down_aerial.startup_ticks != UINT16_C(15) ||
        content->fighter.down_aerial.active_ticks != UINT16_C(5) ||
        content->fighter.down_aerial.recovery_ticks != UINT16_C(24) ||
        content->fighter.forward_aerial_landing_lag_ticks !=
            UINT16_C(19) ||
        content->fighter.back_aerial_landing_lag_ticks != UINT16_C(18) ||
        content->fighter.up_aerial_landing_lag_ticks != UINT16_C(15) ||
        content->fighter.down_aerial_landing_lag_ticks != UINT16_C(24) ||
        !expect_status(
            pf_m4_make_content_view(&changed, &changed_view),
            PF_STATUS_OK,
            "directional-aerial-changed-content-view") ||
        !expect_status(
            pf_m4_make_content_view(&close, &close_view),
            PF_STATUS_OK,
            "directional-aerial-close-content-view") ||
        memcmp(
            view->content_hash.bytes,
            changed_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        !expect_status(
            pf_m4_validate_content(&invalid_extent),
            PF_STATUS_INVALID_CONFIG,
            "reject-directional-aerial-invalid-extent") ||
        !expect_status(
            pf_m4_validate_content(&invalid_timing),
            PF_STATUS_INVALID_CONFIG,
            "reject-directional-aerial-invalid-timing"))
    {
        return fail("directional-aerial-data-and-hash");
    }

    if (!run_directional_aerial_hit_case(
            &close,
            &close_view,
            &content->fighter.forward_aerial,
            0,
            INT16_C(32767),
            INT16_C(0),
            PF_M4_ACTION_FORWARD_AERIAL,
            &forward_reaction) ||
        !run_directional_aerial_hit_case(
            &close,
            &close_view,
            &content->fighter.back_aerial,
            1,
            INT16_C(32767),
            INT16_C(0),
            PF_M4_ACTION_BACK_AERIAL,
            &back_reaction) ||
        !run_directional_aerial_hit_case(
            &close,
            &close_view,
            &content->fighter.up_aerial,
            0,
            INT16_C(0),
            INT16_C(-32767),
            PF_M4_ACTION_UP_AERIAL,
            &up_reaction) ||
        !run_directional_aerial_hit_case(
            &close,
            &close_view,
            &content->fighter.down_aerial,
            0,
            INT16_C(0),
            INT16_C(32767),
            PF_M4_ACTION_DOWN_AERIAL,
            &down_reaction))
    {
        return 0;
    }

    for (attack_index = UINT32_C(0);
         attack_index < UINT32_C(4);
         ++attack_index)
    {
        /* With the captured Melee-root origin, this close down-air fixture
         * contacts the imported outer 290-degree sphere (effect 1). */
        const pf_m4_reference_hit_effect *effect =
            attack_index == UINT32_C(3)
                ? pf_m4_falcon_reference_effect(
                      moves[attack_index],
                      UINT16_C(1))
                : pf_m4_falcon_reference_primary_effect(
                      moves[attack_index]);

        if (!directional_attack_reaction_matches_effect(
                &content->fighter,
                attacks[attack_index],
                reactions[attack_index],
                effect,
                UINT8_C(0)))
        {
            return fail("directional-aerial-exact-launch");
        }
    }

    if (!run_directional_aerial_route_case(
            content,
            view,
            0,
            INT16_C(0),
            (int16_t)(
                ground_input->aerial_neutral_y_threshold - UINT16_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_AERIAL_ATTACK) ||
        !run_directional_aerial_route_case(
            content,
            view,
            0,
            INT16_C(32767),
            INT16_C(-32767),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_FORWARD_AERIAL) ||
        !run_directional_aerial_route_case(
            content,
            view,
            0,
            INT16_C(-32767),
            INT16_C(32767),
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_BACK_AERIAL) ||
        !run_directional_aerial_route_case(
            content,
            view,
            0,
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_STRONG_AERIAL_ATTACK))
    {
        return fail("directional-aerial-input-arbitration");
    }
    if (!run_directional_aerial_landing_case(
            content,
            view,
            INT16_MAX,
            INT16_C(0),
            PF_M4_ACTION_FORWARD_AERIAL,
            PF_M4_ACTION_FORWARD_AERIAL_LANDING,
            content->fighter.forward_aerial_landing_lag_ticks) ||
        !run_directional_aerial_landing_case(
            content,
            view,
            INT16_MIN,
            INT16_C(0),
            PF_M4_ACTION_BACK_AERIAL,
            PF_M4_ACTION_BACK_AERIAL_LANDING,
            content->fighter.back_aerial_landing_lag_ticks) ||
        !run_directional_aerial_landing_case(
            content,
            view,
            INT16_C(0),
            INT16_MIN,
            PF_M4_ACTION_UP_AERIAL,
            PF_M4_ACTION_UP_AERIAL_LANDING,
            content->fighter.up_aerial_landing_lag_ticks) ||
        !run_directional_aerial_landing_case(
            content,
            view,
            INT16_C(0),
            INT16_MAX,
            PF_M4_ACTION_DOWN_AERIAL,
            PF_M4_ACTION_DOWN_AERIAL_LANDING,
            content->fighter.down_aerial_landing_lag_ticks))
    {
        return fail("directional-aerial-landing-table");
    }
    return run_directional_aerial_snapshot_test(&close, &close_view);
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
    int32_t fall_expected_x;
    int32_t fall_expected_y;

    if (content->fighter.v_cancel_velocity_scale_q16 !=
            (INT32_C(95) * PF_Q16_ONE) / INT32_C(100) ||
        content->fighter.v_cancel_window_ticks != UINT16_C(3) ||
        content->fighter.tech_lockout_ticks != UINT16_C(40))
    {
        return fail("v-cancel-default-data");
    }
    if (!run_v_cancel_air_case(
            content, view, UINT32_MAX, 0, 0, &ordinary))
        return fail("v-cancel-air-ordinary-route");
    if (!run_v_cancel_air_case(
            content, view, UINT32_C(0), 0, 0, &age_zero))
        return fail("v-cancel-air-age-zero-route");
    if (!run_v_cancel_air_case(
            content, view, UINT32_C(1), 0, 0, &age_one))
        return fail("v-cancel-air-age-one-route");
    if (!run_v_cancel_fall_special_case(
            content, UINT32_C(2), &age_two))
        return fail("v-cancel-fall-special-age-two-route");
    if (!run_v_cancel_fall_special_case(
            content, UINT32_C(3), &age_three))
        return fail("v-cancel-fall-special-age-three-route");
    if (!run_v_cancel_air_case(
            content, view, UINT32_C(0), 1, 0, &attacking))
        return fail("v-cancel-air-attacking-route");
    if (!run_v_cancel_air_case(
            content, view, UINT32_C(0), 0, 1, &locked_out))
        return fail("v-cancel-air-lockout-route");
    if (!run_v_cancel_ground_case(view, 0, &grounded))
        return fail("v-cancel-ground-route");
    if (!run_v_cancel_ground_case(view, 1, &grounded_trigger))
        return fail("v-cancel-ground-trigger-route");
    if (!run_v_cancel_jump_case(view, 0, &jump_ordinary))
        return fail("v-cancel-jump-route");
    if (!run_v_cancel_jump_case(view, 1, &jump_cancelled))
        return fail("v-cancel-jump-trigger-route");
    if (!run_v_cancel_fall_special_case(
            content, UINT32_MAX, &fall_special_ordinary))
        return fail("v-cancel-fall-special-route");
    if (!run_v_cancel_fall_special_case(
            content, UINT32_C(0), &fall_special_cancelled))
        return fail("v-cancel-fall-special-trigger-route");
    expected_x = (int32_t)(
        ((int64_t)ordinary.velocity_x_q16 *
         (int64_t)content->fighter.v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    expected_y = (int32_t)(
        ((int64_t)ordinary.velocity_y_q16 *
         (int64_t)content->fighter.v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    fall_expected_x = (int32_t)(
        ((int64_t)fall_special_ordinary.velocity_x_q16 *
         (int64_t)content->fighter.v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    fall_expected_y = (int32_t)(
        ((int64_t)fall_special_ordinary.velocity_y_q16 *
         (int64_t)content->fighter.v_cancel_velocity_scale_q16) /
        (int64_t)PF_Q16_ONE);
    if (ordinary.velocity_x_q16 <= INT32_C(0) ||
        ordinary.velocity_y_q16 >= INT32_C(0) ||
        age_zero.velocity_x_q16 != expected_x ||
        age_zero.velocity_y_q16 != expected_y ||
        age_one.velocity_x_q16 != expected_x ||
        age_one.velocity_y_q16 != expected_y ||
        age_two.velocity_x_q16 != fall_expected_x ||
        age_two.velocity_y_q16 != fall_expected_y ||
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
        age_two.hitstun_ticks != fall_special_ordinary.hitstun_ticks ||
        age_zero.tumble != ordinary.tumble ||
        age_one.tumble != ordinary.tumble ||
        age_two.tumble != fall_special_ordinary.tumble)
    {
        return fail("v-cancel-preserves-hitstun-and-tumble");
    }
    if (age_three.trigger_input_age != UINT8_C(3) ||
        age_three.velocity_x_q16 !=
            fall_special_ordinary.velocity_x_q16 ||
        age_three.velocity_y_q16 !=
            fall_special_ordinary.velocity_y_q16 ||
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
    pf_m4_inspection *out_inspection)
{
    return prepare_v_cancel_fall_special_target(sim, out_inspection) &&
           step_reaction_duel(
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
               UINT16_C(0),
               out_inspection) &&
           out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
           out_inspection->players[0].action_ticks == UINT16_C(2) &&
           out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_FALL_SPECIAL &&
           out_inspection->players[1].trigger_input_age == UINT8_C(1);
}

static int run_v_cancel_snapshot_test(
    const pf_m4_content *content)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    pf_m4_content setup_content;
    pf_content_view setup_view;
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

    if (!make_v_cancel_fall_special_setup(
            content,
            &setup_content,
            &setup_view) ||
        !initialize_sim(
            &source_storage,
            &setup_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &source) ||
        !initialize_sim(
            &loaded_storage,
            &setup_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !prepare_v_cancel_snapshot(source, &source_inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-v-cancel-save-size") ||
        save_size != (size_t)915)
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
            (void)fprintf(
                stderr,
                "m4-combat=diagnostic v-cancel world_status=%u"
                " p0=%u/%u/%u/%u/%u p1=%u/%u/%u/%u/%u"
                " hitlag=%u/%u hitstun=%u/%u velocity=(%d,%d)/(%d,%d)\n",
                (unsigned int)pf_sim_snapshot_validate_world(
                    &source->world),
                (unsigned int)source->world.action_state[0],
                (unsigned int)source->world.hitlag_resume_action[0],
                (unsigned int)source->world.action_ticks[0],
                (unsigned int)source->world.grounded[0],
                (unsigned int)source->world.support[0],
                (unsigned int)source->world.action_state[1],
                (unsigned int)source->world.hitlag_resume_action[1],
                (unsigned int)source->world.action_ticks[1],
                (unsigned int)source->world.grounded[1],
                (unsigned int)source->world.support[1],
                (unsigned int)source->world.hitlag_ticks[0],
                (unsigned int)source->world.hitlag_ticks[1],
                (unsigned int)source->world.hitstun_ticks[0],
                (unsigned int)source->world.hitstun_ticks[1],
                source->world.velocity_x_q16[0],
                source->world.velocity_y_q16[0],
                source->world.velocity_x_q16[1],
                source->world.velocity_y_q16[1]);
            return fail("v-cancel-snapshot-continuation");
        }
        if (tick == UINT32_C(0) &&
            (source_inspection.players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_HITLAG ||
             source_inspection.players[1].trigger_input_age !=
                 UINT8_C(2) ||
             source_result.event_count != UINT8_C(2) ||
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
             (uint8_t)PF_M4_ACTION_CROUCH_START) ||
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
    const pf_m4_ssbm_damage_response_attributes *damage_response =
        pf_m4_ssbm_common_reference_damage_response();
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
    const uint32_t boundary_damage_q16 =
        UINT32_C(40) * UINT32_C(65536);

    if (damage_response == NULL ||
        content->fighter.crouch_cancel_max_damage_q16 !=
            PF_SIM_MAX_DAMAGE_Q16 ||
        content->fighter.crouch_cancel_velocity_scale_q16 !=
            (int32_t)damage_response->crouch_knockback_scale_q16 ||
        content->fighter.crouch_cancel_hitstun_scale_q16 !=
            (int32_t)damage_response->crouch_knockback_scale_q16)
    {
        return fail("crouch-cancel-default-data");
    }
    invalid_velocity.fighter.crouch_cancel_velocity_scale_q16 =
        PF_Q16_ONE;
    invalid_hitstun.fighter.crouch_cancel_hitstun_scale_q16 =
        INT32_C(0);
    invalid_damage.fighter.crouch_cancel_max_damage_q16 =
        UINT32_C(0);
    hash_content.fighter.crouch_cancel_velocity_scale_q16 -=
        INT32_C(1);
    exact_content.fighter.jab_damage_q16 =
        boundary_damage_q16;
    exact_content.fighter.crouch_cancel_max_damage_q16 =
        boundary_damage_q16;
    below_content.fighter.jab_damage_q16 =
        boundary_damage_q16;
    below_content.fighter.crouch_cancel_max_damage_q16 =
        boundary_damage_q16 - UINT32_C(1);
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
        crouched.hitlag_ticks !=
            pf_m4_melee_hitlag_ticks(
                crouched.damage_q16,
                UINT8_C(1),
                UINT32_C(65536)) ||
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
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic crouch-scaled flags=%u/%u"
            " velocity=(%d,%d)/(%d,%d) expected=(%d,%d)"
            " hitlag=%u/%u hitstun=%u/%u expected=%u tumble=%u\n",
            (unsigned int)ordinary.event.flags,
            (unsigned int)crouched.event.flags,
            ordinary.event.velocity_x_q16,
            ordinary.event.velocity_y_q16,
            crouched.event.velocity_x_q16,
            crouched.event.velocity_y_q16,
            expected_x,
            expected_y,
            (unsigned int)ordinary.hitlag_ticks,
            (unsigned int)crouched.hitlag_ticks,
            (unsigned int)ordinary.hitstun_ticks,
            (unsigned int)crouched.hitstun_ticks,
            expected_hitstun,
            (unsigned int)crouched.tumble);
        return fail("crouch-cancel-scaled-reaction");
    }
    /* Squat remains crouch-cancel eligible even after down is released. */
    if ((released.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) == UINT16_C(0) ||
        released.event.velocity_x_q16 != crouched.event.velocity_x_q16 ||
        released.event.velocity_y_q16 != crouched.event.velocity_y_q16 ||
        released.hitstun_ticks != crouched.hitstun_ticks)
    {
        return fail("crouch-cancel-squat-release");
    }
    if ((exact.event.flags &
        (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) == UINT16_C(0) ||
        exact.damage_q16 !=
            boundary_damage_q16 ||
        (below.event.flags &
         (uint16_t)PF_SIM_EVENT_FLAG_CROUCH_CANCEL) != UINT16_C(0) ||
        below.damage_q16 != exact.damage_q16 ||
        exact.hitlag_ticks != below.hitlag_ticks ||
        exact.event.velocity_x_q16 != below.event.velocity_x_q16 ||
        exact.event.velocity_y_q16 != below.event.velocity_y_q16 ||
        exact.hitstun_ticks != below.hitstun_ticks)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic crouch-boundary flags=%u/%u"
            " damage=%u/%u hitlag=%u/%u velocity=(%d,%d)/(%d,%d)"
            " hitstun=%u/%u boundary=%u\n",
            (unsigned int)exact.event.flags,
            (unsigned int)below.event.flags,
            exact.damage_q16,
            below.damage_q16,
            (unsigned int)exact.hitlag_ticks,
            (unsigned int)below.hitlag_ticks,
            exact.event.velocity_x_q16,
            exact.event.velocity_y_q16,
            below.event.velocity_x_q16,
            below.event.velocity_y_q16,
            (unsigned int)exact.hitstun_ticks,
            (unsigned int)below.hitstun_ticks,
            boundary_damage_q16);
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
        save_size != (size_t)915)
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
            (void)fprintf(
                stderr,
                "m4-combat=debug operation=crouch-cancel-continuation"
                " tick=%" PRIu32
                " p0=(action=%u ticks=%u grounded=%u dash=%d"
                " velocity=%" PRId32 "/%" PRId32 ")"
                " p1=(action=%u ticks=%u grounded=%u dash=%d"
                " velocity=%" PRId32 "/%" PRId32
                " hitstun=%u hitlag=%u tumble=%u)\n",
                tick,
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].action_ticks,
                (unsigned int)source_inspection.players[0].grounded,
                (int)source_inspection.players[0].dash_direction,
                source_inspection.players[0].velocity_x_q16,
                source_inspection.players[0].velocity_y_q16,
                (unsigned int)source_inspection.players[1].action_state,
                (unsigned int)source_inspection.players[1].action_ticks,
                (unsigned int)source_inspection.players[1].grounded,
                (int)source_inspection.players[1].dash_direction,
                source_inspection.players[1].velocity_x_q16,
                source_inspection.players[1].velocity_y_q16,
                (unsigned int)source_inspection.players[1].hitstun_ticks,
                (unsigned int)source_inspection.players[1].hitlag_ticks,
                (unsigned int)source_inspection.players[1].tumble);
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
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    const pf_sim_event *event;
    uint32_t tick;

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
        out_inspection->players[0].action_ticks != UINT16_C(0))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         out_inspection->players[0].action_ticks <
             content->fighter.aerial_startup_ticks;
         ++tick)
    {
        if (tick > UINT32_C(120) ||
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
    }
    if (!step_duel(
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
            out_inspection))
    {
        return 0;
    }
    while (out_inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_AERIAL_ATTACK &&
           out_inspection->players[0].action_ticks <
               content->fighter.aerial_startup_ticks)
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
            content,
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
        save_size != (size_t)915)
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
         tick + UINT32_C(1) <
             (uint32_t)content->fighter.aerial_hitlag_ticks;
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
    if (!step_duel(
            source,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &source_inspection))
    {
        return fail("double-jump-cancel-counter-source-resume-tick");
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
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "double-jump-cancel-counter-source-resume-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "double-jump-cancel-counter-loaded-resume-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("double-jump-cancel-counter-resume-future");
    }
    if (source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_DELAYED_AIR_JUMP ||
        source_inspection.players[1].action_ticks != UINT16_C(1) ||
        source_inspection.players[1].hitstun_ticks != UINT16_C(0) ||
        source_inspection.players[1].velocity_y_q16 !=
            frozen_velocity_y + content->fighter.gravity_q16)
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
            content,
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
            content,
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
            content,
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
            content->fighter.aerial_startup_ticks + UINT16_C(1) ||
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
         tick + UINT32_C(1) <
             (uint32_t)content->fighter.aerial_hitlag_ticks;
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
            &inspection))
    {
        return fail("aerial-hitlag-resume-tick");
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
        test_last_result.event_count != UINT8_C(2) ||
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
    const pf_m4_ssbm_ground_input_attributes *ground_input =
        pf_m4_ssbm_common_reference_ground_input();
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
    int32_t standing_smash_motion_x_q16;
    uint32_t expected_damage_q16;
    uint32_t tick;
    uint8_t pre_attack_action;
    uint16_t pre_attack_ticks;

    if (ground_input == NULL ||
        content->fighter.forward_smash_input_window_ticks !=
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
    expected_damage_q16 =
        content->fighter.forward_strong_attack.damage_q16;
    if (!pf_m4_falcon_reference_motion_x_q16(
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK,
            UINT16_C(1),
            &standing_smash_motion_x_q16) ||
        !step_duel(
            standing,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &standing_inspection) ||
        standing_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
        standing_inspection.players[0].action_ticks != UINT16_C(1) ||
        standing_inspection.players[0].facing != INT8_C(1) ||
        standing_inspection.players[0].position_x_q16 !=
            standing_x + standing_smash_motion_x_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic small-step-standing action=%u ticks=%u"
            " facing=%d position=%d/%d charge=%u\n",
            (unsigned int)standing_inspection.players[0].action_state,
            (unsigned int)standing_inspection.players[0].action_ticks,
            (int)standing_inspection.players[0].facing,
            standing_inspection.players[0].position_x_q16,
            standing_x,
            (unsigned int)standing_inspection.players[0].smash_charge_ticks);
        return fail("small-step-standing-forward-smash-entry");
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
        save_size != (size_t)915)
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
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE ||
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
                 (uint32_t)content->fighter.forward_strong_attack
                     .startup_ticks +
                     (uint32_t)content->fighter.forward_strong_attack
                         .active_ticks +
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
            expected_damage_q16 ||
        loaded_inspection.players[1].damage_q16 !=
            expected_damage_q16 ||
        source_inspection.players[1].last_hit_damage_q16 !=
            expected_damage_q16 ||
        source_inspection.players[0].position_x_q16 <= standing_x)
    {
        return fail("small-step-forward-smash-extended-range-hit");
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)ground_input->initial_dash_early_end_frame +
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
    pre_attack_action = negative_inspection.players[0].action_state;
    pre_attack_ticks = negative_inspection.players[0].action_ticks;
    if (!step_duel(
            negative,
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic small-step-late action=%u ticks=%u"
            " dash_direction=%d position=%d\n",
            (unsigned int)negative_inspection.players[0].action_state,
            (unsigned int)negative_inspection.players[0].action_ticks,
            (int)negative_inspection.players[0].dash_direction,
            negative_inspection.players[0].position_x_q16);
        return fail("small-step-forward-smash-window-to-dash-attack");
    }
    if (!expect_status(
            pf_sim_reset(negative, UINT64_C(0x5f5f5f47)),
            PF_STATUS_OK,
            "small-step-forward-smash-turn-aged-reset") ||
        !step_duel(
            negative,
            (int16_t)-INT16_MAX,
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection))
    {
        return fail("small-step-forward-smash-turn-aged-entry");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)ground_input->initial_dash_early_end_frame +
                    UINT32_C(1);
         ++tick)
    {
        if (!step_duel(
                negative,
                (int16_t)-INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &negative_inspection))
        {
            return fail("small-step-forward-smash-turn-aged-hold");
        }
    }
    pre_attack_action = negative_inspection.players[0].action_state;
    pre_attack_ticks = negative_inspection.players[0].action_ticks;
    if (!step_duel(
            negative,
            (int16_t)-INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_DASH_ATTACK)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic small-step-turn-aged action=%u ticks=%u"
            " facing=%d dash=%d pre=%u/%u\n",
            (unsigned int)negative_inspection.players[0].action_state,
            (unsigned int)negative_inspection.players[0].action_ticks,
            (int)negative_inspection.players[0].facing,
            (int)negative_inspection.players[0].dash_direction,
            (unsigned int)pre_attack_action,
            (unsigned int)pre_attack_ticks);
        return fail("small-step-forward-smash-turn-aged-dash-attack");
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
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        negative_inspection.players[0].facing != INT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic small-step-pivot action=%u ticks=%u"
            " facing=%d dash=%d\n",
            (unsigned int)negative_inspection.players[0].action_state,
            (unsigned int)negative_inspection.players[0].action_ticks,
            (int)negative_inspection.players[0].facing,
            (int)negative_inspection.players[0].dash_direction);
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
            (uint8_t)PF_M4_ACTION_INITIAL_DASH ||
        negative_inspection.players[0].facing != INT8_C(1))
    {
        return fail("small-step-forward-smash-late-pivot-jab");
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
            (uint8_t)PF_M4_ACTION_INITIAL_DASH)
    {
        return fail("small-step-forward-smash-aged-neutral-dash-attack");
    }
    if (!expect_status(
            pf_sim_reset(negative, UINT64_C(0x5f5f5f46)),
            PF_STATUS_OK,
            "small-step-backward-tilt-jab-reset") ||
        !step_duel(
            negative,
            (int16_t)-(content->fighter.axis_dead_zone + UINT16_C(1)),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &negative_inspection) ||
        negative_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        negative_inspection.players[0].facing != INT8_C(1))
    {
        return fail("small-step-backward-tilt-falls-through-to-jab");
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
        save_size != (size_t)915)
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
        save_size != (size_t)915)
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
            test_last_result.event_count == UINT8_C(2) &&
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
        save_size != (size_t)915)
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
        if (find_last_tick_event(PF_SIM_EVENT_SHIELD_BLOCK) != NULL)
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
        if (find_last_tick_event(PF_SIM_EVENT_SHIELD_BLOCK) != NULL)
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
        if (find_last_tick_event(PF_SIM_EVENT_SHIELD_BLOCK) != NULL)
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
            save_size != (size_t)915 ||
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

static int start_held_shield_block(
    pf_sim *sim,
    uint16_t shield_strength,
    pf_m4_inspection *out_inspection);

static int perform_stale_move_attack(
    pf_sim *sim,
    pf_m4_inspection *out_inspection,
    uint64_t buttons,
    pf_m4_action_state expected_action,
    pf_sim_event *out_event)
{
    uint32_t tick;

    if (!wait_for_kill_confirm_neutral(sim, out_inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            buttons,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
    {
        const pf_sim_event *event =
            find_last_tick_event(PF_SIM_EVENT_HIT);

        if (event != NULL &&
            event->source_player == UINT8_C(0) &&
            event->target_player == UINT8_C(1))
        {
            if (event->detail != (uint16_t)expected_action)
            {
                return fail("stale-move-attack-identity");
            }
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
    return fail("stale-move-attack-timeout");
}

static int run_stale_move_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    static const uint16_t expected_reductions[
        PF_SIM_STALE_MOVE_QUEUE_CAPACITY] = {
        UINT16_C(5898),
        UINT16_C(5243),
        UINT16_C(4588),
        UINT16_C(3932),
        UINT16_C(3277),
        UINT16_C(2621),
        UINT16_C(1966),
        UINT16_C(1311),
        UINT16_C(655)};
    test_sim_storage storage;
    test_sim_storage shield_storage;
    test_sim_storage miss_storage;
    pf_m4_content miss_content;
    pf_content_view miss_view;
    pf_sim *sim = NULL;
    pf_sim *shield_sim = NULL;
    pf_sim *miss_sim = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection shield_inspection;
    pf_m4_inspection miss_inspection;
    pf_sim_observation observation;
    pf_sim_event event;
    uint32_t expected_total_q16 = UINT32_C(0);
    uint32_t hit;
    uint32_t slot;

    if (memcmp(
            content->fighter.stale_move_slot_reduction_q16,
            expected_reductions,
            sizeof(expected_reductions)) != 0 ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "stale-move-initial-inspection") ||
        inspection.players[0].stale_move_count != UINT8_C(0) ||
        inspection.players[0].stale_move_multiplier_q16 !=
            (uint32_t)PF_Q16_ONE ||
        inspection.players[0].attack_stale_registered != UINT8_C(0))
    {
        return fail("stale-move-defaults-and-reset");
    }

    for (hit = UINT32_C(0); hit < UINT32_C(10); ++hit)
    {
        const uint32_t prior_count =
            hit < (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY
                ? hit
                : (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
        const uint32_t resulting_count =
            hit + UINT32_C(1) <
                    (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY
                ? hit + UINT32_C(1)
                : (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
        const uint16_t prior_mask =
            (uint16_t)(
                prior_count == UINT32_C(0)
                    ? UINT32_C(0)
                    : (UINT32_C(1) << prior_count) -
                          UINT32_C(1));
        const uint16_t resulting_mask =
            (uint16_t)(
                (UINT16_C(1) << resulting_count) -
                UINT16_C(1));
        const uint32_t expected_hit_damage_q16 =
            expected_stale_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                prior_mask);

        expected_total_q16 += expected_hit_damage_q16;
        if (!perform_stale_move_attack(
                sim,
                &inspection,
                PF_INPUT_BUTTON_ATTACK,
                PF_M4_ACTION_GROUND_ATTACK,
                &event) ||
            event.value_q16 != expected_hit_damage_q16 ||
            inspection.players[1].damage_q16 != expected_total_q16 ||
            inspection.players[0].stale_move_count !=
                (uint8_t)resulting_count ||
            inspection.players[0].stale_move_multiplier_q16 !=
                expected_stale_damage_q16(
                    &content->fighter,
                    (uint32_t)PF_Q16_ONE,
                    resulting_mask) ||
            inspection.players[0].attack_stale_registered !=
                UINT8_C(1) ||
            inspection.players[1].stale_move_count != UINT8_C(0))
        {
            return fail("stale-move-repeated-damage-and-registration");
        }
        for (slot = UINT32_C(0); slot < resulting_count; ++slot)
        {
            if (inspection.players[0].stale_move_ids[slot] !=
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
            {
                return fail("stale-move-newest-first-queue");
            }
        }
        if (!expect_status(
                pf_sim_observe(sim, &observation),
                PF_STATUS_OK,
                "stale-move-observe") ||
            observation.schema_version !=
                PF_SIM_OBSERVATION_SCHEMA_VERSION ||
            observation.players[0].stale_move_count !=
                inspection.players[0].stale_move_count ||
            observation.players[0].stale_move_multiplier_q16 !=
                inspection.players[0].stale_move_multiplier_q16 ||
            memcmp(
                observation.players[0].stale_move_ids,
                inspection.players[0].stale_move_ids,
                sizeof(observation.players[0].stale_move_ids)) != 0)
        {
            return fail("stale-move-structured-observation");
        }
    }

    if (!perform_stale_move_attack(
            sim,
            &inspection,
            PF_INPUT_BUTTON_STRONG_ATTACK,
            PF_M4_ACTION_STRONG_ATTACK,
            &event) ||
        event.value_q16 != content->fighter.strong_damage_q16 ||
        inspection.players[0].stale_move_count !=
            PF_SIM_STALE_MOVE_QUEUE_CAPACITY ||
        inspection.players[0].stale_move_ids[0] !=
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK)
    {
        return fail("stale-move-fresh-different-move");
    }
    for (slot = UINT32_C(1);
         slot < (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++slot)
    {
        if (inspection.players[0].stale_move_ids[slot] !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
        {
            return fail("stale-move-capacity-evicts-oldest");
        }
    }

    if (!initialize_sim(
            &shield_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &shield_sim) ||
        !expect_status(
            pf_m4_inspect(shield_sim, &shield_inspection),
            PF_STATUS_OK,
            "stale-move-shield-inspection") ||
        !perform_stale_move_attack(
            shield_sim,
            &shield_inspection,
            PF_INPUT_BUTTON_ATTACK,
            PF_M4_ACTION_GROUND_ATTACK,
            &event) ||
        !wait_for_kill_confirm_neutral(
            shield_sim,
            &shield_inspection) ||
        !start_held_shield_block(
            shield_sim,
            UINT16_MAX,
            &shield_inspection))
    {
        return fail("stale-move-shield-setup");
    }
    {
        const uint16_t ground_matching_slots = UINT16_C(1);
        const uint32_t stale_jab_damage_q16 =
            expected_stale_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                ground_matching_slots);
        const uint32_t expected_shield_damage_value_q16 =
            expected_shield_damage_q16(
                &content->fighter,
                stale_jab_damage_q16,
                UINT16_MAX);

        if (test_last_result.event_count != UINT8_C(2) ||
            test_last_result.events[0].type !=
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
            test_last_result.events[0].value_q16 !=
                expected_shield_damage_value_q16 ||
            test_last_result.events[1].type !=
                (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
            test_last_result.events[1].detail != UINT16_C(3) ||
            shield_inspection.players[0].stale_move_count !=
                UINT8_C(1) ||
            shield_inspection.players[0].stale_move_ids[0] !=
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
            shield_inspection.players[0].attack_stale_registered !=
                UINT8_C(0))
        {
            return fail("stale-move-shield-scales-without-registering");
        }
    }

    miss_content = *content;
    miss_content.stage.spawn_spacing_q16 =
        INT32_C(8) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(&miss_content, &miss_view),
            PF_STATUS_OK,
            "stale-move-miss-content") ||
        !initialize_sim(
            &miss_storage,
            &miss_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &miss_sim) ||
        !expect_status(
            pf_m4_inspect(miss_sim, &miss_inspection),
            PF_STATUS_OK,
            "stale-move-miss-inspection") ||
        !step_duel(
            miss_sim,
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            &miss_inspection))
    {
        return fail("stale-move-miss-setup");
    }
    for (hit = UINT32_C(0); hit < UINT32_C(40); ++hit)
    {
        if (miss_inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_duel(
                miss_sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &miss_inspection))
        {
            return 0;
        }
    }
    if (hit == UINT32_C(40) ||
        miss_inspection.players[1].damage_q16 != UINT32_C(0) ||
        miss_inspection.players[0].stale_move_count != UINT8_C(0) ||
        miss_inspection.players[0].attack_stale_registered !=
            UINT8_C(0))
    {
        return fail("stale-move-whiff-does-not-register");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x57a1e5eed)),
            PF_STATUS_OK,
            "stale-move-reset") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "stale-move-reset-inspection") ||
        inspection.players[0].stale_move_count != UINT8_C(0) ||
        inspection.players[0].stale_move_multiplier_q16 !=
            (uint32_t)PF_Q16_ONE ||
        inspection.players[0].attack_stale_registered != UINT8_C(0))
    {
        return fail("stale-move-new-match-clears-queue");
    }
    for (slot = UINT32_C(0);
         slot < (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++slot)
    {
        if (inspection.players[0].stale_move_ids[slot] != UINT8_C(0))
        {
            return fail("stale-move-reset-clears-identities");
        }
    }
    return 1;
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                jab_index + UINT32_C(1));

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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                buildup_jabs + UINT32_C(1)))
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
            save_size != (size_t)915 ||
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
                expected_repeated_move_damage_q16(
                    &content->fighter,
                    content->fighter.jab_damage_q16,
                    buildup_jabs + UINT32_C(1)) &&
            inspection.players[1].respawn_count == UINT16_C(0))
        {
            return 1;
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            if (expect_ko == 0 || finisher_hit == 0 ||
                source_result.event_count != UINT8_C(2) ||
                source_result.events[0].type !=
                    (uint16_t)PF_SIM_EVENT_KO ||
                source_result.events[0].source_player != UINT8_C(0) ||
                source_result.events[0].target_player != UINT8_C(1) ||
                source_result.events[1].type !=
                    (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
                source_result.events[1].detail != UINT16_C(2) ||
                source_result.events[0].value_q16 !=
                    expected_repeated_move_damage_q16(
                        &content->fighter,
                        content->fighter.jab_damage_q16,
                        buildup_jabs + UINT32_C(1)) +
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
    (void)fprintf(
        stderr,
        "m4-combat=debug kill-confirm expect_ko=%d buildup=%" PRIu32
        " finisher=%d escaped=%d action=%u grounded=%u damage=%" PRIu32
        " x=%" PRId32 " y=%" PRId32 " vx=%" PRId32 " vy=%" PRId32
        " respawns=%u\n",
        expect_ko,
        buildup_jabs,
        finisher_hit,
        defender_escaped,
        (unsigned int)inspection.players[1].action_state,
        (unsigned int)inspection.players[1].grounded,
        inspection.players[1].damage_q16,
        inspection.players[1].position_x_q16,
        inspection.players[1].position_y_q16,
        inspection.players[1].velocity_x_q16,
        inspection.players[1].velocity_y_q16,
        (unsigned int)inspection.players[1].respawn_count);
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
                save_size != (size_t)915 ||
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
                        expected_repeated_move_damage_q16(
                            &content->fighter,
                            content->fighter.jab_damage_q16,
                            hit_count)) ||
                   fail("zero-to-death-di-route-still-connected");
        }
        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            if (expect_ko == 0 || strong_started == 0 ||
                hit_count != UINT32_C(22) || saved == 0 ||
                inspection.players[1].damage_q16 != UINT32_C(0) ||
                source_result.event_count != UINT8_C(2) ||
                source_result.events[0].type !=
                    (uint16_t)PF_SIM_EVENT_KO ||
                source_result.events[0].source_player != UINT8_C(0) ||
                source_result.events[0].target_player != UINT8_C(1) ||
                source_result.events[1].type !=
                    (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
                source_result.events[1].detail != UINT16_C(2) ||
                source_result.events[0].value_q16 !=
                    expected_repeated_move_damage_q16(
                        &content->fighter,
                        content->fighter.jab_damage_q16,
                        UINT32_C(21)) +
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
                save_size != (size_t)915 ||
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
                        expected_repeated_move_damage_q16(
                            &content->fighter,
                            content->fighter.aerial_damage_q16,
                            hit_count)) ||
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
                source_result.event_count != UINT8_C(2) ||
                source_result.events[0].type !=
                    (uint16_t)PF_SIM_EVENT_KO ||
                source_result.events[0].source_player != UINT8_C(0) ||
                source_result.events[0].target_player != UINT8_C(1) ||
                source_result.events[0].value_q16 !=
                    expected_repeated_move_damage_q16(
                        &content->fighter,
                        content->fighter.aerial_damage_q16,
                        UINT32_C(3)) +
                        content->fighter.strong_damage_q16)
            {
                (void)fprintf(
                    stderr,
                    "m4-combat=debug ladder expect_ko=%d strong=%d "
                    "hits=%" PRIu32 " double_jump=%d saved=%d above=%d "
                    "vertical=%d damage=%" PRIu32 " events=%u type=%u "
                    "source=%u target=%u value=%" PRIu32
                    " expected_value=%" PRIu32 "\n",
                    expect_ko,
                    strong_started,
                    hit_count,
                    double_jump_used,
                    saved,
                    saw_above_platform,
                    saw_vertical_carry,
                    inspection.players[1].damage_q16,
                    (unsigned int)source_result.event_count,
                    source_result.event_count != UINT8_C(0)
                        ? (unsigned int)source_result.events[0].type
                        : 0U,
                    source_result.event_count != UINT8_C(0)
                        ? (unsigned int)source_result.events[0].source_player
                        : 0U,
                    source_result.event_count != UINT8_C(0)
                        ? (unsigned int)source_result.events[0].target_player
                        : 0U,
                    source_result.event_count != UINT8_C(0)
                        ? source_result.events[0].value_q16
                        : UINT32_C(0),
                    expected_repeated_move_damage_q16(
                        &content->fighter,
                        content->fighter.aerial_damage_q16,
                        UINT32_C(3)) +
                        content->fighter.strong_damage_q16);
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
    if (ceiling_fixture != 0)
    {
        out_content->stage.solid_left_q16 = INT32_C(0);
        out_content->stage.solid_right_q16 = INT32_C(20) * PF_Q16_ONE;
        out_content->stage.floor_y_q16 = INT32_C(55) * PF_Q16_ONE;
        out_content->stage.blast_bottom_q16 = INT32_C(63) * PF_Q16_ONE;
        /* The live route relocates an already-launched fighter through
         * Hyrule's cave. Give this compact fixture enough launch height to
         * reach its equivalent ceiling while retaining room for the
         * post-reflection trace. */
        out_content->fighter.strong_base_knockback_y_q16 =
            INT32_C(2) * PF_Q16_ONE;
    }
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
    (void)fprintf(
        stderr,
        "m4-surface-route=detail expected_action=%u action=%u tick=%u"
        " hitstun=%u tumble=%u tech_window=%u tech_lockout=%u"
        " x=%" PRId32 " y=%" PRId32 "\n",
        (unsigned int)expected_action,
        (unsigned int)inspection.players[1].action_state,
        (unsigned int)inspection.players[1].action_ticks,
        (unsigned int)inspection.players[1].hitstun_ticks,
        (unsigned int)inspection.players[1].tumble,
        (unsigned int)inspection.players[1].tech_window_ticks,
        (unsigned int)inspection.players[1].tech_lockout_ticks,
        inspection.players[1].position_x_q16,
        inspection.players[1].position_y_q16);
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
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint8_t)PF_M4_ACTION_CEILING_TECH,
            &inspection) ||
        inspection.players[1].tumble != UINT8_C(0) ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        inspection.players[1].velocity_y_q16 != INT32_C(0) ||
        inspection.players[1].velocity_x_q16 != INT32_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(1))
    {
        return fail("ceiling-tech-entry");
    }
    for (tick = UINT32_C(1);
         tick <=
             (uint32_t)ceiling_content->fighter.ceiling_tech_control_tick;
         ++tick)
    {
        if (!step_reaction_duel(
                ceiling,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("ceiling-tech-control-step");
        }
    }
    if (inspection.players[1].velocity_x_q16 !=
            ceiling_content->fighter.ceiling_tech_speed_q16 -
                ceiling_content->fighter.air_friction_q16 ||
        inspection.players[1].hitstun_ticks == UINT16_C(0) ||
        inspection.players[1].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-ceiling-tech-control actual_vx=%" PRId32
            " expected_vx=%" PRId32 " hitstun=%u invulnerable=%u"
            " action=%u tick=%u\n",
            inspection.players[1].velocity_x_q16,
            ceiling_content->fighter.ceiling_tech_speed_q16 -
                ceiling_content->fighter.air_friction_q16,
            (unsigned int)inspection.players[1].hitstun_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks);
        return fail("ceiling-tech-control-frame");
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
    uint32_t tick;

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
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_STANDING_TURN)
    {
        return fail("facing-away-turn-entry");
    }
    for (tick = UINT32_C(1);
         tick <
             (uint32_t)content->fighter.standing_turn_facing_tick;
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
            return fail("facing-away-turn-timing");
        }
    }
    if (inspection.players[0].facing != INT8_C(-1) ||
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

    if (inspection.players[0].damage_q16 != UINT32_C(0) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_REBOUND_STOP ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_REBOUND_STOP ||
        inspection.players[0].last_hit_sequence != UINT32_C(0) ||
        inspection.players[1].last_hit_sequence != UINT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic simultaneous damage=%u/%u action=%u/%u"
            " attacker=%u/%u sequence=%u/%u\n",
            inspection.players[0].damage_q16,
            inspection.players[1].damage_q16,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[0].last_hit_attacker,
            (unsigned int)inspection.players[1].last_hit_attacker,
            inspection.players[0].last_hit_sequence,
            inspection.players[1].last_hit_sequence);
        return fail("simultaneous-clank");
    }
    return 1;
}

static int start_held_shield_block(
    pf_sim *sim,
    uint16_t shield_strength,
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
                shield_strength,
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
               shield_strength,
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
               shield_strength,
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
               shield_strength,
               out_inspection);
}

static int start_normal_shield_block(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return start_held_shield_block(
        sim,
        UINT16_MAX,
        out_inspection);
}

static int step_player1_secondary_shield(
    pf_sim *sim,
    int16_t secondary_stick_x,
    int16_t secondary_stick_y,
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_m4_inspection before;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "secondary-shield-inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[1].secondary_stick_x = secondary_stick_x;
    inputs[1].secondary_stick_y = secondary_stick_y;
    inputs[1].buttons = PF_INPUT_BUTTON_STRONG_ATTACK;
    inputs[1].left_trigger = UINT16_MAX;
    return expect_status(
               pf_sim_tick(
                   sim,
                   inputs,
                   (size_t)2,
                   &test_last_result),
               PF_STATUS_OK,
               "secondary-shield-step") &&
           expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "secondary-shield-inspect-after-step");
}

static int start_window_shield_block(
    pf_sim *sim,
    uint16_t shield_strength,
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
               shield_strength,
               out_inspection);
}

static int start_powershield_block(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return start_window_shield_block(
        sim,
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
    test_sim_storage up_cancel_storage;
    test_sim_storage down_cancel_storage;
    test_sim_storage normal_storage;
    pf_sim *early = NULL;
    pf_sim *cancel = NULL;
    pf_sim *strong_cancel = NULL;
    pf_sim *up_cancel = NULL;
    pf_sim *down_cancel = NULL;
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
        !initialize_sim(
            &up_cancel_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &up_cancel) ||
        !initialize_sim(
            &down_cancel_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &down_cancel) ||
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
            up_cancel,
            1,
            &inspection) ||
        !step_reaction_duel(
            up_cancel,
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
            up_cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(-32767),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_UP_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-frame-two-up-attack");
    }

    if (!advance_block_to_release(
            content,
            down_cancel,
            1,
            &inspection) ||
        !step_reaction_duel(
            down_cancel,
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
            down_cancel,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(32767),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_DOWN_ATTACK ||
        inspection.players[1].action_ticks != UINT16_C(0) ||
        inspection.players[1].powershield != UINT8_C(0))
    {
        return fail("powershield-cancel-frame-two-down-attack");
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
            if (inspection.players[0].velocity_y_q16 >= INT32_C(0) &&
                inspection.players[0].fast_fall == UINT8_C(0))
            {
                tick_inputs[0].main_stick_y = INT16_MAX;
            }
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
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
         ++tick)
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
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].velocity_x_q16 >= run_velocity ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16)
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

static uint32_t expected_shield_depletion_q16(
    const pf_m4_fighter_data *fighter,
    uint16_t shield_strength)
{
    return fighter->light_shield_hold_depletion_q16 +
           (uint32_t)(
               ((uint64_t)(
                    fighter->shield_hold_depletion_q16 -
                    fighter->light_shield_hold_depletion_q16) *
                (uint64_t)shield_strength) /
               (uint64_t)UINT16_MAX);
}

typedef struct test_shield_box
{
    int32_t left_q16;
    int32_t right_q16;
    int32_t top_q16;
    int32_t bottom_q16;
} test_shield_box;

static test_shield_box observed_shield_box(
    const pf_m4_player_inspection *player)
{
    const test_shield_box box = {
        player->shield_left_q16,
        player->shield_right_q16,
        player->shield_top_q16,
        player->shield_bottom_q16};

    return box;
}

static int run_light_shield_state_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage source_storage;
    test_sim_storage loaded_storage;
    test_sim_storage midpoint_storage;
    test_sim_storage dense_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *midpoint = NULL;
    pf_sim *dense = NULL;
    pf_m4_inspection inspection;
    pf_m4_inspection loaded_inspection;
    pf_sim_observation observation;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    test_shield_box expected_box;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    const uint16_t light_input =
        content->fighter.light_shield_trigger_threshold;
    const uint16_t light = UINT16_C(1);
    const uint16_t dense_threshold =
        content->fighter.digital_trigger_threshold;
    const uint16_t middle_input =
        (uint16_t)(
            (uint32_t)light_input +
            ((uint32_t)dense_threshold - (uint32_t)light_input) /
                UINT32_C(2));
    const uint16_t middle =
        (uint16_t)(
            ((uint32_t)middle_input -
             ((uint32_t)light_input - UINT32_C(1))) *
            (uint32_t)UINT16_MAX /
            ((uint32_t)UINT16_MAX -
             ((uint32_t)light_input - UINT32_C(1))));
    const uint32_t middle_depletion =
        expected_shield_depletion_q16(&content->fighter, middle);
    const int16_t tilt_x =
        (int16_t)(content->fighter.axis_dead_zone + UINT16_C(1));
    const int16_t tilt_y = (int16_t)-tilt_x;
    int32_t light_width_q16;

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
            &midpoint_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &midpoint) ||
        !initialize_sim(
            &dense_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &dense))
    {
        return fail("light-shield-init");
    }

    if (!step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            (uint16_t)(light_input - UINT16_C(1)),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].shield_strength != UINT16_C(0) ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 ||
        !expect_status(
            pf_sim_reset(source, UINT64_C(0x4d34434f4d424154)),
            PF_STATUS_OK,
            "light-shield-reset"))
    {
        return fail("light-shield-threshold-negative");
    }

    if (!step_reaction_duel(
            source,
            tilt_x,
            tilt_y,
            UINT64_C(0),
            light_input,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return fail("light-shield-minimum-step");
    }
    expected_box = observed_shield_box(&inspection.players[0]);
    light_width_q16 = expected_box.right_q16 - expected_box.left_q16;
    if (
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[0].shield_strength != light ||
        inspection.players[0].shield_tilt_x <= INT16_C(0) ||
        inspection.players[0].shield_tilt_y >= INT16_C(0) ||
        inspection.players[0].shield_active != UINT8_C(1) ||
        inspection.players[0].shield_left_q16 != expected_box.left_q16 ||
        inspection.players[0].shield_right_q16 != expected_box.right_q16 ||
        inspection.players[0].shield_top_q16 != expected_box.top_q16 ||
        inspection.players[0].shield_bottom_q16 != expected_box.bottom_q16 ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 ||
        !expect_status(
            pf_sim_observe(source, &observation),
            PF_STATUS_OK,
            "light-shield-observe") ||
        observation.players[0].shield_strength != light ||
        observation.players[0].shield_tilt_x !=
            inspection.players[0].shield_tilt_x ||
        observation.players[0].shield_tilt_y !=
            inspection.players[0].shield_tilt_y ||
        observation.players[0].shield_health_q16 !=
            inspection.players[0].shield_health_q16 ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "light-shield-query-save") ||
        save_size != (size_t)915)
    {
        return fail("light-shield-minimum-depletion");
    }

    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "light-shield-save") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "light-shield-load") ||
        !expect_status(
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "light-shield-loaded-inspect") ||
        loaded_inspection.players[0].shield_strength != light ||
        loaded_inspection.players[0].shield_tilt_x !=
            inspection.players[0].shield_tilt_x ||
        loaded_inspection.players[0].shield_tilt_y !=
            inspection.players[0].shield_tilt_y ||
        loaded_inspection.players[0].shield_active != UINT8_C(1) ||
        loaded_inspection.players[0].shield_left_q16 !=
            expected_box.left_q16 ||
        loaded_inspection.players[0].shield_right_q16 !=
            expected_box.right_q16 ||
        loaded_inspection.players[0].shield_top_q16 !=
            expected_box.top_q16 ||
        loaded_inspection.players[0].shield_bottom_q16 !=
            expected_box.bottom_q16 ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "light-shield-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "light-shield-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash) ||
        !step_reaction_duel(
            source,
            tilt_x,
            tilt_y,
            UINT64_C(0),
            light_input,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        !step_reaction_duel(
            loaded,
            tilt_x,
            tilt_y,
            UINT64_C(0),
            light_input,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &loaded_inspection) ||
        !expect_status(
            pf_sim_hash(source, &source_hash),
            PF_STATUS_OK,
            "light-shield-future-source-hash") ||
        !expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "light-shield-future-loaded-hash") ||
        !hash_equal(&source_hash, &loaded_hash))
    {
        return fail("light-shield-snapshot-continuation");
    }

    if (!step_reaction_duel(
            midpoint,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            middle_input,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].shield_strength != middle ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16 ||
        middle_depletion <=
            content->fighter.light_shield_hold_depletion_q16 ||
        middle_depletion >=
            content->fighter.shield_hold_depletion_q16 ||
        !step_reaction_duel(
            dense,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            dense_threshold,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].shield_strength != dense_threshold ||
        inspection.players[0].shield_active != UINT8_C(1) ||
        inspection.players[0].shield_right_q16 -
                inspection.players[0].shield_left_q16 >=
            light_width_q16 ||
        inspection.players[0].shield_health_q16 !=
            content->fighter.shield_health_q16)
    {
        return fail("light-shield-interpolation");
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
         tick <
             (uint32_t)content->fighter.dash_run_transition_ticks;
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
        tap_inspection.players[0].action_ticks != UINT16_C(0) ||
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
        save_size != (size_t)915)
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

    for (tick = UINT32_C(0);
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
    if (tick == UINT32_C(8))
        return fail("spacing-counter-jab-never-active");
    if (out_inspection->players[0].damage_q16 != UINT32_C(0))
        return fail("spacing-counter-jab-did-not-whiff");
    if (out_inspection->players[1].action_state !=
        (uint8_t)PF_M4_ACTION_GROUND_ATTACK)
        return fail("spacing-counter-jab-state");
    if (!step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_STRONG_ATTACK,
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
        return 0;
    return (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_STRONG_ATTACK &&
            out_inspection->players[1].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_ATTACK &&
            out_inspection->players[1].hitbox_active != UINT8_C(0)) ||
           fail("spacing-counter-strong-entry");
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
        save_size != (size_t)915)
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
            test_last_result.event_count == UINT8_C(2) &&
            test_last_result.events[0].type ==
                (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK &&
            test_last_result.events[1].type ==
                (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS) ||
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
    if (!walk_to_approach_distance(sim, content, 0, &inspection))
        return fail("approach-safe-walk");
    if (inspection.players[0].position_x_q16 <= start_x ||
        inspection.players[0].facing != INT8_C(1))
        return fail("approach-safe-position");
    if (!start_spacing_whiff_counter(sim, &inspection))
        return fail("approach-safe-counter-entry");
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
    test_sim_storage spot_storage;
    test_sim_storage jump_storage;
    pf_sim *normal = NULL;
    pf_sim *power = NULL;
    pf_sim *spot = NULL;
    pf_sim *jump = NULL;
    pf_m4_inspection normal_inspection;
    pf_m4_inspection power_inspection;
    pf_m4_inspection spot_inspection;
    pf_m4_inspection jump_inspection;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    const uint32_t shield_damage =
        expected_shield_damage_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            UINT16_MAX);
    const uint32_t normal_expected_health =
        content->fighter.shield_health_q16 -
        UINT32_C(7) *
            content->fighter.shield_hold_depletion_q16 -
        shield_damage;
    const uint32_t power_expected_health =
        content->fighter.shield_health_q16;
    int32_t normal_pushback;
    int32_t normal_initial_recoil;
    int32_t normal_expected_resumed_recoil;
    int32_t normal_attacker_start_x;
    uint16_t normal_shield_stun;
    int8_t normal_facing;
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
        !initialize_sim(
            &spot_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &spot) ||
        !initialize_sim(
            &jump_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &jump) ||
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
        normal_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        normal_inspection.players[0].shield_recoil_x_q16 >= INT32_C(0) ||
        normal_inspection.players[1].velocity_x_q16 <= INT32_C(0) ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 != shield_damage ||
        test_last_result.events[0].detail !=
            (uint16_t)PF_M4_ACTION_GROUND_ATTACK ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS)
    {
        return fail("normal-shield-damage-stun-pushback");
    }
    normal_pushback =
        normal_inspection.players[1].velocity_x_q16;
    normal_initial_recoil =
        normal_inspection.players[0].shield_recoil_x_q16;
    normal_expected_resumed_recoil =
        normal_initial_recoil +
        (int32_t)(((int64_t)content->fighter.traction_q16 *
                   (int64_t)content->fighter
                       .shield_attacker_pushback_ground_friction_scale_q16) >>
                  16U);
    if (normal_expected_resumed_recoil > INT32_C(0))
    {
        normal_expected_resumed_recoil = INT32_C(0);
    }
    normal_attacker_start_x =
        normal_inspection.players[0].position_x_q16;
    normal_shield_stun =
        normal_inspection.players[1].shield_stun_ticks;
    normal_facing = normal_inspection.players[1].facing;

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_player1_secondary_shield(
                normal,
                normal_facing == INT8_C(1)
                    ? INT16_MAX
                    : INT16_MIN,
                INT16_C(0),
                &normal_inspection))
        {
            return fail("shield-hitlag-step");
        }
    }
    if (normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        normal_inspection.players[1].hitlag_ticks != UINT16_C(0) ||
        normal_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        normal_inspection.players[0].shield_recoil_x_q16 !=
            normal_expected_resumed_recoil ||
        normal_inspection.players[0].position_x_q16 !=
            normal_attacker_start_x + normal_expected_resumed_recoil)
    {
        return fail("shield-hitlag-resume-separate-attacker-recoil");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_player1_secondary_shield(
                normal,
                normal_facing == INT8_C(1)
                    ? INT16_MAX
                    : INT16_MIN,
                INT16_C(0),
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
    if (!step_player1_secondary_shield(
            normal,
            normal_facing == INT8_C(1)
                ? INT16_MAX
                : INT16_MIN,
            INT16_C(0),
            &normal_inspection) ||
        normal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        normal_inspection.players[1].facing !=
            normal_facing)
    {
        return fail("c-stick-roll-buffer-through-shield-stun");
    }

    if (!start_normal_shield_block(spot, &spot_inspection) ||
        !start_normal_shield_block(jump, &jump_inspection))
    {
        return fail("c-stick-vertical-buffer-shield-block-setup");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_player1_secondary_shield(
                spot,
                INT16_C(0),
                INT16_MAX,
                &spot_inspection) ||
            !step_player1_secondary_shield(
                jump,
                INT16_C(0),
                INT16_MIN,
                &jump_inspection))
        {
            return fail("c-stick-vertical-buffer-hitlag-step");
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_player1_secondary_shield(
                spot,
                INT16_C(0),
                INT16_MAX,
                &spot_inspection) ||
            !step_player1_secondary_shield(
                jump,
                INT16_C(0),
                INT16_MIN,
                &jump_inspection))
        {
            return fail("c-stick-vertical-buffer-shield-stun-step");
        }
        if (spot_inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD &&
            jump_inspection.players[1].action_state ==
                (uint8_t)PF_M4_ACTION_SHIELD)
        {
            break;
        }
    }
    if (!step_player1_secondary_shield(
            spot,
            INT16_C(0),
            INT16_MAX,
            &spot_inspection) ||
        spot_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SPOT_DODGE ||
        !step_player1_secondary_shield(
            jump,
            INT16_C(0),
            INT16_MIN,
            &jump_inspection) ||
        jump_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        return fail("c-stick-vertical-buffer-through-shield-stun");
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
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_POWERSHIELD ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[0].value_q16 != UINT32_C(0) ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS)
    {
        return fail("powershield-window-and-zero-damage");
    }
    if (!step_reaction_duel(
            power,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            content->fighter.light_shield_trigger_threshold,
            &power_inspection) ||
        power_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        power_inspection.players[1].powershield != UINT8_C(1) ||
        power_inspection.players[1].shield_strength !=
            UINT16_MAX)
    {
        return fail("powershield-strength-frozen-during-hitlag");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(power, &destination),
            PF_STATUS_OK,
            "powershield-strength-save"))
    {
        return 0;
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(normal, save),
            PF_STATUS_OK,
            "powershield-strength-load") ||
        !expect_status(
            pf_m4_inspect(normal, &normal_inspection),
            PF_STATUS_OK,
            "powershield-strength-loaded-inspect") ||
        normal_inspection.players[1].powershield != UINT8_C(1) ||
        normal_inspection.players[1].shield_strength !=
            UINT16_MAX)
    {
        return fail("powershield-strength-snapshot");
    }
    for (tick = UINT32_C(1);
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

static int run_shield_sdi_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    const pf_m4_ssbm_damage_response_attributes *source_damage_response =
        pf_m4_ssbm_common_reference_damage_response();
    test_sim_storage horizontal_storage;
    test_sim_storage vertical_storage;
    test_sim_storage reentry_storage;
    test_sim_storage edge_storage;
    pf_sim *horizontal = NULL;
    pf_sim *vertical = NULL;
    pf_sim *reentry = NULL;
    pf_sim *edge = NULL;
    pf_m4_inspection horizontal_inspection;
    pf_m4_inspection vertical_inspection;
    pf_m4_inspection reentry_inspection;
    pf_m4_inspection edge_inspection;
    pf_m4_content changed = *content;
    pf_m4_content edge_content = *content;
    pf_content_view changed_view;
    pf_content_view edge_view;
    pf_content_view invalid_view;
    const int32_t shield_sdi_distance_q16 =
        (int32_t)(
            ((int64_t)content->fighter.sdi_distance_x_q16 *
             (int64_t)content->fighter.shield_sdi_scale_q16) /
            (int64_t)PF_Q16_ONE);
    const int32_t shield_asdi_distance_q16 =
        (int32_t)(
            ((int64_t)content->fighter.asdi_distance_x_q16 *
             (int64_t)content->fighter.shield_sdi_scale_q16) /
            (int64_t)PF_Q16_ONE);
    int32_t horizontal_start_x;
    int32_t horizontal_start_y;
    int32_t first_pulse_x;
    int32_t vertical_start_x;
    int32_t vertical_start_y;
    int32_t reentry_start_x;
    int32_t reentry_start_y;
    uint32_t tick;

    if (source_damage_response == NULL ||
        content->fighter.shield_sdi_scale_q16 !=
            source_damage_response->shield_sdi_scale_q16 ||
        content->fighter.sdi_distance_x_q16 !=
            source_damage_response->sdi_distance_x_q16 ||
        content->fighter.asdi_distance_x_q16 !=
            source_damage_response->asdi_distance_x_q16 ||
        shield_sdi_distance_q16 <= INT32_C(0) ||
        shield_asdi_distance_q16 <= INT32_C(0))
    {
        return fail("shield-sdi-default-data");
    }

    changed.fighter.shield_sdi_scale_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&changed, &invalid_view),
            PF_STATUS_INVALID_CONFIG,
            "shield-sdi-zero-scale"))
    {
        return 0;
    }
    changed.fighter.shield_sdi_scale_q16 = PF_Q16_ONE + INT32_C(1);
    if (!expect_status(
            pf_m4_make_content_view(&changed, &invalid_view),
            PF_STATUS_INVALID_CONFIG,
            "shield-sdi-over-scale"))
    {
        return 0;
    }
    changed.fighter.shield_sdi_scale_q16 = PF_Q16_ONE / INT32_C(2);
    if (!expect_status(
            pf_m4_make_content_view(&changed, &changed_view),
            PF_STATUS_OK,
            "shield-sdi-changed-scale") ||
        memcmp(
            view->content_hash.bytes,
            changed_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0)
    {
        return fail("shield-sdi-content-hash");
    }

    edge_content.fighter.sdi_distance_x_q16 =
        INT32_C(4) * PF_Q16_ONE;
    if (!expect_status(
            pf_m4_make_content_view(&edge_content, &edge_view),
            PF_STATUS_OK,
            "shield-sdi-edge-content"))
    {
        return 0;
    }

    if (!initialize_sim(
            &horizontal_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &horizontal) ||
        !initialize_sim(
            &vertical_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &vertical) ||
        !initialize_sim(
            &reentry_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &reentry) ||
        !initialize_sim(
            &edge_storage,
            &edge_view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &edge) ||
        !start_normal_shield_block(
            horizontal,
            &horizontal_inspection) ||
        !start_normal_shield_block(vertical, &vertical_inspection) ||
        !start_normal_shield_block(reentry, &reentry_inspection))
    {
        return fail("shield-sdi-setup");
    }

    horizontal_start_x =
        horizontal_inspection.players[1].position_x_q16;
    horizontal_start_y =
        horizontal_inspection.players[1].position_y_q16;
    if (!step_reaction_duel(
            horizontal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &horizontal_inspection) ||
        horizontal_inspection.players[1].sdi_pulse_count !=
            UINT8_C(1) ||
        horizontal_inspection.players[1].position_x_q16 !=
            horizontal_start_x + shield_sdi_distance_q16 ||
        horizontal_inspection.players[1].position_y_q16 !=
            horizontal_start_y)
    {
        return fail("shield-sdi-horizontal-pulse");
    }
    first_pulse_x =
        horizontal_inspection.players[1].position_x_q16;

    if (!step_reaction_duel(
            horizontal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &horizontal_inspection) ||
        horizontal_inspection.players[1].sdi_pulse_count !=
            UINT8_C(1) ||
        horizontal_inspection.players[1].position_x_q16 !=
            first_pulse_x)
    {
        return fail("shield-sdi-held-horizontal");
    }

    if (!step_reaction_duel(
            horizontal,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_MIN,
            UINT64_C(0),
            UINT16_MAX,
            &horizontal_inspection) ||
        horizontal_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        horizontal_inspection.players[1].sdi_pulse_count !=
            UINT8_C(1) ||
        horizontal_inspection.players[1].position_x_q16 !=
            first_pulse_x + shield_asdi_distance_q16 +
                horizontal_inspection.players[1].velocity_x_q16 ||
        horizontal_inspection.players[1].position_y_q16 !=
            horizontal_start_y)
    {
        return fail("shield-asdi-horizontal-only");
    }

    vertical_start_x = vertical_inspection.players[1].position_x_q16;
    vertical_start_y = vertical_inspection.players[1].position_y_q16;
    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                vertical,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_MIN,
                UINT64_C(0),
                UINT16_MAX,
                &vertical_inspection))
        {
            return fail("shield-sdi-vertical-step");
        }
    }
    if (vertical_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        vertical_inspection.players[1].sdi_pulse_count != UINT8_C(0) ||
        vertical_inspection.players[1].position_x_q16 !=
            vertical_start_x +
                vertical_inspection.players[1].velocity_x_q16 ||
        vertical_inspection.players[1].position_y_q16 !=
            vertical_start_y)
    {
        return fail("shield-sdi-vertical-negative");
    }

    reentry_start_x = reentry_inspection.players[1].position_x_q16;
    reentry_start_y = reentry_inspection.players[1].position_y_q16;
    if (!step_reaction_duel(
            reentry,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &reentry_inspection) ||
        !step_reaction_duel(
            reentry,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &reentry_inspection) ||
        !step_reaction_duel(
            reentry,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(-32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &reentry_inspection) ||
        reentry_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD_STUN ||
        reentry_inspection.players[1].sdi_pulse_count != UINT8_C(2) ||
        reentry_inspection.players[1].position_x_q16 !=
            reentry_start_x - shield_asdi_distance_q16 +
                reentry_inspection.players[1].velocity_x_q16 ||
        reentry_inspection.players[1].position_y_q16 != reentry_start_y)
    {
        return fail("shield-sdi-horizontal-reentry");
    }

    if (!expect_status(
            pf_m4_inspect(edge, &edge_inspection),
            PF_STATUS_OK,
            "shield-sdi-edge-inspect"))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         tick < UINT32_C(1000) &&
         edge_inspection.players[1].position_x_q16 <
             edge_content.stage.floor_right_q16 -
                 INT32_C(2) * PF_Q16_ONE;
         ++tick)
    {
        if (!step_reaction_duel(
                edge,
                INT16_C(12000),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(12000),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &edge_inspection))
        {
            return fail("shield-sdi-edge-approach");
        }
    }
    if (tick == UINT32_C(1000) ||
        edge_inspection.players[1].grounded == UINT8_C(0) ||
        edge_inspection.players[1].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        !start_normal_shield_block(edge, &edge_inspection))
    {
        return fail("shield-sdi-edge-setup");
    }
    for (tick = UINT32_C(0);
         tick < (uint32_t)edge_content.fighter.jab_hitlag_ticks;
         ++tick)
    {
        if (!step_reaction_duel(
                edge,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_MAX,
                INT16_C(0),
                UINT64_C(0),
                UINT16_MAX,
                &edge_inspection))
        {
            return fail("shield-sdi-edge-step");
        }
    }
    if (edge_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        edge_inspection.players[1].grounded != UINT8_C(0) ||
        edge_inspection.players[1].support !=
            (uint8_t)PF_M4_SURFACE_NONE ||
        edge_inspection.players[1].position_x_q16 <=
            edge_content.stage.floor_right_q16 ||
        edge_inspection.players[1].sdi_pulse_count != UINT8_C(1))
    {
        (void)fprintf(
            stderr,
            "m4-combat=trace operation=shield-sdi-edge-fall"
            " action=%u grounded=%u support=%u x=%" PRId32
            " floor_right=%" PRId32 " y=%" PRId32
            " pulses=%u\n",
            (unsigned int)edge_inspection.players[1].action_state,
            (unsigned int)edge_inspection.players[1].grounded,
            (unsigned int)edge_inspection.players[1].support,
            edge_inspection.players[1].position_x_q16,
            edge_content.stage.floor_right_q16,
            edge_inspection.players[1].position_y_q16,
            (unsigned int)edge_inspection.players[1].sdi_pulse_count);
        return fail("shield-sdi-edge-fall");
    }
    return 1;
}

static int run_light_shield_block_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage light_storage;
    test_sim_storage midpoint_storage;
    test_sim_storage dense_storage;
    test_sim_storage window_storage;
    pf_sim *light = NULL;
    pf_sim *midpoint = NULL;
    pf_sim *dense = NULL;
    pf_sim *window = NULL;
    pf_m4_inspection light_inspection;
    pf_m4_inspection midpoint_inspection;
    pf_m4_inspection dense_inspection;
    pf_m4_inspection window_inspection;
    const uint16_t light_input =
        content->fighter.light_shield_trigger_threshold;
    const uint16_t light_strength = UINT16_C(1);
    const uint16_t dense_strength =
        content->fighter.digital_trigger_threshold;
    const uint16_t midpoint_input =
        (uint16_t)(
            (uint32_t)light_input +
            ((uint32_t)dense_strength - (uint32_t)light_input) /
                UINT32_C(2));
    const uint16_t midpoint_strength =
        (uint16_t)(
            ((uint32_t)midpoint_input -
             ((uint32_t)light_input - UINT32_C(1))) *
            (uint32_t)UINT16_MAX /
            ((uint32_t)UINT16_MAX -
             ((uint32_t)light_input - UINT32_C(1))));
    const uint32_t light_shield_damage =
        expected_shield_damage_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            light_strength);
    const uint32_t dense_shield_damage =
        expected_shield_damage_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            dense_strength);
    const uint32_t midpoint_shield_damage =
        expected_shield_damage_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            midpoint_strength);
    const uint32_t light_expected_health =
        content->fighter.shield_health_q16 -
        UINT32_C(7) *
            content->fighter.light_shield_hold_depletion_q16 -
        light_shield_damage;
    const uint32_t dense_expected_health =
        content->fighter.shield_health_q16 -
        UINT32_C(7) * content->fighter.shield_hold_depletion_q16 -
        dense_shield_damage;
    const uint32_t midpoint_expected_health =
        content->fighter.shield_health_q16 -
        UINT32_C(7) * expected_shield_depletion_q16(
                             &content->fighter,
                             midpoint_strength) -
        midpoint_shield_damage;
    const uint32_t window_expected_health =
        content->fighter.shield_health_q16 - light_shield_damage;
    const uint16_t expected_light_stun =
        expected_shield_stun_ticks(
            &content->fighter,
            content->fighter.jab_damage_q16,
            light_strength);
    const uint16_t expected_dense_stun =
        expected_shield_stun_ticks(
            &content->fighter,
            content->fighter.jab_damage_q16,
            dense_strength);
    const uint16_t expected_midpoint_stun =
        expected_shield_stun_ticks(
            &content->fighter,
            content->fighter.jab_damage_q16,
            midpoint_strength);
    const int32_t expected_light_defender_pushback =
        expected_shield_defender_pushback_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            light_strength,
            0);
    const int32_t expected_dense_defender_pushback =
        expected_shield_defender_pushback_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            dense_strength,
            0);
    const int32_t expected_midpoint_defender_pushback =
        expected_shield_defender_pushback_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            midpoint_strength,
            0);
    const int32_t expected_light_attacker_pushback =
        expected_shield_attacker_pushback_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            light_strength);
    const int32_t expected_dense_attacker_pushback =
        expected_shield_attacker_pushback_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            dense_strength);
    const int32_t expected_midpoint_attacker_pushback =
        expected_shield_attacker_pushback_q16(
            &content->fighter,
            content->fighter.jab_damage_q16,
            midpoint_strength);

    if (!initialize_sim(
            &light_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &light) ||
        !initialize_sim(
            &midpoint_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &midpoint) ||
        !initialize_sim(
            &dense_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &dense) ||
        !initialize_sim(
            &window_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &window))
    {
        return fail("light-shield-block-init");
    }

    if (!start_held_shield_block(
            light,
            light_input,
            &light_inspection) ||
        light_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        light_inspection.players[1].shield_strength != light_strength ||
        light_inspection.players[1].powershield != UINT8_C(0) ||
        light_inspection.players[1].shield_health_q16 !=
            light_expected_health ||
        light_inspection.players[1].shield_stun_ticks !=
            expected_light_stun ||
        light_inspection.players[1].velocity_x_q16 !=
            expected_light_defender_pushback ||
        light_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        light_inspection.players[0].shield_recoil_x_q16 !=
            -expected_light_attacker_pushback ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[0].value_q16 != light_shield_damage)
    {
        return fail("light-shield-held-block");
    }
    if (!start_held_shield_block(
            midpoint,
            midpoint_input,
            &midpoint_inspection) ||
        midpoint_inspection.players[1].shield_strength !=
            midpoint_strength ||
        midpoint_inspection.players[1].powershield != UINT8_C(0) ||
        midpoint_inspection.players[1].shield_health_q16 !=
            midpoint_expected_health ||
        midpoint_inspection.players[1].shield_stun_ticks !=
            expected_midpoint_stun ||
        midpoint_inspection.players[1].velocity_x_q16 !=
            expected_midpoint_defender_pushback ||
        midpoint_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        midpoint_inspection.players[0].shield_recoil_x_q16 !=
            -expected_midpoint_attacker_pushback ||
        test_last_result.events[0].value_q16 !=
            midpoint_shield_damage)
    {
        return fail("midpoint-shield-pressure-response");
    }
    if (!start_held_shield_block(
            dense,
            dense_strength,
            &dense_inspection) ||
        dense_inspection.players[1].shield_strength != dense_strength ||
        dense_inspection.players[1].powershield != UINT8_C(0) ||
        dense_inspection.players[1].shield_health_q16 !=
            dense_expected_health ||
        dense_inspection.players[1].shield_stun_ticks !=
            expected_dense_stun ||
        dense_inspection.players[1].velocity_x_q16 !=
            expected_dense_defender_pushback ||
        dense_inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        dense_inspection.players[0].shield_recoil_x_q16 !=
            -expected_dense_attacker_pushback ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[0].value_q16 != dense_shield_damage)
    {
        return fail("light-shield-pushback-interpolation");
    }
    if (!(light_shield_damage > midpoint_shield_damage &&
          midpoint_shield_damage > dense_shield_damage &&
          expected_light_stun > expected_midpoint_stun &&
          expected_midpoint_stun > expected_dense_stun &&
          expected_light_defender_pushback >
              expected_midpoint_defender_pushback &&
          expected_midpoint_defender_pushback >
              expected_dense_defender_pushback &&
          expected_light_attacker_pushback <
              expected_midpoint_attacker_pushback &&
          expected_midpoint_attacker_pushback <
              expected_dense_attacker_pushback))
    {
        return fail("shield-pressure-response-ordering");
    }

    if (!start_window_shield_block(
            window,
            light_input,
            &window_inspection) ||
        window_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        window_inspection.players[1].shield_strength != light_strength ||
        window_inspection.players[1].powershield != UINT8_C(0) ||
        window_inspection.players[1].shield_health_q16 !=
            window_expected_health ||
        window_inspection.players[1].shield_stun_ticks !=
            expected_light_stun ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[0].value_q16 != light_shield_damage ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS)
    {
        return fail("light-shield-powershield-gate");
    }
    return 1;
}

static int run_shield_geometry_and_poke_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage control_storage;
    test_sim_storage poke_storage;
    pf_sim *control = NULL;
    pf_sim *poke = NULL;
    pf_m4_inspection control_inspection;
    pf_m4_inspection poke_inspection;
    test_shield_box control_box;
    test_shield_box poke_box;
    const uint16_t dense =
        content->fighter.digital_trigger_threshold;
    const int16_t upward_tilt =
        -(int16_t)(
            content->fighter.tap_jump_axis_threshold - UINT16_C(1));
    int32_t attack_left_q16;
    int32_t attack_right_q16;
    int32_t attack_top_q16;
    int32_t attack_bottom_q16;
    uint32_t control_health_before;
    uint32_t tick;

    if (!initialize_sim(
            &control_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &control) ||
        !initialize_sim(
            &poke_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &poke))
    {
        return fail("shield-geometry-init");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                control,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                dense,
                &control_inspection) ||
            !step_reaction_duel(
                poke,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                upward_tilt,
                UINT64_C(0),
                dense,
                &poke_inspection))
        {
            return fail("shield-geometry-hold");
        }
    }

    control_box = observed_shield_box(&control_inspection.players[1]);
    poke_box = observed_shield_box(&poke_inspection.players[1]);
    attack_left_q16 =
        control_inspection.players[0].position_x_q16 +
        content->fighter.jab_hitbox_offset_x_q16 -
        content->fighter.jab_hitbox_half_width_q16;
    attack_right_q16 =
        control_inspection.players[0].position_x_q16 +
        content->fighter.jab_hitbox_offset_x_q16 +
        content->fighter.jab_hitbox_half_width_q16;
    attack_top_q16 =
        control_inspection.players[0].position_y_q16 +
        content->fighter.jab_hitbox_offset_y_q16 -
        content->fighter.jab_hitbox_half_height_q16;
    attack_bottom_q16 =
        control_inspection.players[0].position_y_q16 +
        content->fighter.jab_hitbox_offset_y_q16 +
        content->fighter.jab_hitbox_half_height_q16;
    if (control_inspection.players[1].shield_active != UINT8_C(1) ||
        poke_inspection.players[1].shield_active != UINT8_C(1) ||
        control_inspection.players[1].shield_tilt_y != INT16_C(0) ||
        poke_inspection.players[1].shield_tilt_y >= INT16_C(0) ||
        control_inspection.players[1].shield_left_q16 !=
            control_box.left_q16 ||
        control_inspection.players[1].shield_right_q16 !=
            control_box.right_q16 ||
        control_inspection.players[1].shield_top_q16 !=
            control_box.top_q16 ||
        control_inspection.players[1].shield_bottom_q16 !=
            control_box.bottom_q16 ||
        poke_inspection.players[1].shield_left_q16 != poke_box.left_q16 ||
        poke_inspection.players[1].shield_right_q16 != poke_box.right_q16 ||
        poke_inspection.players[1].shield_top_q16 != poke_box.top_q16 ||
        poke_inspection.players[1].shield_bottom_q16 !=
            poke_box.bottom_q16 ||
        poke_box.top_q16 >= control_box.top_q16 ||
        poke_box.bottom_q16 >= control_box.bottom_q16 ||
        attack_left_q16 > control_box.right_q16 ||
        attack_right_q16 < control_box.left_q16 ||
        attack_top_q16 > control_box.bottom_q16 ||
        attack_bottom_q16 < control_box.top_q16 ||
        attack_top_q16 <= poke_box.bottom_q16 ||
        attack_top_q16 >
            poke_inspection.players[1].position_y_q16 +
                content->fighter.half_height_q16 ||
        attack_bottom_q16 <
            poke_inspection.players[1].position_y_q16 -
                content->fighter.half_height_q16)
    {
        return fail("shield-geometry-tilt-contract");
    }
    control_health_before =
        control_inspection.players[1].shield_health_q16;

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_startup_ticks +
                    (uint32_t)content->fighter.jab_active_ticks +
                    UINT32_C(2);
         ++tick)
    {
        if (!step_reaction_duel(
                control,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                dense,
                &control_inspection))
        {
            return fail("shield-geometry-control-step");
        }
        if (find_last_tick_event(PF_SIM_EVENT_SHIELD_BLOCK) != NULL)
        {
            break;
        }
    }
    if (tick ==
            (uint32_t)content->fighter.jab_startup_ticks +
                (uint32_t)content->fighter.jab_active_ticks +
                UINT32_C(2) ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BLOCK ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
        control_inspection.players[1].damage_q16 != UINT32_C(0) ||
        control_inspection.players[1].shield_health_q16 >=
            control_health_before ||
        control_inspection.players[1].shield_active != UINT8_C(1))
    {
        return fail("shield-geometry-control-block");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.jab_startup_ticks +
                    (uint32_t)content->fighter.jab_active_ticks +
                    UINT32_C(2);
         ++tick)
    {
        if (!step_reaction_duel(
                poke,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                upward_tilt,
                UINT64_C(0),
                dense,
                &poke_inspection))
        {
            return fail("shield-poke-step");
        }
        if (find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            break;
        }
    }
    if (tick ==
            (uint32_t)content->fighter.jab_startup_ticks +
                (uint32_t)content->fighter.jab_active_ticks +
                UINT32_C(2) ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS ||
        poke_inspection.players[1].damage_q16 !=
            content->fighter.jab_damage_q16 ||
        poke_inspection.players[1].last_hit_attacker != UINT8_C(0) ||
        poke_inspection.players[1].shield_active != UINT8_C(0) ||
        poke_inspection.players[1].shield_strength != UINT16_C(0) ||
        poke_inspection.players[1].shield_tilt_x != INT16_C(0) ||
        poke_inspection.players[1].shield_tilt_y != INT16_C(0))
    {
        return fail("shield-poke-exposed-hurtbox");
    }
    return 1;
}

static int32_t melee_distance_hundredths_to_sim_q16(
    uint32_t melee_distance_hundredths)
{
    const int64_t numerator =
        (int64_t)melee_distance_hundredths *
        (int64_t)PF_Q16_ONE * INT64_C(12);
    const int64_t denominator = INT64_C(100) * INT64_C(115);

    return (int32_t)(
        (numerator + denominator / INT64_C(2)) / denominator);
}

static int run_reference_shield_boundary_case(
    uint32_t melee_distance_hundredths,
    int16_t shield_axis_x,
    int16_t shield_axis_y,
    int expect_block)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    const uint16_t light_trigger = UINT16_C(21064);
    const int32_t total_distance_q16 =
        melee_distance_hundredths_to_sim_q16(
            melee_distance_hundredths);
    uint32_t tick;
    int blocked = 0;
    int hit = 0;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "reference-shield-boundary-default-content"))
    {
        return 0;
    }
    content.stage.spawn_spacing_q16 = total_distance_q16 / INT32_C(2);
    content.item.enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "reference-shield-boundary-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                shield_axis_x,
                shield_axis_y,
                UINT64_C(0),
                light_trigger,
                &inspection))
        {
            return fail("reference-shield-boundary-hold");
        }
    }
    if (inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_SHIELD ||
        inspection.players[1].shield_active == UINT8_C(0))
    {
        return fail("reference-shield-boundary-ready");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content.fighter.jab_startup_ticks +
                    (uint32_t)content.fighter.jab_active_ticks +
                    UINT32_C(2);
         ++tick)
    {
        const pf_sim_event *block_event;
        const pf_sim_event *hit_event;

        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                shield_axis_x,
                shield_axis_y,
                UINT64_C(0),
                light_trigger,
                &inspection))
        {
            return fail("reference-shield-boundary-attack");
        }
        block_event = find_last_tick_event(PF_SIM_EVENT_SHIELD_BLOCK);
        hit_event = find_last_tick_event(PF_SIM_EVENT_HIT);
        blocked |= block_event != NULL;
        hit |= hit_event != NULL;
        if (block_event != NULL || hit_event != NULL)
        {
            break;
        }
    }
    if (blocked != expect_block || hit != 0 ||
        inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("reference-shield-boundary-result");
    }
    return 1;
}

static int run_reference_shield_boundary_test(void)
{
    /* These are the last-hit/first-miss boundaries from the pinned 0.05-unit
     * GALE01 sweep. Axes reproduce the controller values observed by Dolphin;
     * X is mirrored because the simulation defender starts facing left. */
    return run_reference_shield_boundary_case(
               UINT32_C(2860),
               INT16_C(0),
               INT16_C(0),
               1) &&
           run_reference_shield_boundary_case(
               UINT32_C(2865),
               INT16_C(0),
               INT16_C(0),
               0) &&
           run_reference_shield_boundary_case(
               UINT32_C(2965),
               INT16_C(-12697),
               INT16_C(-12697),
               1) &&
           run_reference_shield_boundary_case(
               UINT32_C(2970),
               INT16_C(-12697),
               INT16_C(-12697),
               0) &&
           run_reference_shield_boundary_case(
               UINT32_C(2960),
               INT16_C(-12697),
               INT16_C(13107),
               1) &&
           run_reference_shield_boundary_case(
               UINT32_C(2965),
               INT16_C(-12697),
               INT16_C(13107),
               0);
}

static int run_reference_moving_hit_sweep_runtime_case(
    uint32_t melee_distance_hundredths,
    int expect_hit)
{
    test_sim_storage storage;
    const pf_m4_ssbm_ground_input_attributes *ground_input =
        pf_m4_ssbm_common_reference_ground_input();
    pf_m4_content content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    const int32_t total_distance_q16 =
        melee_distance_hundredths_to_sim_q16(
            melee_distance_hundredths);
    int16_t down_tilt_axis;
    uint32_t tick;
    uint16_t hit_action_tick = UINT16_MAX;
    const pf_sim_event *hit_event = NULL;
    int saw_grab = 0;

    if (ground_input == NULL ||
        !expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "reference-moving-hit-runtime-default-content"))
    {
        return 0;
    }
    down_tilt_axis = (int16_t)(
        ground_input->vertical_smash_axis_threshold - UINT16_C(1));
    content.stage.spawn_spacing_q16 = total_distance_q16 / INT32_C(2);
    content.item.enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "reference-moving-hit-runtime-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(30); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                down_tilt_axis,
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                tick == UINT32_C(0) ? UINT16_MAX : UINT16_C(0),
                &inspection))
        {
            return fail("reference-moving-hit-runtime-step");
        }
        saw_grab |= find_last_tick_event(PF_SIM_EVENT_GRAB) != NULL;
        hit_event = find_last_tick_event(PF_SIM_EVENT_HIT);
        if (hit_event != NULL)
        {
            hit_action_tick = inspection.players[0].action_ticks;
            break;
        }
    }
    if (expect_hit != 0)
    {
        if (hit_action_tick != UINT16_C(12) ||
            inspection.players[1].damage_q16 == UINT32_C(0) ||
            inspection.players[1].last_hit_attacker != UINT8_C(0) ||
            hit_event == NULL ||
            hit_event->detail != (uint16_t)PF_M4_ACTION_DOWN_ATTACK ||
            saw_grab != 0)
        {
            return fail("reference-moving-hit-runtime-hit");
        }
    }
    else if (hit_action_tick != UINT16_MAX ||
             inspection.players[0].damage_q16 != UINT32_C(0) ||
             inspection.players[1].damage_q16 != UINT32_C(0) ||
             saw_grab != 0)
    {
        return fail("reference-moving-hit-runtime-miss");
    }
    return 1;
}

static int run_reference_moving_hit_sweep_test(void)
{
    pf_m4_content content;
    const int32_t total_distance_q16 =
        melee_distance_hundredths_to_sim_q16(UINT32_C(2740));

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "reference-moving-hit-default-content"))
    {
        return 0;
    }
    {
        uint8_t current_count = UINT8_C(0);
        uint8_t previous_count = UINT8_C(0);
        uint8_t hurt_count = UINT8_C(0);
        const pf_m4_reference_hit_sphere *current =
            pf_m4_falcon_reference_hit_spheres_at_frame(
                PF_M4_FALCON_DOWN_TILT,
                UINT16_C(12),
                &current_count);
        const pf_m4_reference_hit_sphere *previous =
            pf_m4_falcon_reference_hit_spheres_at_frame(
                PF_M4_FALCON_DOWN_TILT,
                UINT16_C(11),
                &previous_count);
        const pf_m4_reference_hurt_capsule *hurts =
            pf_m4_falcon_reference_hurt_capsules_at_frame(
                PF_M4_FALCON_GRAB,
                UINT16_C(12),
                &hurt_count);
        uint8_t sphere_index;
        int current_hit = 0;
        int swept_hit = 0;

        if (current == NULL || previous == NULL || hurts == NULL)
        {
            return fail("reference-moving-hit-geometry");
        }
        for (sphere_index = UINT8_C(0);
             sphere_index < current_count;
             ++sphere_index)
        {
            uint8_t capsule_index;
            const pf_m4_reference_hit_sphere *prior =
                &current[sphere_index];
            uint8_t previous_index;

            for (previous_index = UINT8_C(0);
                 previous_index < previous_count;
                 ++previous_index)
            {
                if (current[sphere_index].collision_state == UINT8_C(3) &&
                    previous[previous_index].hitbox_id ==
                        current[sphere_index].hitbox_id)
                {
                    prior = &previous[previous_index];
                    break;
                }
            }
            for (capsule_index = UINT8_C(0);
                 capsule_index < hurt_count;
                 ++capsule_index)
            {
                const pf_m4_collision_capsule3_q16 hurt = {
                    total_distance_q16 -
                        hurts[capsule_index].endpoint_a_x_q16,
                    hurts[capsule_index].endpoint_a_y_q16,
                    -hurts[capsule_index].endpoint_a_z_q16,
                    total_distance_q16 -
                        hurts[capsule_index].endpoint_b_x_q16,
                    hurts[capsule_index].endpoint_b_y_q16,
                    -hurts[capsule_index].endpoint_b_z_q16,
                    hurts[capsule_index].radius_q16};
                const pf_m4_collision_capsule3_q16 current_hitbox = {
                    current[sphere_index].offset_x_q16,
                    current[sphere_index].offset_y_q16,
                    current[sphere_index].offset_z_q16,
                    current[sphere_index].offset_x_q16,
                    current[sphere_index].offset_y_q16,
                    current[sphere_index].offset_z_q16,
                    current[sphere_index].radius_q16};
                const pf_m4_collision_capsule3_q16 swept_hitbox = {
                    prior->offset_x_q16,
                    prior->offset_y_q16,
                    prior->offset_z_q16,
                    current[sphere_index].offset_x_q16,
                    current[sphere_index].offset_y_q16,
                    current[sphere_index].offset_z_q16,
                    current[sphere_index].radius_q16};

                current_hit |=
                    pf_m4_collision_capsule_capsule_overlap_q16(
                        &current_hitbox,
                        &hurt);
                swept_hit |=
                    pf_m4_collision_capsule_capsule_overlap_q16(
                        &swept_hitbox,
                        &hurt);
            }
        }
        if (current_hit != 0 || swept_hit == 0)
        {
            return fail("reference-moving-hit-discriminator");
        }
    }
    return run_reference_moving_hit_sweep_runtime_case(
               UINT32_C(2740),
               1) &&
           run_reference_moving_hit_sweep_runtime_case(
               UINT32_C(2830),
               0);
}

static int run_reference_common_hurt_runtime_case(
    uint32_t melee_distance_hundredths,
    int16_t target_main_x,
    int16_t target_main_y,
    uint64_t target_buttons,
    uint16_t target_button_delay_ticks,
    uint8_t expected_target_action,
    uint16_t expected_hit_action_tick,
    int expect_hit)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    const int32_t total_distance_q16 =
        melee_distance_hundredths_to_sim_q16(
            melee_distance_hundredths);
    uint32_t tick;
    uint16_t hit_action_tick = UINT16_MAX;
    int saw_target_action = 0;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "reference-common-hurt-default-content"))
    {
        return 0;
    }
    content.stage.spawn_spacing_q16 = total_distance_q16 / INT32_C(2);
    content.item.enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "reference-common-hurt-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        if (!step_reaction_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                tick == UINT32_C(0)
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                UINT16_C(0),
                target_main_x,
                target_main_y,
                tick >= (uint32_t)target_button_delay_ticks
                    ? target_buttons
                    : UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return fail("reference-common-hurt-step");
        }
        saw_target_action |= inspection.players[1].action_state ==
            expected_target_action;
        if (find_last_tick_event(PF_SIM_EVENT_HIT) != NULL)
        {
            hit_action_tick = inspection.players[0].action_ticks;
            break;
        }
    }
    if (saw_target_action == 0)
    {
        return fail("reference-common-hurt-target-state");
    }
    if (expect_hit != 0)
    {
        if (hit_action_tick == UINT16_MAX ||
            hit_action_tick != expected_hit_action_tick ||
            inspection.players[1].damage_q16 == UINT32_C(0) ||
            inspection.players[1].last_hit_attacker != UINT8_C(0))
        {
            return fail("reference-common-hurt-hit");
        }
    }
    else if (hit_action_tick != UINT16_MAX ||
             inspection.players[0].damage_q16 != UINT32_C(0) ||
             inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("reference-common-hurt-miss");
    }
    return 1;
}

static int reference_hit_geometry_overlap_pose(
    pf_m4_falcon_move_index attacker_move,
    uint16_t attacker_action_frame,
    int8_t attacker_facing,
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_frame,
    int8_t target_facing,
    int32_t target_offset_x_q16,
    int32_t target_offset_y_q16,
    int grabbable_only)
{
    uint8_t hit_sphere_count = UINT8_C(0);
    uint8_t hurt_capsule_count = UINT8_C(0);
    const pf_m4_reference_hit_sphere *hit_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            attacker_move,
            attacker_action_frame,
            &hit_sphere_count);
    const pf_m4_reference_hurt_capsule *hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_for_submotion_at_frame(
            action_state,
            source_submotion,
            action_frame,
            &hurt_capsule_count);
    uint8_t hit_index;

    if (hit_spheres == NULL || hit_sphere_count == UINT8_C(0) ||
        hurt_capsules == NULL || hurt_capsule_count != UINT8_C(11) ||
        (attacker_facing != INT8_C(-1) && attacker_facing != INT8_C(1)) ||
        (target_facing != INT8_C(-1) && target_facing != INT8_C(1)))
    {
        return -1;
    }
    for (hit_index = UINT8_C(0);
         hit_index < hit_sphere_count;
         ++hit_index)
    {
        const pf_m4_collision_capsule3_q16 hit = {
            (int64_t)attacker_facing *
                (int64_t)hit_spheres[hit_index].offset_x_q16,
            (int64_t)hit_spheres[hit_index].offset_y_q16,
            (int64_t)attacker_facing *
                (int64_t)hit_spheres[hit_index].offset_z_q16,
            (int64_t)attacker_facing *
                (int64_t)hit_spheres[hit_index].offset_x_q16,
            (int64_t)hit_spheres[hit_index].offset_y_q16,
            (int64_t)attacker_facing *
                (int64_t)hit_spheres[hit_index].offset_z_q16,
            (int64_t)hit_spheres[hit_index].radius_q16};
        uint8_t hurt_index;

        for (hurt_index = UINT8_C(0);
             hurt_index < hurt_capsule_count;
             ++hurt_index)
        {
            if (grabbable_only != 0 &&
                hurt_capsules[hurt_index].grabbable == UINT8_C(0))
            {
                continue;
            }
            const pf_m4_collision_capsule3_q16 hurt = {
                (int64_t)target_offset_x_q16 +
                    (int64_t)target_facing *
                        (int64_t)hurt_capsules[hurt_index].endpoint_a_x_q16,
                (int64_t)target_offset_y_q16 +
                    (int64_t)hurt_capsules[hurt_index].endpoint_a_y_q16,
                (int64_t)target_facing *
                    (int64_t)hurt_capsules[hurt_index].endpoint_a_z_q16,
                (int64_t)target_offset_x_q16 +
                    (int64_t)target_facing *
                        (int64_t)hurt_capsules[hurt_index].endpoint_b_x_q16,
                (int64_t)target_offset_y_q16 +
                    (int64_t)hurt_capsules[hurt_index].endpoint_b_y_q16,
                (int64_t)target_facing *
                    (int64_t)hurt_capsules[hurt_index].endpoint_b_z_q16,
                (int64_t)hurt_capsules[hurt_index].radius_q16};

            if (pf_m4_collision_capsule_capsule_overlap_q16(
                    &hit,
                    &hurt))
            {
                return 1;
            }
        }
    }
    return 0;
}

static int reference_common_hurt_overlap_at_distance(
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_frame,
    uint16_t jab_frame,
    int8_t facing,
    uint32_t melee_distance_hundredths,
    uint32_t melee_height_hundredths)
{
    return reference_hit_geometry_overlap_pose(
        PF_M4_FALCON_JAB1,
        jab_frame,
        INT8_C(1),
        action_state,
        source_submotion,
        action_frame,
        facing,
        melee_distance_hundredths_to_sim_q16(
            melee_distance_hundredths),
        -melee_distance_hundredths_to_sim_q16(
            melee_height_hundredths),
        0);
}

static uint8_t reference_common_hurt_read_pose(
    void *context,
    uint8_t action_state,
    uint16_t source_submotion,
    uint16_t action_frame,
    pf_ssbm_stored_hurt_capsule *out_capsules,
    uint8_t capacity)
{
    uint8_t capsule_count = UINT8_C(0);
    const pf_m4_reference_hurt_capsule *capsules =
        pf_m4_falcon_reference_common_hurt_capsules_for_submotion_at_frame(
            action_state,
            source_submotion,
            action_frame,
            &capsule_count);
    uint8_t capsule_index;

    (void)context;
    if (capsules == NULL || capsule_count > capacity)
    {
        return UINT8_C(0);
    }
    for (capsule_index = UINT8_C(0);
         capsule_index < capsule_count;
         ++capsule_index)
    {
        out_capsules[capsule_index] = (pf_ssbm_stored_hurt_capsule){
            capsules[capsule_index].endpoint_a_x_q16,
            capsules[capsule_index].endpoint_a_y_q16,
            capsules[capsule_index].endpoint_a_z_q16,
            capsules[capsule_index].endpoint_b_x_q16,
            capsules[capsule_index].endpoint_b_y_q16,
            capsules[capsule_index].endpoint_b_z_q16,
            capsules[capsule_index].radius_q16,
            capsules[capsule_index].hurtbox_id,
            capsules[capsule_index].height,
            capsules[capsule_index].grabbable,
            capsules[capsule_index].reserved};
    }
    return capsule_count;
}

static int reference_common_hurt_run_runtime_case(
    void *context,
    const pf_ssbm_stored_case *stored_case)
{
    (void)context;
    return run_reference_common_hurt_runtime_case(
        stored_case->distance_hundredths,
        stored_case->target_stick_x_or_facing,
        stored_case->target_stick_y,
        stored_case->target_buttons,
        stored_case->button_delay_or_jab_frame,
        stored_case->target_action,
        stored_case->expected_hit_action_tick,
        stored_case->expect_hit != UINT8_C(0));
}

static int reference_common_hurt_run_geometry_case(
    void *context,
    const pf_ssbm_stored_case *stored_case)
{
    (void)context;
    return reference_common_hurt_overlap_at_distance(
        stored_case->target_action,
        stored_case->target_source_submotion,
        stored_case->action_frame,
        stored_case->button_delay_or_jab_frame,
        (int8_t)stored_case->target_stick_x_or_facing,
        stored_case->distance_hundredths,
        stored_case->height_hundredths);
}

static int reference_falcon_turn_hurt_pose_facing_case(
    void *context,
    const pf_ssbm_stored_case *stored_case)
{
    const pf_m4_fighter_data *fighter = context;
    const pf_ssbm_stored_pose_facing_case *phase =
        &stored_case->pose_facing;
    pf_m4_hurt_capsule_inspection
        world_capsules[PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY];
    uint8_t source_count = UINT8_C(0);
    const pf_m4_reference_hurt_capsule *source =
        pf_m4_falcon_reference_common_hurt_capsules_for_submotion_at_frame(
            stored_case->target_action,
            stored_case->target_source_submotion,
            stored_case->action_frame,
            &source_count);
    const uint8_t world_count = pf_m4_reference_world_hurt_capsules(
        fighter,
        INT32_C(0),
        INT32_C(0),
        phase->gameplay_facing,
        phase->dash_direction,
        UINT8_C(1),
        stored_case->target_action,
        UINT8_C(0),
        stored_case->target_source_submotion,
        phase->action_ticks,
        world_capsules);
    int candidate_facing;

    if (fighter == NULL || phase->enabled == UINT8_C(0) || source == NULL ||
        source_count == UINT8_C(0) || world_count != source_count)
    {
        return 2;
    }
    for (candidate_facing = -1; candidate_facing <= 1;
         candidate_facing += 2)
    {
        uint8_t capsule_index;
        int matches = 1;

        for (capsule_index = UINT8_C(0);
             capsule_index < source_count;
             ++capsule_index)
        {
            const pf_m4_reference_hurt_capsule *local =
                &source[capsule_index];
            const pf_m4_hurt_capsule_inspection *world =
                &world_capsules[capsule_index];

            if (world->endpoint_a_x_q16 !=
                    candidate_facing * local->endpoint_a_x_q16 ||
                world->endpoint_a_y_q16 !=
                    fighter->half_height_q16 + local->endpoint_a_y_q16 ||
                world->endpoint_a_z_q16 !=
                    candidate_facing * local->endpoint_a_z_q16 ||
                world->endpoint_b_x_q16 !=
                    candidate_facing * local->endpoint_b_x_q16 ||
                world->endpoint_b_y_q16 !=
                    fighter->half_height_q16 + local->endpoint_b_y_q16 ||
                world->endpoint_b_z_q16 !=
                    candidate_facing * local->endpoint_b_z_q16 ||
                world->radius_q16 != local->radius_q16 ||
                world->hurtbox_id != local->hurtbox_id ||
                world->height != local->height ||
                world->grabbable != local->grabbable)
            {
                matches = 0;
                break;
            }
        }
        if (matches != 0)
        {
            return candidate_facing;
        }
    }
    return 2;
}

static int run_reference_pose_stored_oracle(
    const pf_ssbm_stored_oracle_domain *domain,
    const char *source_pose_sha256,
    int print_pass)
{
    pf_ssbm_stored_oracle_result result;

    if (!pf_ssbm_stored_oracle_run(domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s "
            "case=%s expected_production_pose_sha256=%s "
            "actual_production_pose_sha256=%s\n",
            domain->name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain->expected_production_pose_sha256,
            result.production_pose_sha256[0] != '\0'
                ? result.production_pose_sha256
                : "unavailable");
        return 0;
    }
    if (print_pass != 0)
    {
        (void)printf(
            "m4-ssbm-stored-oracle=pass domain=%s poses=%u cases=%u "
            "source_pose_sha256=%s production_pose_sha256=%s\n",
            domain->name,
            (unsigned int)domain->expected_pose_count,
            (unsigned int)domain->case_count,
            source_pose_sha256,
            result.production_pose_sha256);
    }
    return 1;
}

static int run_reference_common_hurt_stored_oracle(int print_pass)
{
    static const pf_ssbm_stored_oracle_domain domain = {
        "falcon-common-hurt",
        pf_m4_ssbm_falcon_common_hurt_pose_tracks,
        (uint16_t)(
            sizeof(pf_m4_ssbm_falcon_common_hurt_pose_tracks) /
            sizeof(pf_m4_ssbm_falcon_common_hurt_pose_tracks[0])),
        pf_m4_ssbm_falcon_common_hurt_cases,
        PF_M4_SSBM_FALCON_COMMON_HURT_CASE_COUNT,
        PF_M4_SSBM_FALCON_COMMON_HURT_POSE_COUNT,
        PF_M4_SSBM_FALCON_COMMON_HURT_CAPSULES_PER_POSE,
        PF_M4_SSBM_FALCON_COMMON_HURT_PRODUCTION_POSE_SHA256,
        NULL,
        reference_common_hurt_read_pose,
        reference_common_hurt_run_runtime_case,
        reference_common_hurt_run_geometry_case,
        NULL};
    return run_reference_pose_stored_oracle(
        &domain,
        PF_M4_SSBM_FALCON_COMMON_HURT_SOURCE_POSE_SHA256,
        print_pass);
}

static int run_reference_falcon_turn_hurt_stored_oracle(int print_pass)
{
    pf_m4_content content;
    const pf_ssbm_stored_oracle_domain domain = {
        "falcon-turn-hurt",
        pf_m4_ssbm_falcon_turn_hurt_pose_tracks,
        (uint16_t)(
            sizeof(pf_m4_ssbm_falcon_turn_hurt_pose_tracks) /
            sizeof(pf_m4_ssbm_falcon_turn_hurt_pose_tracks[0])),
        pf_m4_ssbm_falcon_turn_hurt_cases,
        PF_M4_SSBM_FALCON_TURN_HURT_CASE_COUNT,
        PF_M4_SSBM_FALCON_TURN_HURT_POSE_COUNT,
        PF_M4_SSBM_FALCON_TURN_HURT_CAPSULES_PER_POSE,
        PF_M4_SSBM_FALCON_TURN_HURT_PRODUCTION_POSE_SHA256,
        &content.fighter,
        reference_common_hurt_read_pose,
        reference_common_hurt_run_runtime_case,
        reference_common_hurt_run_geometry_case,
        reference_falcon_turn_hurt_pose_facing_case};

    if (pf_m4_default_content(&content) != PF_STATUS_OK)
    {
        return 0;
    }

    return run_reference_pose_stored_oracle(
        &domain,
        PF_M4_SSBM_FALCON_TURN_HURT_SOURCE_POSE_SHA256,
        print_pass);
}

static int reference_geometry_only_runtime_case(
    void *context,
    const pf_ssbm_stored_case *stored_case)
{
    (void)context;
    (void)stored_case;
    return 0;
}

static int reference_falcon_dive_grab_geometry_case(
    void *context,
    const pf_ssbm_stored_case *stored_case)
{
    const pf_ssbm_stored_geometry_case *geometry = &stored_case->geometry;

    (void)context;
    if (geometry->enabled == UINT8_C(0) ||
        geometry->attacker_move >= (uint16_t)PF_M4_FALCON_MOVE_COUNT)
    {
        return -1;
    }
    return reference_hit_geometry_overlap_pose(
        (pf_m4_falcon_move_index)geometry->attacker_move,
        geometry->attacker_action_frame,
        geometry->attacker_facing,
        stored_case->target_action,
        stored_case->target_source_submotion,
        stored_case->action_frame,
        geometry->target_facing,
        geometry->target_offset_x_q16,
        geometry->target_offset_y_q16,
        geometry->grabbable_only != UINT8_C(0));
}

static int run_reference_falcon_dive_grab_stored_oracle(int print_pass)
{
    static const pf_ssbm_stored_oracle_domain domain = {
        "falcon-dive-grab-geometry",
        pf_m4_ssbm_falcon_dive_grab_pose_tracks,
        (uint16_t)(
            sizeof(pf_m4_ssbm_falcon_dive_grab_pose_tracks) /
            sizeof(pf_m4_ssbm_falcon_dive_grab_pose_tracks[0])),
        pf_m4_ssbm_falcon_dive_grab_cases,
        PF_M4_SSBM_FALCON_DIVE_GRAB_CASE_COUNT,
        PF_M4_SSBM_FALCON_DIVE_GRAB_POSE_COUNT,
        PF_M4_SSBM_FALCON_DIVE_GRAB_CAPSULES_PER_POSE,
        PF_M4_SSBM_FALCON_DIVE_GRAB_PRODUCTION_POSE_SHA256,
        NULL,
        reference_common_hurt_read_pose,
        reference_geometry_only_runtime_case,
        reference_falcon_dive_grab_geometry_case,
        NULL};
    return run_reference_pose_stored_oracle(
        &domain,
        PF_M4_SSBM_FALCON_DIVE_GRAB_SOURCE_POSE_SHA256,
        print_pass);
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
        UINT32_C(1) * UINT32_C(65536);
    out_content->fighter.shield_reset_health_q16 =
        UINT32_C(1) * UINT32_C(65536);
    out_content->fighter.shield_hold_depletion_q16 =
        UINT32_C(655);
    out_content->fighter.light_shield_hold_depletion_q16 =
        UINT32_C(164);
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
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_SHIELD_BREAK ||
        test_last_result.events[0].source_player != UINT8_C(0) ||
        test_last_result.events[0].target_player != UINT8_C(1) ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS)
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
            -content->fighter.shield_break_launch_speed_q16 +
                content->fighter.gravity_q16 ||
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
            (out_inspection->players[1].action_state ==
                     (uint8_t)PF_M4_ACTION_SHIELD_BREAK_STUN
                 ? content->fighter.shield_reset_health_q16
                 : UINT32_C(0)))
        {
            (void)fprintf(
                stderr,
                "m4-combat=diagnostic shield-break-health tick=%u"
                " action=%u action_ticks=%u health=%u grounded=%u\n",
                tick,
                (unsigned int)out_inspection->players[1].action_state,
                (unsigned int)out_inspection->players[1].action_ticks,
                out_inspection->players[1].shield_health_q16,
                (unsigned int)out_inspection->players[1].grounded);
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
        save_size != (size_t)915)
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
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS)
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

static int run_ssbm_damage_source_test(const pf_m4_content *content)
{
    const pf_m4_ssbm_damage_response_attributes *source =
        pf_m4_ssbm_common_reference_damage_response();
    const uint32_t *raw_words;
    uint16_t raw_word_count = UINT16_C(0);
    int32_t full_x = INT32_C(6839);
    int32_t full_y = INT32_C(0);
    int32_t half_x = full_x;
    int32_t half_y = full_y;
    int32_t down_x = full_x;
    int32_t down_y = full_y;
    int32_t parallel_x = full_x;
    int32_t parallel_y = full_y;
    int32_t decayed_x = INT32_C(1179);
    int32_t decayed_y = -INT32_C(11369);
    int64_t full_vertical;
    int64_t half_vertical;

    raw_words = pf_m4_ssbm_common_reference_raw_words(&raw_word_count);
    if (source == NULL || raw_words == NULL ||
        raw_word_count != PF_M4_SSBM_COMMON_RAW_WORD_COUNT ||
        raw_words[0x154U / 4U] != UINT32_C(0x3ecccccd) ||
        raw_words[0x1A8U / 4U] != UINT32_C(0x41900000) ||
        raw_words[0x200U / 4U] != UINT32_C(0x3f800000) ||
        raw_words[0x204U / 4U] != UINT32_C(0x3d50e560) ||
        raw_words[0x4B0U / 4U] != UINT32_C(0x3f333333) ||
        raw_words[0x4B4U / 4U] != UINT32_C(4) ||
        raw_words[0x4B8U / 4U] != UINT32_C(0x40c00000) ||
        raw_words[0x4BCU / 4U] != UINT32_C(0x40400000) ||
        content->fighter.di_max_angle_radians_q30 !=
            source->di_max_angle_radians_q30 ||
        content->fighter.ground_knockback_decay_scale_q16 !=
            source->ground_knockback_decay_scale_q16 ||
        content->fighter.air_knockback_decay_q16 !=
            source->air_knockback_decay_q16 ||
        content->fighter.sdi_stick_threshold !=
            source->sdi_stick_threshold ||
        content->fighter.sdi_stick_window_ticks !=
            source->sdi_stick_window_ticks ||
        content->fighter.sdi_distance_x_q16 !=
            source->sdi_distance_x_q16 ||
        content->fighter.sdi_distance_y_q16 !=
            source->sdi_distance_y_q16 ||
        content->fighter.asdi_distance_x_q16 !=
            source->asdi_distance_x_q16 ||
        content->fighter.asdi_distance_y_q16 !=
            source->asdi_distance_y_q16)
    {
        return fail("ssbm-damage-source-data");
    }

    if (pf_m4_ssbm_stick_meets_radial_threshold(
            INT16_C(17000),
            INT16_C(17000),
            source->sdi_stick_threshold) == 0 ||
        pf_m4_ssbm_stick_meets_radial_threshold(
            INT16_C(22936),
            INT16_C(0),
            source->sdi_stick_threshold) != 0 ||
        pf_m4_ssbm_stick_meets_radial_threshold(
            INT16_C(22937),
            INT16_C(0),
            source->sdi_stick_threshold) == 0 ||
        pf_m4_ssbm_analog_displacement_q16(
            INT16_C(32767),
            source->sdi_distance_x_q16) !=
            source->sdi_distance_x_q16 ||
        pf_m4_ssbm_analog_displacement_q16(
            INT16_MIN,
            source->sdi_distance_y_q16) !=
            -source->sdi_distance_y_q16)
    {
        return fail("ssbm-damage-radial-and-analog-input");
    }

    if (!expect_status(
            pf_m4_ssbm_apply_di_q16(
                source->di_max_angle_radians_q30,
                INT16_C(0),
                INT16_C(-32767),
                &full_x,
                &full_y),
            PF_STATUS_OK,
            "ssbm-di-full") ||
        !expect_status(
            pf_m4_ssbm_apply_di_q16(
                source->di_max_angle_radians_q30,
                INT16_C(0),
                INT16_C(-16384),
                &half_x,
                &half_y),
            PF_STATUS_OK,
            "ssbm-di-half") ||
        !expect_status(
            pf_m4_ssbm_apply_di_q16(
                source->di_max_angle_radians_q30,
                INT16_C(0),
                INT16_C(32767),
                &down_x,
                &down_y),
            PF_STATUS_OK,
            "ssbm-di-down") ||
        !expect_status(
            pf_m4_ssbm_apply_di_q16(
                source->di_max_angle_radians_q30,
                INT16_C(32767),
                INT16_C(0),
                &parallel_x,
                &parallel_y),
            PF_STATUS_OK,
            "ssbm-di-parallel") ||
        !expect_status(
            pf_m4_ssbm_decay_air_knockback_q16(
                source->air_knockback_decay_q16,
                &decayed_x,
                &decayed_y),
            PF_STATUS_OK,
            "ssbm-air-knockback-decay"))
    {
        return 0;
    }

    full_vertical = full_y < INT32_C(0)
                        ? -(int64_t)full_y
                        : (int64_t)full_y;
    half_vertical = half_y < INT32_C(0)
                        ? -(int64_t)half_y
                        : (int64_t)half_y;
    if (full_x <= INT32_C(0) || full_y >= INT32_C(0) ||
        half_x <= INT32_C(0) || half_y >= INT32_C(0) ||
        down_x <= INT32_C(0) || down_y <= INT32_C(0) ||
        parallel_x != INT32_C(6839) || parallel_y != INT32_C(0) ||
        decayed_x != INT32_C(1118) ||
        decayed_y != -INT32_C(10785) ||
        full_vertical <= half_vertical * INT64_C(3) ||
        full_vertical >= half_vertical * INT64_C(5))
    {
        (void)fprintf(
            stderr,
            "m4-combat=detail operation=ssbm-di-squared-projection"
            " full=(%" PRId32 ",%" PRId32 ")"
            " half=(%" PRId32 ",%" PRId32 ")"
            " down=(%" PRId32 ",%" PRId32 ")"
            " parallel=(%" PRId32 ",%" PRId32 ")\n",
            full_x,
            full_y,
            half_x,
            half_y,
            down_x,
            down_y,
            parallel_x,
            parallel_y);
        return fail("ssbm-di-squared-projection");
    }
    return 1;
}

static int step_ssbm_damage_duel(
    pf_sim *sim,
    int16_t target_main_x,
    int16_t target_main_y,
    int16_t target_c_x,
    int16_t target_c_y,
    uint64_t attacker_buttons,
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_m4_inspection before;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "ssbm-damage-inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[0].buttons = attacker_buttons;
    inputs[1].main_stick_x = target_main_x;
    inputs[1].main_stick_y = target_main_y;
    inputs[1].secondary_stick_x = target_c_x;
    inputs[1].secondary_stick_y = target_c_y;
    return expect_status(
               pf_sim_tick(
                   sim,
                   inputs,
                   (size_t)2,
                   &test_last_result),
               PF_STATUS_OK,
               "ssbm-damage-tick") &&
           expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "ssbm-damage-inspect-after-step");
}

static int step_ssbm_trace_duel(
    pf_sim *sim,
    const pf_ssbm_stored_trace_input *target_input,
    pf_m4_inspection *out_inspection)
{
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_m4_inspection before;

    if (target_input == NULL ||
        !expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "ssbm-trace-inspect-before-step"))
    {
        return 0;
    }
    make_inputs(inputs, UINT8_C(2), before.tick);
    inputs[1].main_stick_x = target_input->main_stick_x;
    inputs[1].main_stick_y = target_input->main_stick_y;
    inputs[1].secondary_stick_x = target_input->secondary_stick_x;
    inputs[1].secondary_stick_y = target_input->secondary_stick_y;
    inputs[1].buttons = target_input->buttons;
    inputs[1].left_trigger = target_input->left_trigger;
    inputs[1].right_trigger = target_input->right_trigger;
    return expect_status(
               pf_sim_tick(
                   sim,
                   inputs,
                   (size_t)2,
                   &test_last_result),
               PF_STATUS_OK,
               "ssbm-trace-tick") &&
           expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "ssbm-trace-inspect-after-step");
}

static void capture_ssbm_stored_trace_sample(
    const pf_m4_player_inspection *player,
    int32_t origin_x_q16,
    int32_t origin_y_q16,
    pf_ssbm_stored_trace_sample *sample)
{
    sample->position_x_q16 = player->position_x_q16 - origin_x_q16;
    sample->position_y_q16 = player->position_y_q16 - origin_y_q16;
    sample->self_velocity_x_q16 = player->self_velocity_x_q16;
    sample->self_velocity_y_q16 = player->self_velocity_y_q16;
    sample->knockback_velocity_x_q16 =
        player->knockback_velocity_x_q16;
    sample->knockback_velocity_y_q16 =
        player->knockback_velocity_y_q16;
    sample->ground_knockback_velocity_q16 =
        player->ground_knockback_velocity_q16;
    sample->damage_q16 = player->damage_q16;
    sample->action_ticks = player->action_ticks;
    sample->hitlag_ticks = player->hitlag_ticks;
    sample->hitstun_ticks = player->hitstun_ticks;
    sample->action_state = player->action_state;
    sample->hitlag_resume_action = player->hitlag_resume_action;
    sample->grounded = player->grounded;
    sample->tumble = player->tumble;
    sample->invulnerable = player->invulnerable;
    sample->tech_direction = player->tech_direction;
    sample->prone_orientation = player->prone_orientation;
    sample->facing = player->facing;
    sample->ledge_regrab_lockout_ticks =
        player->ledge_regrab_lockout_ticks;
}

static int make_ssbm_falcon_punch_content(
    int airborne,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    if (!expect_status(
            pf_m4_default_content(out_content),
            PF_STATUS_OK,
            "ssbm-falcon-punch-default-content"))
    {
        return 0;
    }
    out_content->item.enabled = UINT8_C(0);
    out_content->projectile.enabled = UINT8_C(1);
    out_content->projectile.speed_q16 = INT32_C(1);
    out_content->projectile.lifetime_ticks = UINT16_C(1);
    out_content->reflector.enabled = UINT8_C(1);
    out_content->stage.blast_left_q16 =
        -INT32_C(160) * PF_Q16_ONE;
    out_content->stage.blast_right_q16 =
        INT32_C(160) * PF_Q16_ONE;
    out_content->stage.platform_center_x_q16 =
        -INT32_C(28) * PF_Q16_ONE;
    out_content->stage.platform_half_width_q16 = PF_Q16_ONE;
    out_content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    out_content->stage.solid_left_q16 =
        -INT32_C(22) * PF_Q16_ONE;
    out_content->stage.solid_right_q16 =
        -INT32_C(21) * PF_Q16_ONE;
    out_content->stage.solid_top_q16 =
        INT32_C(28) * PF_Q16_ONE;
    out_content->stage.solid_bottom_q16 =
        INT32_C(29) * PF_Q16_ONE;
    out_content->stage.upper_platform_center_x_q16 =
        -INT32_C(25) * PF_Q16_ONE;
    out_content->stage.upper_platform_half_width_q16 = PF_Q16_ONE;
    if (airborne == 0)
    {
        out_content->stage.floor_left_q16 =
            -INT32_C(128) * PF_Q16_ONE;
        out_content->stage.floor_right_q16 =
            INT32_C(128) * PF_Q16_ONE;
    }
    else
    {
        out_content->stage.spawn_spacing_q16 =
            INT32_C(10) * PF_Q16_ONE;
        out_content->stage.blast_bottom_q16 =
            INT32_C(2048) * PF_Q16_ONE;
    }
    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "ssbm-falcon-punch-content-view");
}

static int prepare_ssbm_falcon_punch_air(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint16_t tick;

    for (tick = UINT16_C(0); tick < UINT16_C(240); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MIN,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].grounded == UINT8_C(0) &&
            out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_AIRBORNE)
        {
            return 1;
        }
    }
    return 0;
}

static uint8_t run_ssbm_falcon_punch_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    int airborne;
    int tail;
    int32_t origin_x_q16;
    int32_t origin_y_q16;
    uint8_t sample_index;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL || capacity < stored_case->sample_count)
    {
        return UINT8_C(0);
    }
    airborne = strcmp(stored_case->id, "ground_complete") != 0;
    tail = strcmp(stored_case->id, "air_physics_tail") == 0;
    if ((!airborne && stored_case->initial_state_variant != UINT8_C(1)) ||
        (airborne && !tail &&
         stored_case->initial_state_variant != UINT8_C(2)) ||
        (tail && stored_case->initial_state_variant != UINT8_C(3)) ||
        (airborne && tail == 0 &&
         strcmp(stored_case->id, "air_complete_clock") != 0) ||
        !make_ssbm_falcon_punch_content(airborne, &content, &view) ||
        !initialize_sim_with_arena_extent(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            INT32_C(256) * PF_Q16_ONE,
            airborne != 0
                ? INT32_C(4096) * PF_Q16_ONE
                : INT32_C(0),
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "ssbm-falcon-punch-initial-inspect") ||
        (airborne && !prepare_ssbm_falcon_punch_air(sim, &inspection)))
    {
        return UINT8_C(0);
    }

    if (tail)
    {
        uint8_t pre_roll;

        if (!step_duel(
                sim,
                INT16_C(0),
                PF_INPUT_BUTTON_SPECIAL,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return UINT8_C(0);
        }
        for (pre_roll = UINT8_C(1); pre_roll < UINT8_C(49); ++pre_roll)
        {
            if (!step_duel(
                    sim,
                    INT16_C(0),
                    UINT64_C(0),
                    INT16_C(0),
                    UINT64_C(0),
                    &inspection))
            {
                return UINT8_C(0);
            }
        }
        if (inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_FALCON_PUNCH_AIR ||
            inspection.players[0].action_ticks != UINT16_C(49))
        {
            return UINT8_C(0);
        }
    }
    origin_x_q16 = inspection.players[0].position_x_q16;
    origin_y_q16 = inspection.players[0].position_y_q16;

    for (sample_index = UINT8_C(0);
         sample_index < stored_case->sample_count;
         ++sample_index)
    {
        const pf_ssbm_stored_trace_input *input =
            &stored_case->inputs[sample_index];
        pf_ssbm_stored_trace_sample *sample = &out_samples[sample_index];

        if (!step_duel(
                sim,
                input->main_stick_x,
                input->buttons,
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return UINT8_C(0);
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[0],
            origin_x_q16,
            origin_y_q16,
            sample);
        if (context != NULL && *(const int *)context != 0)
        {
            (void)printf(
                "m4-ssbm-falcon-punch-observation case=%s frame=%u"
                " action=%u action_tick=%u grounded=%u facing=%d"
                " x=%" PRId32 " y=%" PRId32 " vx=%" PRId32
                " vy=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (int)sample->facing,
                sample->position_x_q16,
                sample->position_y_q16,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16);
        }
    }
    return stored_case->sample_count;
}

static int run_ssbm_falcon_punch_observation_oracle(void)
{
    int print_samples = 0;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-neutral-special",
        pf_m4_ssbm_falcon_punch_cases,
        PF_M4_SSBM_FALCON_PUNCH_CASE_COUNT,
        PF_M4_SSBM_FALCON_PUNCH_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_PUNCH_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_PUNCH_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_PUNCH_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_falcon_punch_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s "
            "case=%s expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass "
        "domain=falcon-neutral-special poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_PUNCH_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_PUNCH_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_PUNCH_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static int step_ssbm_paired_trace_duel(
    pf_sim *sim,
    const pf_ssbm_stored_trace_input *lane_inputs,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection before;
    uint16_t advance_tick;

    if (lane_inputs == NULL || out_inspection == NULL ||
        lane_inputs[0].advance_ticks == UINT16_C(0) ||
        lane_inputs[0].advance_ticks != lane_inputs[1].advance_ticks ||
        !expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "ssbm-paired-trace-inspect-before-step"))
    {
        return 0;
    }
    for (advance_tick = UINT16_C(0);
         advance_tick < lane_inputs[0].advance_ticks;
         ++advance_tick)
    {
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;
        uint8_t lane;

        make_inputs(inputs, UINT8_C(2), before.tick);
        for (lane = UINT8_C(0); lane < UINT8_C(2); ++lane)
        {
            const pf_ssbm_stored_trace_input *source = &lane_inputs[lane];
            pf_input_frame *destination = &inputs[lane];

            destination->main_stick_x = source->main_stick_x;
            destination->main_stick_y = source->main_stick_y;
            destination->secondary_stick_x = source->secondary_stick_x;
            destination->secondary_stick_y = source->secondary_stick_y;
            destination->buttons = source->buttons;
            destination->left_trigger = source->left_trigger;
            destination->right_trigger = source->right_trigger;
        }
        if (!expect_status(
                pf_sim_tick(sim, inputs, (size_t)2, &result),
                PF_STATUS_OK,
                "ssbm-paired-trace-tick") ||
            !expect_status(
                pf_m4_inspect(sim, &before),
                PF_STATUS_OK,
                "ssbm-paired-trace-inspect-after-step"))
        {
            return 0;
        }
    }
    *out_inspection = before;
    return 1;
}

static uint8_t run_ssbm_player_push_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    int32_t origin_x[2] = {INT32_C(0), INT32_C(0)};
    int32_t origin_y[2] = {INT32_C(0), INT32_C(0)};
    uint8_t sample_index;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity < PF_M4_SSBM_FALCON_PLAYER_PUSH_SAMPLES_PER_CASE ||
        (strcmp(stored_case->id, "port_one_right") != 0 &&
         strcmp(stored_case->id, "port_two_left") != 0) ||
        !make_player_push_content(&content, &view) ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return UINT8_C(0);
    }

    for (sample_index = UINT8_C(0);
         sample_index < PF_M4_SSBM_FALCON_PLAYER_PUSH_SAMPLES_PER_CASE;
         ++sample_index)
    {
        uint8_t lane;

        if (!step_ssbm_paired_trace_duel(
                sim,
                &stored_case->inputs[
                    (size_t)sample_index *
                    PF_M4_SSBM_FALCON_PLAYER_PUSH_LANES_PER_SAMPLE],
                &inspection))
        {
            return UINT8_C(0);
        }
        if (sample_index == UINT8_C(0))
        {
            for (lane = UINT8_C(0); lane < UINT8_C(2); ++lane)
            {
                origin_x[lane] = inspection.players[lane].position_x_q16;
                origin_y[lane] = inspection.players[lane].position_y_q16;
            }
        }
        for (lane = UINT8_C(0); lane < UINT8_C(2); ++lane)
        {
            pf_ssbm_stored_trace_sample *sample =
                &out_samples[(size_t)sample_index *
                                 PF_M4_SSBM_FALCON_PLAYER_PUSH_LANES_PER_SAMPLE +
                             lane];

            capture_ssbm_stored_trace_sample(
                &inspection.players[lane],
                origin_x[lane],
                origin_y[lane],
                sample);
            if (context != NULL && *(const int *)context != 0)
            {
                (void)printf(
                    "m4-ssbm-player-push-observation case=%s sample=%u lane=%u"
                    " action=%u action_tick=%u facing=%d grounded=%u"
                    " dx=%" PRId32 " self_vx=%" PRId32 "\n",
                    stored_case->id,
                    (unsigned int)sample_index + 1U,
                    (unsigned int)lane,
                    (unsigned int)sample->action_state,
                    (unsigned int)sample->action_ticks,
                    (int)sample->facing,
                    (unsigned int)sample->grounded,
                    sample->position_x_q16,
                    sample->self_velocity_x_q16);
            }
        }
    }
    return PF_M4_SSBM_FALCON_PLAYER_PUSH_SAMPLES_PER_CASE;
}

static int run_ssbm_player_push_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-player-push",
        pf_m4_ssbm_falcon_player_push_cases,
        PF_M4_SSBM_FALCON_PLAYER_PUSH_CASE_COUNT,
        PF_M4_SSBM_FALCON_PLAYER_PUSH_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_PLAYER_PUSH_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_PLAYER_PUSH_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_PLAYER_PUSH_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_player_push_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s "
            "case=%s expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass "
        "domain=falcon-common-player-push poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_PLAYER_PUSH_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_PLAYER_PUSH_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_PLAYER_PUSH_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static uint8_t run_ssbm_damage_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    int32_t hit_x;
    int32_t hit_y;
    uint32_t tick;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity < PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SAMPLES_PER_CASE ||
        !make_combat_content(&content, &view) ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return UINT8_C(0);
    }

    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (!step_ssbm_damage_duel(
                sim,
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return UINT8_C(0);
        }
    }
    if (!step_ssbm_damage_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            &inspection) ||
        !step_ssbm_damage_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_ssbm_damage_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-damage-observation=fail case=%s operation=hit-setup action=%u\n",
            stored_case->id,
            (unsigned int)inspection.players[1].action_state);
        return UINT8_C(0);
    }
    hit_x = inspection.players[1].position_x_q16;
    hit_y = inspection.players[1].position_y_q16;
    for (tick = UINT32_C(0);
         tick <
             (uint32_t)
                 PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SAMPLES_PER_CASE;
         ++tick)
    {
        const pf_ssbm_stored_trace_input *input =
            &stored_case->inputs[tick];
        pf_ssbm_stored_trace_sample *sample = &out_samples[tick];

        if (!step_ssbm_damage_duel(
                sim,
                input->main_stick_x,
                input->main_stick_y,
                input->secondary_stick_x,
                input->secondary_stick_y,
                UINT64_C(0),
                &inspection))
        {
            return UINT8_C(0);
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            hit_x,
            hit_y,
            sample);
    }
    if (context != NULL && *(const int *)context != 0)
    {
        uint8_t sample_index;

        for (sample_index = UINT8_C(0);
             sample_index <
                 PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SAMPLES_PER_CASE;
             ++sample_index)
        {
            const pf_ssbm_stored_trace_sample *sample =
                &out_samples[sample_index];

            (void)printf(
                "m4-ssbm-damage-observation case=%s frame=%u"
                " hitlag=%u hitstun=%u dx=%" PRId32
                " dy=%" PRId32
                " self_vx=%" PRId32 " self_vy=%" PRId32
                " kb_vx=%" PRId32 " kb_vy=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->hitlag_ticks,
                (unsigned int)sample->hitstun_ticks,
                sample->position_x_q16,
                sample->position_y_q16,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16);
        }
    }
    return PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SAMPLES_PER_CASE;
}

static int run_ssbm_damage_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-damage-response",
        pf_m4_ssbm_falcon_damage_response_cases,
        PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_CASE_COUNT,
        PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_damage_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s "
            "case=%s expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass "
        "domain=falcon-common-damage-response poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_DAMAGE_RESPONSE_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static uint8_t run_ssbm_ground_knockback_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    test_sim_storage loaded_storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    pf_sim *loaded = NULL;
    const pf_sim_event *hit = NULL;
    int32_t hit_x;
    int32_t hit_y;
    uint32_t tick;
    uint8_t sample_index;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity < PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_SAMPLES_PER_CASE ||
        !expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "ssbm-ground-knockback-default-content"))
    {
        return UINT8_C(0);
    }
    /* X coordinates use the same 12/115 scale as the live comparison, so
     * this is the source route's exact 33-unit initial separation. */
    content.stage.spawn_spacing_q16 =
        (INT32_C(198) * PF_Q16_ONE) / INT32_C(115);
    content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    content.item.enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "ssbm-ground-knockback-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim))
    {
        return UINT8_C(0);
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
                UINT64_C(0),
                UINT16_C(0),
                &inspection))
        {
            return UINT8_C(0);
        }
    }
    for (tick = UINT32_C(0); tick < UINT32_C(4); ++tick)
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
                &inspection))
        {
            return UINT8_C(0);
        }
    }
    if (!step_reaction_duel(
            sim,
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
        return UINT8_C(0);
    }
    for (tick = UINT32_C(0); tick < UINT32_C(50); ++tick)
    {
        hit = find_last_tick_event(PF_SIM_EVENT_HIT);
        if (hit != NULL)
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
            return UINT8_C(0);
        }
    }
    if (hit == NULL || hit->source_player != UINT8_C(0) ||
        hit->target_player != UINT8_C(1) ||
        hit->detail != (uint16_t)PF_M4_ACTION_DASH_ATTACK ||
        inspection.players[1].damage_q16 !=
            UINT32_C(7) * UINT32_C(65536) ||
        inspection.players[1].grounded == UINT8_C(0) ||
        inspection.players[1].hitlag_ticks != UINT16_C(5) ||
        inspection.players[1].hitstun_ticks != UINT16_C(8) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].hitlag_resume_action !=
            (uint8_t)PF_M4_ACTION_DAMAGE_LOW_1)
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-ground-knockback=fail operation=route"
            " hit=%d attacker_action=%u attacker_tick=%u"
            " target_action=%u resume=%u damage=%u hitlag=%u hitstun=%u\n",
            hit != NULL,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].hitlag_resume_action,
            (unsigned int)(inspection.players[1].damage_q16 / UINT32_C(65536)),
            (unsigned int)inspection.players[1].hitlag_ticks,
            (unsigned int)inspection.players[1].hitstun_ticks);
        return UINT8_C(0);
    }

    hit_x = inspection.players[1].position_x_q16;
    hit_y = inspection.players[1].position_y_q16;
    for (sample_index = UINT8_C(0);
         sample_index <
             PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_SAMPLES_PER_CASE;
         ++sample_index)
    {
        pf_ssbm_stored_trace_sample *sample =
            &out_samples[sample_index];

        if (sample_index != UINT8_C(0))
        {
            const pf_ssbm_stored_trace_input *input =
                &stored_case->inputs[sample_index];

            if (!step_ssbm_damage_duel(
                    sim,
                    input->main_stick_x,
                    input->main_stick_y,
                    input->secondary_stick_x,
                    input->secondary_stick_y,
                    UINT64_C(0),
                    &inspection))
            {
                return UINT8_C(0);
            }
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            hit_x,
            hit_y,
            sample);
        if (context != NULL && *(const int *)context != 0)
        {
            (void)printf(
                "m4-ssbm-ground-knockback-observation case=%s frame=%u"
                " action=%u resume=%u action_tick=%u grounded=%u tumble=%u"
                " damage=%u hitlag=%u hitstun=%u dx=%" PRId32
                " dy=%" PRId32 " self_vx=%" PRId32
                " self_vy=%" PRId32 " kb_vx=%" PRId32
                " kb_vy=%" PRId32 " ground_kb=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->hitlag_resume_action,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (unsigned int)sample->tumble,
                (unsigned int)(sample->damage_q16 / UINT32_C(65536)),
                (unsigned int)sample->hitlag_ticks,
                (unsigned int)sample->hitstun_ticks,
                sample->position_x_q16,
                sample->position_y_q16,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16,
                sample->ground_knockback_velocity_q16);
        }
        if (sample_index == UINT8_C(5))
        {
            pf_m4_inspection loaded_inspection;

            if (!clone_sim_through_canonical_save(
                    sim,
                    &view,
                    &loaded_storage,
                    &loaded) ||
                !expect_status(
                    pf_m4_inspect(loaded, &loaded_inspection),
                    PF_STATUS_OK,
                    "ground-knockback-loaded-inspection") ||
                loaded_inspection.players[1]
                        .ground_knockback_velocity_q16 !=
                    sample->ground_knockback_velocity_q16 ||
                loaded_inspection.players[1].action_state !=
                    sample->action_state ||
                loaded_inspection.players[1].action_ticks !=
                    sample->action_ticks)
            {
                return UINT8_C(0);
            }
            sim = loaded;
        }
    }
    return PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_SAMPLES_PER_CASE;
}

static int run_ssbm_ground_knockback_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-ground-knockback",
        pf_m4_ssbm_falcon_ground_knockback_cases,
        PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_CASE_COUNT,
        PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_ground_knockback_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s "
            "case=%s expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass "
        "domain=falcon-common-ground-knockback poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_GROUND_KNOCKBACK_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static uint8_t run_ssbm_surface_response_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    uint8_t expected_action;
    int ceiling_fixture = 0;
    int arm_tech = 0;
    int16_t impact_x = INT16_C(0);
    int16_t impact_y = INT16_C(0);
    uint8_t sample_index;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity < PF_M4_SSBM_FALCON_SURFACE_RESPONSE_SAMPLES_PER_CASE)
    {
        return UINT8_C(0);
    }
    if (strcmp(stored_case->id, "right_pillar_wall_bounce") == 0)
    {
        expected_action = (uint8_t)PF_M4_ACTION_WALL_BOUNCE;
    }
    else if (strcmp(stored_case->id, "right_pillar_wall_tech") == 0)
    {
        expected_action = (uint8_t)PF_M4_ACTION_WALL_TECH;
        arm_tech = 1;
    }
    else if (strcmp(stored_case->id, "right_pillar_wall_tech_jump") == 0)
    {
        expected_action = (uint8_t)PF_M4_ACTION_WALL_TECH_JUMP;
        impact_y = INT16_C(-32767);
        arm_tech = 1;
    }
    else if (strcmp(stored_case->id, "cave_ceiling_bounce") == 0)
    {
        expected_action = (uint8_t)PF_M4_ACTION_CEILING_BOUNCE;
        ceiling_fixture = 1;
    }
    else if (strcmp(stored_case->id, "cave_ceiling_tech_drift") == 0)
    {
        expected_action = (uint8_t)PF_M4_ACTION_CEILING_TECH;
        ceiling_fixture = 1;
        arm_tech = 1;
    }
    else
    {
        return UINT8_C(0);
    }
    if (!make_surface_tech_content(ceiling_fixture, &content, &view) ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !drive_strong_to_surface(
            sim,
            arm_tech,
            impact_x,
            impact_y,
            UINT64_C(0),
            expected_action,
            &inspection))
    {
        return UINT8_C(0);
    }

    for (sample_index = UINT8_C(0);
         sample_index <
             PF_M4_SSBM_FALCON_SURFACE_RESPONSE_SAMPLES_PER_CASE;
         ++sample_index)
    {
        pf_ssbm_stored_trace_sample *sample =
            &out_samples[sample_index];

        if (sample_index != UINT8_C(0))
        {
            const pf_ssbm_stored_trace_input *input =
                &stored_case->inputs[sample_index];

            if (!step_ssbm_damage_duel(
                    sim,
                    input->main_stick_x,
                    input->main_stick_y,
                    input->secondary_stick_x,
                    input->secondary_stick_y,
                    UINT64_C(0),
                    &inspection))
            {
                return UINT8_C(0);
            }
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            sample);
        if (context != NULL && *(const int *)context != 0)
        {
            (void)printf(
                "m4-ssbm-surface-response-observation case=%s frame=%u"
                " action=%u action_tick=%u grounded=%u tumble=%u"
                " hitstun=%u invulnerable=%u self_vx=%" PRId32
                " self_vy=%" PRId32 " kb_vx=%" PRId32
                " kb_vy=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (unsigned int)sample->tumble,
                (unsigned int)sample->hitstun_ticks,
                (unsigned int)inspection.players[1].invulnerable,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16);
        }
    }
    return PF_M4_SSBM_FALCON_SURFACE_RESPONSE_SAMPLES_PER_CASE;
}

static int run_ssbm_surface_response_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-surface-response",
        pf_m4_ssbm_falcon_surface_response_cases,
        PF_M4_SSBM_FALCON_SURFACE_RESPONSE_CASE_COUNT,
        PF_M4_SSBM_FALCON_SURFACE_RESPONSE_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_SURFACE_RESPONSE_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_SURFACE_RESPONSE_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_SURFACE_RESPONSE_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_surface_response_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s "
            "case=%s expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass "
        "domain=falcon-common-surface-response poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_SURFACE_RESPONSE_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_SURFACE_RESPONSE_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_SURFACE_RESPONSE_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
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
    int64_t neutral_source_x;
    int64_t neutral_source_y;
    int64_t di_source_x;
    int64_t di_source_y;

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

    neutral_source_x =
        (int64_t)neutral_inspection.players[1].velocity_x_q16 *
        INT64_C(115) / INT64_C(12);
    neutral_source_y =
        (int64_t)neutral_inspection.players[1].velocity_y_q16 *
        INT64_C(62) / INT64_C(11);
    di_source_x =
        (int64_t)di_inspection.players[1].velocity_x_q16 *
        INT64_C(115) / INT64_C(12);
    di_source_y =
        (int64_t)di_inspection.players[1].velocity_y_q16 *
        INT64_C(62) / INT64_C(11);
    neutral_speed_squared =
        neutral_source_x * neutral_source_x +
        neutral_source_y * neutral_source_y;
    di_speed_squared =
        di_source_x * di_source_x + di_source_y * di_source_y;
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
        /* DI preserves magnitude in Melee coordinates. X and Y are scaled
         * differently when represented in the simulation world. */
        speed_difference > neutral_speed_squared / INT64_C(50))
    {
        (void)fprintf(
            stderr,
            "m4-combat=detail operation=trajectory-di-angle-and-magnitude"
            " neutral=(%" PRId32 ",%" PRId32 ")"
            " di=(%" PRId32 ",%" PRId32 ")"
            " neutral_source_speed_sq=%" PRId64
            " di_source_speed_sq=%" PRId64
            " difference=%" PRId64 "\n",
            neutral_inspection.players[1].velocity_x_q16,
            neutral_inspection.players[1].velocity_y_q16,
            di_inspection.players[1].velocity_x_q16,
            di_inspection.players[1].velocity_y_q16,
            neutral_speed_squared,
            di_speed_squared,
            speed_difference);
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
                    INT32_C(2) * PF_Q16_ONE >=
                INT32_C(32) * PF_Q16_ONE;
        const int roll_direction_input =
            tech_mode > 1 &&
            target->action_state != (uint8_t)PF_M4_ACTION_HITLAG;
        const int16_t target_x =
            roll_direction_input != 0
                ? (tech_mode == 3
                       ? INT16_C(-32767)
                       : INT16_C(32767))
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
    (void)fprintf(
        stderr,
        "m4-combat=diagnostic reaction-landing mode=%d action=%u ticks=%u"
        " position=(%d,%d) velocity=(%d,%d) trigger=%d tech=%u/%u\n",
        tech_mode,
        (unsigned int)out_inspection->players[1].action_state,
        (unsigned int)out_inspection->players[1].action_ticks,
        out_inspection->players[1].position_x_q16,
        out_inspection->players[1].position_y_q16,
        out_inspection->players[1].velocity_x_q16,
        out_inspection->players[1].velocity_y_q16,
        trigger_sent,
        (unsigned int)out_inspection->players[1].tech_window_ticks,
        (unsigned int)out_inspection->players[1].tech_lockout_ticks);
    return 0;
}

static int advance_knockdown_to_down_wait(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    uint16_t knockdown_tick;

    if (out_inspection->players[1].action_state !=
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

static int advance_missed_tech_to_down_wait(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    return run_until_reaction_landing(sim, 0, out_inspection) &&
           advance_knockdown_to_down_wait(
               content,
               sim,
               out_inspection);
}

static int prepare_floor_knockdown_orientation(
    const pf_m4_content *content,
    pf_sim *sim,
    uint8_t prone_orientation,
    pf_m4_inspection *out_inspection)
{
    (void)content;
    if ((prone_orientation != (uint8_t)PF_M4_PRONE_BACK &&
         prone_orientation != (uint8_t)PF_M4_PRONE_STOMACH) ||
        !run_until_reaction_landing(sim, 0, out_inspection))
    {
        return 0;
    }

    /* The source initial-state variant is selected from HipN's live matrix,
     * not from launch direction.  This compact fixture deliberately starts
     * at the manifest-owned DownBoundU/DownBoundD boundary so the response
     * oracle does not claim that its synthetic jab reproduces that pose. */
    sim->world.prone_orientation[1] = prone_orientation;
    return expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "inspect-prepared-floor-orientation") &&
           out_inspection->players[1].prone_orientation ==
               prone_orientation;
}

static int prepare_floor_orientation(
    const pf_m4_content *content,
    pf_sim *sim,
    uint8_t prone_orientation,
    pf_m4_inspection *out_inspection)
{
    return prepare_floor_knockdown_orientation(
               content,
               sim,
               prone_orientation,
               out_inspection) &&
           advance_knockdown_to_down_wait(
               content,
               sim,
               out_inspection);
}

static int run_exact_getup_roll_route(
    const pf_m4_content *content,
    pf_sim *sim,
    uint8_t prone_orientation,
    int8_t direction,
    const pf_m4_getup_roll_timing *timing,
    pf_m4_inspection *out_inspection)
{
    uint16_t tick;
    uint16_t action_frame = UINT16_C(1);
    uint16_t submotion_index;
    int32_t translation_x_q16;

    if (!step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            direction < INT8_C(0) ? INT16_C(-32767) : INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection) ||
        (submotion_index = pf_m4_getup_roll_submotion_for(
             prone_orientation,
             direction,
             out_inspection->players[1].facing)) == UINT16_MAX ||
        !pf_m4_falcon_reference_translation_q16(
            submotion_index,
            action_frame,
            &translation_x_q16,
            NULL) ||
        out_inspection->players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        out_inspection->players[1].action_ticks != UINT16_C(0) ||
        out_inspection->players[1].tech_direction != direction ||
        out_inspection->players[1].prone_orientation !=
            prone_orientation ||
        out_inspection->players[1].velocity_x_q16 !=
            (int32_t)out_inspection->players[1].facing *
                translation_x_q16 ||
        out_inspection->players[1].invulnerable !=
            (action_frame >= timing->invulnerability_begin_tick &&
                     action_frame <= timing->invulnerability_end_tick
                 ? UINT8_C(1)
                 : UINT8_C(0)))
    {
        return 0;
    }

    for (tick = UINT16_C(1);
         tick <= content->fighter.getup_roll_ticks;
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
        if (tick < content->fighter.getup_roll_ticks)
        {
            action_frame = (uint16_t)(tick + UINT16_C(1));
            if (!pf_m4_falcon_reference_translation_q16(
                    submotion_index,
                    action_frame,
                    &translation_x_q16,
                    NULL) ||
                out_inspection->players[1].action_state !=
                    (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
                out_inspection->players[1].action_ticks != tick ||
                out_inspection->players[1].prone_orientation !=
                    prone_orientation ||
                out_inspection->players[1].velocity_x_q16 !=
                    (int32_t)out_inspection->players[1].facing *
                        translation_x_q16 ||
                out_inspection->players[1].invulnerable !=
                    (action_frame >=
                                 timing->invulnerability_begin_tick &&
                             action_frame <=
                                 timing->invulnerability_end_tick
                         ? UINT8_C(1)
                         : UINT8_C(0)))
            {
                return 0;
            }
        }
    }
    return out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
           out_inspection->players[1].action_ticks == UINT16_C(0) &&
           out_inspection->players[1].tech_direction == INT8_C(0) &&
           out_inspection->players[1].prone_orientation ==
               (uint8_t)PF_M4_PRONE_NONE &&
           out_inspection->players[1].velocity_x_q16 == INT32_C(0) &&
           out_inspection->players[1].invulnerable == UINT8_C(0);
}

static int run_prone_getup_roll_timing_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage[4];
    pf_sim *sim[4] = {NULL, NULL, NULL, NULL};
    pf_m4_inspection inspection[4];
    const uint8_t orientations[4] = {
        (uint8_t)PF_M4_PRONE_BACK,
        (uint8_t)PF_M4_PRONE_BACK,
        (uint8_t)PF_M4_PRONE_STOMACH,
        (uint8_t)PF_M4_PRONE_STOMACH};
    const int8_t directions[4] = {
        INT8_C(-1), INT8_C(1), INT8_C(1), INT8_C(-1)};
    const pf_m4_getup_roll_timing *timings[4] = {
        &content->fighter.getup_roll_back_forward,
        &content->fighter.getup_roll_back_backward,
        &content->fighter.getup_roll_stomach_forward,
        &content->fighter.getup_roll_stomach_backward};
    uint32_t route;

    for (route = UINT32_C(0); route < UINT32_C(2); ++route)
    {
        if (!initialize_sim(
                &storage[route],
                view,
                UINT8_C(2),
                PF_SIM_MODE_DUEL,
                1,
                &sim[route]) ||
            !prepare_floor_orientation(
                content,
                sim[route],
                orientations[route],
                &inspection[route]) ||
            !run_exact_getup_roll_route(
                content,
                sim[route],
                orientations[route],
                directions[route],
                timings[route],
                &inspection[route]))
        {
            (void)fprintf(
                stderr,
                "m4-combat=debug prone-route=%u action=%u ticks=%u "
                "facing=%d orientation=%u direction=%d velocity=%d "
                "invulnerable=%u\n",
                (unsigned int)route,
                (unsigned int)inspection[route].players[1].action_state,
                (unsigned int)inspection[route].players[1].action_ticks,
                (int)inspection[route].players[1].facing,
                (unsigned int)
                    inspection[route].players[1].prone_orientation,
                (int)inspection[route].players[1].tech_direction,
                inspection[route].players[1].velocity_x_q16,
                (unsigned int)inspection[route].players[1].invulnerable);
            return fail("prone-getup-roll-route");
        }
    }
    return 1;
}

static int run_prone_getup_roll_observation_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_rl_action actions[PF_SIM_MAX_PLAYERS];
    pf_rl_transition transition;
    const uint16_t player_bits_index =
        (uint16_t)(PF_RL_COMPACT_PLAYER_BASE(UINT32_C(1)) +
                   UINT16_C(6));
    uint32_t player_index;
    uint32_t player_bits;

    (void)memset(actions, 0, sizeof(actions));
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        actions[player_index].schema_version =
            PF_RL_ACTION_SCHEMA_VERSION;
    }
    actions[1].main_stick_x = INT16_C(32767);

    if (!initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_floor_orientation(
            content,
            sim,
            (uint8_t)PF_M4_PRONE_BACK,
            &inspection) ||
        !expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "prone-getup-roll-rl-step") ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "prone-getup-roll-rl-inspect") ||
        transition.schema_version != PF_RL_TRANSITION_SCHEMA_VERSION ||
        transition.structured_observation.schema_version !=
            PF_SIM_OBSERVATION_SCHEMA_VERSION ||
        transition.compact_observation.schema_version !=
            PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION ||
        transition.structured_observation.players[1].prone_orientation !=
            (uint8_t)PF_M4_PRONE_BACK ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL)
    {
        return fail("prone-getup-roll-observation");
    }
    player_bits = (uint32_t)transition.compact_observation.values[
        player_bits_index];
    return ((player_bits >> 19U) & UINT32_C(3)) ==
                   (uint32_t)PF_M4_PRONE_BACK
               ? 1
               : fail("prone-getup-roll-compact-observation");
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
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic tech-results missed=%u/%u/%u"
            " in_place=%u/%d/%u/%u/%u roll=%u/%d/%d/%u/%u\n",
            (unsigned int)missed_inspection.players[1].action_state,
            (unsigned int)missed_inspection.players[1].grounded,
            (unsigned int)missed_inspection.players[1].invulnerable,
            (unsigned int)in_place_inspection.players[1].action_state,
            (int)in_place_inspection.players[1].tech_direction,
            (unsigned int)in_place_inspection.players[1].tech_window_ticks,
            (unsigned int)in_place_inspection.players[1].tech_lockout_ticks,
            (unsigned int)in_place_inspection.players[1].invulnerable,
            (unsigned int)roll_inspection.players[1].action_state,
            (int)roll_inspection.players[1].tech_direction,
            roll_inspection.players[1].velocity_x_q16,
            (unsigned int)roll_inspection.players[1].invulnerable,
            (unsigned int)roll_inspection.players[1].tumble);
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

static uint8_t run_ssbm_floor_response_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    int tech_mode;
    uint16_t turn_tick;
    uint8_t sample_index;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity < PF_M4_SSBM_FALCON_FLOOR_RESPONSE_SAMPLES_PER_CASE)
    {
        return UINT8_C(0);
    }
    if (strcmp(stored_case->id, "flat_floor_missed_tech") == 0)
    {
        tech_mode = 0;
    }
    else if (strcmp(stored_case->id, "flat_floor_neutral_tech") == 0)
    {
        tech_mode = 1;
    }
    else if (strcmp(stored_case->id, "flat_floor_forward_tech") == 0)
    {
        tech_mode = 2;
    }
    else if (strcmp(stored_case->id, "flat_floor_backward_tech") == 0)
    {
        tech_mode = 3;
    }
    else
    {
        return UINT8_C(0);
    }
    if (!make_reaction_content(&content, &view))
    {
        return UINT8_C(0);
    }
    /* Keep active hitstun at the landing boundary so this trace proves the
     * source field survives every floor response instead of expiring during
     * the compact fixture's flight. */
    content.fighter.jab_base_knockback_x_q16 =
        (INT32_C(9) * PF_Q16_ONE) / INT32_C(10);
    content.fighter.jab_base_knockback_y_q16 =
        PF_Q16_ONE / INT32_C(10);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "floor-response-content-view") ||
        !initialize_sim(
            &storage,
            &view,
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
            INT16_C(13500),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return UINT8_C(0);
    }
    for (turn_tick = UINT16_C(1);
         turn_tick < content.fighter.standing_turn_facing_tick;
         ++turn_tick)
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
            return UINT8_C(0);
        }
    }
    /* The pinned Dolphin route places Falcon (the target) to the left of the
     * opponent.  The compact reaction fixture attacks with player zero, so
     * mirror its spawn order while preserving the same target/player lane.
     * This matters because DamageFly turns the victim toward the attacker and
     * PassiveStand chooses F/B from stick direction relative to that facing. */
    {
        const int32_t attacker_x = sim->world.position_x_q16[0];

        sim->world.position_x_q16[0] = sim->world.position_x_q16[1];
        sim->world.position_x_q16[1] = attacker_x;
        sim->world.facing[0] = INT8_C(-1);
        sim->world.facing[1] = INT8_C(1);
    }
    if (inspection.players[1].facing != INT8_C(1) ||
        !run_until_reaction_landing(sim, tech_mode, &inspection))
    {
        return UINT8_C(0);
    }

    for (sample_index = UINT8_C(0);
         sample_index < PF_M4_SSBM_FALCON_FLOOR_RESPONSE_SAMPLES_PER_CASE;
         ++sample_index)
    {
        pf_ssbm_stored_trace_sample *sample = &out_samples[sample_index];

        if (sample_index != UINT8_C(0))
        {
            const pf_ssbm_stored_trace_input *input =
                &stored_case->inputs[sample_index];

            if (!step_ssbm_damage_duel(
                    sim,
                    input->main_stick_x,
                    input->main_stick_y,
                    input->secondary_stick_x,
                    input->secondary_stick_y,
                    UINT64_C(0),
                    &inspection))
            {
                return UINT8_C(0);
            }
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            sample);
        if (context != NULL && *(const int *)context != 0)
        {
            (void)printf(
                "m4-ssbm-floor-response-observation case=%s frame=%u"
                " action=%u action_tick=%u grounded=%u tumble=%u"
                " hitstun=%u invulnerable=%u facing=%d tech_direction=%d"
                " self_vx=%" PRId32
                " self_vy=%" PRId32 " kb_vx=%" PRId32
                " kb_vy=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (unsigned int)sample->tumble,
                (unsigned int)sample->hitstun_ticks,
                (unsigned int)inspection.players[1].invulnerable,
                (int)inspection.players[1].facing,
                (int)inspection.players[1].tech_direction,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16);
        }
    }
    return PF_M4_SSBM_FALCON_FLOOR_RESPONSE_SAMPLES_PER_CASE;
}

static int run_ssbm_floor_response_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-floor-response",
        pf_m4_ssbm_falcon_floor_response_cases,
        PF_M4_SSBM_FALCON_FLOOR_RESPONSE_CASE_COUNT,
        PF_M4_SSBM_FALCON_FLOOR_RESPONSE_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_FLOOR_RESPONSE_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_FLOOR_RESPONSE_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_FLOOR_RESPONSE_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_floor_response_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s case=%s "
            "expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL ? result.failed_operation : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass domain=falcon-common-floor-response "
        "poses=0 cases=%u samples=%u source_trace_sha256=%s "
        "production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_FLOOR_RESPONSE_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_FLOOR_RESPONSE_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_FLOOR_RESPONSE_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static uint8_t run_ssbm_prone_response_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    uint8_t sample_index;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity < PF_M4_SSBM_FALCON_PRONE_RESPONSE_SAMPLES_PER_CASE ||
        !make_reaction_content(&content, &view) ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_floor_knockdown_orientation(
            &content,
            sim,
            stored_case->initial_state_variant,
            &inspection))
    {
        return UINT8_C(0);
    }
    if (stored_case->initial_facing != INT8_C(0))
    {
        sim->world.facing[1] = stored_case->initial_facing;
    }

    for (sample_index = UINT8_C(0);
         sample_index < PF_M4_SSBM_FALCON_PRONE_RESPONSE_SAMPLES_PER_CASE;
         ++sample_index)
    {
        const pf_ssbm_stored_trace_input *input =
            &stored_case->inputs[sample_index];
        uint16_t tick;

        for (tick = UINT16_C(0); tick < input->advance_ticks; ++tick)
        {
            if (!step_ssbm_trace_duel(sim, input, &inspection))
            {
                return UINT8_C(0);
            }
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            &out_samples[sample_index]);
        if (context != NULL && *(const int *)context != 0)
        {
            const pf_ssbm_stored_trace_sample *sample =
                &out_samples[sample_index];

            (void)printf(
                "m4-ssbm-prone-response-observation case=%s sample=%u"
                " action=%u action_tick=%u grounded=%u"
                " hitstun_memory=%u invulnerable=%u"
                " self_vx=%" PRId32 " self_vy=%" PRId32
                " tech_direction=%d prone_orientation=%u\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                sample->hitstun_ticks != UINT16_C(0) ? 1U : 0U,
                (unsigned int)sample->invulnerable,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                (int)sample->tech_direction,
                (unsigned int)sample->prone_orientation);
        }
    }
    return PF_M4_SSBM_FALCON_PRONE_RESPONSE_SAMPLES_PER_CASE;
}

static int run_ssbm_prone_response_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-prone-response",
        pf_m4_ssbm_falcon_prone_response_cases,
        PF_M4_SSBM_FALCON_PRONE_RESPONSE_CASE_COUNT,
        PF_M4_SSBM_FALCON_PRONE_RESPONSE_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_PRONE_RESPONSE_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_PRONE_RESPONSE_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_PRONE_RESPONSE_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_prone_response_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s case=%s "
            "expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL ? result.failed_operation : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass domain=falcon-common-prone-response "
        "poses=0 cases=%u samples=%u source_trace_sha256=%s "
        "production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_PRONE_RESPONSE_CASE_COUNT,
        (unsigned int)PF_M4_SSBM_FALCON_PRONE_RESPONSE_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_PRONE_RESPONSE_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static int32_t ssbm_source_x_hundredths_to_sim_q16(int32_t value)
{
    const int64_t numerator =
        (int64_t)value * INT64_C(12) * (int64_t)PF_Q16_ONE;
    const int64_t denominator = INT64_C(11500);

    return numerator < INT64_C(0)
               ? (int32_t)(
                     -((-numerator + denominator / INT64_C(2)) /
                       denominator))
               : (int32_t)(
                     (numerator + denominator / INT64_C(2)) /
                     denominator);
}

static int32_t ssbm_source_y_hundredths_to_sim_q16(int32_t value)
{
    const int64_t numerator =
        (int64_t)value * INT64_C(11) * (int64_t)PF_Q16_ONE;
    const int64_t denominator = INT64_C(6200);
    const int64_t scaled =
        numerator < INT64_C(0)
            ? -((-numerator + denominator / INT64_C(2)) / denominator)
            : (numerator + denominator / INT64_C(2)) / denominator;

    return INT32_C(20) * PF_Q16_ONE - (int32_t)scaled;
}

static int32_t ssbm_source_velocity_q16_to_sim_q16(
    int32_t value_q16,
    int32_t numerator,
    int32_t denominator)
{
    const int64_t product = (int64_t)value_q16 * (int64_t)numerator;
    const int64_t magnitude =
        product < INT64_C(0) ? -product : product;
    const int64_t scaled =
        (magnitude + (int64_t)denominator / INT64_C(2)) /
        (int64_t)denominator;

    return product < INT64_C(0) ? -(int32_t)scaled : (int32_t)scaled;
}

static int prepare_battlefield_surface_response(
    pf_sim *sim,
    uint8_t setup_variant)
{
    const uint32_t player_index = UINT32_C(1);
    int32_t source_x_hundredths;
    int32_t source_y_hundredths;

    if (sim == NULL || setup_variant < UINT8_C(1) ||
        setup_variant > UINT8_C(2))
    {
        return 0;
    }
    if (setup_variant == UINT8_C(1))
    {
        source_x_hundredths = INT32_C(5000);
        source_y_hundredths = -INT32_C(3000);
    }
    else
    {
        source_x_hundredths = INT32_C(4100);
        source_y_hundredths = -INT32_C(3200);
    }

    sim->world.position_x_q16[player_index] =
        ssbm_source_x_hundredths_to_sim_q16(source_x_hundredths);
    sim->world.position_y_q16[player_index] =
        ssbm_source_y_hundredths_to_sim_q16(source_y_hundredths);
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.knockback_velocity_x_q16[player_index] =
        ssbm_source_velocity_q16_to_sim_q16(
            -INT32_C(274154),
            INT32_C(12),
            INT32_C(115));
    sim->world.knockback_velocity_y_q16[player_index] =
        -ssbm_source_velocity_q16_to_sim_q16(
            INT32_C(264747),
            INT32_C(11),
            INT32_C(62));
    sim->world.ground_knockback_velocity_q16[player_index] = INT32_C(0);
    sim->world.action_state[player_index] =
        (uint8_t)PF_M4_ACTION_HITSTUN;
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.source_submotion[player_index] = UINT16_C(0);
    sim->world.grounded[player_index] = UINT8_C(0);
    sim->world.support[player_index] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.fast_fall[player_index] = UINT8_C(0);
    sim->world.damage_q16[player_index] =
        UINT32_C(210) * (uint32_t)PF_Q16_ONE;
    sim->world.hitlag_ticks[player_index] = UINT16_C(0);
    sim->world.hitstun_ticks[player_index] = UINT16_C(77);
    sim->world.tumble[player_index] = UINT8_C(1);
    sim->world.tech_window_ticks[player_index] = UINT16_C(0);
    sim->world.tech_lockout_ticks[player_index] = UINT16_C(0);
    return 1;
}

static uint8_t run_ssbm_battlefield_surface_response_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    uint8_t expected_action;
    uint8_t sample_index;
    int32_t origin_x_q16 = INT32_C(0);
    int32_t origin_y_q16 = INT32_C(0);

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL || stored_case->sample_count == UINT8_C(0) ||
        capacity < stored_case->sample_count)
    {
        return UINT8_C(0);
    }
    expected_action =
        stored_case->initial_state_variant == UINT8_C(1)
            ? (uint8_t)PF_M4_ACTION_CEILING_BOUNCE
            : (uint8_t)PF_M4_ACTION_WALL_BOUNCE;
    if (!expect_status(
            pf_m4_reference_stage_content(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                &content),
            PF_STATUS_OK,
            "battlefield-surface-response-content") ||
        !expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "battlefield-surface-response-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_battlefield_surface_response(
            sim,
            stored_case->initial_state_variant) ||
        !step_ssbm_trace_duel(sim, &stored_case->inputs[0], &inspection))
    {
        return UINT8_C(0);
    }
    if (inspection.players[1].action_state != expected_action ||
        inspection.players[1].action_ticks != UINT16_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-battlefield-surface-response=detail case=%s "
            "expected_action=%u action=%u action_tick=%u x=%" PRId32
            " y=%" PRId32 " kb_vx=%" PRId32 " kb_vy=%" PRId32 "\n",
            stored_case->id,
            (unsigned int)expected_action,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16,
            inspection.players[1].knockback_velocity_x_q16,
            inspection.players[1].knockback_velocity_y_q16);
        return UINT8_C(0);
    }

    for (sample_index = UINT8_C(0);
         sample_index < stored_case->sample_count;
         ++sample_index)
    {
        pf_ssbm_stored_trace_sample *sample = &out_samples[sample_index];

        if (!step_ssbm_trace_duel(
                sim,
                &stored_case->inputs[sample_index],
                &inspection))
        {
            return UINT8_C(0);
        }
        if (sample_index == UINT8_C(0))
        {
            origin_x_q16 = inspection.players[1].position_x_q16;
            origin_y_q16 = inspection.players[1].position_y_q16;
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            origin_x_q16,
            origin_y_q16,
            sample);
        if (context != NULL && *(const int *)context != 0)
        {
            (void)printf(
                "m4-ssbm-battlefield-surface-response-observation "
                "case=%s frame=%u action=%u action_tick=%u grounded=%u "
                "tumble=%u hitstun=%u invulnerable=%u self_vx=%" PRId32
                " self_vy=%" PRId32 " kb_vx=%" PRId32 " kb_vy=%" PRId32
                " position_x=%" PRId32 " position_y=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (unsigned int)sample->tumble,
                (unsigned int)sample->hitstun_ticks,
                (unsigned int)inspection.players[1].invulnerable,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16,
                sample->position_x_q16,
                sample->position_y_q16);
        }
    }
    return stored_case->sample_count;
}

static int run_ssbm_battlefield_surface_response_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-battlefield-surface-response",
        pf_m4_ssbm_falcon_battlefield_surface_response_cases,
        PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_CASE_COUNT,
        PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_battlefield_surface_response_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s case=%s "
            "expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass domain=%s poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        domain.name,
        (unsigned int)
            PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_CASE_COUNT,
        (unsigned int)
            PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_BATTLEFIELD_SURFACE_RESPONSE_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static int prepare_battlefield_bounce_recontact(
    pf_sim *sim,
    const pf_ssbm_stored_trace_case *stored_case)
{
    const uint32_t player_index = UINT32_C(1);
    uint8_t action_state;

    if (sim == NULL || stored_case == NULL ||
        (stored_case->initial_state_variant != UINT8_C(1) &&
         stored_case->initial_state_variant != UINT8_C(2)) ||
        (stored_case->initial_facing != INT8_C(-1) &&
         stored_case->initial_facing != INT8_C(1)))
    {
        return 0;
    }
    action_state =
        stored_case->initial_state_variant == UINT8_C(1)
            ? (uint8_t)PF_M4_ACTION_CEILING_BOUNCE
            : (uint8_t)PF_M4_ACTION_WALL_BOUNCE;

    sim->world.position_x_q16[player_index] =
        ssbm_source_x_hundredths_to_sim_q16(INT32_C(0));
    sim->world.position_y_q16[player_index] =
        ssbm_source_y_hundredths_to_sim_q16(INT32_C(18000)) -
        sim->content.fighter.half_height_q16;
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.knockback_velocity_x_q16[player_index] = INT32_C(0);
    sim->world.knockback_velocity_y_q16[player_index] = INT32_C(0);
    sim->world.ground_knockback_velocity_q16[player_index] = INT32_C(0);
    sim->world.action_state[player_index] = action_state;
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.source_submotion[player_index] = UINT16_C(0);
    sim->world.grounded[player_index] = UINT8_C(0);
    sim->world.support[player_index] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.fast_fall[player_index] = UINT8_C(0);
    sim->world.facing[player_index] = stored_case->initial_facing;
    sim->world.damage_q16[player_index] =
        UINT32_C(200) * (uint32_t)PF_Q16_ONE;
    sim->world.hitlag_ticks[player_index] = UINT16_C(0);
    sim->world.hitstun_ticks[player_index] = UINT16_C(76);
    sim->world.tumble[player_index] = UINT8_C(1);
    sim->world.tech_window_ticks[player_index] = UINT16_C(0);
    sim->world.tech_lockout_ticks[player_index] = UINT16_C(0);
    return 1;
}

static uint8_t run_ssbm_battlefield_bounce_recontact_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    uint8_t sample_index;
    int32_t origin_x_q16 = INT32_C(0);
    int32_t origin_y_q16 = INT32_C(0);

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL || stored_case->sample_count == UINT8_C(0) ||
        capacity < stored_case->sample_count)
    {
        return UINT8_C(0);
    }
    if (!expect_status(
            pf_m4_reference_stage_content(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                &content),
            PF_STATUS_OK,
            "battlefield-bounce-recontact-content") ||
        !expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "battlefield-bounce-recontact-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_battlefield_bounce_recontact(sim, stored_case))
    {
        return UINT8_C(0);
    }

    for (sample_index = UINT8_C(0);
         sample_index < stored_case->sample_count;
         ++sample_index)
    {
        pf_ssbm_stored_trace_sample *sample = &out_samples[sample_index];

        if (!step_ssbm_trace_duel(
                sim,
                &stored_case->inputs[sample_index],
                &inspection))
        {
            return UINT8_C(0);
        }
        if (sample_index == UINT8_C(0))
        {
            origin_x_q16 = inspection.players[1].position_x_q16;
            origin_y_q16 = inspection.players[1].position_y_q16;
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            origin_x_q16,
            origin_y_q16,
            sample);
        if (context != NULL && *(const int *)context != 0)
        {
            (void)printf(
                "m4-ssbm-battlefield-bounce-recontact-observation "
                "case=%s frame=%u action=%u action_tick=%u grounded=%u "
                "support=%u tumble=%u hitstun=%u invulnerable=%u "
                "facing=%d self_vx=%" PRId32 " self_vy=%" PRId32
                " kb_vx=%" PRId32 " kb_vy=%" PRId32
                " position_x=%" PRId32 " position_y=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (unsigned int)inspection.players[1].support,
                (unsigned int)sample->tumble,
                (unsigned int)sample->hitstun_ticks,
                (unsigned int)sample->invulnerable,
                (int)sample->facing,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16,
                sample->position_x_q16,
                sample->position_y_q16);
        }
    }
    return stored_case->sample_count;
}

static int run_ssbm_battlefield_bounce_recontact_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-battlefield-bounce-recontact",
        pf_m4_ssbm_falcon_battlefield_bounce_recontact_cases,
        PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_CASE_COUNT,
        PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_battlefield_bounce_recontact_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s case=%s "
            "expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass domain=%s poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        domain.name,
        (unsigned int)
            PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_CASE_COUNT,
        (unsigned int)
            PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_BATTLEFIELD_BOUNCE_RECONTACT_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static int make_hyrule_response_content(
    uint16_t spawn_line,
    int32_t spawn_x_q16,
    int32_t spawn_spacing_q16,
    pf_m4_content *out_content,
    pf_content_view *out_view)
{
    const pf_m4_ssbm_stage_collision_profile *profile =
        pf_m4_ssbm_reference_stage_collision(
            (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE);
    const pf_m4_ssbm_stage_collision_line *left_ledge;
    const pf_m4_ssbm_stage_collision_line *right_ledge;
    pf_m4_stage_data *stage;

    if (profile == NULL || profile->line_count <= UINT16_C(37) ||
        !make_reaction_content(out_content, out_view))
    {
        return 0;
    }
    left_ledge = &profile->lines[0];
    right_ledge = &profile->lines[37];
    stage = &out_content->stage;
    stage->reference_collision_profile =
        (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE;
    stage->reference_spawn_line = spawn_line;
    stage->reference_spawn_x_q16 = spawn_x_q16;
    stage->spawn_spacing_q16 = spawn_spacing_q16;

    /* The current compact stage contract still exposes one outer ledge pair.
     * Bind it to Hyrule's actual enabled outer floor endpoints while all floor
     * contact and tangent projection comes from the imported line catalog. */
    stage->floor_left_q16 =
        left_ledge->start_x_q16 < left_ledge->end_x_q16
            ? left_ledge->start_x_q16
            : left_ledge->end_x_q16;
    stage->floor_right_q16 =
        right_ledge->start_x_q16 > right_ledge->end_x_q16
            ? right_ledge->start_x_q16
            : right_ledge->end_x_q16;
    stage->floor_y_q16 = pf_m4_ssbm_stage_line_y_q16(
        right_ledge,
        stage->floor_right_q16);

    /* Authored primitives are inert for imported collision, but remain valid
     * deterministic render/debug data until the public stage packet itself is
     * table-backed. */
    stage->blast_left_q16 = stage->floor_left_q16 -
                            INT32_C(8) * PF_Q16_ONE;
    stage->blast_right_q16 = stage->floor_right_q16 +
                             INT32_C(8) * PF_Q16_ONE;
    stage->blast_top_q16 = INT32_C(0);
    stage->blast_bottom_q16 = INT32_C(60) * PF_Q16_ONE;
    stage->platform_center_x_q16 = INT32_C(0);
    stage->platform_y_q16 = INT32_C(4) * PF_Q16_ONE;
    stage->platform_half_width_q16 = PF_Q16_ONE;
    stage->platform_motion_amplitude_q16 = INT32_C(0);
    stage->upper_platform_center_x_q16 = INT32_C(6) * PF_Q16_ONE;
    stage->upper_platform_y_q16 = INT32_C(6) * PF_Q16_ONE;
    stage->upper_platform_half_width_q16 = PF_Q16_ONE;
    stage->solid_left_q16 = -INT32_C(15) * PF_Q16_ONE;
    stage->solid_right_q16 = -INT32_C(13) * PF_Q16_ONE;
    stage->solid_top_q16 = INT32_C(8) * PF_Q16_ONE;
    stage->solid_bottom_q16 = INT32_C(20) * PF_Q16_ONE;
    stage->revival_platform_start_y_q16 =
        PF_Q16_ONE;
    stage->revival_platform_end_y_q16 =
        INT32_C(7) * PF_Q16_ONE;
    stage->revival_platform_half_width_q16 =
        INT32_C(2) * PF_Q16_ONE;

    return expect_status(
        pf_m4_make_content_view(out_content, out_view),
        PF_STATUS_OK,
        "hyrule-response-content-view");
}

static int place_player_on_reference_floor(
    pf_sim *sim,
    uint32_t player_index,
    uint16_t line_index,
    int32_t position_x_q16,
    int8_t facing)
{
    const pf_m4_ssbm_stage_collision_profile *profile =
        pf_m4_ssbm_reference_stage_collision(
            sim->content.stage.reference_collision_profile);
    const pf_m4_ssbm_stage_collision_line *line;
    int32_t left;
    int32_t right;

    if (profile == NULL || player_index >= sim->world.player_count ||
        line_index >= profile->line_count)
    {
        return 0;
    }
    line = &profile->lines[line_index];
    left = line->start_x_q16 < line->end_x_q16
               ? line->start_x_q16
               : line->end_x_q16;
    right = line->start_x_q16 > line->end_x_q16
                ? line->start_x_q16
                : line->end_x_q16;
    if (line->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
        position_x_q16 < left || position_x_q16 > right)
    {
        return 0;
    }
    sim->world.position_x_q16[player_index] = position_x_q16;
    sim->world.position_y_q16[player_index] =
        pf_m4_ssbm_stage_line_y_q16(line, position_x_q16) -
        sim->content.fighter.half_height_q16;
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.knockback_velocity_x_q16[player_index] = INT32_C(0);
    sim->world.knockback_velocity_y_q16[player_index] = INT32_C(0);
    sim->world.ground_knockback_velocity_q16[player_index] = INT32_C(0);
    sim->world.action_state[player_index] =
        (uint8_t)PF_M4_ACTION_GROUND_IDLE;
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.grounded[player_index] = UINT8_C(1);
    sim->world.support[player_index] =
        (uint8_t)(line_index + UINT16_C(1));
    sim->world.facing[player_index] = facing;
    return 1;
}

static int prepare_hyrule_slope_roll(
    const pf_m4_content *content,
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    const int32_t target_x_q16 =
        ssbm_source_x_hundredths_to_sim_q16(INT32_C(7560));
    const int32_t attacker_x_q16 =
        ssbm_source_x_hundredths_to_sim_q16(INT32_C(6760));

    if (!place_player_on_reference_floor(
            sim,
            UINT32_C(1),
            UINT16_C(34),
            target_x_q16,
            INT8_C(-1)) ||
        !place_player_on_reference_floor(
            sim,
            UINT32_C(0),
            UINT16_C(33),
            attacker_x_q16,
            INT8_C(1)))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-slope-ledge-response=fail operation=slope-placement\n");
        return 0;
    }
    if (!prepare_floor_knockdown_orientation(
            content,
            sim,
            (uint8_t)PF_M4_PRONE_STOMACH,
            out_inspection))
    {
        (void)pf_m4_inspect(sim, out_inspection);
        (void)fprintf(
            stderr,
            "m4-ssbm-slope-ledge-response=fail operation=slope-setup"
            " action=%u tick=%u support=%u facing=%d prone=%u x=%" PRId32
            " y=%" PRId32 " grounded=%u\n",
            (unsigned int)out_inspection->players[1].action_state,
            (unsigned int)out_inspection->players[1].action_ticks,
            (unsigned int)out_inspection->players[1].support,
            (int)out_inspection->players[1].facing,
            (unsigned int)out_inspection->players[1].prone_orientation,
            out_inspection->players[1].position_x_q16,
            out_inspection->players[1].position_y_q16,
            (unsigned int)out_inspection->players[1].grounded);
        return 0;
    }
    return 1;
}

static int prepare_hyrule_ledge_departure(
    pf_sim *sim,
    pf_m4_inspection *out_inspection)
{
    sim->world.damage_q16[1] = UINT32_C(50) * UINT32_C(65536);
    if (!start_reaction_hit(sim, out_inspection))
    {
        return 0;
    }
    return out_inspection->players[1].action_state ==
               (uint8_t)PF_M4_ACTION_HITLAG &&
           out_inspection->players[1].hitlag_resume_action ==
               (uint8_t)PF_M4_ACTION_HITSTUN &&
           out_inspection->players[1].facing == INT8_C(-1);
}

static uint8_t run_ssbm_slope_ledge_response_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    int slope_case;
    int ledge_case;
    uint8_t sample_index;
    int32_t origin_x_q16;
    int32_t origin_y_q16;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL ||
        capacity <
            PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_SAMPLES_PER_CASE)
    {
        return UINT8_C(0);
    }
    slope_case = strcmp(
        stored_case->id,
        "hyrule_line34_forward_getup_roll") == 0;
    ledge_case =
        strcmp(
            stored_case->id,
            "hyrule_line36_to_line37_natural_hit_departure") == 0 ||
        strcmp(
            stored_case->id,
            "ledge_grab_down_threshold_accept") == 0 ||
        strcmp(
            stored_case->id,
            "ledge_grab_down_threshold_reject") == 0;
    if (slope_case == 0 && ledge_case == 0)
    {
        return UINT8_C(0);
    }

    if (slope_case != 0)
    {
        const pf_m4_ssbm_stage_collision_line *line =
            pf_m4_ssbm_reference_stage_line(
                (uint16_t)PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE,
                UINT8_C(35));

        if (line == NULL ||
            !make_hyrule_response_content(
                UINT16_C(34),
                (line->start_x_q16 + line->end_x_q16) / INT32_C(2),
                PF_Q16_ONE / INT32_C(3),
                &content,
                &view))
        {
            return UINT8_C(0);
        }
        content.fighter.jab_hitbox_half_height_q16 = PF_Q16_ONE;
        content.fighter.jab_base_knockback_x_q16 =
            PF_Q16_ONE / INT32_C(100);
        content.fighter.jab_base_knockback_y_q16 =
            PF_Q16_ONE / INT32_C(5);
        content.fighter.jab_knockback_growth_q16 = INT32_C(1);
        content.fighter.hitstun_velocity_per_tick_q16 = INT32_C(515);
        content.fighter.tumble_hitstun_threshold_ticks = UINT16_C(13);
    }
    else
    {
        const int32_t target_x_q16 =
            ssbm_source_x_hundredths_to_sim_q16(INT32_C(19000));
        const int32_t spacing_q16 =
            (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);

        if (!make_hyrule_response_content(
                UINT16_C(36),
                target_x_q16 - spacing_q16,
                spacing_q16,
                &content,
                &view))
        {
            return UINT8_C(0);
        }
        content.fighter.jab_damage_q16 =
            UINT32_C(10) * UINT32_C(65536);
        content.fighter.jab_hitlag_ticks = UINT16_C(4);
        content.fighter.jab_base_knockback_x_q16 = INT32_C(12140);
        content.fighter.jab_base_knockback_y_q16 = INT32_C(20002);
        content.fighter.reference_frame_data_enabled = UINT8_C(1);
        content.fighter.jab_knockback_growth_q16 = INT32_C(1);
        content.fighter.hitstun_velocity_per_tick_q16 = INT32_C(977);
    }
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "slope-ledge-response-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        (slope_case != 0
             ? !prepare_hyrule_slope_roll(
                   &content,
                   sim,
                   &inspection)
             : !prepare_hyrule_ledge_departure(sim, &inspection)))
    {
        return UINT8_C(0);
    }
    if (stored_case->initial_facing != INT8_C(0))
    {
        sim->world.facing[1] = stored_case->initial_facing;
        if (!expect_status(
                pf_m4_inspect(sim, &inspection),
                PF_STATUS_OK,
                "inspect-slope-ledge-initial-facing"))
        {
            return UINT8_C(0);
        }
    }

    origin_x_q16 = inspection.players[1].position_x_q16;
    origin_y_q16 = inspection.players[1].position_y_q16;
    for (sample_index = UINT8_C(0);
         sample_index <
             PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_SAMPLES_PER_CASE;
         ++sample_index)
    {
        const pf_ssbm_stored_trace_input *input =
            &stored_case->inputs[sample_index];
        uint16_t tick;

        if (slope_case != 0 || sample_index != UINT8_C(0))
        {
            for (tick = UINT16_C(0); tick < input->advance_ticks; ++tick)
            {
                if (!step_ssbm_trace_duel(sim, input, &inspection))
                {
                    return UINT8_C(0);
                }
            }
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            origin_x_q16,
            origin_y_q16,
            &out_samples[sample_index]);
        if (context != NULL && *(const int *)context != 0)
        {
            const pf_ssbm_stored_trace_sample *sample =
                &out_samples[sample_index];

            (void)printf(
                "m4-ssbm-slope-ledge-response-observation case=%s"
                " sample=%u action=%u resume=%u action_tick=%u"
                " grounded=%u support=%u tumble=%u hitlag=%u hitstun=%u"
                " invulnerable=%u tech=%d prone=%u facing=%d dx=%" PRId32
                " dy=%" PRId32 " self_vx=%" PRId32
                " self_vy=%" PRId32 " kb_vx=%" PRId32
                " kb_vy=%" PRId32 " ground_kb=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->hitlag_resume_action,
                (unsigned int)sample->action_ticks,
                (unsigned int)sample->grounded,
                (unsigned int)inspection.players[1].support,
                (unsigned int)sample->tumble,
                (unsigned int)sample->hitlag_ticks,
                (unsigned int)sample->hitstun_ticks,
                (unsigned int)sample->invulnerable,
                (int)sample->tech_direction,
                (unsigned int)sample->prone_orientation,
                (int)sample->facing,
                sample->position_x_q16,
                sample->position_y_q16,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16,
                sample->knockback_velocity_x_q16,
                sample->knockback_velocity_y_q16,
                sample->ground_knockback_velocity_q16);
        }
    }
    return PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_SAMPLES_PER_CASE;
}

static int run_ssbm_slope_ledge_response_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-slope-ledge-response",
        pf_m4_ssbm_falcon_slope_ledge_response_cases,
        PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_CASE_COUNT,
        PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_slope_ledge_response_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s case=%s "
            "expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL ? result.failed_operation : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass "
        "domain=falcon-common-slope-ledge-response poses=0 cases=%u "
        "samples=%u source_trace_sha256=%s production_trace_sha256=%s\n",
        (unsigned int)PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_CASE_COUNT,
        (unsigned int)
            PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_SLOPE_LEDGE_RESPONSE_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static int prepare_hyrule_ledge_wait(
    pf_sim *sim,
    uint8_t setup_variant,
    pf_m4_inspection *out_inspection)
{
    const pf_m4_falcon_ledge_root_positions *roots =
        pf_m4_falcon_reference_ledge_root_positions();
    const pf_m4_ssbm_ledge_response_attributes *ledge_response =
        pf_m4_ssbm_common_reference_ledge_response();
    const uint32_t player_index = UINT32_C(1);

    if (roots == NULL || ledge_response == NULL ||
        setup_variant > UINT8_C(2))
    {
        return 0;
    }
    sim->world.position_x_q16[player_index] =
        sim->content.stage.floor_right_q16 -
        roots->wait_frame_one_x_q16;
    sim->world.position_y_q16[player_index] =
        sim->content.stage.floor_y_q16 +
        roots->wait_frame_one_y_q16 -
        sim->content.fighter.half_height_q16;
    sim->world.velocity_x_q16[player_index] = INT32_C(0);
    sim->world.velocity_y_q16[player_index] = INT32_C(0);
    sim->world.action_state[player_index] =
        (uint8_t)PF_M4_ACTION_LEDGE_HANG;
    sim->world.action_ticks[player_index] = UINT16_C(0);
    sim->world.source_submotion[player_index] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_WAIT;
    sim->world.grounded[player_index] = UINT8_C(0);
    sim->world.support[player_index] =
        (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.facing[player_index] = INT8_C(-1);
    sim->world.damage_q16[player_index] =
        (setup_variant == UINT8_C(1) ? UINT32_C(100) : UINT32_C(60)) *
        (uint32_t)PF_Q16_ONE;
    sim->world.ledge_invulnerability_ticks[player_index] =
        ledge_response->wait_invulnerability_ticks;
    sim->world.ledge_regrab_lockout_ticks[player_index] = UINT16_C(0);
    sim->world.previous_directional_input_flags[player_index] =
        setup_variant == UINT8_C(2)
            ? PF_M4_DIRECTIONAL_INPUT_LEDGE_READY
            : UINT8_C(0);
    return expect_status(
        pf_m4_inspect(sim, out_inspection),
        PF_STATUS_OK,
        "ledge-options-initial-inspect");
}

static int enable_reference_ledge_options(
    pf_m4_content *content,
    pf_content_view *view)
{
    content->fighter.reference_frame_data_enabled = UINT8_C(1);
    return expect_status(
        pf_m4_make_content_view(content, view),
        PF_STATUS_OK,
        "ledge-options-content-view");
}

static uint8_t run_ssbm_ledge_options_trace_case(
    void *context,
    const pf_ssbm_stored_trace_case *stored_case,
    pf_ssbm_stored_trace_sample *out_samples,
    uint8_t capacity)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    const int32_t spawn_spacing_q16 =
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(5);
    const int32_t spawn_x_q16 =
        ssbm_source_x_hundredths_to_sim_q16(INT32_C(19000)) -
        spawn_spacing_q16;
    const uint8_t sample_count = stored_case != NULL
                                     ? stored_case->sample_count
                                     : UINT8_C(0);
    uint8_t sample_index;
    int32_t origin_x_q16;
    int32_t origin_y_q16;

    if (stored_case == NULL || stored_case->inputs == NULL ||
        out_samples == NULL || sample_count == UINT8_C(0) ||
        capacity < sample_count ||
        !make_hyrule_response_content(
            UINT16_C(36),
            spawn_x_q16,
            spawn_spacing_q16,
            &content,
            &view) ||
        !enable_reference_ledge_options(&content, &view) ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_hyrule_ledge_wait(
            sim,
            stored_case->initial_state_variant,
            &inspection))
    {
        return UINT8_C(0);
    }

    /* Variant 2 represents the live-qualified post-CliffWait release window:
     * the neutral-ready latch is prepared above, then the case's declared
     * first controller sample drives the real release callback before the
     * first sparse cooldown observation. */
    if (stored_case->initial_state_variant == UINT8_C(2) &&
        !step_ssbm_trace_duel(sim, &stored_case->inputs[0], &inspection))
    {
        return UINT8_C(0);
    }

    origin_x_q16 = inspection.players[1].position_x_q16;
    origin_y_q16 = inspection.players[1].position_y_q16;
    if (context != NULL && *(const int *)context != 0)
    {
        const pf_m4_falcon_ledge_root_positions *roots =
            pf_m4_falcon_reference_ledge_root_positions();
        (void)printf(
            "m4-ssbm-ledge-options-meta case=%s x=%" PRId32
            " y=%" PRId32 " edge=%" PRId32 " floor=%" PRId32
            " root_x=%" PRId32 " root_y=%" PRId32
            " half_height=%" PRId32 " blast_top=%" PRId32 "\n",
            stored_case->id,
            origin_x_q16,
            origin_y_q16,
            sim->content.stage.floor_right_q16,
            sim->content.stage.floor_y_q16,
            roots != NULL ? roots->wait_frame_one_x_q16 : INT32_C(0),
            roots != NULL ? roots->wait_frame_one_y_q16 : INT32_C(0),
            sim->content.fighter.half_height_q16,
            sim->content.stage.blast_top_q16);
    }
    for (sample_index = UINT8_C(0);
         sample_index < sample_count;
         ++sample_index)
    {
        const pf_ssbm_stored_trace_input *input =
            &stored_case->inputs[sample_index];
        uint16_t advance_tick;

        if (sample_index != UINT8_C(0))
        {
            for (advance_tick = UINT16_C(0);
                 advance_tick < input->advance_ticks;
                 ++advance_tick)
            {
                if (!step_ssbm_trace_duel(sim, input, &inspection))
                {
                    return UINT8_C(0);
                }
            }
        }
        capture_ssbm_stored_trace_sample(
            &inspection.players[1],
            origin_x_q16,
            origin_y_q16,
            &out_samples[sample_index]);
        if (context != NULL && *(const int *)context != 0)
        {
            const pf_ssbm_stored_trace_sample *sample =
                &out_samples[sample_index];

            (void)printf(
                "m4-ssbm-ledge-options-observation case=%s sample=%u"
                " action=%u action_tick=%u facing=%d grounded=%u"
                " tumble=%u invulnerable=%u ledge_regrab_cooldown=%u ready=%u"
                " input_x=%d dx=%" PRId32 " dy=%" PRId32
                " self_vx=%" PRId32 " self_vy=%" PRId32 "\n",
                stored_case->id,
                (unsigned int)sample_index + 1U,
                (unsigned int)sample->action_state,
                (unsigned int)sample->action_ticks,
                (int)sample->facing,
                (unsigned int)sample->grounded,
                (unsigned int)sample->tumble,
                (unsigned int)sample->invulnerable,
                (unsigned int)sample->ledge_regrab_lockout_ticks,
                (unsigned int)(
                    sim->world.previous_directional_input_flags[1] &
                    PF_M4_DIRECTIONAL_INPUT_LEDGE_READY),
                (int)input->main_stick_x,
                sample->position_x_q16,
                sample->position_y_q16,
                sample->self_velocity_x_q16,
                sample->self_velocity_y_q16);
        }
    }
    return sample_count;
}

static int run_ssbm_ledge_options_observation_oracle(void)
{
    int print_samples = 1;
    const pf_ssbm_stored_trace_domain domain = {
        "falcon-common-ledge-options",
        pf_m4_ssbm_falcon_ledge_options_cases,
        PF_M4_SSBM_FALCON_LEDGE_OPTIONS_CASE_COUNT,
        PF_M4_SSBM_FALCON_LEDGE_OPTIONS_SAMPLES_PER_CASE,
        PF_M4_SSBM_FALCON_LEDGE_OPTIONS_LANES_PER_SAMPLE,
        PF_M4_SSBM_FALCON_LEDGE_OPTIONS_SERIALIZED_FIELDS,
        PF_M4_SSBM_FALCON_LEDGE_OPTIONS_PRODUCTION_TRACE_SHA256,
        &print_samples,
        run_ssbm_ledge_options_trace_case};
    pf_ssbm_stored_trace_result result;

    if (!pf_ssbm_stored_trace_oracle_run(&domain, &result))
    {
        (void)fprintf(
            stderr,
            "m4-ssbm-stored-oracle=fail domain=%s operation=%s case=%s "
            "expected_production_trace_sha256=%s "
            "actual_production_trace_sha256=%s\n",
            domain.name,
            result.failed_operation != NULL
                ? result.failed_operation
                : "unknown",
            result.failed_case != NULL ? result.failed_case : "none",
            domain.expected_production_trace_sha256,
            result.production_trace_sha256[0] != '\0'
                ? result.production_trace_sha256
                : "unavailable");
        return 0;
    }
    (void)printf(
        "m4-ssbm-stored-oracle=pass domain=%s poses=0 cases=%u samples=%u "
        "source_trace_sha256=%s production_trace_sha256=%s\n",
        domain.name,
        (unsigned int)domain.case_count,
        (unsigned int)PF_M4_SSBM_FALCON_LEDGE_OPTIONS_TOTAL_SAMPLE_COUNT,
        PF_M4_SSBM_FALCON_LEDGE_OPTIONS_SOURCE_TRACE_SHA256,
        result.production_trace_sha256);
    return 1;
}

static int16_t tech_chase_axis(
    const pf_m4_content *content,
    const pf_m4_inspection *inspection)
{
    const int32_t delta =
        inspection->players[1].position_x_q16 -
        inspection->players[0].position_x_q16;
    const int aged_walk =
        inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_WALK &&
        inspection->players[0].action_ticks >=
            content->fighter.dash_input_window_ticks;
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
            return INT16_MAX;
        }
        if (delta < INT32_C(0))
        {
            return -INT16_MAX;
        }
    }

    if (delta >
        (INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return aged_walk != 0 ? INT16_C(0) : INT16_MAX;
    }
    if (delta > PF_Q16_ONE / INT32_C(2))
    {
        return INT16_C(13500);
    }
    if (delta <
        -(INT32_C(3) * PF_Q16_ONE) / INT32_C(2))
    {
        return aged_walk != 0 ? INT16_C(0) : -INT16_MAX;
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
    const pf_m4_content *content,
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
                    INT32_C(2) * PF_Q16_ONE >=
                INT32_C(32) * PF_Q16_ONE;
        const int16_t target_x =
            tech_mode > 1 &&
                    (should_trigger || trigger_sent != 0)
                ? INT16_MAX
                : INT16_C(0);

        if (!step_reaction_duel(
                sim,
                tech_chase_axis(content, out_inspection),
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
    uint32_t initial_hit_sequence;
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
            content,
            1,
            &in_place_inspection) ||
        !run_until_tech_chase_landing(
            roll,
            content,
            2,
            &roll_inspection) ||
        !run_until_tech_chase_landing(
            miss,
            content,
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
        int16_t chaser_x =
            tech_chase_axis(content, &in_place_inspection);
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                UINT32_C(2)) ||
        in_place_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        in_place_inspection.players[1].last_hit_attacker !=
            UINT8_C(0) ||
        test_last_result.event_count != UINT8_C(2) ||
        test_last_result.events[0].type !=
            (uint16_t)PF_SIM_EVENT_HIT ||
        test_last_result.events[1].type !=
            (uint16_t)PF_SIM_EVENT_ACTION_TRANSITIONS)
    {
        return fail("tech-chase-in-place-punish");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_reaction_duel(
                roll,
                tech_chase_axis(content, &roll_inspection),
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
        save_size != (size_t)915)
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
        int16_t chaser_x =
            tech_chase_axis(content, &roll_inspection);
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                UINT32_C(2)) ||
        loaded_inspection.players[1].damage_q16 !=
            roll_inspection.players[1].damage_q16 ||
        roll_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return fail("tech-chase-roll-punish");
    }

    initial_damage = miss_inspection.players[1].damage_q16;
    initial_hit_sequence =
        miss_inspection.players[1].last_hit_sequence;
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
        miss_inspection.players[1].last_hit_sequence !=
            initial_hit_sequence)
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
        roll_inspection.players[1].prone_orientation !=
            (uint8_t)PF_M4_PRONE_BACK ||
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
        (void)fprintf(
            stderr,
            "floor-getup-input-routing-detail up=%u/%u shield=%u/%u "
            "roll=%u/%u/%d/%d attack=%u/%u/%u age=%u previous=%" PRIu64 "\n",
            (unsigned int)up_inspection.players[1].action_state,
            (unsigned int)up_inspection.players[1].action_ticks,
            (unsigned int)shield_inspection.players[1].action_state,
            (unsigned int)shield_inspection.players[1].action_ticks,
            (unsigned int)roll_inspection.players[1].action_state,
            (unsigned int)roll_inspection.players[1].action_ticks,
            (int)roll_inspection.players[1].tech_direction,
            (int)roll_inspection.players[1].self_velocity_x_q16,
            (unsigned int)attack_inspection.players[1].action_state,
            (unsigned int)attack_inspection.players[1].action_ticks,
            (unsigned int)attack_inspection.players[1].invulnerable,
            (unsigned int)attack->world.prone_attack_input_age[1],
            attack->world.previous_buttons[1]);
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
                               .getup_roll_back_forward
                               .invulnerability_end_tick
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
                          pf_m4_getup_attack_invulnerability_ticks_for(
                              &content->fighter,
                              attack_inspection.players[1]
                                  .prone_orientation)
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

    for (tick = UINT16_C(0);
         tick < UINT16_C(30) &&
         back_inspection.players[0].position_x_q16 <=
             back_inspection.players[1].position_x_q16 +
                 content->fighter.half_width_q16;
         ++tick)
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
        source_inspection.players[1].prone_orientation !=
            (uint8_t)PF_M4_PRONE_BACK ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-floor-recovery-save-size") ||
        save_size != (size_t)915)
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
            pf_m4_inspect(loaded, &loaded_inspection),
            PF_STATUS_OK,
            "inspect-loaded-floor-recovery") ||
        loaded_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GETUP_ROLL ||
        loaded_inspection.players[1].tech_direction != INT8_C(1) ||
        loaded_inspection.players[1].prone_orientation !=
            (uint8_t)PF_M4_PRONE_BACK ||
        loaded_inspection.players[1].self_velocity_x_q16 !=
            source_inspection.players[1].self_velocity_x_q16 ||
        loaded_inspection.players[1].invulnerable !=
            source_inspection.players[1].invulnerable ||
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                UINT32_C(2)) ||
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
    const pf_m4_content *content)
{
    test_sim_storage storage;
    pf_m4_content body_state_content = *content;
    pf_content_view body_state_view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    /* This test isolates the source body-state interval. Spatial fidelity has
     * its own exact-pose discriminator, so keep the authored overlapping box
     * here instead of making a particular SpotDodge pose a hidden precondition. */
    body_state_content.fighter.reference_frame_data_enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(
                &body_state_content,
                &body_state_view),
            PF_STATUS_OK,
            "spot-dodge-body-state-content-view") ||
        !initialize_sim(
            &storage,
            &body_state_view,
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
            UINT16_MAX,
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
            "m4-combat=diagnostic spot-begin action=%u ticks=%u"
            " invulnerable=%u damage=%u expected_begin=%u\n",
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16,
            (unsigned int)content->fighter
                .spot_dodge_invulnerability_begin_tick);
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
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic roll-invulnerability"
            " attacker_action=%u attacker_ticks=%u active=%u"
            " target_action=%u target_ticks=%u invulnerable=%u"
            " damage=%" PRIu32 " expected_begin=%u\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].invulnerable,
            inspection.players[1].damage_q16,
            (unsigned int)content->fighter.roll_invulnerability_begin_tick);
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
        save_size != (size_t)915)
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
        !step_reaction_duel(
            source,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            INT16_C(32767),
            INT16_C(0),
            UINT64_C(0),
            UINT16_MAX,
            &source_inspection) ||
        source_inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        source_inspection.players[1].sdi_pulse_count != UINT8_C(1) ||
        source_inspection.players[1].shield_stun_ticks ==
            UINT16_C(0) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "query-shield-save-size") ||
        save_size != (size_t)915)
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
            uint32_t player_index;

            (void)pf_m4_inspect(left, &inspection);
            (void)fprintf(
                stderr,
                "m4-combat=diagnostic deterministic-trace tick=%" PRIu64
                " world_status=%s\n",
                tick,
                pf_status_name(
                    pf_sim_snapshot_validate_world(&left->world)));
            for (player_index = UINT32_C(0);
                 player_index < PF_SIM_MAX_PLAYERS;
                 ++player_index)
            {
                (void)fprintf(
                    stderr,
                    " player=%" PRIu32 " action=%u resume=%u ticks=%u"
                    " hitlag=%u hitstun=%u grounded=%u shield=%" PRIu32
                    " dash=%d facing=%d tumble=%u tech=%d"
                    " velocity=(%" PRId32 ",%" PRId32 ") support=%u"
                    " recovery=%u target=%u owner=%u escape=%u\n",
                    player_index,
                    (unsigned int)inspection.players[player_index]
                        .action_state,
                    (unsigned int)inspection.players[player_index]
                        .hitlag_resume_action,
                    (unsigned int)inspection.players[player_index]
                        .action_ticks,
                    (unsigned int)inspection.players[player_index]
                        .hitlag_ticks,
                    (unsigned int)inspection.players[player_index]
                        .hitstun_ticks,
                    (unsigned int)inspection.players[player_index].grounded,
                    inspection.players[player_index].shield_health_q16,
                    (int)inspection.players[player_index].dash_direction,
                    (int)inspection.players[player_index].facing,
                    (unsigned int)inspection.players[player_index].tumble,
                    (int)inspection.players[player_index].tech_direction,
                    inspection.players[player_index].velocity_x_q16,
                    inspection.players[player_index].velocity_y_q16,
                    (unsigned int)inspection.players[player_index].support,
                    (unsigned int)inspection.players[player_index]
                        .recovery_available,
                    (unsigned int)inspection.players[player_index]
                        .grab_target,
                    (unsigned int)inspection.players[player_index]
                        .grab_owner,
                    (unsigned int)inspection.players[player_index]
                        .grab_escape_ticks);
                (void)fprintf(
                    stderr,
                    "  flags=%u source_motion=%u short_hop=%u"
                    " platform_drop=%u attack_mask=%u stale_registered=%u"
                    " kb=(%" PRId32 ",%" PRId32 ") ground_kb=%" PRId32
                    " damage_jump=%u shield_held=%u shield_strength=%u"
                    " powershield=%u sdi=(%d,%d) prone=%u/%u\n",
                    (unsigned int)left->world
                        .previous_directional_input_flags[player_index],
                    (unsigned int)left->world.source_submotion[player_index],
                    (unsigned int)left->world.short_hop_latched[player_index],
                    (unsigned int)left->world.platform_drop_ticks[player_index],
                    (unsigned int)inspection.players[player_index]
                        .attack_hit_mask,
                    (unsigned int)inspection.players[player_index]
                        .attack_stale_registered,
                    inspection.players[player_index]
                        .knockback_velocity_x_q16,
                    inspection.players[player_index]
                        .knockback_velocity_y_q16,
                    inspection.players[player_index]
                        .ground_knockback_velocity_q16,
                    (unsigned int)left->world
                        .damage_jump_buffer_ticks[player_index],
                    (unsigned int)left->world.shield_held[player_index],
                    (unsigned int)inspection.players[player_index]
                        .shield_strength,
                    (unsigned int)inspection.players[player_index].powershield,
                    (int)left->world.sdi_direction_x[player_index],
                    (int)left->world.sdi_direction_y[player_index],
                    (unsigned int)inspection.players[player_index]
                        .prone_orientation,
                    (unsigned int)left->world
                        .prone_roll_motion_orientation[player_index]);
            }
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
                save_size != (size_t)915)
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
        save_size != (size_t)915)
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
                expected_stale_damage_q16(
                    &content->fighter,
                    content->fighter.strong_damage_q16,
                    UINT16_C(2)))
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
            close_content->fighter.jab_startup_ticks +
                close_content->fighter.jab_active_ticks ||
        close_content->fighter.jab_combo_input_end_tick !=
            UINT16_C(7) ||
        close_content->fighter.jab_final_startup_ticks !=
            UINT16_C(3) ||
        close_content->fighter.jab_final_active_ticks !=
            UINT16_C(3) ||
        close_content->fighter.jab_final_recovery_ticks !=
            UINT16_C(13) ||
        close_content->fighter.jab_final_hitlag_ticks !=
            UINT16_C(4) ||
        close_content->fighter.jab_final_damage_q16 !=
            UINT32_C(3) * UINT32_C(65536) ||
        close_content->fighter.jab_final_melee_knockback.enabled !=
            UINT8_C(1) ||
        close_content->fighter.jab_final_melee_knockback.angle_degrees !=
            UINT16_C(80) ||
        close_content->fighter.jab_final_melee_knockback.growth !=
            UINT16_C(100) ||
        close_content->fighter.jab_final_melee_knockback.weight_set !=
            UINT16_C(20) ||
        close_content->fighter.jab_final_melee_knockback.base != UINT16_C(0))
    {
        return fail("jab-cancel-reference-defaults");
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
        inspection.players[0].action_ticks != UINT16_C(0) ||
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
        save_size != (size_t)915)
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
        save_size != (size_t)915)
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
        save_size != (size_t)915)
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
            (uint8_t)PF_M4_ACTION_DASH_GRAB ||
        inspection.players[0].action_ticks != UINT16_C(1))
    {
        return fail("direct-dash-grab-entry");
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
            (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
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
            (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
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
            (uint8_t)PF_M4_ACTION_UP_AERIAL ||
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
            (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "jump-cancel-query-save-size") ||
        save_size != (size_t)915)
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
             (uint32_t)content->fighter.up_strong_attack.startup_ticks +
                 (uint32_t)content->fighter.up_strong_attack.active_ticks +
                 (uint32_t)content->fighter.up_strong_attack.recovery_ticks +
                 (uint32_t)content->fighter.up_strong_attack.hitlag_ticks +
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
                    (uint16_t)PF_M4_ACTION_UP_STRONG_ATTACK)
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
            content->fighter.up_strong_attack.damage_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug jump_cancel_hit=%d action=%u damage=%" PRIu32
            " expected=%" PRIu32 "\n",
            hit_seen,
            (unsigned int)inspection.players[0].action_state,
            inspection.players[1].damage_q16,
            content->fighter.up_strong_attack.damage_q16);
        return fail("jump-cancel-production-hit");
    }
    return 1;
}

static int run_grab_damage_escape_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_sim_event grab_event;
    const uint16_t expected_escape_ticks = (uint16_t)(
        (uint32_t)content->fighter.grab_escape_base_ticks +
        (uint32_t)(((uint64_t)content->fighter.jab_damage_q16 *
                    (uint64_t)(uint32_t)content->fighter
                        .grab_escape_damage_ticks_q16) >>
                   32U));
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
            content->fighter.jab_damage_q16)
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
            content->fighter.jab_damage_q16 ||
        inspection.players[1].grab_escape_ticks != expected_escape_ticks)
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
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
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
                UINT64_C(0),
                INT16_C(0),
                inspection.players[2].action_state ==
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                inspection.players[2].action_state ==
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE
                    ? UINT16_MAX
                    : UINT16_C(0),
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
                UINT64_C(0),
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
            UINT64_C(0),
            UINT16_C(0),
            UINT64_C(0),
            INT16_MAX,
            PF_INPUT_BUTTON_ATTACK,
            UINT16_C(0),
            &inspection))
    {
        return fail("team-handoff-player0-ready");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(48); ++tick)
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
                inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE
                    ? PF_INPUT_BUTTON_ATTACK
                    : UINT64_C(0),
                inspection.players[0].action_state ==
                        (uint8_t)PF_M4_ACTION_GROUND_IDLE
                    ? UINT16_MAX
                    : UINT16_C(0),
                UINT64_C(0),
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
    const pf_m4_content *content,
    const pf_content_view *view,
    const pf_m4_throw_data *throw_data,
    uint16_t target_weight,
    int16_t stick_x,
    int16_t stick_y,
    pf_m4_action_state expected_action)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_sim_event grab_event;
    pf_m4_falcon_move_index move_index;
    const pf_m4_reference_hit_effect *collateral_effect;
    pf_m4_melee_knockback_result melee_result;
    uint32_t collateral_damage_q16;
    uint32_t resulting_damage_q16;
    int32_t expected_velocity_x;
    int32_t expected_velocity_y;
    int32_t expected_resumed_velocity_y;
    uint32_t tick;
    int throw_seen = 0;

    if (!pf_m4_falcon_reference_move_for_action(
            (uint8_t)expected_action,
            &move_index))
    {
        return fail("directional-throw-reference-move");
    }
    collateral_effect =
        pf_m4_falcon_reference_primary_effect(move_index);
    collateral_damage_q16 =
        collateral_effect != NULL
            ? (uint32_t)collateral_effect->damage * UINT32_C(65536)
            : UINT32_C(0);
    resulting_damage_q16 =
        collateral_damage_q16 + throw_data->damage_q16;
    melee_result =
        throw_data->melee_knockback.enabled != UINT8_C(0)
            ? pf_m4_melee_knockback(
                  &throw_data->melee_knockback,
                  target_weight,
                  throw_data->damage_q16,
                  resulting_damage_q16)
            : (pf_m4_melee_knockback_result){0};
    expected_velocity_x =
        throw_data->melee_knockback.enabled != UINT8_C(0)
            ? melee_result.velocity_x_q16
            : expected_throw_velocity(
                  throw_data->base_velocity_x_q16,
                  throw_data->velocity_growth_x_q16,
                  throw_data->damage_q16);
    expected_velocity_y =
        throw_data->melee_knockback.enabled != UINT8_C(0)
            ? -melee_result.velocity_y_q16
            : expected_throw_velocity(
                  throw_data->base_velocity_y_q16,
                  throw_data->velocity_growth_y_q16,
                  throw_data->damage_q16);
    expected_resumed_velocity_y =
        expected_velocity_y + content->fighter.gravity_q16 <
                content->fighter.fall_speed_q16
            ? expected_velocity_y + content->fighter.gravity_q16
            : content->fighter.fall_speed_q16;

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
            UINT64_C(0),
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
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic directional-throw-input"
            " stick=(%d,%d) expected=%u actual=%u ticks=%u"
            " grab_target=%u target_action=%u owner=%u\n",
            (int)stick_x,
            (int)stick_y,
            (unsigned int)expected_action,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].grab_target,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].grab_owner);
        return fail("directional-throw-input");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)throw_data->release_tick + UINT32_C(32);
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
        if (throw_event == NULL)
        {
            if ((inspection.players[0].action_state !=
                     (uint8_t)expected_action &&
                 inspection.players[0].action_state !=
                     (uint8_t)PF_M4_ACTION_HITLAG) ||
                inspection.players[0].grab_target != UINT8_C(1) ||
                inspection.players[1].grab_owner != UINT8_C(0) ||
                (inspection.players[1].action_state !=
                     (uint8_t)PF_M4_ACTION_GRABBED &&
                 inspection.players[1].action_state !=
                     (uint8_t)PF_M4_ACTION_HITLAG))
            {
                return fail("directional-throw-startup");
            }
            continue;
        }
        if (throw_event->source_player != UINT8_C(0) ||
                 throw_event->target_player != UINT8_C(1) ||
                 throw_event->value_q16 != throw_data->damage_q16 ||
                 throw_event->velocity_x_q16 != expected_velocity_x ||
                 throw_event->velocity_y_q16 != expected_velocity_y ||
                 throw_event->flags != UINT16_C(0) ||
                 throw_event->detail != (uint16_t)expected_action ||
                 inspection.players[0].action_state !=
                     (throw_data->hitlag_ticks != UINT16_C(0)
                          ? (uint8_t)PF_M4_ACTION_HITLAG
                          : (uint8_t)expected_action) ||
                 inspection.players[1].action_state !=
                     (throw_data->hitlag_ticks != UINT16_C(0)
                          ? (uint8_t)PF_M4_ACTION_HITLAG
                          : (uint8_t)PF_M4_ACTION_HITSTUN) ||
                 inspection.players[0].hitlag_ticks !=
                     throw_data->hitlag_ticks ||
                 inspection.players[1].hitlag_ticks !=
                     throw_data->hitlag_ticks ||
                 inspection.players[0].grab_target !=
                     PF_SIM_EVENT_NO_PLAYER ||
                 inspection.players[1].grab_owner !=
                     PF_SIM_EVENT_NO_PLAYER ||
                 inspection.players[1].damage_q16 !=
                     resulting_damage_q16 ||
                 inspection.players[1].last_hit_valid != UINT8_C(1) ||
                 inspection.players[1].last_hit_attacker != UINT8_C(0))
        {
            return fail("directional-throw-release");
        }
        throw_seen = 1;
        break;
    }
    if (throw_seen == 0)
    {
        return fail("directional-throw-release-timeout");
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
        inspection.players[0].action_ticks !=
            (uint16_t)(
                throw_data->release_tick +
                (throw_data->hitlag_ticks != UINT16_C(0)
                     ? UINT16_C(1)
                     : UINT16_C(0))) ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITSTUN ||
        inspection.players[1].velocity_x_q16 != expected_velocity_x ||
        inspection.players[1].velocity_y_q16 !=
            (throw_data->hitlag_ticks != UINT16_C(0)
                 ? expected_resumed_velocity_y
                 : expected_velocity_y) ||
        inspection.players[0].stale_move_count != UINT8_C(1) ||
        inspection.players[0].stale_move_ids[0] !=
            (uint8_t)expected_action)
    {
        return fail("directional-throw-hitstun-entry");
    }

    for (tick = UINT32_C(0);
         tick <
             (uint32_t)throw_data->recovery_ticks -
                 (throw_data->hitlag_ticks != UINT16_C(0)
                      ? UINT32_C(1)
                      : UINT32_C(0));
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
            (uint32_t)throw_data->recovery_ticks -
                (throw_data->hitlag_ticks != UINT16_C(0)
                     ? UINT32_C(1)
                     : UINT32_C(0)))
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
        content->fighter.pummel_hit_tick != UINT16_C(4) ||
        content->fighter.pummel_total_ticks != UINT16_C(23))
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
        save_size != (size_t)915)
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
         future_tick <
             (uint32_t)content->fighter.pummel_total_ticks +
                 (uint32_t)pf_m4_melee_hitlag_ticks(
                     content->fighter.pummel_damage_q16,
                     UINT8_C(0),
                     UINT32_C(65536)) -
                 UINT32_C(1);
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
            (void)fprintf(
                stderr,
                "m4-combat=diagnostic pummel-snapshot future_tick=%" PRIu32
                " source_action=%u source_ticks=%u target_action=%u"
                " target_ticks=%u target_damage=%" PRIu32 "\n",
                future_tick,
                (unsigned int)source_inspection.players[0].action_state,
                (unsigned int)source_inspection.players[0].action_ticks,
                (unsigned int)source_inspection.players[1].action_state,
                (unsigned int)source_inspection.players[1].action_ticks,
                source_inspection.players[1].damage_q16);
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
                (source_inspection.players[0].action_state !=
                     (uint8_t)PF_M4_ACTION_PUMMEL &&
                 !(source_inspection.players[0].action_state ==
                       (uint8_t)PF_M4_ACTION_HITLAG &&
                   source_inspection.players[0].hitlag_resume_action ==
                       (uint8_t)PF_M4_ACTION_PUMMEL)) ||
                source_inspection.players[0].action_ticks !=
                    content->fighter.pummel_hit_tick)
            {
                (void)fprintf(
                    stderr,
                    "m4-combat=diagnostic pummel-event"
                    " future_tick=%" PRIu32 " source=%u target=%u"
                    " value=%" PRIu32 " vx=%" PRId32 " vy=%" PRId32
                    " flags=%u detail=%u action=%u resume=%u ticks=%u"
                    " expected_hit_tick=%u stale_count=%u stale0=%u"
                    " stale_registered=%u\n",
                    future_tick,
                    (unsigned int)pummel_event->source_player,
                    (unsigned int)pummel_event->target_player,
                    pummel_event->value_q16,
                    pummel_event->velocity_x_q16,
                    pummel_event->velocity_y_q16,
                    (unsigned int)pummel_event->flags,
                    (unsigned int)pummel_event->detail,
                    (unsigned int)source_inspection.players[0].action_state,
                    (unsigned int)source_inspection.players[0]
                        .hitlag_resume_action,
                    (unsigned int)source_inspection.players[0].action_ticks,
                    (unsigned int)content->fighter.pummel_hit_tick,
                    (unsigned int)source_inspection.players[0]
                        .stale_move_count,
                    (unsigned int)source_inspection.players[0]
                        .stale_move_ids[0],
                    (unsigned int)source_inspection.players[0]
                        .attack_stale_registered);
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
            content->fighter.pummel_damage_q16 ||
        source_inspection.players[0].stale_move_count != UINT8_C(1) ||
        source_inspection.players[0].stale_move_ids[0] !=
            (uint8_t)PF_M4_ACTION_PUMMEL ||
        loaded_inspection.players[0].stale_move_count != UINT8_C(1) ||
        loaded_inspection.players[0].stale_move_ids[0] !=
            (uint8_t)PF_M4_ACTION_PUMMEL)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic pummel-return"
            " events=%" PRIu32 " source_action=%u source_ticks=%u"
            " target_action=%u target_damage=%" PRIu32
            " last_hit=%u last_damage=%" PRIu32
            " source_stale=%u/%u loaded_stale=%u/%u\n",
            pummel_events,
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[1].action_state,
            source_inspection.players[1].damage_q16,
            (unsigned int)source_inspection.players[1].last_hit_valid,
            source_inspection.players[1].last_hit_damage_q16,
            (unsigned int)source_inspection.players[0].stale_move_count,
            (unsigned int)source_inspection.players[0].stale_move_ids[0],
            (unsigned int)loaded_inspection.players[0].stale_move_count,
            (unsigned int)loaded_inspection.players[0].stale_move_ids[0]);
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
            (uint8_t)PF_M4_ACTION_THROW_FORWARD ||
        source_inspection.players[0].action_ticks != UINT16_C(0) ||
        source_inspection.players[0].grab_target != UINT8_C(1) ||
        source_inspection.players[1].grab_owner != UINT8_C(0) ||
        find_last_tick_event(PF_SIM_EVENT_PUMMEL) != NULL ||
        find_last_tick_event(PF_SIM_EVENT_THROW) != NULL)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic pummel-strong"
            " source_action=%u source_ticks=%u grab_target=%u"
            " target_action=%u grab_owner=%u pummel_event=%u"
            " throw_event=%u\n",
            (unsigned int)source_inspection.players[0].action_state,
            (unsigned int)source_inspection.players[0].action_ticks,
            (unsigned int)source_inspection.players[0].grab_target,
            (unsigned int)source_inspection.players[1].action_state,
            (unsigned int)source_inspection.players[1].grab_owner,
            find_last_tick_event(PF_SIM_EVENT_PUMMEL) != NULL,
            find_last_tick_event(PF_SIM_EVENT_THROW) != NULL);
        return fail("grab-fresh-strong-reduced-forward-throw");
    }
    return 1;
}

static int run_directional_throw_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    if (!run_directional_throw_case(
            content,
            view,
            &content->fighter.forward_throw,
            content->fighter.knockback_weight,
            INT16_C(32767),
            INT16_C(0),
            PF_M4_ACTION_THROW_FORWARD) ||
        !run_directional_throw_case(
            content,
            view,
            &content->fighter.back_throw,
            content->fighter.knockback_weight,
            INT16_C(-32767),
            INT16_C(0),
            PF_M4_ACTION_THROW_BACK) ||
        !run_directional_throw_case(
            content,
            view,
            &content->fighter.up_throw,
            content->fighter.knockback_weight,
            INT16_C(0),
            INT16_C(-32767),
            PF_M4_ACTION_THROW_UP) ||
        !run_directional_throw_case(
            content,
            view,
            &content->fighter.down_throw,
            content->fighter.knockback_weight,
            INT16_C(0),
            INT16_C(32767),
            PF_M4_ACTION_THROW_DOWN) ||
        !run_directional_throw_case(
            content,
            view,
            &content->fighter.forward_throw,
            content->fighter.knockback_weight,
            INT16_C(32767),
            INT16_C(-32767),
            PF_M4_ACTION_THROW_FORWARD) ||
        !run_directional_throw_case(
            content,
            view,
            &content->fighter.forward_throw,
            content->fighter.knockback_weight,
            INT16_C(30000),
            INT16_C(-32767),
            PF_M4_ACTION_THROW_FORWARD))
    {
        return 0;
    }
    return 1;
}

static int run_throw_collateral_test(void)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int16_t axes_x[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    int16_t axes_y[PF_SIM_MAX_PLAYERS] = {
        INT16_C(0), INT16_C(0), INT16_C(0), INT16_C(0)};
    uint64_t buttons[PF_SIM_MAX_PLAYERS] = {
        UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0)};
    uint16_t triggers[PF_SIM_MAX_PLAYERS] = {
        UINT16_C(0), UINT16_C(0), UINT16_C(0), UINT16_C(0)};
    uint32_t tick;
    int grab_seen = 0;
    int collateral_seen = 0;
    int throw_seen = 0;

    if (!make_grab_content(
            (INT32_C(3) * PF_Q16_ONE) / INT32_C(10),
            &content,
            &view) ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(4),
            PF_SIM_MODE_TEAMS,
            1,
            &sim))
    {
        return 0;
    }

    buttons[2] = PF_INPUT_BUTTON_ATTACK;
    triggers[2] = UINT16_MAX;
    if (!step_players_with_triggers(
            sim,
            UINT8_C(4),
            axes_x,
            axes_y,
            buttons,
            triggers,
            &inspection))
    {
        return 0;
    }
    (void)memset(buttons, 0, sizeof(buttons));
    (void)memset(triggers, 0, sizeof(triggers));
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        const pf_sim_event *event = find_last_tick_event(PF_SIM_EVENT_GRAB);

        if (event != NULL)
        {
            if (event->source_player != UINT8_C(2) ||
                event->target_player != UINT8_C(1))
            {
                return fail("throw-collateral-grab-priority");
            }
            grab_seen = 1;
            break;
        }
        if (!step_players(
                sim,
                UINT8_C(4),
                axes_x,
                axes_y,
                buttons,
                &inspection))
        {
            return 0;
        }
    }
    if (grab_seen == 0)
    {
        return fail("throw-collateral-grab");
    }

    axes_x[2] = INT16_MAX;
    buttons[2] = UINT64_C(0);
    if (!step_players(
            sim,
            UINT8_C(4),
            axes_x,
            axes_y,
            buttons,
            &inspection) ||
        inspection.players[2].action_state !=
            (uint8_t)PF_M4_ACTION_THROW_BACK)
    {
        (void)fprintf(
            stderr,
            "m4-combat=diagnostic throw-collateral-input"
            " action=%u ticks=%u facing=%d axis=%d\n",
            (unsigned int)inspection.players[2].action_state,
            (unsigned int)inspection.players[2].action_ticks,
            (int)inspection.players[2].facing,
            (int)axes_x[2]);
        return fail("throw-collateral-input");
    }
    (void)memset(axes_x, 0, sizeof(axes_x));
    (void)memset(buttons, 0, sizeof(buttons));
    for (tick = UINT32_C(0); tick < UINT32_C(96); ++tick)
    {
        const pf_sim_event *hit = find_last_tick_event(PF_SIM_EVENT_HIT);
        const pf_sim_event *throw_event =
            find_last_tick_event(PF_SIM_EVENT_THROW);

        if (hit != NULL && hit->source_player == UINT8_C(2) &&
            hit->target_player == UINT8_C(3))
        {
            collateral_seen = 1;
        }
        if (throw_event != NULL &&
            throw_event->source_player == UINT8_C(2) &&
            throw_event->target_player == UINT8_C(1))
        {
            throw_seen = 1;
        }
        if (collateral_seen != 0 && throw_seen != 0)
        {
            break;
        }
        if (!step_players(
                sim,
                UINT8_C(4),
                axes_x,
                axes_y,
                buttons,
                &inspection))
        {
            return 0;
        }
    }
    if (collateral_seen == 0 || throw_seen == 0 ||
        inspection.players[1].damage_q16 !=
            content.fighter.back_throw.damage_q16 +
                UINT32_C(5) * UINT32_C(65536) ||
        inspection.players[3].damage_q16 !=
            UINT32_C(5) * UINT32_C(65536) ||
        inspection.players[2].stale_move_count != UINT8_C(1) ||
        inspection.players[2].stale_move_ids[0] !=
            (uint8_t)PF_M4_ACTION_THROW_BACK)
    {
        return fail("throw-collateral-resolution");
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
            UINT64_C(0),
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.down_throw.damage_q16,
                UINT32_C(3)) ||
        inspection.players[0].stale_move_count != UINT8_C(3) ||
        inspection.players[0].stale_move_ids[0] !=
            (uint8_t)PF_M4_ACTION_THROW_DOWN ||
        inspection.players[0].stale_move_ids[1] !=
            (uint8_t)PF_M4_ACTION_THROW_DOWN ||
        inspection.players[0].stale_move_ids[2] !=
            (uint8_t)PF_M4_ACTION_THROW_DOWN)
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
            UINT64_C(0),
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
        save_size != (size_t)915)
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
            player0_buttons = UINT64_C(0);
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.down_throw.damage_q16,
                UINT32_C(3)))
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                UINT32_C(2)))
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
            expected_repeated_move_damage_q16(
                &content->fighter,
                content->fighter.jab_damage_q16,
                UINT32_C(2)) +
                content->fighter.down_throw.damage_q16)
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

static int prepare_player0_right_ledge(
    pf_sim *sim,
    const pf_m4_content *content,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "ledge-attack-initial-inspect"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(400); ++tick)
    {
        const int16_t target_axis =
            out_inspection->players[1].position_x_q16 <
                    out_inspection->stage.right_ledge_x_q16 -
                        INT32_C(3) * PF_Q16_ONE
                ? INT16_MAX
                : INT16_C(0);

        if (!step_duel(
                sim,
                INT16_MAX,
                UINT64_C(0),
                target_axis,
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
                    INT32_C(5) * PF_Q16_ONE)
        {
            break;
        }
    }
    if (tick == UINT32_C(400))
    {
        return fail("ledge-attack-right-edge-setup");
    }

    for (tick = UINT32_C(0); tick < UINT32_C(80); ++tick)
    {
        const int16_t target_axis =
            out_inspection->players[1].position_x_q16 <
                    out_inspection->stage.right_ledge_x_q16 -
                        INT32_C(3) * PF_Q16_ONE
                ? INT16_MAX
                : INT16_C(0);

        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                target_axis,
                UINT64_C(0),
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
    if (tick == UINT32_C(80) ||
        !step_duel(
            sim,
            INT16_C(-16000),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return fail("ledge-attack-brake");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(24); ++tick)
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
        if (out_inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE &&
            out_inspection->players[0].facing == INT8_C(-1))
        {
            break;
        }
    }
    if (tick == UINT32_C(24) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            UINT64_C(0),
            out_inspection))
    {
        return fail("ledge-attack-inward-turn");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(120); ++tick)
    {
        const int16_t drift_x =
            out_inspection->players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_JUMP_SQUAT
                ? INT16_C(0)
                : INT16_C(20000);

        if (!step_duel(
                sim,
                drift_x,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
        if (out_inspection->players[0].action_state ==
            (uint8_t)PF_M4_ACTION_LEDGE_HANG)
        {
            break;
        }
    }
    if (tick != UINT32_C(120))
    {
        for (tick = UINT32_C(0);
             tick < UINT32_C(32) &&
             out_inspection->players[1].position_x_q16 >=
                 out_inspection->stage.right_ledge_x_q16 -
                     INT32_C(2) * PF_Q16_ONE;
             ++tick)
        {
            if (!step_duel(
                    sim,
                    INT16_C(0),
                    UINT64_C(0),
                    INT16_C(-16000),
                    UINT64_C(0),
                    out_inspection))
            {
                return 0;
            }
        }
        for (tick = UINT32_C(0);
             tick < UINT32_C(32) &&
             out_inspection->players[1].action_state !=
                 (uint8_t)PF_M4_ACTION_GROUND_IDLE;
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
    }
    if (out_inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_HANG ||
        out_inspection->players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        out_inspection->players[1].grounded == UINT8_C(0) ||
        out_inspection->players[1].position_x_q16 <
            out_inspection->stage.right_ledge_x_q16 -
                INT32_C(3) * PF_Q16_ONE ||
        out_inspection->players[1].position_x_q16 >=
            out_inspection->stage.right_ledge_x_q16)
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=ledge-attack-hang-target"
            " attacker_x=%" PRId32 " target_x=%" PRId32 "\n",
            out_inspection->players[0].position_x_q16,
            out_inspection->players[1].position_x_q16);
        return 0;
    }
    (void)content;
    return 1;
}

static int run_ledge_attack_snapshot_test(
    pf_sim *source,
    const pf_m4_content *content,
    const pf_content_view *view,
    pf_m4_inspection *source_inspection)
{
    test_sim_storage loaded_storage;
    pf_sim *loaded = NULL;
    pf_m4_inspection loaded_inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[TEST_SAVE_CAPACITY];
    pf_mut_bytes destination;
    pf_bytes save;
    size_t save_size = (size_t)0;
    uint32_t tick;

    if (!initialize_sim(
            &loaded_storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            0,
            &loaded) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "ledge-attack-query-save-size") ||
        save_size != (size_t)915)
    {
        return fail("ledge-attack-snapshot-size");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "ledge-attack-save") ||
        destination.size != save_size ||
        memcmp(save_bytes, "PFSAVE58", (size_t)8) != 0)
    {
        return fail("ledge-attack-save-format");
    }
    save.bytes = save_bytes;
    save.size = destination.size;
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "ledge-attack-load"))
    {
        return 0;
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)content->fighter.ledge_attack.hitlag_ticks +
                    UINT32_C(4);
         ++tick)
    {
        if (!step_duel(
                source,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                source_inspection) ||
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
                "ledge-attack-source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "ledge-attack-loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("ledge-attack-snapshot-continuation");
        }
        if (source_inspection->players[0].ledge !=
                (uint8_t)PF_M4_LEDGE_RIGHT ||
            loaded_inspection.players[0].ledge !=
                (uint8_t)PF_M4_LEDGE_RIGHT)
        {
            return fail("ledge-attack-hitlag-ledge-claim");
        }
    }
    return 1;
}

static int run_ledge_attack_test(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_sim_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    pf_m4_content changed = *content;
    pf_m4_content invalid_attack = *content;
    pf_m4_content invalid_invulnerability = *content;
    pf_content_view changed_view;
    const pf_m4_attack_data *attack = &content->fighter.ledge_attack;
    const pf_m4_falcon_ledge_attack_reference *reference_attack =
        pf_m4_falcon_reference_ledge_attack(
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK);
    const uint32_t attack_ticks =
        (uint32_t)attack->startup_ticks +
        (uint32_t)attack->active_ticks +
        (uint32_t)attack->recovery_ticks;
    const pf_sim_event *hit_event = NULL;
    uint32_t tick;

    changed.fighter.ledge_attack.damage_q16 += UINT32_C(1);
    invalid_attack.fighter.ledge_attack.startup_ticks = UINT16_C(0);
    invalid_invulnerability.fighter
        .ledge_attack_invulnerability_ticks =
        (uint16_t)(attack_ticks + UINT32_C(1));
    if (reference_attack == NULL ||
        reference_attack->total_frames != UINT16_C(54) ||
        reference_attack->first_active_frame != UINT16_C(24) ||
        reference_attack->last_active_frame != UINT16_C(29) ||
        attack->damage_q16 != UINT32_C(10) * UINT32_C(65536) ||
        attack->startup_ticks != UINT16_C(6) ||
        attack->active_ticks != UINT16_C(3) ||
        attack->recovery_ticks != UINT16_C(20) ||
        content->fighter.ledge_attack_invulnerability_ticks !=
            UINT16_C(10) ||
        !expect_status(
            pf_m4_make_content_view(&changed, &changed_view),
            PF_STATUS_OK,
            "ledge-attack-changed-content") ||
        memcmp(
            view->content_hash.bytes,
            changed_view.content_hash.bytes,
            sizeof(view->content_hash.bytes)) == 0 ||
        !expect_status(
            pf_m4_validate_content(&invalid_attack),
            PF_STATUS_INVALID_CONFIG,
            "ledge-attack-invalid-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_invulnerability),
            PF_STATUS_INVALID_CONFIG,
            "ledge-attack-invalid-invulnerability") ||
        !initialize_sim(
            &storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !prepare_player0_right_ledge(sim, content, &inspection))
    {
        return fail("ledge-attack-content-or-setup");
    }

    if (!step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            content->fighter.digital_trigger_threshold,
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(0) ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        inspection.players[0].damage_q16 != UINT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-combat=debug operation=ledge-attack-input-arbitration"
            " action=%u tick=%u ledge=%u damage=%" PRIu32 "\n",
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].ledge,
            inspection.players[0].damage_q16);
        return fail("ledge-attack-input-arbitration");
    }

    for (tick = UINT32_C(0);
         tick < (uint32_t)reference_attack->last_active_frame +
                    UINT32_C(2);
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
            return 0;
        }
        if (inspection.players[0].damage_q16 != UINT32_C(0))
        {
            return fail("ledge-attack-invulnerability-rejection");
        }
        hit_event = find_last_tick_event(PF_SIM_EVENT_HIT);
        if (hit_event != NULL &&
            hit_event->detail ==
                (uint16_t)PF_M4_ACTION_LEDGE_ATTACK)
        {
            break;
        }
    }
    if (hit_event == NULL ||
        hit_event->source_player != UINT8_C(0) ||
        hit_event->target_player != UINT8_C(1) ||
        hit_event->value_q16 != attack->damage_q16 ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_RIGHT ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG ||
        inspection.players[1].damage_q16 != attack->damage_q16 ||
        !run_ledge_attack_snapshot_test(
            sim,
            content,
            view,
            &inspection) ||
        inspection.players[0].damage_q16 != UINT32_C(0) ||
        inspection.players[0].invulnerable != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-combat=fail operation=ledge-attack-hit-detail"
            " event=%u source=%u target=%u value=%" PRIu32
            " attacker_action=%u target_action=%u"
            " target_damage=%" PRIu32
            " attacker_damage=%" PRIu32 " invulnerable=%u"
            " attacker_x=%" PRId32 " target_x=%" PRId32
            " action_ticks=%u hitbox=(%" PRId32 ",%" PRId32
            ")\n",
            hit_event != NULL ? UINT32_C(1) : UINT32_C(0),
            hit_event != NULL
                ? (unsigned int)hit_event->source_player
                : UINT32_C(255),
            hit_event != NULL
                ? (unsigned int)hit_event->target_player
                : UINT32_C(255),
            hit_event != NULL ? hit_event->value_q16 : UINT32_C(0),
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[1].action_state,
            inspection.players[1].damage_q16,
            inspection.players[0].damage_q16,
            (unsigned int)inspection.players[0].invulnerable,
            inspection.players[0].position_x_q16,
            inspection.players[1].position_x_q16,
            (unsigned int)inspection.players[0].action_ticks,
            inspection.players[0].hitbox_left_q16,
            inspection.players[0].hitbox_right_q16);
        return fail("ledge-attack-hit-or-snapshot");
    }

    for (tick = UINT32_C(0);
         tick < UINT32_C(128) &&
         inspection.players[0].action_state ==
             (uint8_t)PF_M4_ACTION_LEDGE_ATTACK;
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
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.players[0].grounded != UINT8_C(1) ||
        inspection.players[0].support !=
            (uint8_t)PF_M4_SURFACE_FLOOR ||
        inspection.players[0].ledge !=
            (uint8_t)PF_M4_LEDGE_NONE)
    {
        return fail("ledge-attack-completion");
    }

    if (!expect_status(
            pf_sim_reset(sim, UINT64_C(0x1ed6ea77ac)),
            PF_STATUS_OK,
            "ledge-attack-strong-reset") ||
        !prepare_player0_right_ledge(sim, content, &inspection) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        !step_reaction_duel(
            sim,
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LEDGE_ATTACK)
    {
        return fail("ledge-attack-special-input");
    }
    return 1;
}

static int run_falcon_reference_table_test(void)
{
    uint16_t common_attribute_count = UINT16_C(0);
    static const uint8_t aerial_actions[5] = {
        (uint8_t)PF_M4_ACTION_AERIAL_ATTACK,
        (uint8_t)PF_M4_ACTION_FORWARD_AERIAL,
        (uint8_t)PF_M4_ACTION_BACK_AERIAL,
        (uint8_t)PF_M4_ACTION_UP_AERIAL,
        (uint8_t)PF_M4_ACTION_DOWN_AERIAL};
    static const uint16_t autocancel_before[5] = {
        UINT16_C(4), UINT16_C(7), UINT16_C(7), UINT16_C(1), UINT16_C(4)};
    static const uint16_t autocancel_after[5] = {
        UINT16_C(33), UINT16_C(34), UINT16_C(20), UINT16_C(21), UINT16_C(35)};
    static const uint16_t aerial_iasa_frames[5] = {
        UINT16_C(0), UINT16_C(36), UINT16_C(29), UINT16_C(30), UINT16_C(38)};
    uint32_t aerial_index;
    pf_m4_falcon_move_index mapped_move = PF_M4_FALCON_MOVE_COUNT;
    const pf_m4_reference_move *jab =
        pf_m4_falcon_reference_move(PF_M4_FALCON_JAB1);
    const pf_m4_reference_move *dash =
        pf_m4_falcon_reference_move(PF_M4_FALCON_DASH_ATTACK);
    const pf_m4_reference_move *missing =
        pf_m4_falcon_reference_move(PF_M4_FALCON_FORWARD_SMASH_MID_HIGH);
    const pf_m4_reference_move *missing_low =
        pf_m4_falcon_reference_move(PF_M4_FALCON_FORWARD_SMASH_MID_LOW);
    const pf_m4_reference_move *special_n =
        pf_m4_falcon_reference_move(PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND);
    const pf_m4_reference_move *special_s_ground =
        pf_m4_falcon_reference_move(
            PF_M4_FALCON_SIDE_SPECIAL_START_GROUND);
    const pf_m4_reference_move *special_s_ground_hit =
        pf_m4_falcon_reference_move(
            PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND);
    const pf_m4_reference_move *special_s_air =
        pf_m4_falcon_reference_move(
            PF_M4_FALCON_SIDE_SPECIAL_START_AIR);
    const pf_m4_reference_move *special_s_air_hit =
        pf_m4_falcon_reference_move(
            PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR);
    const pf_m4_reference_move *special_last =
        pf_m4_falcon_reference_move(
            PF_M4_FALCON_DOWN_SPECIAL_WALL_REBOUND);
    const pf_m4_reference_hit_phase *jab_phase =
        pf_m4_falcon_reference_phase(PF_M4_FALCON_JAB1, UINT16_C(0));
    const pf_m4_reference_hit_effect *jab_effect =
        pf_m4_falcon_reference_primary_effect(PF_M4_FALCON_JAB1);
    const pf_m4_reference_hit_effect *special_s_ground_effect =
        pf_m4_falcon_reference_primary_effect(
            PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND);
    const pf_m4_reference_hit_effect *special_s_air_effect =
        pf_m4_falcon_reference_primary_effect(
            PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR);
    const pf_m4_reference_throw *down_throw =
        pf_m4_falcon_reference_throw(PF_M4_FALCON_DOWN_THROW);
    const pf_m4_reference_throw *forward_throw =
        pf_m4_falcon_reference_throw(PF_M4_FALCON_FORWARD_THROW);
    const pf_m4_reference_throw *back_throw =
        pf_m4_falcon_reference_throw(PF_M4_FALCON_BACK_THROW);
    const pf_m4_reference_throw *up_throw =
        pf_m4_falcon_reference_throw(PF_M4_FALCON_UP_THROW);
    const pf_m4_reference_timing jab_timing =
        pf_m4_falcon_reference_timing(PF_M4_FALCON_JAB1);
    const pf_m4_reference_timing dash_timing =
        pf_m4_falcon_reference_timing(PF_M4_FALCON_DASH_ATTACK);
    const pf_m4_reference_timing standing_grab_timing =
        pf_m4_falcon_reference_timing(PF_M4_FALCON_GRAB);
    const pf_m4_reference_timing dash_grab_timing =
        pf_m4_falcon_reference_timing(PF_M4_FALCON_DASH_GRAB);
    const pf_m4_reference_timing missing_timing =
        pf_m4_falcon_reference_timing(
            PF_M4_FALCON_FORWARD_SMASH_MID_HIGH);
    const pf_m4_reference_hit_phase *nair_late_phase =
        pf_m4_falcon_reference_phase_at_frame(
            PF_M4_FALCON_NEUTRAL_AERIAL,
            UINT16_C(20));
    const pf_m4_reference_hit_effect *nair_late_effect =
        pf_m4_falcon_reference_effect_at_frame(
            PF_M4_FALCON_NEUTRAL_AERIAL,
            UINT16_C(20));
    uint8_t jab_sphere_count = UINT8_C(0);
    uint8_t jab_continuing_sphere_count = UINT8_C(0);
    uint8_t jab2_sphere_count = UINT8_C(0);
    uint8_t up_smash_sphere_count = UINT8_C(0);
    uint8_t grab_sphere_count = UINT8_C(0);
    uint8_t dash_grab_sphere_count = UINT8_C(0);
    uint8_t neutral_special_sphere_count = UINT8_C(0);
    uint8_t neutral_special_air_sphere_count = UINT8_C(0);
    uint8_t side_special_ground_search_count = UINT8_C(0);
    uint8_t side_special_air_search_count = UINT8_C(0);
    uint8_t down_special_ground_sphere_count = UINT8_C(0);
    uint8_t down_special_air_sphere_count = UINT8_C(0);
    uint8_t down_tilt_sphere_count = UINT8_C(0);
    uint8_t standing_hurt_capsule_count = UINT8_C(0);
    uint8_t dash_hurt_capsule_count = UINT8_C(0);
    uint8_t dash_last_hurt_capsule_count = UINT8_C(0);
    uint8_t run_brake_hurt_capsule_count = UINT8_C(0);
    uint8_t run_brake_last_hurt_capsule_count = UINT8_C(0);
    uint8_t crouch_start_hurt_capsule_count = UINT8_C(0);
    uint8_t crouch_start_last_hurt_capsule_count = UINT8_C(0);
    uint8_t crouch_end_hurt_capsule_count = UINT8_C(0);
    uint8_t crouch_end_last_hurt_capsule_count = UINT8_C(0);
    uint8_t knee_bend_hurt_capsule_count = UINT8_C(0);
    uint8_t knee_bend_last_hurt_capsule_count = UINT8_C(0);
    uint8_t spot_dodge_hurt_capsule_count = UINT8_C(0);
    uint8_t spot_dodge_last_hurt_capsule_count = UINT8_C(0);
    uint8_t roll_forward_hurt_capsule_count = UINT8_C(0);
    uint8_t roll_forward_last_hurt_capsule_count = UINT8_C(0);
    uint8_t roll_backward_hurt_capsule_count = UINT8_C(0);
    uint8_t roll_backward_last_hurt_capsule_count = UINT8_C(0);
    uint8_t air_dodge_hurt_capsule_count = UINT8_C(0);
    uint8_t air_dodge_last_hurt_capsule_count = UINT8_C(0);
    uint8_t fall_special_hurt_capsule_count = UINT8_C(0);
    uint8_t fall_special_last_hurt_capsule_count = UINT8_C(0);
    uint8_t landing_fall_special_hurt_capsule_count = UINT8_C(0);
    uint8_t landing_fall_special_last_hurt_capsule_count = UINT8_C(0);
    uint8_t landing_hurt_capsule_count = UINT8_C(0);
    uint8_t landing_last_hurt_capsule_count = UINT8_C(0);
    uint8_t jab_hurt_capsule_count = UINT8_C(0);
    uint8_t nair_hurt_capsule_count = UINT8_C(0);
    uint8_t grab_hurt_capsule_count = UINT8_C(0);
    uint8_t neutral_special_hurt_capsule_count = UINT8_C(0);
    uint8_t neutral_special_air_hurt_capsule_count = UINT8_C(0);
    const pf_m4_reference_hit_sphere *jab_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_JAB1,
            UINT16_C(3),
            &jab_sphere_count);
    const pf_m4_reference_hit_sphere *jab_continuing_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_JAB1,
            UINT16_C(4),
            &jab_continuing_sphere_count);
    const pf_m4_reference_hit_sphere *jab2_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_JAB2,
            UINT16_C(5),
            &jab2_sphere_count);
    const pf_m4_reference_hit_sphere *up_smash_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_UP_SMASH,
            UINT16_C(21),
            &up_smash_sphere_count);
    const pf_m4_reference_hit_sphere *grab_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_GRAB,
            UINT16_C(7),
            &grab_sphere_count);
    const pf_m4_reference_hit_sphere *dash_grab_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_DASH_GRAB,
            UINT16_C(11),
            &dash_grab_sphere_count);
    const pf_m4_reference_hit_sphere *neutral_special_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND,
            UINT16_C(52),
            &neutral_special_sphere_count);
    const pf_m4_reference_hit_sphere *neutral_special_air_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_NEUTRAL_SPECIAL_AIR,
            UINT16_C(52),
            &neutral_special_air_sphere_count);
    const pf_m4_reference_search_sphere *side_special_ground_search =
        pf_m4_falcon_reference_side_special_search_spheres(
            UINT8_C(0),
            &side_special_ground_search_count);
    const pf_m4_reference_search_sphere *side_special_air_search =
        pf_m4_falcon_reference_side_special_search_spheres(
            UINT8_C(1),
            &side_special_air_search_count);
    const pf_m4_reference_hit_sphere *down_special_ground_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_DOWN_SPECIAL_GROUND,
            UINT16_C(25),
            &down_special_ground_sphere_count);
    const pf_m4_reference_hit_sphere *down_special_air_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_DOWN_SPECIAL_AIR,
            UINT16_C(29),
            &down_special_air_sphere_count);
    const pf_m4_reference_hit_sphere *down_tilt_spheres =
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_DOWN_TILT,
            UINT16_C(15),
            &down_tilt_sphere_count);
    const pf_m4_reference_hurt_capsule *standing_hurt_capsules =
        pf_m4_falcon_reference_standing_hurt_capsules(
            &standing_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *dash_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_INITIAL_DASH,
            UINT16_C(1),
            &dash_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *dash_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_INITIAL_DASH,
            UINT16_C(15),
            &dash_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *run_brake_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_RUN_BRAKE,
            UINT16_C(1),
            &run_brake_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *run_brake_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_RUN_BRAKE,
            UINT16_C(28),
            &run_brake_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *crouch_start_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_CROUCH_START,
            UINT16_C(1),
            &crouch_start_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *crouch_start_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_CROUCH_START,
            UINT16_C(7),
            &crouch_start_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *crouch_end_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_CROUCH_END,
            UINT16_C(1),
            &crouch_end_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *crouch_end_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_CROUCH_END,
            UINT16_C(10),
            &crouch_end_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *knee_bend_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT,
            UINT16_C(1),
            &knee_bend_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *knee_bend_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT,
            UINT16_C(4),
            &knee_bend_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *spot_dodge_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_SPOT_DODGE,
            UINT16_C(1),
            &spot_dodge_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *spot_dodge_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_SPOT_DODGE,
            UINT16_C(32),
            &spot_dodge_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *roll_forward_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD,
            UINT16_C(1),
            &roll_forward_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *roll_forward_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD,
            UINT16_C(31),
            &roll_forward_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *roll_backward_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_ROLL_BACKWARD,
            UINT16_C(1),
            &roll_backward_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *roll_backward_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_ROLL_BACKWARD,
            UINT16_C(31),
            &roll_backward_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *air_dodge_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_AIR_DODGE,
            UINT16_C(1),
            &air_dodge_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *air_dodge_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_AIR_DODGE,
            UINT16_C(49),
            &air_dodge_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *fall_special_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL,
            UINT16_C(1),
            &fall_special_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *fall_special_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL,
            UINT16_C(8),
            &fall_special_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *landing_fall_special_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING,
            UINT16_C(1),
            &landing_fall_special_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *
        landing_fall_special_last_hurt_capsules =
            pf_m4_falcon_reference_common_hurt_capsules_at_frame(
                (uint8_t)PF_M4_ACTION_SPECIAL_LANDING,
                UINT16_C(10),
                &landing_fall_special_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *landing_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_LANDING,
            UINT16_C(1),
            &landing_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *landing_last_hurt_capsules =
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_LANDING,
            UINT16_C(30),
            &landing_last_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *jab_hurt_capsules =
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_JAB1,
            UINT16_C(1),
            &jab_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *nair_hurt_capsules =
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_NEUTRAL_AERIAL,
            UINT16_C(44),
            &nair_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *grab_hurt_capsules =
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_GRAB,
            UINT16_C(29),
            &grab_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *neutral_special_hurt_capsules =
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND,
            UINT16_C(99),
            &neutral_special_hurt_capsule_count);
    const pf_m4_reference_hurt_capsule *neutral_special_air_hurt_capsules =
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_NEUTRAL_SPECIAL_AIR,
            UINT16_C(99),
            &neutral_special_air_hurt_capsule_count);
    const uint8_t *geometry_sha256 =
        pf_m4_falcon_reference_geometry_sha256();
    const uint8_t *complete_source_sha256 =
        pf_m4_falcon_reference_complete_source_sha256();
    const pf_m4_falcon_ledge_attack_reference *ledge_attack_slow =
        pf_m4_falcon_reference_ledge_attack(
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_SLOW);
    const pf_m4_falcon_ledge_attack_reference *ledge_attack_quick =
        pf_m4_falcon_reference_ledge_attack(
            (uint16_t)PF_M4_FALCON_SUBMOTION_LEDGE_ATTACK_QUICK);
    const uint8_t *submotion_catalog_sha256 =
        pf_m4_falcon_reference_submotion_catalog_sha256();
    const uint8_t *action_script_sha256 =
        pf_m4_falcon_reference_action_script_sha256();
    const uint8_t *animation_tracks_sha256 =
        pf_m4_falcon_reference_animation_tracks_sha256();
    const pf_m4_falcon_animation_decode_summary *animation_decode_summary =
        pf_m4_falcon_reference_animation_decode_summary();
    const pf_m4_falcon_submotion_data *dash_submotion =
        pf_m4_falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_DASH);
    const pf_m4_falcon_submotion_data *fall_special_submotion =
        pf_m4_falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_FALL_SPECIAL);
    const pf_m4_falcon_submotion_data *landing_fall_special_submotion =
        pf_m4_falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_LANDING_FALL_SPECIAL);
    const pf_m4_falcon_submotion_data *empty_submotion =
        pf_m4_falcon_reference_submotion(UINT16_C(5));
    const uint32_t *common_attribute_bits =
        pf_m4_falcon_reference_common_attribute_bits(
            &common_attribute_count);
    const pf_m4_falcon_common_attributes *common_attributes =
        pf_m4_falcon_reference_common_attributes();
    const pf_m4_falcon_ledge_attributes *ledge_attributes =
        pf_m4_falcon_reference_ledge_attributes();
    const pf_m4_falcon_special_attributes *special_attributes =
        pf_m4_falcon_reference_special_attributes();
    const pf_m4_falcon_common_special_attributes *common_special_attributes =
        pf_m4_falcon_reference_common_special_attributes();
    const pf_m4_falcon_air_dodge_attributes *air_dodge_attributes =
        pf_m4_falcon_reference_air_dodge_attributes();
    const pf_m4_falcon_collision_pose *collision_pose =
        pf_m4_falcon_reference_collision_pose();
    const pf_m4_falcon_ecb_pose_q16 *jump_forward_first_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_JUMP_FORWARD,
            UINT16_C(0));
    const pf_m4_falcon_ecb_pose_q16 *jump_forward_last_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_JUMP_FORWARD,
            UINT16_MAX);
    const pf_m4_falcon_ecb_pose_q16 *jump_backward_last_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_JUMP_BACKWARD,
            UINT16_MAX);
    const pf_m4_falcon_ecb_pose_q16 *jump_aerial_forward_last_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD,
            UINT16_MAX);
    const pf_m4_falcon_ecb_pose_q16 *jump_aerial_backward_last_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_BACKWARD,
            UINT16_MAX);
    const pf_m4_falcon_ecb_pose_q16 *fall_first_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALL,
            UINT16_C(0));
    const pf_m4_falcon_ecb_pose_q16 *fall_aerial_last_ecb =
        pf_m4_falcon_reference_airborne_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALL_AERIAL,
            UINT16_MAX);
    const pf_m4_melee_stale_move_data *stale_move_data =
        pf_m4_falcon_reference_stale_move_data();
    const pf_m4_falcon_side_special_timing *side_special_timing =
        pf_m4_falcon_reference_side_special_timing();
    const pf_m4_falcon_up_special_timing *up_special_timing =
        pf_m4_falcon_reference_up_special_timing();
    const pf_m4_falcon_down_special_timing *down_special_timing =
        pf_m4_falcon_reference_down_special_timing();
    int32_t dash_motion_q16 = INT32_C(0);
    uint32_t present_count = UINT32_C(0);
    uint32_t animated_submotion_count = UINT32_C(0);
    uint32_t empty_submotion_count = UINT32_C(0);
    uint32_t script_event_count = UINT32_C(0);
    uint32_t script_byte_count = UINT32_C(0);
    uint32_t translation_submotion_count = UINT32_C(0);
    uint32_t translation_sample_count = UINT32_C(0);
    uint16_t submotion_index;

    if (down_tilt_spheres == NULL ||
        down_tilt_sphere_count != UINT8_C(3) ||
        standing_hurt_capsules == NULL ||
        standing_hurt_capsule_count != UINT8_C(11) ||
        dash_hurt_capsules == NULL ||
        dash_hurt_capsule_count != UINT8_C(11) ||
        dash_last_hurt_capsules == NULL ||
        dash_last_hurt_capsule_count != UINT8_C(11) ||
        run_brake_hurt_capsules == NULL ||
        run_brake_hurt_capsule_count != UINT8_C(11) ||
        run_brake_last_hurt_capsules == NULL ||
        run_brake_last_hurt_capsule_count != UINT8_C(11) ||
        crouch_start_hurt_capsules == NULL ||
        crouch_start_hurt_capsule_count != UINT8_C(11) ||
        crouch_start_last_hurt_capsules == NULL ||
        crouch_start_last_hurt_capsule_count != UINT8_C(11) ||
        crouch_end_hurt_capsules == NULL ||
        crouch_end_hurt_capsule_count != UINT8_C(11) ||
        crouch_end_last_hurt_capsules == NULL ||
        crouch_end_last_hurt_capsule_count != UINT8_C(11) ||
        knee_bend_hurt_capsules == NULL ||
        knee_bend_hurt_capsule_count != UINT8_C(11) ||
        knee_bend_last_hurt_capsules == NULL ||
        knee_bend_last_hurt_capsule_count != UINT8_C(11) ||
        spot_dodge_hurt_capsules == NULL ||
        spot_dodge_hurt_capsule_count != UINT8_C(11) ||
        spot_dodge_last_hurt_capsules == NULL ||
        spot_dodge_last_hurt_capsule_count != UINT8_C(11) ||
        roll_forward_hurt_capsules == NULL ||
        roll_forward_hurt_capsule_count != UINT8_C(11) ||
        roll_forward_last_hurt_capsules == NULL ||
        roll_forward_last_hurt_capsule_count != UINT8_C(11) ||
        roll_backward_hurt_capsules == NULL ||
        roll_backward_hurt_capsule_count != UINT8_C(11) ||
        roll_backward_last_hurt_capsules == NULL ||
        roll_backward_last_hurt_capsule_count != UINT8_C(11) ||
        air_dodge_hurt_capsules == NULL ||
        air_dodge_hurt_capsule_count != UINT8_C(11) ||
        air_dodge_last_hurt_capsules == NULL ||
        air_dodge_last_hurt_capsule_count != UINT8_C(11) ||
        fall_special_hurt_capsules == NULL ||
        fall_special_hurt_capsule_count != UINT8_C(11) ||
        fall_special_last_hurt_capsules == NULL ||
        fall_special_last_hurt_capsule_count != UINT8_C(11) ||
        landing_fall_special_hurt_capsules == NULL ||
        landing_fall_special_hurt_capsule_count != UINT8_C(11) ||
        landing_fall_special_last_hurt_capsules == NULL ||
        landing_fall_special_last_hurt_capsule_count != UINT8_C(11) ||
        landing_hurt_capsules == NULL ||
        landing_hurt_capsule_count != UINT8_C(11) ||
        landing_last_hurt_capsules == NULL ||
        landing_last_hurt_capsule_count != UINT8_C(11) ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_INITIAL_DASH,
            UINT16_C(0),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_RUN_BRAKE,
            UINT16_C(29),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_CROUCH_START,
            UINT16_C(8),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_CROUCH_END,
            UINT16_C(11),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT,
            UINT16_C(5),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_SPOT_DODGE,
            UINT16_C(33),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD,
            UINT16_C(32),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_ROLL_BACKWARD,
            UINT16_C(32),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_AIR_DODGE,
            UINT16_C(50),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL,
            UINT16_C(9),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_SPECIAL_LANDING,
            UINT16_C(11),
            NULL) != NULL ||
        pf_m4_falcon_reference_common_hurt_capsules_at_frame(
            (uint8_t)PF_M4_ACTION_LANDING,
            UINT16_C(31),
            NULL) != NULL)
    {
        return fail("falcon-reference-z-collision-source");
    }
    {
        const int64_t target_offset_x =
            (int64_t)down_tilt_spheres[0].offset_x_q16 -
            (int64_t)standing_hurt_capsules[0].endpoint_a_x_q16;
        pf_m4_collision_sphere3_q16 sphere = {
            (int64_t)down_tilt_spheres[0].offset_x_q16,
            (int64_t)down_tilt_spheres[0].offset_y_q16,
            (int64_t)down_tilt_spheres[0].offset_z_q16,
            (int64_t)down_tilt_spheres[0].radius_q16};
        const pf_m4_collision_capsule3_q16 capsule = {
            target_offset_x +
                (int64_t)standing_hurt_capsules[0].endpoint_a_x_q16,
            (int64_t)standing_hurt_capsules[0].endpoint_a_y_q16,
            (int64_t)standing_hurt_capsules[0].endpoint_a_z_q16,
            target_offset_x +
                (int64_t)standing_hurt_capsules[0].endpoint_b_x_q16,
            (int64_t)standing_hurt_capsules[0].endpoint_b_y_q16,
            (int64_t)standing_hurt_capsules[0].endpoint_b_z_q16,
            (int64_t)standing_hurt_capsules[0].radius_q16};

        if (pf_m4_collision_sphere_capsule_overlap_q16(
                &sphere,
                &capsule))
        {
            return fail("falcon-reference-z-collision-false-positive");
        }
        sphere.center_z_q16 = capsule.endpoint_a_z_q16;
        if (!pf_m4_collision_sphere_capsule_overlap_q16(
                &sphere,
                &capsule))
        {
            return fail("falcon-reference-z-collision-control");
        }
    }
    {
        const pf_m4_collision_capsule3_q16 moving_hit = {
            -INT64_C(2) * INT64_C(65536),
            INT64_C(0),
            INT64_C(0),
            INT64_C(2) * INT64_C(65536),
            INT64_C(0),
            INT64_C(0),
            INT64_C(16384)};
        pf_m4_collision_capsule3_q16 crossing_hurt = {
            INT64_C(0),
            -INT64_C(1) * INT64_C(65536),
            INT64_C(0),
            INT64_C(0),
            INT64_C(1) * INT64_C(65536),
            INT64_C(0),
            INT64_C(16384)};
        const pf_m4_collision_sphere3_q16 current_sphere = {
            INT64_C(2) * INT64_C(65536),
            INT64_C(0),
            INT64_C(0),
            INT64_C(16384)};

        if (pf_m4_collision_sphere_capsule_overlap_q16(
                &current_sphere,
                &crossing_hurt) ||
            !pf_m4_collision_capsule_capsule_overlap_q16(
                &moving_hit,
                &crossing_hurt))
        {
            return fail("falcon-reference-moving-hit-sweep");
        }
        crossing_hurt.endpoint_a_z_q16 = INT64_C(65536);
        crossing_hurt.endpoint_b_z_q16 = INT64_C(65536);
        if (pf_m4_collision_capsule_capsule_overlap_q16(
                &moving_hit,
                &crossing_hurt))
        {
            return fail("falcon-reference-moving-hit-sweep-z-miss");
        }
        crossing_hurt.endpoint_a_x_q16 =
            -INT64_C(1) * INT64_C(65536);
        crossing_hurt.endpoint_b_x_q16 =
            INT64_C(1) * INT64_C(65536);
        crossing_hurt.endpoint_a_y_q16 = INT64_C(24576);
        crossing_hurt.endpoint_b_y_q16 = INT64_C(24576);
        crossing_hurt.endpoint_a_z_q16 = INT64_C(0);
        crossing_hurt.endpoint_b_z_q16 = INT64_C(0);
        if (!pf_m4_collision_capsule_capsule_overlap_q16(
                &moving_hit,
                &crossing_hurt))
        {
            return fail("falcon-reference-moving-hit-parallel");
        }
        crossing_hurt.endpoint_a_y_q16 = INT64_C(49152);
        crossing_hurt.endpoint_b_y_q16 = INT64_C(49152);
        if (pf_m4_collision_capsule_capsule_overlap_q16(
                &moving_hit,
                &crossing_hurt))
        {
            return fail("falcon-reference-moving-hit-parallel-miss");
        }
    }

    for (submotion_index = UINT16_C(0);
         submotion_index < PF_M4_FALCON_SUBMOTION_COUNT;
         ++submotion_index)
    {
        const pf_m4_falcon_submotion_data *submotion =
            pf_m4_falcon_reference_submotion(submotion_index);
        const pf_m4_falcon_body_collision_timing *body_collision_timing =
            pf_m4_falcon_reference_body_collision_timing(submotion_index);
        uint32_t script_frame = UINT32_C(0);
        uint16_t expected_state_two_frame = UINT16_MAX;
        uint16_t expected_state_zero_frame = UINT16_MAX;
        uint16_t event_index;

        if (submotion == NULL || body_collision_timing == NULL ||
            submotion->event_count == UINT16_C(0))
        {
            return fail("falcon-reference-submotion-null");
        }
        if ((uint32_t)submotion->event_offset != script_event_count)
        {
            return fail("falcon-reference-submotion-event-span");
        }
        if ((uint32_t)submotion->translation_offset !=
            translation_sample_count)
        {
            return fail("falcon-reference-translation-span");
        }
        if (submotion->translation_count != UINT16_C(0))
        {
            uint16_t displayed_frame;

            if ((submotion->animation_flags & UINT32_C(0x80000000)) ==
                    UINT32_C(0) ||
                submotion->translation_count !=
                    submotion->gameplay_frame_count)
            {
                return fail("falcon-reference-translation-metadata");
            }
            for (displayed_frame = UINT16_C(1);
                 displayed_frame <= submotion->translation_count;
                 ++displayed_frame)
            {
                if (!pf_m4_falcon_reference_translation_q16(
                        submotion_index,
                        displayed_frame,
                        NULL,
                        NULL))
                {
                    return fail("falcon-reference-translation-sample");
                }
            }
            if (pf_m4_falcon_reference_translation_q16(
                    submotion_index,
                    UINT16_C(0),
                    NULL,
                    NULL) ||
                pf_m4_falcon_reference_translation_q16(
                    submotion_index,
                    (uint16_t)(submotion->translation_count + UINT16_C(1)),
                    NULL,
                    NULL))
            {
                return fail("falcon-reference-translation-bounds");
            }
            ++translation_submotion_count;
            translation_sample_count +=
                (uint32_t)submotion->translation_count;
        }
        else if ((submotion->animation_flags & UINT32_C(0x80000000)) !=
                 UINT32_C(0))
        {
            return fail("falcon-reference-missing-translation-span");
        }
        for (event_index = UINT16_C(0);
             event_index < submotion->event_count;
             ++event_index)
        {
            const uint8_t *event_bytes = NULL;
            const pf_m4_falcon_script_event *event =
                pf_m4_falcon_reference_submotion_event(
                    submotion_index,
                    event_index,
                    &event_bytes);

            if (event == NULL || event_bytes == NULL ||
                event->byte_count == UINT8_C(0) ||
                event->byte_count % UINT8_C(4) != UINT8_C(0) ||
                (uint32_t)event->byte_offset != script_byte_count ||
                (event_bytes[0] & UINT8_C(0xfc)) != event->command_id)
            {
                return fail("falcon-reference-action-script-event");
            }
            if ((event->command_id == UINT8_C(0x08) ||
                 event->command_id == UINT8_C(0x04)) &&
                event->byte_count == UINT8_C(4))
            {
                const uint32_t frame_argument =
                    (uint32_t)event_bytes[1] * UINT32_C(65536) +
                    (uint32_t)event_bytes[2] * UINT32_C(256) +
                    (uint32_t)event_bytes[3];

                script_frame = event->command_id == UINT8_C(0x08)
                                   ? frame_argument
                                   : script_frame + frame_argument;
            }
            else if (event->command_id == UINT8_C(0x68) &&
                     event->byte_count == UINT8_C(4))
            {
                if (script_frame > (uint32_t)UINT16_MAX)
                {
                    return fail("falcon-reference-body-collision-frame");
                }
                if (event_bytes[3] == UINT8_C(2) &&
                    expected_state_two_frame == UINT16_MAX)
                {
                    expected_state_two_frame = (uint16_t)script_frame;
                }
                else if (event_bytes[3] == UINT8_C(0) &&
                         expected_state_two_frame != UINT16_MAX &&
                         expected_state_zero_frame == UINT16_MAX)
                {
                    expected_state_zero_frame = (uint16_t)script_frame;
                }
            }
            script_byte_count += (uint32_t)event->byte_count;
            ++script_event_count;
        }
        if (body_collision_timing->state_two_frame !=
                expected_state_two_frame ||
            body_collision_timing->state_zero_frame !=
                expected_state_zero_frame)
        {
            return fail("falcon-reference-body-collision-timing");
        }
        {
            const uint8_t *event_bytes = (const uint8_t *)submotion;

            if (pf_m4_falcon_reference_submotion_event(
                    submotion_index,
                    submotion->event_count,
                    &event_bytes) != NULL ||
                event_bytes != NULL)
            {
                return fail("falcon-reference-action-script-event-bounds");
            }
        }
        if (submotion->animation_frame_count == UINT16_C(0))
        {
            if (submotion->gameplay_frame_count != UINT16_C(0) ||
                submotion->animation_size != UINT32_C(0))
            {
                return fail("falcon-reference-empty-submotion");
            }
            ++empty_submotion_count;
        }
        else
        {
            if ((uint32_t)submotion->gameplay_frame_count + UINT32_C(1) !=
                    (uint32_t)submotion->animation_frame_count ||
                submotion->animation_size == UINT32_C(0))
            {
                return fail("falcon-reference-animated-submotion");
            }
            ++animated_submotion_count;
        }
    }

    if (pf_m4_falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_COUNT) !=
            NULL ||
        animated_submotion_count != UINT32_C(275) ||
        empty_submotion_count != UINT32_C(43) ||
        translation_submotion_count != UINT32_C(65) ||
        translation_sample_count !=
            (uint32_t)PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT ||
        script_event_count !=
            (uint32_t)PF_M4_FALCON_SCRIPT_EVENT_COUNT ||
        script_byte_count !=
            (uint32_t)PF_M4_FALCON_SCRIPT_BYTE_COUNT ||
        submotion_catalog_sha256 == NULL ||
        submotion_catalog_sha256[0] != UINT8_C(0x9b) ||
        submotion_catalog_sha256[31] != UINT8_C(0xce) ||
        action_script_sha256 == NULL ||
        action_script_sha256[0] != UINT8_C(0x6b) ||
        action_script_sha256[31] != UINT8_C(0xfe) ||
        animation_tracks_sha256 == NULL ||
        animation_tracks_sha256[0] != UINT8_C(0xd8) ||
        animation_tracks_sha256[31] != UINT8_C(0xdc) ||
        animation_decode_summary == NULL ||
        animation_decode_summary->node_count != UINT32_C(17271) ||
        animation_decode_summary->track_count != UINT32_C(38560) ||
        animation_decode_summary->key_count != UINT32_C(308057) ||
        dash_submotion == NULL ||
        dash_submotion->animation_frame_count != UINT16_C(29) ||
        dash_submotion->gameplay_frame_count != UINT16_C(28) ||
        dash_submotion->event_count != UINT16_C(9) ||
        dash_submotion->animation_flags != UINT32_C(0x80000002) ||
        fall_special_submotion == NULL ||
        fall_special_submotion->animation_frame_count != UINT16_C(8) ||
        fall_special_submotion->gameplay_frame_count != UINT16_C(7) ||
        fall_special_submotion->event_count != UINT16_C(2) ||
        fall_special_submotion->animation_flags != UINT32_C(0x40000002) ||
        fall_special_submotion->translation_count != UINT16_C(0) ||
        landing_fall_special_submotion == NULL ||
        landing_fall_special_submotion->animation_frame_count !=
            UINT16_C(30) ||
        landing_fall_special_submotion->gameplay_frame_count !=
            UINT16_C(29) ||
        landing_fall_special_submotion->event_count != UINT16_C(4) ||
        landing_fall_special_submotion->animation_flags != UINT32_C(2) ||
        landing_fall_special_submotion->translation_count != UINT16_C(0) ||
        empty_submotion == NULL ||
        empty_submotion->animation_frame_count != UINT16_C(0) ||
        empty_submotion->animation_size != UINT32_C(0))
    {
        return fail("falcon-reference-submotion-completeness");
    }
    {
        const uint8_t *wait_bytes = NULL;
        const uint8_t *body_bytes = NULL;
        const uint8_t *reverse_bytes = NULL;
        const pf_m4_falcon_script_event *wait_event =
            pf_m4_falcon_reference_submotion_event(
                PF_M4_FALCON_SUBMOTION_SPOT_DODGE,
                UINT16_C(1),
                &wait_bytes);
        const pf_m4_falcon_script_event *body_event =
            pf_m4_falcon_reference_submotion_event(
                PF_M4_FALCON_SUBMOTION_SPOT_DODGE,
                UINT16_C(4),
                &body_bytes);
        const pf_m4_falcon_script_event *reverse_event =
            pf_m4_falcon_reference_submotion_event(
                PF_M4_FALCON_SUBMOTION_ROLL_FORWARD,
                UINT16_C(7),
                &reverse_bytes);
        const uint8_t expected_wait[4] = {
            UINT8_C(0x08), UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x03)};
        const uint8_t expected_body[4] = {
            UINT8_C(0x68), UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x02)};
        const uint8_t expected_reverse[4] = {
            UINT8_C(0x50), UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x00)};
        const uint8_t *invalid_bytes = (const uint8_t *)dash_submotion;

        if (wait_event == NULL || wait_event->command_id != UINT8_C(0x08) ||
            wait_event->byte_count != UINT8_C(4) ||
            memcmp(wait_bytes, expected_wait, sizeof(expected_wait)) != 0 ||
            body_event == NULL || body_event->command_id != UINT8_C(0x68) ||
            body_event->byte_count != UINT8_C(4) ||
            memcmp(body_bytes, expected_body, sizeof(expected_body)) != 0 ||
            reverse_event == NULL ||
            reverse_event->command_id != UINT8_C(0x50) ||
            reverse_event->byte_count != UINT8_C(4) ||
            memcmp(reverse_bytes, expected_reverse, sizeof(expected_reverse)) !=
                0 ||
            pf_m4_falcon_reference_submotion_event(
                PF_M4_FALCON_SUBMOTION_COUNT,
                UINT16_C(0),
                &invalid_bytes) != NULL ||
            invalid_bytes != NULL ||
            pf_m4_falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_COUNT) != NULL)
        {
            return fail("falcon-reference-action-script-source-bytes");
        }
    }

    for (mapped_move = PF_M4_FALCON_JAB1;
         mapped_move < PF_M4_FALCON_MOVE_COUNT;
         mapped_move = (pf_m4_falcon_move_index)(mapped_move + 1))
    {
        const pf_m4_reference_move *move =
            pf_m4_falcon_reference_move(mapped_move);

        if (move == NULL)
        {
            return fail("falcon-reference-complete-table-null");
        }
        present_count += move->present != UINT8_C(0)
                             ? UINT32_C(1)
                             : UINT32_C(0);
        if (mapped_move >= PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND)
        {
            uint8_t first_pose_count = UINT8_C(0);
            uint8_t last_pose_count = UINT8_C(0);

            if (move->present == UINT8_C(0) ||
                pf_m4_falcon_reference_hurt_capsules_at_frame(
                    mapped_move,
                    UINT16_C(1),
                    &first_pose_count) == NULL ||
                pf_m4_falcon_reference_hurt_capsules_at_frame(
                    mapped_move,
                    move->total_frames,
                    &last_pose_count) == NULL ||
                first_pose_count != UINT8_C(11) ||
                last_pose_count != UINT8_C(11) ||
                pf_m4_falcon_reference_hurt_capsules_at_frame(
                    mapped_move,
                    (uint16_t)(move->total_frames + UINT16_C(1)),
                    NULL) != NULL)
            {
                return fail("falcon-reference-special-pose-completeness");
            }
        }
    }

    if (jab == NULL || jab->present != UINT8_C(1) ||
        jab->total_frames != UINT16_C(21) ||
        jab->phase_count != UINT8_C(1) ||
        jab->effect_count != UINT8_C(1) ||
        jab_phase == NULL || jab_phase->first_frame != UINT16_C(3) ||
        jab_phase->last_frame != UINT16_C(5) ||
        jab_phase->effect_mask != UINT16_C(1) ||
        jab_effect == NULL || jab_effect->damage != UINT8_C(2) ||
        jab_effect->angle_degrees != UINT16_C(80) ||
        jab_effect->growth != UINT16_C(100) ||
        jab_effect->weight_set != UINT16_C(20) ||
        jab_effect->base != UINT16_C(0) ||
        jab_timing.startup_ticks != UINT16_C(2) ||
        jab_timing.active_ticks != UINT16_C(3) ||
        jab_timing.recovery_ticks != UINT16_C(16) ||
        jab->iasa_frame != UINT16_C(16) ||
        !pf_m4_falcon_reference_attack_matches(
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK,
            jab_timing,
            UINT32_C(2) * UINT32_C(65536)) ||
        pf_m4_falcon_reference_attack_matches(
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK,
            jab_timing,
            UINT32_C(3) * UINT32_C(65536)))
    {
        return fail("falcon-reference-jab");
    }
    if (dash == NULL || dash->animation_flags != UINT32_C(0x80000002) ||
        dash->motion_count != UINT16_C(39) ||
        !pf_m4_falcon_reference_motion_x_q16(
            (uint8_t)PF_M4_ACTION_DASH_ATTACK,
            UINT16_C(1),
            &dash_motion_q16) ||
        dash_motion_q16 != INT32_C(8930) ||
        pf_m4_falcon_reference_motion_x_q16(
            (uint8_t)PF_M4_ACTION_DASH_ATTACK,
            UINT16_C(0),
            NULL) ||
        pf_m4_falcon_reference_motion_x_q16(
            (uint8_t)PF_M4_ACTION_DASH_ATTACK,
            UINT16_C(40),
            NULL) ||
        dash_timing.startup_ticks != UINT16_C(6) ||
        dash_timing.active_ticks != UINT16_C(10) ||
        dash_timing.recovery_ticks != UINT16_C(23) ||
        standing_grab_timing.startup_ticks != UINT16_C(5) ||
        standing_grab_timing.active_ticks != UINT16_C(2) ||
        standing_grab_timing.recovery_ticks != UINT16_C(22) ||
        dash_grab_timing.startup_ticks != UINT16_C(9) ||
        dash_grab_timing.active_ticks != UINT16_C(2) ||
        dash_grab_timing.recovery_ticks != UINT16_C(28) ||
        missing == NULL || missing->present != UINT8_C(0) ||
        missing_low == NULL || missing_low->present != UINT8_C(0) ||
        present_count != UINT32_C(48) ||
        special_n == NULL || special_n->present != UINT8_C(1) ||
        special_n->subaction_index != UINT16_C(301) ||
        special_n->total_frames != UINT16_C(99) ||
        special_n->iasa_frame != UINT16_C(65) ||
        special_s_ground == NULL ||
        special_s_ground->subaction_index != UINT16_C(303) ||
        special_s_ground->total_frames != UINT16_C(79) ||
        special_s_ground_hit == NULL ||
        special_s_ground_hit->subaction_index != UINT16_C(304) ||
        special_s_ground_hit->total_frames != UINT16_C(24) ||
        special_s_air == NULL ||
        special_s_air->subaction_index != UINT16_C(305) ||
        special_s_air->total_frames != UINT16_C(79) ||
        special_s_air_hit == NULL ||
        special_s_air_hit->subaction_index != UINT16_C(306) ||
        special_s_air_hit->total_frames != UINT16_C(44) ||
        special_s_ground_effect == NULL ||
        special_s_ground_effect->damage != UINT8_C(7) ||
        special_s_ground_effect->angle_degrees != UINT16_C(90) ||
        special_s_ground_effect->growth != UINT16_C(80) ||
        special_s_ground_effect->base != UINT16_C(78) ||
        special_s_ground_effect->shield_damage != UINT8_C(2) ||
        special_s_ground_effect->element !=
            (uint8_t)PF_M4_REFERENCE_HIT_FIRE ||
        special_s_air_effect == NULL ||
        special_s_air_effect->damage != UINT8_C(7) ||
        special_s_air_effect->angle_degrees != UINT16_C(270) ||
        special_s_air_effect->growth != UINT16_C(70) ||
        special_s_air_effect->base != UINT16_C(60) ||
        special_s_air_effect->shield_damage != UINT8_C(2) ||
        special_s_air_effect->element !=
            (uint8_t)PF_M4_REFERENCE_HIT_FIRE ||
        special_last == NULL || special_last->present != UINT8_C(1) ||
        special_last->subaction_index != UINT16_C(317) ||
        special_last->total_frames != UINT16_C(59) ||
        missing_timing.startup_ticks != UINT16_C(0) ||
        missing_timing.active_ticks != UINT16_C(0) ||
        missing_timing.recovery_ticks != UINT16_C(0))
    {
        return fail("falcon-reference-timing");
    }
    if (jab_spheres == NULL || jab_sphere_count != UINT8_C(3) ||
        jab_spheres[0].offset_x_q16 != INT32_C(72548) ||
        jab_spheres[0].offset_y_q16 != INT32_C(-73843) ||
        jab_spheres[0].offset_z_q16 != INT32_C(0) ||
        jab_spheres[0].radius_q16 != INT32_C(24040) ||
        jab_spheres[0].effect_index != UINT8_C(0) ||
        jab_spheres[0].hitbox_id != UINT8_C(0) ||
        jab_spheres[0].group_id != UINT8_C(0) ||
        jab_spheres[0].collision_state != UINT8_C(2) ||
        jab_continuing_spheres == NULL ||
        jab_continuing_sphere_count != UINT8_C(3) ||
        jab_continuing_spheres[0].collision_state != UINT8_C(3) ||
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_JAB1,
            UINT16_C(2),
            NULL) != NULL ||
        jab2_spheres == NULL || jab2_sphere_count != UINT8_C(3) ||
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_JAB2,
            UINT16_C(4),
            NULL) != NULL ||
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_UP_AERIAL,
            UINT16_C(14),
            NULL) != NULL ||
        up_smash_spheres == NULL ||
        up_smash_sphere_count != UINT8_C(4) ||
        up_smash_spheres[0].effect_index != UINT8_C(0) ||
        up_smash_spheres[0].hitbox_id != UINT8_C(0) ||
        up_smash_spheres[1].effect_index != UINT8_C(1) ||
        up_smash_spheres[1].hitbox_id != UINT8_C(1) ||
        up_smash_spheres[2].effect_index != UINT8_C(2) ||
        up_smash_spheres[2].hitbox_id != UINT8_C(2) ||
        up_smash_spheres[3].effect_index != UINT8_C(3) ||
        up_smash_spheres[3].hitbox_id != UINT8_C(3) ||
        grab_spheres == NULL || grab_sphere_count != UINT8_C(2) ||
        grab_spheres[0].offset_x_q16 != INT32_C(46638) ||
        grab_spheres[0].offset_y_q16 != INT32_C(-67366) ||
        grab_spheres[0].radius_q16 != INT32_C(26711) ||
        grab_spheres[0].hitbox_id != UINT8_C(0) ||
        grab_spheres[1].offset_x_q16 != INT32_C(15546) ||
        grab_spheres[1].hitbox_id != UINT8_C(1) ||
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_GRAB,
            UINT16_C(6),
            NULL) != NULL ||
        dash_grab_spheres == NULL ||
        dash_grab_sphere_count != UINT8_C(3) ||
        dash_grab_spheres[0].offset_x_q16 != INT32_C(57002) ||
        dash_grab_spheres[0].offset_y_q16 != INT32_C(-62184) ||
        dash_grab_spheres[2].offset_x_q16 != INT32_C(-15546) ||
        dash_grab_spheres[2].hitbox_id != UINT8_C(2) ||
        neutral_special_spheres == NULL ||
        neutral_special_sphere_count != UINT8_C(3) ||
        neutral_special_spheres[0].effect_index != UINT8_C(0) ||
        neutral_special_spheres[0].hitbox_id != UINT8_C(0) ||
        neutral_special_spheres[1].effect_index != UINT8_C(1) ||
        neutral_special_spheres[1].hitbox_id != UINT8_C(1) ||
        neutral_special_spheres[2].effect_index != UINT8_C(2) ||
        neutral_special_spheres[2].hitbox_id != UINT8_C(2) ||
        neutral_special_air_spheres == NULL ||
        neutral_special_air_sphere_count != UINT8_C(3) ||
        neutral_special_air_spheres[0].radius_q16 != INT32_C(36060) ||
        neutral_special_air_spheres[1].radius_q16 != INT32_C(32054) ||
        side_special_ground_search == NULL ||
        side_special_ground_search_count != UINT8_C(3) ||
        side_special_ground_search[0].offset_x_q16 != INT32_C(41456) ||
        side_special_ground_search[0].offset_y_q16 != INT32_C(-21039) ||
        side_special_ground_search[0].radius_q16 != INT32_C(27352) ||
        side_special_ground_search[2].offset_y_q16 != INT32_C(-98354) ||
        side_special_air_search == NULL ||
        side_special_air_search_count != UINT8_C(3) ||
        side_special_air_search[0].offset_x_q16 != INT32_C(38865) ||
        side_special_air_search[1].offset_y_q16 != INT32_C(-66330) ||
        side_special_air_search[2].offset_y_q16 != INT32_C(32025) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_NEUTRAL_SPECIAL_AIR) ||
        down_special_ground_spheres == NULL ||
        down_special_ground_sphere_count != UINT8_C(3) ||
        down_special_ground_spheres[0].effect_index != UINT8_C(2) ||
        down_special_ground_spheres[1].effect_index != UINT8_C(2) ||
        down_special_ground_spheres[2].effect_index != UINT8_C(0) ||
        down_special_air_spheres == NULL ||
        down_special_air_sphere_count != UINT8_C(2) ||
        down_special_air_spheres[0].effect_index != UINT8_C(2) ||
        down_special_air_spheres[1].effect_index != UINT8_C(2) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_DOWN_SPECIAL_GROUND) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_DOWN_SPECIAL_AIR) ||
        pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_SIDE_SPECIAL_START_GROUND) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_SIDE_SPECIAL_HIT_GROUND) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_SIDE_SPECIAL_HIT_AIR) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_UP_SPECIAL_GROUND) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_UP_SPECIAL_AIR) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_UP_SPECIAL_CATCH) ||
        !pf_m4_falcon_reference_has_hit_geometry(
            PF_M4_FALCON_DOWN_SPECIAL_LANDING_HIT) ||
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_DOWN_SPECIAL_LANDING_HIT,
            UINT16_C(0),
            NULL) == NULL ||
        pf_m4_falcon_reference_hit_spheres_at_frame(
            PF_M4_FALCON_DASH_GRAB,
            UINT16_C(10),
            NULL) != NULL ||
        standing_hurt_capsules == NULL ||
        standing_hurt_capsule_count != UINT8_C(11) ||
        standing_hurt_capsules[0].endpoint_a_x_q16 != INT32_C(-1667) ||
        standing_hurt_capsules[0].endpoint_a_y_q16 != INT32_C(-71491) ||
        standing_hurt_capsules[0].endpoint_a_z_q16 != INT32_C(2338) ||
        standing_hurt_capsules[0].endpoint_b_x_q16 != INT32_C(259) ||
        standing_hurt_capsules[0].endpoint_b_y_q16 != INT32_C(-70872) ||
        standing_hurt_capsules[0].endpoint_b_z_q16 != INT32_C(-3980) ||
        standing_hurt_capsules[0].radius_q16 != INT32_C(16412) ||
        standing_hurt_capsules[0].hurtbox_id != UINT8_C(0) ||
        standing_hurt_capsules[0].height != UINT8_C(1) ||
        standing_hurt_capsules[0].grabbable != UINT8_C(1) ||
        complete_source_sha256 == NULL ||
        complete_source_sha256[0] != UINT8_C(0xc5) ||
        complete_source_sha256[31] != UINT8_C(0xd8) ||
        common_attribute_bits == NULL ||
        common_attribute_count != UINT16_C(97) ||
        common_attribute_bits[0] != UINT32_C(0x3e19999a) ||
        common_attribute_bits[96] != UINT32_C(0x07000000) ||
        common_attributes == NULL ||
        common_attributes->walk_maximum_velocity_q16 != INT32_C(5813) ||
        common_attributes->dash_initial_velocity_q16 != INT32_C(13677) ||
        common_attributes->ground_maximum_horizontal_velocity_q16 !=
            INT32_C(20516) ||
        common_attributes->jump_vertical_initial_velocity_q16 !=
            INT32_C(36045) ||
        common_attributes->gravity_q16 != INT32_C(1512) ||
        common_attributes->maximum_horizontal_air_velocity_q16 !=
            INT32_C(20516) ||
        common_attributes->weight != UINT16_C(104) ||
        common_attributes->down_aerial_landing_lag_ticks !=
            UINT16_C(24) ||
        ledge_attributes == NULL ||
        ledge_attributes->snap_x_q16 != INT32_C(61547) ||
        ledge_attributes->snap_y_q16 != INT32_C(197665) ||
        ledge_attributes->snap_height_q16 != INT32_C(127901) ||
        special_attributes == NULL ||
        special_attributes->specialn_stick_range_y_neg_q16 !=
            INT32_C(8192) ||
        special_attributes->specialn_vel_x_q16 != INT32_C(127795) ||
        special_attributes->specials_gr_vel_x_q16 != INT32_C(11796) ||
        special_attributes->specials_grav_q16 != INT32_C(3277) ||
        special_attributes->specials_terminal_vel_q16 != INT32_C(208404) ||
        special_attributes->specials_miss_landing_lag_q16 !=
            INT32_C(1310720) ||
        special_attributes->specials_hit_landing_lag_q16 !=
            INT32_C(2621440) ||
        special_attributes->specialhi_landing_lag_q16 !=
            INT32_C(1966080) ||
        special_attributes->speciallw_on_hit_spd_modifier_q16 !=
            INT32_C(39322) ||
        special_attributes->speciallw_air_landing_traction_q16 !=
            INT32_C(196608) ||
        common_special_attributes == NULL ||
        common_special_attributes
                ->fast_ground_friction_multiplier_q16 !=
            INT32_C(131072) ||
        common_special_attributes
                ->air_drift_over_maximum_deceleration_q16 !=
            INT32_C(205) ||
        common_special_attributes->side_special_stick_threshold_q16 !=
            INT32_C(39322) ||
        common_special_attributes->side_special_turn_threshold_q16 !=
            INT32_C(13107) ||
        common_special_attributes->air_drift_dead_zone_q16 !=
            INT32_C(6554) ||
        air_dodge_attributes == NULL ||
        air_dodge_attributes->initial_velocity_x_q16 != INT32_C(19080) ||
        air_dodge_attributes->initial_velocity_y_q16 != INT32_C(32440) ||
        air_dodge_attributes->decay_q16 != INT32_C(58982) ||
        air_dodge_attributes->dead_zone != UINT16_C(8192) ||
        air_dodge_attributes->item_throw_window_ticks != UINT16_C(3) ||
        air_dodge_attributes->ordinary_physics_begin_frame != UINT16_C(30) ||
        collision_pose == NULL ||
        collision_pose->air_dodge_bottom_y_from_origin_q16[0] !=
            INT32_C(0) ||
        collision_pose->air_dodge_bottom_y_from_origin_q16[47] !=
            INT32_C(24795) ||
        collision_pose->platform_drop_bottom_y_from_origin_q16[0] !=
            INT32_C(0) ||
        collision_pose->platform_drop_bottom_y_from_origin_q16[9] !=
            INT32_C(75621) ||
        collision_pose->platform_drop_bottom_y_from_origin_q16[29] !=
            INT32_C(21330) ||
        collision_pose->aerial_attack_bottom_y_from_origin_q16[0] !=
            INT32_C(0) ||
        collision_pose->aerial_attack_bottom_y_from_origin_q16[43] !=
            INT32_C(25538) ||
        collision_pose->aerial_attack_bottom_y_from_origin_q16[82] !=
            INT32_C(24952) ||
        collision_pose->aerial_attack_bottom_y_from_origin_q16[117] !=
            INT32_C(22943) ||
        collision_pose->aerial_attack_bottom_y_from_origin_q16[150] !=
            INT32_C(25930) ||
        collision_pose->aerial_attack_bottom_y_from_origin_q16[194] !=
            INT32_C(41004) ||
        jump_forward_first_ecb == NULL ||
        jump_forward_first_ecb->top_y_from_origin_q16 != INT32_C(203229) ||
        jump_forward_first_ecb->bottom_y_from_origin_q16 != INT32_C(0) ||
        jump_forward_last_ecb == NULL ||
        jump_forward_last_ecb->bottom_y_from_origin_q16 != INT32_C(25486) ||
        jump_backward_last_ecb == NULL ||
        jump_backward_last_ecb->bottom_y_from_origin_q16 != INT32_C(23099) ||
        jump_aerial_forward_last_ecb == NULL ||
        jump_aerial_forward_last_ecb->right_x_from_origin_q16 !=
            INT32_C(43008) ||
        jump_aerial_backward_last_ecb == NULL ||
        jump_aerial_backward_last_ecb->left_x_from_origin_q16 !=
            INT32_C(-30225) ||
        fall_first_ecb == NULL ||
        fall_first_ecb->bottom_y_from_origin_q16 != INT32_C(23231) ||
        fall_aerial_last_ecb == NULL ||
        fall_aerial_last_ecb->bottom_y_from_origin_q16 != INT32_C(37164) ||
        collision_pose->ceiling_bounce[0].right_x_from_origin_q16 !=
            INT32_C(45697) ||
        collision_pose->ceiling_bounce[8].left_x_from_origin_q16 !=
            INT32_C(-61548) ||
        collision_pose->ceiling_bounce[8].top_y_from_origin_q16 !=
            INT32_C(100691) ||
        collision_pose->wall_bounce[0].top_y_from_origin_q16 !=
            INT32_C(170569) ||
        collision_pose->wall_bounce[50].right_y_from_origin_q16 !=
            INT32_C(89066) ||
        collision_pose->damage_fly_top_y_from_origin_q16[0] !=
            INT32_C(167400) ||
        collision_pose->damage_fly_top_y_from_origin_q16[23] !=
            INT32_C(155432) ||
        collision_pose->damage_fly_side_x_from_origin_q16[0] !=
            INT32_C(25723) ||
        collision_pose->damage_fly_side_x_from_origin_q16[23] !=
            INT32_C(34052) ||
        collision_pose->damage_fly_side_y_from_origin_q16[0] !=
            INT32_C(113769) ||
        collision_pose->damage_fly_side_y_from_origin_q16[23] !=
            INT32_C(122372) ||
        collision_pose->raptor_boost_hit_air_bottom_y_from_origin_q16[0] !=
            INT32_C(25701) ||
        collision_pose->raptor_boost_hit_air_bottom_y_from_origin_q16[34] !=
            INT32_C(19452) ||
        collision_pose->raptor_boost_hit_air_bottom_y_from_origin_q16[44] !=
            INT32_C(27795) ||
        stale_move_data == NULL ||
        stale_move_data->slot_reduction_q16[0] != UINT16_C(5898) ||
        stale_move_data->slot_reduction_q16[8] != UINT16_C(655) ||
        side_special_timing == NULL ||
        side_special_timing->ground_search_begin_frame != UINT16_C(15) ||
        side_special_timing->ground_search_end_frame != UINT16_C(34) ||
        side_special_timing->air_search_begin_frame != UINT16_C(18) ||
        side_special_timing->air_search_end_frame != UINT16_C(34) ||
        side_special_timing->air_gravity_begin_frame != UINT16_C(30) ||
        up_special_timing == NULL ||
        up_special_timing->victim_release_hitstun_ticks != UINT16_C(26) ||
        down_special_timing == NULL ||
        down_special_timing->ground_wall_rebound_begin_frame !=
            UINT16_C(15) ||
        down_special_timing->air_wall_rebound_begin_frame !=
            UINT16_C(15) ||
        down_special_timing->ground_end_traction_begin_frame !=
            UINT16_C(2) ||
        down_special_timing->ground_end_traction_end_frame !=
            UINT16_C(29) ||
        down_special_timing->landing_traction_begin_frame !=
            UINT16_C(0) ||
        down_special_timing->landing_traction_end_frame !=
            UINT16_C(22) ||
        down_special_timing->ground_origin_air_physics_begin_frame !=
            UINT16_C(8) ||
        down_special_timing->ground_end_entry_velocity_scale_q16 !=
            INT32_C(52429) ||
        jab_hurt_capsules == NULL ||
        jab_hurt_capsule_count != UINT8_C(11) ||
        jab_hurt_capsules[0].endpoint_a_x_q16 != INT32_C(1374) ||
        jab_hurt_capsules[0].endpoint_a_y_q16 != INT32_C(-61868) ||
        jab_hurt_capsules[10].endpoint_b_x_q16 != INT32_C(15528) ||
        jab_hurt_capsules[10].endpoint_b_y_q16 != INT32_C(-16299) ||
        nair_hurt_capsules == NULL ||
        nair_hurt_capsule_count != UINT8_C(11) ||
        nair_hurt_capsules[0].endpoint_a_x_q16 != INT32_C(5704) ||
        nair_hurt_capsules[10].endpoint_b_y_q16 != INT32_C(7436) ||
        grab_hurt_capsules == NULL ||
        grab_hurt_capsule_count != UINT8_C(11) ||
        grab_hurt_capsules[0].endpoint_a_x_q16 != INT32_C(-3099) ||
        neutral_special_hurt_capsules == NULL ||
        neutral_special_hurt_capsule_count != UINT8_C(11) ||
        neutral_special_air_hurt_capsules == NULL ||
        neutral_special_air_hurt_capsule_count != UINT8_C(11) ||
        geometry_sha256 == NULL ||
        geometry_sha256[0] != UINT8_C(0x37) ||
        geometry_sha256[1] != UINT8_C(0x7f) ||
        geometry_sha256[30] != UINT8_C(0xe6) ||
        geometry_sha256[31] != UINT8_C(0x45) ||
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_JAB1,
            UINT16_C(0),
            NULL) != NULL ||
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_NEUTRAL_AERIAL,
            UINT16_C(45),
            NULL) != NULL ||
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_NEUTRAL_SPECIAL_GROUND,
            UINT16_C(100),
            NULL) != NULL ||
        pf_m4_falcon_reference_hurt_capsules_at_frame(
            PF_M4_FALCON_NEUTRAL_SPECIAL_AIR,
            UINT16_C(100),
            NULL) != NULL)
    {
        return fail("falcon-reference-hit-geometry");
    }
    if (ledge_attack_slow == NULL ||
        ledge_attack_slow->total_frames != UINT16_C(68) ||
        ledge_attack_slow->first_active_frame != UINT16_C(37) ||
        ledge_attack_slow->last_active_frame != UINT16_C(40) ||
        ledge_attack_slow->effect.damage != UINT8_C(8) ||
        ledge_attack_slow->effect.angle_degrees != UINT16_C(361) ||
        ledge_attack_slow->effect.growth != UINT16_C(100) ||
        ledge_attack_slow->effect.weight_set != UINT16_C(90) ||
        ledge_attack_slow->effect.base != UINT16_C(0) ||
        ledge_attack_slow->effect.shield_damage != UINT8_C(1) ||
        ledge_attack_slow->effect.interaction != UINT8_C(2) ||
        ledge_attack_slow->effect.element !=
            (uint8_t)PF_M4_REFERENCE_HIT_NORMAL ||
        ledge_attack_quick == NULL ||
        ledge_attack_quick->total_frames != UINT16_C(54) ||
        ledge_attack_quick->first_active_frame != UINT16_C(24) ||
        ledge_attack_quick->last_active_frame != UINT16_C(29) ||
        ledge_attack_quick->effect.damage != UINT8_C(10) ||
        ledge_attack_quick->effect.angle_degrees != UINT16_C(361) ||
        ledge_attack_quick->effect.growth != UINT16_C(100) ||
        ledge_attack_quick->effect.weight_set != UINT16_C(90) ||
        ledge_attack_quick->effect.base != UINT16_C(0) ||
        ledge_attack_quick->effect.shield_damage != UINT8_C(1) ||
        ledge_attack_quick->effect.interaction != UINT8_C(2) ||
        ledge_attack_quick->effect.element !=
            (uint8_t)PF_M4_REFERENCE_HIT_NORMAL ||
        pf_m4_falcon_reference_ledge_attack(UINT16_C(220)) != NULL ||
        pf_m4_falcon_reference_ledge_attack(UINT16_C(223)) != NULL)
    {
        return fail("falcon-reference-ledge-attack");
    }
    if (forward_throw == NULL ||
        forward_throw->damage != UINT8_C(4) ||
        forward_throw->angle_degrees != UINT16_C(45) ||
        forward_throw->growth != UINT16_C(105) ||
        forward_throw->base != UINT16_C(11) ||
        forward_throw->release_frame != UINT16_C(18) ||
        back_throw == NULL || back_throw->damage != UINT8_C(4) ||
        back_throw->angle_degrees != UINT16_C(135) ||
        back_throw->growth != UINT16_C(130) ||
        back_throw->base != UINT16_C(7) ||
        back_throw->release_frame != UINT16_C(20) ||
        up_throw == NULL || up_throw->damage != UINT8_C(3) ||
        up_throw->angle_degrees != UINT16_C(85) ||
        up_throw->growth != UINT16_C(105) ||
        up_throw->base != UINT16_C(17) ||
        up_throw->release_frame != UINT16_C(15) ||
        down_throw == NULL || down_throw->damage != UINT8_C(7) ||
        down_throw->angle_degrees != UINT16_C(65) ||
        down_throw->growth != UINT16_C(34) ||
        down_throw->weight_set != UINT16_C(0) ||
        down_throw->base != UINT16_C(18) ||
        down_throw->release_frame != UINT16_C(20) ||
        pf_m4_falcon_reference_move(
            (pf_m4_falcon_move_index)PF_M4_FALCON_MOVE_COUNT) != NULL ||
        pf_m4_falcon_reference_phase(
            PF_M4_FALCON_JAB1,
            UINT16_C(1)) != NULL ||
        pf_m4_falcon_reference_effect(
            PF_M4_FALCON_JAB1,
            UINT16_C(1)) != NULL ||
        pf_m4_falcon_reference_throw(PF_M4_FALCON_JAB1) != NULL)
    {
        return fail("falcon-reference-bounds-and-throw");
    }
    if (pf_m4_falcon_reference_phase_at_frame(
            PF_M4_FALCON_NEUTRAL_AERIAL,
            UINT16_C(13)) != NULL ||
        pf_m4_falcon_reference_effect_at_frame(
            PF_M4_FALCON_NEUTRAL_AERIAL,
            UINT16_C(13)) != NULL ||
        nair_late_phase == NULL ||
        nair_late_phase->first_frame != UINT16_C(20) ||
        nair_late_phase->last_frame != UINT16_C(29) ||
        nair_late_effect == NULL ||
        nair_late_effect->damage != UINT8_C(7) ||
        !pf_m4_falcon_reference_move_for_action(
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK,
            &mapped_move) ||
        mapped_move != PF_M4_FALCON_FORWARD_SMASH ||
        !pf_m4_falcon_reference_move_for_action(
            (uint8_t)PF_M4_ACTION_DOWN_AERIAL,
            NULL) ||
        !pf_m4_falcon_reference_move_for_action(
            (uint8_t)PF_M4_ACTION_DASH_GRAB,
            &mapped_move) ||
        mapped_move != PF_M4_FALCON_DASH_GRAB ||
        pf_m4_falcon_reference_move_for_action(
            (uint8_t)PF_M4_ACTION_STRONG_ATTACK,
            &mapped_move) ||
        pf_m4_falcon_reference_iasa_policy_for_action(
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK) !=
            PF_M4_REFERENCE_IASA_JAB_CHAIN ||
        pf_m4_falcon_reference_iasa_policy_for_action(
            (uint8_t)PF_M4_ACTION_DASH_ATTACK) !=
            PF_M4_REFERENCE_IASA_WAIT ||
        pf_m4_falcon_reference_iasa_policy_for_action(
            (uint8_t)PF_M4_ACTION_FORWARD_ATTACK) !=
            PF_M4_REFERENCE_IASA_WAIT ||
        pf_m4_falcon_reference_iasa_policy_for_action(
            (uint8_t)PF_M4_ACTION_DOWN_ATTACK) !=
            PF_M4_REFERENCE_IASA_DOWN_TILT ||
        pf_m4_falcon_reference_iasa_policy_for_action(
            (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK) !=
            PF_M4_REFERENCE_IASA_FORWARD_SMASH ||
        pf_m4_falcon_reference_iasa_policy_for_action(
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK) !=
            PF_M4_REFERENCE_IASA_NONE ||
        pf_m4_falcon_reference_ground_physics_for_action(
            (uint8_t)PF_M4_ACTION_DASH_ATTACK) !=
            PF_M4_REFERENCE_GROUND_PHYSICS_ROOT_MOTION ||
        pf_m4_falcon_reference_ground_physics_for_action(
            (uint8_t)PF_M4_ACTION_UP_ATTACK) !=
            PF_M4_REFERENCE_GROUND_PHYSICS_FRICTION ||
        pf_m4_falcon_reference_special_iasa_active(
            (uint8_t)PF_M4_ACTION_UP_ATTACK,
            UINT16_C(36)) ||
        !pf_m4_falcon_reference_special_iasa_active(
            (uint8_t)PF_M4_ACTION_UP_ATTACK,
            UINT16_C(37)) ||
        pf_m4_falcon_reference_special_iasa_active(
            (uint8_t)PF_M4_ACTION_DOWN_ATTACK,
            UINT16_C(34)))
    {
        return fail("falcon-reference-phase-and-action-route");
    }
    for (aerial_index = UINT32_C(0);
         aerial_index < UINT32_C(5);
         ++aerial_index)
    {
        const uint16_t before = autocancel_before[aerial_index];
        const uint16_t after = autocancel_after[aerial_index];
        const uint16_t iasa_frame = aerial_iasa_frames[aerial_index];
        const pf_m4_reference_move *aerial_move;

        if (!pf_m4_falcon_reference_move_for_action(
                aerial_actions[aerial_index],
                &mapped_move))
        {
            return fail("falcon-reference-aerial-action-map");
        }
        aerial_move = pf_m4_falcon_reference_move(mapped_move);

        if (aerial_move == NULL ||
            aerial_move->iasa_frame != iasa_frame ||
            pf_m4_falcon_reference_iasa_active(
                aerial_actions[aerial_index],
                iasa_frame == UINT16_C(0)
                    ? (uint32_t)aerial_move->total_frames
                    : (uint32_t)(iasa_frame - UINT16_C(1))) != 0 ||
            (iasa_frame != UINT16_C(0) &&
             pf_m4_falcon_reference_iasa_active(
                 aerial_actions[aerial_index],
                 (uint32_t)iasa_frame) == 0) ||
            pf_m4_falcon_reference_landing_lag_active(
                aerial_actions[aerial_index],
                (uint16_t)(before - UINT16_C(1))) != 0 ||
            pf_m4_falcon_reference_landing_lag_active(
                aerial_actions[aerial_index],
                before) != 1 ||
            pf_m4_falcon_reference_landing_lag_active(
                aerial_actions[aerial_index],
                after) != 1 ||
            pf_m4_falcon_reference_landing_lag_active(
                aerial_actions[aerial_index],
                (uint16_t)(after + UINT16_C(1))) != 0)
        {
            return fail("falcon-reference-aerial-autocancel");
        }
    }
    if (pf_m4_falcon_reference_landing_lag_active(
            (uint8_t)PF_M4_ACTION_GROUND_IDLE,
            UINT16_C(1)) != -1)
    {
        return fail("falcon-reference-aerial-autocancel-bounds");
    }
    return 1;
}

static int run_falcon_jab1_iasa_test(void)
{
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    uint32_t tick;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "falcon-jab1-iasa-default-content"))
    {
        return 0;
    }
    content.stage.spawn_spacing_q16 = INT32_C(10) * PF_Q16_ONE;
    content.item.enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "falcon-jab1-iasa-content-view"))
    {
        return 0;
    }

    /* A jump pulse on displayed frame 15 remains locked. */
    if (!initialize_sim(
            &storage,
            &view,
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
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(13); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(15))
    {
        return fail("falcon-jab1-frame15-jump-lock");
    }

    /* The same pulse on imported IASA frame 16 starts jump squat. */
    if (!initialize_sim(
            &storage,
            &view,
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
            &inspection))
    {
        return 0;
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
            return 0;
        }
    }
    if (!step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_JUMP,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_JUMP_SQUAT)
    {
        return fail("falcon-jab1-frame16-jump-iasa");
    }

    /* Jab 1's custom IASA excludes Guard, even on frame 16. */
    if (!initialize_sim(
            &storage,
            &view,
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
            &inspection))
    {
        return 0;
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
            return 0;
        }
    }
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
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(16) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_SPECIAL,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(17) ||
        !step_duel(
            sim,
            INT16_C(0),
            PF_INPUT_BUTTON_TAUNT,
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(18))
    {
        return fail("falcon-jab1-defense-special-taunt-lock");
    }

    /* A stick held before IASA follows Melee's Dash then Walk ordering. */
    if (!initialize_sim(
            &storage,
            &view,
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
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(15); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_MAX,
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_WALK)
    {
        return fail("falcon-jab1-frame16-held-stick-walk");
    }

    /* The source action displays frame 21 and exits on the next tick. */
    if (!initialize_sim(
            &storage,
            &view,
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
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(20); ++tick)
    {
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].action_ticks != UINT16_C(21) ||
        !step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("falcon-jab1-total-frames");
    }
    return 1;
}

static int prepare_falcon_ground_action_frame(
    test_sim_storage *storage,
    const pf_content_view *view,
    int16_t input_x,
    int16_t input_y,
    uint64_t input_buttons,
    pf_m4_action_state expected_action,
    uint16_t target_frame,
    pf_sim **out_sim,
    pf_m4_inspection *out_inspection)
{
    uint32_t tick;

    if (!initialize_sim(
            storage,
            view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            out_sim) ||
        !step_reaction_duel(
            *out_sim,
            input_x,
            input_y,
            input_buttons,
            UINT16_C(0),
            INT16_C(0),
            INT16_C(0),
            UINT64_C(0),
            UINT16_C(0),
            out_inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0);
         expected_action == PF_M4_ACTION_FORWARD_STRONG_ATTACK &&
         out_inspection->players[0].action_state ==
             (uint8_t)PF_M4_ACTION_FORWARD_STRONG_CHARGE &&
         tick < UINT32_C(120);
         ++tick)
    {
        if (!step_reaction_duel(
                *out_sim,
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
    if (out_inspection->players[0].action_state !=
        (uint8_t)expected_action)
    {
        return 0;
    }
    if (out_inspection->players[0].action_state !=
            (uint8_t)expected_action ||
        out_inspection->players[0].action_ticks == UINT16_C(0) ||
        out_inspection->players[0].action_ticks > target_frame)
    {
        return 0;
    }
    for (tick = out_inspection->players[0].action_ticks;
         tick < (uint32_t)target_frame;
         ++tick)
    {
        if (!step_duel(
                *out_sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                out_inspection))
        {
            return 0;
        }
    }
    return out_inspection->players[0].action_state ==
               (uint8_t)expected_action &&
           out_inspection->players[0].action_ticks == target_frame;
}

static int run_falcon_ground_iasa_policy_test(void)
{
    typedef struct falcon_ground_iasa_case
    {
        int16_t start_x;
        int16_t start_y;
        uint64_t start_buttons;
        pf_m4_action_state start_action;
        uint16_t start_frame;
        int16_t interrupt_x;
        int16_t interrupt_y;
        uint64_t interrupt_buttons;
        uint16_t interrupt_shield;
        pf_m4_action_state expected_action;
        uint16_t expected_frame;
        uint8_t expected_next_action;
        const char *failure;
    } falcon_ground_iasa_case;
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    const pf_m4_ssbm_ground_input_attributes *ground_input =
        pf_m4_ssbm_common_reference_ground_input();
    falcon_ground_iasa_case cases[6];
    int16_t up_tilt_y;
    uint32_t case_index;

    if (ground_input == NULL ||
        !expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "falcon-ground-iasa-default-content"))
    {
        return 0;
    }
    up_tilt_y =
        (int16_t)-(
            (int32_t)ground_input->vertical_smash_axis_threshold -
            INT32_C(1));
    content.stage.spawn_spacing_q16 = INT32_C(10) * PF_Q16_ONE;
    content.item.enabled = UINT8_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "falcon-ground-iasa-content-view"))
    {
        return 0;
    }

    cases[0] = (falcon_ground_iasa_case){
        INT16_C(0), up_tilt_y, PF_INPUT_BUTTON_ATTACK,
        PF_M4_ACTION_UP_ATTACK, UINT16_C(36),
        INT16_C(0), INT16_C(0), UINT64_C(0), UINT16_MAX,
        PF_M4_ACTION_UP_ATTACK, UINT16_C(37), UINT8_MAX,
        "falcon-utilt-frame37-guard-lock"};
    cases[1] = (falcon_ground_iasa_case){
        INT16_C(0), up_tilt_y, PF_INPUT_BUTTON_ATTACK,
        PF_M4_ACTION_UP_ATTACK, UINT16_C(37),
        INT16_C(0), INT16_C(0), UINT64_C(0), UINT16_MAX,
        PF_M4_ACTION_SHIELD, UINT16_MAX, UINT8_MAX,
        "falcon-utilt-frame38-guard-iasa"};
    cases[2] = (falcon_ground_iasa_case){
        INT16_C(0), up_tilt_y, PF_INPUT_BUTTON_ATTACK,
        PF_M4_ACTION_UP_ATTACK, UINT16_C(37),
        INT16_C(0), INT16_MAX, UINT64_C(0), UINT16_MAX,
        PF_M4_ACTION_SPOT_DODGE, UINT16_MAX, UINT8_MAX,
        "falcon-utilt-frame38-escape-iasa"};
    cases[3] = (falcon_ground_iasa_case){
        INT16_C(0),
        (int16_t)(ground_input->vertical_smash_axis_threshold -
                  UINT16_C(1)),
        PF_INPUT_BUTTON_ATTACK, PF_M4_ACTION_DOWN_ATTACK, UINT16_C(34),
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0),
        PF_M4_ACTION_DOWN_ATTACK, UINT16_C(35), UINT8_MAX,
        "falcon-dtilt-frame35-special-lock"};
    cases[4] = (falcon_ground_iasa_case){
        INT16_MAX, INT16_C(0), PF_INPUT_BUTTON_ATTACK,
        PF_M4_ACTION_FORWARD_STRONG_ATTACK, UINT16_C(59),
        INT16_C(0), INT16_MAX, UINT64_C(0), UINT16_MAX,
        PF_M4_ACTION_SHIELD, UINT16_MAX, UINT8_MAX,
        "falcon-fsmash-frame60-escape-excluded"};
    cases[5] = (falcon_ground_iasa_case){
        (int16_t)(content.fighter.axis_dead_zone + UINT16_C(1)),
        INT16_C(0), PF_INPUT_BUTTON_ATTACK, PF_M4_ACTION_FORWARD_ATTACK,
        UINT16_C(28), INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_JUMP,
        UINT16_C(0), PF_M4_ACTION_FORWARD_ATTACK, UINT16_C(29),
        (uint8_t)PF_M4_ACTION_GROUND_IDLE,
        "falcon-ftilt-no-iasa-and-total"};

    for (case_index = UINT32_C(0);
         case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index)
    {
        const falcon_ground_iasa_case *test = &cases[case_index];

        if (!prepare_falcon_ground_action_frame(
                &storage,
                &view,
                test->start_x,
                test->start_y,
                test->start_buttons,
                test->start_action,
                test->start_frame,
                &sim,
                &inspection) ||
            !step_reaction_duel(
                sim,
                test->interrupt_x,
                test->interrupt_y,
                test->interrupt_buttons,
                test->interrupt_shield,
                INT16_C(0),
                INT16_C(0),
                UINT64_C(0),
                UINT16_C(0),
                &inspection) ||
            inspection.players[0].action_state !=
                (uint8_t)test->expected_action ||
            (test->expected_frame != UINT16_MAX &&
             inspection.players[0].action_ticks != test->expected_frame) ||
            (test->expected_next_action != UINT8_MAX &&
             (!step_duel(
                  sim,
                  INT16_C(0),
                  UINT64_C(0),
                  INT16_C(0),
                  UINT64_C(0),
                  &inspection) ||
              inspection.players[0].action_state !=
                  test->expected_next_action)))
        {
            (void)fprintf(
                stderr,
                "m4-combat=diagnostic case=%s action=%u ticks=%u"
                " expected_action=%u expected_ticks=%u\n",
                test->failure,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.players[0].action_ticks,
                (unsigned int)test->expected_action,
                (unsigned int)test->expected_frame);
            return fail(test->failure);
        }
    }
    return 1;
}

static int run_battlefield_stage_catalog_test(void)
{
    const uint16_t profile_id =
        (uint16_t)PF_M4_REFERENCE_STAGE_BATTLEFIELD;
    const pf_m4_ssbm_stage_collision_profile *profile =
        pf_m4_ssbm_reference_stage_collision(profile_id);
    const uint8_t expected_spawn_supports[4] = {
        UINT8_C(2), UINT8_C(4), UINT8_C(3), UINT8_C(5)};
    const int32_t expected_spawn_x_q16[4] = {
        INT32_C(0), INT32_C(0), -INT32_C(265335), INT32_C(265335)};
    const int32_t expected_spawn_y_q16[4] = {
        INT32_C(1217701), INT32_C(585173),
        INT32_C(901437), INT32_C(901437)};
    test_sim_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_m4_inspection inspection;
    pf_sim *sim = NULL;
    pf_m4_reference_stage_line public_line;
    uint16_t public_line_count = UINT16_C(0);
    uint16_t line_index;
    uint8_t spawn_index;
    int32_t ceiling_y_q16 = INT32_C(0);
    uint8_t ceiling_support = UINT8_C(0);
    int32_t wall_x_q16 = INT32_C(0);
    uint8_t wall_support = UINT8_C(0);
    int8_t wall_away_direction = INT8_C(0);

    if (profile == NULL || profile->line_count != UINT16_C(23) ||
        profile->floor_count != UINT16_C(6) ||
        profile->ceiling_count != UINT16_C(5) ||
        profile->right_wall_count != UINT16_C(6) ||
        profile->left_wall_count != UINT16_C(6) ||
        profile->dynamic_count != UINT16_C(0) ||
        profile->source_grkind != UINT16_C(36) ||
        profile->spawn_point_count != UINT8_C(4) ||
        profile->spawn_points == NULL ||
        profile->camera_left_q16 != -INT32_C(1094166) ||
        profile->camera_right_q16 != INT32_C(1094166) ||
        profile->camera_top_q16 != -INT32_C(270600) ||
        profile->camera_bottom_q16 != INT32_C(1859531) ||
        profile->blast_left_q16 != -INT32_C(1531833) ||
        profile->blast_right_q16 != INT32_C(1531833) ||
        profile->blast_top_q16 != -INT32_C(1014751) ||
        profile->blast_bottom_q16 != INT32_C(2575776))
    {
        return fail("battlefield-stage-catalog-shape");
    }
    for (line_index = UINT16_C(0);
         line_index < profile->line_count;
         ++line_index)
    {
        if (profile->lines[line_index].source_index != (uint8_t)line_index ||
            pf_m4_ssbm_reference_stage_line(
                profile_id,
                (uint8_t)(line_index + UINT16_C(1))) !=
                &profile->lines[line_index])
        {
            return fail("battlefield-stage-catalog-dense-access");
        }
    }
    for (line_index = profile->ceiling_start;
         line_index < profile->ceiling_start + profile->ceiling_count;
         ++line_index)
    {
        const pf_m4_ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const int32_t midpoint_x_q16 = (int32_t)(
            ((int64_t)line->start_x_q16 + (int64_t)line->end_x_q16) /
            INT64_C(2));
        const int32_t midpoint_y_q16 =
            pf_m4_ssbm_stage_line_y_q16(line, midpoint_x_q16);

        if (!pf_m4_ssbm_reference_stage_find_ceiling_contact(
                profile_id,
                midpoint_x_q16,
                midpoint_y_q16 + INT32_C(1),
                midpoint_y_q16 - INT32_C(1),
                &ceiling_y_q16,
                &ceiling_support) ||
            ceiling_y_q16 != midpoint_y_q16 ||
            ceiling_support != (uint8_t)(line_index + UINT16_C(1)))
        {
            return fail("battlefield-stage-every-ceiling-query");
        }
    }
    for (line_index = profile->right_wall_start;
         line_index < profile->right_wall_start + profile->right_wall_count;
         ++line_index)
    {
        const pf_m4_ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const int32_t midpoint_x_q16 = (int32_t)(
            ((int64_t)line->start_x_q16 + (int64_t)line->end_x_q16) /
            INT64_C(2));
        const int32_t midpoint_y_q16 = (int32_t)(
            ((int64_t)line->start_y_q16 + (int64_t)line->end_y_q16) /
            INT64_C(2));

        if (!pf_m4_ssbm_reference_stage_find_wall_contact(
                profile_id,
                midpoint_x_q16 + INT32_C(1),
                midpoint_x_q16 - INT32_C(1),
                (int64_t)midpoint_y_q16 - INT64_C(1),
                (int64_t)midpoint_y_q16 + INT64_C(1),
                INT32_C(0),
                &wall_x_q16,
                &wall_support,
                &wall_away_direction) ||
            wall_support != (uint8_t)(line_index + UINT16_C(1)) ||
            wall_away_direction != INT8_C(1))
        {
            return fail("battlefield-stage-every-right-wall-query");
        }
    }
    for (line_index = profile->left_wall_start;
         line_index < profile->left_wall_start + profile->left_wall_count;
         ++line_index)
    {
        const pf_m4_ssbm_stage_collision_line *line =
            &profile->lines[line_index];
        const int32_t midpoint_x_q16 = (int32_t)(
            ((int64_t)line->start_x_q16 + (int64_t)line->end_x_q16) /
            INT64_C(2));
        const int32_t midpoint_y_q16 = (int32_t)(
            ((int64_t)line->start_y_q16 + (int64_t)line->end_y_q16) /
            INT64_C(2));

        if (!pf_m4_ssbm_reference_stage_find_wall_contact(
                profile_id,
                midpoint_x_q16 - INT32_C(1),
                midpoint_x_q16 + INT32_C(1),
                (int64_t)midpoint_y_q16 - INT64_C(1),
                (int64_t)midpoint_y_q16 + INT64_C(1),
                INT32_C(0),
                &wall_x_q16,
                &wall_support,
                &wall_away_direction) ||
            wall_support != (uint8_t)(line_index + UINT16_C(1)) ||
            wall_away_direction != INT8_C(-1))
        {
            return fail("battlefield-stage-every-left-wall-query");
        }
    }
    if (!pf_m4_ssbm_reference_stage_find_ceiling_contact(
            profile_id,
            INT32_C(0),
            INT32_C(1800000),
            INT32_C(1500000),
            &ceiling_y_q16,
            &ceiling_support) ||
        ceiling_y_q16 != profile->lines[8].start_y_q16 ||
        ceiling_support != UINT8_C(9) ||
        pf_m4_ssbm_reference_stage_find_ceiling_contact(
            profile_id,
            INT32_C(0),
            INT32_C(1500000),
            INT32_C(1800000),
            &ceiling_y_q16,
            &ceiling_support) ||
        pf_m4_ssbm_reference_stage_find_ceiling_contact(
            profile_id,
            INT32_C(600000),
            INT32_C(1800000),
            INT32_C(1500000),
            &ceiling_y_q16,
            &ceiling_support))
    {
        return fail("battlefield-stage-ceiling-query");
    }
    if (pf_m4_ssbm_reference_stage_line(
            profile_id,
            (uint8_t)(profile->line_count + UINT16_C(1))) != NULL)
    {
        return fail("battlefield-stage-catalog-bounds");
    }
    if (!expect_status(
            pf_m4_reference_stage_geometry_line_count(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                &public_line_count),
            PF_STATUS_OK,
            "battlefield-stage-public-line-count") ||
        public_line_count != profile->line_count)
    {
        return fail("battlefield-stage-public-line-count-value");
    }
    for (line_index = UINT16_C(0);
         line_index < public_line_count;
         ++line_index)
    {
        if (!expect_status(
                pf_m4_reference_stage_geometry_line(
                    PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                    line_index,
                    &public_line),
                PF_STATUS_OK,
                "battlefield-stage-public-line") ||
            public_line.start_x_q16 != profile->lines[line_index].start_x_q16 ||
            public_line.start_y_q16 != profile->lines[line_index].start_y_q16 ||
            public_line.end_x_q16 != profile->lines[line_index].end_x_q16 ||
            public_line.end_y_q16 != profile->lines[line_index].end_y_q16 ||
            public_line.support != (uint16_t)(line_index + UINT16_C(1)) ||
            public_line.kind != profile->lines[line_index].kind ||
            public_line.reserved != UINT8_C(0))
        {
            return fail("battlefield-stage-public-line-value");
        }
    }
    if (!expect_status(
            pf_m4_reference_stage_geometry_line(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                public_line_count,
                &public_line),
            PF_STATUS_INVALID_CONFIG,
            "battlefield-stage-public-line-bounds") ||
        !expect_status(
            pf_m4_reference_stage_geometry_line_count(
                PF_M4_REFERENCE_STAGE_AUTHORED,
                &public_line_count),
            PF_STATUS_INVALID_CONFIG,
            "battlefield-stage-public-line-authored") ||
        !expect_status(
            pf_m4_reference_stage_geometry_line_count(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                NULL),
            PF_STATUS_INVALID_ARGUMENT,
            "battlefield-stage-public-line-null-count") ||
        !expect_status(
            pf_m4_reference_stage_geometry_line(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                UINT16_C(0),
                NULL),
            PF_STATUS_INVALID_ARGUMENT,
            "battlefield-stage-public-line-null"))
    {
        return fail("battlefield-stage-public-line-rejection");
    }
    for (spawn_index = UINT8_C(0);
         spawn_index < profile->spawn_point_count;
         ++spawn_index)
    {
        const pf_m4_ssbm_stage_spawn_point *spawn =
            pf_m4_ssbm_reference_stage_spawn_point(
                profile_id,
                spawn_index);

        if (spawn != &profile->spawn_points[spawn_index] ||
            spawn->source_index != spawn_index ||
            spawn->support != expected_spawn_supports[spawn_index] ||
            spawn->position_x_q16 != expected_spawn_x_q16[spawn_index] ||
            spawn->position_y_q16 != expected_spawn_y_q16[spawn_index])
        {
            return fail("battlefield-stage-spawn-catalog");
        }
    }
    if (pf_m4_ssbm_reference_stage_spawn_point(
            profile_id,
            profile->spawn_point_count) != NULL ||
        !expect_status(
            pf_m4_reference_stage_content(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                &content),
            PF_STATUS_OK,
            "battlefield-stage-content") ||
        content.stage.reference_collision_profile != profile_id ||
        content.stage.blast_left_q16 != profile->blast_left_q16 ||
        content.stage.blast_right_q16 != profile->blast_right_q16 ||
        content.stage.blast_top_q16 != profile->blast_top_q16 ||
        content.stage.blast_bottom_q16 != profile->blast_bottom_q16 ||
        !expect_status(
            pf_m4_validate_content(&content),
            PF_STATUS_OK,
            "battlefield-stage-content-validation") ||
        !expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "battlefield-stage-content-view") ||
        !initialize_sim(
            &storage,
            &view,
            UINT8_C(2),
            PF_SIM_MODE_DUEL,
            1,
            &sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "battlefield-stage-inspection"))
    {
        return fail("battlefield-stage-content-setup");
    }
    if (!pf_m4_ssbm_reference_stage_find_wall_contact(
            profile_id,
            INT32_C(0),
            INT32_C(90000),
            INT64_C(1675000),
            INT64_C(1770000),
            content.fighter.half_width_q16,
            &wall_x_q16,
            &wall_support,
            &wall_away_direction) ||
        wall_support != UINT8_C(23) ||
        wall_away_direction != INT8_C(-1) ||
        pf_m4_ssbm_reference_stage_find_wall_contact(
            profile_id,
            INT32_C(0),
            INT32_C(90000),
            INT64_C(1200000),
            INT64_C(1300000),
            content.fighter.half_width_q16,
            &wall_x_q16,
            &wall_support,
            &wall_away_direction))
    {
        return fail("battlefield-stage-wall-query");
    }
    for (spawn_index = UINT8_C(0); spawn_index < UINT8_C(2); ++spawn_index)
    {
        const pf_m4_ssbm_stage_spawn_point *spawn =
            &profile->spawn_points[spawn_index];
        const pf_m4_ssbm_stage_collision_line *line =
            pf_m4_ssbm_reference_stage_line(profile_id, spawn->support);
        const int32_t expected_fighter_y_q16 =
            pf_m4_ssbm_stage_line_y_q16(
                line,
                spawn->position_x_q16) -
            content.fighter.half_height_q16;
        uint8_t capsule_index;

        if (inspection.players[spawn_index].position_x_q16 !=
                spawn->position_x_q16 ||
            inspection.players[spawn_index].position_y_q16 !=
                expected_fighter_y_q16 ||
            inspection.players[spawn_index].support != spawn->support ||
            inspection.players[spawn_index].grounded != UINT8_C(1))
        {
            return fail("battlefield-stage-initial-player-position");
        }
        if (inspection.players[spawn_index].hurt_capsule_count !=
            UINT8_C(PF_M4_INSPECTION_HURT_CAPSULE_CAPACITY))
        {
            return fail("battlefield-stage-initial-hurt-capsule-count");
        }
        for (capsule_index = UINT8_C(0);
             capsule_index <
                 inspection.players[spawn_index].hurt_capsule_count;
             ++capsule_index)
        {
            const pf_m4_hurt_capsule_inspection *capsule =
                &inspection.players[spawn_index]
                     .hurt_capsules[capsule_index];

            if (capsule->hurtbox_id != capsule_index ||
                capsule->radius_q16 <= INT32_C(0) ||
                capsule->reserved != UINT8_C(0))
            {
                return fail("battlefield-stage-initial-hurt-capsule-value");
            }
        }
    }
    ceiling_y_q16 = profile->lines[8].start_y_q16;
    sim->world.position_x_q16[0] = INT32_C(0);
    sim->world.position_y_q16[0] =
        ceiling_y_q16 + content.fighter.half_height_q16 +
        PF_Q16_ONE / INT32_C(2);
    sim->world.velocity_x_q16[0] = INT32_C(0);
    sim->world.velocity_y_q16[0] = -PF_Q16_ONE;
    sim->world.grounded[0] = UINT8_C(0);
    sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_AIRBORNE;
    sim->world.action_ticks[0] = UINT16_C(0);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    if (!step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].position_y_q16 !=
            ceiling_y_q16 + content.fighter.half_height_q16 ||
        inspection.players[0].velocity_y_q16 != INT32_C(0) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE)
    {
        return fail("battlefield-stage-ceiling-production-route");
    }
    if (!pf_m4_ssbm_reference_stage_find_wall_contact(
            profile_id,
            INT32_C(0),
            PF_Q16_ONE,
            (int64_t)INT32_C(1722500) -
                (int64_t)content.fighter.half_height_q16,
            (int64_t)INT32_C(1722500) +
                (int64_t)content.fighter.half_height_q16 +
                (int64_t)PF_Q16_ONE,
            content.fighter.half_width_q16,
            &wall_x_q16,
            &wall_support,
            &wall_away_direction))
    {
        return fail("battlefield-stage-wall-production-setup");
    }
    sim->world.position_x_q16[0] = INT32_C(0);
    sim->world.position_y_q16[0] = INT32_C(1722500);
    sim->world.velocity_x_q16[0] = PF_Q16_ONE;
    sim->world.velocity_y_q16[0] = INT32_C(0);
    sim->world.grounded[0] = UINT8_C(0);
    sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_FALL_SPECIAL;
    sim->world.action_ticks[0] = UINT16_C(0);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL_SPECIAL;
    if (!step_duel(
            sim,
            INT16_C(0),
            UINT64_C(0),
            INT16_C(0),
            UINT64_C(0),
            &inspection) ||
        inspection.players[0].position_x_q16 != wall_x_q16 ||
        inspection.players[0].velocity_x_q16 != INT32_C(0) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_FALL_SPECIAL)
    {
        (void)fprintf(
            stderr,
            "battlefield-wall expected_x=%" PRId32
            " actual_x=%" PRId32 " vx=%" PRId32
            " y=%" PRId32 " action=%u support=%u away=%d\n",
            wall_x_q16,
            inspection.players[0].position_x_q16,
            inspection.players[0].velocity_x_q16,
            inspection.players[0].position_y_q16,
            (unsigned)inspection.players[0].action_state,
            (unsigned)wall_support,
            (int)wall_away_direction);
        return fail("battlefield-stage-wall-production-route");
    }
    {
        const pf_m4_ssbm_stage_collision_line *platform_line =
            &profile->lines[2];
        const int32_t platform_midpoint_x_q16 = (int32_t)(
            ((int64_t)platform_line->start_x_q16 +
             (int64_t)platform_line->end_x_q16) /
            INT64_C(2));
        const int32_t platform_y_q16 =
            pf_m4_ssbm_stage_line_y_q16(
                platform_line,
                platform_midpoint_x_q16);
        const int32_t starting_y_q16 =
            platform_y_q16 + INT32_C(2) * PF_Q16_ONE;

        sim->world.position_x_q16[0] = platform_midpoint_x_q16;
        sim->world.position_y_q16[0] = starting_y_q16;
        sim->world.velocity_x_q16[0] = INT32_C(0);
        sim->world.velocity_y_q16[0] = PF_Q16_ONE / INT32_C(8);
        sim->world.grounded[0] = UINT8_C(0);
        sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
        sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_AIRBORNE;
        sim->world.action_ticks[0] = UINT16_C(0);
        sim->world.fast_fall[0] = UINT8_C(0);
        sim->world.source_submotion[0] =
            (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
        if (!step_duel(
                sim,
                INT16_C(0),
                UINT64_C(0),
                INT16_C(0),
                UINT64_C(0),
                &inspection) ||
            inspection.players[0].grounded != UINT8_C(0) ||
            inspection.players[0].support !=
                (uint8_t)PF_M4_SURFACE_NONE ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_AIRBORNE ||
            inspection.players[0].position_y_q16 <= starting_y_q16)
        {
            return fail("battlefield-stage-pass-through-from-below");
        }
    }
    if (!expect_status(
            pf_m4_reference_stage_content(
                PF_M4_REFERENCE_STAGE_HYRULE_TEMPLE,
                &content),
            PF_STATUS_INVALID_CONFIG,
            "battlefield-stage-reject-incomplete-reference") ||
        !expect_status(
            pf_m4_reference_stage_content(
                (pf_m4_reference_stage)UINT16_MAX,
                &content),
            PF_STATUS_INVALID_CONFIG,
            "battlefield-stage-reject-invalid-reference") ||
        !expect_status(
            pf_m4_reference_stage_content(
                PF_M4_REFERENCE_STAGE_BATTLEFIELD,
                NULL),
            PF_STATUS_INVALID_ARGUMENT,
            "battlefield-stage-reject-null-output"))
    {
        return fail("battlefield-stage-content-rejection");
    }
    return 1;
}

int main(int argc, char **argv)
{
    pf_m4_content content;
    pf_m4_content shield_poke_content;
    pf_m4_content ledge_attack_content;
    pf_m4_content invalid_content;
    pf_m4_content invalid_strong_content;
    pf_m4_content invalid_tech_content;
    pf_m4_content invalid_getup_content;
    pf_m4_content invalid_getup_roll_content;
    pf_m4_content invalid_shield_content;
    pf_m4_content invalid_shield_geometry_content;
    pf_m4_content invalid_shield_tilt_content;
    pf_m4_content invalid_light_shield_depletion_content;
    pf_m4_content invalid_shield_pushback_content;
    pf_m4_content invalid_light_shield_threshold_content;
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
    pf_m4_content invalid_stale_zero_content;
    pf_m4_content invalid_stale_order_content;
    pf_m4_content invalid_stale_sum_content;
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
    pf_m4_content light_shield_hash_content;
    pf_m4_content shield_geometry_hash_content;
    pf_m4_content stale_hash_content;
    pf_m4_content getup_roll_hash_content;
    pf_content_view view;
    pf_content_view shield_poke_view;
    pf_content_view ledge_attack_view;
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
    pf_content_view light_shield_hash_view;
    pf_content_view shield_geometry_hash_view;
    pf_content_view stale_hash_view;
    pf_content_view getup_roll_hash_view;

    if (argc != 1)
    {
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-hurt") == 0)
        {
            return run_reference_common_hurt_stored_oracle(1) ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-turn-hurt") == 0)
        {
            return run_reference_falcon_turn_hurt_stored_oracle(1) ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-dive-grab-geometry") == 0)
        {
            return run_reference_falcon_dive_grab_stored_oracle(1) ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-neutral-special") == 0)
        {
            return run_ssbm_falcon_punch_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-damage-response") == 0)
        {
            return run_ssbm_damage_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-ground-knockback") == 0)
        {
            return run_ssbm_ground_knockback_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-surface-response") == 0)
        {
            return run_ssbm_surface_response_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(
                argv[2],
                "falcon-common-battlefield-surface-response") == 0)
        {
            return run_ssbm_battlefield_surface_response_observation_oracle()
                       ? 0
                       : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(
                argv[2],
                "falcon-common-battlefield-bounce-recontact") == 0)
        {
            return run_ssbm_battlefield_bounce_recontact_observation_oracle()
                       ? 0
                       : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-floor-response") == 0)
        {
            return run_ssbm_floor_response_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-prone-response") == 0)
        {
            return run_ssbm_prone_response_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-player-push") == 0)
        {
            return run_ssbm_player_push_observation_oracle() ? 0 : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(
                argv[2],
                "falcon-common-slope-ledge-response") == 0)
        {
            return run_ssbm_slope_ledge_response_observation_oracle()
                       ? 0
                       : 1;
        }
        if (argc == 3 && strcmp(argv[1], "--ssbm-oracle") == 0 &&
            strcmp(argv[2], "falcon-common-ledge-options") == 0)
        {
            return run_ssbm_ledge_options_observation_oracle() ? 0 : 1;
        }
        (void)fprintf(
            stderr,
            "usage: %s [--ssbm-oracle DOMAIN]\n",
            argv[0]);
        return 2;
    }

    if (!run_falcon_reference_table_test() ||
        !run_falcon_jab1_iasa_test() ||
        !run_falcon_ground_iasa_policy_test() ||
        !make_combat_content(&content, &view) ||
        !make_shield_poke_content(
            &shield_poke_content,
            &shield_poke_view) ||
        !make_ledge_attack_content(
            &ledge_attack_content,
            &ledge_attack_view) ||
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
            (INT32_C(3) * PF_Q16_ONE) / INT32_C(4),
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
    light_shield_hash_content = content;
    ++light_shield_hash_content.fighter
          .light_shield_hold_depletion_q16;
    if (!expect_status(
            pf_m4_make_content_view(
                &light_shield_hash_content,
                &light_shield_hash_view),
            PF_STATUS_OK,
            "light-shield-hash-content-view") ||
        memcmp(
            view.content_hash.bytes,
            light_shield_hash_view.content_hash.bytes,
            sizeof(view.content_hash.bytes)) == 0)
    {
        return fail("light-shield-content-hash");
    }
    shield_geometry_hash_content = content;
    ++shield_geometry_hash_content.fighter.shield_radius_x_q16;
    if (!expect_status(
            pf_m4_make_content_view(
                &shield_geometry_hash_content,
                &shield_geometry_hash_view),
            PF_STATUS_OK,
            "shield-geometry-hash-content-view") ||
        memcmp(
            view.content_hash.bytes,
            shield_geometry_hash_view.content_hash.bytes,
            sizeof(view.content_hash.bytes)) == 0)
    {
        return fail("shield-geometry-content-hash");
    }
    stale_hash_content = content;
    ++stale_hash_content.fighter
          .stale_move_slot_reduction_q16[8];
    if (!expect_status(
            pf_m4_make_content_view(
                &stale_hash_content,
                &stale_hash_view),
            PF_STATUS_OK,
            "stale-move-hash-content-view") ||
        memcmp(
            view.content_hash.bytes,
            stale_hash_view.content_hash.bytes,
            sizeof(view.content_hash.bytes)) == 0)
    {
        return fail("stale-move-content-hash");
    }
    getup_roll_hash_content = content;
    ++getup_roll_hash_content.fighter
          .getup_roll_stomach_backward.movement_begin_tick;
    if (!expect_status(
            pf_m4_make_content_view(
                &getup_roll_hash_content,
                &getup_roll_hash_view),
            PF_STATUS_OK,
            "getup-roll-hash-content-view") ||
        memcmp(
            view.content_hash.bytes,
            getup_roll_hash_view.content_hash.bytes,
            sizeof(view.content_hash.bytes)) == 0)
    {
        return fail("getup-roll-content-hash");
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
    invalid_getup_roll_content = content;
    invalid_getup_roll_content.fighter
        .getup_roll_back_backward.invulnerability_begin_tick =
        (uint8_t)(
            invalid_getup_roll_content.fighter
                .getup_roll_back_backward.invulnerability_end_tick +
            UINT8_C(1));
    invalid_shield_content = content;
    invalid_shield_content.fighter.shield_release_ticks =
        UINT16_C(0);
    invalid_shield_geometry_content = content;
    invalid_shield_geometry_content.fighter
        .shield_minimum_size_scale_q16 =
        invalid_shield_geometry_content.fighter
            .dense_shield_size_scale_q16;
    invalid_shield_tilt_content = content;
    invalid_shield_tilt_content.fighter.shield_animation_scale_y_q16 =
        INT32_C(65) * PF_Q16_ONE;
    invalid_light_shield_depletion_content = content;
    invalid_light_shield_depletion_content.fighter
        .light_shield_hold_depletion_q16 =
        invalid_light_shield_depletion_content.fighter
            .shield_hold_depletion_q16 +
        UINT32_C(1);
    invalid_shield_pushback_content = content;
    invalid_shield_pushback_content.fighter
        .shield_defender_pushback_normal_scale_q16 =
        PF_Q16_ONE + INT32_C(1);
    invalid_light_shield_threshold_content = content;
    invalid_light_shield_threshold_content.fighter
        .light_shield_trigger_threshold =
        invalid_light_shield_threshold_content.fighter
            .digital_trigger_threshold;
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
    invalid_stale_zero_content = content;
    invalid_stale_zero_content.fighter
        .stale_move_slot_reduction_q16[8] = UINT16_C(0);
    invalid_stale_order_content = content;
    invalid_stale_order_content.fighter
        .stale_move_slot_reduction_q16[1] =
        invalid_stale_order_content.fighter
            .stale_move_slot_reduction_q16[0];
    invalid_stale_sum_content = content;
    invalid_stale_sum_content.fighter
        .stale_move_slot_reduction_q16[0] = UINT16_C(20000);
    if (!run_battlefield_stage_catalog_test() ||
        !expect_status(
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
            pf_m4_validate_content(&invalid_getup_roll_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-getup-roll-timing") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-data") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_geometry_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-geometry") ||
        !expect_status(
            pf_m4_validate_content(&invalid_shield_tilt_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-tilt") ||
        !expect_status(
            pf_m4_validate_content(
                &invalid_light_shield_depletion_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-light-shield-depletion") ||
        !expect_status(
            pf_m4_validate_content(
                &invalid_shield_pushback_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-shield-pushback") ||
        !expect_status(
            pf_m4_validate_content(
                &invalid_light_shield_threshold_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-invalid-light-shield-threshold") ||
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
        !expect_status(
            pf_m4_validate_content(&invalid_stale_zero_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-zero-stale-move-slot") ||
        !expect_status(
            pf_m4_validate_content(&invalid_stale_order_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-unordered-stale-move-slots") ||
        !expect_status(
            pf_m4_validate_content(&invalid_stale_sum_content),
            PF_STATUS_INVALID_CONFIG,
            "reject-overflowing-stale-move-reduction") ||
        !run_one_way_hit_test(&content, &view) ||
        !run_weight_test(&content, &view) ||
        !run_directional_ground_attack_test(&content, &view) ||
        !run_directional_aerial_test(&content, &view) ||
        !run_ledge_attack_test(
            &ledge_attack_content,
            &ledge_attack_view) ||
        !run_v_cancel_test(&v_cancel_content, &v_cancel_view) ||
        !run_v_cancel_snapshot_test(&v_cancel_content) ||
        !run_crouch_cancel_test(&content, &view) ||
        (0 && !run_double_jump_cancel_counter_test(
            &double_jump_cancel_counter_content,
            &double_jump_cancel_counter_view)) ||
        !run_aerial_hit_test(&content, &view) ||
        !run_strong_aerial_hit_test(&content, &view) ||
        !run_default_strong_tumble_test(&content, &view) ||
        !run_small_step_forward_smash_test(
            &small_step_forward_smash_content,
            &small_step_forward_smash_view) ||
        (0 && !run_drop_cancel_test(
            &drop_cancel_content,
            &drop_cancel_view,
            &drop_cancel_whiff_view)) ||
        (0 && !run_sharking_test(
            &drop_cancel_content,
            &drop_cancel_view)) ||
        (0 && !run_cross_up_test(
            &cross_up_content,
            &cross_up_view)) ||
        (0 && !run_juggling_test(
            &juggling_content,
            &juggling_view)) ||
        (0 && !run_ladder_test(
            &ladder_content,
            &ladder_view)) ||
        !run_stale_move_test(
            &kill_confirm_content,
            &kill_confirm_view) ||
        (0 && !run_kill_confirm_test(
            &kill_confirm_content,
            &kill_confirm_view)) ||
        (0 && !run_zero_to_death_test(
            &kill_confirm_content,
            &kill_confirm_view)) ||
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
        !run_light_shield_state_test(&content, &view) ||
        !run_dashing_shield_test(&content, &view) ||
        !run_shield_block_test(&content, &view) ||
        !run_shield_sdi_test(&content, &view) ||
        !run_light_shield_block_test(&content, &view) ||
        !run_shield_geometry_and_poke_test(
            &shield_poke_content,
            &shield_poke_view) ||
        !run_reference_shield_boundary_test() ||
        !run_reference_moving_hit_sweep_test() ||
        !run_reference_common_hurt_stored_oracle(0) ||
        !run_reference_falcon_dive_grab_stored_oracle(0) ||
        !run_ssbm_falcon_punch_observation_oracle() ||
        !run_powershield_cancel_test(&content, &view) ||
        !run_powershield_cancel_replay_test(&view) ||
        !run_aerial_l_cancel_replay_test() ||
        !run_shield_break_test(
            &shield_break_content,
            &shield_break_view) ||
        !run_ssbm_damage_source_test(&content) ||
        !run_di_and_sdi_test(
            &reaction_content,
            &reaction_view) ||
        !run_knockdown_and_tech_test(
            &reaction_content,
            &reaction_view) ||
        (0 && !run_tech_chase_test(
            &tech_invulnerability_content,
            &tech_invulnerability_view)) ||
        !run_floor_getup_option_test(
            &reaction_content,
            &reaction_view) ||
        !run_prone_getup_roll_timing_test(
            &reaction_content,
            &reaction_view) ||
        !run_prone_getup_roll_observation_test(
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
            &content) ||
        (0 && !run_boost_grab_test(
            &boost_grab_content,
            &boost_grab_view)) ||
        !run_jab_cancel_test(
            &jab_cancel_close_content,
            &jab_cancel_close_view,
            &jab_cancel_far_view) ||
        (0 && !run_jab_reset_test(
            &jab_reset_content,
            &jab_reset_view,
            &jab_reset_exact_content,
            &jab_reset_exact_view,
            &jab_reset_over_damage_view,
            &jab_reset_over_hitstun_view)) ||
        (0 && !run_jump_cancelled_grab_test(
            &grab_close_content,
            &grab_close_view,
            &grab_far_view)) ||
        (0 && !run_jump_cancelling_test(
            &grab_close_content,
            &grab_close_view,
            &grab_far_view)) ||
        !run_pummel_test(
            &grab_close_content,
            &grab_close_view) ||
        !run_throw_collateral_test() ||
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
        !run_grab_damage_escape_test(
            &grab_damage_content,
            &grab_damage_view) ||
        !run_grab_team_resolution_test(&team_wobble_view) ||
        (0 && !run_team_handoff_route(
            &team_wobble_content,
            &team_wobble_view)) ||
        !run_hitlag_snapshot_test(&view) ||
        !run_shield_hitlag_snapshot_test(&view) ||
        !run_deterministic_trace(&view))
    {
        return 1;
    }

    (void)printf(
        "m4-combat=pass content_schema=%u deterministic_ticks=%" PRIu64
        " combat_core=pass journal_invariants=74 weight=1 stale_move=1 prone_getup_roll=2 directional_ground_attacks=1 smash_charge=1 light_shield=1 shield_geometry=1 shield_sdi=1 ssbm_damage=1 directional_aerials=1 ledge_attack=1 crouch_cancel=1 double_jump_cancel_counter=skipped approach=1 spacing=1 mindgame=1 jab_cancel=1 pummel=1 directional_throws=1 chain_grab=1 team_resolution=1 team_wobble=skipped emergent_technique_tests=skipped\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        TEST_DETERMINISTIC_TICKS);
    return 0;
}
