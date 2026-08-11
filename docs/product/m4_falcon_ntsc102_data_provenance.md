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
common fall/air-physics routines in `ftCo_FallSpecial.c`, plus
`ftCo_Landing.c`, `ftCo_Guard.c`, and `fighter.c` for the
shield-health and pressure formulas, `ftcoll.c` for shield-hit damage
conversion, and `ftcommon.c` for attacker-recoil initialization and decay.
Roll and air-dodge callback semantics additionally follow `ftCo_Escape.c`,
`ftCo_EscapeF.c`, `ftCo_EscapeB.c`, `ftCo_EscapeAir.c`, and the shared
`ft_80085004`/`ft_80085030` animation-translation path.
Shield tilt and geometry additionally follow `ftCo_80091BC4`,
`ftCo_80091E78`, `ftAnim_80070108`, `ftColl_8007B1B8`, and the transformed
sphere path in `lbcollision.c` at the same pinned revision.

The 2026-08-10 static differential additionally follows `ftCo_Wait.c`,
`ftwaitanim.c`,
`ftCo_Damage.c`, `ftCo_Guard.c`, `ftCo_Ottotto.c`, `ftCo_Catch.c`, the common
throw/release routes, `ftCo_Rebirth.c`, `ftCo_Rebound.c`, `ftcoll.c`, and the
complete Falcon neutral/side/up/down-special callback families. It imports
typed values from the same pinned common/Falcon data and does not introduce
guessed frame constants. That slice was intentionally not built or executed;
its provenance is source-level until the next validation pass.

The later base-Wait geometry slice enters Wait through Falcon's native
`SquatRv` callback rather than sampling incidental menu idle. Its final paired
hurt captures have raw SHA-256 values
`a9c9b5456f421304e08ae5b63165283b529a3612ee6d991379836f6cb3314dc3`
and `ef72ec895024aa44f96d0b6f99f2913a7d2b370583ab1535654581ff1b7a1089`.
All ten SquatRv observations and direct Wait frames 6-59 contribute 128 source
observations / 1,408 hurt capsules plus ECB and agree with the HSD evaluator
within one Q16 unit. Independent surface-memory captures with raw SHA-256
`0146d99544d0908fc60d962d53cb7356a347175023804e3a49e8e56303e7f1f0`
and `60b2dc3cd36d630cfd7b9d0414ff47d6220cb6a6e27915d44c20be30b00d81c9`
prove frames 0-5 as the ordinary six-update moving-target recurrence under
semantic SHA-256
`0c7ba43ab7022bc2e88bcb369e4fcb9812ebf2397abd7afddfed931e61734983`.
The entry source is SquatRv frame 10, one update beyond its last displayed
frame. Three direct and six transition observations protect the entry route.
`ftCo_Wait_Anim` then calls `ftCo_8008A7A8`; `ftwaitanim.c` and
`baselib/random.c` define the weighted secondary selection and process-global
HSD LCG. The DAT supplies Wait/Wait2/Wait3 weights 70/20/10 and blend bytes
6/0/0. Two byte-identical 440-row captures have raw SHA-256
`d97474f2a15912b1c98fba9b7444883c1db4798290702c311b13bcffb4cc7f7b`
and semantic SHA-256
`afefafe17e8769bc39391d0605d7c392f25ef4d146cf0d868eb895eeee84b570`.
The source theorem predicts and checks 14 uninterrupted RNG draws and two
same-secondary rejection draws across the repeat pair; the live captures
validate the resulting state transitions. Production retains the exact 32-bit global stream,
commits draws transactionally, advances 60/75/70-frame source clocks, and
reproduces zero-blend variant entries plus six-update return/restart blends.
All 145 direct Wait2/Wait3 samples / 1,595 capsules and their ECBs agree with
the independent DAT/HSD evaluator within one Q16 unit; 32 stored boundary and
blend poses protect the native route.

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
`0b12c5cb988da3fb9b67630b1d8347e12cd91528` then produced Falcon's complete
50-slot ordinary, grab/throw, and special schema. It contains 48 concrete
subactions: the common extractor schema's `fsmash_mh` and `fsmash_ml` slots are
absent because Falcon's DAT defines only high, straight, and low forward-smash
angles. Full-hitbox output was byte-identical when
generated from the owner files and the extractor project's published Falcon
source: 120,634 bytes with SHA-256
`287d53686aedb7469e455600cd749001b2f1a04081158236f26b1fae205f6dde`.
The extractor's default list omits character specials, so the Falcon run
explicitly includes the contiguous `0x12d` through `0x13d` rows (DAT
subactions 301 through 317): ground/air neutral special, side-special start
and hit, up-special start/catch/throw, and every down-special ground/air/end
variant. They are part of the same pinned generated table, not later authored
timing constants.

The repository importer canonicalizes the geometry-free timing/effect view to
SHA-256 `42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356`
and rejects any other input, move ordering, or absent row beyond those two
source-defined slots. It also verifies the owner DAT JSON SHA-256
`fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266`.
That original action-script dump is required because the upstream converter
labels opcode `0x14` as `reverseDirection`; the NTSC 1.02 decomp dispatches
argument zero of that opcode to the throw-release flag. The importer decodes
the command bytes and action-script waits directly, yielding exact normal
throw release frames 18/20/15/20 for forward/back/up/down throw. Its generated
table retains every subaction,
total/IASA/charge/autocancel/landing frame, active phase, damage, angle, KBG,
weight-set knockback, BKB, shield damage, interaction class, element, target
kind, and throw effect. It also hash-verifies raw `PlCa.dat` and `PlCaAJ.dat`,
reads every action's packed flags, and decodes every source translation-N
animation track into per-frame Q16 deltas. The small FigaTree decoder follows
the scalar codecs and interpolation rules in HSDLib revision
`29546ad77fdf9ebd9a9940ed44903ef309e810d6`; the resulting movement is still
qualified against Dolphin rather than accepted from the parser alone.
Extracted DAT files and raw executable-memory captures remain temporary
external evidence and are not repository or build inputs.
`tools/import_ssbm_falcon_frame_data.py` is the reproducible conversion path;
`generated/data/m4_falcon_ntsc102_frame_data.inc` is its numeric output.

The same import now covers Falcon's entire `PlCa.dat` submotion catalog rather
than treating the 50 attack-oriented rows as the whole character dataset. It
rejects any catalog other than the source's 318 slots, decodes all 275 present
FigaTree archives, and preserves the 43 intentional no-animation slots. The
decoder visits every one of their 17,271 nodes, 38,560 tracks, and 308,057
keys; their canonical decoded SHA-256 is
`d8a09bf451ce547d8d24634f40f654564e42cbf14ad5339a0c7d93ff7edc15dc`.
Every catalog row stores the raw animation endpoint count, extractor-compatible
last gameplay frame, action-script event count/offset, packed action flags, and
animation byte size. The generated catalog SHA-256 is
`9bd124115e6eb66db0f6152dd6fade2886c85d1ae2368b4ae88b9084c7cc67ce`.

The import also losslessly retains all 2,056 action-script event boundaries and
all 16,516 encoded bytes in one immutable descriptor table and byte blob. Each
event validates its declared length, four-byte alignment, bounds, and
`encoded[0] & 0xfc` opcode. The complete script SHA-256 is
`6bf1021da93ea2f829c812b2bc425fe310808c8ad6e6e75eaf103c42b7ea4cfe`.
O(1) span access exposes unknown commands without interpreting them at runtime;
typed throw and special-command tables continue to be generated from the same
source stream. Full animation keys remain an offline source surface and are
emitted as compact Q16 tables only when production behavior consumes a track,
avoiding a multi-megabyte unused runtime asset.

The generated behavior surface now includes every Falcon submotion whose
packed animation flags select a translation-N node: 65 submotions and 2,536
displayed-frame X/Y deltas in one immutable pool. The node index is
`(flags & 0x3f) - 1`; masking only the low byte is incorrect for flags such as
EscapeF's `0x800000c2`. Each catalog row stores an O(1) offset/count span, and
attack/root-motion accessors reuse the same pool rather than generating
parallel tables. Regeneration produces
`generated/data/m4_falcon_ntsc102_frame_data.inc` byte-for-byte at SHA-256
`baa5d0bd55cc116fa5c6451179b71763b7df5ebd86183f82ce7e531874764df2`.
The expanded behavior-bearing source digest is
`77dc51740b77c3f80e55571bf44aa321a56cef3b22b76f7b320982675421da41`;
the separate logical catalog digest remains stable because derived spans do
not change its original source fields.

A generated 318-row body-collision command view decodes raw `waitUntil`,
`waitFor`, state-2, and state-0 bytes without relying on extractor labels.
Production tech-in-place/roll invulnerability duration now comes from the three
matching frame-0-to-20 source windows, and neutral getup validates both prone
orientations' matching frame-0-to-23 windows. The duplicate authored 20/23
assignments were removed. Getup-attack, ledge, and other unaudited windows are
retained as source commands but are not equated with runtime invulnerability
until their state-specific displayed-frame semantics are qualified.

EscapeN, EscapeF, EscapeB, and EscapeAir are now qualified rather than merely
retained. Ground-dodge displayed frames map directly to production action
ticks; EscapeAir maps displayed frame `n` to tick `n-1`. Raw body-state
commands drive the invulnerability intervals. Forward and backward roll use
the single generated translation stream selected by the decomp callback:
`ft_80085030` replaces ground velocity from the TransN offset, so production
does not add a second authored displacement channel. Direction is a facing
transform of that same source stream, and forward roll's exact frame-29 delta
is 280 Q16 units.

EscapeAir common values come from `PlCo.dat` offsets `0x32c` through `0x340`:
equal-axis dead zone 0.25, three-tick early item-throw window, force 3.1, and
decay 0.9. The item-throw timer is retained for the later common-item IASA
route; the no-item defense capture does not claim to qualify it.
Because EscapeAir physics runs on its entry frame, the generated runtime view
stores the first visible converted X/Y velocities (19,080/32,440 Q16), not a
second pre-decay transient, plus decay 58,982 Q16 and dead zone 8,192. Raw
opcode `0x4c` is decoded as command-variable index `encoded[0] & 3` plus a
24-bit value; EscapeAir writes variable 0 to 1 on displayed frame 30. Before
that gate the callback decays both axes; from it onward the same EscapeAir
action executes ordinary aerial input and gravity, exactly as
`ftCo_EscapeAir_Phys` delegates to `ft_80084DB0`.

Floor contact during EscapeAir uses a 48-frame animated ECB-bottom sequence
from the same schema-9 live-memory route, rather than the generic body extent
that landed one frame early. The complete 329-frame Final Destination defense
capture has SHA-256
`d9dfebcb6e42f5e71ece08490429b61083f81bee067def379b5fdd6270d96b95`.
It covers forward roll, spot dodge, backward roll, held-L/fresh-R upward air
dodge, ordinary-physics handoff, and landing, then a fresh down-left air dodge
whose horizontal LandingFallSpecial route crosses Falcon's walk-speed friction
threshold. Action, tick, grounded state, facing, invulnerability, and velocity
compare strictly; position stays within the established 640-Q16 representation
envelope. Its same-binary unaccelerated control has SHA-256
`d78abcfe3d252d0f87409aba3343cd838efb739d6311494d520f2f076eb5255f`;
the accelerated/control comparison passes all 329 rows, including 225 active-
fighter rows and 185 qualified active-pose rows.

DownBoundU and DownBoundD likewise keep their ground-action physics and retained
floor line separate from their animated ECB contact result. Both imported
26-frame schedules report contact on displayed frames 1-4, no contact on frames
5-22, and contact again on frames 23-26. The canonical two-orientation schedule
has SHA-256
`6c8d97ff1076075616ed06f88c742528eff9c2fb18ab9f2cce09ba895147e556`
and is stored as one allocation-free 32-bit mask per orientation. Production
therefore preserves DownBound friction, root motion, and support identity while
matching the live contact flag; it never converts the middle 18 frames into an
ordinary airborne fall action.

Default production timing for dash, standing/run turn, run brake, ordinary
landing, crouch start/reverse, shield release, spot dodge, both rolls, air
dodge, tech in place/roll, neutral getup, getup roll/attack, and both Falcon
appeals now comes through that generated catalog. Each runtime state selects
either the raw animation endpoint or last gameplay frame according to its
decomp-qualified transition comparator; run brake and appeal retain their
explicit entry-tick counter adjustment. The resulting numeric defaults are
unchanged, but they are no longer duplicated as handwritten frame constants.
Unimplemented common callbacks and pose/command semantics remain fidelity
work; catalog completeness is not presented as behavioral equivalence.

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

