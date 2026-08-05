#!/usr/bin/env python3
"""Validate the pinned native Falcon Dive ledge-catch oracle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


EXPECTED_SHA256 = "5a5b295d0fc7a8d1c06512dc704176a131a7c01a931a0a2b92f6d7ff8c3a8295"
LEFT_LEDGE_FLAG = 0x01000000


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    payload = args.capture.read_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"unexpected Falcon Dive ledge capture SHA-256: {digest}")

    capture: dict[str, Any] = json.loads(payload)
    if capture.get("stage") != "FINAL_DESTINATION":
        raise SystemExit("Falcon Dive ledge oracle must use Final Destination")
    rows = capture["rows"]
    start = next(
        index
        for index, row in enumerate(rows)
        if row["label"] == "special_geometry_up_air_ledge_grab_start"
    )
    catch = next(
        index
        for index in range(start, len(rows))
        if rows[index]["action"] == "EDGE_CATCHING"
    )
    dive_rows = rows[start:catch]
    if [int(row["action_frame"]) for row in dive_rows] != list(range(1, 64)):
        raise SystemExit("Falcon Dive ledge oracle has an incomplete action timeline")
    if any(row["requested_fighter_x_override"] is not None for row in rows[start:]):
        raise SystemExit("Falcon Dive ledge oracle mutates position after move entry")

    catch_row = rows[catch]
    memory = catch_row["hitbox_memory"]
    current = memory["fighter_collision_positions"]["current"]
    previous = memory["fighter_collision_positions"]["previous"]
    contact = memory["fighter_collision_contact"]
    snap_x, snap_y, snap_height = memory["fighter_ledge_snap"]
    ecb = memory["fighter_ecb"]
    left = current[0]
    right = previous[0] + ecb["right"][0] + snap_x
    bottom = current[1] + snap_y - 0.5 * snap_height
    top = previous[1] + snap_y + 0.5 * snap_height
    ecb_bottom = current[1] + ecb["bottom"][1]
    if not (
        previous[1] > current[1]
        and left <= contact[0] <= right
        and bottom <= contact[1] <= top
        and ecb_bottom < contact[1]
        and int(memory["fighter_environment_flags"]) & LEFT_LEDGE_FLAG
        and catch_row["facing"] == 1
    ):
        raise SystemExit("Falcon Dive ledge catch violates the decomp collision probe")

    hanging = next(
        index
        for index in range(catch, len(rows))
        if rows[index]["action"] == "EDGE_HANGING"
    )
    print(
        "ssbm-falcon-dive-ledge=pass "
        f"dive_frames={len(dive_rows)} catch_frame={catch - start + 1} "
        f"edge_hang_frame={hanging - start + 1} sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
