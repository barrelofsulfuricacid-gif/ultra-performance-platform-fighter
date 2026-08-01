# Data and API contracts

This document fixes interface shape rather than wire layout. M2 implements the
contracts incrementally in public headers and conformance tests.

## Simulation lifecycle

The public API must support this conceptual C surface:

```c
uint32_t pf_sim_abi_version(void);

pf_status pf_sim_query_memory(
    const pf_sim_config *config,
    pf_memory_requirements *out_requirements);

pf_status pf_sim_init(
    void *state_memory,
    size_t state_bytes,
    void *scratch_memory,
    size_t scratch_bytes,
    const pf_content_view *content,
    const pf_sim_config *config,
    pf_sim **out_sim);

pf_status pf_sim_reset(pf_sim *sim, uint64_t seed);
pf_status pf_sim_tick(
    pf_sim *sim,
    const pf_input_frame *inputs,
    size_t player_count,
    pf_tick_result *out_result);

pf_status pf_sim_query_save_size(
    const pf_sim *sim,
    size_t *out_save_bytes);
pf_status pf_sim_save(const pf_sim *sim, pf_mut_bytes *destination);
pf_status pf_sim_load(pf_sim *sim, pf_bytes source);
pf_status pf_sim_clone(pf_sim *destination, const pf_sim *source);
pf_status pf_sim_hash(const pf_sim *sim, pf_state_hash *out_hash);

pf_status pf_replay_query_size(
    const pf_replay_source *source,
    size_t *out_replay_bytes);
pf_status pf_replay_encode(
    const pf_replay_source *source,
    pf_mut_bytes *destination);
pf_status pf_replay_verify(
    pf_sim *sim,
    pf_bytes replay,
    pf_replay_verification *out_verification);

pf_status pf_rl_reset(
    pf_sim *sim,
    uint64_t seed,
    pf_rl_transition *out_transition);
pf_status pf_rl_step(
    pf_sim *sim,
    const pf_rl_action *actions,
    size_t action_count,
    pf_rl_transition *out_transition);
pf_status pf_rl_reset_batch(
    pf_sim *const *sims,
    const uint64_t *seeds,
    size_t environment_count,
    pf_rl_transition *out_transitions);
pf_status pf_rl_step_batch(
    pf_sim *const *sims,
    size_t environment_count,
    const pf_rl_action *actions,
    size_t action_stride,
    pf_rl_transition *out_transitions);
```

Rules:

- Initialization is the only phase allowed to construct internal views.
- `tick`, `save`, `load`, and `hash` perform no allocation or I/O.
- `tick` either completes one whole tick or leaves state unchanged and reports a
  deterministic fault.
- Query APIs report required capacity without partial silent truncation.
- Observations and legal-action masks have separately versioned schemas.
- Single and batched RL entry points invoke the same internal tick semantics.

