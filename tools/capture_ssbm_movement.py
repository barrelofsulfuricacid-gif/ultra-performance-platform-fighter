#!/usr/bin/env python3
"""Capture deterministic SSBM movement from Dolphin with scripted inputs.

This developer tool intentionally depends on a separately installed libmelee,
Dolphin/Slippi build, and an owner-supplied GALE01 revision 2 image. None of
those external assets are copied into the repository.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

import melee


SPECIAL_GEOMETRY_SOURCE_KEYS = {
    "neutral_ground": "0x12d",
    "neutral_air": "0x12e",
    "side_ground_miss": "0x12f",
    "side_ground_edge": "0x12f",
    "side_ground_hit": "0x130",
    "side_ground_item_hit": "0x130",
    "side_air_miss": "0x131",
    "side_air_hit": "0x132",
    "side_air_hit_floor": "0x132",
    "up_ground_miss": "0x133",
    "up_air_miss": "0x134",
    "up_air_ledge_grab": "0x134",
    "up_ground_catch": "0x135",
    "up_air_catch": "0x135",
    "down_ground": "0x137",
    "down_ground_hit": "0x137",
    "down_ground_wall": "0x137",
    "down_ground_edge": "0x13b",
    "down_air": "0x139",
    "down_air_land": "0x13a",
}

# MnSlMap.usd's stage-entry table maps St_Kind_Shrine (0x0E) to entry 5. The
# matching x90 anchor animation resolves to (-3.3, 15.7), and odd entry 5 uses
# the x40 template's y=-5.6 child. Keep the resulting source-derived cursor
# coordinate here instead of relying on a hand-tuned menu click.
HYRULE_TEMPLE_STAGE_CURSOR = (-3.3, 10.1)


def input_trace(
    platform_only: bool = False,
    push_only: bool = False,
    shield_only: bool = False,
    shield_geometry_only: bool = False,
    shield_geometry_sweep_only: bool = False,
    shield_hit_only: bool = False,
    damage_hit_only: bool = False,
    attack_iasa_only: bool = False,
    ground_attack_iasa_only: bool = False,
    hitbox_geometry_only: bool = False,
    throw_geometry_only: bool = False,
    special_geometry_only: bool = False,
    ground_attack_moves: tuple[str, ...] | None = None,
    special_geometry_moves: tuple[str, ...] | None = None,
    falcon_frame_data: dict[str, object] | None = None,
    shield_hit_pressure: float = 0.35,
) -> list[dict[str, object]]:
    trace: list[dict[str, object]] = []

    def extend(label: str, xs: list[float]) -> None:
        for x in xs:
            trace.append(command(label, main_x=x))

    def command(
        label: str,
        *,
        main_x: float = 0.5,
        main_y: float = 0.5,
        c_x: float = 0.5,
        c_y: float = 0.5,
        left_shoulder: float = 0.0,
        right_shoulder: float = 0.0,
        digital_left: bool = False,
        digital_right: bool = False,
        jump: bool = False,
        attack: bool = False,
        special: bool = False,
        grab: bool = False,
        taunt: bool = False,
        opponent_main_x: float = 0.5,
        opponent_attack: bool = False,
        opponent_jump: bool = False,
        fighter_x_override: float | None = None,
        fighter_x_from_item_offset: float | None = None,
        fighter_y_override: float | None = None,
        opponent_x_override: float | None = None,
        opponent_x_from_item_offset: float | None = None,
        opponent_y_override: float | None = None,
    ) -> dict[str, object]:
        return {
            "label": label,
            "main_x": main_x,
            "main_y": main_y,
            "c_x": c_x,
            "c_y": c_y,
            "left_shoulder": left_shoulder,
            "right_shoulder": right_shoulder,
            "digital_left": digital_left,
            "digital_right": digital_right,
            "jump": jump,
            "attack": attack,
            "special": special,
            "grab": grab,
            "taunt": taunt,
            "opponent_main_x": opponent_main_x,
            "opponent_attack": opponent_attack,
            "opponent_jump": opponent_jump,
            "fighter_x_override": fighter_x_override,
            "fighter_x_from_item_offset": fighter_x_from_item_offset,
            "fighter_y_override": fighter_y_override,
            "opponent_x_override": opponent_x_override,
            "opponent_x_from_item_offset": opponent_x_from_item_offset,
            "opponent_y_override": opponent_y_override,
        }

    def repeat(label: str, count: int, **inputs: object) -> None:
        trace.extend(command(label, **inputs) for _ in range(count))

    if push_only:
        repeat("push_settle", 60)
        repeat("push_walk_right", 240, main_x=0.7)
        repeat("push_recovery", 60)
        repeat(
            "push_opponent_walk_left",
            120,
            opponent_main_x=0.3,
        )
        repeat("push_opponent_recovery", 60)
        return trace

    if attack_iasa_only:
        # The generated NTSC 1.02 table records Jab 1 IASA on displayed frame
        # 16. Pulse edge-triggered inputs both one frame early and exactly on
        # that boundary; a held stick route independently exposes the first
        # continuous-input transition. These are oracle assertions over the
        # imported value, not a second authored timing source.
        if falcon_frame_data is None:
            raise ValueError("attack IASA capture requires Falcon frame data")
        falcon_jab1_iasa_frame = int(dict(falcon_frame_data["jab1"])["iasa"])

        def jab_interrupt_route(
            label: str,
            interrupt_frame: int,
            **interrupt_inputs: object,
        ) -> None:
            repeat(f"{label}_settle", 30)
            trace.append(command(f"{label}_jab", attack=True))
            repeat(
                f"{label}_before_interrupt",
                interrupt_frame - 2,
            )
            trace.append(
                command(
                    f"{label}_interrupt",
                    **interrupt_inputs,
                )
            )
            repeat(f"{label}_recover", 60)

        for frame_label, interrupt_frame in (
            ("early", falcon_jab1_iasa_frame - 1),
            ("exact", falcon_jab1_iasa_frame),
        ):
            jab_interrupt_route(
                f"attack_iasa_jump_{frame_label}",
                interrupt_frame,
                jump=True,
            )
            jab_interrupt_route(
                f"attack_iasa_guard_{frame_label}",
                interrupt_frame,
                left_shoulder=1.0,
                digital_left=True,
            )

        repeat("attack_iasa_walk_settle", 30)
        trace.append(command("attack_iasa_walk_jab", attack=True))
        repeat("attack_iasa_walk_hold", 30, main_x=1.0)
        repeat("attack_iasa_walk_recover", 60)
        return trace

    if hitbox_geometry_only:
        if falcon_frame_data is None:
            raise ValueError("hitbox-geometry capture requires Falcon frame data")

        def move_total(move: str) -> int:
            return int(dict(falcon_frame_data[move])["totalFrames"])

        def isolated_route(
            move: str,
            starter: list[dict[str, object]],
            recovery_padding: int = 40,
        ) -> None:
            # Reset the remote Falcon's idle animation phase without moving
            # either fighter. Its 21-frame Jab 1 completes inside this fixed
            # settle window, making every later hurt-capsule pose independent
            # of nondeterministic menu-entry timing.
            trace.append(
                command(
                    f"hitbox_geometry_{move}_opponent_pose_reset",
                    opponent_attack=True,
                )
            )
            repeat(f"hitbox_geometry_{move}_settle", 29)
            trace.extend(starter)
            repeat(
                f"hitbox_geometry_{move}_observe",
                move_total(move) + recovery_padding,
            )

        isolated_route(
            "jab1",
            [command("hitbox_geometry_jab1_start", attack=True)],
        )
        isolated_route(
            "jab2",
            [
                command("hitbox_geometry_jab2_jab1", attack=True),
                command("hitbox_geometry_jab2_wait"),
                command("hitbox_geometry_jab2_chain", attack=True),
            ],
        )
        isolated_route(
            "dashattack",
            [
                command("hitbox_geometry_dashattack_dash", main_x=1.0),
                command("hitbox_geometry_dashattack_hold", main_x=1.0),
                command("hitbox_geometry_dashattack_hold", main_x=1.0),
                command("hitbox_geometry_dashattack_hold", main_x=1.0),
                command(
                    "hitbox_geometry_dashattack_start",
                    main_x=1.0,
                    attack=True,
                ),
            ],
        )
        isolated_route(
            "ftilt_m",
            [
                command(
                    "hitbox_geometry_ftilt_m_start",
                    main_x=0.70,
                    attack=True,
                )
            ],
        )
        isolated_route(
            "utilt",
            [command("hitbox_geometry_utilt_start", main_y=0.65, attack=True)],
        )
        isolated_route(
            "dtilt",
            [command("hitbox_geometry_dtilt_start", main_y=0.35, attack=True)],
        )
        isolated_route(
            "fsmash_m",
            [command("hitbox_geometry_fsmash_m_start", c_x=1.0)],
        )
        isolated_route(
            "usmash",
            [command("hitbox_geometry_usmash_start", c_y=1.0)],
        )
        isolated_route(
            "dsmash",
            [command("hitbox_geometry_dsmash_start", c_y=0.0)],
        )
        isolated_route(
            "grab",
            [command("hitbox_geometry_grab_start", grab=True)],
        )
        isolated_route(
            "dashgrab",
            [
                command("hitbox_geometry_dashgrab_dash", main_x=1.0),
                command("hitbox_geometry_dashgrab_hold", main_x=1.0),
                command("hitbox_geometry_dashgrab_hold", main_x=1.0),
                command("hitbox_geometry_dashgrab_hold", main_x=1.0),
                command(
                    "hitbox_geometry_dashgrab_start",
                    main_x=1.0,
                    grab=True,
                ),
            ],
        )

        def aerial_starter(move: str, **attack_input: object) -> None:
            isolated_route(
                move,
                [
                    command(f"hitbox_geometry_{move}_jump", jump=True),
                    command(
                        f"hitbox_geometry_{move}_jump_squat",
                        jump=True,
                    ),
                    command(
                        f"hitbox_geometry_{move}_jump_squat",
                        jump=True,
                    ),
                    command(
                        f"hitbox_geometry_{move}_jump_squat",
                        jump=True,
                    ),
                    command(
                        f"hitbox_geometry_{move}_jump_squat",
                        jump=True,
                    ),
                    command(f"hitbox_geometry_{move}_ascent"),
                    command(f"hitbox_geometry_{move}_ascent"),
                    command(
                        f"hitbox_geometry_{move}_double_jump",
                        jump=True,
                    ),
                    command(
                        f"hitbox_geometry_{move}_start",
                        **attack_input,
                    ),
                ],
                recovery_padding=70,
            )

        aerial_starter("nair", attack=True)
        aerial_starter("fair", c_x=1.0)
        aerial_starter("bair", c_x=0.0)
        aerial_starter("uair", c_y=1.0)
        aerial_starter("dair", c_y=0.0)
        return trace

    if throw_geometry_only:
        if falcon_frame_data is None:
            raise ValueError("throw-geometry capture requires Falcon frame data")

        throw_inputs = {
            "fthrow": {"main_x": 1.0},
            "bthrow": {"main_x": 0.0},
            "dthrow": {"main_y": 0.0},
            "uthrow": {"main_y": 1.0},
        }
        for move, throw_input in throw_inputs.items():
            trace.append(
                command(
                    f"throw_geometry_{move}_preposition",
                    fighter_x_override=-2.0,
                    opponent_x_override=2.0,
                )
            )
            repeat(f"throw_geometry_{move}_settle", 60)
            trace.append(command(f"throw_geometry_{move}_grab", grab=True))
            repeat(f"throw_geometry_{move}_capture", 12)
            trace.append(command(f"throw_geometry_{move}_start", **throw_input))
            repeat(
                f"throw_geometry_{move}_observe",
                int(dict(falcon_frame_data[move])["totalFrames"]) + 45,
            )
            trace.append(
                command(f"throw_geometry_{move}_victim_wakeup", opponent_attack=True)
            )
            repeat(f"throw_geometry_{move}_victim_recover", 60)
        return trace

    if special_geometry_only:
        if falcon_frame_data is None:
            raise ValueError("special-geometry capture requires Falcon frame data")

        special_moves = special_geometry_moves or ("neutral_ground",)
        unsupported = set(special_moves) - set(SPECIAL_GEOMETRY_SOURCE_KEYS)
        if unsupported:
            raise ValueError(
                f"unsupported special-geometry routes: {sorted(unsupported)}"
            )

        def special_total(route: str) -> int:
            return int(
                dict(falcon_frame_data[SPECIAL_GEOMETRY_SOURCE_KEYS[route]])[
                    "totalFrames"
                ]
            )

        for route in special_moves:
            trace.append(
                command(
                    f"special_geometry_{route}_opponent_pose_reset",
                    opponent_attack=True,
                    opponent_x_from_item_offset=(
                        100.0 if route == "side_ground_item_hit" else None
                    ),
                )
            )
            repeat(
                f"special_geometry_{route}_settle",
                29,
                opponent_x_from_item_offset=(
                    100.0 if route == "side_ground_item_hit" else None
                ),
            )
            airborne = route in {
                "neutral_air",
                "side_air_miss",
                "side_air_hit",
                "side_air_hit_floor",
                "up_air_miss",
                "up_air_ledge_grab",
                "up_air_catch",
                "down_air",
                "down_air_land",
            }
            low_airborne = route == "down_air_land"
            elevated_airborne = airborne and not low_airborne
            native_air_hit = route == "side_air_hit_floor"
            if airborne:
                airborne_opponent = route in {
                    "side_air_hit",
                    "side_air_hit_floor",
                    "up_air_catch",
                }

                if route == "up_air_ledge_grab":
                    # Start from a safe grounded point, then create the
                    # recovery setup entirely through native movement. This
                    # preserves Melee's collision history and ledge flags.
                    trace.append(
                        command(
                            "special_geometry_up_air_ledge_grab_preposition",
                            fighter_x_override=-45.0,
                        )
                    )
                    repeat("special_geometry_up_air_ledge_grab_preposition_settle", 3)
                    trace.extend(
                        command(
                            "special_geometry_up_air_ledge_grab_jump",
                            main_x=0.0,
                            jump=True,
                        )
                        for _ in range(5)
                    )
                    repeat(
                        "special_geometry_up_air_ledge_grab_drift_out",
                        25,
                        main_x=0.0,
                    )
                    repeat(
                        "special_geometry_up_air_ledge_grab_descend",
                        32,
                    )
                    airborne_commands = []
                else:

                    def airborne_setup(
                        suffix: str, *, jump: bool = False
                    ) -> dict[str, object]:
                        opponent_elevated = (
                            airborne_opponent and suffix == "opponent_elevate"
                        )
                        native_jump_setup = native_air_hit and suffix == "jump"
                        native_jump_held = suffix in {"jump", "jump_squat"}
                        return command(
                            f"special_geometry_{route}_{suffix}",
                            jump=jump,
                            opponent_jump=(
                                airborne_opponent
                                and (not native_air_hit or native_jump_held)
                            ),
                            fighter_x_override=(-10.0 if native_jump_setup else None),
                            opponent_x_override=(
                                0.0
                                if native_jump_setup
                                or (airborne_opponent and not native_air_hit)
                                else None
                            ),
                            opponent_y_override=(
                                500.0
                                if opponent_elevated and not native_air_hit
                                else None
                            ),
                        )

                    airborne_commands = [
                        airborne_setup("jump", jump=True),
                        airborne_setup("jump_squat", jump=True),
                        airborne_setup("jump_squat", jump=True),
                        airborne_setup("jump_squat", jump=True),
                        airborne_setup("jump_squat", jump=True),
                        airborne_setup("ascent"),
                        airborne_setup("ascent"),
                    ]
                    if not low_airborne:
                        airborne_commands.extend(
                            (
                                airborne_setup("double_jump", jump=True),
                                airborne_setup("airborne_hold"),
                                airborne_setup("airborne_hold"),
                                airborne_setup("opponent_elevate"),
                            )
                        )
                    if native_air_hit:
                        airborne_commands.extend(
                            airborne_setup("descent") for _ in range(6)
                        )
                trace.extend(airborne_commands)
            if route == "neutral_ground":
                trace.append(
                    command(
                        "special_geometry_neutral_ground_start",
                        special=True,
                    )
                )
            elif route == "neutral_air":
                trace.append(
                    command(
                        "special_geometry_neutral_air_start",
                        special=True,
                        fighter_y_override=500.0,
                    )
                )
            elif route in {
                "side_ground_miss",
                "side_ground_edge",
                "side_ground_hit",
                "side_ground_item_hit",
                "side_air_miss",
                "side_air_hit",
                "side_air_hit_floor",
            }:
                collision_route = route.endswith("_hit") or native_air_hit
                trace.append(
                    command(
                        f"special_geometry_{route}_start",
                        main_x=1.0,
                        special=True,
                        fighter_x_override=(
                            78.0
                            if route == "side_ground_edge"
                            else (
                                -10.0
                                if collision_route
                                and not native_air_hit
                                and route != "side_ground_item_hit"
                                else None
                            )
                        ),
                        fighter_x_from_item_offset=(
                            -10.0 if route == "side_ground_item_hit" else None
                        ),
                        fighter_y_override=(
                            500.0 if elevated_airborne and not native_air_hit else None
                        ),
                        opponent_x_override=(
                            0.0
                            if collision_route
                            and not native_air_hit
                            and route != "side_ground_item_hit"
                            else None
                        ),
                        opponent_x_from_item_offset=(
                            100.0 if route == "side_ground_item_hit" else None
                        ),
                        opponent_y_override=(
                            500.0
                            if collision_route and airborne and not native_air_hit
                            else None
                        ),
                        opponent_main_x=(0.0 if route == "side_ground_edge" else 0.5),
                    )
                )
            elif route in {
                "up_ground_miss",
                "up_air_miss",
                "up_air_ledge_grab",
                "up_ground_catch",
                "up_air_catch",
            }:
                collision_route = route.endswith("_catch")
                trace.append(
                    command(
                        f"special_geometry_{route}_start",
                        main_y=1.0,
                        main_x=(0.75 if route == "up_air_ledge_grab" else 0.5),
                        special=True,
                        fighter_x_override=(-5.0 if collision_route else None),
                        fighter_y_override=(
                            500.0
                            if elevated_airborne and route != "up_air_ledge_grab"
                            else None
                        ),
                        opponent_x_override=(0.0 if collision_route else None),
                        opponent_y_override=(
                            500.0 if collision_route and airborne else None
                        ),
                    )
                )
                if route == "up_air_ledge_grab":
                    repeat(
                        "special_geometry_up_air_ledge_grab_steer_toward",
                        12,
                        main_x=1.0,
                    )
                    repeat(
                        "special_geometry_up_air_ledge_grab_steer_away",
                        52,
                        main_x=0.24,
                    )
            elif route in {
                "down_ground",
                "down_ground_hit",
                "down_ground_wall",
                "down_ground_edge",
                "down_air",
                "down_air_land",
            }:
                if route == "down_ground_wall":
                    # Use GrSh.dat's local floor-5/left-wall-79 collision group
                    # as the relocation target. Relocate only after a real jump
                    # has cleared the old floor-support line, then let Melee's
                    # joint transforms and collision code choose the supported
                    # surface naturally. The kick itself is unmodified.
                    trace.extend(
                        command(
                            "special_geometry_down_ground_wall_jump",
                            jump=True,
                        )
                        for _ in range(5)
                    )
                    repeat("special_geometry_down_ground_wall_air_wait", 35)
                    repeat(
                        "special_geometry_down_ground_wall_air_relocate",
                        3,
                        fighter_x_override=-187.0,
                        fighter_y_override=20.0,
                    )
                    repeat("special_geometry_down_ground_wall_land", 40)
                trace.append(
                    command(
                        f"special_geometry_{route}_start",
                        main_y=0.0,
                        special=True,
                        fighter_x_override=(
                            80.0
                            if route == "down_ground_edge"
                            else (-20.0 if route == "down_ground_hit" else None)
                        ),
                        opponent_x_override=(
                            0.0 if route == "down_ground_hit" else None
                        ),
                        fighter_y_override=(500.0 if elevated_airborne else None),
                    )
                )
            repeat(
                f"special_geometry_{route}_observe",
                special_total(route) + 100,
                opponent_main_x=(0.0 if route == "side_ground_edge" else 0.5),
                fighter_y_override=(
                    500.0
                    if elevated_airborne
                    and route
                    not in {
                        "neutral_air",
                        "side_air_hit_floor",
                        "up_air_ledge_grab",
                    }
                    else None
                ),
                opponent_x_override=(
                    0.0
                    if route
                    in {
                        "side_ground_hit",
                        "side_air_hit",
                        "up_ground_catch",
                        "up_air_catch",
                        "down_ground_hit",
                    }
                    else None
                ),
                opponent_y_override=(
                    500.0 if route in {"side_air_hit", "up_air_catch"} else None
                ),
            )
        return trace

    if ground_attack_iasa_only:
        if falcon_frame_data is None:
            raise ValueError("ground-attack IASA capture requires Falcon frame data")

        starters: dict[str, list[dict[str, object]]] = {
            "dashattack": [
                command("dashattack_dash", main_x=1.0),
                command("dashattack_hold", main_x=1.0),
                command("dashattack_hold", main_x=1.0),
                command("dashattack_hold", main_x=1.0),
                # Keep the already-aged dash direction held while A arrives.
                # Releasing it drops the controller below the dash-attack
                # branch; re-flicking it with A requests forward smash.
                command("dashattack_start", main_x=1.0, attack=True),
            ],
            "ftilt_m": [command("ftilt_m_start", main_x=0.70, attack=True)],
            "utilt": [command("utilt_start", main_y=0.65, attack=True)],
            "dtilt": [command("dtilt_start", main_y=0.35, attack=True)],
            "fsmash_m": [command("fsmash_m_start", c_x=1.0)],
            "usmash": [command("usmash_start", c_y=1.0)],
            "dsmash": [command("dsmash_start", c_y=0.0)],
        }

        def move_value(move: str, field: str) -> int:
            return int(dict(falcon_frame_data[move])[field])

        route_index = 0
        capture_facing = 1

        def interrupt_route(
            move: str,
            route: str,
            interrupt_frame: int,
            **interrupt_inputs: object,
        ) -> None:
            nonlocal capture_facing, route_index
            prefix = f"ground_iasa_{move}_{route}"
            if route == "special" and capture_facing < 0:
                # Falcon Punch carries substantial facing-relative root
                # motion. Face back toward stage center before testing a
                # special IASA branch so the isolated character-specific body
                # cannot walk later routes into an edge clamp.
                trace.append(command(f"{prefix}_face_center", main_x=1.0))
                capture_facing = 1
            # Mirror every other route.  Long matrix captures otherwise push
            # Falcon onto Final Destination's x=85.5657 edge, where Melee
            # retains the authored root velocity but collision correctly
            # clamps world position.  Alternation keeps this timing oracle
            # away from stage geometry without resetting game state.
            mirrored = bool(route_index & 1)
            if move == "ftilt_m":
                # Melee's forward-tilt input is facing-relative; an opposite
                # horizontal A press falls through to Jab 1 instead of
                # turning the fighter. Preserve the facing left by the last
                # horizontal route.
                mirrored = capture_facing < 0
            elif move == "fsmash_m" and route == "special":
                mirrored = False
            route_index += 1
            recovery_frames = move_value(move, "totalFrames") + 45
            if route == "special":
                recovery_frames = max(
                    recovery_frames,
                    move_value("0x12d", "totalFrames") + 45,
                )
            repeat(f"{prefix}_settle", 40)
            for starter in starters[move]:
                mirrored_starter = dict(starter)
                if mirrored:
                    # libmelee's normalized controller axes use 0.5 as
                    # neutral, so reflection is 1-x rather than negation.
                    mirrored_starter["main_x"] = 1.0 - float(mirrored_starter["main_x"])
                    mirrored_starter["c_x"] = 1.0 - float(mirrored_starter["c_x"])
                trace.append(mirrored_starter)
            if move in {"dashattack", "fsmash_m"}:
                capture_facing = -1 if mirrored else 1
            repeat(f"{prefix}_before", interrupt_frame - 2)
            trace.append(command(f"{prefix}_interrupt", **interrupt_inputs))
            repeat(
                f"{prefix}_recover",
                recovery_frames,
            )

        # Match setup already places the target on the opposite side. Keep it
        # there: running farther right walks it off Final Destination and its
        # later respawn beside Falcon would inject hitlag into timing routes.
        repeat("ground_iasa_opponent_settle", 60)

        selected_moves = set(ground_attack_moves or starters)
        routed_moves = tuple(
            move
            for move in (
                "dashattack",
                "utilt",
                "dtilt",
                "fsmash_m",
                "usmash",
                "dsmash",
            )
            if move in selected_moves
        )
        for move in routed_moves:
            iasa = move_value(move, "iasa")
            interrupt_route(move, "jump_early", iasa - 1, jump=True)
            interrupt_route(move, "jump_exact", iasa, jump=True)

        # Forward tilt has no IASA command. A late jump must remain locked
        # through its penultimate displayed animation frame.
        if "ftilt_m" in selected_moves:
            interrupt_route(
                "ftilt_m",
                "jump_no_iasa",
                move_value("ftilt_m", "totalFrames") - 1,
                jump=True,
            )

        wait_inputs = {
            "guard": {"left_shoulder": 1.0, "digital_left": True},
            "special": {"special": True},
            "grab": {"grab": True},
            "taunt": {"taunt": True},
            "spot": {
                "main_y": 0.0,
                "left_shoulder": 1.0,
                "digital_left": True,
            },
        }
        if "utilt" in selected_moves:
            for route, inputs in wait_inputs.items():
                interrupt_route(
                    "utilt",
                    route,
                    move_value("utilt", "iasa"),
                    **inputs,
                )

        restricted_inputs = {
            **wait_inputs,
            "up_attack": {"main_y": 0.65, "attack": True},
        }
        for move in ("dtilt", "fsmash_m"):
            if move not in selected_moves:
                continue
            for route, inputs in restricted_inputs.items():
                interrupt_route(
                    move,
                    route,
                    move_value(move, "iasa"),
                    **inputs,
                )
        return trace

    if shield_only:
        repeat("shield_formula_settle", 60)
        repeat(
            "shield_formula_below_dead_zone",
            8,
            left_shoulder=0.29,
        )
        repeat("shield_formula_below_release", 8)
        repeat(
            "shield_formula_threshold_sample",
            12,
            left_shoulder=0.30,
        )
        repeat("shield_formula_threshold_release", 12)
        repeat(
            "shield_formula_light",
            30,
            left_shoulder=0.35,
        )
        repeat("shield_formula_light_release", 20)
        repeat(
            "shield_formula_low_mid",
            30,
            left_shoulder=0.40,
        )
        repeat("shield_formula_low_mid_release", 20)
        repeat(
            "shield_formula_high_mid",
            30,
            left_shoulder=0.65,
        )
        repeat("shield_formula_high_mid_release", 20)
        repeat(
            "shield_formula_near_dense",
            30,
            right_shoulder=0.99,
        )
        repeat("shield_formula_near_dense_release", 20)
        repeat(
            "shield_formula_both_shoulders",
            30,
            left_shoulder=0.40,
            right_shoulder=0.70,
        )
        repeat("shield_formula_both_release", 20)
        repeat(
            "shield_formula_digital_full",
            30,
            left_shoulder=1.0,
            digital_left=True,
        )
        repeat("shield_formula_regeneration", 120)
        return trace

    if shield_geometry_only:
        repeat("shield_geometry_settle", 60)
        repeat(
            "shield_geometry_neutral",
            20,
            left_shoulder=0.35,
        )
        for label, main_x, main_y in (
            ("right", 0.68, 0.5),
            ("up_right", 0.625, 0.625),
            ("up", 0.5, 0.68),
            ("up_left", 0.375, 0.625),
            ("left", 0.32, 0.5),
            ("down_left", 0.375, 0.375),
            ("down", 0.5, 0.32),
            ("down_right", 0.625, 0.375),
        ):
            repeat(
                f"shield_geometry_{label}",
                12,
                main_x=main_x,
                main_y=main_y,
                left_shoulder=0.35,
            )
            repeat(
                f"shield_geometry_{label}_recenter",
                8,
                left_shoulder=0.35,
            )
        repeat("shield_geometry_release", 30)
        return trace

    if shield_geometry_sweep_only:
        repeat("shield_geometry_sweep_settle", 60)
        repeat(
            "shield_geometry_sweep_neutral",
            20,
            left_shoulder=0.35,
        )
        for angle_index in range(256):
            angle = 2.0 * math.pi * angle_index / 256.0
            signed_x = 0.55 * math.cos(angle)
            signed_y = 0.55 * math.sin(angle)
            repeat(
                f"shield_geometry_sweep_{angle_index:03d}",
                8,
                main_x=0.5 + signed_x / 3.125,
                main_y=0.5 + signed_y / 3.125,
                left_shoulder=0.35,
            )
        repeat("shield_geometry_sweep_release", 30)
        return trace

    if shield_hit_only:
        repeat("shield_hit_settle", 60)
        repeat(
            "shield_hit_close_distance",
            115,
            main_x=0.7,
            opponent_main_x=0.3,
        )
        repeat("shield_hit_neutral_settle", 20)
        repeat(
            "shield_hit_hold",
            12,
            left_shoulder=shield_hit_pressure,
        )
        trace.append(
            command(
                "shield_hit_jab",
                left_shoulder=shield_hit_pressure,
                opponent_attack=True,
            )
        )
        repeat(
            "shield_hit_recovery",
            45,
            left_shoulder=shield_hit_pressure,
        )
        repeat("shield_hit_release", 30)
        return trace

    if damage_hit_only:
        repeat("damage_hit_settle", 60)
        repeat(
            "damage_hit_close_distance",
            115,
            main_x=0.7,
            opponent_main_x=0.3,
        )
        repeat("damage_hit_neutral_settle", 20)
        trace.append(command("damage_hit_jab", opponent_attack=True))
        repeat("damage_hit_recovery", 75)
        return trace

    if platform_only:
        repeat("platform_settle", 60)
        trace.append(command("platform_jump", jump=True))
        repeat("platform_jump_squat", 5)
        repeat("platform_landing_setup", 100)
        repeat("platform_landing_settle", 30)
        trace.append(command("platform_tap_down_entry", main_y=0.0))
        repeat("platform_tap_down_release", 30)
        repeat("platform_before_held_down", 30)
        trace.append(command("platform_held_down_entry", main_y=0.0))
        repeat("platform_held_down", 30, main_y=0.0)
        repeat("platform_held_down_recovery", 60)
        return trace

    extend("settle", [0.5] * 10)
    extend("held_dash_right", [1.0] * 25)
    extend("run_turnaround_left", [0.0] * 35)
    extend("held_dash_neutral", [0.5] * 35)
    extend("direct_dash_right", [1.0] * 4)
    extend("direct_dash_dance_left", [0.0] * 5)
    extend("direct_dash_dance_right", [1.0] * 5)
    extend("direct_dash_dance_left_2", [0.0] * 5)
    extend("neutral_brake", [0.5] * 30)
    extend("moving_dash_right", [1.0] * 5)
    extend("moving_neutral", [0.5] * 3)
    extend("moving_dash_left", [0.0] * 5)
    extend("moving_dash_right_return", [1.0] * 5)
    extend("settle_again", [0.5] * 30)
    extend("two_sample_dash", [0.65, 1.0, 1.0, 1.0, 1.0])
    extend("reverse_after_two_sample", [0.0] * 5)
    extend("settle_after_two_sample", [0.5] * 30)
    extend("pivot_dash_right", [1.0] * 4)
    extend("pivot_reverse_left", [0.0])
    extend("pivot_neutral", [0.5] * 15)
    extend("slow_sweep_walk", [0.62, 0.70, 1.0, 1.0, 1.0])
    extend("settle_after_walk", [0.5] * 20)

    # Shield routes deliberately retain analog shoulders and digital clicks as
    # separate inputs. Melee turns an analog shoulder above the common 0.30
    # dead zone into HSD_PAD_LR, while EscapeAir checks only a fresh digital
    # HSD_PAD_L/HSD_PAD_R click.
    repeat("run_for_full_shield", 25, main_x=1.0)
    repeat(
        "run_to_full_shield",
        12,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("release_full_shield", 20)
    repeat("settle_before_light_shield", 20)
    repeat("light_shield_half_press", 20, left_shoulder=0.5)
    repeat("release_light_shield", 20)
    repeat("below_analog_shield_dead_zone", 4, left_shoulder=0.29)
    repeat("settle_before_escapes", 20)
    repeat(
        "shield_before_forward_roll",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    trace.append(
        command(
            "forward_roll_entry",
            main_x=1.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("forward_roll_recovery", 40)
    repeat(
        "shield_before_spot_dodge",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    trace.append(
        command(
            "spot_dodge_entry",
            main_y=0.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("spot_dodge_recovery", 35)

    repeat(
        "shield_before_jump",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    trace.append(
        command(
            "jump_from_held_left_shield",
            left_shoulder=1.0,
            digital_left=True,
            jump=True,
        )
    )
    repeat(
        "held_left_during_jump_squat",
        5,
        left_shoulder=1.0,
        digital_left=True,
    )
    trace.append(
        command(
            "fresh_right_air_dodge_while_left_held",
            main_x=1.0,
            main_y=0.0,
            left_shoulder=1.0,
            right_shoulder=1.0,
            digital_left=True,
            digital_right=True,
        )
    )
    repeat("air_dodge_progress", 12)
    repeat("air_dodge_landing_recovery", 50)

    trace.append(command("jump_for_analog_air_control", jump=True))
    repeat("jump_squat_for_analog_air_control", 5)
    repeat(
        "analog_light_shield_does_not_air_dodge",
        8,
        left_shoulder=0.5,
    )
    repeat("analog_air_control_landing", 70)

    repeat(
        "shield_before_backward_roll",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    trace.append(
        command(
            "backward_roll_entry",
            main_x=1.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("backward_roll_recovery", 45)

    repeat(
        "shield_before_cstick_roll",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat(
        "cstick_roll_buffer",
        4,
        c_x=0.0,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("cstick_roll_recovery", 40)

    repeat(
        "shield_before_cstick_spot_dodge",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat(
        "cstick_spot_dodge_buffer",
        4,
        c_y=0.0,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("cstick_spot_dodge_recovery", 36)

    repeat(
        "shield_before_cstick_jump",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat(
        "cstick_jump_buffer",
        4,
        c_y=1.0,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("cstick_jump_recovery", 32)

    # Aerial locomotion routes retain exact button-hold and stick timing so
    # short/full-hop selection, jump-squat momentum reversal, double-jump
    # velocity replacement, and fast-fall entry are executable-oracle gates.
    repeat("cstick_jump_landing", 60)
    repeat("recenter_after_defense", 15, main_x=0.0)
    repeat("recenter_after_defense_brake", 20)
    repeat("run_for_jump_squat_reverse", 20, main_x=1.0)
    trace.append(command("running_jump_start", main_x=1.0, jump=True))
    repeat("jump_squat_reverse", 5, main_x=0.0)
    repeat("reverse_jump_landing", 80)

    repeat("recenter_before_double_jump", 5, main_x=0.0)
    repeat("recenter_before_double_jump_brake", 20)

    trace.append(command("neutral_jump_for_double_jump", jump=True))
    repeat("neutral_jump_squat_for_double_jump", 5)
    repeat("first_jump_drift_left", 10, main_x=0.0)
    trace.append(command("neutral_stick_double_jump", jump=True))
    repeat("neutral_stick_double_jump_landing", 90)

    trace.append(command("short_hop_press", jump=True))
    repeat("short_hop_release", 80)

    trace.append(command("full_hop_press", jump=True))
    repeat("full_hop_hold", 5, jump=True)
    repeat("full_hop_landing", 95)

    trace.append(command("full_hop_for_fast_fall", jump=True))
    repeat("full_hop_hold_for_fast_fall", 5, jump=True)
    repeat("rise_before_fast_fall", 35)
    repeat("fast_fall_down_press", 4, main_y=0.0)
    repeat("fast_fall_landing", 65)

    # Basic grounded vertical-stick transitions are part of ordinary movement,
    # and must retain Melee's authored squat-start/hold/release sequencing.
    # Keep the stick fully down long enough to observe the complete entry and
    # held states, then release to neutral through the complete exit state.
    repeat("settle_before_crouch", 20)
    repeat("crouch_hold", 30, main_y=0.0)
    repeat("crouch_release", 20)

    # Common-data x90/x94 form a deliberate hysteresis band. Exact x90 does
    # not enter squat; moving just beyond it does. Exact x94 keeps SquatWait;
    # moving just above it starts SquatRv.
    # Dolphin's pipe takes an unsigned controller byte while Slippi reports the
    # in-game signed stick after its 80-unit normalization. Bytes 73/72 observe
    # as 0.15625/0.15; bytes 78/79 observe as 0.1875/0.19375.
    repeat("crouch_entry_boundary", 10, main_y=73.0 / 255.0)
    repeat("crouch_entry_beyond", 20, main_y=72.0 / 255.0)
    repeat("crouch_release_boundary", 10, main_y=78.0 / 255.0)
    repeat("crouch_release_beyond", 20, main_y=79.0 / 255.0)

    # Crouch IASA routes are state-specific in ftCo_Squat*, even when they
    # return to the ordinary movement vocabulary. Pin the common jump route
    # in all three states, SquatWait's direct dash, and SquatRv's direct walk.
    repeat("settle_before_crouch_start_jump", 10)
    repeat("crouch_start_before_jump", 2, main_y=0.0)
    trace.append(command("crouch_start_jump", main_y=0.0, jump=True))
    repeat("crouch_start_jump_landing", 80)

    repeat("settle_before_crouch_wait_jump", 10)
    repeat("crouch_wait_before_jump", 20, main_y=0.0)
    trace.append(command("crouch_wait_jump", main_y=0.0, jump=True))
    repeat("crouch_wait_jump_landing", 80)

    repeat("settle_before_crouch_end_jump", 10)
    repeat("crouch_end_before_jump", 20, main_y=0.0)
    trace.append(command("crouch_end_release", main_y=0.5))
    trace.append(command("crouch_end_jump", jump=True))
    repeat("crouch_end_jump_landing", 80)

    repeat("recenter_before_crouch_wait_dash", 18, main_x=0.0)
    repeat("recenter_before_crouch_wait_dash_brake", 35)
    repeat("settle_before_crouch_wait_dash", 10)
    repeat("crouch_wait_before_dash", 20, main_y=0.0)
    repeat("crouch_wait_dash", 20, main_x=1.0)
    repeat("crouch_wait_dash_recovery", 35)

    repeat("settle_before_crouch_end_walk", 10)
    repeat("crouch_wait_before_end_walk", 20, main_y=0.0)
    trace.append(command("crouch_end_before_walk"))
    repeat("crouch_end_walk", 20, main_x=0.75)
    repeat("crouch_end_walk_recovery", 30)

    # Guard input appears in every crouch-state IASA list. Use a neutral stick
    # on the guard frame so the trace isolates guard precedence from spot dodge
    # and platform-pass input.
    repeat("settle_before_crouch_start_guard", 10)
    repeat("crouch_start_before_guard", 2, main_y=0.0)
    repeat(
        "crouch_start_guard",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("crouch_start_guard_release", 20)

    repeat("settle_before_crouch_wait_guard", 10)
    repeat("crouch_wait_before_guard", 20, main_y=0.0)
    repeat(
        "crouch_wait_guard",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("crouch_wait_guard_release", 20)

    repeat("settle_before_crouch_end_guard", 10)
    repeat("crouch_end_before_guard", 20, main_y=0.0)
    trace.append(command("crouch_end_guard_release"))
    repeat(
        "crouch_end_guard",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("crouch_end_guard_recovery", 20)

    # Taunt is dispatched by ftCo_800DE9D8 from Squat, SquatWait, and
    # SquatRv. D-pad up is held for one scheduled sample only so each route
    # proves fresh-input eligibility without depending on input repetition.
    repeat("settle_before_crouch_start_taunt", 10)
    repeat("crouch_start_before_taunt", 2, main_y=0.0)
    trace.append(command("crouch_start_taunt", taunt=True))
    repeat("crouch_start_taunt_recovery", 110)

    repeat("settle_before_crouch_wait_taunt", 10)
    repeat("crouch_wait_before_taunt", 20, main_y=0.0)
    trace.append(command("crouch_wait_taunt", taunt=True))
    repeat("crouch_wait_taunt_recovery", 110)

    repeat("settle_before_crouch_end_taunt", 10)
    repeat("crouch_end_before_taunt", 20, main_y=0.0)
    trace.append(command("crouch_end_taunt_release"))
    trace.append(command("crouch_end_taunt", taunt=True))
    repeat("crouch_end_taunt_recovery", 110)

    # Core crouch IASA beyond movement/guard/taunt. Neutral A is eligible in
    # Squat, SquatWait, and SquatRv. Physical Z enters Catch from Squat but
    # must reveal its A-component fallback from SquatWait and SquatRv. Neutral B is
    # eligible during Squat; SquatWait and SquatRv list only the down-special
    # dispatcher, so a neutral B sample must not start Falcon Punch there.
    repeat("settle_before_crouch_start_attack", 10)
    repeat("crouch_start_before_attack", 2, main_y=0.0)
    trace.append(command("crouch_start_attack", attack=True))
    repeat("crouch_start_attack_recovery", 60)

    repeat("settle_before_crouch_wait_attack", 10)
    repeat("crouch_wait_before_attack", 20, main_y=0.0)
    trace.append(command("crouch_wait_attack", attack=True))
    repeat("crouch_wait_attack_recovery", 60)

    repeat("settle_before_crouch_end_attack", 10)
    repeat("crouch_end_before_attack", 20, main_y=0.0)
    trace.append(command("crouch_end_attack_release"))
    trace.append(command("crouch_end_attack", attack=True))
    repeat("crouch_end_attack_recovery", 60)

    repeat("settle_before_crouch_wait_neutral_special", 10)
    repeat("crouch_wait_before_neutral_special", 20, main_y=0.0)
    trace.append(command("crouch_wait_neutral_special", special=True))
    repeat("crouch_wait_neutral_special_recovery", 40)

    repeat("settle_before_crouch_end_neutral_special", 10)
    repeat("crouch_end_before_neutral_special", 20, main_y=0.0)
    trace.append(command("crouch_end_neutral_special_release"))
    trace.append(command("crouch_end_neutral_special", special=True))
    repeat("crouch_end_neutral_special_recovery", 40)

    repeat("settle_before_crouch_start_neutral_special", 10)
    repeat("crouch_start_before_neutral_special", 2, main_y=0.0)
    trace.append(command("crouch_start_neutral_special", special=True))
    repeat("crouch_start_neutral_special_recovery", 130)

    repeat("settle_before_crouch_start_grab", 10)
    repeat("crouch_start_before_grab", 2, main_y=0.0)
    trace.append(command("crouch_start_grab", grab=True))
    repeat("crouch_start_grab_recovery", 90)

    repeat("settle_before_crouch_wait_grab", 10)
    repeat("crouch_wait_before_grab", 20, main_y=0.0)
    trace.append(command("crouch_wait_grab", grab=True))
    repeat("crouch_wait_grab_recovery", 90)

    repeat("settle_before_crouch_end_grab", 10)
    repeat("crouch_end_before_grab", 20, main_y=0.0)
    trace.append(command("crouch_end_grab_release"))
    trace.append(command("crouch_end_grab", grab=True))
    repeat("crouch_end_grab_recovery", 90)

    # Turn's common IASA list also dispatches AppealS. Enter an ordinary
    # standing turn for one frame, then press D-pad up before the turn ends.
    repeat("settle_before_standing_turn_taunt", 10)
    trace.append(command("standing_turn_before_taunt", main_x=0.0))
    trace.append(command("standing_turn_taunt", taunt=True))
    repeat("standing_turn_taunt_recovery", 110)

    # Falcon's normal-landing IASA becomes available once the displayed
    # animation reaches the four-frame common landing-lag value. A short hop
    # reaches Landing frame 1 after 35 scheduled samples, so 38 neutral
    # samples leave frame 4 visible immediately before this fresh taunt.
    repeat("settle_before_landing_taunt", 10)
    trace.append(command("landing_taunt_jump", jump=True))
    repeat("landing_taunt_setup", 38)
    trace.append(command("landing_taunt", taunt=True))
    repeat("landing_taunt_recovery", 110)

    # Systematically qualify more entries from ftCo_Landing_IASA on the same
    # first legal displayed-frame boundary. Jump, dash, guard (the decomp's
    # ftCo_80091A4C call), crouch, turn, and walk all appear in the common IASA
    # list; character attacks, specials, and grabs are outside this shared
    # movement trace.
    repeat("settle_before_landing_jump", 10)
    trace.append(command("landing_jump_jump", jump=True))
    repeat("landing_jump_setup", 38)
    trace.append(command("landing_jump", jump=True))
    repeat("landing_jump_recovery", 110)

    repeat("settle_before_landing_dash", 10)
    trace.append(command("landing_dash_jump", jump=True))
    repeat("landing_dash_setup", 38)
    trace.append(command("landing_dash", main_x=1.0))
    repeat("landing_dash_recovery", 110)

    repeat("settle_before_landing_guard", 10)
    trace.append(command("landing_guard_jump", jump=True))
    repeat("landing_guard_setup", 38)
    trace.append(
        command(
            "landing_guard",
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat(
        "landing_guard_held",
        12,
        left_shoulder=1.0,
        digital_left=True,
    )
    repeat("landing_guard_recovery", 110)

    repeat("settle_before_landing_walk", 10)
    trace.append(command("landing_walk_jump", jump=True))
    repeat("landing_walk_setup", 38)
    trace.append(command("landing_walk", main_x=0.75))
    repeat("landing_walk_recovery", 110)

    repeat("settle_before_landing_crouch", 10)
    trace.append(command("landing_crouch_jump", jump=True))
    repeat("landing_crouch_setup", 38)
    trace.append(command("landing_crouch", main_y=0.0))
    repeat("landing_crouch_recovery", 110)

    # SquatWait_CheckInput is gated to the one-frame window ending at
    # normal_landing_lag + frame_speed_mul. Down one displayed frame later
    # must leave Landing active rather than replaying the ordinary Squat entry.
    repeat("settle_before_landing_late_crouch", 10)
    trace.append(command("landing_late_crouch_jump", jump=True))
    repeat("landing_late_crouch_setup", 39)
    trace.append(command("landing_late_crouch", main_y=0.0))
    repeat("landing_late_crouch_recovery", 110)

    repeat("settle_before_landing_turn", 10)
    trace.append(command("landing_turn_jump", jump=True))
    repeat("landing_turn_setup", 38)
    trace.append(command("landing_turn", main_x=0.25))
    repeat("landing_turn_recovery", 110)

    # ftCo_Jump_GetInput checks a fresh main-stick up tilt before X/Y. Hold
    # through Falcon's four-frame KneeBend for the full-hop route, then repeat
    # with a one-sample tap to pin the stick-release short-hop path.
    repeat("settle_before_ground_tap_full_hop", 10)
    repeat("ground_tap_full_hop", 5, main_y=1.0)
    repeat("ground_tap_full_hop_recovery", 110)

    repeat("settle_before_ground_tap_short_hop", 10)
    trace.append(command("ground_tap_short_hop", main_y=1.0))
    repeat("ground_tap_short_hop_recovery", 110)

    repeat("settle_before_air_tap_jump", 10)
    trace.append(command("air_tap_jump_first_jump", jump=True))
    repeat("air_tap_jump_squat", 5)
    repeat("air_tap_jump_before_second", 10)
    trace.append(command("air_tap_jump", main_y=1.0))
    repeat("air_tap_jump_recovery", 90)

    repeat("settle_before_landing_tap_jump", 10)
    trace.append(command("landing_tap_jump_first_jump", jump=True))
    repeat("landing_tap_jump_setup", 38)
    trace.append(command("landing_tap_jump", main_y=1.0))
    repeat("landing_tap_jump_recovery", 110)

    repeat("settle_before_shield_tap_jump", 10)
    repeat(
        "shield_tap_jump_setup",
        10,
        left_shoulder=1.0,
        digital_left=True,
    )
    trace.append(
        command(
            "shield_tap_jump",
            main_y=1.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("shield_tap_jump_recovery", 110)

    # Common-data boundary and age controls. Controller-pipe values 0.83 and
    # 0.835 normalize just below and just above the 0.6625 tap-jump threshold.
    # The slow sweep ages four samples after first vertical tilt and must not
    # jump; the two-sample route reaches the same terminal value in-window.
    repeat("settle_before_tap_jump_below_threshold", 10)
    trace.append(command("tap_jump_below_threshold", main_y=0.83))
    repeat("tap_jump_below_threshold_recovery", 110)

    repeat("settle_before_tap_jump_above_threshold", 10)
    trace.append(command("tap_jump_above_threshold", main_y=0.835))
    repeat("tap_jump_above_threshold_recovery", 110)

    repeat("settle_before_tap_jump_slow_sweep", 10)
    for y in (0.63, 0.68, 0.74, 0.79, 0.835):
        trace.append(command("tap_jump_slow_sweep", main_y=y))
    repeat("tap_jump_slow_sweep_recovery", 110)

    repeat("settle_before_tap_jump_two_sample", 10)
    for y in (0.63, 0.835):
        trace.append(command("tap_jump_two_sample", main_y=y))
    repeat("tap_jump_two_sample_recovery", 110)

    # RunBrake's common IASA is intentionally narrow: jump and crouch are
    # immediate, while guard and taunt are absent. Each route enters terminal
    # run, releases to RunBrake frame 1, then applies one fresh candidate input
    # on the following sample.
    # Keep the three grounded routes away from P2, then put the airborne route
    # last and end before landing. This prevents the still-unqualified player
    # push interaction from contaminating the RunBrake IASA comparison.
    repeat("recenter_before_run_brake_crouch", 25, main_x=0.0)
    repeat("run_brake_crouch_settle", 30)
    repeat("run_brake_crouch_run", 25, main_x=1.0)
    trace.append(command("run_brake_crouch_entry"))
    trace.append(command("run_brake_crouch", main_y=0.0))
    repeat("run_brake_crouch_recovery", 90)

    repeat("recenter_before_run_brake_guard", 25, main_x=0.0)
    repeat("run_brake_guard_settle", 30)
    repeat("run_brake_guard_run", 25, main_x=1.0)
    trace.append(command("run_brake_guard_entry"))
    trace.append(
        command(
            "run_brake_guard",
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("run_brake_guard_recovery", 90)

    repeat("recenter_before_run_brake_spot_dodge", 25, main_x=0.0)
    repeat("run_brake_spot_dodge_settle", 30)
    repeat("run_brake_spot_dodge_run", 25, main_x=1.0)
    trace.append(command("run_brake_spot_dodge_entry"))
    trace.append(
        command(
            "run_brake_spot_dodge",
            main_y=0.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("run_brake_spot_dodge_recovery", 90)

    # The preceding shield-plus-down route crouches and therefore finishes its
    # recovery 13.5 units earlier than a rejected input. Nineteen leftward frames
    # restore the same safe setup reached by the ordinary 25-frame recenter.
    repeat("recenter_before_run_brake_cstick_roll", 19, main_x=0.0)
    repeat("run_brake_cstick_roll_settle", 30)
    repeat("run_brake_cstick_roll_run", 25, main_x=1.0)
    trace.append(command("run_brake_cstick_roll_entry"))
    trace.append(
        command(
            "run_brake_cstick_roll",
            c_x=1.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("run_brake_cstick_roll_recovery", 90)

    repeat("recenter_before_run_brake_cstick_spot", 25, main_x=0.0)
    repeat("run_brake_cstick_spot_settle", 30)
    repeat("run_brake_cstick_spot_run", 25, main_x=1.0)
    trace.append(command("run_brake_cstick_spot_entry"))
    trace.append(
        command(
            "run_brake_cstick_spot",
            c_y=0.0,
            left_shoulder=1.0,
            digital_left=True,
        )
    )
    repeat("run_brake_cstick_spot_recovery", 90)

    repeat("recenter_before_run_brake_taunt", 25, main_x=0.0)
    repeat("run_brake_taunt_settle", 30)
    repeat("run_brake_taunt_run", 25, main_x=1.0)
    trace.append(command("run_brake_taunt_entry"))
    trace.append(command("run_brake_taunt", taunt=True))
    repeat("run_brake_taunt_recovery", 90)

    repeat("recenter_before_run_brake_attack", 25, main_x=0.0)
    repeat("run_brake_attack_settle", 30)
    repeat("run_brake_attack_run", 25, main_x=1.0)
    trace.append(command("run_brake_attack_entry"))
    trace.append(command("run_brake_attack", attack=True))
    repeat("run_brake_attack_recovery", 90)

    repeat("recenter_before_run_brake_grab", 25, main_x=0.0)
    repeat("run_brake_grab_settle", 30)
    repeat("run_brake_grab_run", 25, main_x=1.0)
    trace.append(command("run_brake_grab_entry"))
    trace.append(command("run_brake_grab", grab=True))
    repeat("run_brake_grab_recovery", 90)

    repeat("recenter_before_run_brake_special", 25, main_x=0.0)
    repeat("run_brake_special_settle", 30)
    repeat("run_brake_special_run", 25, main_x=1.0)
    trace.append(command("run_brake_special_entry"))
    trace.append(command("run_brake_special", special=True))
    repeat("run_brake_special_recovery", 90)

    # RunBrake's command-variable gate eventually admits held opposite stick
    # into TurnRun. Hold through the full animation so the executable reveals
    # the first legal displayed frame and the resumed TurnRun animation frame.
    repeat("recenter_before_run_brake_reverse", 25, main_x=0.0)
    repeat("run_brake_reverse_settle", 30)
    repeat("run_brake_reverse_run", 25, main_x=1.0)
    trace.append(command("run_brake_reverse_entry"))
    repeat("run_brake_reverse_hold", 35, main_x=0.0)
    repeat("run_brake_reverse_recovery", 90)

    # TurnRun's neutral recovery already finishes at center-left. Settle there
    # instead of applying the ordinary leftward recenter, which would leave FD.
    repeat("recenter_before_run_brake_jump", 30)
    repeat("run_brake_jump_run", 25, main_x=1.0)
    trace.append(command("run_brake_jump_entry"))
    trace.append(command("run_brake_jump", jump=True))
    repeat("run_brake_jump_recovery", 12)

    # Keep character-specific Falcon Kick bodies at the end so their motion
    # cannot alter the preconditions of later shared-movement routes. Each
    # case creates space on the left, turns back toward stage center, and then
    # qualifies only the common down-special interrupt entry.
    repeat("settle_before_down_special_slice", 100)
    repeat("recenter_before_crouch_start_down_special", 25, main_x=0.0)
    repeat("settle_before_crouch_start_down_special", 30)
    trace.append(
        command(
            "face_right_before_crouch_start_down_special",
            main_x=0.65,
        )
    )
    repeat("face_right_settle_before_crouch_start_down_special", 15)
    repeat("crouch_start_before_down_special", 2, main_y=0.0)
    trace.append(command("crouch_start_down_special", main_y=0.0, special=True))
    repeat("crouch_start_down_special_recovery", 100)

    repeat("recenter_before_crouch_wait_down_special", 25, main_x=0.0)
    repeat("settle_before_crouch_wait_down_special", 30)
    trace.append(
        command(
            "face_right_before_crouch_wait_down_special",
            main_x=0.65,
        )
    )
    repeat("face_right_settle_before_crouch_wait_down_special", 15)
    repeat("crouch_wait_before_down_special", 20, main_y=0.0)
    trace.append(command("crouch_wait_down_special", main_y=0.0, special=True))
    repeat("crouch_wait_down_special_recovery", 100)

    repeat("recenter_before_crouch_end_down_special", 25, main_x=0.0)
    repeat("settle_before_crouch_end_down_special", 30)
    trace.append(
        command(
            "face_right_before_crouch_end_down_special",
            main_x=0.65,
        )
    )
    repeat("face_right_settle_before_crouch_end_down_special", 15)
    repeat("crouch_end_before_down_special", 20, main_y=0.0)
    trace.append(command("crouch_end_down_special_release"))
    trace.append(command("crouch_end_down_special", main_y=0.0, special=True))
    repeat("crouch_end_down_special_recovery", 100)
    return trace


def read_shield_memory_probe(memory_engine: object) -> dict[str, object]:
    """Read Falcon's live guard state and shield-joint world transform."""

    player_slot = 0x80453080
    transformed = memory_engine.read_byte(player_slot + 0x0C)
    fighter_gobj = memory_engine.read_word(player_slot + 0xB0 + 4 * transformed)
    fighter = memory_engine.read_word(fighter_gobj + 0x2C)
    shield_joint = memory_engine.read_word(fighter + 0x19C0)
    matrix = [
        memory_engine.read_float(shield_joint + 0x44 + 4 * index) for index in range(12)
    ]
    return {
        "fighter_address": fighter,
        "shield_joint_address": shield_joint,
        "guard_magnitude": memory_engine.read_float(fighter + 0x2344),
        "guard_angle_degrees": memory_engine.read_float(fighter + 0x2348),
        "shield_health": memory_engine.read_float(fighter + 0x1998),
        "lightshield_amount": memory_engine.read_float(fighter + 0x199C),
        "shield_radius": memory_engine.read_float(fighter + 0x19E0),
        "initial_shield_size": memory_engine.read_float(fighter + 0x1A0),
        "fighter_scale": [
            memory_engine.read_float(fighter + 0x34 + 4 * index) for index in range(3)
        ],
        "fighter_position": [
            memory_engine.read_float(fighter + 0xB0 + 4 * index) for index in range(3)
        ],
        "shield_position": [
            memory_engine.read_float(fighter + 0x19C8 + 4 * index) for index in range(3)
        ],
        "shield_joint_scale": [
            memory_engine.read_float(shield_joint + 0x2C + 4 * index)
            for index in range(3)
        ],
        "shield_joint_translate": [
            memory_engine.read_float(shield_joint + 0x38 + 4 * index)
            for index in range(3)
        ],
        "shield_joint_matrix": matrix,
    }


