# M4 movement vertical-slice contract

## Scope

This contract promotes the accepted Q16.16 movement experiment into the real
simulation. The first original placeholder content consists of one tuning
fighter definition and one moving-platform test stage. These identifiers,
values, and geometry are provisional design data, not final character or stage
content.

The simulation uses screen-oriented coordinates for this slice: positive X is
right and positive Y is down. Fighter positions are center points. Floors and
pass-through platforms are horizontal surface heights.

## Input and movement rules

- Main-stick magnitude below the dash threshold produces proportional walking.
  A full horizontal value can enter initial dash and run.
- Reversing the full horizontal value during initial dash starts a new initial
  dash in the opposite direction without requiring a neutral tick. This is the
  invariant used by keyboard and controller dash-dance tests.
- Once initial dash has become `RUN`, an opposite input at or above the
  data-defined 0.375 threshold enters `RUN TURNAROUND`, never another initial
  dash. Holding at least the data-defined 0.625 threshold toward the new
  direction on the turnaround's final tick returns to `RUN`.
- Neutral, sub-threshold, or weak backward input from an unlocked run enters
  `RUN BRAKE`. A turnaround-completed run has a data-defined ten-tick window
  during which another run turnaround or run brake cannot begin.
- Client keyboard adapters must expose both full-magnitude and reduced-magnitude
  horizontal input. The browser loop will retain explicit walk controls rather
  than collapsing every key press into a full dash value.
- Ground acceleration, turn acceleration, traction, walk speed, run speed,
  initial-dash speed/window, run-turnaround duration/threshold/lockout,
  run-brake duration, aerial acceleration, aerial speed, and facing are
  deterministic data fields.
- Airborne horizontal stick input changes the aerial target velocity and
  acceleration only. It does not change facing. Facing is inherited at
  takeoff and can change only through an explicit action transition, never
  from ordinary aerial drift.
- Down on the floor enters crouch. Down on a pass-through platform drops
  through it for a data-defined exclusion window.

## Jump rule

Short hop and full hop are two discrete launches:

1. Pressing jump enters a data-defined jump-squat state.
2. Releasing jump before jump squat completes selects the short-hop launch
   speed.
3. Holding jump through jump squat selects the full-hop launch speed.
4. Once launched, continuing to hold or releasing jump cannot alter that
   launch speed or apex.

Therefore key duration chooses between two heights; it never continuously
scales jump height. The mechanical oracle compares early and late releases
within each category and requires exact matching apexes.

An airborne fresh jump press consumes one configured air jump without changing
facing, even when the horizontal stick points opposite that facing. Instant
double jump is the earliest legal case: release the first jump during jump
squat, then send a fresh jump edge on the first airborne tick. The takeoff tick
is explicitly excluded, holding one jump input cannot retrigger it, and an
exhausted air jump is rejected. The accepted edge applies the configured
double-jump velocity before that tick's ordinary gravity and motion, so the
result is deterministic across save/load and rollback. A deliberate down input
after the apex enters the fixed fast-fall speed. Landing enters a finite
landing state.

## Grounded rolls and spot dodge

A supported fighter can enter one of three locked grounded defensive actions
through the same normalized trigger used by shield, tech, air dodge, and
L-cancel:

- A fresh full horizontal input while the trigger is held enters a roll.
  Pressing trigger and direction together is legal, as is raising shield first
  and then flicking the direction.
- Horizontal input matching fixed facing selects `ROLL_FORWARD`; the opposite
  direction selects `ROLL_BACKWARD`. Neither action changes facing.
- Fresh down plus trigger selects `SPOT_DODGE`. If down and a full horizontal
  edge arrive together, spot dodge has priority.
- A direction held before the trigger is not fresh and cannot start a roll.
  Down held before the trigger likewise produces ordinary shield instead of a
  spot dodge. Initial dash and other locked actions remain excluded.

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

The state structure follows the pinned Melee decomp's
[air-dodge entry/decay/collision path](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_EscapeAir.c)
and
[special-fall gravity/drift/landing path](https://github.com/doldecomp/melee/blob/c638972460ad11289db50daea8d228ea3fb2c043/src/melee/ft/chara/ftCommon/ftCo_FallSpecial.c).
The placeholder's fixed-point values remain explicit original fighter data,
not a claim that one table represents every Melee character.

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
ledge actions remain canonical across save/load and replay.

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
- proportional walk, initial dash, run, dash-dance reversal, run turnaround,
  turnaround lockout, run brake, facing, traction, and crouch;
- neutral trigger-to-shield behavior; fresh and shield-held forward/backward
  roll entry; spot-dodge down priority; held-direction negative cases; fixed
  facing; exact motion, duration, and invulnerability windows; solid-wall
  clipping; roll-off-edge airborne transition; and mid-dodge save/load future
  equality;
- binary short/full hops, double jump, aerial drift, airborne-facing lock
  across opposite drift and air-jump input, fast fall, and landing;
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
- moving-platform landing/carry, ledge geometry/catch/hang/release/jump/climb,
  one-occupant priority, mid-climb save/load equivalence, platform drop, and
  blast-zone respawn;
- inspectable solid geometry, floor-level traversal beneath it, ordinary side
  and underside collision, landing/support on its top, and mirrored upper-left
  and upper-right inward-drift regressions that never overlap the block; and
- a 20,000-tick four-player trace whose canonical state must remain valid and
  hashable after every tick.

The focused movement oracle currently reports 94 invariants.
