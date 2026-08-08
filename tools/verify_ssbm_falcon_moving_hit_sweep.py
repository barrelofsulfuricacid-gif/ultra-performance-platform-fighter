#!/usr/bin/env python3
"""Qualify Falcon's previous-to-current hit-capsule sweep in Dolphin."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


EXPECTED_CAPTURE_SHA256 = (
    "d8599ecc80efc567d579d9c3df9c10c70f89909dc38358ad29d602ca6ed3f4ea"
)
EXPECTED_COLLISION_SOURCE_SHA256 = (
    "fa47d275f86956edb3c3a228a7fcc160e6f467c2d4bfd5f86d71f1d55e13e1fb"
)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def subtract(left: list[float], right: list[float]) -> list[float]:
    return [a - b for a, b in zip(left, right, strict=True)]


def dot(left: list[float], right: list[float]) -> float:
    return sum(a * b for a, b in zip(left, right, strict=True))


def point_on_segment(
    start: list[float], direction: list[float], parameter: float
) -> list[float]:
    return [
        value + parameter * delta
        for value, delta in zip(start, direction, strict=True)
    ]


def segment_distance(
    first_start: list[float],
    first_end: list[float],
    second_start: list[float],
    second_end: list[float],
) -> float:
    """Closest distance between two finite 3D segments."""

    first_direction = subtract(first_end, first_start)
    second_direction = subtract(second_end, second_start)
    start_delta = subtract(first_start, second_start)
    first_length = dot(first_direction, first_direction)
    second_length = dot(second_direction, second_direction)
    second_projection = dot(second_direction, start_delta)
    epsilon = 1.0e-12

    if first_length <= epsilon and second_length <= epsilon:
        return math.dist(first_start, second_start)
    if first_length <= epsilon:
        first_parameter = 0.0
        second_parameter = max(
            0.0, min(1.0, second_projection / second_length)
        )
    else:
        first_projection = dot(first_direction, start_delta)
        if second_length <= epsilon:
            second_parameter = 0.0
            first_parameter = max(
                0.0, min(1.0, -first_projection / first_length)
            )
        else:
            direction_dot = dot(first_direction, second_direction)
            denominator = (
                first_length * second_length - direction_dot * direction_dot
            )
            first_parameter = (
                max(
                    0.0,
                    min(
                        1.0,
                        (
                            direction_dot * second_projection
                            - first_projection * second_length
                        )
                        / denominator,
                    ),
                )
                if denominator > epsilon
                else 0.0
            )
            second_parameter = (
                direction_dot * first_parameter + second_projection
            ) / second_length
            if second_parameter < 0.0:
                second_parameter = 0.0
                first_parameter = max(
                    0.0, min(1.0, -first_projection / first_length)
                )
            elif second_parameter > 1.0:
                second_parameter = 1.0
                first_parameter = max(
                    0.0,
                    min(
                        1.0,
                        (direction_dot - first_projection) / first_length,
                    ),
                )
    return math.dist(
        point_on_segment(first_start, first_direction, first_parameter),
        point_on_segment(second_start, second_direction, second_parameter),
    )


def collision_margin(
    hitbox: dict[str, Any],
    hurtbox: dict[str, Any],
    moving: bool,
) -> float:
    current = [float(value) for value in hitbox["position"]]
    previous = (
        [float(value) for value in hitbox["previous_position"]]
        if moving
        else current
    )
    distance = segment_distance(
        previous,
        current,
        [float(value) for value in hurtbox["collision_position_a"]],
        [float(value) for value in hurtbox["collision_position_b"]],
    )
    return (
        float(hitbox["radius"]) + float(hurtbox["radius"]) - distance
    )


def continuing_hit_margins(
    attack_row: dict[str, Any],
    pre_hit_target_row: dict[str, Any],
) -> tuple[float, float] | None:
    hitboxes = [
        dict(hitbox)
        for hitbox in dict(attack_row.get("hitbox_memory", {})).get(
            "hitboxes", []
        )
        if int(hitbox.get("state", 0)) != 0
    ]
    hurtboxes = [
        dict(hurtbox)
        for hurtbox in dict(
            pre_hit_target_row.get("hitbox_memory", {})
        ).get("opponent_hurtboxes", [])
        if int(hurtbox.get("state", 1)) == 0
    ]
    if (
        not hitboxes
        or not hurtboxes
        or any(int(hitbox.get("state", 0)) != 3 for hitbox in hitboxes)
    ):
        return None
    return (
        max(
            collision_margin(hitbox, hurtbox, True)
            for hitbox in hitboxes
            for hurtbox in hurtboxes
        ),
        max(
            collision_margin(hitbox, hurtbox, False)
            for hitbox in hitboxes
            for hurtbox in hurtboxes
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("collision_source", type=Path)
    args = parser.parse_args()

    capture_digest = sha256(args.capture)
    if capture_digest != EXPECTED_CAPTURE_SHA256:
        raise SystemExit(
            f"unexpected moving-hit capture SHA-256: {capture_digest}"
        )
    source_digest = sha256(args.collision_source)
    if source_digest != EXPECTED_COLLISION_SOURCE_SHA256:
        raise SystemExit(f"unexpected lbcollision.c SHA-256: {source_digest}")
    capture: dict[str, Any] = json.loads(
        args.capture.read_text(encoding="utf-8")
    )
    rows: list[dict[str, Any]] = list(capture.get("rows", []))
    probe = dict(capture.get("hitbox_memory_probe", {}))
    disc = dict(capture.get("disc", {}))
    if (
        capture.get("schema") != 10
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("dolphin_version") != "3.5.1"
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != EXPECTED_DISC_SHA256
        or probe.get("decomp_revision") != EXPECTED_DECOMP_REVISION
        or len(rows) != 274
    ):
        raise SystemExit("unexpected moving-hit capture provenance")

    evaluation: dict[str, Any] | None = None
    previous_row: dict[str, Any] | None = None
    swept_margin = -math.inf
    current_margin = -math.inf
    for row_index, row in enumerate(rows):
        if (
            row_index == 0
            or row.get("action") != "DOWNTILT"
            or float(row.get("action_frame", -1.0)) != 12.0
        ):
            continue
        previous = rows[row_index - 1]
        if (
            previous.get("opponent_action") != "GRAB"
            or float(previous.get("opponent_action_frame", -1.0)) != 11.0
        ):
            continue
        margins = continuing_hit_margins(row, previous)
        if margins is None:
            continue
        swept_margin, current_margin = margins
        if swept_margin >= 0.0 and current_margin < 0.0:
            evaluation = row
            previous_row = previous
            break

    if evaluation is None or previous_row is None:
        raise SystemExit("moving-hit route never reached a swept collision")
    if (
        float(evaluation["action_frame"]) != 12.0
        or evaluation.get("opponent_action") != "DAMAGE_LOW_2"
        or swept_margin < 0.0
        or current_margin >= 0.0
        or float(previous_row["opponent_damage_percent"]) != 0.0
        or float(evaluation["opponent_damage_percent"]) != 12.0
    ):
        raise SystemExit(
            "moving-hit discriminator failed: "
            f"frame={evaluation['action_frame']} "
            f"swept_margin={swept_margin:.9f} "
            f"current_margin={current_margin:.9f} "
            f"damage={previous_row['opponent_damage_percent']}->"
            f"{evaluation['opponent_damage_percent']}"
        )

    miss_evaluation: dict[str, Any] | None = None
    miss_index = -1
    miss_swept_margin = math.inf
    miss_current_margin = math.inf
    for row_index, row in enumerate(rows):
        if (
            row_index == 0
            or row.get("action") != "DOWNTILT"
            or float(row.get("action_frame", -1.0)) != 12.0
            or row.get("opponent_action") != "GRAB"
            or float(row.get("opponent_action_frame", -1.0)) != 12.0
        ):
            continue
        margins = continuing_hit_margins(row, rows[row_index - 1])
        if margins is None:
            continue
        miss_swept_margin, miss_current_margin = margins
        miss_evaluation = row
        miss_index = row_index
        break

    miss_damage = (
        None
        if miss_evaluation is None
        else miss_evaluation["opponent_damage_percent"]
    )
    if (
        miss_evaluation is None
        or miss_swept_margin >= 0.0
        or miss_current_margin >= 0.0
        or float(miss_evaluation["opponent_damage_percent"]) != 12.0
        or any(
            float(row["opponent_damage_percent"]) != 12.0
            for row in rows[miss_index:]
        )
    ):
        raise SystemExit(
            "moving-hit miss control failed: "
            f"swept_margin={miss_swept_margin:.9f} "
            f"current_margin={miss_current_margin:.9f} "
            f"damage={miss_damage}"
        )

    print(
        "ssbm-moving-hit-sweep=pass "
        f"frames={len(rows)} action_frame=12 "
        f"swept_margin={swept_margin:.9f} "
        f"current_margin={current_margin:.9f} "
        f"miss_swept_margin={miss_swept_margin:.9f} "
        f"miss_current_margin={miss_current_margin:.9f} "
        f"capture_sha256={capture_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
