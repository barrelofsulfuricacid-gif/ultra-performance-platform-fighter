#!/usr/bin/env python3
"""Qualify Falcon flat-floor missed-tech and tech responses against NTSC 1.02."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


EXPECTED_OBSERVATION_SHA256 = "85fd93638bcb26b8b6e405cb1008a396acf05d132e07c7f9dcc3b6993034dd3f"
EXPECTED_SOURCE_SHA256 = {
    "ftCo_DamageFall.c": "973ce744a0e1084377bef6cebdeca6631fb90a0f8a31694621e0c5052b896a8b",
    "ftCo_DamageIce.c": "e1f6905c5b1477eb8962a7e12423f0181bc41ff3f8b6c13901c1d6017a28042d",
    "ftCo_DownBound.c": "55aa8b5e58b9cfa714a5f7f4e7d48724733cd8fc1fd9db64c1e149ec56bca76f",
    "ftCo_Down.c": "5dea755df10b4f7caec9981cdbf95921dd0435aee627d438464644400acc0d22",
    "ftCo_DownAttack.c": "a2de807fd526aa0681aa08e285479535cf4bad944e5533bc26a704006c09445b",
    "ftCo_Passive.c": "aeb74abefac6b57ed31f1210e08d13e138d2953d1f115bbbe0b1b0404bb5e559",
    "ftCo_PassiveStand.c": "af7f97ddb111e188ffdc7699751d77a4fe5f2475d136242e7ac08575e16ae539",
    "fighter.c": "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f",
    "ftcommon.c": "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194",
}
EXPECTED_DISC_SHA256 = "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
EXPECTED_EXIAI_SHA256 = "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
RESPONSE_ACTIONS = {
    "flat_floor_missed_tech": "TECH_MISS_DOWN",
    "flat_floor_neutral_tech": "NEUTRAL_TECH",
    "flat_floor_forward_tech": "FORWARD_TECH",
    "flat_floor_backward_tech": "BACKWARD_TECH",
}


def source_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def close(actual: float, expected: float, tolerance: float, label: str) -> None:
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=tolerance):
        raise SystemExit(f"{label}: {actual} != {expected} +/- {tolerance}")


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    label = f"floor_response_{case_id}_observe"
    return [row for row in rows if row["label"] == label]


def response_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    action = RESPONSE_ACTIONS[case_id]
    return [row for row in rows if row["action"] == action]


def observation_sha256(cases: dict[str, list[dict[str, Any]]]) -> str:
    canonical: dict[str, Any] = {}
    for case_id, rows in cases.items():
        response = response_rows(rows, case_id)
        canonical[case_id] = [
            {
                "action": row["action"],
                "action_frame": int(row["action_frame"]),
                "grounded": bool(row["grounded"]),
                "ground_velocity_x": round(float(row["ground_velocity_x"]), 5),
                "air_velocity_x": round(float(row["air_velocity_x"]), 5),
                "velocity_y": round(float(row["velocity_y"]), 5),
                "hitlag_left": int(row["hitlag_left"]),
                "hitstun_memory": int(row["hitstun_left"] > 0),
                "invulnerable": bool(row["invulnerable"]),
            }
            for row in response[:12]
        ]
    encoded = json.dumps(
        canonical, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def parse_sim(path: Path) -> dict[str, list[dict[str, int]]]:
    prefix = "m4-ssbm-floor-response-observation "
    cases: dict[str, list[dict[str, int]]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith(prefix):
            continue
        fields: dict[str, str] = {}
        for token in line[len(prefix):].split():
            key, value = token.split("=", 1)
            fields[key] = value
        case_id = fields.pop("case")
        cases.setdefault(case_id, []).append(
            {key: int(value) for key, value in fields.items()}
        )
    return cases


def sim_x_to_source(value_f32: int) -> float:
    return float(value_f32) * 115.0 / (12.0 * 65536.0)


def qualify_live(cases: dict[str, list[dict[str, Any]]]) -> None:
    expected_counts = [54, 54, 68, 56]
    if [len(rows) for rows in cases.values()] != expected_counts:
        raise SystemExit("floor-response case row counts changed")
    expected_durations = {
        "flat_floor_missed_tech": 26,
        "flat_floor_neutral_tech": 26,
        "flat_floor_forward_tech": 40,
        "flat_floor_backward_tech": 40,
    }
    for case_id, rows in cases.items():
        response = response_rows(rows, case_id)
        duration = expected_durations[case_id]
        if len(response) != duration or [int(row["action_frame"]) for row in response] != list(range(1, duration + 1)):
            raise SystemExit(f"{case_id} response duration mismatch")
        floor = response[0]["surface_collision_memory"]["surfaces"]["floor"]
        if floor["index"] != 1:
            raise SystemExit(f"{case_id} Final Destination floor discriminator mismatch")
        close(float(floor["normal"][1]), 1.0, 1.0e-4, f"{case_id} floor normal")
        hitstun = [int(row["hitstun_left"]) for row in response]
        if len(set(hitstun)) != 1 or hitstun[0] <= 0:
            raise SystemExit(f"{case_id} did not preserve hitstun memory")
        expected_invulnerability = (
            [False] * duration if case_id == "flat_floor_missed_tech"
            else [True] * 20 + [False] * (duration - 20)
        )
        if [bool(row["invulnerable"]) for row in response] != expected_invulnerability:
            raise SystemExit(f"{case_id} invulnerability boundary mismatch")
        expected_grounded = (
            [True] * 4 + [False] * 18 + [True] * 4
            if case_id == "flat_floor_missed_tech"
            else [True] * duration
        )
        if [bool(row["grounded"]) for row in response] != expected_grounded:
            raise SystemExit(f"{case_id} ECB floor-contact boundary mismatch")
        entry_y = float(response[0]["attack_velocity_y"])
        next_y = float(response[1]["attack_velocity_y"])
        directional = case_id in {
            "flat_floor_forward_tech",
            "flat_floor_backward_tech",
        }
        if directional:
            if abs(entry_y) <= 0.001 or abs(next_y) > 0.001:
                raise SystemExit(
                    f"{case_id} PassiveStand entry-vector ownership mismatch"
                )
        elif abs(entry_y) > 0.001 or abs(next_y) > 0.001:
            raise SystemExit(
                f"{case_id} immediate ground projection mismatch"
            )
    missed_rows = cases["flat_floor_missed_tech"]
    missed_start = next(
        index for index, row in enumerate(missed_rows)
        if row["action"] == RESPONSE_ACTIONS["flat_floor_missed_tech"]
    )
    missed_follow = missed_rows[missed_start + 26:]
    if not missed_follow or missed_follow[0]["action"] != "LYING_GROUND_DOWN" or int(missed_follow[0]["hitstun_left"]) != 220:
        raise SystemExit("missed-tech DownWait 220-frame source value mismatch")
    neutral_rows = cases["flat_floor_neutral_tech"]
    neutral_start = next(
        index for index, row in enumerate(neutral_rows)
        if row["action"] == RESPONSE_ACTIONS["flat_floor_neutral_tech"]
    )
    neutral_follow = neutral_rows[neutral_start + 26:]
    if not neutral_follow or neutral_follow[0]["action"] != "STANDING":
        raise SystemExit("neutral-tech standing transition mismatch")


def compare_sim(live: dict[str, list[dict[str, Any]]], sim_path: Path) -> None:
    sim = parse_sim(sim_path)
    expected_actions = {
        "flat_floor_missed_tech": 15,
        "flat_floor_neutral_tech": 16,
        "flat_floor_forward_tech": 17,
        "flat_floor_backward_tech": 17,
    }
    if set(sim) != set(expected_actions) or any(len(rows) != 12 for rows in sim.values()):
        raise SystemExit("simulation floor-response coverage mismatch")
    for case_id, produced_rows in sim.items():
        source_rows = response_rows(live[case_id], case_id)[:12]
        produced_hitstun = [row["hitstun"] for row in produced_rows]
        if len(set(produced_hitstun)) != 1 or produced_hitstun[0] <= 0:
            raise SystemExit(f"{case_id}: simulation did not preserve hitstun memory")
        for index, (source, produced) in enumerate(zip(source_rows, produced_rows, strict=True)):
            for actual, expected, label in (
                (produced["frame"], index + 1, "sample"),
                (produced["action"], expected_actions[case_id], "action"),
                (
                    produced["grounded"],
                    int(bool(source["grounded"])),
                    "ECB floor contact",
                ),
                (
                    produced["invulnerable"],
                    int(bool(source["invulnerable"])),
                    "invulnerable",
                ),
            ):
                if actual != expected:
                    raise SystemExit(f"{case_id} frame {index + 1}: {label} {actual} != {expected}")
            expected_tick = index
            if produced["action_tick"] != expected_tick:
                raise SystemExit(f"{case_id} frame {index + 1}: action tick mismatch")
            if case_id.endswith("tech") and "neutral" not in case_id and index > 0:
                close(
                    sim_x_to_source(produced["self_vx"]),
                    float(source["air_velocity_x"]),
                    0.0015,
                    f"{case_id} frame {index + 1} root velocity",
                )
        directional = case_id in {
            "flat_floor_forward_tech",
            "flat_floor_backward_tech",
        }
        entry = produced_rows[0]
        following = produced_rows[1]
        if directional:
            if (
                entry["ground_kb"] != 0
                or entry["kb_vy"] == 0
                or following["ground_kb"] == 0
                or following["kb_vy"] != 0
            ):
                raise SystemExit(
                    f"{case_id}: PassiveStand x8c/xF0 ownership mismatch"
                )
        elif (
            entry["ground_kb"] == 0
            or entry["kb_vy"] != 0
            or following["ground_kb"] == 0
            or following["kb_vy"] != 0
        ):
            raise SystemExit(
                f"{case_id}: immediate ftCommon_8007CCE8 ownership mismatch"
            )
    print("floor-response-position-comparison=excluded-stage-pushbox-domain")
    print("floor-response-grounded-comparison=qualified-downbound-ecb")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("sources", nargs=9, type=Path)
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
    ids = [case["id"] for case in coverage["checkpoint_pack"]["capture_plan"]["floor_response_cases"]]
    cases = {case_id: case_rows(rows, case_id) for case_id in ids}
    observed = observation_sha256(cases)
    if args.allow_unpinned_capture:
        print(f"floor-response-observation-sha256={observed}")
    elif observed != EXPECTED_OBSERVATION_SHA256:
        raise SystemExit(f"floor-response observation SHA-256 mismatch: {observed}")
    if len(rows) != int(coverage["checkpoint_pack"]["expected_rows"]):
        raise SystemExit("floor-response capture row count mismatch")
    if (
        capture.get("schema") != 11
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("oracle_execution", {}).get("release_artifact_sha256") != EXPECTED_EXIAI_SHA256
        or capture.get("checkpoint_pack", {}).get("case_count") != 4
    ):
        raise SystemExit("floor-response capture provenance mismatch")
    qualify_live(cases)
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output)
    print(
        "ssbm-falcon-floor-response=pass "
        f"rows={len(rows)} cases={len(cases)} samples={sum(map(len, cases.values()))} "
        f"sim_trace={int(args.sim_output is not None)} observation_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
