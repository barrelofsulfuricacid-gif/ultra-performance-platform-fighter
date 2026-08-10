#!/usr/bin/env python3
"""Verify Falcon's complete ordinary-airborne hurt-pose profile."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_hurt_pose_tracks import extract_track
from ssbm_collision import canonical_json_sha256, hurt_pose_tracks_semantic_payload
from ssbm_falcon_airborne_routes import (
    AIRBORNE_ROUTES,
    AIRBORNE_SUCCESSOR_TRACKS,
)


CAPTURE_SHA256 = (
    "cf458d593451c210b69fe45305c7affa992bf179d14aa2e3ce0b00e81d150a26"
)
REPEAT_CAPTURE_SHA256 = (
    "7f8aee28a613ca5b1ec5c1ea552b140ec515adbd28a4af071931c99d49ecfcab"
)
PROFILE_SHA256 = (
    "3eda3f573971b3f66e0e10e1087db444eb939cf67f6fb53ca72968ea730afb38"
)
SEMANTIC_SHA256 = (
    "71c9e643816604f9d2e90cfc226b907e7ce7cb48edc4fa2fea51d6797013ee7f"
)
DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
LAUNCHER_SHA256 = (
    "f06b96ddf780417e7c23ad2001879207ff8ea4bd88cc52195b297eae37661455"
)


def sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def load_capture(path: Path, expected_digest: str) -> dict[str, Any]:
    raw = path.read_bytes()
    digest = sha256(raw)
    if digest != expected_digest:
        raise ValueError(f"unexpected airborne hurt capture SHA-256: {digest}")
    capture = json.loads(raw)
    execution = capture.get("oracle_execution")
    disc = capture.get("disc")
    rows = capture.get("rows")
    if (
        capture.get("schema") != 12
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "FOX"
        or capture.get("stage") != "BATTLEFIELD"
        or capture.get("libmelee_version") != "0.47.2"
        or not isinstance(execution, dict)
        or execution.get("mode") != "exiai-headless-null-unlimited"
        or execution.get("launcher_sha256") != LAUNCHER_SHA256
        or not isinstance(disc, dict)
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != DISC_SHA256
        or not isinstance(rows, list)
        or len(rows) != 494
        or any(row.get("damage_percent") != 0.0 for row in rows)
    ):
        raise ValueError("unexpected airborne hurt capture provenance")
    return capture


def extract_verified_tracks(capture: dict[str, Any]) -> list[dict[str, Any]]:
    rows = capture["rows"]
    tracks: list[dict[str, Any]] = []
    for route in AIRBORNE_ROUTES:
        route_rows = [
            row
            for row in rows
            if str(row.get("label", "")).startswith(route.label_prefix)
        ]
        owned_rows = [
            row for row in route_rows if row.get("action") == route.source_action
        ]
        if (
            [int(row["action_frame"]) for row in owned_rows]
            != list(range(1, route.frame_count + 1))
            or any(row.get("grounded") is not False for row in owned_rows)
            or any(row.get("facing") != 1 for row in owned_rows)
        ):
            raise ValueError(f"invalid physical hurt span for {route.track_id}")
        last_owned_index = max(
            index
            for index, row in enumerate(route_rows)
            if row.get("action") == route.source_action
        )
        if route_rows[last_owned_index + 1].get("action") != route.successor:
            raise ValueError(f"invalid hurt successor for {route.track_id}")
        tracks.append(
            extract_track(
                rows,
                route.track_id,
                route.source_action,
                1,
                route.frame_count,
                label_prefix=route.label_prefix,
            )
        )
    for track_id, source_action, label, frame_count in AIRBORNE_SUCCESSOR_TRACKS:
        tracks.append(
            extract_track(
                rows,
                track_id,
                source_action,
                1,
                frame_count,
                label_prefix=label,
            )
        )
    return tracks


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("repeat_capture", type=Path)
    parser.add_argument("profile", type=Path)
    args = parser.parse_args()

    capture = load_capture(args.capture, CAPTURE_SHA256)
    tracks = extract_verified_tracks(capture)
    repeat_tracks = extract_verified_tracks(
        load_capture(args.repeat_capture, REPEAT_CAPTURE_SHA256)
    )
    if tracks != repeat_tracks:
        raise SystemExit("independent airborne hurt semantic tracks disagree")
    semantic_digest = canonical_json_sha256(
        hurt_pose_tracks_semantic_payload(tracks)
    )
    if semantic_digest != SEMANTIC_SHA256:
        raise SystemExit(
            f"unexpected airborne hurt semantic SHA-256: {semantic_digest}"
        )

    profile_raw = args.profile.read_bytes()
    profile = json.loads(profile_raw)
    if (
        sha256(profile_raw) != PROFILE_SHA256
        or profile.get("capture_sha256") != CAPTURE_SHA256
        or profile.get("semantic_sha256") != SEMANTIC_SHA256
        or profile.get("tracks") != tracks
    ):
        raise SystemExit("airborne hurt profile does not match live captures")

    pose_count = sum(route.frame_count for route in AIRBORNE_ROUTES) + sum(
        track[3] for track in AIRBORNE_SUCCESSOR_TRACKS
    )
    print(
        "ssbm-falcon-airborne-hurt=pass "
        f"rows={len(capture['rows'])} "
        f"tracks={len(tracks)} poses={pose_count} capsules={pose_count * 11} "
        f"capture_sha256={CAPTURE_SHA256} "
        f"repeat_capture_sha256={REPEAT_CAPTURE_SHA256} "
        f"semantic_sha256={SEMANTIC_SHA256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