def read_damage_memory_probe(memory_engine: object) -> dict[str, object]:
    """Read Falcon's live damage state and the common damage constants."""

    player_slot = 0x80453080
    transformed = memory_engine.read_byte(player_slot + 0x0C)
    fighter_gobj = memory_engine.read_word(player_slot + 0xB0 + 4 * transformed)
    fighter = memory_engine.read_word(fighter_gobj + 0x2C)
    common = memory_engine.read_word(0x804D6554)
    source_gobj = memory_engine.read_word(fighter + 0x1868)
    source_fighter = memory_engine.read_word(source_gobj + 0x2C) if source_gobj else 0
    source_hitboxes = []
    if source_fighter:
        for index in range(4):
            hitbox = source_fighter + 0x914 + index * 0x138
            source_hitboxes.append(
                {
                    "state": memory_engine.read_word(hitbox),
                    "hit_id": memory_engine.read_word(hitbox + 0x04),
                    "damage_count": memory_engine.read_word(hitbox + 0x08),
                    "damage": memory_engine.read_float(hitbox + 0x0C),
                    "angle": memory_engine.read_word(hitbox + 0x20),
                    "knockback_growth": memory_engine.read_word(hitbox + 0x24),
                    "weight_set_knockback": memory_engine.read_word(hitbox + 0x28),
                    "base_knockback": memory_engine.read_word(hitbox + 0x2C),
                }
            )
    common_float_offsets = {
        "weight_scale": 0x0F4,
        "weight_base": 0x0F8,
        "launch_velocity_scale": 0x100,
        "minimum_knockback": 0x104,
        "maximum_knockback": 0x108,
        "damage_term_a": 0x110,
        "damage_term_b": 0x114,
        "weight_set_damage": 0x118,
        "weight_term_scale": 0x11C,
        "base_term": 0x120,
        "crouch_knockback_scale": 0x124,
        "collision_knockback_threshold": 0x12C,
        "sakurai_air_angle_radians": 0x144,
        "sakurai_max_ground_angle": 0x148,
        "sakurai_low_knockback": 0x14C,
        "sakurai_high_knockback": 0x150,
        "hitstun_scale": 0x154,
        "air_motion_knockback_scale": 0x190,
        "maximum_hitlag": 0x194,
        "hitlag_damage_scale": 0x198,
        "hitlag_base": 0x19C,
        "crouch_hitlag_scale": 0x1A0,
        "di_max_angle_degrees": 0x1A8,
        "v_cancel_scale": 0x1AC,
        "ground_knockback_friction_scale": 0x200,
        "air_knockback_decay": 0x204,
        "sdi_minimum_stick_magnitude": 0x4B0,
        "sdi_position_scale": 0x4B8,
        "asdi_position_scale": 0x4BC,
    }
    return {
        "fighter_address": fighter,
        "common_data_address": common,
        "source_gobj_address": source_gobj,
        "source_fighter_address": source_fighter,
        "source_hitboxes": source_hitboxes,
        "fighter_weight": memory_engine.read_float(fighter + 0x198),
        "knockback_velocity": [
            memory_engine.read_float(fighter + 0x8C + 4 * index) for index in range(3)
        ],
        "ground_knockback_velocity": memory_engine.read_float(fighter + 0xF0),
        "damage_percent": memory_engine.read_float(fighter + 0x1830),
        "damage_this_hit": memory_engine.read_float(fighter + 0x1838),
        "knockback_angle": memory_engine.read_word(fighter + 0x1848),
        "knockback_applied": memory_engine.read_float(fighter + 0x1850),
        "knockback_magnitude": memory_engine.read_float(fighter + 0x18A4),
        "knockback_applied_latched": memory_engine.read_float(fighter + 0x18A8),
        "hitlag_frames": memory_engine.read_float(fighter + 0x195C),
        "common": {
            name: memory_engine.read_float(common + offset)
            for name, offset in common_float_offsets.items()
        },
        "sdi_stick_window": memory_engine.read_word(common + 0x4B4),
    }


