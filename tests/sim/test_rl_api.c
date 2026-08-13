#include "pf/rl.h"
#include "pf/sim.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

#define TEST_MEMORY_BYTES 4096U
#define TEST_MEMORY_ALIGNMENT 64U
#define TEST_BATCH_ENVIRONMENTS 6U
#define TEST_LIGHT_SHIELD_TRIGGER UINT16_C(32768)
#define TEST_LIGHT_SHIELD_STRENGTH UINT16_C(18725)
#define TEST_SHIELD_TILT_AXIS INT16_C(10000)

typedef struct test_sim_storage
{
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t state[TEST_MEMORY_BYTES];
    alignas(TEST_MEMORY_ALIGNMENT) uint8_t scratch[TEST_MEMORY_BYTES];
} test_sim_storage;

static int expect_status(
    pf_status actual,
    pf_status expected,
    const char *operation)
{
    if (actual != expected)
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=%s expected=%s actual=%s\n",
            operation,
            pf_status_name(expected),
            pf_status_name(actual));
        return 0;
    }
    return 1;
}

static pf_content_view make_content(void)
{
    pf_content_view content;
    uint32_t byte_index;

    (void)memset(&content, 0, sizeof(content));
    content.struct_size = (uint32_t)sizeof(content);
    content.schema_version = PF_SIM_CONTENT_SCHEMA_VERSION;
    for (byte_index = UINT32_C(0);
         byte_index < (uint32_t)sizeof(content.content_hash.bytes);
         ++byte_index)
    {
        content.content_hash.bytes[byte_index] =
            (uint8_t)(byte_index * UINT32_C(5) + UINT32_C(19));
    }
    return content;
}

static int initialize_sim(
    test_sim_storage *storage,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim)
{
    return expect_status(
        pf_sim_init(
            storage->state,
            sizeof(storage->state),
            storage->scratch,
            sizeof(storage->scratch),
            content,
            config,
            out_sim),
        PF_STATUS_OK,
        "init");
}

static void initialize_actions(
    pf_rl_action actions[PF_SIM_MAX_PLAYERS])
{
    uint32_t player_index;

    (void)memset(
        actions,
        0,
        sizeof(*actions) * (size_t)PF_SIM_MAX_PLAYERS);
    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        actions[player_index].schema_version =
            PF_RL_ACTION_SCHEMA_VERSION;
    }
}

