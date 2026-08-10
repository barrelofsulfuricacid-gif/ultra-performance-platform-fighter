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
    *,
    opponent: bool = False,
    collision_trace_frames: dict[int, int] | None = None,
) -> dict[str, Any]:
    frames: dict[int, tuple[tuple[int, ...], ...]] = {}
    collision_trace_frames = collision_trace_frames or {}
    action_key = "opponent_action" if opponent else "action"
    action_frame_key = (
        "opponent_action_frame" if opponent else "action_frame"
    )
    facing_key = "opponent_facing" if opponent else "facing"
    hurtbox_key = "opponent_hurtboxes" if opponent else "fighter_hurtboxes"
    fighter_position_key = (
        "opponent_fighter_position" if opponent else "fighter_position"
    )
    for row in rows:
        trace_frame = row.get("trace_frame")
        collision_frame = next(
            (
                displayed
                for displayed, expected_trace in collision_trace_frames.items()
                if trace_frame == expected_trace
            ),
            None,
        )
        if collision_frame is None:
            if row.get(action_key) != source_action:
                continue
            raw_frame = row.get(action_frame_key)
            if (
                not isinstance(raw_frame, (int, float))
                or isinstance(raw_frame, bool)
                or float(raw_frame) != int(raw_frame)
            ):
                raise ValueError(f"track {track_id!r} has a non-integral frame")
            displayed_frame = int(raw_frame)
        else:
            displayed_frame = collision_frame
        if not first_displayed_frame <= displayed_frame <= last_displayed_frame:
            continue
        facing = row.get(facing_key)
        memory = row.get("hurtbox_memory", row.get("hitbox_memory"))
        if facing not in (-1, 1) or isinstance(facing, bool):
            raise ValueError(f"track {track_id!r} has invalid facing")
        if not isinstance(memory, dict):
            raise ValueError(f"track {track_id!r} is missing a hitbox probe")
        pose = canonical_hurt_pose_q16(
            memory,
            hurtbox_key,
            fighter_position_key,
            int(facing),
            MELEE_TO_SIM_Q16,
            (
                "collision_position"
                if "hurtbox_memory" not in row or collision_frame is not None
                else "position"
            ),
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
    )
    parser.add_argument(
        "--opponent-track",
        action="append",
        nargs=4,
        metavar=("ID", "SOURCE_ACTION", "FIRST_FRAME", "LAST_FRAME"),
    )
    parser.add_argument(
        "--opponent-collision-frame",
        action="append",
        nargs=3,
        metavar=("TRACK_ID", "DISPLAYED_FRAME", "TRACE_FRAME"),
        help=(
            "Map a post-transition trace row's collision endpoints to the "
            "pending displayed pose evaluated on that source frame."
        ),
    )
    args = parser.parse_args()

    capture_bytes = args.capture.read_bytes()
    capture = json.loads(capture_bytes)
    rows = capture.get("rows")
    if capture.get("schema") not in {9, 12} or not isinstance(rows, list) or not rows:
        raise SystemExit("capture is not a hurt-pose memory trace")

    track_specs = [
        (*values, False) for values in (args.track or [])
    ] + [
        (*values, True) for values in (args.opponent_track or [])
    ]
    if not track_specs:
        raise SystemExit("at least one --track or --opponent-track is required")
    collision_frames_by_track: dict[str, dict[int, int]] = {}
    for track_id, raw_displayed, raw_trace in (
        args.opponent_collision_frame or []
    ):
        try:
            displayed_frame = int(raw_displayed)
            trace_frame = int(raw_trace)
        except ValueError as error:
            raise SystemExit("collision-frame values must be integers") from error
        if (
            displayed_frame < 1
            or trace_frame < 0
            or str(displayed_frame) != raw_displayed
            or str(trace_frame) != raw_trace
            or displayed_frame
            in collision_frames_by_track.setdefault(track_id, {})
        ):
            raise SystemExit("invalid or duplicate opponent collision frame")
        collision_frames_by_track[track_id][displayed_frame] = trace_frame

    tracks: list[dict[str, Any]] = []
    track_ids: set[str] = set()
    roles: set[bool] = set()
    for raw_id, source_action, raw_first, raw_last, opponent in track_specs:
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
                opponent=opponent,
                collision_trace_frames=collision_frames_by_track.get(raw_id),
            )
        )
        track_ids.add(raw_id)
        roles.add(opponent)

    unknown_collision_tracks = set(collision_frames_by_track) - track_ids
    if unknown_collision_tracks:
        raise SystemExit(
            "collision frames reference unknown tracks: "
            + ", ".join(sorted(unknown_collision_tracks))
        )
    if len(roles) != 1:
        raise SystemExit("one profile may not mix fighter and opponent tracks")

    semantic = hurt_pose_tracks_semantic_payload(tracks)
    profile = {
        "schema": 1,
        "scope": "ssbm-bounded-hurt-pose-tracks",
        "oracle": capture.get("oracle"),
        "disc": capture.get("disc"),
        "fighter": (
            capture.get("opponent") if True in roles else capture.get("fighter")
        ),
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
