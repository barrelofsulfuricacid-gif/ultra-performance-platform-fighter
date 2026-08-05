#!/usr/bin/env python3
"""Replay a Dolphin movement capture through the native M4 simulator."""

from __future__ import annotations

import argparse
import csv
import io
import json
from pathlib import Path
import subprocess
import sys


SSBM_TO_M4_ACTION = {
    "STANDING": 0,
    "WALK_SLOW": 1,
    "WALK_MIDDLE": 1,
    "WALK_FAST": 1,
    "DASHING": 2,
    "RUNNING": 3,
    "RUN_BRAKE": 11,
    "TURNING_RUN": 10,
    "FALLING": 6,
    "TURNING": 103,
    "SHIELD_START": 18,
    "SHIELD_REFLECT": 18,
    "SHIELD": 18,
    "SHIELD_RELEASE": 20,
    "SHIELD_STUN": 19,
    "NEUTRAL_ATTACK_1": 12,
    "NEUTRAL_ATTACK_2": 58,
    "DASH_ATTACK": 57,
    "FTILT_HIGH": 87,
    "FTILT_HIGH_MID": 87,
    "FTILT_MID": 87,
    "FTILT_LOW_MID": 87,
    "FTILT_LOW": 87,
    "UPTILT": 79,
    "DOWNTILT": 80,
    "FSMASH_HIGH": 88,
    "FSMASH_MID_HIGH": 88,
    "FSMASH_MID": 88,
    "FSMASH_MID_LOW": 88,
    "FSMASH_LOW": 88,
    "UPSMASH": 89,
    "DOWNSMASH": 90,
    "ROLL_FORWARD": 38,
    "ROLL_BACKWARD": 39,
    "SPOTDODGE": 40,
    "CROUCH_START": 104,
    "CROUCHING": 4,
    "CROUCH_END": 105,
    "KNEE_BEND": 5,
    "JUMPING_FORWARD": 6,
    "JUMPING_BACKWARD": 6,
    "JUMPING_ARIAL_FORWARD": 6,
    "JUMPING_ARIAL_BACKWARD": 6,
    "FALLING_FORWARD": 6,
    "FALLING_BACKWARD": 6,
    "FALLING_AERIAL": 6,
    "FALLING_AERIAL_FORWARD": 6,
    "FALLING_AERIAL_BACKWARD": 6,
    "AIRDODGE": 32,
    "LANDING_SPECIAL": 34,
    "LANDING": 7,
    "TAUNT_RIGHT": 75,
    "TAUNT_LEFT": 75,
    "PLATFORM_DROP": 6,
    "NEUTRAL_B_FULL_CHARGE_AIR": 108,
    "NEUTRAL_B_ATTACKING_AIR": 107,
}

M4_DELAYED_AIR_JUMP = 61

POSITION_ANCHOR_LABELS = {
    "recenter_after_defense",
    "run_for_jump_squat_reverse",
    "recenter_before_double_jump",
    "neutral_jump_for_double_jump",
    "short_hop_press",
    "full_hop_press",
    "full_hop_for_fast_fall",
    "settle_before_crouch_start_jump",
    "settle_before_crouch_wait_jump",
    "settle_before_crouch_end_jump",
    "settle_before_crouch_wait_dash",
    "settle_before_crouch_end_walk",
    "settle_before_crouch_start_guard",
    "settle_before_crouch_wait_guard",
    "settle_before_crouch_end_guard",
    "settle_before_crouch_start_taunt",
    "settle_before_crouch_wait_taunt",
    "settle_before_crouch_end_taunt",
    "settle_before_crouch_start_attack",
    "settle_before_crouch_wait_attack",
    "settle_before_crouch_end_attack",
    "settle_before_crouch_wait_neutral_special",
    "settle_before_crouch_end_neutral_special",
    "settle_before_crouch_start_neutral_special",
    "settle_before_crouch_start_down_special",
    "settle_before_down_special_slice",
    "settle_before_crouch_wait_down_special",
    "settle_before_crouch_end_down_special",
    "settle_before_crouch_start_grab",
    "settle_before_crouch_wait_grab",
    "settle_before_crouch_end_grab",
    "settle_before_standing_turn_taunt",
    "settle_before_landing_taunt",
    "settle_before_landing_jump",
    "settle_before_landing_dash",
    "settle_before_landing_guard",
    "settle_before_landing_walk",
    "settle_before_landing_crouch",
    "settle_before_landing_late_crouch",
    "settle_before_landing_turn",
    "settle_before_ground_tap_full_hop",
    "settle_before_ground_tap_short_hop",
    "settle_before_air_tap_jump",
    "settle_before_landing_tap_jump",
    "settle_before_shield_tap_jump",
    "settle_before_tap_jump_below_threshold",
    "settle_before_tap_jump_above_threshold",
    "settle_before_tap_jump_slow_sweep",
    "settle_before_tap_jump_two_sample",
    "recenter_before_run_brake_jump",
    "recenter_before_run_brake_crouch",
    "recenter_before_run_brake_guard",
    "recenter_before_run_brake_spot_dodge",
    "recenter_before_run_brake_cstick_roll",
    "recenter_before_run_brake_cstick_spot",
    "recenter_before_run_brake_taunt",
    "recenter_before_run_brake_attack",
    "recenter_before_run_brake_grab",
    "recenter_before_run_brake_special",
    "recenter_before_run_brake_reverse",
}

