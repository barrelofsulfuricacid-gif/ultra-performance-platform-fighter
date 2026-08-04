# M4 movement vertical-slice contract

## Scope

The 2026-08-03 Falcon-fidelity pass supersedes earlier statements in this
document that describe all movement numbers as original placeholders. Imported
NTSC 1.02 fields, unit conversions, and remaining divergences are authoritative
in `m4_falcon_ntsc102_data_provenance.md` and `m4_ssbm_fidelity_audit.md`.

This contract promotes the accepted Q16.16 movement experiment into the real
simulation. The first original placeholder content consists of one tuning
fighter definition and one moving-platform test stage. These identifiers,
values, and geometry are provisional design data, not final character or stage
content.

The simulation uses screen-oriented coordinates for this slice: positive X is
right and positive Y is down. Fighter positions are center points. Floors and
pass-through platforms are horizontal surface heights.

## Input and movement rules

The browser polls up to two gamepads using the
[W3C Standard Gamepad layout](https://www.w3.org/TR/gamepad/#remapping) every
simulation tick in addition to its two keyboard slots. The left stick is
deterministically quantized from `[-1, 1]` into signed input magnitude after a
0.2 browser dead zone, while the D-pad emits full magnitude. The first two
connected standard mappings are assigned in browser index order; non-standard
mappings are ignored rather than guessed, except for the explicitly identified
Mayflash `0079:1843` GameCube adapter. Its main stick and C-stick normalize the
adapter's approximately 0.75 cardinal gate report to full magnitude, and empty
adapter ports are skipped. The same adapter in Wii U mode is supported through
WebUSB as `057e:0337`: an explicit owner gesture grants access, the browser sends
the native `0x13` start command, and four-port 37-byte reports preserve raw
sticks, C-stick, buttons, and analog triggers. Hot-plugging does not add state
to the simulation.

- Main-stick magnitude below the dash threshold produces proportional walking.
  Reaching the dash threshold before the data-defined X-tilt timer expires
  enters initial dash, including a low first sample followed by a full second
  sample. Taking longer to sweep from the lower movement threshold to the dash
  threshold commits to walking, so gradually moving the stick produces a fast
  walk rather than a dash.
- This threshold/timer decision follows the pinned Melee decomp's
  [`ftCo_Dash_CheckInput`](https://github.com/doldecomp/melee/blob/9509dc04406fb2028bfab01243841ba4787c0fb7/src/melee/ft/chara/ftCommon/ftCo_Dash.c#L30-L48)
  and
  [X-tilt timer update](https://github.com/doldecomp/melee/blob/9509dc04406fb2028bfab01243841ba4787c0fb7/src/melee/ft/fighter.c#L1898-L1949).
- Reversing the full horizontal value during initial dash enters `STANDING
  TURN` for one displayed frame with the old facing. Holding the reversal on
  the next frame flips facing and enters tick 1 of the opposite initial dash;
  no neutral sample is required. The reversal frame applies Melee's dash IASA
  damping followed by Falcon traction before the opposite dash impulse.
- Releasing horizontal input does not truncate Falcon's dash animation. A held
  direction transitions to `RUN` after 15 displayed dash frames; a released
  dash remains `INITIAL DASH` through displayed frame 28 and becomes grounded
  idle on the next frame. A fresh full input after completion can start another
  dash, while a slowly aged input enters `WALK`.
- A one-tick full reversal during initial dash is the pivot window. Returning
  to neutral on the next tick remains in `STANDING TURN`, flips to the reversed
  facing, and preserves the traction-reduced backward slide; pressing a ground
  action on that tick acts with the same facing and momentum. Holding the
  reversal enters tick 1 of the opposite initial dash. A reversal after `RUN`
  has begun uses `RUN TURNAROUND` instead.
- A non-smash opposite horizontal tilt from idle or walk enters the basic
  standing turn. Falcon keeps the rendered old facing through displayed frame
  7, flips on frame 8, remains `STANDING TURN` through frame 11, and returns to
  idle on the next frame. A fresh taunt on displayed frame 2 first applies
  that frame's facing flip, then enters `TAUNT`, matching the common Turn IASA
  ordering.
- Full direction plus the ordinary light-attack edge enters the production
  forward strong attack directly from idle. Holding that same direction for
  one through the data-defined three initial-dash ticks before pressing light
  retains the traveled distance and traction-controlled dash velocity, making
  the same authored hitbox reach farther. A frame-4 press or a press after the
  direction is released remains the ordinary non-smash ground attack. The
  opposite-direction form has only the one-tick pivot window.
- Once initial dash has become `RUN`, an opposite input at or above the
  data-defined 0.375 threshold enters `RUN TURNAROUND`, never another initial
  dash. The old facing is retained while full TurnRun acceleration reduces the
  old ground velocity. The animation freezes on displayed frame 9 until that
  velocity crosses the common 0.01 threshold, flips facing on the following
  physics tick, then resumes through displayed frame 21. Holding at least the
  data-defined 0.625 threshold toward the new direction returns to `RUN` and
  starts the data-defined ten-tick run lockout.
- Neutral, sub-threshold, or weak backward input from an unlocked run enters
  `RUN BRAKE`. A turnaround-completed run has a data-defined ten-tick window
  during which another run turnaround or run brake cannot begin.
- Down from an unlocked `RUN` cancels directly into `CROUCH`, retaining a
  traction-reduced forward slide and exposing immediate grounded actions.
  Jump and shield provide the other current production dash-cancel routes.
  Shield remains excluded during `INITIAL DASH`, and down cannot cancel the
  locked `RUN TURNAROUND` state.
- Tapping shield for one tick from `RUN` preserves that run momentum through
  the data-defined eight-tick minimum hold, then enters the 15-tick
  `SHIELD RELEASE` while ordinary traction finishes the slide. Holding shield
  follows the same traction path but remains `SHIELD`; raising shield from
  idle has no inherited horizontal travel. This is the dashing-shield route.
- While already in `SHIELD` on pass-through support, down in the validated
  reduced-input band `[0.375, 0.5)` immediately enters ordinary `AIRBORNE`,
  applies the authored platform nudge, and opens the same nine-tick
  pass-through timer as a normal drop. The lower boundary is inclusive and the
  upper boundary is exclusive. Full down instead keeps priority as
  `SPOT DODGE`; too little tilt remains `SHIELD`, and releasing the trigger
  uses ordinary grounded `SHIELD RELEASE`. The route cannot start on solid
  floor or on the same tick shield is raised.
- Client keyboard adapters must expose full and reduced magnitude on both axes.
  The browser uses reduced horizontal input for walk and reduced down for
  shield platform dropping rather than collapsing every key press into a full
  value.
- Ground acceleration, turn acceleration, traction, walk speed, run speed,
  initial-dash speed/window, forward-smash input window,
  run-turnaround duration/threshold/lockout,
  run-brake duration, shield-drop threshold, aerial acceleration, aerial speed,
  and facing are
  deterministic data fields.
- Airborne horizontal stick input changes the aerial target velocity and
  acceleration only. It does not change facing. Facing is inherited at
  takeoff and can change only through an explicit action transition, never
  from ordinary aerial drift.
- Down on the floor enters `CROUCH START` at displayed action tick 1. Falcon's
  authored entry lasts through tick 7; continued down then enters held
  `CROUCH` at tick 1. Releasing down from the held state enters `CROUCH END` at
  tick 1, remains there through tick 10, then returns to ground idle on the
  following tick. `CROUCH START` and held `CROUCH` are crouch-cancel eligible;
  `CROUCH END` is not. Entry requires down strictly beyond 0.6875 (the first
  accepted signed-axis value is 22528); held crouch releases strictly below
  0.625 down (the exact signed-axis boundary 20479 remains held). These timings,
  transitions, and boundary cases match the pinned GALE01 executable trace.
  Jump interrupts all three crouch states. Held crouch accepts a fresh dash
  (including the ordinary opposite-facing `TURN` frame), while crouch release
  accepts forward walk immediately. Fresh digital guard interrupts every
  crouch state through the common shield-start path. Fresh taunt also
  interrupts every crouch state and remains active for Falcon's 60 observed
  AppealS frames. Remaining attack, special, grab, and platform-pass routes
  remain part of the broader executable-oracle gate.
- Down on a pass-through platform starts the data-defined nine-tick
  pass-through window. The drop tick applies only the authored nudge and
  ordinary gravity; it cannot also trigger fast fall. Subsequent down input may
  fast-fall normally.

## Jump rule

Short hop and full hop are two discrete launches:

1. A fresh X/Y edge or fresh main-stick up tilt at or above the data-defined
   0.6625 threshold within the four-tick tilt window enters jump squat.
2. Releasing the selected jump input before jump squat completes selects the
   short-hop launch speed.
3. Holding the selected jump input through jump squat selects the full-hop
   launch speed.
4. Once launched, continuing to hold or releasing jump cannot alter that
   launch speed or apex.

Therefore jump-input duration chooses between two heights; it never
continuously scales jump height. Slowly sweeping the main stick through the up
threshold after the four-tick window does not jump, while a two-sample sweep
inside it does. The mechanical oracle compares early and late releases within
each category and requires exact matching apexes.

An airborne fresh X/Y or in-window main-stick-up jump consumes one configured
air jump without changing facing, even when the horizontal stick points
opposite that facing. Instant
double jump is the earliest legal case: release the first jump during jump
squat, then send a fresh jump edge on the first airborne tick. The takeoff tick
is explicitly excluded, holding one jump input cannot retrigger it, and an
exhausted air jump is rejected. The accepted edge applies the configured
double-jump velocity before that tick's ordinary gravity and motion, so the
result is deterministic across save/load and rollback.

The default fighter authors a six-tick double-jump-cancel window. A legal air
jump enters `DELAYED_AIR_JUMP`; a fresh light or strong aerial during action
ticks `[0, 6)` cancels the remaining upward velocity to zero before that
tick's ordinary gravity and motion, then enters the corresponding existing
aerial action. If no aerial is selected, action tick 6 returns to `AIRBORNE`
without cancelling the jump's remaining rise. An aerial selected on that
first late tick therefore keeps the full jump arc. Simultaneous fresh jump and
attack while ordinarily airborne gives the attack priority and does not
consume an air jump. A configured window of zero disables the delayed action.
The delayed state remains eligible for ordinary ledge catch and V-cancel rules
because both are airborne interactions. All state needed for rollback is the
existing action ID and action timer.

The cancel topology follows the delayed-double-jump behavior documented in
[SmashWiki's double jump cancel description](https://www.ssbwiki.com/Double_jump_cancel);
the six-tick window, velocities, and fighter data remain original placeholders.

That same delayed action is also the movement side of double jump cancel
counter. A qualifying physical hit freezes the existing position, velocity,
and action tick through ordinary hitlag, then resumes `DELAYED_AIR_JUMP`
without launch. The air jump stays consumed and the remaining authored cancel
window is preserved, so a fresh aerial immediately after the freeze uses the
same vertical-momentum cancellation rule above. Late or sufficiently strong
hits launch through the ordinary combat path. The armor threshold and reaction
oracles are defined in the combat contract; movement adds no counter-only
state.

A deliberate down input after the apex enters the fixed fast-fall speed.
Landing enters a finite landing state. Falcon's first common interrupt is
available after displayed `LANDING` frame 4; the internal zero-based timer is
compared as the following displayed frame. Identical-input routes pin taunt,
jump, dash/turn, guard, walk, crouch, and ordinary turn on that boundary. The
landing-specific crouch check enters held `CROUCH`/`SquatWait` directly only on
that first legal frame; down one displayed frame later remains locked in
`LANDING` rather than starting the ordinary seven-frame crouch entry.

## Grounded rolls and spot dodge

A supported fighter can enter one of three locked grounded defensive actions
through the same normalized trigger used by shield, tech, air dodge, and
L-cancel:

- A fresh full horizontal input while the trigger is held enters a roll.
  Pressing trigger and direction together is legal, as is raising shield first
  and then flicking the direction.
- Horizontal input matching fixed facing selects `ROLL_FORWARD`; the opposite
  direction selects `ROLL_BACKWARD`. A forward roll flips facing and a
  backward roll preserves it, so either finishes facing opposite travel.
- The secondary stick remains a distinct normalized axis pair. Holding it
  horizontally while shield is requested buffers the direction through the
  one shield frame and enters the roll without requiring a fresh secondary-
  stick edge. Holding it through shield stun likewise takes the roll on the
  first eligible shield frame. Holding full secondary-stick down uses the
  same route to buffer spot dodge, and holding full secondary-stick up buffers
  jump squat. The main-stick paths remain edge-triggered.
- Secondary down has priority over a simultaneous horizontal secondary-stick
  direction and shield grab, while horizontal roll has priority over
  secondary up. These priorities match Melee's guard router: spot dodge, then
  roll, then grab, then jump.
- Releasing shield after its minimum hold does not discard a held vertical
  secondary-stick option. Down cancels `SHIELD_RELEASE` into spot dodge and up
  cancels it into jump squat; horizontal secondary input intentionally cannot
  roll from shield release. A held secondary-up jump produces a full hop, and
  releasing it during jump squat latches the ordinary short hop.
- Fresh down plus trigger selects `SPOT_DODGE`. If down and a full horizontal
  edge arrive together, spot dodge has priority.
- Reduced down while already shielding is below the spot-dodge threshold and
  therefore remains available to the pass-through-platform shield-drop route.
- A direction held before the trigger is not fresh and cannot start a roll.
  This negative case applies to the main stick; the held secondary-stick
  buffer is intentional. Down held before the trigger likewise produces
  ordinary shield instead of a spot dodge. Initial dash and other locked
  actions remain excluded.

The held secondary-stick thresholds and shield action routing follow the
pinned Melee decomp's
[horizontal, down, and up C-stick predicates](https://github.com/doldecomp/melee/blob/e5c34839555716e305891df8023d15dba8c18bc0/src/melee/ft/ft_0DF1.c#L212-L233),
[roll and spot-dodge selection](https://github.com/doldecomp/melee/blob/e5c34839555716e305891df8023d15dba8c18bc0/src/melee/ft/chara/ftCommon/ftCo_Escape.c#L60-L76),
and
[guard input priority](https://github.com/doldecomp/melee/blob/e5c34839555716e305891df8023d15dba8c18bc0/src/melee/ft/chara/ftCommon/ftCo_Guard.c#L490-L498).
The C-stick jump's hold-versus-release result follows the decomp's
[source-aware short-hop check](https://github.com/doldecomp/melee/blob/e5c34839555716e305891df8023d15dba8c18bc0/src/melee/ft/chara/ftCommon/ftCo_KneeBend.c#L43-L53).

All timing is deterministic fighter data. The current original placeholder
uses:

| Action | Total ticks | Motion | Invulnerability |
|---|---:|---|---|
| Forward roll | 31 | `9/50` Q16 units/tick on action ticks `[3, 20)` | action ticks `[4, 17)` |
| Backward roll | 35 | `4/25` Q16 units/tick on action ticks `[3, 20)` | action ticks `[4, 17)` |
| Spot dodge | 25 | none | action ticks `[3, 16)` |

The half-open ranges make startup and recovery visibly punishable. Rolls use
the ordinary production surface and solid-side collision path: a solid wall
clips motion without tunneling, while crossing a floor/platform edge clears
support and enters airborne movement. The action does not acquire a separate
presentation-only position or invulnerability flag.

The entry priority and relative-to-facing selection follow the grounded escape
structure in the pinned Melee decomp's
[common escape path](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_Escape.c).
The placeholder speeds, durations, movement window, and invulnerability
windows above are independently authored data, not copied character frame
tables.

## Aerial attacks, auto-cancel, and L-cancel landing

A fresh light-attack edge from ordinary non-tumbling airborne movement enters
the first original aerial attack. Its placeholder data defines four startup
ticks, five active ticks, 23 recovery ticks, an 8% facing-mirrored hitbox, and
five hitlag ticks. Aerial drift, gravity, and fast fall remain active throughout
the action. Each target can be hit once through the same production collision,
ownership, hitlag, damage, launch, and hitstun path as a ground attack.

A fresh strong-attack edge from the same airborne state enters
`STRONG_AERIAL_ATTACK`. It deliberately reuses the current strong attack's
five-startup, three-active, 18-recovery, facing-mirrored hitbox, 12% damage,
launch, and six-hitlag data. This gives the browser playtest a strong,
easy-to-see aerial without adding presentation-only combat behavior.

The aerial defines a landing-lag-active half-open action-tick window `[4, 25)`.
Landing outside that window auto-cancels into the ordinary four-tick `LANDING`
state. Landing inside it normally enters `AERIAL_LANDING` for 12 ticks.

A fresh normalized trigger edge maintains a separate saturating age counter.
During an aerial, that edge arms L-cancel timing instead of starting an air
dodge. Trigger ages 0–6 are eligible; age 7 is the exact expired boundary.
Landing inside the active window while eligible enters `L_CANCEL_LANDING` for
the integer quotient of ordinary aerial lag and the fixed divisor: 12 / 2 =
six ticks. The independent counter prevents tech lockout from silently changing
the L-cancel window.

Landing while `STRONG_AERIAL_ATTACK` is active always uses its independently
validated 30-tick `STRONG_AERIAL_LANDING` test lag. An eligible trigger age
selects `STRONG_L_CANCEL_LANDING` and the same fixed divisor reduces that lag to
15 ticks. Both landing states are fully locked; fresh attack, movement, shield,
and jump inputs cannot bypass their timers. The ordinary light aerial retains
its auto-cancel window and 12/6-tick landing route.

This transition and timer structure follows the pinned Melee decomp's
[aerial landing selection](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_AttackAir.c),
[auto-cancel/L-cancel branch](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_LandingAir.c),
and
[fresh-trigger age update](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/fighter.c).
The exact seven-frame window and halving rule are compatibility constraints.
The placeholder aerial's hitbox, damage, and phase timings are explicit
original content, not copied character move data.

## Directional air dodge and momentum landing

A fresh normalized trigger edge from ordinary non-tumbling `AIRBORNE` enters
`AIR DODGE`. The same trigger remains the ground-tech input; tumbling recovery
therefore retains tech priority instead of being silently converted into a
dodge.

- If both stick axes are strictly inside the fighter's rectangular dead zone,
  entry zeros both velocity components. Otherwise the integer-normalized stick
  angle receives the fighter's fixed 0.50 air-dodge speed, independent of stick
  magnitude outside the dead zone.
- Ordinary aerial drift never changes facing, and neither does air dodge.
- The entry tick uses the full vector. Each later air-dodge tick multiplies
  both components by the data-defined 0.90 decay. Gravity, fast fall, drift,
  jumps, and attacks do not modify the active dodge.
- The default action lasts 49 ticks. With action tick zero as frame 1, its
  data-defined invulnerability covers frames 4–29: action ticks 3–28. Startup
  and all ticks beginning at 29 are vulnerable.
- Finishing in the air enters `FALL SPECIAL`. Gravity and fast fall resume,
  horizontal drift is capped at the data-defined 0.08 mobility, ordinary air
  actions and another air dodge remain locked, and a legal ledge catch is
  still possible.

Touching the floor, a pass-through platform, or the solid block's top during
`AIR DODGE` or `FALL SPECIAL` enters `SPECIAL LANDING`. Vertical velocity is
cleared, horizontal momentum is preserved, and the fighter slides under its
normal traction while all control remains locked for ten ticks. A downward
air dodge collides with a pass-through platform rather than inheriting the
ordinary down-held drop-through rule.

This transition intentionally makes wavedash and waveland production
mechanics: short hop or descend toward a surface, air dodge diagonally into it,
then retain the angle-dependent horizontal component through special landing.
A neutral air dodge provides the negative case because it creates no
horizontal slide. Holding the trigger cannot retrigger the dodge; release and
press are still insufficient once `FALL SPECIAL` has begun.

The same generic support transition makes
[ledge-cancelling](https://www.ssbwiki.com/Ledge-canceling) playable. If
retained landing momentum carries the fighter's center beyond the current
surface bound, support is cleared on that tick and the locked landing action
becomes ordinary `AIRBORNE` with action tick zero. The fixture full-hops
through a stationary pass-through platform, air dodges down-right during the
descent, enters `SPECIAL LANDING` beside the right edge, and slides beyond
support on the first recovery tick. Moving the same landing to platform center
is the geometry control: action ticks 0–9 remain locked before idle. This uses
the shared support-loss path; there is no ledge-cancel action or hidden
technique flag.

The state structure follows the pinned Melee decomp's
[air-dodge entry/decay/collision path](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_EscapeAir.c)
and
[special-fall gravity/drift/landing path](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_FallSpecial.c).
The state-machine implementation remains independently written. Numeric values
covered by the 2026-08-03 provenance record are imported Falcon/common data;
all unlisted values remain provisional project data.

## Stage interaction

The initial stage table defines:

- one finite floor with explicit left and right ledge points;
- one pass-through platform with a deterministic integer triangle-wave motion;
- one raised axis-aligned block that is solid from its top, sides, and
  underside;
- spawn spacing for two- and four-player layouts; and
- top, bottom, left, and right blast boundaries.

A supported fighter inherits the moving platform's exact per-tick displacement.
The block's top is a normal support surface, its sides stop horizontal body
motion, and its underside stops upward body motion. The floor-level clearance
under the default block remains traversable. Top and underside sweeps use the
fighter's full horizontal body extent, so diagonal motion into either corner
resolves at the contacted face before any part of the body can enter the solid.
Exact edge tangency remains non-overlapping. Crossing any support edge enters
airborne movement. Crossing a blast boundary currently performs the M4.1
placeholder respawn and increments the canonical respawn counter; stocks and
match termination enter in M4.2.

Falling beside a floor ledge while facing inward enters a pinned ledge catch.
The catch has a data-derived lockout before neutral hang, down/away release,
ledge jump, or inward climb. A ledge has one occupant; simultaneous catches use
stable lower-player-slot priority. Grabbing restores configured air jumps, and
refreshes the original-equivalent 37-tick ledge-invulnerability timer: the
seven-tick catch followed by 30 additional ticks. The remaining timer survives
release, jump, and climb, rejects production hits while nonzero, and remains
canonical across save/load and replay.

Edge dashing composes those production rules without a dedicated debug state.
After the catch lockout, a ledge jump retains the timer and moves inward. Once
the fighter has risen above the floor plane, a fresh down-inward trigger enters
the ordinary directional air dodge and lands on stage in `SPECIAL LANDING`.
The exact ten-tick landing lock and traction slide are unchanged from wavedash;
with default data, the first actionable ground frame still overlaps the
remaining ledge invulnerability and can immediately start an attack. Waiting on
the ledge until the timer expires produces the same movement route without the
invulnerable overlap.

## Data and inspection

`pf_m4_content` contains the validated fighter and stage tables. Its canonical
SHA-256 identity is calculated field by field; native structure padding is not
hashed. A non-empty `pf_content_view` is rejected when its data or hash is
invalid. The simulation copies validated tables only during initialization.

`pf_m4_inspect` exposes the deterministic movement state and current stage
geometry without placing presentation objects in canonical state. It includes
action, action timer, facing, support, remaining air jumps, fast-fall state,
platform-drop timer, trigger-input age, derived L-cancel eligibility, respawn
count, active ledge claim, ledge points, moving-platform bounds, solid-block
bounds, and blast zones.

## Verification

`tests/sim/test_m4_movement.c` and `tools/verify_m4_movement.sh` cover:

- content validation, content-hash rejection, and a data-tuning effect;
- the combat oracle's standing-versus-small-step forward-smash range fixture,
  exact frame-3/frame-4 and missing-direction boundaries, validated authored
  timing, and mid-route save/load future-hash equality;
- proportional walk, initial dash, run, four-burst same-direction fox-trotting,
  held-run and weak-walk negative cases, mid-rhythm save/load continuation,
  one-tick empty/action pivoting with facing and momentum preservation,
  held-reversal and post-run negative cases, mid-pivot save/load continuation,
  jump/shield dash cancels, sliding run-to-crouch cancel and immediate attack,
  early-shield and run-turnaround negative cases, mid-crouch save/load equality,
  dash-dance reversal, run turnaround, turnaround lockout, run brake, facing,
  traction, and crouch;
- neutral trigger-to-shield behavior; fresh and shield-held forward/backward
  roll entry; spot-dodge down priority; held-direction negative cases; fixed
  facing; exact motion, duration, and invulnerability windows; solid-wall
  clipping; roll-off-edge airborne transition; and mid-dodge save/load future
  equality;
- binary short/full hops, double jump, the six-tick double-jump-cancel window,
  early light/strong momentum cancellation, the first-late full-arc boundary,
  simultaneous jump-plus-attack non-consumption, disabled-content behavior,
  mid-window save/load future equality, plus the combat oracle's preserved
  double-jump trajectory/action tick through armored hitlag and immediate
  aerial cancel after resume, aerial drift, airborne-facing lock
  across opposite drift and air-jump input, fast fall, and landing;
- exact nine-tick platform-drop entry, timer exposure, and the same-tick
  fast-fall exclusion used by the drop-cancel combat route;
- validated shield-drop input-band boundaries, same-tick shield-entry and
  solid-floor exclusions, full-down spot-dodge priority, ordinary shield
  release, and mid-route save/load with 24 future-hash comparisons;
- the ordinary airborne light-attack route, early auto-cancel, normal 12-tick
  aerial landing, six-tick L-cancel landing, exact trigger ages 0–6 versus 7,
  invalid timing data, and mid-aerial timer save/load equivalence;
- the airborne strong-attack route, production strong hit schedule, locked
  30-tick normal landing, locked 15-tick L-cancel landing, and invalid
  strong-landing-lag rejection;
- neutral and directional air dodge, normalized vector/decay, exact
  invulnerability boundaries, facing lock, held-trigger rejection,
  `FALL SPECIAL`, mid-action save/load continuation, and the ordinary-input
  first-airborne-frame short-hop air dodge;
- diagonal floor wavedash and pass-through-platform waveland, preserved
  horizontal momentum, traction slide, and exact ten-tick special landing;
- near-edge waveland momentum cancelling `SPECIAL LANDING` into `AIRBORNE` on
  the first recovery tick, the full ten-tick center-platform control, and
  matching future hashes after a save taken on landing;
- moving-platform landing/carry, ledge geometry/catch/hang/release/jump/climb,
  exact 37-tick catch invulnerability and post-release carry, one-occupant
  priority, mid-climb save/load equivalence, the ordinary-input edge-dash route,
  exact special-landing lock, actionable/invulnerable overlap, expired-timer
  negative case, platform drop, and blast-zone respawn;
- inspectable solid geometry, floor-level traversal beneath it, ordinary side
  and underside collision, landing/support on its top, and mirrored upper-left
  and upper-right inward-drift regressions that never overlap the block; and
- a 20,000-tick four-player trace whose canonical state must remain valid and
  hashable after every tick.

`tools/verify_m4_browser.sh` and the generated-page Chrome smoke additionally
cover the standard-gamepad and native Wii U adapter mapping probes, controller
API availability, live per-tick polling, analog quantization/dead zone, D-pad
override, face and shoulder routes, non-standard rejection, and two-slot
assignment. Real USB permission, hardware reports, and browser-specific device
exposure remain part of the owner playtest.

The focused movement oracle currently reports 243 invariants. The focused
combat oracle reports 584 invariants, including the jump-cancelling attack
threshold/late-input routes, the double-jump-cancel-counter
armor/resume boundaries and the dashing-shield
tap-versus-held boundary, reaction-driven tech-chase routes, and the
frame-perfect drop-cancel hit/snap versus one-tick-late and whiff fall-through
cases, plus V-cancel timing, exclusions, lockout, and scaled-launch behavior.
