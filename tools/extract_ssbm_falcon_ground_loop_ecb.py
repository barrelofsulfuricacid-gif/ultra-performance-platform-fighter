#!/usr/bin/env python3
"""Canonicalize Falcon's integer-clock CrouchWait ECB cycle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_ecb_pose_tracks import extract_cyclic_track
from ssbm_ecb_pose import canonical_sha256, semantic_payload


def animation_frame(row: dict[str, Any]) -> object:
    surface = row.get("surface_collision_memory")
    return (
        surface.get("fighter_animation_frame")
        if isinstance(surface, dict)
        else None
    )


def extract_tracks(capture: dict[str, Any]) -> list[dict[str, Any]]:
    rows = capture.get("rows")
    if not isinstance(rows, list) or not rows:
        raise ValueError("ground-loop capture has no rows")
    if not isinstance(capture.get("surface_collision_memory_probe"), dict):
        raise ValueError("ground-loop capture has no surface-memory provenance")
    return [
        extract_cyclic_track(
            rows,
            "crouch_wait",
            "CROUCHING",
            "common_hurt_crouch_wait_",
            158,
            animation_frame,
        )
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    capture_bytes = args.capture.read_bytes()
    capture = json.loads(capture_bytes)
    tracks = extract_tracks(capture)
    semantic = semantic_payload(tracks)
    profile = {
        "schema": 1,
        "scope": "ssbm-action-owned-ecb-pose-tracks",
        "oracle": capture.get("oracle"),
        "disc": capture.get("disc"),
        "fighter": capture.get("fighter"),
        "stage": capture.get("stage"),
        "capture_sha256": hashlib.sha256(capture_bytes).hexdigest(),
        "semantic_sha256": canonical_sha256(semantic),
        "coordinate_conversion": {
            "x_sim_units_per_melee_unit": "12/115",
            "y_sim_units_per_melee_unit": "11/62",
            "rounding": "ieee754-binary32",
        },
        "tracks": tracks,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(profile, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        "ssbm-falcon-ground-loop-ecb-extract=pass "
        "tracks=1 poses=158 "
        f"semantic_sha256={profile['semantic_sha256']} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
