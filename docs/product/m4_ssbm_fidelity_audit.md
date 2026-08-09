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
| Initial dash and dash physics | equivalent | One-shot Falcon 2.0 impulse with no entry-frame displacement, full A/B dash acceleration from the next frame, held transition after 15 displayed dash frames, and released completion after 28 displayed dash frames match the executable oracle. Every displayed Dash hurt pose is imported; a pinned Falcon Jab 1 route hits at 31.0 Melee units and misses at 31.5, while the old generic body rectangle misses the positive route by 3.503404617 units. |
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
| Crouch/crawl | equivalent for captured routes | Full-down input produces Falcon's seven displayed `Squat` frames, held `SquatWait`, ten displayed `SquatRv` frames, then standing. Exact 0.6875 entry and 0.625 release boundaries preserve the decomp's hysteresis. Jump, fresh guard, fresh taunt, neutral A, and down-special from all three states match, as do held-crouch dash/turn and release-state walk. Neutral B is accepted only from `Squat`. Physical Z enters `Catch` from `Squat`, but its A component falls back to `Attack11` from `SquatWait`/`SquatRv`, where catch is absent. `Squat` and `SquatWait` are crouch-cancel eligible while `SquatRv` is not; crawl entry remains disabled because Falcon cannot crawl. A separate Battlefield route proves the one-frame-down negative control and held-down `Squat` frames 1-3 into `Pass`. Every CrouchStart/CrouchEnd hurt pose is imported; Jab 1 hits CrouchStart frame 3 at 17.7 units and misses at 17.84, while the old rectangle falsely reports +0.596595764 overlap for the miss. |
| Ground and platform collision | partial | The isolated 348-frame Battlefield route matches neutral jump-through ascent, the final ordinary-airborne descending crossing before `Landing`, 0.63 Pass entry speed, and same-frame solid-floor landing. A separate 540-frame Final Destination Falcon-versus-Falcon route matches grounded push from both directions and controller ports, with strict discrete state/velocity comparison and one bounded Q16.16-delayed overlap transient. General stage collision primitives and broader ECB evolution remain unqualified. |
| Ledge jump velocities | equivalent | Falcon 1.0 horizontal and 3.3 vertical attributes are mapped. |
| Other ledge actions | partial | Hang, drop, climb, roll, attack, regrab lockout, and invulnerability exist, but exact animation-command and percent-dependent ledge tables are not imported. |
| Wall jump / wall and ceiling tech velocities | equivalent for captured routes | Falcon passive-wall, wall-jump, and passive-ceiling attributes are mapped. A five-case, 719-row Hyrule capture qualifies wall tech, wall-tech jump, wall reflection, ceiling tech with held drift, and ceiling reflection. Production consumes imported collision thresholds, 0.8 reflection, five-frame wall freeze, 14-frame wall-tech invulnerability, 15-frame reflection invulnerability, three-frame re-collision lock, and 31/45/26 action durations. The response comparator matches actions, timers, tumble/grounded/invulnerability, 0.49/-0.13 wall release, 1.39/2.97 wall-jump release, 0.06 ceiling drift, and frame-11 1.99 control release within 0.0015 source units. Absolute position remains outside this route because Hyrule and the production fixture are separate geometry domains. |
| Normal landing lag and shared IASA | equivalent for captured routes | Falcon's four-frame value is mapped. Identical-input taunt, jump, dash/turn, guard, walk, direct-crouch, late-down-lockout, and ordinary-turn routes open or remain locked on the executable's exact boundary. First-frame down enters `SquatWait` directly; down one displayed frame later remains `Landing`. The no-input executable route still plays ordinary Landing's complete 30-frame source motion; every pose is imported, and a pending frame-22 Jab 1 discriminator hits at 20.3 units and misses at 20.6 while the generic rectangle misses both. Character attack/special/grab content remains outside this row. |
| Aerial landing lag | equivalent | Distinct neutral/forward/back/up/down landing states select Falcon's 15/19/18/15/24 table; L-cancel states halve the selected value. |
| Shield input, light shield, size, tilt, and volume | equivalent for captured routes | A 500-frame pressure sweep matches sub-threshold through digital-full input, health, release, and regeneration. Three 283-frame light/intermediate/dense physical-hit captures additionally match integer shield-hit conversion, pressure-dependent health/stun, post-hitlag ordering, defender pushback, and separately decaying attacker recoil. The 270- and 2,158-frame memory-probed geometry captures qualify guard-angle and magnitude smoothing, all eight linear direction-animation keys, Falcon's joint-derived center, health/pressure radius, facing reflection, and the anisotropically mapped volume. A separate 2,568-frame, 33-decision Jab 1 sweep matches the decomp sphere/shield predicate and three last-hit/first-miss boundaries. |
| Roll, spot dodge, air dodge buffering | equivalent for captured routes | A 329-frame identical-input route covers forward roll, spot dodge, backward roll, held-L/fresh-R upward air dodge and landing, then horizontal LandingFallSpecial from a fresh down-left air dodge. EscapeN/EscapeF/EscapeB/EscapeAir duration and invulnerability come from their generated catalog/script rows. Both roll directions use the one source TransN stream selected by the decomp callback, including residual entry momentum, facing, and end position; no parallel authored displacement remains. EscapeAir force/dead zone/entry-frame decay come from `PlCo.dat`, displayed frame 30's variable-0 write switches the unchanged action to ordinary aerial physics, and a 48-frame live ECB-bottom sequence selects the exact landing tick. LandingFallSpecial copies incoming horizontal velocity to ground velocity, applies the common high-speed friction multiplier only above Falcon's walk maximum, then ordinary ground friction below it; its submotion has no TransN, and no authored displacement remains. Every captured flat-stage position delta equals post-friction velocity. Action, tick, grounded state, facing, invulnerability, and velocity compare strictly; position retains only the 640-Q16 representation envelope. Per-trigger edge tracking preserves held-L/fresh-R shoulder behavior. A separate 4,198-row geometry oracle imports all 32 SpotDodge poses, both distinct 31-frame roll tracks, all 49 AirDodge poses, the eight-frame FallSpecial loop, LandingFallSpecial's displayed source frames `1,4,...,28`, and all 30 ordinary Landing poses. It confirms SpotDodge vulnerable frames 1-2/21-32, roll vulnerable frames 1-3/20-31, and AirDodge vulnerable frames 1-3/30-49. Jab 1 hits SpotDodge frame 24 at 21.0 and misses at 22.0; RollForward frame 22 at 12.98 and misses at 14.18; RollBackward frame 24 at 20.00 and misses at 20.75; elevated AirDodge frame 31 at 21.0 and misses at 21.8; FallSpecial frame 5 at 15.5 and misses at 16.2; and LandingFallSpecial source frame 7 at 18.5 and misses at 19.3. The generic rectangle gives the wrong result for each discriminator. Uncaptured starting heights/momenta, slopes, platform edges, and collision cases remain broader-route work. |
| Tech and neutral getup body state | partial | The generated script decoder covers all 318 submotions and exposes raw state-2/state-0 command frames. A four-case, 804-row Final Destination domain now qualifies flat-floor missed tech, neutral tech, and both directional techs: 26/26/40/40 displayed frames, frames 1-20 tech invulnerability, retained hitstun memory, imported forward/backward `TransN`, and the source 220-frame DownWait value. Wall/ceiling tech timing, invulnerability, release velocity, and ceiling control are separately Dolphin-qualified. DownBound's ECB-driven grounded toggles, complete pose geometry, DownWait/getup choices, directional getup rolls, getup attacks, slopes, ledge departure, and pushbox-coupled position still need separate qualification. |
| Damage, knockback, hitlag, hitstun, DI/SDI | partial | The physical shield-hit subset is decomp-mapped and qualified at three pressure bands. Jab 1, jab 2, dash attack, the three tilts, the three explicit smashes, and the five directional aerials select imported per-sphere damage, angle, KBG, set-weight, BKB, and hitlag through Melee's fixed-point response. A six-case, 138-row live Falcon route qualifies open-air self/knockback channel separation, source-order 0.051 magnitude decay, neutral/full/half and squared-projection DI, radial SDI threshold behavior, analog displacement, and C-stick ASDI priority. A separate 64-row late-DashAttack route qualifies 15 flat-ground damage samples: Sakurai-angle projection, distinct `xF0_ground_kb_vel`, Falcon's 0.08 friction, `DamageLw1` timing, and the animation-plus-hitstun release boundary. Declared state, timer, and velocity fields match within 0.001 Melee units; position is reserved for the separate pushbox domain. The wall/ceiling domain additionally qualifies reflected action/hitstun persistence, bounce invulnerability, tech clearing/preservation differences, and release/control velocities. The flat-floor domain qualifies missed/neutral/directional tech response and retained landing channels. Slopes, DownBound ECB pose-grounding, getup branches, pushbox-coupled position, and wider attack interactions remain incomplete. |
| Attacks, grabs, throws, stale moves | partial | The pinned importer retains Falcon's complete 50-slot timing/effect/throw schema: all 48 concrete subactions and both source-absent angled forward-smash slots, every action flag, and all per-frame TransN deltas. Production consumes imported timing/effects for jab 1/2, dash attack, every tilt/smash/aerial, standing/dash grab, pummel, all normal throws, all Falcon specials, and the exact 9/8/7/6/5/4/3/2/1 stale table. Hash-pinned captures provide transformed hit geometry and complete 11-capsule poses for the 16 ordinary actions, 17 special subactions, Initial Dash, RunBrake, CrouchStart, CrouchEnd, KneeBend, SpotDodge, RollForward, RollBackward, AirDodge, FallSpecial, LandingFallSpecial, and ordinary Landing. The common-pose discriminators reject the generic rectangle in both directions. Ground normals use their decomp IASA policies and callback-specific friction/root motion. A 1,250-frame identical-input capture qualifies its 350 actionable jump/jumpsquat/aerial/interrupt frames: the one-frame-early and exact double-jump IASA boundary for fair/bair/uair/dair plus nair's no-IASA control. Melee C-stick input selects these same directional scripts, while non-reference custom content keeps its authored strong aerial. Standing/dash grab, grabbability, imported throw release/response data, inactive hit gaps, late effects, frame 44, hitlag-frozen poses, source X/Y/Z, shared integer Melee response, and state-2/state-3 previous-to-current moving hit-capsule collision are production-routed. A pinned positive and nearby miss control qualify the moving-hit path against Dolphin and the decomp. Other common-state hurt poses, aerial-IASA item/tether branches, and unsampled special dynamics remain incomplete. Original custom strong-aerial/special fixtures remain outside Falcon equivalence. |
| Special moves and recovery | partial | All 17 Falcon special subactions, all 97 common-attribute words, the complete 35-field special-attribute block, raw animation translation, command timelines, effects, six Raptor Boost search spheres, and complete-frame executable pose/hit geometry are imported and hash-pinned. Both importers regenerate byte-for-byte from their pinned inputs. Ground/air Falcon Punch uses source duration, command-variable frames, root motion, angle/velocity attributes, ground/air collision transitions, pose, and hit geometry; two at-will direct Dolphin traces cover 200 frames each. Ground/air Raptor Boost uses source 0.6 selection and 0.2 turnaround thresholds, ground velocity multiplier, frames 15-34/18-34 searches, frame-30 air gravity start, root translation, 7-damage hit effects, 20/40-frame miss/hit landing lag, and ground/air hit states. Its at-will 657-frame suite covers ground hit/miss, aerial miss, a complete 145-frame aerial-hit-to-floor route, 51 ground-edge frames, and a 155-frame native grounded-Capsule item-search route. The item route uses Melee's ambient spawner and item object, isolates the opposing fighter by at least 100 units, and selects the hit state at the first live source command-variable gate. The source predicate excludes the custom Relay Rod and ordinary weapon kinds. The fighter-hit route qualifies search conversion, imported frame-3 damage, hitlag, the entire natural pre-landing recovery tail, exact floor transition with preserved incoming vertical velocity, 40 hit-landing-lag ticks, and return to standing. The edge route qualifies the command-variable gate, full crossing root step, source air-speed clamp, zero-gravity transition row, and common `FallSpecial` continuation. Production consumes all 45 memory-probed `SpecialAirS` ECB-bottom frames; the previous generic body extent landed one tick early. Both miss routes reuse the exact imported common `FallSpecial` pose cycle, and the aerial transition applies common gravity. Ground/air Falcon Dive uses the source start/catch/throw actions, grab spheres, 5% catch, 12% throw, hitlag, grounded throw relocation, root motion, and distinct eight-frame `FallSpecial` ECB bottom. Its at-will verifier covers 116 grounded catch, 92 aerial catch, 103 grounded miss, 165 aerial miss, and 63 aerial ledge-approach frames; 42 aerial victim frames additionally qualify 15.92% post-throw damage, zero launch, ordinary gravity, and the source-visible 26-frame reaction. The ledge route separately verifies exact frame-64 `EdgeCatch` and frame-71 `EdgeHang` from live collision memory. Falcon Kick uses all seven source states, decoded wall/traction/edge/air-physics commands, root motion, hit geometry, imported traction, and ground-hit slowdown/cap. Its at-will 399-frame suite passes ground, air, landing, ground-edge, ground-hit, and Hyrule wall-rebound routes with strict state/velocity/hitlag checks plus the 640-Q16 position envelope, including the source collision conversion's half crossing-tick displacement and no same-tick gravity. The hit route additionally qualifies 15 damage, eight ticks of hitlag, the imported 0.6 speed modifier, and the separate ground/self velocity channels through ground end. The wall route qualifies action 363, preserved entry self velocity, ECB-lock-equivalent transition ordering, and the full rebound trajectory. The original Prism Burst, Arc Reservoir, and Vector Ascent fixtures do not count as equivalents. |
| Items, projectiles, reflector, charge | divergent | These are original technique-support fixtures rather than SSBM content tables. |
| Stage geometry, blast zones, spawns | divergent | The Relay Rod laboratory is an original test stage, not an SSBM stage. |
| Stocks, respawn, match result | partial | Deterministic four-stock flow exists; all tournament-rule and revival-platform details are not decomp-equivalent. |
| Replay, save/load, rollback state, RL API | project-specific | These are deterministic project infrastructure and have no claim of equivalence to SSBM internals. |

