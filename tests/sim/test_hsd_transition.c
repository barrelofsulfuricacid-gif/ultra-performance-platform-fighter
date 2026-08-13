#include "pf/m4.h"
#include "pf/sim.h"
#include "sim_falcon_frame_data.h"
#include "sim_hsd_pose.h"
#include "sim_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#include "generated/ssbm_falcon_ground_loop_transition_oracle.inc"

#define TEST_ROTATION_TOLERANCE_Q15 INT32_C(8)
#define TEST_TRANSLATION_TOLERANCE_Q16 INT32_C(4)
#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

typedef struct qualified_action_ecb_case
{
    uint16_t source_submotion;
    uint16_t frame_count;
    uint8_t action_state;
    int8_t source_frame_offset;
} qualified_action_ecb_case;

static const qualified_action_ecb_case qualified_action_ecb_cases[] = {
    {UINT16_C(46), UINT16_C(21), (uint8_t)PF_M4_ACTION_GROUND_ATTACK, INT8_C(0)},
    {UINT16_C(47), UINT16_C(20), (uint8_t)PF_M4_ACTION_JAB_FINAL, INT8_C(-1)},
    {UINT16_C(48), UINT16_C(12), (uint8_t)PF_M4_ACTION_JAB_THIRD, INT8_C(-1)},
    {UINT16_C(49), UINT16_C(5), (uint8_t)PF_M4_ACTION_RAPID_JAB_START, INT8_C(0)},
    {UINT16_C(50), UINT16_C(40), (uint8_t)PF_M4_ACTION_RAPID_JAB_LOOP, INT8_C(-1)},
    {UINT16_C(51), UINT16_C(9), (uint8_t)PF_M4_ACTION_RAPID_JAB_END, INT8_C(-1)},
    {UINT16_C(52), UINT16_C(39), (uint8_t)PF_M4_ACTION_DASH_ATTACK, INT8_C(0)},
    {UINT16_C(53), UINT16_C(29), (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_HIGH, INT8_C(0)},
    {UINT16_C(54), UINT16_C(29), (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_MID_HIGH, INT8_C(0)},
    {UINT16_C(55), UINT16_C(29), (uint8_t)PF_M4_ACTION_FORWARD_ATTACK, INT8_C(0)},
    {UINT16_C(56), UINT16_C(29), (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_MID_LOW, INT8_C(0)},
    {UINT16_C(57), UINT16_C(29), (uint8_t)PF_M4_ACTION_FORWARD_ATTACK_LOW, INT8_C(0)},
    {UINT16_C(58), UINT16_C(39), (uint8_t)PF_M4_ACTION_UP_ATTACK, INT8_C(0)},
    {UINT16_C(59), UINT16_C(35), (uint8_t)PF_M4_ACTION_DOWN_ATTACK, INT8_C(0)},
    {UINT16_C(60), UINT16_C(64), (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_HIGH, INT8_C(0)},
    {UINT16_C(62), UINT16_C(64), (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK, INT8_C(0)},
    {UINT16_C(64), UINT16_C(64), (uint8_t)PF_M4_ACTION_FORWARD_STRONG_ATTACK_LOW, INT8_C(0)},
    {UINT16_C(66), UINT16_C(54), (uint8_t)PF_M4_ACTION_UP_STRONG_ATTACK, INT8_C(0)},
    {UINT16_C(67), UINT16_C(49), (uint8_t)PF_M4_ACTION_DOWN_STRONG_ATTACK, INT8_C(0)},
    {UINT16_C(242), UINT16_C(30), (uint8_t)PF_M4_ACTION_GRAB, INT8_C(-1)},
    {UINT16_C(243), UINT16_C(40), (uint8_t)PF_M4_ACTION_DASH_GRAB, INT8_C(-1)},
    {UINT16_C(68), UINT16_C(44), (uint8_t)PF_M4_ACTION_AERIAL_ATTACK, INT8_C(0)},
    {UINT16_C(69), UINT16_C(39), (uint8_t)PF_M4_ACTION_FORWARD_AERIAL, INT8_C(0)},
    {UINT16_C(70), UINT16_C(35), (uint8_t)PF_M4_ACTION_BACK_AERIAL, INT8_C(0)},
    {UINT16_C(71), UINT16_C(33), (uint8_t)PF_M4_ACTION_UP_AERIAL, INT8_C(0)},
    {UINT16_C(72), UINT16_C(44), (uint8_t)PF_M4_ACTION_DOWN_AERIAL, INT8_C(0)},
};

static int run_qualified_action_ecb_cases(uint32_t *out_pose_count)
{
    uint32_t pose_count = UINT32_C(0);
    size_t case_index;

    for (case_index = 0;
         case_index < sizeof(qualified_action_ecb_cases) /
                          sizeof(qualified_action_ecb_cases[0]);
         ++case_index)
    {
        const qualified_action_ecb_case *test_case =
            &qualified_action_ecb_cases[case_index];
        uint16_t action_ticks;

        for (action_ticks = UINT16_C(0);
             action_ticks < test_case->frame_count;
             ++action_ticks)
        {
            const int32_t source_frame =
                (int32_t)action_ticks + INT32_C(1) +
                test_case->source_frame_offset;
            falcon_ecb_pose_q16 actual;
            falcon_ecb_pose_q16 expected;

            if (!falcon_reference_action_hsd_ecb_pose(
                    test_case->action_state,
                    action_ticks,
                    UINT8_C(1),
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                    &actual) ||
                !falcon_reference_hsd_ecb_pose(
                    test_case->source_submotion,
                    source_frame * (int32_t)PF_Q16_ONE,
                    1,
                    PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
                    &expected) ||
                memcmp(&actual, &expected, sizeof(actual)) != 0)
            {
                (void)fprintf(
                    stderr,
                    "m4-hsd-transition=fail operation=qualified-action-ecb"
                    " case=%zu action=%u action_ticks=%u"
                    " source_submotion=%u source_frame=%" PRId32 "\n",
                    case_index,
                    (unsigned int)test_case->action_state,
                    (unsigned int)action_ticks,
                    (unsigned int)test_case->source_submotion,
                    source_frame);
                return 0;
            }
            ++pose_count;
        }
    }
    if (falcon_reference_action_hsd_ecb_pose(
            (uint8_t)PF_M4_ACTION_LEDGE_ATTACK,
            UINT16_C(0),
            UINT8_C(1),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &(falcon_ecb_pose_q16){0}))
    {
        (void)fprintf(
            stderr,
            "m4-hsd-transition=fail operation=qualified-action-exclusion\n");
        return 0;
    }
    *out_pose_count = pose_count;
    return 1;
}

static int run_production_common_air_entry_ecb_lock(void)
{
    test_sim_storage storage;
    struct content content;
    pf_content_view view;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_tick_result result;
    struct inspection inspection;
    falcon_ecb_pose_q16 unlocked_frame_9;
    uint32_t tick;

    if (default_content(&content) != PF_STATUS_OK ||
        make_content_view(&content, &view) != PF_STATUS_OK ||
        pf_sim_default_config(&config, UINT8_C(2), PF_SIM_MODE_DUEL) !=
            PF_STATUS_OK ||
        pf_sim_init(
            storage.state,
            sizeof(storage.state),
            storage.scratch,
            sizeof(storage.scratch),
            &view,
            &config,
            &sim) != PF_STATUS_OK ||
        pf_sim_reset(sim, UINT64_C(0x4543424c4f434b31)) != PF_STATUS_OK)
    {
        return 0;
    }
    sim->world.velocity_x_q16[0] = INT32_C(0);
    sim->world.velocity_y_q16[0] = INT32_C(0);
    sim->world.grounded[0] = UINT8_C(0);
    sim->world.support[0] = (uint8_t)PF_M4_SURFACE_NONE;
    sim->world.action_state[0] = (uint8_t)PF_M4_ACTION_AIRBORNE;
    sim->world.action_ticks[0] = UINT16_C(0);
    sim->world.source_submotion[0] =
        (uint16_t)PF_M4_FALCON_SUBMOTION_FALL;
    sim->world.source_animation_frame_q16[0] = INT32_C(0);
    sim->world.source_animation_rate_q16[0] = (int32_t)PF_Q16_ONE;
    sim->world.air_jumps_remaining[0] = UINT8_C(1);
    sim->world.ecb_bottom_lock_ticks[0] = UINT8_C(3);
    sim->world.ecb_locked_bottom_y_q16[0] = INT32_C(0);

    (void)memset(inputs, 0, sizeof(inputs));
    inputs[0].tick = sim->world.tick;
    inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
    inputs[0].player_slot = UINT8_C(0);
    inputs[0].buttons = PF_INPUT_BUTTON_JUMP;
    inputs[1].tick = sim->world.tick;
    inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
    inputs[1].player_slot = UINT8_C(1);
    if (pf_sim_tick(sim, inputs, (size_t)2, &result) != PF_STATUS_OK ||
        sim->world.ecb_bottom_lock_ticks[0] != UINT8_C(9) ||
        sim->world.ecb_locked_bottom_y_q16[0] != INT32_C(0) ||
        sim->world.source_submotion[0] !=
            (uint16_t)PF_M4_FALCON_SUBMOTION_JUMP_AERIAL_FORWARD)
    {
        (void)fprintf(
            stderr,
            "m4-hsd-transition=fail operation=common-ecb-lock-entry"
            " ticks=%u bottom=%" PRId32 " submotion=%u action=%u"
            " grounded=%u active=%u jumps=%u\n",
            (unsigned int)sim->world.ecb_bottom_lock_ticks[0],
            sim->world.ecb_locked_bottom_y_q16[0],
            (unsigned int)sim->world.source_submotion[0],
            (unsigned int)sim->world.action_state[0],
            (unsigned int)sim->world.grounded[0],
            (unsigned int)sim->world.active[0],
            (unsigned int)sim->world.air_jumps_remaining[0]);
        return 0;
    }

    for (tick = UINT32_C(0); tick < UINT32_C(9); ++tick)
    {
        (void)memset(inputs, 0, sizeof(inputs));
        inputs[0].tick = sim->world.tick;
        inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[0].player_slot = UINT8_C(0);
        inputs[0].buttons = tick == UINT32_C(0)
                                ? PF_INPUT_BUTTON_ATTACK
                                : UINT64_C(0);
        inputs[1].tick = sim->world.tick;
        inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[1].player_slot = UINT8_C(1);
        if (pf_sim_tick(sim, inputs, (size_t)2, &result) != PF_STATUS_OK ||
            inspect(sim, &inspection) != PF_STATUS_OK ||
            inspection.players[0].action_state !=
                (uint8_t)PF_M4_ACTION_AERIAL_ATTACK ||
            (tick < UINT32_C(8) &&
             inspection.players[0].ecb_bottom_y_from_origin_q16 !=
                 INT32_C(0)))
        {
            (void)fprintf(
                stderr,
                "m4-hsd-transition=fail operation=common-ecb-lock-hold"
                " tick=%" PRIu32 " action=%u bottom=%" PRId32 "\n",
                tick,
                (unsigned int)inspection.players[0].action_state,
                inspection.players[0].ecb_bottom_y_from_origin_q16);
            return 0;
        }
    }
    if (!falcon_reference_action_hsd_ecb_pose(
            (uint8_t)PF_M4_ACTION_AERIAL_ATTACK,
            UINT16_C(8),
            UINT8_C(0),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &unlocked_frame_9) ||
        sim->world.ecb_bottom_lock_ticks[0] != UINT8_C(0) ||
        inspection.players[0].ecb_bottom_y_from_origin_q16 !=
            unlocked_frame_9.bottom_y_from_origin_q16)
    {
        (void)fprintf(
            stderr,
            "m4-hsd-transition=fail operation=common-ecb-lock-release"
            " ticks=%u expected=%" PRId32 " actual=%" PRId32 "\n",
            (unsigned int)sim->world.ecb_bottom_lock_ticks[0],
            unlocked_frame_9.bottom_y_from_origin_q16,
            inspection.players[0].ecb_bottom_y_from_origin_q16);
        return 0;
    }
    return 1;
}

static int32_t absolute_difference_i32(int32_t left, int32_t right)
{
    const int64_t difference = (int64_t)left - (int64_t)right;

    return (int32_t)(difference < INT64_C(0) ? -difference : difference);
}

static int compare_compact_pose(
    const hsd_pose_data *data,
    const hsd_compact_pose *actual,
    const hsd_compact_pose *expected,
    uint32_t case_index,
    uint32_t trace_frame,
    const char *operation,
    int32_t rotation_tolerance,
    int32_t translation_tolerance,
    int32_t *maximum_rotation_difference,
    int32_t *maximum_translation_difference)
{
    uint8_t index;

    for (index = UINT8_C(0); index < data->rotation_joint_count; ++index)
    {
        uint8_t component;

        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            const int32_t difference = absolute_difference_i32(
                actual->rotation_q15[index][component],
                expected->rotation_q15[index][component]);

            if (difference > *maximum_rotation_difference)
            {
                *maximum_rotation_difference = difference;
            }
            if (difference > rotation_tolerance)
            {
                (void)fprintf(
                    stderr,
                    "m4-hsd-transition=fail operation=%s-rotation"
                    " case=%" PRIu32 " trace_frame=%" PRIu32
                    " joint=%u component=%u expected=%d actual=%d"
                    " difference=%" PRId32 "\n",
                    operation,
                    case_index,
                    trace_frame,
                    (unsigned int)index,
                    (unsigned int)component,
                    (int)expected->rotation_q15[index][component],
                    (int)actual->rotation_q15[index][component],
                    difference);
                return 0;
            }
        }
    }
    for (index = UINT8_C(0); index < data->translation_joint_count; ++index)
    {
        uint8_t component;

        for (component = UINT8_C(0); component < UINT8_C(3); ++component)
        {
            const int32_t difference = absolute_difference_i32(
                actual->translation_q16[index][component],
                expected->translation_q16[index][component]);

            if (difference > *maximum_translation_difference)
            {
                *maximum_translation_difference = difference;
            }
            if (difference > translation_tolerance)
            {
                (void)fprintf(
                    stderr,
                    "m4-hsd-transition=fail operation=%s-translation"
                    " case=%" PRIu32 " trace_frame=%" PRIu32
                    " joint=%u component=%u expected=%" PRId32
                    " actual=%" PRId32 " difference=%" PRId32 "\n",
                    operation,
                    case_index,
                    trace_frame,
                    (unsigned int)index,
                    (unsigned int)component,
                    expected->translation_q16[index][component],
                    actual->translation_q16[index][component],
                    difference);
                return 0;
            }
        }
    }
    return 1;
}

static int run_production_transition_cases(
    const hsd_pose_data *data,
    int32_t *production_maximum_rotation_difference,
    int32_t *production_maximum_translation_difference)
{
    test_sim_storage storage;
    struct content content;
    pf_content_view view;
    pf_sim_config config;
    pf_sim *sim = NULL;
    uint32_t case_index;

    if (default_content(&content) != PF_STATUS_OK ||
        make_content_view(&content, &view) != PF_STATUS_OK ||
        pf_sim_default_config(
            &config, UINT8_C(2), PF_SIM_MODE_DUEL) != PF_STATUS_OK ||
        pf_sim_init(
            storage.state,
            sizeof(storage.state),
            storage.scratch,
            sizeof(storage.scratch),
            &view,
            &config,
            &sim) != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr, "m4-hsd-transition=fail operation=production-init\n");
        return 0;
    }
    for (case_index = UINT32_C(0);
         case_index < PF_M4_HSD_TRANSITION_PRODUCTION_CASE_COUNT;
         ++case_index)
    {
        const hsd_transition_production_case *oracle =
            &hsd_transition_production_cases[case_index];
        pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
        pf_tick_result result;
        pf_world_state *world;
        hsd_local_pose previous_local[PF_M4_HSD_POSE_MAX_JOINTS];
        hsd_compact_pose previous_compact;
        hsd_local_pose target_local[PF_M4_HSD_POSE_MAX_JOINTS];
        hsd_compact_pose target_compact;
        int32_t reference_maximum_rotation_difference = INT32_C(0);
        int32_t reference_maximum_translation_difference = INT32_C(0);

        if (pf_sim_reset(sim, UINT64_C(0x4853445452414e53)) != PF_STATUS_OK)
        {
            return 0;
        }
        world = &sim->world;
        world->action_state[0] = oracle->previous_action;
        world->action_ticks[0] = oracle->previous_action_ticks;
        world->source_submotion[0] = oracle->previous_submotion;
        world->source_animation_frame_q16[0] = oracle->previous_frame_q16;
        world->source_animation_rate_q16[0] = oracle->previous_rate_q16;
        world->velocity_x_q16[0] = oracle->previous_velocity_x_q16;
        world->ground_blend_progress_q16[0] =
            oracle->previous_progress_q16;
        world->ground_blend_pose[0] = oracle->previous_compact;
        world->facing[0] = INT8_C(1);
        world->dash_direction[0] = INT8_C(1);
        world->previous_tilt_x_direction[0] = INT8_C(1);
        world->tilt_x_age[0] = UINT8_C(10);
        world->horizontal_input_direction[0] = INT8_C(1);
        world->horizontal_input_age[0] = UINT8_C(10);
        (void)memset(inputs, 0, sizeof(inputs));
        inputs[0].tick = world->tick;
        inputs[0].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[0].player_slot = UINT8_C(0);
        inputs[0].main_stick_x = oracle->input_x;
        inputs[1].tick = world->tick;
        inputs[1].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[1].player_slot = UINT8_C(1);
        if (oracle->previous_progress_q16 == INT32_C(0) &&
            (!hsd_evaluate_local_pose_q16(
                data,
                oracle->previous_submotion,
                oracle->previous_frame_q16,
                previous_local) ||
            !hsd_pack_compact_pose_q16(
                data, previous_local, &previous_compact) ||
            !compare_compact_pose(
                data,
                &previous_compact,
                &oracle->previous_compact,
                case_index,
                oracle->trace_frame,
                "production-source",
                TEST_ROTATION_TOLERANCE_Q15,
                INT32_C(8),
                &reference_maximum_rotation_difference,
                &reference_maximum_translation_difference)))
        {
            return 0;
        }
        if (!hsd_evaluate_local_pose_q16(
                data,
                oracle->expected_submotion,
                oracle->expected_frame_q16,
                target_local) ||
            !hsd_pack_compact_pose_q16(
                data, target_local, &target_compact) ||
            !compare_compact_pose(
                data,
                &target_compact,
                &oracle->expected_target_compact,
                case_index,
                oracle->trace_frame,
                "production-target",
                TEST_ROTATION_TOLERANCE_Q15,
                TEST_TRANSLATION_TOLERANCE_Q16,
                &reference_maximum_rotation_difference,
                &reference_maximum_translation_difference))
        {
            return 0;
        }
        if (pf_sim_tick(sim, inputs, (size_t)2, &result) != PF_STATUS_OK ||
            world->action_state[0] != oracle->expected_action ||
            world->source_submotion[0] != oracle->expected_submotion ||
            absolute_difference_i32(
                world->source_animation_frame_q16[0],
                oracle->expected_frame_q16) > INT32_C(2) ||
            absolute_difference_i32(
                world->ground_blend_progress_q16[0],
                oracle->expected_progress_q16) > INT32_C(2))
        {
            (void)fprintf(
                stderr,
                "m4-hsd-transition=fail operation=production-state"
                " case=%" PRIu32 " trace_frame=%" PRIu32
                " action=%u/%u submotion=%u/%u frame=%" PRId32
                "/%" PRId32 " progress=%" PRId32 "/%" PRId32 "\n",
                case_index,
                oracle->trace_frame,
                (unsigned int)world->action_state[0],
                (unsigned int)oracle->expected_action,
                (unsigned int)world->source_submotion[0],
                (unsigned int)oracle->expected_submotion,
                world->source_animation_frame_q16[0],
                oracle->expected_frame_q16,
                world->ground_blend_progress_q16[0],
                oracle->expected_progress_q16);
            return 0;
        }
        if (!compare_compact_pose(
                data,
                &world->ground_blend_pose[0],
                &oracle->expected_compact,
                case_index,
                oracle->trace_frame,
                "production",
                TEST_ROTATION_TOLERANCE_Q15,
                TEST_TRANSLATION_TOLERANCE_Q16,
                production_maximum_rotation_difference,
                production_maximum_translation_difference))
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    const hsd_pose_data *data =
        falcon_reference_hsd_pose_data();
    hsd_compact_pose current;
    uint32_t case_index;
    uint32_t qualified_action_pose_count = UINT32_C(0);
    int32_t maximum_rotation_difference = INT32_C(0);
    int32_t maximum_translation_difference = INT32_C(0);
    int32_t production_maximum_rotation_difference = INT32_C(0);
    int32_t production_maximum_translation_difference = INT32_C(0);
    falcon_ecb_pose_q16 raptor_ground_start = {0};
    falcon_ecb_pose_q16 raptor_air_start = {0};
    falcon_ecb_pose_q16 raptor_ground_hit = {0};
    falcon_ecb_pose_q16 raptor_air_hit = {0};
    falcon_ecb_pose_q16 dive_ground_start_airborne = {0};
    falcon_ecb_pose_q16 dive_air_start_relocked = {0};
    falcon_ecb_pose_q16 dive_ground_catch_entry = {0};
    falcon_ecb_pose_q16 dive_air_catch = {0};
    falcon_ecb_pose_q16 dive_throw_relocked = {0};
    falcon_ecb_pose_q16 fall_special_entry = {0};
    falcon_ecb_pose_q16 fall_special_direction_switch = {0};
    falcon_ecb_pose_q16 fall_special_direction_steady = {0};

    if (data == NULL ||
        data->rotation_joint_count != PF_M4_HSD_COMPACT_ROTATION_CAPACITY ||
        data->translation_joint_count !=
            PF_M4_HSD_COMPACT_TRANSLATION_CAPACITY ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_GROUND,
            INT32_C(3) * INT32_C(65536),
            1,
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &raptor_ground_start) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_START_AIR,
            INT32_C(1) * INT32_C(65536),
            0,
            INT32_C(0),
            &raptor_air_start) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_GROUND,
            INT32_C(1) * INT32_C(65536),
            1,
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &raptor_ground_hit) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_RAPTOR_BOOST_HIT_AIR,
            INT32_C(34) * INT32_C(65536),
            0,
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &raptor_air_hit) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND,
            INT32_C(16) * INT32_C(65536),
            0,
            INT32_C(0),
            &dive_ground_start_airborne) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_AIR,
            INT32_C(14) * INT32_C(65536),
            0,
            INT32_C(25250),
            &dive_air_start_relocked) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALCON_DIVE_START_GROUND,
            INT32_C(13) * INT32_C(65536),
            1,
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &dive_ground_catch_entry) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALCON_DIVE_CATCH,
            INT32_C(1) * INT32_C(65536),
            0,
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &dive_air_catch) ||
        !falcon_reference_hsd_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALCON_DIVE_THROW,
            INT32_C(45) * INT32_C(65536),
            0,
            INT32_C(92238),
            &dive_throw_relocked) ||
        !falcon_reference_hsd_fall_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALL_SPECIAL,
            INT32_C(0),
            INT32_C(0),
            UINT8_C(0),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &fall_special_entry) ||
        !falcon_reference_hsd_fall_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_FORWARD,
            PF_Q16_ONE,
            INT32_C(8547),
            UINT8_C(1),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &fall_special_direction_switch) ||
        !falcon_reference_hsd_fall_ecb_pose(
            PF_M4_FALCON_SUBMOTION_FALL_SPECIAL_FORWARD,
            INT32_C(2) * PF_Q16_ONE,
            INT32_C(12497),
            UINT8_C(0),
            PF_M4_FALCON_ECB_BOTTOM_UNLOCKED_Q16,
            &fall_special_direction_steady) ||
        absolute_difference_i32(
            raptor_ground_start.top_y_from_origin_q16,
            INT32_C(146354)) > INT32_C(32) ||
        absolute_difference_i32(
            raptor_ground_start.right_x_from_origin_q16,
            INT32_C(35880)) > INT32_C(32) ||
        absolute_difference_i32(
            raptor_ground_start.left_x_from_origin_q16,
            -INT32_C(51798)) > INT32_C(32) ||
        absolute_difference_i32(
            raptor_air_start.bottom_y_from_origin_q16,
            INT32_C(0)) > INT32_C(32) ||
        absolute_difference_i32(
            raptor_air_start.right_y_from_origin_q16,
            INT32_C(93519)) > INT32_C(32) ||
        absolute_difference_i32(
            raptor_ground_hit.right_x_from_origin_q16,
            INT32_C(62391)) > INT32_C(32) ||
        absolute_difference_i32(
            raptor_air_hit.bottom_y_from_origin_q16,
            INT32_C(19452)) > INT32_C(32) ||
        absolute_difference_i32(
            dive_ground_start_airborne.top_y_from_origin_q16,
            INT32_C(223674)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_ground_start_airborne.right_y_from_origin_q16,
            INT32_C(122505)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_air_start_relocked.bottom_y_from_origin_q16,
            INT32_C(25250)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_air_start_relocked.right_y_from_origin_q16,
            INT32_C(80774)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_ground_catch_entry.top_y_from_origin_q16,
            INT32_C(121441)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_air_catch.bottom_y_from_origin_q16,
            INT32_C(91620)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_throw_relocked.top_y_from_origin_q16,
            INT32_C(107511)) > INT32_C(64) ||
        absolute_difference_i32(
            dive_throw_relocked.right_y_from_origin_q16,
            INT32_C(99874)) > INT32_C(64) ||
        absolute_difference_i32(
            fall_special_entry.bottom_y_from_origin_q16,
            INT32_C(26815)) > INT32_C(64) ||
        absolute_difference_i32(
            fall_special_direction_switch.bottom_y_from_origin_q16,
            INT32_C(21771)) > INT32_C(64) ||
        absolute_difference_i32(
            fall_special_direction_steady.bottom_y_from_origin_q16,
            INT32_C(26863)) > INT32_C(64))
    {
        (void)fprintf(
            stderr,
            "m4-hsd-transition=fail operation=data"
            " ground_start=%" PRId32 "/%" PRId32 "/%" PRId32
            " air_start=%" PRId32 "/%" PRId32
            " ground_hit=%" PRId32 " air_hit_bottom=%" PRId32
            " dive=%" PRId32 "/%" PRId32 "/%" PRId32
            "/%" PRId32 "/%" PRId32 "\n",
            raptor_ground_start.top_y_from_origin_q16,
            raptor_ground_start.right_x_from_origin_q16,
            raptor_ground_start.left_x_from_origin_q16,
            raptor_air_start.bottom_y_from_origin_q16,
            raptor_air_start.right_y_from_origin_q16,
            raptor_ground_hit.right_x_from_origin_q16,
            raptor_air_hit.bottom_y_from_origin_q16,
            dive_ground_start_airborne.top_y_from_origin_q16,
            dive_air_start_relocked.bottom_y_from_origin_q16,
            dive_ground_catch_entry.top_y_from_origin_q16,
            dive_air_catch.bottom_y_from_origin_q16,
            dive_throw_relocked.top_y_from_origin_q16);
        return 1;
    }
    if (!run_qualified_action_ecb_cases(&qualified_action_pose_count) ||
        !run_production_common_air_entry_ecb_lock())
    {
        return 1;
    }
    (void)memset(&current, 0, sizeof(current));
    for (case_index = UINT32_C(0);
         case_index < PF_M4_HSD_TRANSITION_ORACLE_CASE_COUNT;
         ++case_index)
    {
        const hsd_transition_oracle_case *oracle =
            &hsd_transition_oracle_cases[case_index];
        hsd_local_pose target[PF_M4_HSD_POSE_MAX_JOINTS];
        hsd_local_pose source[PF_M4_HSD_POSE_MAX_JOINTS];
        hsd_local_pose result[PF_M4_HSD_POSE_MAX_JOINTS];
        hsd_compact_pose actual;

        if (oracle->reset != UINT8_C(0))
        {
            current = oracle->prior;
        }
        if (!hsd_evaluate_local_pose_q16(
                data,
                oracle->source_submotion,
                oracle->frame_q16,
                target) ||
            !hsd_inflate_compact_pose_q16(
                data, target, &current, source) ||
            !hsd_blend_local_pose_q16(
                data,
                target,
                source,
                oracle->current_weight_q16,
                result) ||
            !hsd_pack_compact_pose_q16(data, result, &actual))
        {
            (void)fprintf(
                stderr,
                "m4-hsd-transition=fail operation=evaluate case=%" PRIu32
                " trace_frame=%" PRIu32 "\n",
                case_index,
                oracle->trace_frame);
            return 1;
        }
        if (!compare_compact_pose(
                data,
                &actual,
                &oracle->expected,
                case_index,
                oracle->trace_frame,
                "recurrence",
                TEST_ROTATION_TOLERANCE_Q15,
                TEST_TRANSLATION_TOLERANCE_Q16,
                &maximum_rotation_difference,
                &maximum_translation_difference))
        {
            return 1;
        }
        current = actual;
    }
    if (!run_production_transition_cases(
            data,
            &production_maximum_rotation_difference,
            &production_maximum_translation_difference))
    {
        return 1;
    }
    (void)printf(
        "m4-hsd-transition=pass cases=%" PRIu32
        " production_cases=%" PRIu32
        " action_ecb_cases=9 action_ecb_tolerance_q16=64"
        " qualified_action_ecb_cases=%zu qualified_action_ecb_poses=%" PRIu32
        " fall_animation_ecb_cases=3"
        " rotation_max_q15=%" PRId32 " translation_max_q16=%" PRId32
        " production_rotation_max_q15=%" PRId32
        " production_translation_max_q16=%" PRId32
        " semantic_sha256=%s\n",
        PF_M4_HSD_TRANSITION_ORACLE_CASE_COUNT,
        PF_M4_HSD_TRANSITION_PRODUCTION_CASE_COUNT,
        sizeof(qualified_action_ecb_cases) /
            sizeof(qualified_action_ecb_cases[0]),
        qualified_action_pose_count,
        maximum_rotation_difference,
        maximum_translation_difference,
        production_maximum_rotation_difference,
        production_maximum_translation_difference,
        PF_M4_HSD_TRANSITION_ORACLE_SEMANTIC_SHA256);
    return 0;
}
