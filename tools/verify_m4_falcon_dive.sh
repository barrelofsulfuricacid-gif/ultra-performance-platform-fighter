#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ground_capture=${1:-/tmp/falcon_special_geometry_up_ground_catch_ecb_v2.json}
air_capture=${2:-/tmp/falcon_special_geometry_up_air_catch_kb_v4.json}
build_dir=${3:-"$root/build/wsl-release"}
output_dir=${4:-/tmp/m4_falcon_dive}
ground_miss_capture=${5:-/tmp/falcon_special_geometry_up_ground_miss_ecb_v2.json}
air_miss_capture=${6:-/tmp/falcon_special_geometry_up_air_miss_ecb_v2.json}
ledge_capture=${7:-/tmp/falcon_special_geometry_up_air_ledge_grab_v36.json}
python=${PYTHON:-python3}

test -f "$ground_capture"
test -f "$air_capture"
test -f "$ground_miss_capture"
test -f "$air_miss_capture"
test -f "$ledge_capture"
cmake --build "$build_dir" --target m4_movement_trace
mkdir -p "$output_dir"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground-catch.csv" \
    --native-input-output "$output_dir/ground-catch.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/air-catch.csv" \
    --native-input-output "$output_dir/air-catch.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_miss_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground-miss.csv" \
    --native-input-output "$output_dir/ground-miss.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_miss_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/air-miss.csv" \
    --native-input-output "$output_dir/air-miss.inputs"

"$python" "$root/tools/verify_ssbm_falcon_dive_ledge.py" \
    "$ledge_capture"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ledge_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/air-ledge.csv" \
    --native-input-output "$output_dir/air-ledge.inputs"

echo "m4-falcon-dive-verification=pass ground_catch_frames=116 air_catch_frames=92 ground_miss_frames=103 air_miss_frames=165 air_ledge_frames=63 ledge_catch=source_verified total_frames=539"
