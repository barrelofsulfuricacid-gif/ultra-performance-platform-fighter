# Ultra-Performance 2D Platform Fighter

## Incremental implementation plan

**Plan basis:** `game(3).txt` plus fresh primary-source technical research performed on 2026-07-27. No previous game analysis is incorporated.

**Status:** Execution active in M4; owner decisions and subsequent binding plan modifications are recorded in `plan_modifications.md`.

---

## 1. Product outcome and non-negotiable constraints

Build an original 2D platform fighting game that:

- Has the speed, responsiveness, movement depth, defensive depth, combo freedom, and competitive feel associated with *Super Smash Bros. Melee*, without copying its protected names, characters, visual assets, audio, music, UI assets, or other expression.
- Supports 1v1, 2v2, and capture-the-flag play.
- Provides an original mechanical counterpart for every playable SSBM fighter/form, preserving substantially the same move functions and matchup identity while using original names, art, audio, lore, presentation, and independently authored implementation data.
- Includes ten original stages with distinct themes and deterministic hazards. The set must include winter, autumn, summer, desert, tropical forest, rocky mountains, and sea themes.
- Has richly animated, painterly, graphic-novel-like presentation with wholly original art direction and assets.
- Has SSBM-like menu flow and usability while using an original visual and audio identity.
- Runs natively on Windows, macOS, and Linux, and runs in current major web browsers.
- Supports peer-to-peer GGPO-style rollback netplay, ranked and unranked queues, Elo ratings, named rank tiers, and leaderboards.
- Supports Steam Input, GameCube controllers, Xbox controllers, PlayStation controllers, and other major controller families with simple remapping and calibration.
- Provides full music and sound coverage for menus, selection screens, stages, fighters, attacks, impacts, and other game events.
- Provides a compile-time headless build in which graphics, GUI, audio, music, controller UI, and other human-client systems are absent from the executable and tick path.
- Functions as an exceptionally fast deterministic reinforcement-learning environment.
- Keeps authored simulation, gameplay, client orchestration, tools, and public APIs in C. Unavoidable third-party C++ implementations are isolated behind narrow C ABIs, and online services use the best separately researched language.
- Makes single-thread simulation throughput the first performance priority. Human-facing clients may use auxiliary threads outside the deterministic simulation.
- Requires production code to be beautifully structured as well as ultra-fast: use the correct zero-cost abstractions, keep each mechanic and invariant under one canonical authority, and reduce code duplication to the minimum unavoidable at external boundaries.
- Stores game-design values in well-designed Excel workbooks. Developer builds import `.xlsx` at startup or reload; release, web, and headless builds consume a validated packed artifact generated from the same workbooks, with an explicit diagnostic runtime-import option.
- Minimizes hardcoded design data, duplicated logic, per-tick allocation, indirection, and unnecessary work.
- Uses SDL3 for native platform and graphics integration, and Dear ImGui through a C-facing boundary for the developer/debug GUI.

The plan is deliberately a living plan. Evidence found during implementation may change it, but every material change must be recorded in `plan_modifications.md`.

---

## 2. Resolved owner decisions

The following choices are binding requirements for implementation:

| Decision | Selected choice | Binding consequence |
|---|---|---|
| **D1-A — Mechanical counterparts** | One original fighter counterpart for every playable SSBM fighter/form. | Preserve substantially the same move functions and matchup identity while using original names, art, audio, lore, presentation, and independently authored implementation data. This fidelity level requires a formal IP/originality review before public release. |
| **D2-A — Authored game code in C** | Simulation, gameplay, client orchestration, tools, and public APIs are C. | Required third-party C++ implementations such as Dear ImGui or GGPO-derived code are isolated behind narrow C ABIs. Online services use the best separately researched language. |
| **D3-C — Relative improvement only** | Do not set an absolute ticks-per-second release target. | Establish reproducible baselines and require non-regression or measured improvement against compatible prior baselines. “Ultra-performant” remains an optimization direction, not an absolute product claim. |
| **D4-A — Full native/web cross-play** | Native and browser clients share one compatible player pool. | Every supported native↔native, browser↔browser, and native↔browser combination must interoperate when build and content hashes match. |
| **D5-A — P2P plus server replay verification** | Ranked gameplay remains P2P. | Signed match inputs/replays are deterministically re-simulated by headless servers before rating is finalized. |
| **D6-A — Runtime authoring import plus validated production pack** | Developer builds import `.xlsx` at startup or reload. | Release, web, and headless builds consume a validated packed artifact generated from the same workbook and retain an explicit diagnostic runtime-import option. |

Changing any selection above is a material plan modification and must be recorded in `plan_modifications.md`.

---

## 3. Architecture guardrails

These are constraints on implementation, not premature choices about data representation.

1. **One deterministic simulation authority.** Local play, netplay, replays, verification, and RL all call the same tick function and use the same state representation.
2. **Simulation is a platform-free C library.** It must not depend on SDL, rendering, audio, Dear ImGui, networking, wall-clock time, filesystems, or operating-system APIs.
3. **Single-thread simulation.** One match advances on one thread. Human clients may use other threads for rendering preparation, audio streaming, networking, asset loading, and services, provided those threads cannot mutate deterministic state.
4. **Headless is a separate build product.** Client-only code is removed at compile/link time, not merely bypassed by runtime `if` statements.
5. **Fixed tick contract.** Gameplay advances at a fixed 60 Hz logical rate. Headless mode runs those ticks as fast as possible without sleeping.
6. **Explicit state ownership.** Deterministic state uses fixed-capacity or preallocated storage, stable indices rather than owning pointers, and no allocation during a tick.
7. **Deterministic side-effect journal.** Simulation emits logical events. Rendering and audio consume only confirmed/reconciled events so rollback does not duplicate or lose effects.
8. **Data-oriented hot path.** State layout, access order, branch structure, and broad-phase representation are selected from measurements. General-purpose object or ECS abstractions do not enter the hot loop without winning a benchmark.
9. **No unmeasured numeric dogma.** Floating point, fixed point, fully quantized cells, a 256×256 logical world, bitboards, and hybrid representations are candidates. M0 selects the representation through experiments measuring throughput, determinism, state size, rollback cost, and game feel.
10. **Excel is authoritative design data, not a per-tick format.** Imported values are validated once and transformed into compact immutable runtime tables.
11. **Version every deterministic input.** Builds, design-data packs, stages, fighters, controller normalization rules, replays, save states, and network handshakes carry compatible version/content hashes.
12. **Original expression throughout.** SSBM is a system/feel reference, never an asset source. Every name, image, animation, sound, composition, story element, UI asset, stage layout, and written description must be original or properly licensed.
13. **Beautiful zero-cost implementation.** Express shared mechanics, formulas, state transitions, validation, serialization, and platform-independent policy once through cohesive C APIs, immutable data, and compile-time or `static inline` composition. An abstraction on a simulation hot path must add no allocation, ownership ambiguity, avoidable data movement, indirect dispatch, branch, or call overhead versus its direct equivalent in optimized builds. Where zero cost is not evident, inspect optimized code or measure it. Authoritative gameplay logic may not be copied among runtime, replay, RL, verifier, native, and web paths; unavoidable adapter or test duplication must be small, explicit, and kept outside the deterministic authority.
14. **Prior art before implementation.** Before starting every implementation slice, fidelity investigation, tool, experiment harness, or optimization, first search the repository, pinned upstream sources, existing project skills, and maintained public implementations for reusable evidence or machinery. Record the relevant result in the milestone evidence. Implement only after this sweep, and do not replace authoritative source data or an established routine with a guess.
15. **Massively fast equivalence validation.** Exactness does not excuse a slow edit loop. Run source/import checks, stored identical-input traces, deterministic simulation tests, replay checks, and affected-coverage selection without Dolphin in the ordinary edit loop. A live oracle uses one persistent headless/null/unlimited Dolphin session, checkpoint-isolated cases, coalesced or streamed memory observation, and one machine-readable coverage manifest. Do not relaunch Dolphin, serialize unrequested memory, or issue per-field cross-process reads when one batch can preserve the same evidence. Target at most 2 seconds after build for the no-Dolphin edit suite, 3 seconds for a warm changed-domain live oracle, and 10 seconds for the complete warm Falcon oracle pack; treat misses as active performance defects, not acceptable test overhead.

---

## 4. Researched starting technology baseline

This is a starting shortlist, not permission to adopt a dependency without an implementation-time comparison. Before first use, each dependency gets a short record in `docs/technology_decisions/` covering current maintenance, license, supported targets, alternatives, reproducibility, measured overhead, and reasons for selection.