The same importer now reads the complete 97-word `ftCo_DatAttrs` common-
attribute payload and the complete 0x8c-byte, 35-field
`ftCaptain_DatAttrs` special-attribute payload directly from the pinned raw
DAT. It preserves all raw common words and exposes a typed, generated Q16.16
view for every common field currently consumed by the simulation: walk,
dash/run, traction, jump, double jump, gravity/fall/fast fall, air mobility,
ledge/wall jump, shield-break launch, weight, jump startup, and landing lags.
`pf_m4_default_content` consumes this typed view rather than repeating authored
ratios. The importer also decodes `ftLoadCommonData` pointer 3, the exact
`Fighter_804D6548` stale-move table, as 9/8/7/6/5/4/3/2/1 percent and routes
that generated nine-slot view into default content. The raw attributes, typed
views, special block, action table, collision poses, Falcon Dive victim reaction
boundary, and source hashes form complete-source SHA-256
`af9020d9a33ccfe37fb0fa86bf89a97d18ed54ddfef54ba2f58e3067ddaa4d2c`.
Fresh regeneration from the five pinned inputs byte-matches the checked-in
include at SHA-256
`08c277adad0fcd126d01351a118578cb73a6ba573f0b8377a26d8f9653f523d8`.
The production ground/aerial Falcon Punch, Raptor Boost, Falcon Dive, and all
seven Falcon Kick states consume this imported data directly. The project's
original directional-special fixtures are not evidence for those source
moves.

Falcon Kick's imported ground-hit attributes are also executable-oracle
qualified rather than transcribed or tuned. The 170-row Dolphin memory capture
SHA-256 is
`0dcf3574554a97a4760ff93e51dd24ebeb6d84d8dc5c23e777054ebe46a5ac32`;
its 77 comparable rows hit on displayed frame 16, deal 15 damage, and preserve
eight ticks of hitlag. After hitlag, the captured self velocity is exactly the
imported 0.6 on-hit multiplier of root velocity. Ground-end samples expose the
source callback's distinct ground and self velocity channels: unscaled ground
friction advances beside the scaled self velocity instead of repeatedly
scaling the already-scaled result. Production reconstructs that bounded channel
from the generated root-motion, entry-scale, common friction, fast-friction,
traction, multiplier, and hit-count-cap fields with no allocation or additional
serialized state. The six-route Falcon Kick verifier now passes 399 comparable
frames. Its 253-row Hyrule Temple wall capture, SHA-256
`fd4b04d9128486d2b690ce7d9b701fa12c7762367d5f822d2f3baca3c3f0d70e`,
adds 58 comparable frames and naturally enters executable action 363 from a
grounded kick. The setup selects St_Kind_Shrine from the source
`MnSlMap.usd` cursor animation, relocates Falcon only while legitimately
airborne, and lands through the normal collision callback beside the rising
wall in `GrSh.dat`. The route qualifies displayed-frame-22 wall contact,
preserved entry self velocity, no same-tick floor reattachment, and the full
rebound root trajectory. No Falcon Kick dynamic route remains unqualified.
The same six routes are registered through the generic native-CSV stored
domain with exact input equality and sparse per-field masks. Canonical
live-source and production-trace SHA-256 are
`2c6f28a9701990b913adb2f2daa214433bb18174a610af6c96fc1dce39deaf33`
and `19a4dd302f0e51fa9d01d8fe7193d57e1b3ea5979e496fb6138bc0c85f356f4e`.
The live projection also guards the ground-origin edge-conversion update: it
enters the aerial end state at zero self velocity without same-update ordinary
air gravity.

Raptor Boost likewise consumes only the generated Falcon action, command,
root-motion, search-sphere, effect, and attribute tables. A clean 431-row
Dolphin memory capture containing its two miss routes has SHA-256
`81cafb4d75e75c1f876b6a903a770a3e20376d0399d9374cab19d7feea413602`.
`tools/verify_m4_raptor_boost.sh` selects the routes directly from that pinned
capture and compares 80 grounded-miss and 180 aerial-miss frames in addition
to the existing 46-frame ground hit. A 192-row natural-floor aerial-hit
capture at SHA-256
`f3f8518c103958f6b6e56e76b5bc728c0b3da0854f26a022fac978869f6d051b`
adds 145 comparable frames. After one pre-action setup relocation, both
fighters launch only through controller input; no position is held or changed
during the compared route. It qualifies frame-18 search conversion, the
imported active frame-3 seven-damage hit, five-frame hitlag, the full hit-state
natural pre-landing tail, air-to-ground conversion, all 40 landing-lag ticks,
and standing return.
A separate complete-state capture at SHA-256
`86e0abff2d1de0483e25ef8db045da323a35331bf95fb7089b00283233b4fc8e`
supplies all displayed frames 0 through 44 of `SpecialAirS`'s live ECB bottom.
The generated 45-entry Q16.16 table delays floor contact by the exact one frame
observed in the natural route and retains incoming vertical velocity on the
first landing row. Both miss paths enter the same imported
eight-frame common `FallSpecial` ECB-bottom cycle used by Falcon Dive through
one allocation-free action predicate; no duplicate Raptor pose table exists.
The aerial transition row applies Falcon's ordinary imported gravity, as the
executable does, instead of suppressing it as if the active special were still
launching the fighter. A separate 210-row ground-edge capture at SHA-256
`3b59fbf62ad880ffe88d694e6e69f0a0b08f23fd7e4714c9a0cf4a41194744ae`
adds 51 strict frames. It uses an ordinary opponent dash to keep the search
route empty, then qualifies `cmd_vars[2]`-gated floor loss, the full frame-20
root step, the source air-speed clamp, no transition-row gravity, and common
`FallSpecial`.

A 155-row native item-search capture at SHA-256
`9efacb94277b8cb870f8c69008e5dd248d4d31cec17e9713323abde94a577028`
closes the remaining source branch. Slippi normally overwrites the live match
rule with item frequency -1, so the capture's isolated Gecko configuration
replaces only `gm_8016AE80` and `gm_8016AEA4`: Very High frequency and runtime
item-kind mask bit 0. Melee's original `it_8026D018` ambient spawner then
creates and settles a real Capsule. Falcon is relocated once before input to
ten units left of that item, while the opposing fighter is kept at least 100
units away. The original search callback selects grounded hit action 350 at
the first live `cmd_vars[0]` gate, displayed start frame 15. The verifier also
pins the otherwise non-obvious item-switch mapping: Capsule runtime kind 0 is
saved preference bit 29 through `lbl_803B7844`.

The decomp predicate accepts runtime container kinds 0 through 5, its two
enemy-kind ranges, and the random Pokemon kind. It rejects ordinary weapon
items. The project's Relay Rod is original content and therefore correctly
does not activate Falcon's source item-search branch; no invented positive
classification is attached to that custom fixture. The at-will Raptor Boost
suite now covers 657 frames across all six routes.

Five fighter routes are also registered in the character-independent stored
gate as 502 numeric samples. `native-csv-trace-v1` reuses the same production
`pf_m4_movement_trace` executable and the same controller normalization and
Raptor action/timer projection as the live comparator; no second C state setup
exists. Per-field row exclusions preserve the live clock boundary exactly for
hitlag-frozen and special-landing samples. The pinned source projection is
`19b5d604d5721e20bc2151e41c11054632a5c384dfd5528cf373dac2bd1abe2c`;
the production projection is
`7733655e234ac2de12fe1b674ed6be967ad7de39b848d94cf97b2e36547509a0`.
The Capsule branch remains live-only because production intentionally has no
source Capsule counterpart.

Ground-attack interruption is routed from the same generated rows rather than
from authored frame guesses. Jab 1/2 use their chain callback; dash attack,
forward/up tilt, and up/down smash use `ftCo_Wait_IASA`; down tilt uses its
restricted common-plus-attack callback; and forward smash uses the Wait set
without escape. Imported IASA frames gate each policy. A held horizontal stick
therefore follows the source callbacks' Dash-then-Walk ordering, down tilt
returns through crouch state, and each source animation retains its final
displayed frame. Neutral-special preprocessing shares the same source IASA
query, so it cannot consume B before the movement state machine sees a legal
interrupt. Customized timing or primary damage fails the compact
reference-match guard and keeps the project's authored fallback semantics.

Animation translation is data, but whether it moves the fighter is decomp
behavior. Jab, dash attack, and forward smash use the common root-motion
physics callbacks; tilts and up/down smash use ordinary ground friction even
when their animation files contain translation tracks. Falcon dash attack's
first source delta is 8,930 Q16 simulation units and the complete Dolphin
replay matches position and velocity without the former authored speed guess.
The final grounded-normal/IASA matrix covers 5,450 identical-input Dolphin
frames, hashes to
`3596f20946bc6e8bd629ec875442857e8986fca6a69fc7a530a8ed6630cc24b1`,
and passes the comparator's 640-Q16 position allowance. Its forward-smash
selection is driven by the source main-stick input-age timer, not action age;
facing-relative tilt selection likewise preserves the source backward-A
fallthrough to Jab.

Aerial interruption consumes the same generated move rows. Falcon fair, back
air, up air, and down air expose IASA frames 36, 29, 30, and 38; neutral air's
zero value is retained as no IASA. Pinned `ftCo_AttackAir.c` routes those
boundaries through the shared double-jump check after item/tether candidates.
The runtime's entry output maps displayed frame 1 to action tick 0, and its
interrupt callback evaluates the pending displayed frame, so the exact
pre-step tick is `IASA - 2`; `IASA - 3` is the adjacent negative boundary.
One generated IASA predicate accepts an explicit displayed frame and is reused
by ground, special-preprocessing, and aerial call sites without duplicating
tables. One double-jump transition helper likewise owns velocity, jump-count,
fast-fall, state, and tick updates.

The 1,250-frame Final Destination Dolphin capture at SHA-256
`3a03c28fa78cf0c4de8fb7b4f4c873dee5df9ca44f46740b0f053da62cc2efaf`
passes the identical-input comparator across its 350 actionable jump,
jumpsquat, aerial, and interrupt frames; settle/recovery rows advance both
sims but are not counted as qualified evidence. It covers a jump pulse one
displayed frame before and exactly on every nonzero aerial IASA, plus a
penultimate-frame neutral-air negative control. It also proves that C-stick aerial input selects
the same directional action scripts as A-button direction. The production
route gates this mapping on exact Falcon reference content, preserving the
project's authored strong aerial for customized content. This capture qualifies
the double-jump branch only; item throw/pickup and tether candidates remain
separate executable-oracle work.

Standing grab and dash grab are distinct production states. Their generated
startup/active/recovery schedules are 5/2/22 and 9/2/28 respectively (active
frames 6-7 and 10-11; total animations 29 and 39). Direct grabs from Dash or
Run and the existing boost-grab cancel select dash grab; idle, walk, shield,
crouch, and jump-squat routes select standing grab. No grab frame count is
transcribed in the default fighter.

All four production throws consume the imported damage, release frame, total
animation duration, angle, KBG, weight-set knockback, and BKB. They use the
shared integer Melee knockback calculation; custom content can explicitly
disable semantic knockback and retain the original vector response. A pinned
894-row Dolphin capture at SHA-256
`368c623e49231aff0f70c8aa687345f10e615b121a675dbddcb8abd99a3a0b95`
supplies the three moving attack spheres on forward/back/up throw. Its 181
throw-action samples prove that the captured victim takes the ordinary 5/5/4%
hit with four synchronized hitlag frames, then the separate 4/4/3% throw at
the DAT-decoded release frame with zero release hitlag; down throw has no
ordinary hitbox and releases 7% with zero hitlag. Production preserves the
capture link for that first hit and routes the same generated spheres through
the ordinary fixed-capacity collision path for bystanders.

The semantic jab route remains explicitly selectable so original/custom
content can retain its authored vector response without silently inheriting
Falcon semantics. Other customized action records fall back when their timing
or primary damage no longer identifies the generated default.

## Executable hit geometry

The complete 50-slot frame-data table above is distinct from animated spatial
geometry. A 2,974-row Dolphin 3.5.1 ExiAI headless/null/unlimited capture reads
the live transformed attack
spheres from the owner's `GALE01` NTSC 1.02 executable. The disc SHA-256 is
`0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464`;
the raw capture SHA-256 is
`aeff75c16b2041fbecc6b8ec2322a614e0695f0d3d9088eb44d60aedbdeb7ca0`.
An independent repeat has raw SHA-256
`5a797d05fe1dfd30ee1a82b7ede3cac3c003a668d20dcd1d53b824450e19bd55`;
`tools/verify_ssbm_oracle_acceleration.py --same-runner-repeat` removes process
addresses and non-importable idle, hitlag, or inherited-pose noise and requires
all 2,974 rows plus 1,312 qualified fighter and 504 opponent pose rows to match
semantically.
The memory layout and transforms were checked against `doldecomp/melee`
revision `9509dc04406fb2028bfab01243841ba4787c0fb7`. The capture records the
active `ftHit` spheres and converts each bone-relative point through the live
`HSD_JObj` world matrix. The importer also verifies every sphere's damage,
angle, KBG, weight-set knockback, and BKB against the hash-pinned full
`meleeFrameDataExtractor` output, so a geometric sphere cannot silently select
an unrelated effect row.

