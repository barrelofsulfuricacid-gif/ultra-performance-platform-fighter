# M4 real-simulation browser playtest

This checkpoint runs the production `pf_sim_tick` M4 movement state in
WebAssembly. It is no longer the disposable M0 float32/Q16.16 comparison.
Both visible players use the same validated M4 fighter and stage content used
by native, replay, rollback, and headless execution.

## Controls

| Action | Player 1 | Player 2 |
|---|---|---|
| Full left/right input | `A` / `D` | Left / Right |
| Reduced-magnitude walk | `Shift+A` / `Shift+D` | `Shift+Left` / `Shift+Right` |
| Jump | `W` or `Space` | Up |
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
9. Repeat with Player 2's arrow-key controls and try both players
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
- the native movement oracle covering ledge catch, hang, release, jump, climb,
  simultaneous occupancy, and mid-climb save/load equivalence.

The page reports
`playtest=ready input_probe=pass controls=keyboard-two-player` only after all
checks pass. Clean-machine Chrome CI also requires that status and the live
playtest DOM.
