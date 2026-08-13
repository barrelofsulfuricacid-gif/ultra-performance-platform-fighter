#!/usr/bin/env python3
"""Qualify Falcon wall/ceiling responses against live NTSC 1.02."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


EXPECTED_OBSERVATION_SHA256 = (
    "5339134dd04cff9612e8c8a3e1d460f85018ae4c081ac7426fbad3cee3b785f5"
)
EXPECTED_SOURCE_SHA256 = {
    "ftCo_PassiveWall.c": (
        "5d05a8df6c5db5ba452a21ee0443f75d033433dd314f00f41d253c98c080daf0"
    ),
    "ftCo_PassiveCeil.c": (
        "ef79a00fe873d57e512bf2248240e65412cc3f8e2098aaa973ea229bc9adfd24"
    ),
    "ftCo_FlyReflect.c": (
        "8e14daf55997bc4f73e3d8b2b646472d2457bca49fbe81f8dc1d0c5797bceb76"
    ),
    "fighter.c": "1501d691fd445b713502770120b0e6f9057223088fa26896accc66a964e8ac3f",
    "ftcommon.c": "6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194",
}
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


def case_rows(rows: list[dict[str, Any]], case_id: str) -> list[dict[str, Any]]:
    label = f"surface_response_{case_id}_observe"
    return [row for row in rows if row["label"] == label]


def observation_sha256(capture: dict[str, Any]) -> str:
    row_fields = (
        "label",
        "requested_main_x",
        "requested_main_y",
        "requested_digital_left",
        "requested_digital_right",
        "requested_jump",
        "action",
        "action_value",
        "action_frame",
        "grounded",
        "ground_velocity_x",
        "air_velocity_x",
        "velocity_y",
        "attack_velocity_x",
        "attack_velocity_y",
        "hitlag_left",
        "hitstun_left",
        "invulnerable",
        "damage_percent",
    )
    canonical_rows = []
    for row in capture["rows"]:
        if not row["label"].endswith("_observe"):
            continue
        selected = {field: row[field] for field in row_fields}
        if (
            "right_pillar" in row["label"]
            and str(row["action"]).startswith("DAMAGE_FLY_")
        ):
            # This is the final pre-contact discriminator, not part of the
            # response. Minor checkpoint phase drift can select an equivalent
            # directional damage-fly animation before the same wall callback.
            selected["action"] = "DAMAGE_FLY"
            selected["action_value"] = None
            selected["action_frame"] = None
        canonical_rows.append(selected)
    stage = capture["stage_collision_memory"]
    stable_line_fields = (
        "index",
        "kind",
        "vertices",
        "start",
        "end",
        "neighbors",
        "hi_flags",
        "lo_flags",
    )
    canonical = {
        "rows": canonical_rows,
        "stage_collision": {
            "vertex_count": stage["vertex_count"],
            "line_count": stage["line_count"],
            "ranges": stage["ranges"],
            # The runtime probe may append derived world coordinates and
            # decoded flags. They are useful diagnostics but duplicate the
            # pinned raw line table and must not churn the oracle identity.
            "lines": [
                {field: line[field] for field in stable_line_fields}
                for line in stage["lines"]
            ],
        },
    }
    encoded = json.dumps(
        canonical,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=True,
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def parse_sim(path: Path) -> dict[str, list[dict[str, int]]]:
    prefix = "m4-ssbm-surface-response-observation "
    cases: dict[str, list[dict[str, int]]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith(prefix):
            continue
        fields: dict[str, str] = {}
        for token in line[len(prefix) :].split():
            key, value = token.split("=", 1)
            fields[key] = value
        case_id = fields.pop("case")
        cases.setdefault(case_id, []).append(
            {key: int(value) for key, value in fields.items()}
        )
    return cases


def sim_x_to_source(value_q16: int) -> float:
    return float(value_q16) * 115.0 / (12.0 * 65536.0)


def sim_y_to_source(value_q16: int) -> float:
    return -float(value_q16) * 62.0 / (11.0 * 65536.0)


def qualify_live(cases: dict[str, list[dict[str, Any]]]) -> None:
    wall_bounce = cases["right_pillar_wall_bounce"]
    wall = cases["right_pillar_wall_tech"]
    wall_jump = cases["right_pillar_wall_tech_jump"]
    ceiling_bounce = cases["cave_ceiling_bounce"]
    ceiling = cases["cave_ceiling_tech_drift"]

    if [len(case) for case in cases.values()] != [15, 30, 50, 15, 35]:
        raise SystemExit("surface-response case row counts changed")
    wall_surface = wall[1]["surface_collision_memory"]["surfaces"][
        "right_facing_wall"
    ]
    ceiling_surface = ceiling[0]["surface_collision_memory"]["surfaces"][
        "ceiling"
    ]
    if wall_surface["index"] != 70 or ceiling_surface["index"] != 47:
        raise SystemExit("Hyrule surface route discriminator mismatch")
    close(float(wall_surface["normal"][0]), 1.0, 1.0e-4, "wall normal")
    close(float(ceiling_surface["normal"][1]), -1.0, 1.0e-4, "ceiling normal")
    if (
        not wall_bounce[0]["action"].startswith("DAMAGE_FLY_")
        or [row["action"] for row in wall_bounce[1:]]
        != [*("BOUNCE_WALL" for _ in range(14))]
    ):
        raise SystemExit("wall-bounce action boundary mismatch")
    if [row["action"] for row in ceiling_bounce] != [
        *("BOUNCE_CEILING" for _ in range(15))
    ]:
        raise SystemExit("ceiling-bounce action boundary mismatch")
    if [int(row["action_frame"]) for row in wall_bounce[1:]] != list(
        range(14)
    ):
        raise SystemExit("wall-bounce animation progression mismatch")
    if [int(row["action_frame"]) for row in ceiling_bounce] != [
        *range(9),
        *([8] * 6),
    ]:
        raise SystemExit("ceiling-bounce animation clamp mismatch")
    for name, bounce in (
        ("wall", wall_bounce[1:]),
        ("ceiling", ceiling_bounce),
    ):
        if not all(row["invulnerable"] and not row["grounded"] for row in bounce):
            raise SystemExit(f"{name}-bounce invulnerability mismatch")
        hitstun = [int(row["hitstun_left"]) for row in bounce]
        if hitstun != list(range(hitstun[0], hitstun[0] - len(hitstun), -1)):
            raise SystemExit(f"{name}-bounce hitstun progression mismatch")

    for name, rows, action, release in (
        ("wall-tech", wall, "WALL_TECH", (0.49, -0.13)),
        ("wall-tech-jump", wall_jump, "WALL_TECH_JUMP", (1.39, 2.97)),
    ):
        response = [row for row in rows if row["action"] == action]
        if len(response) < 14 or any(
            row["action_frame"] != 0.0 for row in response[:6]
        ):
            raise SystemExit(f"{name} five-frame freeze mismatch")
        if any(
            row["air_velocity_x"] != 0.0 or row["velocity_y"] != 0.0
            for row in response[:5]
        ):
            raise SystemExit(f"{name} moved during freeze")
        close(
            float(response[5]["air_velocity_x"]),
            release[0],
            1.0e-6,
            f"{name} release x",
        )
        close(float(response[5]["velocity_y"]), release[1], 1.0e-6, f"{name} release y")
        if [bool(row["invulnerable"]) for row in response[:15]] != [
            *([True] * 14),
            False,
        ]:
            raise SystemExit(f"{name} invulnerability boundary mismatch")
        if any(int(row["hitstun_left"]) != 0 for row in response):
            raise SystemExit(f"{name} did not clear hitstun")

    ceiling_response = [row for row in ceiling if row["action"] == "CEILING_TECH"]
    if len(ceiling_response) != 26 or ceiling[26]["action"] != "FALLING":
        raise SystemExit("ceiling-tech 26-frame duration mismatch")
    if [int(row["action_frame"]) for row in ceiling_response] != list(range(26)):
        raise SystemExit("ceiling-tech frame progression mismatch")
    if [bool(row["invulnerable"]) for row in ceiling_response] != [
        *([True] * 11),
        *([False] * 15),
    ]:
        raise SystemExit("ceiling-tech invulnerability boundary mismatch")
    if any(int(row["hitstun_left"]) != 72 for row in ceiling_response):
        raise SystemExit("ceiling-tech did not preserve hitstun")
    for frame, row in enumerate(ceiling_response[1:11], start=1):
        close(
            float(row["air_velocity_x"]),
            0.06 * frame,
            1.0e-6,
            f"ceiling-tech drift {frame}",
        )
    close(
        float(ceiling_response[11]["air_velocity_x"]),
        1.99,
        1.0e-6,
        "ceiling-tech control release",
    )
    close(
        float(ceiling_response[25]["air_velocity_x"]),
        1.85,
        2.0e-6,
        "ceiling-tech friction",
    )


def compare_sim(
    live: dict[str, list[dict[str, Any]]], sim_path: Path
) -> None:
    sim = parse_sim(sim_path)
    expected_actions = {
        "right_pillar_wall_bounce": 30,
        "right_pillar_wall_tech": 27,
        "right_pillar_wall_tech_jump": 28,
        "cave_ceiling_bounce": 31,
        "cave_ceiling_tech_drift": 29,
    }
    if set(sim) != set(expected_actions) or any(
        len(rows) != 12 for rows in sim.values()
    ):
        raise SystemExit("simulation surface-response coverage mismatch")
    selected_live = {
        case_id: (rows[1:13] if case_id.startswith("right_pillar") else rows[:12])
        for case_id, rows in live.items()
    }
    for case_id, sim_rows in sim.items():
        for index, (source, produced) in enumerate(
            zip(selected_live[case_id], sim_rows, strict=True)
        ):
            exact = (
                (produced["frame"], index + 1, "sample"),
                (produced["action"], expected_actions[case_id], "action"),
                (produced["grounded"], int(bool(source["grounded"])), "grounded"),
                (produced["tumble"], int("bounce" in case_id), "tumble"),
                (
                    produced["invulnerable"],
                    int(bool(source["invulnerable"])),
                    "invulnerable",
                ),
            )
            for actual, expected, label in exact:
                if actual != expected:
                    raise SystemExit(
                        f"{case_id} frame {index + 1}: {label} {actual} != {expected}"
                    )
            if "wall_tech" in case_id:
                expected_frame = max(0, produced["action_tick"] - 5)
            elif case_id == "cave_ceiling_bounce":
                expected_frame = min(produced["action_tick"], 8)
            else:
                expected_frame = produced["action_tick"]
            if int(source["action_frame"]) != expected_frame:
                raise SystemExit(f"{case_id} frame {index + 1}: action-frame mismatch")
            if "tech" in case_id:
                close(
                    abs(sim_x_to_source(produced["self_vx"])),
                    abs(float(source["air_velocity_x"])),
                    0.0015,
                    f"{case_id} frame {index + 1} velocity x",
                )
                close(
                    sim_y_to_source(produced["self_vy"]),
                    float(source["velocity_y"]),
                    0.0015,
                    f"{case_id} frame {index + 1} velocity y",
                )
    print("surface-response-position-comparison=excluded-stage-geometry-domain")


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
        if expected is None or sha256(path) != expected:
            raise SystemExit(f"pinned {path.name} SHA-256 mismatch")
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    coverage = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    rows = list(capture["rows"])
    observed = observation_sha256(capture)
    if args.allow_unpinned_capture:
        print(f"surface-response-observation-sha256={observed}")
    elif observed != EXPECTED_OBSERVATION_SHA256:
        raise SystemExit(f"surface-response observation SHA-256 mismatch: {observed}")
    if len(rows) != int(coverage["checkpoint_pack"]["expected_rows"]):
        raise SystemExit("surface-response capture row count mismatch")
    if (
        capture.get("schema") != 11
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "HYRULE_TEMPLE"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("oracle_execution", {}).get("release_artifact_sha256")
        != EXPECTED_EXIAI_SHA256
        or capture.get("checkpoint_pack", {}).get("case_count") != 5
    ):
        raise SystemExit("surface-response capture provenance mismatch")
    ids = [
        case["id"]
        for case in coverage["checkpoint_pack"]["capture_plan"][
            "surface_response_cases"
        ]
    ]
    cases = {case_id: case_rows(rows, case_id) for case_id in ids}
    qualify_live(cases)
    if args.sim_output is not None:
        compare_sim(cases, args.sim_output)
    print(
        "ssbm-falcon-surface-response=pass "
        f"rows={len(rows)} cases={len(cases)} samples={sum(map(len, cases.values()))} "
        f"sim_trace={int(args.sim_output is not None)} observation_sha256={observed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
