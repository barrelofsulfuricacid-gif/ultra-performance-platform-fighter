#!/usr/bin/env python3
"""Qualify Falcon bounce-pose floor re-contact against NTSC 1.02."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any

from ssbm_live_trace import (
    canonical_sha256,
    normalized_sha256,
    parse_integer_observations,
    parse_integer_observations_text,
    require_equal,
    require_q16_close,
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
SIM_PREFIX = "m4-ssbm-battlefield-bounce-recontact-observation "
ZERO_SHA256 = "0" * 64


def observed_rows(capture: dict[str, Any], case_id: str) -> list[dict[str, Any]]:
    label = f"surface_response_{case_id}_observe"
    return [row for row in capture["rows"] if row.get("label") == label]


def focused_rows(
    capture: dict[str, Any], coverage: dict[str, Any]
) -> dict[str, list[dict[str, Any]]]:
    fields = (
        "action",
        "action_value",
        "action_frame",
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
        "facing",
    )
    return {
        case_id: [
            {field: row[field] for field in fields}
            for row in observed_rows(capture, case_id)[: int(route["landing_sample"])]
        ]
        for case_id, route in coverage["live_recontacts"].items()
    }


def semantic_source_digest(
    source_cases: dict[str, list[dict[str, Any]]], coverage: dict[str, Any]
) -> str:
    return canonical_sha256(
        {
            "bounce_ecb_semantic_q16_sha256": coverage["source_profile"][
                "semantic_q16_sha256"
            ],
            "cases": source_cases,
        }
    )


def qualify_source(
    capture: dict[str, Any], coverage: dict[str, Any]
) -> dict[str, list[dict[str, Any]]]:
    cases = focused_rows(capture, coverage)
    for case_id, route in coverage["live_recontacts"].items():
        all_rows = observed_rows(capture, case_id)
        rows = cases[case_id]
        landing_sample = int(route["landing_sample"])
        require_equal(len(all_rows), 80, f"{case_id} full observation count")
        require_equal(len(rows), landing_sample, f"{case_id} focused sample count")
        require_equal(rows[0]["action"], route["source_bounce_action"], f"{case_id} first action")
        require_equal(int(rows[0]["action_frame"]), 1, f"{case_id} first action frame")
        require_equal(int(rows[0]["hitstun_left"]), 75, f"{case_id} first hitstun")
        require_equal(bool(rows[0]["grounded"]), False, f"{case_id} first grounded")
        require_equal(int(rows[0]["facing"]), int(route["source_facing"]), f"{case_id} facing")
        require_equal(rows[-1]["action"], route["source_landing_action"], f"{case_id} landing action")
        require_equal(int(rows[-1]["action_frame"]), 1, f"{case_id} landing action frame")
        require_equal(bool(rows[-1]["grounded"]), True, f"{case_id} landing grounded")
        require_equal(bool(rows[-1]["invulnerable"]), False, f"{case_id} landing invulnerability")
        for index, row in enumerate(rows[:-1], start=1):
            require_equal(row["action"], route["source_bounce_action"], f"{case_id} sample {index} bounce action")
            require_equal(bool(row["grounded"]), False, f"{case_id} sample {index} airborne")
            require_equal(
                bool(row["invulnerable"]),
                index <= 14,
                f"{case_id} sample {index} invulnerability",
            )
        expected_hitstun = list(range(75, 75 - landing_sample, -1))
        require_equal(
            [int(row["hitstun_left"]) for row in rows],
            expected_hitstun,
            f"{case_id} hitstun countdown",
        )
    return cases


def compare_sim(
    source_cases: dict[str, list[dict[str, Any]]],
    coverage: dict[str, Any],
    sim_output: Path | str,
) -> None:
    produced_cases = (
        parse_integer_observations(sim_output, SIM_PREFIX)
        if isinstance(sim_output, Path)
        else parse_integer_observations_text(sim_output, SIM_PREFIX)
    )
    require_equal(set(produced_cases), set(source_cases), "simulation case coverage")
    for case_id, source_rows in source_cases.items():
        produced_rows = produced_cases[case_id]
        route = coverage["live_recontacts"][case_id]
        require_equal(len(produced_rows), len(source_rows), f"{case_id} simulation rows")
        source_origin_x = float(source_rows[0]["position_x"])
        source_origin_y = float(source_rows[0]["position_y"])
        landing_sample = int(route["landing_sample"])
        for index, (source, produced) in enumerate(
            zip(source_rows, produced_rows, strict=True), start=1
        ):
            landed = index == landing_sample
            # Melee's ceiling-bounce display clamps at frame 8 while the
            # response action clock continues. Production intentionally keeps
            # the same split between real action time and pose selection.
            source_action_tick = index if not landed else 0
            expected_action = int(
                route[
                    "production_landing_action" if landed else "production_bounce_action"
                ]
            )
            exact = (
                (produced["frame"], index, "frame"),
                (produced["action"], expected_action, "action"),
                (produced["action_tick"], source_action_tick, "action tick"),
                (produced["grounded"], int(bool(source["grounded"])), "grounded"),
                (produced["hitstun"], int(source["hitstun_left"]), "hitstun"),
                (produced["invulnerable"], int(bool(source["invulnerable"])), "invulnerable"),
                (produced["facing"], int(source["facing"]), "facing"),
            )
            for actual, expected, label in exact:
                require_equal(actual, expected, f"{case_id} frame {index} {label}")
            q16_values = (
                (produced["position_x"], source_x_to_sim_q16(float(source["position_x"]) - source_origin_x), "position x", 640),
                (produced["position_y"], source_y_to_sim_q16(float(source["position_y"]) - source_origin_y), "position y", 640),
                (produced["self_vx"], source_x_to_sim_q16(float(source["air_velocity_x"])), "self velocity x", 16),
                (produced["self_vy"], source_y_to_sim_q16(float(source["velocity_y"])), "self velocity y", 16),
                (produced["kb_vx"], source_x_to_sim_q16(float(source["attack_velocity_x"])), "knockback velocity x", 16),
                (produced["kb_vy"], source_y_to_sim_q16(float(source["attack_velocity_y"])), "knockback velocity y", 16),
            )
            for actual, expected, label, tolerance in q16_values:
                require_q16_close(actual, expected, tolerance, f"{case_id} frame {index} {label}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    parser.add_argument("--sim-executable", type=Path)
    args = parser.parse_args()

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    require_equal(
        normalized_sha256(args.capture),
        coverage["checkpoint_pack"]["capture_sha256"],
        "pinned capture SHA-256",
    )
    profile_path = args.coverage_manifest.parents[1] / coverage["source_profile"]["path"]
    require_equal(
        normalized_sha256(profile_path),
        coverage["source_profile"]["sha256"],
        "pinned bounce ECB profile SHA-256",
    )
    validate_capture_provenance(
        capture,
        schema=11,
        stage="BATTLEFIELD",
        fighter="CPTFALCON",
        opponent="CPTFALCON",
        disc_sha256=EXPECTED_DISC_SHA256,
        oracle_artifact_sha256=EXPECTED_EXIAI_SHA256,
        case_count=2,
    )
    require_equal(len(capture["rows"]), int(coverage["checkpoint_pack"]["expected_rows"]), "capture row count")
    source_cases = qualify_source(capture, coverage)
    observed = semantic_source_digest(source_cases, coverage)
    expected = coverage["stored_oracle"]["source_trace_sha256"]
    if args.allow_unpinned_capture:
        print(f"battlefield-bounce-recontact-source-trace-sha256={observed}")
    elif expected == ZERO_SHA256 or observed != expected:
        raise SystemExit(
            "Battlefield bounce re-contact source trace SHA-256 mismatch: "
            f"{observed} != {expected}"
        )
    require_equal(
        int(args.sim_output is not None) + int(args.sim_executable is not None) <= 1,
        True,
        "at most one simulation input",
    )
    sim_trace: Path | str | None = args.sim_output
    if args.sim_executable is not None:
        completed = subprocess.run(
            [
                str(args.sim_executable),
                "--ssbm-oracle",
                "falcon-common-battlefield-bounce-recontact",
            ],
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode not in (0, 1):
            raise SystemExit(
                "simulation observation runner failed: "
                f"exit={completed.returncode}\n{completed.stderr}"
            )
        sim_trace = completed.stdout
    if sim_trace is not None:
        compare_sim(source_cases, coverage, sim_trace)
    print(
        "ssbm-falcon-battlefield-bounce-recontact=pass "
        f"rows={len(capture['rows'])} cases={len(source_cases)} "
        f"samples={sum(map(len, source_cases.values()))} "
        f"sim_trace={int(sim_trace is not None)} "
        f"source_trace_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
