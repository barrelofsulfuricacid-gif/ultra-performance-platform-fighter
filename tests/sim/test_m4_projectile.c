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
#define TEST_REPLAY_TICKS UINT32_C(20)
#define TEST_CAMPING_MINIMUM_SEPARATION_Q16 INT32_C(693712)

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
    (void)fprintf(stderr, "m4-projectile=fail operation=%s\n", operation);
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
            "m4-projectile=fail operation=%s expected=%s actual=%s\n",
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

static uint32_t count_events(
    const pf_tick_result *result,
    pf_sim_event_type type)
{
    uint32_t count = UINT32_C(0);
    uint32_t event_index;

    for (event_index = UINT32_C(0);
         event_index < (uint32_t)result->event_count;
         ++event_index)
    {
        if (result->events[event_index].type == (uint16_t)type)
        {
            ++count;
        }
    }
    return count;
}

static int make_projectile_content(
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
    content->stage.spawn_spacing_q16 = INT32_C(2) * PF_Q16_ONE;
    content->stage.platform_center_x_q16 =
        -INT32_C(20) * PF_Q16_ONE;
    content->stage.platform_motion_amplitude_q16 = INT32_C(0);
    content->projectile.enabled = UINT8_C(1);
    return expect_status(
        pf_m4_make_content_view(content, view),
        PF_STATUS_OK,
        "projectile-content-view");
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
        pf_sim_reset(sim, UINT64_C(0x50554c5345424f4c)),
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
    if (!expect_status(
            pf_sim_tick(sim, inputs, (size_t)2, out_result),
            PF_STATUS_OK,
            "sim-step") ||
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
            "disabled-default") ||
        disabled.projectile_count != PF_M4_TEST_PROJECTILE_COUNT ||
        disabled.projectile.enabled != UINT8_C(0) ||
        disabled.projectile.struct_size !=
            (uint32_t)sizeof(disabled.projectile) ||
        disabled.projectile.schema_version !=
            PF_M4_PROJECTILE_SCHEMA_VERSION ||
        !expect_status(
            pf_m4_make_content_view(&disabled, &disabled_view),
            PF_STATUS_OK,
            "disabled-view") ||
        !make_projectile_content(&enabled, &enabled_view) ||
        memcmp(
            disabled_view.content_hash.bytes,
            enabled_view.content_hash.bytes,
            sizeof(disabled_view.content_hash.bytes)) == 0)
    {
        return fail("content-contract");
    }

    invalid = enabled;
    invalid.projectile.enabled = UINT8_C(2);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "invalid-enabled"))
    {
        return 0;
    }
    invalid = enabled;
    invalid.projectile.speed_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_validate_content(&invalid),
            PF_STATUS_INVALID_CONFIG,
            "invalid-speed"))
    {
        return 0;
    }
    invalid = enabled;
    invalid.projectile.powershield_reflect_window_ticks =
        (uint16_t)(enabled.fighter.powershield_window_ticks + UINT16_C(1));
    return expect_status(
        pf_m4_validate_content(&invalid),
        PF_STATUS_INVALID_CONFIG,
        "invalid-reflect-window");
}

