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
Shield tilt and geometry additionally follow `ftCo_80091BC4`,
`ftCo_80091E78`, `ftAnim_80070108`, `ftColl_8007B1B8`, and the transformed
sphere path in `lbcollision.c` at the same pinned revision.

## Complete attack-frame table

The complete Falcon attack table is generated from the owner's same NTSC 1.02
disc rather than transcribed move by move. `gciso` revision
`01b8a938331e3e07623d5284f31a7794d1c81ef4` extracted temporary `PlCa.dat`
(SHA-256 `4cf61a52737d464df9298fd15573345fb3b9a15c79ab47dce4fd2e3e707917af`)
and `PlCaAJ.dat`
(SHA-256 `a9a0ccc2382a2f02d5423675469719488540dd119a14577712c97348f70e1c1a`).
`meleeDat2Json` revision
`d4e6074aa26f388fccc7fe8e825761cf1c1bc7b0` and
`meleeFrameDataExtractor` revision
`0b12c5cb988da3fb9b67630b1d8347e12cd91528` then produced all 50 ordinary,
grab/throw, and special subactions. Full-hitbox output was byte-identical when
generated from the owner files and the extractor project's published Falcon
source: 120,634 bytes with SHA-256
`287d53686aedb7469e455600cd749001b2f1a04081158236f26b1fae205f6dde`.

The repository importer canonicalizes the geometry-free timing/effect view to
SHA-256 `42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356`
and rejects any other input. It also verifies the owner DAT JSON SHA-256
`fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266`.
That original action-script dump is required because the upstream converter
labels opcode `0x14` as `reverseDirection`; the NTSC 1.02 decomp dispatches
argument zero of that opcode to the throw-release flag. The importer decodes
the command bytes and action-script waits directly, yielding exact normal
throw release frames 18/20/15/20 for forward/back/up/down throw. Its generated
table retains every subaction,
total/IASA/charge/autocancel/landing frame, active phase, damage, angle, KBG,
weight-set knockback, BKB, shield damage, interaction class, element, target
kind, and throw effect. Extracted DAT files and bone-relative hitbox geometry
remain temporary external evidence and are not repository or build inputs.
`tools/import_ssbm_falcon_frame_data.py` is the reproducible conversion path;
`generated/data/m4_falcon_ntsc102_frame_data.inc` is its numeric output.
The default production routes for jab 1, jab 2, dash attack, all three tilts,
all three smashes, and all five aerials consume this generated table directly.
Each action uses the imported total timing, and an exact frame lookup preserves
disjoint active windows and selects that phase's damage, angle, KBG,
weight-set knockback, BKB, and hitlag. The five aerial landing-lag values,
their distinct source-defined autocancel intervals, their explicit L-cancelled
landing durations, and pummel damage/timing also come from the same table. The
generated data is compiled once behind a small query API rather than included
separately by each consumer. Its canonical source SHA-256 is folded into the M4
content hash, so changing a late phase or non-primary effect cannot retain a
stale compatibility identity.

Ground-attack interruption is being routed from the same generated rows rather
than from authored frame guesses. The first completed route is Jab 1: imported
IASA frame 16 enables the jump/dash/crouch/turn/walk intersection present in
the pinned `ftCo_Attack11_IASA` callback, while guard, grab, special, and taunt
remain locked because that callback does not test them. A held horizontal stick
therefore follows the source callback's Dash-then-Walk ordering, and the source
animation retains displayed frame 21 before returning to Wait. Customized
timing or primary damage fails the compact reference-match guard and keeps the
project's authored fallback semantics.

Standing grab and dash grab are distinct production states. Their generated
startup/active/recovery schedules are 5/2/22 and 9/2/28 respectively (active
frames 6-7 and 10-11; total animations 29 and 39). Direct grabs from Dash or
Run and the existing boost-grab cancel select dash grab; idle, walk, shield,
crouch, and jump-squat routes select standing grab. No grab frame count is
transcribed in the default fighter.

All four production throws consume the imported damage, release frame, total
animation duration, angle, KBG, weight-set knockback, and BKB. They use the
shared integer Melee knockback calculation; custom content can explicitly
disable semantic knockback and retain the original vector response. The
source-defined collateral hitboxes on forward/back/up throw remain represented
in the complete table but cannot yet hit bystanders through the single-target
grab resolver.

The semantic jab route remains explicitly selectable so original/custom
content can retain its authored vector response without silently inheriting
Falcon semantics. Other customized action records fall back when their timing
or primary damage no longer identifies the generated default. The current
single rectangular collision volume cannot choose among simultaneous
bone-relative sweet and weak hitboxes, so a phase uses its first source-defined
effect until transformed bone geometry is represented independently.

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
| guard-stick smoothing (`x44C`) | 0.5 | shortest wrapped angle delta and stick magnitude each converge by one half per shield tick |
| initial shield size / model scale | 15 / 0.97 | full-health light-shield radii 99,501 x and 169,178 y Q16 units after independent coordinate conversion |
| neutral shield-joint center | 0.194 forward / 10.134072 up from the Melee fighter origin | 1,327 forward and 65,404 up from the simulation fighter center |
| direction-animation scale | 0.97 | 6,633 x and 11,279 y Q16 units per local animation unit |
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
`p` is normalized shield pressure.

The executable geometry oracle reads the live guard magnitude at fighter
offset `0x2344`, biased angle at `0x2348`, shield joint at `0x19C0`, and that
joint's scale, translation, and world matrix. The direction animation is
piecewise linear at unbiased angles 0/45/90/135/180/225/270/315/360 degrees.
Its local `(y, z-1)` Q16 keys are `(0,3)`, `(2.5,2)`, `(4.5,1)`,
`(2.5,-0.200073242)`, `(0,-1)`, `(-1,-0.200073242)`,
`(-1.799804688,1)`, `(-1,2)`, and `(0,3)`. The two small non-decimal values
are the values encoded by the owner executable rather than rounded design
values. `tools/capture_ssbm_movement.py --memory-probe-shield` records these
fields; the 270-frame cardinal/diagonal route and 2,158-frame angular sweep
both pass the identical-input comparator. Their SHA-256 values are
`02b420230efdaf105889c73ec413ff459eadbf98103a4a6a6dea0dacfa49e92f` and
`fb90e6173feb98139019ddd98eda05390bbf7ed38ebad662b1eedb2f1c22f9f0`.

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
