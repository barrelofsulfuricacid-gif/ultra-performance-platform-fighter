#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ground_capture=${1:-/tmp/falcon_special_geometry_up_ground_catch_ecb_v2.json}
air_capture=${2:-/tmp/falcon_special_geometry_up_air_catch_kb_v4.json}
build_dir=${3:-"$root/build/wsl-release"}
output_dir=${4:-/tmp/falcon_dive}
ground_miss_capture=${5:-/tmp/falcon_special_geometry_up_ground_miss_ecb_v2.json}
air_miss_capture=${6:-/tmp/falcon_special_geometry_up_air_miss_ecb_v2.json}
ledge_capture=${7:-/tmp/falcon_special_geometry_up_air_ledge_grab_v36.json}
behind_ledge_capture=${8:-/tmp/falcon_special_geometry_up_air_ledge_grab_behind_v1.json}
python=${PYTHON:-python3}

test -f "$ground_capture"
test -f "$air_capture"
test -f "$ground_miss_capture"
test -f "$air_miss_capture"
test -f "$ledge_capture"
test -f "$behind_ledge_capture"
cmake --build "$build_dir" --target movement_trace combat_test
mkdir -p "$output_dir"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_capture" \
    "$build_dir/movement_trace" \
    --native-output "$output_dir/ground-catch.csv" \
    --native-input-output "$output_dir/ground-catch.inputs"

"$python" "$root/tools/verify_ssbm_falcon_dive_air_catch.py" \
    "$air_capture"
"$build_dir/combat_test" \
    --ssbm-oracle falcon-dive-grab-geometry

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_miss_capture" \
    "$build_dir/movement_trace" \
    --native-output "$output_dir/ground-miss.csv" \
    --native-input-output "$output_dir/ground-miss.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_miss_capture" \
    "$build_dir/movement_trace" \
    --native-output "$output_dir/air-miss.csv" \
    --native-input-output "$output_dir/air-miss.inputs"

"$python" "$root/tools/verify_ssbm_falcon_dive_ledge.py" \
    "$ledge_capture"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ledge_capture" \
    "$build_dir/movement_trace" \
    --native-output "$output_dir/air-ledge.csv" \
    --native-input-output "$output_dir/air-ledge.inputs"

"$python" "$root/tools/verify_ssbm_falcon_dive_ledge.py" \
    "$behind_ledge_capture"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$behind_ledge_capture" \
    "$build_dir/movement_trace" \
    --native-output "$output_dir/air-ledge-behind.csv" \
    --native-input-output "$output_dir/air-ledge-behind.inputs"

echo "m4-falcon-dive-verification=pass ground_catch_frames=116 air_catch_geometry_poses=12 air_catch_geometry_cases=2 ground_miss_frames=103 air_miss_frames=165 air_ledge_frames=126 ledge_directions=2 ledge_catch=source_verified dynamic_frames=510"
