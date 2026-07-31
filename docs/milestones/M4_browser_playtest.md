# M4 real-simulation browser playtest

This checkpoint runs the production `pf_sim_tick` M4 movement, solid stage
geometry, two standing ground attacks, ground/wall/ceiling tech and
missed-impact recovery, reaction-driven tech chasing, directional air dodge,
wavedash/waveland,
light and strong production aerial routes with auto-cancel/L-cancel landing,
grounded forward/backward rolls, spot dodge, shield platform drop,
hit-reaction, and dense-shield
primitives plus a four-stock KO, respawn, invulnerability, sudden-death, and
result/rematch loop and a deterministic combat-event feed in
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
| Light ground/aerial attack; full-direction forward smash | `F` | `/` or Numpad `0` |
| Direct strong ground/aerial attack | `H` | `'` or Numpad `2` |
| Hold shield / tap tech, air-dodge, or L-cancel trigger | `G` | `.` or Numpad `1` |
| Grounded forward/backward roll | Trigger + fresh `A` / `D` | Trigger + fresh Left / Right |
| Grounded spot dodge | Trigger + fresh `S` | Trigger + fresh Down |
| Shield platform drop | Hold trigger, then `Shift+S` | Hold trigger, then `Shift+Down` |
| Crouch, platform drop, fast fall | `S` | Down |
| Reset/rematch both players | `R` or Reset/Rematch button | Same |
| Pause/resume | `P` or Pause button | Same |
| One tick while paused | `N` or Step button | Same |