static int run_ground_hit_contract(
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command special = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    const pf_sim_event *event;
    uint32_t guard;

    if (!initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !step_sim(sim, special, special, &result, &inspection))
    {
        return 0;
    }
    event = find_event(&result, PF_SIM_EVENT_PROJECTILE_FIRE);
    if (event == NULL || event->source_player != UINT8_C(0) ||
        event->target_player != PF_SIM_EVENT_NO_PLAYER ||
        event->detail !=
            (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND ||
        count_events(&result, PF_SIM_EVENT_PROJECTILE_FIRE) != UINT32_C(1) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_GROUND ||
        inspection.players[1].action_state !=
            (uint8_t)PF_M4_ACTION_GROUND_IDLE ||
        inspection.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE ||
        inspection.projectile.owner != UINT8_C(0) ||
        inspection.projectile.hitbox_active != UINT8_C(1) ||
        inspection.projectile.velocity_x_q16 <= INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "m4-projectile=debug fire=%d source=%u target=%u detail=%u "
            "count=%" PRIu32 " p0=%u p1=%u state=%u owner=%u "
            "active=%u vx=%" PRId32 "\n",
            event != NULL,
            event != NULL ? (unsigned int)event->source_player : 999U,
            event != NULL ? (unsigned int)event->target_player : 999U,
            event != NULL ? (unsigned int)event->detail : 999U,
            count_events(&result, PF_SIM_EVENT_PROJECTILE_FIRE),
            (unsigned int)inspection.players[0].action_state,
            (unsigned int)inspection.players[1].action_state,
            (unsigned int)inspection.projectile.state,
            (unsigned int)inspection.projectile.owner,
            (unsigned int)inspection.projectile.hitbox_active,
            inspection.projectile.velocity_x_q16);
        return fail("simultaneous-slot-order");
    }

    event = NULL;
    for (guard = UINT32_C(0); guard < UINT32_C(32); ++guard)
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
        event = find_event(&result, PF_SIM_EVENT_PROJECTILE_HIT);
        if (event != NULL)
        {
            break;
        }
    }
    if (event == NULL || event->source_player != UINT8_C(0) ||
        event->target_player != UINT8_C(1) ||
        event->value_q16 == UINT32_C(0) ||
        inspection.players[1].damage_q16 == UINT32_C(0) ||
        inspection.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE ||
        inspection.projectile.hitbox_active != UINT8_C(0))
    {
        return fail("ground-projectile-hit");
    }
    return 1;
}

static int run_shield_contract(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command special = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    const test_command shield = {
        INT16_C(0), INT16_C(0), UINT64_C(0), UINT16_MAX};
    const pf_sim_event *event = NULL;
    uint32_t guard;

    if (!initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !step_sim(sim, special, shield, &result, &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(32); ++guard)
    {
        if (!step_sim(sim, (test_command){0}, shield, &result, &inspection))
        {
            return 0;
        }
        event = find_event(&result, PF_SIM_EVENT_SHIELD_BLOCK);
        if (event != NULL)
        {
            break;
        }
    }
    if (event == NULL ||
        find_event(&result, PF_SIM_EVENT_PROJECTILE_REFLECT) != NULL ||
        event->source_player != UINT8_C(0) ||
        event->target_player != UINT8_C(1) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[1].shield_health_q16 >=
            content->fighter.shield_health_q16 ||
        inspection.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE)
    {
        return fail("ordinary-shield-block");
    }
    return 1;
}

static int run_reflect_contract(
    const pf_m4_content *content,
    const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command special = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    const test_command shield = {
        INT16_C(0), INT16_C(0), UINT64_C(0), UINT16_MAX};
    const pf_sim_event *event;
    uint32_t guard;

    if (!initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !step_sim(sim, special, (test_command){0}, &result, &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0); guard < UINT32_C(32); ++guard)
    {
        const int32_t target_left =
            inspection.players[1].position_x_q16 -
            content->fighter.half_width_q16;

        if (inspection.projectile.hitbox_right_q16 >= target_left)
        {
            break;
        }
        if (!neutral_step(sim, &result, &inspection) ||
            find_event(&result, PF_SIM_EVENT_PROJECTILE_HIT) != NULL)
        {
            return fail("reflect-alignment");
        }
    }
    if (guard == UINT32_C(32) ||
        !step_sim(sim, (test_command){0}, shield, &result, &inspection))
    {
        return fail("reflect-setup");
    }
    event = find_event(&result, PF_SIM_EVENT_PROJECTILE_REFLECT);
    if (event == NULL || event->source_player != UINT8_C(1) ||
        event->target_player != UINT8_C(0) ||
        inspection.players[1].damage_q16 != UINT32_C(0) ||
        inspection.players[1].powershield != UINT8_C(1) ||
        inspection.projectile.owner != UINT8_C(1) ||
        inspection.projectile.velocity_x_q16 >= INT32_C(0) ||
        inspection.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE)
    {
        return fail("powershield-reflect");
    }

    event = NULL;
    for (guard = UINT32_C(0); guard < UINT32_C(40); ++guard)
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
        event = find_event(&result, PF_SIM_EVENT_PROJECTILE_HIT);
        if (event != NULL)
        {
            break;
        }
    }
    if (event == NULL || event->source_player != UINT8_C(1) ||
        event->target_player != UINT8_C(0) ||
        inspection.players[0].damage_q16 == UINT32_C(0) ||
        inspection.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE)
    {
        return fail("reflected-hit");
    }
    return 1;
}

