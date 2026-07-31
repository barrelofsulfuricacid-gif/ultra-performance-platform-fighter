# TDR-0006: Canonical state format and hash

- **Status:** Accepted for save formats 1–19 / state schemas 1–20
- **Date:** 2026-07-28

## Decision

Save formats are fixed, field-by-field little-endian encodings:

| Format | State schema | Header | Payload | Total | Added state |
|---|---:|---:|---:|---:|---|
| 1 | 1 | 140 | 165 | 305 | M2 match and basic-motion state |
| 2 | 2 | 140 | 217 | 357 | M4 action timers, respawn count, support, air-jump/short-hop/drop/fast-fall flags, facing, dash direction, and prior strong direction |
| 2 | 3 | 140 | 217 | 357 | Canonical ledge-hang, ledge-climb, run-turnaround, and run-brake action IDs; no byte-layout change |
| 3 | 4 | 140 | 361 | 501 | Damage, pending launch, sequenced last-hit metadata, hitlag/hitstun, resume action, and attack hit masks |
| 4 | 5 | 140 | 401 | 541 | Tech window/lockout and trigger-edge state, tumble, SDI component/count state, and tech-roll direction |
| 5 | 6 | 140 | 429 | 569 | Shield health, shield-stun timer, and powershield result state |
| 6 | 7 | 140 | 429 | 569 | Canonical strong-ground-attack action ID; no byte-layout change |
| 7 | 8 | 140 | 429 | 569 | Canonical missed-tech down-wait, neutral-getup, getup-roll, and floor-attack action IDs; no byte-layout change |
| 8 | 9 | 140 | 429 | 569 | Canonical wall-tech, wall-tech-jump, ceiling-tech, wall-bounce, and ceiling-bounce action IDs plus solid-top support ID; no byte-layout change |
| 9 | 10 | 140 | 429 | 569 | Canonical air-dodge, special-fall, and special-landing action IDs; no byte-layout change |
| 10 | 11 | 140 | 433 | 573 | Canonical aerial-attack, aerial-landing, and L-cancel-landing action IDs plus one fresh-trigger age byte per player |
| 11 | 12 | 140 | 437 | 577 | Canonical forward-roll, backward-roll, and spot-dodge action IDs plus one fresh-down history byte per player |
| 12 | 13 | 140 | 437 | 577 | Canonical strong-aerial-attack, strong-aerial-landing, and strong-L-cancel-landing action IDs; no byte-layout change |
| 13 | 14 | 140 | 463 | 603 | Match stock count, respawn-delay and invulnerability rules, sudden-death state, per-player stocks/timers, and respawn-wait/eliminated action IDs |
| 14 | 15 | 140 | 463 | 603 | ABI-4 typed per-tick event journal and authoritative event-sequence semantics; no payload-layout change |
| 15 | 16 | 140 | 463 | 603 | Canonical shield-break flight/down/stand/stun action semantics; no payload-layout change |
| 16 | 17 | 140 | 471 | 611 | One canonical remaining ledge-invulnerability timer per player |
| 17 | 18 | 140 | 479 | 619 | One canonical remaining ledge-regrab-lockout timer per player |
| 18 | 19 | 140 | 495 | 635 | One grab-escape timer, one grab-target slot, and one grab-owner slot per player |
| 19 | 20 | 140 | 495 | 635 | Canonical forward/back/up/down throw action IDs, hitlag-resume and reciprocal-link release semantics, and typed throw events; no payload-layout change |

The header magic is `PFSAVE01`, `PFSAVE02`, `PFSAVE03`, `PFSAVE04`, or
`PFSAVE05`, `PFSAVE06`, `PFSAVE07`, `PFSAVE08`, `PFSAVE09`, `PFSAVE10`, or
`PFSAVE11`, `PFSAVE12`, `PFSAVE13`, `PFSAVE14`, `PFSAVE15`, `PFSAVE16`, or
`PFSAVE17`, `PFSAVE18`, or `PFSAVE19`.
The active M4 runtime emits and accepts format 19 with state schema 20. Earlier
schemas and formats remain documented as historical evidence rather than
being silently converted. The
configuration identity is SHA-256 over the domain `PFCFG001` followed by the
canonical configuration fields. The payload checksum is SHA-256 over the exact
payload bytes. `pf_sim_hash` is SHA-256 over the complete emitted save stream
and reports both its algorithm and algorithm version.

Load parses into a temporary fixed-size world value, validates the complete
header, lengths, compatibility identity, checksum, schema fields, enum values,
flags, player slots, teams, numeric ranges, and inactive-slot invariants, and
only then replaces live state. A failed load therefore leaves the destination
unchanged. Clone applies the same content/configuration compatibility gate and
copies state without serialization.

