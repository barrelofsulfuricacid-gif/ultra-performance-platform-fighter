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


def input_trace(
    platform_only: bool = False,
    push_only: bool = False,
    shield_only: bool = False,
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
    trace.append(
        command("crouch_start_down_special", main_y=0.0, special=True)
    )
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
    trace.append(
        command("crouch_wait_down_special", main_y=0.0, special=True)
    )
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
    trace.append(
        command("crouch_end_down_special", main_y=0.0, special=True)
    )
    repeat("crouch_end_down_special_recovery", 100)
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
    stage: melee.Stage,
    opponent: melee.Character = melee.Character.FOX,
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
        melee.MenuHelper.choose_stage(
            stage,
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
            choose_match(
                gamestate,
                player_one,
                player_two,
                melee.Stage.BATTLEFIELD
                if args.platform_only
                else melee.Stage.FINAL_DESTINATION,
                melee.Character.CPTFALCON
                if args.push_only
                else melee.Character.FOX,
            )
        else:
            state = None if gamestate is None else str(gamestate.menu_state)
            raise TimeoutError(f"Dolphin match setup timed out in {state}")

        player_one.release_all()
        player_two.release_all()
        rows: list[dict[str, object]] = []
        origin_x: float | None = None
        origin_two_x: float | None = None
        trace = input_trace(
            platform_only=args.platform_only,
            push_only=args.push_only,
            shield_only=args.shield_only,
        )
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
                "attack": False,
                "special": False,
                "grab": False,
                "taunt": False,
                "opponent_main_x": 0.5,
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
            observed_attack = bool(
                player.controller_state.button[melee.Button.BUTTON_A]
            )
            observed_special = bool(
                player.controller_state.button[melee.Button.BUTTON_B]
            )
            observed_grab = bool(
                player.controller_state.button[melee.Button.BUTTON_Z]
            )
            observed_taunt = bool(
                player.controller_state.button[melee.Button.BUTTON_D_UP]
            )
            observed_opponent_x = float(
                player_two_state.controller_state.main_stick[0]
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
                0.35
                if bool(scheduled["grab"])
                else 0.0
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
                and observed_attack == bool(scheduled["attack"])
                and observed_special == bool(scheduled["special"])
                and observed_grab == bool(scheduled["grab"])
                and observed_taunt == bool(scheduled["taunt"])
            )
            requested_opponent_x = float(scheduled["opponent_main_x"])
            opponent_axis_aligned = (
                requested_opponent_x == 0.5
                and abs(observed_opponent_x - 0.5) <= 0.02
            ) or (
                requested_opponent_x < 0.5
                and observed_opponent_x < 0.5
            ) or (
                requested_opponent_x > 0.5
                and observed_opponent_x > 0.5
            )
            aligned = (
                axis_aligned
                and c_axis_aligned
                and shoulder_aligned
                and opponent_axis_aligned
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
                    f"opponent_x={observed_opponent_x}"
                )
            if origin_x is None:
                origin_x = player.position.x
            if origin_two_x is None:
                origin_two_x = player_two_state.position.x
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
                    "requested_attack": bool(scheduled["attack"]),
                    "requested_special": bool(scheduled["special"]),
                    "requested_grab": bool(scheduled["grab"]),
                    "requested_taunt": bool(scheduled["taunt"]),
                    "requested_opponent_main_x": requested_opponent_x,
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
                    "opponent_action": player_two_state.action.name,
                    "opponent_action_value": int(
                        player_two_state.action.value
                    ),
                    "opponent_action_frame": float(
                        player_two_state.action_frame
                    ),
                    "opponent_facing": (
                        1 if player_two_state.facing else -1
                    ),
                    "opponent_grounded": bool(player_two_state.on_ground),
                    "opponent_position_x": float(player_two_state.position.x),
                    "opponent_position_x_from_origin": float(
                        player_two_state.position.x - origin_two_x
                    ),
                    "opponent_position_y": float(player_two_state.position.y),
                    "opponent_ground_velocity_x": float(
                        player_two_state.speed_ground_x_self
                    ),
                    "opponent_air_velocity_x": float(
                        player_two_state.speed_air_x_self
                    ),
                    "opponent_velocity_y": float(
                        player_two_state.speed_y_self
                    ),
                }
            )

        return {
            "schema": 5,
            "oracle": "SSBM GALE01 NTSC-U revision 2 via Dolphin/Slippi",
            "dolphin_version": console.version,
            "libmelee_version": importlib.metadata.version("melee"),
            "disc": {
                "game_id": "GALE01",
                "revision": 2,
                "sha256": sha256(iso),
            },
            "fighter": "CPTFALCON",
            "opponent": "CPTFALCON" if args.push_only else "FOX",
            "stage": "BATTLEFIELD" if args.platform_only else "FINAL_DESTINATION",
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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--platform-only", action="store_true")
    mode.add_argument("--push-only", action="store_true")
    mode.add_argument("--shield-only", action="store_true")
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
