# Performance database

This directory holds the versioned SQLite schema, migrations, and any
explicitly approved milestone snapshot. Live per-commit databases belong under
the ignored `performance/local/` tree.

SQLite database files in this directory are ignored by default so an automated
benchmark cannot accidentally enter the post-commit workflow.

`schema.sql` is the authoritative schema for the M3 benchmark history. The
authored-C performance runner applies it transactionally, verifies schema
version `1`, records raw samples and comparison decisions, and regenerates
local graphs without requiring the SQLite command-line shell.