static uint32_t i32_bits(int32_t value)
{
    uint32_t result;

    (void)memcpy(&result, &value, sizeof(result));
    return result;
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

static int verify_transition_contract(
    const pf_rl_transition *transition,
    uint8_t player_count,
    uint64_t tick)
{
    uint32_t player_index;
    const uint32_t match_bits = i32_bits(
        transition->compact_observation.values[
            PF_RL_COMPACT_MATCH_BITS_INDEX]);

    if (transition->struct_size !=
            (uint32_t)sizeof(*transition) ||
        transition->schema_version !=
            PF_RL_TRANSITION_SCHEMA_VERSION ||
        transition->reserved != UINT16_C(0) ||
        transition->compact_observation.struct_size !=
            (uint32_t)sizeof(transition->compact_observation) ||
        transition->compact_observation.schema_version !=
            PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION ||
        transition->compact_observation.value_count !=
            PF_RL_COMPACT_VALUE_COUNT ||
        transition->compact_observation.values[
            PF_RL_COMPACT_RESERVED_LOW_INDEX] != INT32_C(0) ||
        transition->compact_observation.values[
            PF_RL_COMPACT_RESERVED_HIGH_INDEX] != INT32_C(0) ||
        transition->structured_observation.seed != UINT64_C(0) ||
        transition->structured_observation.tick != tick ||
        transition->tick_result.completed_tick != tick ||
        (match_bits & UINT32_C(0xff)) != (uint32_t)player_count ||
        ((match_bits >> 18U) & UINT32_C(0x1)) !=
            (uint32_t)transition->structured_observation.sudden_death ||
        ((match_bits >> 19U) & UINT32_C(0x7f)) !=
            (uint32_t)transition->structured_observation.stock_count ||
        ((match_bits >> 26U) & UINT32_C(0xf)) !=
            (uint32_t)transition->structured_observation.winner_mask)
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=transition-contract tick=%" PRIu64
            "\n",
            tick);
        return 0;
    }

    for (player_index = UINT32_C(0);
         player_index < PF_SIM_MAX_PLAYERS;
         ++player_index)
    {
        const uint16_t base =
            PF_RL_COMPACT_PLAYER_BASE(player_index);
        const uint16_t stale_base =
            PF_RL_COMPACT_STALE_MOVE_PLAYER_BASE(player_index);
        const pf_player_observation *player =
            &transition->structured_observation.players[player_index];
        const uint32_t stale_count_ids_0_2 =
            (uint32_t)player->stale_move_count |
            ((uint32_t)player->stale_move_ids[0] << 8U) |
            ((uint32_t)player->stale_move_ids[1] << 16U) |
            ((uint32_t)player->stale_move_ids[2] << 24U);
        const uint32_t stale_ids_3_6 =
            (uint32_t)player->stale_move_ids[3] |
            ((uint32_t)player->stale_move_ids[4] << 8U) |
            ((uint32_t)player->stale_move_ids[5] << 16U) |
            ((uint32_t)player->stale_move_ids[6] << 24U);
        const uint32_t stale_ids_7_8 =
            (uint32_t)player->stale_move_ids[7] |
            ((uint32_t)player->stale_move_ids[8] << 8U);
        if (transition->compact_observation.values[
                base + UINT16_C(2)] != player->position_x_f32 ||
            transition->compact_observation.values[
                base + UINT16_C(3)] != player->position_y_f32 ||
            transition->compact_observation.values[
                base + UINT16_C(4)] != player->velocity_x_f32 ||
            transition->compact_observation.values[
                base + UINT16_C(5)] != player->velocity_y_f32 ||
            transition->compact_observation.values[
                base + PF_RL_COMPACT_PLAYER_STOCKS_OFFSET] !=
                (int32_t)player->stocks_remaining ||
            transition->compact_observation.values[
                base + PF_RL_COMPACT_PLAYER_RESPAWN_OFFSET] !=
                (int32_t)player->respawn_ticks ||
            transition->compact_observation.values[
                base +
                PF_RL_COMPACT_PLAYER_INVULNERABILITY_OFFSET] !=
                (int32_t)player->respawn_invulnerability_ticks ||
            transition->compact_observation.values[
                PF_RL_COMPACT_CHARGE_BASE +
                (uint16_t)player_index] !=
                (int32_t)player->charge_ticks ||
            transition->compact_observation.values[
                PF_RL_COMPACT_SMASH_CHARGE_BASE +
                (uint16_t)player_index] !=
                (int32_t)player->smash_charge_ticks ||
            transition->compact_observation.values[
                PF_RL_COMPACT_SHIELD_STRENGTH_BASE +
                (uint16_t)player_index] !=
                (int32_t)player->shield_strength ||
            transition->compact_observation.values[
                PF_RL_COMPACT_SHIELD_HEALTH_BASE +
                (uint16_t)player_index] !=
                (int32_t)player->shield_health_f32 ||
            transition->compact_observation.values[
                PF_RL_COMPACT_SHIELD_TILT_BASE +
                (uint16_t)(UINT16_C(2) *
                           (uint16_t)player_index)] !=
                (int32_t)player->shield_tilt_x ||
            transition->compact_observation.values[
                PF_RL_COMPACT_SHIELD_TILT_BASE +
                (uint16_t)(UINT16_C(2) *
                           (uint16_t)player_index) +
                UINT16_C(1)] !=
                (int32_t)player->shield_tilt_y ||
            transition->compact_observation.values[
                stale_base +
                PF_RL_COMPACT_STALE_MOVE_MULTIPLIER_OFFSET] !=
                (int32_t)player->stale_move_multiplier_f32 ||
            i32_bits(transition->compact_observation.values[
                stale_base +
                PF_RL_COMPACT_STALE_MOVE_COUNT_IDS_0_2_OFFSET]) !=
                stale_count_ids_0_2 ||
            i32_bits(transition->compact_observation.values[
                stale_base +
                PF_RL_COMPACT_STALE_MOVE_IDS_3_6_OFFSET]) !=
                stale_ids_3_6 ||
            i32_bits(transition->compact_observation.values[
                stale_base +
                PF_RL_COMPACT_STALE_MOVE_IDS_7_8_OFFSET]) !=
                stale_ids_7_8)
        {
            (void)fprintf(
                stderr,
                "rl-api=fail operation=compact-player slot=%" PRIu32
                "\n",
                player_index);
            return 0;
        }
    }
    return 1;
}

