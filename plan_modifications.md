# Plan modifications

## 2026-08-03 - M4 SSBM frame-data fidelity exception

The owner explicitly authorized importing frame-data tables while requesting
that movement and simulation behavior match the SSBM decomp. This overrides
the M0 wording that required independently authored implementation data and
prohibited imported frame data, but only for numeric gameplay behavior and
state-machine research.

The exception does not authorize importing or shipping executable game code,
the disc image, extracted archives, names, art, animation, audio, stage
geometry, writing, UI expression, or other assets. Every imported value must
have a pinned source revision or user-owned NTSC 1.02 extraction record, an
explicit coordinate conversion where needed, and a field-level provenance
entry. Extracted files remain outside the repository.

This materially changes the project's originality/IP posture. The existing
formal pre-release IP review remains mandatory and must now specifically cover
the imported gameplay tables and the degree of behavioral equivalence.

M4 validation will distinguish `equivalent`, `partial`, `divergent`, and
`missing` behavior. A passing deterministic test proves the implemented
contract; it does not by itself prove complete SSBM equivalence.

## 2026-08-03 - Dolphin identical-input equivalence gate

The owner requires every implemented movement or shared-simulation behavior
with an SSBM counterpart to be exactly equivalent to SSBM rather than tuned
approximately. Only explicitly original mechanics with no intended SSBM
counterpart are excluded. M4 must use the owner's `GALE01` NTSC 1.02 image in
Dolphin as an executable oracle: the verifier supplies the same per-frame
controller trace to Dolphin and the project, captures comparable state and
motion fields, and reports the first divergent frame. This applies with nonzero
as well as zero starting velocity and explicitly includes ordinary dash entry
and repeated dash dancing. Fidelity work continues until the complete
applicable differential corpus agrees; work does not stop and an unresolved
divergence cannot be deferred while accepting M4.

Implementation-authored tests and decomp review remain necessary but are not
sufficient evidence for equivalence. Any owner-observed movement divergence
must become a pinned differential reproducer and blocks M4 acceptance until the
capture agrees. The existing originality boundaries remain in force: the disc,
extracted archives, executable code, audiovisual expression, and stage assets
stay outside the repository; only authorized gameplay data, independently
written behavior, input traces, metadata, and numeric comparison evidence may
be retained.

"Movement or shared-simulation behavior" includes all implemented common
engine routes with an intended SSBM counterpart, not only ground locomotion.
The gate therefore includes jump/landing, shields and light shields, defensive
escapes, ledges and collision, hitlag/hitstun/knockback, DI/SDI, teching, stale
moves, stocks, respawn, and match-state transitions. The currently passing
movement trace is evidence for only the routes it captures; it cannot satisfy
this broader gate until identical-input Dolphin traces cover the remaining
applicable systems. Work continues after any individual capture or CI run, and
M4 remains unfinished while any applicable divergence or uncovered route
remains.

Current qualification evidence is deliberately narrower than this acceptance
gate. The pinned 1,589-frame Captain Falcon trace currently agrees for its
locomotion, shield/light-shield, forward/backward/C-stick roll, spot-dodge,
jump-from-shield, fresh-opposite-trigger air-dodge, analog-trigger-in-air,
nonzero-velocity dash/turn, jump-squat reversal, short/full-hop, neutral-stick
double-jump, fast-fall, landing, and full-down crouch start/hold/release routes.
That passing trace is a regression slice, not evidence that every applicable
movement or shared-simulation route is equivalent; crouch threshold hysteresis,
interrupt routes, and the other remaining systems still require identical-input
Dolphin reproducers and comparable-state assertions.
