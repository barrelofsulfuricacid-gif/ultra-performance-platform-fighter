#!/usr/bin/env python3
"""Verify Falcon Raptor Boost's stored projection against live SSBM captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from compare_ssbm_movement import (
    native_input_line,
    normalized_shield_strength,
    raptor_boost_expected_action,
    raptor_boost_expected_ticks,
    scaled_q16,
    scaled_y_q16,
)
from generate_ssbm_stored_trace_oracle import (
    expand_case_samples,
    native_csv_input_line,
)
from ssbm_live_trace import canonical_sha256, selected_trace_fields


GALE01_NTSC102_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)


def fail(message: str) -> None:
    raise SystemExit(f"ssbm-falcon-raptor-boost-source=fail reason={message}")


def raw_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_capture(
    path: Path,
    expected_sha256: str,
    live_source: dict[str, Any],
) -> dict[str, Any]:
    actual_sha256 = raw_sha256(path)
    if actual_sha256 != expected_sha256:
        fail(
            f"capture-sha256 path={path} expected={expected_sha256} "
            f"actual={actual_sha256}"
        )
    capture = json.loads(path.read_text(encoding="utf-8"))
    disc = capture.get("disc", {})
    probe = capture.get("hitbox_memory_probe", {})
    if (
        capture.get("schema") != 9
        or capture.get("oracle") != "SSBM GALE01 NTSC-U revision 2 via Dolphin/Slippi"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != GALE01_NTSC102_SHA256
        or capture.get("dolphin_version") != live_source.get("dolphin_version")
        or capture.get("libmelee_version") != live_source.get("libmelee_version")
        or probe.get("engine_version") != live_source.get("memory_engine_version")
        or probe.get("decomp_revision") != live_source.get("decomp_revision")
    ):
        fail(f"capture-provenance path={path}")
    return capture


def select_route_rows(
    capture: dict[str, Any],
    case_id: str,
) -> list[dict[str, Any]]:
    start_labels = {
        "ground_hit": "special_geometry_side_ground_hit_start",
        "ground_miss": "special_geometry_side_ground_miss_start",
        "air_miss": "special_geometry_side_air_miss_start",
        "air_hit_floor": "special_geometry_side_air_hit_floor_start",
        "ground_edge": "special_geometry_side_ground_edge_start",
    }
    start_label = start_labels[case_id]
    rows = list(capture.get("rows", []))
    indices = [
        index for index, row in enumerate(rows) if row.get("label") == start_label
    ]
    if len(indices) != 1:
        fail(f"route-start case={case_id} count={len(indices)}")
    selected = rows[indices[0] :]
    if case_id in {"ground_hit", "ground_miss"}:
        end = next(
            (
                index
                for index, row in enumerate(selected)
                if index > 0 and row.get("action") == "STANDING"
            ),
            None,
        )
        if end is None:
            fail(f"route-end case={case_id} action=STANDING")
        return selected[: end + 1]
    if case_id == "ground_edge":
        first_fall = next(
            (
                index
                for index, row in enumerate(selected)
                if row.get("action") == "DEAD_FALL"
            ),
            None,
        )
        if first_fall is None or first_fall + 32 > len(selected):
            fail("route-end case=ground_edge action=DEAD_FALL")
        return selected[: first_fall + 32]
    return selected


def source_sample_values(
    row: dict[str, Any],
    route: str,
    anchor_x: float,
    anchor_y: float,
    shield_strength: int,
) -> dict[str, int | None]:
    action = str(row["action"])
    action_frame = float(row["action_frame"])
    hitlag_left = float(row.get("hitlag_left", 0.0))
    velocity_x_key = (
        "air_velocity_x" if not bool(row["grounded"]) else "ground_velocity_x"
    )
    return {
        "action_state": raptor_boost_expected_action(
            route,
            action,
            action_frame,
            hitlag_left,
        ),
        "action_ticks": raptor_boost_expected_ticks(
            route,
            action,
            action_frame,
            hitlag_left,
        ),
        "facing": int(row["facing"]),
        "grounded": int(bool(row["grounded"])),
        "position_x_q16_from_origin": scaled_q16(
            float(row["position_x_from_origin"]) - anchor_x
        ),
        "position_y_q16_from_origin": scaled_y_q16(
            float(row["position_y"]) - anchor_y
        ),
        "velocity_x_q16": scaled_q16(float(row[velocity_x_key])),
        "velocity_y_q16": scaled_y_q16(float(row["velocity_y"])),
        "shield_health_q16": round(float(row["shield_health"]) * 65536.0),
        "shield_strength": shield_strength,
        "hitlag_ticks": round(hitlag_left),
        "opponent_hitlag_ticks": round(
            float(row.get("opponent_hitlag_left", 0.0))
        ),
        "opponent_damage_q16": round(
            float(row.get("opponent_damage_percent", 0.0)) * 65536.0
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coverage", type=Path)
    parser.add_argument("ground_hit_capture", type=Path)
    parser.add_argument("remaining_capture", type=Path)
    parser.add_argument("air_hit_capture", type=Path)
    parser.add_argument("ground_edge_capture", type=Path)
    parser.add_argument("item_search_capture", type=Path)
    args = parser.parse_args()

    coverage = json.loads(args.coverage.read_text(encoding="utf-8"))
    if (
        coverage.get("schema") != 1
        or coverage.get("domain") != "falcon-side-special"
    ):
        fail("coverage-manifest")
    live_source = coverage.get("live_source", {})
    stored = coverage.get("stored_oracle", {})
    if (
        not isinstance(live_source, dict)
        or stored.get("kind") != "native-csv-trace-v1"
    ):
        fail("coverage-schema")

    remaining_capture = load_capture(
        args.remaining_capture,
        str(live_source["miss_capture_sha256"]),
        live_source,
    )
    captures = {
        "ground_hit": load_capture(
            args.ground_hit_capture,
            str(live_source["ground_hit_capture_sha256"]),
            live_source,
        ),
        "ground_miss": remaining_capture,
        "air_miss": remaining_capture,
        "air_hit_floor": load_capture(
            args.air_hit_capture,
            str(live_source["air_hit_floor_capture_sha256"]),
            live_source,
        ),
        "ground_edge": load_capture(
            args.ground_edge_capture,
            str(live_source["ground_edge_capture_sha256"]),
            live_source,
        ),
    }
    load_capture(
        args.item_search_capture,
        str(live_source["item_search_capture_sha256"]),
        live_source,
    )

    default_fields = stored.get("serialized_fields")
    raw_cases = stored.get("cases")
    if not isinstance(default_fields, list) or not isinstance(raw_cases, list):
        fail("stored-cases")
    canonical_cases: list[dict[str, Any]] = []
    total_samples = 0
    for case in raw_cases:
        if not isinstance(case, dict) or case.get("id") not in captures:
            fail("stored-case")
        case_id = str(case["id"])
        rows = select_route_rows(captures[case_id], case_id)
        sample_count = case.get("sample_count")
        if len(rows) != sample_count:
            fail(
                f"sample-count case={case_id} expected={sample_count} "
                f"actual={len(rows)}"
            )
        inputs = expand_case_samples(case, case_id, len(rows), "inputs")
        stored_input_lines = [
            native_csv_input_line(sample, case_id, sample_index)
            for sample_index, sample in enumerate(inputs)
        ]
        live_input_lines = [native_input_line(row) for row in rows]
        if live_input_lines != stored_input_lines:
            mismatch = next(
                index
                for index, (live, saved) in enumerate(
                    zip(live_input_lines, stored_input_lines, strict=True)
                )
                if live != saved
            )
            fail(
                f"input case={case_id} sample={mismatch} "
                f"live={live_input_lines[mismatch]} "
                f"stored={stored_input_lines[mismatch]}"
            )

        fields = case.get("serialized_fields", default_fields)
        exclusions = case.get("field_exclusions", {})
        if not isinstance(fields, list) or not isinstance(exclusions, dict):
            fail(f"field-mask case={case_id}")
        anchor_x = 0.0
        if case_id in {"ground_hit", "ground_edge"}:
            anchor_x = float(rows[0]["position_x_from_origin"]) - float(
                rows[0]["ground_velocity_x"]
            )
        elif case_id in {"air_miss", "air_hit_floor"}:
            anchor_x = float(rows[0]["position_x_from_origin"])
        anchor_y = (
            float(rows[0]["position_y"])
            if case_id in {"air_miss", "air_hit_floor"}
            else 0.0
        )
        samples: list[dict[str, int]] = []
        retained_shield_strength = 0
        for sample_index, row in enumerate(rows):
            retained_shield_strength = normalized_shield_strength(
                row,
                retained_shield_strength,
            )
            values = source_sample_values(
                row,
                case_id,
                anchor_x,
                anchor_y,
                retained_shield_strength,
            )
            selected_fields = selected_trace_fields(
                fields,
                exclusions,
                sample_index,
            )
            ticks_qualified = values["action_ticks"] is not None
            ticks_selected = "action_ticks" in selected_fields
            if ticks_qualified != ticks_selected:
                fail(
                    f"action-tick-mask case={case_id} sample={sample_index} "
                    f"qualified={int(ticks_qualified)} selected={int(ticks_selected)}"
                )
            sample: dict[str, int] = {}
            for field in selected_fields:
                value = values.get(field)
                if not isinstance(value, int):
                    fail(
                        f"source-field case={case_id} sample={sample_index} "
                        f"field={field}"
                    )
                sample[field] = value
            samples.append(sample)
        canonical_cases.append({"id": case_id, "samples": samples})
        total_samples += len(samples)

    canonical = {
        "schema": 1,
        "domain": str(coverage["domain"]),
        "cases": canonical_cases,
    }
    observed_digest = canonical_sha256(canonical)
    expected_digest = stored.get("source_trace_sha256")
    if observed_digest != expected_digest:
        fail(
            f"source-trace-sha256 expected={expected_digest} actual={observed_digest}"
        )
    print(
        "ssbm-falcon-raptor-boost-source=pass "
        f"stored_cases={len(canonical_cases)} stored_samples={total_samples} "
        f"source_trace_sha256={observed_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