def read_fighter_address(memory_engine: object, player_index: int) -> int:
    """Resolve one of Melee's six StaticPlayer slots to its live Fighter."""

    static_player_stride = 0xE90
    player_slot = 0x80453080 + player_index * static_player_stride
    transformed = memory_engine.read_byte(player_slot + 0x0C)
    fighter_gobj = memory_engine.read_word(player_slot + 0xB0 + 4 * transformed)
    return memory_engine.read_word(fighter_gobj + 0x2C)


def read_fighter_hurt_capsules(
    memory_engine: object, fighter: int
) -> list[dict[str, object]]:
    """Read live pose-transformed FighterHurtCapsule values."""

    def read_vector(address: int) -> list[float]:
        return [memory_engine.read_float(address + 4 * axis) for axis in range(3)]

    def transform(matrix: list[float], value: list[float]) -> list[float]:
        return [
            matrix[row * 4] * value[0]
            + matrix[row * 4 + 1] * value[1]
            + matrix[row * 4 + 2] * value[2]
            + matrix[row * 4 + 3]
            for row in range(3)
        ]

    hurtboxes = []
    hurtbox_count = memory_engine.read_byte(fighter + 0x119E)
    if hurtbox_count > 15:
        raise RuntimeError(f"invalid Fighter hurt-capsule count: {hurtbox_count}")
    for index in range(hurtbox_count):
        hurtbox = fighter + 0x11A0 + index * 0x4C
        bone = memory_engine.read_word(hurtbox + 0x20)
        offset_a = read_vector(hurtbox + 0x04)
        offset_b = read_vector(hurtbox + 0x10)
        bone_matrix = [
            memory_engine.read_float(bone + 0x44 + 4 * element) for element in range(12)
        ]
        hurtboxes.append(
            {
                "state": memory_engine.read_word(hurtbox),
                "radius": memory_engine.read_float(hurtbox + 0x1C),
                "offset_a": offset_a,
                "offset_b": offset_b,
                "position_a": transform(bone_matrix, offset_a),
                "position_b": transform(bone_matrix, offset_b),
                "collision_position_a": read_vector(hurtbox + 0x28),
                "collision_position_b": read_vector(hurtbox + 0x34),
                "bone_index": memory_engine.read_word(hurtbox + 0x40),
                "height": memory_engine.read_word(hurtbox + 0x44),
                "grabbable": memory_engine.read_word(hurtbox + 0x48),
            }
        )
    return hurtboxes


