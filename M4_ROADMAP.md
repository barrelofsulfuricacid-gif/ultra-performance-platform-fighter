# M4 execution roadmap

Last updated: 2026-08-11

This is the short, current status companion to
`ultra_performance_platform_fighter_implementation_plan.md`. A row is marked
done only when its implementation and required local verification are both
complete. Live Dolphin qualification and stored-oracle regression are tracked
separately because a stored pass cannot establish new SSBM truth.

## Goal

1. Make the SSBM-to-simulation equivalence harness fast, reusable across
   characters, and explicit about the behavior domains it proves.
2. Finish the Captain Falcon NTSC 1.02 port, allowing only documented bounded
   Q16.16 representation differences.
3. Deliver a native playtest frontend with Battlefield.
4. Continuously improve the `ssbm-character-importer` skill so later character
   ports reuse common import, oracle, and runtime machinery.

## Current snapshot

| Workstream | State | Current evidence or next gate |
| --- | --- | --- |
| Fast stored equivalence | done for twenty-one domains | The registry contains 117 cases plus deterministic replay. The pose domains hash 1,265 production poses: 689 common/ledge, 33 Turn/TurnRun, 186 ordinary-airborne, 79 Pummel/CaptureWaitHi/CaptureDamageHi, and 278 CrouchWait/Appeal poses. Raptor Boost adds 502 field-masked numeric samples, Falcon Kick adds 399, natural movement adds 520, the five aerial-landing routes add 685, and passive shield break adds 500. Independent domains run concurrently while output retains manifest order. The current full gate takes 0.891 seconds on Windows and 1.298 seconds in WSL, below the two-second budget. Eight fractional ground-loop HSD observations plus focused Raptor/Dive/FallSpecial ECB cases run in the C gate because they validate the generic source evaluator rather than duplicating production pose data in the registry. |
| Fast live Dolphin oracle | done for the current registered domains | Registered packs use headless/null/unlimited ExiAI and checkpoint-isolated cases. The focused Turn/TurnRun pack captures 39 rows in 0.228 seconds warm with fast-forward and 0.565 seconds with fast-forward disabled; both produce semantic SHA `1cc3543b1363ecb5c7427c36f4d8d8a2826f9fb7c5281877f54108e1ffe281a2`. Generic label-based projection now reduces the WalkFast changed-domain capture from seven cases / 423 rows / 4.820601 seconds warm to one case / 32 rows / 0.160820 seconds warm. Unsafe cross-invocation observer reconnection is rejected. |
| Falcon bounded hurt poses | common, turn, ledge, ordinary airborne, pummel/capture, crouch-wait, taunt, and dynamic ground-loop poses imported | The common-state domain covers 255 source poses. Turn/TurnRun add 33 unique poses / 363 capsules, including source frame zero and the seven-observation frame-9 freeze. The single update where gameplay facing has flipped but display bones retain their prior facing is derived from the existing TurnRun tick/facing/direction tuple and now drives combat plus inspection without new snapshot state. Three stored phase cases cover pre-flip frame 9, pending-display frame 9, and resumed frame 10. Two byte-identical no-fast-forward captures add 434 quick/slow ledge-option poses (4,774 capsules). Two independent unlimited headless captures add all 186 ordinary-airborne poses / 2,046 capsules. The natural paired route adds all 24 CatchAttack, 35 CaptureWaitHi, and 20 CaptureDamageHi poses plus the measured 53,806-Q16 capture anchor. Two independent grounded-loop captures add all 158 CrouchWait and both 60-frame Appeal orientations (278 poses / 3,058 capsules). All four velocity-driven WalkSlow/Middle/Fast and Run motions resolve fractional poses from the compact shared Q16 HSD evaluator: each of two independent captures contributes 131 live samples / 1,441 capsules agreeing with Dolphin within 2 Q16 units, and eight stored source observations protect the C implementation. |
| Falcon movement and combat | partial | Captured routes include wall/ceiling response, flat-floor missed/neutral/directional techs, both Up/Down prone/getup orientations, grounded player push from both ports/directions, imported Hyrule slope/DownBound/ordinary-ledge response, the exact common-data `x480` down-input ledge rejection boundary, all eight quick/slow ledge options, exact 640/480-frame CliffWait timeout and regrab cooldown, ordinary Jump/Fall airborne animation clocks, Falcon Punch's complete ground/air clocks and qualified air-physics tail, Raptor Boost's five complete fighter routes plus its live-only native Capsule branch, all six Falcon Kick routes, and a source-qualified complete Battlefield collision/environment catalog. Production imports complete Jump/Fall, all five aerial-attack ECB bottoms, all 42 ShieldBreakFly frames, both 26-frame DownBound tracks, both 70-frame DownWait loops, both neutral getups, both getup attacks, all four prone-orientation/direction roll tracks, and the complete 158-frame CrouchWait ECB cycle: 648 exact four-point ground/prone poses total. The focused DownBound route reproduces 52 poses from 600 rows in 6.6 seconds; the eight-case getup route reproduces 438 poses from 1,150 rows in 10.5-10.9 seconds using four parallel headless workers. The one-case CrouchWait repeat reproduces all 158 source-frame-indexed poses from 160 rows in 5.31 seconds warm and confirms the authoritative five-case capture semantically. Two independent 500-frame passive-depletion captures qualify the exact Fly/DownD/StandD/Furafura sequence, animated-ECB contact update, transition velocity, shield regeneration, and Furafura reset behavior. WalkSlow/Middle/Fast and Run now derive transition and post-blend ECB/hurt geometry from one compact HSD pose. The 27-row fixed-point recurrence oracle plus five checkpoint-equivalent production ticks cover Wait-to-Walk, two ordinary gait changes, a nested gait change, and Dash-to-Run; production transition geometry stays within 4 Q15 rotation and 4 Q16 translation units. Raptor Boost plus Falcon Dive start/catch/throw and common FallSpecial now derive complete ECBs from the same DAT/HSD evaluator. Nine retained captures qualify eight motions, 733 observations, and 715 unique frames within two Q16 units. A fresh 165-frame natural Falcon Dive miss route passes the identical-input comparator with the documented 640-Q16 position envelope. FallSpecial additionally reproduces the decomp's velocity-selected neutral/forward/back target, switch-update double blend, stable-update single blend, and persistent bottom-lock state. The former Raptor/Falcon Dive/FallSpecial authored ECB arrays are removed. Remaining broader action-specific ECB and special-action acquisition coverage stays open. |
| Common damage response | qualified for open-air launch | Six-case live Dolphin trace and generic stored numeric trace pass. Ground and collision response remain separate domains. |
| Separate knockback velocity and decay | open-air and flat-ground routes qualified | A pinned 64-row late DashAttack route agrees for 15 damage samples on action/frame, grounded/tumble, damage, timers, self velocity, projected knockback, and `xF0_ground_kb_vel` within 0.001 source units. Canonical save/load, replay, Windows, WSL, and sanitizers pass. |
| Remaining Falcon gaps | not complete | Work through every incomplete row in `docs/product/m4_ssbm_fidelity_audit.md`; do not infer whole-character equivalence from one domain. |
| Native Battlefield frontend | implemented locally; hands-on gate remains | The SDL target runs the real simulation at fixed 60 Hz, supports 2P Duel and 4P Teams plus 1-8 stocks through the core config contract, renders the complete source-derived 23-line Battlefield catalog and blast-zone inset, and visualizes fighters, crouch, shields, hitboxes, exact 11-capsule source hurt poses, damage, stocks, and actions. Strict MSVC, WSL, smoke, and screenshot QA pass. One real-controller hands-on pass remains. |
| Character-importer skill | active | The skill records reusable HSD/PlCo import, damage-channel, callback-order, ground-projection, save/load, action-release, physical surface-route, lifecycle, semantic-digest, `StageInfo`/JObj stage-import, per-surface collision routing, previous/current animated-ECB identity, bounded one-way-platform crossing qualification, source-pose matrix-branch guidance, and target-skeleton switch versus stable-update blend semantics. |

## Completed and verified: Falcon Dive and common FallSpecial DAT/HSD ECB ownership

- [x] Expand the existing parent-closed HSD profile instead of adding a second
  animation parser or move-specific pose tables. It now contains 17 motions,
  1,204 tracks, and 10,106 keys under generated-data SHA-256
  `ab3eae37b1a8a0e0b495291d3d1548093906b0ffecd9e4bb59ea32d0056b5f9a`.
- [x] Route Falcon Dive ground/air start, catch, throw, and common
  FallSpecial neutral/forward/back through one allocation-free matrix-to-ECB
  evaluator. Raptor and Dive share source-clock dispatch, entry-pose ownership,
  bottom-lock application, TransN subtraction, and four-point ECB conversion.
- [x] Reproduce `ftCo_Fall_Anim_Inner` plus `ftCo_800CC988`: a newly selected
  directional target is blended once when installed and again after its target
  skeleton advances; an unchanged target takes only the latter pass. Persist
  the one-bit switch fact because it changes collision geometry and rollback.
- [x] Qualify nine retained live captures / eight motions / 733 rows / 715
  unique source frames with maximum error two Q16 units. Three focused C cases
  cover FallSpecial entry, target switch, and stable continuation within 64
  Q16 units.
- [x] Refresh the natural `up_air_miss_natural` Dolphin route and replay the
  same inputs through production. All 165 compared frames pass with strict
  actions and velocities plus the existing 640-Q16 position envelope; the raw
  refresh capture is SHA-256
  `55ddf2eed8a8cd52d788075b62bca1e6d7be3a1a26ce89bec6e02b85475ea5b4`.
- [x] Remove the authored Raptor Boost, Falcon Dive, and FallSpecial ECB
  arrays. FallSpecial loop length now comes from the imported submotion catalog,
  leaving one source owner for both timing and geometry. The regenerated
  Falcon include is SHA-256
  `754a72159e5463752e382dd6a2a8e35657bab601b84c228ade5c540d30272a74`;
  its complete-source digest is
  `74e4a7b7a8635a870ba65cb77425eb3500d0df2d53b41fa1ff9aa9fffbb3edee`.
- [x] Carry the new observable switch state through canonical reset, tick,
  snapshot, replay, inspection, and validation. State schema is 74, inspection
  schema is 56, save size is 1,747 bytes, and replay size is 42,519 bytes.
- [x] Pass the full WSL Release suite 40/40 in 5.67 seconds and strict native
  Windows MSVC suite 40/40 in 8.35 seconds, plus the direct HSD source theorem,
  deterministic replay/snapshot tests, web/native smoke, and reproducible
  Falcon/common-data import checks.

## Completed: exact fractional Walk/Run ECB ownership

- [x] Re-sweep pinned/current `doldecomp/melee`, current HSDLib, and the
  existing HSD evaluator before changing production. Pinned/current
  `ftanim.c` and `mpcoll.c` are unchanged; no maintained external tool ports
  Melee's complete local-SRT blend state into a deterministic fixed-point
  runtime.
- [x] Extend the existing surface probe with the six `ECBSource` JObj pointers,
  their parent-closed 25-joint local SRT/world matrices, and the corresponding
  `FighterBone` flags. A four-worker live pack captures all four gaits in
  7.48 seconds warm; the focused one-worker WalkMiddle pack takes 4.66 seconds
  warm including launch/menu setup.
- [x] Isolate and correct the former 0.02-0.16 source-unit discrepancy. The
  capture's Player 1 uses costume ID 1 (`PlCaGy.dat`), while the evaluator used
  neutral `PlCaNr.dat`. The live leaf translations 25/47 exactly match the gray
  costume bind model; `ftAnim_8006FA58`/`ftAnim_8006FB88` copy that active
  costume SRT before attaching target tracks. No persistent-channel state is
  needed after blend completion.
- [x] Generate the six ECB source-joint selectors from the same 25-joint
  parent-closed catalog used by hurt geometry, reproduce grounded
  `mpColl_LoadECB_JObj` including its strict 10-unit symmetry predicate, and
  enable WalkSlow/Middle/Fast and Run production routing. Two independent live
  captures qualify 262 post-blend poses / 2,882 hurt capsules at maxima of one
  Q16 ECB unit and two Q16 hurt units; eight fractional/looped C observations
  protect both consumers.
