"""Unit tests for the complete-match Slippi differential lane."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from ssbm_full_match_differential import (  # noqa: E402
    expected_target_position_f32,
    native_player_fields,
    validate_complete_match,
)
from ssbm_collision import binary32  # noqa: E402


def pre_frame(*, raw_c: bool = True) -> dict:
    return {
        "joystickX": 0.5,
        "joystickY": -0.25,
        "cStickX": -1.0,
        "cStickY": 1.0,
        "physicalButtons": 0,
        "physicalLTrigger": 0.25,
        "physicalRTrigger": 0.5,
        "rawJoystickX": 40,
        "rawJoystickY": -20,
        "rawCStickX": -80 if raw_c else None,
        "rawCStickY": 80 if raw_c else None,
    }


def complete_replay(*, raw_c: bool = True) -> dict:
    player_settings = [
        {
            "playerIndex": index,
            "characterId": 0,
            "type": 0,
            "startStocks": 4,
            "controllerFix": "UCF",
        }
        for index in (0, 1)
    ]
    sample = {
        "pre": pre_frame(raw_c=raw_c),
        "post": {
            "actionStateId": 322,
            "facingDirection": 1,
            "isAirborne": True,
            "stocksRemaining": 4,
            "percent": 0,
        },
    }
    return {
        "settings": {
            "players": player_settings,
            "isPAL": False,
            "isTeams": False,
            "timerType": 2,
            "startingTimerSeconds": 480,
            "itemSpawnBehavior": 255,
            "stageId": 31,
            "isFrozenPS": False,
        },
        "gameEnd": {"gameEndMethod": 2},
        "inputProvenance": {
            "framing": "slp-message-sizes-v1",
            "exactRawMainX": True,
            "exactRawMainY": True,
            "exactRawCX": raw_c,
            "exactRawCY": raw_c,
        },
        "frames": [
            {
                "frame": -123,
                "players": [sample, sample],
            }
        ],
    }


class CompleteMatchValidationTests(unittest.TestCase):
    def test_position_projection_uses_direct_float32_units(self) -> None:
        profile = {
            "source_to_target_x_scale": {
                "numerator": 12,
                "denominator": 115,
            },
            "source_to_target_y_scale": {
                "numerator": 11,
                "denominator": 62,
                "invert": True,
                "origin_f32": 20.0,
                "fighter_root_to_body_center_f32": 0.79998779296875,
            },
        }
        x, y = expected_target_position_f32(
            profile, {"positionX": 9.0, "positionY": 3.0}
        )
        self.assertAlmostEqual(x, 9.0 * 12.0 / 115.0, places=7)
        self.assertEqual(
            y,
            binary32(
                20.0
                + binary32(-3.0 * 11.0 / 62.0)
                - 0.79998779296875
            ),
        )
        self.assertLess(abs(x), 10.0)

    def test_complete_exact_match_is_accepted(self) -> None:
        self.assertEqual(
            validate_complete_match(
                complete_replay(), allow_missing_raw_c=False
            ),
            [],
        )

    def test_missing_raw_c_is_fail_closed_for_qualification(self) -> None:
        self.assertIn(
            "exact-raw-c-unavailable",
            validate_complete_match(
                complete_replay(raw_c=False), allow_missing_raw_c=False
            ),
        )
        self.assertNotIn(
            "exact-raw-c-unavailable",
            validate_complete_match(
                complete_replay(raw_c=False), allow_missing_raw_c=True
            ),
        )

    def test_stadium_must_be_frozen(self) -> None:
        replay = complete_replay()
        replay["settings"]["stageId"] = 28
        self.assertIn(
            "pokemon-stadium-not-frozen",
            validate_complete_match(replay, allow_missing_raw_c=False),
        )

    def test_unsnapped_ucf084_cardinal_rejects_whole_match(self) -> None:
        replay = complete_replay()
        pre = replay["frames"][0]["players"][0]["pre"]
        pre.update(
            {
                "joystickX": -0.9875,
                "joystickY": 0.0,
                "rawJoystickX": -96,
                "rawJoystickY": 1,
            }
        )
        self.assertIn(
            "ucf084-cardinal-signature-mismatch:frame=-123:player=0",
            validate_complete_match(replay, allow_missing_raw_c=False),
        )

    def test_snapped_ucf084_cardinal_accepts_whole_match(self) -> None:
        replay = complete_replay()
        pre = replay["frames"][0]["players"][0]["pre"]
        pre.update(
            {
                "joystickX": -1.0,
                "joystickY": 0.0,
                "rawJoystickX": -96,
                "rawJoystickY": 1,
            }
        )
        self.assertNotIn(
            "ucf084-cardinal-signature-mismatch:frame=-123:player=0",
            validate_complete_match(replay, allow_missing_raw_c=False),
        )

    def test_player_fields_preserve_full_raw_pad(self) -> None:
        fields = native_player_fields(
            pre_frame(raw_c=True), allow_missing_raw_c=False
        )
        self.assertEqual(len(fields), 12)
        self.assertEqual(fields[-5:], [40, -20, -80, 80, 15])

    def test_diagnostic_missing_raw_c_uses_main_only_mask(self) -> None:
        fields = native_player_fields(
            pre_frame(raw_c=False), allow_missing_raw_c=True
        )
        self.assertEqual(fields[-5:], [40, -20, 0, 0, 3])


if __name__ == "__main__":
    unittest.main()
