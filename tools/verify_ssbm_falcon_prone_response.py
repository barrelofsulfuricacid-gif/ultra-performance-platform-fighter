#!/usr/bin/env python3
"""Qualify Falcon's flat-floor prone/getup response against NTSC 1.02."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


EXPECTED_OBSERVATION_SHA256 = "fc91d42660ac0a8df8f0715b183b2ec97bccfe2ee0279491cadf915e64044438"
EXPECTED_SOURCE_SHA256 = {
    "ftCo_DownBound.c": "55aa8b5e58b9cfa714a5f7f4e7d48724733cd8fc1fd9db64c1e149ec56bca76f",
    "ftCo_Down.c": "5dea755df10b4f7caec9981cdbf95921dd0435aee627d438464644400acc0d22",
    "ftCo_DownAttack.c": "a2de807fd526aa0681aa08e285479535cf4bad944e5533bc26a704006c09445b",
    "ftCo_DownStand.c": "6ef3029502eebec4222449be6a533495c709bcbf784ca406f906117ae73213e1",
    "ft_0DF1.c": "6e4a3f8919973ce00fc4ab375b5aef36a0796ff94f020660bf917f228e1af379",
    "fighter.c": "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f",
    "ftcommon.c": "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194",
}
EXPECTED_DISC_SHA256 = "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
EXPECTED_EXIAI_SHA256 = "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
DOWN_BOUND_ACTION = "TECH_MISS_DOWN"
SIM_ACTIONS = {
    0: "STANDING",
    15: DOWN_BOUND_ACTION,
    23: "LYING_GROUND_DOWN",
    24: "NEUTRAL_GETUP",
    26: "GETUP_ATTACK",
}


def source_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def close(actual: float, expected: float, tolerance: float, label: str) -> None:
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=tolerance):
        raise SystemExit(f"{label}: {actual} != {expected} +/- {tolerance}")


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    prefix = f"floor_response_{case_id}_observe_"
    return [row for row in rows if row["label"].startswith(prefix)]


def action_rows(
    rows: list[dict[str, Any]], action: str
) -> list[dict[str, Any]]:
    return [row for row in rows if row["action"] == action]


def first_action_index(rows: list[dict[str, Any]], action: str) -> int:
    try:
        return next(index for index, row in enumerate(rows) if row["action"] == action)
    except StopIteration as error:
        raise SystemExit(f"missing source action {action}") from error


def require_action_frames(
    rows: list[dict[str, Any]], action: str, duration: int, label: str
) -> list[dict[str, Any]]:
    selected = action_rows(rows, action)
    frames = [int(row["action_frame"]) for row in selected]
    if len(selected) != duration or frames != list(range(1, duration + 1)):
        raise SystemExit(f"{label} {action} duration mismatch: {frames}")
    return selected


def semantic_rows(
    rows: list[dict[str, Any]], case_id: str
) -> list[dict[str, Any]]:
    start = first_action_index(rows, DOWN_BOUND_ACTION)
    lengths = {
        "timeout": 276,
        "buffered_a_getup_attack": 75,
        "buffered_b_getup_attack": 38,
        "c_up_getup_attack": 38,
        "c_roll_forward": 61,
        "main_roll_backward": 61,
        "up_neutral_getup": 38,
        "shield_neutral_getup": 56,
        "attack_over_roll_priority": 38,
        "c_roll_below_threshold": 36,
    }
    return rows[start : start + lengths[case_id]]


def observation_sha256(cases: dict[str, list[dict[str, Any]]]) -> str:
    canonical: dict[str, Any] = {}
    for case_id, rows in cases.items():
        canonical[case_id] = [
            {
                "action": row["action"],
                "action_frame": int(row["action_frame"]),
                "grounded": bool(row["grounded"]),
                "ground_velocity_x": round(float(row["ground_velocity_x"]), 5),
                "air_velocity_x": round(float(row["air_velocity_x"]), 5),
                "velocity_y": round(float(row["velocity_y"]), 5),
                "hitlag_left": int(row["hitlag_left"]),
                # This field is action-state union storage after the launch;
                # only the preserved/nonzero damage-response memory is stable
                # across equivalent checkpoint phases.
                "hitstun_memory": int(float(row["hitstun_left"]) > 0.0),
                "invulnerable": bool(row["invulnerable"]),
                "main_x": round(float(row["requested_main_x"]), 5),
                "main_y": round(float(row["requested_main_y"]), 5),
                "c_x": round(float(row["requested_c_x"]), 5),
                "c_y": round(float(row["requested_c_y"]), 5),
                "left": bool(row["requested_digital_left"]),
                "right": bool(row["requested_digital_right"]),
                "attack": bool(row["requested_attack"]),
                "special": bool(row["requested_special"]),
            }
            for row in semantic_rows(rows, case_id)
        ]
    encoded = json.dumps(
        canonical, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def require_invulnerability(
    rows: list[dict[str, Any]], protected: int, label: str
) -> None:
    expected = [True] * protected + [False] * (len(rows) - protected)
    actual = [bool(row["invulnerable"]) for row in rows]
    if actual != expected:
        raise SystemExit(f"{label} invulnerability boundary mismatch")


def qualify_live(cases: dict[str, list[dict[str, Any]]]) -> None:
    for case_id, rows in cases.items():
        start = first_action_index(rows, DOWN_BOUND_ACTION)
        down_bound = rows[start : start + 26]
        require_action_frames(down_bound, DOWN_BOUND_ACTION, 26, case_id)
        grounded = [bool(row["grounded"]) for row in down_bound]
        if grounded != [True] * 4 + [False] * 18 + [True] * 4:
            raise SystemExit(f"{case_id} DownBound ECB grounding mismatch")
        if any(float(row["hitlag_left"]) != 0.0 for row in rows):
            raise SystemExit(f"{case_id} response was contaminated by hitlag")
        floor = down_bound[-1]["surface_collision_memory"]["surfaces"]["floor"]
        if floor["index"] != 1:
            raise SystemExit(f"{case_id} Final Destination floor mismatch")
        close(float(floor["normal"][1]), 1.0, 1.0e-4, f"{case_id} floor normal")

    timeout = semantic_rows(cases["timeout"], "timeout")
    require_action_frames(timeout[:26], DOWN_BOUND_ACTION, 26, "timeout")
    wait = timeout[26:246]
    if len(wait) != 220 or any(row["action"] != "LYING_GROUND_DOWN" for row in wait):
        raise SystemExit("DownWait 220-frame timeout mismatch")
    require_action_frames(timeout[246:276], "NEUTRAL_GETUP", 30, "timeout")

    attack = require_action_frames(
        semantic_rows(cases["buffered_a_getup_attack"], "buffered_a_getup_attack")[26:],
        "GETUP_ATTACK",
        49,
        "buffered A",
    )
    require_invulnerability(attack, 27, "getup attack")
    for case_id in (
        "buffered_b_getup_attack",
        "c_up_getup_attack",
        "attack_over_roll_priority",
    ):
        branch = semantic_rows(cases[case_id], case_id)[26:]
        if len(branch) != 12 or any(row["action"] != "GETUP_ATTACK" for row in branch):
            raise SystemExit(f"{case_id} getup-attack branch mismatch")

    forward = require_action_frames(
        semantic_rows(cases["c_roll_forward"], "c_roll_forward")[26:],
        "GROUND_ROLL_FORWARD_DOWN",
        35,
        "C-stick forward roll",
    )
    backward = require_action_frames(
        semantic_rows(cases["main_roll_backward"], "main_roll_backward")[26:],
        "GROUND_ROLL_BACKWARD_DOWN",
        35,
        "main-stick backward roll",
    )
    require_invulnerability(forward, 19, "forward getup roll")
    require_invulnerability(backward, 25, "backward getup roll")
    if float(forward[-1]["position_x"]) <= float(forward[0]["position_x"]):
        raise SystemExit("forward getup roll root translation mismatch")
    if float(backward[-1]["position_x"]) >= float(backward[0]["position_x"]):
        raise SystemExit("backward getup roll root translation mismatch")

    neutral = require_action_frames(
        semantic_rows(cases["shield_neutral_getup"], "shield_neutral_getup")[26:],
        "NEUTRAL_GETUP",
        30,
        "shield neutral getup",
    )
    require_invulnerability(neutral, 23, "neutral getup")
    up_branch = semantic_rows(cases["up_neutral_getup"], "up_neutral_getup")[26:]
    if len(up_branch) != 12 or any(row["action"] != "NEUTRAL_GETUP" for row in up_branch):
        raise SystemExit("up-stick neutral-getup branch mismatch")

    negative = semantic_rows(
        cases["c_roll_below_threshold"], "c_roll_below_threshold"
    )[26:]
    if len(negative) != 10 or any(row["action"] != "LYING_GROUND_DOWN" for row in negative):
        raise SystemExit("sub-threshold C-stick incorrectly selected a roll")


def parse_sim(path: Path) -> dict[str, list[dict[str, int]]]:
    prefix = "m4-ssbm-prone-response-observation "
    cases: dict[str, list[dict[str, int]]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith(prefix):
            continue
        fields: dict[str, str] = {}
        for token in line[len(prefix) :].split():
            key, value = token.split("=", 1)
            fields[key] = value
        case_id = fields.pop("case")
        cases.setdefault(case_id, []).append(
            {key: int(value) for key, value in fields.items()}
        )
    return cases


def sim_x_to_source(value_q16: int) -> float:
    return float(value_q16) * 115.0 / (12.0 * 65536.0)


def compare_sim(
    live: dict[str, list[dict[str, Any]]], sim_path: Path
) -> None:
    sim = parse_sim(sim_path)
    if set(sim) != set(live) or any(len(rows) != 12 for rows in sim.values()):
        raise SystemExit("simulation prone-response coverage mismatch")

    for case_id, produced_rows in sim.items():
        source = semantic_rows(live[case_id], case_id)
        for produced in produced_rows:
            action = produced["action"]
            expected_action = SIM_ACTIONS.get(action)
            if action == 25:
                expected_action = (
                    "GROUND_ROLL_FORWARD_DOWN"
                    if case_id == "c_roll_forward"
                    else "GROUND_ROLL_BACKWARD_DOWN"
                )
            if expected_action is None:
                raise SystemExit(f"{case_id}: unrecognized simulation action {action}")
            candidates = action_rows(source, expected_action)
            action_tick = produced["action_tick"]
            if not candidates:
                # The compact source windows intentionally stop on the last
                # response frame, before the following standing observation.
                if expected_action == "STANDING":
                    continue
                raise SystemExit(
                    f"{case_id}: source action {expected_action} is missing"
                )
            if action_tick >= len(candidates):
                raise SystemExit(
                    f"{case_id}: action tick {action_tick} exceeds source "
                    f"{expected_action} coverage"
                )
            source_row = candidates[action_tick]
            if expected_action != "LYING_GROUND_DOWN" and int(
                source_row["action_frame"]
            ) != action_tick + 1:
                raise SystemExit(
                    f"{case_id}: source/simulation action frame mismatch"
                )
            if produced["invulnerable"] != int(bool(source_row["invulnerable"])):
                raise SystemExit(
                    f"{case_id} {expected_action} frame {action_tick + 1}: "
                    "invulnerability mismatch"
                )
            if action == 25:
                close(
                    sim_x_to_source(produced["self_vx"]),
                    float(source_row["air_velocity_x"]),
                    0.0015,
                    f"{case_id} frame {action_tick + 1} root velocity",
                )
                expected_direction = 1 if case_id == "c_roll_forward" else -1
                if produced["tech_direction"] != expected_direction:
                    raise SystemExit(f"{case_id}: roll direction mismatch")
            if action not in (0,) and produced["prone_orientation"] != 2:
                raise SystemExit(f"{case_id}: simulation prone orientation changed")

    print("prone-response-position-comparison=excluded-stage-pushbox-domain")
    print("prone-response-grounded-comparison=excluded-downbound-ecb-domain")
    print("prone-response-orientation-comparison=down-only-up-orientation-pending")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("sources", nargs=7, type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()
    for path in args.sources:
        expected = EXPECTED_SOURCE_SHA256.get(path.name)
        if expected is None or source_sha256(path) != expected:
            raise SystemExit(f"pinned {path.name} SHA-256 mismatch")
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    rows = list(capture["rows"])
    ids = [case["id"] for case in coverage["checkpoint_cases"]]
    cases = {case_id: case_rows(rows, case_id) for case_id in ids}
    if any(not rows for rows in cases.values()):
        raise SystemExit("prone-response case extraction mismatch")
    observed = observation_sha256(cases)
    if args.allow_unpinned_capture:
        print(f"prone-response-observation-sha256={observed}")
    elif observed != EXPECTED_OBSERVATION_SHA256:
        raise SystemExit(f"prone-response observation SHA-256 mismatch: {observed}")
    if len(rows) != int(coverage["checkpoint_pack"]["expected_rows"]):
        raise SystemExit("prone-response capture row count mismatch")
    if (
        capture.get("schema") != 11
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("oracle_execution", {}).get("release_artifact_sha256")
        != EXPECTED_EXIAI_SHA256
        or capture.get("checkpoint_pack", {}).get("case_count") != len(cases)
    ):
        raise SystemExit("prone-response capture provenance mismatch")
    qualify_live(cases)
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output)
    else:
        print("prone-response-position-comparison=excluded-stage-pushbox-domain")
        print("prone-response-orientation=down-only-up-orientation-pending")
    print(
        "ssbm-falcon-prone-response=pass "
        f"rows={len(rows)} cases={len(cases)} "
        f"samples={sum(len(case) for case in cases.values())} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"observation_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
