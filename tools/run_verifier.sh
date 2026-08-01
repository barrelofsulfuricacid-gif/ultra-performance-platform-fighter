#!/bin/sh
set -eu

PF_REPOSITORY_ROOT=$(git rev-parse --show-toplevel)
export PF_REPOSITORY_ROOT

. "$PF_REPOSITORY_ROOT/tools/toolchain_common.sh"

pf_commit=${1:-$(git -C "$PF_REPOSITORY_ROOT" rev-parse HEAD)}
pf_output_dir=${2:-"$PF_REPOSITORY_ROOT/performance/local/commits/$pf_commit/verifier"}
pf_issue_dir=${PF_VERIFIER_ISSUE_DIRECTORY:-"$PF_REPOSITORY_ROOT/verifier/issues/unfixed"}
pf_acceptance="$PF_REPOSITORY_ROOT/verifier/acceptance_manifest.tsv"
pf_diff="$pf_output_dir/commit_files.txt"
pf_external="$pf_output_dir/external_checks.tsv"
pf_check_dir="$pf_output_dir/checks"
pf_artifact_dir=${PF_VERIFIER_ARTIFACT_DIRECTORY:-"$PF_REPOSITORY_ROOT/build/verifier-artifacts"}
pf_content_hash=1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526

printf '%s\n' "$pf_commit" |
    grep -Eq '^[0-9a-f]{40}$' ||
    pf_fail "verifier commit must be a 40-character lowercase hash"
git -C "$PF_REPOSITORY_ROOT" cat-file -e "$pf_commit^{commit}" ||
    pf_fail "verifier commit does not exist: $pf_commit"

mkdir -p \
    "$pf_output_dir" \
    "$pf_check_dir" \
    "$pf_artifact_dir" \
    "$pf_issue_dir"
git -C "$PF_REPOSITORY_ROOT" \
    diff-tree --root --no-commit-id --name-only -r "$pf_commit" \
    >"$pf_diff"
printf 'check\tstatus\tevidence\n' >"$pf_external"

pf_platform_key
pf_find_host_tools
pf_validate_compiler
CMAKE_COMMAND=$PF_CMAKE
export CMAKE_COMMAND

pf_sha256()
{
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{ print $1 }'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{ print $1 }'
    else
        pf_fail "sha256sum or shasum is required"
    fi
}

pf_record()
{
    pf_check_name=$1
    shift
    pf_check_log="$pf_check_dir/$pf_check_name.log"
    if "$@" >"$pf_check_log" 2>&1; then
        pf_check_status=pass
    else
        pf_check_status=fail
    fi
    printf '%s\t%s\t%s\n' \
        "$pf_check_name" \
        "$pf_check_status" \
        "${pf_check_log#"$PF_REPOSITORY_ROOT"/}" \
        >>"$pf_external"
}

pf_defer()
{
    printf '%s\tdeferred\t%s\n' "$1" "$2" >>"$pf_external"
}

pf_diff_matches()
{
    grep -Eq "$1" "$pf_diff"
}

pf_run_sanitizer()
{
    pf_asan_options=halt_on_error=1
    if [ "${PF_VERIFIER_DISABLE_LEAK_CHECKS:-0}" = "1" ] ||
       [ -n "${CODEX_CI:-}" ]; then
        pf_asan_options=detect_leaks=0:halt_on_error=1
        echo "leak-sanitizer=deferred reason=workspace-proc-tracing-restriction"
    else
        echo "leak-sanitizer=enabled"
    fi
    ASAN_OPTIONS=$pf_asan_options \
        PF_SDL_UNIX_CONSOLE_BUILD=ON \
        "$PF_REPOSITORY_ROOT/tools/workflow.sh" sanitizer
}

"$PF_REPOSITORY_ROOT/tools/workflow.sh" verifier
pf_verifier="$PF_REPOSITORY_ROOT/build/verifier/pf_verifier"
[ -x "$pf_verifier" ] || pf_fail "verifier executable is missing"

pf_record \
    m0-contract \
    "$PF_REPOSITORY_ROOT/tools/verify_m0.sh" \
    "$pf_commit"
