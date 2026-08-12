#!/usr/bin/env python3
"""Verify Falcon's common-state special acquisition callback masks."""

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
    raise SystemExit(f"ssbm-falcon-special-acquisition=fail reason={message}")


def case_rows(
    rows: list[dict[str, Any]],
    case_id: str,
) -> list[dict[str, Any]]:
    prefix = f"special_acquisition_{case_id}_"
    return [row for row in rows if str(row.get("label", "")).startswith(prefix)]


def require_action_frames(
    rows: list[dict[str, Any]],
    action: str,
    frames: list[int],
    context: str,
) -> None:
    actual = [
        round(float(row["action_frame"]))
        for row in rows
        if row.get("action") == action
    ]
    if actual != frames:
        fail(
            f"action-span context={context} action={action} "
            f"expected={frames} actual={actual}"
        )


def require_ordered_actions(
    rows: list[dict[str, Any]],
    expected: list[tuple[str, int | None]],
    context: str,
) -> None:
    actual = [
        (str(row.get("action")), round(float(row.get("action_frame", 0.0))))
        for row in rows
    ]
    if len(actual) != len(expected) or any(
        action != expected_action
        or (expected_frame is not None and frame != expected_frame)
        for (action, frame), (expected_action, expected_frame) in zip(
            actual, expected, strict=True
        )
    ):
        fail(
            f"ordered-actions context={context} "
            f"expected={expected} actual={actual}"
        )


def requested_main_x_q15(row: dict[str, Any]) -> int:
    return round((float(row["requested_main_x"]) - 0.5) * 65534.0)


def require_turn_input_history(
    rows: list[dict[str, Any]],
    *,
    requested_x: list[int],
    raw_x: list[int],
    label_suffixes: list[str],
    context: str,
) -> None:
    actual_requested = [requested_main_x_q15(row) for row in rows]
    actual_raw = [row.get("observed_raw_main_x") for row in rows]
    actual_raw_y = [row.get("observed_raw_main_y") for row in rows]
    actual_labels = [str(row.get("label", "")) for row in rows]
    expected_labels = [
        f"special_acquisition_{context}_{suffix}" for suffix in label_suffixes
    ]
    if actual_requested != requested_x:
        fail(
            f"requested-main-x context={context} "
            f"expected={requested_x} actual={actual_requested}"
        )
    if actual_raw != raw_x or actual_raw_y != [0] * len(rows):
        fail(
            f"raw-main context={context} expected_x={raw_x} "
            f"actual_x={actual_raw} actual_y={actual_raw_y}"
        )
    if actual_labels != expected_labels:
        fail(
            f"labels context={context} "
            f"expected={expected_labels} actual={actual_labels}"
        )
    if any(row.get("grounded") is not True for row in rows):
        fail(f"grounded context={context}")


def require_tilt_x_ages(
    rows: list[dict[str, Any]],
    expected: list[int],
    context: str,
) -> None:
    actual: list[int] = []
    for row in rows:
        input_memory = row.get("input_memory")
        if not isinstance(input_memory, dict):
            fail(f"input-memory context={context}")
        age = input_memory.get("tilt_x_age")
        if not isinstance(age, int) or isinstance(age, bool):
            fail(f"tilt-x-age-type context={context} actual={age!r}")
        actual.append(age)
    if actual != expected:
        fail(
            f"tilt-x-age context={context} "
            f"expected={expected} actual={actual}"
        )


