#!/usr/bin/env python3
"""Canonicalize bounded hurt-pose tracks from a Dolphin memory capture."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from ssbm_collision import (
    canonical_json_sha256,
    canonical_hurt_pose_q16,
    hurt_pose_tracks_semantic_payload,
    q16_hurt_poses_equivalent,
)


MELEE_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0


def extract_track(
    rows: list[dict[str, Any]],
    track_id: str,
    source_action: str,
    first_displayed_frame: int,
    last_displayed_frame: int,
) -> dict[str, Any]:
    frames: dict[int, tuple[tuple[int, ...], ...]] = {}
    for row in rows:
        if row.get("action") != source_action:
            continue
        raw_frame = row.get("action_frame")
        if (
            not isinstance(raw_frame, (int, float))
            or isinstance(raw_frame, bool)
            or float(raw_frame) != int(raw_frame)
        ):
            raise ValueError(f"track {track_id!r} has a non-integral frame")
        displayed_frame = int(raw_frame)
        if not first_displayed_frame <= displayed_frame <= last_displayed_frame:
            continue
        facing = row.get("facing")
        memory = row.get("hurtbox_memory", row.get("hitbox_memory"))
        if facing not in (-1, 1) or isinstance(facing, bool):
            raise ValueError(f"track {track_id!r} has invalid facing")
        if not isinstance(memory, dict):
            raise ValueError(f"track {track_id!r} is missing a hitbox probe")
        if "hurtbox_memory" not in row:
            memory = dict(memory)
            memory["fighter_hurtboxes"] = [
                {
                    **dict(capsule),
                    "position_a": capsule["collision_position_a"],
                    "position_b": capsule["collision_position_b"],
                }
                for capsule in memory["fighter_hurtboxes"]
            ]
        pose = canonical_hurt_pose_q16(
            memory,
            "fighter_hurtboxes",
            "fighter_position",
            int(facing),
            MELEE_TO_SIM_Q16,
        )
        if len(pose) != 11:
            raise ValueError(
                f"track {track_id!r} frame {displayed_frame} has "
                f"{len(pose)} hurt capsules"
            )
        previous = frames.get(displayed_frame)
        if previous is not None and not q16_hurt_poses_equivalent(previous, pose):
            raise ValueError(
                f"track {track_id!r} frame {displayed_frame} is inconsistent"
            )
        if previous is None:
            frames[displayed_frame] = pose

    expected = list(range(first_displayed_frame, last_displayed_frame + 1))
    if sorted(frames) != expected:
        raise ValueError(
            f"track {track_id!r} frames are {sorted(frames)}, expected {expected}"
        )
    return {
        "id": track_id,
        "source_action": source_action,
        "canonical_facing": 1,
        "first_displayed_frame": first_displayed_frame,
        "frame_count": len(expected),
        "frames": [
            {
                "displayed_frame": displayed_frame,
                "capsules_q16": [list(capsule) for capsule in frames[displayed_frame]],
            }
            for displayed_frame in expected
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--track",
        action="append",
        nargs=4,
        metavar=("ID", "SOURCE_ACTION", "FIRST_FRAME", "LAST_FRAME"),
        required=True,
    )
    args = parser.parse_args()

    capture_bytes = args.capture.read_bytes()
    capture = json.loads(capture_bytes)
    rows = capture.get("rows")
    if capture.get("schema") not in {9, 12} or not isinstance(rows, list) or not rows:
        raise SystemExit("capture is not a hurt-pose memory trace")

    tracks: list[dict[str, Any]] = []
    track_ids: set[str] = set()
    for raw_id, source_action, raw_first, raw_last in args.track:
        if (
            not raw_id
            or raw_id in track_ids
            or any(
                character not in "abcdefghijklmnopqrstuvwxyz0123456789_-"
                for character in raw_id
            )
        ):
            raise SystemExit(f"invalid or duplicate track id: {raw_id!r}")
        try:
            first_frame = int(raw_first)
            last_frame = int(raw_last)
        except ValueError as error:
            raise SystemExit("hurt-pose frame bounds must be integers") from error
        if (
            first_frame < 0
            or last_frame < first_frame
            or str(first_frame) != raw_first
            or str(last_frame) != raw_last
        ):
            raise SystemExit(
                f"invalid hurt-pose frame bounds: {raw_first!r}..{raw_last!r}"
            )
        tracks.append(
            extract_track(
                rows,
                raw_id,
                source_action,
                first_frame,
                last_frame,
            )
        )
        track_ids.add(raw_id)

    semantic = hurt_pose_tracks_semantic_payload(tracks)
    profile = {
        "schema": 1,
        "scope": "ssbm-bounded-hurt-pose-tracks",
        "oracle": capture.get("oracle"),
        "disc": capture.get("disc"),
        "fighter": capture.get("fighter"),
        "stage": capture.get("stage"),
        "capture_sha256": hashlib.sha256(capture_bytes).hexdigest(),
        "semantic_sha256": canonical_json_sha256(semantic),
        "coordinate_conversion": {
            "xyz_sim_units_per_melee_unit": "12/115",
            "rounding": "nearest-python-round",
            "canonical_facing": 1,
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
        "ssbm-hurt-pose-tracks=pass "
        f"tracks={len(tracks)} "
        f"poses={sum(track['frame_count'] for track in tracks)} "
        f"capture_sha256={profile['capture_sha256']} "
        f"semantic_sha256={profile['semantic_sha256']} "
        f"output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