- [x] Add the smallest shared representation of Melee's six-frame local-SRT
  transition recurrence, including a gait change begun before the previous
  blend completes, then qualify transition rows twice. The source half is now
  pinned: two independent 193-row captures verify 54 moving-target updates /
  1,296 joint recurrences within `1.2330335e-6` local and `2.20395109e-7`
  quaternion units, with 158 converged rows protected by semantic SHA-256
  `cd3aba1802a0b749e2b677e720eadb4c97b254574b548f184be529589ef16f1d`.
  Production now stores only 19 canonicalized Q15 quaternion xyz triples, six
  Q16 translation triples, and one Q16 progress value per player. A 27-row C
  oracle replays every captured moving-target update, including nested gait
  changes and accumulated compact re-quantization, with maxima of 8 Q15
  quaternion units and 4 Q16 translation units. Five checkpoint-equivalent
  production ticks now bind the real `pf_sim_tick` path to Wait-to-Walk,
  WalkSlow-to-Middle, nested WalkSlow-to-Middle, WalkMiddle-to-Fast, and
  Dash-to-Run capture rows. This exposed and fixed Melee's pre-IASA ordering:
  the old animation and any active old blend advance before the new action is
  selected. Production geometry now matches those entries within 4 Q15
  rotation and 4 Q16 translation units. Hurt geometry, inspection, and wall
  ECB consume the same reconstructed pose. No persistent per-channel rollback
  state is used for post-blend poses.
- [x] Run the full Windows/WSL/sanitizer/stored/replay/benchmark gates. Windows
  Release passes 40/40, WSL Release passes 40/40, and WSL ASan/UBSan passes
  26/26 after the production-transition correction. The complete 21-domain /
  117-case stored-plus-replay gate takes 1.018 seconds on Windows and 0.686
  seconds in WSL. The canonical tuple is now state 71 / save 66 / `PFSAVE60`,
  with a 1,567-byte payload and 1,707-byte checkpoint. A clean exact-parent
  WSL benchmark compares all ten available scenarios as compatible, with zero
  suspected or confirmed regressions. It uses the installed GCC 13.4.0 under
  the explicit unpinned-compiler override because the pinned 13.3.x compiler
  is unavailable on this host.

## In progress: broader action-specific ECB ownership

- [x] Replace Raptor Boost's route-captured 45-value aerial-hit bottom table
  with the shared DAT/HSD evaluator for all four ground/air start/hit motions.
  The evaluator subtracts the live TransN reference, applies Melee's grounded
  and airborne bottom policies, retains the four-update airborne bottom lock,
  and preserves previous-action pose ownership on frame-zero hit transitions.
- [x] Qualify the source evaluator against five raw Dolphin captures: 423
  observations spanning 411 complete action frames agree within one Q16 unit.
  Four direct C cases accept at most 32 Q16 units from deterministic fixed-point
  evaluation, and the complete 657-frame production Raptor suite passes.
- [ ] Continue the remaining action-specific ECB audit without introducing
  route tables when imported DAT/HSD data and the common evaluator can express
  the source behavior directly.

## Implemented and verified: decomp differential and web-startup cleanup

The 2026-08-10 source audit is now followed by actual generation, native/WSL,
stored-oracle, live Dolphin, Emscripten, and browser validation. The audit still
does not imply whole-game equivalence: only the registered live and stored
domains below are empirically qualified.

- [x] Replace authored or collapsed common behavior with source-routed mash,
  capture, grab/release, throw, shield-break, damage, hitlag, hitstun, meteor-
  cancel, rebirth, teeter, rebound/clank, dash/walk, jump, escape, and input-
  priority semantics wherever the current state model can express the decomp.
- [x] Add Falcon's missing Jab 3 and rapid-jab lifecycle, angled forward-tilt
  and forward-smash actions, exact smash-charge release clock, down-tilt repeat,
  pummel hitlag, throw weight scaling, stale scaling, C-stick defensive edges,
  and special/common collision transitions without adding authored frame data.
- [x] Preserve exact source identities for `Pass` (raw submotion 209),
  `Ottotto`/`OttottoWait` (210/211), `CatchCut`/`CaptureCut` (246/257), and the
  shield-break family (286-291). Keep raw DAT submotion identity separate from
  action-state identity.
- [x] Extend canonical rollback state only for callback-consumed history and
  continuation: mash directions, previous C-stick samples, horizontal input
  age/direction, rebound duration, jab-chain state, damage-jump buffering, and
  match KO/fall counters. The active compatibility tuple is content 77,
  fighter 69, state 71, save format 66, magic `PFSAVE60`, 1,567-byte payload,
  and 1,707-byte checkpoint.
- [x] Repeat the source-to-production callback audit across Wait, Dash/Run,
  Damage/DamageFall, Guard, Ottotto, Catch/Throw, Rebirth, Rebound, common
  ledge eligibility, and all four Falcon specials. No further discrepancy was
  found that is decidable from the currently represented decomp/data paths.

Remaining work is evidence/model work rather than a known code-only fix:

- [x] Import exact executable pose/hit geometry for Jab 3, rapid jab, every
  forward-tilt angle, and Falcon's real high/mid/low forward-smash variants.
  The two absent forward-smash slots remain source-absent. Two independent
  2,974-row headless/unlimited captures reproduce the same semantic payload.
- [x] Import exact pose/hit geometry and lifecycle for pummel/CaptureDamage.
  Two independent 3,142-row captures reproduce the same semantic payload; the
  generic stored domain hashes all 79 poses and exact active-frame hit/miss
  geometry.
- [x] Import and physically qualify exact pose geometry for the complete
  158-frame crouch-wait loop and both distinct 60-frame taunt orientations.
- [x] Implement Melee's velocity-driven WalkSlow/Middle/Fast and Run animation
  cursor, gait selection, and walk-phase remapping. The generic clock oracle
  protects three cases / 111 source samples through the shared C runner.
- [x] Bind WalkSlow, WalkMiddle, WalkFast, and Run fractional hurt geometry to source DAT
  tracks through the shared deterministic Q16 HSD evaluator. The live source
  comparator passes both independent 131-sample / 1,441-capsule captures with
  a maximum 2-Q16 delta; eight stored observations protect the production C path. WalkFast is reached
  naturally by entering Walk below the dash threshold before raising the stick
  above Falcon's fast-gait velocity boundary.
- [x] Import all 158 CrouchWait four-point ECB poses, retain its ordinary
  one-frame source animation clock through canonical tick/snapshot state, and
  select each pose in O(1) from the existing cursor. The independent
  160-row live repeat reproduces semantic SHA
  `ba47ef2736a5677d1909262a20f32991b7c2515407fae26626d5869b95edd265`.
- [x] Remove signed-overflow dependence from shared Q16 collision ratios. The
  fast path is unchanged; only values that cannot safely multiply by 65,536
  use an exact fixed 16-step unsigned divide. This restores bit-identical
  Hyrule contact selection under ASan/UBSan without changing native output.
- [ ] Reproduce WalkSlow/Middle/Fast and Run ECBs through the exact HSD
  six-frame blend and the decomp's 10-unit symmetry branch. The current
  source evaluator is not promoted for collision: tiny pose differences can
  flip that branch and create errors as large as 1.52 Melee units.
- [x] Extend the live hitbox/hurtbox probe with Fighter `cur_anim_frame`
  (`+0x894`) and `frame_speed_mul` (`+0x89C`). A fresh seven-checkpoint,
  423-row capture proves the decomp callback order and exposes fractional
  Walk/Run clocks for the implementation comparator.
- [x] Import shield-break DownU/DownD selection from the terminal ShieldBreakFly
  HipN matrix. Falcon's source component is `-3921` in Q16, so production enters
  DownD then StandD. Two independent 500-row natural shield-depletion captures
  reproduce `ShieldBreakFly -> DownD -> StandD -> Teeter` with exact action
  counts and IDs. Import the complete 42-frame animated ECB so landing occurs
  on the source update; retain the one-frame landing vertical velocity; preserve
  hit-break reset health versus passive-depletion zero health; and run global
  regeneration through Fly/Down/Stand plus Furafura's per-frame reset.
- [ ] Represent dynamic rebirth targets/companion coordination, full stage and
  item-kind behavior, aerial item/tether callbacks, and tournament entry/rule
  choreography before claiming those wider SSBM domains.
- [x] Remove the duplicated simulation scenarios from production browser
  startup. `m4_playtest.c` is reduced from roughly 14,000 to 1,100 lines and
  `pf_web_m4_playtest_install` now receives four real configuration values
  instead of four values plus 58 probe statuses. Native simulation suites own
  simulation fidelity; the web gate retains adapter ABI, controller polling,
  mapping, UI, Wasm-load, and real browser interaction checks.
- [x] Remove the remaining host-compiled gameplay duplication from
  `tests/web/test_m4_playtest.c`. The adapter test is reduced from 1,337 to 397
  lines and now checks only startup/view packing, exported input endpoints,
  independent trigger edges, hit-geometry presentation, duel/team setup, and
  four-player routing. Falcon move lifecycles, tactics, items, and combat-event
  semantics remain solely in native simulation, replay, and SSBM-oracle lanes.
  The shell gate is reduced from 421 to 150 lines by deleting action-name and
  simulation-source greps; it retains syntax checks and compact browser-only
  Gamepad, WebUSB, controller, visual-cue, and endpoint contract markers.
- [x] Correct the imported initial-dash frame clock so an A press after source
  Dash frame 4 enters DashAttack, keep ground-damage animation advancing after
  hitstun unlocks, and remove Falcon's incorrect six-frame delayed-double-jump
  projection. The pinned 64-row ground-knockback capture passes all 15 compared
  samples, and both 520-row natural-landing captures pass the corrected ordinary
  JumpAerial projection.
- [x] Pass Windows Release 36/36 in 1.49 seconds and WSL Release 38/38 in 0.92
  seconds after the DownWait/getup ECB import. The latest stored gate covers all
  21 domains / 117 cases plus replay in 0.932 seconds on Windows and 1.723
  seconds in WSL. WSL ASan/UBSan passes 25/25 in 9.22 seconds. The rebuilt
  Emscripten target and real in-app-browser
  startup/control/console smoke pass; starting a local match advanced the live
  simulation to tick 50 without console warnings or errors.

## Completed and verified

### Equivalence architecture

- [x] Pin owner GALE01 NTSC-U revision 2, decomp revision, Dolphin/ExiAI,
  libmelee, capture protocol, and source/production digests.
- [x] Replace per-experiment Dolphin GUI launches with one headless,
  null-renderer, unlimited-speed process per compatible packed run.
- [x] Restore checkpoint-isolated microcases without reconnecting the Slippi
  observer.
- [x] Batch fighter memory reads and cache unique bone matrices.
- [x] Prune serialization to manifest-declared observations while retaining all
  required command ticks.
- [x] Add character-independent stored-oracle registry, generator, affected-file
  selector, C runner, digest checks, replay gate, diagnostics, and time budget.
- [x] Keep Falcon-specific stored-oracle code to data bindings and thin
  production adapters.

Relevant commits on `agent/m4-combat-vertical-slice`:

- `dbf12f0` - checkpointed Dolphin foundation.
- `5c68258` - generic manifest-selected stored SSBM oracle.
- `b113dbb` - manifest-controlled live observation pruning and warm-budget gate.
- `b3edb14` - qualified open-air damage response, numeric stored oracle, and
  complete `PlCo.dat` import.

### Falcon data and qualified behavior

- [x] Import the complete 318-slot Falcon submotion catalog, all source action
  scripts, all translated root tracks, common character attributes, special
  attributes, stale-move table, and qualified attack/hurt geometry.
- [x] Qualify the currently registered `falcon-common-hurt` domain, including
  complete pose coverage and physical hit/miss discriminators.
- [x] Preserve deterministic replay/snapshot behavior for completed slices on
  Windows and WSL.
