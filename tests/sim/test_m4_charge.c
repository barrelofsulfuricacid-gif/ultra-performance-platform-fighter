#include "pf/m4.h"
#include "pf/replay.h"
#include "pf/rl.h"
#include "pf/sim.h"
#include "sim_sha256.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 8192U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_REPLAY_TICKS UINT32_C(24)
#define TEST_SAVE_HEADER_BYTES ((size_t)140)
#define TEST_SAVE_PAYLOAD_BYTES ((size_t)667)
#define TEST_SAVE_CHECKSUM_OFFSET ((size_t)108)
#define TEST_SAVE_CHARGE0_OFFSET ((size_t)687)

typedef struct test_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_storage;

typedef struct test_command
{
    int16_t y;
    uint64_t buttons;
    uint16_t trigger;
} test_command;

static int fail(const char *operation)
{
    (void)fprintf(stderr, "m4-charge=fail operation=%s\n", operation);
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
            "m4-charge=fail operation=%s expected=%s actual=%s\n",
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

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & UINT16_C(0xff));
    bytes[1] = (uint8_t)(value >> 8U);
}

static void refresh_payload_checksum(uint8_t *save_bytes)
{
    pf_sha256 hash;

    pf_sha256_init(&hash);
    pf_sha256_update(
        &hash,
        &save_bytes[TEST_SAVE_HEADER_BYTES],
        TEST_SAVE_PAYLOAD_BYTES);
    pf_sha256_finish(
        &hash,
        &save_bytes[TEST_SAVE_CHECKSUM_OFFSET]);
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

static int make_charge_content(
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
    content->fighter.reference_frame_data_enabled = UINT8_C(0);
    content->charge.enabled = UINT8_C(1);
    content->charge.hitbox_offset_x_q16 = INT32_C(0);
    content->charge.hitbox_half_width_q16 = INT32_C(4) * PF_Q16_ONE;
    content->fighter.jab_hitbox_offset_x_q16 = INT32_C(0);
    content->fighter.jab_hitbox_half_width_q16 =
        INT32_C(4) * PF_Q16_ONE;
    content->stage.spawn_spacing_q16 = PF_Q16_ONE;
    return expect_status(
        pf_m4_make_content_view(content, view),
        PF_STATUS_OK,
        "charge-content-view");
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
        pf_sim_reset(sim, UINT64_C(0x4152435245534552)),
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
    inputs[0].main_stick_y = player0.y;
    inputs[0].buttons = player0.buttons;
    inputs[0].left_trigger = player0.trigger;
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
    pf_m4_content invalid;
    pf_content_view view;

    if (!expect_status(
            pf_m4_default_content(&disabled),
            PF_STATUS_OK,
            "charge-default") ||
        disabled.charge_count != PF_M4_TEST_CHARGE_COUNT ||
        disabled.charge.schema_version != PF_M4_CHARGE_SCHEMA_VERSION ||
        disabled.charge.enabled != UINT8_C(0) ||
        disabled.charge.max_charge_ticks != UINT16_C(120) ||
        !expect_status(
            pf_m4_make_content_view(&disabled, &view),
            PF_STATUS_OK,
            "charge-disabled-view"))
    {
        return fail("charge-default-contract");
    }
    invalid = disabled;
    invalid.charge.enabled = UINT8_C(2);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "charge-enabled-invalid"))
    {
        return 0;
    }
    invalid = disabled;
    invalid.charge.max_charge_ticks = UINT16_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "charge-duration-invalid"))
    {
        return 0;
    }
    invalid = disabled;
    invalid.charge.schema_version = UINT16_C(0);
    return expect_status(
        pf_m4_validate_content(&invalid),
        PF_STATUS_UNSUPPORTED_VERSION,
        "charge-schema-invalid");
}

