#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
ground_hit_capture=${1:-/tmp/falcon_special_geometry_side_ground_hit_v1.json}
output_dir=${2:-/tmp/raptor_boost}
remaining_capture=${3:-/tmp/falcon_special_geometry_side_misses_ecb_v2.json}
air_hit_capture=${4:-/tmp/falcon_special_geometry_side_air_hit_floor_v1.json}
ground_edge_capture=${5:-/tmp/falcon_special_geometry_side_ground_edge_v3.json}
item_search_capture=${6:-/tmp/falcon_special_geometry_side_ground_item_hit_v6.json}
compiler=${CC:-cc}
python=${PYTHON:-python3}

test -f "$ground_hit_capture"
test -f "$remaining_capture"
test -f "$air_hit_capture"
test -f "$ground_edge_capture"
test -f "$item_search_capture"
mkdir -p "$output_dir"

common_flags="
    -std=c17
    -O2
    -g
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wformat=2
    -Wmissing-prototypes
    -Wshadow
    -Wstrict-prototypes
    -Wundef
    -Wwrite-strings
"

# shellcheck disable=SC2086
"$compiler" $common_flags \
    -I"$root/include" \
    -I"$root/src/sim" \
    "$root/src/sim/sim.c" \
    "$root/src/sim/sim_combat.c" \
    "$root/src/sim/sim_content.c" \
    "$root/src/sim/sim_falcon_frame_data.c" \
    "$root/src/sim/sim_fixed_math.c" \
    "$root/src/sim/sim_hsd_pose.c" \
    "$root/src/sim/sim_ssbm_common_data.c" \
    "$root/src/sim/sim_ssbm_damage.c" \
    "$root/src/sim/sim_ssbm_stage_data.c" \
    "$root/src/sim/sim_event.c" \
    "$root/src/sim/sim_item.c" \
    "$root/src/sim/sim_projectile.c" \
    "$root/src/sim/sim_reflector.c" \
    "$root/src/sim/sim_charge.c" \
    "$root/src/sim/sim_movement.c" \
    "$root/src/sim/sim_replay.c" \
    "$root/src/sim/sim_rl.c" \
    "$root/src/sim/sim_sha256.c" \
    "$root/src/sim/sim_snapshot.c" \
    "$root/src/sim/sim_tick.c" \
    "$root/src/tools/movement_trace.c" \
    -o "$output_dir/movement_trace"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_hit_capture" \
    "$output_dir/movement_trace" \
    --native-output "$output_dir/ground-hit.csv" \
    --native-input-output "$output_dir/ground-hit.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$remaining_capture" \
    "$output_dir/movement_trace" \
    --special-geometry-route side_ground_miss \
    --native-output "$output_dir/ground-miss.csv" \
    --native-input-output "$output_dir/ground-miss.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$remaining_capture" \
    "$output_dir/movement_trace" \
    --special-geometry-route side_air_miss \
    --native-output "$output_dir/air-miss.csv" \
    --native-input-output "$output_dir/air-miss.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_hit_capture" \
    "$output_dir/movement_trace" \
    --special-geometry-route side_air_hit_floor \
    --native-output "$output_dir/air-hit.csv" \
    --native-input-output "$output_dir/air-hit.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_edge_capture" \
    "$output_dir/movement_trace" \
    --special-geometry-route side_ground_edge \
    --native-output "$output_dir/ground-edge.csv" \
    --native-input-output "$output_dir/ground-edge.inputs"

"$python" "$root/tools/verify_ssbm_falcon_item_search.py" \
    "$item_search_capture"

"$python" "$root/tools/verify_ssbm_falcon_raptor_boost.py" \
    "$root/tools/ssbm_falcon_raptor_boost_coverage.json" \
    "$ground_hit_capture" \
    "$remaining_capture" \
    "$air_hit_capture" \
    "$ground_edge_capture" \
    "$item_search_capture"

echo "m4-raptor-boost-verification=pass ground_hit_frames=46 ground_miss_frames=80 air_miss_frames=180 air_hit_floor_frames=145 ground_edge_frames=51 item_search_frames=155 total_frames=657"
