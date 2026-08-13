#!/usr/bin/env python3
"""Qualify Falcon's complete integer-clock CrouchWait ECB cycle."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from extract_ssbm_falcon_ground_loop_ecb import extract_tracks
from ssbm_ecb_pose import canonical_sha256, semantic_payload


EXPECTED_DISC_SHA256 = "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
EXPECTED_SEMANTIC_SHA256 = "ba47ef2736a5677d1909262a20f32991b7c2515407fae26626d5869b95edd265"
EXPECTED_TRACK = ("crouch_wait", "CROUCHING", 0, 158)


def profile_semantic(profile: dict[str, Any]) -> dict[str, Any]:
    tracks = profile.get("tracks")
    if not isinstance(tracks, list) or len(tracks) != 1:
        raise SystemExit("falcon-ground-loop-ecb=fail reason=profile-tracks")
    track = tracks[0]
    track_id, action, first_frame, frame_count = EXPECTED_TRACK
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
        raise SystemExit("falcon-ground-loop-ecb=fail reason=track-crouch-wait")
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
            "falcon-ground-loop-ecb=fail reason=profile-semantic-mismatch "
            f"expected={EXPECTED_SEMANTIC_SHA256} profile={profile_digest}"
        )

    if args.capture is None:
        print(
            "ssbm-falcon-ground-loop-ecb-profile=pass tracks=1 poses=158 "
            f"semantic_sha256={profile_digest}"
        )
        return 0

    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    rows = capture.get("rows")
    checkpoint_pack = capture.get("checkpoint_pack")
    case_labels = (
        checkpoint_pack.get("case_start_labels")
        if isinstance(checkpoint_pack, dict)
        else None
    )
    if (
        capture.get("schema") != 11
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or not isinstance(rows, list)
        or len(rows) < 160
        or not isinstance(case_labels, list)
        or "common_hurt_crouch_wait_place" not in case_labels
        or not isinstance(capture.get("surface_collision_memory_probe"), dict)
    ):
        raise SystemExit("falcon-ground-loop-ecb=fail reason=capture-provenance")

    live_tracks = extract_tracks(capture)
    live_semantic = semantic_payload(live_tracks)
    live_digest = canonical_sha256(live_semantic)
    if live_digest != EXPECTED_SEMANTIC_SHA256 or live_semantic != expected_semantic:
        raise SystemExit(
            "falcon-ground-loop-ecb=fail reason=live-semantic-mismatch "
            f"expected={EXPECTED_SEMANTIC_SHA256} live={live_digest}"
        )

    print(
        "ssbm-falcon-ground-loop-ecb=pass tracks=1 poses=158 "
        f"rows={len(rows)} semantic_sha256={live_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