static int run_short_hop_contract(const pf_content_view *view)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    const test_command jump = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_JUMP, UINT16_C(0)};
    const test_command special = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};
    const pf_sim_event *event;
    uint32_t guard;

    if (!initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !step_sim(sim, jump, (test_command){0}, &result, &inspection))
    {
        return 0;
    }
    for (guard = UINT32_C(0);
         guard < UINT32_C(12) && inspection.players[0].grounded != UINT8_C(0);
         ++guard)
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded != UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_AIRBORNE ||
        !step_sim(sim, special, (test_command){0}, &result, &inspection))
    {
        return fail("short-hop-fire-setup");
    }
    event = find_event(&result, PF_SIM_EVENT_PROJECTILE_FIRE);
    if (event == NULL || event->detail !=
            (uint16_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_PROJECTILE_FIRE_AIR ||
        inspection.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE)
    {
        return fail("short-hop-fire");
    }

    for (guard = UINT32_C(0);
         guard < UINT32_C(180) &&
         inspection.players[0].grounded == UINT8_C(0);
         ++guard)
    {
        if (!neutral_step(sim, &result, &inspection))
        {
            return 0;
        }
    }
    if (inspection.players[0].grounded == UINT8_C(0) ||
        inspection.players[0].action_state !=
            (uint8_t)PF_M4_ACTION_LANDING)
    {
        return fail("short-hop-landing");
    }
    return 1;
}

typedef struct camping_trace
{
    uint32_t projectile_fires;
    uint32_t projectile_hits;
    uint32_t approach_hits;
    uint32_t camper_damage_q16;
    uint32_t approacher_damage_q16;
    int32_t minimum_separation_q16;
    uint64_t completed_ticks;
} camping_trace;

static int run_camping_trace(
    const pf_content_view *view,
    int fire_projectiles,
    camping_trace *out_trace)
{
    test_storage storage;
    pf_sim *sim = NULL;
    pf_tick_result result;
    pf_m4_inspection inspection;
    uint32_t tick;
    int special_held_previous_tick = 0;

    if (out_trace == NULL ||
        !initialize_sim(&storage, view, &sim) || !reset_sim(sim) ||
        !expect_status(
            pf_m4_inspect(sim, &inspection),
            PF_STATUS_OK,
            "camping-inspect-start"))
    {
        return 0;
    }
    (void)memset(out_trace, 0, sizeof(*out_trace));
    out_trace->minimum_separation_q16 = INT32_MAX;
    for (tick = UINT32_C(0); tick < UINT32_C(180); ++tick)
    {
        const int32_t separation_q16 =
            inspection.players[1].position_x_q16 -
            inspection.players[0].position_x_q16;
        const int approach_attack_requested =
            separation_q16 <= INT32_C(2) * PF_Q16_ONE &&
            (tick & UINT32_C(1)) == UINT32_C(0);
        const int special_requested =
            fire_projectiles != 0 &&
            special_held_previous_tick == 0 &&
            inspection.projectile.state ==
                (uint8_t)PF_M4_PROJECTILE_STATE_INACTIVE &&
            inspection.players[0].action_state ==
                (uint8_t)PF_M4_ACTION_GROUND_IDLE;
        const test_command camper = {
            INT16_C(0),
            INT16_C(0),
            special_requested != 0
                ? PF_INPUT_BUTTON_SPECIAL
                : UINT64_C(0),
            UINT16_C(0)};
        const test_command approacher = {
            INT16_MIN,
            INT16_C(0),
            approach_attack_requested != 0
                ? PF_INPUT_BUTTON_ATTACK
                : UINT64_C(0),
            UINT16_C(0)};
        uint32_t event_index;

        if (separation_q16 < out_trace->minimum_separation_q16)
        {
            out_trace->minimum_separation_q16 = separation_q16;
        }
        if (!step_sim(
                sim,
                camper,
                approacher,
                &result,
                &inspection))
        {
            return 0;
        }
        special_held_previous_tick = special_requested;
        for (event_index = UINT32_C(0);
             event_index < (uint32_t)result.event_count;
             ++event_index)
        {
            const pf_sim_event *event = &result.events[event_index];

            if (event->type ==
                    (uint16_t)PF_SIM_EVENT_PROJECTILE_FIRE &&
                event->source_player == UINT8_C(0))
            {
                ++out_trace->projectile_fires;
            }
            else if (event->type ==
                         (uint16_t)PF_SIM_EVENT_PROJECTILE_HIT &&
                     event->source_player == UINT8_C(0) &&
                     event->target_player == UINT8_C(1))
            {
                ++out_trace->projectile_hits;
            }
            else if (event->type == (uint16_t)PF_SIM_EVENT_HIT &&
                     event->source_player == UINT8_C(1) &&
                     event->target_player == UINT8_C(0))
            {
                ++out_trace->approach_hits;
            }
        }
        if (result.terminated != UINT8_C(0) ||
            result.truncated != UINT8_C(0))
        {
            return fail("camping-unexpected-match-end");
        }
    }
    out_trace->camper_damage_q16 =
        inspection.players[0].damage_q16;
    out_trace->approacher_damage_q16 =
        inspection.players[1].damage_q16;
    out_trace->completed_ticks = inspection.tick;
    return 1;
}