Save formats 1–43 remain historical checkpoints. The current M4
movement/combat/item/projectile/reflector/charge/Moonwalk/Teeter/crouch-step/
taunt/wall-jump/Vector-Ascent/pummel/crouch-cancel/directional-ground-attack/
directional-aerial/ledge-option/smash-charge/analog-light-shield/shield-geometry
state uses save format 44: a fixed 726-byte checkpoint with state schema 45 and
a 586-byte payload. It appends one little-endian signed x-tilt and y-tilt value
per fixed player slot and makes tilt lifecycle, shield-volume derivation,
blocking-versus-poke priority, release, interruption, and hitlag-resume state
fail closed. Save
format 42/state schema 43 introduced the smash-charge values and their charge,
early/automatic release, charged damage, interruption, and hitlag-resume
relationships. Save format 41/state schema 42 retained the prior layout while
adding forward tilt plus forward/up/down directional strong actions and their
timing, grounding, hitlag-resume, and input-arbitration semantics. Save format
40/state schema 41 made the two ledge-option action IDs, grounding, ledge
claims, timing, invulnerability, and ledge-attack hitlag resume fail closed.
Save format
39/state schema 40 made the four directional-aerial action IDs, five-way
airborne light-input arbitration, grounding, authored two-axis launch, and
hitlag resume fail closed. Save format 38/state schema 39 made the `UP_ATTACK`
and `DOWN_ATTACK` action IDs,
vertical-dominant ground-light arbitration, authored two-axis launch, hitlag
resume, and powershield-cancel routing fail closed. Save format 37/state schema
38 made the grounded crouch-cancel action,
resulting-damage ceiling, eligible physical event kinds, scaled
launch/hitstun, derived tumble, and typed flag fail closed. Save format
36/state schema 37 made the explicit
`PUMMEL` action ID, authored hit/total ticks, reciprocal-link requirements,
non-launching damage, attribution, and typed event fail closed. Save format
35/state schema 36 appended one
recovery-availability byte per player and made the explicit `VECTOR_ASCENT`
action ID, once-per-airtime consumption, authored launch/steering/duration,
special-fall completion, interruption persistence, and landing/ledge/respawn
restoration fail closed. Save format 34/state schema 35 retained the prior
690-byte layout while making the explicit `WALL_JUMP` action ID, authored
speeds, 24-tick recovery, four-tick invulnerability, preserved air jump, exact
wall contact, and legal jump/aerial cancels fail closed. Save format 33/state schema 34 made
the grounded `TAUNT` action ID, authored 90-tick recovery, inherited dash
momentum, locked controls, held-input non-repetition, and support-edge
cancellation fail closed. State
schema 31 / save format 30 retained the same layout while making the two
Moonwalk action IDs, authored two-tick shallow-back setup, full-back
activation, facing-preserving backward velocity, and mistimed dashback
semantics fail closed. State schema 30 / save format 29 appended one canonical charge-tick
value per player and made Arc Reservoir charge, storage, early cancel, exact
resume, scaled release, completion, and interruption semantics fail closed.
State schema 29 / save format 28 retained
the format-27 byte layout while making grounded/aerial Prism Burst actions,
hitlag resume, landing, downward physical launch, and active-box projectile
reflection fail closed. State schema 28 / save format 27 appended one
fixed-capacity projectile slot with position/velocity, lifetime, state, and
owner, and made grounded/aerial fire, shield block, powershield reflection,
hit, and typed-event semantics fail closed. State schema 27 / save format 26
retained the prior 522-byte payload while making the full-up plus fresh
light/strong jump-squat cancel into standing strong attack fail closed. State
schema 26 / save format 25 added one fixed canonical item entity with
position/velocity, lifecycle timers, state, holder/source slots, hit mask, and
throw direction; load validates all state/ownership/timer relationships before
atomic replacement. The preceding
state schema 25 and save format 24 define canonical solid-surface tech/bounce,
air-dodge/special-fall/special-landing, and
aerial/normal-landing/L-cancel-landing semantics plus trigger age, grounded
forward/backward roll and spot-dodge semantics, fresh-down input history, and
the strong-aerial/normal-landing/L-cancel-landing action semantics. It also
stores configurable stock/respawn rules, per-player stocks and respawn timers,
respawn-wait/eliminated actions, sudden-death state, and the authoritative
monotonic event sequence. It also defines the shield-break
flight/down/stand/stun action semantics, independent remaining
ledge-invulnerability and 29-tick disabled-regrab timers, and reciprocal
grab owner/target links plus escape timers per player. State schema 20 added the
four directional throw action IDs and made startup-link, atomic release,
hitlag-resume, recovery, and typed-event semantics fail closed. State schema 21
added the dash-attack action ID and made its authored run entry, hitlag resume,
and three-frame boost-grab cancel semantics fail closed. State schema 22 added
the final-jab action ID and makes its authored hitlag resume, timing, and
inclusive first-jab choice window fail closed. State schema 23 adds the
reset-bound and forced-getup action IDs and makes weak-hit qualification,
hitlag resume, exact action timing, and grounded-versus-airborne expiry fail
closed, again without changing the byte layout. State schema 24 adds the
`DELAYED_AIR_JUMP` action ID and makes the authored half-open aerial-cancel
window, remaining-upward-velocity cancellation, late full-arc behavior, and
simultaneous jump-plus-attack non-consumption fail closed, again without
changing the byte layout. State schema 25 makes knockback-based
delayed-air-jump armor, zero-launch hit events, preserved trajectory/action
timing, and delayed-action hitlag resume fail closed without changing the byte
layout. State schema 26 adds fixed-capacity item pickup/drop/throw/hit/reset
semantics, two item-throw action IDs, and typed item events. Structured
observation schema 3 and RL schema 5 expose the same item state; compact
observation schema 4 appends eight values without changing existing indices.
State schema 27 retains that layout while making the full-up threshold,
fresh-attack edge, grounded standing-strong selection, retained inherited
momentum, and neutral/shallow-up/first-airborne-frame exclusions fail closed.
State schema 28 appends the projectile slot. Structured observation schema 4
and RL schema 6 expose it; compact observation schema 5 appends six values at
indices 56–61 without changing earlier indices. Input schema 4 adds the
separate special button used to request Pulse Bolt or, with full down, Prism
Burst. State schema 29 adds no bytes: it versions the two reflector action IDs
and their physical/projectile collision interpretation. Content schema 30 adds
one reflector definition under reflector schema 1; inspection schema 25 and
browser view schema 25 expose the action semantics without changing layouts.
State schema 30 appends eight bytes for four players' charge ticks. Structured
observation schema 5 and RL schema 7 expose the same state; compact observation
schema 6 appends four values at indices 62–65, for 66 total. Content schema 31
adds one charge definition under charge schema 1. Inspection schema 26 appends
the charge value, and browser view schema 26 appends it to both visible players
for a 304-value view.
State schema 31 retains the 550-byte payload while versioning
`MOONWALK_SETUP` and `MOONWALK` action timing. Content schema 32/fighter
schema 28 adds and hashes the data-defined setup duration. Inspection and
browser view schema 27 version the new action interpretation without changing
the 304-value layout; structured observation schema 5, RL schema 7, compact
observation schema 6, and its 66 values remain unchanged.
State schema 32 also retains the 550-byte payload while adding explicit
`TEETER` action semantics. Content schema 33/fighter schema 29 add and hash the
data-defined support-edge snap distance and duration. Inspection and browser
view schema 28 version that interpretation without changing the 304-value
layout; observation and RL schemas remain unchanged.
State schema 33 likewise retains the 550-byte payload while adding explicit
`CROUCH_STEP` action semantics. Content schema 34/fighter schema 30 add and
hash the data-defined speed and duration. Inspection and browser view schema
29 version that interpretation without changing the 304-value layout;
observation and RL schemas remain unchanged.
State schema 34 retains that layout while adding explicit grounded `TAUNT`
semantics. Content schema 35/fighter schema 31 add and hash the authored
duration. Input schema 5 assigns bit 4 to Taunt. Inspection and browser view
schema 30 version the new action interpretation without changing the
304-value layout; observation and RL schemas remain unchanged.
State schema 35 retains that layout while adding explicit `WALL_JUMP`
semantics. Content schema 36/fighter schema 32 add and hash the authored
horizontal and vertical speeds, action duration, brief invulnerability, and
enable flag. Inspection and browser view schema 31 version the new action
interpretation without changing the 304-value layout; input, observation, and
RL schemas remain unchanged.
Browser view schema 32 is presentation-only and expands that layout to 392
values by exposing all four already-canonical player inspection records. Each
player keeps the 44-value layout at base 25; event count is at 201, 16
ten-value event records begin at 202, the 18-value item record begins at 362,
and the 12-value projectile record begins at 380. Canonical state schema 35,
save format 34, content schema 36, input schema 5, inspection schema 31,
observation schema 5, RL schema 7, compact schema 6, and 66 compact values are
unchanged.
State schema 36 / save format 35 appends four recovery-availability bytes for
a 554-byte payload and 694-byte checkpoint under `PFSAVE35`. Content schema 37
adds and hashes recovery schema 1 with enable, horizontal/vertical speed, and
18-tick duration fields; fighter schema 32 and input schema 5 remain unchanged.
Inspection schema 32 and structured observation schema 6 expose the resource.
RL schema 8 / compact schema 7 packs it into player flag bit 18 while retaining
66 compact values. Browser view schema 33 appends the four visible values at
indices 392–395 for a 396-value presentation view without moving any existing
offset.
State schema 37 / save format 36 retains the 554-byte payload and 694-byte
checkpoint under `PFSAVE36` while adding fail-closed `PUMMEL` action and typed
event semantics. Content schema 38/fighter schema 33 add and hash pummel
damage, hit tick, and total duration; inspection schema 33 versions the new
action/event interpretation. Input schema 5, observation schema 6, RL schema
8, compact schema 7, and 66 compact values remain unchanged. Browser view
schema 34 retains 396 values while versioning the action and event labels.
State schema 38 / save format 37 retains those sizes under `PFSAVE37` while
making crouch-cancel qualification, scaling, hitstun flooring, tumble, and
event-flag semantics fail closed. Content schema 39/fighter schema 34 add and
hash the post-hit damage ceiling plus independent launch/hitstun scales;
inspection schema 34 versions the interpretation. Input schema 5, observation
schema 6, RL schema 8, compact schema 7, and 66 compact values remain unchanged.
Browser view schema 35 retains 396 values and labels the new typed flag.
Content schema 40/fighter schema 35 add and hash the target-side Q16.16 victim
weight used by the shared unblocked hit-reaction path. The default 1.0 value is
an identity transform; the field changes no canonical, save, replay,
inspection, observation, RL, or 396-value browser-view layout and therefore
leaves their schema versions unchanged.
State schema 39 / save format 38 retains the 554-byte payload and 694-byte
checkpoint under `PFSAVE38` while adding fail-closed `UP_ATTACK` and
`DOWN_ATTACK` action semantics. Content schema 41/fighter schema 36 append and
hash two embedded attack-data records under the parent fighter definition;
each record owns box geometry, damage, two-axis base knockback, growth,
startup, active, recovery, and hitlag values. Inspection schema 35 and browser
view schema 36 version the new action/event interpretation while retaining the
existing inspection and 396-value presentation layouts. Input schema 5,
structured observation schema 6, RL schema 8, compact schema 7, and 66 compact
values remain unchanged.
State schema 40 / save format 39 retains those serialized sizes under
`PFSAVE39` while adding fail-closed `FORWARD_AERIAL`, `BACK_AERIAL`,
`UP_AERIAL`, and `DOWN_AERIAL` semantics. Content schema 42/fighter schema 37
append and hash four more embedded attack-data records. Inspection schema 36
and browser view schema 37 version the new action interpretation while
retaining the inspection and 396-value browser layouts. Input schema 5,
structured observation schema 6, RL schema 8, compact schema 7, and 66 compact
values remain unchanged. The public memory-requirements query now reports
2,080 state bytes and 1,008 scratch bytes; the 4 KiB M4 caller envelopes remain
valid, and this opaque in-memory growth does not change the 694-byte canonical
checkpoint.
State schema 41 / save format 40 retains the 554-byte payload and 694-byte
checkpoint under `PFSAVE40` while adding fail-closed `LEDGE_ROLL` and
`LEDGE_ATTACK` action, timing, grounding, ledge-claim, invulnerability, and
hitlag-resume semantics. Content schema 43/fighter schema 38 append and hash
the ledge-roll distance and timing windows plus one embedded ledge-attack
record. Inspection schema 37 and browser view schema 38 version the new action
interpretation and labels while retaining the inspection and 396-value
browser layouts. Input schema 5, structured observation schema 6, RL schema 8,
compact schema 7, and 66 compact values remain unchanged. The public
memory-requirements query now reports 2,128 state bytes and 1,008 scratch
bytes; existing callers retain their 4 KiB opaque state and scratch envelopes,
and canonical checkpoint size remains unchanged.
State schema 42 / save format 41 retains the 554-byte payload and 694-byte
checkpoint under `PFSAVE41` while adding fail-closed `FORWARD_ATTACK`,
`FORWARD_STRONG_ATTACK`, `UP_STRONG_ATTACK`, and `DOWN_STRONG_ATTACK` action,
grounding, timing, hitlag-resume, powershield-cancel, and directional-input
semantics. Content schema 44/fighter schema 39 append and hash four embedded
attack-data records. Inspection schema 38 and browser view schema 39 version
the new action interpretation and labels while retaining the inspection and
396-value browser layouts. Input schema 5, structured observation schema 6,
RL schema 8, compact schema 7, and 66 compact values remain unchanged.
State schema 43 / save format 42 appends one little-endian `uint16_t` smash-
charge value for each of the four fixed player slots, producing a 562-byte
payload and 702-byte checkpoint under `PFSAVE42`. Loading validates the three
charge actions, 60-tick cap, action-timer equality, early or automatic release,
released-action and attacker-hitlag retention, interruption clearing, and
inactive-slot zero state. Content schema 45/fighter schema 40 add and hash the
maximum charge and Q16.16 damage bonus. Inspection schema 39 and structured
observation schema 7 expose the timer; RL schema 9/transition schema 7 and
compact schema 8 append four values at indices 66–69, for 70 total. Browser
view schema 40 expands each player record from 44 to 45 values and the whole
view from 396 to 400 values. The public memory-requirements query reports 2,312
state bytes and 1,016 scratch bytes; the 4 KiB caller envelopes remain valid.
State schema 44 / save format 43 appends one little-endian `uint16_t` raw shield
strength for each fixed player slot, producing a 570-byte payload and 710-byte
checkpoint under `PFSAVE43`. Loading accepts the value only for `SHIELD`,
`SHIELD_STUN`, or hitlag that resumes into shield stun; it validates the light
and digital thresholds, dense-only powershield state, and inactive-slot zero
state. Content schema 46/fighter schema 41 add and hash the light hold-depletion
and defender-pushback values plus the light trigger threshold. Inspection
schema 40 and structured observation schema 8 expose raw shield strength. RL
schema 10/transition schema 8 and compact schema 9 append four values at indices
70–73, for 74 total. Browser view schema 41 expands each player record from 45
to 46 values and the whole view from 400 to 404 values. The public
memory-requirements query reports 2,328 state bytes and 1,024 scratch bytes; the
4 KiB caller envelopes remain valid.
State schema 45 / save format 44 appends little-endian signed 16-bit x/y shield
tilt for every fixed player slot, producing a 586-byte payload and 726-byte
checkpoint under `PFSAVE44`. Loading permits nonzero tilt only wherever raw
strength is legal and rejects inactive-slot or ended-lifecycle residue. Shield
bounds are derived from canonical position, health, strength, and tilt rather
than serialized. Content schema 47/fighter schema 42 add and hash base half
extents, minimum/dense scales, and maximum x/y tilt. Inspection schema 41
exposes shield-active, exact bounds, and tilt. Structured observation schema 9
adds shield health and tilt. RL schema 11/transition schema 9 and compact schema
10 retain strength at 70–73, append health at 74–77 and two tilt values per
player at 78–85, for 86 total. Browser view schema 42 expands each player
record from 46 to 53 values and the whole view from 404 to 431 values. The
public memory-requirements query reports 2,368 state bytes and 1,040 scratch
bytes; the 4 KiB caller envelopes remain valid.
Content schema 48/fighter schema 43 append, validate, and hash the immutable
`shield_sdi_scale_q16`, exactly 33/50 by default. The field scales grounded
horizontal shield SDI and shield ASDI without adding canonical state: state
schema 45/save format 44, inspection/observation/RL/browser layouts, checkpoint
size, and memory requirements remain unchanged.
State schema 46 / save format 45 retains the 586-byte payload and 726-byte
checkpoint under `PFSAVE45` while recognizing canonical action 94 and support
4 for the moving revival platform. Loading derives the exact slot x and
interpolated y from player count, slot, immutable stage data, and action ticks;
it rejects wrong position, velocity, grounding, support, recovery resource,
timer, or premature post-drop invulnerability. Content schema 49/stage schema
3 append and hash revival start/end y, half-width, descent ticks, and hold
ticks. Inspection schema 42 adds the authored stage values and derived active
platform geometry. Browser schema 43 keeps every earlier index stable and
appends four values per fixed player at 431–446, growing the view from 431 to
447 values. Fighter schema 43, observation schema 9, RL schema 11/transition
schema 9, compact schema 10 with 86 values, checkpoint size, and public memory
scratch requirement remain unchanged. The larger copied immutable stage record
raises the opaque state requirement from 2,368 to 2,384 bytes; the 4 KiB caller
envelope remains valid.
The M4 collision inspector consumes schema-35 stage geometry, fighter and
active attack/grab bounds, schema-42 exact shield bounds, and item/projectile
extents. Its default-on toggle, legend, and pause-safe redraw remain
presentation semantics; the new shield overlay reads the versioned view and
does not add canonical, replay, save, observation, or RL state.
The temporary M4.3 browser setup calls
`pf_web_m4_playtest_configure_duel(stock_count)` for stock choices 1–4. The
bridge validates the value, rebuilds a fresh production duel, and renders tick
zero; it does not patch live canonical state. Result, rematch, and setup are
presentation flow states and add no serialized format.
Format 14 changed the
public tick-result semantics without adding journal payloads to canonical
state.
Format 5 first
introduced the same-size payload containing shield health, shield-stun timers,
and powershield result state. Exact headers, payloads, compatibility, checksum,
and atomic-load behavior are recorded in
[TDR-0006](../technology_decisions/0006-canonical-state-format.md).

