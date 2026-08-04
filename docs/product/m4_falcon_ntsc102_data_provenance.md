# M4 Falcon NTSC 1.02 gameplay-data provenance

## Authorization and scope

On 2026-08-03, the owner authorized importing frame-data tables to make the M4
placeholder move like Captain Falcon in SSBM. This record covers numeric
gameplay values and the corresponding state-machine formulas only. No ISO,
extracted DAT, executable code, art, animation, audio, hitbox dump, or stage
geometry is committed.

The source disc was the owner's `GALE01` NTSC 1.02 image. `DolphinTool.exe`
extracted `PlCa.dat` and `PlCo.dat` to a temporary directory for read-only
analysis. In `PlCa.dat`, the `ftDataCaptain` root object is at `0x9a04`; its
common-attribute pointer is `0x3754`, with the `ftCo_DatAttrs` payload beginning
at `0x3774`. In `PlCo.dat`, `ftLoadCommonData` is at `0xecd8` and the first
`ftCommonData` object is at `0x9fc0`. These offsets are provenance evidence,
not runtime dependencies.

Behavior and field meanings were checked against `doldecomp/melee` revision
`9509dc04406fb2028bfab01243841ba4787c0fb7`, especially `ft/types.h`,
`ft/inlines.h`, `ftCo_Dash.c`, `ftCo_Run.c`, `ftCo_RunBrake.c`,
`ftCo_TurnRun.c`, `ftCo_KneeBend.c`, `ftCo_Jump.c`, `ftCo_JumpAerial.c`, and
`ftCo_Squat.c`, `ftCo_SquatWait.c`, `ftCo_SquatRv.c`, `ftCo_Damage.c`, and the
common fall/air-physics routines, plus `ftCo_Guard.c` and `fighter.c` for the
shield-health and pressure formulas, `ftcoll.c` for shield-hit damage
conversion, and `ftcommon.c` for attacker-recoil initialization and decay.

## Coordinate conversion

The simulation's original stage uses different coordinate units. Horizontal
velocities use `12/115` simulation units per Melee unit, chosen so Falcon's
raw `2.3` terminal run speed maps to the existing `0.24` stage-relative target.
Vertical velocities and acceleration use `11/62`, chosen so raw `3.1` full-hop
velocity maps to `0.55`. Dimensionless multipliers and frame counts are copied
without scaling. Values are stored in deterministic Q16 fixed point.

## Imported fighter values

| Source field | NTSC 1.02 raw | Simulation mapping |
|---|---:|---:|
| walk init / acceleration / maximum | 0.15 / 0.10 / 0.85 | 9/575 / 6/575 / 51/575 |
| ground friction | 0.08 | 24/2875 |
| dash initial velocity | 2.0 | 24/115 |
| dash/run acceleration A / B | 0.15 / 0.01 | 9/575 / 3/2875 |
| run terminal / ground maximum | 2.3 / 3.0 | 6/25 / 36/115 |
| maximum run-brake frames | 30 | 29 action ticks, producing displayed frames 1 through 28 before standing |
| jump startup time | 4 | four action ticks after jump-squat entry |
| jump horizontal input / momentum / cap | 0.95 / 0.75 / 2.1 | 57/575 / 3/4 / 126/575 |
| full hop / short hop | 3.1 / 1.9 | 11/20 / 209/620 |
| double-jump vertical / horizontal multiplier | 0.9 / 0.9 | 3069/6200 / 54/575 |
| gravity / terminal / fast fall | 0.13 / 2.9 / 3.5 | 143/6200 / 319/620 / 77/124 |
| air acceleration A / B / friction | 0.04 / 0.02 / 0.01 | 12/2875 / 6/2875 / 3/2875 |
| air drift target / horizontal cap | 1.12 / 3.0 | 336/2875 / 36/115 |
| shield-break launch | 2.7 | 297/620 |
| ledge jump horizontal / vertical | 1.0 / 3.3 | 12/115 / 363/620 |
| normal landing lag | 4 | 4 ticks |
| squat entry / reverse animation | 7 / 10 displayed frames | `CROUCH START` ticks 1-7 / `CROUCH END` ticks 1-10 |
| neutral / forward / back / up / down aerial landing lag | 15 / 19 / 18 / 15 / 24 | distinct deterministic landing states |
| passive wall / wall-jump X / wall-jump Y / passive ceiling | 0.5 / 1.4 / 3.1 / 2.0 | 6/115 / 84/575 / 11/20 / 24/115 |
| air-dodge force X / Y | 3.1 / 3.1 | 186/575 / 11/20 |
| air-dodge decay | 0.9 | 9/10 |
| post-air-dodge drift cap | 1.12 x 0.6 | 1008/14375 |
| grounded player-push center offset / radius | 0.0 / 3.5 | 0 / 42/115 |
| grounded player-push nudge per overlap | 0.3 | 18/575 |

## Imported common shield values

