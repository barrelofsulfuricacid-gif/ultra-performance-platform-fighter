#!/usr/bin/env python3
"""Qualify Falcon Turn/TurnRun hurt poses across fast and control Dolphin."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_hurt_pose_tracks import extract_track
from ssbm_collision import canonical_json_sha256, hurt_pose_tracks_semantic_payload


EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
EXPECTED_TURN_SOURCE_SHA256 = (
    "3ad604c90ae3f67dd508cced55ab00ca6e7152a4a15693c5c78d4959434cbcfa"
)
EXPECTED_TURN_RUN_SOURCE_SHA256 = (
    "16db69ee636b00389e96d58bd8a7f956ae07704582797c81b895d9e342715d08"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def physical_capture_contract(manifest: dict[str, Any]) -> dict[str, Any]:
    """Exclude goldens and regression bindings from the physical capture."""

    checkpoint_pack = dict(manifest["checkpoint_pack"])
    checkpoint_pack.pop("expected_rows", None)
    checkpoint_pack.pop("warm_budget_seconds", None)
    return {
        "schema": manifest.get("schema"),
        "scope": manifest.get("scope"),
        "domain": manifest.get("domain"),
        "character": manifest.get("character"),
        "oracle": manifest.get("oracle"),
        "checkpoint_pack": checkpoint_pack,
        "checkpoint_cases": manifest.get("checkpoint_cases"),
        "capture_tracks": manifest.get(
            "capture_tracks", manifest.get("pose_tracks")
        ),
    }


def extract_tracks(
    capture: dict[str, Any],
    manifest: dict[str, Any],
) -> list[dict[str, Any]]:
    rows = capture["rows"]
    result: list[dict[str, Any]] = []
    for spec in manifest["capture_tracks"]:
        frames = spec["frames"]
        result.append(
            extract_track(
                rows,
                str(spec["id"]),
                str(spec["source_action"]),
                int(frames["first"]),
                int(frames["last"]),
                label_prefix=str(spec["label_prefix"]),
                pending_pose_facing_frame=spec.get(
                    "pending_pose_facing_frame"
                ),
            )
        )
    return result


def validate_capture(
    path: Path,
    manifest: dict[str, Any],
    expected_sha256: str,
    expected_mode: str,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    if sha256(path) != expected_sha256:
        raise SystemExit(f"unexpected raw capture SHA-256: {path}")
    capture = json.loads(path.read_text(encoding="utf-8"))
    execution = dict(capture.get("oracle_execution", {}))
    disc = dict(capture.get("disc", {}))
    probe = dict(capture.get("hitbox_memory_probe", {}))
    checkpoint = dict(capture.get("checkpoint_pack", {}))
    embedded = dict(checkpoint.get("coverage_manifest", {}))
    expected_labels = [
        str(case["start_label"]) for case in manifest["checkpoint_cases"]
    ]
    if (
        capture.get("schema") != 9
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("dolphin_version") != "3.5.1"
        or capture.get("libmelee_version") != "0.47.2"
        or execution.get("mode") != expected_mode
        or execution.get("release") != "exi-ai-0.2.0"
        or execution.get("release_artifact_sha256") != EXPECTED_EXIAI_SHA256
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != EXPECTED_DISC_SHA256
        or probe.get("decomp_revision") != EXPECTED_DECOMP_REVISION
        or capture.get("common_hurt_geometry_route") is not True
        or len(capture.get("rows", []))
        != int(manifest["checkpoint_pack"]["expected_rows"])
        or checkpoint.get("protocol")
        != manifest["checkpoint_pack"]["protocol"]
        or checkpoint.get("case_start_labels") != expected_labels
        or checkpoint.get("case_count") != len(expected_labels)
        or physical_capture_contract(embedded)
        != physical_capture_contract(manifest)
    ):
        raise SystemExit(f"unexpected turn-hurt capture provenance: {path}")
    return capture, extract_tracks(capture, manifest)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("accelerated_capture", type=Path)
    parser.add_argument("control_capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("profile", type=Path)
    parser.add_argument("turn_source", type=Path)
    parser.add_argument("turn_run_source", type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.coverage_manifest.read_text(encoding="utf-8"))
    live = dict(manifest["live_qualification"])
    accelerated, accelerated_tracks = validate_capture(
        args.accelerated_capture,
        manifest,
        str(live["accelerated_capture_sha256"]),
        "exiai-headless-null-fast-forward",
    )
    control, control_tracks = validate_capture(
        args.control_capture,
        manifest,
        str(live["control_capture_sha256"]),
        "exiai-headless-null-unlimited",
    )
    if accelerated_tracks != control_tracks:
        raise SystemExit("accelerated and control hurt-pose tracks differ")
    semantic_sha256 = canonical_json_sha256(
        hurt_pose_tracks_semantic_payload(accelerated_tracks)
    )
    profile = json.loads(args.profile.read_text(encoding="utf-8"))
    declaration = dict(manifest["stored_oracle"]["pose_profiles"][0])
    if (
        semantic_sha256 != live.get("semantic_sha256")
        or semantic_sha256 != declaration.get("semantic_sha256")
        or profile.get("tracks") != accelerated_tracks
        or profile.get("capture_sha256")
        != live.get("accelerated_capture_sha256")
        or profile.get("semantic_sha256") != semantic_sha256
        or sha256(args.profile) != declaration.get("profile_sha256")
        or sha256(args.turn_source) != EXPECTED_TURN_SOURCE_SHA256
        or sha256(args.turn_run_source) != EXPECTED_TURN_RUN_SOURCE_SHA256
    ):
        raise SystemExit("turn-hurt semantic profile or source pin mismatch")

    print(
        "ssbm-falcon-turn-hurt=pass "
        f"rows={len(accelerated['rows'])}+{len(control['rows'])} "
        f"poses={sum(track['frame_count'] for track in accelerated_tracks)} "
        f"semantic_sha256={semantic_sha256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
