#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
ground_hit_capture=${1:-/tmp/falcon_special_geometry_side_ground_hit_v1.json}
output_dir=${2:-/tmp/m4_raptor_boost}
remaining_capture=${3:-/tmp/falcon_special_geometry_side_misses_ecb_v2.json}
compiler=${CC:-cc}
python=${PYTHON:-python3}

test -f "$ground_hit_capture"
test -f "$remaining_capture"
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
    "$root/src/tools/m4_movement_trace.c" \
    -o "$output_dir/pf_m4_movement_trace"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_hit_capture" \
    "$output_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground-hit.csv" \
    --native-input-output "$output_dir/ground-hit.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$remaining_capture" \
    "$output_dir/pf_m4_movement_trace" \
    --special-geometry-route side_ground_miss \
    --native-output "$output_dir/ground-miss.csv" \
    --native-input-output "$output_dir/ground-miss.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$remaining_capture" \
    "$output_dir/pf_m4_movement_trace" \
    --special-geometry-route side_air_miss \
    --native-output "$output_dir/air-miss.csv" \
    --native-input-output "$output_dir/air-miss.inputs"

echo "m4-raptor-boost-verification=pass ground_hit_frames=46 ground_miss_frames=80 air_miss_frames=180 total_frames=306"