def read_hitbox_memory_probe(memory_engine: object) -> dict[str, object]:
    """Read both Falcons' live attack and hurt-capsule geometry."""

    common = memory_engine.read_word(0x804D6554)

    def read_ecb(fighter_address: int) -> dict[str, list[float]]:
        ecb = fighter_address + 0x794
        return {
            name: [
                memory_engine.read_float(ecb + offset),
                memory_engine.read_float(ecb + offset + 4),
            ]
            for name, offset in (
                ("top", 0x00),
                ("bottom", 0x08),
                ("right", 0x10),
                ("left", 0x18),
            )
        }

    fighter = read_fighter_address(memory_engine, 0)
    opponent = read_fighter_address(memory_engine, 1)
    hitboxes = []
    for index in range(4):
        hitbox = fighter + 0x914 + index * 0x138
        hitboxes.append(
            {
                "state": memory_engine.read_word(hitbox),
                "hit_id": memory_engine.read_word(hitbox + 0x04),
                "damage_count": memory_engine.read_word(hitbox + 0x08),
                "damage": memory_engine.read_float(hitbox + 0x0C),
                "bone_offset": [
                    memory_engine.read_float(hitbox + 0x10 + 4 * axis)
                    for axis in range(3)
                ],
                "radius": memory_engine.read_float(hitbox + 0x1C),
                "angle": memory_engine.read_word(hitbox + 0x20),
                "knockback_growth": memory_engine.read_word(hitbox + 0x24),
                "weight_set_knockback": memory_engine.read_word(hitbox + 0x28),
                "base_knockback": memory_engine.read_word(hitbox + 0x2C),
                "element": memory_engine.read_word(hitbox + 0x30),
                "position": [
                    memory_engine.read_float(hitbox + 0x4C + 4 * axis)
                    for axis in range(3)
                ],
                "previous_position": [
                    memory_engine.read_float(hitbox + 0x58 + 4 * axis)
                    for axis in range(3)
                ],
            }
        )
    return {
        "fighter_address": fighter,
        "fighter_position": [
            memory_engine.read_float(fighter + 0xB0 + 4 * axis) for axis in range(3)
        ],
        "fighter_scale": [
            memory_engine.read_float(fighter + 0x34 + 4 * axis) for axis in range(3)
        ],
        "hitboxes": hitboxes,
        "fighter_hurtboxes": read_fighter_hurt_capsules(memory_engine, fighter),
        "fighter_ecb": read_ecb(fighter),
        "fighter_ledge_snap": [
            memory_engine.read_float(fighter + 0x744),
            memory_engine.read_float(fighter + 0x748),
            memory_engine.read_float(fighter + 0x74C),
        ],
        "fighter_collision_contact": [
            memory_engine.read_float(fighter + 0x830 + 4 * axis) for axis in range(3)
        ],
        "fighter_collision_positions": {
            name: [
                memory_engine.read_float(fighter + offset + 4 * axis)
                for axis in range(3)
            ]
            for name, offset in (
                ("current", 0x6F4),
                ("previous", 0x700),
                ("last", 0x70C),
            )
        },
        "fighter_ledge_ids": [
            memory_engine.read_word(fighter + 0x730),
            memory_engine.read_word(fighter + 0x734),
        ],
        "fighter_environment_flags": memory_engine.read_word(fighter + 0x824),
        "fighter_previous_environment_flags": memory_engine.read_word(fighter + 0x828),
        "fighter_command_variables": [
            memory_engine.read_word(fighter + 0x2200 + 4 * index) for index in range(4)
        ],
        "fighter_captain_specialhi_flags": memory_engine.read_byte(fighter + 0x2342),
        "fighter_captain_specialhi_velocity": [
            memory_engine.read_float(fighter + 0x2344),
            memory_engine.read_float(fighter + 0x2348),
        ],
        "opponent_fighter_address": opponent,
        "opponent_fighter_position": [
            memory_engine.read_float(opponent + 0xB0 + 4 * axis) for axis in range(3)
        ],
        "opponent_damage_percent_internal": memory_engine.read_float(opponent + 0x1830),
        "opponent_damage_percent_temp": memory_engine.read_float(opponent + 0x1838),
        "opponent_knockback_applied": memory_engine.read_float(opponent + 0x1850),
        "opponent_knockback_magnitude": memory_engine.read_float(opponent + 0x18A4),
        "opponent_knockback_applied_latched": memory_engine.read_float(
            opponent + 0x18A8
        ),
        "opponent_damage_state_ticks": memory_engine.read_word(opponent + 0x2340),
        "throw_weight": memory_engine.read_float(common + 0x10C),
        "opponent_hurtboxes": read_fighter_hurt_capsules(memory_engine, opponent),
        "opponent_ecb": read_ecb(opponent),
    }


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def wait_for_udp_listener(port: int, timeout: float) -> None:
    """Wait until Dolphin owns the local Slippi spectator port.

    Calling ENet's connect before Dolphin binds the port leaves the libmelee
    host with a stale peer. Detecting ownership first makes the single connect
    attempt deterministic on slower AppImage/WSL starts.
    """

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            probe.bind(("127.0.0.1", port))
        except OSError:
            return
        finally:
            probe.close()
        time.sleep(0.05)
    raise RuntimeError(f"Dolphin did not bind Slippi UDP port {port}")


