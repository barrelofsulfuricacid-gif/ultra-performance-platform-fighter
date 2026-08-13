#!/usr/bin/env python3
"""Validate the pinned Falcon Punch source projection used by the stored gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from compare_ssbm_movement import (
    GALE01_NTSC102_SHA256,
    SSBM_TO_M4_ACTION,
    expected_action_ticks,
    scaled_q16,
    scaled_y_q16,
)


def fail(reason: str) -> None:
    raise SystemExit(f"ssbm-falcon-punch-source=fail reason={reason}")


def load_capture(
    path: Path,
    expected_sha256: str,
    source: dict[str, Any],
) -> dict[str, Any]:
    raw = path.read_bytes()
    actual_sha256 = hashlib.sha256(raw).hexdigest()
    if actual_sha256 != expected_sha256:
        fail(
            f"capture-sha256 path={path} expected={expected_sha256} "
            f"actual={actual_sha256}"
        )
    capture = json.loads(raw)
    disc = capture.get("disc")
    probe = capture.get("hitbox_memory_probe")
    if (
        capture.get("schema") != 9
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("dolphin_version") != source.get("dolphin_version")
        or capture.get("libmelee_version") != source.get("libmelee_version")
        or not isinstance(disc, dict)
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != GALE01_NTSC102_SHA256
        or not isinstance(probe, dict)
        or probe.get("engine_version") != source.get("memory_engine_version")
        or probe.get("decomp_revision") != source.get("decomp_revision")
    ):
        fail(f"capture-provenance path={path}")
    return capture


def route_rows(
    capture: dict[str, Any], start_label: str, action_name: str
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    rows = capture.get("rows")
    if not isinstance(rows, list):
        fail(f"missing-rows label={start_label}")
    try:
        start = next(
            index
            for index, row in enumerate(rows)
            if row.get("label") == start_label
        )
    except StopIteration:
        fail(f"missing-start label={start_label}")
    owned = rows[start : start + 99]
    if len(owned) != 99 or any(
        row.get("action") != action_name
        or round(float(row.get("action_frame", -1))) != index + 1
        for index, row in enumerate(owned)
    ):
        fail(f"action-span label={start_label}")
    if start + 99 >= len(rows):
        fail(f"missing-successor label={start_label}")
    successor = rows[start + 99]
    if bool(owned[0].get("observed_special")) is not True or any(
        bool(row.get("observed_special")) for row in owned[1:]
    ):
        fail(f"special-edge label={start_label}")
    return owned, successor


def state(row: dict[str, Any], horizontal_mirror: int = 1) -> dict[str, int]:
    action = str(row.get("action"))
    if action not in SSBM_TO_M4_ACTION:
        fail(f"unsupported-action action={action}")
    action_ticks = expected_action_ticks(
        action, float(row.get("action_frame", 0.0))
    )
    return {
        "action_state": SSBM_TO_M4_ACTION[action],
        "action_ticks": 0 if action_ticks is None else action_ticks,
        "grounded": int(bool(row.get("grounded"))),
        "facing": int(row.get("facing", 0)) * horizontal_mirror,
    }


def physics(
    row: dict[str, Any], anchor: dict[str, Any], horizontal_mirror: int
) -> dict[str, int]:
    result = state(row, horizontal_mirror)
    velocity_key = "ground_velocity_x" if bool(row.get("grounded")) else "air_velocity_x"
    result.update(
        {
            "position_x_q16": horizontal_mirror
            * scaled_q16(
                float(row.get("position_x_from_origin", 0.0))
                - float(anchor.get("position_x_from_origin", 0.0))
            ),
            "position_y_q16": scaled_y_q16(
                float(row.get("position_y", 0.0))
                - float(anchor.get("position_y", 0.0))
            ),
            "self_velocity_x_q16": horizontal_mirror
            * scaled_q16(float(row.get(velocity_key, 0.0))),
            "self_velocity_y_q16": scaled_y_q16(
                float(row.get("velocity_y", 0.0))
            ),
        }
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("ground_capture", type=Path)
    parser.add_argument("air_capture", type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    source = manifest.get("live_source")
    stored = manifest.get("stored_oracle")
    if (
        manifest.get("schema") != 1
        or not isinstance(source, dict)
        or not isinstance(stored, dict)
    ):
        fail("manifest")
    ground = load_capture(
        args.ground_capture,
        str(source.get("ground_capture_sha256")),
        source,
    )
    air = load_capture(
        args.air_capture,
        str(source.get("air_capture_sha256")),
        source,
    )
    ground_rows, ground_successor = route_rows(
        ground,
        "special_geometry_neutral_ground_start",
        "NEUTRAL_B_ATTACKING_AIR",
    )
    air_rows, air_successor = route_rows(
        air,
        "special_geometry_neutral_air_start",
        "NEUTRAL_B_FULL_CHARGE_AIR",
    )
    if ground_successor.get("action") != "STANDING" or air_successor.get("action") != "FALLING":
        fail("successor-actions")

    ground_anchor = {
        "position_x_from_origin": 0.0,
        "position_y": 0.0,
    }
    air_tail_anchor = air_rows[48]
    canonical = {
        "schema": 1,
        "cases": [
            {
                "id": "ground_complete",
                "samples": [
                    physics(row, ground_anchor, 1)
                    for row in [*ground_rows, ground_successor]
                ],
            },
            {
                "id": "air_complete_clock",
                "samples": [
                    state(row, -1) for row in [*air_rows, air_successor]
                ],
            },
            {
                "id": "air_physics_tail",
                "samples": [
                    physics(row, air_tail_anchor, -1)
                    for row in [*air_rows[49:], air_successor]
                ],
            },
        ],
    }
    canonical_bytes = json.dumps(
        canonical, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    digest = hashlib.sha256(canonical_bytes).hexdigest()
    expected_digest = stored.get("source_trace_sha256")
    if digest != expected_digest:
        fail(f"source-trace-sha256 expected={expected_digest} actual={digest}")
    print(
        "ssbm-falcon-punch-source=pass "
        f"ground_frames={len(ground_rows)} air_frames={len(air_rows)} "
        f"stored_cases=3 stored_samples=251 source_trace_sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
