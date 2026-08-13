#!/usr/bin/env python3
"""Qualify Falcon's normal throw hitboxes and release against Dolphin."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from import_ssbm_falcon_frame_data import throw_release_frame


EXPECTED_CAPTURE_SHA256 = (
    "368c623e49231aff0f70c8aa687345f10e615b121a675dbddcb8abd99a3a0b95"
)
EXPECTED_FULL_SOURCE_SHA256 = (
    "287d53686aedb7469e455600cd749001b2f1a04081158236f26b1fae205f6dde"
)
EXPECTED_DAT_SOURCE_SHA256 = (
    "fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266"
)
ACTION_BY_MOVE = {
    "fthrow": "THROW_FORWARD",
    "bthrow": "THROW_BACK",
    "uthrow": "THROW_UP",
    "dthrow": "THROW_DOWN",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def active_hitbox_frames(rows: list[dict[str, Any]]) -> set[int]:
    return {
        round(float(row["action_frame"]))
        for row in rows
        if any(int(hitbox["state"]) != 0 for hitbox in row["hitbox_memory"]["hitboxes"])
    }


def verify_move(
    move_key: str,
    source: dict[str, Any],
    dat_source: dict[str, Any],
    capture_rows: list[dict[str, Any]],
) -> int:
    action = ACTION_BY_MOVE[move_key]
    rows = [row for row in capture_rows if row.get("action") == action]
    if not rows:
        raise ValueError(f"{move_key}: missing action rows")
    expected_active = {
        frame
        for phase in source.get("hitFrames", [])
        for frame in range(int(phase["start"]), int(phase["end"]) + 1)
    }
    observed_active = active_hitbox_frames(rows)
    if observed_active != expected_active:
        raise ValueError(
            f"{move_key}: active frames {sorted(observed_active)} != "
            f"{sorted(expected_active)}"
        )

    release_frame = throw_release_frame(
        dat_source,
        int(source["subactionIndex"]),
        str(source["subactionName"]),
    )
    baseline_damage = float(rows[0]["opponent_damage_percent"])
    collateral_damage = (
        int(source["hitFrames"][0]["hitboxes"][0]["damage"]) if expected_active else 0
    )
    throw_damage = int(source["throw"]["damage"])
    first_collateral_rows = [
        row
        for row in rows
        if round(float(row["action_frame"])) == min(expected_active, default=-1)
        and float(row["opponent_damage_percent"]) > baseline_damage
    ]
    if expected_active:
        if not first_collateral_rows or any(
            float(row["opponent_damage_percent"]) != baseline_damage + collateral_damage
            for row in first_collateral_rows
        ):
            raise ValueError(f"{move_key}: captured-hitbox damage mismatch")
        holder_hitlag = [
            round(float(row["hitlag_left"]))
            for row in first_collateral_rows
            if float(row["hitlag_left"]) > 0.0
        ]
        victim_hitlag = [
            round(float(row["opponent_hitlag_left"]))
            for row in first_collateral_rows
            if float(row["opponent_hitlag_left"]) > 0.0
        ]
        if holder_hitlag != [4, 3, 2, 1] or victim_hitlag != [4, 3, 2, 1]:
            raise ValueError(f"{move_key}: synchronized hitlag mismatch")

    release_rows = [
        row
        for row in rows
        if round(float(row["action_frame"])) == release_frame
        and not str(row["opponent_action"]).startswith("THROWN_")
    ]
    if len(release_rows) != 1:
        raise ValueError(f"{move_key}: expected one release row at {release_frame}")
    release = release_rows[0]
    if (
        float(release["opponent_damage_percent"])
        != baseline_damage + collateral_damage + throw_damage
        or float(release["hitlag_left"]) != 0.0
        or float(release["opponent_hitlag_left"]) != 0.0
    ):
        raise ValueError(f"{move_key}: release damage or hitlag mismatch")
    return len(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("full_source", type=Path)
    parser.add_argument("dat_source", type=Path)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()

    if sha256(args.full_source) != EXPECTED_FULL_SOURCE_SHA256:
        raise SystemExit("unexpected Falcon full-source SHA-256")
    if sha256(args.dat_source) != EXPECTED_DAT_SOURCE_SHA256:
        raise SystemExit("unexpected Falcon DAT-source SHA-256")
    if sha256(args.capture) != EXPECTED_CAPTURE_SHA256:
        raise SystemExit("unexpected Dolphin throw-capture SHA-256")
    full_source = json.loads(args.full_source.read_text(encoding="utf-8"))
    dat_source = json.loads(args.dat_source.read_text(encoding="utf-8"))
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    if (
        capture.get("schema") != 9
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("revision") != 2
    ):
        raise SystemExit("unexpected Dolphin throw-capture provenance")

    comparable_frames = sum(
        verify_move(move, dict(full_source[move]), dat_source, capture["rows"])
        for move in ACTION_BY_MOVE
    )
    print(
        "ssbm-falcon-throw=pass "
        f"moves={len(ACTION_BY_MOVE)} comparable_frames={comparable_frames} "
        f"capture_sha256={EXPECTED_CAPTURE_SHA256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