def choose_match(
    gamestate: melee.GameState,
    player_one: melee.Controller,
    player_two: melee.Controller,
    stage: melee.Stage,
    opponent: melee.Character = melee.Character.FOX,
    stage_cursor: tuple[float, float] | None = None,
) -> None:
    if gamestate.menu_state in (
        melee.Menu.CHARACTER_SELECT,
        melee.Menu.SLIPPI_ONLINE_CSS,
    ):
        melee.MenuHelper.choose_character(
            opponent,
            gamestate,
            player_two,
            costume=0,
            swag=False,
            start=False,
        )
        melee.MenuHelper.choose_character(
            melee.Character.CPTFALCON,
            gamestate,
            player_one,
            costume=0,
            swag=False,
            start=True,
        )
    elif gamestate.menu_state == melee.Menu.STAGE_SELECT:
        player_two.release_all()
        if stage_cursor is None:
            melee.MenuHelper.choose_stage(stage, gamestate, player_one)
        else:
            choose_stage_at(gamestate, player_one, *stage_cursor)
    elif gamestate.menu_state in (melee.Menu.PRESS_START, melee.Menu.MAIN_MENU):
        player_two.release_all()
        melee.MenuHelper.choose_versus_mode(gamestate, player_one)
    else:
        player_one.release_all()
        player_two.release_all()


