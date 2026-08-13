#!/usr/bin/env python3
"""Verify Falcon's five natural Battlefield aerial-landing routes."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from ssbm_live_trace import canonical_sha256
from ssbm_natural_movement_domain import (
    NaturalMovementDomainError,
    canonical_capture,
    load_capture,
)


def fail(message: str) -> None:
    raise SystemExit(
        f"ssbm-falcon-aerial-attack-landing=fail reason={message}"
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
        or coverage.get("domain") != "falcon-aerial-attack-landing"
        or not isinstance(coverage.get("live_source"), dict)
        or coverage.get("stored_oracle", {}).get("kind")
        != "native-csv-trace-v1"
        or len(coverage.get("stored_oracle", {}).get("cases", [])) != 5
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
        "ssbm-falcon-aerial-attack-landing=pass "
        f"rows={len(capture['rows'])} stored_cases=5 "
        f"source_trace_sha256={observed_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