## Blocking work before a behavioral-equivalence claim

1. Route the complete imported common submotion catalog through the remaining
   shield, ledge, tech, damage, item, and unaudited brake command/
   callback semantics. Decode the lossless script stream into typed generated
   tables after qualifying displayed-frame semantics; do not re-transcribe its
   bytes or frame lengths.
2. Audit common damage, knockback, shield, hitlag, hitstun, DI, SDI, stale-move,
   crouch-cancel, and collision formulas field by field.
3. Capture common-state hurt poses beyond Initial Dash/RunBrake/CrouchStart/
   CrouchEnd/KneeBend/SpotDodge/RollForward/RollBackward/AirDodge/
   FallSpecial/LandingFallSpecial/Landing and qualify remaining aerial-IASA branches and unsampled dynamic
   routes without duplicating constants. Looping motions require exact phase/
   rate reconstruction rather than a guessed linear pose track.
5. Validate native Windows, WSL Linux, Wasm/browser, replay, save/load, and
   rollback results from the same content hash.

## Executable-oracle evidence

`tools/capture_ssbm_movement.py` drives an owner-supplied GALE01 NTSC-U 1.02
image through Dolphin/Slippi and records the post-frame action, facing,
position, velocity, and observed controller sample. `pf_m4_movement_trace`
replays those observed samples through the native simulator, and
`tools/compare_ssbm_movement.py` stops at the first behavioral divergence.