`tools/import_ssbm_falcon_hit_geometry.py` converts that evidence into
`generated/data/m4_falcon_ntsc102_hit_geometry.inc`. The compact table includes
jab 1-3, the rapid-jab start/loop/end lifecycle, dash attack, all five forward-
tilt angles, up/down tilt, Falcon's real high/mid/low forward smashes, up/down
smash, all five aerials, standing grab, dash grab, and all three ordinary
throw-hitbox actions,
and every damaging or grabbing Falcon-
special phase. The special capture set covers ground/air Falcon Punch;
ground/air Raptor Boost start and hit; ground/air Falcon Dive start, catch, and
throw; and every Falcon Kick ground, air, landing, edge-end, and wall-rebound
subaction. Non-damaging Raptor Boost search volumes are retained as pose/state
evidence rather than mislabeled attack spheres.
Lookup is a move-indexed
offset plus one frame-indexed row; collision uses fixed-capacity stack storage
and performs no allocation. All simultaneous spheres remain independent, so
sweet and weak hitboxes select their own generated effect. Jab 2's displayed
frames 5-7 and up aerial's displayed frames 6-13 follow the live executable
capture where its post-pose collision state differs from the static
action-script boundary. Standing grab's two spheres are live on executable
frames 7-8; dash grab's three spheres are live on frames 11-12. Grab collision
uses the same fixed-capacity sphere path as attacks and considers only source
hurt capsules whose live `grabbable` flag is set.

The resulting canonical geometry SHA-256 is
`fef2aad15411328bed10ba5726edca6312c469b77cbba65d0d8e1e53cf5c5923`.
Jab 3 contributes only live-reachable frames 1-12; rapid jab maps each repeated
active window to its canonical source command phase. The importer rejects the
static extractor's non-executable forward-smash-low frame-63 spill because live
hit memory is inactive after frame 21.

The same generated representation now consumes a natural paired pummel route
from raw capture SHA-256
`9385eb7e314f161274e5d79ad458b5ae7091f29e1490db370aaa35690227fe9b`.
It imports CatchAttack displayed frames 0-23, the single frame-4 pummel sphere,
CaptureWaitHi frames 0-34, CaptureDamageHi frames 0-19, and the natural
53,806-Q16 horizontal / zero vertical capture anchor. A second raw capture at
`8871fd5291ced3a28f61e74fdd0cd7b2f255dfa71a09bcf0e7ee92b08a4f6322`
reproduces the same address-free semantic rows. The shared collision resolver
uses the ordinary pummel sphere for bystanders while excluding the linked
captured target; that target follows the source no-launch CaptureDamage path
with synchronized four-row hitlag and then returns to CaptureWait. The generic
stored domain hashes all 79 holder/victim poses under source digest
`e1ba78ad8a537c192849a295d9db896e0d165e70a13bbe7488bdd85e3885fead`
and production digest
`7b13e659fa8be4e22ef2d390dc029463cbc712547801b4c63e7f1db0068fa2a7`.

The same probe reads Falcon's 11 live `FighterHurtCapsule` records from
`fighter+0x11a0` with stride `0x4c`; one route-qualified capture now owns both
hit and hurt geometry instead of maintaining duplicate ordinary-action traces.
It records every executable displayed frame of all 26 concrete ordinary action
slots before pummel. Additional hash-
pinned executable captures cover every displayed frame of all 17 Falcon
special subactions. The final wall-rebound row reuses the source-defined Falcon
Dive throw animation exactly as the pinned DAT motion-state table does; it is
not an invented pose. The importer rejects even one missing source frame
instead of cloning the previous pose. The phase-pinned Stand frame-18 pose
remains the grounded-idle route. A separate 4,198-row Slippi Dolphin 3.5.1 ExiAI
capture,
SHA-256
`3d1d6b0047fadc3dc53cef830f0784216e8967f0e7424a08736ca787bec26de6`,
adds every displayed Initial Dash frame 1-15, RunBrake frame 1-28, CrouchStart
frame 1-7, CrouchEnd frame 1-10, KneeBend frame 1-4, SpotDodge frame 1-32,
RollForward frame 1-31, RollBackward frame 1-31, AirDodge frame 1-49,
FallSpecial frame 1-8, and speed-scaled LandingFallSpecial source frames
1, 4, 7, 10, 13, 16, 19, 22, 25, and 28, plus ordinary Landing frames 1-30.
Its
controller-port cross-check requires both live poses to canonicalize to the
same Falcon Q16.16 capsules; capture metadata alone is not accepted as proof
that the menu spawned the requested character. The ExiAI path is separately
qualified against an unaccelerated same-binary control at SHA-256
`32a0a742012f360c1e49b27d2fb2023e16eac5af23694b032a3777d41ad16a9d`;
the A/B comparison covers 2,011/1,149 active fighter/opponent rows and
1,909/1,120 qualified action-owned pose rows. Every collision control first
pre-places both ports safely, settles them, establishes explicit facing, and
fully recovers before final placement; this prevents preceding routes from
leaking airborne state, facing, or velocity.

The ordinary-action pair also supplies a non-duplicated collision-ECB theorem.
The source evaluator derives six ECB selector joints from the same owner DAT,
costume model, and parent-closed HSD hierarchy as hurt geometry, then applies
the decomp `mpColl_LoadECB_JObj` rules. Manifest-owned per-motion offsets map
the executable's one-based displayed action frame to the HSD request frame.
Across both captures, Jab 1/2/3, all three rapid-jab phases, Dash Attack, all five
forward tilts, Up/Down Tilt, the three real forward-smash angles, Up/Down
Smash, standing/dash grab, and all five aerials reproduce 2,086 observations /
1,850 unique frames at maximum one-Q16 error. Production uses the imported
move's existing subaction index plus one generated offset switch; all 925
represented runtime frames are covered by the native evaluator test. A
generated view of the DAT submotion animation flags chooses whether Melee has
extracted/zeroed TransN or retained it in the model skeleton. This closes Rapid
Jab Start in model-root space without a move-specific exception, while the
shared ten-update common-air lock owns aerial bottom behavior.

Looping idle, `Ft_MF_SkipAnim` GuardReflect inheritance, and positive-hitlag
bone endpoints are forbidden as geometry sources, so every imported KneeBend,
SpotDodge, RollForward, RollBackward, AirDodge, FallSpecial, and
LandingFallSpecial and ordinary Landing samples come from dedicated active,
non-hitlag tracks. The
pinned `ftCo_Escape.c` SHA-256 is
`762d18265d193e9d4b0b701a7a8048bb8824a4de5f505ceef00e316c1e56fb89`;
its generated state-two/state-zero commands and executable trace agree on
SpotDodge invulnerability frames 3-20. A facing-controlled Jab 1 route hits the
pending frame-24 pose at 21.0 Melee units and misses at 22.0, while the generic
rectangle falsely misses the positive route. The same source commands and
executable agree that both rolls are vulnerable on frames 1-3, invulnerable on
4-19, and vulnerable on 20-31. Jab 1 hits RollForward frame 22 at 12.98 and
misses at 14.18, where the generic rectangle falsely hits; it hits RollBackward
frame 24 at 20.00 and misses at 20.75, where the rectangle falsely misses the
positive route.
The pinned `ftCo_EscapeAir.c` SHA-256 is
`cdff68de39d55855f1ca02b8e4af09ce856a1133cc21b23921a881b23e0dfaf6`.
Its state commands and executable agree that AirDodge is vulnerable on frames
1-3, invulnerable on 4-29, and vulnerable on 30-49. A neutral offstage route
keeps the low frame-31 pose airborne: Jab 1 hits at +21.0 Melee units and a
+3.0-unit root height, then misses at +21.8. Reconstructed margins are
+0.391159288/-0.406612492; the generic rectangle falsely misses the positive
route at -2.563399506. A single move/frame lookup feeds exact 2D
projection for overlays and full 3D point-versus-capsule intersection for
reference attacks and grabs without allocation. Both endpoints of every hurt
capsule and every hit-sphere center retain the executable capture's Z
coordinate; facing reflection applies to X and Z as it does to the source
model transform. Authored item/projectile rectangles retain their separate 2D
route.

The pinned `ftCo_FallSpecial.c` SHA-256 is
`19217b0e24dc138f601b4c9914975da0879ece0a71ef968272fac75238aad6f4`;
the pinned `ftCo_Landing.c` SHA-256 is
`7e33d64809df680df293eeec1189299ab0f77d633f39c00dcd6756faab7d08e8`.
The motion-state table resolves common state 35 to common submotion 26 and
LandingFallSpecial state 43 to common submotion 36; similar character-local
names are not accepted as substitutes. FallSpecial exposes its complete
eight-pose loop. LandingFallSpecial's animation rate exposes the ordered ten-
pose sequence above, rather than all 30 source frames. A pending FallSpecial
frame-5 control hits at 15.5 Melee units and misses at 16.2, with reconstructed
margins +0.563753525/-0.130038736; the generic rectangle falsely hits the miss
at +2.936591339. A pending LandingFallSpecial source-frame-7 control hits at
18.5 and misses at 19.3, with margins +0.509528504/-0.137582566; the generic
rectangle falsely misses the hit at -0.063411903. Common state 42 resolves to
ordinary Landing submotion 15. With no interrupt, the executable exposes all
30 displayed poses even though Falcon's input gate opens after frame 4. Jab 1
evaluates the pending source-frame-22 pose: it hits at 20.3 and misses at 20.6,
with reconstructed margins +0.142592999/-0.150664469; the generic rectangle
misses both at -1.863413048 on the positive route.

The same pinned landing source proves there is no LandingFallSpecial root-
motion correction. `ftCo_Landing_Enter` grounds the fighter through
`ftCommon_8007D7FC`, which copies horizontal self velocity to ground velocity;
`ftCo_Landing_Phys` then calls only `ft_80084F3C`. The latter uses Falcon's
ground friction multiplied by common value `x6C` while absolute ground speed
is greater than walk maximum, otherwise ordinary ground friction, before
calling `ftCommon_ApplyGroundMovement`. The pinned `ft_084E.c` SHA-256 is
`0b14d8214bd4f4f5cd47269d658ecf829ede9b42a756c1492ca21a38abb7e9ad`;
the pinned `ftcommon.c` SHA-256 is
`6a85efe9ef6997a23e5b91fb3c6165e70ca00aac0c617d46c92dc28a5bb86194`.
The imported common submotion 36 has no translation stream. In the qualified
flat-stage rows, every position delta equals the post-friction velocity: the
first five landing velocities decrease by 0.16 Melee units per tick above the
threshold, then by 0.08 below it. Before removal, the parallel authored shim
caused the first divergence at capture row 310/source frame 4 by 1,889 Q16
position units while velocity already matched.

The canonicalized timing, hit-sphere, standing-pose, action-pose, and common-
pose tables
hash to
`377fb771847ff7a6a3dcb6c02e648787c279fc68a5c3eee9aab16ce23d5fe645`.
Pinned regeneration produces the tracked include at SHA-256
`b39310bffe4ac6ec61e4711481e28f24dd01811dcdfdded7d52c921ac8ad415e`.
That digest is compiled into every M4 content hash, so changing geometry cannot
retain an old compatibility identity. Production combat queries this table
for implemented normals, aerials, grabs, normal throws, all 17 Falcon special
subactions, Initial Dash, RunBrake, CrouchStart, CrouchEnd, KneeBend,
SpotDodge, RollForward, RollBackward, AirDodge, FallSpecial, and
LandingFallSpecial, plus ordinary Landing.
Imported hit and hurt geometry is anchored to Melee's fighter root
at the simulation floor-origin offset, rather than incorrectly treating the
simulation body center as the source origin.

Each generated sphere also retains the live `ftHit` collision state. State 2
creates a sphere and resets previous center `x58` to current center `x4C`;
state 3 continues the same executable hitbox ID and preserves the prior
transformed center. The importer rejects a state-2 moving center, a state-3
discontinuity, and inconsistent duplicate hitlag rows. Effect `groupId` is not
used as lifetime identity because Falcon dash attack changes effect group while
the executable sphere remains state 3.

