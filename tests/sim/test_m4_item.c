#include "pf/m4.h"
#include "pf/replay.h"
#include "pf/rl.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 8192U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_REPLAY_TICKS UINT32_C(24)

typedef struct test_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_storage;

typedef struct test_command
{
    int16_t x;
    int16_t y;
    uint64_t buttons;
    uint16_t trigger;
} test_command;

static int fail(const char *operation)
{
    (void)fprintf(stderr, "m4-item=fail operation=%s\n", operation);
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
            "m4-item=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
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

static int event_equal(
    const pf_tick_result *left,
    const pf_tick_result *right)
{
    return left->completed_tick == right->completed_tick &&
           left->fault_flags == right->fault_flags &&
           left->terminated == right->terminated &&
           left->truncated == right->truncated &&
           left->winner_mask == right->winner_mask &&
           left->event_count == right->event_count &&
           memcmp(
               left->events,
               right->events,
               sizeof(left->events[0]) * (size_t)left->event_count) == 0;
}

static const pf_sim_event *find_event(
    const pf_tick_result *result,
    pf_sim_event_type type)
{
    uint32_t event_index;

    for (event_index = UINT32_C(0);
         event_index < (uint32_t)result->event_count;
         ++event_index)
    {
        if (result->events[event_index].type == (uint16_t)type)
        {
            return &result->events[event_index];
        }
    }
    return NULL;
}

static int make_item_content(
    pf_m4_content *content,
    pf_content_view *view)
{
    if (!expect_status(
            pf_m4_default_content(content),
            PF_STATUS_OK,
            "default-content"))
    {
        return 0;
    }
    content->stage.spawn_spacing_q16 = PF_Q16_ONE;
    content->item.enabled = UINT8_C(1);
    content->item.spawn_x_q16 = -PF_Q16_ONE / INT32_C(2);
    content->item.spawn_y_q16 =
        content->stage.floor_y_q16 - content->item.half_height_q16;
    return expect_status(
        pf_m4_make_content_view(content, view),
        PF_STATUS_OK,
        "item-content-view");
}

static int initialize_sim(
    test_storage *storage,
    const pf_content_view *view,
    pf_sim **out_sim)
{
    pf_sim_config config;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "default-config"))
    {
        return 0;
    }
    config.max_ticks = UINT64_C(4000);
    return expect_status(
        pf_sim_init(
            storage->state,
            sizeof(storage->state),
            storage->scratch,
            sizeof(storage->scratch),
            view,
            &config,
            out_sim),
        PF_STATUS_OK,
        "sim-init");
}

static int reset_sim(pf_sim *sim)
{
    return expect_status(
        pf_sim_reset(sim, UINT64_C(0x52454c4159524f44)),
        PF_STATUS_OK,
        "sim-reset");
}

static int step_sim(
    pf_sim *sim,
    test_command player0,
    test_command player1,
    pf_tick_result *out_result,
    pf_m4_inspection *out_inspection)
{
    pf_m4_inspection before;
    pf_input_frame inputs[PF_SIM_MAX_PLAYERS];
    pf_status step_status;
    uint32_t player_index;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-step"))
    {
        return 0;
    }
    (void)memset(inputs, 0, sizeof(inputs));
    for (player_index = UINT32_C(0);
         player_index < UINT32_C(2);
         ++player_index)
    {
        inputs[player_index].tick = before.tick;
        inputs[player_index].schema_version =
            PF_SIM_INPUT_SCHEMA_VERSION;
        inputs[player_index].player_slot = (uint8_t)player_index;
    }
    inputs[0].main_stick_x = player0.x;
    inputs[0].main_stick_y = player0.y;
    inputs[0].buttons = player0.buttons;
    inputs[0].left_trigger = player0.trigger;
    inputs[1].main_stick_x = player1.x;
    inputs[1].main_stick_y = player1.y;
    inputs[1].buttons = player1.buttons;
    inputs[1].left_trigger = player1.trigger;
    step_status = pf_sim_tick(sim, inputs, (size_t)2, out_result);
    if (step_status != PF_STATUS_OK)
    {
        (void)fprintf(
            stderr,
            "m4-item=debug tick=%" PRIu64 " p0_action=%u "
            "p0_action_ticks=%u item_state=%u item_holder=%u "
            "buttons=%" PRIu64 " trigger=%u fault_flags=%" PRIu32 "\n",
            before.tick,
            (unsigned int)before.players[0].action_state,
            (unsigned int)before.players[0].action_ticks,
            (unsigned int)before.item.state,
            (unsigned int)before.item.holder,
            player0.buttons,
            (unsigned int)player0.trigger,
            out_result->fault_flags);
    }
    if (!expect_status(step_status, PF_STATUS_OK, "sim-step") ||
        !expect_status(
            pf_m4_inspect(sim, out_inspection),
            PF_STATUS_OK,
            "inspect-after-step"))
    {
        return 0;
    }
    return 1;
}