| Source field/formula | NTSC 1.02 raw | Simulation mapping |
|---|---:|---:|
| start / reset shield health (`x260` / `x280`) | 60 / 30 | 60 / 30 Q16 HP |
| analog dead zone (`x10`) | 0.30 | first project threshold 19,661 of 65,535 |
| base hold drain (`x278`) | 0.14 | interpolated through the next row |
| hold-density endpoints (`x2EC` / `x2F0`) | 0.1 / 2.0 | 0.014 light to 0.28 dense HP per tick |
| regeneration (`x27C`) | 0.07 | 7/100 Q16 HP per unshielded tick |
| minimum size floor (`x264`) | 0.15 | 3/20 |
| pressure size endpoints (`x2D4` / `x2D8`) | 1.0 / 0.5 | light density 1 to dense density 1/2 |
| guard-stick smoothing (`x44C`) | 0.5 | recorded; exact tilt route remains unqualified |
| shield damage base / pressure (`x284` / `x288`, `x2DC` / `x2E0`) | 1 / 0, 0.1 / 0.3 | `D * (0.9 - 0.2*p)` |
| shield-stun damage endpoints / base (`x28C` / `x290`, `x2E4` / `x2E8`) | 1.5 / 2, 0.05 / 0.7 | duration `D * (1.425 - 0.975*p) + 2` |
| defender pushback scale / cap / ordinary factor (`x294` / `x298` / `x2BC`) | 0.2 / 2 / 0.6 | duration times 0.2, times 0.6 unless powershielded, capped at 2 |
| attacker recoil damage / base (`x3E0` / `x3E4`) | 0.07 / 0.02 | separate component `p * D * 0.07 + 0.02` |
| attacker recoil air decay / ground-friction scale (`x3E8` / `x3EC`) | 0.05 / 1.1 | decay by 0.05 airborne or Falcon friction times 1.1 grounded |

For normalized analog amount `a=(pressure-0.30)/(1-0.30)`, the common hold
drain is `0.14 * (0.1 + 1.9*a)`. The non-Yoshi shield size is
`initial_size * (0.15 + 0.85*(health/60)*(1.0 - 0.5*a))`. The 500-frame
pressure-only executable capture qualifies input, health depletion, release,
and regeneration. Three additional 283-frame captures qualify the sampled
light, intermediate, and dense physical shield-hit routes. Their requested /
observed pressures and SHA-256 values are 0.35 / 0.321428567 /
`563cabf633126656b80a0351b67fdffb35f664774e052e85c04ff7b20fd2e4f5`,
0.65 / 0.592857122 /
`84b462f717074b2a2984b6901ed33a2abd2b9f98527f1c52db400c98ace411ab`,
and 1.0 / 0.914285719 /
`2d95549b7ffe6ac950c339fe9dcd346b4e6c401324d2cce0e8414d2677a3489f`.
The collision path first converts attack damage to an integral shield-hit
amount, preserving a nonzero sub-unit hit as one. Here `D` is that amount and
`p` is normalized shield pressure. Exact stick smoothing and executable shield
geometry remain unqualified.

## Imported common-input values

| Behavior | Raw value | Simulation value |
|---|---:|---:|
| main-stick dead zone | 0.28 | 9175 of 32767 |
| tilt threshold | 0.25 | 8192 of 32767 |
| dash threshold | 0.80 | 26214 of 32767 |
| dash tilt window | 2 | 2 ticks |
| run continuation threshold | 0.625 | 20479 of 32767 |
| walk threshold / taper | 0.18 / 0.5 | 5898 / 1/2 |
| run taper | 0.4 | 2/5 |
| turn threshold | -0.375 | -12288 of 32767 |
| TurnRun animation / velocity-crossing hold | displayed frames 0 through 21 / hold displayed frame 9 until old-facing ground velocity is at most 0.01 | 22 animation ticks with a deterministic frame-9 hold and 68-Q16 converted threshold |
| post-TurnRun run lockout | 10 | 10 ticks |
| tap-jump threshold / window | 0.6625 / 4 | `tap_jump_axis_threshold=21709`; `tap_jump_input_window_ticks=4` |
| fast-fall threshold / window | 0.6625 / 4 | 21709 / 4 ticks |
| crouch entry / release threshold | 0.6875 / 0.625 | first accepted entry axis 22528 / exact held boundary 20479 |
| analog shield common dead zone / first accepted raw value | 0.30 / 0.30 | threshold 19661 of 65535; a digital click is 65535 |
| air-dodge X/Y dead zone | 0.25 / 0.25 | 8192 of 32767 |

The player-push values come from Falcon's `ftDataCaptain` `x2C4` vector in
`PlCa.dat` and common-data field `x450` in `PlCo.dat`. The independently
written implementation follows pinned decomp routines `ftCommon_8007DD7C` and
`ftCommon_8007E0E4`: active grounded fighters on the same connected support
receive the nudge only while the strict sum-of-radii overlap test succeeds.
Falcon's walk maximum uses the nearest Q16 encoding of `51/575`; truncating it
by one Q16 unit accumulates enough error to select the wrong executable push
boundary after a long held walk.

## Repository controls

- Only the converted constants and independently written C state machine ship.
- The temporary extraction is not a build input and is ignored by source
  control.
- Every later imported table must extend this document with source, revision,
  raw value, conversion, and destination field.
- A formal IP/originality review is required before public release.
