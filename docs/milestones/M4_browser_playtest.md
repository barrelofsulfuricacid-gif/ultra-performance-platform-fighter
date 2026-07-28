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

Jump has a three-tick jump squat. Release during jump squat for the one fixed
short-hop launch speed; hold through takeoff for the one fixed full-hop launch
speed. Releasing or continuing to hold after takeoff cannot change the selected
apex.

## Focused owner checks

1. Tap left and right rapidly without `Shift`. Confirm each reversal occurs
   during `INITIAL DASH` without a neutral key press.
2. Hold `Shift+A` and `Shift+D`. Confirm the state inspector says `WALK` and
   movement is visibly slower than unmodified `A`/`D`.
3. Perform ten very quick jump taps. Confirm every short hop reaches the same
   height.
4. Hold jump through takeoff, then vary when it is released. Confirm every full
   hop reaches the same height and is higher than every short hop.
5. Use an airborne fresh jump, reverse aerial drift around the apex, fast-fall
   with down, land on the moving platform, then press down to drop through it.
6. Repeat with Player 2's arrow-key controls and try both players
   simultaneously.

Record any mismatch with the control used, the visible tick/action state, and
whether it repeats after Reset.

## Automated browser contract

Before the interactive loop appears, the Wasm module runs the real simulation
through:

- reduced keyboard magnitude producing `WALK`;
- full magnitude producing `INITIAL DASH`;
- an opposite full magnitude producing an immediate dash-dance reversal;
- two different short-hop release timings producing the same apex;
- two different post-takeoff full-hop hold durations producing the same apex.

The page reports
`playtest=ready input_probe=pass controls=keyboard-two-player` only after all
checks pass. Clean-machine Chrome CI also requires that status and the live
playtest DOM.
