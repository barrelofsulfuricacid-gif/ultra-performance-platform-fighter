#!/usr/bin/env python3
"""Capture deterministic SSBM movement from Dolphin with scripted inputs.

This developer tool intentionally depends on a separately installed libmelee,
Dolphin/Slippi build, and an owner-supplied GALE01 revision 2 image. None of
those external assets are copied into the repository.
"""

from __future__ import annotations

import argparse
import hashlib
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
            trace.append({"label": label, "main_x": x, "main_y": 0.5})

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
            {"label": "pipeline_drain", "main_x": 0.5, "main_y": 0.5}
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
            requested_x = float(scheduled["main_x"])
            aligned = (
                (requested_x == 0.5 and abs(observed_x - 0.5) <= 0.02)
                or (requested_x < 0.5 and observed_x < 0.5)
                or (requested_x > 0.5 and observed_x > 0.5)
            )
            if not aligned:
                raise RuntimeError(
                    "controller/post-frame alignment failed at trace frame "
                    f"{index}: requested={requested_x} observed={observed_x}"
                )
            if origin_x is None:
                origin_x = player.position.x
            rows.append(
                {
                    "trace_frame": index,
                    "game_frame": int(gamestate.frame),
                    "label": scheduled["label"],
                    "requested_main_x": requested_x,
                    "observed_main_x": observed_x,
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
                }
            )

        return {
            "schema": 1,
            "oracle": "SSBM GALE01 NTSC-U revision 2 via Dolphin/Slippi",
            "dolphin_version": console.version,
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
