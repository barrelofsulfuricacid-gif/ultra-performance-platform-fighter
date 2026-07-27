# Performance database

This directory holds the versioned SQLite schema, migrations, and any
explicitly approved milestone snapshot. Live per-commit databases belong under
the ignored `performance/local/` tree.

SQLite database files in this directory are ignored by default so an automated
benchmark cannot accidentally enter the post-commit workflow.
