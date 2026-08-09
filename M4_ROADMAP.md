# M4 execution roadmap

Last updated: 2026-08-09

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
| Fast stored equivalence | done for six domains | Common hurt, open-air damage, flat-ground knockback, wall/ceiling response, flat-floor response, and prone/getup response now contain 50 registered cases plus deterministic replay. Current cross-platform timings are requalified after each affected slice. |
| Fast live Dolphin oracle | done for registered domains | Registered packs use headless/null/unlimited ExiAI. Frame-safe batched controller queues and four isolated physical Dolphin shards reduce the 14-case prone pack from 34.77 seconds to 9.812 seconds while preserving the independently reproduced 1,515-row digest. |
| Falcon common hurt poses | done | 255 source poses, eleven capsules per pose, runtime/source mappings and live Dash hit/miss discriminators qualified. |
| Falcon movement and combat | partial | Captured routes include wall/ceiling response, flat-floor missed/neutral/directional techs, and both Up/Down prone/getup orientations. Slopes, ECB pose behavior, pushboxes, and other fidelity-audit rows remain. |
| Common damage response | qualified for open-air launch | Six-case live Dolphin trace and generic stored numeric trace pass. Ground and collision response remain separate domains. |
| Separate knockback velocity and decay | open-air and flat-ground routes qualified | A pinned 64-row late DashAttack route agrees for 15 damage samples on action/frame, grounded/tumble, damage, timers, self velocity, projected knockback, and `xF0_ground_kb_vel` within 0.001 source units. Canonical save/load, replay, Windows, WSL, and sanitizers pass. |
| Remaining Falcon gaps | not complete | Work through every incomplete row in `docs/product/m4_ssbm_fidelity_audit.md`; do not infer whole-character equivalence from one domain. |
| Native Battlefield frontend | not complete | Existing SDL target is only an early render-packet spike, not the M4 playtest client. |
| Character-importer skill | active | The skill records reusable HSD/PlCo import, damage-channel, callback-order, ground-projection, save/load, action-release, physical surface-route, lifecycle, and semantic-digest guidance. |

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
- [ ] Qualify slopes, ledge departure during floor recovery, and player-pushbox
  interactions in their own live source routes.
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
  current six-domain stored gate is 0.465 seconds on WSL and 0.628 seconds on
  Windows. Live manifests carry explicit per-pack budgets: wall/ceiling is
  2.759 seconds warm; the larger 804-row floor pack measured 2.752 seconds on
  its final run with a four-second guardrail; the 1,515-row, 14-case prone pack
  is physically sharded and measured 9.812 seconds end to end with an
  18-second guardrail.
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
ground-knockback live route, for example, deliberately excludes position while
pushbox behavior is still a separate open domain; a green result must not be
reported as proof of unregistered slopes, ECB, ledges, attacks, or pushboxes.

### Native playtest frontend

- [ ] Replace the M1 SDL render-packet spike with a real native simulation
  client.
- [ ] Render Battlefield, fighters, stocks, damage, hit/hurt/shield/debug
  geometry, and essential state cues.
- [ ] Support keyboard and SDL GameCube-controller input with independent
  analog triggers and both sticks.
- [ ] Provide reset, pause, frame-step, match setup, and collision-inspection
  controls needed for fidelity playtesting.
- [ ] Verify the native client on Windows and WSL/Linux; preserve CI cloud
  coverage and continue other work while CI runs.

### Importer skill

- [x] Add the reusable HSD archive and `ftLoadCommonData` routines discovered
  in the current slice to the personal `ssbm-character-importer` skill.
- [x] Document anisotropic world-coordinate conversion before DI/knockback
  vector operations.
- [x] Document separate self/knockback velocity channels, source callback
  ordering, and imported air-magnitude decay for reuse by later characters.
- [x] Validate the current skill update and exercise its `PlCo.dat` reader
  against the owner extract (145824-byte data block, 23 pointers).
- [ ] Forward-test a future character-style task when an update is substantial
  enough to merit it.

## Completion gate

M4 is not complete until all three top-level deliverables are present and
verified: optimized reusable equivalence infrastructure, source-complete and
route-qualified Falcon behavior, and a playable native Battlefield frontend.
Passing one stored domain or one replay corpus is evidence only for its stated
coverage.
