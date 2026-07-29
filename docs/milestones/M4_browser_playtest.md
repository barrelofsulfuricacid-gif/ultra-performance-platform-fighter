# M4 real-simulation browser playtest

This checkpoint runs the production `pf_sim_tick` M4 movement, solid stage
geometry, two standing ground attacks, ground/wall/ceiling tech and
missed-impact recovery, directional air dodge, wavedash/waveland,
hit-reaction, and dense-shield primitives in
WebAssembly. It is no longer the
disposable M0 float32/Q16.16 comparison. Both visible players use the same
validated M4 fighter and stage content used by native, replay, rollback, and
headless execution.

## Controls

| Action | Player 1 | Player 2 |
|---|---|---|
| Full left/right input and horizontal DI | `A` / `D` | Left / Right |
| Reduced-magnitude walk | `Shift+A` / `Shift+D` | `Shift+Left` / `Shift+Right` |
| Jump | `W` or `Space` | Up |
| Up/down stick and vertical DI | `W` / `S` | Up / Down |
| Light ground attack | `F` | `/` or Numpad `0` |
| Strong ground attack | `H` | `'` or Numpad `2` |
| Hold shield / tap tech or air-dodge trigger | `G` | `.` or Numpad `1` |
| Crouch, platform drop, fast fall | `S` | Down |
| Reset both players | `R` or Reset button | Same |
| Pause/resume | `P` or Pause button | Same |
| One tick while paused | `N` or Step button | Same |

Unmodified horizontal keys emit full stick magnitude and can enter initial
dash. Reversing them during the ten-tick initial-dash window performs a
dash-dance reversal. Holding `Shift` emits a reduced magnitude below the dash
threshold and therefore walks.