- [x] Import all 11 standing-turn and 22 running-turnaround hurt poses from a
  two-case live/control pack; retain source submotion in snapshots and register
  the 33-pose production accessor as its own stored-equivalence domain.
- [x] Preserve TurnRun's one-update gameplay-facing/display-facing split in
  world hurt collision and inspection by deriving it from existing canonical
  state; verify the adjacent phases with reusable stored pose-facing cases.

## Completed locally: native Battlefield playtest client

- [x] Replace the static M1 render-packet window with a direct public-API M4
  simulation session using the source-qualified Battlefield content
  constructor, allocation-query contract, deterministic reset, and fixed
  60 Hz stepping. The smoke-only path and its existing CI contract remain
  unchanged.
- [x] Add one allocation-free public stage-geometry view and render all 23
  imported Battlefield floor, ceiling, and wall lines. Render the exact blast
  bounds in an inset rather than inventing a second stage representation.
- [x] Render both fighters plus facing, damage, stocks, action/frame, crouch
  size/color cue, full/light shield bounds, active attack bounds, and a
  switchable collision inspector. The inspection schema exposes the same 11
  transformed source hurt capsules used by combat, so presentation has no
  duplicate pose transform. Add pause, single-step, reset, and resizable
  high-DPI window controls.
- [x] Add allocation-safe match setup through `pf_sim_default_config`, memory
  query, reinitialization, and reset: F2 atomically switches the only supported
  match pairs (2P Duel / 4P Teams), while F3 cycles 1-8 stocks. Screenshot QA
  confirms all four imported Battlefield spawn points and source hurt poses.
- [x] Reuse SDL's semantic Gamepad API for mapped devices and retain a narrow
  raw-joystick adapter for Mayflash PC mode. The real four-port adapter sample
  established the SDL-specific layout: main stick axes 0/1, C-stick 2/3, and
  L/R 4/5 at `-32768` neutral. This is intentionally separate from the Web
  Gamepad layout.
- [x] Bind Mayflash ports by first real input activity, so neutral empty ports
  cannot consume P1/P2. Once claimed, a port remains stable through neutral
  frames. Keyboard input remains an additive fallback for both players.
- [x] Pass strict Windows MSVC `/W4 /WX`, focused movement and combat suites,
  and the native SDL smoke. Direct window capture confirms the exact stage,
  both source spawn supports, HUD, blast inset, and stable idle state with the
  four-port adapter connected.
- [ ] Complete one hands-on GameCube controller pass covering both sticks,
  A/B/X/Y/Z, independent analog L/R, C-stick roll/attack, jump, shield,
  airdodge, pause/reset, and the crouch cue before marking this product gate
  complete.

## Completed locally: ordinary airborne submotion clock

- [x] Sweep pinned and current `doldecomp/melee` Fall/animation callbacks and
  Falcon's complete imported submotion catalog before changing runtime state.
  The relevant source files are unchanged at current upstream head, and no
  maintained reusable implementation was found.
- [x] Import `ftCommonData.x78` instead of authoring the backward-jump motion
  threshold. Preserve one compact source submotion per fighter while retaining
  the allocation-free public `AIRBORNE` action.
- [x] Advance JumpF/JumpB through their imported animation lengths into Fall,
  JumpAerialF/JumpAerialB into FallAerial, and wrap all ordinary Fall families
  at their imported eight-frame length. `action_ticks` is the exact zero-based
  source animation phase.
- [x] Extend canonical snapshot/hash/replay state by eight fixed bytes, reject
  invalid action/submotion/clock combinations, and migrate to state schema 64,
  save format 60, magic `PFSAVE54`, a 695-byte payload, and an 835-byte save.
  This is the verified historical checkpoint; the newer unverified static
  slice is recorded separately above.
- [x] Qualify the eight-frame Fall loop in the registered Hyrule theorem and
  compare the existing 1,250-frame Dolphin aerial-iasa capture. All 350
  JumpAerial/FallAerial action-frame samples pass strictly. WSL and Windows
  Release pass 27/27; WSL ASan/UBSan passes 20/20; native and Wasm replay are
  byte-identical; the browser adapter and Windows Chrome DOM/Wasm smoke pass.
- [x] Add the coalesced-public-action/source-submotion rule to the reusable
  character-importer skill and validate the skill package.

## Completed locally: deterministic prone response and accelerated live oracle

### Prior art and rejected paths

- [x] Sweep the pinned and current `doldecomp/melee` sources plus current
  libmelee before extending the oracle. No newer implementation replaces the
  existing source/action route.
- [x] Identify the cross-boot instability: `gmmain.c` initializes the shared
  HSD random seed from `OSGetTick()`, while high-knockback damage selection
  consumes `HSD_Randf()` and can choose `DamageFlyRoll`.
- [x] Make the source RNG seed an explicit manifest-owned checkpoint-pack
  event-boundary value, validate the live seed pointer, and read back the
  single pre-hit write while Dolphin is blocked on the corresponding input
  frame. This keeps source randomness visible and reproducible without
  resetting the stream on every emulated frame.
- [x] Prove two fresh independent Dolphin boots produce the same 2,370 rows,
  940 semantic samples, and
  `fc91d42660ac0a8df8f0715b183b2ec97bccfe2ee0279491cadf915e64044438`
  digest with the pinned seed. One transient checkpoint-host crash was retried
  successfully and remains a harness reliability item, not accepted output.
- [x] Reject the first combined 14-case pack despite correct route generation:
  response-only serialization reduced output to 1,515 rows, but replaying all
  physical setups serially still took 34.77 seconds end to end.
- [x] Reject the arbitrary three-slot branch-checkpoint experiment. Restoring
  at live action branches desynchronized host/game boundaries and was removed
  from the capture tool and reproducible Dolphin patch.
- [x] Sweep prior art before replacing it: libmelee supports independent
  Slippi ports, and multi-environment emulator workloads already use isolated
  Dolphin processes. The accepted design therefore shards real physical routes
  instead of fabricating source state.

### Accepted implementation and evidence

- [x] Add frame-safe `BATCH ON` pipe input. Queued FLUSH-delimited samples are
  consumed once per emulated frame; background controller polls are gated by
  ExiAI's `g_needInputForFrame` boundary.
- [x] Run four headless/null/unlimited Dolphin shards concurrently on distinct
  Slippi ports. Unique hardlinked process names let Dolphin Memory Engine hook
  the intended shard, and a generic merger restores manifest case order while
  checking disc, emulator, library, stage-collision, and probe provenance.
- [x] Qualify all 14 physical cases and 1,515 rows twice with semantic digest
  `db317711cb1a5b2c877d4dc8dd57e1ef38c31edd93638dd1d05e272b6d46cd8d`.
  A clean patch rebuild plus live-and-simulation gate passed in 9.812 seconds,
  below its 18-second guardrail. The first uncached concurrent run also exposed
  and fixed a process-shared SHA-256 cache temp-file race.
- [x] Capture and qualify Falcon's opposite prone orientation, including
  timeout, buffered getup attack, C-stick roll, and main-stick roll routes.
- [x] Implement the exact decomp distinction between semantic posture and roll
  motion: `ftCo_Down_CheckInput` chooses U/D roll motion from `DownWaitU`, so a
  roll selected directly from terminal `DownBoundU` intentionally uses the D
  root track and invulnerability table while retaining Back posture semantics.
- [x] Preserve this distinction in canonical snapshots without increasing the
  wire size by using previously unused bits in the prone/tech byte.
- [x] Pin the 14-case stored production trace digest
  `e4e6554506bf01ba299628a205dbe911db967e257812f3142225bd1afc606256`;
  the focused stored lane passes 168 selected production samples.
- [x] Pass all six stored domains and deterministic replay: 50 cases in
  606.209 ms on Windows and 672.759 ms on WSL.
- [x] Pass the full Windows Release suite 25/25 in 1.93 seconds, full WSL
  Release suite 25/25 in 1.44 seconds, and WSL ASan/UBSan suite 18/18 in
  12.04 seconds. The Windows SDL smoke also builds cleanly with the unnecessary
  `SDL_main` wrapper removed from the console executable.
- [x] Import and qualify `DownBound` ECB pose-grounding, including its observed
  4-contact / 18-no-contact / 4-contact sequence, without treating the action
  as ordinary airborne fall. Both orientations now consume one imported
  26-frame bit mask each, retain the source floor line and ground physics
  during the no-contact interval, and discard that retained support if
  airborne damage replaces DownBound.

Current DownBound prior-art/source sweep:

- The pinned decomp and current `doldecomp/melee` head have no relevant change
  in `ftCo_DownBound.c`, `ft_081B.c`, or `mpcoll.c`; current forks and issues
  expose no maintained reusable DownBound/ECB importer.
- `ftCo_DownBound_Phys` keeps ground-friction/root physics, while
  `ftCo_DownBound_Coll` runs `ft_80082708` over the JObj-derived ECB. The live
  action remains DownBound while the reported contact flag changes, so these
  frames must not be converted into ordinary airborne fall.
- Existing live memory evidence contains both Falcon orientations. Their ECB
  shapes differ, but both exact 26-frame contact schedules are frames 1-4 on,
  5-22 off, and 23-26 on. The canonical schedule-pair digest is
  `6c8d97ff1076075616ed06f88c742528eff9c2fb18ab9f2cce09ba895147e556`.
- Fresh identical-input qualification passes all 804 flat-floor rows / 232
  response samples with strict ECB contact comparison. The accelerated pack's
  warm capture took 3.032 seconds and the complete run took 6.065 seconds.
- The pinned ISO/DAT/JSON toolchain regenerates
  `m4_falcon_ntsc102_frame_data.inc` byte-identically at
  `d6e2700a293450bf8f8e5d075881e6284cf09bb56a41e68590a36d779f72004e`;
  its expanded complete-source digest is
  `7b34f2eb4e8edf0c491ea410c27b9790f2127d90a560865c56ebc68bea98c170`.
- The floor-response production trace is
  `bbf01b67f222d78e915f5077eecd9a45282f77e7d32efb4cf7bf8d79b513eb2f`;
  the two-orientation prone trace remains
  `dc416f87ccd228a045c61e87350af2619bad244e6a83dc0708502b414886fefb`.
- Final gates pass native Windows Release 25/25 in 3.90 seconds, WSL Release
  25/25 in 1.72 seconds, WSL ASan/UBSan 18/18 in 11.91 seconds, and all six
  stored domains / 50 cases plus replay in 0.744 seconds on Windows.

## Completed and verified: reusable paired-fighter player-push equivalence

- [x] Re-audit the existing 540-frame Final Destination Falcon-versus-Falcon
  route before writing new behavior. It already qualifies both horizontal
  directions and both controller ports, including strict action, facing,
  grounded state, and velocity plus the documented one-nudge Q16.16 position
  allowance.
