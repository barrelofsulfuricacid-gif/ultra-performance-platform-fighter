#!/usr/bin/env python3
"""Qualify Falcon's complete DownBoundU/D ECB tracks against NTSC 1.02."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from extract_ssbm_ecb_pose_tracks import extract_track
from ssbm_ecb_pose import canonical_sha256, semantic_payload


EXPECTED_DISC_SHA256 = "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
EXPECTED_SEMANTIC_SHA256 = "3c4a4ce4586b11617aa99a08bac8709ea6d7aa8a179b5494c6f3f7fe4785c7df"
TRACKS = (
    ("down_bound_stomach", "TECH_MISS_DOWN", "floor_response_timeout_observe_"),
    ("down_bound_back", "TECH_MISS_UP", "floor_response_up_timeout_observe_"),
)


def profile_semantic(profile: dict[str, Any]) -> dict[str, Any]:
    tracks = profile.get("tracks")
    if not isinstance(tracks, list):
        raise SystemExit("down-bound-ecb=fail reason=profile-tracks")
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
            "down-bound-ecb=fail reason=profile-semantic-mismatch "
            f"expected={EXPECTED_SEMANTIC_SHA256} profile={profile_digest}"
        )

    if args.capture is None:
        print(
            "ssbm-falcon-down-bound-ecb-profile=pass tracks=2 poses=52 "
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
        or capture.get("checkpoint_pack", {}).get("case_count") != 2
        or not isinstance(rows, list)
        or len(rows) != 600
        or not isinstance(capture.get("surface_collision_memory_probe"), dict)
    ):
        raise SystemExit("down-bound-ecb=fail reason=capture-provenance")

    live_tracks = [
        extract_track(rows, track_id, action, label, 1, 26)
        for track_id, action, label in TRACKS
    ]
    live_semantic = semantic_payload(live_tracks)
    live_digest = canonical_sha256(live_semantic)
    if live_digest != EXPECTED_SEMANTIC_SHA256 or live_semantic != expected_semantic:
        raise SystemExit(
            "down-bound-ecb=fail reason=live-semantic-mismatch "
            f"expected={EXPECTED_SEMANTIC_SHA256} live={live_digest}"
        )

    print(
        "ssbm-falcon-down-bound-ecb=pass tracks=2 poses=52 rows=600 "
        f"semantic_sha256={live_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
