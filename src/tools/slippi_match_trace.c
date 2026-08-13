#include "pf/m4.h"
#include "pf/sim.h"
#include "sim_falcon_frame_data.h"
#include "sim_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PF_MATCH_TRACE_MEMORY_BYTES 4096U
#define PF_MATCH_TRACE_MEMORY_ALIGNMENT 64U
#define PF_MATCH_TRACE_FIELD_COUNT 25U

typedef struct pf_match_trace_storage
{
    alignas(PF_MATCH_TRACE_MEMORY_ALIGNMENT)
        uint8_t state[PF_MATCH_TRACE_MEMORY_BYTES];
    alignas(PF_MATCH_TRACE_MEMORY_ALIGNMENT)
        uint8_t scratch[PF_MATCH_TRACE_MEMORY_BYTES];
} pf_match_trace_storage;

static int fail_status(const char *operation, pf_status status)
{
    (void)fprintf(
        stderr,
        "m4-slippi-match-trace=fail operation=%s status=%s\n",
        operation,
        pf_status_name(status));
    return 1;
}

static int parse_i64(const char *text, int64_t *out_value)
{
    char *end = NULL;
    long long value;

    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
    {
        return 0;
    }
    *out_value = (int64_t)value;
    return 1;
}

static int parse_u64(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;

    if (*text == '-')
    {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
    {
        return 0;
    }
    *out_value = (uint64_t)value;
    return 1;
}

static int split_fields(
    char *line,
    char *fields[PF_MATCH_TRACE_FIELD_COUNT])
{
    size_t count;
    char *cursor = line;

    for (count = 0U; count < PF_MATCH_TRACE_FIELD_COUNT; ++count)
    {
        char *comma;

        fields[count] = cursor;
        comma = strchr(cursor, ',');
        if (count + 1U == PF_MATCH_TRACE_FIELD_COUNT)
        {
            if (comma != NULL)
            {
                return 0;
            }
            break;
        }
        if (comma == NULL)
        {
            return 0;
        }
        *comma = '\0';
        cursor = comma + 1;
    }
    cursor = fields[PF_MATCH_TRACE_FIELD_COUNT - 1U];
    cursor[strcspn(cursor, "\r\n")] = '\0';
    return *cursor != '\0';
}

static int parse_player_input(
    char *const fields[PF_MATCH_TRACE_FIELD_COUNT],
    size_t offset,
    uint64_t tick,
    uint8_t player_slot,
    pf_input_frame *out_input)
{
    int64_t values[11];
    uint64_t buttons;
    size_t index;
    pf_input_raw_pad raw_pad;

    for (index = 0U; index < 6U; ++index)
    {
        if (!parse_i64(fields[offset + index], &values[index]))
        {
            return 0;
        }
    }
    if (!parse_u64(fields[offset + 6U], &buttons))
    {
        return 0;
    }
    for (index = 7U; index < 12U; ++index)
    {
        if (!parse_i64(fields[offset + index], &values[index - 1U]))
        {
            return 0;
        }
    }
    if (values[0] < INT16_MIN || values[0] > INT16_MAX ||
        values[1] < INT16_MIN || values[1] > INT16_MAX ||
        values[2] < INT16_MIN || values[2] > INT16_MAX ||
        values[3] < INT16_MIN || values[3] > INT16_MAX ||
        values[4] < 0 || values[4] > UINT16_MAX ||
        values[5] < 0 || values[5] > UINT16_MAX ||
        values[6] < INT8_MIN || values[6] > INT8_MAX ||
        values[7] < INT8_MIN || values[7] > INT8_MAX ||
        values[8] < INT8_MIN || values[8] > INT8_MAX ||
        values[9] < INT8_MIN || values[9] > INT8_MAX ||
        (values[10] !=
             (PF_INPUT_RAW_MAIN_X_VALID | PF_INPUT_RAW_MAIN_Y_VALID) &&
         values[10] != PF_INPUT_RAW_PAD_ALL_VALID) ||
        (values[10] != PF_INPUT_RAW_PAD_ALL_VALID &&
         (values[8] != 0 || values[9] != 0)))
    {
        return 0;
    }

    (void)memset(out_input, 0, sizeof(*out_input));
    out_input->tick = tick;
    out_input->buttons = buttons;
    out_input->main_stick_x = (int16_t)values[0];
    out_input->main_stick_y = (int16_t)values[1];
    out_input->secondary_stick_x = (int16_t)values[2];
    out_input->secondary_stick_y = (int16_t)values[3];
    out_input->left_trigger = (uint16_t)values[4];
    out_input->right_trigger = (uint16_t)values[5];
    out_input->schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
    out_input->player_slot = player_slot;
    raw_pad.main_stick_x = (int8_t)values[6];
    raw_pad.main_stick_y = (int8_t)values[7];
    raw_pad.secondary_stick_x = (int8_t)values[8];
    raw_pad.secondary_stick_y = (int8_t)values[9];
    pf_input_set_raw_pad(out_input, raw_pad);
    out_input->raw_axis_valid_mask = (uint8_t)values[10];
    return 1;
}

static void print_player(
    const player_inspection *player,
    uint8_t ecb_bottom_lock_ticks,
    float ecb_locked_bottom_y_f32)
{
    falcon_ecb_pose_f32 pose;
    const int pose_available = falcon_reference_hsd_ecb_pose(
        player->source_submotion,
        player->source_animation_frame_f32,
        player->grounded != UINT8_C(0),
        PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_F32,
        &pose);

    (void)printf(
        ",%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%u,%.9g,%.9g,%.9g,%.9g"
        ",%.9g,%.9g,%.9g,%.9g,%.9g,%d,%u,%u,%u,%u,%.9g"
        ",%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%.9g,%.9g,%.9g,%.9g"
        ",%.9g,%.9g,%.9g,%.9g,%u,%.9g",
        (unsigned int)player->action_state,
        (unsigned int)player->action_ticks,
        (unsigned int)player->source_submotion,
        player->source_animation_frame_f32,
        player->source_animation_rate_f32,
        player->fall_animation_blend_f32,
        player->ecb_bottom_y_from_origin_f32,
        (unsigned int)player->fall_animation_target_switched,
        player->position_x_f32,
        player->position_y_f32,
        player->self_velocity_x_f32,
        player->self_velocity_y_f32,
        player->knockback_velocity_x_f32,
        player->knockback_velocity_y_f32,
        player->ground_knockback_velocity_f32,
        player->velocity_x_f32,
        player->velocity_y_f32,
        (int)player->facing,
        (unsigned int)player->grounded,
        (unsigned int)player->support,
        (unsigned int)player->platform_drop_ticks,
        (unsigned int)player->fast_fall,
        player->damage_f32,
        (unsigned int)player->stocks_remaining,
        (unsigned int)player->hitlag_ticks,
        (unsigned int)player->hitstun_ticks,
        (unsigned int)player->shield_stun_ticks,
        (unsigned int)player->tumble,
        (unsigned int)player->invulnerable,
        (unsigned int)player->active,
        (unsigned int)player->respawn_ticks,
        (int)player->dash_direction,
        (int)player->previous_strong_direction,
        (unsigned int)player->tilt_x_age,
        pose_available != 0 ? pose.top_x_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.top_y_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.bottom_x_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.bottom_y_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.right_x_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.right_y_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.left_x_from_origin_f32 : 0.0f,
        pose_available != 0 ? pose.left_y_from_origin_f32 : 0.0f,
        (unsigned int)ecb_bottom_lock_ticks,
        ecb_locked_bottom_y_f32);
}

int main(int argc, char **argv)
{
    pf_match_trace_storage storage;
    struct content content;
    pf_content_view view;
    pf_sim_config config;
    pf_sim *sim = NULL;
    struct inspection inspection;
    pf_status status;
    uint64_t seed = UINT64_C(0);
    uint64_t max_ticks = UINT64_C(40000);
    long stage_id;
    char *end = NULL;
    char input_line[1024];
    uint64_t row = UINT64_C(0);

    if (argc != 7 || strcmp(argv[1], "--stage-id") != 0 ||
        strcmp(argv[3], "--seed") != 0 ||
        strcmp(argv[5], "--max-ticks") != 0)
    {
        (void)fprintf(
            stderr,
            "usage: slippi_match_trace --stage-id ID --seed SEED "
            "--max-ticks TICKS\n");
        return 1;
    }
    errno = 0;
    stage_id = strtol(argv[2], &end, 10);
    if (errno != 0 || end == argv[2] || *end != '\0')
    {
        (void)fprintf(stderr, "m4-slippi-match-trace=fail operation=stage-id\n");
        return 1;
    }
    if (!parse_u64(argv[4], &seed) || !parse_u64(argv[6], &max_ticks) ||
        max_ticks == UINT64_C(0))
    {
        (void)fprintf(stderr, "m4-slippi-match-trace=fail operation=arguments\n");
        return 1;
    }
    if (stage_id != 31L)
    {
        (void)fprintf(
            stderr,
            "m4-slippi-match-trace=fail operation=stage-not-imported "
            "stage_id=%ld\n",
            stage_id);
        return 1;
    }

    (void)memset(&storage, 0, sizeof(storage));
    status = reference_stage_content(
        PF_M4_REFERENCE_STAGE_BATTLEFIELD,
        &content);
    if (status != PF_STATUS_OK)
    {
        return fail_status("reference-stage-content", status);
    }
    content.item.enabled = UINT8_C(0);
    content.projectile.enabled = UINT8_C(1);
    content.reflector.enabled = UINT8_C(1);
    status = make_content_view(&content, &view);
    if (status != PF_STATUS_OK)
    {
        return fail_status("content-view", status);
    }
    status = pf_sim_default_config(&config, UINT8_C(2), PF_SIM_MODE_DUEL);
    if (status != PF_STATUS_OK)
    {
        return fail_status("default-config", status);
    }
    config.max_ticks = max_ticks;
    config.arena_half_width_f32 = INT32_C(256) * PF_F32_ONE;
    config.arena_ceiling_f32 = INT32_C(256) * PF_F32_ONE;
    config.stock_count = UINT8_C(4);
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
    status = pf_sim_reset(sim, seed);
    if (status != PF_STATUS_OK)
    {
        return fail_status("reset", status);
    }
    status = start_reference_match(sim);
    if (status != PF_STATUS_OK)
    {
        return fail_status("start-reference-match", status);
    }
    status = inspect(sim, &inspection);
    if (status != PF_STATUS_OK)
    {
        return fail_status("inspect-origin", status);
    }

    (void)puts(
        "source_frame,tick,terminated,truncated,winner_mask,"
        "p0_action,p0_action_ticks,p0_submotion,p0_source_frame_f32,"
        "p0_source_rate_f32,p0_fall_blend_f32,p0_ecb_bottom_f32,"
        "p0_fall_target_switched,p0_x_f32,p0_y_f32,"
        "p0_self_vx_f32,p0_self_vy_f32,p0_kb_vx_f32,p0_kb_vy_f32,"
        "p0_ground_kb_f32,"
        "p0_vx_f32,p0_vy_f32,p0_facing,p0_grounded,p0_support,"
        "p0_platform_drop_ticks,p0_fast_fall,"
        "p0_damage_f32,p0_stocks,p0_hitlag,p0_hitstun,p0_shield_stun,"
        "p0_tumble,p0_invulnerable,p0_active,p0_respawn,"
        "p0_dash_direction,p0_previous_strong_direction,p0_tilt_x_age,"
        "p0_ecb_top_x_f32,p0_ecb_top_y_f32,"
        "p0_ecb_bottom_x_f32,p0_ecb_pose_bottom_y_f32,"
        "p0_ecb_right_x_f32,p0_ecb_right_y_f32,"
        "p0_ecb_left_x_f32,p0_ecb_left_y_f32,"
        "p0_ecb_lock_ticks,p0_ecb_locked_bottom_y_f32,"
        "p1_action,p1_action_ticks,p1_submotion,p1_source_frame_f32,"
        "p1_source_rate_f32,p1_fall_blend_f32,p1_ecb_bottom_f32,"
        "p1_fall_target_switched,p1_x_f32,p1_y_f32,"
        "p1_self_vx_f32,p1_self_vy_f32,p1_kb_vx_f32,p1_kb_vy_f32,"
        "p1_ground_kb_f32,"
        "p1_vx_f32,p1_vy_f32,p1_facing,p1_grounded,p1_support,"
        "p1_platform_drop_ticks,p1_fast_fall,"
        "p1_damage_f32,p1_stocks,p1_hitlag,p1_hitstun,p1_shield_stun,"
        "p1_tumble,p1_invulnerable,p1_active,p1_respawn,"
        "p1_dash_direction,p1_previous_strong_direction,p1_tilt_x_age,"
        "p1_ecb_top_x_f32,p1_ecb_top_y_f32,"
        "p1_ecb_bottom_x_f32,p1_ecb_pose_bottom_y_f32,"
        "p1_ecb_right_x_f32,p1_ecb_right_y_f32,"
        "p1_ecb_left_x_f32,p1_ecb_left_y_f32,"
        "p1_ecb_lock_ticks,p1_ecb_locked_bottom_y_f32");

    while (fgets(input_line, sizeof(input_line), stdin) != NULL)
    {
        char *fields[PF_MATCH_TRACE_FIELD_COUNT];
        int64_t source_frame;
        pf_input_frame inputs[2];
        pf_tick_result result;

        if (!split_fields(input_line, fields) ||
            !parse_i64(fields[0], &source_frame) ||
            !parse_player_input(fields, 1U, row, UINT8_C(0), &inputs[0]) ||
            !parse_player_input(fields, 13U, row, UINT8_C(1), &inputs[1]))
        {
            (void)fprintf(
                stderr,
                "m4-slippi-match-trace=fail operation=input-format row=%" PRIu64
                "\n",
                row);
            return 1;
        }
        status = pf_sim_tick(sim, inputs, (size_t)2, &result);
        if (status != PF_STATUS_OK)
        {
            return fail_status("tick", status);
        }
        status = inspect(sim, &inspection);
        if (status != PF_STATUS_OK)
        {
            return fail_status("inspect", status);
        }
        (void)printf(
            "%" PRId64 ",%" PRIu64 ",%u,%u,%u",
            source_frame,
            inspection.tick,
            (unsigned int)inspection.terminated,
            (unsigned int)inspection.truncated,
            (unsigned int)inspection.winner_mask);
        print_player(
            &inspection.players[0],
            sim->world.ecb_bottom_lock_ticks[0],
            sim->world.ecb_locked_bottom_y_f32[0]);
        print_player(
            &inspection.players[1],
            sim->world.ecb_bottom_lock_ticks[1],
            sim->world.ecb_locked_bottom_y_f32[1]);
        (void)putchar('\n');
        ++row;
    }
    if (ferror(stdin) != 0)
    {
        (void)fprintf(stderr, "m4-slippi-match-trace=fail operation=read-input\n");
        return 1;
    }
    (void)fprintf(
        stderr,
        "m4-slippi-match-trace=pass rows=%" PRIu64 " final_tick=%" PRIu64
        " terminated=%u winner_mask=%u\n",
        row,
        inspection.tick,
        (unsigned int)inspection.terminated,
        (unsigned int)inspection.winner_mask);
    return 0;
}
