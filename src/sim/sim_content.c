#include "sim_internal.h"
#include "sim_falcon_frame_data.h"
#include "sim_sha256.h"
#include "sim_ssbm_common_data.h"
#include "sim_ssbm_stage_data.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PF_F32_RATIO(numerator, denominator) \
    ((float)(numerator) / (float)(denominator))

static const uint8_t content_hash_domain[8] = {
    UINT8_C(0x50), UINT8_C(0x46), UINT8_C(0x4d), UINT8_C(0x34),
    UINT8_C(0x44), UINT8_C(0x41), UINT8_C(0x54), UINT8_C(0x31)};

static uint16_t falcon_reference_hitlag_ticks(uint8_t damage)
{
    return (uint16_t)(damage / UINT8_C(3) + UINT8_C(3));
}

static int falcon_reference_body_collision_window(
    uint16_t submotion_index,
    uint16_t displayed_frame_bias,
    uint16_t *out_begin_tick,
    uint16_t *out_end_tick)
{
    const falcon_body_collision_timing *timing =
        falcon_reference_body_collision_timing(submotion_index);

    if (timing == NULL || out_begin_tick == NULL || out_end_tick == NULL ||
        timing->state_two_frame == UINT16_MAX ||
        timing->state_zero_frame == UINT16_MAX ||
        timing->state_two_frame < displayed_frame_bias ||
        timing->state_zero_frame <= timing->state_two_frame)
    {
        return 0;
    }
    *out_begin_tick =
        (uint16_t)(timing->state_two_frame - displayed_frame_bias);
    *out_end_tick =
        (uint16_t)(timing->state_zero_frame - displayed_frame_bias);
    return 1;
}

static int apply_falcon_reference_common_action_timings(
    fighter_data *fighter)
{
    const falcon_submotion_data *dash =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_DASH);
    const falcon_submotion_data *turn =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_TURN);
    const falcon_submotion_data *turn_run =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_TURN_RUN);
    const falcon_submotion_data *run_brake =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_RUN_BRAKE);
    const falcon_submotion_data *landing =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_LANDING);
    const falcon_submotion_data *squat =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_SQUAT);
    const falcon_submotion_data *squat_reverse =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_SQUAT_REVERSE);
    const falcon_submotion_data *guard_off =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_GUARD_OFF);
    const falcon_submotion_data *spot_dodge =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_SPOT_DODGE);
    const falcon_submotion_data *roll_forward =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_ROLL_FORWARD);
    const falcon_submotion_data *roll_backward =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD);
    const falcon_submotion_data *air_dodge =
        falcon_reference_submotion(PF_M4_FALCON_SUBMOTION_AIR_DODGE);
    const falcon_submotion_data *down_bound_back =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_DOWN_BOUND_BACK);
    const falcon_submotion_data *down_bound_stomach =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_DOWN_BOUND_STOMACH);
    const falcon_air_dodge_attributes *air_dodge_attributes =
        falcon_reference_air_dodge_attributes();
    const falcon_submotion_data *getup_neutral =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_NEUTRAL_BACK);
    const falcon_submotion_data *getup_attack =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_ATTACK_BACK);
    const falcon_submotion_data *getup_attack_stomach =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_ATTACK_STOMACH);
    const falcon_submotion_data *getup_roll_forward =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_BACK);
    const falcon_submotion_data *getup_roll_backward =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_BACK);
    const falcon_submotion_data *getup_roll_forward_stomach =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_STOMACH);
    const falcon_submotion_data *getup_roll_backward_stomach =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_STOMACH);
    const falcon_submotion_data *tech_in_place =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_TECH_IN_PLACE);
    const falcon_submotion_data *tech_roll_forward =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_TECH_ROLL_FORWARD);
    const falcon_submotion_data *tech_roll_backward =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_TECH_ROLL_BACKWARD);
    const falcon_submotion_data *wall_tech =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_WALL_TECH);
    const falcon_submotion_data *wall_tech_jump =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_WALL_TECH_JUMP);
    const falcon_submotion_data *ceiling_tech =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_CEILING_TECH);
    const falcon_submotion_data *shield_break_down_up =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_UP);
    const falcon_submotion_data *shield_break_down_down =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_DOWN_DOWN);
    const falcon_submotion_data *shield_break_stand_up =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_UP);
    const falcon_submotion_data *shield_break_stand_down =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_SHIELD_BREAK_STAND_DOWN);
    const falcon_submotion_data *teeter =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_TEETER);
    const falcon_submotion_data *teeter_wait =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_TEETER_WAIT);
    const falcon_submotion_data *catch_cut =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_CATCH_CUT);
    const falcon_submotion_data *capture_cut =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_CAPTURE_CUT);
    const falcon_submotion_data *appeal_right =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_APPEAL_RIGHT);
    const falcon_submotion_data *appeal_left =
        falcon_reference_submotion(
            PF_M4_FALCON_SUBMOTION_APPEAL_LEFT);
    const falcon_body_collision_timing *getup_neutral_back_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_GETUP_NEUTRAL_BACK);
    const falcon_body_collision_timing *
        getup_neutral_stomach_collision =
            falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_GETUP_NEUTRAL_STOMACH);
    const falcon_body_collision_timing *getup_attack_back_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_GETUP_ATTACK_BACK);
    const falcon_body_collision_timing *
        getup_attack_stomach_collision =
            falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_GETUP_ATTACK_STOMACH);
    const falcon_body_collision_timing *
        getup_roll_back_forward_collision =
            falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_BACK);
    const falcon_body_collision_timing *
        getup_roll_back_backward_collision =
            falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_BACK);
    const falcon_body_collision_timing *
        getup_roll_stomach_forward_collision =
            falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_STOMACH);
    const falcon_body_collision_timing *
        getup_roll_stomach_backward_collision =
            falcon_reference_body_collision_timing(
                PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_STOMACH);
    const falcon_body_collision_timing *tech_in_place_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_TECH_IN_PLACE);
    const falcon_body_collision_timing *tech_roll_forward_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_TECH_ROLL_FORWARD);
    const falcon_body_collision_timing *tech_roll_backward_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_TECH_ROLL_BACKWARD);
    const falcon_body_collision_timing *ceiling_tech_collision =
        falcon_reference_body_collision_timing(
            PF_M4_FALCON_SUBMOTION_CEILING_TECH);
    uint16_t spot_dodge_invulnerability_begin_tick;
    uint16_t spot_dodge_invulnerability_end_tick;
    uint16_t roll_forward_invulnerability_begin_tick;
    uint16_t roll_forward_invulnerability_end_tick;
    uint16_t roll_backward_invulnerability_begin_tick;
    uint16_t roll_backward_invulnerability_end_tick;
    uint16_t air_dodge_invulnerability_begin_tick;
    uint16_t air_dodge_invulnerability_end_tick;

    if (fighter == NULL || dash == NULL || turn == NULL || turn_run == NULL ||
        run_brake == NULL || landing == NULL || squat == NULL ||
        squat_reverse == NULL || guard_off == NULL || spot_dodge == NULL ||
        roll_forward == NULL || roll_backward == NULL || air_dodge == NULL ||
        down_bound_back == NULL || down_bound_stomach == NULL ||
        air_dodge_attributes == NULL ||
        getup_neutral == NULL || getup_attack == NULL ||
        getup_attack_stomach == NULL || getup_roll_forward == NULL ||
        getup_roll_backward == NULL || getup_roll_forward_stomach == NULL ||
        getup_roll_backward_stomach == NULL ||
        tech_in_place == NULL || tech_roll_forward == NULL ||
        tech_roll_backward == NULL || wall_tech == NULL ||
        wall_tech_jump == NULL || ceiling_tech == NULL ||
        shield_break_down_up == NULL ||
        shield_break_down_down == NULL ||
        shield_break_stand_up == NULL ||
        shield_break_stand_down == NULL ||
        teeter == NULL || teeter_wait == NULL || catch_cut == NULL ||
        capture_cut == NULL ||
        appeal_right == NULL ||
        appeal_left == NULL || getup_neutral_back_collision == NULL ||
        getup_neutral_stomach_collision == NULL ||
        getup_attack_back_collision == NULL ||
        getup_attack_stomach_collision == NULL ||
        getup_roll_back_forward_collision == NULL ||
        getup_roll_back_backward_collision == NULL ||
        getup_roll_stomach_forward_collision == NULL ||
        getup_roll_stomach_backward_collision == NULL ||
        tech_in_place_collision == NULL ||
        tech_roll_forward_collision == NULL ||
        tech_roll_backward_collision == NULL ||
        ceiling_tech_collision == NULL ||
        dash->animation_frame_count == UINT16_C(0) ||
        run_brake->animation_frame_count == UINT16_MAX ||
        appeal_right->animation_frame_count !=
        appeal_left->animation_frame_count ||
        air_dodge_attributes->ordinary_physics_begin_frame == UINT16_C(0) ||
        getup_roll_forward->gameplay_frame_count !=
            getup_roll_backward->gameplay_frame_count ||
        getup_roll_forward_stomach->gameplay_frame_count !=
            getup_roll_forward->gameplay_frame_count ||
        getup_roll_backward_stomach->gameplay_frame_count !=
            getup_roll_forward->gameplay_frame_count ||
        getup_attack_stomach->gameplay_frame_count !=
            getup_attack->gameplay_frame_count ||
        tech_roll_forward->animation_frame_count !=
            tech_roll_backward->animation_frame_count ||
        down_bound_back->animation_frame_count == UINT16_C(0) ||
        down_bound_back->animation_frame_count !=
            down_bound_stomach->animation_frame_count ||
        getup_neutral_back_collision->state_two_frame != UINT16_C(0) ||
        getup_neutral_back_collision->state_zero_frame != UINT16_C(23) ||
        getup_neutral_stomach_collision->state_two_frame != UINT16_C(0) ||
        getup_neutral_stomach_collision->state_zero_frame !=
            getup_neutral_back_collision->state_zero_frame ||
        getup_attack_back_collision->state_two_frame != UINT16_C(0) ||
        getup_attack_back_collision->state_zero_frame <= UINT16_C(1) ||
        getup_attack_stomach_collision->state_two_frame != UINT16_C(0) ||
        getup_attack_stomach_collision->state_zero_frame <= UINT16_C(1) ||
        getup_roll_back_forward_collision->state_two_frame != UINT16_C(0) ||
        getup_roll_back_forward_collision->state_zero_frame <= UINT16_C(1) ||
        getup_roll_back_backward_collision->state_two_frame != UINT16_C(0) ||
        getup_roll_back_backward_collision->state_zero_frame <= UINT16_C(1) ||
        getup_roll_stomach_forward_collision->state_two_frame != UINT16_C(0) ||
        getup_roll_stomach_forward_collision->state_zero_frame <= UINT16_C(1) ||
        getup_roll_stomach_backward_collision->state_two_frame != UINT16_C(0) ||
        getup_roll_stomach_backward_collision->state_zero_frame <= UINT16_C(1) ||
        tech_in_place_collision->state_two_frame != UINT16_C(0) ||
        tech_roll_forward_collision->state_two_frame != UINT16_C(0) ||
        tech_roll_backward_collision->state_two_frame != UINT16_C(0) ||
        tech_in_place_collision->state_zero_frame != UINT16_C(20) ||
        tech_roll_forward_collision->state_zero_frame !=
            tech_in_place_collision->state_zero_frame ||
        tech_roll_backward_collision->state_zero_frame !=
            tech_in_place_collision->state_zero_frame ||
        wall_tech->animation_frame_count == UINT16_C(0) ||
        wall_tech_jump->animation_frame_count == UINT16_C(0) ||
        ceiling_tech->animation_frame_count == UINT16_C(0) ||
        ceiling_tech_collision->state_two_frame != UINT16_C(0) ||
        ceiling_tech_collision->state_zero_frame == UINT16_MAX ||
        ceiling_tech_collision->state_zero_frame >=
            ceiling_tech->animation_frame_count ||
        shield_break_down_up->animation_frame_count == UINT16_C(0) ||
        shield_break_down_down->animation_frame_count !=
            shield_break_down_up->animation_frame_count ||
        shield_break_stand_up->animation_frame_count == UINT16_C(0) ||
        shield_break_stand_down->animation_frame_count !=
            shield_break_stand_up->animation_frame_count ||
        teeter->animation_frame_count == UINT16_C(0) ||
        teeter_wait->animation_frame_count == UINT16_C(0) ||
        catch_cut->animation_frame_count == UINT16_C(0) ||
        capture_cut->animation_frame_count == UINT16_C(0) ||
        !falcon_reference_body_collision_window(
            PF_M4_FALCON_SUBMOTION_SPOT_DODGE,
            UINT16_C(0),
            &spot_dodge_invulnerability_begin_tick,
            &spot_dodge_invulnerability_end_tick) ||
        !falcon_reference_body_collision_window(
            PF_M4_FALCON_SUBMOTION_ROLL_FORWARD,
            UINT16_C(0),
            &roll_forward_invulnerability_begin_tick,
            &roll_forward_invulnerability_end_tick) ||
        !falcon_reference_body_collision_window(
            PF_M4_FALCON_SUBMOTION_ROLL_BACKWARD,
            UINT16_C(0),
            &roll_backward_invulnerability_begin_tick,
            &roll_backward_invulnerability_end_tick) ||
        roll_forward_invulnerability_begin_tick !=
            roll_backward_invulnerability_begin_tick ||
        roll_forward_invulnerability_end_tick !=
            roll_backward_invulnerability_end_tick ||
        /* Dolphin exposes EscapeAir displayed frame 1 at M4 action tick 0;
         * ground escapes expose displayed frame 1 at action tick 1. */
        !falcon_reference_body_collision_window(
            PF_M4_FALCON_SUBMOTION_AIR_DODGE,
            UINT16_C(1),
            &air_dodge_invulnerability_begin_tick,
            &air_dodge_invulnerability_end_tick))
    {
        return 0;
    }

    /*
     * The catalog retains both the raw FigaTree endpoint count and the
     * extractor's last gameplay frame. Each state selects the representation
     * matching its already-qualified transition comparator; the two explicit
     * +1 counters account for entry being exposed as action tick 1.
     */
    fighter->initial_dash_ticks = dash->animation_frame_count;
    fighter->teeter_ticks = teeter->animation_frame_count;
    fighter->grab_release_ticks = catch_cut->animation_frame_count;
    fighter->standing_turn_ticks = turn->animation_frame_count;
    fighter->run_turnaround_ticks = turn_run->animation_frame_count;
    fighter->run_brake_ticks =
        (uint16_t)(run_brake->animation_frame_count + UINT16_C(1));
    fighter->landing_ticks = landing->animation_frame_count;
    fighter->crouch_start_ticks = squat->gameplay_frame_count;
    fighter->crouch_end_ticks = squat_reverse->animation_frame_count;
    fighter->shield_release_ticks = guard_off->animation_frame_count;
    fighter->spot_dodge_ticks = spot_dodge->gameplay_frame_count;
    fighter->forward_roll_ticks = roll_forward->gameplay_frame_count;
    fighter->backward_roll_ticks = roll_backward->gameplay_frame_count;
    fighter->air_dodge_ticks = air_dodge->gameplay_frame_count;
    fighter->shield_break_down_ticks =
        shield_break_down_up->animation_frame_count;
    fighter->shield_break_stand_ticks =
        shield_break_stand_up->animation_frame_count;
    fighter->air_dodge_invulnerability_begin_tick =
        air_dodge_invulnerability_begin_tick;
    fighter->air_dodge_invulnerability_end_tick =
        air_dodge_invulnerability_end_tick;
    fighter->air_dodge_ordinary_physics_begin_tick =
        (uint16_t)(
            air_dodge_attributes->ordinary_physics_begin_frame -
            UINT16_C(1));
    fighter->roll_invulnerability_begin_tick =
        roll_forward_invulnerability_begin_tick;
    fighter->roll_invulnerability_end_tick =
        roll_forward_invulnerability_end_tick;
    fighter->spot_dodge_invulnerability_begin_tick =
        spot_dodge_invulnerability_begin_tick;
    fighter->spot_dodge_invulnerability_end_tick =
        spot_dodge_invulnerability_end_tick;
    fighter->getup_neutral_ticks = getup_neutral->animation_frame_count;
    fighter->getup_attack_ticks = getup_attack->gameplay_frame_count;
    fighter->getup_roll_ticks = getup_roll_forward->gameplay_frame_count;
    fighter->getup_roll_back_forward.movement_begin_tick = UINT8_C(1);
    fighter->getup_roll_back_forward.invulnerability_begin_tick = UINT8_C(1);
    fighter->getup_roll_back_forward.invulnerability_end_tick = (uint8_t)(
        getup_roll_back_forward_collision->state_zero_frame - UINT16_C(1));
    fighter->getup_roll_back_backward.movement_begin_tick = UINT8_C(1);
    fighter->getup_roll_back_backward.invulnerability_begin_tick = UINT8_C(1);
    fighter->getup_roll_back_backward.invulnerability_end_tick = (uint8_t)(
        getup_roll_back_backward_collision->state_zero_frame - UINT16_C(1));
    fighter->getup_roll_stomach_forward.movement_begin_tick = UINT8_C(1);
    fighter->getup_roll_stomach_forward.invulnerability_begin_tick = UINT8_C(1);
    fighter->getup_roll_stomach_forward.invulnerability_end_tick = (uint8_t)(
        getup_roll_stomach_forward_collision->state_zero_frame - UINT16_C(1));
    fighter->getup_roll_stomach_backward.movement_begin_tick = UINT8_C(1);
    fighter->getup_roll_stomach_backward.invulnerability_begin_tick = UINT8_C(1);
    fighter->getup_roll_stomach_backward.invulnerability_end_tick = (uint8_t)(
        getup_roll_stomach_backward_collision->state_zero_frame - UINT16_C(1));
    fighter->getup_attack_back_invulnerability_ticks = (uint16_t)(
        getup_attack_back_collision->state_zero_frame - UINT16_C(1));
    fighter->getup_attack_stomach_invulnerability_ticks = (uint16_t)(
        getup_attack_stomach_collision->state_zero_frame - UINT16_C(1));
    fighter->tech_in_place_ticks = tech_in_place->animation_frame_count;
    fighter->tech_roll_ticks = tech_roll_forward->animation_frame_count;
    fighter->tech_invulnerability_ticks =
        tech_in_place_collision->state_zero_frame;
    fighter->wall_tech_ticks = (uint16_t)(
        fighter->wall_tech_stall_ticks + wall_tech->animation_frame_count);
    fighter->wall_tech_jump_ticks = (uint16_t)(
        fighter->wall_tech_stall_ticks +
        wall_tech_jump->animation_frame_count);
    fighter->ceiling_tech_control_tick =
        ceiling_tech_collision->state_zero_frame;
    fighter->ceiling_tech_ticks = ceiling_tech->animation_frame_count;
    fighter->knockdown_ticks = down_bound_back->animation_frame_count;
    fighter->getup_neutral_invulnerability_ticks =
        getup_neutral_back_collision->state_zero_frame;
    fighter->taunt_ticks =
        (uint16_t)(appeal_right->animation_frame_count + UINT16_C(1));
    return 1;
}

static int apply_falcon_reference_attack(
    attack_data *attack,
    falcon_move_index move_index)
{
    const reference_hit_effect *effect =
        falcon_reference_primary_effect(move_index);
    const reference_timing timing =
        falcon_reference_timing(move_index);

    if (attack == NULL || effect == NULL ||
        timing.active_ticks == UINT16_C(0))
    {
        return 0;
    }
    attack->damage_f32 = (float)effect->damage;
    attack->startup_ticks = timing.startup_ticks;
    attack->active_ticks = timing.active_ticks;
    attack->recovery_ticks = timing.recovery_ticks;
    attack->hitlag_ticks =
        falcon_reference_hitlag_ticks(effect->damage);
    return 1;
}

static void apply_falcon_reference_knockback(
    melee_knockback_data *knockback,
    const reference_hit_effect *effect)
{
    knockback->angle_degrees = effect->angle_degrees;
    knockback->growth = effect->growth;
    knockback->weight_set = effect->weight_set;
    knockback->base = effect->base;
    knockback->enabled = UINT8_C(1);
}

static int apply_falcon_reference_throw(
    struct throw_data *throw_data,
    falcon_move_index move_index)
{
    const falcon_common_attributes *attributes =
        falcon_reference_common_attributes();
    const ssbm_ground_input_attributes *common =
        ssbm_common_reference_ground_input();
    const struct reference_move *move =
        falcon_reference_move(move_index);
    const reference_throw *effect =
        falcon_reference_throw(move_index);
    uint32_t release_tick;
    uint32_t total_ticks;

    if (throw_data == NULL || attributes == NULL || common == NULL ||
        move == NULL || effect == NULL ||
        effect->release_frame == UINT16_C(0) ||
        effect->release_frame >= move->total_frames)
    {
        return 0;
    }
    release_tick = effect->release_frame;
    total_ticks = move->total_frames;
    {
        const uint32_t throw_index =
            (uint32_t)move_index -
            (uint32_t)PF_M4_FALCON_FORWARD_THROW;

        if (throw_index >= UINT32_C(4))
        {
            return 0;
        }
        if ((attributes->weight_independent_throws_mask &
             (uint8_t)(UINT8_C(1) << throw_index)) == UINT8_C(0))
        {
            release_tick = (uint32_t)ssbm_throw_animation_ticks(
                (uint16_t)release_tick,
                attributes->weight,
                0);
            total_ticks = (uint32_t)ssbm_throw_animation_ticks(
                (uint16_t)total_ticks,
                attributes->weight,
                0);
        }
    }
    /* Throw/Thrown enter at animation frame zero and immediately run
     * ftAnim_8006EBA4. Public action_ticks starts at zero on that already-
     * advanced row, so script-event and terminal ticks are one less than the
     * number of animation advances needed from frame zero. */
    if (release_tick == UINT32_C(0) || total_ticks == UINT32_C(0))
    {
        return 0;
    }
    --release_tick;
    --total_ticks;
    if (release_tick == UINT32_C(0) || release_tick >= total_ticks ||
        total_ticks > UINT16_MAX)
    {
        return 0;
    }
    throw_data->damage_f32 =
            (float)effect->damage;
    throw_data->base_velocity_x_f32 = INT32_C(0);
    throw_data->base_velocity_y_f32 = INT32_C(0);
    throw_data->velocity_growth_x_f32 = INT32_C(0);
    throw_data->velocity_growth_y_f32 = INT32_C(0);
    throw_data->release_tick = (uint16_t)release_tick;
    throw_data->recovery_ticks =
        (uint16_t)(total_ticks - release_tick);
    /* SSBM's throw absolute-damage command adds no release hitlag. Any
     * synchronized freeze comes from a separate ordinary throw hitbox. */
    throw_data->hitlag_ticks = UINT16_C(0);
    throw_data->reserved = UINT16_C(0);
    throw_data->melee_knockback.angle_degrees = effect->angle_degrees;
    throw_data->melee_knockback.growth = effect->growth;
    throw_data->melee_knockback.weight_set = effect->weight_set;
    throw_data->melee_knockback.base = effect->base;
    throw_data->melee_knockback.enabled = UINT8_C(1);
    (void)memset(
        throw_data->melee_knockback.reserved,
        0,
        sizeof(throw_data->melee_knockback.reserved));
    return 1;
}

static int apply_falcon_reference_grabs(
    fighter_data *fighter)
{
    const reference_timing standing =
        falcon_reference_timing(PF_M4_FALCON_GRAB);
    const reference_timing dash =
        falcon_reference_timing(PF_M4_FALCON_DASH_GRAB);

    if (fighter == NULL || standing.startup_ticks == UINT16_C(0) ||
        standing.active_ticks == UINT16_C(0) ||
        standing.recovery_ticks == UINT16_C(0) ||
        dash.startup_ticks == UINT16_C(0) ||
        dash.active_ticks == UINT16_C(0) ||
        dash.recovery_ticks == UINT16_C(0))
    {
        return 0;
    }
    fighter->grab_startup_ticks = standing.startup_ticks;
    fighter->grab_active_ticks = standing.active_ticks;
    fighter->grab_recovery_ticks = standing.recovery_ticks;
    fighter->dash_grab_startup_ticks = dash.startup_ticks;
    fighter->dash_grab_active_ticks = dash.active_ticks;
    fighter->dash_grab_recovery_ticks = dash.recovery_ticks;
    return 1;
}

static int apply_falcon_reference_jab(
    fighter_data *fighter)
{
    const reference_hit_effect *jab_effect =
        falcon_reference_primary_effect(PF_M4_FALCON_JAB1);
    const reference_hit_effect *jab_final_effect =
        falcon_reference_primary_effect(PF_M4_FALCON_JAB2);
    attack_data jab = {0};
    attack_data jab_final = {0};

    if (!apply_falcon_reference_attack(
        &jab,
        PF_M4_FALCON_JAB1) ||
        !apply_falcon_reference_attack(
        &jab_final,
        PF_M4_FALCON_JAB2) ||
        jab_effect == NULL || jab_final_effect == NULL)
    {
        return 0;
    }
    fighter->jab_damage_f32 = jab.damage_f32;
    fighter->jab_startup_ticks = jab.startup_ticks;
    fighter->jab_active_ticks = jab.active_ticks;
    fighter->jab_recovery_ticks = jab.recovery_ticks;
    fighter->jab_hitlag_ticks = jab.hitlag_ticks;
    fighter->jab_combo_input_begin_tick =
        jab.startup_ticks + jab.active_ticks;
    apply_falcon_reference_knockback(
        &fighter->jab_melee_knockback,
        jab_effect);
    fighter->jab_final_damage_f32 = jab_final.damage_f32;
    fighter->jab_final_startup_ticks = jab_final.startup_ticks;
    fighter->jab_final_active_ticks = jab_final.active_ticks;
    fighter->jab_final_recovery_ticks = jab_final.recovery_ticks;
    fighter->jab_final_hitlag_ticks = jab_final.hitlag_ticks;
    apply_falcon_reference_knockback(
        &fighter->jab_final_melee_knockback,
        jab_final_effect);
    return 1;
}

