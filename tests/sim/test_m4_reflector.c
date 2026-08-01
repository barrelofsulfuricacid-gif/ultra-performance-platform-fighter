#include "pf/m4.h"
#include "pf/replay.h"
#include "pf/rl.h"
#include "pf/sim.h"

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
    (void)fprintf(stderr, "m4-reflector=fail operation=%s\n", operation);
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
            "m4-reflector=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
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

static int result_equal(
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

static int make_reflector_content(
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
    content->reflector.enabled = UINT8_C(1);
    content->projectile.enabled = UINT8_C(1);
    content->stage.floor_left_q16 = -INT32_C(8) * PF_Q16_ONE;
    content->stage.floor_right_q16 = INT32_C(8) * PF_Q16_ONE;
    content->stage.platform_center_x_q16 = -INT32_C(4) * PF_Q16_ONE;
    content->stage.platform_half_width_q16 = INT32_C(1) * PF_Q16_ONE;
    content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    content->stage.solid_left_q16 = INT32_C(2) * PF_Q16_ONE;
    content->stage.solid_right_q16 = INT32_C(6) * PF_Q16_ONE;
    content->stage.blast_left_q16 = -INT32_C(12) * PF_Q16_ONE;
    content->stage.blast_right_q16 = INT32_C(12) * PF_Q16_ONE;
    content->stage.blast_bottom_q16 = INT32_C(40) * PF_Q16_ONE;
    content->stage.spawn_spacing_q16 = INT32_C(1) * PF_Q16_ONE;
    return expect_status(
        pf_m4_make_content_view(content, view),
        PF_STATUS_OK,
        "reflector-content-view");
}

static int initialize_sim(
    test_storage *storage,
    const pf_content_view *view,
    pf_sim **out_sim)
{
    pf_sim_config config;

    if (!expect_status(
            pf_sim_default_config(&config, UINT8_C(2), PF_SIM_MODE_DUEL),
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
        pf_sim_reset(sim, UINT64_C(0x505249534d425552)),
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
    pf_input_frame inputs[2];
    uint32_t player_index;

    if (!expect_status(
            pf_m4_inspect(sim, &before),
            PF_STATUS_OK,
            "inspect-before-step"))
    {
        return 0;
    }
    (void)memset(inputs, 0, sizeof(inputs));
    for (player_index = UINT32_C(0); player_index < UINT32_C(2);
         ++player_index)
    {
        inputs[player_index].tick = before.tick;
        inputs[player_index].schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
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
    return expect_status(
               pf_sim_tick(sim, inputs, (size_t)2, out_result),
               PF_STATUS_OK,
               "sim-step") &&
           expect_status(
               pf_m4_inspect(sim, out_inspection),
               PF_STATUS_OK,
               "inspect-after-step");
}

static int run_content_contract(void)
{
    pf_m4_content disabled;
    pf_m4_content enabled;
    pf_m4_content invalid;
    pf_content_view disabled_view;
    pf_content_view enabled_view;

    if (!expect_status(
            pf_m4_default_content(&disabled),
            PF_STATUS_OK,
            "reflector-default") ||
        disabled.reflector_count != PF_M4_TEST_REFLECTOR_COUNT ||
        disabled.reflector.enabled != UINT8_C(0) ||
        disabled.reflector.struct_size !=
            (uint32_t)sizeof(disabled.reflector) ||
        disabled.reflector.schema_version !=
            PF_M4_REFLECTOR_SCHEMA_VERSION ||
        !expect_status(
            pf_m4_make_content_view(&disabled, &disabled_view),
            PF_STATUS_OK,
            "reflector-disabled-view") ||
        !make_reflector_content(&enabled, &enabled_view) ||
        memcmp(
            disabled_view.content_hash.bytes,
            enabled_view.content_hash.bytes,
            sizeof(disabled_view.content_hash.bytes)) == 0)
    {
        return fail("content-contract");
    }
    invalid = enabled;
    invalid.reflector.enabled = UINT8_C(2);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "reflector-invalid-enabled"))
    {
        return 0;
    }
    invalid = enabled;
    invalid.reflector.active_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "reflector-invalid-active"))
    {
        return 0;
    }
    invalid = enabled;
    invalid.reflector.base_knockback_y_q16 = -INT32_C(1);
    return expect_status(
        pf_m4_validate_content(&invalid),
        PF_STATUS_INVALID_CONFIG,
        "reflector-invalid-knockback");
}

