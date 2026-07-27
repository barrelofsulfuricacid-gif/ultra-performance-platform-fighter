# Verifier records

The verifier writes one durable Markdown record per discovered issue. New
findings enter `issues/unfixed/`; resolved findings move to `issues/fixed/` in
a bookkeeping commit after the corrective commit.

Use `docs/templates/issue.md` and preserve all original reproduction evidence.

Until M3 replaces the provisional command with the full verifier agent,
`tools/verify_m2_kernel.sh` checks the deterministic M2 source directly for
strict-C17 behavior, identical scripted traces, four-player capacity, atomic
invalid-input rejection, episode completion, and forbidden platform or
allocation symbols.