static int apply_falcon_reference_defaults(
    fighter_data *fighter)
{
    const struct reference_move *neutral_aerial =
        falcon_reference_move(PF_M4_FALCON_NEUTRAL_AERIAL);
    const struct reference_move *forward_aerial =
        falcon_reference_move(PF_M4_FALCON_FORWARD_AERIAL);
    const struct reference_move *back_aerial =
        falcon_reference_move(PF_M4_FALCON_BACK_AERIAL);
    const struct reference_move *up_aerial =
        falcon_reference_move(PF_M4_FALCON_UP_AERIAL);
    const struct reference_move *down_aerial =
        falcon_reference_move(PF_M4_FALCON_DOWN_AERIAL);
    const struct reference_move *pummel =
        falcon_reference_move(PF_M4_FALCON_PUMMEL);
    attack_data dash_attack = {0};
    attack_data neutral_aerial_attack = {0};

    if (fighter == NULL || neutral_aerial == NULL ||
        forward_aerial == NULL || back_aerial == NULL ||
        up_aerial == NULL || down_aerial == NULL || pummel == NULL ||
        !apply_falcon_reference_jab(fighter) ||
        !apply_falcon_reference_attack(
            &dash_attack,
            PF_M4_FALCON_DASH_ATTACK) ||
        !apply_falcon_reference_attack(
            &fighter->up_attack,
            PF_M4_FALCON_UP_TILT) ||
        !apply_falcon_reference_attack(
            &fighter->down_attack,
            PF_M4_FALCON_DOWN_TILT) ||
        !apply_falcon_reference_attack(
            &fighter->forward_attack,
            PF_M4_FALCON_FORWARD_TILT) ||
        !apply_falcon_reference_attack(
            &fighter->forward_strong_attack,
            PF_M4_FALCON_FORWARD_SMASH) ||
        !apply_falcon_reference_attack(
            &fighter->up_strong_attack,
            PF_M4_FALCON_UP_SMASH) ||
        !apply_falcon_reference_attack(
            &fighter->down_strong_attack,
            PF_M4_FALCON_DOWN_SMASH) ||
        !apply_falcon_reference_attack(
            &fighter->forward_aerial,
            PF_M4_FALCON_FORWARD_AERIAL) ||
        !apply_falcon_reference_attack(
            &fighter->back_aerial,
            PF_M4_FALCON_BACK_AERIAL) ||
        !apply_falcon_reference_attack(
            &fighter->up_aerial,
            PF_M4_FALCON_UP_AERIAL) ||
        !apply_falcon_reference_attack(
            &fighter->down_aerial,
            PF_M4_FALCON_DOWN_AERIAL) ||
        !apply_falcon_reference_attack(
            &neutral_aerial_attack,
            PF_M4_FALCON_NEUTRAL_AERIAL))
    {
        return 0;
    }

    fighter->dash_attack_damage_f32 = dash_attack.damage_f32;
    fighter->dash_attack_startup_ticks = dash_attack.startup_ticks;
    fighter->dash_attack_active_ticks = dash_attack.active_ticks;
    fighter->dash_attack_recovery_ticks = dash_attack.recovery_ticks;
    fighter->dash_attack_hitlag_ticks = dash_attack.hitlag_ticks;

    fighter->aerial_damage_f32 = neutral_aerial_attack.damage_f32;
    fighter->aerial_startup_ticks = neutral_aerial_attack.startup_ticks;
    fighter->aerial_active_ticks = neutral_aerial_attack.active_ticks;
    fighter->aerial_recovery_ticks = neutral_aerial_attack.recovery_ticks;
    fighter->aerial_hitlag_ticks = neutral_aerial_attack.hitlag_ticks;

    fighter->aerial_landing_lag_ticks = neutral_aerial->landing_lag;
    fighter->forward_aerial_landing_lag_ticks =
        forward_aerial->landing_lag;
    fighter->back_aerial_landing_lag_ticks = back_aerial->landing_lag;
    fighter->up_aerial_landing_lag_ticks = up_aerial->landing_lag;
    fighter->down_aerial_landing_lag_ticks = down_aerial->landing_lag;

    {
        const reference_hit_effect *pummel_effect =
            falcon_reference_primary_effect(PF_M4_FALCON_PUMMEL);
        const reference_hit_phase *pummel_phase =
            falcon_reference_phase(
                PF_M4_FALCON_PUMMEL,
                UINT16_C(0));

        if (pummel_effect == NULL || pummel_phase == NULL)
        {
            return 0;
        }
        fighter->pummel_damage_f32 =
            (float)pummel_effect->damage;
        fighter->pummel_hit_tick = pummel_phase->first_frame;
        fighter->pummel_total_ticks = pummel->total_frames;
        falcon_reference_capture_offset_f32(
            &fighter->grabbed_offset_x_f32,
            &fighter->grabbed_offset_y_f32);
    }
    return 1;
}

static void hash_u8(pf_sha256 *hash, uint8_t value)
{
    pf_sha256_update(hash, &value, sizeof(value));
}

static void hash_u16_value(pf_sha256 *hash, uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    pf_sha256_update(hash, bytes, sizeof(bytes));
}

static void hash_u32_value(pf_sha256 *hash, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
    pf_sha256_update(hash, bytes, sizeof(bytes));
}

static void hash_i32_value(pf_sha256 *hash, int32_t value)
{
    hash_u32_value(hash, (uint32_t)value);
}

static void hash_f32_value(pf_sha256 *hash, float value)
{
    uint32_t bits;

    (void)memcpy(&bits, &value, sizeof(bits));
    hash_u32_value(hash, bits);
}

#define hash_u16(hash, value)                                             \
    _Generic((value), float: hash_f32_value, default: hash_u16_value)(    \
        (hash), (value))
#define hash_u32(hash, value)                                             \
    _Generic((value), float: hash_f32_value, default: hash_u32_value)(    \
        (hash), (value))
#define hash_i32(hash, value)                                             \
    _Generic((value), float: hash_f32_value, default: hash_i32_value)(    \
        (hash), (value))

static void hash_getup_roll_timing(
    pf_sha256 *hash,
    const getup_roll_timing *timing)
{
    hash_u8(hash, timing->movement_begin_tick);
    hash_u8(hash, timing->invulnerability_begin_tick);
    hash_u8(hash, timing->invulnerability_end_tick);
    hash_u8(hash, timing->reserved);
}

static int getup_roll_timing_is_valid(
    const getup_roll_timing *timing,
    uint16_t total_ticks)
{
    return timing->movement_begin_tick != UINT16_C(0) &&
           timing->movement_begin_tick <= total_ticks &&
           timing->invulnerability_begin_tick != UINT16_C(0) &&
           timing->invulnerability_begin_tick <=
               timing->invulnerability_end_tick &&
           timing->invulnerability_end_tick <= total_ticks &&
           timing->reserved == UINT16_C(0);
}

static int velocity_animation_scaling_is_valid(float scaling_f32)
{
    return isfinite(scaling_f32) && scaling_f32 > 0.0f;
}

static void hash_throw(
    pf_sha256 *hash,
    const struct throw_data *throw_data)
{
    hash_u32(hash, throw_data->damage_f32);
    hash_i32(hash, throw_data->base_velocity_x_f32);
    hash_i32(hash, throw_data->base_velocity_y_f32);
    hash_i32(hash, throw_data->velocity_growth_x_f32);
    hash_i32(hash, throw_data->velocity_growth_y_f32);
    hash_u16(hash, throw_data->release_tick);
    hash_u16(hash, throw_data->recovery_ticks);
    hash_u16(hash, throw_data->hitlag_ticks);
    hash_u16(hash, throw_data->reserved);
    hash_u16(
        hash, throw_data->melee_knockback.angle_degrees);
    hash_u16(hash, throw_data->melee_knockback.growth);
    hash_u16(hash, throw_data->melee_knockback.weight_set);
    hash_u16(hash, throw_data->melee_knockback.base);
    hash_u8(hash, throw_data->melee_knockback.enabled);
    hash_u8(hash, throw_data->melee_knockback.reserved[0]);
    hash_u8(hash, throw_data->melee_knockback.reserved[1]);
    hash_u8(hash, throw_data->melee_knockback.reserved[2]);
}

static void hash_attack(
    pf_sha256 *hash,
    const attack_data *attack)
{
    hash_i32(hash, attack->hitbox_offset_x_f32);
    hash_i32(hash, attack->hitbox_offset_y_f32);
    hash_i32(hash, attack->hitbox_half_width_f32);
    hash_i32(hash, attack->hitbox_half_height_f32);
    hash_u32(hash, attack->damage_f32);
    hash_i32(hash, attack->base_knockback_x_f32);
    hash_i32(hash, attack->base_knockback_y_f32);
    hash_i32(hash, attack->knockback_growth_f32);
    hash_u16(hash, attack->startup_ticks);
    hash_u16(hash, attack->active_ticks);
    hash_u16(hash, attack->recovery_ticks);
    hash_u16(hash, attack->hitlag_ticks);
}

static int attack_data_is_valid(
    const attack_data *attack,
    float maximum_extent_f32)
{
    const float maximum_knockback_x =
        attack->base_knockback_x_f32 +
        attack->knockback_growth_f32 * PF_SIM_MAX_DAMAGE_F32;
    const float maximum_knockback_y =
        attack->base_knockback_y_f32 +
        attack->knockback_growth_f32 * PF_SIM_MAX_DAMAGE_F32 * 0.5f;

    return attack->hitbox_offset_x_f32 >= -maximum_extent_f32 &&
           attack->hitbox_offset_x_f32 <= maximum_extent_f32 &&
           attack->hitbox_offset_y_f32 >= -maximum_extent_f32 &&
           attack->hitbox_offset_y_f32 <= maximum_extent_f32 &&
           attack->hitbox_half_width_f32 > INT32_C(0) &&
           attack->hitbox_half_width_f32 <= maximum_extent_f32 &&
           attack->hitbox_half_height_f32 > INT32_C(0) &&
           attack->hitbox_half_height_f32 <= maximum_extent_f32 &&
           attack->damage_f32 > 0.0f &&
           attack->damage_f32 <= 50.0f &&
           attack->base_knockback_x_f32 > INT32_C(0) &&
           attack->base_knockback_y_f32 > INT32_C(0) &&
           attack->knockback_growth_f32 > INT32_C(0) &&
           maximum_knockback_x <=
               PF_SIM_MAX_MOTION_SPEED_F32 &&
           maximum_knockback_y <=
               PF_SIM_MAX_MOTION_SPEED_F32 &&
           attack->startup_ticks != UINT16_C(0) &&
           attack->startup_ticks <= UINT16_C(120) &&
           attack->active_ticks != UINT16_C(0) &&
           attack->active_ticks <= UINT16_C(120) &&
           attack->recovery_ticks != UINT16_C(0) &&
           attack->recovery_ticks <= UINT16_C(240) &&
           attack->hitlag_ticks != UINT16_C(0) &&
           attack->hitlag_ticks <= UINT16_C(120) &&
           (uint32_t)attack->startup_ticks +
                   (uint32_t)attack->active_ticks +
                   (uint32_t)attack->recovery_ticks <=
               UINT32_C(600);
}

static int charged_attack_damage_is_valid(
    const attack_data *attack,
    float bonus_f32)
{
    const float charged_damage = attack->damage_f32 * (1.0f + bonus_f32);

    return isfinite(charged_damage) && charged_damage <= 50.0f;
}

static float maximum_knockback_f32(float base, float growth, int vertical)
{
    return base + growth * PF_SIM_MAX_DAMAGE_F32 *
                      (vertical != 0 ? 0.5f : 1.0f);
}

static int throw_data_is_valid(
    const struct throw_data *throw_data)
{
    const float maximum_velocity_x =
        throw_data->base_velocity_x_f32 +
        throw_data->velocity_growth_x_f32 * PF_SIM_MAX_DAMAGE_F32;
    const float maximum_velocity_y =
        throw_data->base_velocity_y_f32 +
        throw_data->velocity_growth_y_f32 * PF_SIM_MAX_DAMAGE_F32;

    const int semantic =
        throw_data->melee_knockback.enabled != UINT8_C(0);
    const int semantic_is_valid =
        throw_data->melee_knockback.enabled <= UINT8_C(1) &&
        throw_data->melee_knockback.reserved[0] == UINT8_C(0) &&
        throw_data->melee_knockback.reserved[1] == UINT8_C(0) &&
        throw_data->melee_knockback.reserved[2] == UINT8_C(0) &&
        ((semantic != 0 &&
          throw_data->melee_knockback.angle_degrees <= UINT16_C(361) &&
          throw_data->melee_knockback.growth != UINT16_C(0) &&
          throw_data->melee_knockback.growth <= UINT16_C(1000) &&
          throw_data->melee_knockback.weight_set <= UINT16_C(1000) &&
          throw_data->melee_knockback.base <= UINT16_C(1000)) ||
         (semantic == 0 &&
          throw_data->melee_knockback.angle_degrees == UINT16_C(0) &&
          throw_data->melee_knockback.growth == UINT16_C(0) &&
          throw_data->melee_knockback.weight_set == UINT16_C(0) &&
          throw_data->melee_knockback.base == UINT16_C(0)));
    const int vector_is_valid =
        (throw_data->base_velocity_x_f32 != INT32_C(0) ||
         throw_data->base_velocity_y_f32 != INT32_C(0)) &&
        !((throw_data->base_velocity_x_f32 < INT32_C(0) &&
           throw_data->velocity_growth_x_f32 > INT32_C(0)) ||
          (throw_data->base_velocity_x_f32 > INT32_C(0) &&
           throw_data->velocity_growth_x_f32 < INT32_C(0)) ||
          (throw_data->base_velocity_y_f32 < INT32_C(0) &&
           throw_data->velocity_growth_y_f32 > INT32_C(0)) ||
          (throw_data->base_velocity_y_f32 > INT32_C(0) &&
           throw_data->velocity_growth_y_f32 < INT32_C(0))) &&
        maximum_velocity_x >=
            -PF_SIM_MAX_MOTION_SPEED_F32 &&
        maximum_velocity_x <=
            PF_SIM_MAX_MOTION_SPEED_F32 &&
        maximum_velocity_y >=
            -PF_SIM_MAX_MOTION_SPEED_F32 &&
        maximum_velocity_y <=
            PF_SIM_MAX_MOTION_SPEED_F32;

    return throw_data->damage_f32 > 0.0f &&
           throw_data->damage_f32 <= 50.0f &&
           semantic_is_valid != 0 &&
           ((semantic != 0 &&
             throw_data->base_velocity_x_f32 == INT32_C(0) &&
             throw_data->base_velocity_y_f32 == INT32_C(0) &&
             throw_data->velocity_growth_x_f32 == INT32_C(0) &&
             throw_data->velocity_growth_y_f32 == INT32_C(0) &&
             throw_data->hitlag_ticks == UINT16_C(0)) ||
            (semantic == 0 && vector_is_valid != 0)) &&
           throw_data->release_tick != UINT16_C(0) &&
           throw_data->release_tick <= UINT16_C(120) &&
           throw_data->recovery_ticks != UINT16_C(0) &&
           throw_data->recovery_ticks <= UINT16_C(240) &&
           (uint32_t)throw_data->release_tick +
                   (uint32_t)throw_data->recovery_ticks <=
               UINT32_C(600) &&
           ((semantic != 0 &&
             throw_data->hitlag_ticks == UINT16_C(0)) ||
            (semantic == 0 &&
             throw_data->hitlag_ticks != UINT16_C(0) &&
             throw_data->hitlag_ticks <= UINT16_C(120))) &&
           throw_data->reserved == UINT16_C(0);
}