A 2,568-row Dolphin 3.4.0 capture, SHA-256
`2df522e9bc93a09b61d15406f9281f4638f9c81796da349d033d90a112d51289`,
adds 33 Falcon Jab 1 collision decisions against light shield at neutral,
up-right, and down-right offsets. `tools/verify_ssbm_shield_collision.py`
hash-pins the capture and `lbcollision.c`, then applies the decomp's closest-
point capsule distance plus transformed shield and hit radii to live memory.
All decisions match. Last-hit/first-miss boundaries are 28.60/28.65,
29.65/29.70, and 29.60/29.65 Melee units respectively. Production uses the
same squared radius-sum predicate without allocation, square root, floating
point, or the former rectangle/ellipse broad-phase rejection.

Hurt poses for common non-attack actions outside the eleven qualified tracks
remain an active fidelity gap, not values to be filled by guessed frame data.
Looping common animations are not flattened from
their FigaTree endpoints: the Run capture exposes displayed frames `1, 2, 2`
while velocity settles, proving that its animation rate depends on live state.
Run therefore remains unimported until its exact phase/rate inputs are modeled.
For the qualified Dash collision route, Falcon Jab 1 hits an inward-dashing
Falcon from 31.0 Melee units with a source-float margin of +0.289213242 and
misses from 31.5 with -0.156797621. The former misses the old generic body
rectangle by 3.503404617 units, making the route a discriminator for the
imported pose rather than a damage-only coincidence. The capture and verifier
also hash-pin decomp `lbcollision.c` at
`fa47d275f86956edb3c3a228a7fcc160e6f467c2d4bfd5f86d71f1d55e13e1fb`
and `ftCo_Dash.c` at
`23fd2ad0af701c320fb24f6b5e7406971d7c31060b87916a20b242c076d10f7c`.
For CrouchStart frame 3, Jab 1 hits at 17.7 Melee units with a reconstructed
source-float margin of +0.131910442 and misses at 17.84 with -0.005252888. The
old generic rectangle still reports +0.596595764 for the miss, so the negative
route specifically detects the exact crouch pose. The verifier also hash-pins
`ftCo_Squat.c` at
`80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082`.

The source executable's moving-hit
path is now production-routed: previous and current hit centers form one 3D
capsule and intersect the current hurt or shield capsule using a portable
allocation-free Q16.16 closest-segment predicate. A 274-frame Slippi Dolphin
3.5.1 capture, SHA-256
`d8599ecc80efc567d579d9c3df9c10c70f89909dc38358ad29d602ca6ed3f4ea`,
hash-pins the same decomp revision and NTSC 1.02 disc. At 27.4 Melee units,
Falcon down tilt frame 12 deals 12% even though the current sphere misses by
0.451734762 units; the `x58`-to-`x4C` sweep overlaps by 0.692950483. The
28.3-unit control remains a miss with margins -1.178471136 and -0.182688971.
Production Q16.16 tests reproduce both decisions. Custom authored content may
opt out of reference geometry explicitly; default Falcon-counterpart content
opts in.

Falcon Punch timing is decoded from the raw `SpecialAirN` event stream rather
than transcribed: command-variable assignments launch and begin velocity
scaling on displayed frame 50, scale through frame 64, and restore ordinary air
physics on frame 65. The state machine follows decomp revision `9509dc0`
`ftCa_SpecialN_Enter`, `ftCa_SpecialAirN_Enter`, animation, physics, and
collision callbacks. It consumes the imported stick-angle, launch-speed, and
velocity-multiplier fields; the ground state consumes its per-frame animation
translation; and both states query the captured frame-specific pose and hit
geometry. Ground-to-air and air-to-ground changes retain the action frame.

Fresh direct-executable Dolphin captures qualify the two production routes.
The 200-frame grounded capture has SHA-256
`2c8bc604024cfad745e266239dcc4d3e1b1ff1c4a07afcc6eecb9938b5f155b1`;
the 241-frame aerial physics capture has SHA-256
`9cfc8c5632a8bce37a0f79c6999bff6f0742130df5f8f2473196338d8b14d6c5`.
The latter observes the source launch `(1.794, 0)` on frame 50, velocity
scaling through frame 64, ordinary gravity and air control on frame 65, and
`Fall` after frame 99. `tools/verify_m4_falcon_punch.sh` rebuilds the strict
native trace runner and compares both captures on demand after verifying the
GALE01 NTSC 1.02 disc identity. Position tolerance is limited to 640 Q16 units;
velocity tolerance is 32 Q16 units.

The same live verifier now regenerates a source-side stored projection before
reporting success. `tools/ssbm_falcon_punch_coverage.json` records the two raw
capture hashes, pinned/current decomp revisions, and three regression cases:
100 complete grounded samples, 100 complete aerial action-clock samples, and a
51-sample aerial physics tail beginning at displayed frame 50. The aerial clock
case serializes only action, timer, grounded, and facing because the live
comparator deliberately reanchors physics after frame 49; the separate tail
owns position and velocity from that boundary onward. The canonical source
projection hashes to
`defbb9746b3784c6e1aae2b7d176344fadcb9a1ba0c51ac4b8c097e1765a16f1`.
The allocation-free production trace hashes to
`eec11ed8d9050fe51196b9241e326c3e189be8a56dad82e64b5ee28a7c5b527e`.
The character-independent generator and C runner enforce both digests without
copying Falcon Punch constants into the oracle.

An independent recapture produced a different raw JSON hash because unused
single-precision memory samples vary below the retained fixed-point precision.
After conversion, every numeric table row was byte-identical to the pinned
capture (apart from the provenance hash comment), bounding that recapture
difference to discarded/Q16.16-level data rather than gameplay geometry.

Falcon Dive uses the imported ground/air start, catch, and throw subactions,
command timelines, special attributes, root translation, grab spheres, hurt
poses, and throw effect. A fresh 146-row memory-probed grounded capture has
SHA-256
`4518dbb5cd43158baeaa1ddad7d5ffd073b4dda46ecbe2aa55d8c7efa9eadfdb`.
The probe reads the live fighter and opponent ECB top, bottom, right, and left
points from `fighter+0x794`; the source Falling bottom is
`7.932853698730469` Melee units above the fighter origin. Grounded catch-to-
throw relocation is the capture-observed `(-10.7077474594, +2.545643227)`
Melee-unit offset and is applied only on the source grounded transition.
The 116 comparable frames pass strict action, facing, grounded, capture-link,
hitlag, and velocity checks plus the established 640-Q16 position envelope.
The route covers catch, holder/victim attachment, throw release and damage,
source root motion, falling, and floor landing; it does not claim unprobed ECB
evolution for every common state or arbitrary stage solid.

The pinned aerial catch capture has SHA-256
`59a4489ea6e955c9bb587bb5e49bc5d34ce4cce6ae42accd98a24ff97e271a6f`.
Its memory probe records exact internal damage, the victim's applied and
latched knockback channels, throw weight, knockback magnitude, and damage-state
timer. It proves that the 5% catch stales the shared Falcon Dive identity before
the 12% throw, producing 15.92% total damage through the imported 0.91 newest-
slot multiplier. `CaptureCaptain` computes the reaction duration but
`ftCo_800DE7C0` clears applied launch velocity: the victim begins ordinary
0.13-per-frame gravity immediately and exposes 26 post-transition hitstun
frames. The pinned artifact still supplies all 92 holder frames and 42 victim
capture/reaction frames. Its geometry-isolation route pins both fighters at
`y=500`; after the later correct reference-joint origin conversion, the legacy
native comparator's different natural-jump setup catches one frame late. That
lane is therefore currently an explicit validation-architecture gap rather
than an at-will green dynamic theorem. The later one-percent change in the
y=500 capture is off-screen magnifier damage and is excluded from move damage.

The Falcon Dive start collision callback is independently source-defined.
Pinned/current `ftCa_SpecialHi.c::doAirColl` calls
`ft_CheckGroundAndLedge(gobj, 0)` after its command-variable gate, while the
ordinary Fall/FallSpecial paths pass facing direction. Production retains that
argument as one shared ledge-probe policy, allows either endpoint only during
the gated Dive start, and applies the common catch transition's inward facing
from the selected ledge. Both independently pinned directions remain green at
63/63 comparable frames. The facing-away route keeps outward facing from Dive
frame 13 through frame 63, catches on frame 64, turns inward, and enters
`EdgeHang` on frame 71. Its raw SHA-256 is
`026faf91c3582aa5e41c5d95ba757904ec7ef7865a049994ce169f70a6157009`;
the source verifier checks the facing transition and the collision-memory
ledge predicate rather than treating the native discriminator as source proof.

Two pinned miss captures close the ground and aerial `FallSpecial` paths. The
195-row grounded capture has SHA-256
`97672ddf0e5013beaad8ff4c31f54c6bae93551ca3a38755cc3d185bcd5b83c4`;
the 206-row aerial capture has SHA-256
`9ecf456e6377f5b7d371ccb84c9f5bd7b3a1045724a7c223acb6cb9d4681fd21`.
Both memory-probe the live ECB and expose `FallSpecial`'s complete repeating
bottom sequence as 2.306158066, 2.111962318, 2.465172768, 2.718437672,
2.784078598, 2.765646696, 2.499094248, and 2.226554394 Melee units above the
fighter origin. The generated eight-entry Q16.16 array is consumed directly
by floor collision. The grounded transition also proves that
`LandingFallSpecial` retains the incoming -2.426290512 vertical velocity on
its first displayed row and clears it on the next row. The at-will verifier
strictly compares 103 grounded-miss and 165 aerial-miss frames in addition to
the catch routes.

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
| backward jump-motion threshold, `ftCommonData.x78` | 0.125 | 4096 of 32767; relative stick at or below -4096 selects JumpB/JumpAerialB |
| TurnRun animation / velocity-crossing hold | displayed frames 0 through 21 / hold displayed frame 9 until old-facing ground velocity is at most 0.01 | 22 animation ticks with a deterministic frame-9 hold and 68-Q16 converted threshold |
| post-TurnRun run lockout | 10 | 10 ticks |
| tap-jump threshold / window | 0.6625 / 4 | `tap_jump_axis_threshold=21709`; `tap_jump_input_window_ticks=4` |
| fast-fall threshold / window | 0.6625 / 4 | 21709 / 4 ticks |
| crouch entry / release threshold | 0.6875 / 0.625 | first accepted entry axis 22528 / exact held boundary 20479 |
| analog shield common dead zone / first accepted raw value | 0.30 / 0.30 | threshold 19661 of 65535; a digital click is 65535 |
| air-dodge X/Y dead zone | 0.25 / 0.25 | 8192 of 32767 |

## Imported common damage-response values

The standalone common-data generator validates the HSD archive root and all
relocations before copying the complete 0x818-byte, 518-word `ftCommonData`
object. Its owner `PlCo.dat` SHA-256 is
`63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c`.
Typed fields below are views over that complete pinned object rather than a
second authored constants table.

| Source field / offset | NTSC 1.02 raw | Simulation mapping |
|---|---:|---:|
| hitstun per knockback, `0x154` | 0.4 | 26,214 Q16 |
| launch speed per knockback, `0x100` | 0.03 | independently scaled X/Y: 205 / 349 Q16 |
| maximum DI angle, `0x1A8` | 18 degrees | 337,325,943 Q30 radians |
| ground knockback decay scale, `0x200` | 1.0 | 65,536 Q16; multiplied by Falcon's imported 0.08 friction each grounded damage tick |
| air knockback magnitude decay, `0x204` | 0.051 | 3,342 Q16 in Melee source units |
| SDI radial threshold / window, `0x4B0` / `0x4B4` | 0.7 / 4 | 22,937 of 32,767 / four ticks |
| SDI distance, `0x4B8` | 6.0 | independently scaled X/Y: 41,031 / 69,764 Q16 |
| ASDI distance, `0x4BC` | 3.0 | independently scaled X/Y: 20,516 / 34,882 Q16 |
| shield SDI multiplier, `0x4C0` | 0.66 | 43,254 Q16 |

## Imported common surface-response values

| Source field / offset | NTSC 1.02 raw | Simulation mapping |
|---|---:|---:|
| wall/ceiling reflection threshold, `0x1B0` | 1.0 | independently scaled X/Y: 6,839 / 11,627 Q16 |
| reflection invulnerability, `0x1B8` | 15 | 15 ticks |
| reflection multiplier, `0x1BC` | 0.8 | 52,429 Q16 |
| reflection re-collision lock, `0x1C0` | 3.0 | 3 ticks |
| passive-wall freeze, `0x760` | 5 | 5 ticks |
| passive-wall invulnerability, `0x764` | 14 | 14 ticks |