The post-build edit loop additionally runs
`tools/verify_ssbm_stored_equivalence.py`. Its generic registry selects affected
domain manifests, rejects stale generated rows, runs each filtered production
oracle, and then requires the pinned deterministic replay corpus. The first
registered domain, `falcon-common-hurt`, hashes all 255 production-accessed
poses and runs 20 manifest-owned hit/miss controls in 116.845-120.355 ms on
Windows and 148.121-166.786 ms in WSL across five warm runs. This is regression
against already-qualified live truth; it does not turn uncovered routes into
evidence or replace a fresh Dolphin qualification when a golden changes.
The corresponding live common-hurt pack executes eight checkpoint-isolated
cases in one headless/null/unlimited ExiAI process, serializes 255 declared
poses plus 28 discriminator rows, and passes five fully verified warm runs in
2.635-2.729 seconds against a manifest-owned three-second budget.

The second registered domain, `falcon-common-damage-response`, uses the same
registry and an allocation-free numeric-trace runner. Six manifest-owned cases
compare three canonical samples each, covering neutral, full and half DI,
radial and below-radial SDI, and C-stick-priority ASDI. Its source observation
SHA-256 is
`51402cd3605ba2761e3c11ed6baab74eb1b7ab22136822507b39d0a00cc40d95`
and its production trace SHA-256 is
`91f1664c3c81795cf10bcfd6777a6d5934f48017cb073dabe9fb4d98fe9b745e`.
The live damage pack completes its 138 rows in 0.665 seconds warm and 3.438
seconds for the full process lifecycle.

