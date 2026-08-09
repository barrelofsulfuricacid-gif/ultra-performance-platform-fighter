#!/usr/bin/env python3
"""Merge independently captured checkpoint shards in manifest case order."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
from typing import Any


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("inputs", nargs="+", type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    case_order = [case["id"] for case in manifest["checkpoint_cases"]]
    captures: list[dict[str, Any]] = [
        json.loads(path.read_text(encoding="utf-8")) for path in args.inputs
    ]
    first = captures[0]
    invariant_keys = (
        "schema",
        "oracle",
        "fighter",
        "opponent",
        "stage",
        "disc",
        "dolphin_version",
        "libmelee_version",
        "stage_collision_memory",
        "surface_collision_memory_probe",
    )
    for capture in captures[1:]:
        for key in invariant_keys:
            if capture.get(key) != first.get(key):
                raise SystemExit(f"checkpoint shard provenance mismatch: {key}")
        if capture.get("oracle_execution") != first.get("oracle_execution"):
            raise SystemExit("checkpoint shard oracle execution mismatch")

    rows_by_case: dict[str, list[dict[str, Any]]] = {}
    for capture in captures:
        for label in capture.get("checkpoint_pack", {}).get(
            "case_start_labels", []
        ):
            case_id = str(label).removeprefix("floor_response_").removesuffix(
                "_setup"
            )
            if case_id in rows_by_case:
                raise SystemExit(f"duplicate checkpoint shard case: {case_id}")
            prefix = f"floor_response_{case_id}_observe_"
            rows_by_case[case_id] = [
                row for row in capture["rows"] if row["label"].startswith(prefix)
            ]
            if not rows_by_case[case_id]:
                raise SystemExit(f"empty checkpoint shard case: {case_id}")

    if set(rows_by_case) != set(case_order):
        missing = sorted(set(case_order) - set(rows_by_case))
        extra = sorted(set(rows_by_case) - set(case_order))
        raise SystemExit(f"checkpoint shard coverage mismatch: missing={missing} extra={extra}")

    merged = copy.deepcopy(first)
    merged["rows"] = [row for case_id in case_order for row in rows_by_case[case_id]]
    pack = merged["checkpoint_pack"]
    pack["case_count"] = len(case_order)
    pack["case_start_labels"] = [
        f"floor_response_{case_id}_setup" for case_id in case_order
    ]
    pack["coverage_manifest"] = manifest
    merged["parallel_capture"] = {
        "schema": 1,
        "shards": len(captures),
        "case_order": case_order,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(merged, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        f"ssbm-checkpoint-merge=pass shards={len(captures)} "
        f"cases={len(case_order)} rows={len(merged['rows'])} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