def qualify_capture(
    capture: dict[str, Any],
    coverage: dict[str, Any],
) -> None:
    rows = capture["rows"]
    checkpoint = capture.get("checkpoint_pack")
    expected_labels = [
        case["start_label"] for case in coverage["checkpoint_cases"]
    ]
    expected_count = len(expected_labels)
    expected_slots = coverage["checkpoint_pack"]["capture_plan"][
        "checkpoint_slot_count"
    ]
    if (
        not isinstance(checkpoint, dict)
        or checkpoint.get("protocol")
        != "immutable-multislot-slippi-state-file-control-v2"
        or checkpoint.get("slot_count") != expected_slots
        or checkpoint.get("case_count") != expected_count
        or checkpoint.get("case_start_labels") != expected_labels
    ):
        fail("checkpoint-isolation")

    neutral = case_rows(rows, "turn_neutral")
    if len(neutral) != 7:
        fail(f"row-count case=turn_neutral actual={len(neutral)}")
    require_action_frames(neutral, "TURNING", list(range(1, 8)), "turn_neutral")

    turn_specials = {
        "turn_side": "SWORD_DANCE_1",
        "turn_up": "SWORD_DANCE_3_MID",
        "turn_down": "SWORD_DANCE_4_LOW",
    }
    for case_id, acquired_action in turn_specials.items():
        current = case_rows(rows, case_id)
        if (
            len(current) != 7
            or current[0].get("action") != "TURNING"
            or round(float(current[0]["action_frame"])) != 1
            or current[0].get("facing") != 1.0
        ):
            fail(f"turn-source case={case_id}")
        require_action_frames(
            current[1:],
            acquired_action,
            list(range(1, 7)),
            case_id,
        )
        if any(row.get("facing") != -1.0 for row in current[1:]):
            fail(f"turn-facing case={case_id}")

    ucf_positive = case_rows(rows, "turn_ucf_raw_delta_76")
    if len(ucf_positive) != 4:
        fail(
            "row-count case=turn_ucf_raw_delta_76 "
            f"actual={len(ucf_positive)}"
        )
    require_ordered_actions(
        ucf_positive,
        [("STANDING", None), ("TURNING", 1), ("DASHING", 1), ("DASHING", 2)],
        "turn_ucf_raw_delta_76",
    )
    if [row.get("facing") for row in ucf_positive] != [1.0, 1.0, -1.0, -1.0]:
        fail("turn-facing case=turn_ucf_raw_delta_76")
    require_tilt_x_ages(
        ucf_positive,
        [254, 0, 254, 254],
        "turn_ucf_raw_delta_76",
    )
    require_turn_input_history(
        ucf_positive,
        requested_x=[0, -16384, -31129, 0],
        raw_x=[0, -40, -76, 0],
        label_suffixes=["setup", "setup", "edge", "observe"],
        context="turn_ucf_raw_delta_76",
    )
    if ucf_positive[2]["observed_raw_main_x"] - ucf_positive[0]["observed_raw_main_x"] != -76:
        fail("ucf-raw-delta case=turn_ucf_raw_delta_76")

    delayed = case_rows(rows, "turn_delayed_pending_dash")
    if len(delayed) != 9:
        fail(f"row-count case=turn_delayed_pending_dash actual={len(delayed)}")
    require_ordered_actions(
        delayed,
        [("TURNING", frame) for frame in range(1, 8)]
        + [("DASHING", 1), ("DASHING", 2)],
        "turn_delayed_pending_dash",
    )
    if [row.get("facing") for row in delayed] != [1.0] * 7 + [-1.0] * 2:
        fail("turn-facing case=turn_delayed_pending_dash")
    require_tilt_x_ages(
        delayed,
        [0, 254, 0, 1, 2, 3, 4, 254, 254],
        "turn_delayed_pending_dash",
    )
    require_turn_input_history(
        delayed,
        requested_x=[-16384, 0] + [-32767] * 6 + [0],
        raw_x=[-40, 0] + [-80] * 6 + [0],
        label_suffixes=["setup"] * 7 + ["edge", "observe"],
        context="turn_delayed_pending_dash",
    )

    ucf_negative = case_rows(rows, "turn_ucf_raw_delta_75_release")
    if len(ucf_negative) != 9:
        fail(
            "row-count case=turn_ucf_raw_delta_75_release "
            f"actual={len(ucf_negative)}"
        )
    require_ordered_actions(
        ucf_negative,
        [("STANDING", None)] + [("TURNING", frame) for frame in range(1, 9)],
        "turn_ucf_raw_delta_75_release",
    )
    if [row.get("facing") for row in ucf_negative] != [1.0] * 8 + [-1.0]:
        fail("turn-facing case=turn_ucf_raw_delta_75_release")
    require_tilt_x_ages(
        ucf_negative,
        [254, 0, 1, 254, 254, 254, 254, 254, 254],
        "turn_ucf_raw_delta_75_release",
    )
    require_turn_input_history(
        ucf_negative,
        requested_x=[0, -16384, -30719] + [0] * 6,
        raw_x=[0, -40, -75] + [0] * 6,
        label_suffixes=["setup", "setup", "edge"] + ["observe"] * 6,
        context="turn_ucf_raw_delta_75_release",
    )
    if ucf_negative[2]["observed_raw_main_x"] - ucf_negative[0]["observed_raw_main_x"] != -75:
        fail("ucf-raw-delta case=turn_ucf_raw_delta_75_release")

    crouch_cases = {
        "squat_wait_down": (26, "CROUCHING"),
        "squat_wait_diagonal_down": (26, "CROUCHING"),
        "squat_rv_down": (27, "CROUCH_END"),
        "squat_rv_diagonal_down": (27, "CROUCH_END"),
    }
    for case_id, (expected_count, source_action) in crouch_cases.items():
        current = case_rows(rows, case_id)
        edge_index = next(
            (
                index
                for index, row in enumerate(current)
                if str(row.get("label", "")).endswith("_edge")
            ),
            -1,
        )
        if (
            len(current) != expected_count
            or edge_index <= 0
            or current[edge_index - 1].get("action") != source_action
        ):
            fail(f"crouch-source case={case_id}")
        require_action_frames(
            current[edge_index:],
            "SWORD_DANCE_4_LOW",
            list(range(1, 7)),
            case_id,
        )

    walk_priority = case_rows(rows, "walk_grab_special_priority")
    if (
        len(walk_priority) != 7
        or walk_priority[0].get("action")
        not in {"WALK_SLOW", "WALK_MIDDLE", "WALK_FAST"}
    ):
        fail("walk-source case=walk_grab_special_priority")
    require_action_frames(
        walk_priority[1:],
        "GRAB",
        list(range(1, 7)),
        "walk_grab_special_priority",
    )

    walk_special = case_rows(rows, "walk_side_special")
    if (
        len(walk_special) != 7
        or walk_special[0].get("action")
        not in {"WALK_SLOW", "WALK_MIDDLE", "WALK_FAST"}
    ):
        fail("walk-source case=walk_side_special")
    require_action_frames(
        walk_special[1:],
        "NEUTRAL_B_ATTACKING_AIR",
        list(range(1, 7)),
        "walk_side_special",
    )

    wait_priority = case_rows(rows, "wait_grab_special_priority")
    require_action_frames(
        wait_priority,
        "NEUTRAL_B_ATTACKING_AIR",
        list(range(1, 7)),
        "wait_grab_special_priority",
    )

    wait_up_priority = case_rows(
        rows, "wait_up_special_attack_priority"
    )
    require_action_frames(
        wait_up_priority,
        "SWORD_DANCE_3_MID",
        list(range(1, 7)),
        "wait_up_special_attack_priority",
    )

    dash_side = case_rows(rows, "dash_side")
    if (
        len(dash_side) != 7
        or dash_side[0].get("action") != "DASHING"
        or round(float(dash_side[0]["action_frame"])) != 1
    ):
        fail("dash-source case=dash_side")
    require_action_frames(
        dash_side[1:],
        "SWORD_DANCE_1",
        list(range(1, 7)),
        "dash_side",
    )
    require_tilt_x_ages(dash_side, [254] * 7, "dash_side")

    for case_id in ("dash_neutral", "dash_down"):
        current = case_rows(rows, case_id)
        require_action_frames(
            current,
            "DASHING",
            list(range(1, 8)),
            case_id,
        )
        require_tilt_x_ages(current, [254] * 7, case_id)

    dash_up = case_rows(rows, "dash_up")
    if len(dash_up) != 7 or dash_up[0].get("action") != "DASHING":
        fail("dash-source case=dash_up")
    require_action_frames(
        dash_up[1:5],
        "KNEE_BEND",
        list(range(1, 5)),
        "dash_up",
    )
    require_tilt_x_ages(dash_up, [254] * 7, "dash_up")
    require_action_frames(
        dash_up[5:],
        "JUMPING_FORWARD",
        [1, 2],
        "dash_up",
    )

    run_guard_hit = case_rows(rows, "run_guard_dash_grab_hit")
    if len(run_guard_hit) != 28:
        fail(
            "row-count case=run_guard_dash_grab_hit "
            f"actual={len(run_guard_hit)}"
        )
    require_ordered_actions(
        run_guard_hit[-3:],
        [("SHIELD_REFLECT", -1), ("GRAB_RUNNING", 1), ("GRAB_RUNNING", 2)],
        "run_guard_dash_grab_hit",
    )

    wait_guard = case_rows(rows, "wait_guard_ordinary_grab")
    if len(wait_guard) != 3:
        fail(
            "row-count case=wait_guard_ordinary_grab "
            f"actual={len(wait_guard)}"
        )
    require_ordered_actions(
        wait_guard,
        [("SHIELD_REFLECT", -1), ("GRAB", 1), ("GRAB", 2)],
        "wait_guard_ordinary_grab",
    )

    run_guard_expired = case_rows(rows, "run_guard_dash_grab_expired")
    if len(run_guard_expired) != 31:
        fail(
            "row-count case=run_guard_dash_grab_expired "
            f"actual={len(run_guard_expired)}"
        )
    require_ordered_actions(
        run_guard_expired[-6:],
        [("SHIELD_REFLECT", -1)] * 4 + [("GRAB", 1), ("GRAB", 2)],
        "run_guard_dash_grab_expired",
    )

    for case_id, current in (
        ("run_guard_dash_grab_hit", run_guard_hit),
        ("wait_guard_ordinary_grab", wait_guard),
        ("run_guard_dash_grab_expired", run_guard_expired),
    ):
        if any(row.get("grounded") is not True for row in current):
            fail(f"grounded context={case_id}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coverage", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("repeat_capture", type=Path)
    args = parser.parse_args()

    coverage = json.loads(args.coverage.read_text(encoding="utf-8"))
    if (
        coverage.get("schema") != 1
        or coverage.get("domain") != "falcon-common-special-acquisition"
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

    qualify_capture(capture, coverage)
    qualify_capture(repeat_capture, coverage)
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
        "ssbm-falcon-special-acquisition=pass "
        f"rows={len(capture['rows'])} "
        f"stored_cases={len(coverage['checkpoint_cases'])} "
        f"source_trace_sha256={observed_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