## Normalized input frame

The logical fields are:

| Field | Type | Rule |
|---|---|---|
| tick | `uint64_t` | Exact target logical tick |
| player slot | `uint8_t` | Stable slot, not controller/device ID |
| buttons | `uint64_t` | Versioned action bitset |
| main stick x/y | `int16_t` each | Canonical calibrated range |
| secondary stick x/y | `int16_t` each | Canonical calibrated range |
| left/right trigger | `uint16_t` each | Canonical range |
| input schema | `uint16_t` | Reject unknown incompatible mappings |

The C ABI structure is not the replay/network byte encoding.

Input schema 3 assigns bit 0 to jump, bit 1 to light attack, bit 2 to strong
attack, and bit 63 to forfeit. Unknown bits fail before any player state is
advanced.
Input schema 4 additionally assigns bit 3 to special. Neutral special may
request the fixed Pulse Bolt; full down plus a fresh special edge may request
the grounded or airborne Prism Burst, and grounded full up plus a fresh
special edge may start or resume Arc Reservoir when its content and action
state are legal.
Input schema 5 additionally assigns bit 4 to Taunt. A fresh grounded edge may
enter the authored `TAUNT` action from ordinary standing, dash, run, crouch,
turnaround/brake, or teeter states; held input does not retrigger it.

