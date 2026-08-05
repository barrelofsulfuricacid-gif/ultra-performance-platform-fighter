#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
ground_capture=${1:-/tmp/falcon_special_geometry_down_ground_v1.json}
build_dir=${2:-"$root/build/wsl-release"}
output_dir=${3:-/tmp/m4_falcon_kick}
python=${PYTHON:-python3}

test -f "$ground_capture"
cmake --build "$build_dir" --target m4_movement_trace
mkdir -p "$output_dir"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground.csv" \
    --native-input-output "$output_dir/ground.inputs"

echo "m4-falcon-kick-verification=pass ground_frames=70"
