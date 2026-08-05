# M4 SSBM behavior-fidelity audit

## Meaning of status

- `equivalent`: the relevant decomp route and numeric data have been mapped and
  deterministic verification exists for the supported scope.
- `partial`: the major route exists, but one or more frame, animation-command,
  collision, or character-data details still differ.
- `divergent`: the current behavior intentionally or accidentally differs.
- `missing`: no production-path counterpart exists.

No row implies whole-game equivalence. This audit covers the single M4 Falcon-
movement placeholder on the original laboratory stage.

Equivalence is behavioral. Small, bounded numeric differences caused by the
simulator's Q16.16 representation are acceptable when the verifier reports and
justifies them; discrete action/state, timing, facing, grounded-state,
threshold, and route differences are not.

## Current audit

| System | Status | Evidence and remaining gap |
|---|---|---|
| Stick aging, dead zones, dash/jump recognition | equivalent | Fresh horizontal/vertical tilt age, reversal reset, 0.80 dash threshold with two-tick dash window, and 0.6625 tap-jump threshold with four-tick window follow the common input/decomp routes. Just-below/above and slow/two-sample tap-jump controls match the executable. |
| Initial dash and dash physics | equivalent | One-shot Falcon 2.0 impulse with no entry-frame displacement, full A/B dash acceleration from the next frame, held transition after 15 displayed dash frames, and released completion after 28 displayed dash frames match the executable oracle. |
| Walk/run acceleration and friction | equivalent | The generated typed view of Falcon's raw NTSC 1.02 common attributes drives the runtime friction-aware target/overshoot formulas; slow stick motion enters walk rather than dash. |
| Dash dance and backward dash acceleration | equivalent | A fresh reversal enters one displayed frame of smash `TURNING` with the old facing and damped velocity; a held reversal then enters opposite dash with the measured residual momentum plus Falcon's impulse. |
| Run braking and common IASA | equivalent for captured routes | Neutral from terminal run produces 28 displayed `RUN_BRAKE` frames with Falcon's 0.08 friction before standing. Jump and main-stick down enter frame-1 `KneeBend`/`Squat`; shield-plus-down keeps crouch priority. Opposite stick on displayed brake frame 2 enters displayed TurnRun frame 1 with the old facing, resumed cursor, and 0.16 acceleration. Neutral guard, C-stick roll/spot, taunt, A, Z, and B remain in RunBrake. The executable and simulator agree for the complete captured IASA matrix. |
| Standing turn | equivalent for captured routes | Smash turn flips on the following frame and can enter dash; basic turn flips on displayed frame 8 and completes after displayed frame 11. A fresh second-frame taunt applies the turn's facing flip first and then enters Falcon's 60-frame taunt. Timing and friction routes match the executable oracle. |
| Run turnaround | equivalent for captured route | Full reversal from terminal run retains the old facing, applies full TurnRun acceleration, freezes displayed frame 9 until velocity crosses the common 0.01 threshold, flips facing on the following physics tick, resumes through displayed frame 21, and enters the ten-tick locked run route. The identical-input oracle covers the complete held reversal and neutral brake after exit. |
| Jump squat and takeoff momentum | equivalent | Falcon startup 4, 0.75 retained momentum, 0.95 stick contribution, and 2.1 cap are mapped. X/Y and main-stick tap jump match from idle, Landing, shield, and air. |
| Short/full hop | equivalent | Falcon 1.9 and 3.1 vertical velocities are converted to stage units. |
| Double jump | equivalent | Horizontal velocity is replaced from neutral/stick input using Falcon's 0.9 multiplier; vertical velocity uses the 0.9 multiplier. |
| Gravity, terminal velocity, air drift | equivalent | Falcon A/B acceleration, drift target, friction, gravity, terminal, and absolute horizontal cap are mapped. |
| Fast fall | equivalent | Requires a fresh downward tilt within four ticks after descent begins; holding down before the apex does not trigger it. |
| Crouch/crawl | equivalent for captured routes | Full-down input produces Falcon's seven displayed `Squat` frames, held `SquatWait`, ten displayed `SquatRv` frames, then standing. Exact 0.6875 entry and 0.625 release boundaries preserve the decomp's hysteresis. Jump, fresh guard, fresh taunt, neutral A, and down-special from all three states match, as do held-crouch dash/turn and release-state walk. Neutral B is accepted only from `Squat`. Physical Z enters `Catch` from `Squat`, but its A component falls back to `Attack11` from `SquatWait`/`SquatRv`, where catch is absent. `Squat` and `SquatWait` are crouch-cancel eligible while `SquatRv` is not; crawl entry remains disabled because Falcon cannot crawl. A separate Battlefield route proves the one-frame-down negative control and held-down `Squat` frames 1-3 into `Pass`. |
| Ground and platform collision | partial | The isolated 348-frame Battlefield route matches neutral jump-through ascent, the final ordinary-airborne descending crossing before `Landing`, 0.63 Pass entry speed, and same-frame solid-floor landing. A separate 540-frame Final Destination Falcon-versus-Falcon route matches grounded push from both directions and controller ports, with strict discrete state/velocity comparison and one bounded Q16.16-delayed overlap transient. General stage collision primitives and broader ECB evolution remain unqualified. |
| Ledge jump velocities | equivalent | Falcon 1.0 horizontal and 3.3 vertical attributes are mapped. |
| Other ledge actions | partial | Hang, drop, climb, roll, attack, regrab lockout, and invulnerability exist, but exact animation-command and percent-dependent ledge tables are not imported. |
| Wall jump / wall and ceiling tech velocities | equivalent | Falcon passive-wall, wall-jump, and passive-ceiling attributes are mapped. |
| Normal landing lag and shared IASA | equivalent for captured routes | Falcon's four-frame value is mapped. Identical-input taunt, jump, dash/turn, guard, walk, direct-crouch, late-down-lockout, and ordinary-turn routes open or remain locked on the executable's exact boundary. First-frame down enters `SquatWait` directly; down one displayed frame later remains `Landing`. Character attack/special/grab content remains outside this row. |
| Aerial landing lag | equivalent | Distinct neutral/forward/back/up/down landing states select Falcon's 15/19/18/15/24 table; L-cancel states halve the selected value. |
| Shield input, light shield, size, tilt, and volume | equivalent for captured routes | A 500-frame pressure sweep matches sub-threshold through digital-full input, health, release, and regeneration. Three 283-frame light/intermediate/dense physical-hit captures additionally match integer shield-hit conversion, pressure-dependent health/stun, post-hitlag ordering, defender pushback, and separately decaying attacker recoil. New 270- and 2,158-frame memory-probed executable captures qualify guard-angle and magnitude smoothing, all eight linear direction-animation keys, Falcon's joint-derived center, health/pressure radius, facing reflection, and the anisotropically mapped elliptical volume. |
| Roll, spot dodge, air dodge buffering | partial | Production paths and per-trigger edge tracking exist. Air-dodge force, dead zone, decay, and post-dodge drift cap are imported with axis-specific unit conversion; exact action/animation tables remain authored. |
| Damage, knockback, hitlag, hitstun, DI/SDI | partial | The physical shield-hit subset is decomp-mapped and qualified at three pressure bands. Jab 1, jab 2, dash attack, the three tilts, the three explicit smashes, and the five directional aerials now select imported per-sphere damage, angle, KBG, set-weight, BKB, and hitlag through Melee's fixed-point response. The jab-1 zero-percent Dolphin capture reports three hitlag frames, 13 hitstun frames, and the expected launch. Knockback decay, DI, and the remaining action geometry are incomplete. |
| Attacks, grabs, throws, stale moves | partial | A pinned importer retains Falcon's complete 50-slot timing/effect/throw schema: all 48 concrete subactions plus the source-defined absence of the mid-high/mid-low forward-smash slots, with every present action flag and per-frame translation-N delta decoded from the raw animation DAT. Production routes consume exact imported timing for jab 1/2, dash attack, all tilts, all explicit smashes, all five aerials, standing/dash grab, aerial landing lag, pummel, all four normal throws, ground/air Falcon Punch, ground/air Raptor Boost, and ground/air Falcon Dive. Separately hash-pinned Dolphin captures supply transformed hit geometry and complete 11-capsule poses for those 16 actions and all 17 Falcon special subactions; simultaneous sweet/weak spheres retain independent effects, while Raptor Boost uses its separately typed six-sphere search geometry. Grounded normals use their distinct decomp IASA policies: jab chain, full Wait, restricted down tilt, and forward smash without escape. Root translation drives jab, dash attack, forward smash, grounded Falcon Punch, Raptor Boost, and Falcon Dive where their physics callbacks consume it; the remaining normals apply source friction semantics. Standing grab uses 5/2/22 startup/active/recovery and two live spheres on executable frames 7-8, while dash grab uses its distinct 9/2/28 state and three live spheres on frames 11-12. Grab resolution filters the target's current captured pose by the executable `grabbable` bit. Throw release frames are decoded from the original DAT action scripts and throw response uses imported damage/angle/KBG/WDKB/BKB through the shared integer Melee formula. Frame lookup preserves inactive gaps, late-hit changes, aerial frame 44, and hitlag-frozen poses. Common-state hurt poses, source-Z collision semantics, exact sphere-versus-shield intersection, normal-throw collateral hits, aerial IASA, and Falcon Kick remain incomplete. The custom generic strong-aerial/special fixtures intentionally remain outside Falcon equivalence. |
| Special moves and recovery | partial | All 17 Falcon special subactions, all 97 common-attribute words, the complete 35-field special-attribute block, raw animation translation, command timelines, effects, six Raptor Boost search spheres, and complete-frame executable pose/hit geometry are imported and hash-pinned. Both importers regenerate byte-for-byte from their pinned inputs. Ground/air Falcon Punch uses source duration, command-variable frames, root motion, angle/velocity attributes, ground/air collision transitions, pose, and hit geometry; two at-will direct Dolphin traces cover 200 frames each. Ground/air Raptor Boost uses source 0.6 selection and 0.2 turnaround thresholds, ground velocity multiplier, frames 15-34/18-34 searches, frame-30 air gravity start, root translation, 7-damage hit effects, 20/40-frame miss/hit landing lag, and ground/air hit states. Its at-will 46-frame ground-hit differential passes strict action/velocity checks and the 640-Q16 position envelope. Ground/air Falcon Dive now uses the source start/catch/throw actions, grab spheres, 5% catch, 12% throw, hitlag, grounded throw relocation, root motion, and falling ECB bottom. Its 116-frame grounded catch/throw differential passes strict discrete and velocity checks plus the 640-Q16 position envelope. Falcon Kick remains to be production-routed and qualified; the original Prism Burst, Arc Reservoir, and Vector Ascent fixtures do not count as equivalents. |
| Items, projectiles, reflector, charge | divergent | These are original technique-support fixtures rather than SSBM content tables. |
| Stage geometry, blast zones, spawns | divergent | The Relay Rod laboratory is an original test stage, not an SSBM stage. |
| Stocks, respawn, match result | partial | Deterministic four-stock flow exists; all tournament-rule and revival-platform details are not decomp-equivalent. |
| Replay, save/load, rollback state, RL API | project-specific | These are deterministic project infrastructure and have no claim of equivalence to SSBM internals. |

