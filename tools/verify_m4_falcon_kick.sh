#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
ground_capture=${1:-/tmp/falcon_special_geometry_down_ground_v1.json}
air_capture=${2:-/tmp/falcon_special_geometry_down_air_v1.json}
air_land_capture=${3:-/tmp/falcon_special_geometry_down_air_land_v1.json}
ground_edge_capture=${4:-/tmp/falcon_special_geometry_down_ground_edge_v1.json}
ground_hit_capture=${5:-/tmp/falcon_special_geometry_down_ground_hit_v1.json}
ground_wall_capture=${6:-/tmp/falcon_special_geometry_down_ground_wall_v1.json}
build_dir=${7:-"$root/build/wsl-release"}
output_dir=${8:-/tmp/m4_falcon_kick}
python=${PYTHON:-python3}

test -f "$ground_capture"
test -f "$air_capture"
test -f "$air_land_capture"
test -f "$ground_edge_capture"
test -f "$ground_hit_capture"
test -f "$ground_wall_capture"
cmake --build "$build_dir" --target m4_movement_trace
mkdir -p "$output_dir"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground.csv" \
    --native-input-output "$output_dir/ground.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/air.csv" \
    --native-input-output "$output_dir/air.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_land_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/air-land.csv" \
    --native-input-output "$output_dir/air-land.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_edge_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground-edge.csv" \
    --native-input-output "$output_dir/ground-edge.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_hit_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground-hit.csv" \
    --native-input-output "$output_dir/ground-hit.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_wall_capture" \
    "$build_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground-wall.csv" \
    --native-input-output "$output_dir/ground-wall.inputs"

echo "m4-falcon-kick-verification=pass ground_frames=70 air_frames=59 air_land_frames=65 ground_edge_frames=70 ground_hit_frames=77 ground_wall_frames=58"
