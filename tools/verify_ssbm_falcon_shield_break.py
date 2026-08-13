#!/usr/bin/env python3
"""Verify Falcon's passive shield-break route against two live captures."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
from typing import Any

from ssbm_live_trace import canonical_sha256, normalized_sha256


SEMANTIC_FIELDS = (
    "trace_frame",
    "label",
    "action",
    "action_frame",
    "position_x_from_origin",
    "position_y",
    "air_velocity_x",
    "ground_velocity_x",
    "velocity_y",
    "shield_health",
    "grounded",
    "facing",
    "invulnerable",
    "requested_left_shoulder",
    "requested_digital_left",
    "observed_main_x",
    "observed_main_y",
    "observed_analog_shoulder",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"ssbm-falcon-shield-break=fail reason={message}")


def semantic_rows(capture: dict[str, Any]) -> list[dict[str, Any]]:
    rows = capture.get("rows")
    require(isinstance(rows, list), "capture-rows")
    result: list[dict[str, Any]] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        sample = {field: row.get(field) for field in SEMANTIC_FIELDS}
        # The persistent Dolphin worker can begin at any phase of Wait's
        # looping animation. That phase cannot affect the later Guard route.
        if sample["action"] == "STANDING":
            sample["action_frame"] = 0.0
        result.append(sample)
    return result


def validate_provenance(
    capture: dict[str, Any],
    live_source: dict[str, Any],
) -> None:
    require(capture.get("schema") == 11, "capture-schema")
    require(capture.get("fighter") == "CPTFALCON", "capture-fighter")
    require(capture.get("stage") == "FINAL_DESTINATION", "capture-stage")
    require(
        capture.get("shield_break_orientation_route") is True,
        "capture-route",
    )
    require(
        capture.get("disc", {}).get("sha256") == live_source["disc_sha256"],
        "disc-identity",
    )
    execution = capture.get("oracle_execution", {})
    require(
        execution.get("release_artifact_sha256")
        == live_source["release_artifact_sha256"],
        "oracle-artifact",
    )
    require(
        execution.get("launcher_sha256") == live_source["launcher_sha256"],
        "oracle-launcher",
    )
    require(
        capture.get("surface_collision_memory_probe", {}).get("engine_version")
        == live_source["memory_engine_version"],
        "surface-probe-engine",
    )


def replay_compare(
    compare_script: Path,
    capture: Path,
    runner: Path,
    position_tolerance_f32: float,
    velocity_tolerance_f32: float,
    shield_health_tolerance_f32: float,
) -> None:
    completed = subprocess.run(
        [
            sys.executable,
            str(compare_script),
            str(capture),
            str(runner),
            "--position-tolerance-f32",
            str(position_tolerance_f32),
            "--velocity-tolerance-f32",
            str(velocity_tolerance_f32),
            "--shield-health-tolerance-f32",
            str(shield_health_tolerance_f32),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise SystemExit(completed.returncode)
    require("ssbm-movement-compare=pass frames=500" in completed.stdout, "native-replay")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coverage", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("repeat_capture", type=Path)
    parser.add_argument("runner", type=Path)
    args = parser.parse_args()

    coverage = json.loads(args.coverage.read_text(encoding="utf-8"))
    require(
        coverage.get("schema") == 1
        and coverage.get("domain") == "falcon-common-shield-break",
        "coverage-schema",
    )
    live_source = coverage.get("live_source")
    require(isinstance(live_source, dict), "coverage-live-source")
    require(
        normalized_sha256(args.capture) == live_source["capture_sha256"],
        "capture-digest",
    )
    require(
        normalized_sha256(args.repeat_capture)
        == live_source["repeat_capture_sha256"],
        "repeat-capture-digest",
    )

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    repeat = json.loads(args.repeat_capture.read_text(encoding="utf-8"))
    validate_provenance(capture, live_source)
    validate_provenance(repeat, live_source)
    rows = semantic_rows(capture)
    repeat_rows = semantic_rows(repeat)
    require(len(rows) == live_source["row_count"], "row-count")
    require(rows == repeat_rows, "repeat-semantic-track")
    semantic_digest = canonical_sha256(rows)
    require(
        semantic_digest == live_source["semantic_trace_sha256"],
        "semantic-trace-digest",
    )

    policy = coverage.get("comparison_policy")
    require(isinstance(policy, dict), "comparison-policy")
    compare_script = Path(__file__).with_name("compare_ssbm_movement.py")
    replay_compare(
        compare_script,
        args.capture,
        args.runner,
        float(policy["position_tolerance_f32"]),
        float(policy["velocity_tolerance_f32"]),
        float(policy["shield_health_tolerance_f32"]),
    )
    replay_compare(
        compare_script,
        args.repeat_capture,
        args.runner,
        float(policy["position_tolerance_f32"]),
        float(policy["velocity_tolerance_f32"]),
        float(policy["shield_health_tolerance_f32"]),
    )
    print(
        "ssbm-falcon-shield-break=pass "
        f"frames={len(rows)} repeats=2 semantic_sha256={semantic_digest} "
        f"position_tolerance_f32={policy['position_tolerance_f32']} "
        f"velocity_tolerance_f32={policy['velocity_tolerance_f32']} "
        f"shield_health_tolerance_f32={policy['shield_health_tolerance_f32']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