Once `INITIAL DASH` has transitioned to `RUN`, a full opposite input enters
`RUN TURNAROUND`, not another initial dash. The placeholder fighter uses a
data-driven 12-tick turnaround. Holding at least 0.625 stick magnitude toward
the new direction on its final tick returns to `RUN`; releasing or using a
sub-threshold direction enters `RUN BRAKE`. A turnaround-completed run cannot
enter another turnaround or run brake for ten ticks. These state and threshold
rules follow the documented Melee `RUN`/`TURNRUN` behavior in
[ShyPF's dash-and-run analysis](https://shypf.blogspot.com/p/an-overvie.html).

Jump has a three-tick jump squat. Release during jump squat for the one fixed
short-hop launch speed; hold through takeoff for the one fixed full-hop launch
speed. Releasing or continuing to hold after takeoff cannot change the selected
apex.

Once airborne, horizontal input changes drift but does not change the direction
the fighter faces. An air jump also preserves facing, even when performed while
holding the opposite horizontal direction. Turn on the ground before takeoff
when an inward or outward airborne facing is required.

A fresh shield/tech-key press while ordinarily airborne performs an air dodge.
Hold a stick direction with it for a fixed-speed directional dodge; neutral
stick zeros both velocity components. Facing never follows the air-dodge
direction. `AIR DODGE` decays momentum without gravity, becomes invulnerable on
action ticks 3–28, and enters `FALL SPECIAL` after 49 ticks if it does not land.
`FALL SPECIAL` restores gravity/fast fall, limits horizontal drift, allows a
legal ledge catch, and locks ordinary air actions.

For a wavedash, tap and release jump during the three-tick jump squat, then on
the first airborne frame hold down-left or down-right and press the trigger.
Floor contact enters `SPECIAL LANDING`, preserves the horizontal air-dodge
component, and slides under traction for action ticks 0–9 before returning to
idle. The same diagonal contact on a pass-through platform performs a
waveland; the downward dodge lands instead of invoking ordinary platform
drop-through. The state card exposes the exact action tick for these checks.

To grab a ledge, fall beside it while facing inward. After the seven-tick catch
window, press toward the stage to climb, press down or away to release, or press
jump for a ledge jump. A claimed ledge rejects another fighter until its current
occupant releases or completes the climb.

The light jab has two startup ticks, two active ticks, eight recovery ticks,
6% damage, and four hitlag ticks. Its translucent amber rectangle is the exact
inspected active hitbox. The strong attack has five startup ticks, three active
ticks, 18 recovery ticks, 12% damage, six hitlag ticks, and a pink active
hitbox. Its first clean hit against a fresh default fighter exceeds the
32-tick tumble threshold. After the frozen hitlag pose, a tumbling fighter
visibly rotates and the state card prefixes its action with `TUMBLE`. The cards
also show percent, hitstun, SDI pulse count, tech window/lockout, tech
direction, and the last combat-event sequence.

During target hitlag, crossing into a new horizontal or vertical stick
component produces one SDI pulse. Holding that component does not repeat it.
The final hitlag input also supplies ASDI and trajectory DI, with full
perpendicular input reaching the configured 18-degree maximum.

`G`, `.`, and Numpad `1` drive one normalized analog trigger. Hold the key from
idle, walk, crouch, or run to raise the full-density shield; initial dash
cannot shield until it reaches run. A run-to-shield transition keeps momentum
and slides under traction as a shield stop. The translucent player-color
bubble shrinks with the inspected 60-point shield health.

Holding shield drains 0.28 health per tick. Releasing before eight ticks keeps
the shield active until that minimum completes, then starts the 15-tick
`SHIELD RELEASE`; jumping cancels an already active shield or release. Shield
health regenerates by 0.07 per non-shield tick.

Blocking the current physical attack prevents percent and launch, freezes both
players in hitlag, applies damage-scaled pushback, and resumes the defender in
`SHIELD STUN`. Raising shield within four ticks of contact powershields the
attack: the defender takes no shield damage but keeps ordinary physical
hitlag/stun and receives the larger Melee-style pushback. The state card shows
shield health, shield stun, and a powershield indicator.

After a physical powershield, release shield by the end of shield stun. Frame 1
of `SHIELD RELEASE` cannot start a ground attack; a fresh attack press on frame
2 cancels the remaining release animation into the current attack. An early
attack is not buffered. Holding shield until stun ends consumes the opportunity,
and an ordinary block retains the full 15-tick release.

A new trigger press still opens a 20-tick tech window and a 40-tick lockout. A
tumbling floor/platform impact with neutral horizontal input enters
`TECH IN PLACE`; holding left or right enters `TECH ROLL`; missing the window
enters `KNOCKDOWN`. Use the strong attack for an immediate default-content
tumble test; the light jab intentionally remains below the threshold on a
fresh fighter. Tech in place lasts 26 ticks and tech roll lasts 40; both reject
hits for their first 20 ticks, shown by a dashed gold ring and the
`invulnerable` state-card field. Missed-tech knockdown has no such protection
and lasts 26 ticks before entering `DOWN WAIT`.

From `DOWN WAIT`, press up or make a fresh shield press for `NEUTRAL GETUP`,
press left/right for `GETUP ROLL`, or press either attack key for `FLOOR
ATTACK`. Neutral getup lasts 30 ticks with 23 invulnerable; getup roll lasts 35
ticks with 19 invulnerable; floor attack lasts 49 ticks with 26 invulnerable.
The floor attack deals 6% and attacks in front on frames 17–19, then behind on
frames 24–26. Prone states render as a flattened fighter, both attack phases
draw their inspected purple hitbox, and the same dashed gold ring shows the
exact recovery invulnerability.

The raised block is production collision geometry, not decoration. Its top can
be landed on, its floor-level clearance can be traversed, and its sides and
underside stop body motion. Strong-launch a tumbling target into a side and
press the target's tech key inside the 20-tick window for `WALL TECH`; hold up
for `WALL TECH JUMP`. Launch upward into the underside for `CEILING TECH`.
Successful surface techs clear hitstun/tumble and show the gold
invulnerability ring. Missing the input produces `WALL BOUNCE` or `CEILING
BOUNCE`, reflects and scales the launch, and keeps tumble/hitstun active.

This shield slice does not yet include analog light shield, shield tilt/poke,
shield SDI, shield roll/spot dodge, platform shield drop, grab, projectile
reflection, or the complete airborne/knockdown/stun shield-break sequence.
Future ground actions must join the same powershield-cancel router before that
registry row can advance from `playable` to `verified`.

## Focused owner checks

1. Tap left and right rapidly without `Shift`. Confirm each reversal occurs
   during `INITIAL DASH` without a neutral key press.
2. Hold `Shift+A` and `Shift+D`. Confirm the state inspector says `WALK` and
   movement is visibly slower than unmodified `A`/`D`.
3. Hold a direction until the inspector says `RUN`, then press the opposite
   full direction. Confirm `RUN TURNAROUND`, never `INITIAL DASH`, appears
   before the fighter begins running the other way. Release from a run and
   confirm `RUN BRAKE`.
4. Perform ten very quick jump taps. Confirm every short hop reaches the same
   height.
5. Hold jump through takeoff, then vary when it is released. Confirm every full
   hop reaches the same height and is higher than every short hop.
6. Jump while facing right, hold left until moving left, then hold right until
   moving right. Confirm the facing indicator stays right throughout both drift
   directions and an air jump. Repeat after turning left on the ground.
7. Full hop and press up-right plus the trigger while airborne. Confirm `AIR
   DODGE`, unchanged facing, the gold invulnerability ring only on action ticks
   3–28, and `FALL SPECIAL` if the dodge finishes without touching a surface.
   Keep holding the trigger and confirm it never starts a second dodge.
8. Tap jump, release during jump squat, then on the first airborne frame press
   down-right plus the trigger. Confirm immediate `SPECIAL LANDING`, a
   horizontal slide with unchanged facing, action ticks 0–9, then idle. Repeat
   with neutral stick and confirm it does not create the diagonal slide.
9. Full hop upward through the moving platform. On descent, press down-left or
   down-right plus the trigger just above it. Confirm `SPECIAL LANDING` uses
   platform support and preserves horizontal momentum instead of dropping
   through.
10. Use an airborne fresh jump, reverse aerial drift around the apex, fast-fall
   with down, land on the moving platform, then press down to drop through it.
11. Approach the right edge in `RUN`, press left before leaving it to enter
   `RUN TURNAROUND`, and let the retained rightward momentum carry the fighter
   off while facing inward. Confirm `LEDGE HANG`, then try neutral hang,
   down/away release, jump, and inward climb.
12. Put one player on a ledge and attempt to grab it with the other player.
   Confirm only the original occupant enters `LEDGE HANG`.
13. Move into range, press `F`, and confirm the amber hitbox appears only on the
   active frames. On contact, confirm the target gains 6%, both players visibly
   freeze, and the target then launches in `HITSTUN`.
14. Attack facing away and confirm the active hitbox whiffs. Reset, bring both
    players into range, and attack on the same tick to confirm a simultaneous
    trade.
15. Pause just before contact and use `N` to step through hitlag. On the first
    target hitlag tick, press a horizontal direction and confirm `SDI pulses`
    becomes 1 and the target shifts. Keep holding it for another tick and
    confirm the count stays 1. Add the vertical component and confirm the count
    becomes 2.
16. Compare otherwise similar launches while holding perpendicular opposite
    vertical directions on the final hitlag tick. Confirm the visible launch
    vector changes while the state remains deterministic after Reset.
17. Tap the target's tech key and confirm the state card shows
    `tech window 20` and `lockout 40`. Hold the key through the next tick and
    confirm they count down to 19/39 rather than reopening.
18. Move into range and press `H` (or Player 2's strong-attack key). Confirm the
    pink hitbox, 12% damage, six frozen hitlag ticks, then visible rotation with
    `TUMBLE · HITSTUN`. Press the target's tech key within 20 ticks of landing.
    Use neutral horizontal input for `TECH IN PLACE`, then repeat while holding
    a direction for `TECH ROLL`. Repeat without the tech input and confirm
    `KNOCKDOWN`. Confirm the gold invulnerability ring clears after 20 ticks
    while each tech action continues.
19. Let the missed tech finish into `DOWN WAIT`. Try up and a fresh shield
    press for `NEUTRAL GETUP`, left and right for both `GETUP ROLL`
    directions, and either attack key for `FLOOR ATTACK`. Pause and step
    through the floor attack: confirm purple hitboxes in front on frames
    17–19 and behind on frames 24–26, with no hitbox in between. Confirm the
    gold ring clears at each option's documented invulnerability boundary.
20. Move both fighters near the raised block and strong-launch the target into
    its side. Tap the target's tech key during tumble for `WALL TECH`; repeat
    while holding up for `WALL TECH JUMP`, then omit the tech input for `WALL
    BOUNCE`. Repeat with an upward launch into the underside for `CEILING TECH`
    and `CEILING BOUNCE`. Confirm successful techs clear tumble/hitstun while
    missed impacts retain them.
21. From idle, hold the shield key. Confirm the bubble appears on frame 1,
    health drains, an early key release waits for the eight-tick minimum, and
    `SHIELD RELEASE` lasts 15 ticks. Press jump during shield/release and
    confirm `JUMP SQUAT`.
22. Reach `RUN`, then hold shield. Confirm `SHIELD` replaces `RUN` while the
    fighter slides forward and slows under traction. Reset, press shield during
    `INITIAL DASH`, and confirm the fighter does not shield until run.
23. Hold shield for more than four ticks and block an attack. Confirm no
    percent is added, shield health drops, both fighters freeze, and the
    defender resumes in `SHIELD STUN`. Repeat by raising shield immediately
    before contact; confirm the powershield indicator appears, shield health
    loses only its normal hold depletion, and pushback is larger.
24. After that powershield, release shield before `SHIELD STUN` ends. Leave the
    first `SHIELD RELEASE` tick neutral, then press the defender's attack key
    on frame 2 and confirm it enters `GROUND ATTACK`. Repeat after an ordinary
    block and confirm the attack cannot skip the 15-tick release.
25. Repeat with Player 2's arrow-key controls and try both players
    simultaneously.

Record any mismatch with the control used, the visible tick/action state, and
whether it repeats after Reset.

## Automated browser contract

Before the interactive loop appears, the Wasm module runs the real simulation
through:

- reduced keyboard magnitude producing `WALK`;
- full magnitude producing `INITIAL DASH`;
- an opposite full magnitude producing an immediate dash-dance reversal;
- an opposite full magnitude after `RUN` producing `RUN TURNAROUND`, never a
  new initial dash;
- two different short-hop release timings producing the same apex;
- two different post-takeoff full-hop hold durations producing the same apex;
- opposite-direction aerial drift and an opposite-direction air jump changing
  velocity without changing takeoff facing;
- a full-hop directional air dodge reaching its exact invulnerability window
  and `FALL SPECIAL`, plus a first-airborne-frame diagonal short-hop air dodge
  entering `SPECIAL LANDING` and retaining horizontal slide;
- a real grounded attack producing the configured damage, hitlag, attacker
  identity, and canonical combat event; and
- a default strong attack producing 12%, six hitlag ticks, at least 32 hitstun
  ticks, and canonical tumble state;
- an exact 26-tick missed-tech animation entering `DOWN WAIT`, all three
  floor-recovery input routes, their initial invulnerability, and both active
  phases of the floor attack;
- a production-path target SDI pulse producing a positional shift;
- a trigger edge producing the 20-tick tech window and 40-tick lockout, with a
  held trigger counting down rather than retriggering;
- default-content positioning, a real strong launch into the raised block,
  an in-flight trigger edge plus held up input, and observed
  `WALL_TECH_JUMP` with cleared hitstun/tumble and active invulnerability;
- a normal physical shield block producing zero percent, shield damage,
  shield stun, hitlag, and ordinary pushback;
- a physical attack inside the four-tick powershield window producing zero
  shield damage and the powershield result;
- release after that physical powershield preserving the cancel opportunity,
  followed by the one-frame delay and frame-2 ground attack; and
- the native movement oracle covering ledge catch, hang, release, jump, climb,
  simultaneous occupancy, and mid-climb save/load equivalence.

The page reports
`playtest=ready input_probe=pass air_facing_probe=pass combat_probe=pass reaction_probe=pass
shield_probe=pass powershield_cancel_probe=pass tumble_probe=pass
floor_recovery_probe=pass surface_tech_probe=pass air_dodge_probe=pass
controls=keyboard-two-player` only after all checks pass.
Clean-machine Chrome CI also requires that status and the live playtest DOM.