## Blocking work before a behavioral-equivalence claim

1. Import and route the remaining common action/animation-command timings for
   shield, dodges, ledges, techs, landing, and unaudited brake interrupts.
2. Audit common damage, knockback, shield, hitlag, hitstun, DI, SDI, stale-move,
   crouch-cancel, and collision formulas field by field.
3. Route Falcon Kick from the imported timing, attributes, animation
   translation, and pose/hit geometry, then qualify each source state and
   collision transition against Dolphin. Extend Falcon Dive qualification to
   aerial start/catch/throw and extend Raptor Boost qualification
   to air, miss/edge, and source item-search routes.
4. Capture remaining common-state hurt poses and qualify source-Z, sphere-
   versus-shield, and normal-throw collateral collision semantics without
   duplicating constants.
5. Validate native Windows, WSL Linux, Wasm/browser, replay, save/load, and
   rollback results from the same content hash.

## Executable-oracle evidence

`tools/capture_ssbm_movement.py` drives an owner-supplied GALE01 NTSC-U 1.02
image through Dolphin/Slippi and records the post-frame action, facing,
position, velocity, and observed controller sample. `pf_m4_movement_trace`
replays those observed samples through the native simulator, and
`tools/compare_ssbm_movement.py` stops at the first behavioral divergence.

