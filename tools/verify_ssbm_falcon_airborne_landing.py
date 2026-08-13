#!/usr/bin/env python3
"""Verify Falcon's natural Battlefield jump/landing stored projection."""

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
    raise SystemExit(f"ssbm-falcon-airborne-landing=fail reason={message}")


def require_action_span(
    rows: list[dict[str, Any]],
    label_prefix: str,
    action: str,
    expected_frames: list[int],
) -> None:
    actual = [
        round(float(row["action_frame"]))
        for row in rows
        if str(row.get("label", "")).startswith(label_prefix)
        and row.get("action") == action
    ]
    if actual != expected_frames:
        fail(
            f"action-span route={label_prefix} action={action} "
            f"expected={expected_frames} actual={actual}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coverage", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("repeat_capture", type=Path)
    args = parser.parse_args()

    coverage = json.loads(args.coverage.read_text(encoding="utf-8"))
    if (
        coverage.get("schema") != 1
        or coverage.get("domain") != "falcon-common-airborne-landing"
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
    except NaturalMovementDomainError as error:
        fail(str(error))
    for current in (capture, repeat_capture):
        rows = current["rows"]
        require_action_span(
            rows,
            "airborne_landing_jump_backward",
            "JUMPING_BACKWARD",
            list(range(1, 32)),
        )
        require_action_span(
            rows,
            "airborne_landing_jump_aerial_forward",
            "JUMPING_ARIAL_FORWARD",
            list(range(1, 46)),
        )
        require_action_span(
            rows,
            "airborne_landing_jump_aerial_backward",
            "JUMPING_ARIAL_BACKWARD",
            list(range(1, 36)),
        )
        require_action_span(
            rows,
            "airborne_landing_jump_aerial_backward",
            "FALLING_AERIAL",
            list(range(1, 9)) * 2 + list(range(1, 5)),
        )

    try:
        canonical = canonical_capture(capture, coverage)
        repeat_canonical = canonical_capture(repeat_capture, coverage)
    except NaturalMovementDomainError as error:
        fail(str(error))
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
        "ssbm-falcon-airborne-landing=pass "
        f"rows={len(capture['rows'])} stored_cases=1 "
        f"source_trace_sha256={observed_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