static int run_hit_and_reflection_contract(
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command down_special = {
        INT16_C(0), INT16_MAX, PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    const test_command special = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    const test_command right = {
        INT16_MAX, INT16_C(0), UINT64_C(0), UINT16_C(0)};
    const pf_sim_event *event;
    uint32_t guard;

    if (!initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !step_sim(sim, right, (test_command){0}, &result, &inspection) ||
        !step_sim(sim, down_special, (test_command){0}, &result, &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_REFLECTOR_GROUND ||
        inspection.players[0].hitbox_active != UINT8_C(0) ||
        !step_sim(
            sim,
            (test_command){0},
            (test_command){0},
            &result,
            &inspection))
    {
        return fail("ground-reflector-startup");
    }
    event = find_event(&result, PF_SIM_EVENT_HIT);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        event->target_player != UINT8_C(1) ||
        event->detail != (uint16_t)PF_M4_ACTION_REFLECTOR_GROUND ||
        event->velocity_y_q16 <= INT32_C(0) ||
        inspection.players[1].damage_q16 == UINT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-reflector=debug ground event=%d source=%u target=%u "
            "detail=%u vy=%d damage=%u p0_action=%u p0_ticks=%u "
            "hitbox=%u p0x=%d p1x=%d box=[%d,%d]\n",
            event != NULL,
            event != NULL ? (unsigned int)event->source_player : 255U,
            event != NULL ? (unsigned int)event->target_player : 255U,
            event != NULL ? (unsigned int)event->detail : 65535U,
            event != NULL ? event->velocity_y_q16 : 0,
            inspection.players[1].damage_q16,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active,
            inspection.players[0].position_x_q16,
            inspection.players[1].position_x_q16,
            inspection.players[0].hitbox_left_q16,
            inspection.players[0].hitbox_right_q16);
        return fail("ground-reflector-downward-hit");
    }

    if (!reset_sim(sim) ||
        !step_sim(sim, down_special, special, &result, &inspection) ||
        !step_sim(
            sim,
            (test_command){0},
            (test_command){0},
            &result,
            &inspection))
    {
        return fail("projectile-reflect-setup");
    }
    event = find_event(&result, PF_SIM_EVENT_PROJECTILE_REFLECT);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        event->target_player != UINT8_C(1) ||
        event->detail != (uint16_t)PF_M4_ACTION_REFLECTOR_GROUND ||
        inspection.projectile.owner != UINT8_C(0) ||
        inspection.projectile.velocity_x_q16 <= INT32_C(0) ||
        inspection.players[0].powershield != UINT8_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-reflector=debug reflect event=%d source=%u target=%u "
            "detail=%u state=%u owner=%u vx=%d p0_action=%u ticks=%u "
            "hitbox=%u\n",
            event != NULL,
            event != NULL ? (unsigned int)event->source_player : 255U,
            event != NULL ? (unsigned int)event->target_player : 255U,
            event != NULL ? (unsigned int)event->detail : 65535U,
            (unsigned int)inspection.projectile.state,
            (unsigned int)inspection.projectile.owner,
            inspection.projectile.velocity_x_q16,
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[0].action_ticks,
            (unsigned int)inspection.players[0].hitbox_active);
        return fail("reflector-projectile-reflection");
    }
    event = NULL;
    for (guard = UINT32_C(0); guard < UINT32_C(24); ++guard)
    {
        if (!step_sim(
                sim,
                (test_command){0},
                (test_command){0},
                &result,
                &inspection))
        {
            return 0;
        }
        event = find_event(&result, PF_SIM_EVENT_PROJECTILE_HIT);
        if (event != NULL)
        {
            break;
        }
    }
    return event != NULL && event->source_player == UINT8_C(0) &&
                   event->target_player == UINT8_C(1)
               ? 1
               : fail("reflected-projectile-return-hit");
}

