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
from pathlib import Path
import socket
import sys
import time

import melee


def input_trace() -> list[dict[str, object]]:
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
        }

    def repeat(label: str, count: int, **inputs: object) -> None:
        trace.extend(command(label, **inputs) for _ in range(count))

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
    return trace


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
) -> None:
    if gamestate.menu_state in (
        melee.Menu.CHARACTER_SELECT,
        melee.Menu.SLIPPI_ONLINE_CSS,
    ):
        melee.MenuHelper.choose_character(
            melee.Character.FOX,
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
        melee.MenuHelper.choose_stage(
            melee.Stage.FINAL_DESTINATION,
            gamestate,
            player_one,
        )
    elif gamestate.menu_state in (melee.Menu.PRESS_START, melee.Menu.MAIN_MENU):
        player_two.release_all()
        melee.MenuHelper.choose_versus_mode(gamestate, player_one)
    else:
        player_one.release_all()
        player_two.release_all()


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
    player_one = melee.Controller(console, 1, melee.ControllerType.STANDARD)
    player_two = melee.Controller(console, 2, melee.ControllerType.STANDARD)
    started_at = time.monotonic()

    try:
        environment = None
        if dolphin.is_file() and dolphin.name.lower().endswith(".appimage"):
            # WSL commonly lacks FUSE. AppImage's supported extraction fallback
            # keeps the oracle runnable without installing a kernel component.
            environment = {"APPIMAGE_EXTRACT_AND_RUN": "1"}
        console.run(iso_path=str(iso), environment_vars=environment)
        wait_for_udp_listener(console.slippi_port, 30.0)
        if not console.connect():
            raise RuntimeError("Dolphin Slippi stream did not connect")
        if not player_one.connect() or not player_two.connect():
            raise RuntimeError("Dolphin controller pipes did not connect")

        gamestate = None
        while time.monotonic() - started_at < args.menu_timeout:
            gamestate = console.step()
            if gamestate is None:
                continue
            if (
                gamestate.menu_state in (melee.Menu.IN_GAME, melee.Menu.SUDDEN_DEATH)
                and gamestate.frame >= args.start_frame
                and 1 in gamestate.players
                and gamestate.players[1].character == melee.Character.CPTFALCON
            ):
                break
            choose_match(gamestate, player_one, player_two)
        else:
            state = None if gamestate is None else str(gamestate.menu_state)
            raise TimeoutError(f"Dolphin match setup timed out in {state}")

        player_one.release_all()
        player_two.release_all()
        rows: list[dict[str, object]] = []
        origin_x: float | None = None
        trace = input_trace()
        pipeline_delay = 2
        commands = trace + [
            {
                "label": "pipeline_drain",
                "main_x": 0.5,
                "main_y": 0.5,
                "c_x": 0.5,
                "c_y": 0.5,
                "left_shoulder": 0.0,
                "right_shoulder": 0.0,
                "digital_left": False,
                "digital_right": False,
                "jump": False,
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
            gamestate = console.step()
            if gamestate is None or 1 not in gamestate.players:
                raise RuntimeError(
                    f"missing player state at command frame {command_index}"
                )
            if command_index < pipeline_delay:
                continue
            index = command_index - pipeline_delay
            scheduled = trace[index]
            player = gamestate.players[1]
            observed_x = float(player.controller_state.main_stick[0])
            observed_y = float(player.controller_state.main_stick[1])
            observed_c_x = float(player.controller_state.c_stick[0])
            observed_c_y = float(player.controller_state.c_stick[1])
            observed_left_shoulder = float(
                player.controller_state.l_shoulder
            )
            observed_right_shoulder = float(
                player.controller_state.r_shoulder
            )
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
            requested_x = float(scheduled["main_x"])
            requested_y = float(scheduled["main_y"])
            requested_c_x = float(scheduled["c_x"])
            requested_c_y = float(scheduled["c_y"])
            axis_aligned = (
                (requested_x == 0.5 and abs(observed_x - 0.5) <= 0.02)
                or (requested_x < 0.5 and observed_x < 0.5)
                or (requested_x > 0.5 and observed_x > 0.5)
            ) and (
                (requested_y == 0.5 and abs(observed_y - 0.5) <= 0.02)
                or (requested_y < 0.5 and observed_y < 0.5)
                or (requested_y > 0.5 and observed_y > 0.5)
            )
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
                0.0
                if requested_analog_shoulder <= 0.30
                else requested_analog_shoulder
            )
            shoulder_aligned = (
                abs(
                    max(observed_left_shoulder, observed_right_shoulder)
                    - expected_observed_shoulder
                )
                <= 0.10
                and observed_digital_left
                == bool(scheduled["digital_left"])
                and observed_digital_right
                == bool(scheduled["digital_right"])
                and observed_jump == bool(scheduled["jump"])
            )
            aligned = axis_aligned and c_axis_aligned and shoulder_aligned
            if not aligned:
                raise RuntimeError(
                    "controller/post-frame alignment failed at trace frame "
                    f"{index}: requested={scheduled} "
                    "observed="
                    f"x={observed_x} y={observed_y} "
                    f"cx={observed_c_x} cy={observed_c_y} "
                    f"l={observed_left_shoulder}/{observed_digital_left} "
                    f"r={observed_right_shoulder}/{observed_digital_right} "
                    f"jump={observed_jump}"
                )
            if origin_x is None:
                origin_x = player.position.x
            rows.append(
                {
                    "trace_frame": index,
                    "game_frame": int(gamestate.frame),
                    "label": scheduled["label"],
                    "requested_main_x": requested_x,
                    "requested_main_y": requested_y,
                    "requested_c_x": requested_c_x,
                    "requested_c_y": requested_c_y,
                    "requested_left_shoulder": float(
                        scheduled["left_shoulder"]
                    ),
                    "requested_right_shoulder": float(
                        scheduled["right_shoulder"]
                    ),
                    "requested_digital_left": bool(
                        scheduled["digital_left"]
                    ),
                    "requested_digital_right": bool(
                        scheduled["digital_right"]
                    ),
                    "requested_jump": bool(scheduled["jump"]),
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
                    "shield_health": float(player.shield_strength),
                }
            )

        return {
            "schema": 2,
            "oracle": "SSBM GALE01 NTSC-U revision 2 via Dolphin/Slippi",
            "dolphin_version": console.version,
            "libmelee_version": importlib.metadata.version("melee"),
            "disc": {
                "game_id": "GALE01",
                "revision": 2,
                "sha256": sha256(iso),
            },
            "fighter": "CPTFALCON",
            "stage": "FINAL_DESTINATION",
            "controller_postframe_pipeline_delay": pipeline_delay,
            "rows": rows,
        }
    finally:
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
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    result = capture(args)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        "ssbm-movement-capture=pass "
        f"frames={len(result['rows'])} output={output}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
