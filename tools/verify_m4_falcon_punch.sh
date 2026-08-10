#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
ground_capture=${1:-/tmp/falcon_special_geometry_neutral_ground_v1.json}
air_capture=${2:-/tmp/falcon_special_physics_neutral_air_v1.json}
output_dir=${3:-/tmp/m4_falcon_punch}
compiler=${CC:-cc}
python=${PYTHON:-python3}

test -f "$ground_capture"
test -f "$air_capture"
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
    "$root/src/tools/m4_movement_trace.c" \
    -o "$output_dir/pf_m4_movement_trace"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$ground_capture" \
    "$output_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/ground.csv" \
    --native-input-output "$output_dir/ground.inputs"

"$python" "$root/tools/compare_ssbm_movement.py" \
    "$air_capture" \
    "$output_dir/pf_m4_movement_trace" \
    --native-output "$output_dir/air.csv" \
    --native-input-output "$output_dir/air.inputs"

"$python" "$root/tools/verify_ssbm_falcon_punch.py" \
    "$root/tools/ssbm_falcon_punch_coverage.json" \
    "$ground_capture" \
    "$air_capture"

echo "m4-falcon-punch-verification=pass ground_frames=200 air_frames=200"
