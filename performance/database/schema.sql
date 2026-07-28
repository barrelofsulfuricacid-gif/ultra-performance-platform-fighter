PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;

CREATE TABLE IF NOT EXISTS schema_metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
) WITHOUT ROWID;

INSERT OR IGNORE INTO schema_metadata(key, value)
VALUES ('schema_version', '1');

CREATE TABLE IF NOT EXISTS benchmark_runs (
    id INTEGER PRIMARY KEY,
    commit_hash TEXT NOT NULL,
    dirty_state INTEGER NOT NULL CHECK (dirty_state IN (0, 1)),
    run_mode TEXT NOT NULL CHECK (run_mode IN ('commit', 'milestone')),
    status TEXT NOT NULL CHECK (
        status IN ('running', 'pass', 'regression', 'failed')),
    started_utc TEXT NOT NULL,
    finished_utc TEXT,
    benchmark_schema_version INTEGER NOT NULL,
    build_configuration TEXT NOT NULL,
    compiler TEXT NOT NULL,
    compiler_flags TEXT NOT NULL,
    dependency_hash TEXT NOT NULL,
    content_hash TEXT NOT NULL,
    machine_fingerprint TEXT NOT NULL,
    os_fingerprint TEXT NOT NULL,
    cpu_fingerprint TEXT NOT NULL,
    power_metadata TEXT NOT NULL,
    thermal_metadata TEXT NOT NULL,
    executable_hash TEXT NOT NULL,
    sample_target_ns INTEGER NOT NULL,
    repetition_count INTEGER NOT NULL,
    failure_reason TEXT
);

CREATE TABLE IF NOT EXISTS benchmark_scenarios (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    scenario_version INTEGER NOT NULL,
    seed INTEGER NOT NULL,
    unit TEXT NOT NULL,
    capability_stage TEXT NOT NULL,
    UNIQUE(name, scenario_version, seed)
);

CREATE TABLE IF NOT EXISTS benchmark_samples (
    run_id INTEGER NOT NULL REFERENCES benchmark_runs(id) ON DELETE CASCADE,
    scenario_id INTEGER NOT NULL
        REFERENCES benchmark_scenarios(id) ON DELETE RESTRICT,
    sample_index INTEGER NOT NULL,
    iterations INTEGER NOT NULL,
    logical_ticks INTEGER NOT NULL,
    elapsed_ns INTEGER NOT NULL,
    rate_per_second REAL NOT NULL,
    ns_per_operation REAL NOT NULL,
    checksum INTEGER NOT NULL,
    PRIMARY KEY(run_id, scenario_id, sample_index)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS benchmark_summaries (
    run_id INTEGER NOT NULL REFERENCES benchmark_runs(id) ON DELETE CASCADE,
    scenario_id INTEGER NOT NULL
        REFERENCES benchmark_scenarios(id) ON DELETE RESTRICT,
    sample_count INTEGER NOT NULL,
    median_rate REAL NOT NULL,
    mad_rate REAL NOT NULL,
    p50_ns REAL NOT NULL,
    p95_ns REAL NOT NULL,
    p99_ns REAL NOT NULL,
    state_bytes INTEGER NOT NULL,
    snapshot_bytes INTEGER NOT NULL,
    baseline_run_id INTEGER REFERENCES benchmark_runs(id),
    relative_change REAL,
    relative_noise_floor REAL,
    meaningful_threshold REAL,
    confidence_low REAL,
    confidence_high REAL,
    comparison_status TEXT NOT NULL CHECK (
        comparison_status IN (
            'baseline',
            'compatible',
            'suspected_regression',
            'confirmed_regression',
            'invalid')),
    comparison_reason TEXT NOT NULL,
    PRIMARY KEY(run_id, scenario_id)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS benchmark_unavailable (
    run_id INTEGER NOT NULL REFERENCES benchmark_runs(id) ON DELETE CASCADE,
    scenario_id INTEGER NOT NULL
        REFERENCES benchmark_scenarios(id) ON DELETE RESTRICT,
    reason_code TEXT NOT NULL,
    details TEXT NOT NULL,
    PRIMARY KEY(run_id, scenario_id)
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS benchmark_runs_compatibility
ON benchmark_runs(
    dirty_state,
    build_configuration,
    compiler,
    compiler_flags,
    dependency_hash,
    content_hash,
    machine_fingerprint,
    os_fingerprint,
    cpu_fingerprint,
    benchmark_schema_version,
    id);

CREATE INDEX IF NOT EXISTS benchmark_summaries_scenario
ON benchmark_summaries(scenario_id, run_id);