static int run_duel_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_rl_action actions[PF_SIM_MAX_PLAYERS];
    pf_rl_transition transition;
    pf_state_hash before_invalid;
    pf_state_hash after_invalid;
    pf_sim_identity identity;
    pf_sim_observation diagnostic_observation;
    uint64_t charge_tick;
    uint64_t movement_tick;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "duel-config"))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=duel-config-contract\n");
        return 0;
    }
    config.max_ticks = UINT64_C(100);
    if (!initialize_sim(&storage, content, &config, &sim) ||
        !expect_status(
            pf_sim_query_identity(sim, &identity),
            PF_STATUS_OK,
            "duel-identity") ||
        identity.struct_size != (uint32_t)sizeof(identity) ||
        identity.schema_version != PF_SIM_IDENTITY_SCHEMA_VERSION ||
        identity.sim_abi_version != PF_SIM_ABI_VERSION ||
        identity.tick_rate_hz != PF_SIM_TICK_RATE_HZ ||
        identity.player_count != UINT8_C(2) ||
        identity.mode != (uint8_t)PF_SIM_MODE_DUEL ||
        identity.max_ticks != config.max_ticks ||
        identity.stock_count != PF_SIM_DEFAULT_STOCK_COUNT ||
        identity.respawn_delay_ticks !=
            PF_SIM_DEFAULT_RESPAWN_DELAY_TICKS ||
        identity.respawn_invulnerability_ticks !=
            PF_SIM_DEFAULT_RESPAWN_INVULNERABILITY_TICKS ||
        memcmp(
            identity.content_hash.bytes,
            content->content_hash.bytes,
            sizeof(identity.content_hash.bytes)) != 0 ||
        !expect_status(
            pf_rl_reset(
                sim,
                UINT64_C(0xabcdef0123456789),
                &transition),
            PF_STATUS_OK,
            "duel-rl-reset") ||
        transition.status != (uint32_t)PF_STATUS_OK ||
        !verify_transition_contract(&transition, UINT8_C(2), UINT64_C(0)) ||
        transition.legal_buttons[0] != PF_INPUT_KNOWN_BUTTONS ||
        transition.legal_buttons[1] != PF_INPUT_KNOWN_BUTTONS ||
        transition.legal_buttons[2] != UINT64_C(0) ||
        transition.reward_f32[0] != INT32_C(0) ||
        transition.reward_f32[1] != INT32_C(0) ||
        transition.structured_observation.stock_count !=
            PF_SIM_DEFAULT_STOCK_COUNT ||
        transition.structured_observation.players[0].stocks_remaining !=
            PF_SIM_DEFAULT_STOCK_COUNT ||
        transition.structured_observation.players[1].stocks_remaining !=
            PF_SIM_DEFAULT_STOCK_COUNT ||
        transition.structured_observation.players[0].respawn_ticks !=
            UINT16_C(0) ||
        transition.structured_observation.players[0].
                respawn_invulnerability_ticks != UINT16_C(0) ||
        !expect_status(
            pf_sim_observe(sim, &diagnostic_observation),
            PF_STATUS_OK,
            "duel-diagnostic-observe") ||
        diagnostic_observation.seed !=
            UINT64_C(0xabcdef0123456789))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=duel-initial-contract\n");
        return 0;
    }

    initialize_actions(actions);
    actions[0].left_trigger = TEST_LIGHT_SHIELD_TRIGGER;
    actions[0].main_stick_x = TEST_SHIELD_TILT_AXIS;
    actions[0].main_stick_y = -TEST_SHIELD_TILT_AXIS;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "duel-light-shield-step") ||
        !verify_transition_contract(
            &transition,
            UINT8_C(2),
            UINT64_C(1)) ||
        transition.structured_observation.players[0]
                .shield_strength != TEST_LIGHT_SHIELD_STRENGTH ||
        transition.compact_observation.values[
            PF_RL_COMPACT_SHIELD_STRENGTH_BASE] !=
            (int32_t)TEST_LIGHT_SHIELD_STRENGTH ||
        transition.structured_observation.players[0].shield_tilt_x <=
            INT16_C(0) ||
        transition.structured_observation.players[0].shield_tilt_y >=
            INT16_C(0) ||
        transition.compact_observation.values[
            PF_RL_COMPACT_SHIELD_HEALTH_BASE] !=
            (int32_t)transition.structured_observation.players[0]
                .shield_health_f32 ||
        transition.compact_observation.values[
            PF_RL_COMPACT_SHIELD_TILT_BASE] !=
            (int32_t)transition.structured_observation.players[0]
                .shield_tilt_x ||
        transition.compact_observation.values[
            PF_RL_COMPACT_SHIELD_TILT_BASE + UINT16_C(1)] !=
            (int32_t)transition.structured_observation.players[0]
                .shield_tilt_y ||
        !expect_status(
            pf_rl_reset(
                sim,
                UINT64_C(0xabcdef0123456789),
                &transition),
            PF_STATUS_OK,
            "duel-post-light-shield-reset") ||
        !verify_transition_contract(
            &transition,
            UINT8_C(2),
            UINT64_C(0)))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=duel-light-shield strength=%u"
            " compact=%" PRId32 " tilt=(%d,%d) health=%" PRIu32
            "\n",
            (unsigned int)transition.structured_observation.players[0]
                .shield_strength,
            transition.compact_observation.values[
                PF_RL_COMPACT_SHIELD_STRENGTH_BASE],
            (int)transition.structured_observation.players[0]
                .shield_tilt_x,
            (int)transition.structured_observation.players[0]
                .shield_tilt_y,
            transition.structured_observation.players[0]
                .shield_health_f32);
        return 0;
    }

    initialize_actions(actions);
    actions[0].buttons = PF_INPUT_BUTTON_ATTACK;
    actions[0].main_stick_x = INT16_MAX;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "duel-smash-charge-step") ||
        !verify_transition_contract(&transition, UINT8_C(2), UINT64_C(1)))
    {
        (void)fprintf(stderr, "rl-api=fail operation=duel-smash-entry\n");
        return 0;
    }
    for (charge_tick = UINT64_C(2);
         charge_tick <= UINT64_C(30) &&
         transition.structured_observation.players[0]
                 .smash_charge_ticks == UINT16_C(0);
         ++charge_tick)
    {
        if (!expect_status(
                pf_rl_step(sim, actions, (size_t)2, &transition),
                PF_STATUS_OK,
                "duel-smash-charge-advance") ||
            !verify_transition_contract(
                &transition,
                UINT8_C(2),
                charge_tick))
        {
            return 0;
        }
    }
    if (transition.structured_observation.players[0]
                .smash_charge_ticks != UINT16_C(1) ||
        transition.compact_observation.values[
            PF_RL_COMPACT_SMASH_CHARGE_BASE] != INT32_C(1) ||
        !expect_status(
            pf_rl_reset(
                sim,
                UINT64_C(0xabcdef0123456789),
                &transition),
            PF_STATUS_OK,
            "duel-post-smash-charge-reset") ||
        !verify_transition_contract(&transition, UINT8_C(2), UINT64_C(0)))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=duel-smash-charge tick=%" PRIu64
            " charge=%u compact=%" PRId32 "\n",
            transition.structured_observation.tick,
            (unsigned int)transition.structured_observation.players[0]
                .smash_charge_ticks,
            transition.compact_observation.values[
                PF_RL_COMPACT_SMASH_CHARGE_BASE]);
        return 0;
    }

    initialize_actions(actions);
    actions[0].buttons = PF_INPUT_BUTTON_JUMP;
    actions[0].main_stick_x = INT16_MAX;
    actions[1].main_stick_x = INT16_MIN;
    for (movement_tick = UINT64_C(1);
         movement_tick <= UINT64_C(5);
         ++movement_tick)
    {
        if (!expect_status(
                pf_rl_step(sim, actions, (size_t)2, &transition),
                PF_STATUS_OK,
                "duel-rl-step") ||
            !verify_transition_contract(
                &transition,
                UINT8_C(2),
                movement_tick))
        {
            return 0;
        }
    }
    if (transition.structured_observation.players[0].grounded !=
            UINT8_C(0) ||
        transition.structured_observation.players[0].position_y_f32 <=
            INT32_C(0) ||
        transition.reward_f32[0] <= INT32_C(0) ||
        transition.reward_f32[0] != transition.reward_f32[1] ||
        !expect_status(
            pf_sim_hash(sim, &before_invalid),
            PF_STATUS_OK,
            "duel-before-invalid-hash"))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=duel-movement tick=%" PRIu64
            " grounded=%u y=%" PRId32 " reward0=%" PRId32
            " reward1=%" PRId32 "\n",
            transition.structured_observation.tick,
            (unsigned int)transition.structured_observation.players[0]
                .grounded,
            transition.structured_observation.players[0].position_y_f32,
            transition.reward_f32[0],
            transition.reward_f32[1]);
        return 0;
    }

    initialize_actions(actions);
    actions[0].buttons = UINT64_C(1) << 7U;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_INVALID_ARGUMENT,
            "duel-invalid-action") ||
        transition.status != (uint32_t)PF_STATUS_INVALID_ARGUMENT ||
        transition.structured_observation.tick != UINT64_C(5) ||
        !expect_status(
            pf_sim_hash(sim, &after_invalid),
            PF_STATUS_OK,
            "duel-after-invalid-hash") ||
        !hash_equal(&before_invalid, &after_invalid))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=invalid-action-atomicity\n");
        return 0;
    }

    initialize_actions(actions);
    actions[0].buttons = PF_INPUT_BUTTON_FORFEIT;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "duel-forfeit") ||
        transition.tick_result.completed_tick != UINT64_C(6) ||
        transition.tick_result.terminated != UINT8_C(1) ||
        transition.tick_result.winner_mask != UINT8_C(2) ||
        transition.reward_f32[1] - transition.reward_f32[0] !=
            INT32_C(2) * 1.0f ||
        transition.legal_buttons[0] != UINT64_C(0) ||
        transition.legal_buttons[1] != UINT64_C(0))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=duel-terminal-reward"
            " completed=%" PRIu64 " terminated=%u winner=%u"
            " reward=(%" PRId32 ",%" PRId32 ") legal=(%" PRIu64
            ",%" PRIu64 ")\n",
            transition.tick_result.completed_tick,
            (unsigned int)transition.tick_result.terminated,
            (unsigned int)transition.tick_result.winner_mask,
            transition.reward_f32[0],
            transition.reward_f32[1],
            transition.legal_buttons[0],
            transition.legal_buttons[1]);
        return 0;
    }

    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_EPISODE_DONE,
            "duel-post-terminal") ||
        transition.reward_f32[0] != INT32_C(0) ||
        transition.reward_f32[1] != INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=terminal-reward-repeat\n");
        return 0;
    }
    return 1;
}