static int neutral_step(
    pf_sim *sim,
    pf_tick_result *result,
    pf_m4_inspection *inspection)
{
    const test_command neutral = {0};

    return step_sim(sim, neutral, neutral, result, inspection);
}

static int pickup_item(
    pf_sim *sim,
    pf_tick_result *result,
    pf_m4_inspection *inspection)
{
    const test_command pickup = {
        INT16_C(0),
        INT16_C(0),
        PF_INPUT_BUTTON_ATTACK,
        UINT16_MAX};
    const test_command neutral = {0};
    const pf_sim_event *event;

    if (!step_sim(sim, pickup, neutral, result, inspection))
    {
        return 0;
    }
    event = find_event(result, PF_SIM_EVENT_ITEM_PICKUP);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        inspection->item.state != (uint8_t)PF_M4_ITEM_STATE_HELD ||
        inspection->item.holder != UINT8_C(0) ||
        inspection->players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
    {
        return fail("pickup");
    }
    return neutral_step(sim, result, inspection);
}

static int run_content_contract(void)
{
    pf_m4_content disabled;
    pf_m4_content enabled;
    pf_m4_content invalid;
    pf_content_view disabled_view;
    pf_content_view enabled_view;
    test_storage storage;
    pf_sim *sim = NULL;
    pf_m4_inspection inspection;

    if (!expect_status(
            pf_m4_default_content(&disabled),
            PF_STATUS_OK,
            "disabled-default") ||
        disabled.item.enabled != UINT8_C(0) ||
        disabled.item.struct_size != (uint32_t)sizeof(disabled.item) ||
        disabled.item.schema_version != PF_M4_ITEM_SCHEMA_VERSION ||
        !expect_status(
            pf_m4_make_content_view(&disabled, &disabled_view),
            PF_STATUS_OK,
            "disabled-view") ||
        !make_item_content(&enabled, &enabled_view) ||
        memcmp(
            disabled_view.content_hash.bytes,
            enabled_view.content_hash.bytes,
            sizeof(disabled_view.content_hash.bytes)) == 0)
    {
        return fail("content-contract");
    }

    invalid = enabled;
    invalid.item.enabled = UINT8_C(2);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "invalid-enabled"))
    {
        return 0;
    }
    invalid = enabled;
    invalid.item.glide_toss_end_tick =
        enabled.fighter.forward_roll_ticks;
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "invalid-glide-window"))
    {
        return 0;
    }
    invalid = enabled;
    invalid.item.spawn_y_q16 -= INT32_C(1);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "invalid-spawn"))
    {
        return 0;
    }

    if (!initialize_sim(&storage, &disabled_view, &sim) ||
        !reset_sim(sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "inspect-disabled") ||
        inspection.item.enabled != UINT8_C(0) ||
        inspection.item.state !=
            (uint8_t)PF_M4_ITEM_STATE_INACTIVE)
    {
        return fail("disabled-state");
    }
    return 1;
}

