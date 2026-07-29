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
facing, even when the horizontal stick points opposite that facing. A
deliberate down input after the apex enters the fixed fast-fall speed. Landing
enters a finite landing state.

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
under the default block remains traversable. Crossing any support edge enters
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
  platform-drop timer, respawn count, active ledge claim, ledge points,
  moving-platform bounds, solid-block bounds, and blast zones.

## Verification

`tests/sim/test_m4_movement.c` and `tools/verify_m4_movement.sh` cover:

- content validation, content-hash rejection, and a data-tuning effect;
- proportional walk, initial dash, run, dash-dance reversal, run turnaround,
  turnaround lockout, run brake, facing, traction, and crouch;
- binary short/full hops, double jump, aerial drift, airborne-facing lock
  across opposite drift and air-jump input, fast fall, and landing;
- moving-platform landing/carry, ledge geometry/catch/hang/release/jump/climb,
  one-occupant priority, mid-climb save/load equivalence, platform drop, and
  blast-zone respawn;
- inspectable solid geometry, floor-level traversal beneath it, ordinary side
  and underside collision, and landing/support on its top; and
- a 20,000-tick four-player trace whose canonical state must remain valid and
  hashable after every tick.

The focused movement oracle currently reports 35 invariants.