static int run_team_reward_test(const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_rl_action actions[PF_SIM_MAX_PLAYERS];
    pf_rl_transition transition;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(4),
                PF_SIM_MODE_TEAMS),
            PF_STATUS_OK,
            "team-config") ||
        !initialize_sim(&storage, content, &config, &sim) ||
        !expect_status(
            pf_rl_reset(sim, UINT64_C(77), &transition),
            PF_STATUS_OK,
            "team-reset"))
    {
        (void)fprintf(stderr, "rl-api=fail operation=team-initial-contract\n");
        return 0;
    }

    initialize_actions(actions);
    actions[3].buttons = PF_INPUT_BUTTON_FORFEIT;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)4, &transition),
            PF_STATUS_OK,
            "team-forfeit") ||
        transition.tick_result.winner_mask != UINT8_C(5) ||
        transition.reward_f32[0] != 1.0f ||
        transition.reward_f32[1] != -1.0f ||
        transition.reward_f32[2] != 1.0f ||
        transition.reward_f32[3] != -1.0f)
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=team-terminal-reward\n");
        return 0;
    }

    if (!expect_status(
            pf_rl_reset(sim, UINT64_C(78), &transition),
            PF_STATUS_OK,
            "team-draw-reset"))
    {
        return 0;
    }
    initialize_actions(actions);
    actions[0].buttons = PF_INPUT_BUTTON_FORFEIT;
    actions[1].buttons = PF_INPUT_BUTTON_FORFEIT;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)4, &transition),
            PF_STATUS_OK,
            "team-simultaneous-forfeit") ||
        transition.tick_result.winner_mask != UINT8_C(0) ||
        transition.reward_f32[0] != INT32_C(0) ||
        transition.reward_f32[1] != INT32_C(0) ||
        transition.reward_f32[2] != INT32_C(0) ||
        transition.reward_f32[3] != INT32_C(0))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=team-draw-reward\n");
        return 0;
    }
    return 1;
}