static int run_cancel_and_resume_contract(
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command up_special = {
        INT16_MIN,
        PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK,
        UINT16_C(0)};
    const test_command shield = {
        INT16_C(0), UINT64_C(0), UINT16_MAX};
    uint16_t stored_charge;
    uint32_t tick;

    if (!initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !step_sim(sim, up_special, (test_command){0}, &result, &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
        inspection.players[0].charge_ticks != UINT16_C(1))
    {
        return fail("charge-start");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(7); ++tick)
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
    }
    stored_charge = inspection.players[0].charge_ticks;
    if (stored_charge != UINT16_C(8) ||
        !step_sim(sim, shield, (test_command){0}, &result, &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND ||
        inspection.players[0].charge_ticks != stored_charge ||
        !step_sim(
            sim,
            (test_command){INT16_C(0), PF_INPUT_BUTTON_ATTACK, UINT16_C(0)},
            (test_command){0},
            &result,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_ATTACK ||
        inspection.players[0].charge_ticks != stored_charge)
    {
        return fail("charge-early-store-cancel");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(32); ++tick)
    {
        if (inspection.players[0].action_state ==
            (uint8_t)PF_M4_ACTION_GROUND_IDLE)
        {
            break;
        }
        if (!step_sim(
                sim,
                (test_command){0},
                (test_command){0},
                &result,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        !step_sim(sim, up_special, (test_command){0}, &result, &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CHARGE_GROUND ||
        inspection.players[0].charge_ticks !=
            (uint16_t)(stored_charge + UINT16_C(1)))
    {
        return fail("charge-resume");
    }

    if (!reset_sim(sim) ||
        !step_sim(sim, up_special, (test_command){0}, &result, &inspection) ||
        !step_sim(sim, shield, (test_command){0}, &result, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1);
         tick < UINT32_C(4);
         ++tick)
    {
        if (!step_sim(sim, shield, (test_command){0}, &result, &inspection))
        {
            return 0;
        }
    }
    return inspection.players[0].action_state ==
               (uint8_t)PF_M4_ACTION_SHIELD &&
                   inspection.players[0].charge_ticks == UINT16_C(1)
               ? 1
               : fail("charge-held-shield-negative");
}

static int release_damage_after_charge(
    pf_sim *sim,
    uint16_t charge_ticks,
    uint32_t *out_damage_q16)
{
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command up_special = {
        INT16_MIN,
        PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK,
        UINT16_C(0)};
    uint32_t tick;

    if (!reset_sim(sim) ||
        !step_sim(sim, up_special, (test_command){0}, &result, &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(1); tick < (uint32_t)charge_ticks; ++tick)
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
    }
    if (inspection.players[0].charge_ticks != charge_ticks ||
        !step_sim(
            sim,
            (test_command){INT16_C(0), PF_INPUT_BUTTON_ATTACK, UINT16_C(0)},
            (test_command){0},
            &result,
            &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND)
    {
        return fail("charge-release-start");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(12); ++tick)
    {
        const pf_sim_event *hit = find_event(&result, PF_SIM_EVENT_HIT);

        if (hit != NULL)
        {
            if (hit->detail !=
                    (uint16_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND ||
                inspection.players[0].stale_move_count !=
                    UINT8_C(1) ||
                inspection.players[0].stale_move_ids[0] !=
                    (uint8_t)PF_M4_ACTION_CHARGE_RELEASE_GROUND)
            {
                return fail("charge-release-event-detail");
            }
            *out_damage_q16 = hit->value_q16;
            return 1;
        }
        if (!step_sim(
                sim,
                (test_command){0},
                (test_command){0},
                &result,
                &inspection))
        {
            return 0;
        }
    }
    return fail("charge-release-hit");
}

static int run_release_and_interrupt_contract(
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    uint32_t low_damage;
    uint32_t high_damage;
    uint32_t tick;
    const test_command up_special = {
        INT16_MIN,
        PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK,
        UINT16_C(0)};

    if (!initialize_sim(&storage, view, &sim) ||
        !release_damage_after_charge(sim, UINT16_C(1), &low_damage) ||
        !release_damage_after_charge(sim, UINT16_C(120), &high_damage) ||
        low_damage != UINT32_C(4) * UINT32_C(65536) +
                          (UINT32_C(16) * UINT32_C(65536) /
                           UINT32_C(120)) ||
        high_damage != UINT32_C(20) * UINT32_C(65536) ||
        high_damage <= low_damage)
    {
        return fail("charge-release-scaling");
    }

    if (!reset_sim(sim) ||
        !step_sim(
            sim,
            up_special,
            (test_command){INT16_C(0), PF_INPUT_BUTTON_ATTACK, UINT16_C(0)},
            &result,
            &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(8); ++tick)
    {
        if (inspection.players[0].damage_q16 != UINT32_C(0))
        {
            break;
        }
        if (!step_sim(
                sim,
                (test_command){0},
                (test_command){0},
                &result,
                &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].damage_q16 == UINT32_C(0) ||
        inspection.players[0].charge_ticks != UINT16_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_HITLAG)
    {
        (void)fprintf(
            stderr,
            "m4-charge=debug interruption damage=%u charge=%u action=%u\n",
            inspection.players[0].damage_q16,
            (unsigned int)inspection.players[0].charge_ticks,
            (unsigned int)inspection.players[0].action_state);
        return fail("charge-interruption-clears");
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
    const test_command up_special = {
        INT16_MIN,
        PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK,
        UINT16_C(0)};
    const test_command shield = {
        INT16_C(0), UINT64_C(0), UINT16_MAX};
    uint32_t tick;

    if (!initialize_sim(&source_storage, view, &source) ||
        !initialize_sim(&loaded_storage, view, &loaded) ||
        !reset_sim(source) || !reset_sim(loaded) ||
        !step_sim(source, up_special, (test_command){0}, &source_result,
                  &inspection))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(5); ++tick)
    {
        if (!step_sim(source, (test_command){0}, (test_command){0},
                      &source_result, &inspection))
        {
            return 0;
        }
    }
    if (!step_sim(source, shield, (test_command){0}, &source_result,
                  &inspection) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_CHARGE_STORE_GROUND ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "charge-save-size") ||
        save_size != (size_t)807)
    {
        return fail("charge-save-setup");
    }
    save_destination.bytes = save_bytes;
    save_destination.capacity = sizeof(save_bytes);
    save_destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &save_destination),
            PF_STATUS_OK,
            "charge-save") ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "charge-load"))
    {
        return 0;
    }
    if (!expect_status(
            pf_sim_hash(loaded, &loaded_hash),
            PF_STATUS_OK,
            "charge-rejected-load-before-hash"))
    {
        return 0;
    }
    write_u16_le(
        &save_bytes[TEST_SAVE_CHARGE0_OFFSET],
        UINT16_C(121));
    refresh_payload_checksum(save_bytes);
    if (!expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_INVALID_STATE,
            "charge-over-cap-load") ||
        !expect_status(
            pf_sim_hash(loaded, &source_hash),
            PF_STATUS_OK,
            "charge-rejected-load-after-hash") ||
        !hash_equal(&loaded_hash, &source_hash))
    {
        return fail("charge-rejected-load-mutated");
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        const test_command command =
            tick == UINT32_C(1) ? up_special : (test_command){0};

        if (!step_sim(source, command, (test_command){0}, &source_result,
                      &inspection) ||
            !step_sim(loaded, command, (test_command){0}, &loaded_result,
                      &inspection) ||
            !result_equal(&source_result, &loaded_result) ||
            !expect_status(
                pf_sim_hash(source, &source_hash),
                PF_STATUS_OK,
                "charge-source-hash") ||
            !expect_status(
                pf_sim_hash(loaded, &loaded_hash),
                PF_STATUS_OK,
                "charge-loaded-hash") ||
            !hash_equal(&source_hash, &loaded_hash))
        {
            return fail("charge-save-future");
        }
    }

    if (!initialize_sim(&initial_storage, view, &initial) ||
        !initialize_sim(&verifier_storage, view, &verifier) ||
        !reset_sim(source) || !reset_sim(initial) || !reset_sim(verifier) ||
        !expect_status(
            pf_sim_clone(initial, source),
            PF_STATUS_OK,
            "charge-clone") ||
        !expect_status(
            pf_sim_hash(initial, &replay_hashes[0]),
            PF_STATUS_OK,
            "charge-replay-initial-hash"))
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
            replay_inputs[0].main_stick_y = INT16_MIN;
            replay_inputs[0].buttons =
                PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK;
        }
        else if (tick == UINT32_C(6))
        {
            replay_inputs[tick * UINT32_C(2)].left_trigger = UINT16_MAX;
        }
        else if (tick == UINT32_C(7))
        {
            replay_inputs[tick * UINT32_C(2)].buttons =
                PF_INPUT_BUTTON_ATTACK;
        }
        if (!expect_status(
                pf_sim_tick(
                    source,
                    &replay_inputs[tick * UINT32_C(2)],
                    (size_t)2,
                    &source_result),
                PF_STATUS_OK,
                "charge-replay-tick") ||
            !expect_status(
                pf_sim_hash(source, &replay_hashes[tick + UINT32_C(1)]),
                PF_STATUS_OK,
                "charge-replay-hash"))
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
            "charge-replay-size") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("charge-replay-capacity");
    }
    replay_destination.bytes = replay_bytes;
    replay_destination.capacity = sizeof(replay_bytes);
    replay_destination.size = (size_t)0;
    replay.bytes = replay_bytes;
    replay.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &replay_destination),
            PF_STATUS_OK,
            "charge-replay-encode") ||
        !expect_status(
            pf_replay_verify(verifier, replay, &verification),
            PF_STATUS_OK,
            "charge-replay-verify") ||
        verification.verified_ticks != (uint64_t)TEST_REPLAY_TICKS)
    {
        return fail("charge-replay-result");
    }

    if (!initialize_sim(&rl_storage, view, &rl_sim) ||
        !expect_status(
            pf_rl_reset(
                rl_sim,
                UINT64_C(0x524c434841524745),
                &transition),
            PF_STATUS_OK,
            "charge-rl-reset"))
    {
        return 0;
    }
    (void)memset(actions, 0, sizeof(actions));
    actions[0].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[1].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[0].main_stick_y = INT16_MIN;
    actions[0].buttons =
        PF_INPUT_BUTTON_SPECIAL | PF_INPUT_BUTTON_ATTACK;
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "charge-rl-step") ||
        transition.structured_observation.players[0].charge_ticks !=
            UINT16_C(1) ||
        transition.compact_observation.values[PF_RL_COMPACT_CHARGE_BASE] !=
            INT32_C(1) ||
        (transition.legal_buttons[0] & PF_INPUT_BUTTON_SPECIAL) ==
            UINT64_C(0))
    {
        return fail("charge-rl-observation");
    }
    return 1;
}

int main(void)
{
    pf_m4_content content;
    pf_content_view view;

    if (!run_content_contract() ||
        !make_charge_content(&content, &view) ||
        !run_cancel_and_resume_contract(&view) ||
        !run_release_and_interrupt_contract(&view) ||
        !run_state_interfaces_contract(&view))
    {
        return 1;
    }
    (void)printf(
        "m4-charge=pass content_schema=%u state_schema=%u save_bytes=807 "
        "charge_invariants=28 charge_storage_cancel=1 resumed_release=1 "
        "replay=1 rl=1\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        (unsigned int)PF_SIM_STATE_SCHEMA_VERSION);
    return 0;
}
