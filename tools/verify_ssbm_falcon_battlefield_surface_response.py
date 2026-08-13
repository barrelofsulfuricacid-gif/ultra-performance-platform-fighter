#!/usr/bin/env python3
"""Qualify Falcon's sloped Battlefield wall/ceiling response against NTSC 1.02."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from import_ssbm_stage_collision import compact_capture
from ssbm_live_trace import (
    canonical_sha256,
    normalized_sha256,
    parse_numeric_observations,
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
EXPECTED_SOURCE_SHA256 = {
    "ftCo_PassiveWall.c": (
        "5d05a8df6c5db5ba452a21ee0443f75d033433dd314f00f41d253c98c080daf0"
    ),
    "ftCo_PassiveCeil.c": (
        "ef79a00fe873d57e512bf2248240e65412cc3f8e2098aaa973ea229bc9adfd24"
    ),
    "ftCo_FlyReflect.c": (
        "8e14daf55997bc4f73e3d8b2b646472d2457bca49fbe81f8dc1d0c5797bceb76"
    ),
    "fighter.c": (
        "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f"
    ),
    "ftcommon.c": (
        "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194"
    ),
    "mpcoll.c": (
        "eb9fe73452c3e9d9677c1c1b0fdf809d18947a6582fb15920146139e1c194acd"
    ),
    "mplib.c": (
        "efb21dd888eae5dbd203931a6d66f6a360a1a1d57aa8306b1c12c734f64627b0"
    ),
}

ROUTE = "surface_response"
SIM_PREFIX = "m4-ssbm-battlefield-surface-response-observation "
ZERO_SHA256 = "0" * 64


def labeled_row(
    capture: dict[str, Any], case_id: str, suffix: str
) -> dict[str, Any]:
    label = f"{ROUTE}_{case_id}_{suffix}"
    rows = [row for row in capture["rows"] if row.get("label") == label]
    require_equal(len(rows), 1, f"{case_id} {suffix} row count")
    return rows[0]


def observed_rows(
    capture: dict[str, Any], case_id: str
) -> list[dict[str, Any]]:
    label = f"{ROUTE}_{case_id}_observe"
    return [row for row in capture["rows"] if row.get("label") == label]


def selected_surface(row: dict[str, Any], surface: str) -> dict[str, Any]:
    return row["surface_collision_memory"]["surfaces"][surface]


def semantic_source_digest(
    capture: dict[str, Any],
    coverage: dict[str, Any],
    stage_source: dict[str, Any],
) -> str:
    row_fields = (
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
    )
    cases: dict[str, Any] = {}
    for case_id, contact in coverage["live_contacts"].items():
        precontact = labeled_row(
            capture, case_id, contact["precontact_label_suffix"]
        )
        impact = labeled_row(capture, case_id, contact["impact_label_suffix"])
        surface = str(contact["surface"])
        impact_surface = selected_surface(impact, surface)
        cases[case_id] = {
            "precontact": {field: precontact[field] for field in row_fields},
            "impact": {
                **{field: impact[field] for field in row_fields},
                "surface": {
                    "index": impact_surface["index"],
                    "flags": impact_surface["flags"],
                    "normal": impact_surface["normal"],
                },
            },
            "observe": [
                {field: row[field] for field in row_fields}
                for row in observed_rows(capture, case_id)
            ],
        }
    return canonical_sha256(
        {
            "stage_collision": stage_source["source_stage_collision_sha256"],
            "cases": cases,
        }
    )


def qualify_source(
    capture: dict[str, Any], coverage: dict[str, Any]
) -> dict[str, list[dict[str, Any]]]:
    cases: dict[str, list[dict[str, Any]]] = {}
    for case_id, contact in coverage["live_contacts"].items():
        precontact = labeled_row(
            capture, case_id, contact["precontact_label_suffix"]
        )
        impact = labeled_row(capture, case_id, contact["impact_label_suffix"])
        rows = observed_rows(capture, case_id)
        surface_name = str(contact["surface"])
        surface = selected_surface(impact, surface_name)

        require_equal(len(rows), 12, f"{case_id} observe row count")
        require_equal(precontact["action"], "DAMAGE_FLY_NEUTRAL", f"{case_id} precontact action")
        require_equal(int(precontact["action_frame"]), 1, f"{case_id} precontact frame")
        require_equal(int(precontact["hitstun_left"]), 77, f"{case_id} precontact hitstun")
        for name in ("left_facing_wall", "right_facing_wall", "ceiling"):
            require_equal(
                int(selected_surface(precontact, name)["index"]),
                0xFFFFFFFF,
                f"{case_id} precontact {name}",
            )
        require_equal(impact["action"], contact["source_action"], f"{case_id} impact action")
        require_equal(int(impact["action_frame"]), 0, f"{case_id} impact frame")
        require_equal(int(impact["hitstun_left"]), 76, f"{case_id} impact hitstun")
        require_equal(bool(impact["invulnerable"]), True, f"{case_id} impact invulnerability")
        require_equal(int(surface["index"]), int(contact["line_index"]), f"{case_id} line")
        for axis, (actual, expected) in enumerate(
            zip(surface["normal"][:2], contact["normal"], strict=True)
        ):
            if abs(float(actual) - float(expected)) > 1.0e-6:
                raise SystemExit(f"{case_id} normal axis {axis} mismatch")
        require_equal([row["action"] for row in rows], [contact["source_action"]] * 12, f"{case_id} response action")
        expected_frames = (
            [*range(1, 9), *([8] * 4)]
            if contact["source_action"] == "BOUNCE_CEILING"
            else list(range(1, 13))
        )
        require_equal([int(row["action_frame"]) for row in rows], expected_frames, f"{case_id} response clock")
        require_equal([int(row["hitstun_left"]) for row in rows], list(range(75, 63, -1)), f"{case_id} response hitstun")
        if any(bool(row["grounded"]) or not bool(row["invulnerable"]) for row in rows):
            raise SystemExit(f"{case_id} response state invariant mismatch")
        cases[case_id] = rows
    return cases


def compare_sim(
    source_cases: dict[str, list[dict[str, Any]]],
    coverage: dict[str, Any],
    sim_output: Path,
) -> None:
    produced_cases = parse_numeric_observations(sim_output, SIM_PREFIX)
    require_equal(set(produced_cases), set(source_cases), "simulation case coverage")
    for case_id, source_rows in source_cases.items():
        produced_rows = produced_cases[case_id]
        contact = coverage["live_contacts"][case_id]
        require_equal(len(produced_rows), len(source_rows), f"{case_id} simulation rows")
        source_origin_x = float(source_rows[0]["position_x"])
        source_origin_y = float(source_rows[0]["position_y"])
        for index, (source, produced) in enumerate(
            zip(source_rows, produced_rows, strict=True), start=1
        ):
            exact = (
                (produced["frame"], index, "frame"),
                (produced["action"], int(contact["production_action"]), "action"),
                (produced["action_tick"], index, "action tick"),
                (produced["grounded"], int(bool(source["grounded"])), "grounded"),
                (produced["tumble"], 1, "tumble"),
                (produced["hitstun"], int(source["hitstun_left"]), "hitstun"),
                (produced["invulnerable"], int(bool(source["invulnerable"])), "invulnerable"),
            )
            for actual, expected, label in exact:
                require_equal(actual, expected, f"{case_id} frame {index} {label}")
            float32_values = (
                (produced["position_x"], source_x_to_sim_f32(float(source["position_x"]) - source_origin_x), "position x", 0.009765625),
                (produced["position_y"], source_y_to_sim_f32(float(source["position_y"]) - source_origin_y), "position y", 0.009765625),
                (produced["self_vx"], source_x_to_sim_f32(float(source["air_velocity_x"])), "self velocity x", 0.000244140625),
                (produced["self_vy"], source_y_to_sim_f32(float(source["velocity_y"])), "self velocity y", 0.000244140625),
                (produced["kb_vx"], source_x_to_sim_f32(float(source["attack_velocity_x"])), "knockback velocity x", 0.000244140625),
                (produced["kb_vy"], source_y_to_sim_f32(float(source["attack_velocity_y"])), "knockback velocity y", 0.000244140625),
            )
            for actual, expected, label, tolerance in float32_values:
                require_f32_close(
                    actual,
                    expected,
                    tolerance,
                    f"{case_id} frame {index} {label}",
                )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("stage_source", type=Path)
    parser.add_argument("sources", nargs=7, type=Path)
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
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    stage_source = json.loads(args.stage_source.read_text(encoding="utf-8"))
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
    require_equal(
        len(capture["rows"]),
        int(coverage["checkpoint_pack"]["expected_rows"]),
        "capture row count",
    )
    require_equal(
        compact_capture(
            args.capture,
            "BATTLEFIELD",
            "SSBM GALE01 NTSC-U revision 2",
        ),
        stage_source,
        "imported Battlefield collision catalog",
    )
    source_cases = qualify_source(capture, coverage)
    observed = semantic_source_digest(capture, coverage, stage_source)
    expected = coverage["stored_oracle"]["source_trace_sha256"]
    if args.allow_unpinned_capture:
        print(f"battlefield-surface-response-source-trace-sha256={observed}")
    elif expected == ZERO_SHA256 or observed != expected:
        raise SystemExit(
            "Battlefield surface-response source trace SHA-256 mismatch: "
            f"{observed} != {expected}"
        )
    if args.sim_output is not None:
        compare_sim(source_cases, coverage, args.sim_output)
    print(
        "ssbm-falcon-battlefield-surface-response=pass "
        f"rows={len(capture['rows'])} cases={len(source_cases)} "
        f"samples={sum(map(len, source_cases.values()))} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"source_trace_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
