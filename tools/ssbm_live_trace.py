"""Shared primitives for live SSBM-to-production numeric qualification."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


MELEE_X_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0
MELEE_Y_TO_SIM_Q16 = 65536.0 * 11.0 / 62.0


def normalized_sha256(path: Path) -> str:
    """Hash source text independently of the host checkout's line endings."""

    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def canonical_sha256(value: object) -> str:
    encoded = json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=True,
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def parse_integer_observations(
    path: Path,
    prefix: str,
    group_field: str = "case",
) -> dict[str, list[dict[str, int]]]:
    """Parse allocation-free C runner key/value diagnostics by case."""

    groups: dict[str, list[dict[str, int]]] = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not line.startswith(prefix):
            continue
        fields = dict(
            token.split("=", 1) for token in line[len(prefix) :].split()
        )
        group = fields.pop(group_field)
        groups.setdefault(group, []).append(
            {key: int(value) for key, value in fields.items()}
        )
    return groups


def select_labeled_rows(
    capture: dict[str, Any],
    *,
    route: str,
    case_id: str,
    segment: str = "response",
    include_derived_labels: bool = False,
) -> list[dict[str, Any]]:
    """Select one manifest case without depending on capture frame numbers."""

    label = f"{route}_{case_id}_observe_{segment}"
    return [
        row
        for row in capture["rows"]
        if row.get("label") == label
        or (
            include_derived_labels
            and str(row.get("label", "")).startswith(f"{label}_")
        )
    ]


def require_equal(actual: object, expected: object, label: str) -> None:
    if actual != expected:
        raise SystemExit(f"{label}: {actual!r} != {expected!r}")


def require_q16_close(
    actual: int,
    expected: int,
    tolerance: int,
    label: str,
) -> None:
    if abs(actual - expected) > tolerance:
        raise SystemExit(
            f"{label}: {actual} != {expected} +/- {tolerance} Q16.16"
        )


def source_x_to_sim_q16(value: float) -> int:
    return round(value * MELEE_X_TO_SIM_Q16)


def source_y_to_sim_q16(value: float) -> int:
    """Convert a source-up displacement/vector to simulation-down Q16.16."""

    return round(-value * MELEE_Y_TO_SIM_Q16)


def validate_capture_provenance(
    capture: dict[str, Any],
    *,
    schema: int,
    stage: str,
    fighter: str,
    opponent: str,
    disc_sha256: str,
    oracle_artifact_sha256: str,
    case_count: int,
) -> None:
    if (
        capture.get("schema") != schema
        or capture.get("stage") != stage
        or capture.get("fighter") != fighter
        or capture.get("opponent") != opponent
        or capture.get("disc", {}).get("sha256") != disc_sha256
        or capture.get("oracle_execution", {}).get("release_artifact_sha256")
        != oracle_artifact_sha256
        or capture.get("checkpoint_pack", {}).get("case_count") != case_count
    ):
        raise SystemExit("live capture provenance mismatch")