## Deterministic state schema

The state is partitioned into:

1. Match header: ABI/content/config versions, tick, seed/RNG streams, mode,
   team/stock/score state, and deterministic fault flags.
2. Fighter hot arrays: positions, velocities, state IDs, timers, damage,
   stocks, facing, collision masks, and current move references.
3. Bounded object pools: projectiles, items if enabled, hitboxes, hurtboxes,
   ledge claims, flags, and stage hazards.
4. Stage/mode state: platform/hazard phases, blast zones, CTF flags/bases,
   respawn queues, and team rules.
5. Rollback bookkeeping that affects future simulation, excluding transport
   statistics.

Immutable move/stage tables remain in a separately hashed content view and are
not duplicated per match.

## Event journal

ABI 4 exposes up to 16 caller-owned, fixed-size events in every tick result.
Each event records the processed input tick, a match-monotonic sequence,
type, flags, source and target slots, one Q16.16 value, one Q16.16 velocity
pair, and a type-specific 16-bit detail. `255` denotes a system/no-player
endpoint. The currently produced types are hit, shield block, powershield,
shield break, grab, grab escape, throw, item pickup/drop/throw/hit/reset,
projectile fire/hit/reflect, KO, respawn, revival drop, sudden death, match
result, forfeit, and time limit.