static void hash_fighter(
    pf_sha256 *hash,
    const fighter_data *fighter)
{
    pf_sha256_update(
        hash,
        falcon_reference_complete_source_sha256(),
        (size_t)32);
    pf_sha256_update(
        hash,
        falcon_reference_geometry_sha256(),
        (size_t)32);
    uint32_t stale_index;

    hash_u16(hash, fighter->schema_version);
    hash_u8(hash, fighter->reference_frame_data_enabled);
    hash_i32(hash, fighter->half_width_f32);
    hash_i32(hash, fighter->half_height_f32);
    hash_i32(hash, fighter->player_push_half_width_f32);
    hash_i32(hash, fighter->player_push_speed_f32);
    hash_i32(hash, fighter->weight_f32);
    hash_i32(hash, fighter->ground_acceleration_f32);
    hash_i32(hash, fighter->turn_acceleration_f32);
    hash_i32(hash, fighter->traction_f32);
    hash_i32(hash, fighter->walk_speed_f32);
    hash_i32(hash, fighter->run_speed_f32);
    hash_i32(hash, fighter->initial_dash_speed_f32);
    hash_i32(hash, fighter->walk_initial_velocity_f32);
    hash_i32(hash, fighter->walk_acceleration_f32);
    hash_i32(hash, fighter->slow_walk_animation_scaling_f32);
    hash_i32(hash, fighter->middle_walk_animation_scaling_f32);
    hash_i32(hash, fighter->fast_walk_animation_scaling_f32);
    hash_i32(hash, fighter->run_animation_scaling_f32);
    hash_u16(hash, fighter->walk_middle_speed_ratio_f32);
    hash_u16(hash, fighter->walk_fast_speed_ratio_f32);
    hash_i32(hash, fighter->dash_run_base_acceleration_f32);
    hash_i32(hash, fighter->ground_max_horizontal_speed_f32);
    hash_i32(hash, fighter->walk_acceleration_taper_f32);
    hash_i32(hash, fighter->run_acceleration_taper_f32);
    hash_i32(hash, fighter->teeter_snap_distance_f32);
    hash_i32(hash, fighter->crouch_step_speed_f32);
    hash_i32(hash, fighter->air_acceleration_f32);
    hash_i32(hash, fighter->air_base_acceleration_f32);
    hash_i32(hash, fighter->air_friction_f32);
    hash_i32(hash, fighter->air_max_horizontal_speed_f32);
    hash_i32(hash, fighter->air_speed_f32);
    hash_i32(hash, fighter->jump_horizontal_input_speed_f32);
    hash_i32(
        hash,
        fighter->jump_horizontal_momentum_multiplier_f32);
    hash_i32(hash, fighter->jump_horizontal_max_speed_f32);
    hash_i32(hash, fighter->gravity_f32);
    hash_i32(hash, fighter->fall_speed_f32);
    hash_i32(hash, fighter->fast_fall_speed_f32);
    hash_i32(hash, fighter->full_hop_speed_f32);
    hash_i32(hash, fighter->short_hop_speed_f32);
    hash_i32(hash, fighter->double_jump_speed_f32);
    hash_i32(hash, fighter->double_jump_horizontal_speed_f32);
    hash_i32(hash, fighter->platform_drop_nudge_f32);
    hash_i32(hash, fighter->platform_drop_speed_y_f32);
    hash_i32(hash, fighter->ledge_jump_speed_x_f32);
    hash_i32(hash, fighter->ledge_jump_speed_y_f32);
    hash_i32(hash, fighter->ledge_roll_distance_f32);
    hash_i32(hash, fighter->drop_cancel_snap_distance_f32);
    hash_i32(hash, fighter->air_dodge_speed_x_f32);
    hash_i32(hash, fighter->air_dodge_speed_y_f32);
    hash_i32(hash, fighter->air_dodge_decay_f32);
    hash_i32(hash, fighter->fall_special_mobility_f32);
    hash_i32(
        hash,
        fighter->shield_break_launch_speed_f32);
    hash_i32(hash, fighter->dash_attack_speed_f32);
    hash_i32(
        hash,
        fighter->dash_attack_hitbox_offset_x_f32);
    hash_i32(
        hash,
        fighter->dash_attack_hitbox_offset_y_f32);
    hash_i32(
        hash,
        fighter->dash_attack_hitbox_half_width_f32);
    hash_i32(
        hash,
        fighter->dash_attack_hitbox_half_height_f32);
    hash_u32(hash, fighter->dash_attack_damage_f32);
    hash_i32(
        hash,
        fighter->dash_attack_base_knockback_x_f32);
    hash_i32(
        hash,
        fighter->dash_attack_base_knockback_y_f32);
    hash_i32(
        hash,
        fighter->dash_attack_knockback_growth_f32);
    hash_i32(hash, fighter->jab_hitbox_offset_x_f32);
    hash_i32(hash, fighter->jab_hitbox_offset_y_f32);
    hash_i32(hash, fighter->jab_hitbox_half_width_f32);
    hash_i32(hash, fighter->jab_hitbox_half_height_f32);
    hash_u32(hash, fighter->jab_damage_f32);
    hash_i32(hash, fighter->jab_base_knockback_x_f32);
    hash_i32(hash, fighter->jab_base_knockback_y_f32);
    hash_i32(hash, fighter->jab_knockback_growth_f32);
    hash_u16(hash, fighter->jab_melee_knockback.angle_degrees);
    hash_u16(hash, fighter->jab_melee_knockback.growth);
    hash_u16(hash, fighter->jab_melee_knockback.weight_set);
    hash_u16(hash, fighter->jab_melee_knockback.base);
    hash_u8(hash, fighter->jab_melee_knockback.enabled);
    hash_u8(hash, fighter->jab_melee_knockback.reserved[0]);
    hash_u8(hash, fighter->jab_melee_knockback.reserved[1]);
    hash_u8(hash, fighter->jab_melee_knockback.reserved[2]);
    hash_i32(hash, fighter->jab_final_hitbox_offset_x_f32);
    hash_i32(hash, fighter->jab_final_hitbox_offset_y_f32);
    hash_i32(hash, fighter->jab_final_hitbox_half_width_f32);
    hash_i32(hash, fighter->jab_final_hitbox_half_height_f32);
    hash_u32(hash, fighter->jab_final_damage_f32);
    hash_i32(hash, fighter->jab_final_base_knockback_x_f32);
    hash_i32(hash, fighter->jab_final_base_knockback_y_f32);
    hash_i32(hash, fighter->jab_final_knockback_growth_f32);
    hash_u16(hash, fighter->jab_final_melee_knockback.angle_degrees);
    hash_u16(hash, fighter->jab_final_melee_knockback.growth);
    hash_u16(hash, fighter->jab_final_melee_knockback.weight_set);
    hash_u16(hash, fighter->jab_final_melee_knockback.base);
    hash_u8(hash, fighter->jab_final_melee_knockback.enabled);
    hash_u8(hash, fighter->jab_final_melee_knockback.reserved[0]);
    hash_u8(hash, fighter->jab_final_melee_knockback.reserved[1]);
    hash_u8(hash, fighter->jab_final_melee_knockback.reserved[2]);
    hash_attack(hash, &fighter->up_attack);
    hash_attack(hash, &fighter->down_attack);
    hash_attack(hash, &fighter->forward_attack);
    hash_attack(hash, &fighter->forward_strong_attack);
    hash_attack(hash, &fighter->up_strong_attack);
    hash_attack(hash, &fighter->down_strong_attack);
    hash_u32(hash, fighter->smash_charge_damage_bonus_f32);
    hash_u16(hash, fighter->smash_charge_max_ticks);
    hash_u16(hash, fighter->smash_charge_reserved);
    hash_attack(hash, &fighter->forward_aerial);
    hash_attack(hash, &fighter->back_aerial);
    hash_attack(hash, &fighter->up_aerial);
    hash_attack(hash, &fighter->down_aerial);
    hash_attack(hash, &fighter->ledge_attack);
    hash_u32(hash, fighter->reset_max_damage_f32);
    hash_i32(hash, fighter->reset_bound_speed_f32);
    hash_i32(hash, fighter->strong_hitbox_offset_x_f32);
    hash_i32(hash, fighter->strong_hitbox_offset_y_f32);
    hash_i32(hash, fighter->strong_hitbox_half_width_f32);
    hash_i32(hash, fighter->strong_hitbox_half_height_f32);
    hash_u32(hash, fighter->strong_damage_f32);
    hash_i32(hash, fighter->strong_base_knockback_x_f32);
    hash_i32(hash, fighter->strong_base_knockback_y_f32);
    hash_i32(hash, fighter->strong_knockback_growth_f32);
    hash_i32(hash, fighter->aerial_hitbox_offset_x_f32);
    hash_i32(hash, fighter->aerial_hitbox_offset_y_f32);
    hash_i32(hash, fighter->aerial_hitbox_half_width_f32);
    hash_i32(hash, fighter->aerial_hitbox_half_height_f32);
    hash_u32(hash, fighter->aerial_damage_f32);
    hash_i32(hash, fighter->aerial_base_knockback_x_f32);
    hash_i32(hash, fighter->aerial_base_knockback_y_f32);
    hash_i32(hash, fighter->aerial_knockback_growth_f32);
    hash_i32(hash, fighter->hitstun_velocity_per_tick_f32);
    hash_i32(hash, fighter->v_cancel_velocity_scale_f32);
    hash_u16(hash, fighter->knockback_weight);
    hash_u16(hash, fighter->knockback_reserved);
    hash_u32(hash, fighter->crouch_cancel_max_damage_f32);
    hash_i32(hash, fighter->crouch_cancel_velocity_scale_f32);
    hash_i32(hash, fighter->crouch_cancel_hitstun_scale_f32);
    hash_i32(hash, fighter->di_max_angle_radians_q30);
    hash_i32(hash, fighter->ground_knockback_decay_scale_f32);
    hash_i32(hash, fighter->air_knockback_decay_f32);
    hash_i32(hash, fighter->sdi_distance_x_f32);
    hash_i32(hash, fighter->sdi_distance_y_f32);
    hash_i32(hash, fighter->asdi_distance_x_f32);
    hash_i32(hash, fighter->asdi_distance_y_f32);
    hash_i32(hash, fighter->shield_sdi_scale_f32);
    hash_i32(hash, fighter->tech_roll_speed_f32);
    hash_i32(hash, fighter->wall_tech_speed_f32);
    hash_i32(hash, fighter->wall_tech_jump_speed_x_f32);
    hash_i32(hash, fighter->wall_tech_jump_speed_y_f32);
    hash_i32(hash, fighter->wall_jump_speed_x_f32);
    hash_i32(hash, fighter->wall_jump_speed_y_f32);
    hash_i32(hash, fighter->ceiling_tech_speed_f32);
    hash_i32(hash, fighter->surface_collision_threshold_x_f32);
    hash_i32(hash, fighter->surface_collision_threshold_y_f32);
    hash_i32(hash, fighter->surface_bounce_multiplier_f32);
    hash_i32(hash, fighter->forward_roll_speed_f32);
    hash_i32(hash, fighter->backward_roll_speed_f32);
    hash_i32(
        hash,
        fighter->getup_attack_hitbox_offset_x_f32);
    hash_i32(
        hash,
        fighter->getup_attack_hitbox_offset_y_f32);
    hash_i32(
        hash,
        fighter->getup_attack_hitbox_half_width_f32);
    hash_i32(
        hash,
        fighter->getup_attack_hitbox_half_height_f32);
    hash_u32(hash, fighter->getup_attack_damage_f32);
    hash_i32(
        hash,
        fighter->getup_attack_base_knockback_x_f32);
    hash_i32(
        hash,
        fighter->getup_attack_base_knockback_y_f32);
    hash_i32(
        hash,
        fighter->getup_attack_knockback_growth_f32);
    hash_u32(hash, fighter->shield_health_f32);
    hash_u32(hash, fighter->shield_reset_health_f32);
    hash_u32(hash, fighter->shield_hold_depletion_f32);
    hash_u32(
        hash,
        fighter->light_shield_hold_depletion_f32);
    hash_u32(hash, fighter->shield_regeneration_f32);
    hash_u32(
        hash,
        fighter->light_shield_damage_multiplier_f32);
    hash_u32(
        hash,
        fighter->dense_shield_damage_multiplier_f32);
    hash_i32(
        hash,
        fighter->light_shield_stun_damage_multiplier_f32);
    hash_i32(
        hash,
        fighter->dense_shield_stun_damage_multiplier_f32);
    hash_i32(hash, fighter->shield_stun_base_f32);
    hash_i32(
        hash,
        fighter->shield_defender_pushback_stun_scale_f32);
    hash_i32(
        hash,
        fighter->shield_defender_pushback_normal_scale_f32);
    hash_i32(
        hash,
        fighter->shield_defender_pushback_max_f32);
    hash_i32(
        hash,
        fighter->shield_attacker_pushback_damage_f32);
    hash_i32(
        hash,
        fighter->shield_attacker_pushback_base_f32);
    hash_i32(
        hash,
        fighter->shield_attacker_pushback_air_decay_f32);
    hash_i32(
        hash,
        fighter->shield_attacker_pushback_ground_friction_scale_f32);
    hash_i32(hash, fighter->shield_radius_x_f32);
    hash_i32(hash, fighter->shield_radius_y_f32);
    hash_i32(
        hash,
        fighter->shield_minimum_size_scale_f32);
    hash_i32(
        hash,
        fighter->dense_shield_size_scale_f32);
    hash_i32(hash, fighter->shield_center_forward_f32);
    hash_i32(hash, fighter->shield_center_up_f32);
    hash_i32(hash, fighter->shield_animation_scale_x_f32);
    hash_i32(hash, fighter->shield_animation_scale_y_f32);
    hash_i32(hash, fighter->grabbox_offset_x_f32);
    hash_i32(hash, fighter->grabbox_offset_y_f32);
    hash_i32(hash, fighter->grabbox_half_width_f32);
    hash_i32(hash, fighter->grabbox_half_height_f32);
    hash_i32(hash, fighter->grabbed_offset_x_f32);
    hash_i32(hash, fighter->grabbed_offset_y_f32);
    hash_i32(hash, fighter->grab_escape_damage_ticks_f32);
    hash_u32(hash, fighter->pummel_damage_f32);
    hash_u16(hash, fighter->pummel_hit_tick);
    hash_u16(hash, fighter->pummel_total_ticks);
    hash_throw(hash, &fighter->forward_throw);
    hash_throw(hash, &fighter->back_throw);
    hash_throw(hash, &fighter->up_throw);
    hash_throw(hash, &fighter->down_throw);
    hash_u16(hash, fighter->jump_squat_ticks);
    hash_u16(hash, fighter->double_jump_cancel_ticks);
    hash_u16(
        hash,
        fighter->double_jump_armor_max_hitstun_ticks);
    hash_u16(hash, fighter->initial_dash_ticks);
    hash_u16(hash, fighter->dash_run_transition_ticks);
    hash_u16(hash, fighter->standing_turn_ticks);
    hash_u16(hash, fighter->standing_turn_facing_tick);
    hash_u16(hash, fighter->dash_input_window_ticks);
    hash_u16(hash, fighter->teeter_ticks);
    hash_u16(hash, fighter->teeter_turn_axis_threshold);
    hash_u16(hash, fighter->teeter_walk_axis_threshold);
    hash_u16(hash, fighter->walk_axis_threshold);
    hash_u16(hash, fighter->crouch_step_ticks);
    hash_u16(hash, fighter->taunt_ticks);
    hash_u16(
        hash,
        fighter->forward_smash_input_window_ticks);
    hash_u16(hash, fighter->landing_ticks);
    hash_u16(hash, fighter->landing_interruptible_tick);
    hash_u16(hash, fighter->platform_drop_ticks);
    hash_u16(hash, fighter->platform_drop_startup_ticks);
    hash_u16(hash, fighter->air_dodge_ticks);
    hash_u16(
        hash,
        fighter->air_dodge_invulnerability_begin_tick);
    hash_u16(
        hash,
        fighter->air_dodge_invulnerability_end_tick);
    hash_u16(
        hash,
        fighter->air_dodge_ordinary_physics_begin_tick);
    hash_u16(hash, fighter->ledge_invulnerability_ticks);
    hash_u16(hash, fighter->ledge_regrab_lockout_ticks);
    hash_u16(hash, fighter->ledge_transition_ticks);
    hash_u16(hash, fighter->ledge_roll_ticks);
    hash_u16(hash, fighter->ledge_roll_movement_ticks);
    hash_u16(hash, fighter->ledge_roll_invulnerability_ticks);
    hash_u16(hash, fighter->ledge_attack_invulnerability_ticks);
    hash_u16(hash, fighter->special_landing_ticks);
    hash_u16(hash, fighter->run_turnaround_ticks);
    hash_u16(hash, fighter->run_brake_ticks);
    hash_u16(hash, fighter->axis_dead_zone);
    hash_u16(hash, fighter->dash_axis_threshold);
    hash_u16(hash, fighter->run_turnaround_axis_threshold);
    hash_u16(hash, fighter->run_continue_axis_threshold);
    hash_u16(hash, fighter->run_turnaround_lockout_ticks);
    hash_u16(hash, fighter->tilt_axis_threshold);
    hash_u16(hash, fighter->tap_jump_axis_threshold);
    hash_u16(hash, fighter->tap_jump_input_window_ticks);
    hash_u16(hash, fighter->fast_fall_axis_threshold);
    hash_u16(hash, fighter->fast_fall_input_window_ticks);
    hash_u16(hash, fighter->air_dodge_dead_zone);
    hash_u16(hash, fighter->crouch_axis_threshold);
    hash_u16(hash, fighter->shield_drop_axis_threshold);
    hash_u16(hash, fighter->dash_attack_startup_ticks);
    hash_u16(hash, fighter->dash_attack_active_ticks);
    hash_u16(hash, fighter->dash_attack_recovery_ticks);
    hash_u16(hash, fighter->dash_attack_hitlag_ticks);
    hash_u16(
        hash,
        fighter->boost_grab_cancel_begin_tick);
    hash_u16(
        hash,
        fighter->boost_grab_cancel_end_tick);
    hash_u16(hash, fighter->jab_startup_ticks);
    hash_u16(hash, fighter->jab_active_ticks);
    hash_u16(hash, fighter->jab_recovery_ticks);
    hash_u16(hash, fighter->jab_hitlag_ticks);
    hash_u16(hash, fighter->jab_combo_input_begin_tick);
    hash_u16(hash, fighter->jab_combo_input_end_tick);
    hash_u16(hash, fighter->jab_final_startup_ticks);
    hash_u16(hash, fighter->jab_final_active_ticks);
    hash_u16(hash, fighter->jab_final_recovery_ticks);
    hash_u16(hash, fighter->jab_final_hitlag_ticks);
    hash_u16(hash, fighter->reset_max_hitstun_ticks);
    hash_u16(hash, fighter->reset_bound_ticks);
    hash_u16(hash, fighter->reset_forced_getup_ticks);
    hash_u16(hash, fighter->strong_startup_ticks);
    hash_u16(hash, fighter->strong_active_ticks);
    hash_u16(hash, fighter->strong_recovery_ticks);
    hash_u16(hash, fighter->strong_hitlag_ticks);
    hash_u16(hash, fighter->aerial_startup_ticks);
    hash_u16(hash, fighter->aerial_active_ticks);
    hash_u16(hash, fighter->aerial_recovery_ticks);
    hash_u16(hash, fighter->aerial_hitlag_ticks);
    hash_u16(
        hash,
        fighter->aerial_landing_lag_begin_tick);
    hash_u16(
        hash,
        fighter->aerial_landing_lag_end_tick);
    hash_u16(hash, fighter->aerial_landing_lag_ticks);
    hash_u16(hash, fighter->forward_aerial_landing_lag_ticks);
    hash_u16(hash, fighter->back_aerial_landing_lag_ticks);
    hash_u16(hash, fighter->up_aerial_landing_lag_ticks);
    hash_u16(hash, fighter->down_aerial_landing_lag_ticks);
    hash_u16(
        hash,
        fighter->strong_aerial_landing_lag_ticks);
    hash_u16(hash, fighter->l_cancel_window_ticks);
    hash_u16(hash, fighter->l_cancel_divisor);
    hash_u16(hash, fighter->v_cancel_window_ticks);
    hash_u16(hash, fighter->sdi_stick_threshold);
    hash_u16(hash, fighter->sdi_stick_window_ticks);
    hash_u16(
        hash,
        fighter->light_shield_trigger_threshold);
    hash_u16(hash, fighter->digital_trigger_threshold);
    hash_u16(hash, fighter->tumble_hitstun_threshold_ticks);
    hash_u16(hash, fighter->tech_window_ticks);
    hash_u16(hash, fighter->tech_lockout_ticks);
    hash_u16(hash, fighter->tech_roll_axis_threshold);
    hash_u16(hash, fighter->tech_in_place_ticks);
    hash_u16(hash, fighter->tech_roll_ticks);
    hash_u16(hash, fighter->tech_invulnerability_ticks);
    hash_u16(hash, fighter->wall_tech_stall_ticks);
    hash_u16(hash, fighter->wall_tech_invulnerability_ticks);
    hash_u16(hash, fighter->wall_tech_ticks);
    hash_u16(hash, fighter->wall_tech_jump_ticks);
    hash_u16(
        hash,
        fighter->surface_bounce_invulnerability_ticks);
    hash_u16(
        hash,
        fighter->surface_bounce_collision_lockout_ticks);
    hash_u16(hash, fighter->wall_jump_ticks);
    hash_u16(hash, fighter->wall_jump_invulnerability_ticks);
    hash_u16(hash, fighter->ceiling_tech_control_tick);
    hash_u16(hash, fighter->ceiling_tech_ticks);
    hash_u16(hash, fighter->knockdown_ticks);
    hash_u16(hash, fighter->down_wait_ticks);
    hash_i32(hash, fighter->down_horizontal_angle_tan_f32);
    hash_u16(hash, fighter->down_up_axis_threshold);
    hash_u16(hash, fighter->down_horizontal_axis_threshold);
    hash_u16(hash, fighter->down_attack_input_window_ticks);
    hash_u16(hash, fighter->down_c_up_axis_threshold);
    hash_u16(hash, fighter->getup_neutral_ticks);
    hash_u16(
        hash,
        fighter->getup_neutral_invulnerability_ticks);
    hash_u16(hash, fighter->getup_roll_ticks);
    hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_back_forward);
    hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_back_backward);
    hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_stomach_forward);
    hash_getup_roll_timing(
        hash,
        &fighter->getup_roll_stomach_backward);
    hash_u16(hash, fighter->getup_attack_ticks);
    hash_u16(
        hash,
        fighter->getup_attack_back_invulnerability_ticks);
    hash_u16(
        hash,
        fighter->getup_attack_stomach_invulnerability_ticks);
    hash_u16(
        hash,
        fighter->getup_attack_front_active_begin_tick);
    hash_u16(
        hash,
        fighter->getup_attack_front_active_end_tick);
    hash_u16(
        hash,
        fighter->getup_attack_back_active_begin_tick);
    hash_u16(
        hash,
        fighter->getup_attack_back_active_end_tick);
    hash_u16(hash, fighter->getup_attack_hitlag_ticks);
    hash_u16(hash, fighter->forward_roll_ticks);
    hash_u16(hash, fighter->backward_roll_ticks);
    hash_u16(hash, fighter->roll_movement_begin_tick);
    hash_u16(hash, fighter->roll_movement_end_tick);
    hash_u16(
        hash,
        fighter->roll_invulnerability_begin_tick);
    hash_u16(
        hash,
        fighter->roll_invulnerability_end_tick);
    hash_u16(hash, fighter->spot_dodge_ticks);
    hash_u16(
        hash,
        fighter->spot_dodge_invulnerability_begin_tick);
    hash_u16(
        hash,
        fighter->spot_dodge_invulnerability_end_tick);
    hash_u16(hash, fighter->shield_minimum_hold_ticks);
    hash_u16(hash, fighter->shield_release_ticks);
    hash_u16(hash, fighter->powershield_window_ticks);
    hash_u16(
        hash,
        fighter->powershield_cancel_delay_ticks);
    hash_u16(hash, fighter->shield_break_stun_ticks);
    hash_u16(
        hash,
        fighter->shield_break_minimum_stun_ticks);
    hash_u16(hash, fighter->shield_break_down_ticks);
    hash_u16(hash, fighter->shield_break_stand_ticks);
    hash_u16(
        hash,
        fighter->shield_break_mash_reduction_ticks);
    hash_u16(hash, fighter->mash_stick_axis_threshold);
    hash_u16(
        hash,
        fighter->shield_break_stun_tick_decrement);
    hash_u16(hash, fighter->grab_startup_ticks);
    hash_u16(hash, fighter->grab_active_ticks);
    hash_u16(hash, fighter->grab_recovery_ticks);
    hash_u16(hash, fighter->dash_grab_startup_ticks);
    hash_u16(hash, fighter->dash_grab_active_ticks);
    hash_u16(hash, fighter->dash_grab_recovery_ticks);
    hash_u16(hash, fighter->grab_escape_base_ticks);
    hash_u16(hash, fighter->grab_escape_max_ticks);
    hash_u16(hash, fighter->grab_mash_reduction_ticks);
    hash_u16(hash, fighter->grab_escape_tick_decrement);
    hash_u16(hash, fighter->grab_release_ticks);
    hash_i32(hash, fighter->grab_release_speed_x_f32);
    hash_i32(hash, fighter->grab_release_air_speed_x_f32);
    hash_i32(hash, fighter->grab_release_air_speed_y_f32);
    hash_u8(hash, fighter->air_jump_count);
    hash_u8(
        hash,
        fighter->powershield_cancel_enabled);
    hash_u8(hash, fighter->wall_jump_enabled);
    for (stale_index = UINT32_C(0);
         stale_index <
             (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++stale_index)
    {
        hash_u16(
            hash,
            fighter->stale_move_slot_reduction_f32[stale_index]);
    }
    hash_u16(hash, fighter->crouch_start_ticks);
    hash_u16(hash, fighter->crouch_end_ticks);
    hash_u16(hash, fighter->crouch_release_axis_threshold);
}

static void hash_stage(
    pf_sha256 *hash,
    const stage_data *stage)
{
    const ssbm_stage_collision_profile *reference_stage =
        ssbm_reference_stage_collision(
            stage->reference_collision_profile);

    hash_u16(hash, stage->schema_version);
    hash_u16(hash, stage->reference_collision_profile);
    if (reference_stage != NULL)
    {
        pf_sha256_update(
            hash,
            reference_stage->source_sha256,
            (size_t)32);
    }
    hash_i32(hash, stage->floor_left_f32);
    hash_i32(hash, stage->floor_right_f32);
    hash_i32(hash, stage->floor_y_f32);
    hash_i32(hash, stage->platform_center_x_f32);
    hash_i32(hash, stage->platform_y_f32);
    hash_i32(hash, stage->platform_half_width_f32);
    hash_i32(hash, stage->platform_motion_amplitude_f32);
    hash_i32(hash, stage->solid_left_f32);
    hash_i32(hash, stage->solid_right_f32);
    hash_i32(hash, stage->solid_top_f32);
    hash_i32(hash, stage->solid_bottom_f32);
    hash_i32(hash, stage->blast_left_f32);
    hash_i32(hash, stage->blast_right_f32);
    hash_i32(hash, stage->blast_top_f32);
    hash_i32(hash, stage->blast_bottom_f32);
    hash_i32(hash, stage->spawn_spacing_f32);
    hash_u16(hash, stage->platform_motion_period_ticks);
    hash_u16(hash, stage->reference_spawn_line);
    hash_i32(hash, stage->reference_spawn_x_f32);
    hash_i32(hash, stage->revival_platform_start_y_f32);
    hash_i32(hash, stage->revival_platform_end_y_f32);
    hash_i32(hash, stage->revival_platform_half_width_f32);
    hash_u16(hash, stage->revival_platform_descent_ticks);
    hash_u16(hash, stage->revival_platform_hold_ticks);
    hash_i32(hash, stage->upper_platform_center_x_f32);
    hash_i32(hash, stage->upper_platform_y_f32);
    hash_i32(hash, stage->upper_platform_half_width_f32);
}

static void hash_item(
    pf_sha256 *hash,
    const item_data *item)
{
    hash_u16(hash, item->schema_version);
    hash_u8(hash, item->enabled);
    hash_i32(hash, item->half_width_f32);
    hash_i32(hash, item->half_height_f32);
    hash_i32(hash, item->spawn_x_f32);
    hash_i32(hash, item->spawn_y_f32);
    hash_i32(hash, item->pickup_half_width_f32);
    hash_i32(hash, item->pickup_half_height_f32);
    hash_i32(hash, item->held_offset_x_f32);
    hash_i32(hash, item->held_offset_y_f32);
    hash_i32(hash, item->gravity_f32);
    hash_i32(hash, item->fall_speed_f32);
    hash_i32(hash, item->drop_velocity_y_f32);
    hash_i32(hash, item->forward_throw.velocity_x_f32);
    hash_i32(hash, item->forward_throw.velocity_y_f32);
    hash_i32(hash, item->back_throw.velocity_x_f32);
    hash_i32(hash, item->back_throw.velocity_y_f32);
    hash_i32(hash, item->up_throw.velocity_x_f32);
    hash_i32(hash, item->up_throw.velocity_y_f32);
    hash_i32(hash, item->down_throw.velocity_x_f32);
    hash_i32(hash, item->down_throw.velocity_y_f32);
    hash_i32(hash, item->momentum_transfer_f32);
    hash_i32(hash, item->hitbox_half_width_f32);
    hash_i32(hash, item->hitbox_half_height_f32);
    hash_u32(hash, item->damage_f32);
    hash_i32(hash, item->base_knockback_x_f32);
    hash_i32(hash, item->base_knockback_y_f32);
    hash_i32(hash, item->knockback_growth_f32);
    hash_i32(hash, item->hit_bounce_velocity_y_f32);
    hash_i32(hash, item->dash_throw_speed_f32);
    hash_u16(hash, item->throw_recovery_ticks);
    hash_u16(hash, item->dash_throw_recovery_ticks);
    hash_u16(hash, item->glide_toss_begin_tick);
    hash_u16(hash, item->glide_toss_end_tick);
    hash_u16(hash, item->pickup_lockout_ticks);
    hash_u16(hash, item->lifetime_ticks);
    hash_u16(hash, item->respawn_ticks);
    hash_u16(hash, item->hitlag_ticks);
}

static void hash_projectile(
    pf_sha256 *hash,
    const projectile_data *projectile)
{
    hash_u16(hash, projectile->schema_version);
    hash_u8(hash, projectile->enabled);
    hash_i32(hash, projectile->half_width_f32);
    hash_i32(hash, projectile->half_height_f32);
    hash_i32(hash, projectile->spawn_offset_x_f32);
    hash_i32(hash, projectile->spawn_offset_y_f32);
    hash_i32(hash, projectile->speed_f32);
    hash_u32(hash, projectile->damage_f32);
    hash_i32(hash, projectile->base_knockback_x_f32);
    hash_i32(hash, projectile->base_knockback_y_f32);
    hash_i32(hash, projectile->knockback_growth_f32);
    hash_u16(hash, projectile->lifetime_ticks);
    hash_u16(hash, projectile->fire_recovery_ticks);
    hash_u16(hash, projectile->hitlag_ticks);
    hash_u16(hash, projectile->powershield_reflect_window_ticks);
}

static void hash_reflector(
    pf_sha256 *hash,
    const reflector_data *reflector)
{
    hash_u16(hash, reflector->schema_version);
    hash_u8(hash, reflector->enabled);
    hash_i32(hash, reflector->hitbox_offset_x_f32);
    hash_i32(hash, reflector->hitbox_offset_y_f32);
    hash_i32(hash, reflector->hitbox_half_width_f32);
    hash_i32(hash, reflector->hitbox_half_height_f32);
    hash_u32(hash, reflector->damage_f32);
    hash_i32(hash, reflector->base_knockback_x_f32);
    hash_i32(hash, reflector->base_knockback_y_f32);
    hash_i32(hash, reflector->knockback_growth_f32);
    hash_u16(hash, reflector->startup_ticks);
    hash_u16(hash, reflector->active_ticks);
    hash_u16(hash, reflector->recovery_ticks);
    hash_u16(hash, reflector->hitlag_ticks);
}

static void hash_charge(
    pf_sha256 *hash,
    const charge_data *charge)
{
    hash_u16(hash, charge->schema_version);
    hash_u8(hash, charge->enabled);
    hash_i32(hash, charge->hitbox_offset_x_f32);
    hash_i32(hash, charge->hitbox_offset_y_f32);
    hash_i32(hash, charge->hitbox_half_width_f32);
    hash_i32(hash, charge->hitbox_half_height_f32);
    hash_u32(hash, charge->base_damage_f32);
    hash_u32(hash, charge->bonus_damage_f32);
    hash_i32(hash, charge->base_knockback_x_f32);
    hash_i32(hash, charge->base_knockback_y_f32);
    hash_i32(hash, charge->knockback_growth_f32);
    hash_u16(hash, charge->max_charge_ticks);
    hash_u16(hash, charge->store_animation_ticks);
    hash_u16(hash, charge->release_startup_ticks);
    hash_u16(hash, charge->release_active_ticks);
    hash_u16(hash, charge->release_recovery_ticks);
    hash_u16(hash, charge->release_hitlag_ticks);
}

static void hash_recovery(
    pf_sha256 *hash,
    const recovery_data *recovery)
{
    hash_u16(hash, recovery->schema_version);
    hash_u8(hash, recovery->enabled);
    hash_i32(hash, recovery->horizontal_speed_f32);
    hash_i32(hash, recovery->vertical_speed_f32);
    hash_u16(hash, recovery->ascent_ticks);
}

static void content_hash(
    const struct content *content,
    uint8_t digest[32])
{
    pf_sha256 hash;

    pf_sha256_init(&hash);
    pf_sha256_update(
        &hash,
        content_hash_domain,
        sizeof(content_hash_domain));
    hash_u16(&hash, content->schema_version);
    hash_u8(&hash, content->fighter_count);
    hash_u8(&hash, content->stage_count);
    hash_u8(&hash, content->item_count);
    hash_u8(&hash, content->projectile_count);
    hash_u8(&hash, content->reflector_count);
    hash_u8(&hash, content->charge_count);
    hash_u8(&hash, content->recovery_count);
    hash_u8(&hash, content->gameplay_ruleset);
    hash_fighter(&hash, &content->fighter);
    hash_stage(&hash, &content->stage);
    hash_item(&hash, &content->item);
    hash_projectile(&hash, &content->projectile);
    hash_reflector(&hash, &content->reflector);
    hash_charge(&hash, &content->charge);
    hash_recovery(&hash, &content->recovery);
    pf_sha256_finish(&hash, digest);
}

static int hash_equal(
    const uint8_t left[32],
    const uint8_t right[32])
{
    uint8_t difference = UINT8_C(0);
    uint32_t byte_index;

    for (byte_index = UINT32_C(0);
         byte_index < UINT32_C(32);
         ++byte_index)
    {
        difference |= (uint8_t)(left[byte_index] ^ right[byte_index]);
    }
    return difference == UINT8_C(0);
}

const getup_roll_timing *getup_roll_timing_for(
    const fighter_data *fighter,
    uint8_t prone_orientation,
    int8_t roll_direction,
    int8_t facing)
{
    const int is_forward = roll_direction == facing;

    if (fighter == NULL ||
        (roll_direction != INT8_C(-1) &&
         roll_direction != INT8_C(1)) ||
        (facing != INT8_C(-1) && facing != INT8_C(1)))
    {
        return NULL;
    }

    if (prone_orientation == (uint8_t)PF_M4_PRONE_BACK)
    {
        return is_forward ? &fighter->getup_roll_back_forward
                          : &fighter->getup_roll_back_backward;
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_STOMACH)
    {
        return is_forward ? &fighter->getup_roll_stomach_forward
                          : &fighter->getup_roll_stomach_backward;
    }
    return NULL;
}

uint16_t getup_roll_submotion_for(
    uint8_t prone_orientation,
    int8_t roll_direction,
    int8_t facing)
{
    const int is_forward = roll_direction == facing;

    if ((roll_direction != INT8_C(-1) &&
         roll_direction != INT8_C(1)) ||
        (facing != INT8_C(-1) && facing != INT8_C(1)))
    {
        return UINT16_MAX;
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_BACK)
    {
        return is_forward != 0
                   ? (uint16_t)
                         PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_BACK
                   : (uint16_t)
                         PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_BACK;
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_STOMACH)
    {
        return is_forward != 0
                   ? (uint16_t)
                         PF_M4_FALCON_SUBMOTION_GETUP_ROLL_FORWARD_STOMACH
                   : (uint16_t)
                         PF_M4_FALCON_SUBMOTION_GETUP_ROLL_BACKWARD_STOMACH;
    }
    return UINT16_MAX;
}

uint16_t getup_attack_invulnerability_ticks_for(
    const fighter_data *fighter,
    uint8_t prone_orientation)
{
    if (fighter == NULL)
    {
        return UINT16_C(0);
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_BACK)
    {
        return fighter->getup_attack_back_invulnerability_ticks;
    }
    if (prone_orientation == (uint8_t)PF_M4_PRONE_STOMACH)
    {
        return fighter->getup_attack_stomach_invulnerability_ticks;
    }
    return UINT16_C(0);
}

