# Verifier records

The verifier writes one durable Markdown record per discovered issue. New
findings enter `issues/unfixed/`; resolved findings move to `issues/fixed/` in
a bookkeeping commit after the corrective commit.

Use `docs/templates/issue.md` and preserve all original reproduction evidence.