static int run_camping_contract(void)
{
    pf_m4_content content;
    pf_content_view view;
    camping_trace camping;
    camping_trace no_fire;

    if (!expect_status(
            pf_m4_default_content(&content),
            PF_STATUS_OK,
            "camping-default-content"))
    {
        return 0;
    }
    content.projectile.enabled = UINT8_C(1);
    content.stage.spawn_spacing_q16 = INT32_C(8) * PF_Q16_ONE;
    content.stage.platform_motion_amplitude_q16 = INT32_C(0);
    if (!expect_status(
            pf_m4_make_content_view(&content, &view),
            PF_STATUS_OK,
            "camping-content-view") ||
        !run_camping_trace(&view, 1, &camping) ||
        !run_camping_trace(&view, 0, &no_fire))
    {
        return 0;
    }
    if (camping.completed_ticks != UINT64_C(180) ||
        camping.projectile_fires != UINT32_C(7) ||
        camping.projectile_hits != UINT32_C(6) ||
        camping.approach_hits != UINT32_C(0) ||
        camping.camper_damage_q16 != UINT32_C(0) ||
        camping.approacher_damage_q16 <
            UINT32_C(18) * UINT32_C(65536) ||
        camping.minimum_separation_q16 !=
            TEST_CAMPING_MINIMUM_SEPARATION_Q16 ||
        no_fire.completed_ticks != UINT64_C(180) ||
        no_fire.projectile_fires != UINT32_C(0) ||
        no_fire.projectile_hits != UINT32_C(0) ||
        no_fire.approach_hits != UINT32_C(3) ||
        no_fire.camper_damage_q16 == UINT32_C(0) ||
        no_fire.minimum_separation_q16 >=
            camping.minimum_separation_q16)
    {
        (void)fprintf(
            stderr,
            "m4-projectile=debug operation=camping"
            " camping_fires=%" PRIu32
            " camping_hits=%" PRIu32
            " camping_counter_hits=%" PRIu32
            " camping_damage=%" PRIu32
            " camping_target_damage=%" PRIu32
            " camping_min=%" PRId32
            " control_hits=%" PRIu32
            " control_damage=%" PRIu32
            " control_min=%" PRId32 "\n",
            camping.projectile_fires,
            camping.projectile_hits,
            camping.approach_hits,
            camping.camper_damage_q16,
            camping.approacher_damage_q16,
            camping.minimum_separation_q16,
            no_fire.approach_hits,
            no_fire.camper_damage_q16,
            no_fire.minimum_separation_q16);
        return fail("camping-route");
    }
    (void)printf(
        "m4-camping=pass ticks=180 fires=%" PRIu32
        " projectile_hits=%" PRIu32
        " minimum_separation_q16=%" PRId32
        " no_fire_approach_hits=%" PRIu32 "\n",
        camping.projectile_fires,
        camping.projectile_hits,
        camping.minimum_separation_q16,
        no_fire.approach_hits);
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
    size_t save_size = (size_t)0;
    pf_mut_bytes destination;
    pf_bytes save;
    uint32_t tick;
    pf_input_frame replay_inputs[
        TEST_REPLAY_TICKS * UINT32_C(2)];
    pf_state_hash replay_hashes[TEST_REPLAY_TICKS + UINT32_C(1)];
    pf_replay_source replay_source;
    pf_replay_verification verification;
    uint8_t replay_bytes[16384];
    size_t replay_size = (size_t)0;
    pf_mut_bytes replay_destination;
    pf_bytes replay;
    pf_rl_action actions[2];
    pf_rl_transition transition;
    const test_command special = {
        INT16_C(0), INT16_C(0), PF_INPUT_BUTTON_SPECIAL, UINT16_C(0)};

    if (!initialize_sim(&source_storage, view, &source) ||
        !initialize_sim(&loaded_storage, view, &loaded) ||
        !reset_sim(source) || !reset_sim(loaded) ||
        !step_sim(
            source,
            special,
            (test_command){0},
            &source_result,
            &inspection) ||
        !neutral_step(source, &source_result, &inspection) ||
        !expect_status(
            pf_sim_query_save_size(source, &save_size),
            PF_STATUS_OK,
            "projectile-save-size") ||
        save_size != (size_t)690)
    {
        return fail("projectile-save-setup");
    }
    destination.bytes = save_bytes;
    destination.capacity = sizeof(save_bytes);
    destination.size = (size_t)0;
    save.bytes = save_bytes;
    save.size = save_size;
    if (!expect_status(
            pf_sim_save(source, &destination),
            PF_STATUS_OK,
            "projectile-save") ||
        destination.size != save_size ||
        !expect_status(
            pf_sim_load(loaded, save),
            PF_STATUS_OK,
            "projectile-load"))
    {
        return 0;
    }
    for (tick = UINT32_C(0); tick < UINT32_C(16); ++tick)
    {
        if (!neutral_step(source, &source_result, &inspection) ||
            !neutral_step(loaded, &loaded_result, &inspection) ||
            !result_equal(&source_result, &loaded_result) ||
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
            return fail("projectile-save-future");
        }
    }

    if (!initialize_sim(&initial_storage, view, &initial) ||
        !initialize_sim(&verifier_storage, view, &verifier) ||
        !reset_sim(source) || !reset_sim(initial) || !reset_sim(verifier) ||
        !expect_status(
            pf_sim_clone(initial, source),
            PF_STATUS_OK,
            "projectile-clone-initial") ||
        !expect_status(
            pf_sim_hash(initial, &replay_hashes[0]),
            PF_STATUS_OK,
            "projectile-replay-initial-hash"))
    {
        return 0;
    }
    (void)memset(replay_inputs, 0, sizeof(replay_inputs));
    for (tick = UINT32_C(0); tick < TEST_REPLAY_TICKS; ++tick)
    {
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
        replay_inputs[tick * UINT32_C(2)].buttons =
            tick == UINT32_C(0)
                ? PF_INPUT_BUTTON_JUMP
                : tick == UINT32_C(3)
                ? PF_INPUT_BUTTON_SPECIAL
                : UINT64_C(0);
        if (!expect_status(
                pf_sim_tick(
                    source,
                    &replay_inputs[tick * UINT32_C(2)],
                    (size_t)2,
                    &source_result),
                PF_STATUS_OK,
                "projectile-replay-tick") ||
            !expect_status(
                pf_sim_hash(
                    source,
                    &replay_hashes[tick + UINT32_C(1)]),
                PF_STATUS_OK,
                "projectile-replay-hash"))
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
            "projectile-replay-size") ||
        replay_size > sizeof(replay_bytes))
    {
        return fail("projectile-replay-capacity");
    }
    replay_destination.bytes = replay_bytes;
    replay_destination.capacity = sizeof(replay_bytes);
    replay_destination.size = (size_t)0;
    replay.bytes = replay_bytes;
    replay.size = replay_size;
    if (!expect_status(
            pf_replay_encode(&replay_source, &replay_destination),
            PF_STATUS_OK,
            "projectile-replay-encode") ||
        !expect_status(
            pf_replay_verify(verifier, replay, &verification),
            PF_STATUS_OK,
            "projectile-replay-verify") ||
        verification.status != (uint32_t)PF_STATUS_OK ||
        verification.verified_ticks != (uint64_t)TEST_REPLAY_TICKS)
    {
        return fail("projectile-replay-verification");
    }

    if (!initialize_sim(&rl_storage, view, &rl_sim) ||
        !expect_status(
            pf_rl_reset(
                rl_sim,
                UINT64_C(0x524c50554c5345),
                &transition),
            PF_STATUS_OK,
            "projectile-rl-reset"))
    {
        return 0;
    }
    (void)memset(actions, 0, sizeof(actions));
    actions[0].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[1].schema_version = PF_RL_ACTION_SCHEMA_VERSION;
    actions[0].buttons = PF_INPUT_BUTTON_SPECIAL;
    if (!expect_status(
            pf_rl_step(rl_sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "projectile-rl-step") ||
        (transition.legal_buttons[0] & PF_INPUT_BUTTON_SPECIAL) ==
            UINT64_C(0) ||
        transition.structured_observation.projectile.state !=
            (uint8_t)PF_M4_PROJECTILE_STATE_ACTIVE ||
        transition.structured_observation.projectile.velocity_x_q16 !=
            content->projectile.speed_q16 ||
        transition.compact_observation.values[
            PF_RL_COMPACT_PROJECTILE_BASE + UINT16_C(2)] !=
            content->projectile.speed_q16 ||
        (uint32_t)transition.compact_observation.values[
            PF_RL_COMPACT_PROJECTILE_BASE +
            PF_RL_COMPACT_PROJECTILE_STATE_BITS_OFFSET] !=
            ((uint32_t)PF_M4_PROJECTILE_STATE_ACTIVE |
             (UINT32_C(1) << 2U)) ||
        transition.compact_observation.values[
            PF_RL_COMPACT_PROJECTILE_BASE +
            PF_RL_COMPACT_PROJECTILE_LIFETIME_OFFSET] !=
            (int32_t)content->projectile.lifetime_ticks)
    {
        return fail("projectile-rl-observation");
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
    if (!make_projectile_content(&content, &view))
    {
        return fail("fixture-suite");
    }
    if (!run_ground_hit_contract(&view))
    {
        return fail("ground-hit-suite");
    }
    if (!run_shield_contract(&content, &view))
    {
        return fail("shield-suite");
    }
    if (!run_reflect_contract(&content, &view))
    {
        return fail("reflect-suite");
    }
    if (!run_short_hop_contract(&view))
    {
        return fail("short-hop-suite");
    }
    if (!run_camping_contract())
    {
        return fail("camping-suite");
    }
    if (!run_save_replay_rl_contract(&content, &view))
    {
        return fail("save-replay-rl-suite");
    }

    (void)printf(
        "m4-projectile=pass content_schema=%u state_schema=%u "
        "save_bytes=690 projectile_invariants=46 short_hop_laser=1 "
        "camping=1 powershield_reflect=1 replay=1 rl=1\n",
        (unsigned int)PF_M4_CONTENT_SCHEMA_VERSION,
        (unsigned int)PF_SIM_STATE_SCHEMA_VERSION);
    return 0;
}