static int run_directional_throw_contract(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    static const test_command throws[4] = {
        {INT16_MAX, INT16_C(0), PF_INPUT_BUTTON_ATTACK, UINT16_C(0)},
        {INT16_MIN, INT16_C(0), PF_INPUT_BUTTON_ATTACK, UINT16_C(0)},
        {INT16_C(0), INT16_MIN, PF_INPUT_BUTTON_ATTACK, UINT16_C(0)},
        {INT16_C(0), INT16_MAX, PF_INPUT_BUTTON_ATTACK, UINT16_C(0)}};
    static const uint8_t expected_directions[4] = {
        (uint8_t)PF_M4_ITEM_THROW_FORWARD,
        (uint8_t)PF_M4_ITEM_THROW_BACK,
        (uint8_t)PF_M4_ITEM_THROW_UP,
        (uint8_t)PF_M4_ITEM_THROW_DOWN};
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    pf_state_hash grounded_drop_hash;
    uint32_t direction_index;

    (void)content;
    if (!initialize_sim(&storage, view, &sim))
    {
        return 0;
    }
    {
        const test_command grounded_drop = {
            INT16_C(0),
            INT16_C(0),
            PF_INPUT_BUTTON_ATTACK,
            UINT16_MAX};
        const test_command neutral = {0};

        if (!reset_sim(sim) ||
            !pickup_item(sim, &result, &inspection) ||
            !step_sim(
                sim,
                grounded_drop,
                neutral,
                &result,
                &inspection) ||
            find_event(&result, PF_SIM_EVENT_ITEM_DROP) == NULL ||
            inspection.item.state !=
                (uint8_t)PF_M4_ITEM_STATE_GROUND ||
            inspection.item.holder != PF_SIM_EVENT_NO_PLAYER ||
            inspection.item.source != PF_SIM_EVENT_NO_PLAYER ||
            inspection.item.hit_mask != UINT8_C(0) ||
            inspection.item.throw_direction !=
                (uint8_t)PF_M4_ITEM_THROW_NONE ||
            inspection.item.velocity_x_q16 != INT32_C(0) ||
            inspection.item.velocity_y_q16 != INT32_C(0) ||
            !expect_status(
                pf_sim_hash(sim, &grounded_drop_hash),
                PF_STATUS_OK,
                "grounded-drop-hash"))
        {
            return fail("grounded-drop");
        }
    }
    for (direction_index = UINT32_C(0);
         direction_index < UINT32_C(4);
         ++direction_index)
    {
        const test_command neutral = {0};
        const pf_sim_event *event;

        if (!reset_sim(sim) ||
            !pickup_item(sim, &result, &inspection) ||
            !step_sim(
                sim,
                throws[direction_index],
                neutral,
                &result,
                &inspection))
        {
            return 0;
        }
        event = find_event(&result, PF_SIM_EVENT_ITEM_THROW);
        if (event == NULL ||
            event->detail !=
                (uint16_t)expected_directions[direction_index] ||
            event->source_player != UINT8_C(0) ||
            inspection.item.source != UINT8_C(0) ||
            inspection.item.holder != PF_SIM_EVENT_NO_PLAYER ||
             inspection.players[0].action_state !=
                 (uint8_t)PF_M4_ACTION_ITEM_THROW)
        {
            (void)fprintf(
                stderr,
                "m4-item=debug direction_index=%" PRIu32
                " event=%u detail=%u source=%u item_source=%u "
                "holder=%u action=%u state=%u velocity_x=%" PRId32
                " velocity_y=%" PRId32 "\n",
                direction_index,
                event != NULL ? (unsigned int)event->type : 0U,
                event != NULL ? (unsigned int)event->detail : 0U,
                event != NULL ? (unsigned int)event->source_player : 0U,
                (unsigned int)inspection.item.source,
                (unsigned int)inspection.item.holder,
                (unsigned int)inspection.players[0].action_state,
                (unsigned int)inspection.item.state,
                inspection.item.velocity_x_q16,
                inspection.item.velocity_y_q16);
            return fail("directional-throw");
        }
    }
    return 1;
}

