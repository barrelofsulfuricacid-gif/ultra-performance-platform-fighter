# [VRF-2026-2603] external:m2-replay

ID: VRF-2026-2603
Status: fixed
Severity: high
Detected commit: 320906fd49b923b739769359a653c2012d0974ee
Build hash: cf1313ad04c50ef9115990daf607aa7aecea7a92c57eff37ec1ad9304e7f364e
Content hash: 1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526
Fixed commit: 7aa766650cb6f48491ec889374d195d12c2b3dbc

## Reproduction

Build the Web target at a commit whose canonical replay digest differs from
the current source, then run
`tools/run_verifier.sh 320906fd49b923b739769359a653c2012d0974ee` without
`PF_REQUIRE_WEB_REPLAY=1`.

## Expected behavior

The local verifier's optional Web replay lane is explicitly deferred unless
the caller requests it. The CI lane that sets `PF_REQUIRE_WEB_REPLAY=1` must
continue to require and compare a freshly built WebAssembly corpus.

## Observed behavior

The optional `m2-replay` check consumed any existing
`build/web/pf_replay_corpus.js`, even when that untracked build artifact came
from an older source revision. At the detected commit, the native corpus
correctly produced replay SHA-256
`36452288611860eea89e051f26ce32dfe1a431537a0dae9bb7379047eadf1c2f`, while
the stale Web artifact still produced
`0a48c51b303ccd8a7f2f6bc8d65763e9f96205cc374dd7b1b721e894c44bb43f`.
All 12 configured CTest targets and the other 15 verifier checks passed.

## Evidence

`performance/local/commits/320906fd49b923b739769359a653c2012d0974ee/verifier/checks/m2-replay.log`

The differing native and Web outputs were preserved in
`build/verifier-artifacts/m2_replay/native.txt` and
`build/verifier-artifacts/m2_replay/wasm.txt` in the detecting WSL workspace.

## Resolution

Corrective commit `7aa766650cb6f48491ec889374d195d12c2b3dbc` gates use of
the generated Web corpus behind `PF_REQUIRE_WEB_REPLAY=1`. Optional local runs
now report WebAssembly as deferred and cannot consume an unversioned stale
artifact. The flag still requires the artifact, Node execution, the expected
digest, and byte equality, preserving the mandatory Web CI contract.

## Fix verification

- With the stale Web artifact present, the optional replay verifier passed and
  reported `wasm=deferred`; mandatory mode rejected the old digest.
- A fresh Emscripten build followed by mandatory mode passed the native/Wasm
  byte comparison at replay SHA-256
  `36452288611860eea89e051f26ce32dfe1a431537a0dae9bb7379047eadf1c2f`.
- The corrective commit passed all 16 WSL verifier checks with zero failures.
  Benchmark run 12 covered nine available scenarios with zero suspected or
  confirmed regressions; Windows/MSVC remained green at 18/18 tests.
- This following bookkeeping commit moves the report only after the corrective
  commit passed, preserving the required two-commit issue lifecycle.