The current comparison passes 8,675 identical input frames covering held
dash/run, complete run turnaround and post-turnaround lockout, released dash
and run brake, direct dash dancing, moving dashbacks, two-sample dash
recognition, smash and empty pivots, basic standing turn including its
second-frame taunt/facing order, slow-stick sweep,
shield/light shield and defensive escapes, jump/air movement/landing, and
Falcon's complete full-down crouch start/hold/release sequence, exact and
just-beyond entry/release threshold samples, jump interruption from every
crouch state, held-crouch opposite dash/turn, crouch-release walk, and fresh
digital guard and fresh taunt from every crouch state, including Falcon's
complete 60-frame taunt duration, and first-legal-frame normal-Landing taunt,
jump, dash/turn, guard, walk, direct crouch, and ordinary turn plus the
one-frame-late down-input lockout. It additionally covers main-stick tap-jump
full/short hop, aerial jump, Landing and shield entry, threshold boundaries,
slow-sweep age rejection, and two-sample in-window recognition.
The expanded route also covers common RunBrake IASA and crouch common-IASA
entry: neutral A from all three crouch states; neutral B accepted only from
`Squat`; and physical Z selecting `Catch` from `Squat` but `Attack11` from
`SquatWait`/`SquatRv`. Because fighter attacks and specials remain original
content, those routes compare semantic eligibility at entry, skip the
character-specific action body, then resume exact comparison at the next
stationary anchor. Down-special entry from all three crouch states is also
covered with its character-specific bodies isolated at the end of the corpus.
A separate 348-frame Battlefield capture covers ordinary jump-through and
platform landing plus the final crouch platform-pass route. The neutral jump
retains Dolphin's final airborne crossing before `Landing`; one-frame down
release does not drop; held down exposes `Squat` frames 1-3, enters `Pass`
frame 0 at 0.63 downward speed, and lands on the solid floor on the
executable's frame.
A separate 540-frame Final Destination Falcon-versus-Falcon capture drives
grounded approach from both controller ports and directions, comparing both
fighters' action/state, action frame, facing, grounded state, position, and
self-induced velocity. Its position gate reports a 2,692-Q16 bound: the
ordinary 640 float-to-Q16.16 envelope plus one mapped 0.3-unit push nudge for a
one-tick strict-boundary transient. The other captures retain the 640-Q16
position gate; action, facing, velocity, and applicable action ticks use their
tighter independent gates. This remains a regression slice, not evidence that
the whole shared simulation has completed the binding equivalence gate.