def choose_stage_at(
    gamestate: melee.GameState,
    controller: melee.Controller,
    target_x: float,
    target_y: float,
) -> None:
    """Choose a vanilla stage at a source-derived stage-select coordinate."""

    if gamestate.frame < 20:
        controller.release_all()
        return
    cursor = gamestate.players[controller.port].cursor
    wiggleroom = 1.5
    controller.release_button(melee.Button.BUTTON_A)
    if cursor.y < target_y - wiggleroom:
        controller.tilt_analog(melee.Button.BUTTON_MAIN, 0.5, 1.0)
    elif cursor.y > target_y + wiggleroom:
        controller.tilt_analog(melee.Button.BUTTON_MAIN, 0.5, 0.0)
    elif cursor.x < target_x - wiggleroom:
        controller.tilt_analog(melee.Button.BUTTON_MAIN, 1.0, 0.5)
    elif cursor.x > target_x + wiggleroom:
        controller.tilt_analog(melee.Button.BUTTON_MAIN, 0.0, 0.5)
    else:
        controller.press_button(melee.Button.BUTTON_A)


def wait_for_grounded_capsule(
    console: melee.Console,
    player_one: melee.Controller,
    player_two: melee.Controller,
    gamestate: melee.GameState,
) -> melee.GameState:
    """Advance neutral frames until the native Capsule has stopped bouncing."""

    for _ in range(600):
        if len(gamestate.projectiles) == 1:
            item = gamestate.projectiles[0]
            if (
                int(item.type.value) == 255
                and int(item.subtype) == 0
                and float(item.speed.x) == 0.0
                and float(item.speed.y) == 0.0
            ):
                return gamestate
        player_one.release_all()
        player_two.release_all()
        next_state = console.step()
        if next_state is None:
            raise RuntimeError("missing state while waiting for grounded Capsule")
        gamestate = next_state
    raise RuntimeError("native Capsule did not settle within 600 frames")


def hook_memory_engine(dolphin: Path) -> object:
    """Hook DME, including extracted-AppImage process-name discovery."""

    if "DME_DOLPHIN_PROCESS_NAME" not in os.environ and (
        dolphin.suffix.lower() == ".appimage"
        or dolphin.name.lower() in {"apprun", "apprun.wrapped"}
    ):
        os.environ["DME_DOLPHIN_PROCESS_NAME"] = "AppRun.wrapped"
    try:
        import dolphin_memory_engine as memory_engine_module
    except ImportError as error:
        raise RuntimeError("memory probes require dolphin-memory-engine") from error
    memory_engine_module.hook()
    return memory_engine_module


def wait_for_memory_engine_hook(dolphin: Path) -> object:
    memory_engine = hook_memory_engine(dolphin)
    hook_deadline = time.monotonic() + 10.0
    while not memory_engine.is_hooked() and time.monotonic() < hook_deadline:
        time.sleep(0.05)
        memory_engine.hook()
    if not memory_engine.is_hooked():
        raise RuntimeError(
            "dolphin-memory-engine could not hook the oracle process: "
            f"{memory_engine.get_status()}"
        )
    return memory_engine


def set_native_capsule_preferences(memory_engine: object) -> None:
    """Set the source item-switch preference to Capsule at Very High."""

    # gm_80167BC8 maps the item-switch UI mask through lbl_803B7844 when it
    # constructs StartMeleeRules.x20. Capsule (It_Kind_Capsule == 0) is UI
    # bit 29, not bit 0.
    capsule_item_switch_mask = (1 << 29).to_bytes(8, "big")
    main_data = int(memory_engine.read_word(0x804D3EE0))
    if not 0x80000000 <= main_data < 0x81800000:
        raise RuntimeError(f"invalid gmMainLib_804D3EE0 pointer: 0x{main_data:08x}")
    item_preferences = main_data + 0x1898 + 0x448
    memory_engine.write_byte(item_preferences, 4)
    memory_engine.write_bytes(
        item_preferences + 8,
        capsule_item_switch_mask,
    )
    if (
        int(memory_engine.read_byte(item_preferences)) != 4
        or bytes(memory_engine.read_bytes(item_preferences + 8, 8))
        != capsule_item_switch_mask
    ):
        raise RuntimeError("native item preferences did not retain write")


def read_native_item_rules(memory_engine: object) -> dict[str, object]:
    """Read the source preferences and isolated item-rule accessor code."""

    main_data = int(memory_engine.read_word(0x804D3EE0))
    if not 0x80000000 <= main_data < 0x81800000:
        raise RuntimeError(f"invalid gmMainLib_804D3EE0 pointer: 0x{main_data:08x}")
    item_preferences = main_data + 0x1898 + 0x448
    return {
        "spawns_enabled": int(memory_engine.read_word(0x8049FAA0 + 8)),
        "rule_accessor_code": {
            "frequency": bytes(memory_engine.read_bytes(0x8016AE80, 8)).hex(),
            "runtime_mask": bytes(memory_engine.read_bytes(0x8016AEA4, 12)).hex(),
        },
        "preferences": {
            "frequency": int(memory_engine.read_byte(item_preferences)),
            "item_switch_mask": bytes(
                memory_engine.read_bytes(item_preferences + 8, 8)
            ).hex(),
        },
    }


def enable_native_capsule_gecko(console: object) -> None:
    """Configure the native spawner despite Slippi's items-off rule override."""

    game_settings = (
        Path(console._get_dolphin_home_path()) / "GameSettings" / "GALE01r2.ini"
    )
    if not game_settings.is_file():
        raise FileNotFoundError(f"missing libmelee Gecko config: {game_settings}")
    # Slippi's recording rules replace StartMeleeRules.xB with -1 during the
    # match handoff, after vanilla preferences are copied. Override only the
    # two source accessors consumed by it_8026D018 and the ambient spawner:
    # frequency 4 (Very High), and runtime item-kind bit 0 (Capsule).
    enabled_header = "[Gecko_Enabled]\n"
    gecko_header = "[Gecko]\n"
    code_name = "$Oracle: Native Capsule Spawning"
    text = game_settings.read_text(encoding="ascii")
    if enabled_header not in text or gecko_header not in text:
        raise RuntimeError("unexpected libmelee Gecko config structure")
    text = text.replace(
        enabled_header,
        f"{enabled_header}{code_name}\n",
        1,
    )
    text = text.replace(
        gecko_header,
        (
            f"{gecko_header}{code_name}\n"
            "0416AE80 38600004 # Oracle/Items/NativeFrequencyVeryHigh.asm\n"
            "0416AE84 4E800020\n"
            "0416AEA4 38600000 # Oracle/Items/NativeMaskCapsuleHi.asm\n"
            "0416AEA8 38800001\n"
            "0416AEAC 4E800020\n"
        ),
        1,
    )
    game_settings.write_text(text, encoding="ascii", newline="\n")