static int run_engagement_shaping_test(
    const pf_content_view *content)
{
    test_sim_storage storage;
    pf_sim_config config;
    pf_sim *sim = NULL;
    pf_rl_action actions[PF_SIM_MAX_PLAYERS];
    pf_rl_transition transition;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "shaping-config") ||
        !initialize_sim(&storage, content, &config, &sim) ||
        !expect_status(
            pf_rl_reset(sim, UINT64_C(909), &transition),
            PF_STATUS_OK,
            "shaping-reset"))
    {
        return 0;
    }

    initialize_actions(actions);
    actions[0].main_stick_x = INT16_MIN;
    actions[1].main_stick_x = INT16_MAX;
    if (!expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "shaping-dash-entry") ||
        !expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "shaping-dash-impulse") ||
        !expect_status(
            pf_rl_step(sim, actions, (size_t)2, &transition),
            PF_STATUS_OK,
            "shaping-separate") ||
        transition.reward_f32[0] >= INT32_C(0) ||
        transition.reward_f32[0] != transition.reward_f32[1] ||
        transition.reward_f32[0] <
            -PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_F32)
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=engagement-shaping\n");
        return 0;
    }
    return 1;
}

static int run_batch_test(const pf_content_view *content)
{
    test_sim_storage storage[TEST_BATCH_ENVIRONMENTS];
    pf_sim *sims[TEST_BATCH_ENVIRONMENTS];
    uint64_t seeds[TEST_BATCH_ENVIRONMENTS];
    pf_rl_action actions[
        TEST_BATCH_ENVIRONMENTS * PF_SIM_MAX_PLAYERS];
    pf_rl_transition transitions[TEST_BATCH_ENVIRONMENTS];
    pf_sim_config config;
    size_t environment_index;
    size_t action_index;

    if (!expect_status(
            pf_sim_default_config(
                &config,
                UINT8_C(2),
                PF_SIM_MODE_DUEL),
            PF_STATUS_OK,
            "batch-config"))
    {
        return 0;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)TEST_BATCH_ENVIRONMENTS;
         ++environment_index)
    {
        sims[environment_index] = NULL;
        seeds[environment_index] =
            UINT64_C(1000) + (uint64_t)environment_index;
        if (!initialize_sim(
                &storage[environment_index],
                content,
                &config,
                &sims[environment_index]))
        {
            return 0;
        }
    }

    if (!expect_status(
            pf_rl_reset_batch(
                sims,
                seeds,
                (size_t)TEST_BATCH_ENVIRONMENTS,
                transitions),
            PF_STATUS_OK,
            "batch-reset"))
    {
        return 0;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)TEST_BATCH_ENVIRONMENTS;
         ++environment_index)
    {
        if (transitions[environment_index].structured_observation.seed !=
                UINT64_C(0) ||
            transitions[environment_index].structured_observation.tick !=
                UINT64_C(0))
        {
            (void)fprintf(
                stderr,
                "rl-api=fail operation=batch-reset-result env=%zu\n",
                environment_index);
            return 0;
        }
    }

    (void)memset(actions, 0, sizeof(actions));
    for (action_index = (size_t)0;
         action_index <
             (size_t)TEST_BATCH_ENVIRONMENTS *
                 (size_t)PF_SIM_MAX_PLAYERS;
         ++action_index)
    {
        actions[action_index].schema_version =
            PF_RL_ACTION_SCHEMA_VERSION;
    }
    actions[0].buttons = PF_INPUT_BUTTON_FORFEIT;
    actions[
        (size_t)2 * (size_t)PF_SIM_MAX_PLAYERS].buttons =
        UINT64_C(1) << 9U;

    if (!expect_status(
            pf_rl_step_batch(
                sims,
                (size_t)TEST_BATCH_ENVIRONMENTS,
                actions,
                (size_t)PF_SIM_MAX_PLAYERS,
                transitions),
            PF_STATUS_INVALID_ARGUMENT,
            "batch-step-with-one-invalid"))
    {
        return 0;
    }
    for (environment_index = (size_t)0;
         environment_index < (size_t)TEST_BATCH_ENVIRONMENTS;
         ++environment_index)
    {
        const uint64_t expected_tick =
            environment_index == (size_t)2
                ? UINT64_C(0)
                : UINT64_C(1);
        const uint32_t expected_status =
            environment_index == (size_t)2
                ? (uint32_t)PF_STATUS_INVALID_ARGUMENT
                : (uint32_t)PF_STATUS_OK;
        if (transitions[environment_index].structured_observation.tick !=
                expected_tick ||
            transitions[environment_index].status != expected_status)
        {
            (void)fprintf(
                stderr,
                "rl-api=fail operation=batch-independent-status env=%zu\n",
                environment_index);
            return 0;
        }
    }
    if (transitions[0].tick_result.terminated != UINT8_C(1) ||
        transitions[0].reward_f32[0] != -1.0f ||
        transitions[0].reward_f32[1] != 1.0f)
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=batch-terminal-result\n");
        return 0;
    }

    if (!expect_status(
            pf_rl_step_batch(
                sims,
                (size_t)TEST_BATCH_ENVIRONMENTS,
                actions,
                (size_t)PF_SIM_MAX_PLAYERS + (size_t)1,
                transitions),
            PF_STATUS_INVALID_ARGUMENT,
            "batch-fixed-action-stride"))
    {
        return 0;
    }

    if (!expect_status(
            pf_rl_step_batch(
                NULL,
                (size_t)0,
                NULL,
                (size_t)0,
                NULL),
            PF_STATUS_OK,
            "empty-batch") ||
        !expect_status(
            pf_sim_deinit(
                sims[TEST_BATCH_ENVIRONMENTS - (size_t)1]),
            PF_STATUS_OK,
            "deinit") ||
        !expect_status(
            pf_sim_deinit(
                sims[TEST_BATCH_ENVIRONMENTS - (size_t)1]),
            PF_STATUS_INVALID_STATE,
            "double-deinit"))
    {
        return 0;
    }
    return 1;
}

