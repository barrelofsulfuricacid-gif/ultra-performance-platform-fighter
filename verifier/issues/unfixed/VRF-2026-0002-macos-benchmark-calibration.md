# [VRF-2026-0002] macOS benchmark calibration rejects a short probe

ID: VRF-2026-0002
Status: unfixed
Severity: high
Detected commit: 27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8
Build hash: 27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8
Content hash: not-applicable
Fixed commit: not-fixed

## Reproduction

Run `M3 clean-machine CI` for detected commit
`27a8a3a1159c0ed83d12eed2fe4dfe10433f9ac8`. Both the `macos-15`
and `macos-15-intel` native release jobs build successfully, then run
`benchmarks --self-test` through CTest.

## Expected behavior

Benchmark calibration increases the iteration count until a sample reaches
the target duration. A first probe shorter than the monotonic clock's
observable resolution is calibration input, not a benchmark failure.

## Observed behavior

Both macOS jobs fail `benchmarks.self_test` with
`invalid benchmark timing or checksum`. The calibration path begins with one
iteration, but `complete_sample` rejects equal start and finish timestamps
before `run_measured_scenario` can double the iteration count.

## Evidence

- Workflow run:
  <https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30345167511>
- Failed jobs: `Native macos-15`, job ID `90229567582`; and
  `Native macos-15-intel`, job ID `90229567559`.
- The same revision passes benchmark qualification in the setup-contract job
  and passes the Ubuntu x86-64 and Ubuntu arm64 native release jobs.

## Resolution

Not fixed.

## Fix verification

Pending a corrective commit and following bookkeeping commit.