pf_record \
    m1-foundation \
    "$PF_REPOSITORY_ROOT/tools/verify_m1_foundation.sh" \
    "$pf_artifact_dir/m1_foundation"
pf_record \
    m1-workflow \
    "$PF_REPOSITORY_ROOT/tools/verify_m1_workflow.sh"
pf_record \
    m1-setup \
    "$PF_REPOSITORY_ROOT/tools/verify_m1_setup.sh"
pf_record \
    m2-kernel \
    "$PF_REPOSITORY_ROOT/tools/verify_m2_kernel.sh" \
    "$pf_artifact_dir/m2_kernel"
pf_record \
    m2-replay \
    "$PF_REPOSITORY_ROOT/tools/verify_m2_replay.sh" \
    "$pf_artifact_dir/m2_replay"
pf_record \
    mechanical-oracle \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_movement.sh" \
    "$pf_artifact_dir/m4_movement"
pf_record \
    m4-combat \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_combat.sh" \
    "$pf_artifact_dir/m4_combat"
pf_record \
    m4-item \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_item.sh" \
    "$pf_artifact_dir/m4_item"
pf_record \
    m4-projectile \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_projectile.sh" \
    "$pf_artifact_dir/m4_projectile"
pf_record \
    m4-reflector \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_reflector.sh" \
    "$pf_artifact_dir/m4_reflector"
pf_record \
    m4-charge \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_charge.sh" \
    "$pf_artifact_dir/m4_charge"
pf_record \
    m4-browser-adapter \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_browser.sh" \
    "$pf_artifact_dir/m4_browser"
pf_record \
    collision-hitbox-overlay \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_collision_overlay.sh" \
    "$pf_artifact_dir/m4_collision_overlay"
pf_record \
    m4-local-match-flow \
    "$PF_REPOSITORY_ROOT/tools/verify_m4_match_flow.sh" \
    "$pf_artifact_dir/m4_match_flow"
pf_record \
    m3-regression-qualification \
    "$PF_REPOSITORY_ROOT/tools/verify_m3_performance.sh" \
    "$pf_artifact_dir/m3_performance_qualification"
pf_record \
    m3-performance \
    "$PF_REPOSITORY_ROOT/tools/run_performance.sh" \
    commit \
    "$pf_artifact_dir/m3_performance"

if pf_diff_matches \
    '^(CMakeLists\.txt|CMakePresets\.json|cmake/|include/pf/|src/(benchmarks|checkpoint|headless|presentation|sim|verifier|web_client)/|tests/(presentation|sim|web)/|tools/(bootstrap|workflow)\.)'
then
    pf_record \
        sanitizer \
        pf_run_sanitizer
else
    pf_defer sanitizer "not selected: commit does not affect compiled deterministic code"
fi

if pf_diff_matches \
    '^(CMakeLists\.txt|CMakePresets\.json|cmake/|include/pf/render_packet\.h|src/(presentation|web_client)/|tests/presentation/|tools/(serve_web|verify_web_smoke)\.)'
then
    if [ "${PF_VERIFIER_RUN_WEB:-0}" = "1" ]; then
        pf_record \
            browser-smoke \
            "$PF_REPOSITORY_ROOT/tools/workflow.sh" \
            web
        pf_record \
            browser-runtime \
            "$PF_REPOSITORY_ROOT/tools/verify_web_smoke.sh"
    else
        pf_defer \
            browser-smoke \
            "selected for browser CI; set PF_VERIFIER_RUN_WEB=1 for local Chrome execution"
    fi
else
    pf_defer browser-smoke "not selected: commit does not affect browser presentation"
fi

pf_build_hash=$(pf_sha256 "$pf_verifier")

if "$pf_verifier" \
    --verify \
    "$pf_acceptance" \
    "$pf_diff" \
    "$pf_external" \
    "$pf_output_dir" \
    "$pf_issue_dir" \
    "$pf_commit" \
    "$pf_build_hash" \
    "$pf_content_hash"
then
    pf_verifier_status=pass
else
    pf_verifier_status=fail
fi

printf 'verifier-workflow=%s commit=%s manifest=%s\n' \
    "$pf_verifier_status" \
    "$pf_commit" \
    "$pf_output_dir/pass_manifest.md"
[ "$pf_verifier_status" = pass ]
