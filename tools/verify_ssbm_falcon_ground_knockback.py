#!/usr/bin/env python3
"""Qualify Falcon's flat-ground damage knockback against live NTSC 1.02."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from ssbm_live_trace import parse_numeric_observations


EXPECTED_OBSERVATION_SHA256 = (
    "e08d7149e3f46d814d5c4a709e316cf3063208bb9673141effe6b1958f03fc79"
)
EXPECTED_DAMAGE_SOURCE_SHA256 = (
    "a3852f6377a71d03736b70b3869016a437b68c17dd703faead5be2954eb0278a"
)
EXPECTED_FIGHTER_SOURCE_SHA256 = (
    "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f"
)
EXPECTED_FTCOMMON_SOURCE_SHA256 = (
    "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194"
)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def close(actual: float, expected: float, tolerance: float, label: str) -> None:
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=tolerance):
        raise SystemExit(f"{label}: {actual} != {expected} +/- {tolerance}")


def observation_sha256(rows: list[dict[str, Any]]) -> str:
    row_fields = (
        "label",
        "action",
        "action_value",
        "action_frame",
        "grounded",
        "position_x",
        "position_y",
        "ground_velocity_x",
        "air_velocity_x",
        "velocity_y",
        "attack_velocity_x",
        "attack_velocity_y",
        "hitlag_left",
        "hitstun_left",
        "damage_percent",
        "opponent_action",
        "opponent_action_frame",
    )
    canonical = []
    for row in rows:
        selected = {field: row[field] for field in row_fields}
        if float(row["damage_percent"]) == 0.0:
            selected["action_frame"] = None
        if row["opponent_action"] == "STANDING":
            # The checkpoint fixes position and combat state, not the cosmetic
            # phase of Falcon's looping idle animation. Once the approach
            # begins, the attacker's action frames remain authoritative.
            selected["opponent_action_frame"] = None
        selected["damage_memory"] = {
            "ground_knockback_velocity": row["damage_memory"][
                "ground_knockback_velocity"
            ],
            "ground_knockback_friction_scale": row["damage_memory"]["common"][
                "ground_knockback_friction_scale"
            ],
        }
        canonical.append(selected)
    encoded = json.dumps(
        canonical,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=True,
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def parse_sim_observations(
    path: Path,
) -> dict[str, list[dict[str, int | float]]]:
    return parse_numeric_observations(
        path, "m4-ssbm-ground-knockback-observation "
    )


def sim_x_to_source(value_f32: int | float) -> float:
    return float(value_f32) * 115.0 / 12.0


def sim_y_to_source(value_f32: int | float) -> float:
    return -float(value_f32) * 62.0 / 11.0


def compare_sim_observations(
    live_rows: list[dict[str, Any]], sim_output: Path
) -> None:
    cases = parse_sim_observations(sim_output)
    expected_case = "late_dash_attack_flat"
    if set(cases) != {expected_case}:
        raise SystemExit(f"simulation case coverage mismatch: {sorted(cases)}")
    sim_rows = cases[expected_case]
    if len(sim_rows) != len(live_rows):
        raise SystemExit(
            f"simulation sample count mismatch: {len(sim_rows)} != {len(live_rows)}"
        )
    for frame, (live, sim) in enumerate(
        zip(live_rows, sim_rows, strict=True), start=1
    ):
        effective_action = sim["resume"] if sim["action"] == 13 else sim["action"]
        exact_pairs = (
            (sim["frame"], frame, "frame"),
            (effective_action, 130, "effective action"),
            (sim["action_tick"] + 1, int(float(live["action_frame"])), "action frame"),
            (sim["grounded"], int(bool(live["grounded"])), "grounded"),
            (sim["tumble"], 0, "tumble"),
            (sim["damage"], int(float(live["damage_percent"])), "damage"),
            (sim["hitlag"], int(float(live["hitlag_left"])), "hitlag"),
            (sim["hitstun"], int(float(live["hitstun_left"])), "hitstun"),
        )
        for actual, expected, label in exact_pairs:
            if actual != expected:
                raise SystemExit(
                    f"frame {frame}: {label} mismatch: {actual} != {expected}"
                )
        close(
            sim_x_to_source(sim["ground_kb"]),
            float(live["damage_memory"]["ground_knockback_velocity"]),
            0.001,
            f"frame {frame} ground knockback",
        )
        close(
            sim_x_to_source(sim["kb_vx"]),
            float(live["attack_velocity_x"]),
            0.001,
            f"frame {frame} projected knockback x",
        )
        close(
            sim_y_to_source(sim["kb_vy"]),
            float(live["attack_velocity_y"]),
            0.001,
            f"frame {frame} projected knockback y",
        )
        close(
            sim_x_to_source(sim["self_vx"]),
            float(live["ground_velocity_x"]),
            0.001,
            f"frame {frame} self velocity x",
        )
        close(
            sim_y_to_source(sim["self_vy"]),
            float(live["velocity_y"]),
            0.001,
            f"frame {frame} self velocity y",
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("damage_source", type=Path)
    parser.add_argument("fighter_source", type=Path)
    parser.add_argument("ftcommon_source", type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()

    for path, expected, label in (
        (args.damage_source, EXPECTED_DAMAGE_SOURCE_SHA256, "ftCo_Damage.c"),
        (args.fighter_source, EXPECTED_FIGHTER_SOURCE_SHA256, "fighter.c"),
        (args.ftcommon_source, EXPECTED_FTCOMMON_SOURCE_SHA256, "ftcommon.c"),
    ):
        if sha256(path) != expected:
            raise SystemExit(f"pinned {label} SHA-256 mismatch")

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    rows = list(capture["rows"])
    observed_sha256 = observation_sha256(rows)
    if args.allow_unpinned_capture:
        print(f"ground-knockback-observation-sha256={observed_sha256}")
    elif observed_sha256 != EXPECTED_OBSERVATION_SHA256:
        raise SystemExit(
            "ground-knockback observation SHA-256 mismatch: " + observed_sha256
        )

    expected_rows = int(coverage["checkpoint_pack"]["expected_rows"])
    if len(rows) != expected_rows:
        raise SystemExit(f"capture row count mismatch: {len(rows)} != {expected_rows}")
    if (
        capture.get("schema") != 8
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("oracle_execution", {}).get("release_artifact_sha256")
        != EXPECTED_EXIAI_SHA256
        or capture.get("checkpoint_pack", {}).get("case_count") != 1
    ):
        raise SystemExit("ground-knockback capture provenance mismatch")

    # Damage percent persists after the reaction; the action family is the
    # source-defined boundary for this lifecycle.
    damage_rows = [row for row in rows if row["action"] == "DAMAGE_LOW_1"]
    if len(damage_rows) != 15:
        raise SystemExit(f"expected 15 damaged rows, got {len(damage_rows)}")
    first = damage_rows[0]
    if (
        first["action"] != "DAMAGE_LOW_1"
        or first["opponent_action"] != "DASH_ATTACK"
        or float(first["opponent_action_frame"]) != 15.0
        or float(first["damage_percent"]) != 7.0
    ):
        raise SystemExit("late DashAttack route discriminator mismatch")
    if not all(bool(row["grounded"]) for row in damage_rows):
        raise SystemExit("late DashAttack route left the ground")

    hitlag = [int(round(float(row["hitlag_left"]))) for row in damage_rows[:6]]
    hitstun = [int(round(float(row["hitstun_left"]))) for row in damage_rows[:14]]
    if hitlag != [5, 4, 3, 2, 1, 0]:
        raise SystemExit(f"ground hitlag boundary mismatch: {hitlag}")
    if hitstun != [8, 8, 8, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0]:
        raise SystemExit(f"ground hitstun boundary mismatch: {hitstun}")

    initial = float(first["damage_memory"]["ground_knockback_velocity"])
    close(initial, 0.634852945804596, 1.0e-7, "initial ground knockback")
    close(float(first["attack_velocity_x"]), initial, 1.0e-6, "initial projected x")
    close(float(first["attack_velocity_y"]), 0.0, 1.0e-7, "initial projected y")

    moving = damage_rows[5:13]
    expected_ground_velocity = initial
    for frame, row in enumerate(moving, start=1):
        expected_ground_velocity = max(0.0, expected_ground_velocity - 0.08)
        observed_ground = float(
            row["damage_memory"]["ground_knockback_velocity"]
        )
        close(observed_ground, expected_ground_velocity, 2.0e-7, f"decay frame {frame}")
        close(
            float(row["attack_velocity_x"]),
            observed_ground,
            1.0e-6,
            f"flat projection frame {frame}",
        )
        close(float(row["attack_velocity_y"]), 0.0, 1.0e-7, f"flat y frame {frame}")
    close(
        float(first["damage_memory"]["common"]["ground_knockback_friction_scale"]),
        1.0,
        1.0e-7,
        "PlCo ground knockback friction scale",
    )
    if args.sim_output is not None:
        compare_sim_observations(damage_rows, args.sim_output)

    print(
        "ssbm-falcon-ground-knockback=pass "
        f"rows={len(rows)} samples={len(damage_rows)} "
        f"sim_trace={int(args.sim_output is not None)} "
        "position_comparison=excluded_pushbox_domain "
        f"observation_sha256={observed_sha256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
