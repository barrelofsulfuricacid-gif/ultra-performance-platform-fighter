# Plan modifications

## 2026-08-12 - Falcon completion, native Battlefield, and reusable character import

The active M4 outcome is now explicitly twofold: finish the Captain Falcon
NTSC 1.02 simulation port under the existing exact-behavior/float32 contract,
and provide a native frontend in which the owner can playtest that simulation
on Battlefield. A browser frontend remains useful, but it does not substitute
for the required native Battlefield playtest path.

When a source audit establishes that legacy authored simulation code blatantly
diverges from Melee, remove that code instead of retaining it as a parallel
Falcon path or dormant compatibility fixture. Preserve original/custom fighter
behavior only where it is explicitly outside the imported-reference content
path; use a zero-cost imported/reference gate when the same runtime supports
both contracts. Superseded tests and current player-facing documentation must
be deleted or corrected with the implementation. Historical milestone text may
remain only when it is clearly marked as superseded.

Every Falcon slice must use and improve the `ssbm-character-importer` skill.
The long-term target is a source-driven toolchain that can import a new Melee
character completely, quickly, and without human intervention: discover and
validate data, map action/callback tables, generate compact runtime artifacts,
produce provenance, construct identical-input live cases, and bind stored
regressions. Until full automation is reached, each manual discovery must be
captured as reusable skill guidance, a character-independent routine, or an
explicitly documented gap rather than remaining implicit Falcon knowledge.

Equivalence validation follows the same reuse rule. Capture protocols,
checkpoint orchestration, trace schemas, projection/comparison logic,
generation, affected-domain selection, hashing, budgets, and result reporting
must be character-independent. Character-specific files should contain only
data bindings, source mappings, case declarations, and irreducible semantic
adapters. Do not clone a runner or verifier for each character when a manifest
entry or typed generated descriptor can express the distinction.

## 2026-08-04 - Beautiful zero-cost implementation gate

The owner added implementation quality as a binding requirement alongside
ultra-high throughput and near-SSBM behavioral equivalence. Correct behavior
and benchmark speed are necessary but no longer sufficient: production code
must use the correct zero-cost abstractions, make ownership and invariants
clear, and reduce duplication to the minimum unavoidable at external adapters
and test boundaries.

Every mechanic, formula, state transition, and representation conversion must
have one canonical runtime authority shared by local play, netplay, replay,
verification, RL, native, and web execution. Hot-path abstractions must compile
to the direct equivalent without avoidable allocations, copies, branches,
indirect calls, or call overhead. If that property is not evident, optimized
code inspection or a compatible benchmark must demonstrate it. A faster patch
that forks gameplay policy or a clean-looking abstraction that adds measurable
hot-path cost is not acceptable.

This is a cross-cutting milestone gate and not deferred cleanup. Material
changes require review for cohesive C interfaces, data-driven reuse, explicit
state ownership, readable invariants, removal of superseded paths, and minimal
adapter/test duplication. The requirement applies immediately to ongoing M4
SSBM-fidelity work and to all later milestones.

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

This is an exhaustive coverage obligation, not a finite bug list. Owner reports
add mandatory reproducers but do not define the boundary of the work. The M4
audit must systematically enumerate the remaining shared states, transitions,
input thresholds, timers, and momentum conditions from the decomp, then replay
the same ordered per-frame inputs in Dolphin and the simulator for every
applicable route. A passing current corpus proves only its captured routes.

The owner explicitly accepts very small differences caused by the simulator's
float32 representation. The oracle therefore treats exact equivalence as
behavioral: discrete state/action/timing, facing, grounded state, thresholds,
and route selection remain strict, while numeric position or velocity
tolerances must be narrow, recorded, and justified by the source and binary32
operation order. For strict collision boundaries, a single bounded one-tick
binary32 transient is acceptable; cumulative drift or a different
behavioral result is not.

Current qualification evidence is deliberately narrower than this acceptance
gate. The pinned 8,675-frame Captain Falcon trace currently agrees for its
locomotion, shield/light-shield, forward/backward/C-stick roll, spot-dodge,
jump-from-shield, fresh-opposite-trigger air-dodge, analog-trigger-in-air,
nonzero-velocity dash/turn, jump-squat reversal, short/full-hop, neutral-stick
double-jump, fast-fall, landing, full-down crouch start/hold/release, exact
crouch entry/release threshold boundaries, jump interrupts from all three
crouch states, held-crouch opposite dash through `Turn`, and
crouch-release walk, guard interruption from all three crouch states, and
fresh taunt from all three crouch states, standing turn, and normal-Landing
taunt, jump, dash/turn, guard, walk, exact-frame direct crouch, late-down
lockout, and ordinary-turn routes through Falcon's complete 60-frame taunt.
It also covers main-stick tap-jump full/short hop, aerial jump, Landing and
shield entry, just-below/just-above 0.6625 threshold samples, a four-tick-aged
slow-sweep rejection, and an in-window two-sample acceptance.
The expanded common-RunBrake matrix covers frame-1 neutral entry; jump and
main-stick crouch; shield-plus-down crouch priority; rejected neutral guard,
C-stick roll/spot, taunt, A, Z, and B; and the animation-command transition to
TurnRun with its resumed cursor, facing, and velocity.
The expanded crouch common-IASA matrix covers neutral A from `Squat`,
`SquatWait`, and `SquatRv`; neutral B accepted from `Squat` but rejected from
`SquatWait` and `SquatRv`; and physical Z entering `Catch` from `Squat` while
falling back through its A component to `Attack11` from `SquatWait` and
`SquatRv`, where the common catch check is absent. The special and attack
fixtures remain original project content, so this comparison asserts the
common transition eligibility at entry and resumes exact shared-state
comparison only after the route settles.
Down-special entry is accepted from all three crouch states, matching their
shared `ftCo_800D68C0` check. Those character-specific bodies are isolated at
the end of the corpus and receive the same semantic-entry treatment.