The event array itself is same-tick output scratch, not rolling canonical
history. Canonical state stores the next sequence authority. Loading a
checkpoint and replaying the same inputs therefore reproduces exactly the
same event records and sequence IDs without making rollback memory grow with
match length. Presentation clients may retain a bounded recent history and
must discard/reconcile it when the simulation tick rewinds.

The current production path has a statically proven upper bound of 13 events
per tick: at most one movement, one combat, and one forfeit event per player,
plus one match-resolution event. Capacity is 16. Sequence exhaustion or
capacity overflow faults before the tick mutates canonical state; events are
never silently dropped.

Event categories include:

- Action/state transitions.
- Hits, shields, parries if retained, grabs, throws, damage, launch, and KO.
- Spawn/despawn.
- Ledge, platform, hazard, flag, stock, score, and match-result changes.
- Presentation cues for animation, particles, camera, audio, and controller
  feedback.

Presentation policy—speculative, replaceable, or confirmation-only—is metadata
outside canonical state.

## Replay format

The replay is a chunked, length-delimited binary container:

| Chunk | Required content |
|---|---|
| Header | Magic, replay version, simulation ABI, content/config hashes, seed, players/teams, tick rate |
| Inputs | Delta-compressed normalized frames in tick order |
| Checkpoints | Optional canonical save states plus per-tick or periodic hashes |
| Result | Final deterministic result; a later chunk version may add a journal digest |
| Metadata | Non-authoritative display names, client platform, annotations |
| Verification envelope | Server protocol version, signer ID, signature, acceptance/rejection reason |

