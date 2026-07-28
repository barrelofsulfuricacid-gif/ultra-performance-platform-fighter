# M4 combat vertical-slice progress

**Status:** In progress; M4.1 movement/ledge core, first M4.2 attack and
hit-reaction layers, dense shield, and physical powershield cancel implemented

**Accepted baseline:** `5cfb263d9ba322da0bf330b75e3c7e656a15043a`

**Working branch:** `agent/m4-combat-vertical-slice`

## Delivered in the first M4 slice

- A validated, hash-identified `pf_m4_content` precursor containing one
  original placeholder fighter table and one original test-stage table.
- Real-simulation Q16.16 states for proportional walk, initial dash, run,
  dash-dance reversal, run turnaround, run brake, post-turnaround run lockout,
  facing, traction, crouch, jump squat, binary short/full hop, configured air
  jump, independently steerable aerial drift with takeoff-facing lock, fast
  fall, landing, moving-platform support, platform drop, support edges,
  blast-zone respawn, ledge catch, catch lockout, hang, release, ledge jump,
  and ledge climb.
- Deterministic one-fighter-per-ledge occupancy with stable lower-slot priority
  for simultaneous catches.
- A rollback-safe state-schema-6/save-format-5 contract that serializes every
  future-affecting movement, attack, hit-reaction, ground-tech, and current
  shield field.
- Replay format 1 regenerated against the new canonical state schema and real
  attack/hit inputs, with native and WebAssembly comparisons still using the
  same corpus path.
- A public `pf_m4_inspect` surface for movement state, active ledge claims,
  ledge points, moving-platform geometry, blast zones, percent, hitlag,
  hitstun, tumble, tech timers, SDI state, active hitbox bounds, and last-hit
  metadata, plus shield health/stun/powershield state.
- Thirty movement/content invariants plus a 20,000-tick four-player
  canonical-state trace under the active `M4-MECHANICS` verifier entry.
- A live two-player browser adapter that advances the production simulation at
  fixed 60 Hz, draws its inspected stage/player state, and supports pause,
  single-step, and reset.
- Explicit full-magnitude dash/dash-dance keys and reduced-magnitude walk keys
  for both keyboard players, with the real binary jump-squat selection rule.
- A native and Wasm startup contract that refuses readiness unless walk,
  dash-dance reversal, short/full-hop apex, real damage/hitlag, and
  reaction-input invariants pass.

## Delivered in the first M4.2 combat slice

- Input-schema-2 attack buttons for native, replay, RL, and browser callers.
- One original data-driven grounded attack with explicit startup, active, and
  recovery phases plus facing-mirrored hitbox geometry.
- Deterministic hurtbox overlap, one-hit-per-action masks, lower-slot
  same-target ownership, team friendly-fire rejection, and simultaneous
  trades.
- Q16.16 percent, percent-scaled launch, hitlag freeze, pending launch,
  hitstun control lockout, gravity/landing continuation, and blast reset.
- A monotonic rollback-safe combat-event sequence with per-target last-hit
  tick, attacker, and damage.
- Twenty-eight focused attack/reaction invariants, mid-hitlag save/load
  continuation, and a 20,000-tick four-player deterministic combat trace under
  active `M4-COMBAT` verification.
- Browser attack controls (`F` and `/` or Numpad `0`), active-hitbox overlay,
  percent/hitlag/hitstun inspection, and an independent combat startup probe.

## Delivered in the first hit-reaction slice

- Fixed-point trajectory DI read on the final hitlag tick, with a data-defined
  18-degree maximum and deterministic launch-speed renormalization.
- Component-edge SDI during target hitlag plus final-tick ASDI, with
  data-defined thresholds and distances and collision-safe positional shifts.
- Data-defined tumble threshold and explicit `KNOCKDOWN`, `TECH_IN_PLACE`, and
  `TECH_ROLL` action states.
- A rising analog-trigger edge opens the 20-tick tech window and 40-tick
  lockout; held input cannot retrigger it.
- Neutral and directional ground-tech outcomes on floor and pass-through
  platform contact, with explicit locked-action durations.
- Browser vertical DI and tech inputs, live tumble/SDI/tech inspection, and an
  independent startup probe that observes production-path SDI displacement
  and tech-timer behavior.
- Canonical replay inputs now exercise vertical axes and trigger edges and
  require observed SDI and tech-window state.

The exact first-primitive behavior and intentional remaining scope are fixed in
[`m4_combat_contract.md`](../product/m4_combat_contract.md).

## Delivered in the first shield slice

- A frame-1 full-density grounded shield using the existing normalized analog
  triggers, while preserving the trigger-edge tech-window contract.
- Melee-style run shield stop with retained momentum and traction, plus an
  explicit oracle that initial dash itself cannot enter shield.
- Data-defined 60 HP, 0.28/tick hold depletion, 0.07/tick regeneration,
  eight-tick minimum hold, 15-tick release, and jump out of shield/release.
- Physical block resolution with zero percent/launch, ordinary hitlag,
  damage-scaled shield health/stun, Melee-style attacker/defender pushback, and
  shield-stun continuation.
- A four-tick physical powershield window with no shield damage, ordinary
  hitlag/stun, larger Melee defender pushback, and inspectable result state.
