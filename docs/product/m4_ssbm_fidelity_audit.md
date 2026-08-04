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

## Current audit

| System | Status | Evidence and remaining gap |
|---|---|---|
| Stick aging, dead zones, dash recognition | equivalent | Fresh horizontal tilt age, reversal reset, 0.80 dash threshold, and two-tick dash window follow the common input/decomp route. |
| Initial dash and dash physics | equivalent | One-shot Falcon 2.0 impulse with no entry-frame displacement, full A/B dash acceleration from the next frame, held transition after 15 displayed dash frames, and released completion after 28 displayed dash frames match the executable oracle. |
| Walk/run acceleration and friction | equivalent | Falcon attributes and the friction-aware target/overshoot formulas are mapped; slow stick motion enters walk rather than dash. |
| Dash dance and backward dash acceleration | equivalent | A fresh reversal enters one displayed frame of smash `TURNING` with the old facing and damped velocity; a held reversal then enters opposite dash with the measured residual momentum plus Falcon's impulse. |
| Run braking | equivalent for captured route | Neutral from terminal run produces 28 displayed `RUN_BRAKE` frames with Falcon's 0.08 friction before standing, matching the executable oracle. Other animation-command interrupts remain unaudited. |
| Standing turn | equivalent for captured routes | Smash turn flips on the following frame and can enter dash; basic turn flips on displayed frame 8 and completes after displayed frame 11. Both timing and friction routes match the executable oracle. |
| Run turnaround | equivalent for captured route | Full reversal from terminal run retains the old facing, applies full TurnRun acceleration, freezes displayed frame 9 until velocity crosses the common 0.01 threshold, flips facing on the following physics tick, resumes through displayed frame 21, and enters the ten-tick locked run route. The identical-input oracle covers the complete held reversal and neutral brake after exit. |
| Jump squat and takeoff momentum | equivalent | Falcon startup 4, 0.75 retained momentum, 0.95 stick contribution, and 2.1 cap are mapped. |
| Short/full hop | equivalent | Falcon 1.9 and 3.1 vertical velocities are converted to stage units. |
| Double jump | equivalent | Horizontal velocity is replaced from neutral/stick input using Falcon's 0.9 multiplier; vertical velocity uses the 0.9 multiplier. |
| Gravity, terminal velocity, air drift | equivalent | Falcon A/B acceleration, drift target, friction, gravity, terminal, and absolute horizontal cap are mapped. |
| Fast fall | equivalent | Requires a fresh downward tilt within four ticks after descent begins; holding down before the apex does not trigger it. |
| Crouch/crawl | equivalent for captured routes | Full-down input produces Falcon's seven displayed `Squat` frames, held `SquatWait`, ten displayed `SquatRv` frames, then standing. Exact 0.6875 entry and 0.625 release boundaries preserve the decomp's hysteresis. Jump and fresh guard from all three states, held-crouch dash/turn, and release-state walk match. `Squat` and `SquatWait` are crouch-cancel eligible while `SquatRv` is not; crawl entry remains disabled because Falcon cannot crawl. The remaining IASA routes are unaudited. |
| Ground and platform collision | partial | Deterministic swept collision and corner-overlap recovery are present; stage collision primitives, ECB evolution, and the executable-observed grounded player-push displacement do not yet reproduce Melee's engine. |
| Ledge jump velocities | equivalent | Falcon 1.0 horizontal and 3.3 vertical attributes are mapped. |
| Other ledge actions | partial | Hang, drop, climb, roll, attack, regrab lockout, and invulnerability exist, but exact animation-command and percent-dependent ledge tables are not imported. |
| Wall jump / wall and ceiling tech velocities | equivalent | Falcon passive-wall, wall-jump, and passive-ceiling attributes are mapped. |
| Normal landing lag | equivalent | Falcon's four-frame value is mapped. |
| Aerial landing lag | equivalent | Distinct neutral/forward/back/up/down landing states select Falcon's 15/19/18/15/24 table; L-cancel states halve the selected value. |
| Shield input, light shield, shield size | partial | Separate analog triggers, no light-shield air dodge, shield health scaling, and rendering exist; full common shield formulas/tables are not yet imported. |
| Roll, spot dodge, air dodge buffering | partial | Production paths and per-trigger edge tracking exist. Air-dodge force, dead zone, decay, and post-dodge drift cap are imported with axis-specific unit conversion; exact action/animation tables remain authored. |
| Damage, knockback, hitlag, hitstun, DI/SDI | partial | Deterministic systems exist, but formulas and common constants have not completed a field-by-field decomp equivalence review. |
| Attacks, grabs, throws, stale moves | divergent | They are original M4 fixtures, not Falcon's action, hitbox, damage, or frame tables. |
| Special moves and recovery | divergent | Pulse Bolt, Prism Burst, Arc Reservoir, and Vector Ascent are original fixtures, not Falcon specials. |
| Items, projectiles, reflector, charge | divergent | These are original technique-support fixtures rather than SSBM content tables. |
| Stage geometry, blast zones, spawns | divergent | The Relay Rod laboratory is an original test stage, not an SSBM stage. |
| Stocks, respawn, match result | partial | Deterministic four-stock flow exists; all tournament-rule and revival-platform details are not decomp-equivalent. |
| Replay, save/load, rollback state, RL API | project-specific | These are deterministic project infrastructure and have no claim of equivalence to SSBM internals. |

## Blocking work before an exact-equivalence claim

1. Import and route the remaining common action/animation-command timings for
   shield, dodges, ledges, techs, landing, and unaudited brake interrupts.
2. Audit common damage, knockback, shield, hitlag, hitstun, DI, SDI, stale-move,
   crouch-cancel, and collision formulas field by field.
3. Replace original combat fixtures with separately approved counterpart data
   before making character-wide equivalence claims.
4. Validate native Windows, WSL Linux, Wasm/browser, replay, save/load, and
   rollback results from the same content hash.

## Executable-oracle evidence

`tools/capture_ssbm_movement.py` drives an owner-supplied GALE01 NTSC-U 1.02
image through Dolphin/Slippi and records the post-frame action, facing,
position, velocity, and observed controller sample. `pf_m4_movement_trace`
replays those observed samples through the native simulator, and
`tools/compare_ssbm_movement.py` stops at the first behavioral divergence.

The current comparison passes 2,347 identical input frames covering held
dash/run, complete run turnaround and post-turnaround lockout, released dash
and run brake, direct dash dancing, moving dashbacks, two-sample dash
recognition, smash and empty pivots, basic standing turn, slow-stick sweep,
shield/light shield and defensive escapes, jump/air movement/landing, and
Falcon's complete full-down crouch start/hold/release sequence, exact and
just-beyond entry/release threshold samples, jump interruption from every
crouch state, held-crouch opposite dash/turn, crouch-release walk, and fresh
digital guard from every crouch state. Position
comparison allows only the documented accumulated float-to-Q16.16 conversion
tolerance; action, facing, velocity, and applicable action ticks use their
tighter independent gates. This remains a regression slice, not evidence that
the whole shared simulation has completed the binding equivalence gate.