A separate 500-frame Final Destination analog-shield capture covers both sides
of the common dead zone, four accepted pressure bands across both shoulder
inputs, simultaneous shoulders, digital full shield, release, and regeneration.
It compares action/state, shield health, and normalized pressure; the pressure
gate accepts only one unit of 16-bit conversion error.

Separate 270- and 2,158-frame memory-probed shield-geometry captures compare
the simulator against live GALE01 guard magnitude, biased guard angle, shield
joint world center, and transformed sphere radius. The sweep covers all eight
45-degree animation keys and intermediate linear samples. The comparator
accounts explicitly for the controller/post-frame pipeline offset, maps the
Melee sphere into the project's independent horizontal and vertical units,
and stops on angle, magnitude, center, or radius divergence outside the small
fixed-point gates.

Three separate 283-frame physical shield-hit captures request light,
intermediate, and dense pressure. They compare both fighters' discrete state,
facing, grounded state, position, self velocity, shield health/pressure,
hitlag, shield stun, and the attacker recoil inferred independently from the
executable's position delta minus self velocity. Component gates are exact or
32 Q16 units; position retains the established 640-Q16 conversion envelope.
Their SHA-256 values are
`563cabf633126656b80a0351b67fdffb35f664774e052e85c04ff7b20fd2e4f5`,
`84b462f717074b2a2984b6901ed33a2abd2b9f98527f1c52db400c98ace411ab`,
and `2d95549b7ffe6ac950c339fe9dcd346b4e6c401324d2cce0e8414d2677a3489f`.

A separate 271-frame Falcon jab capture reads the source fighter's live
hitboxes and the target's ordinary damage state. All three active hitboxes
report damage 2, angle 80, KBG 100, set weight 20, and BKB 0. At zero percent,
the target receives three hitlag frames and 13 hitstun frames; captured Melee
velocity is -0.17242245 x and 0.97785604 y before the documented coordinate
conversion. Capture schema 8 SHA-256 is
`2660274136b77aef393db391c85582be7795bee7360ebd6607325e437ac9af04`.

A separate 543-frame Jab 1 interruption capture pulses jump and guard one
displayed frame before and exactly on imported IASA frame 16, then holds a
horizontal stick through the same boundary. Dolphin keeps frame-15 jump and
both guard pulses in `Attack11`, starts `KneeBend` from the frame-16 jump,
enters Walk from the pre-held stick, retains displayed frame 21, and returns to
Wait on the next tick. The identical-input comparison passes with the ordinary
640-Q16 position envelope. Capture SHA-256 is
`d17f8e9a7dfc3c1a0d260d3ffe3c7fd9c3e2b5f89f3f17b1c3ce9e7218a8f427`.
Across the main and isolated corpora, the current aggregate executable-oracle
evidence is 14,154 captured frames. The memory-probed routes qualify the
sampled Falcon shield tilt and geometry surface; they do not qualify broader
uncaptured pressure/time/spacing routes or the other partial/divergent systems
listed above.
