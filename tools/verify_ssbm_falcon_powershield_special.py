#!/usr/bin/env python3
"""Verify Falcon's powershield-only GuardOff special dispatcher."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from ssbm_live_trace import canonical_sha256
from ssbm_natural_movement_domain import (
    NaturalMovementDomainError,
    canonical_capture,
    load_capture,
)


def fail(message: str) -> None:
    raise SystemExit(f"ssbm-falcon-powershield-special=fail reason={message}")


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    prefix = f"special_acquisition_{case_id}_"
    return [row for row in rows if str(row.get("label", "")).startswith(prefix)]


def qualify_capture(capture: dict[str, Any], coverage: dict[str, Any]) -> None:
    checkpoint = capture.get("checkpoint_pack")
    expected_labels = [case["start_label"] for case in coverage["checkpoint_cases"]]
    if (
        capture.get("opponent") != "CPTFALCON"
        or not isinstance(checkpoint, dict)
        or checkpoint.get("protocol")
        != "immutable-multislot-slippi-state-file-control-v2"
        or checkpoint.get("slot_count") != 5
        or checkpoint.get("case_count") != 5
        or checkpoint.get("case_start_labels") != expected_labels
    ):
        fail("checkpoint-isolation")

    acquired_actions = {
        "powershield_neutral": "NEUTRAL_B_ATTACKING_AIR",
        "powershield_side": "SWORD_DANCE_1",
        "powershield_up": "SWORD_DANCE_3_MID",
        "powershield_down": "SWORD_DANCE_4_LOW",
    }
    powershield_health: list[float] = []
    for case_id, acquired_action in acquired_actions.items():
        current = case_rows(capture["rows"], case_id)
        if (
            len(current) != 7
            or current[0].get("action") != "SHIELD_RELEASE"
            or not 1 <= round(float(current[0]["action_frame"])) <= 5
            or current[0].get("opponent_action") != "NEUTRAL_ATTACK_1"
            or current[0].get("facing") != 1.0
            or not bool(current[0].get("grounded"))
        ):
            fail(f"powershield-source case={case_id}")
        actual_frames = [
            round(float(row["action_frame"]))
            for row in current[1:]
            if row.get("action") == acquired_action
        ]
        if actual_frames != list(range(1, 7)):
            fail(
                f"action-span case={case_id} action={acquired_action} "
                f"actual={actual_frames}"
            )
        if any(
            row.get("facing") != 1.0 or not bool(row.get("grounded"))
            for row in current[1:]
        ):
            fail(f"powershield-result case={case_id}")
        powershield_health.append(float(current[0]["shield_health"]))

    ordinary = case_rows(capture["rows"], "ordinary_neutral")
    if (
        len(ordinary) != 7
        or [row.get("action") for row in ordinary] != ["SHIELD_RELEASE"] * 7
        or [round(float(row["action_frame"])) for row in ordinary]
        != list(range(1, 8))
        or ordinary[0].get("opponent_action") != "NEUTRAL_ATTACK_1"
        or any(
            row.get("facing") != 1.0 or not bool(row.get("grounded"))
            for row in ordinary
        )
    ):
        fail("ordinary-release-control")
    if (
        any(
            health - float(ordinary[0]["shield_health"]) <= 1.0
            for health in powershield_health
        )
    ):
        fail("physical-powershield-discriminator")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coverage", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("repeat_capture", type=Path)
    args = parser.parse_args()

    coverage = json.loads(args.coverage.read_text(encoding="utf-8"))
    if (
        coverage.get("schema") != 1
        or coverage.get("domain")
        != "falcon-common-powershield-special-acquisition"
        or not isinstance(coverage.get("live_source"), dict)
        or coverage.get("stored_oracle", {}).get("kind")
        != "native-csv-trace-v1"
    ):
        fail("coverage-schema")
    live_source = coverage["live_source"]
    try:
        capture = load_capture(
            args.capture,
            str(live_source["capture_sha256"]),
            live_source,
        )
        repeat_capture = load_capture(
            args.repeat_capture,
            str(live_source["repeat_capture_sha256"]),
            live_source,
        )
        canonical = canonical_capture(capture, coverage)
        repeat_canonical = canonical_capture(repeat_capture, coverage)
    except NaturalMovementDomainError as error:
        fail(str(error))

    qualify_capture(capture, coverage)
    qualify_capture(repeat_capture, coverage)
    if canonical != repeat_canonical:
        fail("repeat-semantic-trace")
    observed_digest = canonical_sha256(canonical)
    expected_digest = coverage["stored_oracle"].get("source_trace_sha256")
    if observed_digest != expected_digest:
        fail(
            f"source-trace-sha256 expected={expected_digest} "
            f"actual={observed_digest}"
        )
    print(
        "ssbm-falcon-powershield-special=pass "
        f"rows={len(capture['rows'])} stored_cases=5 "
        f"source_trace_sha256={observed_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
