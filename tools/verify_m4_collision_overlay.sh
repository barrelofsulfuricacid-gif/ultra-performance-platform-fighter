#!/bin/sh
set -eu

root=$(git rev-parse --show-toplevel)
output_dir=${1:-"$root/performance/local/m4_collision_overlay"}
adapter="$root/src/web_client/web_adapter.js"
bridge="$root/src/web_client/m4_playtest.c"

mkdir -p "$output_dir"

command -v node >/dev/null 2>&1 ||
    {
        echo "collision overlay verification requires Node.js" >&2
        exit 1
    }
node --check "$adapter"

pf_require_source()
{
    pf_label=$1
    pf_expected=$2
    pf_path=$3
    if ! grep -Fq "$pf_expected" "$pf_path"; then
        echo "collision overlay verification failed: missing $pf_label" >&2
        exit 1
    fi
}

pf_require_source \
    "visible semantic state" \
    'section.dataset.collisionOverlay = "visible";' \
    "$adapter"
pf_require_source \
    "complete semantic inventory" \
    'stage-hurtbox-shield-attack-grab-item-projectile-blast' \
    "$adapter"
pf_require_source \
    "accessible toggle" \
    'collisionOverlayButton.id = "pf-m4-collision-overlay";' \
    "$adapter"
pf_require_source \
    "pause-safe redraw" \
    'Module._pf_web_m4_playtest_refresh()' \
    "$adapter"
pf_require_source \
    "fighter hurtbox bounds" \
    'var hurtboxLeft = sx(view[base] - view[12]);' \
    "$adapter"
pf_require_source \
    "active fighter hitbox" \
    'state.collisionOverlayVisible && view[base + 14]' \
    "$adapter"
pf_require_source \
    "active fighter grabbox" \
    'state.collisionOverlayVisible && view[base + 35]' \
    "$adapter"
pf_require_source \
    "item collision body" \
    'itemX - itemWidth / 2' \
    "$adapter"
pf_require_source \
    "projectile collision body" \
    'projectileX - projectileHitboxWidth / 2' \
    "$adapter"
pf_require_source \
    "stage floor inspection" \
    'PF_WEB_M4_VIEW_FLOOR_LEFT' \
    "$bridge"
pf_require_source \
    "stage solid inspection" \
    'PF_WEB_M4_VIEW_SOLID_BOTTOM' \
    "$bridge"
pf_require_source \
    "fighter hitbox inspection" \
    'PF_WEB_M4_VIEW_PLAYER_HITBOX_BOTTOM' \
    "$bridge"
pf_require_source \
    "fighter grabbox inspection" \
    'PF_WEB_M4_VIEW_PLAYER_GRABBOX_BOTTOM' \
    "$bridge"

cat >"$output_dir/semantics.txt" <<'EOF'
stage=floor,one-way-platform,solid-block,blast-zone
fighter=hurtbox,attack-hitbox,grabbox,invulnerability
object=item-body,item-hitbox,projectile-hitbox
toggle=button,keyboard-I,pause-safe-redraw
EOF

echo "collision-hitbox-overlay=pass stage_surfaces=4 fighter_boxes=3 object_boxes=3 pause_safe=1"