The per-tick event array is transient output and is deliberately absent from
the save payload. The existing canonical event sequence remains serialized.
After load, identical subsequent inputs must therefore reproduce the same
typed events and sequence IDs. The format/state bump makes that new semantic
contract fail closed even though the 463-byte payload layout is unchanged.
Format 15 applies the same fail-closed rule to the new shield-break action IDs
and action-timer semantics: no new mutable field is needed, but a format-14
reader must not silently reinterpret down/stand elapsed ticks or stun
remaining ticks.
Format 16 adds four little-endian 16-bit ledge-invulnerability timers. A timer
is refreshed by a legal ledge catch, survives ledge release and jump, counts
down once per simulation tick, rejects production hit ownership while
nonzero, and remains canonical across save/load and rollback.
Format 17 adds four little-endian 16-bit ledge-regrab-lockout timers. Ordinary
ledge release sets the data-defined 29-tick disabled-regrab period; the release
frame and the following 28 movement resolutions cannot catch either ledge,
while the next resolution may catch if the ordinary position, velocity,
facing, and occupancy rules also pass. The lockout is independent of the
pass-through-platform timer, counts down in canonical time, clears on
reset/respawn, and remains deterministic across save/load and rollback.
Format 18 appends four little-endian 16-bit grab-escape timers, four one-byte
grab-target slots, and four one-byte grab-owner slots. Slot value 0 encodes no
link and values 1–4 encode player index plus one. Loading rejects out-of-range,
self, same-team, non-reciprocal, inactive, or action-incompatible links.
Capture, natural escape, mash escape, interruption, reset, respawn, and stock
loss update both sides atomically, so mid-hold rollback cannot create a
one-sided capture or duplicate its typed event.
Format 19 retains the 495-byte payload while making four directional throw
actions and their timing semantics fail closed. A throw startup keeps the
existing reciprocal grab link until its authored release tick; release clears
both sides atomically, applies damage and signed launch through the ordinary
hit-reaction/DI/SDI path, freezes both players in hitlag, resumes the thrower
into authored recovery, and emits one typed throw event. Loading accepts live
links during throw startup only when they remain reciprocal and action
compatible, and accepts post-release throw recovery only after both links have
cleared.

## Why SHA-256

SHA-256 has a stable public specification in
[NIST FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final), produces a
full 256-bit digest, and can be implemented with fixed-width integer operations
without a platform or allocation dependency. That makes format compatibility
and native/WebAssembly behavior straightforward.

XXH3-128 from xxHash 0.8.3 was also reviewed. It is a strong candidate if
measured replay-verifier hashing cost becomes material, but adopting it now
would add a third-party source and update surface to the deterministic target.
The versioned hash identifier permits a future format migration with explicit
compatibility handling.

Hashing is opt-in through `pf_sim_hash`; the normal tick path does not hash.
M3 measures hash/snapshot overhead before any policy puts a digest on a
performance-critical path. Ranked replay authentication remains a separate
service-envelope responsibility.

## Conformance

`tests/sim/test_sim_snapshot.c` checks:

- The FIPS SHA-256 `abc` vector.
- The exact format size and representative little-endian offsets.
- Equality between `pf_sim_hash` and SHA-256 of emitted save bytes.
- Save/load/clone equality and equal future evolution after clone.
- Required-size reporting.
- Checksum, version, header, trailing-byte, content, and configuration
  rejection.
- Atomicity of every failed load checked by before/after state hash.
- Mid-hitlag save/load plus equal future combat hashes in
  `tests/sim/test_m4_combat.c`.
- Mid-shield-hitlag save/load plus equal shield-stun and shield-health
  continuation hashes in `tests/sim/test_m4_combat.c`.
- Mid-getup-roll save/load plus equal direction, invulnerability, and future
  continuation hashes in `tests/sim/test_m4_combat.c`.
- Exact validation and future equality for DI/SDI, tumble, tech-window,
  lockout, tech outcome, shield health, shield stun, powershield, and the
  shield-break flight/down/stand/stun route in `tests/sim/test_m4_combat.c`.
- Validation of the canonical wall/ceiling tech and missed-bounce action
  relationships, including continued hitstun/tumble for a missed impact and
  cleared reaction state for a successful tech.
- Mid-air-dodge save/load plus equal action timer, fixed-point velocity,
  invulnerability derivation, and future continuation hashes; validation also
  enforces airborne dodge/special-fall and grounded special-landing
  relationships.
- Mid-aerial save/load plus equal trigger age, L-cancel eligibility, action,
  and future hash; validation enforces airborne aerial attack, grounded aerial
  landing states, and inactive-slot trigger-age rules.
- Mid-spot-dodge save/load plus equal fresh-down history, action,
  invulnerability derivation, and future hashes; validation enforces grounded
  roll/spot-dodge reaction-state rules and inactive-slot fresh-down history.
- Strong-aerial action, airborne hitlag-resume, grounded 30-tick landing, and
  grounded 15-tick L-cancel action relationships, while retaining the fixed
  437-byte payload.
- Mid-respawn save/load plus equal future hashes, stock-loss and elimination
  invariants, exact respawn-delay and invulnerability timers, 300% sudden-death
  setup, deterministic repeated-tie resolution, and team winner masks in
  `tests/sim/test_m4_match.c`.
- Exact per-tick journal equality after a mid-respawn save/load continuation,
  plus typed KO, respawn, sudden-death, result, forfeit, and time-limit event
  validation.
- Exact disabled-regrab rejection through remaining tick 1, first-legal-tick
  catch and ledge-invulnerability refresh, reset/respawn clearing, and equal
  future hashes through three planking cycles in
  `tests/sim/test_m4_movement.c`.
- Mid-grab save/load plus reciprocal owner/target validation, exact natural
  and fresh-input mash escape timing, byte-identical future events, and equal
  future hashes in `tests/sim/test_m4_combat.c`.
- Mid-chain save/load during down-throw startup plus reciprocal throw-link
  validation, byte-identical future throw events, equal future hashes, and two
  subsequent legal regrabs in `tests/sim/test_m4_combat.c`.

`tools/verify_m2_kernel.sh` compiles and runs this conformance test directly
under the strict C17 warning policy, and includes serialization/hash objects in
the forbidden-symbol inspection.