Unknown optional chunks can be skipped by length. Unknown required chunks fail
closed. Display metadata is not included in deterministic result hashing.

Ranked clients sign or authenticate the submitted input/replay envelope. The
server re-simulates it with the identified headless build/content pair before
rating is finalized.

Replay format 1 uses five checksummed required chunks and mandatory per-tick
hashes. Replay API schema 2 carries ABI-4 tick results, but the format-1 result
chunk intentionally retains its original 16-byte terminal summary and does
not encode the inline last-tick journal. Verification re-simulates the journal;
the canonical cross-target corpus separately hashes every emitted event under
the `PFEVT001` domain. Its exact ownership, compatibility, failure, and
golden-corpus rules are recorded in
[TDR-0007](../technology_decisions/0007-replay-container.md).

`pf_replay_verify_observed` adds replay-observer schema 1 without changing the
container. After the initial save and each subsequent tick pass their stored
SHA-256 comparison, the caller receives the read-only simulation, total replay
tick count, verified checkpoint hash, and that transition's exact ABI-4 tick
result. Checkpoint zero has an empty journal; checkpoint `N` carries the events
emitted while processing input tick `N - 1`. Callback failure aborts inspection
and is reported through the ordinary verification status, so presentation and
tools can stage a trace and publish it only after the terminal result also
matches.