- [x] Recheck pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` against current
  `doldecomp/melee` master `013091add6d46d2d809d163371deab97ab5e37eb`.
  `ftcommon.c` and `fighter.c` are unchanged for this route; Project Slippi
  and libmelee provide transport/observation but no maintained reusable
  player-push equivalence domain.
- [x] Replace the legacy monolithic comparator-only proof with manifest-owned,
  checkpoint-isolated paired cases and a shared paired numeric-trace runner.
- [x] Pin fresh live and production semantic digests, register the domain in
  the sub-two-second stored gate, and rerun Windows/WSL/replay validation.
- [x] Generalize numeric traces to one or two frame-major observation lanes and
  manifest-selected serialized fields. Existing one-lane byte streams and all
  five numeric production digests remain unchanged; repeated input phases are
  expanded only by the offline generator with compile-time lane counts.
- [x] Reproduce the 48-row / 96-lane source trace on three fresh Dolphin boots.
  Source digest
  `3c6ade86d516474c60b7559690b3b858f2b7a66b41982859e4f81df70a7c73f5`
  and production digest
  `079a34868db4fff30719d7a784d7bd102aab7a81acd189ad6293c65a9056bc7a`
  are pinned. Action, facing, and grounded state are strict; velocity allows 32
  Q16 units and position retains the reviewed 2,692-Q16 one-nudge envelope.
- [x] Run the final live pack in 0.090 seconds warm / 3.011 seconds end to end,
  the seven-domain stored gate in 0.802 seconds on Windows and 0.716 seconds in
  WSL, Windows Release 26/26 in 7.31 seconds, WSL Release 26/26 in 1.52
  seconds, and WSL ASan/UBSan 19/19 in 12.27 seconds.
- [x] Extend and validate the `ssbm-character-importer` skill with the reusable
  paired-lane/field-mask pattern.

## Completed and verified: imported Hyrule slope and ordinary ledge response

- [x] Sweep the pinned/current `doldecomp/melee` stage-collision, DownBound,
  damage-flight, and ledge routes before adding project machinery. No maintained
  reusable importer or exact-response harness covers this slice; ExiAI remains
  the qualified transport.
- [x] Import Hyrule MapCollData lines 34-37 into one immutable runtime catalog.
  Raw vertices are converted from joint-local to runtime world space, while the
  semantic source digest
  `4a0dd57bb8d9532589d3ecd129213d3a0876538a2dc7f733eca6c1e73c04db9c`
  pins topology independently of generated-C formatting.
- [x] Import Falcon's 24-frame DamageFly ECB-bottom track and source ledge-snap
  attributes. Production now projects landing knockback onto the imported slope
  tangent, evaluates DownBound contact before decay, retains support only until
  current and previous roots leave the endpoint, clears hitstun on the exact
  collision-driven Fall boundary, and performs ordinary ledge reach from source
  root/ECB coordinates.
- [x] Qualify two checkpoint-isolated physical cases: line-34 forward getup roll
  and line-36-to-line-37 natural hit departure through exact ordinary ledge
  catch. The live source digest is
  `8c62ce678732b38d157f1e3cee2409b0da22835bc63c906c41e765ce1a879a6d`;
  the production digest is
  `4dce7db6baa8a11fb90b77438ef5e423b500e5e5a3f7a4da835bfc979d6f0167`.
  Two fresh Dolphin boots reproduced the source digest. The final live gate
  measured 1.234 seconds warm and 4.859 seconds including process lifecycle.
- [x] Generalize stored numeric cases with an optional per-case field mask.
  Inherited masks emit an explicit zero initializer so MSVC and GCC
  `-Werror=missing-field-initializers` agree without runtime branching.
- [x] Requalify the damage and floor domains after correcting DamageFly entry
  grounding and landing ordering. Their production digests are
  `53471e3af8475959868a5f64361200151bc9fd9b640dbb5b3d66e4cd5031db4b`
  and `0596ba59ba2f03076b8b6828bae662eda6a7861ff8222af8788b47738aeece16`.
- [x] Repin deterministic replay only after the live-qualified production
  changes: 41,599 bytes, corpus
  `0d1c16c1e231d29c89a49d193f6b10deb081297821d5448239307cae4d33f4ad`,
  final state
  `6c648e4463b070ad4b7e3b013ea620e21463b281fe39b00980cf0cbf558bfcd5`,
  and events
  `0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e`.
- [x] Pass the eight-domain/54-case stored gate in 0.804 seconds on Windows and
  0.778 seconds in WSL; Windows and WSL Release pass 27/27, WSL ASan/UBSan
  passes 20/20, and the cross-platform verifier soak digest is
  `7f584c16f3d23773`.

## Completed locally: common ledge down-input rejection boundary

- [x] Re-sweep current and pinned `doldecomp/melee`, libmelee, Slippi, and
  Dolphin prior art. Current decomp changes do not touch `ftcliffcommon.c`,
  `mpcoll.c`, or the common-data layout; no maintained executable replacement
  covers this branch.
- [x] Import `PlCo.dat` common-data word `x480` (`0.6600000262260437`) as the
  immutable Q15 ledge-grab down-axis threshold and consume it in the shared
  ordinary-ledge predicate. Requested controller values are kept separate from
  the quantized values observed by Melee.
- [x] Add adjacent live controls: requested `-21400` is observed as source
  `-0.65` and catches, while requested `-21626` is observed as source
  `-0.6625000238` and remains in Fall. Both routes reuse the existing Hyrule
  damage/DownBound/endpoint fixture; no duplicate setup or runtime branch was
  added.
- [x] Reconcile the DownBound endpoint callback with its imported contact mask:
  the first contactless ECB frame consumes the current root, while later
  contactless frames retain the prior floor root. LedgeCatch entry now clears
  all knockback channels on the source transition.
- [x] Expand the checkpoint pack from 180 to 290 rows and the stored projection
  from two to four cases / 220 samples. Two fresh warm captures complete in
  2.716 and 2.493 seconds and reproduce semantic source SHA-256
  `9df8c72fca21359281d7d89391a9c363e08e6cf5c06db8873868e10521f27b49`;
  the reviewed production trace SHA-256 is
  `73f3dae4bf726aedd1e2ab37911818faa9b3fff4d1a19ed2a92a41148f142f5d`.
- [x] Express position allowance as base Q16 error plus at most the already
  qualified velocity error per integrated tick. This bounds fixed-point drift
  without widening a flat magic tolerance.
- [x] Reproduce the intentional seeded match-soak identity
  `52e8cab76719e97c` three times each on Windows and WSL before repinning it;
  all six runs report 8 matches, 2,848 ticks, and identical event counts.
- [x] Pass Windows GCC Release 28/28 in 4.15 seconds, WSL Release 30/30 in
  1.99 seconds, WSL ASan/UBSan 23/23 in 14.16 seconds, and the eleven-domain /
  79-case stored gate plus replay in 1.277/1.370 seconds on Windows/WSL.
  Regeneration, documentation, and importer-skill validation pass. The local
  pinned MSVC lane is unavailable because Visual Studio `vswhere.exe` is not
  installed; remote CI retains that compiler gate.

## Completed and verified: Falcon Dive action-specific ledge acquisition and aerial-catch geometry

- [x] Re-sweep pinned/current `doldecomp/melee`. The relevant
  `ftCa_SpecialHi.c`, `ft_081B.c`, `ftcliffcommon.c`, and `mpcoll.c` paths are
  unchanged at current head `6ddd74ecbb755df25b32f137b5f7b7f6d7005e91`.
- [x] Preserve the collision callback's direction policy as one zero-cost
  shared ledge-probe value: Falcon Dive start passes direction `0` and can
  acquire either endpoint after its imported command-variable gate; ordinary
  airborne/fall states remain facing-only. Falcon Dive throw no longer inherits
  a ledge query that its source callback never makes.
- [x] Apply the common catch transition's inward facing from the selected ledge
  endpoint. A deterministic physical route turns Falcon outward during Dive,
  catches the ledge behind him, and asserts inward facing plus zero velocity.
- [x] Preserve the existing 63-frame facing-toward Dolphin route: source still
  catches on frame 64, enters `EdgeHang` on frame 71, and production passes all
  63 comparable frames with the 640-Q16 envelope.
- [x] Capture and pin the facing-away route in headless/null/unlimited Dolphin.
  Source retains outward facing from Dive frame 13 through frame 63, catches
  on frame 64, turns inward, and enters `EdgeHang` on frame 71. All 63
  production samples pass within the 640-Q16 envelope; raw SHA-256 is
  `026faf91c3582aa5e41c5d95ba757904ec7ef7865a049994ce169f70a6157009`.
- [x] Repair the pre-existing aerial-catch validation lane. Bisect identifies
  `821fde3` as the first red commit: it correctly changed imported geometry from
  feet-origin to Melee reference-joint origin, while the source route pins both
  fighters at `y=500` and the native runner follows a different natural-jump
  trajectory. The repaired lane imports collision-authoritative JumpF frames
  9-20, replays exact source-relative placements, and uses a shared stored
  sphere-versus-pose predicate. Its pinned hit and translated miss margins are
  `+4.645228676` and `-1.151280430`; no production-origin rollback or
  character-specific collision routine was introduced.

## Completed and verified: distinct CliffCatch and CliffWait entry

- [x] Reuse the already-qualified Hyrule route and current/pinned decomp sweep;
  no redundant Dolphin boot or new character-specific capture lane was needed.
- [x] Import live-qualified absolute frame-one catch/wait roots, reuse decoded
  `TransN` deltas, preserve seven displayed catch frames, and enter wait only
  on source animation completion.
- [x] Expand the live comparison to 110 production samples with source digest
  `0b23132b7a217ff173397faf8ac9e59169092c99095b4b4e3fbd885526b7a3f3`
  and production digest
  `9c562426f42c4b01b08a7bbea9c667f56661a2787d107870a14208f326ccd94e`.
- [x] Validate catch in snapshot/hash and exclusive ledge ownership, and count
  its seven elapsed invulnerability frames without restarting at wait.
- [x] Pass WSL Release 27/27, Windows MSVC Release 27/27, WSL ASan/UBSan 20/20,
  the 0.605/0.805-second WSL/Windows stored gates, Emscripten rebuild,
  browser adapter, native/Wasm replay identity, and Windows Chrome DOM smoke.
- [x] Update provenance, fidelity audit, roadmap, and importer skill.

Remaining scope is explicit: ordinary Fall animation-clock equivalence,
later wait/ledge-option behavior and geometry, post-wall-contact absolute X,
broader stage topology, the other incomplete fidelity-audit rows, and the native
Battlefield frontend.

Primary sources:

- Owner `PlCo.dat` SHA-256
  `63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c`.
- `doldecomp/melee` revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`.
- `ftCo_Damage.c` for SDI, ASDI, and DI operation order.
- `fighter.c` for the later separate knockback-velocity decay channel.

Implemented and pushed in `b3edb14`:

- [x] Strict reusable HSD archive root/relocation validation in
  `tools/ssbm_dat.py`.
- [x] Separate character-independent `PlCo.dat` generator preserving all 518
  raw `ftCommonData` words plus typed damage-response fields.
- [x] Source-derived 18-degree DI limit, radial 0.7 SDI/ASDI threshold,
  four-frame SDI window, analog SDI/ASDI distances, and shield multiplier.
- [x] Shared allocation-free fixed-point DI and analog-displacement primitives.
- [x] C-stick ASDI priority over the main stick.
- [x] Rotate DI in Melee coordinate units before converting back through the
  project's anisotropic X/Y world scale.
- [x] Add an executable source-data check for all 518 words plus radial
  threshold, analog displacement, and squared-projection DI discriminators;
  focused Windows combat test passes with `ssbm_damage=1`.
- [x] Add source-derived discriminating cases for squared DI projection,
  threshold-adjacent radial SDI, analog displacement, and C-stick ASDI.
- [x] Capture and source-qualify a robust checkpointed live Falcon damage
  route.
- [x] Replay the same six input routes in the simulation and compare every
  hitlag/launch position, self velocity, knockback velocity, and timer against
  the live trace within a 0.001 Melee-unit Q16.16 envelope.
- [x] Keep the six-case live pack to 138 rows and a 2-second warm budget;
  measured warm capture is 0.671 seconds and its stable observation digest is
  `51402cd3605ba2761e3c11ed6baab74eb1b7ab22136822507b39d0a00cc40d95`.
- [x] Register the damage-response domain in the generic stored-equivalence
  lane.
- [x] Regenerate intentional replay/content hashes only after live
  qualification.
- [x] Run both stored-domain suites: Windows 205.990 ms and WSL 285.842 ms.
- [x] Complete the full affected Windows and WSL suites: Windows 21/21 in
  0.45 seconds and WSL Ubuntu 20/20 in 1.94 seconds. The verifier's new stable
  digest is `07cc4f4247d83066`; the synthetic browser ladder fixture explicitly
  retains its authored decay instead of inheriting imported Falcon data.
- [x] Pass WSL ASan/UBSan 16/16 after the final DI boundary review.
- [x] Commit and push the qualified slice to PR #3 as `b3edb14`.

Implemented and cross-platform verified locally after `b3edb14`:

