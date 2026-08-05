#include "pf/m4.h"
#include "pf/sim.h"
#include "sim_falcon_frame_data.h"

#include <inttypes.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PF_TRACE_MEMORY_BYTES 4096U
#define PF_TRACE_MEMORY_ALIGNMENT 64U

typedef struct pf_trace_storage
{
    alignas(PF_TRACE_MEMORY_ALIGNMENT) uint8_t state[PF_TRACE_MEMORY_BYTES];
    alignas(PF_TRACE_MEMORY_ALIGNMENT) uint8_t scratch[PF_TRACE_MEMORY_BYTES];
} pf_trace_storage;

static int fail_status(const char *operation, pf_status status)
{
    (void)fprintf(
        stderr,
        "m4-movement-trace=fail operation=%s status=%s\n",
        operation,
        pf_status_name(status));
    return 1;
}

static int run_elevated_special_pre_roll(
    pf_sim *sim,
    pf_m4_inspection *inspection,
    int opponent_flees,
    int opponent_jumps)
{
    const uint32_t pre_roll_ticks =
        opponent_jumps != 0 ? UINT32_C(17) : UINT32_C(11);
    uint32_t pre_roll_tick;

    /* The aerial-hit fixture launches the victim through ordinary input and
     * waits until its descent intersects the imported up-hit geometry. The
     * ordinary miss fixtures need only Falcon's 11-tick ascent sequence. */
    for (pre_roll_tick = UINT32_C(0);
         pre_roll_tick < pre_roll_ticks;
         ++pre_roll_tick)
    {
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;
        pf_status status;

        (void)memset(inputs, 0, sizeof(inputs));
        inputs[0].tick = inspection->tick;
        inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[0].player_slot = UINT8_C(0);
        inputs[1].tick = inspection->tick;
        inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[1].player_slot = UINT8_C(1);
        if (pre_roll_tick < UINT32_C(5) ||
            pre_roll_tick == UINT32_C(7))
        {
            inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
        }
        if (opponent_flees != 0)
        {
            inputs[1].main_stick_x = INT16_MIN;
        }
        if (opponent_jumps != 0 && pre_roll_tick < UINT32_C(5))
        {
            inputs[1].buttons = PF_INPUT_BUTTON_JUMP;
        }
        status = pf_sim_tick(sim, inputs, (size_t)2, &result);
        if (status != PF_STATUS_OK)
        {
            return fail_status("elevated-special-pre-roll-tick", status);
        }
        status = pf_m4_inspect(sim, inspection);
        if (status != PF_STATUS_OK)
        {
            return fail_status("elevated-special-pre-roll-inspect", status);
        }
    }
    if (inspection->players[0].grounded != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-movement-trace=fail operation=elevated-special-pre-roll\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    pf_trace_storage storage;
    pf_m4_content content;
    pf_content_view view;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;
    int32_t origin_x_q16;
    int32_t origin_y_q16;
    int32_t opponent_origin_x_q16;
    int32_t opponent_origin_y_q16;
    uint32_t trace_frame = UINT32_C(0);
    int input_x;
    int input_y;
    int input_c_x;
    int input_c_y;
    int opponent_input_x;
    uint64_t opponent_buttons;
    char input_line[256];
    unsigned int left_trigger;
    unsigned int right_trigger;
    uint64_t buttons;
    pf_status status;
    int platform_mode = 0;
    int push_mode = 0;
    int shield_hit_mode = 0;
    int falcon_punch_air_mode = 0;
    int raptor_boost_ground_hit_mode = 0;
    int raptor_boost_ground_edge_mode = 0;
    int raptor_boost_air_miss_mode = 0;
    int raptor_boost_air_hit_mode = 0;
    int falcon_dive_ground_catch_mode = 0;
    int falcon_dive_air_catch_mode = 0;
    int falcon_dive_air_miss_mode = 0;
    int falcon_dive_air_ledge_mode = 0;
    int falcon_kick_ground_hit_mode = 0;
    int falcon_kick_ground_wall_mode = 0;
    int falcon_kick_ground_edge_mode = 0;
    int falcon_kick_air_mode = 0;
    int falcon_kick_air_land_mode = 0;

    if (argc == 2 && strcmp(argv[1], "--platform") == 0)
    {
        platform_mode = 1;
    }
    else if (argc == 2 && strcmp(argv[1], "--push") == 0)
    {
        push_mode = 1;
    }
    else if (argc == 2 && strcmp(argv[1], "--shield-hit") == 0)
    {
        shield_hit_mode = 1;
    }
    else if (argc == 2 && strcmp(argv[1], "--falcon-punch-air") == 0)
    {
        falcon_punch_air_mode = 1;
    }
    else if (
        argc == 2 &&
        strcmp(argv[1], "--raptor-boost-ground-hit") == 0)
    {
        raptor_boost_ground_hit_mode = 1;
    }
    else if (
        argc == 2 &&
        strcmp(argv[1], "--raptor-boost-ground-miss") == 0)
    {
        /* The ground miss oracle uses the ordinary wide-floor setup. */
    }
    else if (
        argc == 2 &&
        strcmp(argv[1], "--raptor-boost-ground-edge") == 0)
    {
        raptor_boost_ground_edge_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--raptor-boost-air-miss") == 0)
    {
        raptor_boost_air_miss_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--raptor-boost-air-hit") == 0)
    {
        raptor_boost_air_hit_mode = 1;
    }
    else if (
        argc == 2 &&
        strcmp(argv[1], "--falcon-dive-ground-catch") == 0)
    {
        falcon_dive_ground_catch_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-dive-air-catch") == 0)
    {
        falcon_dive_air_catch_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-dive-air-miss") == 0)
    {
        falcon_dive_air_miss_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-dive-air-ledge") == 0)
    {
        falcon_dive_air_ledge_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-kick-ground") == 0)
    {
        /* The ground oracle uses the runner's ordinary wide-floor setup. */
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-kick-ground-hit") == 0)
    {
        falcon_kick_ground_hit_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-kick-ground-wall") == 0)
    {
        falcon_kick_ground_wall_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-kick-ground-edge") == 0)
    {
        falcon_kick_ground_edge_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-kick-air") == 0)
    {
        falcon_kick_air_mode = 1;
    }
    else if (
        argc == 2 && strcmp(argv[1], "--falcon-kick-air-land") == 0)
    {
        falcon_kick_air_land_mode = 1;
    }
    else if (argc != 1)
    {
        (void)fprintf(
            stderr,
            "usage: pf_m4_movement_trace "
            "[--platform|--push|--shield-hit|--falcon-punch-air|"
            "--raptor-boost-ground-miss|--raptor-boost-ground-hit|"
            "--raptor-boost-ground-edge|"
            "--raptor-boost-air-miss|--raptor-boost-air-hit|"
            "--falcon-dive-ground-catch|"
            "--falcon-dive-air-catch|"
            "--falcon-dive-air-miss|"
            "--falcon-dive-air-ledge|"
            "--falcon-kick-ground|--falcon-kick-ground-edge|"
            "--falcon-kick-ground-hit|--falcon-kick-ground-wall|"
            "--falcon-kick-air|"
            "--falcon-kick-air-land]\n");
        return 1;
    }

    (void)memset(&storage, 0, sizeof(storage));
    status = pf_m4_default_content(&content);
    if (status != PF_STATUS_OK)
    {
        return fail_status("default-content", status);
    }
    /*
     * Keep the executable-oracle runner on an intentionally plain, wide floor.
     * The production laboratory stage retains its original moving platforms
     * and solid block, but those unrelated fixtures must not intercept a
     * Final-Destination locomotion trace after hundreds of accumulated frames.
     * Disable the original Relay Rod as well: Dolphin's oracle match has items
     * off, and this tool compares common movement rather than original content.
     */
    content.item.enabled = UINT8_C(0);
    /*
     * Keep one inert, one-tick projectile definition available so neutral-B
     * samples can qualify common-state special IASA without introducing the
     * original live projectile into later movement/collision frames.
     */
    content.projectile.enabled = UINT8_C(1);
    content.projectile.speed_q16 = INT32_C(1);
    content.projectile.lifetime_ticks = UINT16_C(1);
    content.reflector.enabled = UINT8_C(1);
    if (falcon_punch_air_mode == 0 && falcon_kick_air_mode == 0 &&
        falcon_dive_air_ledge_mode == 0)
    {
        content.stage.floor_left_q16 = -INT32_C(128) * PF_Q16_ONE;
        content.stage.floor_right_q16 = INT32_C(128) * PF_Q16_ONE;
    }
    content.stage.blast_left_q16 = -INT32_C(160) * PF_Q16_ONE;
    content.stage.blast_right_q16 = INT32_C(160) * PF_Q16_ONE;
    content.stage.platform_center_x_q16 =
        (platform_mode != 0 ? -INT32_C(8) : -INT32_C(28)) *
        PF_Q16_ONE;
    content.stage.platform_half_width_q16 =
        (platform_mode != 0 ? INT32_C(6) : INT32_C(1)) *
        PF_Q16_ONE;
    if (platform_mode != 0)
    {
        content.stage.platform_y_q16 =
            content.stage.floor_y_q16 - INT32_C(316264);
    }
    content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    content.stage.solid_left_q16 = -INT32_C(22) * PF_Q16_ONE;
    content.stage.solid_right_q16 = -INT32_C(21) * PF_Q16_ONE;
    content.stage.solid_top_q16 = INT32_C(28) * PF_Q16_ONE;
    content.stage.solid_bottom_q16 = INT32_C(29) * PF_Q16_ONE;
    content.stage.upper_platform_center_x_q16 =
        -INT32_C(25) * PF_Q16_ONE;
    content.stage.upper_platform_half_width_q16 = PF_Q16_ONE;
    if (push_mode != 0 || shield_hit_mode != 0)
    {
        /* Final Destination starts ports one and two at -60/+60. */
        content.stage.spawn_spacing_q16 =
            (int32_t)((INT64_C(144) * PF_Q16_ONE) / INT64_C(23));
    }
    else if (falcon_punch_air_mode != 0 || falcon_kick_air_mode != 0 ||
             falcon_dive_air_miss_mode != 0)
    {
        content.stage.spawn_spacing_q16 = INT32_C(10) * PF_Q16_ONE;
        content.stage.blast_bottom_q16 =
            INT32_C(2048) * PF_Q16_ONE;
    }
    else if (raptor_boost_air_miss_mode != 0)
    {
        content.stage.spawn_spacing_q16 = PF_Q16_ONE / INT32_C(32);
        content.fighter.player_push_half_width_q16 = INT32_C(1);
    }
    else if (raptor_boost_ground_hit_mode != 0 ||
             raptor_boost_air_hit_mode != 0)
    {
        /* The pinned Dolphin capture starts Falcon 10 Melee units from the
         * stationary target. Translate that symmetric half-spacing through
         * the repository's exact horizontal world scale. */
        content.stage.spawn_spacing_q16 =
            (int32_t)(
                (INT64_C(5) * INT64_C(12) * PF_Q16_ONE) /
                INT64_C(115));
    }
    else if (raptor_boost_ground_edge_mode != 0)
    {
        int32_t before_crossing_q16 = INT32_C(0);
        int32_t after_crossing_q16 = INT32_C(0);
        uint16_t displayed_frame;

        content.stage.spawn_spacing_q16 = PF_Q16_ONE / INT32_C(32);
        content.fighter.player_push_half_width_q16 = INT32_C(1);
        content.stage.revival_platform_half_width_q16 =
            content.fighter.half_width_q16;
        for (displayed_frame = UINT16_C(1);
             displayed_frame <= UINT16_C(20);
             ++displayed_frame)
        {
            int32_t motion_q16 = INT32_C(0);

            if (!pf_m4_falcon_reference_motion_x_q16(
                    (uint8_t)PF_M4_ACTION_RAPTOR_BOOST_START_GROUND,
                    displayed_frame,
                    &motion_q16))
            {
                (void)fprintf(
                    stderr,
                    "m4-movement-trace=fail operation="
                    "raptor-boost-ground-edge-motion\n");
                return 1;
            }
            if (displayed_frame <= UINT16_C(19))
            {
                before_crossing_q16 += motion_q16;
            }
            after_crossing_q16 += motion_q16;
        }
        content.stage.floor_right_q16 =
            -content.stage.spawn_spacing_q16 +
            before_crossing_q16 +
            (after_crossing_q16 - before_crossing_q16) / INT32_C(2);
    }
    else if (falcon_kick_ground_hit_mode != 0)
    {
        /* Align source sphere contact after five root-motion steps. The
         * symmetric half-separation is 14 + 4/256 Melee units, translated
         * through the repository's exact horizontal world scale. */
        content.stage.spawn_spacing_q16 =
            (int32_t)(
                (INT64_C(3588) * INT64_C(12) * PF_Q16_ONE) /
                (INT64_C(256) * INT64_C(115)));
        content.fighter.player_push_half_width_q16 = INT32_C(1);
    }
    else if (falcon_kick_ground_wall_mode != 0)
    {
        /* Hyrule exposes action 363 one post-frame after its displayed-frame
         * 22 wall-hug sample. Place the fixture one Q16 unit beyond the exact
         * imported frame-22 endpoint (201289) so its crossing is exposed on
         * that same next row. The wall is deliberately tall so only the
         * Falcon Kick callback, rather than unrelated stage topology, is
         * under comparison. */
        content.stage.solid_left_q16 =
            -content.stage.spawn_spacing_q16 +
            content.fighter.half_width_q16 + INT32_C(201290);
        content.stage.solid_right_q16 =
            content.stage.solid_left_q16 + PF_Q16_ONE;
        content.stage.solid_bottom_q16 =
            content.stage.floor_y_q16 - PF_Q16_ONE / INT32_C(4);
    }
    else if (falcon_dive_ground_catch_mode != 0)
    {
        /* The pinned catch trace starts Falcon and the victim 6.2 Melee
         * units apart. Spawn spacing is the symmetric half-separation. */
        content.stage.spawn_spacing_q16 =
            (int32_t)(
                (INT64_C(31) * INT64_C(12) * PF_Q16_ONE) /
                (INT64_C(10) * INT64_C(115)));
    }
    else if (falcon_dive_air_catch_mode != 0)
    {
        /* The pinned aerial capture starts Falcon five Melee units from the
         * airborne victim. Spawn spacing is the symmetric half-separation. */
        content.stage.spawn_spacing_q16 =
            (int32_t)(
                (INT64_C(5) * INT64_C(12) * PF_Q16_ONE) /
                (INT64_C(2) * INT64_C(115)));
    }
    else if (falcon_dive_air_ledge_mode != 0)
    {
        /* Final Destination's left ledge and the controller route's safe
         * on-stage start, transformed through the comparison scale. */
        content.stage.floor_left_q16 = -INT32_C(585144);
        content.stage.floor_right_q16 = INT32_C(585144);
        content.stage.spawn_spacing_q16 = INT32_C(2) * PF_Q16_ONE;
        content.stage.platform_center_x_q16 = INT32_C(0);
        content.stage.platform_half_width_q16 = PF_Q16_ONE;
        content.stage.upper_platform_center_x_q16 = INT32_C(2) * PF_Q16_ONE;
        content.stage.upper_platform_half_width_q16 = PF_Q16_ONE;
        content.stage.solid_left_q16 = INT32_C(2) * PF_Q16_ONE;
        content.stage.solid_right_q16 = INT32_C(3) * PF_Q16_ONE;
        content.stage.blast_bottom_q16 = INT32_C(2048) * PF_Q16_ONE;
    }
    else if (falcon_kick_ground_edge_mode != 0)
    {
        /*
         * Isolate the source edge-fall boundary without changing production
         * physics.  This symmetric two-player fixture uses the smallest
         * practical spawn separation and disables irrelevant player push.
         * At the pinned root track's displayed frame 13 Falcon remains over
         * this floor; displayed frame 14 crosses it, exactly as in the owner
         * Final Destination capture.
         */
        content.stage.spawn_spacing_q16 = PF_Q16_ONE / INT32_C(32);
        content.fighter.player_push_half_width_q16 = INT32_C(1);
        content.stage.revival_platform_half_width_q16 =
            content.fighter.half_width_q16;
        content.stage.floor_right_q16 =
            content.fighter.half_width_q16 +
            INT32_C(3) * content.stage.spawn_spacing_q16;
    }
    if (raptor_boost_air_miss_mode != 0)
    {
        /* Start both fighters on a legitimate floor, then let the imported
         * rightward root track carry Falcon beyond its endpoint. This
         * recreates the high offstage capture without mutating fighter state. */
        content.stage.floor_right_q16 =
            content.fighter.half_width_q16 +
            INT32_C(3) * content.stage.spawn_spacing_q16;
        content.stage.revival_platform_half_width_q16 =
            content.fighter.half_width_q16;
        content.stage.blast_bottom_q16 =
            INT32_C(2048) * PF_Q16_ONE;
    }
    if (shield_hit_mode != 0)
    {
        /* Falcon's Jab 1 hits on displayed frames 3-4 for 2 damage. */
        content.fighter.jab_damage_q16 =
            UINT32_C(2) * UINT32_C(65536);
        content.fighter.jab_startup_ticks = UINT16_C(2);
        content.fighter.jab_active_ticks = UINT16_C(2);
        content.fighter.jab_recovery_ticks = UINT16_C(18);
        content.fighter.jab_hitlag_ticks = UINT16_C(3);
    }
    status = pf_m4_make_content_view(&content, &view);
    if (status != PF_STATUS_OK)
    {
        return fail_status("content-view", status);
    }
    status = pf_sim_default_config(
        &config,
        UINT8_C(2),
        PF_SIM_MODE_DUEL);
    if (status != PF_STATUS_OK)
    {
        return fail_status("default-config", status);
    }
    config.max_ticks = UINT64_C(100000);
    config.arena_half_width_q16 = INT32_C(256) * PF_Q16_ONE;
    config.arena_ceiling_q16 =
        (falcon_punch_air_mode != 0 || falcon_kick_air_mode != 0 ||
         falcon_dive_air_catch_mode != 0 ||
         falcon_dive_air_miss_mode != 0 ||
         falcon_dive_air_ledge_mode != 0 ||
         raptor_boost_air_miss_mode != 0 ||
         raptor_boost_air_hit_mode != 0
             ? INT32_C(4096)
             : INT32_C(256)) *
        PF_Q16_ONE;
    config.stock_count = UINT8_C(0);
    status = pf_sim_init(
        storage.state,
        sizeof(storage.state),
        storage.scratch,
        sizeof(storage.scratch),
        &view,
        &config,
        &sim);
    if (status != PF_STATUS_OK)
    {
        return fail_status("init", status);
    }
    status = pf_sim_reset(sim, UINT64_C(2));
    if (status != PF_STATUS_OK)
    {
        return fail_status("reset", status);
    }
    status = pf_m4_inspect(sim, &inspection);
    if (status != PF_STATUS_OK)
    {
        return fail_status("inspect-origin", status);
    }
    if (platform_mode != 0)
    {
        uint32_t pre_roll_tick;
        int platform_ready = 0;

        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(205);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            if (pre_roll_tick < UINT32_C(5))
            {
                inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
            }
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("platform-pre-roll-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("platform-pre-roll-inspect", status);
            }
            if (pre_roll_tick >= UINT32_C(5) &&
                inspection.players[0].grounded != UINT8_C(0) &&
                inspection.players[0].support ==
                    (uint8_t)PF_M4_SURFACE_PLATFORM &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE)
            {
                platform_ready = 1;
                break;
            }
        }
        if (platform_ready == 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=platform-pre-roll\n");
            return 1;
        }
    }
    else if (falcon_kick_ground_edge_mode != 0)
    {
        uint32_t pre_roll_tick;

        /* Move the non-oracle fighter off the tiny fixture before Falcon
         * Kick becomes active, so this route measures edge conversion only. */
        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(60);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            inputs[1].main_stick_x = INT16_MAX;
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("ground-edge-pre-roll-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("ground-edge-pre-roll-inspect", status);
            }
        }
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=ground-edge-pre-roll\n");
            return 1;
        }
    }
    else if (falcon_kick_air_land_mode != 0)
    {
        uint32_t pre_roll_tick;

        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(7);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("air-land-pre-roll-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("air-land-pre-roll-inspect", status);
            }
        }
        if (inspection.players[0].grounded != UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=air-land-pre-roll\n");
            return 1;
        }
    }
    else if (falcon_dive_air_miss_mode != 0 ||
             raptor_boost_air_miss_mode != 0 ||
             raptor_boost_air_hit_mode != 0)
    {
        if (run_elevated_special_pre_roll(
                sim,
                &inspection,
                raptor_boost_air_miss_mode,
                raptor_boost_air_hit_mode) != 0)
        {
            return 1;
        }
    }
    else if (falcon_dive_air_catch_mode != 0)
    {
        uint32_t pre_roll_tick;

        /* Match the oracle's high airborne entry without relocating state:
         * both fighters perform a normal jump and an ordinary double jump.
         * Falcon then starts the move; capture itself clears victim velocity. */
        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(20);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            if (pre_roll_tick < UINT32_C(5) ||
                (pre_roll_tick >= UINT32_C(10) &&
                 pre_roll_tick < UINT32_C(15)))
            {
                inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
            }
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            if (pre_roll_tick < UINT32_C(5) ||
                (pre_roll_tick >= UINT32_C(10) &&
                 pre_roll_tick < UINT32_C(15)))
            {
                inputs[1].buttons = PF_INPUT_BUTTON_JUMP;
            }
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-air-pre-roll-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-air-pre-roll-inspect", status);
            }
        }
        if (inspection.players[0].grounded != UINT8_C(0) ||
            inspection.players[1].grounded != UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=falcon-dive-air-pre-roll\n");
            return 1;
        }
    }
    else if (falcon_dive_air_ledge_mode != 0)
    {
        uint32_t pre_roll_tick;
        int setup_ready = 0;

        /* Walk to the capture's safe on-stage start without mutating fighter
         * state, settle, and face inward. The content validator deliberately
         * keeps every possible spawn inside the narrow oracle floor. */
        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(300);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            if (inspection.players[0].position_x_q16 > -INT32_C(302203))
            {
                inputs[0].main_stick_x = -INT16_C(12000);
            }
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-ledge-walk-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-ledge-walk-inspect", status);
            }
            if (inspection.players[0].position_x_q16 <= -INT32_C(302203) &&
                inspection.players[0].velocity_x_q16 == INT32_C(0) &&
                inspection.players[0].grounded != UINT8_C(0) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE)
            {
                setup_ready = 1;
                break;
            }
        }
        if (setup_ready == 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation="
                "falcon-dive-ledge-walk\n");
            return 1;
        }
        setup_ready = 0;
        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(60);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            if (inspection.players[0].facing != INT8_C(1))
            {
                inputs[0].main_stick_x = INT16_C(12000);
            }
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-ledge-turn-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-ledge-turn-inspect", status);
            }
            if (inspection.players[0].facing == INT8_C(1) &&
                inspection.players[0].velocity_x_q16 == INT32_C(0) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_GROUND_IDLE)
            {
                setup_ready = 1;
                break;
            }
        }
        if (setup_ready == 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation="
                "falcon-dive-ledge-turn\n");
            return 1;
        }

        /* Reproduce the controller-only jump: five held jump-left frames,
         * twenty-five continued left-drift frames, then thirty-two neutral
         * descent frames. The next input row begins aerial Falcon Dive. */
        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(62);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            if (pre_roll_tick < UINT32_C(30))
            {
                inputs[0].main_stick_x = INT16_MIN;
            }
            if (pre_roll_tick < UINT32_C(5))
            {
                inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
            }
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("falcon-dive-ledge-pre-roll-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status(
                    "falcon-dive-ledge-pre-roll-inspect", status);
            }
        }
        if (inspection.players[0].grounded != UINT8_C(0))
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation="
                "falcon-dive-ledge-pre-roll\n");
            return 1;
        }
    }
    else if (falcon_punch_air_mode != 0 || falcon_kick_air_mode != 0)
    {
        uint32_t pre_roll_tick;
        int airborne_ready = 0;

        for (pre_roll_tick = UINT32_C(0);
             pre_roll_tick < UINT32_C(240);
             ++pre_roll_tick)
        {
            pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
            pf_tick_result result;

            (void)memset(inputs, 0, sizeof(inputs));
            inputs[0].tick = inspection.tick;
            inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[0].player_slot = UINT8_C(0);
            inputs[0].main_stick_x = INT16_MIN;
            inputs[1].tick = inspection.tick;
            inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            inputs[1].player_slot = UINT8_C(1);
            status = pf_sim_tick(sim, inputs, (size_t)2, &result);
            if (status != PF_STATUS_OK)
            {
                return fail_status("air-special-pre-roll-tick", status);
            }
            status = pf_m4_inspect(sim, &inspection);
            if (status != PF_STATUS_OK)
            {
                return fail_status("air-special-pre-roll-inspect", status);
            }
            if (inspection.players[0].grounded == UINT8_C(0) &&
                inspection.players[0].action_state ==
                    (uint8_t)PF_M4_ACTION_AIRBORNE)
            {
                airborne_ready = 1;
                break;
            }
        }
        if (airborne_ready == 0)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail "
                "operation=air-special-pre-roll\n");
            return 1;
        }
    }
    origin_x_q16 = inspection.players[0].position_x_q16;
    origin_y_q16 = inspection.players[0].position_y_q16;
    opponent_origin_x_q16 = inspection.players[1].position_x_q16;
    opponent_origin_y_q16 = inspection.players[1].position_y_q16;
    (void)puts(
        "trace_frame,input_x,input_y,input_c_x,input_c_y,left_trigger,"
        "right_trigger,buttons,tick,"
        "action_state,action_ticks,facing,grounded,"
        "dash_direction,previous_strong_direction,position_x_q16_from_origin,"
        "position_y_q16_from_origin,"
        "velocity_x_q16,velocity_y_q16,shield_recoil_x_q16,"
        "shield_health_q16,shield_strength,shield_angle_turn,"
        "shield_magnitude,shield_center_offset_x_q16,"
        "shield_center_offset_y_q16,shield_radius_x_q16,shield_radius_y_q16,"
        "powershield,hitlag_ticks,shield_stun_ticks,"
        "opponent_action_state,opponent_action_ticks,opponent_hitlag_ticks,"
        "opponent_hitstun_ticks,"
        "opponent_facing,opponent_grounded,"
        "opponent_position_x_q16_from_origin,"
        "opponent_position_y_q16_from_origin,"
        "opponent_velocity_x_q16,opponent_velocity_y_q16,"
        "opponent_shield_recoil_x_q16,opponent_damage_q16");
    while (fgets(input_line, sizeof(input_line), stdin) != NULL)
    {
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;
        int parsed_input_count;

        opponent_buttons = UINT64_C(0);
        parsed_input_count = sscanf(
            input_line,
            "%d,%d,%d,%d,%u,%u,%" SCNu64 ",%d,%" SCNu64,
            &input_x,
            &input_y,
            &input_c_x,
            &input_c_y,
            &left_trigger,
            &right_trigger,
            &buttons,
            &opponent_input_x,
            &opponent_buttons);
        if (parsed_input_count != 8 && parsed_input_count != 9)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=input-format frame=%" PRIu32
                " fields=%d\n",
                trace_frame,
                parsed_input_count);
            return 1;
        }

        if (input_x < (int)INT16_MIN || input_x > (int)INT16_MAX ||
            input_y < (int)INT16_MIN || input_y > (int)INT16_MAX ||
            input_c_x < (int)INT16_MIN || input_c_x > (int)INT16_MAX ||
            input_c_y < (int)INT16_MIN || input_c_y > (int)INT16_MAX ||
            opponent_input_x < (int)INT16_MIN ||
            opponent_input_x > (int)INT16_MAX ||
            left_trigger > (unsigned int)UINT16_MAX ||
            right_trigger > (unsigned int)UINT16_MAX)
        {
            (void)fprintf(
                stderr,
                "m4-movement-trace=fail operation=input-range frame=%" PRIu32
                " x=%d y=%d cx=%d cy=%d left=%u right=%u\n",
                trace_frame,
                input_x,
                input_y,
                input_c_x,
                input_c_y,
                left_trigger,
                right_trigger);
            return 1;
        }
        (void)memset(inputs, 0, sizeof(inputs));
        inputs[0].tick = inspection.tick;
        inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[0].player_slot = UINT8_C(0);
        inputs[0].main_stick_x = (int16_t)input_x;
        inputs[0].main_stick_y = (int16_t)input_y;
        inputs[0].secondary_stick_x = (int16_t)input_c_x;
        inputs[0].secondary_stick_y = (int16_t)input_c_y;
        inputs[0].left_trigger = (uint16_t)left_trigger;
        inputs[0].right_trigger = (uint16_t)right_trigger;
        inputs[0].buttons = buttons;
        inputs[1].tick = inspection.tick;
        inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[1].player_slot = UINT8_C(1);
        inputs[1].main_stick_x = (int16_t)opponent_input_x;
        inputs[1].buttons = opponent_buttons;
        status = pf_sim_tick(sim, inputs, (size_t)2, &result);
        if (status != PF_STATUS_OK)
        {
            return fail_status("tick", status);
        }
        status = pf_m4_inspect(sim, &inspection);
        if (status != PF_STATUS_OK)
        {
            return fail_status("inspect", status);
        }
        (void)printf(
            "%" PRIu32 ",%d,%d,%d,%d,%u,%u,%" PRIu64 ",%" PRIu64
            ",%u,%u,%d,%u,%d,%d,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
            ",%" PRId32
            ",%" PRIu32 ",%u,%u,%u,%" PRId32 ",%" PRId32 ",%" PRId32
            ",%" PRId32 ",%u,%u,%u,%u,%u,%u,%u,%d,%u,%" PRId32 ",%" PRId32
            ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRIu32 "\n",
            trace_frame,
            input_x,
            input_y,
            input_c_x,
            input_c_y,
            left_trigger,
            right_trigger,
            buttons,
            inspection.tick,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (int)inspection.players[0].facing,
            (unsigned int)inspection.players[0].grounded,
            (int)inspection.players[0].dash_direction,
            (int)inspection.players[0].previous_strong_direction,
            inspection.players[0].position_x_q16 - origin_x_q16,
            inspection.players[0].position_y_q16 - origin_y_q16,
            inspection.players[0].velocity_x_q16,
            inspection.players[0].velocity_y_q16,
            inspection.players[0].shield_recoil_x_q16,
            inspection.players[0].shield_health_q16,
            (unsigned int)inspection.players[0].shield_strength,
            (unsigned int)inspection.players[0].shield_angle_turn,
            (unsigned int)inspection.players[0].shield_magnitude,
            inspection.players[0].shield_left_q16 +
                (inspection.players[0].shield_right_q16 -
                 inspection.players[0].shield_left_q16) /
                    INT32_C(2) -
                inspection.players[0].position_x_q16,
            inspection.players[0].shield_top_q16 +
                (inspection.players[0].shield_bottom_q16 -
                 inspection.players[0].shield_top_q16) /
                    INT32_C(2) -
                inspection.players[0].position_y_q16,
            (inspection.players[0].shield_right_q16 -
             inspection.players[0].shield_left_q16) /
                INT32_C(2),
            (inspection.players[0].shield_bottom_q16 -
             inspection.players[0].shield_top_q16) /
                INT32_C(2),
            (unsigned int)inspection.players[0].powershield,
            (unsigned int)inspection.players[0].hitlag_ticks,
            (unsigned int)inspection.players[0].shield_stun_ticks,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (unsigned int)inspection.players[1].hitlag_ticks,
            (unsigned int)inspection.players[1].hitstun_ticks,
            (int)inspection.players[1].facing,
            (unsigned int)inspection.players[1].grounded,
            inspection.players[1].position_x_q16 -
                opponent_origin_x_q16,
            inspection.players[1].position_y_q16 -
                opponent_origin_y_q16,
            inspection.players[1].velocity_x_q16,
            inspection.players[1].velocity_y_q16,
            inspection.players[1].shield_recoil_x_q16,
            inspection.players[1].damage_q16);
        ++trace_frame;
    }
    if (ferror(stdin) != 0)
    {
        (void)fprintf(
            stderr,
            "m4-movement-trace=fail operation=read-input\n");
        return 1;
    }
    return 0;
}