def capture(args: argparse.Namespace) -> dict[str, object]:
    dolphin = Path(args.dolphin).resolve()
    iso = Path(args.iso).resolve()
    if dolphin.is_dir():
        if not (dolphin / "dolphin-emu").is_file():
            raise FileNotFoundError(f"missing dolphin-emu under {dolphin}")
    elif not dolphin.is_file():
        raise FileNotFoundError(f"missing Dolphin executable or AppImage: {dolphin}")
    if not iso.is_file():
        raise FileNotFoundError(f"missing GALE01 image: {iso}")
    wall_geometry_route = bool(
        args.special_geometry_only
        and args.special_geometry_move
        and "down_ground_wall" in args.special_geometry_move
    )
    if wall_geometry_route and set(args.special_geometry_move) != {"down_ground_wall"}:
        raise ValueError(
            "down_ground_wall uses Hyrule Temple and must be captured alone"
        )

    console = melee.Console(
        path=str(dolphin),
        blocking_input=True,
        polling_mode=False,
        tmp_home_directory=True,
        copy_home_directory=False,
        fullscreen=False,
        gfx_backend="",
        disable_audio=True,
        save_replays=False,
    )
    if args.enable_items:
        enable_native_capsule_gecko(console)
    player_one = melee.Controller(console, 1, melee.ControllerType.STANDARD)
    player_two = melee.Controller(console, 2, melee.ControllerType.STANDARD)
    started_at = time.monotonic()
    memory_engine = None

    try:
        environment = None
        if dolphin.is_file() and dolphin.name.lower().endswith(".appimage"):
            # WSL commonly lacks FUSE. AppImage's supported extraction fallback
            # keeps the oracle runnable without installing a kernel component.
            environment = {"APPIMAGE_EXTRACT_AND_RUN": "1"}
        if args.batch:
            executable = dolphin / "dolphin-emu" if dolphin.is_dir() else dolphin
            dolphin_environment = os.environ.copy()
            if environment is not None:
                dolphin_environment.update(environment)
            # libmelee 0.44.0 does not expose Dolphin's --batch or null-video
            # switches. Preserve its process/home ownership while extending
            # only the launch command used by this reproducible oracle route.
            console._process = subprocess.Popen(
                [
                    str(executable),
                    "--batch",
                    "--video_backend=Null",
                    "--exec",
                    str(iso),
                    "--user",
                    console._get_dolphin_home_path(),
                ],
                env=dolphin_environment,
            )
        else:
            console.run(iso_path=str(iso), environment_vars=environment)
        wait_for_udp_listener(console.slippi_port, 30.0)
        if not console.connect():
            raise RuntimeError("Dolphin Slippi stream did not connect")
        if not player_one.connect() or not player_two.connect():
            raise RuntimeError("Dolphin controller pipes did not connect")

        gamestate = None
        items_configured = False
        while time.monotonic() - started_at < args.menu_timeout:
            gamestate = console.step()
            if gamestate is None:
                continue
            if (
                args.enable_items
                and not items_configured
                and gamestate.menu_state == melee.Menu.MAIN_MENU
            ):
                memory_engine = wait_for_memory_engine_hook(dolphin)
                set_native_capsule_preferences(memory_engine)
                items_configured = True
            if (
                gamestate.menu_state in (melee.Menu.IN_GAME, melee.Menu.SUDDEN_DEATH)
                and gamestate.frame >= args.start_frame
                and 1 in gamestate.players
                and gamestate.players[1].character == melee.Character.CPTFALCON
            ):
                break
            choose_match(
                gamestate,
                player_one,
                player_two,
                (
                    melee.Stage.BATTLEFIELD
                    if args.platform_only
                    else melee.Stage.FINAL_DESTINATION
                ),
                (
                    melee.Character.CPTFALCON
                    if (
                        args.push_only
                        or args.shield_hit_only
                        or args.damage_hit_only
                        or args.hitbox_geometry_only
                        or args.throw_geometry_only
                        or args.special_geometry_only
                    )
                    else melee.Character.FOX
                ),
                stage_cursor=(
                    HYRULE_TEMPLE_STAGE_CURSOR if wall_geometry_route else None
                ),
            )
        else:
            state = None if gamestate is None else str(gamestate.menu_state)
            raise TimeoutError(f"Dolphin match setup timed out in {state}")

        if (
            args.memory_probe_shield
            or args.memory_probe_damage
            or args.memory_probe_hitbox
        ):
            if memory_engine is None:
                memory_engine = wait_for_memory_engine_hook(dolphin)

        if (
            args.special_geometry_move
            and "side_ground_item_hit" in args.special_geometry_move
        ):
            gamestate = wait_for_grounded_capsule(
                console,
                player_one,
                player_two,
                gamestate,
            )

        player_one.release_all()
        player_two.release_all()
        rows: list[dict[str, object]] = []
        origin_x: float | None = None
        origin_two_x: float | None = None
        falcon_frame_data = (
            json.loads(args.falcon_frame_data.read_text(encoding="utf-8"))
            if args.falcon_frame_data is not None
            else None
        )
        trace = input_trace(
            platform_only=args.platform_only,
            push_only=args.push_only,
            shield_only=args.shield_only,
            shield_geometry_only=args.shield_geometry_only,
            shield_geometry_sweep_only=args.shield_geometry_sweep_only,
            shield_hit_only=args.shield_hit_only,
            damage_hit_only=args.damage_hit_only,
            attack_iasa_only=args.attack_iasa_only,
            ground_attack_iasa_only=args.ground_attack_iasa_only,
            hitbox_geometry_only=args.hitbox_geometry_only,
            throw_geometry_only=args.throw_geometry_only,
            special_geometry_only=args.special_geometry_only,
            ground_attack_moves=(
                tuple(args.ground_attack_move) if args.ground_attack_move else None
            ),
            special_geometry_moves=(
                tuple(args.special_geometry_move)
                if args.special_geometry_move
                else None
            ),
            falcon_frame_data=falcon_frame_data,
            shield_hit_pressure=args.shield_hit_pressure,
        )
        pipeline_delay = 2
        commands = trace + [
            {
                **trace[0],
                "label": "pipeline_drain",
                "fighter_x_from_item_offset": None,
                "opponent_x_from_item_offset": None,
            }
            for _ in range(pipeline_delay)
        ]
        for command_index, sample in enumerate(commands):
            player_one.release_all()
            player_two.release_all()
            player_one.tilt_analog(
                melee.Button.BUTTON_MAIN,
                float(sample["main_x"]),
                float(sample["main_y"]),
            )
            player_one.tilt_analog(
                melee.Button.BUTTON_C,
                float(sample["c_x"]),
                float(sample["c_y"]),
            )
            player_one.press_shoulder(
                melee.Button.BUTTON_L,
                float(sample["left_shoulder"]),
            )
            player_one.press_shoulder(
                melee.Button.BUTTON_R,
                float(sample["right_shoulder"]),
            )
            if bool(sample["digital_left"]):
                player_one.press_button(melee.Button.BUTTON_L)
            if bool(sample["digital_right"]):
                player_one.press_button(melee.Button.BUTTON_R)
            if bool(sample["jump"]):
                player_one.press_button(melee.Button.BUTTON_X)
            if bool(sample["attack"]):
                player_one.press_button(melee.Button.BUTTON_A)
            if bool(sample["special"]):
                player_one.press_button(melee.Button.BUTTON_B)
            if bool(sample["grab"]):
                player_one.press_button(melee.Button.BUTTON_Z)
            if bool(sample["taunt"]):
                player_one.press_button(melee.Button.BUTTON_D_UP)
            player_two.tilt_analog(
                melee.Button.BUTTON_MAIN,
                float(sample["opponent_main_x"]),
                0.5,
            )
            if bool(sample["opponent_attack"]):
                player_two.press_button(melee.Button.BUTTON_A)
            if bool(sample["opponent_jump"]):
                player_two.press_button(melee.Button.BUTTON_X)
            fighter_x_override = sample["fighter_x_override"]
            opponent_x_override = sample["opponent_x_override"]
            if sample["fighter_x_from_item_offset"] is not None:
                if not gamestate.projectiles:
                    raise RuntimeError(
                        "item-relative fighter position requires a live item"
                    )
                fighter_x_override = float(gamestate.projectiles[0].position.x) + float(
                    sample["fighter_x_from_item_offset"]
                )
            if sample["opponent_x_from_item_offset"] is not None:
                if not gamestate.projectiles:
                    raise RuntimeError(
                        "item-relative opponent position requires a live item"
                    )
                opponent_x_override = float(
                    gamestate.projectiles[0].position.x
                ) + float(sample["opponent_x_from_item_offset"])
            position_overrides = (
                (0, 0xB0, fighter_x_override),
                (0, 0xB4, sample["fighter_y_override"]),
                (1, 0xB0, opponent_x_override),
                (1, 0xB4, sample["opponent_y_override"]),
            )
            if any(value is not None for _, _, value in position_overrides):
                if memory_engine is None:
                    raise RuntimeError(
                        "fighter position override requires a memory probe"
                    )
                fighter_addresses = (
                    read_fighter_address(memory_engine, 0),
                    read_fighter_address(memory_engine, 1),
                )
                for fighter_index, offset, value in position_overrides:
                    if value is not None:
                        memory_engine.write_float(
                            fighter_addresses[fighter_index] + offset,
                            float(value),
                        )
            gamestate = console.step()
            if (
                gamestate is None
                or 1 not in gamestate.players
                or 2 not in gamestate.players
            ):
                raise RuntimeError(
                    f"missing player state at command frame {command_index}"
                )
            if command_index < pipeline_delay:
                continue
            index = command_index - pipeline_delay
            scheduled = trace[index]
            player = gamestate.players[1]
            player_two_state = gamestate.players[2]
            observed_x = float(player.controller_state.main_stick[0])
            observed_y = float(player.controller_state.main_stick[1])
            observed_c_x = float(player.controller_state.c_stick[0])
            observed_c_y = float(player.controller_state.c_stick[1])
            observed_left_shoulder = float(player.controller_state.l_shoulder)
            observed_right_shoulder = float(player.controller_state.r_shoulder)
            observed_digital_left = bool(
                player.controller_state.button[melee.Button.BUTTON_L]
            )
            observed_digital_right = bool(
                player.controller_state.button[melee.Button.BUTTON_R]
            )
            observed_jump = bool(
                player.controller_state.button[melee.Button.BUTTON_X]
                or player.controller_state.button[melee.Button.BUTTON_Y]
            )
            observed_attack = bool(
                player.controller_state.button[melee.Button.BUTTON_A]
            )
            observed_special = bool(
                player.controller_state.button[melee.Button.BUTTON_B]
            )
            observed_grab = bool(player.controller_state.button[melee.Button.BUTTON_Z])
            observed_taunt = bool(
                player.controller_state.button[melee.Button.BUTTON_D_UP]
            )
            observed_opponent_x = float(player_two_state.controller_state.main_stick[0])
            observed_opponent_attack = bool(
                player_two_state.controller_state.button[melee.Button.BUTTON_A]
            )
            requested_x = float(scheduled["main_x"])
            requested_y = float(scheduled["main_y"])
            requested_c_x = float(scheduled["c_x"])
            requested_c_y = float(scheduled["c_y"])
            axis_aligned = (
                (requested_x == 0.5 and abs(observed_x - 0.5) <= 0.02)
                or (abs(requested_x - 0.5) <= 0.02 and abs(observed_x - 0.5) <= 0.02)
                or (requested_x < 0.5 and observed_x < 0.5)
                or (requested_x > 0.5 and observed_x > 0.5)
            ) and (
                (requested_y == 0.5 and abs(observed_y - 0.5) <= 0.02)
                or (abs(requested_y - 0.5) <= 0.02 and abs(observed_y - 0.5) <= 0.02)
                or (requested_y < 0.5 and observed_y < 0.5)
                or (requested_y > 0.5 and observed_y > 0.5)
            )
            if args.shield_geometry_sweep_only:
                # The game's per-axis dead zone intentionally collapses small
                # sweep components to neutral. The recorded post-frame sample
                # remains the authoritative input for this extraction route.
                axis_aligned = True
            c_axis_aligned = (
                (requested_c_x == 0.5 and abs(observed_c_x - 0.5) <= 0.02)
                or (requested_c_x < 0.5 and observed_c_x < 0.5)
                or (requested_c_x > 0.5 and observed_c_x > 0.5)
            ) and (
                (requested_c_y == 0.5 and abs(observed_c_y - 0.5) <= 0.02)
                or (requested_c_y < 0.5 and observed_c_y < 0.5)
                or (requested_c_y > 0.5 and observed_c_y > 0.5)
            )
            # The Slippi post-frame payload exposes the aggregate analog
            # shoulder pressure on both ControllerState shoulder fields. The
            # digital L/R bits remain independent, so validate the aggregate
            # analog amount and both digital clicks separately.
            requested_analog_shoulder = max(
                float(scheduled["left_shoulder"]),
                float(scheduled["right_shoulder"]),
            )
            expected_observed_shoulder = (
                0.35
                if bool(scheduled["grab"])
                else (
                    0.0
                    if requested_analog_shoulder <= 0.30
                    else requested_analog_shoulder
                )
            )
            shoulder_aligned = (
                abs(
                    max(observed_left_shoulder, observed_right_shoulder)
                    - expected_observed_shoulder
                )
                <= 0.10
                and observed_digital_left == bool(scheduled["digital_left"])
                and observed_digital_right == bool(scheduled["digital_right"])
                and observed_jump == bool(scheduled["jump"])
                and observed_attack == bool(scheduled["attack"])
                and observed_special == bool(scheduled["special"])
                and observed_grab == bool(scheduled["grab"])
                and observed_taunt == bool(scheduled["taunt"])
            )
            requested_opponent_x = float(scheduled["opponent_main_x"])
            opponent_axis_aligned = (
                (requested_opponent_x == 0.5 and abs(observed_opponent_x - 0.5) <= 0.02)
                or (requested_opponent_x < 0.5 and observed_opponent_x < 0.5)
                or (requested_opponent_x > 0.5 and observed_opponent_x > 0.5)
            )
            aligned = (
                axis_aligned
                and c_axis_aligned
                and shoulder_aligned
                and opponent_axis_aligned
                and observed_opponent_attack == bool(scheduled["opponent_attack"])
            )
            if not aligned:
                raise RuntimeError(
                    "controller/post-frame alignment failed at trace frame "
                    f"{index}: requested={scheduled} "
                    "observed="
                    f"x={observed_x} y={observed_y} "
                    f"cx={observed_c_x} cy={observed_c_y} "
                    f"l={observed_left_shoulder}/{observed_digital_left} "
                    f"r={observed_right_shoulder}/{observed_digital_right} "
                    f"jump={observed_jump} attack={observed_attack} "
                    f"special={observed_special} grab={observed_grab} "
                    f"taunt={observed_taunt} "
                    f"opponent_x={observed_opponent_x} "
                    f"opponent_attack={observed_opponent_attack}"
                )
            if origin_x is None:
                origin_x = player.position.x
            if origin_two_x is None:
                origin_two_x = player_two_state.position.x
            row = {
                "trace_frame": index,
                "game_frame": int(gamestate.frame),
                "label": scheduled["label"],
                "requested_main_x": requested_x,
                "requested_main_y": requested_y,
                "requested_c_x": requested_c_x,
                "requested_c_y": requested_c_y,
                "requested_left_shoulder": float(scheduled["left_shoulder"]),
                "requested_right_shoulder": float(scheduled["right_shoulder"]),
                "requested_digital_left": bool(scheduled["digital_left"]),
                "requested_digital_right": bool(scheduled["digital_right"]),
                "requested_jump": bool(scheduled["jump"]),
                "requested_attack": bool(scheduled["attack"]),
                "requested_special": bool(scheduled["special"]),
                "requested_grab": bool(scheduled["grab"]),
                "requested_taunt": bool(scheduled["taunt"]),
                "requested_opponent_main_x": requested_opponent_x,
                "requested_opponent_attack": bool(scheduled["opponent_attack"]),
                "requested_opponent_jump": bool(scheduled["opponent_jump"]),
                "requested_fighter_y_override": scheduled["fighter_y_override"],
                "requested_fighter_x_override": scheduled["fighter_x_override"],
                "requested_opponent_x_override": scheduled["opponent_x_override"],
                "requested_opponent_y_override": scheduled["opponent_y_override"],
                "observed_main_x": observed_x,
                "observed_main_y": observed_y,
                "observed_c_x": observed_c_x,
                "observed_c_y": observed_c_y,
                "observed_left_shoulder": observed_left_shoulder,
                "observed_right_shoulder": observed_right_shoulder,
                "observed_analog_shoulder": max(
                    observed_left_shoulder,
                    observed_right_shoulder,
                ),
                "observed_digital_left": observed_digital_left,
                "observed_digital_right": observed_digital_right,
                "observed_jump": observed_jump,
                "observed_attack": observed_attack,
                "observed_special": observed_special,
                "observed_grab": observed_grab,
                "observed_taunt": observed_taunt,
                "observed_opponent_main_x": observed_opponent_x,
                "observed_opponent_attack": observed_opponent_attack,
                "action": player.action.name,
                "action_value": int(player.action.value),
                "action_frame": float(player.action_frame),
                "facing": 1 if player.facing else -1,
                "grounded": bool(player.on_ground),
                "position_x": float(player.position.x),
                "position_x_from_origin": float(player.position.x - origin_x),
                "position_y": float(player.position.y),
                "ground_velocity_x": float(player.speed_ground_x_self),
                "air_velocity_x": float(player.speed_air_x_self),
                "velocity_y": float(player.speed_y_self),
                "attack_velocity_x": float(player.speed_x_attack),
                "attack_velocity_y": float(player.speed_y_attack),
                "hitlag_left": float(player.hitlag_left),
                "hitstun_left": float(player.hitstun_frames_left),
                "damage_percent": float(player.percent),
                "shield_health": float(player.shield_strength),
                "opponent_action": player_two_state.action.name,
                "opponent_action_value": int(player_two_state.action.value),
                "opponent_action_frame": float(player_two_state.action_frame),
                "opponent_facing": (1 if player_two_state.facing else -1),
                "opponent_grounded": bool(player_two_state.on_ground),
                "opponent_position_x": float(player_two_state.position.x),
                "opponent_position_x_from_origin": float(
                    player_two_state.position.x - origin_two_x
                ),
                "opponent_position_y": float(player_two_state.position.y),
                "opponent_ground_velocity_x": float(
                    player_two_state.speed_ground_x_self
                ),
                "opponent_air_velocity_x": float(player_two_state.speed_air_x_self),
                "opponent_velocity_y": float(player_two_state.speed_y_self),
                "opponent_attack_velocity_x": float(player_two_state.speed_x_attack),
                "opponent_attack_velocity_y": float(player_two_state.speed_y_attack),
                "opponent_hitlag_left": float(player_two_state.hitlag_left),
                "opponent_hitstun_left": float(player_two_state.hitstun_frames_left),
                "opponent_damage_percent": float(player_two_state.percent),
                "projectiles": [
                    {
                        "type": int(projectile.type.value),
                        "subtype": int(projectile.subtype),
                        "x": float(projectile.position.x),
                        "y": float(projectile.position.y),
                        "velocity_x": float(projectile.speed.x),
                        "velocity_y": float(projectile.speed.y),
                        "owner": int(projectile.owner),
                        "frame": int(projectile.frame),
                    }
                    for projectile in gamestate.projectiles
                ],
            }
            if memory_engine is not None:
                if args.memory_probe_shield:
                    row["shield_memory"] = read_shield_memory_probe(memory_engine)
                if args.memory_probe_damage:
                    row["damage_memory"] = read_damage_memory_probe(memory_engine)
                if args.memory_probe_hitbox:
                    row["hitbox_memory"] = read_hitbox_memory_probe(memory_engine)
            rows.append(row)

        item_rules = (
            read_native_item_rules(memory_engine)
            if args.enable_items and memory_engine is not None
            else None
        )
        return {
            "schema": (
                9
                if args.memory_probe_hitbox
                else 8 if args.memory_probe_shield or args.memory_probe_damage else 7
            ),
            "oracle": "SSBM GALE01 NTSC-U revision 2 via Dolphin/Slippi",
            "dolphin_version": console.version,
            "libmelee_version": importlib.metadata.version("melee"),
            "disc": {
                "game_id": "GALE01",
                "revision": 2,
                "sha256": sha256(iso),
            },
            "fighter": "CPTFALCON",
            "opponent": (
                "CPTFALCON"
                if (
                    args.push_only
                    or args.shield_hit_only
                    or args.damage_hit_only
                    or args.hitbox_geometry_only
                    or args.throw_geometry_only
                    or args.special_geometry_only
                )
                else "FOX"
            ),
            "stage": (
                "HYRULE_TEMPLE"
                if wall_geometry_route
                else "BATTLEFIELD" if args.platform_only else "FINAL_DESTINATION"
            ),
            "shield_hit_requested_pressure": (
                args.shield_hit_pressure if args.shield_hit_only else None
            ),
            "damage_hit_route": bool(args.damage_hit_only),
            "controller_postframe_pipeline_delay": pipeline_delay,
            "shield_memory_probe": (
                {
                    "engine_version": importlib.metadata.version(
                        "dolphin-memory-engine"
                    ),
                    "player_slot_address": "0x80453080",
                    "guard_state_pipeline_delay_frames": 1,
                    "fields": {
                        "guard_magnitude": "fighter+0x2344",
                        "guard_angle_degrees": "fighter+0x2348",
                        "shield_health": "fighter+0x1998",
                        "lightshield_amount": "fighter+0x199c",
                        "shield_joint": "fighter+0x19c0",
                    },
                }
                if args.memory_probe_shield
                else None
            ),
            "damage_memory_probe": (
                {
                    "engine_version": importlib.metadata.version(
                        "dolphin-memory-engine"
                    ),
                    "player_slot_address": "0x80453080",
                    "common_data_pointer_address": "0x804d6554",
                    "decomp_revision": ("9509dc04406fb2028bfab01243841ba4787c0fb7"),
                }
                if args.memory_probe_damage
                else None
            ),
            "item_rules": item_rules,
            "hitbox_memory_probe": (
                {
                    "engine_version": importlib.metadata.version(
                        "dolphin-memory-engine"
                    ),
                    "player_slot_address": "0x80453080",
                    "fighter_hitbox_array": "fighter+0x914",
                    "hitbox_stride": "0x138",
                    "position": "hitbox+0x4c",
                    "previous_position": "hitbox+0x58",
                    "static_player_stride": "0xe90",
                    "fighter_hurtbox_count": "fighter+0x119e",
                    "fighter_hurtbox_array": "fighter+0x11a0",
                    "hurtbox_stride": "0x4c",
                    "hurtbox_position_a": "hurtbox+0x28",
                    "hurtbox_position_b": "hurtbox+0x34",
                    "fighter_ecb": "fighter+0x794",
                    "ecb_layout": "top,bottom,right,left Vec2",
                    "fighter_ledge_snap": "fighter+0x744,+0x748,+0x74c",
                    "fighter_collision_contact": "fighter+0x830 Vec3",
                    "fighter_collision_positions": ("fighter+0x6f4,+0x700,+0x70c Vec3"),
                    "fighter_ledge_ids": "fighter+0x730,+0x734",
                    "decomp_revision": ("9509dc04406fb2028bfab01243841ba4787c0fb7"),
                }
                if args.memory_probe_hitbox
                else None
            ),
            "rows": rows,
        }
    finally:
        if memory_engine is not None:
            memory_engine.un_hook()
        player_one.disconnect()
        player_two.disconnect()
        console.stop()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dolphin", required=True)
    parser.add_argument("--iso", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--menu-timeout", type=float, default=120.0)
    parser.add_argument("--start-frame", type=int, default=120)
    parser.add_argument("--batch", action="store_true")
    parser.add_argument("--enable-items", action="store_true")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--platform-only", action="store_true")
    mode.add_argument("--push-only", action="store_true")
    mode.add_argument("--shield-only", action="store_true")
    mode.add_argument("--shield-geometry-only", action="store_true")
    mode.add_argument("--shield-geometry-sweep-only", action="store_true")
    mode.add_argument("--shield-hit-only", action="store_true")
    mode.add_argument("--damage-hit-only", action="store_true")
    mode.add_argument("--attack-iasa-only", action="store_true")
    mode.add_argument("--ground-attack-iasa-only", action="store_true")
    mode.add_argument("--hitbox-geometry-only", action="store_true")
    mode.add_argument("--throw-geometry-only", action="store_true")
    mode.add_argument("--special-geometry-only", action="store_true")
    parser.add_argument(
        "--ground-attack-move",
        action="append",
        choices=(
            "dashattack",
            "ftilt_m",
            "utilt",
            "dtilt",
            "fsmash_m",
            "usmash",
            "dsmash",
        ),
    )
    parser.add_argument(
        "--special-geometry-move",
        action="append",
        choices=tuple(SPECIAL_GEOMETRY_SOURCE_KEYS),
    )
    parser.add_argument("--falcon-frame-data", type=Path)
    parser.add_argument("--memory-probe-shield", action="store_true")
    parser.add_argument("--memory-probe-damage", action="store_true")
    parser.add_argument("--memory-probe-hitbox", action="store_true")
    parser.add_argument("--shield-hit-pressure", type=float, default=0.35)
    args = parser.parse_args()
    if not 0.30 <= args.shield_hit_pressure <= 1.0:
        parser.error("--shield-hit-pressure must be in [0.30, 1.0]")
    if (
        args.special_geometry_move
        and "side_ground_item_hit" in args.special_geometry_move
        and not args.enable_items
    ):
        parser.error("side_ground_item_hit requires --enable-items")
    if args.memory_probe_damage and not args.damage_hit_only:
        parser.error("--memory-probe-damage requires --damage-hit-only")
    if args.memory_probe_hitbox and not (
        args.hitbox_geometry_only
        or args.throw_geometry_only
        or args.special_geometry_only
    ):
        parser.error("--memory-probe-hitbox requires a geometry-only mode")
    if (
        sum(
            (
                args.memory_probe_shield,
                args.memory_probe_damage,
                args.memory_probe_hitbox,
            )
        )
        > 1
    ):
        parser.error("select only one memory probe")
    return args


def main() -> int:
    args = parse_args()
    result = capture(args)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("ssbm-movement-capture=pass " f"frames={len(result['rows'])} output={output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
