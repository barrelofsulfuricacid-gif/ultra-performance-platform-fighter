#!/usr/bin/env python3
"""Qualify Falcon fixed-duration common hurt poses and collision boundaries."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from ssbm_collision import (
    canonical_json_sha256,
    canonical_hurt_pose_q16,
    captured_collision_margin,
    hurt_pose_tracks_semantic_payload,
    q16_hurt_poses_equivalent,
)
from ssbm_checkpoint_manifest import projected_manifest


EXPECTED_CAPTURE_SHA256 = (
    "3d1d6b0047fadc3dc53cef830f0784216e8967f0e7424a08736ca787bec26de6"
)
EXPECTED_CHECKPOINT_POSE_SHA256 = (
    "3a1b182dc64ee6db6caa7cc316c633e3330a9001344ca88f5cd57a441b48cdf1"
)
EXPECTED_COLLISION_SOURCE_SHA256 = (
    "fa47d275f86956edb3c3a228a7fcc160e6f467c2d4bfd5f86d71f1d55e13e1fb"
)
EXPECTED_DASH_SOURCE_SHA256 = (
    "23fd2ad0af701c320fb24f6b5e7406971d7c31060b87916a20b242c076d10f7c"
)
EXPECTED_CROUCH_SOURCE_SHA256 = (
    "80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082"
)
EXPECTED_KNEE_SOURCE_SHA256 = (
    "91249dcf7a0aa59277e8912bd8b5a82548262df66ef3426d6ed3d27cebdd6c12"
)
EXPECTED_ESCAPE_SOURCE_SHA256 = (
    "762d18265d193e9d4b0b701a7a8048bb8824a4de5f505ceef00e316c1e56fb89"
)
EXPECTED_AIR_ESCAPE_SOURCE_SHA256 = (
    "cdff68de39d55855f1ca02b8e4af09ce856a1133cc21b23921a881b23e0dfaf6"
)
EXPECTED_FALL_SPECIAL_SOURCE_SHA256 = (
    "19217b0e24dc138f601b4c9914975da0879ece0a71ef968272fac75238aad6f4"
)
EXPECTED_LANDING_SOURCE_SHA256 = (
    "7e33d64809df680df293eeec1189299ab0f77d633f39c00dcd6756faab7d08e8"
)
EXPECTED_DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXPECTED_DECOMP_REVISION = "9509dc04406fb2028bfab01243841ba4787c0fb7"
EXPECTED_EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
MELEE_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checkpoint_capture_contract(manifest: dict[str, Any]) -> dict[str, Any]:
    """Return only fields that define the physical checkpoint capture."""
    contract = {
        key: manifest.get(key)
        for key in (
            "schema",
            "scope",
            "domain",
            "character",
            "oracle",
            "checkpoint_cases",
            "pose_tracks",
            "collision_qualification",
        )
    }
    checkpoint_pack = dict(manifest.get("checkpoint_pack") or {})
    contract["checkpoint_pack"] = {
        key: checkpoint_pack.get(key)
        for key in ("protocol", "capture_plan")
    }
    return contract


def active_hurtboxes(memory: dict[str, Any], key: str) -> list[dict[str, Any]]:
    return [
        dict(hurtbox)
        for hurtbox in memory.get(key, [])
        if int(hurtbox.get("state", 1)) == 0
    ]


def exact_action_frames(
    rows: list[dict[str, Any]],
    label: str | tuple[str, ...],
    action: str,
    first_frame: int,
    last_frame: int,
) -> list[dict[str, Any]]:
    labels = (label,) if isinstance(label, str) else label
    selected = [
        row
        for row in rows
        if row.get("label") in labels and row.get("action") == action
    ]
    observed = [round(float(row["action_frame"])) for row in selected]
    expected = list(range(first_frame, last_frame + 1))
    if observed != expected:
        raise SystemExit(
            f"{action} frame coverage mismatch: {observed} != {expected}"
        )
    if any(
        len(active_hurtboxes(dict(row["hitbox_memory"]), "fighter_hurtboxes"))
        != 11
        for row in selected
    ):
        raise SystemExit(f"{action} did not expose 11 live hurt capsules")
    return selected


def exact_looping_action_frames(
    rows: list[dict[str, Any]],
    labels: tuple[str, ...],
    action: str,
    frame_count: int,
) -> list[dict[str, Any]]:
    selected = [
        row
        for row in rows
        if row.get("label") in labels and row.get("action") == action
    ]
    observed = [round(float(row["action_frame"])) for row in selected]
    expected = [index % frame_count + 1 for index in range(len(selected))]
    if len(selected) < frame_count or observed != expected:
        raise SystemExit(
            f"{action} looping frame coverage mismatch: "
            f"{observed} != {expected}"
        )
    first_cycle = selected[:frame_count]
    if any(
        len(active_hurtboxes(dict(row["hitbox_memory"]), "fighter_hurtboxes"))
        != 11
        for row in selected
    ):
        raise SystemExit(f"{action} did not expose 11 live hurt capsules")
    return first_cycle


def exact_action_frame_sequence(
    rows: list[dict[str, Any]],
    label: str,
    action: str,
    expected_frames: tuple[int, ...],
) -> list[dict[str, Any]]:
    selected = [
        row
        for row in rows
        if row.get("label") == label and row.get("action") == action
    ]
    observed = tuple(round(float(row["action_frame"])) for row in selected)
    if observed != expected_frames:
        raise SystemExit(
            f"{action} frame sequence mismatch: "
            f"{observed} != {expected_frames}"
        )
    if any(
        len(active_hurtboxes(dict(row["hitbox_memory"]), "fighter_hurtboxes"))
        != 11
        for row in selected
    ):
        raise SystemExit(f"{action} did not expose 11 live hurt capsules")
    return selected


def route_rows(
    rows: list[dict[str, Any]], motion: str, route: str
) -> list[dict[str, Any]]:
    prefix = f"common_hurt_{motion}_collision_{route}"
    return [row for row in rows if str(row.get("label", "")).startswith(prefix)]


def verify_collision_outcome(
    rows: list[dict[str, Any]],
    motion: str,
    initial_damage: float,
    final_damage: float,
    damage_action: str,
    target_prefix: str = "opponent_",
    negative_damage: float | None = None,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    positive = route_rows(rows, motion, "hit")
    negative = route_rows(rows, motion, "miss")
    damage_key = f"{target_prefix}damage_percent"
    action_key = f"{target_prefix}action"
    expected_negative_damage = (
        final_damage if negative_damage is None else negative_damage
    )

    def close(left: Any, right: float) -> bool:
        return math.isclose(
            float(left), right, rel_tol=0.0, abs_tol=0.000001
        )
    if (
        not positive
        or not negative
        or not close(positive[0][damage_key], initial_damage)
        or not close(positive[-1][damage_key], final_damage)
        or not any(row.get(action_key) == damage_action for row in positive)
        or not close(negative[0][damage_key], expected_negative_damage)
        or any(
            not close(row[damage_key], expected_negative_damage)
            for row in negative
        )
    ):
        raise SystemExit(f"{motion} collision hit/miss outcome mismatch")
    return positive, negative


def requested_route_distance(
    rows: list[dict[str, Any]],
    target: str = "opponent",
    attacker: str = "fighter",
    placement_suffix: str = "_place",
) -> float:
    target_key = f"requested_{target}_x_override"
    attacker_key = f"requested_{attacker}_x_override"
    distances = {
        float(row[target_key]) - float(row[attacker_key])
        for row in rows
        if str(row.get("label", "")).endswith(placement_suffix)
        and not str(row.get("label", "")).endswith("_reset_place")
        and row.get(target_key) is not None
        and row.get(attacker_key) is not None
    }
    if len(distances) != 1:
        raise SystemExit(f"expected one requested route distance, got {distances}")
    return next(iter(distances))


def collision_frame(
    rows: list[dict[str, Any]],
    attack_frame: int,
    target_action: str,
    target_frame: int,
    attacker_prefix: str = "",
    target_prefix: str = "opponent_",
) -> dict[str, Any]:
    attacker_action_key = f"{attacker_prefix}action"
    attacker_frame_key = f"{attacker_prefix}action_frame"
    target_action_key = f"{target_prefix}action"
    target_frame_key = f"{target_prefix}action_frame"
    candidates = [
        row
        for row in rows
        if row.get(attacker_action_key) == "NEUTRAL_ATTACK_1"
        and float(row.get(attacker_frame_key, -1.0)) == float(attack_frame)
        and row.get(target_action_key) == target_action
        and float(row.get(target_frame_key, -1.0)) == float(target_frame)
    ]
    if len(candidates) != 1:
        raise SystemExit(
            f"expected one Jab 1 frame {attack_frame} versus "
            f"{target_action} frame {target_frame}, got {len(candidates)}"
        )
    return candidates[0]


def verify_captured_pose(
    source_row: dict[str, Any],
    observed_row: dict[str, Any],
    label: str,
    observed_hurtbox_key: str = "opponent_hurtboxes",
    observed_position_key: str = "opponent_fighter_position",
    observed_facing_key: str = "opponent_facing",
) -> None:
    source_pose = canonical_hurt_pose_q16(
        dict(source_row["hitbox_memory"]),
        "fighter_hurtboxes",
        "fighter_position",
        int(source_row["facing"]),
        MELEE_TO_SIM_Q16,
    )
    observed_pose = canonical_hurt_pose_q16(
        dict(observed_row["hitbox_memory"]),
        observed_hurtbox_key,
        observed_position_key,
        int(observed_row[observed_facing_key]),
        MELEE_TO_SIM_Q16,
    )
    if not q16_hurt_poses_equivalent(source_pose, observed_pose):
        raise SystemExit(f"{label} pose does not match captured Falcon pose")


def row_with_pending_fighter_pose(
    observed_row: dict[str, Any], source_row: dict[str, Any]
) -> dict[str, Any]:
    """Install the next displayed fighter pose in an observed collision row."""

    reconstructed = copy.deepcopy(observed_row)
    memory = dict(reconstructed["hitbox_memory"])
    source_memory = dict(source_row["hitbox_memory"])
    target_position = [float(value) for value in memory["fighter_position"]]
    source_position = [
        float(value) for value in source_memory["fighter_position"]
    ]
    target_facing = int(observed_row["facing"])
    source_facing = int(source_row["facing"])
    target_hurtboxes = list(memory["fighter_hurtboxes"])
    source_hurtboxes = list(source_memory["fighter_hurtboxes"])
    if (
        target_facing not in (-1, 1)
        or source_facing not in (-1, 1)
        or len(target_hurtboxes) != len(source_hurtboxes)
    ):
        raise SystemExit("cannot reconstruct pending fighter hurt pose")
    for target_hurtbox, source_hurtbox in zip(
        target_hurtboxes, source_hurtboxes, strict=True
    ):
        for suffix in ("a", "b"):
            source_endpoint = [
                float(value) for value in source_hurtbox[f"position_{suffix}"]
            ]
            local_x = source_facing * (
                source_endpoint[0] - source_position[0]
            )
            local_y = source_endpoint[1] - source_position[1]
            local_z = source_facing * (
                source_endpoint[2] - source_position[2]
            )
            target_hurtbox[f"collision_position_{suffix}"] = [
                target_position[0] + target_facing * local_x,
                target_position[1] + local_y,
                target_position[2] + target_facing * local_z,
            ]
    reconstructed["hitbox_memory"] = memory
    return reconstructed


def reconstructed_collision_margins(
    miss_row: dict[str, Any],
    target_shift_x: float,
    hitbox_key: str = "hitboxes",
    hurtbox_key: str = "opponent_hurtboxes",
) -> tuple[float, float]:
    memory = dict(miss_row["hitbox_memory"])
    hitboxes = [
        dict(hitbox)
        for hitbox in memory[hitbox_key]
        if int(hitbox.get("state", 0)) != 0
    ]
    hurtboxes = active_hurtboxes(memory, hurtbox_key)
    miss_margin = max(
        captured_collision_margin(hitbox, hurtbox, True)
        for hitbox in hitboxes
        for hurtbox in hurtboxes
    )
    reconstructed_hit_hurtboxes = copy.deepcopy(hurtboxes)
    for hurtbox in reconstructed_hit_hurtboxes:
        hurtbox["collision_position_a"][0] += target_shift_x
        hurtbox["collision_position_b"][0] += target_shift_x
    hit_margin = max(
        captured_collision_margin(hitbox, hurtbox, True)
        for hitbox in hitboxes
        for hurtbox in reconstructed_hit_hurtboxes
    )
    return hit_margin, miss_margin


def generic_rectangle_margin(
    memory: dict[str, Any],
    target_shift_x: float,
    hitbox_key: str = "hitboxes",
    target_position_key: str = "opponent_fighter_position",
) -> float:
    target = [float(value) for value in memory[target_position_key]]
    center_x = target[0] + target_shift_x
    center_y = target[1] + 115.0 * 0.8 / 12.0
    half_width = 115.0 * 0.45 / 12.0
    half_height = 115.0 * 0.8 / 12.0
    margins = []
    for hitbox in memory[hitbox_key]:
        if int(hitbox.get("state", 0)) == 0:
            continue
        position = [float(value) for value in hitbox["position"]]
        nearest_x = min(
            max(position[0], center_x - half_width),
            center_x + half_width,
        )
        nearest_y = min(
            max(position[1], center_y - half_height),
            center_y + half_height,
        )
        margins.append(
            float(hitbox["radius"])
            - math.hypot(position[0] - nearest_x, position[1] - nearest_y)
        )
    return max(margins, default=-math.inf)


def active_hitbox_signature_q16(
    row: dict[str, Any],
    hitbox_key: str,
    position_key: str,
    facing_key: str,
) -> tuple[tuple[int, ...], ...]:
    """Return facing-right local executable hit geometry and effect data."""

    memory = dict(row["hitbox_memory"])
    root = [float(value) for value in memory[position_key]]
    facing = int(row[facing_key])
    if facing not in (-1, 1):
        raise SystemExit("collision attacker facing is invalid")

    def local_q16(position: object) -> tuple[int, int, int]:
        if not isinstance(position, list) or len(position) != 3:
            raise SystemExit("collision hitbox position is invalid")
        return (
            round(facing * (float(position[0]) - root[0]) * MELEE_TO_SIM_Q16),
            round((float(position[1]) - root[1]) * MELEE_TO_SIM_Q16),
            round(facing * (float(position[2]) - root[2]) * MELEE_TO_SIM_Q16),
        )

    signatures = []
    for hitbox in memory[hitbox_key]:
        if int(hitbox.get("state", 0)) == 0:
            continue
        current = local_q16(hitbox["position"])
        previous = local_q16(hitbox["previous_position"])
        signatures.append(
            (
                int(hitbox["hit_id"]),
                int(hitbox["state"]),
                round(float(hitbox["damage"]) * 65536.0),
                round(float(hitbox["radius"]) * MELEE_TO_SIM_Q16),
                int(hitbox["angle"]),
                int(hitbox["knockback_growth"]),
                int(hitbox["weight_set_knockback"]),
                int(hitbox["base_knockback"]),
                int(hitbox["element"]),
                *current,
                *previous,
            )
        )
    return tuple(signatures)


def profile_hurt_pose(
    profile: dict[str, Any], track_id: str, displayed_frame: int
) -> tuple[tuple[int, ...], ...]:
    tracks = [track for track in profile["tracks"] if track.get("id") == track_id]
    if len(tracks) != 1:
        raise SystemExit(f"expected one hurt-pose track {track_id!r}")
    frames = [
        frame
        for frame in tracks[0]["frames"]
        if int(frame.get("displayed_frame", -1)) == displayed_frame
    ]
    if len(frames) != 1:
        raise SystemExit(
            f"expected one {track_id} displayed frame {displayed_frame}"
        )
    return tuple(
        tuple(int(value) for value in capsule)
        for capsule in frames[0]["capsules_q16"]
    )


def verify_ledge_collision_discriminator(
    capture_path: Path,
    coverage_path: Path,
    ledge_profile: dict[str, Any],
    qualification: dict[str, Any],
) -> tuple[str, float, float, float]:
    capture = json.loads(capture_path.read_text(encoding="utf-8"))
    coverage_bytes = coverage_path.read_bytes()
    coverage = json.loads(coverage_bytes)
    case_ids = list(qualification.get("case_ids", []))
    if (
        case_ids
        != ["quick_climb_collision_hit", "quick_climb_collision_miss"]
        or qualification.get("target_track") != "ledge_climb_quick"
        or qualification.get("target_displayed_frame") != 29
        or qualification.get("attacker_action") != "NEUTRAL_ATTACK_1"
        or qualification.get("attacker_displayed_frame") != 4
        or qualification.get("positive_damage_action") != "DAMAGE_NEUTRAL_2"
        or qualification.get("expected_rows") != 143
        or qualification.get("shards") != 2
        or qualification.get("warm_budget_seconds") != 8.0
        or qualification.get("cold_budget_seconds") != 9.0
    ):
        raise SystemExit("invalid ledge live-collision qualification manifest")
    expected_manifest = projected_manifest(
        coverage,
        case_ids,
        hashlib.sha256(coverage_bytes).hexdigest(),
        int(qualification["shards"]),
        float(qualification["warm_budget_seconds"]),
        float(qualification["cold_budget_seconds"]),
    )
    checkpoint_pack = capture.get("checkpoint_pack", {})
    if (
        capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "HYRULE_TEMPLE"
        or capture.get("disc", {}).get("sha256") != EXPECTED_DISC_SHA256
        or capture.get("dolphin_version") != "3.5.1"
        or capture.get("libmelee_version") != "0.47.2"
        or capture.get("oracle_execution", {}).get("release_artifact_sha256")
        != EXPECTED_EXIAI_SHA256
        or capture.get("hitbox_memory_probe", {}).get("decomp_revision")
        != EXPECTED_DECOMP_REVISION
        or checkpoint_pack.get("coverage_manifest") != expected_manifest
        or capture.get("parallel_capture", {}).get("case_order") != case_ids
        or len(capture.get("rows", [])) != qualification["expected_rows"]
        or sha256(capture_path) != qualification.get("capture_sha256")
    ):
        raise SystemExit("unexpected ledge collision capture provenance")

    rows = list(capture.get("rows", []))
    positive = [
        row
        for row in rows
        if "quick_climb_collision_hit" in str(row.get("label", ""))
    ]
    negative = [
        row
        for row in rows
        if "quick_climb_collision_miss" in str(row.get("label", ""))
    ]
    if (
        not positive
        or not negative
        or not math.isclose(float(positive[0]["damage_percent"]), 60.0)
        or not math.isclose(float(positive[-1]["damage_percent"]), 62.0)
        or not any(row.get("action") == "DAMAGE_NEUTRAL_2" for row in positive)
        or any(
            not math.isclose(float(row["damage_percent"]), 60.0)
            for row in negative
        )
        or any(
            str(row.get("action", "")).startswith("DAMAGE_NEUTRAL")
            for row in negative
        )
    ):
        raise SystemExit("ledge collision hit/miss outcome mismatch")

    miss_frame = collision_frame(
        negative,
        4,
        "EDGE_GETUP_QUICK",
        29,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    expected_target_pose = profile_hurt_pose(
        ledge_profile, "ledge_climb_quick", 29
    )
    observed_target_pose = canonical_hurt_pose_q16(
        dict(miss_frame["hitbox_memory"]),
        "fighter_hurtboxes",
        "fighter_position",
        int(miss_frame["facing"]),
        MELEE_TO_SIM_Q16,
    )
    if not q16_hurt_poses_equivalent(expected_target_pose, observed_target_pose):
        raise SystemExit("ledge collision target does not match imported frame 29")

    expected_hitboxes = tuple(
        tuple(int(value) for value in hitbox)
        for hitbox in qualification.get("attacker_hitboxes_q16", [])
    )
    observed_hitboxes = active_hitbox_signature_q16(
        miss_frame,
        "opponent_hitboxes",
        "opponent_fighter_position",
        "opponent_facing",
    )
    if len(expected_hitboxes) != 3 or expected_hitboxes != observed_hitboxes:
        raise SystemExit(
            "ledge collision port-2 attacker does not match Falcon Jab 1 geometry"
        )

    requested_positions: dict[str, float] = {}
    for case_id, case_rows in ((case_ids[0], positive), (case_ids[1], negative)):
        positions = {
            float(row["requested_opponent_x_override"])
            for row in case_rows
            if row.get("requested_opponent_x_override") is not None
        }
        if len(positions) != 1:
            raise SystemExit(f"{case_id} has invalid attacker placement {positions}")
        requested_positions[case_id] = next(iter(positions))
    target_shift_x = (
        requested_positions[case_ids[1]] - requested_positions[case_ids[0]]
    )
    hit_margin, miss_margin = reconstructed_collision_margins(
        miss_frame,
        target_shift_x,
        hitbox_key="opponent_hitboxes",
        hurtbox_key="fighter_hurtboxes",
    )
    generic_margin = generic_rectangle_margin(
        dict(miss_frame["hitbox_memory"]),
        target_shift_x,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if hit_margin < 0.0 or miss_margin >= 0.0 or generic_margin >= 0.0:
        raise SystemExit(
            "ledge collision discriminator failed: "
            f"hit_margin={hit_margin:.9f} miss_margin={miss_margin:.9f} "
            f"generic_margin={generic_margin:.9f}"
        )

    semantic_payload = {
        "schema": 1,
        "case_ids": case_ids,
        "initial_damage_q16": round(60.0 * 65536.0),
        "positive_damage_q16": round(62.0 * 65536.0),
        "positive_action": "DAMAGE_NEUTRAL_2",
        "negative_action": "EDGE_GETUP_QUICK",
        "target_action": "EDGE_GETUP_QUICK",
        "target_displayed_frame": 29,
        "attacker_action": "NEUTRAL_ATTACK_1",
        "attacker_displayed_frame": 4,
        "attacker_positions_q16": [
            round(requested_positions[case_id] * MELEE_TO_SIM_Q16)
            for case_id in case_ids
        ],
        "target_pose_q16": expected_target_pose,
        "attacker_hitboxes_q16": observed_hitboxes,
    }
    semantic_sha256 = canonical_json_sha256(semantic_payload)
    if semantic_sha256 != qualification.get("semantic_sha256"):
        raise SystemExit(
            "unexpected ledge collision semantic SHA-256: "
            f"{semantic_sha256}"
        )
    return (
        semantic_sha256,
        hit_margin,
        miss_margin,
        generic_margin,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("collision_source", type=Path)
    parser.add_argument("dash_source", type=Path)
    parser.add_argument("crouch_source", type=Path)
    parser.add_argument("knee_source", type=Path)
    parser.add_argument("escape_source", type=Path)
    parser.add_argument("air_escape_source", type=Path)
    parser.add_argument("fall_special_source", type=Path)
    parser.add_argument("landing_source", type=Path)
    parser.add_argument("--checkpoint-pack", action="store_true")
    parser.add_argument(
        "--additional-hurt-profile",
        action="append",
        type=Path,
    )
    parser.add_argument("--ledge-collision-capture", type=Path)
    parser.add_argument("--ledge-coverage-manifest", type=Path)
    parser.add_argument("--ledge-import-manifest", type=Path)
    args = parser.parse_args()
    if args.additional_hurt_profile is not None and not args.checkpoint_pack:
        parser.error("--additional-hurt-profile requires --checkpoint-pack")
    ledge_inputs = (
        args.ledge_collision_capture,
        args.ledge_coverage_manifest,
        args.ledge_import_manifest,
    )
    if any(value is None for value in ledge_inputs) and any(
        value is not None for value in ledge_inputs
    ):
        parser.error(
            "ledge collision capture, coverage, and import manifest must be "
            "provided together"
        )
    if args.ledge_collision_capture is not None and (
        not args.checkpoint_pack or args.additional_hurt_profile is None
    ):
        parser.error(
            "ledge collision verification requires the checkpoint pack and "
            "additional hurt profile"
        )

    capture_digest = sha256(args.capture)
    if not args.checkpoint_pack and capture_digest != EXPECTED_CAPTURE_SHA256:
        raise SystemExit(
            f"unexpected common-hurt capture SHA-256: {capture_digest}"
        )
    if sha256(args.collision_source) != EXPECTED_COLLISION_SOURCE_SHA256:
        raise SystemExit("unexpected lbcollision.c SHA-256")
    if sha256(args.dash_source) != EXPECTED_DASH_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_Dash.c SHA-256")
    if sha256(args.crouch_source) != EXPECTED_CROUCH_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_Squat.c SHA-256")
    if sha256(args.knee_source) != EXPECTED_KNEE_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_KneeBend.c SHA-256")
    if sha256(args.escape_source) != EXPECTED_ESCAPE_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_Escape.c SHA-256")
    if sha256(args.air_escape_source) != EXPECTED_AIR_ESCAPE_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_EscapeAir.c SHA-256")
    if (
        sha256(args.fall_special_source)
        != EXPECTED_FALL_SPECIAL_SOURCE_SHA256
    ):
        raise SystemExit("unexpected ftCo_FallSpecial.c SHA-256")
    if sha256(args.landing_source) != EXPECTED_LANDING_SOURCE_SHA256:
        raise SystemExit("unexpected ftCo_Landing.c SHA-256")

    capture: dict[str, Any] = json.loads(
        args.capture.read_text(encoding="utf-8")
    )
    rows: list[dict[str, Any]] = list(capture.get("rows", []))
    probe = dict(capture.get("hitbox_memory_probe", {}))
    disc = dict(capture.get("disc", {}))
    execution = dict(capture.get("oracle_execution", {}))
    checkpoint_pack = dict(capture.get("checkpoint_pack", {}))
    expected_coverage_manifest = json.loads(
        Path(__file__)
        .with_name("ssbm_falcon_common_hurt_coverage.json")
        .read_text(encoding="utf-8")
    )
    expected_checkpoint_pack = dict(
        expected_coverage_manifest["checkpoint_pack"]
    )
    additional_hurt_profiles: list[dict[str, Any]] = []
    if args.additional_hurt_profile is not None:
        declarations = expected_coverage_manifest["stored_oracle"].get(
            "pose_profiles", []
        )
        if (
            not isinstance(declarations, list)
            or len(declarations) != len(args.additional_hurt_profile)
        ):
            raise SystemExit("unexpected stored hurt-pose profile declaration")
        declarations_by_digest = {
            str(declaration["profile_sha256"]): declaration
            for declaration in declarations
        }
        supplied_profiles: dict[str, dict[str, Any]] = {}
        for profile_path in args.additional_hurt_profile:
            profile_bytes = profile_path.read_bytes()
            profile_digest = hashlib.sha256(profile_bytes).hexdigest()
            declaration = declarations_by_digest.get(profile_digest)
            profile = json.loads(profile_bytes)
            profile_tracks = profile.get("tracks")
            if (
                declaration is None
                or profile_digest in supplied_profiles
                or profile.get("schema") != 1
                or profile.get("scope")
                != "ssbm-bounded-hurt-pose-tracks"
                or profile.get("fighter") != "CPTFALCON"
                or profile.get("capture_sha256")
                != declaration.get("capture_sha256")
                or not isinstance(profile_tracks, list)
            ):
                raise SystemExit(
                    "unexpected additional hurt-pose profile provenance"
                )
            semantic_digest = canonical_json_sha256(
                hurt_pose_tracks_semantic_payload(profile_tracks)
            )
            if (
                semantic_digest != declaration.get("semantic_sha256")
                or semantic_digest != profile.get("semantic_sha256")
            ):
                raise SystemExit(
                    "unexpected additional hurt-pose semantic SHA-256: "
                    f"{semantic_digest}"
                )
            supplied_profiles[profile_digest] = profile
        additional_hurt_profiles = [
            supplied_profiles[str(declaration["profile_sha256"])]
            for declaration in declarations
        ]
    expected_case_labels = [
        str(case["start_label"])
        for case in expected_coverage_manifest["checkpoint_cases"]
    ]
    if (
        capture.get("schema") != 9
        or capture.get("fighter") != "CPTFALCON"
        or capture.get("opponent") != "CPTFALCON"
        or capture.get("stage") != "FINAL_DESTINATION"
        or capture.get("dolphin_version") != "3.5.1"
        or capture.get("libmelee_version") != "0.47.2"
        or execution.get("mode") != "exiai-headless-null-fast-forward"
        or execution.get("release") != "exi-ai-0.2.0"
        or execution.get("release_artifact_sha256") != EXPECTED_EXIAI_SHA256
        or capture.get("common_hurt_geometry_route") is not True
        or disc.get("game_id") != "GALE01"
        or disc.get("revision") != 2
        or disc.get("sha256") != EXPECTED_DISC_SHA256
        or probe.get("decomp_revision") != EXPECTED_DECOMP_REVISION
        or len(rows)
        != (
            int(expected_checkpoint_pack["expected_rows"])
            if args.checkpoint_pack
            else 4198
        )
        or (
            args.checkpoint_pack
            and (
                checkpoint_pack.get("protocol")
                != expected_checkpoint_pack["protocol"]
                or checkpoint_pack.get("case_count") != len(expected_case_labels)
                or checkpoint_pack.get("case_start_labels")
                != expected_case_labels
                or checkpoint_capture_contract(
                    dict(checkpoint_pack.get("coverage_manifest", {}))
                )
                != checkpoint_capture_contract(expected_coverage_manifest)
            )
        )
    ):
        raise SystemExit("unexpected common-hurt capture provenance")

    dash_rows = exact_action_frames(
        rows,
        "common_hurt_dash_hold",
        "DASHING",
        1,
        15,
    )
    run_brake_rows = exact_action_frames(
        rows,
        "common_hurt_dash_recover",
        "RUN_BRAKE",
        1,
        28,
    )
    crouch_start_rows = exact_action_frames(
        rows,
        "common_hurt_crouch_hold",
        "CROUCH_START",
        1,
        7,
    )
    crouch_end_rows = exact_action_frames(
        rows,
        "common_hurt_crouch_release",
        "CROUCH_END",
        1,
        10,
    )
    knee_bend_rows = exact_action_frames(
        rows,
        "common_hurt_knee_bend_hold",
        "KNEE_BEND",
        1,
        4,
    )
    landing_rows = exact_action_frames(
        rows,
        "common_hurt_knee_bend_recover",
        "LANDING",
        1,
        30,
    )
    spot_dodge_rows = exact_action_frames(
        rows,
        "common_hurt_spot_dodge_hold",
        "SPOTDODGE",
        1,
        32,
    )
    roll_forward_rows = exact_action_frames(
        rows,
        "common_hurt_roll_forward_hold",
        "ROLL_FORWARD",
        1,
        31,
    )
    roll_backward_rows = exact_action_frames(
        rows,
        "common_hurt_roll_backward_hold",
        "ROLL_BACKWARD",
        1,
        31,
    )
    air_dodge_rows = exact_action_frames(
        rows,
        (
            "common_hurt_air_dodge_entry",
            "common_hurt_air_dodge_hold",
        ),
        "AIRDODGE",
        1,
        49,
    )
    fall_special_rows = exact_looping_action_frames(
        rows,
        (
            "common_hurt_air_dodge_hold",
            "common_hurt_air_dodge_recover",
        ),
        "DEAD_FALL",
        8,
    )
    landing_fall_special_rows = exact_action_frame_sequence(
        rows,
        "common_hurt_air_dodge_recover",
        "LANDING_SPECIAL",
        tuple(range(1, 29, 3)),
    )
    if any(
        float(row["damage_percent"]) != 0.0
        for row in fall_special_rows + landing_fall_special_rows
    ):
        raise SystemExit(
            "FallSpecial/LandingFallSpecial source route changed damage"
        )
    observed_invulnerability = [
        round(float(row["action_frame"]))
        for row in spot_dodge_rows
        if bool(row["invulnerable"])
    ]
    if observed_invulnerability != list(range(3, 21)):
        raise SystemExit(
            "SpotDodge invulnerability mismatch: "
            f"{observed_invulnerability} != {list(range(3, 21))}"
        )
    for action, action_rows in (
        ("ROLL_FORWARD", roll_forward_rows),
        ("ROLL_BACKWARD", roll_backward_rows),
    ):
        observed_invulnerability = [
            round(float(row["action_frame"]))
            for row in action_rows
            if bool(row["invulnerable"])
        ]
        if observed_invulnerability != list(range(4, 20)):
            raise SystemExit(
                f"{action} invulnerability mismatch: "
                f"{observed_invulnerability} != {list(range(4, 20))}"
            )
    observed_invulnerability = [
        round(float(row["action_frame"]))
        for row in air_dodge_rows
        if bool(row["invulnerable"])
    ]
    if observed_invulnerability != list(range(4, 30)):
        raise SystemExit(
            "AirDodge invulnerability mismatch: "
            f"{observed_invulnerability} != {list(range(4, 30))}"
        )

    if args.checkpoint_pack:
        canonical_poses = []
        for action, action_rows in (
            ("DASHING", dash_rows),
            ("RUN_BRAKE", run_brake_rows),
            ("CROUCH_START", crouch_start_rows),
            ("CROUCH_END", crouch_end_rows),
            ("KNEE_BEND", knee_bend_rows),
            ("SPOTDODGE", spot_dodge_rows),
            ("ROLL_FORWARD", roll_forward_rows),
            ("ROLL_BACKWARD", roll_backward_rows),
            ("AIRDODGE", air_dodge_rows),
            ("DEAD_FALL", fall_special_rows),
            ("LANDING_SPECIAL", landing_fall_special_rows),
            ("LANDING", landing_rows),
        ):
            canonical_poses.extend(
                (
                    action,
                    round(float(row["action_frame"])),
                    canonical_hurt_pose_q16(
                        dict(row["hitbox_memory"]),
                        "fighter_hurtboxes",
                        "fighter_position",
                        int(row["facing"]),
                        MELEE_TO_SIM_Q16,
                    ),
                )
                for row in action_rows
            )
        for additional_hurt_profile in additional_hurt_profiles:
            for profile_track in additional_hurt_profile["tracks"]:
                action = str(profile_track["source_action"])
                canonical_poses.extend(
                    (
                        action,
                        int(frame["displayed_frame"]),
                        tuple(
                            tuple(int(value) for value in capsule)
                            for capsule in frame["capsules_q16"]
                        ),
                    )
                    for frame in profile_track["frames"]
                )
        pose_digest = hashlib.sha256(
            json.dumps(
                canonical_poses,
                separators=(",", ":"),
            ).encode("utf-8")
        ).hexdigest()
        expected_pose_digest = (
            str(expected_coverage_manifest["stored_oracle"]["source_pose_sha256"])
            if additional_hurt_profiles
            else EXPECTED_CHECKPOINT_POSE_SHA256
        )
        if pose_digest != expected_pose_digest:
            raise SystemExit(
                f"unexpected checkpoint pose SHA-256: {pose_digest}"
            )
    positive, negative = verify_collision_outcome(
        rows,
        "dash",
        0.0,
        2.0,
        "DAMAGE_HIGH_2",
        negative_damage=0.0 if args.checkpoint_pack else None,
    )

    dash_miss_frame = collision_frame(negative, 5, "DASHING", 5)
    verify_captured_pose(dash_rows[4], dash_miss_frame, "port-2 Dash frame 5")
    dash_target_shift = (
        requested_route_distance(positive)
        - requested_route_distance(negative)
    )
    dash_hit_margin, dash_miss_margin = reconstructed_collision_margins(
        dash_miss_frame, dash_target_shift
    )
    dash_generic_margin = generic_rectangle_margin(
        dict(dash_miss_frame["hitbox_memory"]), dash_target_shift
    )
    if (
        dash_hit_margin < 0.0
        or dash_miss_margin >= 0.0
        or dash_generic_margin >= 0.0
    ):
        raise SystemExit(
            "dash collision discriminator failed: "
            f"hit_margin={dash_hit_margin:.9f} "
            f"miss_margin={dash_miss_margin:.9f} "
            f"generic_margin={dash_generic_margin:.9f}"
        )

    if args.checkpoint_pack:
        ledge_collision_result: tuple[str, float, float, float] | None = None
        if args.ledge_collision_capture is not None:
            if not additional_hurt_profiles:
                raise SystemExit("ledge collision profile was not loaded")
            ledge_hurt_profile = next(
                (
                    profile
                    for profile in additional_hurt_profiles
                    if any(
                        str(track.get("source_action", "")).startswith("EDGE_")
                        for track in profile["tracks"]
                    )
                ),
                None,
            )
            if ledge_hurt_profile is None:
                raise SystemExit("ledge collision profile was not loaded")
            ledge_collision_result = verify_ledge_collision_discriminator(
                args.ledge_collision_capture,
                args.ledge_coverage_manifest,
                ledge_hurt_profile,
                dict(
                    json.loads(
                        args.ledge_import_manifest.read_text(encoding="utf-8")
                    )["live_collision_qualification"]
                ),
            )
        print(
            "ssbm-common-hurt-checkpoint=pass "
            f"rows={len(rows)} poses={len(canonical_poses)} "
            f"dash_hit_margin={dash_hit_margin:.9f} "
            f"dash_miss_margin={dash_miss_margin:.9f} "
            f"pose_sha256={pose_digest}"
            + (
                " ledge_collision_sha256="
                f"{ledge_collision_result[0]} "
                f"ledge_hit_margin={ledge_collision_result[1]:.9f} "
                f"ledge_miss_margin={ledge_collision_result[2]:.9f} "
                f"ledge_generic_margin={ledge_collision_result[3]:.9f}"
                if ledge_collision_result is not None
                else ""
            )
        )
        return 0

    crouch_positive, crouch_negative = verify_collision_outcome(
        rows,
        "crouch",
        2.0,
        3.819999933242798,
        "DAMAGE_NEUTRAL_1",
    )

    crouch_miss_frame = collision_frame(
        crouch_negative, 3, "CROUCH_START", 3
    )
    verify_captured_pose(
        crouch_start_rows[2], crouch_miss_frame, "port-2 CrouchStart frame 3"
    )
    crouch_target_shift = (
        requested_route_distance(crouch_positive)
        - requested_route_distance(crouch_negative)
    )
    crouch_hit_margin, crouch_miss_margin = reconstructed_collision_margins(
        crouch_miss_frame, crouch_target_shift
    )
    crouch_generic_margin = generic_rectangle_margin(
        dict(crouch_miss_frame["hitbox_memory"]), 0.0
    )
    if (
        crouch_hit_margin < 0.0
        or crouch_miss_margin >= 0.0
        or crouch_generic_margin < 0.0
    ):
        raise SystemExit(
            "crouch collision discriminator failed: "
            f"hit_margin={crouch_hit_margin:.9f} "
            f"miss_margin={crouch_miss_margin:.9f} "
            f"generic_margin={crouch_generic_margin:.9f}"
        )

    knee_positive, knee_negative = verify_collision_outcome(
        rows,
        "knee_bend",
        3.819999933242798,
        5.480000019073486,
        "DAMAGE_HIGH_2",
    )
    knee_miss_frame = collision_frame(
        knee_negative, 3, "KNEE_BEND", 2
    )
    verify_captured_pose(
        knee_bend_rows[1], knee_miss_frame, "port-2 KneeBend frame 2"
    )
    knee_target_shift = (
        requested_route_distance(knee_positive)
        - requested_route_distance(knee_negative)
    )
    knee_hit_margin, knee_miss_margin = reconstructed_collision_margins(
        knee_miss_frame, knee_target_shift
    )
    knee_generic_margin = generic_rectangle_margin(
        dict(knee_miss_frame["hitbox_memory"]), 0.0
    )
    if (
        knee_hit_margin < 0.0
        or knee_miss_margin >= 0.0
        or knee_generic_margin < 0.0
    ):
        raise SystemExit(
            "KneeBend collision discriminator failed: "
            f"hit_margin={knee_hit_margin:.9f} "
            f"miss_margin={knee_miss_margin:.9f} "
            f"generic_margin={knee_generic_margin:.9f}"
        )

    spot_positive, spot_negative = verify_collision_outcome(
        rows,
        "spot_dodge",
        0.0,
        2.0,
        "DAMAGE_NEUTRAL_2",
        target_prefix="",
    )
    spot_miss_frame = collision_frame(
        spot_negative,
        4,
        "SPOTDODGE",
        23,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        spot_dodge_rows[22],
        spot_miss_frame,
        "port-1 SpotDodge frame 23",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    spot_target_shift = (
        requested_route_distance(
            spot_positive, target="fighter", attacker="opponent"
        )
        - requested_route_distance(
            spot_negative, target="fighter", attacker="opponent"
        )
    )
    # The post-frame observer reports the collision on the row after Melee has
    # evaluated SpotDodge frame 24.  Install that pending pose into the frame-23
    # observation before reconstructing the frame-4 Jab check.
    spot_pending_collision_frame = row_with_pending_fighter_pose(
        spot_miss_frame, spot_dodge_rows[23]
    )
    spot_hit_margin, spot_miss_margin = reconstructed_collision_margins(
        spot_pending_collision_frame,
        spot_target_shift,
        hitbox_key="opponent_hitboxes",
        hurtbox_key="fighter_hurtboxes",
    )
    spot_generic_margin = generic_rectangle_margin(
        dict(spot_miss_frame["hitbox_memory"]),
        0.0,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        spot_hit_margin < 0.0
        or spot_miss_margin >= 0.0
        or spot_generic_margin >= 0.0
    ):
        raise SystemExit(
            "SpotDodge collision discriminator failed: "
            f"hit_margin={spot_hit_margin:.9f} "
            f"miss_margin={spot_miss_margin:.9f} "
            f"generic_margin={spot_generic_margin:.9f}"
        )

    roll_forward_positive, roll_forward_negative = verify_collision_outcome(
        rows,
        "roll_forward",
        2.0,
        3.819999933242798,
        "DAMAGE_NEUTRAL_2",
        target_prefix="",
    )
    roll_forward_miss_frame = collision_frame(
        roll_forward_negative,
        3,
        "ROLL_FORWARD",
        22,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        roll_forward_rows[21],
        roll_forward_miss_frame,
        "port-1 RollForward frame 22",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    roll_forward_target_shift = (
        requested_route_distance(
            roll_forward_positive, target="fighter", attacker="opponent"
        )
        - requested_route_distance(
            roll_forward_negative, target="fighter", attacker="opponent"
        )
    )
    roll_forward_hit_margin, roll_forward_miss_margin = (
        reconstructed_collision_margins(
            roll_forward_miss_frame,
            roll_forward_target_shift,
            hitbox_key="opponent_hitboxes",
            hurtbox_key="fighter_hurtboxes",
        )
    )
    roll_forward_generic_margin = generic_rectangle_margin(
        dict(roll_forward_miss_frame["hitbox_memory"]),
        0.0,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        roll_forward_hit_margin < 0.0
        or roll_forward_miss_margin >= 0.0
        or roll_forward_generic_margin < 0.0
    ):
        raise SystemExit(
            "RollForward collision discriminator failed: "
            f"hit_margin={roll_forward_hit_margin:.9f} "
            f"miss_margin={roll_forward_miss_margin:.9f} "
            f"generic_margin={roll_forward_generic_margin:.9f}"
        )

    roll_backward_positive, roll_backward_negative = verify_collision_outcome(
        rows,
        "roll_backward",
        3.819999933242798,
        5.480000019073486,
        "DAMAGE_NEUTRAL_2",
        target_prefix="",
    )
    roll_backward_miss_frame = collision_frame(
        roll_backward_negative,
        5,
        "ROLL_BACKWARD",
        24,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        roll_backward_rows[23],
        roll_backward_miss_frame,
        "port-1 RollBackward frame 24",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    roll_backward_target_shift = (
        requested_route_distance(
            roll_backward_positive, target="fighter", attacker="opponent"
        )
        - requested_route_distance(
            roll_backward_negative, target="fighter", attacker="opponent"
        )
    )
    roll_backward_hit_margin, roll_backward_miss_margin = (
        reconstructed_collision_margins(
            roll_backward_miss_frame,
            roll_backward_target_shift,
            hitbox_key="opponent_hitboxes",
            hurtbox_key="fighter_hurtboxes",
        )
    )
    roll_backward_generic_margin = generic_rectangle_margin(
        dict(roll_backward_miss_frame["hitbox_memory"]),
        roll_backward_target_shift,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        roll_backward_hit_margin < 0.0
        or roll_backward_miss_margin >= 0.0
        or roll_backward_generic_margin >= 0.0
    ):
        raise SystemExit(
            "RollBackward collision discriminator failed: "
            f"hit_margin={roll_backward_hit_margin:.9f} "
            f"miss_margin={roll_backward_miss_margin:.9f} "
            f"generic_margin={roll_backward_generic_margin:.9f}"
        )

    air_dodge_positive, air_dodge_negative = verify_collision_outcome(
        rows,
        "air_dodge",
        5.480000019073486,
        7.0,
        "DAMAGE_AIR_2",
        target_prefix="",
        negative_damage=5.480000019073486,
    )
    air_dodge_miss_frame = collision_frame(
        air_dodge_negative,
        3,
        "AIRDODGE",
        31,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        air_dodge_rows[30],
        air_dodge_miss_frame,
        "port-1 AirDodge frame 31",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    air_dodge_target_shift = (
        requested_route_distance(
            air_dodge_positive,
            target="fighter",
            attacker="opponent",
            placement_suffix="_offstage_place",
        )
        - requested_route_distance(
            air_dodge_negative,
            target="fighter",
            attacker="opponent",
            placement_suffix="_offstage_place",
        )
    )
    air_dodge_hit_margin, air_dodge_miss_margin = (
        reconstructed_collision_margins(
            air_dodge_miss_frame,
            air_dodge_target_shift,
            hitbox_key="opponent_hitboxes",
            hurtbox_key="fighter_hurtboxes",
        )
    )
    air_dodge_generic_margin = generic_rectangle_margin(
        dict(air_dodge_miss_frame["hitbox_memory"]),
        air_dodge_target_shift,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        air_dodge_hit_margin < 0.0
        or air_dodge_miss_margin >= 0.0
        or air_dodge_generic_margin >= 0.0
    ):
        raise SystemExit(
            "AirDodge collision discriminator failed: "
            f"hit_margin={air_dodge_hit_margin:.9f} "
            f"miss_margin={air_dodge_miss_margin:.9f} "
            f"generic_margin={air_dodge_generic_margin:.9f}"
        )

    fall_special_positive, fall_special_negative = verify_collision_outcome(
        rows,
        "fall_special",
        7.0,
        8.399999618530273,
        "DAMAGE_AIR_2",
        target_prefix="",
        negative_damage=7.0,
    )
    fall_special_miss_frame = collision_frame(
        fall_special_negative,
        3,
        "DEAD_FALL",
        4,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        fall_special_rows[3],
        fall_special_miss_frame,
        "port-1 FallSpecial frame 4",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    fall_special_target_shift = (
        requested_route_distance(
            fall_special_positive,
            target="fighter",
            attacker="opponent",
            placement_suffix="_jab_start",
        )
        - requested_route_distance(
            fall_special_negative,
            target="fighter",
            attacker="opponent",
            placement_suffix="_jab_start",
        )
    )
    fall_special_pending_frame = row_with_pending_fighter_pose(
        fall_special_miss_frame, fall_special_rows[4]
    )
    fall_special_hit_margin, fall_special_miss_margin = (
        reconstructed_collision_margins(
            fall_special_pending_frame,
            fall_special_target_shift,
            hitbox_key="opponent_hitboxes",
            hurtbox_key="fighter_hurtboxes",
        )
    )
    fall_special_generic_margin = generic_rectangle_margin(
        dict(fall_special_pending_frame["hitbox_memory"]),
        fall_special_target_shift,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        fall_special_hit_margin < 0.0
        or fall_special_miss_margin >= 0.0
        or fall_special_generic_margin < 0.0
    ):
        raise SystemExit(
            "FallSpecial collision discriminator failed: "
            f"hit_margin={fall_special_hit_margin:.9f} "
            f"miss_margin={fall_special_miss_margin:.9f} "
            f"generic_margin={fall_special_generic_margin:.9f}"
        )

    landing_positive, landing_negative = verify_collision_outcome(
        rows,
        "landing_fall_special",
        8.399999618530273,
        9.699999809265137,
        "DAMAGE_NEUTRAL_2",
        target_prefix="",
        negative_damage=8.399999618530273,
    )
    landing_miss_frame = collision_frame(
        landing_negative,
        3,
        "LANDING_SPECIAL",
        4,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        landing_fall_special_rows[1],
        landing_miss_frame,
        "port-1 LandingFallSpecial frame 4",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    landing_target_shift = (
        requested_route_distance(
            landing_positive,
            target="fighter",
            attacker="opponent",
        )
        - requested_route_distance(
            landing_negative,
            target="fighter",
            attacker="opponent",
        )
    )
    landing_pending_frame = row_with_pending_fighter_pose(
        landing_miss_frame, landing_fall_special_rows[2]
    )
    landing_hit_margin, landing_miss_margin = (
        reconstructed_collision_margins(
            landing_pending_frame,
            landing_target_shift,
            hitbox_key="opponent_hitboxes",
            hurtbox_key="fighter_hurtboxes",
        )
    )
    landing_generic_margin = generic_rectangle_margin(
        dict(landing_pending_frame["hitbox_memory"]),
        landing_target_shift,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        landing_hit_margin < 0.0
        or landing_miss_margin >= 0.0
        or landing_generic_margin >= 0.0
    ):
        raise SystemExit(
            "LandingFallSpecial collision discriminator failed: "
            f"hit_margin={landing_hit_margin:.9f} "
            f"miss_margin={landing_miss_margin:.9f} "
            f"generic_margin={landing_generic_margin:.9f}"
        )

    normal_landing_positive, normal_landing_negative = (
        verify_collision_outcome(
            rows,
            "landing",
            9.699999809265137,
            10.920000076293945,
            "DAMAGE_NEUTRAL_2",
            target_prefix="",
            negative_damage=9.699999809265137,
        )
    )
    normal_landing_miss_frame = collision_frame(
        normal_landing_negative,
        3,
        "LANDING",
        21,
        attacker_prefix="opponent_",
        target_prefix="",
    )
    verify_captured_pose(
        landing_rows[20],
        normal_landing_miss_frame,
        "port-1 Landing frame 21",
        observed_hurtbox_key="fighter_hurtboxes",
        observed_position_key="fighter_position",
        observed_facing_key="facing",
    )
    normal_landing_target_shift = (
        requested_route_distance(
            normal_landing_positive,
            target="fighter",
            attacker="opponent",
        )
        - requested_route_distance(
            normal_landing_negative,
            target="fighter",
            attacker="opponent",
        )
    )
    normal_landing_pending_frame = row_with_pending_fighter_pose(
        normal_landing_miss_frame, landing_rows[21]
    )
    normal_landing_hit_margin, normal_landing_miss_margin = (
        reconstructed_collision_margins(
            normal_landing_pending_frame,
            normal_landing_target_shift,
            hitbox_key="opponent_hitboxes",
            hurtbox_key="fighter_hurtboxes",
        )
    )
    normal_landing_generic_margin = generic_rectangle_margin(
        dict(normal_landing_pending_frame["hitbox_memory"]),
        normal_landing_target_shift,
        hitbox_key="opponent_hitboxes",
        target_position_key="fighter_position",
    )
    if (
        normal_landing_hit_margin < 0.0
        or normal_landing_miss_margin >= 0.0
        or normal_landing_generic_margin >= 0.0
    ):
        raise SystemExit(
            "Landing collision discriminator failed: "
            f"hit_margin={normal_landing_hit_margin:.9f} "
            f"miss_margin={normal_landing_miss_margin:.9f} "
            f"generic_margin={normal_landing_generic_margin:.9f}"
        )

    print(
        "ssbm-common-hurt=pass "
        f"frames={len(rows)} dash_frames={len(dash_rows)} "
        f"run_brake_frames={len(run_brake_rows)} "
        f"crouch_start_frames={len(crouch_start_rows)} "
        f"crouch_end_frames={len(crouch_end_rows)} "
        f"knee_bend_frames={len(knee_bend_rows)} "
        f"landing_frames={len(landing_rows)} "
        f"spot_dodge_frames={len(spot_dodge_rows)} "
        f"roll_forward_frames={len(roll_forward_rows)} "
        f"roll_backward_frames={len(roll_backward_rows)} "
        f"air_dodge_frames={len(air_dodge_rows)} "
        f"fall_special_frames={len(fall_special_rows)} "
        f"landing_fall_special_frames={len(landing_fall_special_rows)} "
        f"dash_hit_margin={dash_hit_margin:.9f} "
        f"dash_miss_margin={dash_miss_margin:.9f} "
        f"dash_generic_margin={dash_generic_margin:.9f} "
        f"crouch_hit_margin={crouch_hit_margin:.9f} "
        f"crouch_miss_margin={crouch_miss_margin:.9f} "
        f"crouch_generic_margin={crouch_generic_margin:.9f} "
        f"knee_hit_margin={knee_hit_margin:.9f} "
        f"knee_miss_margin={knee_miss_margin:.9f} "
        f"knee_generic_margin={knee_generic_margin:.9f} "
        f"spot_hit_margin={spot_hit_margin:.9f} "
        f"spot_miss_margin={spot_miss_margin:.9f} "
        f"spot_generic_margin={spot_generic_margin:.9f} "
        f"roll_forward_hit_margin={roll_forward_hit_margin:.9f} "
        f"roll_forward_miss_margin={roll_forward_miss_margin:.9f} "
        f"roll_forward_generic_margin={roll_forward_generic_margin:.9f} "
        f"roll_backward_hit_margin={roll_backward_hit_margin:.9f} "
        f"roll_backward_miss_margin={roll_backward_miss_margin:.9f} "
        f"roll_backward_generic_margin={roll_backward_generic_margin:.9f} "
        f"air_dodge_hit_margin={air_dodge_hit_margin:.9f} "
        f"air_dodge_miss_margin={air_dodge_miss_margin:.9f} "
        f"air_dodge_generic_margin={air_dodge_generic_margin:.9f} "
        f"fall_special_hit_margin={fall_special_hit_margin:.9f} "
        f"fall_special_miss_margin={fall_special_miss_margin:.9f} "
        f"fall_special_generic_margin={fall_special_generic_margin:.9f} "
        f"landing_fall_special_hit_margin={landing_hit_margin:.9f} "
        f"landing_fall_special_miss_margin={landing_miss_margin:.9f} "
        f"landing_fall_special_generic_margin={landing_generic_margin:.9f} "
        f"landing_hit_margin={normal_landing_hit_margin:.9f} "
        f"landing_miss_margin={normal_landing_miss_margin:.9f} "
        f"landing_generic_margin={normal_landing_generic_margin:.9f} "
        f"capture_sha256={capture_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
