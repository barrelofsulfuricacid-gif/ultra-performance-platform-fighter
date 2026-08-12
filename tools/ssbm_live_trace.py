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


def selected_trace_fields(
    fields: list[str],
    field_exclusions: dict[str, list[list[int]]],
    sample_index: int,
) -> list[str]:
    """Return the fields live-qualified for one stored numeric sample."""

    return [
        field
        for field in fields
        if not any(
            start <= sample_index < end
            for start, end in field_exclusions.get(field, [])
        )
    ]


def parse_integer_observations(
    path: Path,
    prefix: str,
    group_field: str = "case",
) -> dict[str, list[dict[str, int]]]:
    """Parse allocation-free C runner key/value diagnostics by case."""

    return parse_integer_observations_text(
        path.read_text(encoding="utf-8-sig"), prefix, group_field
    )


def parse_integer_observations_text(
    text: str,
    prefix: str,
    group_field: str = "case",
) -> dict[str, list[dict[str, int]]]:
    """Parse allocation-free C runner diagnostics already held in memory."""

    groups: dict[str, list[dict[str, int]]] = {}
    for line in text.lstrip("\ufeff").splitlines():
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


def common_movement_source_sample(
    row: dict[str, Any],
    *,
    action_state: int,
    action_ticks: int,
    origin_x: float = 0.0,
    origin_y: float = 0.0,
) -> dict[str, int]:
    """Project one ordinary movement row into the native CSV field contract."""

    grounded = int(bool(row["grounded"]))
    support = 0
    normal_x_q16 = 0
    normal_y_q16 = 0
    if grounded != 0:
        collision = row.get("surface_collision_memory")
        surfaces = collision.get("surfaces") if isinstance(collision, dict) else None
        floor = surfaces.get("floor") if isinstance(surfaces, dict) else None
        if isinstance(floor, dict):
            line_index = floor.get("index")
            normal = floor.get("normal")
            if (
                isinstance(line_index, int)
                and not isinstance(line_index, bool)
                and 0 <= line_index < 0xFFFFFFFF
            ):
                support = line_index + 1
            if isinstance(normal, list) and len(normal) >= 2:
                normal_x_q16 = round(float(normal[0]) * 65536.0)
                normal_y_q16 = round(float(normal[1]) * 65536.0)
    velocity_x_key = "ground_velocity_x" if grounded != 0 else "air_velocity_x"
    sample = {
        "action_state": action_state,
        "action_ticks": action_ticks,
        "facing": int(row["facing"]),
        "grounded": grounded,
        "support": support,
        "surface_normal_source_x_q16": normal_x_q16,
        "surface_normal_source_y_q16": normal_y_q16,
        "position_x_q16_from_origin": source_x_to_sim_q16(
            float(row["position_x_from_origin"]) - origin_x
        ),
        "position_y_q16_from_origin": source_y_to_sim_q16(
            float(row["position_y"]) - origin_y
        ),
        "velocity_x_q16": source_x_to_sim_q16(float(row[velocity_x_key])),
        "velocity_y_q16": source_y_to_sim_q16(float(row["velocity_y"])),
    }
    input_memory = row.get("input_memory")
    if isinstance(input_memory, dict):
        tilt_x_age = input_memory.get("tilt_x_age")
        if (
            isinstance(tilt_x_age, int)
            and not isinstance(tilt_x_age, bool)
            and 0 <= tilt_x_age <= 254
        ):
            sample["tilt_x_age"] = tilt_x_age
    return sample


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
