# M1 reproducible foundation progress

**Status:** In progress

## M1.1 repository and build products

The first permanent foundation slice establishes:

- `sim`, an authored-C17 static library with a public `pf_` ABI.
- `headless`, a separate renderer/audio/input-free executable that explicitly
  links only `sim`.
- A fixed 60 Hz simulation contract and versioned ABI smoke surface.
- Strict warnings-as-errors for GCC, Clang, and MSVC project code.
- Native direct-compiler and CMake/CTest verification.

This is intentionally not the M2 simulation kernel. Seeded world state, input
frames, ticks, observations, save/load, and hashing remain M2 work.

## M1.3 workflow scaffold

The tracked workflow now includes all required design, generated-data,
original-asset, performance, verifier-issue, human-feedback, optimization, and
release locations. Versioned templates and valid lifecycle fixtures are checked
by `tools/verify_m1_workflow.sh`. The validator also proves that per-commit
evidence remains ignored, preventing the post-commit workflow from recursively
creating commits.

## Remaining M1 work

- Add clean native-client, web-client, tools, benchmark, and verifier targets.
- Add pinned POSIX and PowerShell bootstrap flows.
- Add debug, sanitizer, release, profile, benchmark, headless, and web presets.
- Add clean-machine Windows, macOS, Linux, and web CI.
- Complete native/web render and platform adoption spikes.
- Stop for the mandatory owner setup checkpoint.
