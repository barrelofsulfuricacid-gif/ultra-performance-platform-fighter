#!/usr/bin/env python3
"""Qualify Falcon slope, DownBound edge departure, and ordinary ledge catch."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

from import_ssbm_stage_collision import compact_capture
from ssbm_live_trace import (
    canonical_sha256,
    normalized_sha256,
    parse_integer_observations,
    require_equal,
    require_q16_close,
    select_labeled_rows,
    source_x_to_sim_q16,
    source_y_to_sim_q16,
    validate_capture_provenance,
)


EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
EXPECTED_SOURCE_SHA256 = {
    "ftCo_DownBound.c": (
        "55aa8b5e58b9cfa714a5f7f4e7d48724733cd8fc1fd9db64c1e149ec56bca76f"
    ),
    "ftCo_Down.c": (
        "5dea755df10b4f7caec9981cdbf95921dd0435aee627d438464644400acc0d22"
    ),
    "ftCo_Escape.c": (
        "762d18265d193e9d4b0b701a7a8048bb8824a4de5f505ceef00e316c1e56fb89"
    ),
    "ftCo_Fall.c": (
        "155a09a0b1243a77133db0113e9e49e5f6ef8b13d9f2c1a067bcc4a8243d233f"
    ),
    "ft_081B.c": (
        "0cb3fd9947c9ae2e6fd7f14836f2330d2f35d1544afeafff4980a97da2b15aac"
    ),
    "mpcoll.c": (
        "eb9fe73452c3e9d9677c1c1b0fdf809d18947a6582fb15920146139e1c194acd"
    ),
    "ftcliffcommon.c": (
        "a267a7a04a35ff31587608f1683206d566cf07855b7e422e55b45368c7ed627f"
    ),
    "fighter.c": (
        "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f"
    ),
    "ftcommon.c": (
        "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194"
    ),
}

ROUTE = "surface_response"
SLOPE_CASE = "hyrule_line34_forward_getup_roll"
LEDGE_CASE = "hyrule_line36_to_line37_natural_hit_departure"
LEDGE_ACCEPT_CASE = "ledge_grab_down_threshold_accept"
LEDGE_REJECT_CASE = "ledge_grab_down_threshold_reject"
SAMPLES_PER_CASE = 55
LEDGE_GRAB_DOWN_THRESHOLD = 0.6600000262260437
LEDGE_ACCEPT_REQUESTED_AXIS = -21400
LEDGE_REJECT_REQUESTED_AXIS = -21626
LEDGE_ACCEPT_OBSERVED_Y = 0.175000011920929
LEDGE_REJECT_OBSERVED_Y = 0.168749988079071

# Thin Python bindings for the public production action enum.  The comparator
# remains generic over integer observations; only this source-action mapping is
# Falcon/domain-specific.
ACTION_GROUND_IDLE = 0
ACTION_AIRBORNE = 6
ACTION_LEDGE_HANG = 8
ACTION_HITLAG = 13
ACTION_HITSTUN = 14
ACTION_KNOCKDOWN = 15
ACTION_ROLL = 25
ACTION_LEDGE_CATCH = 133


def source_cases(capture: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    slope = select_labeled_rows(
        capture,
        route=ROUTE,
        case_id=SLOPE_CASE,
        include_derived_labels=True,
    )
    ledge = select_labeled_rows(
        capture,
        route=ROUTE,
        case_id=LEDGE_CASE,
        include_derived_labels=True,
    )
    ledge_accept = select_labeled_rows(
        capture,
        route=ROUTE,
        case_id=LEDGE_ACCEPT_CASE,
        include_derived_labels=True,
    )
    ledge_reject = select_labeled_rows(
        capture,
        route=ROUTE,
        case_id=LEDGE_REJECT_CASE,
        include_derived_labels=True,
    )
    require_equal(len(slope), 90, "slope live row count")
    require_equal(len(ledge), 90, "ledge live row count")
    require_equal(len(ledge_accept), 55, "ledge accept live row count")
    require_equal(len(ledge_reject), 55, "ledge reject live row count")
    return {
        SLOPE_CASE: slope[24:79],
        LEDGE_CASE: ledge[:55],
        LEDGE_ACCEPT_CASE: ledge_accept,
        LEDGE_REJECT_CASE: ledge_reject,
    }


def semantic_source_digest(
    cases: dict[str, list[dict[str, Any]]],
    stage_source: dict[str, Any],
) -> str:
    row_fields = (
        "label",
        "requested_main_x",
        "requested_main_y",
        "observed_main_y",
        "requested_c_x",
        "requested_c_y",
        "requested_digital_left",
        "requested_digital_right",
        "action",
        "action_value",
        "action_frame",
        "facing",
        "grounded",
        "position_x",
        "position_y",
        "ground_velocity_x",
        "air_velocity_x",
        "velocity_y",
        "attack_velocity_x",
        "attack_velocity_y",
        "hitlag_left",
        "hitstun_left",
        "invulnerable",
    )
    canonical_cases: dict[str, list[dict[str, Any]]] = {}
    for case_id, rows in cases.items():
        canonical_rows: list[dict[str, Any]] = []
        for row in rows:
            floor = row["surface_collision_memory"]["surfaces"]["floor"]
            canonical_rows.append(
                {
                    **{field: row[field] for field in row_fields},
                    "floor": {
                        "index": floor["index"],
                        "flags": floor["flags"],
                        "normal": floor["normal"],
                    },
                }
            )
        canonical_cases[case_id] = canonical_rows
    return canonical_sha256(
        {
            "stage_collision": stage_source["source_stage_collision_sha256"],
            "cases": canonical_cases,
        }
    )


def qualify_stage(
    capture_path: Path,
    stage_source: dict[str, Any],
) -> None:
    regenerated = compact_capture(
        capture_path,
        "HYRULE_TEMPLE",
        "SSBM GALE01 NTSC-U revision 2",
    )
    require_equal(stage_source.get("schema"), 3, "stage source schema")
    for field in (
        "oracle",
        "stage",
        "coordinate_space",
        "simulation_transform",
        "ranges",
        "lines",
    ):
        require_equal(
            regenerated.get(field),
            stage_source.get(field),
            f"imported Hyrule collision catalog {field}",
        )
    require_equal(
        stage_source.get("source_stage_collision_sha256"),
        "4a0dd57bb8d9532589d3ecd129213d3a0876538a2dc7f733eca6c1e73c04db9c",
        "legacy Hyrule collision semantic digest",
    )
    require_equal(len(stage_source["lines"]), 91, "Hyrule collision line count")

    for index in (34, 35, 36, 37):
        line = stage_source["lines"][index]
        require_equal(line["index"], index, f"Hyrule line {index} index")
        require_equal(line["kind"], "floor", f"Hyrule line {index} kind")
        if int(line["runtime_flags"]) & (1 << 16) == 0:
            raise SystemExit(f"Hyrule line {index} is not runtime-enabled")
    line34 = stage_source["lines"][34]
    line37 = stage_source["lines"][37]
    require_equal(line34["neighbors"], [33, 71, -1, -1], "line 34 topology")
    require_equal(line37["neighbors"], [36, 74, -1, -1], "line 37 topology")
    if int(line37["lo_flags"]) & (1 << 9) == 0:
        raise SystemExit("Hyrule line 37 is missing its source ledge flag")


def qualify_source(cases: dict[str, list[dict[str, Any]]], stage: dict[str, Any]) -> None:
    slope = cases[SLOPE_CASE]
    ledge = cases[LEDGE_CASE]
    slope_actions = [row["action"] for row in slope]
    require_equal(
        slope_actions,
        [
            "TECH_MISS_DOWN",
            "TECH_MISS_DOWN",
            *(["GROUND_ROLL_FORWARD_DOWN"] * 35),
            *(["STANDING"] * 18),
        ],
        "slope response action boundary",
    )
    require_equal(
        [int(row["action_frame"]) for row in slope[:37]],
        [25, 26, *range(1, 36)],
        "slope response animation clock",
    )
    require_equal(
        [bool(row["invulnerable"]) for row in slope[2:37]],
        [*([True] * 19), *([False] * 16)],
        "slope roll invulnerability",
    )
    if not all(
        bool(row["grounded"])
        and row["surface_collision_memory"]["surfaces"]["floor"]["index"] == 34
        and int(row["hitlag_left"]) == 0
        and int(row["hitstun_left"]) == 15
        and int(row["facing"]) == 1
        for row in slope
    ):
        raise SystemExit("slope response state invariant mismatch")

    require_equal(
        [row["action"] for row in ledge],
        [
            *(["DAMAGE_FLY_NEUTRAL"] * 27),
            *(["TECH_MISS_UP"] * 7),
            *(["FALLING"] * 13),
            *(["EDGE_CATCHING"] * 7),
            "EDGE_HANGING",
        ],
        "ledge response action boundary",
    )
    require_equal(
        [int(row["hitlag_left"]) for row in ledge[:5]],
        [4, 3, 2, 1, 0],
        "DamageFly hitlag boundary",
    )
    require_equal(
        [int(row["action_frame"]) for row in ledge[27:34]],
        list(range(1, 8)),
        "DownBound animation clock",
    )
    require_equal(
        [bool(row["grounded"]) for row in ledge[27:35]],
        [True, True, True, True, False, False, False, False],
        "DownBound contactless departure boundary",
    )
    require_equal(
        [row["surface_collision_memory"]["surfaces"]["floor"]["index"]
         for row in ledge],
        [*([36] * 27), *([37] * 28)],
        "line 36 to line 37 route",
    )
    wait = ledge[-1]
    contact = ledge[47]["surface_collision_memory"]["contact"]
    endpoint = stage["lines"][37]["end"]
    if not (
        math.isclose(float(contact[0]), float(endpoint[0]), abs_tol=1.0e-4)
        and math.isclose(float(contact[1]), float(endpoint[1]), abs_tol=1.0e-4)
        and all(bool(row["invulnerable"]) for row in ledge[47:])
        and int(wait["facing"]) == -1
    ):
        raise SystemExit("ordinary ledge-catch endpoint discriminator mismatch")
    require_equal(
        [int(row["action_frame"]) for row in ledge[47:]],
        [*range(1, 8), 1],
        "EdgeCatch to EdgeWait animation clock",
    )

    ledge_accept = cases[LEDGE_ACCEPT_CASE]
    ledge_reject = cases[LEDGE_REJECT_CASE]
    require_equal(
        [row["action"] for row in ledge_accept],
        [
            *(["DAMAGE_FLY_NEUTRAL"] * 26),
            *(["TECH_MISS_UP"] * 4),
            *(["FALLING"] * 13),
            *(["EDGE_CATCHING"] * 7),
            *(["EDGE_HANGING"] * 5),
        ],
        "ledge grab down-threshold accept boundary",
    )
    require_equal(
        [row["action"] for row in ledge_reject],
        [
            *(["DAMAGE_FLY_NEUTRAL"] * 26),
            *(["TECH_MISS_UP"] * 4),
            *(["FALLING"] * 25),
        ],
        "ledge grab down-threshold reject boundary",
    )
    for name, rows, requested, observed in (
        (
            "accept",
            ledge_accept,
            LEDGE_ACCEPT_REQUESTED_AXIS,
            LEDGE_ACCEPT_OBSERVED_Y,
        ),
        (
            "reject",
            ledge_reject,
            LEDGE_REJECT_REQUESTED_AXIS,
            LEDGE_REJECT_OBSERVED_Y,
        ),
    ):
        expected_requested = (requested / 32767.0 + 1.0) * 0.5
        if not all(
            math.isclose(
                float(row["requested_main_y"]),
                expected_requested,
                abs_tol=1.0e-12,
            )
            and math.isclose(
                float(row["observed_main_y"]),
                observed,
                abs_tol=1.0e-12,
            )
            for row in rows
        ):
            raise SystemExit(f"ledge grab {name} input sample mismatch")
    accept_source_y = (LEDGE_ACCEPT_OBSERVED_Y - 0.5) * 2.0
    reject_source_y = (LEDGE_REJECT_OBSERVED_Y - 0.5) * 2.0
    if not (
        accept_source_y > -LEDGE_GRAB_DOWN_THRESHOLD
        and reject_source_y <= -LEDGE_GRAB_DOWN_THRESHOLD
    ):
        raise SystemExit("ledge grab source threshold discriminator mismatch")


def mapped_source_action(row: dict[str, Any], sample: int) -> int:
    action = str(row["action"])
    if action in ("TECH_MISS_DOWN", "TECH_MISS_UP"):
        return ACTION_KNOCKDOWN
    if action == "GROUND_ROLL_FORWARD_DOWN":
        return ACTION_ROLL
    if action == "STANDING":
        return ACTION_GROUND_IDLE
    if action == "DAMAGE_FLY_NEUTRAL":
        return ACTION_HITLAG if int(row["hitlag_left"]) > 0 else ACTION_HITSTUN
    if action == "FALLING":
        return ACTION_AIRBORNE
    if action == "EDGE_CATCHING":
        return ACTION_LEDGE_CATCH
    if action == "EDGE_HANGING":
        return ACTION_LEDGE_HANG
    raise SystemExit(f"sample {sample}: unmapped source action {action}")


def mapped_source_support(row: dict[str, Any]) -> int:
    # The source collision record retains its last floor index in ordinary air.
    # The simulator's support byte means an active constraint. DownBound is the
    # one contactless action whose collision callback still consumes that line.
    if bool(row["grounded"]) or row["action"] in ("TECH_MISS_UP", "TECH_MISS_DOWN"):
        return int(row["surface_collision_memory"]["surfaces"]["floor"]["index"]) + 1
    return 0


def compare_case(
    case_id: str,
    source: list[dict[str, Any]],
    produced: list[dict[str, int]],
    policy: dict[str, Any],
) -> None:
    require_equal(len(produced), SAMPLES_PER_CASE, f"{case_id} simulation samples")
    velocity_tolerance = int(
        policy.get("velocity_tolerance_q16", policy.get("q16_tolerance", 0))
    )
    position_tolerance = int(policy.get("position_tolerance_q16", 0))
    position_drift_per_tick = int(policy.get("position_drift_per_tick_q16", 0))
    if (
        velocity_tolerance > 32
        or position_tolerance > 192
        or position_drift_per_tick > velocity_tolerance
    ):
        raise SystemExit(f"{case_id}: live tolerance exceeds the qualified Q16 envelope")

    origin_x = float(source[0]["position_x"])
    origin_y = float(source[0]["position_y"])
    for index, (row, actual) in enumerate(zip(source, produced, strict=True), 1):
        prefix = f"{case_id} sample {index}"
        accumulated_position_tolerance = (
            position_tolerance + (index - 1) * position_drift_per_tick
        )
        expected_action = mapped_source_action(row, index)
        require_equal(actual["sample"], index, f"{prefix} sample index")
        require_equal(actual["action"], expected_action, f"{prefix} action")
        require_equal(actual["grounded"], int(bool(row["grounded"])), f"{prefix} grounded")
        require_equal(actual["support"], mapped_source_support(row), f"{prefix} support")
        require_equal(actual["hitlag"], int(row["hitlag_left"]), f"{prefix} hitlag")
        require_equal(actual["invulnerable"], int(bool(row["invulnerable"])), f"{prefix} invulnerable")
        require_equal(actual["facing"], int(row["facing"]), f"{prefix} facing")

        expected_tumble = int(str(row["action"]).startswith("DAMAGE_FLY_"))
        require_equal(actual["tumble"], expected_tumble, f"{prefix} tumble")
        expected_resume = ACTION_HITSTUN if expected_action == ACTION_HITLAG else 0
        require_equal(actual["resume"], expected_resume, f"{prefix} hitlag resume")

        require_equal(
            actual["hitstun"],
            int(row["hitstun_left"]),
            f"{prefix} hitstun storage",
        )

        source_frame = int(row["action_frame"])
        compare_clock = not (
            row["action"] == "STANDING" and source_frame > 1
        )
        if compare_clock:
            expected_tick = 0 if expected_action == ACTION_HITLAG else source_frame - 1
            require_equal(actual["action_tick"], expected_tick, f"{prefix} action tick")

        require_q16_close(
            actual["self_vx"],
            source_x_to_sim_q16(float(row["air_velocity_x"])),
            velocity_tolerance,
            f"{prefix} self velocity x",
        )
        require_q16_close(
            actual["self_vy"],
            source_y_to_sim_q16(float(row["velocity_y"])),
            velocity_tolerance,
            f"{prefix} self velocity y",
        )
        if case_id in (LEDGE_CASE, LEDGE_ACCEPT_CASE, LEDGE_REJECT_CASE):
            require_q16_close(
                actual["kb_vx"],
                source_x_to_sim_q16(float(row["attack_velocity_x"])),
                velocity_tolerance,
                f"{prefix} knockback velocity x",
            )
            require_q16_close(
                actual["kb_vy"],
                source_y_to_sim_q16(float(row["attack_velocity_y"])),
                velocity_tolerance,
                f"{prefix} knockback velocity y",
            )

            # The compact runtime stage reproduces line 36/37 motion through
            # the response. X becomes a wall-response domain after sample 43;
            # The intervening wall-contact X span remains owned by the
            # separate surface domain. EdgeCatch/EdgeWait are source-root
            # relative again and re-enter the position comparison.
            if index <= 43 or index >= 48:
                require_q16_close(
                    actual["dx"],
                    source_x_to_sim_q16(float(row["position_x"]) - origin_x),
                    accumulated_position_tolerance,
                    f"{prefix} position x",
                )
            require_q16_close(
                actual["dy"],
                source_y_to_sim_q16(float(row["position_y"]) - origin_y),
                accumulated_position_tolerance,
                f"{prefix} position y",
            )


def compare_sim(
    cases: dict[str, list[dict[str, Any]]],
    sim_path: Path,
    coverage: dict[str, Any],
) -> None:
    produced = parse_integer_observations(
        sim_path,
        "m4-ssbm-slope-ledge-response-observation ",
    )
    require_equal(set(produced), set(cases), "simulation case set")
    policies = coverage["stored_oracle"]["live_comparison"]
    for case_id, source in cases.items():
        compare_case(case_id, source, produced[case_id], policies[case_id])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("stage_source", type=Path)
    parser.add_argument("sources", nargs=9, type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()

    for path in args.sources:
        expected = EXPECTED_SOURCE_SHA256.get(path.name)
        if expected is None or normalized_sha256(path) != expected:
            raise SystemExit(f"pinned {path.name} SHA-256 mismatch")

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    stage_source = json.loads(args.stage_source.read_text(encoding="utf-8"))
    validate_capture_provenance(
        capture,
        schema=11,
        stage="HYRULE_TEMPLE",
        fighter="CPTFALCON",
        opponent="CPTFALCON",
        disc_sha256=EXPECTED_DISC_SHA256,
        oracle_artifact_sha256=EXPECTED_EXIAI_SHA256,
        case_count=len(coverage["checkpoint_cases"]),
    )
    require_equal(
        len(capture["rows"]),
        int(coverage["checkpoint_pack"]["expected_rows"]),
        "capture row count",
    )
    qualify_stage(args.capture, stage_source)
    cases = source_cases(capture)
    qualify_source(cases, stage_source)
    observed = semantic_source_digest(cases, stage_source)
    expected = coverage["stored_oracle"]["source_trace_sha256"]
    if args.allow_unpinned_capture:
        print(f"slope-ledge-response-source-trace-sha256={observed}")
    elif observed != expected:
        raise SystemExit(
            "slope-ledge-response source trace SHA-256 mismatch: "
            f"{observed} != {expected}"
        )
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output, coverage)
    print(
        "ssbm-falcon-slope-ledge-response=pass "
        f"rows={len(capture['rows'])} cases={len(cases)} "
        f"samples={sum(map(len, cases.values()))} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"source_trace_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
