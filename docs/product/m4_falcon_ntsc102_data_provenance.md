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
common fall/air-physics routines.

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

## Repository controls

- Only the converted constants and independently written C state machine ship.
- The temporary extraction is not a build input and is ignored by source
  control.
- Every later imported table must extend this document with source, revision,
  raw value, conversion, and destination field.
- A formal IP/originality review is required before public release.
