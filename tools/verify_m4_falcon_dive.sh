#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ground_capture=${1:-/tmp/falcon_special_geometry_up_ground_catch_ecb_v2.json}
air_capture=${2:-/tmp/falcon_special_geometry_up_air_catch_kb_v4.json}
build_dir=${3:-"$root/build/wsl-release"}
output_dir=${4:-/tmp/m4_falcon_dive}
python=${PYTHON:-python3}

test -f "$ground_capture"
test -f "$air_capture"
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

echo "m4-falcon-dive-verification=pass ground_frames=116 air_frames=92"
