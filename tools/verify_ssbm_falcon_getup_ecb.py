#!/usr/bin/env python3
"""Qualify Falcon's complete DownWait/getup ECB tracks against NTSC 1.02."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from extract_ssbm_falcon_getup_ecb import extract_tracks
from ssbm_ecb_pose import canonical_sha256, semantic_payload


EXPECTED_DISC_SHA256 = "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
EXPECTED_SEMANTIC_SHA256 = "f519d632a88bcb582cb68865dd9a58d27e862fe619fc05d76ff3252ad5204f19"
EXPECTED_TRACKS = (
    ("down_wait_stomach", "LYING_GROUND_DOWN", 0, 70),
    ("down_wait_back", "LYING_GROUND_UP", 0, 70),
    ("getup_neutral_stomach", "NEUTRAL_GETUP", 1, 30),
    ("getup_attack_stomach", "GETUP_ATTACK", 1, 49),
    ("getup_roll_forward_stomach", "GROUND_ROLL_FORWARD_DOWN", 1, 35),
    ("getup_roll_backward_stomach", "GROUND_ROLL_BACKWARD_DOWN", 1, 35),
    ("getup_neutral_back", "GROUND_GETUP", 1, 30),
    ("getup_attack_back", "GROUND_ATTACK_UP", 1, 49),
    ("getup_roll_forward_back", "GROUND_ROLL_FORWARD_UP", 1, 35),
    ("getup_roll_backward_back", "GROUND_ROLL_BACKWARD_UP", 1, 35),
)


def profile_semantic(profile: dict[str, Any]) -> dict[str, Any]:
    tracks = profile.get("tracks")
    if not isinstance(tracks, list) or len(tracks) != len(EXPECTED_TRACKS):
        raise SystemExit("falcon-getup-ecb=fail reason=profile-tracks")
    for track, expected in zip(tracks, EXPECTED_TRACKS, strict=True):
        track_id, action, first_frame, frame_count = expected
        if (
            not isinstance(track, dict)
            or track.get("id") != track_id
            or track.get("source_action") != action
            or track.get("canonical_facing") != 1
            or track.get("first_displayed_frame") != first_frame
            or track.get("frame_count") != frame_count
            or not isinstance(track.get("frames"), list)
            or len(track["frames"]) != frame_count
            or [frame.get("displayed_frame") for frame in track["frames"]]
            != list(range(first_frame, first_frame + frame_count))
        ):
            raise SystemExit(f"falcon-getup-ecb=fail reason=track-{track_id}")
    return semantic_payload(tracks)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("profile", type=Path)
    parser.add_argument("--capture", type=Path)
    args = parser.parse_args()

    profile = json.loads(args.profile.read_text(encoding="utf-8"))
    expected_semantic = profile_semantic(profile)
    profile_digest = canonical_sha256(expected_semantic)
    if (
        profile.get("schema") != 1
        or profile.get("scope") != "ssbm-action-owned-ecb-pose-tracks"
        or profile.get("fighter") != "CPTFALCON"
        or profile.get("stage") != "FINAL_DESTINATION"
        or profile.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or profile.get("semantic_sha256") != EXPECTED_SEMANTIC_SHA256
        or profile_digest != EXPECTED_SEMANTIC_SHA256
    ):
        raise SystemExit(
            "falcon-getup-ecb=fail reason=profile-semantic-mismatch "
            f"expected={EXPECTED_SEMANTIC_SHA256} profile={profile_digest}"
        )

    if args.capture is None:
        print(
            "ssbm-falcon-getup-ecb-profile=pass tracks=10 poses=438 "
            f"semantic_sha256={profile_digest}"
        )
        return 0

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    rows = capture.get("rows")
    if (
        capture.get("schema") != 11
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("checkpoint_pack", {}).get("case_count") != 8
        or not isinstance(rows, list)
        or len(rows) != 1150
        or not isinstance(capture.get("surface_collision_memory_probe"), dict)
    ):
        raise SystemExit("falcon-getup-ecb=fail reason=capture-provenance")

    live_tracks = extract_tracks(capture)
    live_semantic = semantic_payload(live_tracks)
    live_digest = canonical_sha256(live_semantic)
    if live_digest != EXPECTED_SEMANTIC_SHA256 or live_semantic != expected_semantic:
        raise SystemExit(
            "falcon-getup-ecb=fail reason=live-semantic-mismatch "
            f"expected={EXPECTED_SEMANTIC_SHA256} live={live_digest}"
        )

    print(
        "ssbm-falcon-getup-ecb=pass tracks=10 poses=438 rows=1150 "
        f"semantic_sha256={live_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
