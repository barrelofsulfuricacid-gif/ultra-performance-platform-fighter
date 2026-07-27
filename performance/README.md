# Performance evidence

Tracked milestone reports, benchmark definitions, and profiling analyses live
under `performance/`.

The durable locations are:

- `database/` for schemas, migrations, and explicitly approved snapshots.
- `graphs/` for milestone graph exports and their source-data references.
- `reports/` for milestone and optimization performance reports.

Per-commit raw measurements and verifier logs live under
`performance/local/commits/<commit>/` and are intentionally ignored by Git.
This prevents benchmark output from creating a new commit that would itself
require another benchmark run. Each record is keyed by the commit it measured.

M0 uses relative comparisons only. A result is comparable only when its
scenario, seed, compiler, flags, executable, design data, and machine
fingerprint match.
