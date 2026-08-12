#!/usr/bin/env python3
"""Fast contracts for the reusable UCF full-PAD trace boundary."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from compare_ssbm_movement import native_input_line, source_axis_q15  # noqa: E402
from generate_ssbm_stored_trace_oracle import native_csv_input_line  # noqa: E402
from ssbm_live_trace import source_axis_to_sim_q15  # noqa: E402


class FullPadNativeCsvTests(unittest.TestCase):
    def test_full_raw_pad_is_appended_without_reencoding_processed_axes(self) -> None:
        sample = {
            "lanes": [
                {
                    "main": [32767, 2458],
                    "c_stick": [-2867, -32767],
                    "raw_main": [80, 6],
                    "raw_c": [-7, -80],
                },
                {},
            ]
        }
        line = native_csv_input_line(
            sample,
            "full-pad",
            0,
            include_raw_main=True,
            include_raw_c=True,
        )
        self.assertEqual(
            line,
            "32767,-2458,-2867,32767,0,0,0,0,0,80,6,-7,-80,15",
        )

    def test_raw_c_without_raw_main_fails_closed(self) -> None:
        sample = {"lanes": [{"raw_c": [80, 0]}, {}]}
        with self.assertRaisesRegex(ValueError, "requires raw main"):
            native_csv_input_line(
                sample,
                "partial-pad",
                0,
                include_raw_c=True,
            )

    def test_live_line_uses_pre_ucf_pad_axes_and_full_raw_mask(self) -> None:
        row = {
            "observed_main_x": 1.0,
            "observed_main_y": 0.5,
            "observed_c_x": 0.5,
            "observed_c_y": 0.5,
            "observed_raw_main_x": 80,
            "observed_raw_main_y": 6,
            "observed_raw_c_x": -7,
            "observed_raw_c_y": -80,
            "input_memory": {
                "fighter_pre_ucf_main_x": 1.0,
                "fighter_pre_ucf_main_y": 0.075,
                "fighter_pre_ucf_c_x": -0.0875,
                "fighter_pre_ucf_c_y": -1.0,
            },
        }
        self.assertEqual(
            native_input_line(
                row,
                include_raw_main=True,
                include_raw_c=True,
            ),
            "32767,-2458,-2867,32767,0,0,0,0,0,80,6,-7,-80,15",
        )


class SourceAxisTests(unittest.TestCase):
    def test_pre_ucf_and_post_ucf_negative_endpoints_are_distinct(self) -> None:
        self.assertEqual(source_axis_q15(-1.0), -32767)
        self.assertEqual(source_axis_to_sim_q15(-1.0), -32768)
        self.assertEqual(source_axis_to_sim_q15(1.0, invert=True), -32768)
        self.assertEqual(source_axis_to_sim_q15(-1.0, invert=True), 32767)


if __name__ == "__main__":
    unittest.main()
