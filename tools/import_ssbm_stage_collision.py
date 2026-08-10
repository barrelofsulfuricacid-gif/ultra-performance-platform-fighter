#!/usr/bin/env python3
"""Import a compact, deterministic SSBM stage collision catalog.

The live capture owns the source MapCollData topology and runtime world-space
vertex reads.  This importer strips runtime addresses, converts source-up
coordinates to the simulation's down-positive Q16.16 coordinates, translates
the complete catalog into the simulation arena, and emits the immutable C
table used by every environment.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any


SCHEMA = 4
LEGACY_SCHEMAS = {3}
MELEE_X_TO_SIM = 12.0 / 115.0
MELEE_Y_TO_SIM = 11.0 / 62.0
SIM_Y_ORIGIN = 20.0
KIND_VALUES = {
    "unclassified": 0,
    "floor": 1,
    "ceiling": 2,
    "right_wall": 3,
    "left_wall": 4,
    "dynamic": 5,
}


def q16(value: float) -> int:
    result = round(value * 65536.0)
    if not -(1 << 31) <= result < (1 << 31):
        raise ValueError(f"Q16.16 coordinate is out of range: {value}")
    return result


def canonical_bytes(value: object) -> bytes:
    return (
        json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
    ).encode("ascii")


def compact_capture(
    capture_path: Path,
    stage_name: str,
    oracle: str,
) -> dict[str, object]:
    capture = json.loads(capture_path.read_text(encoding="utf-8"))
    observed_stage = capture.get("stage")
    if observed_stage != stage_name:
        raise ValueError(
            f"capture stage mismatch: expected {stage_name!r}, got {observed_stage!r}"
        )
    memory = capture.get("stage_collision_memory")
    if not isinstance(memory, dict):
        raise ValueError("capture does not contain stage_collision_memory")
    lines = memory.get("lines")
    ranges = memory.get("ranges")
    if not isinstance(lines, list) or not isinstance(ranges, dict):
        raise ValueError("capture has an invalid stage collision catalog")
    if not 0 < len(lines) <= 254:
        raise ValueError("stage line count does not fit the runtime support byte")

    compact_lines: list[dict[str, object]] = []
    for expected_index, raw in enumerate(lines):
        if not isinstance(raw, dict) or raw.get("index") != expected_index:
            raise ValueError("stage collision lines are not dense and ordered")
        kind = raw.get("kind")
        start = raw.get("world_start")
        end = raw.get("world_end")
        neighbors = raw.get("neighbors")
        if (
            kind not in KIND_VALUES
            or not isinstance(start, list)
            or len(start) != 2
            or not isinstance(end, list)
            or len(end) != 2
            or not isinstance(neighbors, list)
            or len(neighbors) != 4
        ):
            raise ValueError(f"invalid stage collision line {expected_index}")
        compact_lines.append(
            {
                "index": expected_index,
                "kind": kind,
                "start": [float(start[0]), float(start[1])],
                "end": [float(end[0]), float(end[1])],
                "neighbors": [int(value) for value in neighbors],
                "hi_flags": int(raw["hi_flags"]),
                "lo_flags": int(raw["lo_flags"]),
                "runtime_flags": int(raw["runtime_flags"]),
            }
        )

    compact_ranges: dict[str, dict[str, int]] = {}
    for kind in ("floor", "ceiling", "right_wall", "left_wall", "dynamic"):
        bounds = ranges.get(kind)
        if not isinstance(bounds, dict):
            raise ValueError(f"stage collision catalog is missing {kind} range")
        compact_ranges[kind] = {
            "start": int(bounds["start"]),
            "count": int(bounds["count"]),
        }

    camera = memory.get("camera")
    blast_zone = memory.get("blast_zone")
    spawn_points = memory.get("player_spawn_points")
    grkind = int(memory.get("grkind", -1))
    if (
        not isinstance(camera, dict)
        or not isinstance(camera.get("effective_bounds"), dict)
        or not isinstance(blast_zone, dict)
        or not isinstance(blast_zone.get("effective"), dict)
        or not isinstance(spawn_points, list)
        or len(spawn_points) != 4
        or not 0 <= grkind <= 0xFFFF
    ):
        raise ValueError("capture has invalid stage environment metadata")

    def compact_bounds(raw: object, label: str) -> dict[str, float]:
        if not isinstance(raw, dict):
            raise ValueError(f"capture has invalid {label}")
        result = {
            side: float(raw[side])
            for side in ("left", "right", "top", "bottom")
        }
        if not all(math.isfinite(value) for value in result.values()):
            raise ValueError(f"capture has non-finite {label}")
        if result["left"] >= result["right"] or result["bottom"] >= result["top"]:
            raise ValueError(f"capture has inverted {label}")
        return result

    compact_camera_bounds = compact_bounds(
        camera["effective_bounds"], "camera bounds"
    )
    compact_blast_zone = compact_bounds(
        blast_zone["effective"], "blast zone"
    )

    floor_start = compact_ranges["floor"]["start"]
    floor_end = floor_start + compact_ranges["floor"]["count"]

    def floor_height_at(line: dict[str, object], x: float) -> float | None:
        start = line["start"]
        end = line["end"]
        assert isinstance(start, list) and isinstance(end, list)
        x0, y0 = float(start[0]), float(start[1])
        x1, y1 = float(end[0]), float(end[1])
        left, right = min(x0, x1), max(x0, x1)
        if x < left - 1.0e-5 or x > right + 1.0e-5:
            return None
        if x0 == x1:
            return max(y0, y1)
        return y0 + (x - x0) * (y1 - y0) / (x1 - x0)

    compact_spawn_points: list[dict[str, object]] = []
    for expected_index, raw in enumerate(spawn_points):
        if not isinstance(raw, dict) or raw.get("index") != expected_index:
            raise ValueError("stage spawn points are not dense and ordered")
        position = raw.get("position")
        if not isinstance(position, list) or len(position) != 3:
            raise ValueError(f"invalid stage spawn point {expected_index}")
        x, y, z = (float(value) for value in position)
        if not all(math.isfinite(value) for value in (x, y, z)):
            raise ValueError(f"non-finite stage spawn point {expected_index}")
        support_candidates: list[tuple[float, int]] = []
        for line_index in range(floor_start, floor_end):
            height = floor_height_at(compact_lines[line_index], x)
            if height is not None and height <= y + 1.0e-5:
                support_candidates.append((height, line_index))
        if not support_candidates:
            raise ValueError(
                f"stage spawn point {expected_index} has no floor below it"
            )
        _, support_line = max(support_candidates)
        compact_spawn_points.append(
            {
                "index": expected_index,
                "position": [x, y, z],
                "support_line": support_line,
            }
        )

    source_semantics = {
        "stage": stage_name,
        "grkind": grkind,
        "camera_bounds": compact_camera_bounds,
        "blast_zone": compact_blast_zone,
        "player_spawn_points": compact_spawn_points,
        "ranges": compact_ranges,
        "lines": compact_lines,
    }
    return {
        "schema": SCHEMA,
        "oracle": oracle,
        "stage": stage_name,
        "coordinate_space": "runtime-world-space",
        "simulation_transform": {
            "x_scale": "12/115",
            "y_scale": "-11/62",
            "y_origin": 20,
        },
        # Stable across fresh boots: process addresses, frame counters, and
        # unrelated fighter rows are deliberately outside stage provenance.
        "source_stage_collision_sha256": hashlib.sha256(
            canonical_bytes(source_semantics)
        ).hexdigest(),
        "grkind": grkind,
        "camera_bounds": compact_camera_bounds,
        "blast_zone": compact_blast_zone,
        "player_spawn_points": compact_spawn_points,
        "ranges": compact_ranges,
        "lines": compact_lines,
    }


def c_i32(value: int) -> str:
    if value == -(1 << 31):
        return "INT32_MIN"
    sign = "-" if value < 0 else ""
    return f"{sign}INT32_C({abs(value)})"


def c_i16(value: int) -> str:
    if not -(1 << 15) <= value < (1 << 15):
        raise ValueError(f"i16 value is out of range: {value}")
    return f"INT16_C({value})"


def render_inc(source: dict[str, Any], symbol: str) -> str:
    schema = source.get("schema")
    if schema != SCHEMA and schema not in LEGACY_SCHEMAS:
        raise ValueError("unsupported compact stage collision schema")
    lines = source.get("lines")
    ranges = source.get("ranges")
    if not isinstance(lines, list) or not isinstance(ranges, dict):
        raise ValueError("invalid compact stage collision source")
    digest = hashlib.sha256(canonical_bytes(source)).digest()
    rendered = [
        "/* Generated by tools/import_ssbm_stage_collision.py. */",
        f"static const uint8_t {symbol}_source_sha256[32] = {{",
        "    " + ", ".join(f"UINT8_C(0x{byte:02x})" for byte in digest),
        "};",
        "",
        f"static const pf_m4_ssbm_stage_collision_line {symbol}_lines[] = {{",
    ]
    for expected_index, line in enumerate(lines):
        if line.get("index") != expected_index:
            raise ValueError("compact stage collision lines are not dense and ordered")
        kind = str(line["kind"])
        source_start_x = float(line["start"][0])
        source_start_y = float(line["start"][1])
        source_end_x = float(line["end"][0])
        source_end_y = float(line["end"][1])
        start_x = source_start_x * MELEE_X_TO_SIM
        start_y = SIM_Y_ORIGIN - source_start_y * MELEE_Y_TO_SIM
        end_x = source_end_x * MELEE_X_TO_SIM
        end_y = SIM_Y_ORIGIN - source_end_y * MELEE_Y_TO_SIM
        source_dx = source_end_x - source_start_x
        source_dy = source_end_y - source_start_y
        source_length = math.hypot(source_dx, source_dy)
        # Ground/self velocity scalars are stored in horizontally scaled Melee
        # units.  Project with the source-space unit tangent, then compensate
        # Y for the simulation's intentionally anisotropic world scale.
        projection_x = (
            q16(source_dx / source_length) if source_length != 0.0 else 0
        )
        projection_y = (
            q16(
                -source_dy
                / source_length
                * (MELEE_Y_TO_SIM / MELEE_X_TO_SIM)
            )
            if source_length != 0.0
            else 0
        )
        # mpLineGetNormal at decomp revision 9509dc0 computes the source-space
        # unit basis (-dy, dx).  Keep it independent of the anisotropic render
        # transform so collision response can convert velocities back to Melee
        # units once, mirror there, and convert the result back once.
        source_normal_x = (
            q16(-source_dy / source_length) if source_length != 0.0 else 0
        )
        source_normal_y = (
            q16(source_dx / source_length) if source_length != 0.0 else 0
        )
        neighbors = [int(value) for value in line["neighbors"]]
        rendered.extend(
            [
                "    {",
                f"        {c_i32(q16(start_x))}, {c_i32(q16(start_y))},",
                f"        {c_i32(q16(end_x))}, {c_i32(q16(end_y))},",
                f"        {c_i32(projection_x)}, {c_i32(projection_y)},",
                f"        {c_i32(source_normal_x)}, {c_i32(source_normal_y)},",
                "        " + ", ".join(c_i16(value) for value in neighbors) + ",",
                f"        UINT32_C({int(line['runtime_flags'])}),",
                f"        UINT16_C({int(line['hi_flags'])}), UINT16_C({int(line['lo_flags'])}),",
                f"        UINT8_C({KIND_VALUES[kind]}), UINT8_C({expected_index})",
                "    },",
            ]
        )
    rendered.extend(["};", ""])
    spawn_points = source.get("player_spawn_points", [])
    if not isinstance(spawn_points, list):
        raise ValueError("invalid compact stage spawn points")
    if spawn_points:
        rendered.append(
            f"static const pf_m4_ssbm_stage_spawn_point {symbol}_spawn_points[] = {{"
        )
        for expected_index, point in enumerate(spawn_points):
            if not isinstance(point, dict) or point.get("index") != expected_index:
                raise ValueError("compact stage spawn points are not dense and ordered")
            position = point.get("position")
            support_line = int(point.get("support_line", -1))
            if (
                not isinstance(position, list)
                or len(position) != 3
                or not 0 <= support_line < len(lines)
            ):
                raise ValueError(f"invalid compact stage spawn point {expected_index}")
            rendered.extend(
                [
                    "    {",
                    f"        {c_i32(q16(float(position[0]) * MELEE_X_TO_SIM))},",
                    f"        {c_i32(q16(SIM_Y_ORIGIN - float(position[1]) * MELEE_Y_TO_SIM))},",
                    f"        UINT8_C({support_line + 1}), UINT8_C({expected_index})",
                    "    },",
                ]
            )
        rendered.extend(["};", ""])
    range_values: list[str] = []
    for kind in ("floor", "ceiling", "right_wall", "left_wall", "dynamic"):
        bounds = ranges[kind]
        range_values.extend(
            [f"UINT16_C({int(bounds['start'])})", f"UINT16_C({int(bounds['count'])})"]
        )

    def transformed_bounds(name: str) -> list[str]:
        bounds = source.get(name)
        if not isinstance(bounds, dict):
            return [c_i32(0)] * 4
        return [
            (
                c_i32(q16(float(bounds[side]) * MELEE_X_TO_SIM))
                if side in ("left", "right")
                else c_i32(
                    q16(SIM_Y_ORIGIN - float(bounds[side]) * MELEE_Y_TO_SIM)
                )
            )
            for side in ("left", "right", "top", "bottom")
        ]

    camera_bounds = transformed_bounds("camera_bounds")
    blast_zone = transformed_bounds("blast_zone")
    rendered.extend(
        [
            f"static const pf_m4_ssbm_stage_collision_profile {symbol}_profile = {{",
            f"    {symbol}_lines,",
            f"    UINT16_C({len(lines)}),",
            "    " + ", ".join(range_values[:4]) + ",",
            "    " + ", ".join(range_values[4:8]) + ",",
            "    " + ", ".join(range_values[8:]) + ",",
            f"    UINT16_C({int(source.get('grkind', 0))}),",
            (
                f"    {symbol}_spawn_points, UINT8_C({len(spawn_points)}),"
                if spawn_points
                else "    NULL, UINT8_C(0),"
            ),
            "    " + ", ".join(camera_bounds) + ",",
            "    " + ", ".join(blast_zone) + ",",
            f"    {symbol}_source_sha256",
            "};",
            "",
        ]
    )
    return "\n".join(rendered)


def write_or_check(path: Path, payload: bytes, check: bool) -> None:
    if check:
        if not path.is_file() or path.read_bytes() != payload:
            raise SystemExit(f"generated output is stale: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--source-json", type=Path, required=True)
    parser.add_argument("--output-inc", type=Path, required=True)
    parser.add_argument("--stage", required=True)
    parser.add_argument("--oracle", default="SSBM GALE01 NTSC-U revision 2")
    parser.add_argument("--symbol", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    if args.capture is not None:
        source = compact_capture(args.capture, args.stage, args.oracle)
        write_or_check(args.source_json, canonical_bytes(source), args.check)
    else:
        source = json.loads(args.source_json.read_text(encoding="utf-8"))
    rendered = render_inc(source, args.symbol).encode("ascii")
    write_or_check(args.output_inc, rendered, args.check)


if __name__ == "__main__":
    main()