static int advance_roll_to_tick(
    pf_sim *sim,
    uint16_t target_tick,
    pf_tick_result *result,
    pf_m4_inspection *inspection)
{
    const test_command roll = {
        INT16_MAX,
        INT16_C(0),
        UINT64_C(0),
        UINT16_MAX};
    const test_command neutral = {0};
    uint32_t guard;

    if (!step_sim(sim, roll, neutral, result, inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(40); ++guard)
    {
        if (inspection->players[0].action_state ==
                (uint8_t)PF_M4_ACTION_ROLL_FORWARD &&
            inspection->players[0].action_ticks == target_tick)
        {
            return 1;
        }
        if (!neutral_step(sim, result, inspection))
        {
            return 0;
        }
    }
    return fail("roll-target-tick");
}

static int run_glide_toss_contract(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command attack = {
        INT16_MAX,
        INT16_C(0),
        PF_INPUT_BUTTON_ATTACK,
        UINT16_C(0)};
    const test_command neutral = {0};
    int32_t glide_velocity;

    if (!initialize_sim(&storage, view, &sim) ||
        !reset_sim(sim) ||
        !pickup_item(sim, &result, &inspection) ||
        !advance_roll_to_tick(
            sim,
            content->item.glide_toss_end_tick,
            &result,
            &inspection) ||
        !step_sim(sim, attack, neutral, &result, &inspection))
    {
        return 0;
    }
    glide_velocity = inspection.players[0].velocity_x_q16;
    if (find_event(&result, PF_SIM_EVENT_ITEM_THROW) == NULL ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ITEM_THROW ||
        glide_velocity <= INT32_C(0) ||
        inspection.item.velocity_x_q16 <=
            content->item.forward_throw.velocity_x_q16)
    {
        return fail("glide-toss-positive");
    }

    if (!reset_sim(sim) ||
        !pickup_item(sim, &result, &inspection) ||
        !advance_roll_to_tick(
            sim,
            (uint16_t)(content->item.glide_toss_end_tick +
                       UINT16_C(1)),
            &result,
            &inspection) ||
        !step_sim(sim, attack, neutral, &result, &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ROLL_FORWARD ||
        inspection.item.state != (uint8_t)PF_M4_ITEM_STATE_HELD ||
        find_event(&result, PF_SIM_EVENT_ITEM_THROW) != NULL)
    {
        return fail("glide-toss-late-negative");
    }
    return 1;
}

static int start_jump_from_dash(
    pf_sim *sim,
    pf_tick_result *result,
    pf_m4_inspection *inspection)
{
    const test_command dash = {INT16_MAX, 0, 0, 0};
    const test_command jump = {
        INT16_MAX,
        0,
        PF_INPUT_BUTTON_JUMP,
        0};
    const test_command neutral = {0};

    return step_sim(sim, dash, neutral, result, inspection) &&
           step_sim(sim, jump, neutral, result, inspection) &&
           inspection->players[0].action_state ==
               (uint8_t)PF_M4_ACTION_JUMP_SQUAT;
}

static int run_jump_cancel_throw_contract(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command attack = {
        INT16_MAX,
        INT16_C(0),
        PF_INPUT_BUTTON_ATTACK,
        UINT16_C(0)};
    const test_command neutral = {0};
    int32_t jump_cancel_velocity;
    uint32_t guard;

    if (!initialize_sim(&storage, view, &sim) ||
        !reset_sim(sim) ||
        !pickup_item(sim, &result, &inspection) ||
        !start_jump_from_dash(sim, &result, &inspection) ||
        !step_sim(sim, attack, neutral, &result, &inspection))
    {
        return 0;
    }
    jump_cancel_velocity = inspection.players[0].velocity_x_q16;
    if (find_event(&result, PF_SIM_EVENT_ITEM_THROW) == NULL ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_ITEM_THROW ||
        inspection.players[0].grounded != UINT8_C(1) ||
        jump_cancel_velocity <= content->item.dash_throw_speed_q16 ||
        inspection.item.velocity_x_q16 <=
            content->item.forward_throw.velocity_x_q16)
    {
        return fail("jump-cancel-throw-positive");
    }

    if (!reset_sim(sim) ||
        !pickup_item(sim, &result, &inspection) ||
        !start_jump_from_dash(sim, &result, &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(8); ++guard)
    {
        if (inspection.players[0].grounded == UINT8_C(0))
        {
            break;
        }
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        !step_sim(sim, attack, neutral, &result, &inspection) ||
        inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_ITEM_THROW ||
        find_event(&result, PF_SIM_EVENT_ITEM_THROW) == NULL)
    {
        return fail("jump-cancel-throw-late-negative");
    }
    return 1;
}

static int run_aerial_drop_contract(
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command jump = {0, 0, PF_INPUT_BUTTON_JUMP, 0};
    const test_command drift = {INT16_MAX, 0, 0, 0};
    const test_command drop = {
        0,
        0,
        PF_INPUT_BUTTON_ATTACK,
        UINT16_MAX};
    const test_command neutral = {0};
    uint32_t guard;
    int hit_seen = 0;

    if (!initialize_sim(&storage, view, &sim) ||
        !reset_sim(sim) ||
        !pickup_item(sim, &result, &inspection) ||
        !step_sim(sim, jump, neutral, &result, &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(80); ++guard)
    {
        const int64_t delta =
            (int64_t)inspection.item.position_x_q16 -
            (int64_t)inspection.players[1].position_x_q16;

        if (inspection.players[0].grounded == UINT8_C(0) &&
            delta >= -(int64_t)(PF_Q16_ONE / INT32_C(2)) &&
            delta <= (int64_t)(PF_Q16_ONE / INT32_C(2)))
        {
            break;
        }
        if (!step_sim(sim, drift, neutral, &result, &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        !step_sim(sim, drop, neutral, &result, &inspection) ||
        find_event(&result, PF_SIM_EVENT_ITEM_DROP) == NULL ||
        inspection.item.state !=
            (uint8_t)PF_M4_ITEM_STATE_AIRBORNE)
    {
        return fail("aerial-drop-entry");
    }
    for (guard = UINT32_C(0); guard < UINT32_C(180); ++guard)
    {
        const pf_sim_event *hit =
            find_event(&result, PF_SIM_EVENT_ITEM_HIT);

        if (hit != NULL)
        {
            hit_seen = hit->source_player == UINT8_C(0) &&
                       hit->target_player == UINT8_C(1) &&
                       hit->value_q16 == UINT32_C(7) * UINT32_C(65536);
            break;
        }
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
    }
    if (hit_seen == 0 ||
        inspection.players[1].damage_q16 !=
            UINT32_C(7) * UINT32_C(65536) ||
        inspection.item.velocity_y_q16 >= INT32_C(0) ||
        inspection.item.hit_mask != UINT8_C(2) ||
        inspection.item.stale_registered != UINT8_C(1) ||
        inspection.players[0].stale_move_count != UINT8_C(1) ||
        inspection.players[0].stale_move_ids[0] !=
            (uint8_t)PF_M4_ACTION_ITEM_THROW)
    {
        (void)fprintf(
            stderr,
            "m4-item=debug drop-hit=%d damage=%" PRIu32
            " item_x=%" PRId32 " item_y=%" PRId32
            " target_x=%" PRId32 " target_y=%" PRId32 "\n",
            hit_seen,
            inspection.players[1].damage_q16,
            inspection.item.position_x_q16,
            inspection.item.position_y_q16,
            inspection.players[1].position_x_q16,
            inspection.players[1].position_y_q16);
        return fail("aerial-drop-hit");
    }

    if (!reset_sim(sim) ||
        !pickup_item(sim, &result, &inspection) ||
        !step_sim(sim, jump, neutral, &result, &inspection))
    {
        return 0;
    }
    while (inspection.players[0].grounded != UINT8_C(0))
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
    }
    if (!step_sim(sim, drop, neutral, &result, &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(180) &&
                              inspection.item.state ==
                                  (uint8_t)PF_M4_ITEM_STATE_AIRBORNE;
         ++guard)
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[1].damage_q16 != UINT32_C(0))
    {
        return fail("aerial-drop-spacing-negative");
    }
    return 1;
}

static int run_save_replay_rl_contract(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_storage source_storage;
    test_storage loaded_storage;
    test_storage initial_storage;
    test_storage verifier_storage;
    test_storage rl_storage;
    pf_sim *source = NULL;
    pf_sim *loaded = NULL;
    pf_sim *initial = NULL;
    pf_sim *verifier = NULL;
    pf_sim *rl_sim = NULL;
    pf_tick_result source_result;
    pf_tick_result loaded_result;
    pf_m4_inspection inspection;
    pf_state_hash source_hash;
    pf_state_hash loaded_hash;
    uint8_t save_bytes[1024];
    size_t save_size;
    pf_mut_bytes destination;
    pf_bytes save;
    uint32_t tick;
    pf_input_frame replay_inputs[
        TEST_REPLAY_TICKS * UINT32_C(2)];
    pf_state_hash replay_hashes[TEST_REPLAY_TICKS + UINT32_C(1)];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    uint8_t replay_bytes[16384];
    size_t replay_size;
    pf_mut_bytes replay_destination;
    pf_bytes replay;
    pf_rl_transition transition;

    if (!initialize_sim(&source_storage, view, &source) ||
        !initialize_sim(&loaded_storage, view, &loaded) ||
        !reset_sim(source) ||
        !reset_sim(loaded) ||
        !pickup_item(source, &source_result, &inspection) ||
        !start_jump_from_dash(source, &source_result, &inspection))
    {
        return 0;
    }
    {
        const test_command attack = {
            INT16_MAX, 0, PF_INPUT_BUTTON_ATTACK, 0};
        const test_command neutral = {0};

        if (!step_sim(
                source,
                attack,
                neutral,
                &source_result,
                &inspection) ||
            !expect_status(
                pf_sim_query_save_size(source, &save_size),
                PF_STATUS_OK,
                "item-save-size") ||
            save_size != (size_t)827)
        {
            return fail("item-save-setup");
        }
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "item-save") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "item-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(40); ++tick)
    {
        if (!neutral_step(source, &source_result, &inspection) ||
            !neutral_step(loaded, &loaded_result, &inspection) ||
            !event_equal(&source_result, &loaded_result) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "source-future-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "loaded-future-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("item-save-future");
        }
    }

    if (!initialize_sim(&initial_storage, view, &initial) ||
        !initialize_sim(&verifier_storage, view, &verifier) ||
        !reset_sim(source) ||
        !reset_sim(initial) ||
        !reset_sim(verifier) ||
        !expect_status(
            pf_sim_clone(initial, source),
            PF_STATUS_OK,
            "item-clone-initial") ||
        !expect_status(
            pf_sim_hash(initial, &replay_hashes[0]),
            PF_STATUS_OK,
            "item-replay-initial-hash"))
    {
        return 0;
    }
    (void)memset(replay_inputs, 0, sizeof(replay_inputs));
    for (tick = UINT32_C(0); tick < TEST_REPLAY_TICKS; ++tick)
    {
        const uint64_t buttons =
            tick == UINT32_C(0)
                ? PF_INPUT_BUTTON_ATTACK
                : tick == UINT32_C(2)
                ? PF_INPUT_BUTTON_JUMP
                : tick == UINT32_C(3)
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0);
        const uint16_t trigger =
            tick == UINT32_C(0) ? UINT16_MAX : UINT16_C(0);
        const int16_t x =
            tick == UINT32_C(1) || tick == UINT32_C(2) ||
                    tick == UINT32_C(3)
                ? INT16_MAX
                : INT16_C(0);
        uint32_t player_index;

        for (player_index = UINT32_C(0);
             player_index < UINT32_C(2);
             ++player_index)
        {
            pf_input_frame *frame =
                &replay_inputs[tick * UINT32_C(2) + player_index];
            frame->tick = (uint64_t)tick;
            frame->schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            frame->player_slot = (uint8_t)player_index;
        }
        replay_inputs[tick * UINT32_C(2)].buttons = buttons;
        replay_inputs[tick * UINT32_C(2)].left_trigger = trigger;
        replay_inputs[tick * UINT32_C(2)].main_stick_x = x;
        if (!expect_status(
                pf_sim_tick(
                    source,
                    &replay_inputs[tick * UINT32_C(2)],
                    (size_t)2,
                    &source_result),
                PF_STATUS_OK,
                "item-replay-tick") ||
            !expect_status(
                pf_sim_hash(source, &replay_hashes[tick + UINT32_C(1)]),
                PF_STATUS_OK,
                "item-replay-hash"))
        {
            return 0;
        }
    }
    (void)memset(&replay_source, 0, sizeof(replay_source));
    replay_source.struct_size = (uint32_t)sizeof(replay_source);
    replay_source.schema_version = PF_REPLAY_SCHEMA_VERSION;
    replay_source.flags = PF_REPLAY_FLAG_PER_TICK_HASHES;
    replay_source.initial_state = initial;
    replay_source.input_frames = replay_inputs;
    replay_source.input_frame_count =
        (size_t)TEST_REPLAY_TICKS * (size_t)2;
    replay_source.state_hashes = replay_hashes;
    replay_source.state_hash_count =
        (size_t)TEST_REPLAY_TICKS + (size_t)1;
    replay_source.tick_count = (uint64_t)TEST_REPLAY_TICKS;
    replay_source.final_result = source_result;
    if (!expect_status(
            pf_replay_query_size(&replay_source, &replay_size),
            PF_STATUS_OK,
            "item-replay-size") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("item-replay-capacity");
    }
    replay_destination.bytes = replay_bytes;
    replay_destination.capacity = sizeof(replay_bytes);
    replay_destination.size = (size_t)0;
    replay.bytes = replay_bytes;
    replay.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &replay_destination),
            PF_STATUS_OK,
            "item-replay-encode") ||
        !expect_status(
            pf_replay_verify(verifier, replay, &verification),
            PF_STATUS_OK,
            "item-replay-verify") ||
        verification.status != (uint32_t)PF_STATUS_OK ||
        verification.verified_ticks != (uint64_t)TEST_REPLAY_TICKS)
    {
        return fail("item-replay-verification");
    }

    if (!initialize_sim(&rl_storage, view, &rl_sim) ||
        !expect_status(
            pf_rl_reset(
                rl_sim,
                UINT64_C(0x524c4954454d),
                &transition),
            PF_STATUS_OK,
            "item-rl-reset") ||
        transition.structured_observation.item.state !=
            (uint8_t)PF_M4_ITEM_STATE_GROUND ||
        transition.structured_observation.item.position_x_q16 !=
            content->item.spawn_x_q16 ||
        transition.compact_observation.values[
            PF_RL_COMPACT_ITEM_BASE] != content->item.spawn_x_q16 ||
        (uint32_t)transition.compact_observation.values[
            PF_RL_COMPACT_ITEM_BASE +
            PF_RL_COMPACT_ITEM_STATE_BITS_OFFSET] !=
            (uint32_t)PF_M4_ITEM_STATE_GROUND)
    {
        return fail("item-rl-observation");
    }
    return 1;
}

static int run_reset_contract(
    const pf_m4_content *base_content)
{
    pf_m4_content content = *base_content;
    pf_content_view view;
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    uint32_t tick;
    int wait_seen = 0;
    int reset_seen = 0;

    content.item.lifetime_ticks = UINT16_C(4);
    content.item.respawn_ticks = UINT16_C(3);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "reset-content-view") ||
        !initialize_sim(&storage, &view, &sim) ||
        !reset_sim(sim))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
        if (inspection.item.state ==
            (uint8_t)PF_M4_ITEM_STATE_RESPAWN_WAIT)
        {
            wait_seen = 1;
        }
        if (find_event(&result, PF_SIM_EVENT_ITEM_RESET) != NULL)
        {
            reset_seen = 1;
            break;
        }
    }
    if (wait_seen == 0 || reset_seen == 0 ||
        inspection.item.state != (uint8_t)PF_M4_ITEM_STATE_GROUND ||
        inspection.item.position_x_q16 != content.item.spawn_x_q16 ||
        inspection.item.lifetime_ticks != content.item.lifetime_ticks)
    {
        return fail("item-despawn-reset");
    }
    return 1;
}

int main(void)
{
    pf_m4_content content;
    pf_content_view view;

    if (!run_content_contract())
    {
        return fail("content-suite");
    }
    if (!make_item_content(&content, &view))
    {
        return fail("fixture-suite");
    }
    if (!run_directional_throw_contract(&content, &view))
    {
        return fail("directional-throw-suite");
    }
    if (!run_glide_toss_contract(&content, &view))
    {
        return fail("glide-toss-suite");
    }
    if (!run_jump_cancel_throw_contract(&content, &view))
    {
        return fail("jump-cancel-throw-suite");
    }
    if (!run_aerial_drop_contract(&view))
    {
        return fail("aerial-drop-suite");
    }
    if (!run_save_replay_rl_contract(&content, &view))
    {
        return fail("save-replay-rl-suite");
    }
    if (!run_reset_contract(&content))
    {
        return fail("reset-suite");
    }

    (void)printf(
        "m4-item=pass content_schema=%u state_schema=%u save_bytes=827 "
        "item_invariants=44 bat_drop=1 glide_toss=1 "
        "jump_cancel_throw=1 directional_throws=4 replay=1 rl=1\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        (unsigned int)PF_SIM_STATE_SCHEMA_VERSION);
    return 0;
}
