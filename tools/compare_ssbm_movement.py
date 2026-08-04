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
}

# M4's Falcon movement values use a 12/115 world-unit scale relative to
# GALE01's Falcon attributes (for example, 2.0 becomes 24/115).
SSBM_TO_M4_Q16 = 65536.0 * 12.0 / 115.0


def controller_axis(value: float) -> int:
    return round((value - 0.5) * 65534.0)


def scaled_q16(value: float) -> int:
    return round(value * SSBM_TO_M4_Q16)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("runner", type=Path)
    parser.add_argument(
        "--position-tolerance-q16", type=int, default=384,
        help="allowed float-to-fixed position quantization difference",
    )
    parser.add_argument(
        "--velocity-tolerance-q16", type=int, default=32,
        help="allowed float-to-fixed velocity quantization difference",
    )
    args = parser.parse_args()

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    oracle_rows = capture["rows"]
    input_text = "".join(
        f"{controller_axis(float(row['observed_main_x']))}\n"
        for row in oracle_rows
    )
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

    for oracle, native in zip(oracle_rows, native_rows, strict=True):
        frame = int(oracle["trace_frame"])
        action_name = str(oracle["action"])
        expected_action = SSBM_TO_M4_ACTION.get(action_name)
        actual_action = int(native["action_state"])
        expected_facing = int(oracle["facing"])
        actual_facing = int(native["facing"])
        expected_position = scaled_q16(float(oracle["position_x_from_origin"]))
        actual_position = int(native["position_x_q16_from_origin"])
        expected_velocity = scaled_q16(float(oracle["ground_velocity_x"]))
        actual_velocity = int(native["velocity_x_q16"])
        differences: list[str] = []
        if expected_action is None:
            differences.append(f"unsupported_action={action_name}")
        elif actual_action != expected_action:
            differences.append(
                f"action expected={action_name}/{expected_action} actual={actual_action}"
            )
        if actual_facing != expected_facing:
            differences.append(
                f"facing expected={expected_facing} actual={actual_facing}"
            )
        if abs(actual_position - expected_position) > args.position_tolerance_q16:
            differences.append(
                f"position_q16 expected={expected_position} actual={actual_position} "
                f"delta={actual_position - expected_position}"
            )
        if abs(actual_velocity - expected_velocity) > args.velocity_tolerance_q16:
            differences.append(
                f"velocity_q16 expected={expected_velocity} actual={actual_velocity} "
                f"delta={actual_velocity - expected_velocity}"
            )
        if differences:
            print(
                "ssbm-movement-compare=fail "
                f"frame={frame} label={oracle['label']} " + " | ".join(differences)
            )
            return 1

    print(f"ssbm-movement-compare=pass frames={len(oracle_rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