- [x] Pin the decomp routes for grounded damage classification, Sakurai-angle
  handling, `xF0_ground_kb_vel`, friction decay, flat tangent projection, and
  the animation-plus-hitstun release boundary.
- [x] Capture a stable 64-row live late-DashAttack route in 2.801 seconds total;
  checkpoint restore is 0.011 seconds and the warm packed case is 0.128
  seconds.
- [x] Import the relevant `PlCo.dat` thresholds and ground-knockback constants
  rather than authoring replacements.
- [x] Add distinct `DamageLow1/2/3` runtime states and source submotion
  durations without expanding the canonical flat-stage snapshot payload.
- [x] Keep self velocity and ground knockback under separate deterministic
  state, including source-order friction decay and hitlag resume behavior.
- [x] Extend the generic numeric stored-oracle sample with action, resume
  action, grounded/tumble, damage, and ground-knockback fields; both prior
  domains continue through the same runner.
- [x] Register `falcon-common-ground-knockback` with one case and fifteen
  samples. Live-vs-sim comparison passes for every declared field; position is
  explicitly excluded because the chosen rows also exercise the separate
  player-pushbox domain.
- [x] Normalize only the attacker idle-loop phase that checkpoint restore does
  not freeze; two independent live captures now share observation digest
  `e08d7149e3f46d814d5c4a709e316cf3063208bb9673141effe6b1958f03fc79`.
- [x] Preserve the 807-byte canonical flat-stage save format by reconstructing
  `xF0` from the serialized flat-tangent `x8c` component; a mid-damage
  save/load continuation has exact state hashes and samples. State schema 61 /
  save format 57 fail closed on the new action semantics without adding bytes.
- [x] Clear the ground-only `xF0` scalar when a sliding fighter leaves a
  surface while retaining the projected airborne `x8c` velocity.
- [x] Review and repin the intentional deterministic replay identities after
  three identical runs; the root three-domain gate includes replay and passes
  27 cases in 0.305 seconds.
- [x] Pass the complete Windows release suite (21/21 in 1.13 seconds), WSL
  Ubuntu release suite (23/23 in 1.32 seconds), and WSL ASan/UBSan suite (16/16
  in 10.99 seconds).
- [x] Restore every standalone shell verifier's complete simulation source
  graph, then pass native/Wasm replay identity, the browser adapter verifier,
  and the full headless Chrome smoke with the repinned 81-event replay.
- [x] Commit and push this qualified slice to PR #3 as `9189aa0`.

## Completed locally: wall and ceiling damage response

Prior-art/source sweep completed before implementation:

- The pinned decomp routes are `ftCo_PassiveWall.c`,
  `ftCo_PassiveCeil.c`, `ftCo_FlyReflect.c`, `ftCo_Damage.c`, and the common
  collision callbacks in `ftcoll.c`; the public decomp and libmelee expose the
  same source actions `PassiveWall` 202, `PassiveWallJump` 203, and
  `PassiveCeil` 204.
- Falcon's already imported common-attribute words give passive-wall X speed
  0.5, wall-tech-jump X/Y 1.4/3.1, and passive-ceiling X speed 2.0. The current
  Q16.16 velocity constants are the exact project-unit conversions, so this
  slice must preserve them rather than replace them with new authored values.
- `PlCo.dat` words `x760=5` and `x764=14` feed the source wall-tech freeze and
  collision/invulnerability paths. Falcon's complete generated submotion
  catalog reports 26/40/26 endpoint counts for wall tech, wall-tech jump, and
  ceiling tech. Their exact observer-visible duration/order still requires a
  live route; the old 24/30 action durations and three-tick stall are not
  accepted as source truth merely because synthetic tests pass.
- Existing prior captures include Hyrule wall-collision experiments and the
  qualified Falcon Kick rebound route, but no normal-damage wall/ceiling
  tech-and-bounce oracle was found. The new route will reuse checkpointed
  headless Dolphin and manifest-driven numeric samples instead of creating a
  technique-specific test lane.

Execution results:

- [x] Add reusable, manifest-declared initial-state and collision-memory
  observations needed to place a physically launched fighter against a source
  wall or ceiling without modifying the response callbacks under test.
- [x] Capture wall tech, wall-tech jump, wall bounce, ceiling tech, and ceiling
  bounce across 719 source rows and 145 focused response observations. The
  accepted five-case live pack is 2.759 seconds warm and 5.493 seconds end to
  end.
- [x] Replace remaining placeholder timing/reflection behavior only from live
  observations plus pinned source/data, then register a generic stored domain.
- [x] Import `PlCo.dat` collision threshold `x1B0`, reflection multiplier
  `x1BC`, 15-frame reflection invulnerability `x1B8`, three-frame re-collision
  lock `x1C0`, five-frame wall freeze `x760`, and 14-frame wall-tech
  invulnerability `x764` without authored duplicates.
- [x] Preserve the source reflection action throughout hitstun; import
  31/45/26-tick wall-tech, wall-tech-jump, and ceiling-tech lifecycles from
  Falcon's submotion catalog plus the common five-frame freeze.
- [x] Match wall release 0.49/-0.13, wall-jump release 1.39/2.97, ceiling drift
  0.06 per tick, frame-11 1.99 control release, invulnerability boundaries,
  and preserved ceiling-tech hitstun within 0.0015 source units.
- [x] Register a five-case, 60-sample stored numeric domain. The root gate now
  runs four generated domains, 32 cases, and replay in 0.404 seconds on WSL and
  0.401 seconds on Windows.
- [x] Pass Windows 21/21, WSL 23/23, WSL ASan/UBSan 16/16, browser-adapter,
  collision-overlay, replay, and stored-oracle gates.
- [x] Update and validate the importer skill with the reusable runtime
  `MapCollData`, physical-waypoint, pre-contact trigger, response-field, and
  stage-geometry exclusion routine.

## Completed locally: flat-floor impact, missed tech, and tech response

Prior-art/source sweep completed before implementation:

- The pinned decomp route establishes the exact landing priority as directional
  tech roll (`ftCo_80098928`), neutral tech (`ftCo_8009872C`), then missed tech
  (`ftCo_80097D40`). No authored timing was accepted as source truth.
- `PlCo.dat` supplies the 20-frame tech window, 40-frame lockout, 0.2 roll-axis
  threshold, and 220-frame DownWait value. Falcon submotions supply 26-frame
  DownBound/neutral-tech and 40-frame directional-tech lifecycles plus the
  exact forward/backward `TransN` root tracks.
- Existing probes and tests did not form a pinned, same-input flat-floor domain;
  the new pack reuses the generic checkpoint, numeric trace, digest, and replay
  machinery.

Execution results:

- [x] Generalize the surface-response capture route once for both wall/ceiling
  and flat-floor manifests, including native collision during an X-only
  placement hold and a separately timed pre-contact trigger edge.
- [x] Import and validate the source tech window, lockout, direction threshold,
  and DownWait fields; content schema is now 74 and fighter schema is 67.
- [x] Preserve incoming self/knockback channels and stale hitstun memory on the
  landing callback frame, then project and decay ground knockback on the next
  tick in source order.
- [x] Replace the authored constant tech-roll speed with Falcon's imported
  forward/backward translation tracks through the existing allocation-free
  frame-data lookup.
- [x] Make snapshot validation distinguish active stun from Melee's retained
  hitstun memory so canonical hash/save validation accepts subsequent normal
  actions without weakening active-damage invariants.
- [x] Qualify four live cases over 804 rows / 232 focused observations. The
  stable semantic digest is
  `85fd93638bcb26b8b6e405cb1008a396acf05d132e07c7f9dcc3b6993034dd3f`;
  three independent captures agree after normalizing only landing-phase attack
  velocity and the two valid retained-hitstun magnitudes outside this domain's
  declared response semantics.
- [x] Compare 48 production samples on action/tick, invulnerability, preserved
  hitstun memory, and directional root translation within 0.0015 source units.
  Position remains assigned to stage/pushbox qualification. DownBound's source
  ECB contact toggles are now strictly qualified by the later pose slice.
- [x] Register the fifth stored domain. The root gate now covers 36 cases plus
  replay in 0.564 seconds on WSL and 0.543 seconds on Windows.
- [x] Pass WSL release 24/24 in 1.45 seconds, native Windows MinGW release 17/17
  in 1.05 seconds, focused WSL ASan/UBSan 4/4, and the browser adapter gate.
  The unavailable Visual Studio installation is not counted as a Windows
  result; the direct native Windows compiler lane is the recorded evidence.

## Completed locally: both prone/getup orientations

Prior-art/source sweep completed before implementation:

- The current `doldecomp/melee` head leaves the pinned DownBound, DownWait,
  DownStand, DownAttack, and input-helper callbacks unchanged. `libmelee`
  exposes the needed action labels but no replacement state semantics; the
  existing Slippi/ExiAI checkpoint transport therefore remains the reusable
  live-oracle path.
- Source option priority is buffered A/B or upward C-stick getup attack,
  horizontal C-stick edge or angle-qualified main-stick roll, then upward
  main-stick or fresh L/R neutral getup. Timeout selects neutral getup after
  the imported 220-frame DownWait timer.
- `PlCo.dat` owns the 0.2 main/C-stick magnitude threshold, 50-degree
  horizontal angle limit, 60-frame A/B buffer, and 0.6625 upward C-stick
  threshold. Falcon's generated catalog already contains the two 26-frame
  DownBound poses, two 70-frame DownWait poses, both 30-frame neutral getups,
  both 50-frame getup attacks, and all four 36-frame getup-roll `TransN`
  tracks.

Execution results:

- [x] Add one manifest-driven observation-segment primitive to the existing
  checkpoint route so delayed button/stick edges do not require another
  character-specific capture mode.
- [x] Capture and qualify the Down orientation across buffered A/B and upward-C
  getup attacks, main/C-stick rolls, neutral getup, option priority, timeout,
  exact action lengths, invulnerability, and root translation. Two independent
  2,370-row captures share semantic digest
  `fc91d42660ac0a8df8f0715b183b2ec97bccfe2ee0279491cadf915e64044438`.
- [x] Capture the opposite orientation and qualify posture-specific getup
  attack/neutral timing plus the source-selected roll motion and timing.
- [x] Implement and live-qualify the separately assigned DownBound ECB-driven
  contact transitions for both prone orientations.
- [x] Replace the authored getup thresholds, buffer, and constant roll speed
  only after live qualification; preserve the source input priority in one
  allocation-free common-state path.
- [x] Preserve raw A/B edges before projectile/item/special adapters consume
  effective inputs, pack C-stick edge state into the existing directional byte,
  and add only four bytes of canonical state for the combined A/B buffer age.
- [x] Register the resulting domain in the fast stored gate and rerun WSL,
  native Windows, sanitizer, browser, and replay validation.
- [x] Compare sparse production routes against independent live captures on
  action/frame, posture, invulnerability, option priority, roll direction, and
  root velocity within 0.0015 source units. Position remains an explicit
  separate pushbox/stage domain; DownBound contact is now strictly qualified.
- [x] Pass the latest WSL release 25/25 in 0.92 seconds, native Windows MinGW
  18/18 in 0.75 seconds, and focused WSL ASan/UBSan 5/5 in 6.80 seconds. The
  latest six-domain stored gate covers 46 cases and 120 prone samples in 0.465
  seconds on WSL and 0.628 seconds on Windows.

## Completed locally: quick/slow ledge-option lifecycle

- [x] Sweep pinned and current `doldecomp/melee` CliffWait, CliffClimb,
  CliffEscape, CliffAttack, and CliffJump callbacks plus maintained emulator
  harness prior art before extending the existing checkpoint architecture. No
  reusable exact ledge-option implementation replaces the pinned source path.
- [x] Capture quick and slow climb, roll, attack, and jump plus a drop probe in
  four concurrent headless/null/unlimited Dolphin shards. Moving the setup
  opponent away on the attack edge removes five unrelated hitlag holds from
  the action-owned trace. The clean pack contains 479 rows and measured 9.110
  seconds end to end under its 10-second guardrail.