Falcon's generated submotion catalog supplies 26, 40, and 26 animation ticks
for `PassiveWall`, `PassiveWallJump`, and `PassiveCeil`. Adding the common
five-tick passive-wall freeze yields production durations 31/45 for the two
wall actions; ceiling remains 26. The ceiling body-state script changes at
displayed frame 11, which is the imported control-release and invulnerability
boundary. A five-case Hyrule capture pins these action, velocity, hitstun, and
invulnerability observations under digest
`5339134dd04cff9612e8c8a3e1d460f85018ae4c081ac7426fbad3cee3b785f5`.

DI and knockback decay operate after converting the project's anisotropically
scaled velocity channels back to Melee units. The runtime keeps ordinary self
velocity and damage knockback velocity separate, applies physics first,
subtracts the imported scalar from knockback magnitude, then integrates their
sum, matching `Fighter_procUpdate` ordering. The six-case checkpointed Dolphin
route and stored numeric oracle qualify this open-air boundary. A separate
64-row late-DashAttack route qualifies the flat-ground branch: Sakurai-angle
ground projection, distinct `xF0_ground_kb_vel`, 0.08-per-frame friction,
`DamageLw1` frames 1-11, and release only after both hitstun and animation
finish. Its 15 damage samples match action/frame, grounded/tumble state,
damage, timers, self velocity, projected `x8c`, and `xF0` within 0.001 source
units. Position is excluded because the route also activates the independently
qualified player-push system; slopes and broader collision response remain
unqualified.

The player-push values come from Falcon's `ftDataCaptain` `x2C4` vector in
`PlCa.dat` and common-data field `x450` in `PlCo.dat`. The independently
written implementation follows pinned decomp routines `ftCommon_8007DD7C` and
`ftCommon_8007E0E4`: active grounded fighters on the same connected support
receive the nudge only while the strict sum-of-radii overlap test succeeds.
Falcon's walk maximum uses the nearest Q16 encoding of `51/575`; truncating it
by one Q16 unit accumulates enough error to select the wrong executable push
boundary after a long held walk.

The registered `falcon-common-player-push` domain replaces the legacy
comparator-only regression with two checkpoint-isolated cases starting just
outside the strict 7.0-unit combined radius. It drives port one right and port
two left, records both fighters in frame-major/lane-major order, and observes
the source's fixed 0.3 nudge in both directions. Three fresh ExiAI boots
produce the same 48-row / 96-lane source trace SHA-256
`3c6ade86d516474c60b7559690b3b858f2b7a66b41982859e4f81df70a7c73f5`.
The production trace SHA-256 is
`079a34868db4fff30719d7a784d7bd102aab7a81acd189ad6293c65a9056bc7a`.
Action, facing, and grounded state compare strictly; horizontal velocity uses
the existing 32-Q16 conversion allowance, and relative position retains the
reviewed 2,692-Q16 envelope composed of the ordinary 640-Q16 conversion bound
plus one mapped 0.3-unit nudge. The final live capture takes 0.090 seconds warm
and 3.011 seconds including process launch and menu setup.

The character-independent numeric stored runner now accepts one or two
manifest-declared observation lanes and hashes only the manifest-selected
fields in canonical frame/lane order. Existing one-lane domains default to
their original field mask and preserve all five prior production digests.
Repeated input phases expand offline into immutable C arrays with compile-time
lane counts; production adds no allocation, parsing, float work, or character
switch for this generalization.

The ordinary ledge route distinguishes decoded animation deltas from callbacks
that place from an absolute animation root. Pinned `ftcliffcommon.c` positions
`CliffCatch` and `CliffWait` from `x68C_transNPos` plus the selected endpoint on
every physics update. The live-qualified frame-one source roots are
`(-5.906890869140625, -20.114771366119385)` for catch and
`(-2.4527130126953125, -23.096231937408447)` for wait. Offline conversion emits
catch `(-40395, 233882)` and wait `(-16773, 268548)` Q16 anchors; subsequent
catch frames reuse the existing decoded `TransN` delta pool. Runtime converts
the source root to its body-center convention once and retains a distinct
allocation-free `LEDGE_CATCH` action for all seven displayed frames.

The expanded Hyrule response theorem compares 110 production samples across
the two existing cases, including `CliffCatch` frames 1-7 and the first
`CliffWait` frame. Its source semantic SHA-256 is
`0b23132b7a217ff173397faf8ac9e59169092c99095b4b4e3fbd885526b7a3f3` and its
production trace SHA-256 is
`9c562426f42c4b01b08a7bbea9c667f56661a2787d107870a14208f326ccd94e`.
Snapshot validation and exclusive ledge ownership accept the distinct catch
state; invulnerability continues to elapse from acquisition rather than being
restarted at wait.

The later four-case revision imports `ftCommonData.x480` directly from the
owner-supplied `PlCo.dat`: source float `0.6600000262260437`, emitted once as
Q15 `21626`. `ftCliffCommon_80081298` rejects acquisition when source left-stick
Y is at or below its negative. The capture preserves both requested and live
quantized controller values: requested `-21400` is observed as `-0.65` and
enters `CliffCatch`; requested `-21626` is observed as `-0.6625000238` and
remains in `Fall`.

Two fresh 290-row checkpoint captures complete in 2.716 and 2.493 seconds warm
and reproduce semantic source SHA-256
`9df8c72fca21359281d7d89391a9c363e08e6cf5c06db8873868e10521f27b49`.
The four stored cases contain 220 production samples under SHA-256
`73f3dae4bf726aedd1e2ab37911818faa9b3fff4d1a19ed2a92a41148f142f5d`.
The comparison keeps state, clocks, support, catch/wait, and velocity strict
within the existing Q16 conversion allowance. Position uses a zero base plus
at most that same allowed velocity error per integrated tick, making fixed-
point accumulation explicit instead of widening a flat tolerance.

The same trace closes two transition-order gaps. DownBound consults its
imported per-frame contact mask: the first contactless frame consumes the
current root at the line endpoint, while later contactless frames retain the
preceding floor root. `CliffCatch` entry clears self, ground, and attack
knockback motion as observed. Both rules are shared common-state behavior and
add no Falcon-specific runtime switch.

## Imported Pass collision pose

Pinned decomp `ftCo_8009A228` enters common motion state `Pass` (Falcon raw
submotion 209), applies `ftCommon_8007D5D4`, clamps air drift, writes
`ftCommonData.x46C`, and calls `mpUpdateFloorSkip`. Its animation callback does
not enter `Fall` until the complete 30-frame motion finishes. The separate
common-data `x470` value controls only the departed-platform collision skip;
it is not the animation duration.

The focused Battlefield capture entered Pass through ordinary input, waited
until the state was active, and then relocated Falcon high enough that natural
landing could not truncate the ECB. It records exactly source frames 0 through
29. Raw JSON SHA-256 is
`0dc57f8ffb85549be76b3b5a0017690b0df16905456169eaceaa2e7975eedc0c`.
The canonical big-endian stream of `(u32 action frame, f32 ECB bottom Y)` has
SHA-256
`90060e614f359189c32b25d76b780b3fa92861dfdcfae0fd357dcc07ec10e6f8`.
`tools/import_ssbm_falcon_frame_data.py` pins both identities and converts all
30 samples once to `platform_drop_bottom_y_from_origin_q16`; runtime indexing
is allocation-free and uses the existing canonical source-submotion clock.
The resulting complete Falcon source SHA-256 was
`147520a32bd20dc99dc2f326f52f8fcfc56c57058cf99669c762eea0c776720a`
before the later complete JumpF ECB schedule extended the same identity.

## Imported JumpF collision pose

The natural Battlefield vertical route exposed JumpF displayed frames 1..31,
but landing replaced the action before its final four animation-driven ECB
poses could be observed. A focused headless/null/unlimited ExiAI route entered
JumpF through ordinary input and relocated Falcon only after the action was
active. It records all 35 displayed frames without changing the source action.

Raw JSON SHA-256 is
`28c4e902d8860f6d02ec779004c67c7ab94f87c7f3970699cfd9a44a8844cf1d`.
The canonical big-endian stream of `(u32 displayed frame, f32 ECB bottom Y)`
has SHA-256
`6db927d319942e07d90ba6dd30aad39ad40bb42ab3cc09d498ea2587bfe233bb`.
The generator pins both identities and emits the complete immutable 35-sample
Q16.16 table. Runtime selects it through retained source submotion JumpF rather
than the coalesced public `AIRBORNE` action. Exact imported poses use the normal
previous-to-current platform sweep; only approximate poses retain the bounded
one-update compatibility path.

The resulting complete Falcon source SHA-256 is
`46c97fcbe303628fb1bf0ce3415431c01c16a0e73961ce5a1e78dd5dd1f1bfa9`.

## Complete ordinary airborne collision poses

The focused headless/null/unlimited capture now supplies all four directional
jump tracks and both ordinary looping successors: JumpF 35, JumpB 50,
JumpAerialF 50, JumpAerialB 35, Fall 8, and FallAerial 8 displayed poses. The
494-row raw captures are byte-identical at SHA-256
`4e6768e0862307eb32a14532fae8e2991e2900ea932b7af45850803c2ec8673f`.
The six-track profile and canonical `(action, displayed frame, ECB)` stream are
`407a62269b2aa65002bb4a78152f12a49b56d36d8b68a684c6d55a11ce69a1ba`
and `21a2d02fbb3abfcd9c29bb170c4c378fc8972fe191098fb5587140e965dac25a`.
The generator packs all 186 bottom samples into immutable Q16.16 data and the
allocation-free source-submotion accessor selects the exact track in O(1).
The resulting complete Falcon source SHA-256 is
`a71076ea7bb97215e95afdf2be5b395791ab34cb28ca3c60fe478d026e48d51c`;
the generated include SHA-256 is
`9587384e409f8275c03459d29f947e9f4a01c99c0a359d867bb3d354ffb67946`.

## Complete ordinary airborne hurt poses

The same 494-row physical route was captured twice with the
collision-authoritative hurtbox probe, headless/null rendering, unlimited
emulation speed, and fast-forward disabled so display-side bones remain live.
The raw captures have distinct SHA-256 identities
`cf458d593451c210b69fe45305c7affa992bf179d14aa2e3ce0b00e81d150a26`
and
`7f8aee28a613ca5b1ec5c1ea552b140ec515adbd28a4af071931c99d49ecfcab`,
but canonicalize to the same 186-pose / 2,046-capsule semantic SHA-256
`71c9e643816604f9d2e90cfc226b907e7ce7cb48edc4fa2fea51d6797013ee7f`.
The six complete tracks are JumpF 35, JumpB 50, JumpAerialF 50,
JumpAerialB 35, Fall 8, and FallAerial 8.

`tools/generate_ssbm_hurt_pose_include.py` consumes the profile and its small
binding manifest and emits one deduplicated immutable table. Runtime selects
it by retained source submotion in O(1). The stored source and production
digests are respectively
`de89004396090825835ef3e8606d852fc3d1f41414f4c61c4fe139ed45079b4f`
and
`13763a74d044b686b5ae065e7120ac984d823cb8e31f7bf15597c16268277a72`.
Falcon Dive's existing hit/miss theorem consumes JumpF frame 20 from this
complete table; the prior 12-frame Dive-only JumpF table was deleted.

## Imported DamageFlyN complete collision pose

Three independent owner captures in
`build/oracle/falcon-slope-ledge-response-qualified.json` and its two fresh
repetitions expose the same 24 displayed DamageFlyN ECB frames. Canonical
Q16.16 `(frame, top, bottom, side X, side Y)` SHA-256 is
`9efade94dbd61446decfabeedce910e4a2823bfc65299b7ecb4cb31fb368eee1`;
the previously pinned raw bottom-only stream remains
`d011c9bb79f93840d1d97bf241b754cedf5669c2578c9f1f7f85b45a3f6bd84`.
`tools/import_ssbm_falcon_frame_data.py` owns the complete arrays and converts
source X/Y distances once into immutable Q16.16 top, bottom, side-X, and side-Y
tables. Runtime selects the frame through the shared DamageFly action clock and
performs no allocation or float work.

The resulting complete Falcon source SHA-256 is
`0adc405c5affe87ae3bcc84e7665b53869231e0f4ffa6f4043586bd953782df3`.

## Battlefield sloped surface-response oracle

The two-case checkpoint pack relocates an already-launched DamageFlyN Falcon
only through declared precontact waypoints. Native collision then selects
Battlefield ceiling line 10 with normal `(0.3781174421, -0.9257575870)` or
right-wall line 15 with normal `(0.5692099929, -0.8221922517)`. It records the
impact and twelve natural response updates per case. Two fresh Dolphin boots
produce semantic source SHA-256
`8a0c463ffae10b1567815013c85c500bcb25869727874086c96d0e9c522a2f68`;
the reviewed production trace SHA-256 is
`107ea657a7bad069ea8ee02cb98306dd116b78838c8e6899a4adf9ff6fcf0982`.
The comparator binds the source stage catalog, selected line/normal, action and
display clocks, hitstun/tumble/invulnerability, self and damage velocity, and
relative position with only the documented Q16.16 allowances.

