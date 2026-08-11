#!/usr/bin/env python3
"""Canonicalize Falcon DownWait and getup ECB tracks from focused captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_ecb_pose_tracks import extract_track
from ssbm_ecb_pose import (
    canonical_source_ecb,
    canonical_sha256,
    pose_q16,
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


def extract_cyclic_track(
    rows: list[dict[str, Any]],
    track_id: str,
    source_action: str,
    label_substring: str,
    frame_count: int,
) -> dict[str, Any]:
    selected = [
        row
        for row in rows
        if row.get("action") == source_action
        and label_substring in str(row.get("label", ""))
    ]
    try:
        start = next(
            index
            for index, row in enumerate(selected)
            if float(row.get("action_frame", -1.0)) == 0.0
        )
    except StopIteration as error:
        raise ValueError(f"track {track_id!r} has no frame-zero wrap") from error

    frames: list[dict[str, Any]] = []
    for displayed_frame, row in enumerate(selected[start : start + frame_count]):
        if float(row.get("action_frame", -1.0)) != float(displayed_frame):
            raise ValueError(
                f"track {track_id!r} frame order diverged at {displayed_frame}"
            )
        surface = row.get("surface_collision_memory")
        facing = row.get("facing")
        if (
            not isinstance(surface, dict)
            or not isinstance(surface.get("ecb"), dict)
            or facing not in (-1, 1)
            or isinstance(facing, bool)
        ):
            raise ValueError(f"track {track_id!r} has an invalid ECB row")
        source_ecb = canonical_source_ecb(surface["ecb"], int(facing))
        frames.append(
            {
                "displayed_frame": displayed_frame,
                "source_ecb": source_ecb,
                "ecb_q16": pose_q16(source_ecb),
            }
        )
    if len(frames) != frame_count:
        raise ValueError(f"track {track_id!r} is incomplete")
    for row in selected:
        displayed_frame = int(float(row.get("action_frame", -1.0)))
        if not 0 <= displayed_frame < frame_count:
            raise ValueError(f"track {track_id!r} has an invalid loop frame")
        surface = row.get("surface_collision_memory")
        facing = row.get("facing")
        if (
            not isinstance(surface, dict)
            or not isinstance(surface.get("ecb"), dict)
            or facing not in (-1, 1)
            or isinstance(facing, bool)
        ):
            raise ValueError(f"track {track_id!r} has an invalid ECB row")
        source_ecb = canonical_source_ecb(surface["ecb"], int(facing))
        if pose_q16(source_ecb) != frames[displayed_frame]["ecb_q16"]:
            raise ValueError(
                f"track {track_id!r} frame {displayed_frame} is non-deterministic"
            )
    return {
        "id": track_id,
        "source_action": source_action,
        "canonical_facing": 1,
        "label_substring": label_substring,
        "first_displayed_frame": 0,
        "frame_count": frame_count,
        "frames": frames,
    }


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
            "rounding": "nearest-python-round",
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