- [x] Import exact CliffWait thresholds/timers, ledge roots, body-collision
  invulnerability, grounding frames, common jump impulse, and the Hyrule
  CliffJump wall-resolved path. Keep the latter as immutable qualified data so
  mirrored ledges share one allocation-free runtime path.
- [x] Preserve quick/slow source submotion identity through coalesced public
  ledge actions, including CliffJump1 to CliffJump2 and ordinary air physics.
  The compact neutral-ready latch reuses an existing serialized directional
  bit and does not increase snapshot or replay size.
- [x] Generalize numeric stored cases to optional per-case sample counts up to
  128 and teach the root verifier to sum those exact counts. Existing fixed-
  length domains retain byte-identical inputs and production digests.
- [x] Pin the clean live source digest
  `0882c32de5571a7fedec49d2b7e447bd46ccef930274b9603682239de57ce371`
  and production digest
  `3459f671fa8573a238b2d97bcec3a5fba1cb05a154b249cd02237cc2d1d88080`.
  All 8 cases / 450 samples pass on action, clock, facing, grounding,
  invulnerability, position, and velocity within the manifest-owned bounded
  Q16.16 envelope.
- [x] Register the focused CTest and ninth generic stored domain. The WSL
  focused test takes 0.02 seconds; all nine generated checks, 62 cases, and
  deterministic replay pass in 0.926 seconds. Full Windows, WSL, sanitizer,
  browser, and legacy-suite requalification remains the next gate.
- [x] Extend CliffWait qualification through the source IASA order: rising
  C-stick up attacks, rising inward C-stick rolls, jump, then the exclusive
  main-stick / drop-only C-stick direction router. Import `PlCo.dat` x7F8
  (0.6625) and x7FC (0.8) rather than reusing authored combat thresholds.
- [x] Add source-backed inward-roll, upward-attack, neutral-latched outward
  drop, main/C priority, held-before-ready, and sub-roll-threshold negative
  cases. The complete pack now contains 16 physical cases and 622 rows; its
  502 compared samples share source digest
  `6e404d19f093cb2bc082f8de21d6b10468550ce34d85949a084760423dbe2748`
  and production digest
  `64fcfecdb247d50a138021ceb316beb4380d176e8c99a186009f07d951e3d4a8`.
- [x] Make generated numeric domains own their exact total sample count, so
  variable-length additions cannot leave stale test reporting. The nine-domain
  gate now covers 70 cases in 0.943 seconds on WSL and 0.929 seconds on native
  Windows. Full release validation passes 28/28 on WSL in 0.49 seconds and
  26/26 on Windows in 3.15 seconds.
- [x] Bring the cold 16-case live pack back below its 10-second wall guardrail.
  EXI batching, a WSL-native pinned Python environment, and one preloaded
  parent forking six balanced capture workers reduce the complete pack from
  12.971 to 9.633 seconds. The generic runner validates an exact manifest case
  partition and keeps Dolphin executable names within Linux's 15-byte DME
  process-name limit. Eight Dolphins oversubscribe this 6-core host and are
  slower; three shards also lose to restore serialization.
- [x] Qualify both exact CliffWait timeout clocks from the internal
  `Fighter + 0x2344` motion variable rather than the looping displayed
  animation. The expanded 18-case pack contains 630 rows / 510 compared
  samples and pins source digest
  `9059582bb3abf01a99897935e9f27a6b78e2dec822936eec0adae62ed7b14f7e`
  plus production digest
  `0df9a4bbfc7f492e9c5e60f6e7d4a9af5358278768f55f959187d3b94ae48828`.
- [x] Preserve the source timeout transition into `DamageFall`/`TUMBLING`
  as the allocation-free public `AIRBORNE` state plus the existing tumble
  bit. The live comparison now checks that semantic bit explicitly while
  ordinary stick/C-stick drops remain non-tumbling.
- [x] Separate the live pack's warm execution budget from its honest cold
  startup budget. The final six-shard run passes at 10.666 seconds warm and
  11.036 seconds cold; no Python-import time is hidden. Full Windows Release
  passes 28/28 in 5.22 seconds and WSL Release passes 28/28 in 2.05 seconds.
- [x] Qualify the post-release ledge-regrab cooldown as the source-observable
  `29 -> 2 -> 1 -> 0` boundary. Production derives 29 from the imported
  30-frame common-data constant according to source callback ordering instead
  of carrying a second authored constant. The final 19-case pack contains 558
  rows / 514 compared samples and pins source digest
  `80c9ab75a98e2e7ef901055131b24f413c5e2672038315b526999f3143b38383`
  plus production digest
  `136f2c97c30bf041018014d0e1a5fb20caf0c942a77aca5a53ab465cd99adfcf`.
  A source-equivalent sparse timer jump, removal of unused observation tails,
  compact capture output, one inode-fingerprint hash per immutable oracle
  input, and one parent-side libmelee Dolphin-version probe shared by verified
  hardlinks reduce three independent complete runs to 9.649, 8.924, and 9.614
  seconds warm. Their cold lifecycle times are 9.969, 9.099, and 9.937 seconds,
  and all reproduce the same source digest. The manifest now enforces 10.0
  seconds warm and 10.75 seconds cold. The nine-domain gate now
  covers 73 cases in 0.908 seconds on WSL and 1.569 seconds on native Windows;
  full Release validation passes 28/28 in 1.96 and 4.37 seconds respectively.
  The final current-production verifier independently passes all 558 rows / 19
  cases / 514 samples against the latest simulator build with the same source
  digest.

## In progress: exact quick/slow ledge hurt poses

- [x] Reject ExiAI fast-forward pose capture after two clean runs preserved
  action, clock, and root state but produced different display-bone endpoints.
  Use checkpointed ExiAI input with fast-forward disabled and read all eleven
  collision-authoritative `FighterHurtCapsule` records from one contiguous
  fighter snapshot instead.
- [x] Reproduce the compact 450-row, eight-case capture byte-for-byte twice.
  The raw capture SHA-256 is
  `3055455eb02949e15c240f563a49648578b6c5affa4dc5dd7ca62f2c7b19c1e3`;
  its ten action tracks contain 434 exact displayed poses with semantic
  SHA-256
  `9125200e3e162822131fd8805ae1551371c4ebf0abc2256bba9a167cc181103a`.
- [x] Add one character-independent profile extractor and immutable C include
  generator. Falcon-specific code is limited to a data manifest and the
  zero-cost public-action/source-submotion binding; the shared pose lookup,
  world transform, combat collision, and inspection paths remain common.
- [x] Route quick/slow climb, roll, attack, and both jump phases through the
  generated 434-pose table. Strict Windows and WSL builds plus focused
  movement, combat, and deterministic replay tests pass.
- [x] Repin the intentional seeded match-soak identity after both Windows and
  WSL independently reproduced `52600d79f2b95349`; the prior digest did not
  include the newly routed ledge hurt geometry.
- [x] Extend the generic `falcon-common-hurt` stored-equivalence digest to all
  ten ledge tracks without copying their frame ranges. The generator consumes
  the hash-pinned profile, retains source-submotion identity, and hashes 689
  production poses under source/production digests
  `2aadf4b37b26796bdbc08fe026b234542f2c61914a4488e35e0dccd72a72e151` /
  `d691705692841bfabb8a2407ab31037bf398b097fc461574ecd07954e16a4331`.
  The complete eleven-domain plus replay gate passes in 1.12-1.14 seconds on
  Windows and 1.15-1.29 seconds in WSL.
- [x] Add the smallest source-qualified ledge hit/miss discriminator without
  inferring it from the exhaustive stored pose digest. Two independently
  launched two-shard captures are byte-identical at SHA-256
  `f31de47e694e46bf2269945747c97238ce443ddf88cbadc0a8e4214026f2785d`.
  Falcon Jab 1 frame 4 hits quick-climb frame 29 at the positive placement and
  misses 0.75 Melee units farther away. Reconstructed actual-pose margins are
  `+0.573037244` / `-0.098777672`; the generic rectangle remains a miss at
  `-1.895534515`. Semantic SHA-256 is
  `fbe0cf877402bf82aba10d8ae3dceecb4e431caa87d9b75b5844bfb7b132af2d`.
  Supplemental manifest projections and ordered semantic edges keep these two
  routes outside the ordinary 19-case ledge pack and the sub-two-second stored
  edit loop. The at-will live command passes in 6.07 seconds warm on this host.

## Completed locally: source-qualified Battlefield catalog and content

- [x] Sweep pinned `stage.c`, `ground.c`, `lbarchive.c`/JObj helpers, runtime
  `MapCollData`, and maintained stage-tooling prior art before extending the
  existing generic stage importer. `StageInfo` remains the authoritative
  runtime source for effective bounds and initial-player JObjs.
- [x] Extend the platform-only memory probe and schema-4 stage importer once,
  without a Battlefield-specific parser. The immutable catalog contains all
  23 dense collision lines (6 floors, 5 ceilings, 6 right walls, 6 left
  walls), runtime stage kind 36, four player points with floor supports, and
  effective camera/blast bounds after the source camera offset.
- [x] Pin the address-free semantic source digest
  `29525b7e0db4de8bf1a228f47e4216869ca362aff9d558a0c9ae81340103aa50`.
  Two independent 348-frame Dolphin captures regenerate the same compact JSON
  and generated C byte-for-byte; the second capture completed in 8.681
  seconds including process lifecycle.
- [x] Add one allocation-free reference-stage content constructor and O(1)
  line/spawn accessors. Battlefield content consumes imported blast bounds,
  uses imported floor/platform lines for its current stage primitives, and
  resets each fighter onto the floor directly below its corresponding source
  initial point. Pre-match entry/fall choreography remains outside this slice.
- [x] Make vertical arena validation symmetric so source-negative Battlefield
  top/camera coordinates remain valid through initialization and canonical
  state checks. One shared inline predicate covers fighters, items, and
  projectiles without runtime allocation or duplicated bounds rules.
- [x] Qualify catalog shape, exact transformed environment values, all four
  spawn/support mappings, content validation, and actual two-player reset.
  Focused combat passes in 0.40 seconds on WSL and 0.51 seconds with pinned
  native MSVC. Full-suite, browser, replay, and sanitizer qualification remains
  part of the final M4 gate.

## Remaining work, in priority order

### Falcon equivalence

- [x] Give open-air damage knockback its own source-equivalent velocity channel
  rather than folding it into ordinary self velocity.
- [x] Implement imported air magnitude decay in the exact source callback
  order and qualify the six-case open-air launch boundary live.
- [x] Qualify flat-ground knockback/friction decay and grounded damage-action
  release against a live source route.
- [x] Qualify wall and ceiling tech/bounce behavior against a live source route.
- [x] Qualify flat-floor landing plus missed, neutral, forward, and backward
  tech response against a live source route.
- [x] Qualify DownBound ECB pose-grounding in its own live source route.
- [x] Qualify imported Hyrule slope and ordinary ledge departure during floor
  recovery and promote the player-push route to the generic stored architecture.
- [x] Preserve all seven displayed `CliffCatch` frames, import absolute
  `CliffCatch`/`CliffWait` root anchors, and qualify the first wait frame rather
  than collapsing ledge acquisition directly into hang.
- [x] Preserve imported JumpF/JumpB and JumpAerialF/JumpAerialB clocks through
  their exact Fall/FallAerial successors; qualify the ordinary eight-frame
  Fall loop and the complete captured aerial-jump route.
- [x] Qualify quick/slow ledge climb, roll, attack, and two-phase jump through
  exact action clocks, grounding, invulnerability, Hyrule collision-adjusted
  roots, and common airborne physics.
- [x] Qualify neutral-latched main/C drop arbitration, main-versus-C-stick
  priority, threshold-adjacent negative inputs, and C-stick attack/roll edges.
- [x] Qualify the full 640/480-frame CliffWait timeout boundary, including
  its source `TUMBLING` exit.
- [x] Qualify the drop/timeout regrab-cooldown boundary.
- [x] Import and independently reproduce the complete Battlefield collision
  catalog, effective camera/blast bounds, and four initial-player points.
