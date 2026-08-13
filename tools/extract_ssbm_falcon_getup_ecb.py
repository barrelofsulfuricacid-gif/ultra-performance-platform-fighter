#!/usr/bin/env python3
"""Canonicalize Falcon DownWait and getup ECB tracks from focused captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_ecb_pose_tracks import extract_cyclic_track, extract_track
from ssbm_ecb_pose import (
    canonical_sha256,
    semantic_payload,
)


PRIMARY_TRACKS = (
    ("getup_neutral_stomach", "NEUTRAL_GETUP", "floor_response_timeout_observe_", 1, 30),
    ("getup_attack_stomach", "GETUP_ATTACK", "floor_response_buffered_a_getup_attack_observe_", 1, 49),
    ("getup_roll_forward_stomach", "GROUND_ROLL_FORWARD_DOWN", "floor_response_c_roll_forward_observe_", 1, 35),
    ("getup_roll_backward_stomach", "GROUND_ROLL_BACKWARD_DOWN", "floor_response_main_roll_backward_observe_", 1, 35),
    ("getup_neutral_back", "GROUND_GETUP", "floor_response_up_timeout_observe_", 1, 30),
    ("getup_attack_back", "GROUND_ATTACK_UP", "floor_response_up_buffered_a_getup_attack_observe_", 1, 49),
)

SUPPLEMENTAL_TRACKS = (
    ("getup_roll_forward_back", "GROUND_ROLL_FORWARD_UP", "floor_response_up_wait_c_roll_forward_observe_", 1, 35),
    ("getup_roll_backward_back", "GROUND_ROLL_BACKWARD_UP", "floor_response_up_wait_main_roll_backward_observe_", 1, 35),
)


def capture_rows(capture: dict[str, Any], label: str) -> list[dict[str, Any]]:
    rows = capture.get("rows")
    if not isinstance(rows, list) or not rows:
        raise ValueError(f"{label} capture has no rows")
    if not isinstance(capture.get("surface_collision_memory_probe"), dict):
        raise ValueError(f"{label} capture has no surface-memory provenance")
    return rows


def extract_tracks(capture: dict[str, Any]) -> list[dict[str, Any]]:
    rows = capture_rows(capture, "getup")
    return [
        extract_cyclic_track(
            rows,
            "down_wait_stomach",
            "LYING_GROUND_DOWN",
            "floor_response_timeout_observe_",
            70,
        ),
        extract_cyclic_track(
            rows,
            "down_wait_back",
            "LYING_GROUND_UP",
            "floor_response_up_timeout_observe_",
            70,
        ),
        *(
            extract_track(rows, *track)
            for track in PRIMARY_TRACKS
        ),
        *(
            extract_track(rows, *track)
            for track in SUPPLEMENTAL_TRACKS
        ),
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
        "ssbm-falcon-getup-ecb-extract=pass "
        f"tracks={len(tracks)} poses={sum(track['frame_count'] for track in tracks)} "
        f"semantic_sha256={profile['semantic_sha256']} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
