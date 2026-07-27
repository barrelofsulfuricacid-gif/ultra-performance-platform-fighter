# M2 checkpoint report

**Status:** Owner review

**Build commit:** `e59672c2a719f084c289330f49bc656b5498cda2`

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
- A stable candidate C RL ABI with single/batched reset and step, structured
  and compact observations, legal-button masks, exact Q16.16 rewards, and
  diagnostic flags.
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
| Observations and rewards are versioned, deterministic, and tested | Pass | TDR-0008 and C/Python conformance suites |
| Gymnasium wrapper passes its API tests | Pass | Gymnasium 1.3 CI job |

## Verification

GitHub Actions run
[`30308891149`](https://github.com/barrelofsulfuricacid-gif/ultra-performance-platform-fighter/actions/runs/30308891149)
passed all nine jobs:

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

## Unresolved issues

No critical verifier issue blocks M2. The agent-side browser connection was
unavailable during final visual QA; the hosted production build validated
successfully and the clean Chrome CI job passed, so owner-browser interaction
is the remaining visual gate.

## Owner checkpoint

In the browser checkpoint:

1. Confirm that the runtime reports WebGL 2 and replay pass.
2. Drag the timeline to ticks 0, 90, and 180 and confirm that player positions
   and the displayed state hash change.
3. Choose one option for each RL contract decision:

| Decision | A | B |
|---|---|---|
| Action vocabulary | Keep raw normalized analog actions | Replace with a discrete action vocabulary |
| Observation | Keep structured plus 36-word compact form, including seed | Request a different compact layout or hide seed |
| Reward | Keep sparse terminal-only `+1/-1` | Add shaped rewards now |
| Batch behavior | Keep fixed four-action stride and independent errors | Request variable stride or fail-fast behavior |

M3 must not begin until the owner accepts these four choices or requests
specific contract changes.

## Follow-up

After owner approval, M3 adds the persistent performance database, graphs,
regression comparison, and verifier qualification system.