The owner-approved action, structured/compact observation, reward, legal-mask,
and batch semantics are recorded in
[TDR-0008](../technology_decisions/0008-rl-contract-candidate.md).

## Design-data pipeline

```mermaid
flowchart LR
    Workbook["Authoritative .xlsx"] --> Import["C importer"]
    Import --> Model["Canonical validated model"]
    Model --> Pack["Production binary pack"]
    Model --> Diagnostics["Human diagnostics"]
    Pack --> Sim["Immutable simulation view"]
```

The workbook importer reads values, not Excel formulas as an execution engine.
A workbook is accepted only after required sheets/columns, types, units,
ranges, IDs, references, capacities, and cross-table invariants validate.

The production pack contains:

- Magic, pack/schema versions, endian marker, total length, and section
  directory.
- Source workbook SHA-256 and canonical content hash.
- Fixed-width integer/fixed-point values already converted using the chosen
  rounding rules.
- Offset/count pairs rather than pointers.
- Deduplicated string/asset IDs outside hot simulation tables.
- A checksum for each section and the complete pack.

Release, web, headless, replay verification, and ranked handshake consume the
same pack bytes. Developer runtime import produces the same canonical model and
must byte-match the offline packer for identical workbooks.

## Compatibility identity

A peer, replay, save state, or verifier job is compatible only when all
deterministic identity fields match:

- Simulation ABI and arithmetic version.
- Input, state, replay, observation, and pack schema versions.
- Canonical content/config hash.
- Build compatibility ID.
- Controller-normalization version.
- Required feature/mode flags.

Human-facing client version, renderer, audio backend, and cosmetic pack may
differ only when their assets cannot alter deterministic tables or visibility
rules.