pf_status default_content(struct content *out_content)
{
    const falcon_common_attributes *falcon_attributes;
    const falcon_air_dodge_attributes *air_dodge_attributes;
    const ssbm_damage_response_attributes *damage_response;
    const ssbm_surface_response_attributes *surface_response;
    const ssbm_ledge_response_attributes *ledge_response;
    const ssbm_mash_attributes *mash;
    const ssbm_ground_input_attributes *ground_input;
    const ssbm_rebirth_attributes *rebirth;
    const melee_stale_move_data *stale_move_data;
    const falcon_smash_charge_attributes *smash_charge;
    fighter_data *fighter;
    stage_data *stage;
    item_data *item;
    projectile_data *projectile;
    reflector_data *reflector;
    charge_data *charge;
    recovery_data *recovery;

    if (out_content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }

    falcon_attributes = falcon_reference_common_attributes();
    air_dodge_attributes = falcon_reference_air_dodge_attributes();
    damage_response = ssbm_common_reference_damage_response();
    surface_response = ssbm_common_reference_surface_response();
    ledge_response = ssbm_common_reference_ledge_response();
    mash = ssbm_common_reference_mash();
    ground_input = ssbm_common_reference_ground_input();
    rebirth = ssbm_common_reference_rebirth();
    stale_move_data = falcon_reference_stale_move_data();
    smash_charge = falcon_reference_smash_charge_attributes();
    if (falcon_attributes == NULL || air_dodge_attributes == NULL ||
        damage_response == NULL || surface_response == NULL ||
        ledge_response == NULL || mash == NULL || ground_input == NULL ||
        rebirth == NULL || stale_move_data == NULL || smash_charge == NULL)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    (void)memset(out_content, 0, sizeof(*out_content));
    out_content->struct_size = (uint32_t)sizeof(*out_content);
    out_content->schema_version = PF_M4_CONTENT_SCHEMA_VERSION;
    out_content->fighter_count = PF_M4_PLACEHOLDER_FIGHTER_COUNT;
    out_content->stage_count = PF_M4_TEST_STAGE_COUNT;
    out_content->item_count = PF_M4_TEST_ITEM_COUNT;
    out_content->projectile_count = PF_M4_TEST_PROJECTILE_COUNT;
    out_content->reflector_count = PF_M4_TEST_REFLECTOR_COUNT;
    out_content->charge_count = PF_M4_TEST_CHARGE_COUNT;
    out_content->recovery_count = PF_M4_TEST_RECOVERY_COUNT;
    out_content->gameplay_ruleset =
        (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084;

    fighter = &out_content->fighter;
    fighter->struct_size = (uint32_t)sizeof(*fighter);
    fighter->schema_version = PF_M4_FIGHTER_SCHEMA_VERSION;
    fighter->reference_frame_data_enabled = UINT8_C(1);
    fighter->half_width_f32 = PF_F32_RATIO(9, 20);
    fighter->half_height_f32 = PF_F32_RATIO(4, 5);
    fighter->player_push_half_width_f32 = PF_F32_RATIO(42, 115);
    fighter->player_push_speed_f32 = PF_F32_RATIO(18, 575);
    fighter->weight_f32 = PF_F32_ONE;
    fighter->ground_acceleration_f32 =
        falcon_attributes->dash_run_acceleration_a_f32;
    fighter->turn_acceleration_f32 = PF_F32_RATIO(48, 2875);
    fighter->traction_f32 = falcon_attributes->friction_f32;
    fighter->walk_speed_f32 =
        falcon_attributes->walk_maximum_velocity_f32;
    fighter->run_speed_f32 =
        falcon_attributes->dash_run_terminal_velocity_f32;
    fighter->initial_dash_speed_f32 =
        falcon_attributes->dash_initial_velocity_f32;
    fighter->walk_initial_velocity_f32 =
        falcon_attributes->initial_walk_velocity_f32;
    fighter->walk_acceleration_f32 =
        falcon_attributes->walk_acceleration_f32;
    fighter->slow_walk_animation_scaling_f32 =
        falcon_attributes->slow_walk_animation_scaling_f32;
    fighter->middle_walk_animation_scaling_f32 =
        falcon_attributes->middle_walk_animation_scaling_f32;
    fighter->fast_walk_animation_scaling_f32 =
        falcon_attributes->fast_walk_animation_scaling_f32;
    fighter->run_animation_scaling_f32 =
        falcon_attributes->run_animation_scaling_f32;
    fighter->walk_middle_speed_ratio_f32 =
        ground_input->walk_middle_speed_ratio_f32;
    fighter->walk_fast_speed_ratio_f32 =
        ground_input->walk_fast_speed_ratio_f32;
    fighter->dash_run_base_acceleration_f32 =
        falcon_attributes->dash_run_acceleration_b_f32;
    fighter->ground_max_horizontal_speed_f32 =
        falcon_attributes->ground_maximum_horizontal_velocity_f32;
    fighter->walk_acceleration_taper_f32 = PF_F32_RATIO(1, 2);
    fighter->run_acceleration_taper_f32 = PF_F32_RATIO(2, 5);
    fighter->teeter_snap_distance_f32 = PF_F32_RATIO(2, 5);
    fighter->crouch_step_speed_f32 = INT32_C(0);
    fighter->air_acceleration_f32 = falcon_attributes->air_mobility_a_f32;
    fighter->air_base_acceleration_f32 =
        falcon_attributes->air_mobility_b_f32;
    fighter->air_friction_f32 = falcon_attributes->air_friction_f32;
    fighter->air_max_horizontal_speed_f32 =
        falcon_attributes->maximum_horizontal_air_velocity_f32;
    fighter->air_speed_f32 =
        falcon_attributes->max_aerial_horizontal_velocity_f32;
    fighter->jump_horizontal_input_speed_f32 =
        falcon_attributes->jump_horizontal_initial_velocity_f32;
    fighter->jump_horizontal_momentum_multiplier_f32 =
        falcon_attributes->ground_air_jump_momentum_multiplier_f32;
    fighter->jump_horizontal_max_speed_f32 =
        falcon_attributes->jump_horizontal_maximum_velocity_f32;
    fighter->gravity_f32 = falcon_attributes->gravity_f32;
    fighter->fall_speed_f32 = falcon_attributes->terminal_velocity_f32;
    fighter->fast_fall_speed_f32 =
        falcon_attributes->fast_fall_terminal_velocity_f32;
    fighter->full_hop_speed_f32 =
        falcon_attributes->jump_vertical_initial_velocity_f32;
    fighter->short_hop_speed_f32 =
        falcon_attributes->shorthop_vertical_initial_velocity_f32;
    fighter->double_jump_speed_f32 =
        falcon_attributes->double_jump_vertical_velocity_f32;
    fighter->double_jump_horizontal_speed_f32 =
        falcon_attributes->double_jump_horizontal_velocity_f32;
    fighter->platform_drop_nudge_f32 = PF_F32_RATIO(1, 256);
    fighter->platform_drop_speed_y_f32 = PF_F32_RATIO(693, 6200);
    fighter->ledge_jump_speed_x_f32 =
        falcon_attributes->ledge_jump_horizontal_velocity_f32;
    fighter->ledge_jump_speed_y_f32 =
        falcon_attributes->ledge_jump_vertical_velocity_f32;
    fighter->ledge_roll_distance_f32 = PF_F32_RATIO(7, 4);
    fighter->drop_cancel_snap_distance_f32 = PF_F32_RATIO(5, 8);
    fighter->air_dodge_speed_x_f32 =
        air_dodge_attributes->initial_velocity_x_f32;
    fighter->air_dodge_speed_y_f32 =
        air_dodge_attributes->initial_velocity_y_f32;
    fighter->air_dodge_decay_f32 = air_dodge_attributes->decay_f32;
    fighter->fall_special_mobility_f32 = PF_F32_RATIO(1008, 14375);
    fighter->shield_break_launch_speed_f32 =
        falcon_attributes->shield_break_initial_velocity_f32;
    fighter->dash_attack_speed_f32 = PF_F32_RATIO(7, 20);
    fighter->dash_attack_hitbox_offset_x_f32 =
        PF_F32_RATIO(4, 5);
    fighter->dash_attack_hitbox_offset_y_f32 =
        PF_F32_RATIO(1, 20);
    fighter->dash_attack_hitbox_half_width_f32 =
        PF_F32_RATIO(13, 20);
    fighter->dash_attack_hitbox_half_height_f32 =
        PF_F32_RATIO(9, 20);
    fighter->dash_attack_damage_f32 =
        8.0f;
    fighter->dash_attack_base_knockback_x_f32 =
        PF_F32_RATIO(6, 25);
    fighter->dash_attack_base_knockback_y_f32 =
        PF_F32_RATIO(3, 10);
    fighter->dash_attack_knockback_growth_f32 =
        PF_F32_RATIO(1, 768);
    fighter->jab_hitbox_offset_x_f32 = PF_F32_RATIO(3, 4);
    fighter->jab_hitbox_offset_y_f32 = INT32_C(0);
    fighter->jab_hitbox_half_width_f32 = PF_F32_RATIO(3, 5);
    fighter->jab_hitbox_half_height_f32 = PF_F32_RATIO(9, 20);
    fighter->jab_base_knockback_x_f32 = PF_F32_RATIO(9, 50);
    fighter->jab_base_knockback_y_f32 = PF_F32_RATIO(11, 50);
    fighter->jab_knockback_growth_f32 = PF_F32_RATIO(1, 512);
    fighter->jab_final_hitbox_offset_x_f32 = PF_F32_RATIO(4, 5);
    fighter->jab_final_hitbox_offset_y_f32 = INT32_C(0);
    fighter->jab_final_hitbox_half_width_f32 = PF_F32_RATIO(13, 20);
    fighter->jab_final_hitbox_half_height_f32 = PF_F32_RATIO(9, 20);
    fighter->jab_final_base_knockback_x_f32 = PF_F32_RATIO(1, 4);
    fighter->jab_final_base_knockback_y_f32 = PF_F32_RATIO(3, 10);
    fighter->jab_final_knockback_growth_f32 = PF_F32_RATIO(1, 512);
    fighter->up_attack.hitbox_offset_x_f32 = PF_F32_RATIO(1, 4);
    fighter->up_attack.hitbox_offset_y_f32 = -PF_F32_RATIO(1, 2);
    fighter->up_attack.hitbox_half_width_f32 = PF_F32_RATIO(13, 20);
    fighter->up_attack.hitbox_half_height_f32 = PF_F32_RATIO(3, 4);
    fighter->up_attack.damage_f32 = 9.0f;
    fighter->up_attack.base_knockback_x_f32 = PF_F32_RATIO(1, 10);
    fighter->up_attack.base_knockback_y_f32 = PF_F32_RATIO(21, 50);
    fighter->up_attack.knockback_growth_f32 = PF_F32_RATIO(1, 640);
    fighter->up_attack.startup_ticks = UINT16_C(4);
    fighter->up_attack.active_ticks = UINT16_C(3);
    fighter->up_attack.recovery_ticks = UINT16_C(12);
    fighter->up_attack.hitlag_ticks = UINT16_C(5);
    fighter->down_attack.hitbox_offset_x_f32 = PF_F32_RATIO(3, 5);
    fighter->down_attack.hitbox_offset_y_f32 = PF_F32_RATIO(9, 20);
    fighter->down_attack.hitbox_half_width_f32 = PF_F32_RATIO(3, 4);
    fighter->down_attack.hitbox_half_height_f32 = PF_F32_RATIO(7, 20);
    fighter->down_attack.damage_f32 = 8.0f;
    fighter->down_attack.base_knockback_x_f32 = PF_F32_RATIO(1, 5);
    fighter->down_attack.base_knockback_y_f32 = PF_F32_RATIO(9, 50);
    fighter->down_attack.knockback_growth_f32 = PF_F32_RATIO(1, 768);
    fighter->down_attack.startup_ticks = UINT16_C(5);
    fighter->down_attack.active_ticks = UINT16_C(3);
    fighter->down_attack.recovery_ticks = UINT16_C(11);
    fighter->down_attack.hitlag_ticks = UINT16_C(4);
    fighter->forward_attack.hitbox_offset_x_f32 =
        PF_F32_RATIO(4, 5);
    fighter->forward_attack.hitbox_offset_y_f32 =
        -PF_F32_RATIO(1, 20);
    fighter->forward_attack.hitbox_half_width_f32 =
        PF_F32_RATIO(7, 10);
    fighter->forward_attack.hitbox_half_height_f32 =
        PF_F32_RATIO(9, 20);
    fighter->forward_attack.damage_f32 =
        7.0f;
    fighter->forward_attack.base_knockback_x_f32 =
        PF_F32_RATIO(6, 25);
    fighter->forward_attack.base_knockback_y_f32 =
        PF_F32_RATIO(1, 4);
    fighter->forward_attack.knockback_growth_f32 =
        PF_F32_RATIO(1, 704);
    fighter->forward_attack.startup_ticks = UINT16_C(4);
    fighter->forward_attack.active_ticks = UINT16_C(3);
    fighter->forward_attack.recovery_ticks = UINT16_C(12);
    fighter->forward_attack.hitlag_ticks = UINT16_C(4);
    fighter->forward_strong_attack.hitbox_offset_x_f32 =
        PF_F32_RATIO(9, 10);
    fighter->forward_strong_attack.hitbox_offset_y_f32 =
        -PF_F32_RATIO(1, 10);
    fighter->forward_strong_attack.hitbox_half_width_f32 =
        PF_F32_RATIO(3, 4);
    fighter->forward_strong_attack.hitbox_half_height_f32 =
        PF_F32_RATIO(11, 20);
    fighter->forward_strong_attack.damage_f32 =
        12.0f;
    fighter->forward_strong_attack.base_knockback_x_f32 =
        PF_F32_RATIO(9, 20);
    fighter->forward_strong_attack.base_knockback_y_f32 =
        PF_F32_RATIO(17, 20);
    fighter->forward_strong_attack.knockback_growth_f32 =
        PF_F32_RATIO(1, 512);
    fighter->forward_strong_attack.startup_ticks = UINT16_C(5);
    fighter->forward_strong_attack.active_ticks = UINT16_C(3);
    fighter->forward_strong_attack.recovery_ticks = UINT16_C(18);
    fighter->forward_strong_attack.hitlag_ticks = UINT16_C(6);
    fighter->up_strong_attack.hitbox_offset_x_f32 =
        PF_F32_RATIO(1, 10);
    fighter->up_strong_attack.hitbox_offset_y_f32 =
        -PF_F32_RATIO(4, 5);
    fighter->up_strong_attack.hitbox_half_width_f32 =
        PF_F32_RATIO(11, 10);
    fighter->up_strong_attack.hitbox_half_height_f32 =
        PF_F32_RATIO(4, 5);
    fighter->up_strong_attack.damage_f32 =
        13.0f;
    fighter->up_strong_attack.base_knockback_x_f32 =
        PF_F32_RATIO(3, 20);
    fighter->up_strong_attack.base_knockback_y_f32 =
        PF_F32_RATIO(9, 10);
    fighter->up_strong_attack.knockback_growth_f32 =
        PF_F32_RATIO(1, 544);
    fighter->up_strong_attack.startup_ticks = UINT16_C(7);
    fighter->up_strong_attack.active_ticks = UINT16_C(4);
    fighter->up_strong_attack.recovery_ticks = UINT16_C(22);
    fighter->up_strong_attack.hitlag_ticks = UINT16_C(6);
    fighter->down_strong_attack.hitbox_offset_x_f32 =
        PF_F32_RATIO(7, 10);
    fighter->down_strong_attack.hitbox_offset_y_f32 =
        PF_F32_RATIO(2, 5);
    fighter->down_strong_attack.hitbox_half_width_f32 =
        PF_F32_RATIO(9, 10);
    fighter->down_strong_attack.hitbox_half_height_f32 =
        PF_F32_RATIO(2, 5);
    fighter->down_strong_attack.damage_f32 =
        11.0f;
    fighter->down_strong_attack.base_knockback_x_f32 =
        PF_F32_RATIO(2, 5);
    fighter->down_strong_attack.base_knockback_y_f32 =
        PF_F32_RATIO(3, 10);
    fighter->down_strong_attack.knockback_growth_f32 =
        PF_F32_RATIO(1, 576);
    fighter->down_strong_attack.startup_ticks = UINT16_C(6);
    fighter->down_strong_attack.active_ticks = UINT16_C(4);
    fighter->down_strong_attack.recovery_ticks = UINT16_C(20);
    fighter->down_strong_attack.hitlag_ticks = UINT16_C(5);
    fighter->smash_charge_damage_bonus_f32 =
        (float)smash_charge->damage_multiplier_q8 / 256.0f - 1.0f;
    fighter->smash_charge_max_ticks = smash_charge->max_charge_ticks;
    fighter->forward_aerial.hitbox_offset_x_f32 =
        PF_F32_RATIO(3, 4);
    fighter->forward_aerial.hitbox_offset_y_f32 =
        -PF_F32_RATIO(1, 20);
    fighter->forward_aerial.hitbox_half_width_f32 =
        PF_F32_RATIO(13, 20);
    fighter->forward_aerial.hitbox_half_height_f32 =
        PF_F32_RATIO(9, 20);
    fighter->forward_aerial.damage_f32 =
        10.0f;
    fighter->forward_aerial.base_knockback_x_f32 =
        PF_F32_RATIO(3, 10);
    fighter->forward_aerial.base_knockback_y_f32 =
        PF_F32_RATIO(1, 4);
    fighter->forward_aerial.knockback_growth_f32 =
        PF_F32_RATIO(1, 640);
    fighter->forward_aerial.startup_ticks = UINT16_C(5);
    fighter->forward_aerial.active_ticks = UINT16_C(4);
    fighter->forward_aerial.recovery_ticks = UINT16_C(19);
    fighter->forward_aerial.hitlag_ticks = UINT16_C(5);
    fighter->back_aerial.hitbox_offset_x_f32 =
        PF_F32_RATIO(7, 10);
    fighter->back_aerial.hitbox_offset_y_f32 =
        -PF_F32_RATIO(1, 10);
    fighter->back_aerial.hitbox_half_width_f32 =
        PF_F32_RATIO(7, 10);
    fighter->back_aerial.hitbox_half_height_f32 =
        PF_F32_RATIO(1, 2);
    fighter->back_aerial.damage_f32 =
        11.0f;
    fighter->back_aerial.base_knockback_x_f32 =
        PF_F32_RATIO(19, 50);
    fighter->back_aerial.base_knockback_y_f32 =
        PF_F32_RATIO(11, 50);
    fighter->back_aerial.knockback_growth_f32 =
        PF_F32_RATIO(1, 576);
    fighter->back_aerial.startup_ticks = UINT16_C(4);
    fighter->back_aerial.active_ticks = UINT16_C(4);
    fighter->back_aerial.recovery_ticks = UINT16_C(20);
    fighter->back_aerial.hitlag_ticks = UINT16_C(5);
    fighter->up_aerial.hitbox_offset_x_f32 =
        PF_F32_RATIO(1, 10);
    fighter->up_aerial.hitbox_offset_y_f32 =
        -PF_F32_RATIO(7, 10);
    fighter->up_aerial.hitbox_half_width_f32 =
        PF_F32_RATIO(13, 20);
    fighter->up_aerial.hitbox_half_height_f32 =
        PF_F32_RATIO(13, 20);
    fighter->up_aerial.damage_f32 =
        9.0f;
    fighter->up_aerial.base_knockback_x_f32 =
        PF_F32_RATIO(3, 25);
    fighter->up_aerial.base_knockback_y_f32 =
        PF_F32_RATIO(19, 50);
    fighter->up_aerial.knockback_growth_f32 =
        PF_F32_RATIO(1, 672);
    fighter->up_aerial.startup_ticks = UINT16_C(5);
    fighter->up_aerial.active_ticks = UINT16_C(4);
    fighter->up_aerial.recovery_ticks = UINT16_C(18);
    fighter->up_aerial.hitlag_ticks = UINT16_C(5);
    fighter->down_aerial.hitbox_offset_x_f32 =
        PF_F32_RATIO(1, 10);
    fighter->down_aerial.hitbox_offset_y_f32 =
        PF_F32_RATIO(13, 20);
    fighter->down_aerial.hitbox_half_width_f32 =
        PF_F32_RATIO(3, 5);
    fighter->down_aerial.hitbox_half_height_f32 =
        PF_F32_RATIO(11, 20);
    fighter->down_aerial.damage_f32 =
        10.0f;
    fighter->down_aerial.base_knockback_x_f32 =
        PF_F32_RATIO(7, 50);
    fighter->down_aerial.base_knockback_y_f32 =
        PF_F32_RATIO(17, 50);
    fighter->down_aerial.knockback_growth_f32 =
        PF_F32_RATIO(1, 640);
    fighter->down_aerial.startup_ticks = UINT16_C(7);
    fighter->down_aerial.active_ticks = UINT16_C(4);
    fighter->down_aerial.recovery_ticks = UINT16_C(21);
    fighter->down_aerial.hitlag_ticks = UINT16_C(5);
    fighter->ledge_attack.hitbox_offset_x_f32 =
        PF_F32_RATIO(3, 4);
    fighter->ledge_attack.hitbox_offset_y_f32 =
        -PF_F32_RATIO(1, 20);
    fighter->ledge_attack.hitbox_half_width_f32 =
        PF_F32_RATIO(13, 20);
    fighter->ledge_attack.hitbox_half_height_f32 =
        PF_F32_RATIO(9, 20);
    fighter->ledge_attack.damage_f32 =
        10.0f;
    fighter->ledge_attack.base_knockback_x_f32 =
        PF_F32_RATIO(8, 25);
    fighter->ledge_attack.base_knockback_y_f32 =
        PF_F32_RATIO(1, 4);
    fighter->ledge_attack.knockback_growth_f32 =
        PF_F32_RATIO(1, 640);
    fighter->ledge_attack.startup_ticks = UINT16_C(6);
    fighter->ledge_attack.active_ticks = UINT16_C(3);
    fighter->ledge_attack.recovery_ticks = UINT16_C(20);
    fighter->ledge_attack.hitlag_ticks = UINT16_C(5);
    fighter->reset_max_damage_f32 =
        7.0f;
    fighter->reset_bound_speed_f32 = PF_F32_RATIO(1, 10);
    fighter->strong_hitbox_offset_x_f32 = PF_F32_RATIO(9, 10);
    fighter->strong_hitbox_offset_y_f32 = -PF_F32_RATIO(1, 10);
    fighter->strong_hitbox_half_width_f32 = PF_F32_RATIO(3, 4);
    fighter->strong_hitbox_half_height_f32 = PF_F32_RATIO(11, 20);
    fighter->strong_damage_f32 = 12.0f;
    fighter->strong_base_knockback_x_f32 = PF_F32_RATIO(9, 20);
    fighter->strong_base_knockback_y_f32 = PF_F32_RATIO(17, 20);
    fighter->strong_knockback_growth_f32 = PF_F32_RATIO(1, 512);
    fighter->aerial_hitbox_offset_x_f32 = PF_F32_RATIO(7, 20);
    fighter->aerial_hitbox_offset_y_f32 = INT32_C(0);
    fighter->aerial_hitbox_half_width_f32 = PF_F32_RATIO(17, 20);
    fighter->aerial_hitbox_half_height_f32 = PF_F32_RATIO(13, 20);
    fighter->aerial_damage_f32 = 8.0f;
    fighter->aerial_base_knockback_x_f32 = PF_F32_RATIO(1, 4);
    fighter->aerial_base_knockback_y_f32 = PF_F32_RATIO(7, 20);
    fighter->aerial_knockback_growth_f32 = PF_F32_RATIO(1, 1024);
    fighter->hitstun_velocity_per_tick_f32 = PF_F32_RATIO(1, 25);
    fighter->v_cancel_velocity_scale_f32 = PF_F32_RATIO(95, 100);
    fighter->knockback_weight = falcon_attributes->weight;
    fighter->knockback_reserved = UINT16_C(0);
    fighter->crouch_cancel_max_damage_f32 =
        PF_SIM_MAX_DAMAGE_F32;
    fighter->crouch_cancel_velocity_scale_f32 =
        damage_response->crouch_knockback_scale_f32;
    fighter->crouch_cancel_hitstun_scale_f32 =
        damage_response->crouch_knockback_scale_f32;
    fighter->di_max_angle_radians_q30 =
        damage_response->di_max_angle_radians_q30;
    fighter->ground_knockback_decay_scale_f32 =
        damage_response->ground_knockback_decay_scale_f32;
    fighter->air_knockback_decay_f32 =
        damage_response->air_knockback_decay_f32;
    fighter->sdi_distance_x_f32 = damage_response->sdi_distance_x_f32;
    fighter->sdi_distance_y_f32 = damage_response->sdi_distance_y_f32;
    fighter->asdi_distance_x_f32 = damage_response->asdi_distance_x_f32;
    fighter->asdi_distance_y_f32 = damage_response->asdi_distance_y_f32;
    fighter->shield_sdi_scale_f32 = damage_response->shield_sdi_scale_f32;
    fighter->tech_roll_speed_f32 = PF_F32_RATIO(1, 5);
    fighter->wall_tech_speed_f32 = PF_F32_RATIO(6, 115);
    fighter->wall_tech_jump_speed_x_f32 = PF_F32_RATIO(84, 575);
    fighter->wall_tech_jump_speed_y_f32 = PF_F32_RATIO(11, 20);
    fighter->wall_jump_speed_x_f32 =
        falcon_attributes->wall_jump_horizontal_velocity_f32;
    fighter->wall_jump_speed_y_f32 =
        falcon_attributes->wall_jump_vertical_velocity_f32;
    fighter->ceiling_tech_speed_f32 = PF_F32_RATIO(24, 115);
    fighter->surface_collision_threshold_x_f32 =
        surface_response->collision_threshold_x_f32;
    fighter->surface_collision_threshold_y_f32 =
        surface_response->collision_threshold_y_f32;
    fighter->surface_bounce_multiplier_f32 =
        surface_response->bounce_multiplier_f32;
    fighter->forward_roll_speed_f32 = PF_F32_RATIO(9, 50);
    fighter->backward_roll_speed_f32 = PF_F32_RATIO(4, 25);
    fighter->getup_attack_hitbox_offset_x_f32 =
        PF_F32_RATIO(3, 4);
    fighter->getup_attack_hitbox_offset_y_f32 =
        PF_F32_RATIO(1, 5);
    fighter->getup_attack_hitbox_half_width_f32 =
        PF_F32_RATIO(4, 5);
    fighter->getup_attack_hitbox_half_height_f32 =
        PF_F32_RATIO(2, 5);
    fighter->getup_attack_damage_f32 =
        6.0f;
    fighter->getup_attack_base_knockback_x_f32 =
        PF_F32_RATIO(3, 20);
    fighter->getup_attack_base_knockback_y_f32 =
        PF_F32_RATIO(1, 8);
    fighter->getup_attack_knockback_growth_f32 =
        PF_F32_RATIO(1, 1024);
    fighter->shield_health_f32 =
        60.0f;
    fighter->shield_reset_health_f32 =
        mash->furafura_shield_health_f32;
    fighter->shield_hold_depletion_f32 = PF_F32_RATIO(7, 25);
    fighter->light_shield_hold_depletion_f32 = PF_F32_RATIO(7, 500);
    fighter->shield_regeneration_f32 = PF_F32_RATIO(7, 100);
    fighter->light_shield_damage_multiplier_f32 = PF_F32_RATIO(9, 10);
    fighter->dense_shield_damage_multiplier_f32 = PF_F32_RATIO(7, 10);
    fighter->light_shield_stun_damage_multiplier_f32 =
        PF_F32_RATIO(57, 40);
    fighter->dense_shield_stun_damage_multiplier_f32 =
        PF_F32_RATIO(9, 20);
    fighter->shield_stun_base_f32 = INT32_C(2) * PF_F32_ONE;
    fighter->shield_defender_pushback_stun_scale_f32 =
        PF_F32_RATIO(12, 575);
    fighter->shield_defender_pushback_normal_scale_f32 =
        PF_F32_RATIO(3, 5);
    fighter->shield_defender_pushback_max_f32 =
        PF_F32_RATIO(24, 115);
    fighter->shield_attacker_pushback_damage_f32 =
        PF_F32_RATIO(21, 2875);
    fighter->shield_attacker_pushback_base_f32 =
        PF_F32_RATIO(6, 2875);
    fighter->shield_attacker_pushback_air_decay_f32 =
        PF_F32_RATIO(3, 575);
    fighter->shield_attacker_pushback_ground_friction_scale_f32 =
        PF_F32_RATIO(11, 10);
    fighter->shield_radius_x_f32 = 1.5182648f;
    fighter->shield_radius_y_f32 = 2.5814514f;
    fighter->shield_minimum_size_scale_f32 =
        PF_F32_RATIO(3, 20);
    fighter->dense_shield_size_scale_f32 =
        PF_F32_RATIO(1, 2);
    fighter->shield_center_forward_f32 = 0.020248413f;
    fighter->shield_center_up_f32 = 0.99798584f;
    fighter->shield_animation_scale_x_f32 = 0.10121155f;
    fighter->shield_animation_scale_y_f32 = 0.17210388f;
    fighter->grabbox_offset_x_f32 = PF_F32_RATIO(3, 4);
    fighter->grabbox_offset_y_f32 = INT32_C(0);
    fighter->grabbox_half_width_f32 = PF_F32_RATIO(1, 2);
    fighter->grabbox_half_height_f32 = PF_F32_RATIO(7, 10);
    fighter->grabbed_offset_x_f32 = PF_F32_RATIO(3, 5);
    fighter->grabbed_offset_y_f32 = INT32_C(0);
    fighter->grab_escape_damage_ticks_f32 = PF_F32_RATIO(1, 10);
    if (!apply_falcon_reference_throw(
            &fighter->forward_throw,
            PF_M4_FALCON_FORWARD_THROW) ||
        !apply_falcon_reference_throw(
            &fighter->back_throw,
            PF_M4_FALCON_BACK_THROW) ||
        !apply_falcon_reference_throw(
            &fighter->up_throw,
            PF_M4_FALCON_UP_THROW) ||
        !apply_falcon_reference_throw(
            &fighter->down_throw,
            PF_M4_FALCON_DOWN_THROW))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    fighter->jump_squat_ticks = falcon_attributes->jump_startup_ticks;
    /* Falcon takes the generic ftCo_JumpAerial_Enter_Basic route.  The
     * specialized delayed cancel/armor route belongs to other characters
     * (notably Yoshi, whose entry explicitly installs dmg.armor1). */
    fighter->double_jump_cancel_ticks = UINT16_C(0);
    fighter->double_jump_armor_max_hitstun_ticks = UINT16_C(0);
    fighter->dash_run_transition_ticks = UINT16_C(16);
    fighter->standing_turn_facing_tick = UINT16_C(8);
    fighter->dash_input_window_ticks = UINT16_C(2);
    fighter->teeter_ticks = UINT16_C(30);
    fighter->teeter_turn_axis_threshold =
        ground_input->teeter_turn_axis_threshold;
    fighter->teeter_walk_axis_threshold =
        ground_input->teeter_walk_axis_threshold;
    fighter->walk_axis_threshold = ground_input->walk_axis_threshold;
    fighter->crouch_step_ticks = UINT16_C(1);
    fighter->forward_smash_input_window_ticks = UINT16_C(3);
    fighter->landing_interruptible_tick = UINT16_C(4);
    fighter->platform_drop_ticks = UINT16_C(9);
    fighter->platform_drop_startup_ticks = UINT16_C(3);
    fighter->ledge_invulnerability_ticks = UINT16_C(37);
    fighter->ledge_regrab_lockout_ticks =
        (uint16_t)(ledge_response->regrab_cooldown_ticks - UINT16_C(1));
    fighter->ledge_transition_ticks = UINT16_C(8);
    fighter->ledge_roll_ticks = UINT16_C(30);
    fighter->ledge_roll_movement_ticks = UINT16_C(20);
    fighter->ledge_roll_invulnerability_ticks = UINT16_C(22);
    fighter->ledge_attack_invulnerability_ticks = UINT16_C(10);
    fighter->special_landing_ticks = UINT16_C(10);
    fighter->axis_dead_zone = UINT16_C(9175);
    fighter->dash_axis_threshold = UINT16_C(26214);
    fighter->run_turnaround_axis_threshold = UINT16_C(12288);
    fighter->run_continue_axis_threshold = UINT16_C(20480);
    fighter->run_turnaround_lockout_ticks = UINT16_C(10);
    fighter->tilt_axis_threshold = damage_response->stick_tilt_threshold;
    fighter->tap_jump_axis_threshold = UINT16_C(21709);
    fighter->tap_jump_input_window_ticks = UINT16_C(4);
    fighter->fast_fall_axis_threshold = UINT16_C(21709);
    fighter->fast_fall_input_window_ticks = UINT16_C(4);
    fighter->air_dodge_dead_zone = air_dodge_attributes->dead_zone;
    fighter->crouch_axis_threshold = UINT16_C(22528);
    fighter->shield_drop_axis_threshold = UINT16_C(12288);
    fighter->dash_attack_startup_ticks = UINT16_C(4);
    fighter->dash_attack_active_ticks = UINT16_C(3);
    fighter->dash_attack_recovery_ticks = UINT16_C(12);
    fighter->dash_attack_hitlag_ticks = UINT16_C(5);
    fighter->boost_grab_cancel_begin_tick = UINT16_C(1);
    fighter->boost_grab_cancel_end_tick = UINT16_C(3);
    fighter->jab_combo_input_begin_tick = UINT16_C(4);
    fighter->jab_combo_input_end_tick = UINT16_C(7);
    fighter->reset_max_hitstun_ticks = UINT16_C(12);
    fighter->reset_bound_ticks = UINT16_C(12);
    fighter->reset_forced_getup_ticks = UINT16_C(30);
    fighter->strong_startup_ticks = UINT16_C(5);
    fighter->strong_active_ticks = UINT16_C(3);
    fighter->strong_recovery_ticks = UINT16_C(18);
    fighter->strong_hitlag_ticks = UINT16_C(6);
    fighter->aerial_startup_ticks = UINT16_C(4);
    fighter->aerial_active_ticks = UINT16_C(5);
    fighter->aerial_recovery_ticks = UINT16_C(23);
    fighter->aerial_hitlag_ticks = UINT16_C(5);
    fighter->aerial_landing_lag_begin_tick = UINT16_C(4);
    fighter->aerial_landing_lag_end_tick = UINT16_C(25);
    fighter->aerial_landing_lag_ticks =
        falcon_attributes->neutral_aerial_landing_lag_ticks;
    fighter->forward_aerial_landing_lag_ticks =
        falcon_attributes->forward_aerial_landing_lag_ticks;
    fighter->back_aerial_landing_lag_ticks =
        falcon_attributes->back_aerial_landing_lag_ticks;
    fighter->up_aerial_landing_lag_ticks =
        falcon_attributes->up_aerial_landing_lag_ticks;
    fighter->down_aerial_landing_lag_ticks =
        falcon_attributes->down_aerial_landing_lag_ticks;
    fighter->strong_aerial_landing_lag_ticks = UINT16_C(30);
    fighter->l_cancel_window_ticks = UINT16_C(7);
    fighter->l_cancel_divisor = UINT16_C(2);
    fighter->v_cancel_window_ticks = UINT16_C(3);
    fighter->sdi_stick_threshold = damage_response->sdi_stick_threshold;
    fighter->sdi_stick_window_ticks =
        damage_response->sdi_stick_window_ticks;
    fighter->light_shield_trigger_threshold = UINT16_C(19661);
    fighter->digital_trigger_threshold = UINT16_MAX;
    fighter->tumble_hitstun_threshold_ticks = UINT16_C(32);
    fighter->tech_window_ticks = surface_response->tech_window_ticks;
    fighter->tech_lockout_ticks = surface_response->tech_lockout_ticks;
    fighter->tech_roll_axis_threshold =
        surface_response->tech_roll_axis_threshold;
    fighter->wall_tech_stall_ticks =
        surface_response->wall_tech_stall_ticks;
    fighter->wall_tech_invulnerability_ticks =
        surface_response->wall_tech_invulnerability_ticks;
    fighter->surface_bounce_invulnerability_ticks =
        surface_response->bounce_invulnerability_ticks;
    fighter->surface_bounce_collision_lockout_ticks =
        surface_response->bounce_collision_lockout_ticks;
    fighter->wall_jump_ticks = UINT16_C(24);
    fighter->wall_jump_invulnerability_ticks = UINT16_C(4);
    fighter->down_wait_ticks = surface_response->down_wait_ticks;
    fighter->down_horizontal_angle_tan_f32 =
        surface_response->down_horizontal_angle_tan_f32;
    fighter->down_up_axis_threshold =
        surface_response->down_up_axis_threshold;
    fighter->down_horizontal_axis_threshold =
        surface_response->down_horizontal_axis_threshold;
    fighter->down_attack_input_window_ticks =
        surface_response->down_attack_input_window_ticks;
    fighter->down_c_up_axis_threshold =
        surface_response->down_c_up_axis_threshold;
    fighter->getup_attack_front_active_begin_tick = UINT16_C(17);
    fighter->getup_attack_front_active_end_tick = UINT16_C(19);
    fighter->getup_attack_back_active_begin_tick = UINT16_C(24);
    fighter->getup_attack_back_active_end_tick = UINT16_C(26);
    fighter->getup_attack_hitlag_ticks = UINT16_C(3);
    fighter->roll_movement_begin_tick = UINT16_C(3);
    fighter->roll_movement_end_tick = UINT16_C(20);
    fighter->shield_minimum_hold_ticks = UINT16_C(8);
    fighter->powershield_window_ticks = UINT16_C(4);
    fighter->powershield_cancel_delay_ticks = UINT16_C(1);
    fighter->shield_break_stun_ticks = (uint16_t)(
        mash->furafura_max_damage_reduction_ticks +
        mash->furafura_minimum_ticks);
    fighter->shield_break_minimum_stun_ticks =
        mash->furafura_minimum_ticks;
    fighter->shield_break_down_ticks = UINT16_C(30);
    fighter->shield_break_stand_ticks = UINT16_C(30);
    fighter->shield_break_mash_reduction_ticks =
        mash->furafura_mash_reduction_ticks;
    fighter->mash_stick_axis_threshold = mash->stick_axis_threshold;
    fighter->shield_break_stun_tick_decrement =
        mash->furafura_tick_decrement;
    if (!apply_falcon_reference_grabs(fighter))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    fighter->grab_escape_base_ticks = UINT16_C(30);
    fighter->grab_escape_max_ticks = UINT16_C(90);
    fighter->grab_mash_reduction_ticks =
        mash->capture_mash_reduction_ticks;
    fighter->grab_escape_tick_decrement =
        mash->capture_tick_decrement;
    fighter->grab_release_ticks = UINT16_C(8);
    fighter->grab_release_speed_x_f32 =
        ground_input->grab_release_speed_x_f32;
    fighter->grab_release_air_speed_x_f32 =
        ground_input->grab_release_air_speed_x_f32;
    fighter->grab_release_air_speed_y_f32 =
        ground_input->grab_release_air_speed_y_f32;
    fighter->pummel_damage_f32 = 3.0f;
    fighter->pummel_hit_tick = UINT16_C(2);
    fighter->pummel_total_ticks = UINT16_C(10);
    fighter->air_jump_count = UINT8_C(1);
    fighter->powershield_cancel_enabled = UINT8_C(1);
    fighter->wall_jump_enabled = UINT8_C(1);
    (void)memcpy(
        fighter->stale_move_slot_reduction_f32,
        stale_move_data->slot_reduction_f32,
        sizeof(fighter->stale_move_slot_reduction_f32));
    fighter->crouch_release_axis_threshold = UINT16_C(20479);
    if (!apply_falcon_reference_common_action_timings(fighter))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    if (!apply_falcon_reference_defaults(fighter))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    stage = &out_content->stage;
    stage->struct_size = (uint32_t)sizeof(*stage);
    stage->schema_version = PF_M4_STAGE_SCHEMA_VERSION;
    stage->reference_collision_profile =
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED;
    stage->floor_left_f32 = -INT32_C(32) * PF_F32_ONE;
    stage->floor_right_f32 = INT32_C(32) * PF_F32_ONE;
    stage->floor_y_f32 = INT32_C(32) * PF_F32_ONE;
    stage->platform_center_x_f32 = INT32_C(0);
    stage->platform_y_f32 = INT32_C(26) * PF_F32_ONE;
    stage->platform_half_width_f32 = INT32_C(5) * PF_F32_ONE;
    stage->platform_motion_amplitude_f32 =
        INT32_C(4) * PF_F32_ONE;
    stage->solid_left_f32 = INT32_C(14) * PF_F32_ONE;
    stage->solid_right_f32 = INT32_C(27) * PF_F32_ONE;
    stage->solid_top_f32 = INT32_C(16) * PF_F32_ONE;
    stage->solid_bottom_f32 = INT32_C(29) * PF_F32_ONE;
    stage->blast_left_f32 = -INT32_C(52) * PF_F32_ONE;
    stage->blast_right_f32 = INT32_C(52) * PF_F32_ONE;
    stage->blast_top_f32 = INT32_C(2) * PF_F32_ONE;
    stage->blast_bottom_f32 = INT32_C(58) * PF_F32_ONE;
    stage->spawn_spacing_f32 = INT32_C(8) * PF_F32_ONE;
    stage->platform_motion_period_ticks = UINT16_C(120);
    stage->reference_spawn_line = UINT16_C(0);
    stage->reference_spawn_x_f32 = INT32_C(0);
    stage->revival_platform_start_y_f32 =
        INT32_C(4) * PF_F32_ONE;
    stage->revival_platform_end_y_f32 =
        INT32_C(12) * PF_F32_ONE;
    stage->revival_platform_half_width_f32 =
        INT32_C(2) * PF_F32_ONE;
    stage->revival_platform_descent_ticks = rebirth->descent_ticks;
    stage->revival_platform_hold_ticks = rebirth->wait_ticks;
    stage->upper_platform_center_x_f32 =
        INT32_C(20) * PF_F32_ONE;
    stage->upper_platform_y_f32 = INT32_C(13) * PF_F32_ONE;
    stage->upper_platform_half_width_f32 =
        INT32_C(4) * PF_F32_ONE;

    item = &out_content->item;
    item->struct_size = (uint32_t)sizeof(*item);
    item->schema_version = PF_M4_ITEM_SCHEMA_VERSION;
    item->enabled = UINT8_C(0);
    item->half_width_f32 = PF_F32_RATIO(1, 8);
    item->half_height_f32 = PF_F32_RATIO(1, 2);
    item->spawn_x_f32 = -INT32_C(7) * PF_F32_ONE;
    item->spawn_y_f32 =
        stage->floor_y_f32 - item->half_height_f32;
    item->pickup_half_width_f32 = PF_F32_RATIO(3, 2);
    item->pickup_half_height_f32 = PF_F32_RATIO(5, 4);
    item->held_offset_x_f32 = PF_F32_RATIO(3, 4);
    item->held_offset_y_f32 = -PF_F32_RATIO(3, 4);
    item->gravity_f32 = PF_F32_RATIO(1, 40);
    item->fall_speed_f32 = PF_F32_RATIO(1, 2);
    item->drop_velocity_y_f32 = INT32_C(0);
    item->forward_throw.velocity_x_f32 = PF_F32_RATIO(1, 2);
    item->forward_throw.velocity_y_f32 = -PF_F32_RATIO(1, 10);
    item->back_throw.velocity_x_f32 = -PF_F32_RATIO(9, 20);
    item->back_throw.velocity_y_f32 = -PF_F32_RATIO(3, 25);
    item->up_throw.velocity_x_f32 = PF_F32_RATIO(1, 20);
    item->up_throw.velocity_y_f32 = -PF_F32_RATIO(11, 20);
    item->down_throw.velocity_x_f32 = PF_F32_RATIO(1, 20);
    item->down_throw.velocity_y_f32 = PF_F32_RATIO(3, 5);
    item->momentum_transfer_f32 = PF_F32_RATIO(3, 4);
    item->hitbox_half_width_f32 = PF_F32_RATIO(7, 20);
    item->hitbox_half_height_f32 = PF_F32_RATIO(11, 20);
    item->damage_f32 = 7.0f;
    item->base_knockback_x_f32 = PF_F32_RATIO(9, 50);
    item->base_knockback_y_f32 = PF_F32_RATIO(11, 50);
    item->knockback_growth_f32 = PF_F32_RATIO(1, 768);
    item->hit_bounce_velocity_y_f32 = -PF_F32_RATIO(7, 20);
    item->dash_throw_speed_f32 = PF_F32_RATIO(1, 20);
    item->throw_recovery_ticks = UINT16_C(12);
    item->dash_throw_recovery_ticks = UINT16_C(20);
    item->glide_toss_begin_tick = UINT16_C(0);
    item->glide_toss_end_tick = UINT16_C(4);
    item->pickup_lockout_ticks = UINT16_C(8);
    item->lifetime_ticks = UINT16_C(600);
    item->respawn_ticks = UINT16_C(60);
    item->hitlag_ticks = UINT16_C(4);

    projectile = &out_content->projectile;
    projectile->struct_size = (uint32_t)sizeof(*projectile);
    projectile->schema_version = PF_M4_PROJECTILE_SCHEMA_VERSION;
    projectile->enabled = UINT8_C(0);
    projectile->half_width_f32 = PF_F32_RATIO(1, 5);
    projectile->half_height_f32 = PF_F32_RATIO(1, 5);
    projectile->spawn_offset_x_f32 = PF_F32_RATIO(4, 5);
    projectile->spawn_offset_y_f32 = INT32_C(0);
    projectile->speed_f32 = PF_F32_RATIO(3, 5);
    projectile->damage_f32 = 6.0f;
    projectile->base_knockback_x_f32 = PF_F32_RATIO(1, 5);
    projectile->base_knockback_y_f32 = PF_F32_RATIO(1, 10);
    projectile->knockback_growth_f32 = PF_F32_RATIO(1, 1024);
    projectile->lifetime_ticks = UINT16_C(120);
    projectile->fire_recovery_ticks = UINT16_C(8);
    projectile->hitlag_ticks = UINT16_C(3);
    projectile->powershield_reflect_window_ticks = UINT16_C(2);

    reflector = &out_content->reflector;
    reflector->struct_size = (uint32_t)sizeof(*reflector);
    reflector->schema_version = PF_M4_REFLECTOR_SCHEMA_VERSION;
    reflector->enabled = UINT8_C(0);
    reflector->hitbox_offset_x_f32 = INT32_C(0);
    reflector->hitbox_offset_y_f32 = INT32_C(0);
    reflector->hitbox_half_width_f32 = PF_F32_RATIO(7, 5);
    reflector->hitbox_half_height_f32 = PF_F32_RATIO(3, 2);
    reflector->damage_f32 = 3.0f;
    reflector->base_knockback_x_f32 = PF_F32_RATIO(4, 5);
    reflector->base_knockback_y_f32 = PF_F32_RATIO(7, 20);
    reflector->knockback_growth_f32 = PF_F32_RATIO(1, 2048);
    reflector->startup_ticks = UINT16_C(1);
    reflector->active_ticks = UINT16_C(2);
    reflector->recovery_ticks = UINT16_C(9);
    reflector->hitlag_ticks = UINT16_C(3);

    charge = &out_content->charge;
    charge->struct_size = (uint32_t)sizeof(*charge);
    charge->schema_version = PF_M4_CHARGE_SCHEMA_VERSION;
    charge->enabled = UINT8_C(0);
    charge->hitbox_offset_x_f32 = PF_F32_RATIO(7, 10);
    charge->hitbox_offset_y_f32 = INT32_C(0);
    charge->hitbox_half_width_f32 = PF_F32_RATIO(4, 5);
    charge->hitbox_half_height_f32 = PF_F32_RATIO(3, 4);
    charge->base_damage_f32 = 4.0f;
    charge->bonus_damage_f32 = 16.0f;
    charge->base_knockback_x_f32 = PF_F32_RATIO(1, 5);
    charge->base_knockback_y_f32 = PF_F32_RATIO(3, 20);
    charge->knockback_growth_f32 = PF_F32_RATIO(1, 768);
    charge->max_charge_ticks = UINT16_C(120);
    charge->store_animation_ticks = UINT16_C(4);
    charge->release_startup_ticks = UINT16_C(4);
    charge->release_active_ticks = UINT16_C(3);
    charge->release_recovery_ticks = UINT16_C(14);
    charge->release_hitlag_ticks = UINT16_C(5);

    recovery = &out_content->recovery;
    recovery->struct_size = (uint32_t)sizeof(*recovery);
    recovery->schema_version = PF_M4_RECOVERY_SCHEMA_VERSION;
    recovery->enabled = UINT8_C(0);
    recovery->horizontal_speed_f32 = PF_F32_RATIO(1, 4);
    recovery->vertical_speed_f32 = PF_F32_RATIO(4, 5);
    recovery->ascent_ticks = UINT16_C(18);

    return PF_STATUS_OK;
}

static float stage_line_center_x_f32(
    const ssbm_stage_collision_line *line)
{
    return (line->start_x_f32 + line->end_x_f32) * 0.5f;
}

static float stage_line_half_width_f32(
    const ssbm_stage_collision_line *line)
{
    const float width =
        line->end_x_f32 >= line->start_x_f32
            ? line->end_x_f32 - line->start_x_f32
            : line->start_x_f32 - line->end_x_f32;

    return width * 0.5f;
}

static pf_status apply_battlefield_stage(struct content *content)
{
    const ssbm_stage_collision_profile *profile =
        ssbm_reference_stage_collision(
            (uint16_t)PF_M4_REFERENCE_STAGE_BATTLEFIELD);
    const ssbm_stage_spawn_point *spawn;
    const ssbm_stage_collision_line *left_edge;
    const ssbm_stage_collision_line *main_floor;
    const ssbm_stage_collision_line *side_platform;
    const ssbm_stage_collision_line *top_platform;
    const ssbm_stage_collision_line *right_edge;
    stage_data *stage;

    if (profile == NULL || profile->line_count != UINT16_C(23) ||
        profile->floor_start != UINT16_C(0) ||
        profile->floor_count != UINT16_C(6) ||
        profile->spawn_point_count != UINT8_C(4) ||
        profile->source_grkind != UINT16_C(36))
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }
    spawn = &profile->spawn_points[0];
    left_edge = &profile->lines[0];
    main_floor = &profile->lines[1];
    side_platform = &profile->lines[2];
    top_platform = &profile->lines[3];
    right_edge = &profile->lines[5];
    if (spawn->support == UINT8_C(0) ||
        left_edge->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
        main_floor->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
        side_platform->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
        top_platform->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
        right_edge->kind != (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR)
    {
        return PF_STATUS_DETERMINISTIC_FAULT;
    }

    stage = &content->stage;
    stage->reference_collision_profile =
        (uint16_t)PF_M4_REFERENCE_STAGE_BATTLEFIELD;
    stage->floor_left_f32 =
        left_edge->start_x_f32 < left_edge->end_x_f32
            ? left_edge->start_x_f32
            : left_edge->end_x_f32;
    stage->floor_right_f32 =
        right_edge->start_x_f32 > right_edge->end_x_f32
            ? right_edge->start_x_f32
            : right_edge->end_x_f32;
    stage->floor_y_f32 = ssbm_stage_line_y_f32(
        main_floor,
        stage_line_center_x_f32(main_floor));
    stage->platform_center_x_f32 =
        stage_line_center_x_f32(top_platform);
    stage->platform_y_f32 = ssbm_stage_line_y_f32(
        top_platform,
        stage->platform_center_x_f32);
    stage->platform_half_width_f32 =
        stage_line_half_width_f32(top_platform);
    stage->platform_motion_amplitude_f32 = INT32_C(0);
    stage->upper_platform_center_x_f32 =
        stage_line_center_x_f32(side_platform);
    stage->upper_platform_y_f32 = ssbm_stage_line_y_f32(
        side_platform,
        stage->upper_platform_center_x_f32);
    stage->upper_platform_half_width_f32 =
        stage_line_half_width_f32(side_platform);
    stage->solid_left_f32 = -PF_F32_ONE;
    stage->solid_right_f32 = PF_F32_ONE;
    stage->solid_top_f32 = stage->floor_y_f32 - INT32_C(3) * PF_F32_ONE;
    stage->solid_bottom_f32 = stage->floor_y_f32 - PF_F32_ONE;
    stage->blast_left_f32 = profile->blast_left_f32;
    stage->blast_right_f32 = profile->blast_right_f32;
    stage->blast_top_f32 = profile->blast_top_f32;
    stage->blast_bottom_f32 = profile->blast_bottom_f32;
    stage->spawn_spacing_f32 = PF_F32_ONE;
    stage->platform_motion_period_ticks = UINT16_C(120);
    stage->reference_spawn_line = (uint16_t)spawn->support - UINT16_C(1);
    stage->reference_spawn_x_f32 = spawn->position_x_f32;
    stage->revival_platform_start_y_f32 = profile->camera_top_f32;
    /* Battlefield's Rebirth target root is source-world Y=80. */
    stage->revival_platform_end_y_f32 = 5.8064575f;
    stage->revival_platform_half_width_f32 = INT32_C(2) * PF_F32_ONE;
    return PF_STATUS_OK;
}

pf_status reference_stage_content(
    enum reference_stage reference_stage,
    struct content *out_content)
{
    pf_status status;

    if (out_content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    status = default_content(out_content);
    if (status != PF_STATUS_OK ||
        reference_stage == PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        return status;
    }
    if (reference_stage != PF_M4_REFERENCE_STAGE_BATTLEFIELD)
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    status = apply_battlefield_stage(out_content);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    return validate_content(out_content);
}

pf_status validate_content(const struct content *content)
{
    const fighter_data *fighter;
    const stage_data *stage;
    const ssbm_stage_collision_profile *reference_stage;
    const ssbm_stage_collision_line *reference_spawn_line;
    const item_data *item;
    const projectile_data *projectile;
    const reflector_data *reflector;
    const charge_data *charge;
    const recovery_data *recovery;
    const float maximum_coordinate_f32 = 4096.0f;
    const float maximum_fighter_extent_f32 = 64.0f;
    float platform_left_extent;
    float platform_right_extent;
    float spawn_left_extent;
    float spawn_right_extent;
    float revival_left_extent;
    float revival_right_extent;
    float upper_platform_left_extent;
    float upper_platform_right_extent;
    float maximum_dash_attack_knockback_x;
    float maximum_dash_attack_knockback_y;
    float maximum_jab_knockback_x;
    float maximum_jab_knockback_y;
    float maximum_jab_final_knockback_x;
    float maximum_jab_final_knockback_y;
    float maximum_strong_knockback_x;
    float maximum_strong_knockback_y;
    float maximum_aerial_knockback_x;
    float maximum_aerial_knockback_y;
    float maximum_getup_attack_knockback_x;
    float maximum_getup_attack_knockback_y;
    float maximum_item_knockback_x;
    float maximum_item_knockback_y;
    float maximum_projectile_knockback_x;
    float maximum_projectile_knockback_y;
    float maximum_reflector_knockback_x;
    float maximum_reflector_knockback_y;
    float maximum_charge_knockback_x;
    float maximum_charge_knockback_y;
    uint32_t stale_index;
    float stale_reduction_total_f32 = UINT32_C(0);
    int solid_overlaps_platform;
    int upper_overlaps_platform;
    int upper_overlaps_revival;
    int upper_overlaps_solid;
    uint32_t reference_spawn_index;

    if (content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (content->struct_size != (uint32_t)sizeof(*content) ||
        content->schema_version != PF_M4_CONTENT_SCHEMA_VERSION ||
        content->fighter.struct_size !=
            (uint32_t)sizeof(content->fighter) ||
        content->fighter.schema_version !=
            PF_M4_FIGHTER_SCHEMA_VERSION ||
        content->stage.struct_size !=
            (uint32_t)sizeof(content->stage) ||
        content->stage.schema_version != PF_M4_STAGE_SCHEMA_VERSION ||
        content->item.struct_size !=
            (uint32_t)sizeof(content->item) ||
        content->item.schema_version != PF_M4_ITEM_SCHEMA_VERSION ||
        content->projectile.struct_size !=
            (uint32_t)sizeof(content->projectile) ||
        content->projectile.schema_version !=
            PF_M4_PROJECTILE_SCHEMA_VERSION ||
        content->reflector.struct_size !=
            (uint32_t)sizeof(content->reflector) ||
        content->reflector.schema_version !=
            PF_M4_REFLECTOR_SCHEMA_VERSION ||
        content->charge.struct_size !=
            (uint32_t)sizeof(content->charge) ||
        content->charge.schema_version !=
            PF_M4_CHARGE_SCHEMA_VERSION ||
        content->recovery.struct_size !=
            (uint32_t)sizeof(content->recovery) ||
        content->recovery.schema_version !=
            PF_M4_RECOVERY_SCHEMA_VERSION)
    {
        return PF_STATUS_UNSUPPORTED_VERSION;
    }
    if (content->fighter_count != PF_M4_PLACEHOLDER_FIGHTER_COUNT ||
        content->stage_count != PF_M4_TEST_STAGE_COUNT ||
        content->item_count != PF_M4_TEST_ITEM_COUNT ||
        content->projectile_count != PF_M4_TEST_PROJECTILE_COUNT ||
        content->reflector_count != PF_M4_TEST_REFLECTOR_COUNT ||
        content->charge_count != PF_M4_TEST_CHARGE_COUNT ||
        content->recovery_count != PF_M4_TEST_RECOVERY_COUNT ||
        content->gameplay_ruleset >
            (uint8_t)PF_M4_GAMEPLAY_RULESET_SSBM_NTSC102_UCF084 ||
        content->fighter.reference_frame_data_enabled > UINT8_C(1) ||
        content->fighter.reserved != UINT8_C(0) ||
        content->fighter.smash_charge_reserved != UINT16_C(0) ||
        content->fighter.reserved2 != UINT8_C(0) ||
        content->item.reserved != UINT8_C(0) ||
        content->item.reserved2 != UINT16_C(0) ||
        content->projectile.reserved != UINT8_C(0) ||
        content->reflector.reserved != UINT8_C(0) ||
        content->charge.reserved != UINT8_C(0) ||
        content->recovery.reserved != UINT8_C(0) ||
        content->recovery.reserved2 != UINT16_C(0))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    fighter = &content->fighter;
    for (stale_index = UINT32_C(0);
         stale_index <
             (uint32_t)PF_SIM_STALE_MOVE_QUEUE_CAPACITY;
         ++stale_index)
    {
        const float reduction =
            fighter->stale_move_slot_reduction_f32[stale_index];

        if (reduction == UINT16_C(0) ||
            (stale_index != UINT32_C(0) &&
             reduction >= fighter->stale_move_slot_reduction_f32[
                              stale_index - UINT32_C(1)]))
        {
            return PF_STATUS_INVALID_CONFIG;
        }
        stale_reduction_total_f32 += reduction;
    }
    if (stale_reduction_total_f32 >
        PF_F32_ONE * 0.5f)
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    if (!throw_data_is_valid(&fighter->forward_throw) ||
        !throw_data_is_valid(&fighter->back_throw) ||
        !throw_data_is_valid(&fighter->up_throw) ||
        !throw_data_is_valid(&fighter->down_throw) ||
        !attack_data_is_valid(
            &fighter->up_attack,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->down_attack,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->forward_attack,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->forward_strong_attack,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->up_strong_attack,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->down_strong_attack,
            maximum_fighter_extent_f32) ||
        fighter->smash_charge_damage_bonus_f32 == 0.0f ||
        fighter->smash_charge_damage_bonus_f32 >
            PF_F32_ONE ||
        fighter->smash_charge_max_ticks == UINT16_C(0) ||
        fighter->smash_charge_max_ticks > UINT16_C(600) ||
        !charged_attack_damage_is_valid(
            &fighter->forward_strong_attack,
            fighter->smash_charge_damage_bonus_f32) ||
        !charged_attack_damage_is_valid(
            &fighter->up_strong_attack,
            fighter->smash_charge_damage_bonus_f32) ||
        !charged_attack_damage_is_valid(
            &fighter->down_strong_attack,
            fighter->smash_charge_damage_bonus_f32) ||
        !attack_data_is_valid(
            &fighter->forward_aerial,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->back_aerial,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->up_aerial,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->down_aerial,
            maximum_fighter_extent_f32) ||
        !attack_data_is_valid(
            &fighter->ledge_attack,
            maximum_fighter_extent_f32))
    {
        return PF_STATUS_INVALID_CONFIG;
    }
    maximum_dash_attack_knockback_x =
        maximum_knockback_f32(
            fighter->dash_attack_base_knockback_x_f32,
            fighter->dash_attack_knockback_growth_f32,
            0);
    maximum_dash_attack_knockback_y =
        maximum_knockback_f32(
            fighter->dash_attack_base_knockback_y_f32,
            fighter->dash_attack_knockback_growth_f32,
            1);
    maximum_jab_knockback_x =
        maximum_knockback_f32(
            fighter->jab_base_knockback_x_f32,
            fighter->jab_knockback_growth_f32,
            0);
    maximum_jab_knockback_y =
        maximum_knockback_f32(
            fighter->jab_base_knockback_y_f32,
            fighter->jab_knockback_growth_f32,
            1);
    maximum_jab_final_knockback_x =
        maximum_knockback_f32(
            fighter->jab_final_base_knockback_x_f32,
            fighter->jab_final_knockback_growth_f32,
            0);
    maximum_jab_final_knockback_y =
        maximum_knockback_f32(
            fighter->jab_final_base_knockback_y_f32,
            fighter->jab_final_knockback_growth_f32,
            1);
    maximum_strong_knockback_x =
        maximum_knockback_f32(
            fighter->strong_base_knockback_x_f32,
            fighter->strong_knockback_growth_f32,
            0);
    maximum_strong_knockback_y =
        maximum_knockback_f32(
            fighter->strong_base_knockback_y_f32,
            fighter->strong_knockback_growth_f32,
            1);
    maximum_aerial_knockback_x =
        maximum_knockback_f32(
            fighter->aerial_base_knockback_x_f32,
            fighter->aerial_knockback_growth_f32,
            0);
    maximum_aerial_knockback_y =
        maximum_knockback_f32(
            fighter->aerial_base_knockback_y_f32,
            fighter->aerial_knockback_growth_f32,
            1);
    maximum_getup_attack_knockback_x =
        maximum_knockback_f32(
            fighter->getup_attack_base_knockback_x_f32,
            fighter->getup_attack_knockback_growth_f32,
            0);
    maximum_getup_attack_knockback_y =
        maximum_knockback_f32(
            fighter->getup_attack_base_knockback_y_f32,
            fighter->getup_attack_knockback_growth_f32,
            1);
    if (fighter->half_width_f32 <= INT32_C(0) ||
        fighter->half_height_f32 <= INT32_C(0) ||
        fighter->half_width_f32 > maximum_fighter_extent_f32 ||
        fighter->half_height_f32 > maximum_fighter_extent_f32 ||
        fighter->player_push_half_width_f32 <= INT32_C(0) ||
        fighter->player_push_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->player_push_speed_f32 <= INT32_C(0) ||
        fighter->player_push_speed_f32 >
            maximum_fighter_extent_f32 ||
        fighter->weight_f32 < PF_F32_ONE / INT32_C(2) ||
        fighter->weight_f32 > INT32_C(2) * PF_F32_ONE ||
        fighter->ground_acceleration_f32 <= INT32_C(0) ||
        fighter->turn_acceleration_f32 <
            fighter->ground_acceleration_f32 ||
        fighter->traction_f32 <= INT32_C(0) ||
        fighter->walk_speed_f32 <= INT32_C(0) ||
        fighter->run_speed_f32 <= fighter->walk_speed_f32 ||
        fighter->initial_dash_speed_f32 <= INT32_C(0) ||
        fighter->walk_initial_velocity_f32 <= INT32_C(0) ||
        fighter->walk_acceleration_f32 <= INT32_C(0) ||
        !velocity_animation_scaling_is_valid(
            fighter->slow_walk_animation_scaling_f32) ||
        !velocity_animation_scaling_is_valid(
            fighter->middle_walk_animation_scaling_f32) ||
        !velocity_animation_scaling_is_valid(
            fighter->fast_walk_animation_scaling_f32) ||
        !velocity_animation_scaling_is_valid(
            fighter->run_animation_scaling_f32) ||
        fighter->walk_middle_speed_ratio_f32 == UINT16_C(0) ||
        fighter->walk_middle_speed_ratio_f32 >=
            fighter->walk_fast_speed_ratio_f32 ||
        fighter->dash_run_base_acceleration_f32 <= INT32_C(0) ||
        fighter->ground_max_horizontal_speed_f32 <
            fighter->initial_dash_speed_f32 ||
        fighter->walk_acceleration_taper_f32 <= INT32_C(0) ||
        fighter->walk_acceleration_taper_f32 > PF_F32_ONE ||
        fighter->run_acceleration_taper_f32 <= INT32_C(0) ||
        fighter->run_acceleration_taper_f32 > PF_F32_ONE ||
        fighter->teeter_snap_distance_f32 <= INT32_C(0) ||
        fighter->crouch_step_speed_f32 < INT32_C(0) ||
        fighter->crouch_step_speed_f32 > fighter->walk_speed_f32 ||
        fighter->air_acceleration_f32 <= INT32_C(0) ||
        fighter->air_base_acceleration_f32 <= INT32_C(0) ||
        fighter->air_friction_f32 <= INT32_C(0) ||
        fighter->air_max_horizontal_speed_f32 <
            fighter->air_speed_f32 ||
        fighter->air_speed_f32 <= INT32_C(0) ||
        fighter->jump_horizontal_input_speed_f32 <= INT32_C(0) ||
        fighter->jump_horizontal_momentum_multiplier_f32 <= INT32_C(0) ||
        fighter->jump_horizontal_momentum_multiplier_f32 > PF_F32_ONE ||
        fighter->jump_horizontal_max_speed_f32 <
            fighter->jump_horizontal_input_speed_f32 ||
        fighter->gravity_f32 <= INT32_C(0) ||
        fighter->fall_speed_f32 <= fighter->gravity_f32 ||
        fighter->fast_fall_speed_f32 <= fighter->fall_speed_f32 ||
        fighter->full_hop_speed_f32 <=
            fighter->short_hop_speed_f32 ||
        fighter->short_hop_speed_f32 <= INT32_C(0) ||
        fighter->double_jump_speed_f32 <= INT32_C(0) ||
        fighter->double_jump_horizontal_speed_f32 <= INT32_C(0) ||
        fighter->double_jump_horizontal_speed_f32 >
            fighter->air_max_horizontal_speed_f32 ||
        fighter->platform_drop_nudge_f32 <= INT32_C(0) ||
        fighter->platform_drop_speed_y_f32 <= fighter->gravity_f32 ||
        fighter->platform_drop_speed_y_f32 > fighter->fall_speed_f32 ||
        fighter->ledge_jump_speed_x_f32 <= INT32_C(0) ||
        fighter->ledge_jump_speed_y_f32 <= fighter->gravity_f32 ||
        fighter->ledge_roll_distance_f32 <=
            fighter->half_width_f32 +
                fighter->platform_drop_nudge_f32 ||
        fighter->ledge_roll_distance_f32 >
            INT32_C(8) * PF_F32_ONE ||
        fighter->drop_cancel_snap_distance_f32 <=
            fighter->platform_drop_nudge_f32 ||
        fighter->drop_cancel_snap_distance_f32 >
            fighter->half_height_f32 ||
        fighter->air_dodge_speed_x_f32 <= INT32_C(0) ||
        fighter->air_dodge_speed_y_f32 <= INT32_C(0) ||
        fighter->air_dodge_decay_f32 <= INT32_C(0) ||
        fighter->air_dodge_decay_f32 > PF_F32_ONE ||
        fighter->fall_special_mobility_f32 <= INT32_C(0) ||
        fighter->fall_special_mobility_f32 >
            fighter->air_speed_f32 ||
        fighter->dash_attack_speed_f32 <=
            fighter->initial_dash_speed_f32 ||
        fighter->dash_attack_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->dash_attack_hitbox_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->dash_attack_hitbox_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->dash_attack_hitbox_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->dash_attack_hitbox_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->dash_attack_hitbox_half_width_f32 <= INT32_C(0) ||
        fighter->dash_attack_hitbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->dash_attack_hitbox_half_height_f32 <= INT32_C(0) ||
        fighter->dash_attack_hitbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->dash_attack_damage_f32 == UINT32_C(0) ||
        fighter->dash_attack_damage_f32 >
            50.0f ||
        fighter->dash_attack_base_knockback_x_f32 <= INT32_C(0) ||
        fighter->dash_attack_base_knockback_y_f32 <= INT32_C(0) ||
        fighter->dash_attack_knockback_growth_f32 <= INT32_C(0) ||
        fighter->jab_hitbox_offset_x_f32 < -maximum_fighter_extent_f32 ||
        fighter->jab_hitbox_offset_x_f32 > maximum_fighter_extent_f32 ||
        fighter->jab_hitbox_offset_y_f32 < -maximum_fighter_extent_f32 ||
        fighter->jab_hitbox_offset_y_f32 > maximum_fighter_extent_f32 ||
        fighter->jab_hitbox_half_width_f32 <= INT32_C(0) ||
        fighter->jab_hitbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->jab_hitbox_half_height_f32 <= INT32_C(0) ||
        fighter->jab_hitbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->jab_damage_f32 == UINT32_C(0) ||
        fighter->jab_damage_f32 >
            50.0f ||
        fighter->jab_base_knockback_x_f32 <= INT32_C(0) ||
        fighter->jab_base_knockback_y_f32 <= INT32_C(0) ||
        fighter->jab_knockback_growth_f32 <= INT32_C(0) ||
        fighter->jab_melee_knockback.angle_degrees > UINT16_C(361) ||
        fighter->jab_melee_knockback.growth == UINT16_C(0) ||
        fighter->jab_melee_knockback.growth > UINT16_C(1000) ||
        fighter->jab_melee_knockback.weight_set > UINT16_C(1000) ||
        fighter->jab_melee_knockback.base > UINT16_C(1000) ||
        fighter->jab_melee_knockback.enabled > UINT8_C(1) ||
        fighter->jab_melee_knockback.reserved[0] != UINT8_C(0) ||
        fighter->jab_melee_knockback.reserved[1] != UINT8_C(0) ||
        fighter->jab_melee_knockback.reserved[2] != UINT8_C(0) ||
        fighter->jab_final_hitbox_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->jab_final_hitbox_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->jab_final_hitbox_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->jab_final_hitbox_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->jab_final_hitbox_half_width_f32 <= INT32_C(0) ||
        fighter->jab_final_hitbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->jab_final_hitbox_half_height_f32 <= INT32_C(0) ||
        fighter->jab_final_hitbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->jab_final_damage_f32 == UINT32_C(0) ||
        fighter->jab_final_damage_f32 >
            50.0f ||
        fighter->jab_final_base_knockback_x_f32 <= INT32_C(0) ||
        fighter->jab_final_base_knockback_y_f32 <= INT32_C(0) ||
        fighter->jab_final_knockback_growth_f32 <= INT32_C(0) ||
        fighter->jab_final_melee_knockback.angle_degrees > UINT16_C(361) ||
        fighter->jab_final_melee_knockback.growth == UINT16_C(0) ||
        fighter->jab_final_melee_knockback.growth > UINT16_C(1000) ||
        fighter->jab_final_melee_knockback.weight_set > UINT16_C(1000) ||
        fighter->jab_final_melee_knockback.base > UINT16_C(1000) ||
        fighter->jab_final_melee_knockback.enabled > UINT8_C(1) ||
        fighter->jab_final_melee_knockback.reserved[0] != UINT8_C(0) ||
        fighter->jab_final_melee_knockback.reserved[1] != UINT8_C(0) ||
        fighter->jab_final_melee_knockback.reserved[2] != UINT8_C(0) ||
        fighter->reset_max_damage_f32 == UINT32_C(0) ||
        fighter->reset_max_damage_f32 >
            7.0f ||
        fighter->reset_bound_speed_f32 <= INT32_C(0) ||
        fighter->reset_bound_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->strong_hitbox_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->strong_hitbox_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->strong_hitbox_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->strong_hitbox_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->strong_hitbox_half_width_f32 <= INT32_C(0) ||
        fighter->strong_hitbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->strong_hitbox_half_height_f32 <= INT32_C(0) ||
        fighter->strong_hitbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->strong_damage_f32 == UINT32_C(0) ||
        fighter->strong_damage_f32 >
            50.0f ||
        fighter->strong_base_knockback_x_f32 <= INT32_C(0) ||
        fighter->strong_base_knockback_y_f32 <= INT32_C(0) ||
        fighter->strong_knockback_growth_f32 <= INT32_C(0) ||
        fighter->aerial_hitbox_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->aerial_hitbox_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->aerial_hitbox_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->aerial_hitbox_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->aerial_hitbox_half_width_f32 <= INT32_C(0) ||
        fighter->aerial_hitbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->aerial_hitbox_half_height_f32 <= INT32_C(0) ||
        fighter->aerial_hitbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->aerial_damage_f32 == UINT32_C(0) ||
        fighter->aerial_damage_f32 >
            50.0f ||
        fighter->aerial_base_knockback_x_f32 <= INT32_C(0) ||
        fighter->aerial_base_knockback_y_f32 <= INT32_C(0) ||
        fighter->aerial_knockback_growth_f32 <= INT32_C(0) ||
        fighter->forward_roll_speed_f32 <= INT32_C(0) ||
        fighter->forward_roll_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->backward_roll_speed_f32 <= INT32_C(0) ||
        fighter->backward_roll_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->getup_attack_hitbox_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->getup_attack_hitbox_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->getup_attack_hitbox_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->getup_attack_hitbox_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->getup_attack_hitbox_half_width_f32 <=
            INT32_C(0) ||
        fighter->getup_attack_hitbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->getup_attack_hitbox_half_height_f32 <=
            INT32_C(0) ||
        fighter->getup_attack_hitbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->getup_attack_damage_f32 == UINT32_C(0) ||
        fighter->getup_attack_damage_f32 >
            50.0f ||
        fighter->getup_attack_base_knockback_x_f32 <= INT32_C(0) ||
        fighter->getup_attack_base_knockback_y_f32 <= INT32_C(0) ||
        fighter->getup_attack_knockback_growth_f32 <= INT32_C(0) ||
        fighter->hitstun_velocity_per_tick_f32 <= INT32_C(0) ||
        fighter->v_cancel_velocity_scale_f32 <= INT32_C(0) ||
        fighter->v_cancel_velocity_scale_f32 >= PF_F32_ONE ||
        fighter->knockback_weight == UINT16_C(0) ||
        fighter->knockback_weight > UINT16_C(1000) ||
        fighter->knockback_reserved != UINT16_C(0) ||
        fighter->crouch_cancel_max_damage_f32 == UINT32_C(0) ||
        fighter->crouch_cancel_max_damage_f32 >
            PF_SIM_MAX_DAMAGE_F32 ||
        fighter->crouch_cancel_velocity_scale_f32 <= INT32_C(0) ||
        fighter->crouch_cancel_velocity_scale_f32 >= PF_F32_ONE ||
        fighter->crouch_cancel_hitstun_scale_f32 <= INT32_C(0) ||
        fighter->crouch_cancel_hitstun_scale_f32 >= PF_F32_ONE ||
        fighter->di_max_angle_radians_q30 <= INT32_C(0) ||
        fighter->di_max_angle_radians_q30 > INT32_C(1073741824) ||
        fighter->ground_knockback_decay_scale_f32 <= INT32_C(0) ||
        fighter->ground_knockback_decay_scale_f32 > PF_F32_ONE ||
        fighter->air_knockback_decay_f32 <= INT32_C(0) ||
        fighter->air_knockback_decay_f32 > PF_F32_ONE ||
        fighter->sdi_distance_x_f32 <= INT32_C(0) ||
        fighter->sdi_distance_x_f32 >
            INT32_C(4) * PF_F32_ONE ||
        fighter->sdi_distance_y_f32 <= INT32_C(0) ||
        fighter->sdi_distance_y_f32 >
            INT32_C(4) * PF_F32_ONE ||
        fighter->asdi_distance_x_f32 <= INT32_C(0) ||
        fighter->asdi_distance_x_f32 >
            fighter->sdi_distance_x_f32 ||
        fighter->asdi_distance_y_f32 <= INT32_C(0) ||
        fighter->asdi_distance_y_f32 >
            fighter->sdi_distance_y_f32 ||
        fighter->shield_sdi_scale_f32 <= INT32_C(0) ||
        fighter->shield_sdi_scale_f32 > PF_F32_ONE ||
        fighter->tech_roll_speed_f32 <= INT32_C(0) ||
        fighter->tech_roll_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->wall_tech_speed_f32 <= INT32_C(0) ||
        fighter->wall_tech_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->wall_tech_jump_speed_x_f32 <= INT32_C(0) ||
        fighter->wall_tech_jump_speed_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->wall_tech_jump_speed_y_f32 <= fighter->gravity_f32 ||
        fighter->wall_tech_jump_speed_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->wall_jump_speed_x_f32 <= INT32_C(0) ||
        fighter->wall_jump_speed_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->wall_jump_speed_y_f32 <= fighter->gravity_f32 ||
        fighter->wall_jump_speed_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->ceiling_tech_speed_f32 <= INT32_C(0) ||
        fighter->ceiling_tech_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->surface_collision_threshold_x_f32 <= INT32_C(0) ||
        fighter->surface_collision_threshold_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->surface_collision_threshold_y_f32 <= INT32_C(0) ||
        fighter->surface_collision_threshold_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->surface_bounce_multiplier_f32 <= INT32_C(0) ||
        fighter->surface_bounce_multiplier_f32 > PF_F32_ONE ||
        fighter->shield_health_f32 == UINT32_C(0) ||
        fighter->shield_health_f32 >
            PF_SIM_MAX_SHIELD_HEALTH_F32 ||
        fighter->shield_reset_health_f32 == UINT32_C(0) ||
        fighter->shield_reset_health_f32 >
            fighter->shield_health_f32 ||
        fighter->shield_hold_depletion_f32 == UINT32_C(0) ||
        fighter->shield_hold_depletion_f32 >
            fighter->shield_health_f32 ||
        fighter->light_shield_hold_depletion_f32 ==
            UINT32_C(0) ||
        fighter->light_shield_hold_depletion_f32 >
            fighter->shield_hold_depletion_f32 ||
        fighter->shield_regeneration_f32 == UINT32_C(0) ||
        fighter->shield_regeneration_f32 >
            fighter->shield_health_f32 ||
        fighter->light_shield_damage_multiplier_f32 == UINT32_C(0) ||
        fighter->light_shield_damage_multiplier_f32 >
            2.0f ||
        fighter->dense_shield_damage_multiplier_f32 == UINT32_C(0) ||
        fighter->dense_shield_damage_multiplier_f32 >
            fighter->light_shield_damage_multiplier_f32 ||
        fighter->light_shield_stun_damage_multiplier_f32 <= INT32_C(0) ||
        fighter->light_shield_stun_damage_multiplier_f32 >
            INT32_C(2) * PF_F32_ONE ||
        fighter->dense_shield_stun_damage_multiplier_f32 <= INT32_C(0) ||
        fighter->dense_shield_stun_damage_multiplier_f32 >
            fighter->light_shield_stun_damage_multiplier_f32 ||
        fighter->shield_stun_base_f32 <= INT32_C(0) ||
        fighter->shield_stun_base_f32 >
            INT32_C(16) * PF_F32_ONE ||
        fighter->shield_defender_pushback_stun_scale_f32 <=
            INT32_C(0) ||
        fighter->shield_defender_pushback_stun_scale_f32 >
            PF_F32_ONE ||
        fighter->shield_defender_pushback_normal_scale_f32 <=
            INT32_C(0) ||
        fighter->shield_defender_pushback_normal_scale_f32 >
            PF_F32_ONE ||
        fighter->shield_defender_pushback_max_f32 <= INT32_C(0) ||
        fighter->shield_defender_pushback_max_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->shield_attacker_pushback_damage_f32 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_damage_f32 >
            PF_F32_ONE ||
        fighter->shield_attacker_pushback_base_f32 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_base_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->shield_attacker_pushback_air_decay_f32 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_air_decay_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->shield_attacker_pushback_ground_friction_scale_f32 <=
            INT32_C(0) ||
        fighter->shield_attacker_pushback_ground_friction_scale_f32 >
            INT32_C(2) * PF_F32_ONE ||
        fighter->shield_radius_x_f32 <= INT32_C(0) ||
        fighter->shield_radius_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->shield_radius_y_f32 <= INT32_C(0) ||
        fighter->shield_radius_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->shield_minimum_size_scale_f32 <= INT32_C(0) ||
        fighter->shield_minimum_size_scale_f32 >=
            fighter->dense_shield_size_scale_f32 ||
        fighter->dense_shield_size_scale_f32 > PF_F32_ONE ||
        fighter->shield_center_forward_f32 < INT32_C(0) ||
        fighter->shield_center_forward_f32 >
            maximum_fighter_extent_f32 ||
        fighter->shield_center_up_f32 < INT32_C(0) ||
        fighter->shield_center_up_f32 >
            maximum_fighter_extent_f32 ||
        fighter->shield_animation_scale_x_f32 < INT32_C(0) ||
        fighter->shield_animation_scale_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->shield_animation_scale_y_f32 < INT32_C(0) ||
        fighter->shield_animation_scale_y_f32 >
            maximum_fighter_extent_f32 ||
        maximum_dash_attack_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_dash_attack_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_jab_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_jab_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_jab_final_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_jab_final_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_strong_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_strong_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_aerial_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_aerial_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_getup_attack_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_getup_attack_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->ground_acceleration_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->turn_acceleration_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->traction_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->walk_speed_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->run_speed_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->initial_dash_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->walk_initial_velocity_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->walk_acceleration_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->slow_walk_animation_scaling_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->middle_walk_animation_scaling_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->fast_walk_animation_scaling_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->run_animation_scaling_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->dash_run_base_acceleration_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->ground_max_horizontal_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->teeter_snap_distance_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->crouch_step_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_acceleration_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_base_acceleration_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_friction_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_max_horizontal_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_speed_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->jump_horizontal_input_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->jump_horizontal_max_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->gravity_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->fall_speed_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->fast_fall_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->full_hop_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->double_jump_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->double_jump_horizontal_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->platform_drop_nudge_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->ledge_jump_speed_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->ledge_jump_speed_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_dodge_speed_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->air_dodge_speed_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->fall_special_mobility_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->jab_base_knockback_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->jab_base_knockback_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->strong_base_knockback_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->strong_base_knockback_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->aerial_base_knockback_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->aerial_base_knockback_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->getup_attack_base_knockback_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->getup_attack_base_knockback_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->hitstun_velocity_per_tick_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->jump_squat_ticks == UINT16_C(0) ||
        fighter->jump_squat_ticks > UINT16_C(60) ||
        fighter->double_jump_cancel_ticks > UINT16_C(120) ||
        fighter->double_jump_armor_max_hitstun_ticks >
            PF_SIM_MAX_HITSTUN_TICKS ||
        (fighter->double_jump_cancel_ticks == UINT16_C(0) &&
         fighter->double_jump_armor_max_hitstun_ticks !=
             UINT16_C(0)) ||
        fighter->initial_dash_ticks == UINT16_C(0) ||
        fighter->initial_dash_ticks > UINT16_C(120) ||
        fighter->dash_run_transition_ticks < UINT16_C(2) ||
        fighter->dash_run_transition_ticks >=
            fighter->initial_dash_ticks ||
        fighter->standing_turn_ticks < UINT16_C(2) ||
        fighter->standing_turn_ticks > UINT16_C(120) ||
        fighter->standing_turn_facing_tick < UINT16_C(2) ||
        fighter->standing_turn_facing_tick >=
            fighter->standing_turn_ticks ||
        fighter->dash_input_window_ticks == UINT16_C(0) ||
        fighter->dash_input_window_ticks >
            fighter->initial_dash_ticks ||
        fighter->teeter_ticks == UINT16_C(0) ||
        fighter->teeter_ticks > UINT16_C(120) ||
        fighter->teeter_turn_axis_threshold == UINT16_C(0) ||
        fighter->teeter_turn_axis_threshold >=
            fighter->teeter_walk_axis_threshold ||
        fighter->teeter_walk_axis_threshold > UINT16_C(32767) ||
        fighter->walk_axis_threshold == UINT16_C(0) ||
        fighter->walk_axis_threshold >= fighter->teeter_turn_axis_threshold ||
        fighter->crouch_step_ticks == UINT16_C(0) ||
        fighter->crouch_step_ticks > UINT16_C(30) ||
        fighter->crouch_start_ticks == UINT16_C(0) ||
        fighter->crouch_start_ticks > UINT16_C(120) ||
        fighter->crouch_end_ticks == UINT16_C(0) ||
        fighter->crouch_end_ticks > UINT16_C(120) ||
        fighter->taunt_ticks == UINT16_C(0) ||
        fighter->taunt_ticks > UINT16_C(600) ||
        fighter->forward_smash_input_window_ticks == UINT16_C(0) ||
        fighter->forward_smash_input_window_ticks >
            fighter->initial_dash_ticks ||
        fighter->landing_ticks == UINT16_C(0) ||
        fighter->landing_ticks > UINT16_C(120) ||
        fighter->landing_interruptible_tick == UINT16_C(0) ||
        fighter->landing_interruptible_tick >= fighter->landing_ticks ||
        fighter->platform_drop_ticks == UINT16_C(0) ||
        fighter->platform_drop_ticks > UINT16_C(120) ||
        fighter->platform_drop_startup_ticks == UINT16_C(0) ||
        fighter->platform_drop_startup_ticks >=
            fighter->crouch_start_ticks ||
        fighter->air_dodge_ticks == UINT16_C(0) ||
        fighter->air_dodge_ticks > UINT16_C(240) ||
        fighter->air_dodge_invulnerability_begin_tick >=
            fighter->air_dodge_invulnerability_end_tick ||
        fighter->air_dodge_invulnerability_end_tick >
            fighter->air_dodge_ticks ||
        fighter->air_dodge_ordinary_physics_begin_tick == UINT16_C(0) ||
        fighter->air_dodge_ordinary_physics_begin_tick >=
            fighter->air_dodge_ticks ||
        fighter->ledge_invulnerability_ticks == UINT16_C(0) ||
        fighter->ledge_invulnerability_ticks > UINT16_C(600) ||
        fighter->ledge_regrab_lockout_ticks == UINT16_C(0) ||
        fighter->ledge_regrab_lockout_ticks > UINT16_C(600) ||
        fighter->ledge_transition_ticks == UINT16_C(0) ||
        fighter->ledge_transition_ticks > UINT16_C(120) ||
        fighter->ledge_roll_ticks == UINT16_C(0) ||
        fighter->ledge_roll_ticks > UINT16_C(240) ||
        fighter->ledge_roll_movement_ticks == UINT16_C(0) ||
        fighter->ledge_roll_movement_ticks >
            fighter->ledge_roll_ticks ||
        fighter->ledge_roll_invulnerability_ticks == UINT16_C(0) ||
        fighter->ledge_roll_invulnerability_ticks >
            fighter->ledge_roll_ticks ||
        fighter->ledge_attack_invulnerability_ticks == UINT16_C(0) ||
        fighter->ledge_attack_invulnerability_ticks >
            (uint32_t)fighter->ledge_attack.startup_ticks +
                (uint32_t)fighter->ledge_attack.active_ticks +
                (uint32_t)fighter->ledge_attack.recovery_ticks ||
        fighter->special_landing_ticks == UINT16_C(0) ||
        fighter->special_landing_ticks > UINT16_C(240) ||
        fighter->run_turnaround_ticks < UINT16_C(2) ||
        fighter->run_turnaround_ticks > UINT16_C(120) ||
        fighter->run_brake_ticks == UINT16_C(0) ||
        fighter->run_brake_ticks > UINT16_C(120) ||
        fighter->axis_dead_zone >= fighter->dash_axis_threshold ||
        fighter->dash_axis_threshold > UINT16_C(32767) ||
        fighter->run_turnaround_axis_threshold <=
            fighter->axis_dead_zone ||
        fighter->run_turnaround_axis_threshold >=
            fighter->run_continue_axis_threshold ||
        fighter->run_continue_axis_threshold >
            fighter->dash_axis_threshold ||
        fighter->run_turnaround_lockout_ticks == UINT16_C(0) ||
        fighter->run_turnaround_lockout_ticks > UINT16_C(120) ||
        fighter->tilt_axis_threshold == UINT16_C(0) ||
        fighter->tilt_axis_threshold >= fighter->axis_dead_zone ||
        fighter->tap_jump_axis_threshold <= fighter->axis_dead_zone ||
        fighter->tap_jump_axis_threshold > UINT16_C(32767) ||
        fighter->tap_jump_input_window_ticks == UINT16_C(0) ||
        fighter->tap_jump_input_window_ticks > UINT16_C(120) ||
        fighter->fast_fall_axis_threshold <=
            fighter->crouch_release_axis_threshold ||
        fighter->fast_fall_axis_threshold > UINT16_C(32767) ||
        fighter->fast_fall_input_window_ticks == UINT16_C(0) ||
        fighter->fast_fall_input_window_ticks > UINT16_C(120) ||
        fighter->air_dodge_dead_zone == UINT16_C(0) ||
        fighter->air_dodge_dead_zone > fighter->axis_dead_zone ||
        fighter->crouch_axis_threshold <= fighter->axis_dead_zone ||
        fighter->crouch_axis_threshold > UINT16_C(32767) ||
        fighter->crouch_release_axis_threshold <=
            fighter->axis_dead_zone ||
        fighter->crouch_release_axis_threshold >=
            fighter->crouch_axis_threshold ||
        fighter->shield_drop_axis_threshold <=
            fighter->axis_dead_zone ||
        fighter->shield_drop_axis_threshold >=
            fighter->crouch_axis_threshold ||
        fighter->dash_attack_startup_ticks == UINT16_C(0) ||
        fighter->dash_attack_startup_ticks > UINT16_C(120) ||
        fighter->dash_attack_active_ticks == UINT16_C(0) ||
        fighter->dash_attack_active_ticks > UINT16_C(120) ||
        fighter->dash_attack_recovery_ticks == UINT16_C(0) ||
        fighter->dash_attack_recovery_ticks > UINT16_C(240) ||
        fighter->dash_attack_hitlag_ticks == UINT16_C(0) ||
        fighter->dash_attack_hitlag_ticks > UINT16_C(120) ||
        (uint32_t)fighter->dash_attack_startup_ticks +
                (uint32_t)fighter->dash_attack_active_ticks +
                (uint32_t)fighter->dash_attack_recovery_ticks >
            UINT32_C(600) ||
        fighter->boost_grab_cancel_begin_tick == UINT16_C(0) ||
        fighter->boost_grab_cancel_begin_tick >
            fighter->boost_grab_cancel_end_tick ||
        fighter->boost_grab_cancel_end_tick >=
            fighter->dash_attack_startup_ticks ||
        fighter->jab_startup_ticks == UINT16_C(0) ||
        fighter->jab_startup_ticks > UINT16_C(120) ||
        fighter->jab_active_ticks == UINT16_C(0) ||
        fighter->jab_active_ticks > UINT16_C(120) ||
        fighter->jab_recovery_ticks == UINT16_C(0) ||
        fighter->jab_recovery_ticks > UINT16_C(240) ||
        fighter->jab_hitlag_ticks == UINT16_C(0) ||
        fighter->jab_hitlag_ticks > UINT16_C(120) ||
        fighter->jab_combo_input_begin_tick <
            fighter->jab_startup_ticks + fighter->jab_active_ticks ||
        fighter->jab_combo_input_begin_tick >
            fighter->jab_combo_input_end_tick ||
        fighter->jab_combo_input_end_tick >=
            fighter->jab_startup_ticks + fighter->jab_active_ticks +
                fighter->jab_recovery_ticks ||
        fighter->jab_final_startup_ticks == UINT16_C(0) ||
        fighter->jab_final_startup_ticks > UINT16_C(120) ||
        fighter->jab_final_active_ticks == UINT16_C(0) ||
        fighter->jab_final_active_ticks > UINT16_C(120) ||
        fighter->jab_final_recovery_ticks == UINT16_C(0) ||
        fighter->jab_final_recovery_ticks > UINT16_C(240) ||
        fighter->jab_final_hitlag_ticks == UINT16_C(0) ||
        fighter->jab_final_hitlag_ticks > UINT16_C(120) ||
        (uint32_t)fighter->jab_final_startup_ticks +
                (uint32_t)fighter->jab_final_active_ticks +
                (uint32_t)fighter->jab_final_recovery_ticks >
            UINT32_C(600) ||
        fighter->reset_max_hitstun_ticks == UINT16_C(0) ||
        fighter->reset_max_hitstun_ticks > UINT16_C(12) ||
        fighter->reset_max_hitstun_ticks >=
            fighter->tumble_hitstun_threshold_ticks ||
        fighter->reset_bound_ticks == UINT16_C(0) ||
        fighter->reset_bound_ticks > UINT16_C(120) ||
        fighter->reset_forced_getup_ticks == UINT16_C(0) ||
        fighter->reset_forced_getup_ticks > UINT16_C(240) ||
        fighter->strong_startup_ticks == UINT16_C(0) ||
        fighter->strong_startup_ticks > UINT16_C(120) ||
        fighter->strong_active_ticks == UINT16_C(0) ||
        fighter->strong_active_ticks > UINT16_C(120) ||
        fighter->strong_recovery_ticks == UINT16_C(0) ||
        fighter->strong_recovery_ticks > UINT16_C(240) ||
        fighter->strong_hitlag_ticks == UINT16_C(0) ||
        fighter->strong_hitlag_ticks > UINT16_C(120) ||
        fighter->aerial_startup_ticks == UINT16_C(0) ||
        fighter->aerial_startup_ticks > UINT16_C(120) ||
        fighter->aerial_active_ticks == UINT16_C(0) ||
        fighter->aerial_active_ticks > UINT16_C(120) ||
        fighter->aerial_recovery_ticks == UINT16_C(0) ||
        fighter->aerial_recovery_ticks > UINT16_C(240) ||
        fighter->aerial_hitlag_ticks == UINT16_C(0) ||
        fighter->aerial_hitlag_ticks > UINT16_C(120) ||
        fighter->platform_drop_ticks <=
            fighter->aerial_startup_ticks + UINT16_C(1) ||
        fighter->platform_drop_ticks >
            fighter->aerial_startup_ticks + UINT16_C(1) +
                fighter->aerial_hitlag_ticks ||
        fighter->aerial_landing_lag_begin_tick >
            fighter->aerial_startup_ticks ||
        fighter->aerial_landing_lag_end_tick <=
            fighter->aerial_landing_lag_begin_tick ||
        fighter->aerial_landing_lag_end_tick >
            fighter->aerial_startup_ticks +
                fighter->aerial_active_ticks +
                fighter->aerial_recovery_ticks ||
        fighter->aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->forward_aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->forward_aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->back_aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->back_aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->up_aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->up_aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->down_aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->down_aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->strong_aerial_landing_lag_ticks == UINT16_C(0) ||
        fighter->strong_aerial_landing_lag_ticks > UINT16_C(240) ||
        fighter->l_cancel_window_ticks != UINT16_C(7) ||
        fighter->l_cancel_divisor != UINT16_C(2) ||
        fighter->v_cancel_window_ticks == UINT16_C(0) ||
        fighter->v_cancel_window_ticks >
            fighter->tech_lockout_ticks ||
        fighter->sdi_stick_threshold <= fighter->axis_dead_zone ||
        fighter->sdi_stick_threshold > UINT16_C(32767) ||
        fighter->sdi_stick_window_ticks == UINT16_C(0) ||
        fighter->sdi_stick_window_ticks > UINT16_C(254) ||
        fighter->light_shield_trigger_threshold == UINT16_C(0) ||
        fighter->light_shield_trigger_threshold >=
            fighter->digital_trigger_threshold ||
        fighter->digital_trigger_threshold == UINT16_C(0) ||
        fighter->tumble_hitstun_threshold_ticks == UINT16_C(0) ||
        fighter->tumble_hitstun_threshold_ticks >
            PF_SIM_MAX_HITSTUN_TICKS ||
        fighter->tech_window_ticks == UINT16_C(0) ||
        fighter->tech_window_ticks > UINT16_C(120) ||
        fighter->tech_lockout_ticks <
            fighter->tech_window_ticks ||
        fighter->tech_lockout_ticks > UINT16_C(240) ||
        fighter->tech_roll_axis_threshold == UINT16_C(0) ||
        fighter->tech_roll_axis_threshold > UINT16_C(32767) ||
        fighter->tech_in_place_ticks == UINT16_C(0) ||
        fighter->tech_in_place_ticks > UINT16_C(240) ||
        fighter->tech_roll_ticks == UINT16_C(0) ||
        fighter->tech_roll_ticks > UINT16_C(240) ||
        fighter->tech_invulnerability_ticks == UINT16_C(0) ||
        fighter->tech_invulnerability_ticks >
            fighter->tech_in_place_ticks ||
        fighter->tech_invulnerability_ticks >
            fighter->tech_roll_ticks ||
        fighter->wall_tech_stall_ticks == UINT16_C(0) ||
        fighter->wall_tech_stall_ticks >= fighter->wall_tech_ticks ||
        fighter->wall_tech_invulnerability_ticks == UINT16_C(0) ||
        fighter->wall_tech_invulnerability_ticks >
            fighter->wall_tech_ticks ||
        fighter->wall_tech_ticks > UINT16_C(240) ||
        fighter->wall_tech_jump_ticks <=
            fighter->wall_tech_stall_ticks ||
        fighter->wall_tech_jump_ticks > UINT16_C(240) ||
        fighter->surface_bounce_invulnerability_ticks == UINT16_C(0) ||
        fighter->surface_bounce_invulnerability_ticks > UINT16_C(240) ||
        fighter->surface_bounce_collision_lockout_ticks == UINT16_C(0) ||
        fighter->surface_bounce_collision_lockout_ticks >
            fighter->surface_bounce_invulnerability_ticks ||
        fighter->wall_jump_ticks == UINT16_C(0) ||
        fighter->wall_jump_ticks > UINT16_C(240) ||
        fighter->wall_jump_invulnerability_ticks == UINT16_C(0) ||
        fighter->wall_jump_invulnerability_ticks >
            fighter->wall_jump_ticks ||
        fighter->ceiling_tech_ticks == UINT16_C(0) ||
        fighter->ceiling_tech_ticks > UINT16_C(240) ||
        fighter->ceiling_tech_control_tick == UINT16_C(0) ||
        fighter->ceiling_tech_control_tick >=
            fighter->ceiling_tech_ticks ||
        fighter->tech_invulnerability_ticks >
            fighter->wall_tech_ticks ||
        fighter->knockdown_ticks == UINT16_C(0) ||
        fighter->knockdown_ticks > UINT16_C(480) ||
        fighter->down_wait_ticks == UINT16_C(0) ||
        fighter->down_wait_ticks > UINT16_C(480) ||
        fighter->down_horizontal_angle_tan_f32 <= INT32_C(0) ||
        fighter->down_up_axis_threshold == UINT16_C(0) ||
        fighter->down_up_axis_threshold > UINT16_C(32767) ||
        fighter->down_horizontal_axis_threshold == UINT16_C(0) ||
        fighter->down_horizontal_axis_threshold > UINT16_C(32767) ||
        fighter->down_attack_input_window_ticks == UINT16_C(0) ||
        fighter->down_attack_input_window_ticks > UINT16_C(254) ||
        fighter->down_c_up_axis_threshold == UINT16_C(0) ||
        fighter->down_c_up_axis_threshold > UINT16_C(32767) ||
        fighter->getup_neutral_ticks == UINT16_C(0) ||
        fighter->getup_neutral_ticks > UINT16_C(240) ||
        fighter->getup_neutral_invulnerability_ticks ==
            UINT16_C(0) ||
        fighter->getup_neutral_invulnerability_ticks >
            fighter->getup_neutral_ticks ||
        fighter->getup_roll_ticks == UINT16_C(0) ||
        fighter->getup_roll_ticks > UINT16_C(240) ||
        !getup_roll_timing_is_valid(
            &fighter->getup_roll_back_forward,
            fighter->getup_roll_ticks) ||
        !getup_roll_timing_is_valid(
            &fighter->getup_roll_back_backward,
            fighter->getup_roll_ticks) ||
        !getup_roll_timing_is_valid(
            &fighter->getup_roll_stomach_forward,
            fighter->getup_roll_ticks) ||
        !getup_roll_timing_is_valid(
            &fighter->getup_roll_stomach_backward,
            fighter->getup_roll_ticks) ||
        fighter->getup_attack_ticks == UINT16_C(0) ||
        fighter->getup_attack_ticks > UINT16_C(240) ||
        fighter->getup_attack_back_invulnerability_ticks ==
            UINT16_C(0) ||
        fighter->getup_attack_back_invulnerability_ticks >
            fighter->getup_attack_ticks ||
        fighter->getup_attack_stomach_invulnerability_ticks ==
            UINT16_C(0) ||
        fighter->getup_attack_stomach_invulnerability_ticks >
            fighter->getup_attack_ticks ||
        fighter->getup_attack_front_active_begin_tick ==
            UINT16_C(0) ||
        fighter->getup_attack_front_active_begin_tick >
            fighter->getup_attack_front_active_end_tick ||
        fighter->getup_attack_front_active_end_tick >=
            fighter->getup_attack_back_active_begin_tick ||
        fighter->getup_attack_back_active_begin_tick >
            fighter->getup_attack_back_active_end_tick ||
        fighter->getup_attack_back_active_end_tick >
        fighter->getup_attack_ticks ||
        fighter->getup_attack_hitlag_ticks == UINT16_C(0) ||
        fighter->getup_attack_hitlag_ticks > UINT16_C(120) ||
        fighter->forward_roll_ticks == UINT16_C(0) ||
        fighter->forward_roll_ticks > UINT16_C(240) ||
        fighter->backward_roll_ticks == UINT16_C(0) ||
        fighter->backward_roll_ticks > UINT16_C(240) ||
        fighter->roll_movement_begin_tick >=
            fighter->roll_movement_end_tick ||
        fighter->roll_movement_end_tick >
            fighter->forward_roll_ticks ||
        fighter->roll_movement_end_tick >
            fighter->backward_roll_ticks ||
        fighter->roll_invulnerability_begin_tick >=
            fighter->roll_invulnerability_end_tick ||
        fighter->roll_invulnerability_end_tick >
            fighter->forward_roll_ticks ||
        fighter->roll_invulnerability_end_tick >
            fighter->backward_roll_ticks ||
        fighter->spot_dodge_ticks == UINT16_C(0) ||
        fighter->spot_dodge_ticks > UINT16_C(240) ||
        fighter->spot_dodge_invulnerability_begin_tick >=
            fighter->spot_dodge_invulnerability_end_tick ||
        fighter->spot_dodge_invulnerability_end_tick >
            fighter->spot_dodge_ticks ||
        fighter->shield_minimum_hold_ticks == UINT16_C(0) ||
        fighter->shield_minimum_hold_ticks > UINT16_C(120) ||
        fighter->shield_release_ticks == UINT16_C(0) ||
        fighter->shield_release_ticks > UINT16_C(240) ||
        fighter->powershield_window_ticks == UINT16_C(0) ||
        fighter->powershield_window_ticks >=
            fighter->shield_minimum_hold_ticks ||
        fighter->powershield_cancel_delay_ticks == UINT16_C(0) ||
        fighter->powershield_cancel_delay_ticks >=
            fighter->shield_release_ticks ||
        fighter->shield_break_launch_speed_f32 <=
            fighter->gravity_f32 ||
        fighter->shield_break_launch_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->shield_break_stun_ticks == UINT16_C(0) ||
        fighter->shield_break_stun_ticks > UINT16_C(600) ||
        fighter->shield_break_minimum_stun_ticks == UINT16_C(0) ||
        fighter->shield_break_minimum_stun_ticks >
            fighter->shield_break_stun_ticks ||
        fighter->shield_break_down_ticks == UINT16_C(0) ||
        fighter->shield_break_down_ticks > UINT16_C(240) ||
        fighter->shield_break_stand_ticks == UINT16_C(0) ||
        fighter->shield_break_stand_ticks > UINT16_C(240) ||
        fighter->shield_break_mash_reduction_ticks ==
            UINT16_C(0) ||
        fighter->shield_break_mash_reduction_ticks >
            UINT16_C(60) ||
        fighter->mash_stick_axis_threshold == UINT16_C(0) ||
        fighter->mash_stick_axis_threshold > UINT16_C(32767) ||
        fighter->shield_break_stun_tick_decrement == UINT16_C(0) ||
        fighter->shield_break_stun_tick_decrement > UINT16_C(60) ||
        fighter->grabbox_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->grabbox_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->grabbox_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->grabbox_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->grabbox_half_width_f32 <= INT32_C(0) ||
        fighter->grabbox_half_width_f32 >
            maximum_fighter_extent_f32 ||
        fighter->grabbox_half_height_f32 <= INT32_C(0) ||
        fighter->grabbox_half_height_f32 >
            maximum_fighter_extent_f32 ||
        fighter->grabbed_offset_x_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->grabbed_offset_x_f32 >
            maximum_fighter_extent_f32 ||
        fighter->grabbed_offset_y_f32 <
            -maximum_fighter_extent_f32 ||
        fighter->grabbed_offset_y_f32 >
            maximum_fighter_extent_f32 ||
        fighter->grab_escape_damage_ticks_f32 < INT32_C(0) ||
        fighter->grab_escape_damage_ticks_f32 > PF_F32_ONE ||
        fighter->grab_startup_ticks == UINT16_C(0) ||
        fighter->grab_startup_ticks > UINT16_C(120) ||
        fighter->grab_active_ticks == UINT16_C(0) ||
        fighter->grab_active_ticks > UINT16_C(120) ||
        fighter->grab_recovery_ticks == UINT16_C(0) ||
        fighter->grab_recovery_ticks > UINT16_C(240) ||
        (uint32_t)fighter->grab_startup_ticks +
                (uint32_t)fighter->grab_active_ticks +
                (uint32_t)fighter->grab_recovery_ticks >
            UINT32_C(600) ||
        fighter->dash_grab_startup_ticks == UINT16_C(0) ||
        fighter->dash_grab_startup_ticks > UINT16_C(120) ||
        fighter->dash_grab_active_ticks == UINT16_C(0) ||
        fighter->dash_grab_active_ticks > UINT16_C(120) ||
        fighter->dash_grab_recovery_ticks == UINT16_C(0) ||
        fighter->dash_grab_recovery_ticks > UINT16_C(240) ||
        (uint32_t)fighter->dash_grab_startup_ticks +
                (uint32_t)fighter->dash_grab_active_ticks +
                (uint32_t)fighter->dash_grab_recovery_ticks >
            UINT32_C(600) ||
        fighter->grab_escape_base_ticks == UINT16_C(0) ||
        fighter->grab_escape_max_ticks <
            fighter->grab_escape_base_ticks ||
        fighter->grab_escape_max_ticks > UINT16_C(600) ||
        fighter->grab_mash_reduction_ticks == UINT16_C(0) ||
        fighter->grab_mash_reduction_ticks > UINT16_C(60) ||
        fighter->grab_escape_tick_decrement == UINT16_C(0) ||
        fighter->grab_escape_tick_decrement > UINT16_C(60) ||
        fighter->grab_release_ticks == UINT16_C(0) ||
        fighter->grab_release_ticks > UINT16_C(120) ||
        fighter->grab_release_speed_x_f32 <= INT32_C(0) ||
        fighter->grab_release_speed_x_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->grab_release_air_speed_x_f32 <= INT32_C(0) ||
        fighter->grab_release_air_speed_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->grab_release_air_speed_y_f32 <= INT32_C(0) ||
        fighter->grab_release_air_speed_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        fighter->pummel_damage_f32 == UINT32_C(0) ||
        fighter->pummel_damage_f32 >
            50.0f ||
        fighter->pummel_hit_tick == UINT16_C(0) ||
        fighter->pummel_hit_tick >= fighter->pummel_total_ticks ||
        fighter->pummel_total_ticks > UINT16_C(120) ||
        fighter->air_jump_count > UINT8_C(8) ||
        fighter->powershield_cancel_enabled > UINT8_C(1) ||
        fighter->wall_jump_enabled > UINT8_C(1))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    stage = &content->stage;
    reference_stage = ssbm_reference_stage_collision(
        stage->reference_collision_profile);
    reference_spawn_line = NULL;
    if (stage->reference_collision_profile ==
        (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED)
    {
        if (stage->reference_spawn_line != UINT16_C(0) ||
            stage->reference_spawn_x_f32 != INT32_C(0))
        {
            return PF_STATUS_INVALID_CONFIG;
        }
    }
    else
    {
        if (reference_stage == NULL ||
            stage->reference_spawn_line >= reference_stage->line_count)
        {
            return PF_STATUS_INVALID_CONFIG;
        }
        reference_spawn_line =
            &reference_stage->lines[stage->reference_spawn_line];
        if (reference_spawn_line->kind !=
                (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
            ssbm_stage_support_valid(
                stage->reference_collision_profile,
                (uint8_t)(stage->reference_spawn_line + UINT16_C(1)),
                UINT8_C(1)) == 0)
        {
            return PF_STATUS_INVALID_CONFIG;
        }
        if (reference_stage->spawn_point_count > UINT8_C(0))
        {
            if (reference_stage->spawn_points == NULL ||
                reference_stage->spawn_point_count >
                    (uint8_t)PF_SIM_MAX_PLAYERS ||
                stage->blast_left_f32 != reference_stage->blast_left_f32 ||
                stage->blast_right_f32 != reference_stage->blast_right_f32 ||
                stage->blast_top_f32 != reference_stage->blast_top_f32 ||
                stage->blast_bottom_f32 != reference_stage->blast_bottom_f32)
            {
                return PF_STATUS_INVALID_CONFIG;
            }
            for (reference_spawn_index = UINT32_C(0);
                 reference_spawn_index <
                     (uint32_t)reference_stage->spawn_point_count;
                 ++reference_spawn_index)
            {
                const ssbm_stage_spawn_point *spawn =
                    &reference_stage->spawn_points[reference_spawn_index];
                const ssbm_stage_collision_line *spawn_line =
                    ssbm_reference_stage_line(
                        stage->reference_collision_profile,
                        spawn->support);
                const float spawn_left =
                    spawn_line != NULL &&
                            spawn_line->start_x_f32 < spawn_line->end_x_f32
                        ? spawn_line->start_x_f32
                        : spawn_line != NULL ? spawn_line->end_x_f32
                                             : 0.0f;
                const float spawn_right =
                    spawn_line != NULL &&
                            spawn_line->start_x_f32 > spawn_line->end_x_f32
                        ? spawn_line->start_x_f32
                        : spawn_line != NULL ? spawn_line->end_x_f32
                                             : 0.0f;

                if (spawn->source_index != (uint8_t)reference_spawn_index ||
                    spawn_line == NULL ||
                    spawn_line->kind !=
                        (uint8_t)PF_M4_SSBM_STAGE_SURFACE_FLOOR ||
                    spawn->position_x_f32 < spawn_left ||
                    spawn->position_x_f32 > spawn_right)
                {
                    return PF_STATUS_INVALID_CONFIG;
                }
            }
        }
    }
    platform_left_extent =
        stage->platform_center_x_f32 -
        stage->platform_motion_amplitude_f32 -
        stage->platform_half_width_f32;
    platform_right_extent =
        stage->platform_center_x_f32 +
        stage->platform_motion_amplitude_f32 +
        stage->platform_half_width_f32;
    spawn_left_extent =
        stage->reference_spawn_x_f32 -
        3.0f * stage->spawn_spacing_f32 -
        fighter->half_width_f32;
    spawn_right_extent =
        stage->reference_spawn_x_f32 +
        3.0f * stage->spawn_spacing_f32 +
        fighter->half_width_f32;
    revival_left_extent =
        -3.0f * stage->spawn_spacing_f32 -
        stage->revival_platform_half_width_f32;
    revival_right_extent =
        3.0f * stage->spawn_spacing_f32 +
        stage->revival_platform_half_width_f32;
    upper_platform_left_extent =
        stage->upper_platform_center_x_f32 -
        stage->upper_platform_half_width_f32;
    upper_platform_right_extent =
        stage->upper_platform_center_x_f32 +
        stage->upper_platform_half_width_f32;
    solid_overlaps_platform =
        stage->platform_y_f32 >= stage->solid_top_f32 &&
        stage->platform_y_f32 <= stage->solid_bottom_f32 &&
        platform_right_extent >= stage->solid_left_f32 &&
        platform_left_extent <= stage->solid_right_f32;
    upper_overlaps_platform =
        stage->upper_platform_y_f32 == stage->platform_y_f32 &&
        upper_platform_right_extent >= platform_left_extent &&
        upper_platform_left_extent <= platform_right_extent;
    upper_overlaps_revival =
        stage->upper_platform_y_f32 >=
            stage->revival_platform_start_y_f32 &&
        stage->upper_platform_y_f32 <=
            stage->revival_platform_end_y_f32 &&
        upper_platform_right_extent >= revival_left_extent &&
        upper_platform_left_extent <= revival_right_extent;
    upper_overlaps_solid =
        stage->upper_platform_y_f32 >= stage->solid_top_f32 &&
        stage->upper_platform_y_f32 <= stage->solid_bottom_f32 &&
        upper_platform_right_extent >= stage->solid_left_f32 &&
        upper_platform_left_extent <= stage->solid_right_f32;
    if (stage->floor_left_f32 >= stage->floor_right_f32 ||
        stage->blast_left_f32 >= stage->floor_left_f32 ||
        stage->blast_right_f32 <= stage->floor_right_f32 ||
        stage->blast_top_f32 >= stage->platform_y_f32 ||
        stage->platform_y_f32 >= stage->floor_y_f32 ||
        stage->solid_left_f32 >= stage->solid_right_f32 ||
        stage->solid_left_f32 < stage->floor_left_f32 ||
        stage->solid_right_f32 > stage->floor_right_f32 ||
        stage->blast_top_f32 >= stage->solid_top_f32 ||
        stage->solid_top_f32 >= stage->solid_bottom_f32 ||
        stage->solid_bottom_f32 >= stage->floor_y_f32 ||
        solid_overlaps_platform != 0 ||
        stage->floor_y_f32 >= stage->blast_bottom_f32 ||
        stage->platform_half_width_f32 <= INT32_C(0) ||
        stage->platform_motion_amplitude_f32 < INT32_C(0) ||
        platform_left_extent < stage->floor_left_f32 ||
        platform_right_extent > stage->floor_right_f32 ||
        stage->blast_top_f32 >= stage->upper_platform_y_f32 ||
        stage->upper_platform_y_f32 >= stage->floor_y_f32 ||
        stage->upper_platform_half_width_f32 <= INT32_C(0) ||
        upper_platform_left_extent < stage->floor_left_f32 ||
        upper_platform_right_extent > stage->floor_right_f32 ||
        upper_overlaps_platform != 0 ||
        upper_overlaps_revival != 0 ||
        upper_overlaps_solid != 0 ||
        (reference_spawn_line != NULL &&
         (spawn_left_extent <
              (reference_spawn_line->start_x_f32 <
                                reference_spawn_line->end_x_f32
                            ? reference_spawn_line->start_x_f32
                            : reference_spawn_line->end_x_f32) ||
          spawn_right_extent >
              (reference_spawn_line->start_x_f32 >
                                reference_spawn_line->end_x_f32
                            ? reference_spawn_line->start_x_f32
                            : reference_spawn_line->end_x_f32))) ||
        stage->spawn_spacing_f32 <= INT32_C(0) ||
        spawn_right_extent > stage->floor_right_f32 ||
        spawn_left_extent < stage->floor_left_f32 ||
        stage->revival_platform_start_y_f32 -
                fighter->half_height_f32 <
            stage->blast_top_f32 ||
        stage->revival_platform_end_y_f32 <=
            stage->revival_platform_start_y_f32 ||
        stage->revival_platform_end_y_f32 >= stage->solid_top_f32 ||
        stage->revival_platform_half_width_f32 <
            fighter->half_width_f32 ||
        revival_left_extent < stage->floor_left_f32 ||
        revival_right_extent > stage->floor_right_f32 ||
        stage->revival_platform_descent_ticks == UINT16_C(0) ||
        stage->revival_platform_hold_ticks == UINT16_C(0) ||
        (uint32_t)stage->revival_platform_descent_ticks +
                (uint32_t)stage->revival_platform_hold_ticks >
            UINT32_C(600) ||
        stage->blast_left_f32 < -maximum_coordinate_f32 ||
        stage->blast_right_f32 > maximum_coordinate_f32 ||
        (stage->reference_collision_profile ==
             (uint16_t)PF_M4_REFERENCE_STAGE_AUTHORED &&
         stage->blast_top_f32 < INT32_C(0)) ||
        stage->blast_bottom_f32 > maximum_coordinate_f32 ||
        stage->platform_motion_period_ticks < UINT16_C(4) ||
        (stage->platform_motion_period_ticks % UINT16_C(4)) !=
            UINT16_C(0))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    item = &content->item;
    maximum_item_knockback_x =
        maximum_knockback_f32(
            item->base_knockback_x_f32,
            item->knockback_growth_f32,
            0);
    maximum_item_knockback_y =
        maximum_knockback_f32(
            item->base_knockback_y_f32,
            item->knockback_growth_f32,
            1);
    if (item->enabled > UINT8_C(1) ||
        item->half_width_f32 <= INT32_C(0) ||
        item->half_height_f32 <= INT32_C(0) ||
        item->half_width_f32 > maximum_fighter_extent_f32 ||
        item->half_height_f32 > maximum_fighter_extent_f32 ||
        item->spawn_x_f32 < -maximum_coordinate_f32 ||
        item->spawn_x_f32 > maximum_coordinate_f32 ||
        item->spawn_y_f32 < INT32_C(0) ||
        item->spawn_y_f32 > maximum_coordinate_f32 ||
        item->pickup_half_width_f32 < item->half_width_f32 ||
        item->pickup_half_height_f32 < item->half_height_f32 ||
        item->pickup_half_width_f32 > maximum_fighter_extent_f32 ||
        item->pickup_half_height_f32 > maximum_fighter_extent_f32 ||
        item->held_offset_x_f32 < -maximum_fighter_extent_f32 ||
        item->held_offset_x_f32 > maximum_fighter_extent_f32 ||
        item->held_offset_y_f32 < -maximum_fighter_extent_f32 ||
        item->held_offset_y_f32 > maximum_fighter_extent_f32 ||
        item->gravity_f32 <= INT32_C(0) ||
        item->gravity_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->fall_speed_f32 < item->gravity_f32 ||
        item->fall_speed_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->drop_velocity_y_f32 < INT32_C(0) ||
        item->drop_velocity_y_f32 > item->fall_speed_f32 ||
        item->forward_throw.velocity_x_f32 <= INT32_C(0) ||
        item->forward_throw.velocity_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->forward_throw.velocity_y_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->forward_throw.velocity_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->back_throw.velocity_x_f32 >= INT32_C(0) ||
        item->back_throw.velocity_x_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->back_throw.velocity_y_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->back_throw.velocity_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->up_throw.velocity_y_f32 >= INT32_C(0) ||
        item->up_throw.velocity_x_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->up_throw.velocity_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->up_throw.velocity_y_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->down_throw.velocity_y_f32 <= INT32_C(0) ||
        item->down_throw.velocity_x_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->down_throw.velocity_x_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->down_throw.velocity_y_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->momentum_transfer_f32 < INT32_C(0) ||
        item->momentum_transfer_f32 > PF_F32_ONE ||
        item->hitbox_half_width_f32 < item->half_width_f32 ||
        item->hitbox_half_height_f32 < item->half_height_f32 ||
        item->hitbox_half_width_f32 > maximum_fighter_extent_f32 ||
        item->hitbox_half_height_f32 > maximum_fighter_extent_f32 ||
        item->damage_f32 == UINT32_C(0) ||
        item->damage_f32 > 50.0f ||
        item->base_knockback_x_f32 <= INT32_C(0) ||
        item->base_knockback_y_f32 <= INT32_C(0) ||
        item->knockback_growth_f32 < INT32_C(0) ||
        maximum_item_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_item_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->hit_bounce_velocity_y_f32 >= INT32_C(0) ||
        item->hit_bounce_velocity_y_f32 <
            -PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->dash_throw_speed_f32 < INT32_C(0) ||
        item->dash_throw_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        item->throw_recovery_ticks == UINT16_C(0) ||
        item->throw_recovery_ticks > UINT16_C(240) ||
        item->dash_throw_recovery_ticks <=
            item->throw_recovery_ticks ||
        item->dash_throw_recovery_ticks > UINT16_C(240) ||
        item->glide_toss_begin_tick >
            item->glide_toss_end_tick ||
        item->glide_toss_end_tick >= fighter->forward_roll_ticks ||
        item->glide_toss_end_tick >= fighter->backward_roll_ticks ||
        item->pickup_lockout_ticks == UINT16_C(0) ||
        item->pickup_lockout_ticks > UINT16_C(240) ||
        item->lifetime_ticks == UINT16_C(0) ||
        item->lifetime_ticks > UINT16_C(3600) ||
        item->respawn_ticks == UINT16_C(0) ||
        item->respawn_ticks > UINT16_C(3600) ||
        item->hitlag_ticks == UINT16_C(0) ||
        item->hitlag_ticks > UINT16_C(120) ||
        (item->enabled != UINT8_C(0) &&
         (item->spawn_x_f32 - item->half_width_f32 <
              stage->floor_left_f32 ||
          item->spawn_x_f32 + item->half_width_f32 >
              stage->floor_right_f32 ||
          item->spawn_y_f32 !=
              stage->floor_y_f32 - item->half_height_f32)))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    projectile = &content->projectile;
    maximum_projectile_knockback_x =
        maximum_knockback_f32(
            projectile->base_knockback_x_f32,
            projectile->knockback_growth_f32,
            0);
    maximum_projectile_knockback_y =
        maximum_knockback_f32(
            projectile->base_knockback_y_f32,
            projectile->knockback_growth_f32,
            1);
    if (projectile->enabled > UINT8_C(1) ||
        projectile->half_width_f32 <= INT32_C(0) ||
        projectile->half_height_f32 <= INT32_C(0) ||
        projectile->half_width_f32 > maximum_fighter_extent_f32 ||
        projectile->half_height_f32 > maximum_fighter_extent_f32 ||
        projectile->spawn_offset_x_f32 < INT32_C(0) ||
        projectile->spawn_offset_x_f32 > maximum_fighter_extent_f32 ||
        projectile->spawn_offset_y_f32 < -maximum_fighter_extent_f32 ||
        projectile->spawn_offset_y_f32 > maximum_fighter_extent_f32 ||
        projectile->speed_f32 <= INT32_C(0) ||
        projectile->speed_f32 > PF_SIM_MAX_MOTION_SPEED_F32 ||
        projectile->damage_f32 == UINT32_C(0) ||
        projectile->damage_f32 > 50.0f ||
        projectile->base_knockback_x_f32 <= INT32_C(0) ||
        projectile->base_knockback_y_f32 <= INT32_C(0) ||
        projectile->knockback_growth_f32 < INT32_C(0) ||
        maximum_projectile_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_projectile_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        projectile->lifetime_ticks == UINT16_C(0) ||
        projectile->lifetime_ticks > UINT16_C(3600) ||
        projectile->fire_recovery_ticks <= UINT16_C(1) ||
        projectile->fire_recovery_ticks > UINT16_C(240) ||
        projectile->hitlag_ticks == UINT16_C(0) ||
        projectile->hitlag_ticks > UINT16_C(120) ||
        projectile->powershield_reflect_window_ticks == UINT16_C(0) ||
        projectile->powershield_reflect_window_ticks >
            fighter->powershield_window_ticks)
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    reflector = &content->reflector;
    maximum_reflector_knockback_x =
        maximum_knockback_f32(
            reflector->base_knockback_x_f32,
            reflector->knockback_growth_f32,
            0);
    maximum_reflector_knockback_y =
        maximum_knockback_f32(
            reflector->base_knockback_y_f32,
            reflector->knockback_growth_f32,
            1);
    if (reflector->enabled > UINT8_C(1) ||
        reflector->hitbox_offset_x_f32 < -maximum_fighter_extent_f32 ||
        reflector->hitbox_offset_x_f32 > maximum_fighter_extent_f32 ||
        reflector->hitbox_offset_y_f32 < -maximum_fighter_extent_f32 ||
        reflector->hitbox_offset_y_f32 > maximum_fighter_extent_f32 ||
        reflector->hitbox_half_width_f32 <= INT32_C(0) ||
        reflector->hitbox_half_width_f32 > maximum_fighter_extent_f32 ||
        reflector->hitbox_half_height_f32 <= INT32_C(0) ||
        reflector->hitbox_half_height_f32 > maximum_fighter_extent_f32 ||
        reflector->damage_f32 == UINT32_C(0) ||
        reflector->damage_f32 > 50.0f ||
        reflector->base_knockback_x_f32 <= INT32_C(0) ||
        reflector->base_knockback_y_f32 < INT32_C(0) ||
        reflector->knockback_growth_f32 < INT32_C(0) ||
        maximum_reflector_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_reflector_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        reflector->startup_ticks > UINT16_C(60) ||
        reflector->active_ticks == UINT16_C(0) ||
        reflector->active_ticks > UINT16_C(60) ||
        reflector->recovery_ticks == UINT16_C(0) ||
        reflector->recovery_ticks > UINT16_C(240) ||
        (uint32_t)reflector->startup_ticks +
                (uint32_t)reflector->active_ticks +
                (uint32_t)reflector->recovery_ticks >
            UINT32_C(600) ||
        reflector->hitlag_ticks == UINT16_C(0) ||
        reflector->hitlag_ticks > UINT16_C(120))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    charge = &content->charge;
    maximum_charge_knockback_x =
        maximum_knockback_f32(
            charge->base_knockback_x_f32,
            charge->knockback_growth_f32,
            0);
    maximum_charge_knockback_y =
        maximum_knockback_f32(
            charge->base_knockback_y_f32,
            charge->knockback_growth_f32,
            1);
    if (charge->enabled > UINT8_C(1) ||
        charge->hitbox_offset_x_f32 < -maximum_fighter_extent_f32 ||
        charge->hitbox_offset_x_f32 > maximum_fighter_extent_f32 ||
        charge->hitbox_offset_y_f32 < -maximum_fighter_extent_f32 ||
        charge->hitbox_offset_y_f32 > maximum_fighter_extent_f32 ||
        charge->hitbox_half_width_f32 <= INT32_C(0) ||
        charge->hitbox_half_width_f32 > maximum_fighter_extent_f32 ||
        charge->hitbox_half_height_f32 <= INT32_C(0) ||
        charge->hitbox_half_height_f32 > maximum_fighter_extent_f32 ||
        charge->base_damage_f32 == UINT32_C(0) ||
        charge->base_damage_f32 >
            50.0f ||
        charge->bonus_damage_f32 >
            50.0f ||
        charge->base_damage_f32 + charge->bonus_damage_f32 > 50.0f ||
        charge->base_knockback_x_f32 <= INT32_C(0) ||
        charge->base_knockback_y_f32 < INT32_C(0) ||
        charge->knockback_growth_f32 < INT32_C(0) ||
        maximum_charge_knockback_x >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        maximum_charge_knockback_y >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        charge->max_charge_ticks == UINT16_C(0) ||
        charge->max_charge_ticks > UINT16_C(600) ||
        charge->store_animation_ticks == UINT16_C(0) ||
        charge->store_animation_ticks > UINT16_C(120) ||
        charge->release_startup_ticks > UINT16_C(120) ||
        charge->release_active_ticks == UINT16_C(0) ||
        charge->release_active_ticks > UINT16_C(120) ||
        charge->release_recovery_ticks == UINT16_C(0) ||
        charge->release_recovery_ticks > UINT16_C(600) ||
        (uint32_t)charge->release_startup_ticks +
                (uint32_t)charge->release_active_ticks +
                (uint32_t)charge->release_recovery_ticks >
            UINT32_C(600) ||
        charge->release_hitlag_ticks == UINT16_C(0) ||
        charge->release_hitlag_ticks > UINT16_C(120))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    recovery = &content->recovery;
    if (recovery->enabled > UINT8_C(1) ||
        recovery->horizontal_speed_f32 <= INT32_C(0) ||
        recovery->horizontal_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        recovery->vertical_speed_f32 <= fighter->gravity_f32 ||
        recovery->vertical_speed_f32 >
            PF_SIM_MAX_MOTION_SPEED_F32 ||
        recovery->ascent_ticks == UINT16_C(0) ||
        recovery->ascent_ticks > UINT16_C(120))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    return PF_STATUS_OK;
}

pf_status make_content_view(
    const struct content *content,
    pf_content_view *out_view)
{
    pf_status status;

    if (out_view == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_view, 0, sizeof(*out_view));

    status = validate_content(content);
    if (status != PF_STATUS_OK)
    {
        return status;
    }

    out_view->struct_size = (uint32_t)sizeof(*out_view);
    out_view->schema_version = PF_SIM_CONTENT_SCHEMA_VERSION;
    out_view->bytes = content;
    out_view->byte_count = sizeof(*content);
    content_hash(content, out_view->content_hash.bytes);
    return PF_STATUS_OK;
}

pf_status content_from_view(
    const pf_content_view *view,
    struct content *out_content)
{
    pf_content_view canonical_view;
    struct content candidate;
    pf_status status;

    if (view == NULL || out_content == NULL)
    {
        return PF_STATUS_INVALID_ARGUMENT;
    }
    if (view->byte_count == (size_t)0)
    {
        return default_content(out_content);
    }
    if (view->bytes == NULL ||
        view->byte_count != sizeof(struct content))
    {
        return PF_STATUS_INVALID_CONFIG;
    }

    (void)memcpy(&candidate, view->bytes, sizeof(candidate));
    status = make_content_view(&candidate, &canonical_view);
    if (status != PF_STATUS_OK)
    {
        return status;
    }
    if (!hash_equal(
            canonical_view.content_hash.bytes,
            view->content_hash.bytes))
    {
        return PF_STATUS_CHECKSUM_MISMATCH;
    }

    *out_content = candidate;
    return PF_STATUS_OK;
}