- [x] Import and independently reproduce CrouchWait's complete looping ECB,
  including canonical source-frame advancement and rollback retention.
- [ ] Replace remaining authored damage, hitstun, launch, collision, and input
  behavior with imported data or explicitly documented gaps.
- [ ] Convert each completed fidelity family into a generic manifest-driven
  live and stored domain.
- [ ] Exercise positive, threshold-adjacent negative, and entry/exit boundary
  routes for every primitive; do not add dedicated tests for emergent
  techniques.
- [ ] Close every incomplete or partial row in
  `docs/product/m4_ssbm_fidelity_audit.md`.

### Test infrastructure

- [x] Generalize checkpoint-pack route selection beyond the original
  common-hurt command-line mode.
- [x] Extend the generic stored C runner from geometry domains to numeric
  trace/transition domains without character-specific runner loops.
- [x] Keep changed-domain local validation comfortably below two seconds. The
  current nine-domain, 73-case stored gate is 0.904 seconds on WSL and 0.925
  seconds on native Windows. Live manifests carry explicit
  per-pack budgets: paired player push
  is 0.090 seconds warm; wall/ceiling is
  2.759 seconds warm; the larger 804-row floor pack measured 2.752 seconds on
  its final run with a four-second guardrail; the 1,515-row, 14-case prone pack
  is physically sharded and measured 9.812 seconds end to end with an
  18-second guardrail; the original 479-row ledge-option pack measured 9.110
  seconds. Its expanded timeout/regrab-qualified 558-row / 19-case pack passes
  three independent runs at 9.649, 8.924, and 9.614 seconds warm after sparse
  timeout acceleration, compact output, hardlink-aware source hashing, and one
  parent-side libmelee version probe for the immutable Dolphin inode.
- [x] Make the expanded ledge pack repeatably complete within ten seconds on
  this six-core host without loosening provenance or semantic coverage. The
  manifest enforces a 10.0-second warm guardrail and all three accepted runs
  reproduce source digest
  `80c9ab75a98e2e7ef901055131b24f413c5e2672038315b526999f3143b38383`.
  Six shards remain faster than seven or eight on this host.
- [x] Meet the affected-domain three-second warm target without source-invalid
  cross-invocation reconnection. The common-hurt route measures 2.635-2.729
  seconds warm, while each packed invocation keeps one Slippi observer attached
  across its checkpoint-isolated cases. A prototype that attached a new observer
  to an already-running process resumed a desynchronized event stream and is
  deliberately rejected; it is not part of the accepted architecture.
- [x] Maintain this roadmap and explicit coverage ledgers; no finite scenario
  may be described
  as detecting every possible anomaly.

### What a green equivalence result means

Each registered domain is a small executable theorem, not a universal anomaly
detector. Live headless Dolphin establishes pinned NTSC 1.02 observations;
the simulator adapter receives the corresponding inputs and compares only the
manifest-declared fields and tolerances; the approved production digest then
makes the ordinary no-Dolphin edit loop bit-exact. Replay hashes additionally
prove deterministic continuation, but do not establish Melee fidelity on their
own. Whole-Falcon equivalence is reached only by closing every fidelity-audit
row with positive, negative, threshold, entry, and exit coverage.

The reusable proof path is:

1. A pinned manifest declares the owner disc/revisions, route, cases, observed
   fields, tolerances, source files, expected digests, and time budget.
2. One live Dolphin process restores a known checkpoint for each independent
   route or branch, applies the declared inputs, and records source state.
3. A semantic source verifier also checks the relevant decomp code/data, so a
   coincidentally matching trace cannot silently replace the intended rule.
4. The simulator runner applies the corresponding inputs through shared C
   adapters and compares only the domain's declared state and geometry.
5. Once reviewed, generated stored cases and production digests make the fast
   Windows/WSL edit loop independent of Dolphin while preserving provenance.

This architecture catches regressions only inside registered domains. The
ground-knockback live route, for example, deliberately excludes position even
though flat-floor player push is independently registered; a green result must
not be reported as proof of unregistered slopes, ECBs, ledges, attacks, or
other stage/pushbox topologies.

### Native playtest frontend

- [x] Provide a validated source-derived Battlefield content constructor for
  the client, including current floor/platform primitives, blast bounds, and
  player reset supports.
- [x] Replace the M1 SDL render-packet spike with a real native simulation
  client.
- [x] Render the complete source Battlefield line catalog, fighters, stocks,
  damage, actions, hit/shield geometry, blast bounds, crouch, and essential
  state cues.
- [x] Expose and render the complete source hurt-capsule set in the native
  collision inspector instead of treating the fighter body cue as exact hurt
  geometry. The append-only inspection schema is now version 54; both native
  spawn poses expose all 11 ordered capsules and are covered by the focused
  combat gate.
- [x] Support keyboard and SDL GameCube-controller input with independent
  analog triggers and both sticks.
- [x] Provide reset, pause, frame-step, and collision-inspection controls.
- [x] Add native match setup without duplicating simulation configuration
  policy in the presentation layer.
- [x] Verify the native client on Windows and WSL/Linux: both full Release
  suites pass 28/28 in 3.35 and 1.92 seconds respectively after the current
  source-`Pass` qualification slice.
- [ ] Complete hands-on GameCube input qualification and preserve the existing
  cloud CI coverage when the branch is published; continue other work while
  CI runs.

### Importer skill

- [x] Add the reusable HSD archive and `ftLoadCommonData` routines discovered
  in the current slice to the personal `ssbm-character-importer` skill.
- [x] Document anisotropic world-coordinate conversion before DI/knockback
  vector operations.
- [x] Document separate self/knockback velocity channels, source callback
  ordering, and imported air-magnitude decay for reuse by later characters.
- [x] Document absolute animation-root placement for `CliffCatch`/`CliffWait`,
  body-center conversion, catch lifecycle qualification, and snapshot/ledge-
  ownership validation.
- [x] Document canonical source submotion retention when a target engine
  coalesces multiple Melee motions into one public action, including imported
  transition/loop lengths and directional predicates.
- [x] Document that a complete rendered line catalog is not a complete stage
  collision port: floor, ceiling, right-wall, and left-wall ranges need
  independent runtime routing, action-specific ECB timing, selected-line and
  normal evidence, and response qualification.
- [x] Document that `Pass`'s imported 30-frame submotion clock and common-data
  platform collision-skip timer are independent, plus the relocation method
  for capturing an animation ECB that natural landing would truncate.
- [x] Document exact sloped wall/ceiling qualification: complete top/side ECB
  schedules, earliest point/line intersection, pre-transform source normals,
  and the gravity-before-total-velocity reflection order. The personal skill
  validates after the update.
- [x] Validate the current skill update and exercise its `PlCo.dat` reader
  against the owner extract (145824-byte data block, 23 pointers).
- [ ] Forward-test a future character-style task when an update is substantial
  enough to merit it.

### Validated: Battlefield wall and ceiling routing

- [x] Sweep pinned and current `doldecomp/melee` `mpcoll.c`/fighter collision
  paths plus current libmelee before changing production. The relevant source
  files are unchanged; libmelee's projection helper explicitly omits ECB
  evolution and is unsuitable as an exact runtime oracle.
- [x] Replace the reference-stage central-rectangle wall and ceiling checks
  with allocation-free Q16.16 queries over the imported immutable line ranges.
  The same catalog now drives ordinary movement, hitlag/SDI rejection, and
  stationary wall-contact probing without duplicating stage geometry.
- [x] Exhaustively exercise the five ceiling and twelve wall catalog entries,
  motion-direction and geometry negative controls, plus representative
  production contact against Battlefield's central underside and side wall.
- [x] Bound the compact approximate-airborne platform compatibility path to
  the immediately preceding crossing, and bypass it for a source-qualified
  exact ECB schedule. A fighter placed fully below a pass-through line remains
  airborne instead of teleporting upward; imported JumpF lands on source frame
  95 without the compatibility path's extra tick.
- [x] Review the intentional seeded-soak ripple and repin only after three
  Windows and three WSL runs reproduced `70dda9b2e5d9d936` with identical
  8-match / 2,847-tick event counts, rollback, and replay results.
- [x] Complete Windows Release 28/28 in 1.74 seconds, WSL Release 28/28 in
  1.95 seconds, WSL ASan/UBSan 21/21 in 13.50 seconds, and both nine-domain
  stored-oracle lanes (73 cases plus replay) in 1.01-1.13 seconds. The rebuilt
  native smoke passes and the interactive client has been relaunched.
- [x] Add identical-input live Battlefield qualification for JumpF/Pass ECB
  contact timing, selected floor-line identity, and absolute vertical
  resolution through the production reference-stage content.
- [x] Add selected-normal and normal-based response qualification plus live
  Battlefield wall/ceiling routes. Two checkpoint-isolated cases select source
  line 10 / ceiling normal `(0.378117, -0.925758)` and line 15 / right-wall
  normal `(0.569210, -0.822192)`, then compare 24 response samples across
  action time, displayed frame, state, timers, self/knockback velocity, and
  relative position. Source and production digests are
  `8a0c463ffae10b1567815013c85c500bcb25869727874086c96d0e9c522a2f68`
  and `107ea657a7bad069ea8ee02cb98306dd116b78838c8e6899a4adf9ff6fcf0982`.
- [x] Replace DamageFly rectangle contact with its complete 24-frame top and
  side ECB schedule and an allocation-free earliest segment intersection.
  Reflect against the imported source-space unit normal after applying the
  frame's gravity to total motion; do not integrate remaining reflected motion
  on the contact tick. This closes the wrong-ceiling selection and the 5.6%
  horizontal wall-response error found by the numeric verifier.
- [x] Reproduce the source digest in a second fresh Dolphin process. The warm
  two-case pack takes 0.367 seconds and the complete launch/menu/capture cycle
  3.096 seconds.
- [x] Validate Windows Release 29/29 in 1.70 seconds, WSL Release 29/29 in
  1.89 seconds, WSL ASan/UBSan 22/22 in 13.86 seconds, and all ten stored
  domains / 75 cases plus replay in 1.03-1.05 seconds. The 348-frame production
  Battlefield route still passes on both hosts within 640 Q16 units. Three
  Windows and three WSL soaks reproduce digest `d3b4c23cb8a9dd7e` with the
  same 8-match / 2,847-tick outcomes before repinning.

### Validated: reflected-action ECB evolution and floor re-contact

- [x] Extend the checkpoint capture protocol with one-shot post-entry fighter
  velocity reset and synchronized fighter/collision position reset. This keeps
  the native wall/ceiling collision and action transition intact while moving
  only the later observation away from the blast zone and stage surfaces.
- [x] Capture the complete observable Falcon response clocks in one 0.739-second
  warm two-case pack: `BOUNCE_CEILING` frames 0..8 (frame 8 then remains held)
  and `BOUNCE_WALL` frames 0..50, with top, bottom, left, and right ECB points
  on every row.
- [x] Canonicalize the source poses facing-right and generate immutable full
  top/bottom/left/right Q16.16 tracks: nine ceiling poses and 51 wall poses.
  Raw/profile/semantic identities are
  `f1989a139185635d41d5cc2a51b0f88d41c1a26cf24c57fa82614feed6fda1c2`,
  `d6ccb5701f0bada0d7de1874004281e8ca46fcc0070db94e529d84d3fc637608`,
  and `9d162fe7917f0c23894ad1fe54a1a665d5c8e446d5ca439180811d706b2431a5`.
- [x] Route both actions through one allocation-free full-pose adapter shared
  with DamageFly. The real action clock continues while ceiling presentation
  clamps to pose eight; facing is applied only at the world-space wall side.
- [x] Live-qualify 111 focused updates from a native bounce entry through the
  first Battlefield top-platform re-contact. Ceiling lands on sample 57 and
  wall on sample 54 with source action, real clock, grounded state, hitstun,
  invulnerability, facing, relative position, self velocity, and knockback
  velocity all agreeing inside the existing 16/640-Q16 bounds. Source and
  production trace identities are
  `4e9a0ad3222bd0d6b6d7ab7def0177cf4b5c361bded3826abfe2e91f9210dd5a`
  and `222a5504d62bc5500e57a88a0adad108b931ea73d2b70cdf46faccde3f36d2db`.
