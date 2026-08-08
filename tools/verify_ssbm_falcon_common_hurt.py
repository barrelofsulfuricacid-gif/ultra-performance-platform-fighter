#!/usr/bin/env python3
"""Qualify Falcon fixed-duration common hurt poses and collision boundaries."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from ssbm_collision import (
    canonical_hurt_pose_q16,
    captured_collision_margin,
    q16_hurt_poses_equivalent,
)


EXPECTED_CAPTURE_SHA256 = (
    "aa64e6261e50130a70c6714e9b3177d44733a6c003de12cdff1581fb557380b0"
)
EXPECTED_COLLISION_SOURCE_SHA256 = (
    "fa47d275f86956edb3c3a228a7fcc160e6f467c2d4bfd5f86d71f1d55e13e1fb"
)
EXPECTED_DASH_SOURCE_SHA256 = (
    "23fd2ad0af701c320fb24f6b5e7406971d7c31060b87916a20b242c076d10f7c"
)
EXPECTED_CROUCH_SOURCE_SHA256 = (
    "80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082"
)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"
MELEE_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def active_hurtboxes(memory: dict[str, Any], key: str) -> list[dict[str, Any]]:
    return [
        dict(hurtbox)
        for hurtbox in memory.get(key, [])
        if int(hurtbox.get("state", 1)) == 0
    ]


def exact_action_frames(
    rows: list[dict[str, Any]],
    label: str,
    action: str,
    first_frame: int,
    last_frame: int,
) -> list[dict[str, Any]]:
    selected = [
        row
        for row in rows
        if row.get("label") == label and row.get("action") == action
    ]
    observed = [round(float(row["action_frame"])) for row in selected]
    expected = list(range(first_frame, last_frame + 1))
    if observed != expected:
        raise SystemExit(
            f"{action} frame coverage mismatch: {observed} != {expected}"
        )
    if any(
        len(active_hurtboxes(dict(row["hitbox_memory"]), "fighter_hurtboxes"))
        != 11
        for row in selected
    ):
        raise SystemExit(f"{action} did not expose 11 live hurt capsules")
    return selected


def route_rows(
    rows: list[dict[str, Any]], motion: str, route: str
) -> list[dict[str, Any]]:
    prefix = f"common_hurt_{motion}_collision_{route}"
    return [row for row in rows if str(row.get("label", "")).startswith(prefix)]


def requested_route_distance(rows: list[dict[str, Any]]) -> float:
    distances = {
        float(row["requested_opponent_x_override"])
        - float(row["requested_fighter_x_override"])
        for row in rows
        if row.get("requested_opponent_x_override") is not None
        and row.get("requested_fighter_x_override") is not None
    }
    if len(distances) != 1:
        raise SystemExit(f"expected one requested route distance, got {distances}")
    return next(iter(distances))


def collision_frame(
    rows: list[dict[str, Any]],
    attack_frame: int,
    target_action: str,
    target_frame: int,
) -> dict[str, Any]:
    candidates = [
        row
        for row in rows
        if row.get("action") == "NEUTRAL_ATTACK_1"
        and float(row.get("action_frame", -1.0)) == float(attack_frame)
        and row.get("opponent_action") == target_action
        and float(row.get("opponent_action_frame", -1.0))
        == float(target_frame)
    ]
    if len(candidates) != 1:
        raise SystemExit(
            f"expected one Jab 1 frame {attack_frame} versus "
            f"{target_action} frame {target_frame}, got {len(candidates)}"
        )
    return candidates[0]


def verify_cross_port_pose(
    source_row: dict[str, Any],
    opponent_row: dict[str, Any],
    label: str,
) -> None:
    source_pose = canonical_hurt_pose_q16(
        dict(source_row["hitbox_memory"]),
        "fighter_hurtboxes",
        "fighter_position",
        int(source_row["facing"]),
        MELEE_TO_SIM_Q16,
    )
    opponent_pose = canonical_hurt_pose_q16(
        dict(opponent_row["hitbox_memory"]),
        "opponent_hurtboxes",
        "opponent_fighter_position",
        int(opponent_row["opponent_facing"]),
        MELEE_TO_SIM_Q16,
    )
    if not q16_hurt_poses_equivalent(source_pose, opponent_pose):
        raise SystemExit(f"port-2 {label} pose does not match captured Falcon pose")


def reconstructed_collision_margins(
    miss_row: dict[str, Any], target_shift_x: float
) -> tuple[float, float]:
    memory = dict(miss_row["hitbox_memory"])
    hitboxes = [
        dict(hitbox)
        for hitbox in memory["hitboxes"]
        if int(hitbox.get("state", 0)) != 0
    ]
    hurtboxes = active_hurtboxes(memory, "opponent_hurtboxes")
    miss_margin = max(
        captured_collision_margin(hitbox, hurtbox, True)
        for hitbox in hitboxes
        for hurtbox in hurtboxes
    )
    reconstructed_hit_hurtboxes = copy.deepcopy(hurtboxes)
    for hurtbox in reconstructed_hit_hurtboxes:
        hurtbox["collision_position_a"][0] += target_shift_x
        hurtbox["collision_position_b"][0] += target_shift_x
    hit_margin = max(
        captured_collision_margin(hitbox, hurtbox, True)
        for hitbox in hitboxes
        for hurtbox in reconstructed_hit_hurtboxes
    )
    return hit_margin, miss_margin


def generic_rectangle_margin(
    memory: dict[str, Any], target_shift_x: float
) -> float:
    target = [float(value) for value in memory["opponent_fighter_position"]]
    center_x = target[0] + target_shift_x
    center_y = target[1] + 115.0 * 0.8 / 12.0
    half_width = 115.0 * 0.45 / 12.0
    half_height = 115.0 * 0.8 / 12.0
    margins = []
    for hitbox in memory["hitboxes"]:
        if int(hitbox.get("state", 0)) == 0:
            continue
        position = [float(value) for value in hitbox["position"]]
        nearest_x = min(
            max(position[0], center_x - half_width),
            center_x + half_width,
        )
        nearest_y = min(
            max(position[1], center_y - half_height),
            center_y + half_height,
        )
        margins.append(
            float(hitbox["radius"])
            - math.hypot(position[0] - nearest_x, position[1] - nearest_y)
        )
    return max(margins, default=-math.inf)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("collision_source", type=Path)
    parser.add_argument("dash_source", type=Path)
    parser.add_argument("crouch_source", type=Path)
    args = parser.parse_args()

    capture_digest = sha256(args.capture)
    if capture_digest != EXPECTED_CAPTURE_SHA256:
        raise SystemExit(
            f"unexpected common-hurt capture SHA-256: {capture_digest}"
        )
    if sha256(args.collision_source) != EXPECTED_COLLISION_SOURCE_SHA256:
        raise SystemExit("unexpected lbcollision.c SHA-256")
    if sha256(args.dash_source) != EXPECTED_DASH_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_Dash.c SHA-256")
    if sha256(args.crouch_source) != EXPECTED_CROUCH_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_Squat.c SHA-256")

    capture: dict[str, Any] = json.loads(
        args.capture.read_text(encoding="utf-8")
    )
    rows: list[dict[str, Any]] = list(capture.get("rows", []))
    probe = dict(capture.get("hitbox_memory_probe", {}))
    disc = dict(capture.get("disc", {}))
    if (
        capture.get("schema") != 9
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("dolphin_version") != "3.5.1"
        or capture.get("common_hurt_geometry_route") is not True
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != EXPECTED_DISC_SHA256
        or probe.get("decomp_revision") != EXPECTED_DECOMP_REVISION
        or len(rows) != 433
    ):
        raise SystemExit("unexpected common-hurt capture provenance")

    dash_rows = exact_action_frames(
        rows,
        "common_hurt_dash_hold",
        "DASHING",
        1,
        15,
    )
    run_brake_rows = exact_action_frames(
        rows,
        "common_hurt_dash_recover",
        "RUN_BRAKE",
        1,
        28,
    )
    crouch_start_rows = exact_action_frames(
        rows,
        "common_hurt_crouch_hold",
        "CROUCH_START",
        1,
        7,
    )
    crouch_end_rows = exact_action_frames(
        rows,
        "common_hurt_crouch_release",
        "CROUCH_END",
        1,
        10,
    )
    positive = route_rows(rows, "dash", "hit")
    negative = route_rows(rows, "dash", "miss")
    if (
        not positive
        or not negative
        or float(positive[0]["opponent_damage_percent"]) != 0.0
        or float(positive[-1]["opponent_damage_percent"]) != 2.0
        or not any(row.get("opponent_action") == "DAMAGE_HIGH_2" for row in positive)
        or float(negative[0]["opponent_damage_percent"]) != 2.0
        or any(float(row["opponent_damage_percent"]) != 2.0 for row in negative)
    ):
        raise SystemExit("dash collision hit/miss outcome mismatch")

    dash_miss_frame = collision_frame(negative, 5, "DASHING", 5)
    verify_cross_port_pose(dash_rows[4], dash_miss_frame, "Dash frame 5")
    dash_target_shift = (
        requested_route_distance(positive)
        - requested_route_distance(negative)
    )
    dash_hit_margin, dash_miss_margin = reconstructed_collision_margins(
        dash_miss_frame, dash_target_shift
    )
    dash_generic_margin = generic_rectangle_margin(
        dict(dash_miss_frame["hitbox_memory"]), dash_target_shift
    )
    if (
        dash_hit_margin < 0.0
        or dash_miss_margin >= 0.0
        or dash_generic_margin >= 0.0
    ):
        raise SystemExit(
            "dash collision discriminator failed: "
            f"hit_margin={dash_hit_margin:.9f} "
            f"miss_margin={dash_miss_margin:.9f} "
            f"generic_margin={dash_generic_margin:.9f}"
        )

    crouch_positive = route_rows(rows, "crouch", "hit")
    crouch_negative = route_rows(rows, "crouch", "miss")
    if (
        not crouch_positive
        or not crouch_negative
        or float(crouch_positive[0]["opponent_damage_percent"]) != 2.0
        or float(crouch_positive[-1]["opponent_damage_percent"]) != 3.0
        or not any(
            row.get("opponent_action") == "DAMAGE_NEUTRAL_1"
            for row in crouch_positive
        )
        or float(crouch_negative[0]["opponent_damage_percent"]) != 3.0
        or any(
            float(row["opponent_damage_percent"]) != 3.0
            for row in crouch_negative
        )
    ):
        raise SystemExit("crouch collision hit/miss outcome mismatch")

    crouch_miss_frame = collision_frame(
        crouch_negative, 3, "CROUCH_START", 3
    )
    verify_cross_port_pose(
        crouch_start_rows[2], crouch_miss_frame, "CrouchStart frame 3"
    )
    crouch_target_shift = (
        requested_route_distance(crouch_positive)
        - requested_route_distance(crouch_negative)
    )
    crouch_hit_margin, crouch_miss_margin = reconstructed_collision_margins(
        crouch_miss_frame, crouch_target_shift
    )
    crouch_generic_margin = generic_rectangle_margin(
        dict(crouch_miss_frame["hitbox_memory"]), 0.0
    )
    if (
        crouch_hit_margin < 0.0
        or crouch_miss_margin >= 0.0
        or crouch_generic_margin < 0.0
    ):
        raise SystemExit(
            "crouch collision discriminator failed: "
            f"hit_margin={crouch_hit_margin:.9f} "
            f"miss_margin={crouch_miss_margin:.9f} "
            f"generic_margin={crouch_generic_margin:.9f}"
        )

    print(
        "ssbm-common-hurt=pass "
        f"frames={len(rows)} dash_frames={len(dash_rows)} "
        f"run_brake_frames={len(run_brake_rows)} "
        f"crouch_start_frames={len(crouch_start_rows)} "
        f"crouch_end_frames={len(crouch_end_rows)} "
        f"dash_hit_margin={dash_hit_margin:.9f} "
        f"dash_miss_margin={dash_miss_margin:.9f} "
        f"dash_generic_margin={dash_generic_margin:.9f} "
        f"crouch_hit_margin={crouch_hit_margin:.9f} "
        f"crouch_miss_margin={crouch_miss_margin:.9f} "
        f"crouch_generic_margin={crouch_generic_margin:.9f} "
        f"capture_sha256={capture_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
