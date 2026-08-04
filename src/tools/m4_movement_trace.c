#include "pf/m4.h"
#include "pf/sim.h"

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
    unsigned int left_trigger;
    unsigned int right_trigger;
    uint64_t buttons;
    pf_status status;
    int platform_mode = 0;
    int push_mode = 0;

    if (argc == 2 && strcmp(argv[1], "--platform") == 0)
    {
        platform_mode = 1;
    }
    else if (argc == 2 && strcmp(argv[1], "--push") == 0)
    {
        push_mode = 1;
    }
    else if (argc != 1)
    {
        (void)fprintf(
            stderr,
            "usage: pf_m4_movement_trace [--platform|--push]\n");
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
    content.stage.floor_left_q16 = -INT32_C(128) * PF_Q16_ONE;
    content.stage.floor_right_q16 = INT32_C(128) * PF_Q16_ONE;
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
    if (push_mode != 0)
    {
        /* Final Destination starts ports one and two at -60/+60. */
        content.stage.spawn_spacing_q16 =
            (int32_t)((INT64_C(144) * PF_Q16_ONE) / INT64_C(23));
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
    config.arena_ceiling_q16 = INT32_C(256) * PF_Q16_ONE;
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
        "velocity_x_q16,velocity_y_q16,shield_health_q16,shield_strength,"
        "powershield,opponent_action_state,opponent_action_ticks,"
        "opponent_facing,opponent_grounded,"
        "opponent_position_x_q16_from_origin,"
        "opponent_position_y_q16_from_origin,"
        "opponent_velocity_x_q16,opponent_velocity_y_q16");
    while (scanf(
               "%d,%d,%d,%d,%u,%u,%" SCNu64 ",%d",
               &input_x,
               &input_y,
               &input_c_x,
               &input_c_y,
               &left_trigger,
               &right_trigger,
               &buttons,
               &opponent_input_x) == 8)
    {
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;

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
            ",%" PRIu32 ",%u,%u,%u,%u,%d,%u,%" PRId32 ",%" PRId32
            ",%" PRId32 ",%" PRId32 "\n",
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
            inspection.players[0].shield_health_q16,
            (unsigned int)inspection.players[0].shield_strength,
            (unsigned int)inspection.players[0].powershield,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.players[1].action_ticks,
            (int)inspection.players[1].facing,
            (unsigned int)inspection.players[1].grounded,
            inspection.players[1].position_x_q16 -
                opponent_origin_x_q16,
            inspection.players[1].position_y_q16 -
                opponent_origin_y_q16,
            inspection.players[1].velocity_x_q16,
            inspection.players[1].velocity_y_q16);
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
