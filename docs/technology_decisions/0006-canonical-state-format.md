# TDR-0006: Canonical state format and hash

- **Status:** Accepted for save formats 1–34 / state schemas 1–35
- **Date:** 2026-07-31

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
| 20 | 21 | 140 | 495 | 635 | Canonical dash-attack action ID, run-entry and hitlag-resume semantics, and the boost-grab cancel window; no payload-layout change |
| 21 | 22 | 140 | 495 | Canonical final-jab action ID, hitlag-resume semantics, and the inclusive first-jab choice window; no payload-layout change |
| 22 | 23 | 140 | 495 | 635 | Canonical reset-bound and forced-getup action IDs, weak-hit qualification, hitlag-resume, exact timing, and grounded-versus-airborne expiry semantics; no payload-layout change |
| 23 | 24 | 140 | 495 | 635 | Canonical delayed-air-jump action ID, exact authored aerial-cancel window, vertical-momentum cancellation, and late full-arc semantics; no payload-layout change |
| 24 | 25 | 140 | 495 | 635 | Knockback-based delayed-air-jump armor, zero-launch hit events, preserved action timing/trajectory, and delayed-action hitlag resume semantics; no payload-layout change |
| 25 | 26 | 140 | 522 | 662 | One fixed canonical item entity: position/velocity, lifetime/respawn/pickup-lockout timers, state, holder/source slots, hit mask, and throw direction; item-throw action IDs and typed item-event semantics |
| 26 | 27 | 140 | 522 | 662 | Full-up plus fresh light/strong during jump squat selects grounded standing strong attack while retaining inherited momentum; no payload-layout change |
| 27 | 28 | 140 | 542 | 682 | One fixed canonical projectile slot: position/velocity, lifetime, inactive/spawning/active state, owner slot, grounded/aerial fire action IDs, and typed fire/hit/reflect semantics |
| 28 | 29 | 140 | 542 | 682 | Canonical grounded/aerial Prism Burst action IDs, hitlag-resume and landing semantics, downward physical launch, and active-box projectile reflection; no payload-layout change |
| 29 | 30 | 140 | 550 | 690 | One canonical charge-tick value per player plus Arc Reservoir charge, store, early-cancel, resume, scaled-release, completion, interruption, and action-ID semantics |
| 30 | 31 | 140 | 550 | 690 | Canonical `MOONWALK_SETUP` and `MOONWALK` action IDs, authored shallow-back timing, full-back activation, retained facing/dash direction, backward velocity, and mistimed dashback semantics; no payload-layout change |
| 31 | 32 | 140 | 550 | 690 | Canonical `TEETER` action ID, authored support-edge snap distance and duration, neutral persistence, zero-velocity grounding, standing-attack/reverse-dash cancels, held-outward run-off, and early-release semantics; no payload-layout change |
| 32 | 33 | 140 | 550 | 690 | Canonical `CROUCH_STEP` action ID, authored speed and one-tick duration, fresh diagonal-down entry, release-gated repetition, and ordinary crouch transition; no payload-layout change |
| 33 | 34 | 140 | 550 | 690 | Canonical grounded `TAUNT` action ID, authored duration, inherited dash momentum and traction, locked recovery, held-input non-repetition, and support-edge cancellation into `TEETER`; no payload-layout change |
| 34 | 35 | 140 | 550 | 690 | Canonical `WALL_JUMP` action ID, authored speed, duration, and brief invulnerability, preserved air jump, fresh-away wall contact, and jump/aerial cancel; no payload-layout change |

