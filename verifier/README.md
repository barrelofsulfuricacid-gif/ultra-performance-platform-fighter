# Verifier agent

`verifier/acceptance_manifest.tsv` is the machine-readable acceptance surface.
The authored-C `pf_verifier` reads it with the commit file list and external
check manifest, then:

- drives deterministic scripted and exploratory matches through the public
  player/RL action layer;
- runs eight seeded production M4 stock matches through ordinary player input,
  then verifies a lockstep twin, saved-state rewind/resimulation, and encoded
  replay for every match against one pinned cross-target digest;
- verifies seed-redacted policy observations, diagnostic seed visibility,
  four-player snapshot continuation, state hashes, and tolerant semantic
  render packets;
- enforces that every active acceptance entry has an internal or external
  result;
- writes `pass_manifest.md` even when no issue exists; and
- writes one durable Markdown record per failed check.

Run the complete current-commit workflow with:

```sh
./tools/run_verifier.sh
```

New findings enter `issues/unfixed/`; critical failures block milestone
completion. Resolved findings move to `issues/fixed/` in a bookkeeping commit
after the corrective commit, preserving all original reproduction evidence
and recording the actual fix hash. `tools/verify_m3_verifier.sh` qualifies the
mechanical, visual, menu, determinism, acceptance-coverage, and issue-lifecycle
detectors with isolated fixtures.
