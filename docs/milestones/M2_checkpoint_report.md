# M2 checkpoint report

**Status:** Owner contract accepted; schema-2 qualification passed

**Baseline build commit:** `e59672c2a719f084c289330f49bc656b5498cda2`

**Content hash:** Not applicable; authoritative gameplay data begins in M5.

## Scope delivered

M2 establishes the deterministic headless simulation and reinforcement-learning
foundation:

- Caller-owned, preallocated two/four-player simulation state and scratch
  memory with seeded reset and a fixed 60 Hz tick.
- Versioned identity, input, state, observation, snapshot, replay, and RL
  contracts.
- Canonical save/load/clone and SHA-256 state hashing.
- A length-delimited replay container with allocation-free verification and
  exact first-divergence reporting.
- One shared authored-C replay trace exercised natively, in Node-hosted
  WebAssembly, and in the browser inspector.
- An owner-approved C RL ABI with single/batched reset and step, raw analog
  actions, structured and compact seed-redacted policy observations,
  legal-button masks, exact shaped/outcome float32 rewards, and diagnostic
  flags.
- An uncapped headless throughput runner and an optional Gymnasium 1.3 vector
  adapter over the C ABI.

The arena motion in this checkpoint is a deterministic conformance fixture,
not final gameplay. Dash dancing, slow walking, final short/full-hop behavior,
combat, and the first playable stage begin in M4.

## Acceptance criteria

| Criterion | Result | Evidence |
|---|---|---|
| Compatible native and WebAssembly inputs produce identical hashes | Pass | 180-tick corpus byte-matches in `tools/verify_m2_replay.sh` and CI |
| Save/load followed by replay reaches the exact same state | Pass | Snapshot and replay corpus tests |
| Tick code has no allocation or platform calls | Pass | Deterministic object undefined-symbol scan |
| State capacity covers four players and team mode | Pass | Four-player world, replay, and RL tests |
| Corruption and compatibility mismatches fail with diagnostics | Pass | Snapshot/replay negative corpus |
| Headless initializes without client systems | Pass | CMake link guard and headless workflow |
| Headless runs uncapped and reports throughput | Pass | `headless --throughput` |
| One API supports duel and team configurations | Pass | RL duel/team reward tests |
| Batched stepping reduces Python boundary overhead | Pass | 18.2x–21.6x across repeated 64-environment runs |
| Observations and rewards are versioned, deterministic, and tested | Pass | TDR-0008, C/Python conformance suites, and schema-2 CI |
| Gymnasium wrapper passes its API tests | Pass | Gymnasium 1.3 CI job |

## Verification

GitHub Actions run
[`30308891149`](https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30308891149)
passed all nine baseline M2 jobs:

- Native release builds on Ubuntu x64/Arm64, macOS Intel/Arm64, and Windows
  x64.
- Linux address/undefined-behavior sanitizers.
- Setup and deterministic-kernel contract verification.
- Gymnasium 1.3 wrapper tests and boundary benchmark.
- Emscripten/Chrome WebGL 2, replay status, and replay-inspector checks.

Local GCC debug, release, and sanitizer workflows each passed 13/13 tests;
headless passed 8/8. The native and WebAssembly replay outputs match exactly:

- Replay SHA-256:
  `fd86a7c0801302d9a5feb203792a6feef939724054a9b3551aeca99f7d11066e`.
- Final state SHA-256:
  `7571f4ec1375cecbde2c6dc1b9e8ea00a8d368c876bda87e8adcdb354af83ea7`.

For RL schema 2, strict C17 kernel verification, the 13-test debug/release
workflows, the 8-test headless workflow, and all five Gymnasium tests pass
locally. AddressSanitizer/UndefinedBehaviorSanitizer pass 13/13 with leak
discovery disabled because this workspace's tracing sandbox prevents
LeakSanitizer from reading `/proc`. Native and WebAssembly replay output still
byte-match the hashes above.

Schema-2 GitHub Actions run
[`30317408257`](https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30317408257)
passed all nine jobs, including the normal Linux sanitizer lane, Gymnasium,
Windows, Ubuntu x64/Arm64, macOS Intel/Arm64, and generated WebAssembly in
Chrome.

The owner-only browser checkpoint is
[`platform-fighter-m1.lol1234.chatgpt.site`](https://platform-fighter-m1.lol1234.chatgpt.site).
It executes and verifies the same 180-tick trace inside WebAssembly and exposes
a draggable timeline with all 181 state hashes.

## Performance evidence

On the local M2 qualification host:

- Latest native sample: 15,974,047 single-step and 15,246,088 batched
  environment-ticks/s, with exact final-state equality.
- Repeated Python boundary samples: 18.2x–21.6x faster with one batched C call
  than with one `ctypes` call per environment for 64 environments.
- Latest Python sample: 317,231 versus 6,866,198
  environment-ticks/s (21.6441x).

These are same-host comparative results, not machine-independent performance
claims. M3 establishes persistent per-commit benchmark history and regression
statistics.

The owner-selected engagement reward adds deterministic work to each RL step.
An interleaved same-host comparison against the sparse-reward M2 baseline
measured schema 2 at 14,457,153 single-step and 14,734,170 batched
environment-ticks/s, changes of -4.71% and -5.62%, respectively. The common
duel path shares one potential between both players and uses a constant shift
rather than runtime division. Full method and distribution notes are in
[`performance/reports/2026-07-27_m2_rl_schema2.md`](../../performance/reports/2026-07-27_m2_rl_schema2.md).

## Unresolved issues

No critical verifier issue blocks M2. The agent-side browser connection was
unavailable during final visual QA; the hosted production build validated
successfully and the clean Chrome CI job passed, so owner-browser interaction
is the remaining visual gate.

## Owner decision record

The owner resolved all four RL choices on 2026-07-27:

| Decision | Accepted contract |
|---|---|
| Action vocabulary | Raw normalized analog actions |
| Observation | Keep structured and 36-word compact forms; redact the seed from normal bot observations and retain it only through explicit simulation diagnostics/reset ownership |
| Reward | Add a bounded potential-based engagement delta to the terminal outcome component |
| Batch behavior | Fixed four-action stride and independent per-environment status/error behavior |

RL schema 2 implements these choices. Compact words 2–3 and the structured
seed field are zero in `pf_rl_transition`; direct `pf_sim_observe` remains the
explicit diagnostic surface. The engagement component rewards reducing
horizontal distance to the nearest opponent, penalizes separation, and has a
total potential range of `0.25`, leaving the `+1/-1` match outcome dominant.

M3 begins only after this schema-2 change passes the full qualification matrix
and is merged.

## Follow-up

After schema-2 qualification and merge, M3 adds the persistent performance
database, graphs, regression comparison, and verifier qualification system.
