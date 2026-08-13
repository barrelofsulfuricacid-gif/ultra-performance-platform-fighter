#!/usr/bin/env python3
"""Verify Falcon's physical jump/fall ECB capture and imported profile."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_ecb_pose_tracks import extract_track
from ssbm_ecb_pose import canonical_sha256, semantic_payload
from ssbm_falcon_airborne_routes import (
    AIRBORNE_ROUTES,
    AIRBORNE_SUCCESSOR_TRACKS,
)


CAPTURE_SHA256 = (
    "4e6768e0862307eb32a14532fae8e2991e2900ea932b7af45850803c2ec8673f"
)
PROFILE_SHA256 = (
    "407a62269b2aa65002bb4a78152f12a49b56d36d8b68a684c6d55a11ce69a1ba"
)
SEMANTIC_SHA256 = (
    "21a2d02fbb3abfcd9c29bb170c4c378fc8972fe191098fb5587140e965dac25a"
)
DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
LAUNCHER_SHA256 = (
    "f06b96ddf780417e7c23ad2001879207ff8ea4bd88cc52195b297eae37661455"
)


def sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def load_capture(path: Path) -> tuple[bytes, dict[str, Any]]:
    raw = path.read_bytes()
    digest = sha256(raw)
    if digest != CAPTURE_SHA256:
        raise ValueError(f"unexpected airborne ECB capture SHA-256: {digest}")
    capture = json.loads(raw)
    execution = capture.get("oracle_execution")
    disc = capture.get("disc")
    if (
        capture.get("schema") != 11
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "FOX"
        or capture.get("stage") != "BATTLEFIELD"
        or capture.get("libmelee_version") != "0.47.2"
        or not isinstance(execution, dict)
        or execution.get("mode") != "exiai-headless-null-fast-forward"
        or execution.get("launcher_sha256") != LAUNCHER_SHA256
        or not isinstance(disc, dict)
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != DISC_SHA256
    ):
        raise ValueError("unexpected airborne ECB capture provenance")
    rows = capture.get("rows")
    if not isinstance(rows, list) or len(rows) != 494:
        raise ValueError("unexpected airborne ECB capture row count")
    if any(row.get("damage_percent") != 0.0 for row in rows):
        raise ValueError("airborne ECB capture is contaminated by damage")
    return raw, capture


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
            raise ValueError(f"invalid physical action span for {route.track_id}")
        last_owned_index = max(
            index
            for index, row in enumerate(route_rows)
            if row.get("action") == route.source_action
        )
        if route_rows[last_owned_index + 1].get("action") != route.successor:
            raise ValueError(f"invalid successor for {route.track_id}")
        tracks.append(
            extract_track(
                rows,
                route.track_id,
                route.source_action,
                route.label_prefix,
                1,
                route.frame_count,
            )
        )
    for track_id, source_action, label, frame_count in AIRBORNE_SUCCESSOR_TRACKS:
        successor_rows = [
            row
            for row in rows
            if row.get("label") == label and row.get("action") == source_action
        ]
        if (
            not successor_rows
            or any(row.get("grounded") is not False for row in successor_rows)
            or any(row.get("facing") != 1 for row in successor_rows)
        ):
            raise ValueError(f"invalid physical action span for {track_id}")
        tracks.append(
            extract_track(
                rows,
                track_id,
                source_action,
                label,
                1,
                frame_count,
            )
        )
    return tracks


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("repeat_capture", type=Path)
    parser.add_argument("profile", type=Path)
    args = parser.parse_args()

    raw, capture = load_capture(args.capture)
    repeat_raw, repeat_capture = load_capture(args.repeat_capture)
    if raw != repeat_raw:
        raise SystemExit("independent airborne ECB captures are not byte-identical")

    tracks = extract_verified_tracks(capture)
    repeat_tracks = extract_verified_tracks(repeat_capture)
    if tracks != repeat_tracks:
        raise SystemExit("independent airborne ECB semantic tracks disagree")
    semantic_digest = canonical_sha256(semantic_payload(tracks))
    if semantic_digest != SEMANTIC_SHA256:
        raise SystemExit(
            f"unexpected airborne ECB semantic SHA-256: {semantic_digest}"
        )

    profile_raw = args.profile.read_bytes()
    if sha256(profile_raw) != PROFILE_SHA256:
        raise SystemExit("unexpected airborne ECB profile SHA-256")
    profile = json.loads(profile_raw)
    if (
        profile.get("capture_sha256") != CAPTURE_SHA256
        or profile.get("semantic_sha256") != SEMANTIC_SHA256
        or profile.get("tracks") != tracks
    ):
        raise SystemExit("airborne ECB profile does not match live capture")

    pose_count = sum(route.frame_count for route in AIRBORNE_ROUTES) + sum(
        track[3] for track in AIRBORNE_SUCCESSOR_TRACKS
    )
    print(
        "ssbm-falcon-airborne-ecb=pass "
        f"rows={len(capture['rows'])} tracks={len(tracks)} "
        f"poses={pose_count} "
        f"capture_sha256={CAPTURE_SHA256} "
        f"semantic_sha256={SEMANTIC_SHA256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