# These samples qualify common-state IASA eligibility, not the execution of
# character-specific moves. Compare the transition sample against the
# project's semantic counterpart, then resume exact shared-state comparison at
# the next stationary anchor after both character-specific actions recover.
CONTENT_ROUTE_ENTRY_ACTIONS = {
    "crouch_start_attack": 12,
    "crouch_wait_attack": 12,
    "crouch_end_attack": 12,
    "crouch_start_neutral_special": 64,
    "crouch_start_down_special": 66,
    "crouch_wait_down_special": 66,
    "crouch_end_down_special": 66,
    "crouch_start_grab": 49,
    "crouch_wait_grab": 12,
    "crouch_end_grab": 12,
    "ground_iasa_utilt_special_interrupt": 64,
    "ground_iasa_utilt_grab_interrupt": 49,
    "ground_iasa_fsmash_m_special_interrupt": 64,
    "ground_iasa_fsmash_m_grab_interrupt": 49,
}

# M4's Falcon movement values use a 12/115 world-unit scale relative to
# GALE01's Falcon attributes (for example, 2.0 becomes 24/115).
SSBM_TO_M4_Q16 = 65536.0 * 12.0 / 115.0
SSBM_TO_M4_Y_Q16 = 65536.0 * 11.0 / 62.0
GALE01_NTSC102_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)


def controller_axis(value: float) -> int:
    return round((value - 0.5) * 65534.0)


def controller_axis_y(value: float) -> int:
    return round((0.5 - value) * 65534.0)


def scaled_q16(value: float) -> int:
    return round(value * SSBM_TO_M4_Q16)


def scaled_y_q16(value: float) -> int:
    return -round(value * SSBM_TO_M4_Y_Q16)


def controller_trigger(value: float) -> int:
    return round(max(0.0, min(1.0, value)) * 65535.0)


def normalized_shield_strength(
    row: dict[str, object], retained_strength: int = 0
) -> int:
    if str(row.get("action")) not in {
        "SHIELD_START",
        "SHIELD_REFLECT",
        "SHIELD",
        "SHIELD_STUN",
    }:
        return 0
    if bool(row.get("requested_digital_left")) or bool(
        row.get("requested_digital_right")
    ):
        return 65535
    analog = float(row.get("observed_analog_shoulder", 0.0))
    if analog <= 0.30:
        # GuardOn/GuardReflect keep the trigger magnitude which created the
        # shield until their locked animation ends. libmelee exposes current
        # controller pressure, not that retained fighter field.
        if str(row.get("action")) in {"SHIELD_START", "SHIELD_REFLECT"}:
            return retained_strength
        return 0
    return round(((analog - 0.30) / 0.70) * 65535.0)


def expected_action_ticks(action: str, action_frame: float) -> int | None:
    frame = round(action_frame)
    if action in {
        "DASHING",
        "RUN_BRAKE",
        "ROLL_FORWARD",
        "ROLL_BACKWARD",
        "SPOTDODGE",
        "CROUCH_START",
        "CROUCHING",
        "CROUCH_END",
        "TAUNT_RIGHT",
        "TAUNT_LEFT",
        "NEUTRAL_B_FULL_CHARGE_AIR",
        "NEUTRAL_B_ATTACKING_AIR",
    }:
        return frame
    if action == "TURNING_RUN":
        return frame + 1
    if action in {"KNEE_BEND", "AIRDODGE", "LANDING"}:
        return frame - 1
    if action == "LANDING_SPECIAL":
        return (frame - 1) // 3
    return None


