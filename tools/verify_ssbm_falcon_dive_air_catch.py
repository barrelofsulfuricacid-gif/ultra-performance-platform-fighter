#!/usr/bin/env python3
"""Verify the pinned, position-isolated Falcon Dive aerial catch theorem."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
from typing import Any

from extract_ssbm_hurt_pose_tracks import extract_track
from ssbm_collision import (
    binary32,
    canonical_json_sha256,
    captured_collision_margin,
    hurt_poses_equivalent,
    hurt_pose_tracks_semantic_payload,
)


MELEE_TO_SIM_F32 = 12.0 / 115.0


def maximum_grab_margin(
    memory: dict[str, Any],
    shift_x: float,
    shift_y: float,
) -> tuple[float, int]:
    hitboxes = [
        dict(hitbox)
        for hitbox in memory["hitboxes"]
        if float(hitbox["radius"]) > 0.0
    ]
    hurtboxes = [
        {**dict(hurtbox), "hurtbox_id": hurtbox_id}
        for hurtbox_id, hurtbox in enumerate(memory["opponent_hurtboxes"])
        if int(hurtbox["state"]) == 0 and int(hurtbox["grabbable"]) != 0
    ]
    if not hitboxes or not hurtboxes:
        raise ValueError("capture has no active grab/hurt geometry")
    margins: list[tuple[float, int]] = []
    for hitbox in hitboxes:
        for source_hurtbox in hurtboxes:
            hurtbox = copy.deepcopy(source_hurtbox)
            for endpoint in ("collision_position_a", "collision_position_b"):
                hurtbox[endpoint][0] += shift_x
                hurtbox[endpoint][1] += shift_y
            margins.append(
                (
                    captured_collision_margin(hitbox, hurtbox, False),
                    int(hurtbox["hurtbox_id"]),
                )
            )
    return max(margins)


def main() -> int:
    repository = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument(
        "--profile",
        type=Path,
        default=repository / "tools/data/ssbm_falcon_airborne_hurt.json",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=repository / "tools/ssbm_falcon_dive_grab_coverage.json",
    )
    args = parser.parse_args()

    capture_bytes = args.capture.read_bytes()
    profile_bytes = args.profile.read_bytes()
    capture = json.loads(capture_bytes)
    profile = json.loads(profile_bytes)
    manifest = json.loads(args.manifest.read_bytes())
    qualification = manifest.get("live_qualification")
    declaration = manifest["stored_oracle"]["pose_profiles"][0]
    if not isinstance(qualification, dict):
        raise SystemExit("coverage manifest has no live qualification")
    if (
        hashlib.sha256(capture_bytes).hexdigest()
        != qualification.get("capture_sha256")
        or capture.get("schema") != 9
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
    ):
        raise SystemExit("unexpected Falcon Dive capture provenance")
    profile_tracks = profile.get("tracks")
    if (
        hashlib.sha256(profile_bytes).hexdigest()
        != declaration.get("profile_sha256")
        or profile.get("capture_sha256") != declaration.get("capture_sha256")
        or profile.get("fighter") != "CPTFALCON"
        or not isinstance(profile_tracks, list)
        or canonical_json_sha256(hurt_pose_tracks_semantic_payload(profile_tracks))
        != declaration.get("semantic_sha256")
    ):
        raise SystemExit("unexpected Falcon Dive hurt-pose profile provenance")

    rows = capture.get("rows")
    if not isinstance(rows, list):
        raise SystemExit("Falcon Dive capture is missing rows")
    previous_frame = int(qualification["previous_trace_frame"])
    collision_frame = int(qualification["collision_trace_frame"])
    previous = next(row for row in rows if row.get("trace_frame") == previous_frame)
    collision = next(row for row in rows if row.get("trace_frame") == collision_frame)
    if (
        previous.get("action") != qualification.get("attacker_source_action")
        or previous.get("action_frame")
        != float(int(qualification["attacker_pending_frame"]) - 1)
        or previous.get("opponent_action")
        != qualification.get("target_source_action")
        or previous.get("opponent_action_frame")
        != float(int(qualification["target_pending_frame"]) - 1)
        or collision.get("action") != qualification.get("attacker_result_action")
        or collision.get("opponent_action")
        != qualification.get("target_result_action")
    ):
        raise SystemExit("Falcon Dive collision phase mapping drifted")

    extracted = extract_track(
        rows,
        "jump_forward",
        str(qualification["target_source_action"]),
        9,
        int(qualification["target_pending_frame"]),
        opponent=True,
        collision_trace_frames={
            int(qualification["target_pending_frame"]): collision_frame
        },
    )
    jump_forward_track = next(
        (track for track in profile_tracks if track.get("id") == "jump_forward"),
        None,
    )
    expected_frames = (
        []
        if not isinstance(jump_forward_track, dict)
        else [
            frame
            for frame in jump_forward_track["frames"]
            if 9 <= int(frame["displayed_frame"]) <= 20
        ]
    )
    if len(expected_frames) != len(extracted["frames"]):
        raise SystemExit("Falcon Dive pending hurt pose no longer matches profile")
    for extracted_frame, expected_frame in zip(
        extracted["frames"], expected_frames, strict=True
    ):
        actual_pose = tuple(
            tuple(capsule)
            for capsule in extracted_frame["capsules_f32"]
        )
        expected_pose = tuple(
            tuple(capsule)
            for capsule in expected_frame["capsules_f32"]
        )
        if int(extracted_frame["displayed_frame"]) < 20:
            if not hurt_poses_equivalent(actual_pose, expected_pose):
                raise SystemExit("Falcon Dive pre-catch JumpF pose drifted")
        else:
            hurtbox_id = int(qualification["target_collision_hurtbox_id"])
            actual_capsule = next(
                capsule for capsule in actual_pose if capsule[7] == hurtbox_id
            )
            expected_capsule = next(
                capsule for capsule in expected_pose if capsule[7] == hurtbox_id
            )
            if actual_capsule != expected_capsule:
                raise SystemExit("Falcon Dive colliding JumpF capsule drifted")

    memory = dict(collision["hitbox_memory"])
    attacker_position = [float(value) for value in memory["fighter_position"]]
    live_spheres = []
    for hitbox in memory["hitboxes"]:
        if float(hitbox["radius"]) <= 0.0:
            continue
        position = [float(value) for value in hitbox["position"]]
        live_spheres.append(
            [
                binary32((position[0] - attacker_position[0]) * MELEE_TO_SIM_F32),
                binary32(-(position[1] - attacker_position[1]) * MELEE_TO_SIM_F32),
                binary32((position[2] - attacker_position[2]) * MELEE_TO_SIM_F32),
                binary32(float(hitbox["radius"]) * MELEE_TO_SIM_F32),
            ]
        )
    if live_spheres != qualification.get("attacker_hit_spheres_f32"):
        raise SystemExit("Falcon Dive pending grab-sphere signature drifted")

    actual_target = [
        float(value) for value in memory["opponent_fighter_position"]
    ]
    actual_dx = actual_target[0] - attacker_position[0]
    actual_dy = actual_target[1] - attacker_position[1]
    actual_margin, actual_hurtbox_id = maximum_grab_margin(memory, 0.0, 0.0)
    if (
        actual_margin < 0.0
        or actual_hurtbox_id != int(qualification["target_collision_hurtbox_id"])
    ):
        raise SystemExit("Falcon Dive live collision winner drifted")
    margins: list[float] = []
    for case in manifest["stored_oracle"]["cases"]:
        geometry = case["geometry_f32"]
        offset_x_f32, offset_y_f32 = geometry["target_offset_f32"]
        requested_dx = float(offset_x_f32) / MELEE_TO_SIM_F32
        requested_dy = -float(offset_y_f32) / MELEE_TO_SIM_F32
        margin, _ = maximum_grab_margin(
            memory,
            requested_dx - actual_dx,
            requested_dy - actual_dy,
        )
        if (margin >= 0.0) != bool(case["expect_hit"]):
            raise SystemExit(
                f"Falcon Dive geometry case {case['id']!r} has margin {margin:.9f}"
            )
        margins.append(margin)

    print(
        "ssbm-falcon-dive-air-catch=pass "
        f"profile_poses={sum(int(track['frame_count']) for track in profile_tracks)} "
        f"qualified_jump_forward_poses={extracted['frame_count']} "
        f"pending_target_frame={qualification['target_pending_frame']} "
        f"pending_attacker_frame={qualification['attacker_pending_frame']} "
        f"hit_margin={margins[0]:.9f} miss_margin={margins[1]:.9f} "
        f"capture_sha256={qualification['capture_sha256']} "
        f"semantic_sha256={declaration['semantic_sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
