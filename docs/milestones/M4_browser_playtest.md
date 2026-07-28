# M4 real-simulation browser playtest

This checkpoint runs the production `pf_sim_tick` M4 movement, first attack,
hit-reaction, and dense-shield primitives in WebAssembly. It is no longer the
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
| Ground attack | `F` | `/` or Numpad `0` |
| Hold shield / tap tech trigger | `G` | `.` or Numpad `1` |
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

To grab a ledge, fall beside it while facing inward. After the seven-tick catch
window, press toward the stage to climb, press down or away to release, or press
jump for a ledge jump. A claimed ledge rejects another fighter until its current
occupant releases or completes the climb.

The first placeholder ground attack has two startup ticks, two active ticks,
and eight recovery ticks. The translucent amber rectangle is the exact
inspected active hitbox. A hit adds 6%, freezes both fighters for four hitlag
ticks, then launches the target into hitstun. The state cards show percent,
hitlag, hitstun, tumble, SDI pulse count, tech window/lockout, tech direction,
and the last combat-event sequence.

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
enters `KNOCKDOWN`. The current light attack needs substantial accumulated
damage before its hitstun reaches the 32-tick tumble threshold.

This shield slice does not yet include analog light shield, shield tilt/poke,
shield SDI, roll/spot dodge, platform shield drop, grab, projectile reflection,
or the complete airborne/knockdown/stun shield-break sequence. Future ground
actions must join the same powershield-cancel router before that registry row
can advance from `playable` to `verified`.

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
6. Use an airborne fresh jump, reverse aerial drift around the apex, fast-fall
   with down, land on the moving platform, then press down to drop through it.
7. Run off the right side, reverse toward the stage while falling, and confirm
   `LEDGE HANG`. Try neutral hang, down/away release, jump, and inward climb.
8. Put one player on a ledge and attempt to grab it with the other player.
   Confirm only the original occupant enters `LEDGE HANG`.
9. Move into range, press `F`, and confirm the amber hitbox appears only on the
   active frames. On contact, confirm the target gains 6%, both players visibly
   freeze, and the target then launches in `HITSTUN`.
10. Attack facing away and confirm the active hitbox whiffs. Reset, bring both
    players into range, and attack on the same tick to confirm a simultaneous
    trade.
11. Pause just before contact and use `N` to step through hitlag. On the first
    target hitlag tick, press a horizontal direction and confirm `SDI pulses`
    becomes 1 and the target shifts. Keep holding it for another tick and
    confirm the count stays 1. Add the vertical component and confirm the count
    becomes 2.
12. Compare otherwise similar launches while holding perpendicular opposite
    vertical directions on the final hitlag tick. Confirm the visible launch
    vector changes while the state remains deterministic after Reset.
13. Tap the target's tech key and confirm the state card shows
    `tech window 20` and `lockout 40`. Hold the key through the next tick and
    confirm they count down to 19/39 rather than reopening.
14. After building enough damage for the card to show `tumble 1`, press the
    target's tech key within 20 ticks of landing. Use neutral horizontal input
    for `TECH IN PLACE`, then repeat while holding a direction for `TECH ROLL`.
    Repeat without the tech input and confirm `KNOCKDOWN`.
15. From idle, hold the shield key. Confirm the bubble appears on frame 1,
    health drains, an early key release waits for the eight-tick minimum, and
    `SHIELD RELEASE` lasts 15 ticks. Press jump during shield/release and
    confirm `JUMP SQUAT`.
16. Reach `RUN`, then hold shield. Confirm `SHIELD` replaces `RUN` while the
    fighter slides forward and slows under traction. Reset, press shield during
    `INITIAL DASH`, and confirm the fighter does not shield until run.
17. Hold shield for more than four ticks and block an attack. Confirm no
    percent is added, shield health drops, both fighters freeze, and the
    defender resumes in `SHIELD STUN`. Repeat by raising shield immediately
    before contact; confirm the powershield indicator appears, shield health
    loses only its normal hold depletion, and pushback is larger.
18. After that powershield, release shield before `SHIELD STUN` ends. Leave the
    first `SHIELD RELEASE` tick neutral, then press the defender's attack key
    on frame 2 and confirm it enters `GROUND ATTACK`. Repeat after an ordinary
    block and confirm the attack cannot skip the 15-tick release.
19. Repeat with Player 2's arrow-key controls and try both players
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
- a real grounded attack producing the configured damage, hitlag, attacker
  identity, and canonical combat event; and
- a production-path target SDI pulse producing a positional shift;
- a trigger edge producing the 20-tick tech window and 40-tick lockout, with a
  held trigger counting down rather than retriggering;
- a normal physical shield block producing zero percent, shield damage,
  shield stun, hitlag, and ordinary pushback;
- a physical attack inside the four-tick powershield window producing zero
  shield damage and the powershield result;
- release after that physical powershield preserving the cancel opportunity,
  followed by the one-frame delay and frame-2 ground attack; and
- the native movement oracle covering ledge catch, hang, release, jump, climb,
  simultaneous occupancy, and mid-climb save/load equivalence.

The page reports
`playtest=ready input_probe=pass combat_probe=pass reaction_probe=pass
shield_probe=pass powershield_cancel_probe=pass
controls=keyboard-two-player` only after all checks pass.
Clean-machine Chrome CI also requires that status and the live playtest DOM.
