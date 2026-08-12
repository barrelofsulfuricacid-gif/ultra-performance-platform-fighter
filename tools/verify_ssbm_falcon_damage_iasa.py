#!/usr/bin/env python3
"""Qualify Falcon's released Damage and DamageFall air-dodge callbacks."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from ssbm_live_trace import (
    canonical_sha256,
    normalized_sha256,
    parse_integer_observations,
    require_equal,
    validate_capture_provenance,
)


EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
EXPECTED_SOURCE_SHA256 = {
    "ftCo_Damage.c": (
        "a3852f6377a71d03736b70b3869016a437b68c17dd703faead5be2954eb0278a"
    ),
    "ftCo_DamageFall.c": (
        "973ce744a0e1084377bef6cebdeca6631fb90a0f8a31694621e0c5052b896a8b"
    ),
    "ftCo_EscapeAir.c": (
        "cdff68de39d55855f1ca02b8e4af09ce856a1133cc21b23921a881b23e0dfaf6"
    ),
}
ROUTE = "surface_response"
DAMAGE_CASE = "hyrule_line36_damage_airdodge"
DAMAGE_FALL_CASE = "hyrule_tumble_damage_reject_airdodge"
CASE_IDS = (DAMAGE_CASE, DAMAGE_FALL_CASE)
CASE_ROWS = {DAMAGE_CASE: 30, DAMAGE_FALL_CASE: 38}
SIM_PREFIX = "m4-ssbm-damage-iasa-observation "
ACTION_HITSTUN = 14
ACTION_AIR_DODGE = 32


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
        require_equal(len(rows), CASE_ROWS[case_id], f"{case_id} row count")
    return cases


def qualify_source(cases: dict[str, list[dict[str, Any]]]) -> None:
    damage = cases[DAMAGE_CASE]
    damage_fall = cases[DAMAGE_FALL_CASE]

    require_equal(
        [row["action"] for row in damage],
        [*("DAMAGE_NEUTRAL_2" for _ in range(14)),
         *("AIRDODGE" for _ in range(16))],
        "released Damage action boundary",
    )
    require_equal(
        [int(row["hitstun_left"]) for row in damage[:14]],
        [13, 13, *range(12, 0, -1)],
        "released Damage hitstun boundary",
    )
    require_equal(
        [int(row["hitlag_left"]) for row in damage[:2]],
        [2, 1],
        "released Damage hitlag boundary",
    )
    if not (
        all(not bool(row["grounded"]) for row in damage)
        and all(int(row["damage_percent"]) == 2 for row in damage)
        and damage[13]["action"] == "DAMAGE_NEUTRAL_2"
        and int(damage[13]["hitstun_left"]) == 1
        and damage[14]["action"] == "AIRDODGE"
        and int(damage[14]["action_frame"]) == 1
        and int(damage[14]["hitstun_left"]) == 0
        and bool(damage[14]["requested_digital_left"])
        and bool(damage[14]["observed_digital_left"])
        and str(damage[14]["label"]).endswith("_edge")
        and all(
            row["opponent_action"] in ("NEUTRAL_ATTACK_1", "STANDING")
            for row in damage
        )
    ):
        raise SystemExit("released Damage air-dodge discriminator mismatch")

    require_equal(
        [row["action"] for row in damage_fall],
        [*("DAMAGE_FLY_NEUTRAL" for _ in range(35)),
         *("TUMBLING" for _ in range(3))],
        "released DamageFall action boundary",
    )
    require_equal(
        [int(row["hitstun_left"]) for row in damage_fall[:35]],
        [32, 32, 32, 32, *range(31, 0, -1)],
        "released DamageFall hitstun boundary",
    )
    require_equal(
        [int(row["hitlag_left"]) for row in damage_fall[:4]],
        [4, 3, 2, 1],
        "released DamageFall hitlag boundary",
    )
    if not (
        all(not bool(row["grounded"]) for row in damage_fall)
        and all(int(row["damage_percent"]) == 61 for row in damage_fall)
        and damage_fall[34]["action"] == "DAMAGE_FLY_NEUTRAL"
        and int(damage_fall[34]["hitstun_left"]) == 1
        and damage_fall[35]["action"] == "TUMBLING"
        and int(damage_fall[35]["action_frame"]) == 1
        and int(damage_fall[35]["hitstun_left"]) == 0
        and bool(damage_fall[35]["requested_digital_left"])
        and bool(damage_fall[35]["observed_digital_left"])
        and str(damage_fall[35]["label"]).endswith("_reject_edge")
        and all(row["action"] != "AIRDODGE" for row in damage_fall)
        and all(
            row["opponent_action"] in ("FTILT_MID", "STANDING")
            for row in damage_fall
        )
    ):
        raise SystemExit("released DamageFall air-dodge rejection mismatch")


def semantic_source_digest(cases: dict[str, list[dict[str, Any]]]) -> str:
    fields = (
        "label",
        "action",
        "action_frame",
        "grounded",
        "hitlag_left",
        "hitstun_left",
        "damage_percent",
        "opponent_action",
        "requested_digital_left",
        "observed_digital_left",
    )
    return canonical_sha256(
        {
            case_id: [
                {field: row[field] for field in fields}
                for row in rows
            ]
            for case_id, rows in cases.items()
        }
    )


def compare_sim(cases: dict[str, list[dict[str, Any]]], output: Path) -> None:
    produced = parse_integer_observations(output, SIM_PREFIX)
    require_equal(set(produced), set(cases), "simulation case coverage")

    expected = {
        DAMAGE_CASE: (ACTION_AIR_DODGE, 0),
        DAMAGE_FALL_CASE: (ACTION_HITSTUN, 1),
    }
    for case_id, (expected_action, expected_tumble) in expected.items():
        rows = produced[case_id]
        require_equal(len(rows), 2, f"{case_id} simulation row count")
        for index, row in enumerate(rows, 1):
            require_equal(row["sample"], index, f"{case_id} sample index")
            require_equal(row["action"], expected_action, f"{case_id} action")
            require_equal(row["grounded"], 0, f"{case_id} grounded")
            require_equal(row["tumble"], expected_tumble, f"{case_id} tumble")
            require_equal(row["hitstun"], 0, f"{case_id} hitstun")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("sources", nargs=3, type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()

    for path in args.sources:
        require_equal(
            normalized_sha256(path),
            EXPECTED_SOURCE_SHA256.get(path.name),
            f"pinned {path.name} SHA-256",
        )
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
        print(f"damage-iasa-source-trace-sha256={source_digest}")
    elif source_digest != expected_digest:
        raise SystemExit(
            "Damage IASA source trace SHA-256 mismatch: "
            f"{source_digest} != {expected_digest}"
        )
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output)
    print(
        "ssbm-falcon-damage-iasa=pass "
        f"rows={len(capture['rows'])} cases={len(cases)} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"source_trace_sha256={source_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