- [x] Register the reusable numeric domain as the eleventh fast gate. Three
  warm Windows all-domain runs take 1.123-1.136 seconds; WSL takes 1.121
  seconds. Windows/WSL Release, deterministic replay, byte-identical Falcon
  regeneration, Python compilation, and WSL ASan/UBSan all pass. The focused
  CTest takes 0.03-0.04 seconds, and fresh Windows/WSL native-client smoke
  binaries pass without replacing the already-running Windows playtest.

### Validated: landing entry and source `Pass`

- [x] Follow `ftCo_Landing_Enter` through `ftCommon_8007D7FC` and preserve the
  incoming vertical self velocity on the `Landing` entry row; project it to
  zero on the next grounded-physics update, matching the saved source trace.
- [x] Follow `ftCo_8009A228`, `ftCo_Pass_Anim`, `ftCo_Pass_Phys`, and
  `ftCo_Pass_Coll`; retain raw submotion 209 inside the existing allocation-free
  public-`AIRBORNE` source-submotion adapter instead of adding a duplicate
  public action or schema field.
- [x] Capture all source `Pass` frames 0..29 by moving the fighter only after
  action entry. Pin raw SHA-256
  `0dc57f8ffb85549be76b3b5a0017690b0df16905456169eaceaa2e7975eedc0c`
  and semantic frame/ECB SHA-256
  `90060e614f359189c32b25d76b780b3fa92861dfdcfae0fd357dcc07ec10e6f8`;
  generate the complete immutable Q16.16 bottom-ECB table byte-identically.
- [x] Pass the full 348-frame identical-input Battlefield movement capture on
  Windows and WSL within the existing 640-Q16 position envelope. The former
  frame-276 divergence now lands on the same source frame with the same
  incoming vertical velocity.
- [x] Capture all 35 displayed JumpF ECB frames with the same post-entry
  relocation method. Pin raw SHA-256
  `28c4e902d8860f6d02ec779004c67c7ab94f87c7f3970699cfd9a44a8844cf1d`
  and semantic frame/ECB SHA-256
  `6db927d319942e07d90ba6dd30aad39ad40bb42ab3cc09d498ea2587bfe233bb`;
  use the exact table to remove the approximate crossing delay on source
  frame 32 and enter `Landing` on observed route frame 95.
- [x] Run the identical inputs through `pf_m4_reference_stage_content` instead
  of an authored vertical mimic and compare the selected imported support on
  every grounded source row. The left-platform phase selects source line 2
  (support 3), and the final Pass landing selects main-floor line 1 (support
  2), identically on Windows and WSL.
- [x] Repin downstream deterministic identities only after three Windows and
  three WSL runs reproduced verifier digest `3c3f20d38cee6e59` and identical
  8-match / 2,847-tick event counts. Replay final/event digests remain
  unchanged; its content-bearing corpus digest is now
  `02f52e1f9c9dbf29e21264c50d2139b8968c6bff810da3b30e00d9ba34fb2e0b`.
- [x] Complete Windows Release 28/28 in 1.91 seconds, WSL Release 28/28 in
  1.78 seconds, WSL ASan/UBSan 21/21 in 13.66 seconds, and both nine-domain
  stored lanes (73 cases plus replay) in 1.008-1.168 seconds under manifest
  SHA-256 `f16fa189ecb621a54c6ed4921aa920a257316b0fbeb869fb21caa08104ccefb3`.
- [x] Close live Battlefield selected-normal, normal-response, wall, and
  ceiling qualification through the dedicated two-case theorem above.
- [ ] Continue the remaining edge-acquisition/action-specific ECB audit. The
  special-callback inventory confirms Falcon Dive is the only Falcon special
  that calls the combined ground-and-ledge query, and its bidirectional route
  is already qualified. Ordinary down-input rejection, all five aerial-attack
  floor-contact tracks, both complete 26-frame DownBound ECB tracks, both
  70-frame DownWait loops, and all eight getup-option tracks are closed.
  Broader per-action ECB evolution remains open;
  passing selected floor/wall/ceiling routes does not claim whole-stage or
  whole-Falcon equivalence.

### Validated: reusable native-trace Raptor Boost oracle

- [x] Reuse `pf_m4_movement_trace` directly through the generic
  `native-csv-trace-v1` stored schema instead of cloning a Falcon-specific C
  adapter. Manifest input phases expand offline and compress to deterministic
  runs; the verifier parses only declared integer fields.
- [x] Preserve the live comparator boundary exactly with per-case field masks
  and half-open per-field sample exclusions. This retains every qualified
  action-tick row while excluding only hitlag-frozen and special-landing rows
  where the live comparator deliberately has no clock assertion.
- [x] Reuse one shared controller normalization and Raptor action/timer mapping
  in the live comparator and independent source projection. The source gate
  verifies that all 502 stored inputs are identical to the pinned captures.
- [x] Pin five fighter routes under source/production SHA-256
  `19b5d604d5721e20bc2151e41c11054632a5c384dfd5528cf373dac2bd1abe2c` /
  `7733655e234ac2de12fe1b674ed6be967ad7de39b848d94cf97b2e36547509a0`.
  The 155-frame native Capsule search remains a separate live-only branch
  because the project's Relay Rod intentionally is not a Melee Capsule.
- [x] Run independent native cases concurrently while retaining manifest order
  in the canonical digest. Six Windows all-domain runs take 1.435-1.550
  seconds and three WSL runs take 1.448-1.585 seconds for 14 domains / 89 cases
  plus deterministic replay, below the two-second edit-loop budget.
- [x] Register the focused Raptor stored gate in ordinary CTest/CI. Windows and
  WSL Release both pass 33/33 tests; the added cross-platform test takes 0.36
  seconds on each host.

### Validated: natural airborne callback order and complete landing ECB

- [x] Follow the pinned decomp's interrupt-to-physics update order. A double
  jump entered from an interrupt callback runs the new JumpAerial physics
  callback in the same fighter update, so its imported 0.9 horizontal impulse
  is immediately followed by ordinary air control. Production now performs
  both operations in that order without a character-specific state branch.
- [x] Import complete JumpF, JumpB, JumpAerialF, JumpAerialB, Fall, and
  FallAerial bottom-ECB tracks: 186 immutable Q16.16 poses from two
  byte-identical 494-row captures. Profile/semantic SHA-256 are
  `407a62269b2aa65002bb4a78152f12a49b56d36d8b68a684c6d55a11ce69a1ba` /
  `21a2d02fbb3abfcd9c29bb170c4c378fc8972fe191098fb5587140e965dac25a`.
- [x] Replay two independent 520-frame natural Battlefield captures through
  the same Windows and WSL production runner. Actions, clocks, facing,
  grounded/support state, surface normals, and velocities are strict; position
  uses only the existing 640-Q16 accumulated-conversion envelope. Source and
  production trace SHA-256 are
  `43eb893b7d70852b03b696c993db09e08ee4554bee5922c2a214aef73da7bf95` /
  `9e8fd5b0c3ee8d065c0fef6c906c9700d1600d6ef34e7eb498735a25ed81f26b`.
- [x] Register the route through the generic native-CSV stored kind. The
  fifteen-domain / 90-case gate plus deterministic replay passes three Windows
  runs in 1.592-1.678 seconds and three WSL runs in 1.279-1.578 seconds, below
  the two-second edit-loop budget. Byte-identical regeneration, Python syntax,
  focused movement/combat, and replay pass on both hosts.
- [x] Reproduce the first CI result locally: the headless lane's only failure
  was an incidental player-1 attack-recovery transition landing on the final
  player-0 KO tick after the corrected trajectory shifted that tick. The match
  test now settles the setup attacker before asserting the final-stock journal;
  all 27 headless tests and strict Windows MSVC pass.

### Validated: Falcon Kick stored oracle and edge-conversion ordering

- [x] Recheck current upstream decomp against pinned revision `9509dc0`; the
  Falcon Kick state table, attributes, and callback implementation are
  unchanged at current revision `6ddd74e`.
- [x] Reuse the generic `native-csv-trace-v1` runner and factor the live
  Falcon Kick action/timer mapping into shared pure projections. Six
  hash-pinned ground, air, landing, edge, hit, and Hyrule wall routes bind 399
  source samples under source/production SHA-256
  `2c6f28a9701990b913adb2f2daa214433bb18174a610af6c96fc1dce39deaf33` /
  `19a4dd302f0e51fa9d01d8fe7193d57e1b3ea5979e496fb6138bc0c85f356f4e`.
- [x] Let the live rerun reject a one-frame regression before accepting the
  stored digest. Ground-origin edge conversion now enters its aerial end state
  with zero self velocity and suppresses ordinary air physics on that update;
  all six live routes pass again on WSL.
- [x] Repin the intentional seeded verifier soak only after Windows and WSL
  independently reproduced 2,848 ticks and digest `b9239b63a68a1a18` for all
  eight twin/save-load/replay matches. The deterministic replay corpus remains
  unchanged.
- [x] Run independent registered domains concurrently, retaining deterministic
  manifest order for counts and the final digest. The complete 16-domain /
  96-case gate plus replay now takes 0.430-0.508 seconds on Windows and
  0.384-0.408 seconds in WSL, down from 1.758-1.819 seconds immediately before
  the domain-level concurrency refactor.

### Validated: all five aerial-attack ECBs and natural landing callbacks

- [x] Sweep current `doldecomp/melee` prior art before implementation. Pinned
  and current `ftCo_AttackAir_Coll` both route through floor collision without
  the ordinary Fall callback's `ftCliffCommon` branch, so aerial ledge catch is
  intentionally excluded rather than implemented from a false hypothesis.
- [x] Capture complete Nair/Fair/Bair/Uair/Dair ECB tracks twice without
  display-bone-skipping fast-forward. Both independent runs reproduce 195
  poses and semantic SHA-256
  `55e686a07cf3d064618104051f0085ed2a398e9a1612847200b2cba51a665f10`;
  production uses one packed bottom table while retaining the full profile as
  source evidence.
- [x] Preserve distinct previous and current action/submotion clocks when
  sweeping animated ECB endpoints. Reconstructing the previous endpoint from
  the post-callback pose can miss a crossing and drop the fighter through the
  platform.
- [x] Replay two independent 685-frame natural Battlefield captures. Five
  concurrent cases prove Nair/Fair/Dair landing-lag entry and Bair/Uair
  auto-cancel under source/production SHA-256
  `83e1fadc017af2c5005411ea2fae8d378855127662196fa0b9b81a37c7a11efe` /
  `dd14d194d15a115925c17761fd5fff692413b26a268ef96286180e176273e490`.
  Only each landing-entry row excludes libmelee's stale incoming self-Y field;
  action, elapsed clock, support, normal, and subsequent velocity remain exact.
- [x] Generalize the existing natural-movement verifier into one shared
  multi-case source projector. The 17-domain / 101-case gate plus replay takes
  0.575 seconds in WSL and 1.458 seconds on Windows. Movement, combat, replay,
  deterministic regeneration, and ASan/UBSan pass.
- [x] Repin the deterministic replay only after three identical reruns. The
  intentional floor-contact correction yields corpus/final/event SHA-256
  `0d3ccb293d0735102c13d020d469f13b202eede2b54052881d0380efb765e172` /
  `3a9bb1e28fd635dcde8f1ec98d0705babd12ee64ee7e036e8f986c5a15a874d5` /
  `370975f72bbd6546f5253607ef62b811cb4f126889ad3c89bf4b2955703430cb`.

## Completion gate

M4 is not complete until all three top-level deliverables are present and
verified: optimized reusable equivalence infrastructure, source-complete and
route-qualified Falcon behavior, and a playable native Battlefield frontend.
Passing one stored domain or one replay corpus is evidence only for its stated
coverage.