## Falcon reflected-action ECB and floor re-contact

The same native Battlefield bounce entries are relocated after the response
begins, with self and knockback velocity reset to zero at source position
`(0, 180)`. The 304-row capture has SHA-256
`f1989a139185635d41d5cc2a51b0f88d41c1a26cf24c57fa82614feed6fda1c2`.
`tools/extract_ssbm_ecb_pose_tracks.py` canonicalizes facing-right and rejects
non-contiguous or conflicting repeated displayed frames. Its compact profile
contains all nine observable `BOUNCE_CEILING` poses and all 51
`BOUNCE_WALL` poses under profile/semantic Q16.16 identities
`d6ccb5701f0bada0d7de1874004281e8ca46fcc0070db94e529d84d3fc637608`
and `9d162fe7917f0c23894ad1fe54a1a665d5c8e446d5ca439180811d706b2431a5`.

The Falcon generator verifies those identities and emits one immutable full-
pose representation consumed by the shared collision adapter. The complete
Falcon source identity is now
`af458556b4ba5bf0ec9cb86d4bb0a7ac3643a015f77a7de62617e71d18b49555`;
the generated include regenerates byte-identically at SHA-256
`972e79093c9bdfe20101b0bd182ab1a759fb8cb40e416d9e15f7f528f240a80a`.

The live comparison follows ceiling bounce through sample 57 and wall bounce
through sample 54, including the first native Battlefield top-platform
`TECH_MISS_DOWN` row. It compares action, real clock, grounded state, hitstun,
invulnerability, facing, relative root position, and independent self/damage
velocity channels. Source and production semantic trace identities are
`4e9a0ad3222bd0d6b6d7ab7def0177cf4b5c361bded3826abfe2e91f9210dd5a`
and `222a5504d62bc5500e57a88a0adad108b931ea73d2b70cdf46faccde3f36d2db`.
Later DownBound pose/contact evolution remains owned by its separate domain.

## Falcon quick/slow ledge hurt poses

Two independent compact no-fast-forward captures reproduce the same 450-row
source artifact byte-for-byte at SHA-256
`3055455eb02949e15c240f563a49648578b6c5affa4dc5dd7ca62f2c7b19c1e3`.
The canonical profile contains 434 displayed poses and 4,774 eleven-capsule
rows for quick/slow climb, roll, attack, and both ledge-jump phases. Its profile
and semantic SHA-256 values are
`2630b5be93c7f55b869ac1da64aaa4e4716b35e601ec6e8a47fc5d8e4ef91ff7`
and `9125200e3e162822131fd8805ae1551371c4ebf0abc2256bba9a167cc181103a`.
Fast-forward is intentionally disabled because ExiAI omits display-bone work;
the capture reads collision-authoritative hurt capsules from one contiguous
fighter snapshot instead.

The character-independent stored-oracle generator verifies that profile and
semantic identity, derives every frame span, and requires only a track-ID-to-
source-submotion binding. The production accessor therefore distinguishes ten
Melee submotions even though they coalesce into four public actions. Combined
with the original 255 common poses, the live source stream contains 689 poses
at SHA-256
`2aadf4b37b26796bdbc08fe026b234542f2c61914a4488e35e0dccd72a72e151`;
the independently serialized production accessor matches pinned SHA-256
`d691705692841bfabb8a2407ab31037bf398b097fc461574ecd07954e16a4331`.
The stored digest proves regression identity only. A separate two-case live
collision theorem completes the end-to-end geometry qualification. It schedules
ordered semantic edges at `CliffWait` frame 2 and quick-climb frame 25, then
observes exact Falcon Jab 1 frame 4 against imported quick-climb frame 29. The
attacker's three active spheres match the independently captured port-1 Falcon
signature exactly. At source X 198.75 the target takes two damage and enters
`DamageN2`; at 198.0 it remains in `CliffClimbQuick` at unchanged damage.
Reconstructed actual-capsule margins are `+0.573037244` and `-0.098777672`,
while the old generic rectangle still misses the positive route at
`-1.895534515`. Two fresh two-shard captures reproduce the same 143-row file
byte-for-byte at SHA-256
`f31de47e694e46bf2269945747c97238ce443ddf88cbadc0a8e4214026f2785d`;
the canonical outcome/pose/attacker semantic SHA-256 is
`fbe0cf877402bf82aba10d8ae3dceecb4e431caa87d9b75b5844bfb7b132af2d`.
The route is a supplemental projection of the existing ledge setup, so it does
not add frames to the default 19-case numeric pack.

## Falcon Turn and TurnRun hurt poses

The focused Final Destination checkpoint pack uses the pinned ExiAI 0.2.0
launcher, libmelee 0.47.2, GALE01 NTSC-U revision 2, and decomp revision
`9509dc04406fb2028bfab01243841ba4787c0fb7`. Its accelerated and
no-fast-forward raw captures have SHA-256
`e0240567b226cdd1802a3b7ad14384cf5096df9c8b323b383020c0a1844cf901`
and `18abfc0b39cc15614dcda03e243abb8298adcd4738869ea1738cf550cbed6be5`.
The warm capture portions take 0.227533 and 0.565447 seconds respectively.

Both captures reproduce Turn frames 1-11 and TurnRun frames 0-21 under the
same 33-pose / 363-capsule semantic SHA-256
`1cc3543b1363ecb5c7427c36f4d8d8a2826f9fb7c5281877f54108e1ffe281a2`.
The checked canonical profile SHA-256 is
`6addc920dda39acf06df2f8bcecee7ee23a3200c128b6545d0232c58c5704ea6`.
TurnRun freezes displayed frame 9 for seven observations; the final duplicate
has gameplay facing reversed while its display bones still retain the prior
facing. The extractor classifies exactly one such row and rejects any other
conflicting duplicate.

Production retains source submotions 10 and 11 and selects one immutable
generated table in O(1). The separate stored domain hashes all 33 production
poses under SHA-256
`0750e32d78d2b51b49f4e917dde6088a266e6940d92351250c8a167422563d07`.
The stored digest is regression evidence, not new source evidence. The
transient pending display-facing collision phase is derived from the unique
production tuple `TurnRun`, action tick 10, and gameplay facing equal to the
stored target direction. World hurt collision and inspection retain the prior
display facing for that update without adding rollback state. Three generic
stored pose-facing cases bind pre-flip frame 9, pending-display frame 9, and
resumed frame 10 to the live-qualified phase; the domain now owns seven cases.

## Falcon CrouchWait and Appeal hurt poses

The grounded-loop checkpoint pack keeps one headless/null/unlimited ExiAI
process and resets to seven independent checkpoints. Its current 423-row
control and repeat captures have SHA-256
`4894c8d3917568e43d3499b71cb3b3ebc361575670aeaad63c8a99bcba106b4e`
and
`2c0dda770bd58774e0681b3736ea3f7c7de61f15a116849f9381b0460c4af529`;
their qualified CrouchWait/Appeal observations reproduce semantic SHA-256
`3c72296c3c1558d7df32228892f5b1adec4b4370e72e4d415fdc981cd2aa3ed3`.
The qualified capture portions complete in 4.820601 and 4.932569 seconds.

The canonical profile contains all 158 `SquatWait`, 60 `AppealR`, and 60
`AppealL` displayed poses: 278 poses and 3,058 eleven-capsule rows under profile
SHA-256
`67d3f20d79b808c5d193892fd529c7e270fa1f175a8a2f92ee403c8a3e8d0ac7`.
Its source pose serialization is pinned at SHA-256
`f3c9ca8bfa7fc3d85acf2a0b74eeb6ab2452159cd7c8c7dbb3f66bd870cf84e6`;
the independent production accessor serializes to SHA-256
`1a83c02e310c097ad609dd2e18787bec2b6589fc76f5daa039578b8e04f6a81f`.
Six physical hit/miss controls cover the three retained submotion identities.

The same raw pack records the velocity-driven WalkSlow/Middle/Fast and Run
animation clocks. `ftWalkCommon_800DFDDC` derives animation rate from ground
velocity and `ftWalkCommon_800DFEC8` changes gait while remapping animation
phase. Production now retains source submotion, fractional animation cursor,
and rate in canonical rollback state; three stored cases protect 111 clock
samples through the character-independent numeric runner.

Walk and Run hurt geometry cannot be represented by a static one-pose-per-tick
table. `tools/generate_ssbm_dynamic_hurt_pose_include.py` therefore reads the
owner-extracted `PlCa.dat`, `PlCaAJ.dat`, `PlCo.dat`, and active gray-costume
`PlCaGy.dat`, validates their pinned SHA-256 values, maps Falcon's 63 model
joints through the complete 63-entry runtime part layout, and closes the 11
hurt capsules plus six ECB source selectors over one 25-joint ancestor catalog.
At the first ground-loop/Raptor checkpoint it emitted 580 FObj tracks / 5,346
keys for ten motions: the six compact Wait/Walk/Dash/Run blend motions plus all
four Raptor Boost ground/air start/hit motions. Only the original six motions
participate in compact
rollback blend-channel generation, so direct action evaluation adds no
persistent state. The gray model SHA-256 is
`dcc34bbb428f978858e95b18e29d4a476b4582d59cdc5daca3814dcaf2eef872`;
that checkpoint's generated semantic data SHA-256 was
`c8a4f062eb8a35d12d3ea2fa0787a76211bcee3d7379a6bd7c51b71d2cdd07ee`.
The shared runtime evaluator implements the HSD FObj interpolation and Euler
SRT hierarchy in deterministic Q16.16. In particular, it follows
`HSD_FObjReqAnim` by adding each FObj `startframe` to the requested animation
frame; subtracting it produces a large, motion-specific WalkMiddle error.

The two 423-row live headless/null/unlimited captures named above also record
motion ID, animation ID, blend state, complete ECB, costume ID, and the active
costume-root pointer. Player 1 is costume ID 1, which the decomp maps to
`PlCaGy.dat`; this resolves the former leaf-joint residual caused by evaluating
the neutral model. After the source six-frame default blend finishes, the
independent Python source evaluator agrees in each for all 51 WalkSlow, 31
WalkMiddle, 29 WalkFast, and 20 Run observations: 131 poses / 1,441 capsules,
with maximum capsule-coordinate error 2 Q16 units and maximum ECB-coordinate
error 1 Q16 unit. Eight observations, including pre- and post-loop WalkFast
samples, are stored as a separate C oracle with a 64-Q16 deterministic-runtime
bound and now protect both the capsule and ECB consumers. The WalkFast trace
first enters Walk with a sub-dash tilt and then raises the stick above Falcon's
fast-gait velocity boundary, avoiding an authored state override.

The same evaluator owns Raptor Boost ECB generation. It subtracts the
TransN/current-position reference from the six source JObj origins before
applying `mpColl_LoadECB_JObj`'s width, height, side-clamp, grounded-bottom,
and airborne-bottom rules. Air start retains the zero actual bottom for its
first four displayed frames while side Y uses the unlocked desired bottom,
matching `ftCommon_8007D60C` and `Fighter_procMap` callback order. A hit-state
frame zero still displays the final start-motion pose that entered the state;
hitlag repeats that pose rather than advancing the new animation.

Five manifest-bound captures with raw SHA-256 values
`81cafb4d75e75c1f876b6a903a770a3e20376d0399d9374cab19d7feea413602`,
`1780ae376ae2be8f26187db96b81df96b8a750ff9d1ef8934631ab02da6e4ae1`,
`20f39477c01894751724b5e0097a7c2646baa4e12bfbbfe7bcdb09edf10bf864`,
`9efacb94277b8cb870f8c69008e5dd248d4d31cec17e9713323abde94a577028`,
and
`86e0abff2d1de0483e25ef8db045da323a35331bf95fb7089b00283233b4fc8e`
cover 423 observations / 411 complete action frames across all four motions.
The independent Python source evaluator differs from live Dolphin by at most
one Q16 unit. Four selected C poses allow 32 Q16 units for deterministic
fixed-point matrix evaluation, and the complete 657-frame production Raptor
suite passes. The former authored 45-value aerial-hit ECB-bottom series is
removed.

The same parent-closed profile now owns Falcon Dive ground/air start, catch,
throw, and common FallSpecial neutral/forward/back geometry. Nine retained
captures cover eight motions, 733 rows, and 715 unique frames; the independent
source evaluator differs from live Dolphin by at most two Q16 units. The HSD
profile contained 17 motions, 1,204 tracks, and 10,106 keys at this checkpoint.
The later shield-break import extends the same profile to 21 motions, 1,514
tracks, and 12,609 keys under SHA-256
`a5edfc9fabbd3ed9c351fbe68b3a91c16e4954243ca14e3d7273baadc44fc2b8`.