int main(void)
{
    pf_content_view content = make_content();
    pf_rl_spec spec;

    if (!expect_status(
            pf_rl_query_spec(&spec),
            PF_STATUS_OK,
            "query-spec") ||
        spec.struct_size != (uint32_t)sizeof(spec) ||
        spec.schema_version != PF_RL_SCHEMA_VERSION ||
        spec.action_schema_version != PF_RL_ACTION_SCHEMA_VERSION ||
        spec.transition_schema_version !=
            PF_RL_TRANSITION_SCHEMA_VERSION ||
        spec.compact_observation_schema_version !=
            PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION ||
        spec.compact_value_count != PF_RL_COMPACT_VALUE_COUNT ||
        spec.action_stride != (uint16_t)PF_SIM_MAX_PLAYERS ||
        spec.max_players != (uint8_t)PF_SIM_MAX_PLAYERS ||
        spec.reward_component_flags !=
            (PF_RL_REWARD_COMPONENT_TERMINAL |
             PF_RL_REWARD_COMPONENT_ENGAGEMENT) ||
        spec.reserved[0] != UINT8_C(0) ||
        spec.reserved[1] != UINT8_C(0) ||
        spec.known_buttons != PF_INPUT_KNOWN_BUTTONS ||
        spec.axis_minimum != INT16_MIN ||
        spec.axis_maximum != INT16_MAX ||
        spec.trigger_minimum != UINT16_C(0) ||
        spec.trigger_maximum != UINT16_MAX ||
        spec.terminal_reward_one_f32 != 1.0f ||
        spec.engagement_potential_limit_f32 !=
            PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_F32)
    {
        (void)fprintf(stderr, "rl-api=fail operation=spec-contract\n");
        return 1;
    }

    if (!run_duel_test(&content))
    {
        (void)fprintf(stderr, "rl-api=fail operation=duel-test\n");
        return 1;
    }
    if (!run_team_reward_test(&content))
    {
        (void)fprintf(stderr, "rl-api=fail operation=team-reward-test\n");
        return 1;
    }
    if (!run_engagement_shaping_test(&content))
    {
        (void)fprintf(
            stderr,
            "rl-api=fail operation=engagement-shaping-test\n");
        return 1;
    }
    if (!run_batch_test(&content))
    {
        (void)fprintf(stderr, "rl-api=fail operation=batch-test\n");
        return 1;
    }

    (void)printf(
        "rl-api=pass compact_values=%u batch_environments=%u"
        " reward_f32=%" PRId32
        " engagement_limit_f32=%" PRId32
        " schema=%u\n",
        (unsigned int)PF_RL_COMPACT_VALUE_COUNT,
        (unsigned int)TEST_BATCH_ENVIRONMENTS,
        1.0f,
        PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_F32,
        (unsigned int)PF_RL_SCHEMA_VERSION);
    return 0;
}
