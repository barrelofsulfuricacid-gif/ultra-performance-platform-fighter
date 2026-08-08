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
`ffc2c3fb3ba2b2cb7591fb857b7396d6bc901a3e34a7cd5cb24c47334bfa86d3`.
The expanded behavior-bearing source digest is
`c7cf308115511ee2872fa9fe610f0bb264a7dde34f3f3edc357cbfe73822a015`;
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
geometry. For every normal and aerial currently routed by production combat,
plus standing and dash grab,
an identical-input Dolphin 3.4.0 capture reads the live transformed attack
spheres from the owner's `GALE01` NTSC 1.02 executable. The disc SHA-256 is
`0de05981a34156b9cedcef73c73d4244ac05cf6149ab3c9cfed917698819e464`;
the 1,933-row capture SHA-256 is
`5a7ac3a35775b0352d48566d622860c846fa2907c4bef03f760080f2a18ba3e8`.
The memory layout and transforms were checked against `doldecomp/melee`
revision `9509dc04406fb2028bfab01243841ba4787c0fb7`. The capture records the
active `ftHit` spheres and converts each bone-relative point through the live
`HSD_JObj` world matrix. The importer also verifies every sphere's damage,
angle, KBG, weight-set knockback, and BKB against the hash-pinned full
`meleeFrameDataExtractor` output, so a geometric sphere cannot silently select
an unrelated effect row.

`tools/import_ssbm_falcon_hit_geometry.py` converts that evidence into
`generated/data/m4_falcon_ntsc102_hit_geometry.inc`. The compact table includes
jab 1, jab 2, dash attack, forward/up/down tilt, forward/up/down smash, all five
aerials, standing grab, dash grab, all three ordinary throw-hitbox actions,
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

The probe reads Falcon's 11 live `FighterHurtCapsule` records from
`fighter+0x11a0` with stride `0x4c`. A second 1,948-row Dolphin capture, SHA-256
`d9fea72b7eb86447e5bd53b2157ec7f3dde9a27f02a28750ec4964ab6bd7ef32`,
records the acting Falcon on every displayed frame of all 16 routed
normal/aerial/grab actions. Its full-hop, delayed-double-jump aerial setup keeps
Falcon airborne through neutral-air and down-air frame 44. Additional hash-
pinned executable captures cover every displayed frame of all 17 Falcon
special subactions. The final wall-rebound row reuses the source-defined Falcon
Dive throw animation exactly as the pinned DAT motion-state table does; it is
not an invented pose. The importer rejects even one missing source frame
instead of cloning the previous pose. The phase-pinned Stand frame-18 pose
remains the grounded-idle route. A third 3,818-row Slippi Dolphin 3.5.1 ExiAI
capture,
SHA-256
`ccfbb5edaa952760a4058a98213ee3fb6b54cd3bd9900e8c25aaa2535e4c8a5e`,
adds every displayed Initial Dash frame 1-15, RunBrake frame 1-28, CrouchStart
frame 1-7, CrouchEnd frame 1-10, KneeBend frame 1-4, SpotDodge frame 1-32,
RollForward frame 1-31, RollBackward frame 1-31, AirDodge frame 1-49,
FallSpecial frame 1-8, and speed-scaled LandingFallSpecial source frames
1, 4, 7, 10, 13, 16, 19, 22, 25, and 28. Its
controller-port cross-check requires both live poses to canonicalize to the
same Falcon Q16.16 capsules; capture metadata alone is not accepted as proof
that the menu spawned the requested character. The ExiAI path is separately
qualified against an unaccelerated same-binary control at SHA-256
`8f27b27e82cbcb3f50db6595f050580c921ad7316a40f1b819489c73df12da1c`;
the A/B comparison covers 1,773/1,049 active fighter/opponent rows and
1,674/1,023 qualified action-owned pose rows. Every collision control first
pre-places both ports safely, settles them, establishes explicit facing, and
fully recovers before final placement; this prevents preceding routes from
leaking airborne state, facing, or velocity.
Looping idle, `Ft_MF_SkipAnim` GuardReflect inheritance, and positive-hitlag
bone endpoints are forbidden as geometry sources, so every imported KneeBend,
SpotDodge, RollForward, RollBackward, AirDodge, FallSpecial, and
LandingFallSpecial sample comes from a dedicated active, non-hitlag track. The
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
rectangle falsely misses the hit at -0.063411903.

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
`181c614a07192564a18c779515c53e7f906c34a1d6ececf67e912cb8ed8d2e42`.
Pinned regeneration produces the tracked include at SHA-256
`f9e823f41da8ae4684791ba1aa667f903e422bc23d237139634118230607e6df`.
That digest is compiled into every M4 content hash, so changing geometry cannot
retain an old compatibility identity. Production combat queries this table
for implemented normals, aerials, grabs, normal throws, all 17 Falcon special
subactions, Initial Dash, RunBrake, CrouchStart, CrouchEnd, KneeBend,
SpotDodge, RollForward, RollBackward, AirDodge, FallSpecial, and
LandingFallSpecial.
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
frames. `tools/verify_m4_falcon_dive.sh` checks all 92 holder frames plus 42
victim capture/reaction frames at will. The later one-percent change in the
y=500 capture is off-screen magnifier damage and is excluded from move damage.

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
