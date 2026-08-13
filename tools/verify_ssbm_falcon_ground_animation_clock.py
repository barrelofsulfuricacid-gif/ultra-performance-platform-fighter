#!/usr/bin/env python3
"""Replay Falcon gait inputs and compare production clocks to Dolphin."""

from __future__ import annotations

import argparse
import csv
import io
import json
from pathlib import Path
import subprocess
from typing import Any

from ssbm_live_trace import canonical_sha256, require_equal, require_f32_close


def best_alignment(
    actual: list[dict[str, str]], expected: list[dict[str, float]]
) -> int:
    if len(actual) < len(expected):
        raise SystemExit(
            f"native trace has {len(actual)} samples for {len(expected)} oracle rows"
        )
    return min(
        range(len(actual) - len(expected) + 1),
        key=lambda start: sum(
            abs(
                float(actual[start + index]["source_animation_frame_f32"])
                - sample["frame_f32"]
            )
            for index, sample in enumerate(expected)
        ),
    )


def replay_case(
    runner: Path,
    case: dict[str, Any],
    tolerances: dict[str, float],
) -> int:
    expected = case["samples"]
    input_line = f"{case['input_x']},0,0,0,0,0,0,0\n"
    replay_ticks = len(expected) + 48
    completed = subprocess.run(
        [str(runner)],
        input=input_line * replay_ticks,
        text=True,
        encoding="utf-8",
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        raise SystemExit(
            f"{case['id']}: native runner failed ({completed.returncode}): "
            f"{completed.stderr.strip()}"
        )
    rows = list(csv.DictReader(io.StringIO(completed.stdout)))
    candidates = [
        row
        for row in rows
        if int(row["action_state"]) == case["action_state"]
        and int(row["source_submotion"]) == case["source_submotion"]
    ]
    start = best_alignment(candidates, expected)
    for sample_index, oracle in enumerate(expected):
        native = candidates[start + sample_index]
        prefix = f"{case['id']}[{sample_index}]"
        require_f32_close(
            float(native["source_animation_frame_f32"]),
            oracle["frame_f32"],
            tolerances["frame"],
            f"{prefix} source frame",
        )
        require_f32_close(
            float(native["source_animation_rate_f32"]),
            oracle["rate_f32"],
            tolerances["rate"],
            f"{prefix} source rate",
        )
        require_f32_close(
            float(native["velocity_x_f32"]),
            oracle["velocity_x_f32"],
            tolerances["velocity_x"],
            f"{prefix} ground velocity",
        )
    return start


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("runner", type=Path)
    args = parser.parse_args()

    oracle = json.loads(args.oracle.read_text(encoding="utf-8"))
    require_equal(oracle.get("schema"), 2, "stored oracle schema")
    require_equal(
        oracle.get("semantic_sha256"),
        canonical_sha256(
            {"domain": oracle.get("domain"), "cases": oracle.get("cases")}
        ),
        "stored oracle semantic digest",
    )
    tolerances = oracle.get("float32_tolerances")
    cases = oracle.get("cases")
    if not isinstance(tolerances, dict) or not isinstance(cases, list):
        raise SystemExit("invalid stored gait-clock oracle")

    alignments = [
        replay_case(args.runner, case, tolerances) for case in cases
    ]
    print(
        "ssbm-falcon-ground-animation-clock=pass "
        f"cases={len(cases)} samples={sum(len(case['samples']) for case in cases)} "
        f"native_alignments={','.join(str(value) for value in alignments)} "
        f"semantic_sha256={oracle['semantic_sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
