# [VRF-2026-0003] MSVC rejects portable tool CRT calls

ID: VRF-2026-0003
Status: unfixed
Severity: high
Detected commit: 27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8
Build hash: 27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8
Content hash: not-applicable
Fixed commit: not-fixed

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

Not fixed.

## Fix verification

Pending a corrective commit and following bookkeeping commit.
