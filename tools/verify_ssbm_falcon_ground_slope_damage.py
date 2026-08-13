#!/usr/bin/env python3
"""Qualify Falcon's grounded damage response on a sloped source floor."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

from ssbm_live_trace import (
    canonical_sha256,
    normalized_sha256,
    parse_integer_observations,
    require_equal,
    require_f32_close,
    source_x_to_sim_f32,
    source_y_to_sim_f32,
    validate_capture_provenance,
)


EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
EXPECTED_DAMAGE_SOURCE_SHA256 = (
    "a3852f6377a71d03736b70b3869016a437b68c17dd703faead5be2954eb0278a"
)
ROUTE = "surface_response"
GROUND_CASE = "hyrule_line36_forward_tilt_grounded_projection"
AIR_CASE = "hyrule_line36_forward_tilt_airborne_departure"
JAB_CASE = "hyrule_line36_jab_low_speed_keep_damage"
IASA_SPECIAL_CASE = "hyrule_line36_jab_damage_iasa_special"
CASE_IDS = (GROUND_CASE, AIR_CASE, JAB_CASE, IASA_SPECIAL_CASE)
SAMPLES_PER_CASE = 30

ACTION_GROUND_IDLE = 0
ACTION_CROUCH = 4
ACTION_LANDING = 7
ACTION_HITLAG = 13
ACTION_HITSTUN = 14
ACTION_CROUCH_START = 104
ACTION_FALCON_PUNCH_GROUND = 107
ACTION_DAMAGE_LOW_2 = 131


def source_cases(capture: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    cases = {
        case_id: [
            row
            for row in capture["rows"]
            if str(row.get("label", "")).startswith(
                f"{ROUTE}_{case_id}_observe"
            )
        ]
        for case_id in CASE_IDS
    }
    for case_id, rows in cases.items():
        require_equal(len(rows), SAMPLES_PER_CASE, f"{case_id} row count")
    return cases


def qualify_source(cases: dict[str, list[dict[str, Any]]]) -> None:
    ground = cases[GROUND_CASE]
    air = cases[AIR_CASE]
    jab = cases[JAB_CASE]
    iasa_special = cases[IASA_SPECIAL_CASE]
    expected_ground_actions = [
        *(["CROUCH_START"] * 5),
        *(["DAMAGE_NEUTRAL_2"] * 13),
        *(["CROUCH_START"] * 7),
        *(["CROUCHING"] * 5),
    ]
    expected_air_actions = [
        *(["CROUCH_START"] * 5),
        *(["DAMAGE_NEUTRAL_2"] * 4),
        *(["LANDING"] * 4),
        *(["CROUCHING"] * 17),
    ]
    expected_jab_actions = [
        *(["DAMAGE_NEUTRAL_2"] * 24),
        *(["STANDING"] * 6),
    ]
    expected_iasa_special_actions = [
        *(["DAMAGE_NEUTRAL_2"] * 16),
        *(["NEUTRAL_B_ATTACKING_AIR"] * 14),
    ]
    require_equal(
        [row["action"] for row in ground],
        expected_ground_actions,
        "grounded slope action boundary",
    )
    require_equal(
        [row["action"] for row in air],
        expected_air_actions,
        "airborne slope action boundary",
    )
    require_equal(
        [row["action"] for row in jab],
        expected_jab_actions,
        "low-speed keep-damage action boundary",
    )
    require_equal(
        [row["action"] for row in iasa_special],
        expected_iasa_special_actions,
        "released damage special IASA boundary",
    )
    for case_id, rows in cases.items():
        jab_case = case_id in (JAB_CASE, IASA_SPECIAL_CASE)
        if not all(
            row["surface_collision_memory"]["surfaces"]["floor"]["index"]
            == 36
            and row["opponent_action"]
                in (
                    ("NEUTRAL_ATTACK_1", "STANDING")
                    if jab_case
                    else ("FTILT_MID",)
                )
            and int(row["damage_percent"])
                in ((0, 2) if jab_case else (0, 11))
            for row in rows
        ):
            raise SystemExit(f"{case_id}: setup invariant mismatch")

    ground_hit = ground[5]
    air_hit = air[5]
    if not (
        bool(ground_hit["grounded"])
        and not bool(air_hit["grounded"])
        and int(ground_hit["facing"]) == -1
        and int(air_hit["facing"]) == 1
        and float(ground_hit["attack_velocity_x"]) > 0.0
        and float(ground_hit["attack_velocity_y"]) > 0.0
        and float(air_hit["attack_velocity_x"]) < 0.0
        and float(air_hit["attack_velocity_y"]) == 0.0
    ):
        raise SystemExit("slope projection/departure discriminator mismatch")
    landing_index = next(
        index for index, row in enumerate(air) if row["action"] == "LANDING"
    )
    if not (
        landing_index > 0
        and math.hypot(
            float(air[landing_index - 1]["attack_velocity_x"]),
            float(air[landing_index - 1]["attack_velocity_y"]),
        ) >= 0.5
        and not bool(jab[14]["grounded"])
        and bool(jab[15]["grounded"])
        and jab[15]["action"] == "DAMAGE_NEUTRAL_2"
        and math.hypot(
            float(jab[14]["attack_velocity_x"]),
            float(jab[14]["attack_velocity_y"]),
        ) < 0.5
        and iasa_special[15]["action"] == "DAMAGE_NEUTRAL_2"
        and bool(iasa_special[15]["grounded"])
        and iasa_special[16]["action"] == "NEUTRAL_B_ATTACKING_AIR"
        and int(iasa_special[16]["hitstun_left"]) == 0
        and str(iasa_special[16]["label"]).endswith("_edge")
    ):
        raise SystemExit("damage floor magnitude selector mismatch")
    require_equal(
        [int(row["hitlag_left"]) for row in ground[5:9]],
        [4, 3, 2, 1],
        "grounded slope hitlag boundary",
    )
    require_equal(
        [int(row["hitstun_left"]) for row in ground[5:18]],
        [10, 10, 10, 10, *range(9, 0, -1)],
        "grounded slope hitstun boundary",
    )


def semantic_source_digest(
    cases: dict[str, list[dict[str, Any]]],
) -> str:
    fields = (
        "label",
        "action",
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
        "damage_percent",
        "opponent_action",
    )
    return canonical_sha256(
        {
            case_id: [
                {
                    **{field: row[field] for field in fields},
                    "floor": row["surface_collision_memory"]["surfaces"][
                        "floor"
                    ],
                }
                for row in rows
            ]
            for case_id, rows in cases.items()
        }
    )


def mapped_action(case_id: str, row: dict[str, Any]) -> int:
    action = str(row["action"])
    if action == "CROUCH_START":
        return ACTION_CROUCH_START
    if action == "CROUCHING":
        return ACTION_CROUCH
    if action == "LANDING":
        return ACTION_LANDING
    if action == "DAMAGE_NEUTRAL_2":
        if int(row["hitlag_left"]) > 0:
            return ACTION_HITLAG
        if case_id in (JAB_CASE, IASA_SPECIAL_CASE):
            return ACTION_HITSTUN
        return ACTION_DAMAGE_LOW_2 if bool(row["grounded"]) else ACTION_HITSTUN
    if action == "STANDING":
        return ACTION_GROUND_IDLE
    if action == "NEUTRAL_B_ATTACKING_AIR":
        return ACTION_FALCON_PUNCH_GROUND
    raise SystemExit(f"unmapped source action {action}")


def compare_sim(
    cases: dict[str, list[dict[str, Any]]],
    output: Path,
    manifest: dict[str, Any],
) -> None:
    produced = parse_integer_observations(
        output,
        "m4-ssbm-ground-slope-damage-observation ",
    )
    require_equal(set(produced), set(cases), "simulation case set")
    policy = manifest["stored_oracle"]["live_comparison"]
    velocity_tolerance = int(policy["velocity_tolerance_f32"])
    position_tolerance = int(policy["position_tolerance_f32"])
    drift = int(policy["position_drift_per_tick_f32"])
    if velocity_tolerance > 32 or position_tolerance > 192 or drift > 32:
        raise SystemExit("live tolerance exceeds the qualified Q16 envelope")

    for case_id, source in cases.items():
        actual_rows = produced[case_id]
        require_equal(len(actual_rows), len(source), f"{case_id} sample count")
        source_origin_x = float(source[0]["position_x"])
        source_origin_y = float(source[0]["position_y"])
        for index, (row, actual) in enumerate(
            zip(source, actual_rows, strict=True), 1
        ):
            label = f"{case_id} sample {index}"
            require_equal(actual["sample"], index, f"{label} index")
            require_equal(
                actual["action"],
                mapped_action(case_id, row),
                f"{label} action",
            )
            require_equal(
                actual["grounded"], int(bool(row["grounded"])),
                f"{label} grounded",
            )
            require_equal(
                actual["hitlag"], int(row["hitlag_left"]), f"{label} hitlag"
            )
            require_equal(
                actual["hitstun"],
                int(row["hitstun_left"]),
                f"{label} hitstun",
            )
            require_equal(actual["facing"], int(row["facing"]), f"{label} facing")
            require_f32_close(
                actual["kb_vx"],
                source_x_to_sim_f32(float(row["attack_velocity_x"])),
                velocity_tolerance,
                f"{label} knockback x",
            )
            require_f32_close(
                actual["kb_vy"],
                source_y_to_sim_f32(float(row["attack_velocity_y"])),
                velocity_tolerance,
                f"{label} knockback y",
            )
            accumulated = position_tolerance + (index - 1) * drift
            require_f32_close(
                actual["dx"],
                source_x_to_sim_f32(float(row["position_x"]) - source_origin_x),
                accumulated,
                f"{label} position x",
            )
            require_f32_close(
                actual["dy"],
                source_y_to_sim_f32(float(row["position_y"]) - source_origin_y),
                accumulated,
                f"{label} position y",
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("damage_source", type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()

    if normalized_sha256(args.damage_source) != EXPECTED_DAMAGE_SOURCE_SHA256:
        raise SystemExit("pinned ftCo_Damage.c SHA-256 mismatch")
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    manifest = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    validate_capture_provenance(
        capture,
        schema=11,
        stage="HYRULE_TEMPLE",
        fighter="CPTFALCON",
        opponent="CPTFALCON",
        disc_sha256=EXPECTED_DISC_SHA256,
        oracle_artifact_sha256=EXPECTED_EXIAI_SHA256,
        case_count=len(manifest["checkpoint_cases"]),
    )
    require_equal(
        len(capture["rows"]),
        manifest["checkpoint_pack"]["expected_rows"],
        "capture row count",
    )
    cases = source_cases(capture)
    qualify_source(cases)
    source_digest = semantic_source_digest(cases)
    expected_digest = manifest["stored_oracle"]["source_trace_sha256"]
    if args.allow_unpinned_capture:
        print(f"ground-slope-damage-source-trace-sha256={source_digest}")
    elif source_digest != expected_digest:
        raise SystemExit(
            f"ground slope source trace SHA-256 mismatch: "
            f"{source_digest} != {expected_digest}"
        )
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output, manifest)
    print(
        "ssbm-falcon-ground-slope-damage=pass "
        f"rows={len(capture['rows'])} cases={len(cases)} "
        f"samples={sum(map(len, cases.values()))} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"source_trace_sha256={source_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