static int run_shine_route(
    pf_sim *sim,
    int attack_enabled,
    int *out_reflector_hit,
    int *out_recovered)
{
    pf_tick_result result;
    pf_m4_inspection inspection;
    uint32_t target_offstage_ticks = UINT32_C(0);
    int target_left_stage = 0;
    int reflector_used = 0;
    int ko_seen = 0;
    uint32_t tick;

    *out_reflector_hit = 0;
    *out_recovered = 0;
    if (!reset_sim(sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "shine-initial-inspect"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(260); ++tick)
    {
        test_command player0 = {INT16_MAX, INT16_C(0), UINT64_C(0), UINT16_C(0)};
        test_command player1 = {INT16_MAX, INT16_C(0), UINT64_C(0), UINT16_C(0)};
        const pf_sim_event *hit;

        if (inspection.players[1].respawn_count != UINT16_C(0))
        {
            return attack_enabled != 0 && *out_reflector_hit != 0 &&
                   ko_seen != 0;
        }
        if (target_left_stage != 0 &&
            inspection.players[1].grounded != UINT8_C(0))
        {
            *out_recovered = 1;
            return attack_enabled == 0;
        }
        if (inspection.players[1].grounded == UINT8_C(0))
        {
            target_left_stage = 1;
            ++target_offstage_ticks;
            player1.x = INT16_MIN;
            if (target_offstage_ticks == UINT32_C(9))
            {
                player1.buttons = PF_INPUT_BUTTON_JUMP;
            }
        }
        if (attack_enabled == 0)
        {
            player0.x = INT16_C(0);
        }
        else if (reflector_used == 0 &&
                 inspection.players[0].grounded == UINT8_C(0) &&
                 target_left_stage != 0)
        {
            player0.y = INT16_MAX;
            player0.buttons = PF_INPUT_BUTTON_SPECIAL;
            reflector_used = 1;
        }
        if (!step_sim(sim, player0, player1, &result, &inspection))
        {
            return 0;
        }
        hit = find_event(&result, PF_SIM_EVENT_HIT);
        if (hit != NULL &&
            hit->detail == (uint16_t)PF_M4_ACTION_REFLECTOR_AIR &&
            hit->source_player == UINT8_C(0) &&
            hit->target_player == UINT8_C(1) &&
            hit->velocity_y_q16 > INT32_C(0))
        {
            *out_reflector_hit = 1;
        }
        {
            const pf_sim_event *ko =
                find_event(&result, PF_SIM_EVENT_KO);

            if (ko != NULL && ko->source_player == UINT8_C(0) &&
                ko->target_player == UINT8_C(1))
            {
                ko_seen = 1;
            }
        }
    }
    return fail(attack_enabled != 0 ? "shine-route-timeout" :
                                      "recovery-route-timeout");
}

static int run_shine_spike_contract(const pf_content_view *view)
{
    test_storage attack_storage;
    test_storage control_storage;
    pf_sim *attack_sim = NULL;
    pf_sim *control_sim = NULL;
    int reflector_hit;
    int recovered;
    int control_hit;
    int control_recovered;

    if (!initialize_sim(&attack_storage, view, &attack_sim) ||
        !initialize_sim(&control_storage, view, &control_sim) ||
        !run_shine_route(
            attack_sim,
            1,
            &reflector_hit,
            &recovered) ||
        !run_shine_route(
            control_sim,
            0,
            &control_hit,
            &control_recovered) ||
        reflector_hit == 0 || recovered != 0 || control_hit != 0 ||
        control_recovered == 0)
    {
        return fail("shine-spike-positive-negative");
    }
    return 1;
}

static int run_state_interfaces_contract(
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
    size_t save_size = (size_t)0;
    pf_mut_bytes save_destination;
    pf_bytes save;
    pf_input_frame replay_inputs[TEST_REPLAY_TICKS * UINT32_C(2)];
    pf_state_hash replay_hashes[TEST_REPLAY_TICKS + UINT32_C(1)];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    uint8_t replay_bytes[16384];
    size_t replay_size = (size_t)0;
    pf_mut_bytes replay_destination;
    pf_bytes replay;
    pf_rl_action actions[2];
    pf_rl_transition transition;
    pf_m4_inspection rl_inspection;
    const test_command down_special = {
        INT16_C(0), INT16_MAX, PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    uint32_t tick;

    if (!initialize_sim(&source_storage, view, &source) ||
        !initialize_sim(&loaded_storage, view, &loaded) ||
        !reset_sim(source) || !reset_sim(loaded) ||
        !step_sim(
            source,
            down_special,
            (test_command){0},
            &source_result,
            &inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "reflector-save-size") ||
        save_size != (size_t)726)
    {
        return fail("reflector-save-setup");
    }
    save_destination.bytes = save_bytes;
    save_destination.capacity = sizeof(save_bytes);
    save_destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &save_destination),
            PF_STATUS_OK,
            "reflector-save") ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "reflector-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!step_sim(
                source,
                (test_command){0},
                (test_command){0},
                &source_result,
                &inspection) ||
            !step_sim(
                loaded,
                (test_command){0},
                (test_command){0},
                &loaded_result,
                &inspection) ||
            !result_equal(&source_result, &loaded_result) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "reflector-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "reflector-loaded-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("reflector-save-future");
        }
    }

    if (!initialize_sim(&initial_storage, view, &initial) ||
        !initialize_sim(&verifier_storage, view, &verifier) ||
        !reset_sim(source) || !reset_sim(initial) || !reset_sim(verifier) ||
        !expect_status(
            pf_sim_clone(initial, source),
            PF_STATUS_OK,
            "reflector-clone") ||
        !expect_status(
            pf_sim_hash(initial, &replay_hashes[0]),
            PF_STATUS_OK,
            "reflector-replay-initial-hash"))
    {
        return 0;
    }
    (void)memset(replay_inputs, 0, sizeof(replay_inputs));
    for (tick = UINT32_C(0); tick < TEST_REPLAY_TICKS; ++tick)
    {
        uint32_t player_index;

        for (player_index = UINT32_C(0); player_index < UINT32_C(2);
             ++player_index)
        {
            pf_input_frame *frame =
                &replay_inputs[tick * UINT32_C(2) + player_index];
            frame->tick = (uint64_t)tick;
            frame->schema_version = PF_SIM_INPUT_SCHEMA_VERSION;
            frame->player_slot = (uint8_t)player_index;
        }
        if (tick == UINT32_C(0))
        {
            replay_inputs[0].main_stick_y = INT16_MAX;
            replay_inputs[0].buttons = PF_INPUT_BUTTON_SPECIAL;
        }
        if (!expect_status(
                pf_sim_tick(
                    source,
                    &replay_inputs[tick * UINT32_C(2)],
                    (size_t)2,
                    &source_result),
                PF_STATUS_OK,
                "reflector-replay-tick") ||
            !expect_status(
                pf_sim_hash(source, &replay_hashes[tick + UINT32_C(1)]),
                PF_STATUS_OK,
                "reflector-replay-hash"))
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
            "reflector-replay-size") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("reflector-replay-capacity");
    }
    replay_destination.bytes = replay_bytes;
    replay_destination.capacity = sizeof(replay_bytes);
    replay_destination.size = (size_t)0;
    replay.bytes = replay_bytes;
    replay.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &replay_destination),
            PF_STATUS_OK,
            "reflector-replay-encode") ||
        !expect_status(
            pf_replay_verify(verifier, replay, &verification),
            PF_STATUS_OK,
            "reflector-replay-verify") ||
        verification.verified_ticks != (uint64_t)TEST_REPLAY_TICKS)
    {
        return fail("reflector-replay-result");
    }

    if (!initialize_sim(&rl_storage, view, &rl_sim) ||
        !expect_status(
            pf_rl_reset(
                rl_sim,
                UINT64_C(0x524c5245464c4543),
                &transition),
            PF_STATUS_OK,
            "reflector-rl-reset"))
    {
        return 0;
    }
    (void)memset(actions, 0, sizeof(actions));
    actions[0].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[1].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[0].main_stick_x = INT16_MAX;
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "reflector-rl-approach"))
    {
        return 0;
    }
    actions[0].main_stick_x = INT16_C(0);
    actions[0].main_stick_y = INT16_MAX;
    actions[0].buttons = PF_INPUT_BUTTON_SPECIAL;
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "reflector-rl-step") ||
        !expect_status(
            pf_m4_inspect(rl_sim, &rl_inspection),
            PF_STATUS_OK,
            "reflector-rl-inspect") ||
        transition.structured_observation.players[0].previous_buttons !=
            PF_INPUT_BUTTON_SPECIAL ||
        (transition.legal_buttons[0] & PF_INPUT_BUTTON_SPECIAL) ==
            UINT64_C(0))
    {
        return fail("reflector-rl-observation");
    }
    actions[0].main_stick_y = INT16_C(0);
    actions[0].buttons = UINT64_C(0);
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "reflector-rl-hit-step") ||
        !expect_status(
            pf_m4_inspect(rl_sim, &rl_inspection),
            PF_STATUS_OK,
            "reflector-rl-hit-inspect") ||
        transition.structured_observation.players[1].active != UINT8_C(1) ||
        rl_inspection.players[1].damage_q16 == UINT32_C(0) ||
        rl_inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        return fail("reflector-rl-hit-observation");
    }
    return 1;
}

int main(void)
{
    pf_m4_content content;
    pf_content_view view;

    if (!run_content_contract() ||
        !make_reflector_content(&content, &view) ||
        !run_hit_and_reflection_contract(&view) ||
        !run_shine_spike_contract(&view) ||
        !run_state_interfaces_contract(&view))
    {
        return 1;
    }
    (void)printf(
        "m4-reflector=pass content_schema=%u state_schema=%u "
        "save_bytes=726 reflector_invariants=32 shine_spike=1 "
        "projectile_reflect=1 replay=1 rl=1\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        (unsigned int)PF_SIM_STATE_SCHEMA_VERSION);
    return 0;
}