Up to two gamepads using the
[W3C Standard Gamepad layout](https://www.w3.org/TR/gamepad/#remapping) are
assigned in browser index order. On each pad, the left stick supplies analog
movement/DI, the D-pad supplies full
magnitude, the bottom face button is light attack or a directional forward
smash, the right face button is a direct strong attack, either left/top face
button jumps, and any shoulder or trigger
holds shield or supplies the tech/air-dodge/L-cancel trigger. Keyboard and
gamepad inputs can be mixed for the same player. Non-standard browser mappings
are ignored rather than guessed.

Unmodified horizontal keys emit full stick magnitude and can enter initial
dash. Reversing them during the ten-tick initial-dash window performs a
dash-dance reversal. Holding `Shift` emits a reduced magnitude below the dash
threshold and therefore walks. `Shift` also reduces vertical magnitude, making
the shield-drop band distinct from full-down spot dodge.

For a fox-trot, tap and release one full direction, then repeat that same
direction. Every fresh tap returns the inspector to tick 1 of `INITIAL DASH`
and the neutral tick preserves a short traction slide. Holding the direction
instead reaches `RUN` after the data-defined ten-tick window; using the
`Shift`/reduced-magnitude input after release produces `WALK`, not another dash.

For a pivot, begin an initial dash, tap the opposite full direction for one
tick, then return to neutral and immediately press the attack key. The attack
uses the reversed facing while residual reversal momentum slides under
traction. Omitting the attack produces an empty pivot; holding the reversal
continues the opposite dash, and attempting the route after `RUN` instead
enters `RUN TURNAROUND`.

For a dash cancel, press jump during `INITIAL DASH` to enter `JUMP SQUAT`, or
reach `RUN` and either press down for a sliding `CROUCH` or hold shield for a
sliding `SHIELD`. A ground attack can immediately interrupt the crouch while
the remaining momentum carries forward. Shield during `INITIAL DASH` and down
during `RUN TURNAROUND` are deliberate negative cases.

For a dashing shield, reach `RUN`, tap shield for one tick, and release it.
The inherited run momentum continues sliding under traction through the
eight-tick minimum hold and 15-tick `SHIELD RELEASE`. Repeat while holding
shield: the travel path is the same, but the fighter remains `SHIELD`. A shield
tap from idle is the no-travel negative case.

For a shield platform drop, land on the pass-through platform and raise shield.
On a later tick, keep holding the trigger and press reduced down with
`Shift+S` / `Shift+Down`, or tilt a gamepad stick into the same analog band.
The fighter immediately enters `AIRBORNE` with the ordinary nine-tick platform
pass timer. Too little down remains `SHIELD`, while full down enters
`SPOT DODGE`; releasing the trigger after minimum hold instead enters grounded
`SHIELD RELEASE`. This follows the documented Melee
[shield drop](https://www.ssbwiki.com/Shield_drop) interaction.

For a small-step forward smash, press full direction and light attack together
for the standing comparison. Reset, tap and hold the same full direction, wait
one to three simulation ticks, then press light attack. The fighter enters
`STRONG ATTACK` after traveling forward, extending the same pink hitbox's range.
Waiting four ticks or releasing the direction before light attack instead
produces the ordinary non-smash ground attack. The dedicated strong key/button
remains the direct strong-attack route.

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
apex. For an instant double jump, release the first jump during jump squat and
press the other jump key on the first airborne frame. The live `air jumps`
counter changes from 1 to 0; holding one key through takeoff does not consume
the air jump because only a fresh edge is accepted.

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

Press the light-attack key while airborne for the original 8% aerial. It has
four startup ticks, five active ticks, 23 recovery ticks, five hitlag ticks,
and keeps normal drift, gravity, and fast-fall control. Landing outside its
action-tick `[4, 25)` landing-lag window auto-cancels into generic four-tick
`LANDING`. Landing inside that window normally produces 12 ticks of
`AERIAL LANDING`.

For a drop cancel, place both fighters close together on the moving
pass-through platform and leave the target still. Press down with the attacker,
then press light attack on the first airborne tick. A connecting hit uses its
hitlag to expire the nine-tick pass timer and returns the attacker to
`AERIAL LANDING` on the same platform. Wait one extra tick before attacking, or
move the target out of range, and the attacker falls through. The initial down
tick never also starts fast fall. This follows the documented
[Drop cancel](https://www.ssbwiki.com/Drop_cancel) interaction.

For a V-cancel, move both fighters close and airborne, start an attack, and
fully press the defender's trigger on the collision tick or either of the two
preceding ticks. A clean eligible input scales both launch components to 95%
while leaving hitstun unchanged. Compare against the same hit with no trigger,
then repeat while the defender is performing an aerial and after another full
trigger inside the 40-tick lockout; both negative routes keep ordinary launch.
The exact age-3 boundary is also ineligible. This follows the documented
[V-cancelling](https://www.ssbwiki.com/V-cancelling) interaction.

For an L-cancel, make a fresh trigger press during the seven ticks before
landing while the aerial's landing-lag window is active. Trigger ages 0–6 are
eligible; age 7 is not. Success shows `L-CANCEL LANDING` for six ticks. The
state card exposes both trigger age and eligibility. For SHFFL, tap and release
jump during jump squat, press the aerial, hold down after the apex to fast-fall,
then make the fresh trigger press shortly before contact. A trigger pressed
during the active aerial arms this timer rather than starting an air dodge.

For easier L-cancel practice, press the strong-attack key while airborne. The
strong aerial reuses the visible pink strong hitbox and its five-startup,
three-active, 18-recovery, 12%-damage, and six-hitlag data. Landing while that
action is active always enters a deliberately long 30-tick `STRONG AERIAL
LANDING`; an eligible trigger press halves it to 15 ticks of `STRONG L-CANCEL
LANDING`. Every aerial landing displays a large red missed-L-cancel or green
successful-L-cancel banner, matching ring, and live remaining-frame count over
the fighter. The light aerial remains the ordinary 12/6-frame SHFFL route.

The live fixture is a four-stock match. Crossing any blast boundary consumes
one stock and shows `RESPAWN WAIT` with a 60-frame countdown. The fighter then
returns at its authored ground spawn with zero damage and 120 frames of
hitbox-rejecting invulnerability; the dashed gold ring and state card expose
the exact timer while movement and attacks remain available. Losing the final
stock enters `ELIMINATED`, pauses the match, displays the winner, and changes
Reset to Rematch.

If both players lose their final stock on the same tick, neither is awarded an
immediate win. The page displays `SUDDEN DEATH · 300%`, gives each fighter one
stock, waits through the same respawn countdown, and returns both at 300%. A
second simultaneous KO resolves deterministically to Player 1 rather than
looping forever.

To grab a ledge, fall beside it while facing inward. After the seven-tick catch
window, press toward the stage to climb, press down or away to release, or press
jump for a ledge jump. A claimed ledge rejects another fighter until its current
occupant releases or completes the climb.

For an edge hop, wait until `LEDGE HANG`, tap down to release, then release down
and press jump while holding toward the stage on the next tick. The fresh jump
consumes the air jump while the fighter keeps its inward facing; press either
attack key for the aerial follow-up. The dashed invulnerability ring remains
until the original 37-tick ledge timer expires. Neutral hang and a second jump
after the counter reaches zero are the negative cases.

For an edge dash, wait until `LEDGE HANG`, press jump, and hold toward the
stage while rising until the fighter's feet clear the floor plane. Press
down-toward plus a fresh trigger to air dodge onto stage. Confirm `SPECIAL
LANDING`, an inward slide, action ticks 0–9, then an actionable frame while the
dashed invulnerability ring remains. Wait on ledge until the ring disappears
before repeating for the negative case; the movement route remains, but the
actionable frame is vulnerable.

The light jab has two startup ticks, two active ticks, eight recovery ticks,
6% damage, and four hitlag ticks. Its translucent amber rectangle is the exact
inspected active hitbox. The strong attack has five startup ticks, three active
ticks, 18 recovery ticks, 12% damage, six hitlag ticks, and a pink active
hitbox. Its first clean hit against a fresh default fighter exceeds the
32-tick tumble threshold. After the frozen hitlag pose, a tumbling fighter
visibly rotates and the state card prefixes its action with `TUMBLE`. The cards
also show percent, hitstun, SDI pulse count, tech window/lockout, tech
direction, and the last combat-event sequence.

The event panel is driven by the ABI-4 per-tick journal rather than inferred
from the rendered state. It shows canonical sequence/tick labels for hits,
shield interactions, KOs, respawns, sudden death, results, forfeits, and time
limits. The simulation returns at most 16 records for the current tick; the
browser keeps only the newest ten as non-authoritative presentation history
and clears them on Reset or any observed rewind.

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

From an otherwise actionable grounded state, press a fresh full horizontal
direction with the trigger to roll. Direction is interpreted relative to the
fighter's fixed facing: toward facing is `FORWARD ROLL`, away is `BACKWARD
ROLL`, and neither option flips the facing arrow. Press fresh down with the
trigger for `SPOT DODGE`; down wins over a simultaneous horizontal input.
Forward roll lasts 31 ticks, backward roll 35, and spot dodge 25. The dashed
gold ring shows the exact roll action-tick window `[4, 17)` or spot-dodge
window `[3, 16)`. Holding the direction before pressing trigger is the
negative case because the direction is no longer fresh.

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

When shield health reaches zero from holding or a physical hit, the fighter
launches straight upward in `SHIELD BREAK`; ordinary input cannot steer or
fast-fall it. Landing is forced even through held-down platform input and
enters `SHIELD BREAK DOWN`, then `SHIELD BREAK STAND`, then vulnerable
`SHIELD BREAK STUN`. The first two grounded phases and flight show the
invulnerability ring; stun replaces it with orbiting stars and a live
`MASH · Nf` counter. Alternate fresh jump/attack presses, trigger presses, or
stick flicks to remove three extra frames per input. Holding a key does not
repeat the reduction. Natural recovery restores 30 shield HP; an opponent hit
during stun interrupts it through ordinary hitlag/hitstun.

A new trigger press still opens a 20-tick tech window and a 40-tick lockout. A
tumbling floor/platform impact with neutral horizontal input enters
`TECH IN PLACE`; holding left or right enters `TECH ROLL`; missing the window
enters `KNOCKDOWN`. Use the strong attack for an immediate default-content
tumble test; the light jab intentionally remains below the threshold on a
fresh fighter. Tech in place lasts 26 ticks and tech roll lasts 40; both reject
hits for their first 20 ticks, shown by a dashed gold ring and the
`invulnerable` state-card field. Missed-tech knockdown has no such protection
and lasts 26 ticks before entering `DOWN WAIT`.

For a tech chase, strong-launch the opponent and follow their airborne path.
Have the opponent tech in place or hold a direction for tech roll, keep
adjusting toward the observed outcome, and jab after the dashed gold ring
clears at action tick 20 but before the recovery ends. Attacking from the
original spacing at that same tick is the roll-escape comparison and should
miss.

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

This shield slice does not yet include analog light shield, general shield
tilt/poke, shield SDI, grab, projectile
reflection.
Future ground actions must join the same powershield-cancel router before that
registry row can advance from `playable` to `verified`.

## Focused owner checks

1. Tap left and right rapidly without `Shift`. Confirm each reversal occurs
   during `INITIAL DASH` without a neutral key press. Then dash right, tap left
   for exactly one tick, release to neutral, and immediately press `F`. Confirm
   the attack faces left while the fighter still slides left. Repeat without
   `F` for grounded idle, with left held for continued dash, and after reaching
   `RUN` for the `RUN TURNAROUND` negative case.
2. Hold `Shift+A` and `Shift+D`. Confirm the state inspector says `WALK` and
   movement is visibly slower than unmodified `A`/`D`.
3. Hold a direction until the inspector says `RUN`, then press the opposite
   full direction. Confirm `RUN TURNAROUND`, never `INITIAL DASH`, appears
   before the fighter begins running the other way. Release from a run and
   confirm `RUN BRAKE`. Repeat from `RUN` with down and confirm a forward slide
   in `CROUCH`, then immediately attack; repeat with shield for `SHIELD`. Jump
   during `INITIAL DASH` for `JUMP SQUAT`, then confirm shield in that same
   early state and down during `RUN TURNAROUND` do not take the cancel routes.
4. Rhythmically tap and release `D` at least four times. Confirm every fresh
   press returns to tick 1 of `INITIAL DASH`, facing remains right, and the
   fighter keeps advancing without reaching `RUN`. Then hold `D` to confirm
   `RUN`, and repeat the rhythm with `Shift+D` to confirm `WALK` instead.
5. Perform ten very quick jump taps. Confirm every short hop reaches the same
   height.
6. Hold jump through takeoff, then vary when it is released. Confirm every full
   hop reaches the same height and is higher than every short hop.
7. Tap `Space`, release it during jump squat, then tap `W` on the first
   airborne frame. Confirm the immediate double-jump arc and the `air jumps`
   counter changing from 1 to 0. Repeat while holding only one jump key through
   takeoff and confirm the counter remains 1. Then jump while facing right,
   hold left until moving left, and hold right until moving right. Confirm the
   facing indicator stays right throughout both drift directions and the air
   jump. Repeat after turning left on the ground.
8. Full hop and press up-right plus the trigger while airborne. Confirm `AIR
   DODGE`, unchanged facing, the gold invulnerability ring only on action ticks
   3–28, and `FALL SPECIAL` if the dodge finishes without touching a surface.
   Keep holding the trigger and confirm it never starts a second dodge.
9. Tap jump, release during jump squat, then on the first airborne frame press
   down-right plus the trigger. Confirm immediate `SPECIAL LANDING`, a
   horizontal slide with unchanged facing, action ticks 0–9, then idle. Repeat
   with neutral stick and confirm it does not create the diagonal slide.
10. Full hop upward through the moving platform. On descent, press down-left or
   down-right plus the trigger just above it. Confirm `SPECIAL LANDING` uses
   platform support and preserves horizontal momentum instead of dropping
   through.
11. Use an airborne fresh jump, reverse aerial drift around the apex, fast-fall
   with down, land on the moving platform, then press down to drop through it.
   Next, put both fighters close together on that platform, leave the target
   still, press down with the attacker, and press light attack on the first
   airborne tick. Confirm the hit returns the attacker to `AERIAL LANDING` on
   the same platform. Repeat one tick late and with the target out of range;
   both negative routes must fall through.
12. Approach the right edge in `RUN`, press left before leaving it to enter
   `RUN TURNAROUND`, and let the retained rightward momentum carry the fighter
   off while facing inward. Confirm `LEDGE HANG`, then try neutral hang,
   down/away release, jump, and inward climb.
13. Put one player on a ledge and attempt to grab it with the other player.
   Confirm only the original occupant enters `LEDGE HANG`.
14. From `LEDGE HANG`, tap down, release it, then press jump plus inward on the
   next tick and follow with an aerial. Confirm `air jumps` changes from 1 to 0,
   facing remains inward, and the ledge-invulnerability ring persists. Repeat
   without leaving hang and after exhausting the air jump for the two negative
   cases.
15. From `LEDGE HANG`, jump and hold inward until the fighter's feet clear the
   stage, then press down-inward plus a fresh trigger. Confirm inward `SPECIAL
   LANDING`, action ticks 0–9, and an actionable frame while the dashed ring
   remains. Wait on ledge until the ring expires and repeat to confirm the same
   route is vulnerable.
16. Move into range, press `F`, and confirm the amber hitbox appears only on the
   active frames. On contact, confirm the target gains 6%, both players visibly
   freeze, the target then launches in `HITSTUN`, and the event feed adds one
   sequenced 6% `hit` entry naming both players.
17. Attack facing away and confirm the active hitbox whiffs. Reset, bring both
    players into range, and attack on the same tick to confirm a simultaneous
    trade.
18. Pause just before contact and use `N` to step through hitlag. On the first
    target hitlag tick, press a horizontal direction and confirm `SDI pulses`
    becomes 1 and the target shifts. Keep holding it for another tick and
    confirm the count stays 1. Add the vertical component and confirm the count
    becomes 2.
19. Compare otherwise similar launches while holding perpendicular opposite
    vertical directions on the final hitlag tick. Confirm the visible launch
    vector changes while the state remains deterministic after Reset.
20. Tap the target's tech key and confirm the state card shows
    `tech window 20` and `lockout 40`. Hold the key through the next tick and
    confirm they count down to 19/39 rather than reopening.
21. Move into range and press `H` (or Player 2's strong-attack key). Confirm the
    pink hitbox, 12% damage, six frozen hitlag ticks, then visible rotation with
    `TUMBLE · HITSTUN`. Press the target's tech key within 20 ticks of landing.
    Use neutral horizontal input for `TECH IN PLACE`, then repeat while holding
    a direction for `TECH ROLL`. Repeat without the tech input and confirm
    `KNOCKDOWN`. Confirm the gold invulnerability ring clears after 20 ticks
    while each tech action continues.
22. Let the missed tech finish into `DOWN WAIT`. Try up and a fresh shield
    press for `NEUTRAL GETUP`, left and right for both `GETUP ROLL`
    directions, and either attack key for `FLOOR ATTACK`. Pause and step
    through the floor attack: confirm purple hitboxes in front on frames
    17–19 and behind on frames 24–26, with no hitbox in between. Confirm the
    gold ring clears at each option's documented invulnerability boundary.
23. Move both fighters near the raised block and strong-launch the target into
    its side. Tap the target's tech key during tumble for `WALL TECH`; repeat
    while holding up for `WALL TECH JUMP`, then omit the tech input for `WALL
    BOUNCE`. Repeat with an upward launch into the underside for `CEILING TECH`
    and `CEILING BOUNCE`. Confirm successful techs clear tumble/hitstun while
    missed impacts retain them.
24. Get above the raised block with a full/double jump or launch, descend
    beside its upper-left corner, and hold toward the block. Confirm the
    fighter lands or stays flush with the contacted face and no part of the
    body enters the block. Repeat from the upper-right corner.
25. From idle, press trigger plus the direction the fighter faces. Confirm
    `FORWARD ROLL`, unchanged facing, movement only during its middle window,
    the gold ring only on action ticks 4–16, and idle after 31 ticks. Repeat
    away from facing for `BACKWARD ROLL` and its 35-tick duration. Then press
    trigger plus fresh down for `SPOT DODGE`, confirm no horizontal movement,
    the ring only on ticks 3–15, and idle after 25 ticks. Hold down before
    pressing trigger and confirm ordinary `SHIELD` instead.
26. From idle, hold the shield key. Confirm the bubble appears on frame 1,
    health drains, an early key release waits for the eight-tick minimum, and
    `SHIELD RELEASE` lasts 15 ticks. Press jump during shield/release and
    confirm `JUMP SQUAT`.
27. Reach `RUN`, tap shield for one tick, and release. Confirm `SHIELD`
    replaces `RUN`, the fighter slides forward under traction, enters
    `SHIELD RELEASE` when the eight-tick minimum completes, and returns to idle
    after 15 release ticks. Repeat while holding shield and confirm the same
    travel path remains `SHIELD`. Reset, tap shield from idle and confirm no
    horizontal travel; then press shield during `INITIAL DASH` and confirm the
    fighter does not shield until run.
28. Hold shield for more than four ticks and block an attack. Confirm no
    percent is added, shield health drops, both fighters freeze, and the
    defender resumes in `SHIELD STUN`. Repeat by raising shield immediately
    before contact; confirm the powershield indicator appears, shield health
    loses only its normal hold depletion, and pushback is larger.
29. After that powershield, release shield before `SHIELD STUN` ends. Leave the
    first `SHIELD RELEASE` tick neutral, then press the defender's attack key
    on frame 2 and confirm it enters `GROUND ATTACK`. Repeat after an ordinary
    block and confirm the attack cannot skip the 15-tick release.
30. Hold shield until it reaches zero. Confirm upward `SHIELD BREAK`, forced
    landing, prone `SHIELD BREAK DOWN`, `SHIELD BREAK STAND`, then the
    vulnerable orbiting-star stun and `MASH · Nf` counter. Hold one mash key
    and confirm only the first tick gets the extra reduction; alternate fresh
    inputs to recover faster and confirm shield health resets to 30. Repeat
    and have the opponent hit during stun to confirm ordinary hitlag/hitstun.
31. Full hop and use the aerial late enough to land during its startup
    auto-cancel frames; confirm generic `LANDING`. Then short-hop aerial,
    fast-fall, and land without the trigger for 12 ticks of `AERIAL LANDING`.
    Repeat with a fresh trigger in the last seven ticks and confirm six ticks
    of `L-CANCEL LANDING`. Pause and step while watching trigger age: 0–6 must
    say eligible and 7 must not.
32. Short hop, press the strong-attack key while airborne, hold down to
    fast-fall, and intentionally omit the trigger. Confirm the pink hitbox,
    `STRONG AERIAL LANDING`, a red missed-L-cancel banner/ring, and a countdown
    from 30 frames. Repeat with a fresh trigger shortly before contact and
    confirm `STRONG L-CANCEL LANDING`, a green success banner/ring, and a
    15-frame countdown.
33. Run Player 1 beyond a blast boundary. Confirm one stock disappears,
    `RESPAWN WAIT` counts down from 60, then the fighter returns at zero
    percent with the dashed ring and 120-frame invulnerability timer. Move and
    attack during that timer, then confirm the ring expires. Repeat until the
    final stock and confirm the result banner, paused match, and Rematch
    button. Confirm the feed records KO, respawn, and final match result in
    increasing sequence order.
34. Repeat with Player 2's arrow-key controls and try both players
    simultaneously.
35. Strong-launch Player 2 and move Player 1 toward the projected landing.
    Tech in place with Player 2, wait for the gold ring to clear, and jab before
    `TECH IN PLACE` ends. Repeat with a right tech roll and chase the observed
    movement before jabbing. Finally reset, leave Player 1 at the original
    spacing, and jab at the same target action tick; confirm the roll escapes.
36. Connect one standard-mapped gamepad, press or move it once so the browser
    exposes it, and confirm the toolbar says `standard gamepads 1/2`. Verify
    analog walk versus dash, D-pad full input, both attacks, jump, shield, tech,
    air dodge, and L-cancel. Hot-plug a second pad and confirm it controls
    Player 2 while the first remains Player 1; disconnect either pad and confirm
    keyboard control remains live.
37. Move both fighters close, jump, and have Player 1's aerial reach Player 2.
    First omit Player 2's trigger and record the hit-event launch vector. Reset
    and fully press Player 2's trigger as the hit connects; confirm both launch
    components are 95% of the first route while hitstun is unchanged. Repeat
    while Player 2 is attacking, then after a prior full trigger inside the
    40-tick lockout, and confirm both exclusions retain ordinary launch.
38. Land on the pass-through platform, hold shield, then press `Shift+S` or
    `Shift+Down` while continuing to hold the trigger. Confirm immediate
    `AIRBORNE` fall-through without `SHIELD RELEASE`. Repeat with unmodified
    full down for grounded `SPOT DODGE`, with too little analog down to remain
    `SHIELD`, and by releasing the trigger for ordinary grounded
    `SHIELD RELEASE`.
39. Use reduced horizontal input to place Player 1 just outside Player 2's
    light-attack hitbox but inside Player 1's strong-attack reach. Have Player 2
    jab, then press Player 1's strong attack while the jab is active; confirm
    Player 1 remains untouched and the counter hits during recovery. Move
    closer and confirm the jab hits first, move farther and confirm the counter
    also whiffs, then shield at the safe distance and confirm the strong tip is
    blocked.
40. Reset to the default full-stage separation and approach only with reduced
    horizontal input. Brake at the same safe band, have Player 2 jab, and
    convert the whiff with Player 1's strong attack. Reset and continue walking
    into close range instead; confirm Player 2's jab intercepts the reckless
    approach before Player 1 can counter.
41. Put Player 2 on the pass-through platform, drop Player 1 to the floor, and
    move underneath. Full hop and press light attack shortly before crossing
    the platform; confirm the aerial hits Player 2 from below. Repeat with the
    attack immediately after takeoff and confirm its active frames whiff too
    low, then hold Player 2's shield and confirm the timed route damages the
    shield without adding percent.
42. Move both players to clear floor space and hold Player 2's shield. Put
    Player 1 in front while facing away, short hop through Player 2, then use
    the light aerial on descent. Confirm the block finishes with Player 1
    behind and still facing away. Attack immediately after takeoff to confirm
    the wrong-side whiff, then repeat facing toward Player 2 and confirm the
    blocked aerial leaves Player 1 in front.
43. Repeat the same reduced-stick approach cue against two Player 2 responses.
    If Player 2 jabs, brake just outside reach and strong-counter the recovery.
    If Player 2 holds shield, face away and use the short-hop rear cross-up.
    Deliberately strong-attack the shield and immediately aerial before
    crossing to confirm the blocked and whiffed wrong reads.
44. Move Player 2 near center and Player 1 just inside strong-attack range.
    Strong-launch Player 2, follow the airborne path, then full hop and use the
    light aerial on descent; confirm the second hit connects before Player 2
    touches a surface. Repeat while Player 2 holds DI away and presses a fresh
    trigger after hitstun for a directional air dodge; confirm Player 1's
    active aerial whiffs and Player 2 retains only the launcher damage.

Record any mismatch with the control used, the visible tick/action state, and
whether it repeats after Reset.

## Automated browser contract

Before the interactive loop appears, the Wasm module runs the real simulation
through:

- reduced keyboard magnitude producing `WALK`;
- a deterministic standard-gamepad mapping probe covering analog dead zone and
  quantization, D-pad override, face/shoulder buttons, non-standard rejection,
  and stable assignment of the first two connected pads; live input polls
  `navigator.getGamepads()` every simulation tick for hot-plug behavior;
- full magnitude producing `INITIAL DASH`;
- an opposite full magnitude producing an immediate dash-dance reversal;
- an opposite full magnitude after `RUN` producing `RUN TURNAROUND`, never a
  new initial dash;
- four same-direction full-input tap/release bursts each restarting tick 1 of
  `INITIAL DASH`, plus held-run and reduced-magnitude-walk negative routes;
- a one-tick initial-dash reversal followed by an immediate ground attack or
  empty pivot while retaining facing and momentum, plus held-reversal and
  post-run negative routes;
- initial-dash jump cancel plus run-to-crouch and run-to-shield cancels, an
  immediate crouch attack with retained slide, and early-shield/turnaround
  negative routes;
- one-tick run-to-shield tap/release travel through the minimum hold and full
  release, compared with a held shield stop on the same traction path and an
  idle no-travel negative route;
- reduced down on pass-through support producing an ordinary shield platform
  drop only after shield entry, plus below-band shield and full-down spot-dodge
  negative routes;
- simultaneous full-direction/light input producing the standing forward
  smash, a frame-3 delayed light input producing the farther-traveled strong
  attack, and frame-4 plus missing-direction negative routes producing the
  ordinary non-smash attack;
- a first-airborne-frame drop aerial hitting, expiring the nine-tick pass timer
  during attacker hitlag, and returning to aerial landing on the same platform,
  plus a one-tick-late connecting route that falls through;
- ordinary and collision-frame-trigger aerial hits proving exact 95% two-axis
  V-cancel launch with unchanged hitstun, plus active-aerial and repeated-edge
  lockout exclusions;
- reduced-stick walking to close, safe, and far attack bands, followed by a
  jab-first responder proving close jab contact, a safe jab-whiff-to-strong
  counter, a far double whiff, and a safe-tip shield block;
- default-stage reduced-stick walking from the full neutral separation into
  the safe whiff conversion, plus an overextended walk being intercepted by
  the same responder;
- floor-to-platform full-hop aerial sharking on the default moving platform,
  plus a too-early active-hitbox whiff and a correctly timed held-shield block;
- ordinary movement into a short-hop back-aerial cross-up behind held shield,
  plus an immediate wrong-side whiff and a forward-facing front-block control;
- a combined mindgame gate requiring the jab-read ground conversion,
  shield-read rear cross-up, and both wrong-read outcomes together;
- a grounded launcher into an airborne light-aerial juggle before landing,
  plus a DI-and-directional-air-dodge route that makes the active follow-up
  whiff;
- a validated kill-confirm fixture building 120% through legal jabs, linking
  the 126% setup into a 138% typed KO without an actionable defender frame,
  and comparing an 18% non-KO route with a 126% outward-DI active whiff before
  restoring default content;
- the same fixture beginning at exact 0% and chaining 21 earliest-recovery
  jabs into a 138% strong-finisher stock loss without an actionable defender
  frame, plus outward DI after first contact breaking the sequence before the
  finisher while a later active jab whiffs;
- fresh trigger-plus-horizontal input selecting forward/backward roll relative
  to facing, fresh trigger-plus-down selecting spot dodge, and both actions
  reaching their authored invulnerability windows without changing facing;
- two different short-hop release timings producing the same apex;
- two different post-takeoff full-hop hold durations producing the same apex;
- an exact first-airborne-frame instant double jump consuming one air jump and
  applying its authored velocity, plus a held jump through takeoff proving
  edge-triggered non-repeat;
- opposite-direction aerial drift and an opposite-direction air jump changing
  velocity without changing takeoff facing;
- a full-hop directional air dodge reaching its exact invulnerability window
  and `FALL SPECIAL`, plus a first-airborne-frame diagonal short-hop air dodge
  entering `SPECIAL LANDING` and retaining horizontal slide;
- a default-content aerial auto-cancel route, 12-tick normal aerial landing,
  six-tick L-cancel landing, and exact eligible trigger ages 0–6 versus the
  ineligible age-7 boundary;
- a strong airborne attack route with production hit data, 30-tick normal
  landing lag, and 15-tick L-cancel landing lag;
- an ordinary-input blast KO consuming exactly one of four stocks, entering
  the exact 60-tick respawn wait, then returning active with the exact
  120-tick hitbox-rejecting invulnerability timer;
- a real grounded attack producing the configured damage, hitlag, attacker
  identity, and typed ABI-4 hit event; and
- a default strong attack producing 12%, six hitlag ticks, at least 32 hitstun
  ticks, and canonical tumble state;
- an exact 26-tick missed-tech animation entering `DOWN WAIT`, all three
  floor-recovery input routes, their initial invulnerability, and both active
  phases of the floor attack;
- airborne following into observed tech-in-place and right-tech-roll outcomes,
  a jab during each vulnerable recovery tail, and a same-action-tick
  non-following jab that misses the roll;
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
- full held-shield depletion producing the typed system-source break event,
  upward launch, down/stand/stun phase order, fresh-versus-held mash behavior,
  and 30-HP recovery; and
- the native movement oracle covering ledge catch, hang, release, jump, climb,
  simultaneous occupancy, and mid-climb save/load equivalence.

The page reports
`playtest=ready input_probe=pass air_facing_probe=pass
instant_double_jump_probe=pass edge_hop_probe=pass edge_dash_probe=pass
fox_trot_probe=pass pivot_probe=pass dash_cancel_probe=pass
dashing_shield_probe=pass shield_platform_drop_probe=pass
small_step_forward_smash_probe=pass
drop_cancel_probe=pass v_cancel_probe=pass approach_probe=pass
spacing_probe=pass sharking_probe=pass cross_up_probe=pass
mindgame_probe=pass juggling_probe=pass kill_confirm_probe=pass
zero_to_death_probe=pass combat_probe=pass
event_journal_probe=pass reaction_probe=pass
shield_probe=pass shield_break_probe=pass powershield_cancel_probe=pass
tumble_probe=pass
floor_recovery_probe=pass tech_chase_probe=pass surface_tech_probe=pass
air_dodge_probe=pass
ground_dodge_probe=pass
aerial_l_cancel_probe=pass match_probe=pass gamepad_probe=pass
gamepad_api=available controls=keyboard-gamepad-two-player` only after all checks
pass.
Clean-machine Chrome CI also requires that status and the live playtest DOM.
