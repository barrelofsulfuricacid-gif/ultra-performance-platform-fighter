#!/usr/bin/env python3
"""Verify Falcon's Ottotto/OttottoWait special-acquisition table."""

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
    raise SystemExit(f"ssbm-falcon-teeter-special=fail reason={message}")


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    prefix = f"special_acquisition_{case_id}_"
    return [row for row in rows if str(row.get("label", "")).startswith(prefix)]


def qualify_capture(capture: dict[str, Any], coverage: dict[str, Any]) -> None:
    checkpoint = capture.get("checkpoint_pack")
    expected_labels = [case["start_label"] for case in coverage["checkpoint_cases"]]
    if (
        not isinstance(checkpoint, dict)
        or checkpoint.get("protocol")
        != "immutable-multislot-slippi-state-file-control-v2"
        or checkpoint.get("slot_count") != 4
        or checkpoint.get("case_count") != 4
        or checkpoint.get("case_start_labels") != expected_labels
    ):
        fail("checkpoint-isolation")

    acquired_actions = {
        "teeter_neutral": "NEUTRAL_B_ATTACKING_AIR",
        "teeter_side": "SWORD_DANCE_1",
        "teeter_up": "SWORD_DANCE_3_MID",
        "teeter_down": "SWORD_DANCE_4_LOW",
    }
    for case_id, acquired_action in acquired_actions.items():
        current = case_rows(capture["rows"], case_id)
        if (
            len(current) != 7
            or current[0].get("action") != "EDGE_TEETERING_START"
            or round(float(current[0]["action_frame"])) != 1
            or current[0].get("facing") != 1.0
            or not bool(current[0].get("grounded"))
        ):
            fail(f"teeter-source case={case_id}")
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
            fail(f"teeter-result case={case_id}")


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
        != "falcon-common-teeter-special-acquisition"
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
        "ssbm-falcon-teeter-special=pass "
        f"rows={len(capture['rows'])} stored_cases=4 "
        f"source_trace_sha256={observed_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
