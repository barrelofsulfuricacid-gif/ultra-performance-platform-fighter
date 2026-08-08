#!/usr/bin/env python3
"""Qualify Falcon common-state hurt poses and a dash collision boundary."""

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
    "df7085d40479c81634a34796c830a4be73d81ab64cce10f218c5508d5f8a2958"
)
EXPECTED_COLLISION_SOURCE_SHA256 = (
    "fa47d275f86956edb3c3a228a7fcc160e6f467c2d4bfd5f86d71f1d55e13e1fb"
)
EXPECTED_DASH_SOURCE_SHA256 = (
    "23fd2ad0af701c320fb24f6b5e7406971d7c31060b87916a20b242c076d10f7c"
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
    rows: list[dict[str, Any]], route: str
) -> list[dict[str, Any]]:
    prefix = f"common_hurt_dash_collision_{route}"
    return [row for row in rows if str(row.get("label", "")).startswith(prefix)]


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
        or len(rows) != 262
    ):
        raise SystemExit("unexpected common-hurt capture provenance")

    dash_rows = exact_action_frames(
        rows,
        "common_hurt_dash_hold",
        "DASHING",
        1,
        15,
    )
    exact_action_frames(
        rows,
        "common_hurt_dash_recover",
        "RUN_BRAKE",
        1,
        28,
    )
    positive = route_rows(rows, "hit")
    negative = route_rows(rows, "miss")
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

    dash_frame_five = dash_rows[4]
    miss_frame_five_candidates = [
        row
        for row in negative
        if row.get("action") == "NEUTRAL_ATTACK_1"
        and float(row.get("action_frame", -1.0)) == 5.0
        and row.get("opponent_action") == "DASHING"
        and float(row.get("opponent_action_frame", -1.0)) == 5.0
    ]
    if len(miss_frame_five_candidates) != 1:
        raise SystemExit("missing negative Jab 1 frame 5 versus Dash frame 5")
    miss_frame_five = miss_frame_five_candidates[0]
    source_pose = canonical_hurt_pose_q16(
        dict(dash_frame_five["hitbox_memory"]),
        "fighter_hurtboxes",
        "fighter_position",
        int(dash_frame_five["facing"]),
        MELEE_TO_SIM_Q16,
    )
    opponent_pose = canonical_hurt_pose_q16(
        dict(miss_frame_five["hitbox_memory"]),
        "opponent_hurtboxes",
        "opponent_fighter_position",
        int(miss_frame_five["opponent_facing"]),
        MELEE_TO_SIM_Q16,
    )
    if not q16_hurt_poses_equivalent(source_pose, opponent_pose):
        raise SystemExit("port-2 Dash pose does not match captured Falcon pose")

    memory = dict(miss_frame_five["hitbox_memory"])
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
    reconstructed_positive_hurtboxes = copy.deepcopy(hurtboxes)
    for hurtbox in reconstructed_positive_hurtboxes:
        hurtbox["collision_position_a"][0] -= 0.5
        hurtbox["collision_position_b"][0] -= 0.5
    hit_margin = max(
        captured_collision_margin(hitbox, hurtbox, True)
        for hitbox in hitboxes
        for hurtbox in reconstructed_positive_hurtboxes
    )
    generic_margin = generic_rectangle_margin(memory, -0.5)
    if hit_margin < 0.0 or miss_margin >= 0.0 or generic_margin >= 0.0:
        raise SystemExit(
            "dash collision discriminator failed: "
            f"hit_margin={hit_margin:.9f} "
            f"miss_margin={miss_margin:.9f} "
            f"generic_margin={generic_margin:.9f}"
        )

    print(
        "ssbm-common-hurt=pass "
        f"frames={len(rows)} dash_frames=15 run_brake_frames=28 "
        f"hit_margin={hit_margin:.9f} "
        f"miss_margin={miss_margin:.9f} "
        f"generic_margin={generic_margin:.9f} "
        f"capture_sha256={capture_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
