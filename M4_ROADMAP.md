# M4 execution roadmap

Last updated: 2026-08-08

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
| Fast stored equivalence | done for three domains | `falcon-common-hurt`, `falcon-common-damage-response`, and flat-ground knockback: 27 cases plus deterministic replay in 0.305 seconds on Windows. |
| Fast live Dolphin oracle | done for `falcon-common-hurt` | One headless/null/unlimited ExiAI process, eight checkpoint-isolated cases, 283 rows, warm 2.635-2.729 s. |
| Falcon common hurt poses | done | 255 source poses, eleven capsules per pose, runtime/source mappings and live Dash hit/miss discriminators qualified. |
| Falcon movement and combat | partial | Many captured routes pass, but the fidelity audit still lists unqualified combinations and incomplete damage/knockback behavior. |
| Common damage response | qualified for open-air launch | Six-case live Dolphin trace and generic stored numeric trace pass. Ground and collision response remain separate domains. |
| Separate knockback velocity and decay | open-air and flat-ground routes qualified | A pinned 64-row late DashAttack route agrees for 15 damage samples on action/frame, grounded/tumble, damage, timers, self velocity, projected knockback, and `xF0_ground_kb_vel` within 0.001 source units. Canonical save/load, replay, Windows, WSL, and sanitizers pass. |
| Remaining Falcon gaps | not complete | Work through every incomplete row in `docs/product/m4_ssbm_fidelity_audit.md`; do not infer whole-character equivalence from one domain. |
| Native Battlefield frontend | not complete | Existing SDL target is only an early render-packet spike, not the M4 playtest client. |
| Character-importer skill | active | The skill now records reusable HSD/PlCo import, damage-channel, callback-order, ground-projection, save/load, and action-release guidance from this slice. |

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

## In progress: common damage and ground response

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
- [ ] Commit and push this qualified slice to PR #3.

## Remaining work, in priority order

### Falcon equivalence

- [x] Give open-air damage knockback its own source-equivalent velocity channel
  rather than folding it into ordinary self velocity.
- [x] Implement imported air magnitude decay in the exact source callback
  order and qualify the six-case open-air launch boundary live.
- [x] Qualify flat-ground knockback/friction decay and grounded damage-action
  release against a live source route.
- [ ] Qualify wall, ceiling, floor landing, tech, bounce, getup, slopes, and
  player-pushbox interactions against their own live source routes.
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

- [ ] Generalize checkpoint-pack route selection beyond the original
  common-hurt command-line mode.
- [x] Extend the generic stored C runner from geometry domains to numeric
  trace/transition domains without character-specific runner loops.
- [x] Keep changed-domain local validation comfortably below two seconds and
  warm live changed-domain validation below three seconds where the manifest
  declares that budget. Current full stored gate is 0.305 seconds; the ground
  live pack is 0.128 seconds warm and 2.801 seconds end to end.
- [ ] Maintain explicit coverage ledgers; no finite scenario may be described
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
