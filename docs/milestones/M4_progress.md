# M4 combat vertical-slice progress

**Status:** In progress; M4.1 movement/ledge core and first M4.2 combat primitive implemented

**Accepted baseline:** `5cfb263d9ba322da0bf330b75e3c7e656a15043a`

**Working branch:** `agent/m4-combat-vertical-slice`

## Delivered in the first M4 slice

- A validated, hash-identified `pf_m4_content` precursor containing one
  original placeholder fighter table and one original test-stage table.
- Real-simulation Q16.16 states for proportional walk, initial dash, run,
  dash-dance reversal, run turnaround, run brake, post-turnaround run lockout,
  facing, traction, crouch, jump squat, binary short/full hop, configured air
  jump, aerial drift, fast fall, landing, moving-platform support, platform
  drop, support edges, blast-zone respawn, ledge catch, catch lockout, hang,
  release, ledge jump, and ledge climb.
- Deterministic one-fighter-per-ledge occupancy with stable lower-slot priority
  for simultaneous catches.
- A rollback-safe state-schema-4/save-format-3 contract that serializes every
  future-affecting movement and first-combat-primitive field.
- Replay format 1 regenerated against the new canonical state schema and real
  attack/hit inputs, with native and WebAssembly comparisons still using the
  same corpus path.
- A public `pf_m4_inspect` surface for movement state, active ledge claims,
  ledge points, moving-platform geometry, blast zones, percent, hitlag,
  hitstun, active hitbox bounds, and last-hit metadata.
- Twenty-six movement/content invariants plus a 20,000-tick four-player
  canonical-state trace under the active `M4-MECHANICS` verifier entry.
- A live two-player browser adapter that advances the production simulation at
  fixed 60 Hz, draws its inspected stage/player state, and supports pause,
  single-step, and reset.
- Explicit full-magnitude dash/dash-dance keys and reduced-magnitude walk keys
  for both keyboard players, with the real binary jump-squat selection rule.
- A native and Wasm startup contract that refuses readiness unless walk,
  dash-dance reversal, short/full-hop apex, and real damage/hitlag invariants
  pass.

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
- Fourteen focused combat invariants, mid-hitlag save/load continuation, and a
  20,000-tick four-player deterministic combat trace under active
  `M4-COMBAT` verification.
- Browser attack controls (`F` and `/` or Numpad `0`), active-hitbox overlay,
  percent/hitlag/hitstun inspection, and an independent combat startup probe.

The exact first-primitive behavior and intentional remaining scope are fixed in
[`m4_combat_contract.md`](../product/m4_combat_contract.md).

## Explicitly preserved playtest requirements

- Keyboard clients must emit reduced horizontal magnitude for slow walk and
  full magnitude for dash/dash-dance.
- Jump release during jump squat selects one short-hop speed; holding through
  jump squat selects one full-hop speed. Hold duration after launch does not
  change either height.
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

- Remaining ground attacks, aerials, specials, recovery, grabs/throws, shields,
  shield stun/damage, rolls, spot dodge, air dodge, launch angles, DI/SDI,
  stale-move behavior, teching, knockdown, stocks, respawn invulnerability,
  match result, and the complete bounded combat-event journal.
- Local setup, complete 1v1 loop, results/rematch, replay visualization,
  collision/hitbox overlay, and repeated verifier/human matches.
- Representative M4 performance/profile evidence and the mandatory owner
  combat playtest.

## First-slice verification

- Release workflow: 16/16 tests.
- Address/undefined-behavior sanitizer workflow: 16/16 tests; leak discovery
  disabled only for the restricted workspace.
- Mechanical oracle: 26 invariants and 20,000 deterministic four-player ticks.
- M2 kernel compatibility: movement, snapshot, RL, replay, and forbidden-symbol
  checks passed after the state-schema migration.
- Native replay corpus: exact 180-tick combat trace at 31,193 bytes; fresh
  WebAssembly byte comparison remains mandatory in CI.
- Clean Chrome CI executes the generated Wasm, canonical replay inspector, and
  live playtest DOM.

## Browser-adapter verification

- Strict-warning native adapter contract: pass
  (`walk_axis=13500`, `dash_axis=32767`, movement/combat probes and live
  rendering).
- Address/undefined-behavior sanitizer adapter contract: pass.
- Emscripten 6.0.3 build and native/WebAssembly replay comparison: pass.
- Browser JavaScript syntax and M1 source-boundary checks: pass.
- Focused owner controls and expected results:
  [`M4_browser_playtest.md`](M4_browser_playtest.md).
- Generated-page execution: pass in the clean Chrome CI lane.

M5 content scaling remains blocked until M4 combat feel is approved.
