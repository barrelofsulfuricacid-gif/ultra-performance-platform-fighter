#!/usr/bin/env python3
"""Compact an owned Dolphin Falcon gait-clock capture into a stored oracle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from ssbm_live_trace import MELEE_X_TO_SIM_Q16, canonical_sha256


SOURCE_CAPTURE_SHA256 = (
    "0651712d514246b4cd8c01c2606adca1e561d31cf076f60610f7a8c2d2d5680b"
)
DISC_SHA256 = (
    "0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464"
)
EXIAI_SHA256 = (
    "87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd"
)
CASE_DECLARATIONS = (
    ("walk_slow", "WALK_SLOW", 1, 7, 9830, 55),
    ("walk_middle", "WALK_MIDDLE", 1, 8, 16384, 35),
    ("run", "RUNNING", 3, 13, 32767, 21),
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def generate(capture: dict[str, Any], source_digest: str) -> str:
    disc = capture.get("disc")
    execution = capture.get("oracle_execution")
    require(capture.get("schema") == 9, "unexpected source capture schema")
    require(capture.get("fighter") == "CPTFALCON", "capture is not Falcon")
    require(capture.get("stage") == "FINAL_DESTINATION", "unexpected stage")
    require(
        isinstance(disc, dict)
        and disc.get("game_id") == "GALE01"
        and disc.get("revision") == 2
        and disc.get("sha256") == DISC_SHA256,
        "unexpected disc identity",
    )
    require(
        isinstance(execution, dict)
        and execution.get("release_artifact_sha256") == EXIAI_SHA256,
        "unexpected Dolphin oracle artifact",
    )
    rows = capture.get("rows")
    require(isinstance(rows, list), "capture rows are missing")

    cases: list[dict[str, Any]] = []
    for case_id, action, action_state, submotion, input_x, expected_count in (
        CASE_DECLARATIONS
    ):
        selected = [row for row in rows if row.get("action") == action]
        require(
            len(selected) == expected_count,
            f"{case_id}: expected {expected_count} rows, got {len(selected)}",
        )
        samples: list[dict[str, int]] = []
        for sample_index, row in enumerate(selected):
            memory = row.get("hitbox_memory")
            require(
                isinstance(memory, dict),
                f"{case_id}[{sample_index}]: missing fighter memory",
            )
            frame = memory.get("fighter_animation_frame")
            rate = memory.get("fighter_animation_rate")
            velocity = row.get("ground_velocity_x")
            require(
                all(isinstance(value, (int, float)) for value in (frame, rate, velocity)),
                f"{case_id}[{sample_index}]: invalid numeric sample",
            )
            samples.append(
                {
                    "frame_f32": round(float(frame) * 65536.0),
                    "rate_f32": round(float(rate) * 65536.0),
                    "velocity_x_f32": round(
                        float(velocity) * MELEE_X_TO_SIM_Q16
                    ),
                }
            )
        cases.append(
            {
                "id": case_id,
                "source_action": action,
                "action_state": action_state,
                "source_submotion": submotion,
                "input_x": input_x,
                "samples": samples,
            }
        )

    semantic_payload = {
        "domain": "falcon-ground-animation-clock",
        "cases": cases,
    }
    output = {
        "schema": 1,
        "domain": semantic_payload["domain"],
        "fighter": "CPTFALCON",
        "disc_sha256": DISC_SHA256,
        "dolphin_release_artifact_sha256": EXIAI_SHA256,
        "source_capture_sha256": source_digest,
        "semantic_sha256": canonical_sha256(semantic_payload),
        "q16_tolerances": {
            "frame": 1200,
            "rate": 128,
            "velocity_x": 4,
        },
        "cases": cases,
    }
    return json.dumps(output, indent=2, sort_keys=True) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    raw = args.capture.read_bytes()
    source_digest = hashlib.sha256(raw).hexdigest()
    if source_digest != SOURCE_CAPTURE_SHA256:
        raise SystemExit(f"unexpected source capture SHA-256: {source_digest}")
    generated = generate(json.loads(raw), source_digest)
    if args.check:
        if (
            not args.output.is_file()
            or args.output.read_text(encoding="utf-8") != generated
        ):
            raise SystemExit(f"stale Falcon gait-clock oracle: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
    print(
        "ssbm-falcon-ground-animation-clock-import=pass "
        f"cases={len(CASE_DECLARATIONS)} "
        f"samples={sum(case[-1] for case in CASE_DECLARATIONS)} "
        f"source_sha256={source_digest} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
