#!/usr/bin/env python3
"""Qualify Falcon DI, SDI, and ASDI against a live NTSC 1.02 capture."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from ssbm_live_trace import normalized_sha256


EXPECTED_OBSERVATION_SHA256 = (
    "51402cd3605ba2761e3c11ed6baab74eb1b7ab22136822507b39d0a00cc40d95"
)
EXPECTED_DAMAGE_SOURCE_SHA256 = (
    "a3852f6377a71d03736b70b3869016a437b68c17dd703faead5be2954eb0278a"
)
EXPECTED_FIGHTER_SOURCE_SHA256 = (
    "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f"
)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)


def sha256(path: Path) -> str:
    return normalized_sha256(path)


def observation_sha256(rows: list[dict[str, Any]]) -> str:
    """Hash deterministic observations, excluding process/frame identities."""

    row_fields = (
        "label",
        "observed_main_x",
        "observed_main_y",
        "observed_c_x",
        "observed_c_y",
        "action",
        "action_value",
        "action_frame",
        "grounded",
        "position_x",
        "position_y",
        "velocity_y",
        "attack_velocity_x",
        "attack_velocity_y",
        "hitlag_left",
        "hitstun_left",
        "damage_percent",
    )
    memory_fields = (
        "knockback_velocity",
        "ground_knockback_velocity",
        "damage_percent",
        "knockback_angle",
        "knockback_magnitude",
        "hitlag_frames",
        "common",
        "sdi_stick_window",
    )
    canonical = []
    for row in rows:
        selected = {field: row[field] for field in row_fields}
        # The checkpoint is deliberately taken from an already-running match;
        # its harmless looping Standing frame depends on when menu setup
        # crossed the start-frame boundary. Damage-owned action frames are the
        # domain observation and remain hashed below.
        if float(row["damage_percent"]) == 0.0:
            selected["action_frame"] = None
        memory = dict(row["damage_memory"])
        selected["damage_memory"] = {
            field: memory[field] for field in memory_fields
        }
        canonical.append(selected)
    encoded = json.dumps(
        canonical,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=True,
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def close(actual: float, expected: float, tolerance: float, label: str) -> None:
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=tolerance):
        raise SystemExit(f"{label}: {actual} != {expected} +/- {tolerance}")


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    prefix = f"damage_response_{case_id}_"
    selected = [row for row in rows if str(row.get("label", "")).startswith(prefix)]
    if len(selected) != 23:
        raise SystemExit(f"{case_id}: expected 23 rows, got {len(selected)}")
    if max(float(row["damage_percent"]) for row in selected) != 2.0:
        raise SystemExit(f"{case_id}: Falcon Jab 1 did not deal exactly 2 damage")
    hitlag = [
        int(round(float(row["hitlag_left"])))
        for row in selected
        if float(row["damage_percent"]) > 0.0
    ][:4]
    if hitlag != [3, 2, 1, 0]:
        raise SystemExit(f"{case_id}: hitlag boundary mismatch: {hitlag}")
    return selected


def first_hit(rows: list[dict[str, Any]]) -> dict[str, Any]:
    return next(row for row in rows if float(row["hitlag_left"]) > 0.0)


def launch(rows: list[dict[str, Any]]) -> dict[str, Any]:
    return next(
        row
        for row in rows
        if float(row["damage_percent"]) > 0.0
        and float(row["hitlag_left"]) == 0.0
    )


def angle(row: dict[str, Any]) -> float:
    return math.degrees(
        math.atan2(
            float(row["attack_velocity_y"]),
            float(row["attack_velocity_x"]),
        )
    )


def parse_sim_observations(path: Path) -> dict[str, list[dict[str, int]]]:
    prefix = "m4-ssbm-damage-observation "
    parsed: dict[str, list[dict[str, int]]] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if not raw_line.startswith(prefix):
            continue
        fields: dict[str, str] = {}
        for token in raw_line[len(prefix) :].split():
            key, value = token.split("=", 1)
            fields[key] = value
        case_id = fields.pop("case")
        parsed.setdefault(case_id, []).append(
            {key: int(value) for key, value in fields.items()}
        )
    return parsed


def sim_x_to_source(value_f32: int) -> float:
    return float(value_f32) * 115.0 / (12.0 * 65536.0)


def sim_y_to_source(value_f32: int) -> float:
    return -float(value_f32) * 62.0 / (11.0 * 65536.0)


def compare_sim_observations(
    cases: dict[str, list[dict[str, Any]]],
    sim_output: Path,
) -> None:
    sim_cases = parse_sim_observations(sim_output)
    if set(sim_cases) != set(cases):
        raise SystemExit(
            f"simulation case coverage mismatch: {sorted(sim_cases)}"
        )
    for case_id, live_rows in cases.items():
        sim_rows = sim_cases[case_id]
        if len(sim_rows) != 3:
            raise SystemExit(
                f"{case_id}: expected three simulation observations"
            )
        live_hit = first_hit(live_rows)
        live_origin_x = float(live_hit["position_x"])
        live_origin_y = float(live_hit["position_y"])
        live_hitlag_rows = [
            row
            for row in live_rows
            if str(row["label"]).startswith(
                f"damage_response_{case_id}_hitlag_"
            )
        ]
        if len(live_hitlag_rows) != 3:
            raise SystemExit(
                f"{case_id}: expected three live hitlag observations"
            )
        for frame, (live, sim) in enumerate(
            zip(live_hitlag_rows, sim_rows, strict=True),
            start=1,
        ):
            if sim["frame"] != frame:
                raise SystemExit(
                    f"{case_id}: simulation frame order mismatch"
                )
            for key in ("hitlag", "hitstun"):
                live_key = f"{key}_left"
                if sim[key] != int(round(float(live[live_key]))):
                    raise SystemExit(
                        f"{case_id} frame {frame}: {key} mismatch"
                    )
            close(
                sim_x_to_source(sim["dx"]),
                float(live["position_x"]) - live_origin_x,
                0.001,
                f"{case_id} frame {frame} position x",
            )
            close(
                sim_y_to_source(sim["dy"]),
                float(live["position_y"]) - live_origin_y,
                0.001,
                f"{case_id} frame {frame} position y",
            )
            close(
                sim_x_to_source(sim["kb_vx"]),
                float(live["attack_velocity_x"]),
                0.001,
                f"{case_id} frame {frame} knockback x",
            )
            close(
                sim_y_to_source(sim["kb_vy"]),
                float(live["attack_velocity_y"]),
                0.001,
                f"{case_id} frame {frame} knockback y",
            )
            close(
                sim_x_to_source(sim["self_vx"]),
                float(live["air_velocity_x"]),
                0.001,
                f"{case_id} frame {frame} self velocity x",
            )
            close(
                sim_y_to_source(sim["self_vy"]),
                float(live["velocity_y"]),
                0.001,
                f"{case_id} frame {frame} self velocity y",
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("damage_source", type=Path)
    parser.add_argument("fighter_source", type=Path)
    parser.add_argument("--allow-unpinned-capture", action="store_true")
    parser.add_argument("--sim-output", type=Path)
    args = parser.parse_args()

    if sha256(args.damage_source) != EXPECTED_DAMAGE_SOURCE_SHA256:
        raise SystemExit("pinned ftCo_Damage.c SHA-256 mismatch")
    if sha256(args.fighter_source) != EXPECTED_FIGHTER_SOURCE_SHA256:
        raise SystemExit("pinned fighter.c SHA-256 mismatch")

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    rows = list(capture["rows"])
    observed_sha256 = observation_sha256(rows)
    if (
        not args.allow_unpinned_capture
        and observed_sha256 != EXPECTED_OBSERVATION_SHA256
    ):
        raise SystemExit(
            "damage-response observation SHA-256 mismatch: "
            f"{observed_sha256}"
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
        or capture.get("checkpoint_pack", {}).get("case_count") != 6
    ):
        raise SystemExit("damage-response capture provenance mismatch")

    raw_cases = coverage["checkpoint_pack"]["capture_plan"][
        "damage_response_cases"
    ]
    case_ids = [str(case["id"]) for case in raw_cases]
    cases = {case_id: case_rows(rows, case_id) for case_id in case_ids}
    if set(cases) != {
        "neutral",
        "di_full_right",
        "di_half_right",
        "sdi_radial_diagonal",
        "sdi_below_radial",
        "asdi_c_priority",
    }:
        raise SystemExit("damage-response case coverage mismatch")

    common = dict(first_hit(cases["neutral"])["damage_memory"]["common"])
    for key, expected in {
        "launch_velocity_scale": 0.03,
        "hitstun_scale": 0.4,
        "di_max_angle_degrees": 18.0,
        "ground_knockback_friction_scale": 1.0,
        "air_knockback_decay": 0.051,
        "sdi_minimum_stick_magnitude": 0.7,
        "sdi_position_scale": 6.0,
        "asdi_position_scale": 3.0,
    }.items():
        close(float(common[key]), expected, 1.0e-6, f"PlCo {key}")

    neutral_launch = launch(cases["neutral"])
    full_launch = launch(cases["di_full_right"])
    half_launch = launch(cases["di_half_right"])
    neutral_angle = angle(neutral_launch)
    full_delta = neutral_angle - angle(full_launch)
    half_delta = neutral_angle - angle(half_launch)
    if not 17.0 < full_delta <= 18.0 or not 4.0 < half_delta < 4.6:
        raise SystemExit(
            f"DI angular bounds mismatch: full={full_delta} half={half_delta}"
        )
    close(full_delta / half_delta, 4.0, 0.01, "squared DI projection")
    neutral_speed = math.hypot(
        float(neutral_launch["attack_velocity_x"]),
        float(neutral_launch["attack_velocity_y"]),
    )
    for label, row in (("full DI", full_launch), ("half DI", half_launch)):
        close(
            math.hypot(
                float(row["attack_velocity_x"]),
                float(row["attack_velocity_y"]),
            ),
            neutral_speed,
            2.0e-6,
            f"{label} speed",
        )

    radial_hit = first_hit(cases["sdi_radial_diagonal"])
    radial_pulse = next(
        row
        for row in cases["sdi_radial_diagonal"]
        if str(row["label"]).endswith("_hitlag_1")
    )
    below_hit = first_hit(cases["sdi_below_radial"])
    below_pulse = next(
        row
        for row in cases["sdi_below_radial"]
        if str(row["label"]).endswith("_hitlag_1")
    )
    close(float(radial_pulse["position_x"]) - float(radial_hit["position_x"]), 3.0, 0.01, "radial SDI x")
    close(float(radial_pulse["position_y"]) - float(radial_hit["position_y"]), 3.0, 0.01, "radial SDI y")
    close(float(below_pulse["position_x"]) - float(below_hit["position_x"]), 0.0, 0.01, "below-radial SDI x")
    close(float(below_pulse["position_y"]) - float(below_hit["position_y"]), 0.0, 0.01, "below-radial SDI y")

    asdi_hold = next(
        row
        for row in cases["asdi_c_priority"]
        if str(row["label"]).endswith("_hitlag_2")
    )
    asdi_exit = launch(cases["asdi_c_priority"])
    close(
        float(asdi_exit["position_x"])
        - float(asdi_hold["position_x"])
        - float(asdi_exit["attack_velocity_x"]),
        0.0,
        0.01,
        "C-stick-priority ASDI x",
    )
    close(
        float(asdi_exit["position_y"])
        - float(asdi_hold["position_y"])
        - float(asdi_exit["attack_velocity_y"]),
        3.0 + float(asdi_exit["velocity_y"]),
        0.01,
        "C-stick-priority ASDI y",
    )
    if args.sim_output is not None:
        compare_sim_observations(cases, args.sim_output)

    print(
        "ssbm-falcon-damage-response=pass "
        f"rows={len(rows)} cases={len(cases)} "
        f"di_full_degrees={full_delta:.6f} "
        f"di_half_degrees={half_delta:.6f} "
        "radial_sdi=1 c_stick_asdi_priority=1 "
        f"sim_trace={int(args.sim_output is not None)} "
        f"observation_sha256={observed_sha256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