def expected_action_state(action: str, action_frame: float) -> int | None:
    if action in {"JUMPING_ARIAL_FORWARD", "JUMPING_ARIAL_BACKWARD"}:
        # M4 exposes the six-frame deterministic double-jump-cancel window as
        # a distinct internal action before returning to its common airborne
        # state. Both states correspond to Melee's JumpAerial action.
        if round(action_frame) <= 6:
            return M4_DELAYED_AIR_JUMP
    return SSBM_TO_M4_ACTION.get(action)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("runner", type=Path)
    parser.add_argument(
        "--position-tolerance-q16", type=int, default=640,
        help="allowed float-to-fixed position quantization difference",
    )
    parser.add_argument(
        "--velocity-tolerance-q16", type=int, default=32,
        help="allowed float-to-fixed velocity quantization difference",
    )
    parser.add_argument(
        "--native-output", type=Path,
        help="optionally preserve the native CSV trace for diagnostics",
    )
    parser.add_argument(
        "--native-input-output", type=Path,
        help="optionally preserve the normalized native input trace",
    )
    args = parser.parse_args()

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    disc = capture.get("disc")
    if (
        not isinstance(disc, dict)
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != GALE01_NTSC102_SHA256
    ):
        print(
            "ssbm-movement-compare=fail reason=oracle-disc-identity",
            file=sys.stderr,
        )
        return 1
    oracle_rows = capture["rows"]
    falcon_punch_air_mode = any(
        str(row.get("label", "")) == "special_geometry_neutral_air_start"
        for row in oracle_rows
    )
    if falcon_punch_air_mode:
        first_special_row = next(
            index
            for index, row in enumerate(oracle_rows)
            if str(row.get("label", ""))
            == "special_geometry_neutral_air_start"
        )
        oracle_rows = oracle_rows[first_special_row:]
    push_mode = bool(oracle_rows) and str(
        oracle_rows[0].get("label", "")
    ).startswith("push_")
    shield_hit_mode = bool(oracle_rows) and str(
        oracle_rows[0].get("label", "")
    ).startswith("shield_hit_")
    two_player_mode = push_mode or shield_hit_mode
    # Q16.16 accumulation can move a strict float overlap across the boundary
    # by one tick. Accept at most one 0.3-unit Melee push nudge per fighter in
    # this route, in addition to the ordinary position quantization envelope;
    # action, facing, grounded state, and self-induced velocity remain strict.
    position_tolerance_q16 = args.position_tolerance_q16
    if push_mode:
        position_tolerance_q16 += abs(scaled_q16(0.3))
    input_lines: list[str] = []
    for row in oracle_rows:
        observed_analog = float(row.get("observed_analog_shoulder", 0.0))
        # The project action packet has no device-specific Z bit. Its input
        # normalizer represents GameCube Z as the canonical full-shield-plus-A
        # grab chord, so replay the same physical sample through that mapping.
        left_trigger = (
            65535
            if bool(row.get("observed_grab", False))
            or bool(row.get("requested_digital_left"))
            else controller_trigger(observed_analog)
            if float(row.get("requested_left_shoulder", 0.0)) > 0.0
            else 0
        )
        right_trigger = (
            65535
            if bool(row.get("requested_digital_right"))
            else controller_trigger(observed_analog)
            if float(row.get("requested_right_shoulder", 0.0)) > 0.0
            else 0
        )
        buttons = 1 if bool(row.get("observed_jump", False)) else 0
        if bool(row.get("observed_attack", False)) or bool(
            row.get("observed_grab", False)
        ):
            buttons |= 2
        if bool(row.get("observed_special", False)):
            buttons |= 8
        if (
            abs(float(row.get("observed_c_x", 0.5)) - 0.5) > 0.02
            or abs(float(row.get("observed_c_y", 0.5)) - 0.5) > 0.02
        ):
            # Device normalization emits the canonical strong-attack bit with
            # every non-neutral C-stick sample.
            buttons |= 4
        if bool(row.get("observed_taunt", False)):
            buttons |= 16
        opponent_input_x = controller_axis(
            float(row.get("observed_opponent_main_x", 0.5))
        )
        opponent_buttons = (
            2 if bool(row.get("observed_opponent_attack", False)) else 0
        )
        input_lines.append(
            f"{controller_axis(float(row['observed_main_x']))},"
            f"{controller_axis_y(float(row.get('observed_main_y', 0.5)))},"
            f"{controller_axis(float(row.get('observed_c_x', 0.5)))},"
            f"{controller_axis_y(float(row.get('observed_c_y', 0.5)))},"
            f"{left_trigger},{right_trigger},{buttons},{opponent_input_x},"
            f"{opponent_buttons}\n"
        )
    input_text = "".join(input_lines)
    if args.native_input_output is not None:
        args.native_input_output.write_text(input_text, encoding="utf-8")
    runner_command = [str(args.runner)]
    if capture.get("stage") == "BATTLEFIELD":
        runner_command.append("--platform")
    elif push_mode:
        runner_command.append("--push")
    elif shield_hit_mode:
        runner_command.append("--shield-hit")
    elif falcon_punch_air_mode:
        runner_command.append("--falcon-punch-air")
    completed = subprocess.run(
        runner_command,
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        return completed.returncode
    if args.native_output is not None:
        args.native_output.write_text(completed.stdout, encoding="utf-8")
    native_rows = list(csv.DictReader(io.StringIO(completed.stdout)))
    if len(native_rows) != len(oracle_rows):
        print(
            "ssbm-movement-compare=fail reason=row-count "
            f"oracle={len(oracle_rows)} native={len(native_rows)}",
            file=sys.stderr,
        )
        return 1

    previous_oracle: dict[str, object] | None = None
    previous_native: dict[str, str] | None = None
    oracle_anchor_x = 0.0
    oracle_anchor_y = (
        float(oracle_rows[0]["position_y"])
        if capture.get("stage") == "BATTLEFIELD"
        else 0.0
    )
    native_anchor_x = 0
    native_anchor_y = 0
    horizontal_mirror = -1 if falcon_punch_air_mode else 1
    previous_label: str | None = None
    skip_character_content = False
    shield_contact_seen = False
    shield_numeric_ticks = 0
    expected_shield_strength = 0
    for oracle_index, (oracle, native) in enumerate(
        zip(oracle_rows, native_rows, strict=True)
    ):
        frame = int(oracle["trace_frame"])
        label = str(oracle["label"])
        entering_anchor = (
            label != previous_label
            and (
                label in POSITION_ANCHOR_LABELS
                or label.endswith("_settle")
            )
            and previous_oracle is not None
        )
        if skip_character_content and not entering_anchor:
            previous_oracle = oracle
            previous_native = native
            previous_label = label
            continue
        if entering_anchor:
            skip_character_content = False
        if (
            entering_anchor
        ):
            oracle_anchor_x = float(previous_oracle["position_x_from_origin"])
            oracle_anchor_y = float(previous_oracle["position_y"])
            native_anchor_x = int(previous_native["position_x_q16_from_origin"])
            native_anchor_y = int(previous_native["position_y_q16_from_origin"])
        action_name = str(oracle["action"])
        action_frame = round(float(oracle["action_frame"]))
        if (
            falcon_punch_air_mode
            and action_name == "NEUTRAL_B_FULL_CHARGE_AIR"
            and action_frame == 49
        ):
            oracle_anchor_x = float(oracle["position_x_from_origin"])
            oracle_anchor_y = float(oracle["position_y"])
            native_anchor_x = int(native["position_x_q16_from_origin"])
            native_anchor_y = int(native["position_y_q16_from_origin"])
        expected_action = CONTENT_ROUTE_ENTRY_ACTIONS.get(label)
        if expected_action is None:
            expected_action = expected_action_state(
                action_name, float(oracle["action_frame"])
            )
        if shield_hit_mode and float(oracle.get("hitlag_left", 0.0)) > 0.0:
            expected_action = 13
        actual_action = int(native["action_state"])
        expected_ticks = (
            None
            if label in CONTENT_ROUTE_ENTRY_ACTIONS
            else expected_action_ticks(
                action_name, float(oracle["action_frame"])
            )
        )
        if shield_hit_mode:
            expected_ticks = None
        actual_ticks = int(native["action_ticks"])
        expected_facing = int(oracle["facing"]) * horizontal_mirror
        actual_facing = int(native["facing"])
        expected_position = horizontal_mirror * scaled_q16(
            float(oracle["position_x_from_origin"]) - oracle_anchor_x
        )
        actual_position = (
            int(native["position_x_q16_from_origin"]) - native_anchor_x
        )
        expected_position_y = scaled_y_q16(
            float(oracle["position_y"]) - oracle_anchor_y
        )
        actual_position_y = (
            int(native["position_y_q16_from_origin"]) - native_anchor_y
        )
        expected_velocity = horizontal_mirror * scaled_q16(
            float(
                oracle[
                    "air_velocity_x"
                    if action_name in {
                        "JUMPING_FORWARD",
                        "JUMPING_BACKWARD",
                        "JUMPING_ARIAL_FORWARD",
                        "JUMPING_ARIAL_BACKWARD",
                        "FALLING",
                        "FALLING_FORWARD",
                        "FALLING_BACKWARD",
                        "FALLING_AERIAL",
                        "FALLING_AERIAL_FORWARD",
                        "FALLING_AERIAL_BACKWARD",
                        "AIRDODGE",
                        "NEUTRAL_B_FULL_CHARGE_AIR",
                    }
                    else "ground_velocity_x"
                ]
            )
        )
        actual_velocity = int(native["velocity_x_q16"])
        expected_velocity_y = scaled_y_q16(float(oracle["velocity_y"]))
        actual_velocity_y = int(native["velocity_y_q16"])
        skip_special_physics = (
            falcon_punch_air_mode
            and action_name == "NEUTRAL_B_FULL_CHARGE_AIR"
            and action_frame < 50
        )
        expected_grounded = 1 if bool(oracle["grounded"]) else 0
        actual_grounded = int(native["grounded"])
        expected_shield_health = round(float(oracle["shield_health"]) * 65536.0)
        actual_shield_health = int(native["shield_health_q16"])
        expected_shield_strength = normalized_shield_strength(
            oracle, expected_shield_strength
        )
        actual_shield_strength = int(native["shield_strength"])
        expected_hitlag = round(float(oracle.get("hitlag_left", 0.0)))
        actual_hitlag = int(native["hitlag_ticks"])
        differences: list[str] = []
        if expected_action is None:
            differences.append(f"unsupported_action={action_name}")
        elif actual_action != expected_action:
            differences.append(
                f"action expected={action_name}/{expected_action} actual={actual_action}"
            )
        elif expected_ticks is not None and actual_ticks != expected_ticks:
            differences.append(
                f"action_ticks expected={expected_ticks} actual={actual_ticks}"
            )
        if actual_facing != expected_facing:
            differences.append(
                f"facing expected={expected_facing} actual={actual_facing}"
            )
        if actual_grounded != expected_grounded:
            differences.append(
                f"grounded expected={expected_grounded} actual={actual_grounded}"
            )
        if (
            not skip_special_physics
            and abs(actual_position - expected_position) > position_tolerance_q16
        ):
            differences.append(
                f"position_q16 expected={expected_position} actual={actual_position} "
                f"delta={actual_position - expected_position}"
            )
        if (
            not skip_special_physics
            and abs(actual_position_y - expected_position_y)
            > args.position_tolerance_q16
        ):
            differences.append(
                "position_y_q16 "
                f"expected={expected_position_y} actual={actual_position_y} "
                f"delta={actual_position_y - expected_position_y}"
            )
        if (
            not skip_special_physics
            and abs(actual_velocity - expected_velocity)
            > args.velocity_tolerance_q16
        ):
            differences.append(
                f"velocity_q16 expected={expected_velocity} actual={actual_velocity} "
                f"delta={actual_velocity - expected_velocity}"
            )
        if (
            not skip_special_physics
            and abs(actual_velocity_y - expected_velocity_y) > 32
        ):
            differences.append(
                "velocity_y_q16 "
                f"expected={expected_velocity_y} actual={actual_velocity_y} "
                f"delta={actual_velocity_y - expected_velocity_y}"
            )
        if (
            previous_oracle is not None
            and float(oracle["shield_health"])
            != float(previous_oracle["shield_health"])
        ):
            shield_numeric_ticks += 1
        shield_health_tolerance_q16 = max(64, shield_numeric_ticks * 2)
        if (
            abs(actual_shield_health - expected_shield_health)
            > shield_health_tolerance_q16
        ):
            differences.append(
                "shield_health_q16 "
                f"expected={expected_shield_health} actual={actual_shield_health} "
                f"delta={actual_shield_health - expected_shield_health}"
            )
        if abs(actual_shield_strength - expected_shield_strength) > 1:
            differences.append(
                "shield_strength "
                f"expected={expected_shield_strength} "
                f"actual={actual_shield_strength} "
                f"delta={actual_shield_strength - expected_shield_strength}"
            )
        shield_memory = oracle.get("shield_memory")
        shield_state_memory = (
            oracle_rows[oracle_index + 1].get("shield_memory")
            if oracle_index + 1 < len(oracle_rows)
            and str(oracle_rows[oracle_index + 1]["action"]) == "SHIELD"
            else None
        )
        if (
            isinstance(shield_memory, dict)
            and isinstance(shield_state_memory, dict)
            and action_name == "SHIELD"
            and actual_shield_strength > 0
        ):
            expected_guard_magnitude = round(
                float(shield_state_memory["guard_magnitude"]) * 65535.0
            )
            actual_guard_magnitude = int(native["shield_magnitude"])
            expected_guard_angle = round(
                (
                    (float(shield_state_memory["guard_angle_degrees"]) - 10.0)
                    % 360.0
                )
                * 65536.0
                / 360.0
            ) % 65536
            actual_guard_angle = int(native["shield_angle_turn"])
            guard_angle_delta = (
                (actual_guard_angle - expected_guard_angle + 32768) % 65536
            ) - 32768
            fighter_position = shield_state_memory["fighter_position"]
            shield_matrix = shield_state_memory["shield_joint_matrix"]
            expected_center_offset_x = scaled_q16(
                float(shield_matrix[3]) - float(fighter_position[0])
            )
            expected_center_offset_y = (
                scaled_y_q16(
                    float(shield_matrix[7]) - float(fighter_position[1])
                )
                + round(0.8 * 65536.0)
            )
            size_matrix = shield_memory["shield_joint_matrix"]
            shield_world_radius = (
                float(size_matrix[0]) ** 2
                + float(size_matrix[4]) ** 2
                + float(size_matrix[8]) ** 2
            ) ** 0.5
            expected_radius_x = scaled_q16(shield_world_radius)
            expected_radius_y = round(
                shield_world_radius * SSBM_TO_M4_Y_Q16
            )
            geometry_pairs = (
                (
                    "shield_center_offset_x_q16",
                    expected_center_offset_x,
                    96,
                ),
                (
                    "shield_center_offset_y_q16",
                    expected_center_offset_y,
                    96,
                ),
                ("shield_radius_x_q16", expected_radius_x, 96),
                ("shield_radius_y_q16", expected_radius_y, 96),
            )
            if abs(actual_guard_magnitude - expected_guard_magnitude) > 64:
                differences.append(
                    "shield_magnitude "
                    f"expected={expected_guard_magnitude} "
                    f"actual={actual_guard_magnitude} "
                    f"delta={actual_guard_magnitude - expected_guard_magnitude}"
                )
            if abs(guard_angle_delta) > 64:
                differences.append(
                    "shield_angle_turn "
                    f"expected={expected_guard_angle} actual={actual_guard_angle} "
                    f"delta={guard_angle_delta}"
                )
            for field, expected_geometry, tolerance in geometry_pairs:
                actual_geometry = int(native[field])
                if abs(actual_geometry - expected_geometry) > tolerance:
                    differences.append(
                        f"{field} expected={expected_geometry} "
                        f"actual={actual_geometry} "
                        f"delta={actual_geometry - expected_geometry}"
                    )
        if shield_hit_mode and actual_hitlag != expected_hitlag:
            differences.append(
                f"hitlag expected={expected_hitlag} actual={actual_hitlag}"
            )
        if two_player_mode:
            opponent_action_name = str(oracle["opponent_action"])
            expected_opponent_action = expected_action_state(
                opponent_action_name,
                float(oracle["opponent_action_frame"]),
            )
            if (
                shield_hit_mode
                and float(oracle.get("opponent_hitlag_left", 0.0)) > 0.0
            ):
                expected_opponent_action = 13
            actual_opponent_action = int(native["opponent_action_state"])
            expected_opponent_ticks = expected_action_ticks(
                opponent_action_name,
                float(oracle["opponent_action_frame"]),
            )
            if shield_hit_mode:
                expected_opponent_ticks = None
            actual_opponent_ticks = int(native["opponent_action_ticks"])
            expected_opponent_hitlag = round(
                float(oracle.get("opponent_hitlag_left", 0.0))
            )
            actual_opponent_hitlag = int(native["opponent_hitlag_ticks"])
            expected_opponent_facing = int(oracle["opponent_facing"])
            actual_opponent_facing = int(native["opponent_facing"])
            expected_opponent_position = scaled_q16(
                float(oracle["opponent_position_x_from_origin"])
            )
            actual_opponent_position = int(
                native["opponent_position_x_q16_from_origin"]
            )
            expected_opponent_grounded = (
                1 if bool(oracle["opponent_grounded"]) else 0
            )
            actual_opponent_grounded = int(native["opponent_grounded"])
            expected_opponent_velocity = scaled_q16(
                float(oracle["opponent_ground_velocity_x"])
            )
            actual_opponent_velocity = int(native["opponent_velocity_x_q16"])
            opponent_hitlag = float(oracle.get("opponent_hitlag_left", 0.0))
            if shield_hit_mode and opponent_hitlag > 0.0:
                shield_contact_seen = True
            if expected_opponent_action is None:
                differences.append(
                    f"unsupported_opponent_action={opponent_action_name}"
                )
            elif actual_opponent_action != expected_opponent_action:
                differences.append(
                    "opponent_action "
                    f"expected={opponent_action_name}/{expected_opponent_action} "
                    f"actual={actual_opponent_action}"
                )
            elif (
                expected_opponent_ticks is not None
                and actual_opponent_ticks != expected_opponent_ticks
            ):
                differences.append(
                    "opponent_action_ticks "
                    f"expected={expected_opponent_ticks} "
                    f"actual={actual_opponent_ticks}"
                )
            if actual_opponent_facing != expected_opponent_facing:
                differences.append(
                    "opponent_facing "
                    f"expected={expected_opponent_facing} "
                    f"actual={actual_opponent_facing}"
                )
            if (
                shield_hit_mode
                and actual_opponent_hitlag != expected_opponent_hitlag
            ):
                differences.append(
                    "opponent_hitlag "
                    f"expected={expected_opponent_hitlag} "
                    f"actual={actual_opponent_hitlag}"
                )
            if (
                abs(
                    actual_opponent_position -
                    expected_opponent_position
                )
                > position_tolerance_q16
            ):
                differences.append(
                    "opponent_position_q16 "
                    f"expected={expected_opponent_position} "
                    f"actual={actual_opponent_position} "
                    "delta="
                    f"{actual_opponent_position - expected_opponent_position}"
                )
            if actual_opponent_grounded != expected_opponent_grounded:
                differences.append(
                    "opponent_grounded "
                    f"expected={expected_opponent_grounded} "
                    f"actual={actual_opponent_grounded}"
                )
            if (
                abs(actual_opponent_velocity - expected_opponent_velocity)
                > args.velocity_tolerance_q16
            ):
                differences.append(
                    "opponent_velocity_q16 "
                    f"expected={expected_opponent_velocity} "
                    f"actual={actual_opponent_velocity} "
                    "delta="
                    f"{actual_opponent_velocity - expected_opponent_velocity}"
                )
            if (
                shield_hit_mode
                and shield_contact_seen
                and opponent_hitlag == 0.0
                and previous_oracle is not None
            ):
                expected_opponent_recoil = scaled_q16(
                    float(oracle["opponent_position_x_from_origin"])
                    - float(previous_oracle["opponent_position_x_from_origin"])
                    - float(oracle["opponent_ground_velocity_x"])
                )
                actual_opponent_recoil = int(
                    native["opponent_shield_recoil_x_q16"]
                )
                if (
                    abs(actual_opponent_recoil - expected_opponent_recoil)
                    > args.velocity_tolerance_q16
                ):
                    differences.append(
                        "opponent_shield_recoil_q16 "
                        f"expected={expected_opponent_recoil} "
                        f"actual={actual_opponent_recoil} "
                        "delta="
                        f"{actual_opponent_recoil - expected_opponent_recoil}"
                    )
        if differences:
            print(
                "ssbm-movement-compare=fail "
                f"frame={frame} label={oracle['label']} " + " | ".join(differences)
            )
            return 1

        previous_oracle = oracle
        previous_native = native
        previous_label = label
        if label in CONTENT_ROUTE_ENTRY_ACTIONS:
            skip_character_content = True

    print(
        f"ssbm-movement-compare=pass frames={len(oracle_rows)} "
        f"position_tolerance_q16={position_tolerance_q16}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