The grounded-player-push route is qualified separately by a 540-frame
Final Destination Falcon-versus-Falcon capture. It drives the approach from
both controller ports and in both directions, compares both fighters' actions,
action frames, facing, grounded state, positions, and self-induced velocities,
and allows no more than one 0.3-unit push nudge plus the ordinary float32
position envelope at the strict overlap boundary.

The analog shield-input/health route is qualified separately by a 500-frame
Final Destination capture. It samples both sides of the common dead zone,
light, intermediate, near-dense, simultaneous-shoulder, and digital-full input,
then release and regeneration. Action/state and shield health remain strict
within the existing `0.0009765625` float32 health gate; normalized shield pressure allows only
one unit of 16-bit conversion error. Shield collision damage, stun, pushback,
tilt smoothing, and exact collision geometry are not inferred from this
pressure-only route.

Shield-hit response is qualified separately by three 283-frame identical-input
Final Destination captures at requested light, intermediate, and dense
pressure. The comparator checks both fighters' discrete state, position,
self-velocity, health/pressure, hitlag, shield stun, defender pushback, and the
attacker recoil independently inferred from executable position delta minus
self velocity. The implementation follows the pinned decomp and owner-disc
common table: integer shield-hit conversion; pressure-dependent damage and
stun; post-hitlag ordering; and a separate attacker-recoil component with
ground/air decay. The three captures pass with strict `0.00048828125` float32
component gates and the established `0.009765625` float32 position envelope. Exact shield tilt, collision
geometry, and uncaptured shield-hit routes remain active work.

The platform routes are qualified separately by a 348-frame Battlefield
capture. A neutral jump passes upward through the platform, crosses it on
descent for one final airborne frame, then enters `Landing` on the same frame
as Dolphin. A one-frame down tap is a negative control; held down produces
displayed `Squat` frames 1-3, then `Pass` frame 0 with the executable's 0.63
downward speed, and lands on the solid floor on the same frame as Dolphin.
The ordinary laboratory-stage runner remains isolated from this Battlefield
fixture.

The production player-push implementation follows pinned
`ftCommon_8007DD7C`/`ftCommon_8007E0E4` behavior for active grounded players on
the same project support. These passing traces are regression slices, not
evidence that every applicable movement or shared-simulation route is
equivalent; the other remaining systems still require identical-input Dolphin
reproducers and comparable-state assertions.

## 2026-08-08 - Prior-art-first gate

Before starting any substantive implementation, tooling, infrastructure,
algorithm, data-format, or behavior-fidelity work item, perform a focused
prior-art sweep. Check the authoritative upstream, maintained forks, releases,
issues, documentation, and established workflows that could already solve or
materially simplify the item. Record the relevant source revision or release,
the reusable part, and why it was selected or rejected before building a
custom replacement. A custom implementation is allowed only after this sweep
shows that the maintained prior art is missing, incompatible, insufficiently
exact, or slower after measurement.

For Dolphin executable-oracle acceleration, the first qualified candidate is
Vlad Firoiu's ExiAI Slippi/libmelee path rather than a project-authored Dolphin
fork. The project must pin its release artifact and Python package versions,
run headless with null video and disabled audio, enable its EXI-input
fast-forward path, and prove identical relevant game state and collision
observations against an unaccelerated control before accepting captures as M4
evidence. Experiment traces sharing a match configuration should be batched in
one process; repeated GUI launches at real-time speed are not an acceptable
default workflow. Save-state or persistent-process extensions remain optional
only when measurements show material benefit beyond the qualified prior art.

## 2026-08-08 - Massively fast executable-oracle validation

The ordinary edit loop must not launch Dolphin. It runs source/import checks,
stored authoritative identical-input traces, deterministic simulation and
replay checks, and changed-case selection from one machine-readable coverage
manifest. After the build, this no-Dolphin suite targets at most 2 seconds.

Live qualification uses one persistent headless/null/unlimited Dolphin process
containing many short, checkpoint-isolated cases. Each case declares its
checkpoint, ordered inputs, observed fields, source rows and callback branches,
and exact or bounded comparison policy. The runner restores the checkpoint
between cases, batches cross-process memory observation, and emits one aggregate
artifact. A changed-domain warm run targets at most 3 seconds and the complete
warm Falcon pack targets at most 10 seconds. Missing either budget is an active
performance defect.

One continuous scenario is not sufficient because state leaks between actions
and failures become difficult to localize. No finite scenario can detect every
possible anomaly either. Completeness is therefore an explicit, extensible
manifest accounting for every imported table row, action-frame pose,
transition, callback branch, and claimed positive/negative physical boundary.