- Melee-style physical powershield canceling: release by shield-stun end,
  reject attack on shield-drop frame 1, and accept a fresh ground-attack edge
  on frame 2. Ordinary blocks retain all 15 release ticks.
- A deterministic grounded shield-break lockout/reset foundation. The Melee
  launch, landing, vulnerable mashable stun, and interruption sequence remains
  explicit follow-up work; the placeholder lockout ignores further hitboxes.
- Browser shield bubbles, health/stun/powershield diagnostics, hold-to-shield
  controls on `G` and `.`/Numpad `1`, and an independent normal-block plus
  powershield startup probe.
- Mid-shield-hitlag save/load with equal future hashes and focused shield
  invariants inside the 20,000-tick combat verifier.

## Explicitly preserved playtest requirements

- Keyboard clients must emit reduced horizontal magnitude for slow walk and
  full magnitude for dash/dash-dance.
- Jump release during jump squat selects one short-hop speed; holding through
  jump squat selects one full-hop speed. Hold duration after launch does not
  change either height.
- Airborne horizontal input changes drift velocity but never changes facing;
  an opposite-direction air jump likewise preserves the takeoff-facing
  direction.
- Opposite input during initial dash remains a dash-dance reversal. Opposite
  input after entering `RUN` must instead enter `RUN TURNAROUND`; neutral or
  sub-threshold run input enters `RUN BRAKE`.

## New binding M4.4 scope

- The governing plan now pins and enumerates all 61 unique techniques marked
  available for SSBM in the referenced advanced-technique table.
- This first movement slice does not claim full technique parity. Dash-dancing
  has direct invariant coverage; other rows remain `planned` until their full
  movement, combat, item, team, or fighter-content dependencies are present.
- A versioned row-by-row registry, deterministic evidence links, and browser
  playtest recipes are required for all 61 rows before M4 can be accepted; none
  may be deferred to a later milestone.
- Registry schema 1 now exists at
  [`m4_advanced_technique_registry.md`](../product/m4_advanced_technique_registry.md)
  and is mechanically checked for all 61 ordered rows. Its current gate is
  blocked: 1 verified, 4 playable, 5 primitive-ready, and 51 planned.
- M4 must include narrow production-path item, team, projectile, charge,
  reflector-like, shield, grab/throw, aerial, and ledge fixtures wherever the
  non-character-specific registry needs them.
- Character-specific SSBM advanced techniques are a separate M8 fighter-wave
  gate and are not counted among these 61 M4 rows.

## Remaining M4.1 work

- Gamepad polling for the temporary browser presentation; two keyboard slots
  and their explicit walk/dash controls are implemented.
- Any stage wall/ceiling collision required by the final vertical-slice test
  geometry.

## Remaining M4.2 and M4.3 work

- Remaining ground attacks, aerials, specials, recovery, grabs/throws, analog
  light shield, shield size/tilt/pokes and shield SDI, defensive rolls, spot
  dodge, air dodge, platform shield drop, projectile powershield/reflection,
  expansion of the powershield-cancel router to each future ground action,
  complete shield-break behavior, complete
  knockback/angle data, stale-move behavior, wall/ceiling techs, missed-tech
  get-up choices, tech invulnerability, stocks, respawn invulnerability, match
  result, and the complete bounded combat-event journal.
- Local setup, complete 1v1 loop, results/rematch, replay visualization,
  collision/hitbox overlay, and repeated verifier/human matches.
- Representative M4 performance/profile evidence and the mandatory owner
  combat playtest.

## First-slice verification

- Release workflow: 16/16 tests.
- Address/undefined-behavior sanitizer workflow: 16/16 tests; leak discovery
  disabled only for the restricted workspace.
- Mechanical oracles: 30 movement invariants, 51 attack/reaction/shield
  invariants,
  and separate 20,000-tick deterministic four-player traces.
- M2 kernel compatibility: movement, snapshot, RL, replay, and forbidden-symbol
  checks passed after the state-schema migration.
- Native replay corpus: exact 180-tick attack/reaction/shield trace at 31,261
  bytes,
  replay SHA-256
  `628685db3a1ce96383608dc48f356346f8a0ddfc785a8fe0a00bb21c3977e3b3`,
  final SHA-256
  `d0d2eab988ab7c8597829297601d533c69259ee9f0f8203cd6c385d2ed20db17`;
  local native/WebAssembly output is byte-identical and CI repeats it.
- Clean Chrome CI remains the generated-Wasm, canonical replay-inspector, and
  live-playtest DOM gate.

## Browser-adapter verification

- Strict-warning native adapter contract: pass
  (`walk_axis=13500`, `dash_axis=32767`,
  movement/air-facing/combat/reaction/shield-and-PSC probes and live
  rendering).
- Address/undefined-behavior sanitizer adapter contract: pass.
- Emscripten 6.0.3 build and native/WebAssembly replay comparison: pass.
- Browser JavaScript syntax and M1 source-boundary checks: pass.
- Focused owner controls and expected results:
  [`M4_browser_playtest.md`](M4_browser_playtest.md).
- Generated-page execution: pass in the clean Chrome CI lane.

M5 content scaling remains blocked until M4 combat feel is approved.
