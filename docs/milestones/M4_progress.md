# M4 combat vertical-slice progress

**Status:** In progress; M4.1 deterministic movement core implemented

**Accepted baseline:** `5cfb263d9ba322da0bf330b75e3c7e656a15043a`

**Working branch:** `agent/m4-combat-vertical-slice`

## Delivered in the first M4 slice

- A validated, hash-identified `pf_m4_content` precursor containing one
  original placeholder fighter table and one original test-stage table.
- Real-simulation Q16.16 states for proportional walk, initial dash, run,
  dash-dance reversal, facing, traction, crouch, jump squat, binary short/full
  hop, configured air jump, aerial drift, fast fall, landing, moving-platform
  support, platform drop, support edges, and blast-zone respawn.
- A rollback-safe state-schema-2/save-format-2 migration that serializes every
  new future-affecting movement field.
- Replay format 1 regenerated against the new canonical state schema, with
  native and WebAssembly comparisons still using the same corpus path.
- A public `pf_m4_inspect` surface for movement state, ledge points,
  moving-platform geometry, and blast zones.
- Thirteen movement/content invariants plus a 20,000-tick four-player
  canonical-state trace under the active `M4-MECHANICS` verifier entry.

## Explicitly preserved playtest requirements

- Keyboard clients must emit reduced horizontal magnitude for slow walk and
  full magnitude for dash/dash-dance.
- Jump release during jump squat selects one short-hop speed; holding through
  jump squat selects one full-hop speed. Hold duration after launch does not
  change either height.

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

- Ledge occupancy, grab transitions, release, and the movement-side ledge
  states needed by M4.2 actions.
- Temporary browser presentation wired to this real simulation, with two
  keyboard/controller input slots and explicit walk/dash controls.
- Any stage wall/ceiling collision required by the final vertical-slice test
  geometry.

## Remaining M4.2 and M4.3 work

- Hitboxes, hurtboxes, offense, defense, grabs/throws, hitlag, knockback,
  hitstun, DI/SDI, teching, stocks, recovery, respawn invulnerability, match
  result, and rollback-safe combat events.
- Local setup, complete 1v1 loop, results/rematch, replay visualization,
  collision/hitbox overlay, and repeated verifier/human matches.
- Representative M4 performance/profile evidence and the mandatory owner
  combat playtest.

## First-slice verification

- Release workflow: 14/14 tests.
- Address/undefined-behavior sanitizer workflow: 14/14 tests; leak discovery
  disabled only for the restricted workspace.
- Mechanical oracle: 13 invariants and 20,000 deterministic four-player ticks.
- M2 kernel compatibility: movement, snapshot, RL, replay, and forbidden-symbol
  checks passed after the state-schema migration.
- Native/WebAssembly replay corpus: exact 180-tick match at 31,049 bytes.
- Local Chrome execution is unavailable in this workspace and remains selected
  for the clean browser CI lane.

M5 content scaling remains blocked until M4 combat feel is approved.
