# [VRF-2026-0003] MSVC rejects portable tool CRT calls

ID: VRF-2026-0003
Status: fixed
Severity: high
Detected commit: 27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8
Build hash: 27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8
Content hash: not-applicable
Fixed commit: c175db53b96dfafeb49892a9c46ada596b130c12

## Reproduction

Run the `M3 clean-machine CI` `Native windows-2025` release workflow for
detected commit `27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8`.
The benchmark target compiles with MSVC, `/W4`, and `/WX`.

## Expected behavior

The authored-C benchmark and verifier executables compile under the supported
MSVC toolchain while retaining strict warnings. Their standard C `getenv` and
`fopen` uses are intentional and confined to these command-line tools.

## Observed behavior

MSVC emits C4996 deprecation warnings for `getenv` in `main.c` and `fopen` in
`history.c`. Because project warnings are errors, C2220 stops compilation
before the Windows release tests can run. After the benchmark target received
the target-local compatibility definition, the next clean run exposed the
same C4996 policy warning for `fopen` in `src/verifier/main.c`.

## Evidence

- Workflow run:
  <https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30345167511>
- Failed job: `Native windows-2025`, job ID `90229567723`.
- Diagnostics identify `main.c` lines 44, 607, and 623 and `history.c` lines
  97, 1470, and 1628.
- Follow-up workflow run:
  <https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30346116745>
- Follow-up failed job: `Native windows-2025`, job ID `90232611900`.
  The benchmark sources compile, then the verifier target reports C4996 for
  `fopen`.
- The same revision compiles the benchmark target with strict warnings on
  Ubuntu and macOS.

## Resolution

Corrective commit `39181569577e338353a55e7a72a1cafa079d3093`
adds `_CRT_SECURE_NO_WARNINGS` only to the MSVC benchmark target. Follow-up
corrective commit `c175db53b96dfafeb49892a9c46ada596b130c12`
applies the same target-local policy to the verifier executable after its
previously hidden C4996 diagnostics became reachable. Project warnings remain
at `/W4 /WX`, and no global or runtime target receives the compatibility
definition.

## Fix verification

- The benchmark and verifier self-tests pass locally, and both corrective
  trees passed the 13-check post-commit verifier.
- `M3 clean-machine CI` run
  <https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30346494750>
  completed `Native windows-2025` successfully under MSVC with the release
  benchmark and verifier targets enabled.
- The same clean-machine run passed both macOS architectures, Ubuntu x86-64
  and arm64, sanitizers, Emscripten/Chrome, Gymnasium, and setup/profile jobs.
- This following bookkeeping commit moves the report only after both
  corrective commits and clean-machine verification passed.