FallSpecial direction selection follows `ftCo_Fall_Anim_Inner` using the
imported common threshold `0.1`, Falcon's imported air-speed maximum, and the
common blend rate `0.5`. A target switch first installs and blends the target
skeleton, then `ftCo_800CC988` advances and blends it again; a stable target
performs only the latter pass. Production retains the one-bit switch result
through rollback because that callback ordering changes the current ECB.
Bottom-lock value and lifetime are likewise explicit state: the desired side-Y
continues to come from the unlocked pose while only the actual bottom is
replaced.

A fresh headless/null/unlimited natural Falcon Dive miss capture has SHA-256
`55ddf2eed8a8cd52d788075b62bca1e6d7be3a1a26ce89bec6e02b85475ea5b4`.
The same-input production route passes all 165 compared frames under the
existing 640-Q16 accumulated-position envelope, with actions and velocity
channels strict. The authored Falcon Dive and FallSpecial ECB arrays are now
removed. Their checkpoint include SHA-256 was
`754a72159e5463752e382dd6a2a8e35657bab601b84c228ade5c540d30272a74`;
after Wait2/Wait3 and the 20 qualified ordinary-action motions, the current
profile contains 45 motions, 2,976 tracks, and 32,285 keys under decoded-data
SHA-256
`17da37dd9cdb080559407a7b8268bc52a590063bf9c84ef9b34e2de324e78dee`.
The generated include SHA-256 is
`798cc73467981d07c23724f011a8c9a0fd626d96d765f762a3c56af9f6c12e8e`
and the Falcon complete-source digest is
`280abf47cbc18b5802e1c98048c7830808541766dd6c646d31c34eb0b0d3eb64`.

## Falcon shield-break orientation branch

The pinned common-state source selects `ShieldBreakDownU` only when
`ftCo_80097570` observes `HipN->mtx[1][1] > 0` at the terminal
`ShieldBreakFly` pose; otherwise it selects `ShieldBreakDownD`. Falcon uses the
ordinary matrix predicate (`x2226_b0 == 0`). The shared DAT importer evaluates
submotion 286 at its last FigaTree frame, maps runtime part 4 through the full
`ftPartsTable`, and obtains `-3921` Q16 for that matrix component. Production
therefore enters source submotion 289 (`ShieldBreakDownD`) and follows it with
submotion 291 (`ShieldBreakStandD`) instead of the previous hardcoded DownU
path.

Two independent 500-row headless/null/unlimited natural shield-depletion
captures have SHA-256
`e6f7efe3ed1776ff0317e597102772b3cc8f80cea4ee87be20d6b711707b38a4`
and
`120a497bbea0c8b1157d1124cc7cce807af28e2d4bf82d29300955c1902e7e3f`.
Both reproduce 42 `ShieldBreakFly`, 26 `ShieldBreakDownD`, 30
`ShieldBreakStandD`, and 127 `ShieldBreakTeeter` observations in that exact
order. The manifest-bound source verifier recomputes the matrix predicate from
the four owner-extracted DATs and validates both captures; the runtime stores
no new branch state and performs only a constant lookup at landing.

The original surface-probed controls have normalized SHA-256
`1109c92ec4c57bff5658d25c432383ccc4c63e2caed73a1575ae3ef80c7c802d`
and
`3daad9d8a51ad0c6eb106fd14c25b6814a543dd359ca07f9a1e6b84e47ac4e74`.
Their route semantic SHA-256 is
`67d6f164a7d94e7171f1b8d4ace195676fac180faf1c9f044351ebeaab3f4ae5`;
the persistent worker's unrelated looping Wait phase is normalized out. Fresh
memory-pose controls have raw SHA-256
`ae02dca6e63eda47e780ee96cae26c4c8a565f4e2d534979791f827d737f5645`
and `ddbc82fcd401fdbea41a202964f3dff678eff32a23199567607e26cb9ba5f40b`.
They independently qualify all 98 linear-motion observations plus 127
Furafura observations: the generic HSD evaluator reproduces 450 hurt poses /
4,950 capsules within one Q16 unit.

Raw HSD selector-joint matrices do not exactly reproduce the live ECB during
these actions because Melee's runtime callbacks and `ftCommon_8007D5D4` entry
lock participate in collision geometry. Hurt and ECB qualification are
therefore deliberately separate. The compact live
Fly/DownD/StandD/Furafura ECB profile
has file / semantic SHA-256
`2b4354f075594264ddb1686c9123c78459658a8dec145d78445c1b115585bc7c` /
`11b28d22f68f7bb87c99dbc5f949f5456d1a69ab7bfa78360927ecb334064eeb`.
Production consumes the complete four-point ECB at collision time, retains the
source one-row landing vertical velocity, and reproduces the source shield
health lifecycle. The generic stored route pins production SHA-256
`d2080f96a9477f5615f777d657e70cab9181015f7058b4257dba8c9c52334cc4`.

Furafura enters with source animation frame zero and loops independently from
its grab/daze countdown. The two 127-row traces both show frames 0 through 99,
then 0 through 26. Production advances the existing rollback source-animation
cursor by one per non-hitlag update and leaves mash reductions confined to the
countdown, reproducing the full captured clock without a new state field.

## Falcon DownBound collision poses

The existing prone-response checkpoint route was rerun with EXI batching
disabled only for geometry sampling, so every response frame retains its live
`fighter+0x794` collision box. The focused `timeout` and `up_timeout` cases
produce 600 rows in one persistent headless/null/unlimited Dolphin process and
cover displayed frames 1 through 26 of both `DownBoundD` (`TECH_MISS_DOWN`)
and `DownBoundU` (`TECH_MISS_UP`). A repeat process regenerated the same
canonical semantic SHA-256
`3c4a4ce4586b11617aa99a08bac8709ea6d7aa8a179b5494c6f3f7fe4785c7df`.

`tools/data/ssbm_falcon_down_bound_ecb.json` stores all 52 top, bottom, left,
and right points in source float and Q16.16 form. Its profile SHA-256 is
`a51838128df5c2df0df68a1df507b05ef868217d76b1c5fe57471f094d084f28`;
the generating capture SHA-256 is
`c9bca1cb43fad6c0b6fb73c123faaeef0725b9737b84de4abf38d917386a2cfb`.
The frame-data importer rejects drift in all three identities. Production
maps source DownBoundD to canonical stomach and DownBoundU to canonical back,
then selects the table by prone orientation and action tick through the
same allocation-free ECB resolver used by shield break and damage bounces.
The earlier compact floor-contact masks remain authoritative for the unusual
frames 1-4 / 23-26 grounding schedule, while the complete pose now drives
wall and ceiling queries during the airborne middle of DownBound.

## Falcon DownWait and getup collision poses

The prone-response manifest has two supplemental projection-only cases that
wait for displayed `DownWaitU` frame 1 before selecting forward/backward roll.
They do not lengthen the normal 14-case numeric pack. This is required by
`ftCo_Down_CheckInput`: a roll selected directly by terminal `DownBound_Anim`
uses the D motion even for an Up/Back prone fighter, while selection after
entering `DownWaitU` uses the distinct U motion.

The focused geometry route projects eight cases across four headless/null/
unlimited Dolphin workers with per-frame EXI batching disabled. It records
1,150 rows and covers both 70-frame DownWait loops, both 30-frame neutral
getups, both 49-frame getup attacks, and all four 35-frame roll motions. The
authoritative and independent repeat captures have SHA-256
`22d96deaa0e2c32ce9edba670285ce6268b442edf05ae97c01abe85a93c8059c`
and
`891a17f858b9d6ad15d4dbf892546966fe67e225e6d565a7ff98a571522459f0`;
both regenerate 438 top/bottom/left/right poses under semantic SHA-256
`f519d632a88bcb582cb68865dd9a58d27e862fe619fc05d76ff3252ad5204f19`.

`tools/data/ssbm_falcon_getup_ecb.json` has profile SHA-256
`9c3dfc58d1f34acf1ff264fc443d70e0ba283f5bd09da71bb7134fe8e8e9a1e0`.
The importer pins the profile, authoritative capture, and semantic identities.
For DownWait, the live sequence enters at displayed frame 1 and wraps
`... 68, 69, 0`; the committed profile stores canonical source frames 0-69,
validates every repeated live cycle, and production selects
`(action_tick + 1) % 70`. All prone geometry is held in orientation/direction-
indexed arrays and selected by one allocation-free adapter. No additional
rollback state was introduced: the existing semantic prone orientation and
resolved roll-motion orientation are the exact callback inputs.

## Falcon CrouchWait collision poses

The grounded-loop checkpoint pack now exposes the live fighter animation
frame/rate, motion and animation IDs, six-frame blend state, and the complete
`fighter+0x794` ECB. Its authoritative five-case capture has SHA-256
`cb07f5c3bff1f55e7f223e3863822a6d023bb6adf9ad13b69918111fcb341ba6`.
An independent one-case / 160-row process has SHA-256
`6d66a2e7e88f6264fb4932c7395d0a1344f8548da5ec2b68100c244c7e749c82`.
The reusable cyclic extractor accepts a capture starting at any point in the
loop, validates adjacent modulo order and repeated-frame determinism, and
canonicalizes all source frames `0..157`. Both captures reproduce semantic
SHA-256
`ba47ef2736a5677d1909262a20f32991b7c2515407fae26626d5869b95edd265`.

`tools/data/ssbm_falcon_ground_loop_ecb.json` stores the complete source float
and converted Q16.16 top, bottom, left, and right points. Its profile SHA-256 is
`a1d4a9eb47dd16630812fbdb59eaaf377f3580e313436523d0ea81088cafceb3`.
The importer pins the profile, authoritative capture, and semantic identities
and emits a 158-entry immutable array. Production maps `SquatWait` action tick
one to source animation frame zero, advances at one frame per update, wraps at
158, and performs a direct array lookup from the canonical Q16 cursor. The
existing cursor is now retained for all source-clock-owning actions rather
than only velocity-driven Walk/Run, so save/load, inspection, and every ECB
collision query observe the same frame without adding rollback state.

WalkSlow, WalkMiddle, WalkFast, and Run now consume the generic source ECB
evaluator. The generated six-joint selector arrays share the same parent-closed
catalog and matrix pass as hurt geometry. The runtime reproduces grounded
`mpColl_LoadECB_JObj`: min/max reduction, its strict less-than-10 symmetry
predicate, the plus/minus-2 side clamps, grounded zero bottom, and midpoint
side height. Two independent 131-pose source comparisons have maximum 1-Q16
ECB-coordinate error, and the eight stored fractional observations exercise
the production C path. The source six-frame local-SRT recurrence and its
production entry ordering are qualified. Five checkpoint-equivalent ticks
cover Wait-to-Walk, ordinary and nested gait changes, and Dash-to-Run.
Production advances the old animation and any active old blend before IASA
replacement, matching the executable within 4 Q15 rotation and 4 Q16
translation units.

## Repository controls

- Only the converted constants and independently written C state machine ship.
- The temporary extraction is not a build input and is ignored by source
  control.
- Every later imported table must extend this document with source, revision,
  raw value, conversion, and destination field.
- A formal IP/originality review is required before public release.

## Falcon ordinary guard hurt and ECB poses

Pinned `ftCo_Guard.c` establishes three different pose clocks. `GuardOn`
enters with `Ft_MF_SkipAnim`; `ftCo_80091E78` manually interpolates Wait toward
the shield skeleton for eight updates while the public animation frame remains
`-1`. `Guard` freezes the terminal manual pose, also at public frame `-1`.
`GuardOff` is ordinary submotion 39 and exposes displayed frames 0-15. Raw HSD
sampling matches GuardOff within one Q16 unit but differs from live GuardOn by
9,594-31,608 Q16, so the executable manual poses are retained explicitly.

Two independent 40-row captures have raw SHA-256
`e9141d1ce253bee82233d9545cf20145d594d60510cee5ea77b19ca5e12390b9`
and `88943aab9a5d70c79570ab108f9a9183fd69d3a0bde8c9d2ee38a641a089b1ef`.
Both regenerate 25 hurt poses / 275 capsules under semantic SHA-256
`4db8c524835e969b5b34fda81e53b59d6af99aa68d13e7203086c6441a41abde`
and 25 complete four-point ECB poses under semantic SHA-256
`a1bd5b9937cb342a053415ecc674b36dc5a01fb575ed688b32f8e097e1b209c1`.
The committed hurt and ECB profile SHA-256 values are respectively
`e1d76de8fac684d0976fa464d5906a145aeb340a99c0f88490da3574231c763b`
and `4ac108b18b77438b84760dd0dbea1ac830e8b5f323429aaeb01ecd4b66e48165`.