The header magic is `PFSAVE01`, `PFSAVE02`, `PFSAVE03`, `PFSAVE04`, or
`PFSAVE05`, `PFSAVE06`, `PFSAVE07`, `PFSAVE08`, `PFSAVE09`, `PFSAVE10`, or
`PFSAVE11`, `PFSAVE12`, `PFSAVE13`, `PFSAVE14`, `PFSAVE15`, `PFSAVE16`, or
`PFSAVE17`, `PFSAVE18`, `PFSAVE19`, `PFSAVE20`, `PFSAVE21`, `PFSAVE22`, or
`PFSAVE23`, `PFSAVE24`, `PFSAVE25`, `PFSAVE26`, `PFSAVE27`, `PFSAVE28`,
`PFSAVE29`, `PFSAVE30`, `PFSAVE31`, `PFSAVE32`, `PFSAVE33`, or `PFSAVE34`. The active M4 runtime emits
and accepts format 34 with state schema 35. Earlier
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
Format 20 also retains the 495-byte payload while making the production dash
attack fail closed. A light-attack edge from run enters the authored action and
speed; hitlag resumes the same action; stored action ticks 1–3 accept the
light-plus-shield grab cancel while the initiation and later frames do not;
the cancel preserves momentum and enters the existing standing grab. Loading
rejects the new action under any earlier schema and validates its existing
reaction, hitlag-resume, and reciprocal-link relationships under schema 21.
Format 21 retains the same payload while making the production two-hit jab
sequence fail closed. Stored first-jab action ticks 4–7 accept a fresh shield
cancel or fresh light selection of the independently authored final jab; an
early-held or first-late input cannot select either transition. Loading rejects
the final-jab action under earlier schemas and validates its hitlag-resume and
action schedule under schema 22.
Format 22 retains the same payload while making the production jab-reset
reaction fail closed. A physical hit against vulnerable down wait or an
existing reset bound qualifies only at or below the authored damage and
computed-hitstun limits, resumes after hitlag into the exact low bound, and
enters forced getup only if grounded when that bound expires. Loading rejects
both new actions under earlier schemas and validates their existing reaction,
hitlag-resume, grounded/airborne, and action-timer relationships under schema
23.
Format 23 retains the same payload while making the production delayed air
jump fail closed. A legal air jump enters the new action for the authored
half-open window; a fresh light or strong aerial inside that window cancels
remaining upward velocity before ordinary gravity and motion, while the first
late aerial preserves the full rise. Simultaneous fresh jump and attack from
ordinary airborne state selects the attack without consuming an air jump, and
a zero authored window disables the delayed state. Loading rejects the new
action under earlier schemas and validates its existing airborne, action-timer,
ledge-catch, and V-cancel relationships under schema 24.
Format 24 retains the same payload while making delayed-air-jump armor fail
closed. A qualifying physical hit still applies authored damage and hitlag,
but emits zero launch, preserves the defender's existing velocity and action
tick through the freeze, and resumes `DELAYED_AIR_JUMP` with no hitstun or
tumble. Qualification is bounded by the authored computed-hitstun threshold;
non-physical reactions, late hits, disabled armor, and stronger hits use the
ordinary launch path. Loading validates the zero-launch pending state and the
airborne delayed-action resume relationship under schema 25.
Format 25 expands the payload by 27 bytes for one fixed canonical item entity
and makes its lifecycle, ownership, attribution, hit mask, directional throw,
item action, and typed event semantics fail closed under schema 26. Format 26
retains that 522-byte payload while making the production jump-cancel attack
router fail closed: only full up plus a fresh light or strong edge during jump
squat selects grounded standing strong attack and retains inherited momentum;
neutral or shallow up continues jump squat, and the first airborne frame uses
the ordinary aerial route. Loading validates those semantics under schema 27.
Format 27 expands the payload by 20 bytes for one fixed canonical Pulse Bolt
slot and makes its deterministic controller-slot arbitration, one-tick
spawning phase, straight active motion, lifetime/blast despawn, ownership,
grounded/aerial fire actions, shield block, two-frame powershield reflection,
ordinary hit reaction, and typed fire/hit/reflect events fail closed under
schema 28. The serialized owner uses the same zero-for-none and slot-plus-one
encoding as other canonical links; load rejects invalid state, owner, action,
position, velocity, lifetime, and enabled-content relationships before atomic
replacement.
Format 28 retains the same payload while making Prism Burst fail closed. Full
down plus a fresh special edge may select the grounded or airborne reflector
action from its legal movement states; the authored physical hit launches
downward through the ordinary combat pipeline, and its active box may reverse
projectile velocity and ownership without setting powershield. Loading rejects
the new action IDs under earlier schemas and validates hitlag resume, grounded
versus airborne action choice, action timing, landing, and existing projectile
ownership relationships under schema 29.
Format 29 appends one little-endian `uint16_t` charge value for each of the
four fixed player slots. Loading requires the values to fit the enabled
content's configured maximum, validates the three grounded action IDs,
their action timing and hitlag resume, and rejects charge state that is
incompatible with disabled content or the action/grounding relationship.
Storage cancel, exact resume, charge-scaled release, completion clearing, and
hit-interruption clearing therefore participate in the canonical future under
schema 30.
Format 30 retains the same payload and adds no mutable field. During initial
dash, the two explicit Moonwalk actions encode the authored shallow-back setup
and the subsequent full-back slide in existing action ID/timer, facing,
dash-direction, and velocity fields. Loading rejects zero/out-of-range setup
or active ticks, airborne/reaction-incompatible actions, missing or
facing-inconsistent dash direction, and any action value unknown to schema 31.
The bump prevents a format-29 reader from silently treating those action IDs
or their facing-preserving reverse-motion semantics as ordinary movement.
Format 31 also retains the same payload and adds no mutable field. Explicit
`TEETER` action ID/ticks encode the authored edge state. Loading rejects
out-of-range Teeter ticks, an airborne Teeter, nonzero Teeter horizontal
velocity, reaction-incompatible state, or any action value unknown to schema
32. The bump prevents a format-30 reader from silently treating action 73 or
its edge-clamp and standing-cancel semantics as ordinary grounded movement.
Format 32 again retains the same payload and adds no mutable field. Explicit
`CROUCH_STEP` action ID/ticks and existing fresh-down history encode one
authored diagonal-down microstep followed by ordinary crouch. Loading rejects
out-of-range ticks, an airborne or reaction-incompatible step, and action
values unknown to schema 33. The bump prevents a format-31 reader from
silently treating action 74 and its release-gated transition as ordinary
grounded movement.

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
- Mid-dash-attack save/load on the first boost-grab cancel tick plus
  byte-identical future events, equal future hashes, retained momentum, and
  deterministic capture in `tests/sim/test_m4_combat.c`.
- Mid-reset-bound save/load plus exact weak-hit qualification boundaries,
  grounded forced-getup and airborne SDI escape outcomes, byte-identical future
  events, and 64 equal future hashes through a real punish in
  `tests/sim/test_m4_combat.c`.
- Mid-item-throw save/load plus equal future hashes and events, strict item
  state/holder/source/timer validation, encoded replay verification, and
  structured/compact RL observation checks in `tests/sim/test_m4_item.c`.
- Mid-jump-cancel save/load plus retained dash momentum, both attack-button
  routes, threshold/neutral/late exclusions, a real strong hit, and equal
  future hashes and events in `tests/sim/test_m4_combat.c`.
- Mid-Moonwalk-setup save/load plus exact setup/activation timing, preserved
  facing, reverse velocity, traction exit, two mistimed dashback controls, and
  equal future hashes in `tests/sim/test_m4_movement.c`.

`tools/verify_m2_kernel.sh` compiles and runs this conformance test directly
under the strict C17 warning policy, and includes serialization/hash objects in
the forbidden-symbol inspection.
