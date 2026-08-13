#!/usr/bin/env python3
"""Qualify Falcon's quick/slow ledge-option clocks and root motion."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

from ssbm_live_trace import (
    canonical_sha256,
    normalized_sha256,
    parse_numeric_observations,
    require_equal,
    require_f32_close,
    source_x_to_sim_f32,
    source_y_to_sim_f32,
    validate_capture_provenance,
)


EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
EXPECTED_SOURCE_SHA256 = {
    "ftCo_CliffWait.c": "f43b4e3ea6bcee040aa61b5ef9628d8accb28a3f0c91893026dc619360955180",
    "ftCo_CliffClimb.c": "34d527eb1c79d978cb2ffdaef4a9860e5124f5949b62d50eebde92d909a66c4b",
    "ftCo_CliffEscape.c": "94f323fbf8fc18b31e4293dd1c38826d6ad5098a371bcd4c37b24ffe7cb88514",
    "ftCo_CliffJump.c": "55b7b72c136221ca014c52a1199f87cb2f64b0064d321dccf52c701ed6468bfa",
    "ftCo_CliffAttack.c": "be1ba084ec454179c6fdce983627c9c2b9468721b406b1126f6d7849739e9b88",
}

ACTION_LEDGE_HANG = 8
ACTION_LEDGE_CLIMB = 9
ACTION_AIRBORNE = 6
ACTION_LEDGE_ROLL = 85
ACTION_LEDGE_ATTACK = 86
ACTION_LEDGE_JUMP = 134

CASE_ACTIONS = {
    "quick_climb": ("EDGE_HANGING", "EDGE_GETUP_QUICK"),
    "slow_climb": ("EDGE_HANGING", "EDGE_GETUP_SLOW"),
    "quick_roll": ("EDGE_HANGING", "EDGE_ROLL_QUICK"),
    "slow_roll": ("EDGE_HANGING", "EDGE_ROLL_SLOW"),
    "quick_attack": ("EDGE_HANGING", "EDGE_ATTACK_QUICK"),
    "slow_attack": ("EDGE_HANGING", "EDGE_ATTACK_SLOW"),
    "quick_jump": (
        "EDGE_HANGING",
        "EDGE_JUMP_1_QUICK",
        "EDGE_JUMP_2_QUICK",
    ),
    "slow_jump": (
        "EDGE_HANGING",
        "EDGE_JUMP_1_SLOW",
        "EDGE_JUMP_2_SLOW",
    ),
    "drop": ("EDGE_HANGING", "FALLING"),
    "drop_cooldown": ("FALLING",),
    "quick_timeout": ("EDGE_HANGING", "TUMBLING"),
    "slow_timeout": ("EDGE_HANGING", "TUMBLING"),
    "c_inward_no_climb": ("EDGE_HANGING",),
    "c_inward_roll": ("EDGE_HANGING", "EDGE_ROLL_QUICK"),
    "c_up_attack": ("EDGE_HANGING", "EDGE_ATTACK_QUICK"),
    "c_outward_drop": ("EDGE_HANGING", "FALLING"),
    "main_priority_climb": ("EDGE_HANGING", "EDGE_GETUP_QUICK"),
    "main_priority_drop": ("EDGE_HANGING", "FALLING"),
    "c_outward_before_ready": ("EDGE_HANGING",),
}


def case_id(row: dict[str, Any]) -> str | None:
    match = re.fullmatch(
        r"surface_response_(.*?)_observe_[a-z0-9_-]+(?:_edge)?",
        row["label"],
    )
    return match.group(1) if match is not None else None


def source_cases(
    capture: dict[str, Any],
    sample_counts: dict[str, int],
) -> dict[str, list[dict[str, Any]]]:
    grouped = {name: [] for name in CASE_ACTIONS}
    for row in capture["rows"]:
        name = case_id(row)
        if name not in grouped or row["action"] not in CASE_ACTIONS[name]:
            continue
        grouped[name].append(row)

    # Ledge attacks freeze their source animation while colliding with the
    # setup opponent. This domain owns the underlying action/root clock, so a
    # single row per displayed frame removes only those qualified hitlag holds.
    for name in ("quick_attack", "slow_attack", "c_up_attack"):
        unique: list[dict[str, Any]] = []
        seen: set[tuple[str, int]] = set()
        for row in grouped[name]:
            key = (str(row["action"]), int(row["action_frame"]))
            if key not in seen:
                unique.append(row)
                seen.add(key)
        grouped[name] = unique
    for name, rows in grouped.items():
        if name == "drop_cooldown":
            grouped[name] = rows[: sample_counts[name]]
            continue
        first_wait = next(
            (index for index, row in enumerate(rows) if row["action"] == "EDGE_HANGING"),
            len(rows),
        )
        grouped[name] = rows[first_wait : first_wait + sample_counts[name]]
    return grouped


def mapped_action(action: str) -> int:
    if action == "EDGE_HANGING":
        return ACTION_LEDGE_HANG
    if "GETUP" in action:
        return ACTION_LEDGE_CLIMB
    if "ROLL" in action:
        return ACTION_LEDGE_ROLL
    if "ATTACK" in action:
        return ACTION_LEDGE_ATTACK
    if "JUMP" in action:
        return ACTION_LEDGE_JUMP
    if action in {"FALLING", "TUMBLING"}:
        return ACTION_AIRBORNE
    raise ValueError(f"unmapped source action {action}")


def qualify_source(
    cases: dict[str, list[dict[str, Any]]],
    expected_counts: dict[str, int],
    case_specs: dict[str, dict[str, Any]],
) -> None:
    require_equal(set(cases), set(expected_counts), "ledge-option case set")
    for name, rows in cases.items():
        require_equal(len(rows), expected_counts[name], f"{name} source rows")
        declared_samples = case_specs[name].get("source_samples")
        declared_cooldowns = case_specs[name].get(
            "source_cooldown_samples"
        )
        if declared_cooldowns is not None:
            require_equal(
                [
                    [
                        str(row["action"]),
                        int(row["ledge_regrab_cooldown"]),
                    ]
                    for row in rows
                ],
                declared_cooldowns,
                f"{name} sparse source cooldown",
            )
        elif declared_samples is not None:
            require_equal(
                [
                    [
                        str(row["action"]),
                        int(row.get("cliff_wait_timer", 0)),
                    ]
                    for row in rows
                ],
                declared_samples,
                f"{name} sparse source clock",
            )
        else:
            require_equal(
                [row["action"] for row in rows[:2]],
                ["EDGE_HANGING", "EDGE_HANGING"],
                f"{name} wait prefix",
            )
            require_equal(
                [int(row["action_frame"]) for row in rows[:2]],
                [1, 2],
                f"{name} wait clock",
            )
            previous_action = ""
            expected_frame = 0
            for row in rows:
                action = str(row["action"])
                if action != previous_action:
                    expected_frame = 1
                    previous_action = action
                require_equal(
                    int(row["action_frame"]),
                    expected_frame,
                    f"{name} {action} frame {expected_frame}",
                )
                expected_frame += 1
        if not all(int(row["facing"]) == -1 for row in rows):
            raise SystemExit(f"{name}: source facing changed")


def semantic_source_digest(cases: dict[str, list[dict[str, Any]]]) -> str:
    fields = (
        "action",
        "action_frame",
        "facing",
        "grounded",
        "position_x",
        "position_y",
        "air_velocity_x",
        "velocity_y",
        "invulnerable",
        "cliff_wait_timer",
        "ledge_regrab_cooldown",
    )
    return canonical_sha256(
        {
            name: [{field: row.get(field) for field in fields} for row in rows]
            for name, rows in cases.items()
        }
    )


def compare_sim(
    cases: dict[str, list[dict[str, Any]]],
    sim_output: Path,
    comparison_policy: dict[str, Any],
) -> None:
    velocity_tolerance = float(comparison_policy.get("velocity_tolerance_f32", 0.0))
    position_tolerance = float(comparison_policy.get("position_tolerance_f32", 0.0))
    if velocity_tolerance > 0.00048828125 or position_tolerance > 0.00439453125:
        raise SystemExit(
            "ledge-option live tolerance exceeds the qualified float32 envelope"
        )
    produced = parse_numeric_observations(
        sim_output,
        "m4-ssbm-ledge-options-observation ",
    )
    require_equal(set(produced), set(cases), "ledge-option simulation case set")
    for name, source in cases.items():
        actual = produced[name]
        require_equal(len(actual), len(source), f"{name} simulation rows")
        source_origin_x = float(source[0]["position_x"])
        source_origin_y = float(source[0]["position_y"])
        for index, (row, sample) in enumerate(zip(source, actual, strict=True), 1):
            prefix = f"{name} sample {index}"
            action = str(row["action"])
            require_equal(sample["sample"], index, f"{prefix} index")
            require_equal(sample["action"], mapped_action(action), f"{prefix} action")
            cooldown_sample = "ledge_regrab_cooldown" in row
            expected_action_tick = int(row["action_frame"]) - 1
            if "cliff_wait_timer" in row:
                expected_action_tick = (
                    (480 if name == "slow_timeout" else 640)
                    - int(row["cliff_wait_timer"])
                    if action == "EDGE_HANGING"
                    else 0
                )
            if not cooldown_sample:
                require_equal(
                    sample["action_tick"],
                    expected_action_tick,
                    f"{prefix} action clock",
                )
            require_equal(sample["facing"], int(row["facing"]), f"{prefix} facing")
            require_equal(
                sample["grounded"], int(bool(row["grounded"])), f"{prefix} grounded"
            )
            if not cooldown_sample:
                require_equal(
                    sample["invulnerable"],
                    int(bool(row["invulnerable"])),
                    f"{prefix} invulnerability",
                )
            require_equal(
                sample["tumble"],
                int(action == "TUMBLING"),
                f"{prefix} tumble",
            )
            if cooldown_sample:
                require_equal(
                    sample["ledge_regrab_cooldown"],
                    int(row["ledge_regrab_cooldown"]),
                    f"{prefix} ledge regrab cooldown",
                )
                continue
            require_f32_close(
                sample["dx"],
                source_x_to_sim_f32(float(row["position_x"]) - source_origin_x),
                position_tolerance,
                f"{prefix} position x",
            )
            require_f32_close(
                sample["dy"],
                source_y_to_sim_f32(float(row["position_y"]) - source_origin_y),
                position_tolerance,
                f"{prefix} position y",
            )
            require_f32_close(
                sample["self_vx"],
                source_x_to_sim_f32(float(row["air_velocity_x"])),
                velocity_tolerance,
                f"{prefix} self velocity x",
            )
            require_f32_close(
                sample["self_vy"],
                source_y_to_sim_f32(float(row["velocity_y"])),
                velocity_tolerance,
                f"{prefix} self velocity y",
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("sources", nargs=5, type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()

    for path in args.sources:
        expected = EXPECTED_SOURCE_SHA256.get(path.name)
        if expected is None or normalized_sha256(path) != expected:
            raise SystemExit(f"pinned {path.name} SHA-256 mismatch")
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    manifest = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    case_specs = {
        str(case["id"]): case for case in manifest["stored_oracle"]["cases"]
    }
    sample_counts = {
        name: int(case["sample_count"]) for name, case in case_specs.items()
    }
    require_equal(set(sample_counts), set(CASE_ACTIONS), "ledge-option manifest case set")
    validate_capture_provenance(
        capture,
        schema=11,
        stage="HYRULE_TEMPLE",
        fighter="CPTFALCON",
        opponent="CPTFALCON",
        disc_sha256=EXPECTED_DISC_SHA256,
        oracle_artifact_sha256=EXPECTED_EXIAI_SHA256,
        case_count=len(manifest["checkpoint_cases"]),
    )
    require_equal(
        len(capture["rows"]),
        int(manifest["checkpoint_pack"]["expected_rows"]),
        "ledge-option capture rows",
    )
    cases = source_cases(capture, sample_counts)
    qualify_source(cases, sample_counts, case_specs)
    observed = semantic_source_digest(cases)
    expected = manifest["stored_oracle"]["source_trace_sha256"]
    if args.allow_unpinned_capture:
        print(f"ledge-options-source-trace-sha256={observed}")
    elif observed != expected:
        raise SystemExit(f"ledge-option source trace SHA-256 mismatch: {observed} != {expected}")
    if args.sim_output is not None:
        compare_sim(
            cases,
            args.sim_output,
            dict(manifest["stored_oracle"]["comparison_policy"]),
        )
    print(
        "ssbm-falcon-ledge-options=pass "
        f"rows={len(capture['rows'])} cases={len(cases)} "
        f"samples={sum(map(len, cases.values()))} "
        f"sim_trace={int(args.sim_output is not None)} "
        f"source_trace_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
