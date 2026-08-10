#!/usr/bin/env python3
"""Verify Falcon's natural Battlefield jump/landing stored projection."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from compare_ssbm_movement import (
    expected_action_state,
    expected_action_ticks,
    native_input_line,
)
from generate_ssbm_stored_trace_oracle import (
    expand_case_samples,
    native_csv_input_line,
)
from ssbm_live_trace import (
    canonical_sha256,
    common_movement_source_sample,
    selected_trace_fields,
)


def fail(message: str) -> None:
    raise SystemExit(f"ssbm-falcon-airborne-landing=fail reason={message}")


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
    execution = capture.get("oracle_execution")
    disc = capture.get("disc")
    rows = capture.get("rows")
    if (
        capture.get("schema") != 11
        or capture.get("oracle")
        != "SSBM GALE01 NTSC-U revision 2 via Dolphin/Slippi"
        or capture.get("stage") != "BATTLEFIELD"
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "FOX"
        or capture.get("dolphin_version") != live_source.get("dolphin_version")
        or capture.get("libmelee_version") != live_source.get("libmelee_version")
        or not isinstance(execution, dict)
        or execution.get("mode") != "exiai-headless-null-fast-forward"
        or execution.get("release_artifact_sha256")
        != live_source.get("release_artifact_sha256")
        or execution.get("launcher_sha256")
        != live_source.get("launcher_sha256")
        or not isinstance(disc, dict)
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != live_source.get("disc_sha256")
        or not isinstance(rows, list)
        or len(rows) != live_source.get("row_count")
    ):
        fail(f"capture-provenance path={path}")
    if any(row.get("damage_percent") != 0.0 for row in rows):
        fail(f"capture-damage-contamination path={path}")
    return capture


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


def canonical_capture(
    capture: dict[str, Any],
    coverage: dict[str, Any],
) -> dict[str, Any]:
    rows = capture["rows"]
    stored = coverage["stored_oracle"]
    cases = stored.get("cases")
    fields = stored.get("serialized_fields")
    if (
        not isinstance(cases, list)
        or len(cases) != 1
        or not isinstance(cases[0], dict)
        or not isinstance(fields, list)
    ):
        fail("stored-case-schema")
    case = cases[0]
    case_id = str(case.get("id"))
    if len(rows) != case.get("sample_count"):
        fail(f"sample-count case={case_id}")
    inputs = expand_case_samples(case, case_id, len(rows), "inputs")
    stored_input_lines = [
        native_csv_input_line(sample, case_id, sample_index)
        for sample_index, sample in enumerate(inputs)
    ]
    live_input_lines = [native_input_line(row) for row in rows]
    if stored_input_lines != live_input_lines:
        mismatch = next(
            index
            for index, (stored_line, live_line) in enumerate(
                zip(stored_input_lines, live_input_lines, strict=True)
            )
            if stored_line != live_line
        )
        fail(f"input case={case_id} sample={mismatch}")

    exclusions = case.get("field_exclusions", {})
    if not isinstance(exclusions, dict):
        fail(f"field-exclusions case={case_id}")
    origin_y = float(rows[0]["position_y"])
    samples: list[dict[str, int]] = []
    for sample_index, row in enumerate(rows):
        action_state = expected_action_state(
            str(row["action"]),
            float(row["action_frame"]),
        )
        action_ticks = expected_action_ticks(
            str(row["action"]),
            float(row["action_frame"]),
        )
        selected_fields = selected_trace_fields(
            fields,
            exclusions,
            sample_index,
        )
        if not isinstance(action_state, int):
            fail(f"action-state sample={sample_index}")
        if (action_ticks is not None) != ("action_ticks" in selected_fields):
            fail(f"action-tick-mask sample={sample_index}")
        values = common_movement_source_sample(
            row,
            action_state=action_state,
            action_ticks=action_ticks if action_ticks is not None else 0,
            origin_y=origin_y,
        )
        samples.append({field: values[field] for field in selected_fields})
    return {
        "schema": 1,
        "domain": str(coverage["domain"]),
        "cases": [{"id": case_id, "samples": samples}],
    }


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

    canonical = canonical_capture(capture, coverage)
    repeat_canonical = canonical_capture(repeat_capture, coverage)
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
