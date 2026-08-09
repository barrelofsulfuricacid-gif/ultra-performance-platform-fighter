#!/usr/bin/env python3
"""Qualify Falcon grounded player push against NTSC 1.02."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
EXPECTED_FTCOMMON_SHA256 = (
    "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194"
)
SOURCE_TO_PROJECT_Q16 = 65536.0 * 12.0 / 115.0
POSITION_TOLERANCE_Q16 = 640 + round(0.3 * SOURCE_TO_PROJECT_Q16)
VELOCITY_TOLERANCE_Q16 = 32
ACTION_MAP = {"STANDING": 0, "WALK_SLOW": 1, "WALK_MIDDLE": 1, "WALK_FAST": 1}


def normalized_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    prefix = f"player_push_{case_id}_"
    return [row for row in rows if str(row["label"]).startswith(prefix)]


def lane(row: dict[str, Any], index: int) -> dict[str, Any]:
    prefix = "" if index == 0 else "opponent_"
    return {
        "action": str(row[f"{prefix}action"]),
        "facing": int(row[f"{prefix}facing"]),
        "grounded": bool(row[f"{prefix}grounded"]),
        "position_x": float(row[f"{prefix}position_x_from_origin"]),
        "self_velocity_x": float(row[f"{prefix}ground_velocity_x"]),
    }


def source_trace_sha256(cases: dict[str, list[dict[str, Any]]]) -> str:
    canonical = [
        {
            "case": case_id,
            "samples": [
                [
                    {
                        **lane(row, lane_index),
                        "position_x": round(lane(row, lane_index)["position_x"], 6),
                        "self_velocity_x": round(
                            lane(row, lane_index)["self_velocity_x"], 6
                        ),
                    }
                    for lane_index in range(2)
                ]
                for row in rows
            ],
        }
        for case_id, rows in cases.items()
    ]
    encoded = json.dumps(
        canonical, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def qualify_source(cases: dict[str, list[dict[str, Any]]]) -> None:
    mover_lane = {"port_one_right": 0, "port_two_left": 1}
    direction = {"port_one_right": 1.0, "port_two_left": -1.0}
    if set(cases) != set(mover_lane):
        raise SystemExit("player-push source case set mismatch")
    for case_id, rows in cases.items():
        if len(rows) != 24:
            raise SystemExit(f"{case_id}: expected 24 source samples")
        moving = mover_lane[case_id]
        stationary = 1 - moving
        for row in rows:
            lanes = (lane(row, 0), lane(row, 1))
            if (
                not all(value["grounded"] for value in lanes)
                or [value["facing"] for value in lanes] != [1, -1]
                or any(value["action"] not in ACTION_MAP for value in lanes)
            ):
                raise SystemExit(f"{case_id}: discrete source state changed")
        moving_actions = [lane(row, moving)["action"] for row in rows]
        if (
            not all(action.startswith("WALK_") for action in moving_actions[:16])
            or moving_actions[16:] != ["STANDING"] * 8
        ):
            raise SystemExit(f"{case_id}: mover action boundary changed")
        stationary_positions = [lane(row, stationary)["position_x"] for row in rows]
        nudges = [
            current - previous
            for previous, current in zip(
                stationary_positions, stationary_positions[1:], strict=False
            )
            if not math.isclose(current, previous, rel_tol=0.0, abs_tol=1.0e-6)
        ]
        if len(nudges) < 8 or any(
            not math.isclose(
                value, 0.3 * direction[case_id], rel_tol=0.0, abs_tol=2.0e-5
            )
            for value in nudges
        ):
            raise SystemExit(f"{case_id}: fixed 0.3 player nudge changed")


def parse_sim(path: Path) -> dict[str, list[list[dict[str, int]]]]:
    prefix = "m4-ssbm-player-push-observation "
    flat: dict[str, dict[int, dict[int, dict[str, int]]]] = {}
    for line_text in path.read_text(encoding="utf-8").splitlines():
        if not line_text.startswith(prefix):
            continue
        fields = dict(token.split("=", 1) for token in line_text[len(prefix):].split())
        case_id = fields.pop("case")
        sample = int(fields.pop("sample")) - 1
        lane_index = int(fields.pop("lane"))
        flat.setdefault(case_id, {}).setdefault(sample, {})[lane_index] = {
            key: int(value) for key, value in fields.items()
        }
    return {
        case_id: [
            [samples[index][lane_index] for lane_index in range(2)]
            for index in range(24)
        ]
        for case_id, samples in flat.items()
    }


def compare_sim(
    source: dict[str, list[dict[str, Any]]],
    sim_path: Path,
) -> None:
    sim = parse_sim(sim_path)
    if set(sim) != set(source) or any(len(samples) != 24 for samples in sim.values()):
        raise SystemExit("simulation player-push coverage mismatch")
    for case_id, source_rows in source.items():
        for sample_index, (source_row, sim_lanes) in enumerate(
            zip(source_rows, sim[case_id], strict=True), start=1
        ):
            for lane_index, produced in enumerate(sim_lanes):
                expected = lane(source_row, lane_index)
                for field, expected_value in (
                    ("action", ACTION_MAP[expected["action"]]),
                    ("facing", expected["facing"]),
                    ("grounded", int(expected["grounded"])),
                ):
                    if produced[field] != expected_value:
                        raise SystemExit(
                            f"{case_id} sample {sample_index} lane {lane_index}: "
                            f"{field} {produced[field]} != {expected_value}"
                        )
                expected_position = round(expected["position_x"] * SOURCE_TO_PROJECT_Q16)
                if abs(produced["dx"] - expected_position) > POSITION_TOLERANCE_Q16:
                    raise SystemExit(
                        f"{case_id} sample {sample_index} lane {lane_index}: "
                        "position exceeded Q16.16 push allowance"
                    )
                expected_velocity = round(
                    expected["self_velocity_x"] * SOURCE_TO_PROJECT_Q16
                )
                if abs(produced["self_vx"] - expected_velocity) > VELOCITY_TOLERANCE_Q16:
                    raise SystemExit(
                        f"{case_id} sample {sample_index} lane {lane_index}: "
                        "velocity exceeded Q16.16 allowance"
                    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("ftcommon", type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()
    if normalized_sha256(args.ftcommon) != EXPECTED_FTCOMMON_SHA256:
        raise SystemExit("pinned ftcommon.c SHA-256 mismatch")
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    rows = list(capture["rows"])
    case_ids = [
        case["id"]
        for case in coverage["checkpoint_pack"]["capture_plan"]["player_push_cases"]
    ]
    cases = {case_id: case_rows(rows, case_id) for case_id in case_ids}
    observed = source_trace_sha256(cases)
    expected_source = coverage["stored_oracle"]["source_trace_sha256"]
    if args.allow_unpinned_capture:
        print(f"player-push-source-trace-sha256={observed}")
    elif observed != expected_source:
        raise SystemExit(f"player-push source trace SHA-256 mismatch: {observed}")
    if (
        len(rows) != coverage["checkpoint_pack"]["expected_rows"]
        or capture.get("schema") != 7
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("oracle_execution", {}).get("release_artifact_sha256")
        != EXPECTED_EXIAI_SHA256
        or capture.get("checkpoint_pack", {}).get("case_count") != 2
    ):
        raise SystemExit("player-push capture provenance mismatch")
    qualify_source(cases)
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output)
    print(
        "ssbm-falcon-player-push=pass "
        f"rows={len(rows)} cases={len(cases)} samples={len(rows) * 2} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"position_tolerance_q16={POSITION_TOLERANCE_Q16} "
        f"velocity_tolerance_q16={VELOCITY_TOLERANCE_Q16} "
        f"source_trace_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