The third registered domain, `falcon-common-ground-knockback`, contributes one
late-DashAttack case and 15 damage samples. Its source observation SHA-256 is
`e08d7149e3f46d814d5c4a709e316cf3063208bb9673141effe6b1958f03fc79`
and its production trace SHA-256 is
`954931140122b77790e334b4d1742709c853fcc060fdc692fc9e45522ff7a379`.
All three domains cover 27 cases and, with the replay gate, complete in about
0.305 seconds on Windows and 0.310 seconds in WSL. The live ground pack is
0.128 seconds warm and 2.801 seconds end to end. Position is explicitly
excluded from this domain because
the chosen route also exercises player push; slopes and remaining collision
response need their own live routes before their goldens can be admitted.

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

A separate 329-frame defense-state capture has SHA-256
`d9dfebcb6e42f5e71ece08490429b61083f81bee067def379b5fdd6270d96b95`.
It qualifies forward roll, spot dodge, backward roll, and a held-L/fresh-R
upward air dodge through ordinary-physics handoff and floor landing, plus a
horizontal special landing that crosses the above-walk/ordinary friction
threshold. Its same-binary control SHA-256 is
`d78abcfe3d252d0f87409aba3343cd838efb739d6311494d520f2f076eb5255f`.
The comparator now checks invulnerability as an exact discrete field in addition
to action, tick, grounded state, facing, velocity, and bounded position. This
route exposed and removed duplicate backward-roll displacement, decoded the
EscapeAir frame-30 command-variable gate, and replaced generic air-dodge floor
extent with the captured 48-frame ECB bottom.
It also removed a parallel authored LandingFallSpecial displacement: the
source submotion has no TransN, and the decomp callback applies only ground
friction and post-friction ground movement.

Across the main and isolated corpora, the current aggregate executable-oracle
evidence is 18,697 qualified frames. The memory-probed routes qualify the
sampled Falcon shield tilt and geometry surface; they do not qualify broader
uncaptured pressure/time/spacing routes or the other partial/divergent systems
listed above.
