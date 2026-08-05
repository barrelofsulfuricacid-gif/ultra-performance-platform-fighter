#!/usr/bin/env python3
"""Validate the native Capsule branch of Falcon's Raptor Boost search."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


GALE01_NTSC_102_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    rows = list(capture.get("rows", ()))

    require(capture.get("schema") == 9, "item route requires schema 9")
    disc = dict(capture.get("disc", {}))
    require(disc.get("game_id") == "GALE01", "unexpected game ID")
    require(disc.get("revision") == 2, "unexpected disc revision")
    require(disc.get("sha256") == GALE01_NTSC_102_SHA256, "unexpected disc hash")
    require(capture.get("fighter") == "CPTFALCON", "unexpected fighter")
    require(capture.get("stage") == "FINAL_DESTINATION", "unexpected stage")
    require(len(rows) == 155, "unexpected item-route frame count")

    rules = dict(capture.get("item_rules", {}))
    require(rules.get("spawns_enabled") == 1, "native item spawns are disabled")
    accessors = dict(rules.get("rule_accessor_code", {}))
    require(
        accessors.get("frequency") == "386000044e800020",
        "native frequency accessor is not Very High",
    )
    require(
        accessors.get("runtime_mask") == "38600000388000014e800020",
        "native item mask accessor is not Capsule-only",
    )
    preferences = dict(rules.get("preferences", {}))
    require(preferences.get("frequency") == 4, "unexpected item preference")
    require(
        preferences.get("item_switch_mask") == "0000000020000000",
        "Capsule must use item-switch bit 29",
    )

    start_indices = [
        index
        for index, row in enumerate(rows)
        if row.get("label") == "special_geometry_side_ground_item_hit_start"
    ]
    require(len(start_indices) == 1, "expected exactly one Raptor Boost start")
    start_index = start_indices[0]
    start = rows[start_index]
    require(start.get("action_value") == 349, "Raptor Boost start did not begin")
    require(start.get("action_frame") == 1.0, "unexpected start action frame")

    hit_indices = [
        index
        for index, row in enumerate(rows)
        if index >= start_index and row.get("action_value") == 350
    ]
    require(hit_indices, "Capsule did not select grounded Raptor Boost hit")
    hit_index = hit_indices[0]
    require(hit_index - start_index == 14, "unexpected Capsule detection tick")
    hit = rows[hit_index]
    before_hit = rows[hit_index - 1]
    require(hit.get("action_frame") == 0.0, "hit action did not begin at frame zero")
    require(before_hit.get("action_value") == 349, "invalid pre-detection action")
    require(before_hit.get("action_frame") == 14.0, "invalid source gate frame")
    command_variables = hit.get("hitbox_memory", {}).get(
        "fighter_command_variables", ()
    )
    require(command_variables and command_variables[0] == 1, "search gate was closed")

    for name, row in (("start", start), ("hit", hit)):
        projectiles = list(row.get("projectiles", ()))
        require(len(projectiles) == 1, f"{name}: expected one native item")
        capsule = projectiles[0]
        require(capsule.get("type") == 255, f"{name}: unexpected Slippi item type")
        require(capsule.get("subtype") == 0, f"{name}: item is not grounded Capsule")
        require(capsule.get("velocity_x") == 0.0, f"{name}: Capsule moved horizontally")
        require(capsule.get("velocity_y") == 0.0, f"{name}: Capsule was airborne")
        require(
            abs(float(row["opponent_position_x"]) - float(capsule["x"])) >= 100.0,
            f"{name}: fighter target was not isolated from Capsule",
        )
        require(row.get("opponent_damage_percent") == 0.0, "opponent was damaged")

    print(
        "ssbm-falcon-item-search=pass "
        f"frames={len(rows)} detection_tick={hit_index - start_index} "
        "item=Capsule runtime_kind=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