| Area | Starting baseline | Adoption rule |
|---|---|---|
| Build | CMake workflow presets plus Ninja, with toolchains and dependency revisions pinned | Use one command per supported build; retain a lock manifest and checksums. [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) |
| Native platform | SDL3 for windows, events, gamepads, timing utilities outside simulation, and native graphics | Use SDL3’s GPU API for the native renderer if the M1 render spike confirms fit; it currently targets Vulkan, D3D12, and Metal. [SDL3](https://wiki.libsdl.org/SDL3/FrontPage), [SDL GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU) |
| Web | Emscripten/WebAssembly, SDL3-compatible platform glue, and a measured WebGL 2 baseline; evaluate WebGPU before locking the renderer | The simulation must compile from the same C sources. Browser-specific JavaScript remains a narrow adapter. [Emscripten](https://emscripten.org/docs/), [WebGL guidance](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html) |
| Debug GUI | Dear ImGui through a generated C API and a narrow adapter target | Dear ImGui is C++; do not leak C++ types or ownership into authored C code. [Dear ImGui bindings](https://github.com/ocornut/imgui/wiki/Bindings) |
| Rollback | GGPO behavioral model/API, adapted or ported behind the engine’s C interface | Upstream documents Windows-only builds, so portability must be proven through conformance tests rather than assumed. GGPO is MIT licensed. [GGPO repository](https://github.com/pond3r/ggpo), [GGPO model](https://www.ggpo.net/) |
| P2P transport | Compare a native WebRTC DataChannel path using libdatachannel’s C API with a smaller ICE/UDP path using libjuice; browser uses `RTCDataChannel` | Choose only after native↔web latency, unordered delivery, loss, NAT, TURN fallback, binary size, and C-boundary tests. [libdatachannel](https://github.com/paullouisageneau/libdatachannel), [libjuice](https://github.com/paullouisageneau/libjuice), [RTCDataChannel](https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel) |
| Controller input | SDL3 Gamepad as the non-Steam base, Steam Input action API on Steam, plus a dedicated GameCube-adapter normalization module | Expose action bindings, glyphs, calibration, and hot-plug behavior without changing simulation inputs. [Steam Input](https://partner.steamgames.com/doc/features/steam_controller), [supported Steam devices](https://partner.steamgames.com/doc/features/steam_controller/device) |
| Excel | XLSX I/O is the initial native C candidate; compare it against a deliberately small schema-specific importer | Import values only, validate strictly, and convert to packed runtime data. [XLSX I/O](https://github.com/brechtsanders/xlsxio) |
| Audio | Benchmark miniaudio against SDL_mixer 3 for latency, mixing, streaming, web support, binary size, and licensing | Keep audio outside deterministic state; select the best verified fit. [miniaudio](https://github.com/mackron/miniaudio) |
| Profiling | Tracy instrumentation plus native platform profilers: `perf` on Linux, Instruments on macOS, and ETW/WPA or an equivalent current Windows profiler | Profiling must be compile-time removable from final headless throughput builds. [Tracy](https://github.com/wolfpld/tracy) |
| Performance history | SQLite database under `performance/`, populated by the benchmark runner | Record commit, scenario, machine fingerprint, compiler/configuration, distribution summary, and environment metadata. [SQLite](https://sqlite.org/) |
| RL interoperability | Stable C ABI first; thin Gymnasium-compatible vector wrapper second; optional shared-memory/zero-copy adapters after measurement | Python convenience must not define or slow the core API. [Gymnasium vector environments](https://gymnasium.farama.org/api/vector/) |

No dependency is selected because it is fashionable or merely claims to be fast. Where two credible candidates remain, an isolated representative benchmark decides.

---

## 5. Milestone map

| Milestone | Incremental outcome | Mandatory human stop |
|---|---|---|
| M0 | Product contract, performance charter, and architecture experiments | Yes |
| M1 | Reproducible repository, builds, dependency boundaries, and workflow scaffolding | Yes |
| M2 | Deterministic headless simulation kernel and RL API | Yes |
| M3 | Per-commit performance system and verifier agent | Yes |
| M4 | Playable combat vertical slice with complete non-character-specific SSBM advanced-technique coverage | Yes |
| M5 | Excel-driven gameplay data and Dear ImGui debug editing | Yes |
| M6 | Complete local 1v1, 2v2, hazards framework, and capture the flag | Yes |
| M7 | Native/web client presentation, menus, audio, and controller support | Yes |
| M8 | Full original roster and ten-stage content set | After every roster/stage wave |
| M9 | Cross-platform GGPO rollback netplay | Yes |
| M10 | Ranked/unranked online services, Elo, ranks, and leaderboards | Yes |
| M11 | Release hardening across native, web, and headless products | Final acceptance |
| M12 | Continuous evidence-driven performance-improvement loop | At every gameplay-changing candidate |

Execution stops at each marked point until the owner playtests or explicitly waives that checkpoint.

---

## 6. Detailed milestones and acceptance criteria

The architecture guardrails are acceptance gates at every milestone, not a
later refactoring phase. A behaviorally correct or faster change is incomplete
if it duplicates an authoritative formula or transition, creates parallel
simulation policy, obscures state ownership or invariants, or introduces an
avoidable runtime abstraction cost. Review each material implementation for:

- one canonical owner for each mechanic, formula, state transition, and data conversion;
- reusable, cohesive C interfaces and data-driven composition instead of copied branches;
- zero per-tick allocation and no abstraction overhead in optimized hot paths;
- minimal boundary/test scaffolding duplication, with any unavoidable duplication documented; and
- readable names, explicit invariants, bounded responsibilities, and removal of superseded paths.

When performance and structure appear to conflict, compare equivalent correct
implementations with the canonical benchmark/profile workflow. Select the
fastest design that still preserves one clear authority and the smallest
reasonable duplication; do not accept speculative indirection or speculative
copying.

### M0 — Product contract and measured architecture decisions

#### M0.1 — Gameplay and originality contract

- Create a concise “Melee-feel” system matrix covering movement, aerial control, dash/dash-dance, jump squat, short hop, fast fall, crouch, platforms, shields, dodges, air dodge, recovery, ledges, attacks, grabs/throws, hitlag, hitstun, knockback, directional influence, teching, combos, stocks, blast zones, and team play.
- Define a human playtest rubric for responsiveness, control precision, expressive movement, combo freedom, defense, readability, and fun.
- Implement D1-A by creating a one-to-one roster-coverage matrix for every playable SSBM fighter/form without importing names, art, frame data, audio, lore, or assets.
- Define ten original stage briefs. Seven must use the required themes; three remain creative decisions.
- Create an originality checklist and an asset/license provenance register.

**Acceptance criteria**

- Every source-level gameplay/content requirement is represented in the system, roster, stage, or mode matrix.
- The owner approves the intended fidelity level and playtest rubric.
- No Nintendo or third-party game assets are present in the repository.
- Every planned character, stage, menu, sound, and music asset has an original-production or licensed-source path.

#### M0.2 — Performance charter

- Implement D3-C by defining canonical native and browser benchmark environments for comparison without setting absolute ticks-per-second pass/fail targets.
- Define benchmark scenarios and metrics: logical ticks/second, nanoseconds/tick, p50/p95/p99 time, state bytes, snapshot time, restore time, rollback re-simulation cost, instructions, branches, cache misses where available, and build size.
- Capture the first valid baseline for every canonical scenario and preserve compatibility metadata for later comparisons.
- Define measurement noise, minimum meaningful improvement, regression thresholds, warm-up, repetition count, CPU-affinity/power settings, and statistical comparison method.
- Define relative non-regression envelopes for 1v1, 2v2, worst-case hitbox load, worst-case hazards, rollback, and RL batches.

**Acceptance criteria**

- Every milestone reports compatible performance relative to an identified prior baseline; changed hardware, compiler, content, or scenarios cannot be silently compared.
- No absolute TPS threshold or machine-independent “ultra-performant” claim is used as a release gate.
- Single-thread headless simulation throughput is the primary optimization metric.
- Regressions beyond the defined noise/envelope require correction or an explicit documented owner exception.
- Performance changes cannot be accepted from a single unrepeatable timing result.

#### M0.3 — Representation experiments

Build disposable but comparable prototypes for:

- Floating-point, fixed-point, fully quantized, and hybrid position/velocity representations.
- A 256×256 logical grid and at least one higher-resolution alternative.
- Bitboard, spatial-bin/grid, sweep, and hybrid broad-phase collision candidates.
- Array-of-structures, structure-of-arrays, and compact hot/cold state separation.
- Branch-heavy and table-driven move/state dispatch.
- Full-state copy snapshots versus structured/delta snapshot candidates.

Use the same scripted traces and correctness checks for all candidates. Measure throughput, snapshot/rollback cost, determinism, state size, implementation risk, precision, and playtest feel.

**Acceptance criteria**

- The selected representation wins a documented Pareto decision; no choice is justified only by intuition.
- At least one human playtest compares the leading quantized and higher-precision candidates.
- Rejected candidates and the conditions under which they might be reconsidered are recorded.
- Bitwise techniques are used where they win representative measurements, not as a blanket style rule.

#### M0.4 — Architecture and dependency decision records

- Implement D2-A and establish the authored-C/third-party-C++ ABI boundaries.
- Re-research and compare the current candidates from Section 4.
- Record licenses, pinned versions/commits, supported targets, and replacement plans.
- Define the deterministic-state schema, simulation API, event journal, replay format, design-data format, and build-product boundaries at interface level.

**Acceptance criteria**

- A dependency cannot be introduced without a current decision record.
- The deterministic simulation has no client, platform, network, GUI, or audio dependency.
- C++ implementation details cannot cross a public C header.
- M0 results are committed as reproducible experiments and reports, not retained only as prose claims.

**Human checkpoint:** Choose the architecture candidate to carry forward after playing the representation prototypes.

---

### M1 — Reproducible foundation

#### M1.1 — Repository and build products

Create clean targets for:

- `sim`: deterministic C simulation library.
- `headless`: maximum-throughput executable/library with client systems excluded.
- `native_client`: SDL3 Windows/macOS/Linux client.
- `web_client`: Emscripten/WebAssembly client.
- `tools`: importers, validators, replay inspection, graphs, and content tools.
- `benchmarks`: isolated and end-to-end performance targets.
- `verifier`: automated test-player and visual/mechanical verification.

Establish directories for source, tests, design workbooks, generated data, original assets, performance, verifier issues, human feedback, optimization experiments, documentation, and release output.

**Acceptance criteria**

- The empty/minimal project configures, builds, tests, and runs on Windows, macOS, Linux, and the browser.
- Headless links no renderer, GUI, music, audio, or input-configuration code.
- The deterministic simulation target is single-threaded and contains no thread creation.
- Human-client auxiliary threading is behind explicit ownership boundaries.
- Compiler warnings are treated as errors in project code.

#### M1.2 — Reproducible setup

- Provide robust PowerShell and POSIX bootstrap scripts with the same conceptual commands.
- Pin compiler/toolchain, CMake/Ninja, Emscripten, dependency revisions, and checksums.
- Add CMake configure/build/test/workflow presets for debug, sanitizer, release, profile, benchmark, headless, and web configurations.
- Make scripts idempotent, validate prerequisites, print actionable errors, and verify the resulting environment with a smoke build.
- Add clean-machine CI jobs for every supported operating system and web.

**Acceptance criteria**

- A documented clean machine can build and run the native and headless smoke targets with one bootstrap command and one workflow command.
- A documented clean machine can produce and serve the browser smoke target without undocumented manual edits.
- Re-running setup is safe and yields the same dependency/tool versions.
- Dependency upgrades are explicit reviewed commits, never floating downloads.

#### M1.3 — Execution workflow scaffolding

Create:

- `plan_modifications.md`.
- `performance/` with database/graph/report locations.
- `verifier/issues/unfixed/` and `verifier/issues/fixed/`.
- `human_feedback/unfixed/` and `human_feedback/fixed/`.
- `optimizations/merged/`, `optimizations/pending/`, and `optimizations/discarded/`.
- Templates for issues, human feedback, optimization analyses, decision records, and milestone reports.
- A versioned commit workflow command that runs the required post-commit verifier and performance jobs.

**Acceptance criteria**

- Every required directory and template exists and is documented.
- A sample plan modification, verifier issue lifecycle, human-feedback lifecycle, and optimization analysis can be validated automatically.
- The workflow does not recursively create commits from its own generated benchmark data.

**Human checkpoint:** Confirm that setup succeeds on the owner’s development machine.

---

### M2 — Deterministic simulation kernel and RL surface

#### M2.1 — Minimal deterministic world

- Implement seeded reset, normalized per-player inputs, fixed 60 Hz tick, deterministic state transition, and terminal conditions for a minimal arena.
- Use preallocated state and scratch memory; prohibit heap allocation, I/O, locks, logging, and wall-clock reads inside a tick.
- Implement canonical save, load, clone, and state hashing.
- Add replay recording/playback based on initial seed, content hash, and input stream.
- Run the same replay across supported native builds and WebAssembly and compare per-tick hashes.

**Acceptance criteria**

- Identical compatible inputs produce identical hashes on every supported target and optimization level in the determinism matrix.
- Save/load followed by replay reaches exactly the same state.
- A tick performs zero dynamic allocations and no platform calls.
- State capacity supports four fighters and all deterministic stage/mode data even if the first scenario uses fewer.
- Every nondeterministic build or content mismatch fails loudly with useful diagnostics.

#### M2.2 — Headless and reinforcement-learning API

Expose a stable C ABI for:

- Library/version/content queries.
- Environment create/destroy and caller-supplied allocator or arena.
- Seeded reset.
- Single and batched step.
- Structured and compact observations.
- Legal-action masks where applicable.
- Rewards, termination, truncation, and diagnostic flags.
- State clone/save/load/hash.
- Deterministic replay import/export.

Add a thin Gymnasium-compatible vector wrapper without making Python part of the engine.

**Acceptance criteria**

- Headless mode initializes and steps environments without loading SDL, graphics, GUI, audio, music, controllers, fonts, or visual assets.
- The headless executable runs uncapped and reports measured ticks per second.
- One API can run 1v1, 2v2, and later mode configurations without separate simulation code.
- Batched stepping reduces boundary overhead relative to one call per environment.
- Observations and rewards are versioned, documented, deterministic, and tested.
- The Gymnasium wrapper passes its API tests while native C benchmarks remain the authoritative performance measure.

**Human checkpoint:** Inspect deterministic replays and approve the RL observation/action contract before it becomes expensive to change.

---

### M3 — Performance and verifier infrastructure

#### M3.1 — Comprehensive performance suite

Implement reusable benchmark scenarios for:

- Empty tick overhead.
- Representative 1v1.
- Representative 2v2.
- Maximum simultaneous hitboxes/hurtboxes/effects.
- Hazard-heavy stages.
- Snapshot, restore, and rollback re-simulation.
- Replay verification.
- Single-environment and batched RL stepping.
- Design-data import and client frame performance outside the core TPS metric.

After every commit, the workflow must:

1. Build the canonical benchmark configuration.
2. Warm up and run the required repetitions.
3. Store results in a local SQLite database under `performance/`.
4. Regenerate ticks-per-second evolution graphs keyed by commit.
5. Flag statistically meaningful regressions.

After every important milestone or large implementation change, capture a profile using Tracy and the appropriate platform profiler, then store the analysis under `performance/profiles/<milestone-or-change>/`.

**Acceptance criteria**

- Every commit has a benchmark record or an explicit machine-readable reason it could not be measured.
- Database rows include commit, dirty-state flag, scenario, seed, build configuration, compiler, dependency/content hashes, machine/OS fingerprint, thermal/power metadata where available, sample distribution, and summary statistics.
- Graphs show engine ticks per second over commits for every canonical scenario.
- The benchmark runner detects invalid comparisons caused by changed hardware, content, compiler, or scenario.
- All benchmark databases, graphs, profiles, and analyses live under `performance/`.
- Instrumentation can be completely removed from the final maximum-throughput headless build.
- Generated per-commit data remains local by default, avoiding an infinite “commit results, benchmark new commit” loop; explicit milestone snapshots may be committed.

#### M3.2 — Verifier agent

Implement a verifier that can:

- Read the specification/acceptance manifest and the commit diff.
- Control the game through the same action layer used by players/RL.
- Play deterministic scripted and exploratory matches.
- Inspect state, collision/hitbox overlays, replays, menu navigation, controller prompts, rendered frames, and screenshots.
- Compare mechanics against invariant/oracle tests.
- Compare GUI states against approved visual references using tolerant image and semantic checks.
- Run relevant smoke, determinism, sanitizer, replay, and benchmark-regression checks after every commit.
- Write one Markdown report per discovered issue to `verifier/issues/unfixed/`.

When fixed, the issue is moved to `verifier/issues/fixed/` and records the fix commit. Because a commit cannot contain its own final hash, the fix is implemented first and a following bookkeeping commit moves/annotates the report with the prior fix hash.

**Acceptance criteria**

- The verifier runs after every commit and produces a pass manifest even when it finds no issue.
- Deliberately seeded mechanical, visual, menu, and determinism defects are found in a verifier qualification test.
- Reports include reproduction steps, expected/observed behavior, evidence, severity, affected build/content hash, and the detecting commit.
- Unfixed critical issues block milestone completion and merge to `master`.
- Fixed reports preserve all original evidence and identify the actual fix commit.

**Human checkpoint:** Review benchmark stability, graphs, and verifier qualification results.

---

### M4 — Combat vertical slice

#### M4.1 — Movement and stage interaction

Implement one original placeholder fighter and one original test stage with:

- Ground and air movement, traction, acceleration, facing, dash, run, dash-dance, crouch, jump squat, full hop, short hop, double jump where configured, fast fall, aerial drift, landing, drop-through platforms, and blast zones.
- Deterministic collision with floors, pass-through platforms, walls/ceilings where the stage requires them, ledges, and platform motion.
- Data-driven movement parameters and state transitions.

SSBM executable-oracle equivalence is a binding M4 gate. Every implemented M4
movement or shared-simulation behavior with an SSBM counterpart must match the
owner's `GALE01` NTSC 1.02 image in Dolphin, not merely resemble it or pass tests
derived from this implementation. The only exclusions are explicitly original
mechanics with no intended SSBM counterpart. Work on fidelity continues until
the complete applicable identical-input differential corpus agrees; unresolved
divergences are not deferrable acceptance notes, and M4 work does not stop while
one remains. Numeric frame-data tables may be imported under the recorded
fidelity exception, while protected audiovisual, stage,
character-expression, and executable assets remain outside the project.
Before implementing combat content with a Captain Falcon counterpart, import
and validate the complete version-pinned numeric table from the owner's NTSC
1.02 data. Do not guess routine move timing, damage, angle, knockback growth,
set weight, base knockback, shield damage, interaction flags, or landing data
when that authoritative table contains the value. Identical-input Dolphin
captures verify the imported table and runtime behavior and remain the oracle
for dynamic semantics that a static frame-data table cannot express.

The verifier must drive Dolphin and this simulation with the same ordered,
per-frame controller samples and compare at least action/state transitions,
action frame, facing, grounded state, position, self-induced velocity, and
relevant timers. Its corpus must cover neutral and nonzero starting momentum,
both directions, analog threshold boundaries, slow walk entry, direct and
two-sample dash entry, repeated dash-dance reversals, fox-trot, run, run brake,
standing turn, run turnaround, and crouch entry/hold/release including its
distinct start/reverse timings and analog threshold boundaries. Captures must
identify the Dolphin build, disc revision/hash, fighter, stage/setup, input
trace, coordinate conversion, and first divergent frame. Internal deterministic
tests remain required, but cannot substitute for this executable-oracle
comparison.

The executable oracle is one persistent scenario pack, not one state-leaking
continuous match. Each short case declares its starting checkpoint, ordered
inputs, observed fields, source rows/callback branches, and exact or bounded
comparison policy; the runner restores the checkpoint between cases and emits
one aggregate artifact. Its coverage manifest must account for every imported
table row, action-frame pose, transition, callback branch, and physical
positive/negative boundary claimed by production. The fast at-will suite
replays stored authoritative inputs/traces without launching Dolphin and
selects changed cases from that same manifest. A warm live run requalifies the
affected domain, while the complete pack periodically requalifies the whole
manifest. No finite scenario proves the absence of every possible anomaly, so
coverage is explicit and extensible rather than described as universal.
Source-complete data and action-owned geometry are captured once and verified
exhaustively by canonical action/frame payload. They must not be re-simulated
through a long physical route for every row when the same pinned collision
routine can evaluate those exact capsules offline. Live Dolphin cases are
reserved for the smallest positive/negative discriminators that prove dynamic
integration, callbacks, and phase ordering. Benchmark timing and incidental
idle phase stay outside the authoritative digest; two warm regenerations must
produce the same canonical payload.

Exact equivalence here is behavioral rather than a demand that Q16.16 fixed
point reproduce every least-significant bit of Dolphin's single-precision
positions. Small numeric differences are accepted only when they are bounded,
recorded by the verifier, and attributable to Q16.16 representation or
accumulation. State/action transitions, action timing, facing, grounded state,
input thresholds, and other discrete outcomes remain strict. A collision
boundary may tolerate at most the corresponding one-tick fixed-point
quantization transient; tolerances must not hide cumulative drift or a
materially different route.

Current regression evidence consists of an 8,675-frame Final Destination
movement/defense/crouch corpus, a separate 348-frame Battlefield platform
corpus, and a separate 540-frame Final Destination Falcon-versus-Falcon
grounded-player-push corpus, plus a separate 500-frame Final Destination
analog-shield-pressure corpus. A focused 329-frame defense-state route covers
forward roll, spot dodge, backward roll, held-L/fresh-R upward air dodge
through ordinary-physics handoff and floor landing, then a down-left air dodge
that enters horizontal LandingFallSpecial above Falcon's walk-speed threshold
and crosses into the ordinary-friction branch. It compares action, tick,
grounded state, facing, invulnerability, and velocity exactly. The shield route
covers sub-threshold, light,
intermediate, near-dense, both-shoulder, digital-full, release, and regeneration
samples while comparing action/state, health, and normalized pressure; its
normalized-pressure allowance is one 16-bit unit. The player-push route
compares both players'
actions, action frames, facing, grounded state, positions, and self-induced
velocities in both approach directions and from both controller ports. It pins
Falcon's 3.5-unit push radius, the common 0.3-unit nudge, strict overlap
boundary, and a bounded one-nudge positional allowance for a Q16.16-delayed
boundary crossing. The Battlefield
route includes ordinary jump-through and landing,
a one-frame-down negative control, held-down `Squat` frames 1-3, `Pass` entry
at the executable's 0.63 downward speed, and same-frame solid-floor landing.
Three additional 283-frame Final Destination shield-hit captures request
light, intermediate, and dense pressure. They compare both fighters' strict
discrete state, self velocity, shield health/pressure, hitlag and shield stun,
plus position within the established 640-Q16 envelope and the attacker's
separate recoil within 32 Q16 units, inferred independently from executable
position delta minus self velocity. These routes qualify integer shield-hit
conversion, pressure-dependent damage/stun and defender pushback, same-frame
post-hitlag ordering, and separate ground-decaying attacker recoil. Separate
270- and 2,158-frame memory-probed shield routes qualify the half-step wrapped
angle/magnitude smoothing, all eight linear guard-animation keys, Falcon's
joint-derived center and radius, facing reflection, health/pressure scaling,
and the anisotropically mapped elliptical collision volume. A 2,568-frame,
33-decision Jab 1 sweep additionally qualifies exact sphere-versus-shield
collision at neutral and two diagonal guard offsets, including all three
last-hit/first-miss boundaries. Aggregate executable-oracle evidence is
therefore 18,697 qualified frames, including 350 actionable frames from a
1,250-frame aerial-IASA capture covering one-frame-early/exact fair, back-air,
up-air, and down-air double-jump interrupts plus neutral-air's no-IASA control,
and
116-frame grounded and 92-frame aerial Falcon Dive catch/throw routes plus
103-frame grounded, 165-frame aerial miss, and 63-frame aerial ledge-approach
routes with memory-probed ECB,
internal damage, knockback, and reaction-timer state,
46-frame Raptor Boost ground-hit, 80-frame ground-miss, 180-frame aerial-miss,
145-frame aerial-hit-to-floor, 51-frame ground-edge, and 155-frame native
Capsule item-search routes,
and a 77-frame Falcon Kick ground-hit route with memory-probed parallel ground
and self velocities, plus 181 live normal-throw frames covering all four
release routes, three ordinary hitbox intervals, captured-victim damage and
hitlag, and zero release hitlag.
Uncaptured pressure/time/spacing routes and the broader shared-simulation
inventory remain active work.

Falcon's complete attack-oriented source is imported as a hash-pinned 50-slot
schema with 48 concrete subactions; the only absent rows are the two angled
forward-smash variants that Falcon's NTSC 1.02 DAT does not define. It includes
ordinary attacks, grabs/throws, all five aerials, and the contiguous 17
character-special subactions. This is complemented by a complete 318-slot
`PlCa.dat` submotion catalog: all 275 present FigaTree animations and all 43
source-defined empty slots, with frame endpoints, gameplay last frames,
action-script event counts/offsets, animation flags, and source byte sizes. The
import must retain all 2,056 event boundaries and 16,516 raw script bytes, and
must exhaustively decode/hash all 17,271 animation nodes, 38,560 tracks, and
308,057 keys. It must also derive one shared O(1) translation pool for all 65
translation-bearing submotions and their 2,536 X/Y frame samples, using the
six-bit translation-node field rather than a low-byte approximation. Runtime
code may keep animation tracks offline until a behavior consumes them, but it
may not replace a source track or command with an authored approximation or
generate a second per-action copy of the same translation samples. Default
dash/turn/brake/landing/crouch/shield-release/dodge/roll/tech/getup/appeal
timing must consume this catalog rather than repeat literals. The same
generated source preserves all 97 raw
common-attribute words and the complete 0x8c-byte, 35-field Falcon special-
attribute block, plus Falcon's complete `ftData_x44` collision/ledge-snap
block. A typed zero-cost view supplies the default runtime's mapped
movement, jump, fall, weight, and landing values directly; it may not be
replaced by hand-entered approximations. No implemented Falcon-counterpart move
may use a guessed timing, effect, or character attribute when this source
contains it.

Roll and air-dodge semantics consume that source through decomp-qualified
callbacks. Ground rolls replace their ground-velocity channel from the one
generated TransN stream; they must not add an independent authored roll curve.
EscapeAir consumes common force/dead-zone/decay attributes from `PlCo.dat`,
applies decay on the entry frame, and decodes the raw frame-30 variable-0 write
that changes its physics callback to ordinary aerial input/gravity without
changing action. Its floor contact consumes the captured 48-frame animated ECB
bottom. Body-state command windows map through the state-specific displayed-
frame bias and drive invulnerability rather than duplicated constants.

Future character ports must reuse the installed `ssbm-character-importer`
workflow and generic source-manifest routine. Each port must maintain separate
source-available, losslessly-imported, production-consumed, and Dolphin-
qualified coverage; complete files or tables alone are never an equivalence
claim. Shared hashing, event validation, fixed-point conversion, span access,
and Dolphin route construction must be generalized rather than copied into a
new character-specific implementation.

Hash-pinned Dolphin captures provide transformed hit geometry and complete-
frame 11-capsule hurt poses for the 14 production normals/aerials, standing and
dash grab, all 17 Falcon special subactions, and the complete 15-frame Initial
Dash, 28-frame RunBrake, 7-frame CrouchStart, and 10-frame CrouchEnd common
tracks, all four KneeBend frames, all 32 SpotDodge frames, and both complete
31-frame roll tracks, all 49 action-owned AirDodge frames, the complete
eight-frame looping FallSpecial motion, and LandingFallSpecial's exact
10-tick source-frame sequence `1,4,...,28`. Every damaging/grabbing
special phase is represented, while non-damaging Raptor Boost search volumes
are imported separately as the source's six search spheres rather than being
misclassified as attacks. The imported special timing, attributes, and
geometry are not by themselves an equivalence claim. Default reference content
now routes neutral special through the source Falcon Punch ground/air state
machine, side special through the source Raptor Boost ground/air start,
search-hit, miss, landing, and hit states, up special through Falcon Dive
ground/air start, catch, and throw, and down special through Falcon Kick's
ground/air start, ground end, air end, landing-hit, edge-fall, and wall-rebound
states. The original Pulse Bolt, Prism Burst, Vector Ascent, and Arc Reservoir
are explicit custom-content opt-outs. An at-will 657-frame Dolphin Raptor
Boost suite covers the 46-frame ground-hit, 80-frame ground-
miss, 180-frame aerial-miss, 145-frame aerial-hit-to-floor, and 51-frame
ground-edge routes plus a 155-frame native Capsule search route. The item
route forces only Melee's item-rule accessors to Very High and runtime kind 0,
then uses the native ambient spawner, grounded Capsule, Falcon search callback,
and hit-state transition. Its isolated opponent remains at least 100 Melee units
away, and the verifier asserts the first live command-variable gate. The
project's Relay Rod is not a Falcon item-search target because the source
predicate accepts container kinds 0 through 5, two enemy ranges, and the
random Pokemon kind rather than ordinary weapon items. The
aerial-hit route includes search conversion, the imported frame-3 seven-damage
hit, five-frame hitlag, the complete natural pre-landing recovery tail, the exact air-to-ground
transition, 40 ticks of hit landing lag, and return to standing. It consumes
all 45 memory-probed `SpecialAirS` ECB-bottom frames; the former generic body
extent landed one tick early. The suite strictly matches action transitions
and velocities and matches position within the bounded 640-Q16 representation
allowance.
The ground-edge route matches the decomp's command-variable gate, source root
motion through floor loss, air-speed clamp, zero-gravity transition row, and
common `FallSpecial` continuation.
Both miss routes consume the imported common `FallSpecial` pose cycle; the
aerial transition applies ordinary common gravity rather than a move-specific
approximation.
A separate 116-frame grounded Falcon Dive capture strictly matches catch,
hitlag, captured-target attachment, throw release, source relocation/root
motion, fall, and floor landing within the same representation allowance.
The 92-frame aerial catch/throw differential additionally qualifies the
victim path: 5% catch, the imported nine-slot stale table's 0.91 first-slot
multiplier, 15.92% post-throw internal damage, zero launch velocity, ordinary
gravity, and the source-visible 26-frame damage reaction. Forty-two comparable
victim frames are strict; the final three victim samples are excluded because
the legitimate native jump fixture reaches its floor while the isolated
Dolphin capture remains held at y=500. The static 26-frame boundary remains
hash-pinned and asserted from that capture/decomp path.
The 103-frame grounded and 165-frame aerial miss differentials qualify both
`FallSpecial` transitions. Production consumes the executable's distinct
eight-frame ECB-bottom cycle rather than substituting the ordinary `Falling`
pose; the grounded route additionally preserves the executable's incoming
vertical velocity on the `LandingFallSpecial` transition row. No common-state
collision value is inferred from the special-move timing table.
The 63-frame aerial ledge approach strictly matches Falcon Dive action,
facing, position, and both velocity axes. Its hash-pinned live-memory capture
then proves the native frame-64 `EdgeCatch` transition and frame-71 `EdgeHang`
against the decomp's descending ledge probe. Production consumes the DAT's
exact 9/17/11 ledge-snap block and all 64 observed Falcon Dive ECB right/bottom
samples; it does not substitute generic body width or hand-tuned reach.
A 399-frame Falcon Kick differential suite strictly matches the imported
ground start/end, air start/end, air-to-ground landing, ground-to-air edge, and
ground-hit and wall-rebound routes, including root translation, velocity,
decoded traction/air-physics command boundaries, and the source collision conversion's half
crossing-tick displacement within that same allowance. The ground-hit route
also qualifies eight ticks of hitlag, 15 damage, the imported 0.6 on-hit speed
multiplier, and Melee's parallel ground/self velocity update through the ground
end state. The 58-frame Hyrule wall route additionally qualifies displayed-
frame-22 wall-hug detection, action 363, preserved entry self velocity, and the
complete rebound root trajectory. Its speed cap, rebound graph, and all hit
geometry consume the imported tables; no Falcon Kick dynamic state remains
without identical-input qualification.
Hurt capsules and action-command/callback semantics for common actions beyond
Initial Dash, RunBrake, CrouchStart, CrouchEnd, KneeBend, SpotDodge,
RollForward, RollBackward, AirDodge, FallSpecial, LandingFallSpecial, and
ordinary Landing remain
explicit
M4 gaps and must be extracted or qualified rather than approximated with
invented frame data. Common poses use
one compact generated state index and reuse the action-pose capsule pool; public
action values are mapped once and no snapshot state or allocation is added. A
pinned 31.0-unit hit/31.5-unit miss route proves the Dash track against Dolphin
and rejects the old generic rectangle. A second 17.7-unit hit/17.84-unit miss
route proves CrouchStart frame 3 and rejects the rectangle's false positive.
The four-frame KneeBend track is likewise imported; a 16.5-unit hit/16.8-unit
miss route proves frame 2 and rejects the rectangle's false positive.
FallSpecial resolves through common submotion 26 rather than the similarly
named character animation and loops all eight executable poses. Its frame-5
Jab 1 discriminator hits at 15.5 units and misses at 16.2, where the generic
rectangle falsely hits the miss. LandingFallSpecial resolves through common
submotion 36 and plays displayed source frames `1,4,...,28` over the ten-tick
lag; its source-frame-7 discriminator hits at 18.5 and misses at 19.3, where
the generic rectangle falsely misses the hit. Runtime lookup uses one
action-specific tick-to-source-frame adapter so movements that already enter
at source frame 1 are not shifted, while zero-based KneeBend/dodge/roll/
FallSpecial/LandingFallSpecial/Landing ticks are converted without duplicate
tables. Ordinary Landing resolves to common submotion 15 and retains all 30
displayed poses when no interrupt is supplied even though its input gate opens
after Falcon's four-frame landing lag. A pending source-frame-22 Jab 1 control
hits at 20.3 units and misses at 20.6, while the generic rectangle misses both.
LandingFallSpecial physics follows `ftCo_Landing_Phys -> ft_80084F3C`: entry
copies horizontal self velocity to ground velocity, each flat-stage tick moves
by the post-friction velocity, and friction is Falcon's ground friction times
common multiplier `x6C` only while absolute speed exceeds walk maximum. The
imported submotion has no TransN stream, so production adds no animation or
authored root displacement. The 329-frame Dolphin route crosses both friction
regimes and passes within only the established 640-Q16 position envelope.
The complete 32-frame SpotDodge track is imported from a pinned active,
non-hitlag executable trace. Its source body state is vulnerable on frames 1-2,
invulnerable on frames 3-20, and vulnerable on frames 21-32. A facing-controlled
Jab 1 route hits the pending frame-24 pose at 21.0 Melee units and misses at
22.0; the old generic rectangle falsely misses the positive route. The
post-frame observer's displayed-pose/collision-report ordering and damage-facing
reset are explicit verifier inputs rather than hidden distance adjustments.
The two distinct 31-frame roll-pose tracks reuse the same compact table and
deduplicated capsule pool. Pinned Jab 1 controls hit RollForward frame 22 at
12.98 Melee units and miss at 14.18, where the generic rectangle falsely hits;
they hit RollBackward frame 24 at 20.00 and miss at 20.75, where the generic
rectangle falsely misses the positive route. Each control first pre-places
both ports safely, settles, establishes explicit facing through controller
input, fully recovers, and only then applies its final placement, so route
order cannot leak airborne state, facing, or velocity into the result.
The executable's previous-to-current
moving hit-capsule sweep is production-routed and Dolphin-qualified: imported
collision state distinguishes creation from continuation, continuation finds
the prior same-ID sphere without enlarging rollback state, and one shared
zero-allocation 3D capsule-to-capsule predicate serves attacks, grabs, and
shields. Imported attack spheres and both hurt-capsule endpoints retain source
X/Y/Z; source Z may not be dropped by collision or inspection.
The complete submotion catalog closes data availability, not those behavior
routes. Imported hit and hurt geometry is rooted
at Melee's fighter origin, and reference hit spheres use the decomp's exact
radius-sum shield predicate in the uniform source spatial metric; authored
rectangles retain their separate ellipse collision path.
These captures qualify only their sampled routes and do not reduce the
exhaustive obligation below.

The first manifest-selected no-Dolphin regression domain covers those twelve
Falcon common-hurt tracks. One character-independent registry, generator,
affected-file selector, C runner, and replay gate validate all 255
production-accessed poses plus 20 manifest-owned hit/miss cases; Falcon owns
only its action bindings, coverage rows, and thin production adapters. Five
warm post-build runs take 116.845-120.355 ms on native Windows MSVC Release and
148.121-166.786 ms on WSL GCC 13.3 Release, meeting the two-second target. This
does not satisfy the separate warm changed-domain live target: the current
checkpoint pack is 4.01-4.90 seconds and remains an active optimization defect
against the three-second requirement. Additional behavior domains must join
the same registry as their live evidence becomes source-complete.

This gate is not limited to locomotion. It also covers every implemented shared
simulation path for which SSBM supplies the intended behavior, including
shield and light-shield input/health/size, roll, spot dodge, air dodge, jump
and landing transitions, ledge and collision interaction, hitlag, hitstun,
knockback, DI/SDI, teching, stale moves, stocks, respawn, and match-state
transitions. Each applicable route must gain an identical-input Dolphin
reproducer and comparable-state assertions before it may be called equivalent.
Project-specific infrastructure and explicitly original content mechanics have
no SSBM-equivalence claim, but they do not weaken the equivalence requirement
for the shared engine behavior they use.

Coverage of this gate is exhaustive over the applicable M4 behavior surface;
it is not limited to owner-reported bugs, the routes in the current corpus, or
behavior already named in this plan. Passing a finite differential capture is
only evidence for the sampled routes. Decomp review and systematic route
inventory must continue to discover and qualify every other applicable state,
transition, threshold, timer, and momentum condition before M4 can pass.

#### M4.2 — Combat system

Implement:

- Ground, aerial, special, and recovery actions.
- Hitboxes, hurtboxes, grabs, throws, shields, shield stun/damage, rolls, spot dodge, air dodge, and ledge actions.
- Damage, hitlag, hitstun, knockback, launch angle, directional influence, smash directional influence if retained by the M0 contract, stale-move behavior if retained, teching, knockdown, stocks, respawn, invulnerability, and match end.
- Rollback-safe combat events and replay visualization.
- A verifier-readable frame/state/hitbox inspector.

#### M4.3 — First playable loop

- Provide local 1v1 from match setup through results and rematch using temporary presentation.
- Tune through the approved Excel-data path precursor even before the full workbook UI lands.
- Run repeated human and verifier matches.

#### M4.4 — Complete SSBM advanced-technique compatibility

Treat the `SSBM = Yes` rows in SmashWiki's
[List of advanced techniques, revision 2048934](https://www.ssbwiki.com/index.php?title=Advanced_technique&oldid=2048934#List_of_advanced_techniques),
captured on 2026-07-28, as a binding gameplay requirement. Every unique technique
in that baseline must be supported and functional before M4 can be accepted. The source table lists
dash-dancing twice; this plan counts it once, yielding 61 unique required
techniques:

1. Approach
2. Auto-canceling
3. Bat dropping
4. Boost grab
5. Camping
6. Chain grab
7. Charge storage canceling
8. Cross-up
9. Dash cancel
10. Dash-dancing
11. Dashing shield
12. Double jump cancel
13. Double jump cancel counter
14. Drop cancel
15. Edge dashing
16. Edge hopping
17. Fox-trotting
18. Glide toss
19. Gimp
20. Infinite
21. Instant double jump
22. Jab cancel
23. Jab reset
24. Juggling
25. Jump cancel throw
26. Jump-canceled grab
27. Jump-cancelling
28. Kill confirm
29. L-cancelling
30. Ladder
31. Ledge-cancelling
32. Mindgame
33. Moonwalk
34. Powershield
35. Pivoting
36. Planking
37. Power shield canceling
38. Scar Jump
39. Sharking
40. Shield break combo
41. Shield platform dropping
42. Shield-stop
43. Shine spike
44. Short hop laser
45. Short hop air dodge
46. Short hop fast fall l-cancel
47. Small step forward smash
48. Smash directional influence
49. Spacing
50. Stage humping
51. Stage spike
52. Stalling
53. Taunt cancelling
54. Team wobble
55. Teching
56. Tech-chasing
57. Teeter cancel
58. Turtling
59. V-cancelling
60. Wavedash
61. Zero-to-death combo

This baseline refers specifically to the linked cross-game table, not the page's
separate character-specific section. D1-A mechanical-counterpart coverage may
independently require character-specific techniques.

For this requirement, “supported and functional” means:

- A player can perform the technique during an ordinary match with the relevant
  fighter, stage, item, or team configuration. Debug-only commands and scripted
  state injection do not count.
- The underlying input windows, state transitions, momentum/collision rules, and
  combat outcomes are deterministic in native, browser, and headless builds and
  survive save/load, replay, rewind, and rollback.
- Each mechanical technique has positive and negative invariant tests plus a
  verifier-readable execution trace. Each technique also has a concise human
  test recipe that can be executed in the browser.
- Tactical or emergent entries such as approach, camping, gimp, juggling,
  mindgame, spacing, stalling, and zero-to-death are not hardcoded outcomes.
  They count only when at least one legal, repeatable match sequence demonstrates
  the tactic and the constituent mechanics are independently verified.
- Original fighters, items, animation, audio, and presentation must express the
  behavior without copying protected SSBM content.

M4 is the completion gate for this entire 61-row non-character-specific
baseline:

- No row may be deferred to M5, M6, M8, or release hardening. If a technique
  needs an item, team interaction, projectile, charge state, reflector-like
  action, or other capability beyond the ordinary 1v1 slice, M4 supplies the
  narrow original fixture and live browser configuration needed to perform and
  verify it.
- M4 maintains a versioned row-by-row registry with dependencies, supporting
  configuration, implementation state, automated evidence, and browser
  playtest recipe. M4 acceptance requires every row to be `verified`.
- M5 may move the already-working timings into authoritative workbooks, and
  later milestones may broaden content and presentation, but neither may
  change or remove the accepted behavior without rerunning the M4 oracles and
  owner playtests.
- M11 reruns the complete registry as a regression gate. Recheck the live source
  table before M4 acceptance, at every M8 wave, and before M11; add any newly
  marked SSBM technique to this registry before accepting the current milestone
  unless the owner explicitly changes scope.

#### M4.5 — Technique-support fixtures

Implement the smallest original, production-path fixtures required to exercise
all M4.4 techniques:

- Data-driven pickup, carry, drop, aerial drop, directional throw, item hitbox,
  despawn/reset, and momentum-transfer rules, plus one original bat-like test
  item.
- A narrow multi-fighter/team laboratory configuration for team wobble and any
  other multi-fighter verification. Full local team setup and mode UX remain in
  M6.
- Placeholder-fighter actions needed by the general registry, including charge,
  projectile, reflector-like, shield, grab/throw, aerial, and ledge
  interactions. These fixtures use the same action/combat paths later content
  uses and are not debug-only shortcuts.

**Acceptance criteria**

- Bat dropping, glide toss, jump cancel throw, team wobble, shine spike, and
  short hop laser can be performed in an ordinary browser match/laboratory
  configuration and have positive/negative deterministic verifier oracles.
- Fixture state serializes, hashes, rewinds, replays, rolls back, and remains
  available to the headless/RL API.
- Fixtures stay inside the M0 performance and state-size budgets and introduce
  no copied names, art, audio, animation, or data.

**Acceptance criteria**

- A complete local 1v1 match can be played from start to result with two supported inputs.
- The selected M0 “Melee-feel” mechanics for the vertical slice are implemented and have invariant tests.
- The pinned identical-input Dolphin differential corpus has no unresolved
  divergence for any implemented movement or shared-simulation behavior with
  an SSBM counterpart. A newly observed owner-playtest divergence adds a
  reproducer to this corpus and blocks M4 acceptance until resolved.
- Collision, knockback, hitlag, hitstun, DI, stocks, ledges, and recovery are deterministic and replayable.
- Performance remains within the M0 1v1 budget with profiling evidence.
- The owner rates control responsiveness and core combat acceptable on the M0 playtest rubric.
- The advanced-technique registry records one of `planned`, `primitive-ready`,
  `playable`, or `verified` for every required row, with dependencies, target
  milestone, automated evidence, and a browser playtest recipe; no row may be
  silently omitted.
- All 61 non-character-specific rows are `verified`; any lesser state blocks M4
  acceptance and the transition to M5.

**Human checkpoint:** Mandatory combat playtest. Do not scale content until the core feel is approved.

---

### M5 — Excel data and in-game debug tooling

#### M5.1 — Authoritative design workbooks

Create documented, visually clear workbook schemas for:

- Global rules and mode rules.
- Fighter physics and state parameters.
- Moves, phases, cancel windows, hitboxes, hurtboxes, movement curves, and event cues.
- Stage geometry, spawn points, blast zones, platforms, and hazards.
- Audio/music event mappings.
- UI/menu values and controller defaults where appropriate.

Implement D6-A: developer builds import workbooks at startup or reload, while release, web, and headless builds load a validated packed artifact generated from those workbooks and retain an explicit diagnostic runtime-import mode. Validate types/ranges/references and transform imported data into versioned compact tables. Generate shared schema identifiers so code, importer, verifier, and debug GUI do not duplicate field definitions.

**Acceptance criteria**

- All tunable game-design values are in workbooks unless a documented technical invariant requires code.
- Invalid, missing, duplicate, cyclic, or out-of-range data fails with workbook/sheet/row/column diagnostics.
- Runtime tables are immutable, compact, versioned, and hashed.
- Release/headless data has identical semantics to the authoritative workbook.
- Import and lookup add no parsing or string-search cost to the tick loop.
- The schema and generated code measurably reduce hardcoded values and duplicated field logic.

#### M5.2 — Dear ImGui debug GUI

Provide panels for:

- Live state, frame advance, pause, rewind, replay, hitboxes/hurtboxes, collision, input history, RNG/seed, state hash, and performance counters.
- Searching and overriding imported design values.
- Showing each value’s workbook provenance, valid range, current override, and default.
- Reloading the workbook/data pack, clearing overrides, comparing changes, and exporting a proposed change set.
- Marking overridden sessions as non-ranked and embedding override hashes in replays.

**Acceptance criteria**

- A developer can find and override any supported workbook design value without recompiling.
- An override takes effect only at a documented safe boundary and cannot corrupt a rollback/state snapshot.
- Reload/reset/export behavior is deterministic and covered by tests.
- Debug GUI code and C++ types are absent from headless and authored C public interfaces.
- Debug tooling itself can be disabled without changing simulation results.

**Human checkpoint:** Tune the vertical slice through Excel and ImGui and approve the workflow.

---

### M6 — Local modes, teams, hazards, and capture the flag

#### M6.1 — Complete local player/team model

- Generalize the simulation to one through four fighters.
- Implement 1v1 and 2v2 team setup, team spawn logic, stock/score rules, ally identification, optional friendly-fire rule, results, and rematch.
- Support multiple controllers on one machine and the network ownership model needed later.

**Acceptance criteria**

- 1v1 and 2v2 use the same combat systems and deterministic tick.
- Four-fighter worst-case state, collision, combat, replay, save/load, and RL stepping pass determinism and performance budgets.
- Team rules and result calculation are data-driven and verifier-tested.

#### M6.2 — Hazard framework

- Implement deterministic, data-driven hazard activation, movement, collision, damage, spawning, and reset.
- Separate visual-only effects from gameplay-affecting hazard state.
- Support a hazard-disabled competitive variant when a stage’s hazard changes gameplay.

**Acceptance criteria**

- Hazards serialize, hash, rewind, replay, and rollback correctly.
- Hazard scripts cannot allocate, access wall-clock time, or mutate state outside declared deterministic data.
- Worst-case hazard scenarios remain within the M0 budget.

#### M6.3 — Capture the flag

- Implement flags, bases, pickup, carry, drop, return, capture, scoring, respawn, overtime/tie handling, and HUD events.
- Support configurations suitable for 1v1 and 2v2.

**Acceptance criteria**

- A complete capture-the-flag match can be played locally and through the RL API.
- All flag transitions and scoring rules are deterministic, replayable, data-driven, and verifier-tested.
- CTF does not fork or duplicate the core movement/combat implementation.

**Human checkpoint:** Mandatory 2v2 and capture-the-flag playtest.

---

### M7 — Human-facing client

#### M7.1 — Native and web rendering

- Implement an SDL3 native renderer selected by the M1 spike, with batched 2D draw submission, texture atlases, precompiled shaders, animation blending/state, particles, camera, lighting/color treatment, and HUD.
- Implement the browser renderer from the same presentation data through Emscripten, with a measured WebGL 2 baseline and WebGPU only if the adoption spike wins.
- Build an original art-direction bible capturing painterly, high-contrast, graphic-novel qualities without reproducing *Hades* characters, assets, compositions, UI, or distinctive expression.
- Ensure rendering interpolates presentation without changing deterministic state.

**Acceptance criteria**

- Native clients run on Windows, macOS, and Linux; the web client runs in approved current browser versions.
- The same replay produces semantically identical action, camera, and UI sequences on every client.
- Graphics can be disabled without changing any state hash.
- Native rendering uses SDL3 as required.
- Art assets and animations pass the originality/provenance checklist.
- Client frame-time budgets are met independently of headless simulation TPS.

#### M7.2 — Menus and GUI

Implement original versions of:

- Title/main menu.
- Local/online/mode selection.
- Character/team selection.
- Stage selection and hazard option.
- Controller assignment and configuration.
- Match HUD, pause, results, rematch, settings, credits, and accessibility basics.

Navigation timing, clarity, controller-first operation, and flow should feel familiar to SSBM players without copying its visual assets or exact layouts.

**Acceptance criteria**

- Every feature and mode is reachable without debug commands.
- All menus are fully navigable by supported controllers and keyboard.
- Back/cancel, reconnect, invalid selection, and multi-controller edge cases are verifier-tested.
- Visual verifier references exist for every stable menu state.
- The owner approves menu flow and readability.

#### M7.3 — Audio and music

- Implement low-latency mixing, buses, volume controls, streaming, and rollback event reconciliation.
- Add original main-menu music, character-select music, stage music, results music, UI sounds, movement sounds, attack sounds, hit sounds, defensive sounds, and fighter-specific vocal/effect cues.
- Define completion checks in the design workbooks so missing event audio is detectable.

**Acceptance criteria**

- Required menu, selection, stage, fighter, and move events have mapped original audio.
- Rollback never doubles confirmed sounds or permanently drops required cues.
- Audio can be disabled or compiled out without changing simulation behavior.
- Native and web builds have functional music, mixing, and volume controls.
- All audio/music has recorded ownership/license provenance.

#### M7.4 — Controllers and input configuration

- Normalize physical inputs into a compact deterministic per-tick action packet.
- Support SDL3 gamepads, Steam Input, GameCube controller/adapters, Xbox, PlayStation, and other major mapped controller types.
- Provide assignment, remapping, dead zones, stick calibration, trigger thresholds, rumble where supported, controller glyphs, profiles, hot-plug, disconnect/reconnect, and conflict handling.
- Provide equivalent browser mappings where the browser exposes the device.

**Acceptance criteria**

- Each named controller family is verified on representative real hardware or explicitly tracked until hardware is available.
- Configuration is easy to reach, test, save, reset, and understand.
- Native non-Steam play does not require Steam.
- Steam Input uses action-oriented bindings and official configurations.
- Input normalization is deterministic and recorded in replays.
- A disconnect cannot corrupt or silently alter a ranked match.

**Human checkpoint:** Mandatory native and browser GUI/audio/controller playtest.

---

### M8 — Full original content production

#### M8.1 — Fighter waves

Scale the approved combat system in waves rather than all at once:

1. Four-fighter archetype coverage.
2. Eight fighters.
3. Approximately half the final roster.
4. Full D1-A mechanical-counterpart coverage.

Each fighter receives original naming, silhouette, lore, move expression, animation, effects, voice/sound set, physics/data, AI/verifier scripts, portraits, selection assets, and balance tests.

M8 is the completion gate for character-specific SSBM advanced techniques.
Before the first fighter wave, create a pinned row-by-row registry from the
separate
[character-specific advanced-technique section in revision 2048934](https://www.ssbwiki.com/index.php?title=Advanced_technique&oldid=2048934#List_of_character-specific_advanced_techniques).
Include every listed technique applicable to a fighter or form in the SSBM
roster, then map it to the corresponding original D1-A mechanical counterpart.
Entries exclusive to fighters that are not in SSBM are outside this
SSBM-counterpart requirement.

Each mapped technique must be performable in an ordinary match, use
production-path mechanics and original presentation, have deterministic
positive/negative verifier evidence, and include a browser playtest recipe.
The supporting fighter wave cannot be accepted until all of its mapped
techniques are verified; the final M8 fighter wave is blocked until the entire
character-specific registry is verified.

**Acceptance criteria for each wave**

- Every fighter is complete enough for full 1v1, 2v2, CTF, replay, rollback, and RL use; no “visual-only” roster entries count.
- Every move/state has valid data, hit/hurt boxes, animation, and required sound/event mappings.
- Automated matchup smoke tests cover every ordered fighter pair and representative teams.
- Every character-specific advanced-technique registry row mapped to the wave's
  fighters is playable and verified; each row identifies the exact supporting
  fighter/item/stage configuration.
- Originality/provenance review passes.
- Worst-case fighter combinations meet state-size and performance budgets.
- The owner playtests and accepts the wave before the next wave begins.

**Final fighter acceptance**

- Every playable SSBM fighter/form has an original D1-A mechanical counterpart preserving substantially the same move functions and matchup identity.
- Every character-specific SSBM advanced technique mapped to those counterparts
  is playable, deterministic, verifier-tested, and browser-testable.
- Names, art, audio, music, animation, writing, and presentation are original.
- The roster preserves broad matchup and playstyle diversity rather than becoming cosmetic variants.

#### M8.2 — Ten-stage waves

Produce stages in waves, each with original layout, theme, art, music, animation, collision, hazards, spawn/blast-zone data, hazard-off rules where needed, and performance tests.

The final ten must include:

- Winter.
- Autumn.
- Summer.
- Desert.
- Tropical forest.
- Rocky mountains.
- Sea.
- Three additional original themes chosen during production.

**Acceptance criteria**

- Exactly ten release-ready stages exist, each visually and mechanically distinguishable.
- Every stage has at least one unique themed hazard or stage behavior, plus deterministic tests.
- Every stage works in 1v1, 2v2, CTF where its mode eligibility says it should, RL, replay, and rollback.
- Stage art/music is original and provenance-recorded.
- Hazard-heavy four-fighter scenarios stay inside the performance charter.
- The owner playtests each stage wave.

#### M8.3 — Content-wide polish and balance

- Run roster/stage matchup automation, recovery reach tests, infinite/stall detection, collision sanity, visual readability, audio completeness, and frame-budget checks.
- Use human playtesting to tune responsiveness, fun, counterplay, team readability, and stage fairness.

**Acceptance criteria**

- No critical verifier or human-feedback issue remains open for full-content netplay integration.
- All design changes flow through the workbook/data system.
- Balance decisions are documented without claiming exact SSBM equivalence.

---

### M9 — GGPO rollback and P2P netplay

#### M9.1 — Rollback core

- Implement or adapt the GGPO save/load/predict/rollback/time-sync model behind a C interface.
- Preserve the GGPO behavioral contract while making the implementation portable to native and web targets.
- Build conformance tests against upstream GGPO concepts/sample traces because upstream currently documents Windows-only builds.
- Reconcile visual, audio, rumble, and camera events across speculative and confirmed frames.
- Support 1v1 and four-player 2v2 ownership/topologies.

**Acceptance criteria**

- Rollback uses the exact deterministic simulation and snapshots used by local play and RL.
- Prediction errors restore and re-simulate to the same hash as an input-complete reference run.
- Rollback depth, prediction frames, input delay, and time sync are configurable and reported.
- 1v1 and 2v2 pass automated delay/jitter/loss/reorder simulations.
- GGPO origin/license and any modifications are documented.

#### M9.2 — P2P transport and session setup

- Implement D4-A full native/web cross-play.
- Implement signaling, session/version/content negotiation, peer identity, NAT traversal, STUN, TURN/relay fallback, encryption, connection-quality measurement, and teardown.
- Select the transport after measured native↔native, browser↔browser, and native↔browser spikes.
- Keep signaling/relay services outside the deterministic simulation.

**Acceptance criteria**

- Supported peer combinations establish sessions behind representative home NATs; relay fallback works when direct P2P fails.
- Input packets prioritize latency and tolerate loss/reorder without head-of-line behavior that damages play.
- Incompatible builds/content/configurations cannot start a match.
- Network code cannot mutate deterministic state except by supplying authenticated normalized inputs.
- P2P topology remains the gameplay path; ranked verification happens after the match and does not make simulation server-authoritative.
- Native↔native, browser↔browser, and native↔browser matches share the same versioned protocol and player pool.

#### M9.3 — Netplay quality, desyncs, and recovery

- Add state-hash exchange, desync capture, deterministic replay dumps, ping/jitter/rollback display, pause/disconnect/forfeit rules, and reconnect policy.
- Test long sessions and adversarial network profiles.

**Acceptance criteria**

- No unexplained desync remains in the supported platform matrix.
- On a desync, the build automatically preserves enough inputs, hashes, states, versions, and network metadata to reproduce it.
- Match outcomes remain correct under configured delay, jitter, loss, and rollback limits.
- Local and online play use identical gameplay values and state transitions.
- The owner completes 1v1 and 2v2 netplay playtests across the required platform combinations.

**Human checkpoint:** Mandatory real-network rollback playtest before online ranking work.

---

### M10 — Online services and competitive progression

#### M10.1 — Accounts, lobbies, and matchmaking

- Implement secure player identity/session handling, presence, invitations or lobby codes, regional/ping filtering, version/content enforcement, and match tickets.
- Provide separate ranked and unranked queues for supported modes/team arrangements.
- Keep offline/local and direct unranked play available when ranking services are unavailable where practical.

**Acceptance criteria**

- Ranked and unranked matchmaking complete end to end on every supported client class.
- Queue rules, team composition, connection-quality limits, cancellation, timeout, and failure behavior are documented and tested.
- Services cannot inject arbitrary state into a match.

#### M10.2 — Elo, ranks, and leaderboards

- Implement a documented Elo rating system, placement/provisional handling, team-rating policy, disconnect/forfeit policy, seasons if selected, and decay/reset policy if selected.
- Map rating bands to configurable named ranks including at least Bronze, Silver, Gold, and Platinum; additional tiers remain a design choice.
- Implement overall and mode-specific leaderboards with pagination, player lookup, ties, and privacy/moderation controls.

**Acceptance criteria**

- Rating updates match golden mathematical test vectors.
- The same verified match cannot be applied twice.
- Rank names and thresholds are configuration, not hardcoded UI logic.
- Leaderboard ordering, ties, season boundaries, and banned/invalidated results are tested.
- Players can clearly distinguish ranked from unranked play and inspect their rating/rank.

#### M10.3 — Ranked integrity

- Implement D5-A P2P gameplay plus server replay verification.
- Issue signed match tickets, collect canonical input streams and peer reports, re-simulate in the headless validator, compare hashes/results, and apply ratings only after verification.
- Add replay protection, rate limits, abuse/dispute logging, modified-content rejection, and service observability.

**Acceptance criteria**

- A modified client cannot submit arbitrary winner/score data and receive rating without the selected verification path.
- Validator simulation uses the same versioned deterministic core and design-data hash as clients.
- Conflicts, missing peers, disconnects, validation failures, and timeouts have explicit outcomes.
- Security/load testing covers forged, replayed, duplicated, malformed, and oversized submissions.
- Ranking failure cannot corrupt local replays or deterministic simulation.

**Human checkpoint:** Validate the full ranked/unranked user journey and ranking behavior.

---

### M11 — Release hardening

#### M11.1 — Platform and packaging matrix

- Produce signed/notarized or otherwise platform-appropriate native packages for supported Windows, macOS, and Linux architectures.
- Produce a deployable web package with correct caching, compression, content hashes, controller messaging, and browser compatibility handling.
- Produce documented headless library/executable packages and RL wrapper packages.
- Re-run the clean-machine reproducibility scripts on every target.

**Acceptance criteria**

- Windows, macOS, Linux, web, and headless release artifacts build from a clean checkout using documented scripts.
- Release artifacts contain only their intended systems; headless has no GUI/graphics/audio assets or dependencies.
- Browser startup, input, audio unlock, save/config persistence, and networking pass the supported-browser matrix.

#### M11.2 — Full-system verification

- Run unit, integration, determinism, replay, rollback, network-fault, visual, controller, workbook, migration, fuzz, sanitizer, soak, security, and performance suites.
- Profile all canonical scenarios and resolve material bottlenecks or document an approved exception.
- Close or explicitly defer every verifier and human-feedback issue.

**Acceptance criteria**

- 1v1, 2v2, and capture the flag are release-ready.
- Full D1-A mechanical-counterpart roster coverage and all ten required stages are release-ready.
- Native and web clients have complete menus, GUI, original visuals, animation, music, sound, and required controller support.
- Ranked/unranked netplay, Elo, rank tiers, and leaderboards pass end-to-end tests.
- Headless/RL builds meet the D3-C relative-performance charter.
- Every supported platform produces identical deterministic replay hashes for the canonical corpus.
- Every one of the 61 required SSBM advanced-technique registry rows is playable,
  deterministic, verifier-tested, browser-testable, and linked to passing
  evidence for its supporting configuration.
- No unresolved critical/high issue remains unless the owner explicitly accepts it in writing.
- The requirement traceability matrix is fully green.

#### M11.3 — Final human acceptance

Provide a release candidate, benchmark report, verifier report, content/provenance report, known-issues list, and reproducible build instructions.

**Acceptance criteria**

- The owner completes final playtests for local 1v1, local 2v2, CTF, native netplay, browser netplay, ranked, unranked, representative fighters, every stage, menus, audio, and controller configuration.
- All resulting feedback is processed through `human_feedback/`.
- The owner explicitly approves entry into M12.

---

### M12 — Continuous performance-improvement loop

After the complete game is accepted, repeat the following loop until the owner explicitly pauses or ends it.

#### M12.1 — Maintain hypothesis queues

Maintain:

- `optimizations/tested_hypotheses.md`, containing hypothesis, branch, date, baseline, result, and reason.
- `optimizations/hypothesis_backlog.md`, ordered by expected impact divided by implementation/validation cost.

At the beginning of each cycle, generate exactly three prioritized hypotheses that differ materially from previously tested hypotheses. A prior idea may be retried only if a documented system change invalidates the old result.

#### M12.2 — Isolated experiments

For each hypothesis:

- Create a separate feature branch from the same current `master` baseline.
- State the mechanism, expected affected profile region, expected speedup, gameplay risk, determinism risk, and success threshold before implementation.
- Implement the smallest valid version in bite-sized commits.
- Run verifier, determinism, full relevant performance scenarios, and playtests proportional to gameplay risk.
- Write a complete analysis file under the eventual result directory.

#### M12.3 — Evidence-based disposition

- **Measured speed improvement with minimal gameplay/game-feel change:** automatically merge into `master` only when it exceeds the M0 meaningful-improvement threshold, passes all required verification, and has no hidden memory/platform regression. Store the analysis under `optimizations/merged/<feature_branch_name>/`.
- **Measured speed improvement with substantial gameplay/game-feel change:** do not merge. Store the branch analysis under `optimizations/pending/<feature_branch_name>/` and stop for owner playtesting/approval, even if the improvement is very large or extreme.
- **No meaningful speed improvement, regression, or invalid result:** do not merge. Store the analysis under `optimizations/discarded/<feature_branch_name>/`.

After an automatic merge, run the complete post-commit verifier/performance workflow on `master` and revert through a normal commit if integration invalidates the branch result.

**Acceptance criteria for every cycle**

- Three genuinely different hypotheses are proposed and their pre-implementation expectations are recorded.
- Every result is reproducible from a named baseline, branch, build, scenario, seed, and machine configuration.
- Statistical noise is not called an improvement.
- State size, rollback cost, determinism, other platforms, memory, and game feel are considered alongside raw TPS.
- No gameplay-changing optimization is silently merged.
- Tested and backlog lists are updated before the next cycle.
- All analysis files use the required merged/pending/discarded directory.

---

## 7. Execution protocol

### 7.1 — Bite-sized commits

A work item is small enough to review, verify, benchmark, and revert independently. Each is committed with a message describing the behavior or infrastructure change, not merely the files edited. Generated content and mechanical refactors are separated from behavior changes where practical.

### 7.2 — Mandatory post-commit sequence

After every commit:

1. Run the verifier’s diff-selected checks and required global smoke checks.
2. Run the canonical performance suite and record it in local SQLite.
3. Regenerate performance graphs.
4. Mark the commit pass/fail with links to local evidence.
5. If a critical failure appears, stop feature work and create an issue under `verifier/issues/unfixed/`.

CI repeats portable correctness checks before anything reaches `master`; canonical local performance numbers remain separated by machine fingerprint.

### 7.3 — Plan modifications

`plan_modifications.md` records:

- Sequential modification ID and date.
- Affected milestone/requirement.
- Old plan.
- New plan.
- Evidence and reason.
- Performance/gameplay/schedule consequences.
- Whether owner approval was required and obtained.
- Implementing commit(s).

The plan is not treated as ground truth when measurements or implementation reality contradict it, but changes are never silent.

### 7.4 — Verifier issue lifecycle

- Discovery creates `verifier/issues/unfixed/<issue-id>.md`.
- The fix is made and committed.
- A following bookkeeping commit adds the prior fix hash and moves the file to `verifier/issues/fixed/`.
- Regression tests remain after the issue moves.

### 7.5 — Human feedback lifecycle

- At every mandatory checkpoint, stop and provide a playable build plus focused test notes.
- The owner adds reports to `human_feedback/unfixed/`.
- Each fix is implemented in a bite-sized commit.
- A following bookkeeping commit records the prior fix hash and moves the report to `human_feedback/fixed/`.
- The milestone remains unaccepted while blocking feedback is still unfixed.

### 7.6 — Experimental branches

- Extreme or gameplay-changing performance work always begins on a feature branch.
- Branch analyses use a common benchmark baseline and report format.
- Only M12’s minimal-change category can auto-merge.
- Pending gameplay changes remain available for owner comparison and are never represented as accepted mainline behavior.

---

## 8. Requirement-to-acceptance traceability

| Source requirement | Acceptance location |
|---|---|
| Fun 2D platform fighter, Melee-like play | M0.1, M4, M8.3, M11.3 |
| All non-character-specific techniques marked available for SSBM in the pinned advanced-technique table | M4.4–M4.5, M11.2 |
| Character-specific advanced techniques applicable to the SSBM roster | M8.1, M11.2 |
| Maximum single-thread performance | M0.2–M0.3, M2, M3, M11, M12 |
| Ultra-fast RL tool | M2.2, M3.1, M11.2 |
| 1v1 and 2v2 | M6.1, M9, M11.2 |
| All SSBM characters without copied IP | D1-A, M0.1, M8.1, M11.2 |
| Ten themed stages and unique hazards | M0.1, M6.2, M8.2, M11.2 |
| Painterly Hades-like quality with original assets | M7.1, M8, M11 |
| SSBM-like menus/GUI | M7.2, M11 |
| Headless maximum-performance mode | M1.1, M2.2, M11 |
| Complete music and sound | M7.3, M8, M11 |
| GGPO P2P rollback | M9 |
| Ranked/unranked, leaderboards, ranks, Elo | M10, M11 |
| Steam/GameCube/Xbox/PlayStation/major controllers | M7.4, M11 |
| Windows/macOS/Linux/browser | M1, M7, M9, M11 |
| ImGui design-value editing | M5.2 |
| Capture the flag | M6.3, M11 |
| C implementation | D2-A, M0.4, M1 |
| Current best tools selected through research | Section 4, M0.4, per-dependency decision records |
| Single-thread engine; client may multithread | Section 3, M1.1, M2 |
| Excel design data imported at runtime | D6-A, M5.1 |
| Minimize hardcodes and duplication | Section 3, M5.1, verifier rules |
| Beautiful implementation, correct zero-cost abstractions, and minimal-to-nonexistent logic duplication | Sections 1, 3, and 6; every milestone code review; M11.1–M11.2 |
| SDL3 native graphics | Section 4, M7.1 |
| Reproducible setup scripts | M1.2, M11.1 |
| Comprehensive performance suite | M3.1 |
| Performance tests, SQLite, and graphs after every commit | M3.1, Section 7.2 |
| Periodic profiling and `performance/` storage | M3.1 |
| Verifier agent after every commit and issue directories | M3.2, Section 7.4 |
| Plan can change; log changes | Section 7.3 |
| Bite-sized Git commits | Section 7.1 |
| Human playtest stops and feedback directories | Milestone map, Section 7.5 |
| Performance experiments encouraged | M0.3, M12 |
| Separate branches for extreme/gameplay-changing changes | Section 7.6, M12 |
| Continuous three-hypothesis optimization loop | M12 |
| Merged/pending/discarded analyses and hypothesis lists | M12 |

---

## 9. Principal risks and explicit controls

| Risk | Control |
|---|---|
| D1-A mechanical counterparts create elevated originality/IP risk | Preserve original expression and independently authored data, maintain provenance, and perform formal IP review before public release. |
| Authored C conflicts with required C++ libraries | Enforce D2-A through narrow generated/handwritten C ABI boundaries. |
| Upstream GGPO is not presently a turnkey cross-platform dependency | Use its documented behavior and MIT code as a reference, then prove the port/adaptation with conformance, determinism, and fault tests. |
| Quantization/bitboards improve speed but damage feel | M0 side-by-side prototypes, human rubric, separate experimental branches, and pending approval for substantial changes. |
| Excel parsing harms startup/web/headless simplicity | Apply D6-A, validate once, use packed production data, and keep all parsing out of ticks. |
| P2P ranked play enables cheating | Apply D5-A deterministic server replay verification using the headless engine. |
| Native/browser renderer or transport divergence | Apply D4-A with shared simulation/protocol schemas, cross-platform replay hashes, and explicit compatibility matrices. |
| Full roster/art/audio scope overwhelms the project | Vertical slice first, fighter/stage production waves, hard acceptance gates, and no scaling before feel approval. |
| Per-commit benchmark artifacts create a Git recursion | Keep canonical SQLite/graphs local and keyed to commits; commit only explicit milestone reports. |
| Fix reports cannot contain their own commit hash | Use a fix commit followed by a bookkeeping commit that moves/annotates the issue with the actual prior hash. |
| “Non-stop” optimization has no natural finish | M12 repeats until the owner explicitly pauses or terminates it; every cycle remains auditable. |

---

## 10. Definition of complete

The game is complete enough to enter the continuing optimization phase only when:

- Every M0–M11 acceptance criterion is met or explicitly accepted as an exception by the owner.
- The full requirement traceability table is green.
- There are no unapproved critical/high verifier or human-feedback issues.
- The owner has playtested and approved native, web, local, headless/RL, rollback, ranked, unranked, 1v1, 2v2, CTF, full roster, all stages, menus, controllers, visuals, music, and sound.
- Reproducible release artifacts and the final performance/verifier reports exist.
- `master` is the accepted release baseline from which M12 branches begin.

At that point, M12 starts and continues as an evidence-producing optimization program rather than an unbounded stream of undocumented tweaks.
