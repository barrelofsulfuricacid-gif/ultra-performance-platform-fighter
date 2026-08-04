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
}

# M4's Falcon movement values use a 12/115 world-unit scale relative to
# GALE01's Falcon attributes (for example, 2.0 becomes 24/115).
SSBM_TO_M4_Q16 = 65536.0 * 12.0 / 115.0
SSBM_TO_M4_Y_Q16 = 65536.0 * 11.0 / 62.0


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


def normalized_shield_strength(row: dict[str, object]) -> int:
    if str(row.get("action")) not in {
        "SHIELD_START",
        "SHIELD_REFLECT",
        "SHIELD",
    }:
        return 0
    if bool(row.get("requested_digital_left")) or bool(
        row.get("requested_digital_right")
    ):
        return 65535
    analog = float(row.get("observed_analog_shoulder", 0.0))
    if analog <= 0.30:
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
    args = parser.parse_args()

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    oracle_rows = capture["rows"]
    input_lines: list[str] = []
    for row in oracle_rows:
        observed_analog = float(row.get("observed_analog_shoulder", 0.0))
        left_trigger = (
            65535
            if bool(row.get("requested_digital_left"))
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
        input_lines.append(
            f"{controller_axis(float(row['observed_main_x']))},"
            f"{controller_axis_y(float(row.get('observed_main_y', 0.5)))},"
            f"{controller_axis(float(row.get('observed_c_x', 0.5)))},"
            f"{controller_axis_y(float(row.get('observed_c_y', 0.5)))},"
            f"{left_trigger},{right_trigger},{buttons}\n"
        )
    input_text = "".join(input_lines)
    completed = subprocess.run(
        [str(args.runner)],
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        return completed.returncode
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
    oracle_anchor_y = 0.0
    native_anchor_x = 0
    native_anchor_y = 0
    previous_label: str | None = None
    for oracle, native in zip(oracle_rows, native_rows, strict=True):
        frame = int(oracle["trace_frame"])
        label = str(oracle["label"])
        if (
            label != previous_label
            and label in POSITION_ANCHOR_LABELS
            and previous_oracle is not None
        ):
            oracle_anchor_x = float(previous_oracle["position_x_from_origin"])
            oracle_anchor_y = float(previous_oracle["position_y"])
            native_anchor_x = int(previous_native["position_x_q16_from_origin"])
            native_anchor_y = int(previous_native["position_y_q16_from_origin"])
        action_name = str(oracle["action"])
        expected_action = expected_action_state(
            action_name, float(oracle["action_frame"])
        )
        actual_action = int(native["action_state"])
        expected_ticks = expected_action_ticks(
            action_name, float(oracle["action_frame"])
        )
        actual_ticks = int(native["action_ticks"])
        expected_facing = int(oracle["facing"])
        actual_facing = int(native["facing"])
        expected_position = scaled_q16(
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
        expected_velocity = scaled_q16(
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
                    }
                    else "ground_velocity_x"
                ]
            )
        )
        actual_velocity = int(native["velocity_x_q16"])
        expected_velocity_y = scaled_y_q16(float(oracle["velocity_y"]))
        actual_velocity_y = int(native["velocity_y_q16"])
        expected_grounded = 1 if bool(oracle["grounded"]) else 0
        actual_grounded = int(native["grounded"])
        expected_shield_health = round(float(oracle["shield_health"]) * 65536.0)
        actual_shield_health = int(native["shield_health_q16"])
        expected_shield_strength = normalized_shield_strength(oracle)
        actual_shield_strength = int(native["shield_strength"])
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
        if abs(actual_position - expected_position) > args.position_tolerance_q16:
            differences.append(
                f"position_q16 expected={expected_position} actual={actual_position} "
                f"delta={actual_position - expected_position}"
            )
        if (
            abs(actual_position_y - expected_position_y)
            > args.position_tolerance_q16
        ):
            differences.append(
                "position_y_q16 "
                f"expected={expected_position_y} actual={actual_position_y} "
                f"delta={actual_position_y - expected_position_y}"
            )
        if abs(actual_velocity - expected_velocity) > args.velocity_tolerance_q16:
            differences.append(
                f"velocity_q16 expected={expected_velocity} actual={actual_velocity} "
                f"delta={actual_velocity - expected_velocity}"
            )
        if abs(actual_velocity_y - expected_velocity_y) > 32:
            differences.append(
                "velocity_y_q16 "
                f"expected={expected_velocity_y} actual={actual_velocity_y} "
                f"delta={actual_velocity_y - expected_velocity_y}"
            )
        if abs(actual_shield_health - expected_shield_health) > 64:
            differences.append(
                "shield_health_q16 "
                f"expected={expected_shield_health} actual={actual_shield_health} "
                f"delta={actual_shield_health - expected_shield_health}"
            )
        if actual_shield_strength != expected_shield_strength:
            differences.append(
                "shield_strength "
                f"expected={expected_shield_strength} actual={actual_shield_strength}"
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

    print(f"ssbm-movement-compare=pass frames={len(oracle_rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