The complete Falcon source digest is now
`1e26a7fcb73c506e7dd446119896f6df90bdea0bb244c178066b4f19f5b72946`.
Production selects the immutable profiles from action plus retained source
submotion in constant time. GuardSetOff/shield stun uses a callback-derived
dynamic animation rate and is explicitly outside this profile rather than
being approximated with Guard. It is qualified by the shared HSD source below.

## Falcon GuardSetOff dynamic hurt and ECB poses

Pinned `ftCo_80092F2C` enters ordinary submotion 40 at frame zero with rate
`(0.1 + lbGetJObjEndFrame()) / f`, where `f` is the unrounded shield-stun
duration. The integer stun countdown is therefore not an animation clock.
During hitlag, the new rate exists but display/collision still owns the
GuardOn/Guard pose that received the hit. The first resumed animation update
switches to GuardSetOff and advances by the pressure-derived fractional rate.

The light/mid/dense control captures have raw SHA-256 values
`ed7023223832ca898467b5a30d55af9c62aa2abd8fc0da2d9f744cd1ecbeea36`,
`07537b376ae658a9e173ade4b31295e3b4a981c1bc74f6093fe2f0e4c0faca2e`, and
`8fc1da4409473796523e3775d22dd07e2e90808580dcad23f43f2db40fc7b5b3`.
Independent repeats are respectively
`fd3106778f8b3b4902068acb45993560b6e3a795c82c20bc029e0e26237f9621`,
`fafff5fed079fc8963be4a0e5f042151771931443631cadde518f617fa96e350`, and
`db5422a410358c9fba7e8853c2e338d53f925de80b6f944df130497ad9adae6c`.
Each repeat has an identical address-free semantic projection. Across the six
captures, the shared DAT/HSD evaluator qualifies 18 updates / 198 capsules
with exact hurt coordinates and maximum one-Q16 four-point ECB difference.

The parent-closed HSD profile now contains 22 motions, 1,574 FObj tracks, and
12,784 keys under data SHA-256
`386f7caf986b582363efc79aaf2efda04a93b812f9f3565ef62c6690eefe6e1b`.
Production reuses the canonical source-animation cursor and adds no rollback
field, runtime parsing, allocation, or host floating point. Nine generated
stored observations cover every captured post-hitlag phase; ordinary
production tests independently cover rate derivation, hitlag freeze, and the
first resumed pose for each pressure band. The identical-input movement gate
also replays every control and repeat capture from its explicit 22-unit
placement boundary through the production combat runner. All six 99-frame
comparisons pass and directly compare 36 live/native GuardSetOff frame/rate
rows. The native fixture retains Falcon's imported Jab 1 timing and source
spheres, rather than the removed legacy two-active-frame rectangle fallback.

## Falcon aerial-attack HSD geometry and common airborne ECB lock

The five aerial actions are parent-closed additions to the same pinned
`PlCa.dat` / `PlCaAJ.dat` / `PlCo.dat` / `PlCaGy.dat` profile used by ordinary
action geometry. The resulting immutable profile contains 50 motions, 3,366
FObj tracks, and 37,366 keys under decoded-data SHA-256
`caab1daafb4b54c836b1eee697ebe01935780561ed5ddaf421c3039ea4d7a552`.
Two independent Dolphin captures qualify Nair/Fair/Bair/Uair/Dair top, right,
and left coordinates together with the 20 grounded routes: 2,066 observations
and 1,840 unique frames, with maximum one-Q16 coordinate error.

The bottom coordinate is not an aerial-owned frame-data table. Pinned
`ftCommon_8007D5D4` enters air with ECB lock 10, while `Fighter_procMap`
decrements the lock before collision and retains the previous desired bottom
until expiry. Production therefore derives the live top/sides and eventual
bottom from HSD, while one shared transition rule owns the inherited-bottom
lifecycle. The former packed 195-value aerial bottom table has been removed;
the retained complete captures remain independent source evidence for the
shared rule and natural landing routes.

## Rapid Jab Start ECB reference space

Rapid Jab Start extends that same immutable profile to 51 motions, 3,424 FObj
tracks, and 37,533 keys under decoded-data SHA-256
`2e1bec542d6c3ae6ce21f814039bab2b81caf05f2eac03b05ecd0d0118189bd2`.
Pinned and current decomp agree that Attack100Start enters with zero blend.
The apparent 3,702-Q16 mismatch was exactly animated TransN: Falcon's submotion
49 lacks Fighter animation flag `0x80000000`, so `ftAnim_8006E054` does not
extract/zero TransN and `mpColl_LoadECB_JObj` remains model-root-relative.
Production and the generic source verifiers now derive this reference-space
choice from the imported per-submotion flag rather than an action special case.

Both existing ordinary-action captures qualify all five Rapid Jab Start frames
exactly. The complete ordinary-action source theorem now owns 26 motions,
2,086 observations, and 1,850 unique frames with maximum one-Q16 error; the
native production primitive exercises all 925 represented action poses.

## Falcon ordinary damage animation identity and HSD geometry

Pinned `ftCo_Damage.c` revision
`9509dc04406fb2028bfab01243841ba4787c0fb7` and current upstream revision
`d882af9` agree on the ordinary damage-motion table and callback order.
`ftCo_8008DCE0` indexes the table with the ground/air state captured before
launch conversion, knockback level 0-3, and the collided hurtbox height 0-2.
Falcon's resulting raw submotions are 165-179 (`DamageHi1` through
`DamageFlyLw`). `Fighter_ChangeMotionState(..., 0x40, 0, 1, 0)` followed by
`ftAnim_8006EBA4` makes source frame one visible immediately with no blend.
The selected frame remains frozen during hitlag and advances by one on each
resumed animation update.

The existing full collision-memory probe produced independent 138-row files
with SHA-256
`e34454e4f4cd7c3e02d46285820ce8210b9c002f6a32242577fba98aa9f0e437`
and
`24dc8291bcfe9ca8e470bda95e34e97242eb1138a5fc356eef91746777201401`.
Each contains 72 `DamageN2` rows with six repetitions of source frames
`1,1,1,2,3,4,5,6,7,8,9,10`. The generic HSD source verifier compares both
captures: 276 selected pose observations / 3,036 hurt capsules have maximum
one-Q16 coordinate error. After excluding each case's `_pre_hit` entry row,
132 ECB observations also match all four source-derived points within one Q16
unit. The exclusion is a callback boundary, not a geometry tolerance: the
entry row exposes the new action/skeleton while `fighter+0x794` still contains
the preceding map callback's desired ECB.

The generated parent-closed profile now contains 66 motions, 4,381 FObj
tracks, and 44,149 keys under decoded-data SHA-256
`d013285272bfe3c4ad7a52218d24dbc7aabda24293289fbc06445fd51ae68109`.
Production keeps the hurtbox height selected by the collision narrow phase,
looks up the raw damage submotion through the decomp table, and evaluates the
same immutable HSD profile used by ordinary attacks, specials, guard, and
movement. Generic rectangle fallback uses middle height explicitly. The
existing source-animation cursor is sufficient for rollback; no damage-only
pose array, parser, allocation, host floating point, or new snapshot field is
introduced.

The existing flat-ground late-DashAttack oracle now checks the production
`DamageLw1` source identity, frame-one hitlag freeze, resumed frames 2-11,
source-derived hurt capsules, and mid-damage save/load. The intentional source
identity update changes the deterministic replay corpus SHA-256 to
`7f210b0b70d2a506f60da411d4212885a5714ddc816c6fb076ad6273939a5ef0`;
final-state and event SHA-256 remain
`7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48`
and `55581ad6489814368e540e8eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd`.
The paired physical captures also qualify all six mixed hit-entry rows. The
frame pipeline runs map collision before attack collision and damage entry, so
each transition row combines DamageN2 frame-one hurt capsules with the
preceding Wait variant's ECB evaluated after its animation update. The generic
source verifier represents that ownership as `previous-row-post-animation`;
across both captures the complete damage theorem now covers 288 observations /
3,168 capsules and 12 mixed ECB rows at maximum one-Q16 error. Production
already has the same movement-before-combat order, so this theorem requires no
sampled table, duplicate pose evaluator, or rollback field.

## Falcon DamageFlyTop and DamageFlyRoll

Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7` and
current upstream revision `d882af94175e3c880ad51039e2979aa9a50aea09` agree
on this callback surface. `ftCo_8008DCE0` selects `DamageFlyTop` only for
airborne knockback level three inside the strict common-data `x234/x238`
70/110-degree cone. Otherwise, resulting damage at or above `x23C` (100%)
selects `DamageFlyRoll` when the next process-global `HSD_Randf` value is below
`x240` (0.3). The importer represents that float comparison as the exact
exclusive u16 boundary 19,661; no runtime float or guessed frame constant is
used. DI remains later, on hitlag exit, and therefore does not alter this
initial selector.

Falcon raw submotions 180 and 181 extend the shared parent-closed profile to
68 motions, 4,511 FObj tracks, and 44,881 keys under decoded-data SHA-256
`4994dfb44a97051627fb557c8f371f047d2e28cd5672c9b4c4a2aa143aa82ad3`.
For Roll, both entry and physics callbacks overwrite `FtPart_XRotN` with
`facing * atan2(self_vel.x + kb_vel.x, self_vel.y + kb_vel.y)`. Production
undoes the simulator's anisotropic coordinate conversion, performs that angle
in fixed turns, overwrites the imported local joint, and feeds the resulting
single pose to hurt-capsule and ECB evaluation. This adds no parser,
allocation, duplicated pose table, host floating point, or rollback field.

The existing accelerated checkpoint infrastructure supplies the live oracle.
A fresh response-only surface run contains `DamageFlyRoll` and passes all 145
rows across five cases at semantic SHA-256
`5339134dd04cff9612e8c8a3e1d460f85018ae4c081ac7426fbad3cee3b785f5`.
The pinned prone-response capture supplies a frame-one velocity-oriented Roll
ECB observation; the native gate bounds fixed-point matrix accumulation to
1,536 Q16 units (less than 0.024 simulation units) on every selector. The
surface verifier hashes only the raw stage-line ownership fields: later-added
derived world coordinates and decoded runtime flags are diagnostics that
duplicate those raw values, not new oracle truth.
The focused output removes 574 setup rows from the artifact but does not hide
the current harness cost: the single-process wrapper measures 4.234 seconds
warm against its strict three-second gate. The generic process-shard runner was
also measured and rejected here because each worker repeats Dolphin menu
startup, making end-to-end capture slower rather than faster.

## Falcon grounded slope damage response

Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7` and
current upstream `d882af94175e3c880ad51039e2979aa9a50aea09` have identical
`ftCo_8008DCE0` source. The route consumes `PlCo.dat` float words `x1E8`
(`0x3e32b8c2`, 0.17453292 radians) and `x1EC` (`0x3f4ccccd`, 0.8). The common
data importer emits their exact Q16 representations 11,380 and 52,429; native
tests also pin the owner raw words.

For a grounded victim, the source forms an isotropic positive-Y-up launch
vector and compares it with the current floor normal. Levels below three stay
grounded when the dot product is non-positive: `xF0_ground_kb_vel` retains the
raw horizontal scalar while `x8c_kb_vel` becomes the floor-tangent projection.
Level three is airborne regardless; only an angle strictly greater than 90
degrees plus `x1E8` reflects and scales vertical velocity by `x1EC`. Damage
motion selection still owns the original pre-projection/pre-DI vector.

The at-will checkpoint pack uses two actual Forward Tilts against a
crouch-cancelled Falcon on Hyrule line 36. Its independent 60-row captures
have identical observation arrays under canonical SHA-256
`cf0e43f47f38bccd2b344c516259620606f315bf46ac9373ac69e926cdb45c00`.
The selected address-free source fields hash to
`657b816faa98658d10be6783b912a380cf88c24ccc1120d0a5836f61e6aa6ac9`;
the production trace hashes to
`99e2eeefbeffde01318bf81dee3b5f57a8ed7db3fef755d9860beaa7c1af2e1f`.
Warm capture durations were 0.805771 and 0.737824 seconds.

The departing case also pins callback ownership beyond the numeric slope
rule. `ftCommon_8007D5D4` locks the grounded desired ECB bottom before the
ground-to-air transition, so the first post-hitlag DamageN2 collision can
recontact the downhill floor immediately. `ftCo_Landing_Enter_Basic` changes
ground/air state without rewriting `x8c_kb_vel`; the incoming air vector is
visible on Landing frame one and floor projection begins on the following
grounded physics update. The simulation stores the lock in root-space bottom
coordinates and converts its centre-space floor-sweep extent exactly once.
